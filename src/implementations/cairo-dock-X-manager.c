/**
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define __USE_POSIX
#include <signal.h>

#include <cairo.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>

#include "gldi-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.iMinimizedWindowRenderType
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-X-manager.h"

#define CAIRO_DOCK_CHECK_XEVENTS_INTERVAL 200

// public (manager, config, data)
GldiXManager myXMgr;

// dependancies
extern GldiContainer *g_pPrimaryContainer;
//extern int g_iDamageEvent;

// private
static Display *s_XDisplay = NULL;
static int s_iSidPollXEvents = 0;
static Atom s_aNetClientList;
static Atom s_aNetActiveWindow;
static Atom s_aNetCurrentDesktop;
static Atom s_aNetDesktopViewport;
static Atom s_aNetDesktopGeometry;
static Atom s_aNetWorkarea;
static Atom s_aNetShowingDesktop;
static Atom s_aRootMapID;
static Atom s_aNetNbDesktops;
static Atom s_aNetDesktopNames;
static Atom s_aXKlavierState;
static Atom s_aNetWmState;
static Atom s_aNetWmDesktop;
static Atom s_aNetWmName;
static Atom s_aWmName;
static Atom s_aWmClass;
static Atom s_aNetWmIcon;
static Atom s_aWmHints;
static GHashTable *s_hXWindowTable = NULL;  // table of (Xid,actor)
static int s_iTime = 1;  // on peut aller jusqu'a 2^31, soit 17 ans a 4Hz.
static int s_iNumWindow = 1;  // used to order appli icons by age (=creation date).
static Window s_iCurrentActiveWindow = 0;

typedef enum {
	X_DEMANDS_ATTENTION = (1<<0),
	X_URGENCY_HINT = (1 << 2)
} XAttentionFlag;


static GldiXWindowActor *_make_new_actor (Window Xid)
{
	GldiXWindowActor *xactor;
	gboolean bShowInTaskbar = FALSE;
	gboolean bNormalWindow = FALSE;
	Window iTransientFor = None;
	gchar *cClass = NULL, *cWmClass = NULL;
	gboolean bIsHidden = FALSE, bIsFullScreen = FALSE, bIsMaximized = FALSE, bDemandsAttention = FALSE;
	
	//\__________________ see if we should skip it
	bShowInTaskbar = cairo_dock_xwindow_is_fullscreen_or_hidden_or_maximized (Xid, &bIsFullScreen, &bIsHidden, &bIsMaximized, &bDemandsAttention);
	
	if (bShowInTaskbar)
	{
		//\__________________ get its type
		bNormalWindow = cairo_dock_get_xwindow_type (Xid, &iTransientFor);
		if (bNormalWindow || iTransientFor != None)
		{
			//\__________________ get its class
			cClass = cairo_dock_get_xwindow_class (Xid, &cWmClass);
			if (cClass == NULL)
			{
				cd_warning ("this window (%s, %ld) doesn't belong to any class, skip it.\nPlease report this bug to the application's devs.", cairo_dock_get_xwindow_name (Xid, TRUE), Xid);
				bShowInTaskbar = FALSE;
			}
		}
		else
		{
			cd_debug ("unwanted type -> ignore this window");
			bShowInTaskbar = FALSE;
		}
	}
	
	if (bShowInTaskbar)  // the window passed all the tests -> make a new actor
	{
		xactor = (GldiXWindowActor*)gldi_object_new (GLDI_MANAGER (&myXMgr), &Xid);
		// set all the properties we got before
		xactor->XTransientFor = iTransientFor;
		GldiWindowActor *actor = (GldiWindowActor*)xactor;
		actor->bDisplayed = bNormalWindow;
		actor->cClass = cClass;
		actor->cWmClass = cWmClass;
		actor->bIsHidden = bIsHidden;
		actor->bIsMaximized = bIsMaximized;
		actor->bIsFullScreen = bIsFullScreen;
		actor->bDemandsAttention = bDemandsAttention;
	}
	else  // make a dumy actor, so that we don't try to check it any more
	{
		cd_debug ("a shy window");
		xactor = g_new0 (GldiXWindowActor, 1);
		xactor->Xid = Xid;
		xactor->bIgnored = TRUE;
		
		// add to table
		Window *pXid = g_new (Window, 1);
		*pXid = xactor->Xid;
		g_hash_table_insert (s_hXWindowTable, pXid, xactor);
	}
	
	xactor->iLastCheckTime = s_iTime;
	return xactor;
}

static void _delete_actor (GldiXWindowActor *actor)
{
	if (actor->bIgnored)  // it's a dummy actor, just free it
	{
		// remove from table
		if (actor->iLastCheckTime != -1)  // if not already removed
			g_hash_table_remove (s_hXWindowTable, &actor->Xid);
		g_free (actor);
	}
	else
	{
		gldi_object_unref (GLDI_OBJECT(actor));
	}
}

  ////////////////
 /// X events ///
////////////////

static inline void _cairo_dock_retrieve_current_desktop_and_viewport (void)
{
	g_desktopGeometry.iCurrentDesktop = cairo_dock_get_current_desktop ();
	cairo_dock_get_current_viewport (&g_desktopGeometry.iCurrentViewportX, &g_desktopGeometry.iCurrentViewportY);
	g_desktopGeometry.iCurrentViewportX /= gldi_get_desktop_width();
	g_desktopGeometry.iCurrentViewportY /= gldi_get_desktop_height();
}

static gboolean _on_change_current_desktop_viewport (void)
{
	_cairo_dock_retrieve_current_desktop_and_viewport ();
	
	// on propage la notification.
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	
	// on gere le cas delicat de X qui nous fait sortir du dock plus tard.
	return FALSE;
}

static void _on_change_nb_desktops (void)
{
	g_desktopGeometry.iNbDesktops = cairo_dock_get_nb_desktops ();
	_cairo_dock_retrieve_current_desktop_and_viewport ();  // au cas ou on enleve le bureau courant.
	
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
}

static void _on_change_desktop_geometry (void)
{
	// check if the resolution has changed
	gboolean bSizeChanged = cairo_dock_update_screen_geometry ();
	
	// check if the number of viewports has changed.
	cairo_dock_get_nb_viewports (&g_desktopGeometry.iNbViewportX, &g_desktopGeometry.iNbViewportY);
	_cairo_dock_retrieve_current_desktop_and_viewport ();  // au cas ou on enleve le viewport courant.
	
	// notify everybody
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, bSizeChanged);
}

static void _update_backing_pixmap (GldiXWindowActor *actor)
{
#ifdef HAVE_XEXTEND
	if (myTaskbarParam.bShowAppli && myTaskbarParam.iMinimizedWindowRenderType == 1)
	{
		if (actor->iBackingPixmap != 0)
			XFreePixmap (s_XDisplay, actor->iBackingPixmap);
		actor->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, actor->Xid);
		cd_debug ("new backing pixmap : %d", actor->iBackingPixmap);
	}
#endif
}

static gboolean _remove_old_applis (Window *Xid, GldiXWindowActor *actor, gpointer iTimePtr)
{
	gint iTime = GPOINTER_TO_INT (iTimePtr);
	if (actor->iLastCheckTime >= 0 && actor->iLastCheckTime < iTime)
	{
		cd_message ("cette fenetre (%ld, %p, %s) est trop vieille (%d / %d)", *Xid, actor, actor->actor.cName, actor->iLastCheckTime, iTime);
		// notify everybody
		if (! actor->bIgnored)
			gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
		
		actor->iLastCheckTime = -1;  // to not remove it from the table during the free
		_delete_actor (actor);
		return TRUE;  // remove it from the table
	}
	return FALSE;
}
static void _on_update_applis_list (void)
{
	s_iTime ++;
	// get all windows sorted by z-order
	gulong i, iNbWindows = 0;
	Window *pXWindowsList = cairo_dock_get_windows_list (&iNbWindows, TRUE);  // TRUE => ordered by z-stack.
	
	Window Xid;
	GldiXWindowActor *actor;
	int iStackOrder = 0;
	for (i = 0; i < iNbWindows; i ++)
	{
		Xid = pXWindowsList[i];
		
		// check if the window is already known
		actor = g_hash_table_lookup (s_hXWindowTable, &Xid);
		if (actor == NULL)
		{
			// create a window actor
			cd_message (" cette fenetre (%ld) de la pile n'est pas dans la liste", Xid);
			actor = _make_new_actor (Xid);
			
			// notify everybody
			if (! actor->bIgnored)
				gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_CREATED, actor);
		}
		else  // just update its check-time
			actor->iLastCheckTime = s_iTime;
		
		// update the z-order
		if (! actor->bIgnored)
			actor->actor.iStackOrder = iStackOrder ++;
	}
	
	// remove old windows
	g_hash_table_foreach_remove (s_hXWindowTable, (GHRFunc) _remove_old_applis, GINT_TO_POINTER (s_iTime));
	
	XFree (pXWindowsList);
}

static void _set_demand_attention (GldiXWindowActor *actor, XAttentionFlag flag)
{
	cd_debug ("%d; %s/%s", actor->iDemandsAttention, actor->actor.cLastAttentionDemand, actor->actor.cName);
	if (actor->iDemandsAttention != 0  // already demanding attention
	&& g_strcmp0 (actor->actor.cLastAttentionDemand, actor->actor.cName) == 0)  // and message has not changed between the 2 demands.
	{
		actor->iDemandsAttention |= flag;
		return;  // some WM tend to abuse this state, so we only acknowledge it if it's not already set.
	}
	actor->iDemandsAttention |= flag;
	g_free (actor->actor.cLastAttentionDemand);
	actor->actor.cLastAttentionDemand = g_strdup (actor->actor.cName);
	actor->actor.bDemandsAttention = TRUE;
	gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_ATTENTION_CHANGED, actor);
}

static void _unset_demand_attention (GldiXWindowActor *actor, XAttentionFlag flag)
{
	if (actor->iDemandsAttention == 0)
		return;
	cd_debug ("%d; %s/%s", actor->iDemandsAttention, actor->actor.cLastAttentionDemand, actor->actor.cName);
	actor->iDemandsAttention &= (~flag);
	if (actor->iDemandsAttention != 0)  // still demanding attention
		return;
	actor->actor.bDemandsAttention = FALSE;
	gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_ATTENTION_CHANGED, actor);
}

static gboolean _cairo_dock_unstack_Xevents (G_GNUC_UNUSED gpointer data)
{
	static XEvent event;
	
	if (!g_pPrimaryContainer)  // peut arriver en cours de chargement d'un theme.
		return TRUE;
	
	long event_mask = 0xFFFFFFFF;  // on les recupere tous, ca vide la pile au fur et a mesure plutot que tout a la fin.
	Window Xid;
	Window root = DefaultRootWindow (s_XDisplay);
	
	while (XCheckMaskEvent (s_XDisplay, event_mask, &event))
	{
		Xid = event.xany.window;
		//g_print ("  type : %d; atom : %s; window : %d\n", event.type, XGetAtomName (s_XDisplay, event.xproperty.atom), Xid);
		if (Xid == root)  // event on the desktop
		{
			if (event.type == PropertyNotify)
			{
				if (event.xproperty.atom == s_aNetClientList)
				{
					_on_update_applis_list ();
				}
				else if (event.xproperty.atom == s_aNetActiveWindow)
				{
					Window XActiveWindow = cairo_dock_get_active_xwindow ();
					
					gboolean bForceKbdStateRefresh = FALSE;
					if (XActiveWindow != s_iCurrentActiveWindow)
					{
						if (s_iCurrentActiveWindow == None)
							bForceKbdStateRefresh = TRUE;
						s_iCurrentActiveWindow = XActiveWindow;
						GldiXWindowActor *xactor = g_hash_table_lookup (s_hXWindowTable, &XActiveWindow);
						gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_ACTIVATED, xactor && ! xactor->bIgnored ? xactor : NULL);
						if (bForceKbdStateRefresh)
						{
							// si on active une fenetre n'ayant pas de focus clavier, on n'aura pas d'evenement kbd_changed, pourtant en interne le clavier changera. du coup si apres on revient sur une fenetre qui a un focus clavier, il risque de ne pas y avoir de changement de clavier, et donc encore une fois pas d'evenement ! pour palier a ce, on considere que les fenetres avec focus clavier sont celles presentes en barre des taches. On decide de generer un evenement lorsqu'on revient sur une fenetre avec focus, a partir d'une fenetre sans focus (mettre a jour le clavier pour une fenetre sans focus n'a pas grand interet, autant le laisser inchange).
							gldi_object_notify (&myDesktopMgr, NOTIFICATION_KBD_STATE_CHANGED, xactor);
						}
					}
				}
				else if (event.xproperty.atom == s_aNetCurrentDesktop || event.xproperty.atom == s_aNetDesktopViewport)
				{
					_on_change_current_desktop_viewport ();  // -> NOTIFICATION_DESKTOP_CHANGED
				}
				else if (event.xproperty.atom == s_aNetNbDesktops)
				{
					_on_change_nb_desktops ();  // -> NOTIFICATION_DESKTOP_GEOMETRY_CHANGED
				}
				else if (event.xproperty.atom == s_aNetDesktopGeometry || event.xproperty.atom == s_aNetWorkarea)  // check s_aNetWorkarea too, to workaround a bug in Compiz (or X?) : when down-sizing the screen, the _NET_DESKTOP_GEOMETRY atom is not received  (up-sizing is ok though, and changing the viewport makes the atom to be received); but _NET_WORKAREA is correctly sent; since it's only sent when the resolution is changed, or the dock's height (if space is reserved), it's not a big overload to check it too.
				{
					_on_change_desktop_geometry ();  // -> NOTIFICATION_DESKTOP_GEOMETRY_CHANGED
				}
				else if (event.xproperty.atom == s_aRootMapID)
				{
					gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_WALLPAPER_CHANGED);
				}
				else if (event.xproperty.atom == s_aNetShowingDesktop)
				{
					gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_VISIBILITY_CHANGED);
				}
				else if (event.xproperty.atom == s_aXKlavierState)
				{
					gldi_object_notify (&myDesktopMgr, NOTIFICATION_KBD_STATE_CHANGED, NULL);
				}
				else if (event.xproperty.atom == s_aNetDesktopNames)
				{
					gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_NAMES_CHANGED);
				}
			}  // end of PropertyNotify on root.
		}
		else  // event on a window.
		{
			GldiXWindowActor *xactor = g_hash_table_lookup (s_hXWindowTable, &Xid);
			GldiWindowActor *actor = (GldiWindowActor*)xactor;
			if (! actor)
				continue;
			
			if (event.type == PropertyNotify)
			{
				if (event.xproperty.atom == s_aXKlavierState)
				{
					gldi_object_notify (&myDesktopMgr, NOTIFICATION_KBD_STATE_CHANGED, actor);
				}
				else if (event.xproperty.atom == s_aNetWmState)
				{
					// get current state
					gboolean bIsFullScreen, bIsHidden, bIsMaximized, bDemandsAttention;
					gboolean bSkipTaskbar = ! cairo_dock_xwindow_is_fullscreen_or_hidden_or_maximized (Xid, &bIsFullScreen, &bIsHidden, &bIsMaximized, &bDemandsAttention);
					
					// special case where a window enters/leaves the taskbar
					if (bSkipTaskbar != xactor->bIgnored)
					{
						if (xactor->bIgnored)  // was ignored, simply recreate it
						{
							// remove it from the table, so that the XEvent loop detects it again
							g_hash_table_remove (s_hXWindowTable, &Xid);  // remove it explicitely, because the 'unref' might not free it
							xactor->iLastCheckTime = -1;
							_delete_actor (xactor);  // unref it since we don't need it anymore
						}
						else  // is now ignored
						{
							xactor->bIgnored = bSkipTaskbar;
							gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
						}
						continue;  // actor is either freeed or ignored
					}
					
					if (xactor->bIgnored)  // skip taskbar
						continue;
					// update the actor
					gboolean bHiddenChanged     = (bIsHidden != actor->bIsHidden);
					gboolean bMaximizedChanged  = (bIsMaximized != actor->bIsMaximized);
					gboolean bFullScreenChanged = (bIsFullScreen != actor->bIsFullScreen);
					actor->bIsHidden     = bIsHidden;
					actor->bIsMaximized  = bIsMaximized;
					actor->bIsFullScreen = bIsFullScreen;
					if (bHiddenChanged && ! bIsHidden)  // the window is now mapped => BackingPixmap is available.
						_update_backing_pixmap (xactor);
					
					// notify everybody
					if (bDemandsAttention)
						_set_demand_attention (xactor, X_DEMANDS_ATTENTION);  // -> NOTIFICATION_WINDOW_ATTENTION_CHANGED
					else
						_unset_demand_attention (xactor, X_DEMANDS_ATTENTION);  // -> NOTIFICATION_WINDOW_ATTENTION_CHANGED
					gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_STATE_CHANGED, actor, bHiddenChanged, bMaximizedChanged, bFullScreenChanged);
				}
				else if (event.xproperty.atom == s_aNetWmDesktop)
				{
					if (xactor->bIgnored)  // skip taskbar
						continue;
					// update the actor
					actor->iNumDesktop = cairo_dock_get_xwindow_desktop (Xid);
					
					// notify everybody
					gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, actor);
				}
				else if (event.xproperty.atom == s_aWmName
				|| event.xproperty.atom == s_aNetWmName)
				{
					if (xactor->bIgnored)  // skip taskbar
						continue;
					// update the actor
					g_free (actor->cName);
					actor->cName = cairo_dock_get_xwindow_name (Xid, event.xproperty.atom == s_aWmName);
					// notify everybody
					gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_NAME_CHANGED, actor);
				}
				else if (event.xproperty.atom == s_aWmHints)
				{
					if (xactor->bIgnored)  // skip taskbar
						continue;
					// get the hints
					XWMHints *pWMHints = XGetWMHints (s_XDisplay, Xid);
					if (pWMHints != NULL)
					{
						// notify everybody
						if (pWMHints->flags & XUrgencyHint)  // urgency flag is set
							_set_demand_attention (xactor, X_URGENCY_HINT);  // -> NOTIFICATION_WINDOW_ATTENTION_CHANGED
						else
							_unset_demand_attention (xactor, X_URGENCY_HINT);  // -> NOTIFICATION_WINDOW_ATTENTION_CHANGED
						
						if (event.xproperty.state == PropertyNewValue && (pWMHints->flags & (IconPixmapHint | IconMaskHint | IconWindowHint)))
						{
							gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_ICON_CHANGED, actor);
						}
						XFree (pWMHints);
					}
					else  // no hints set on this window, assume it unsets the urgency flag
					{
						_unset_demand_attention (xactor, X_URGENCY_HINT);  // -> NOTIFICATION_WINDOW_ATTENTION_CHANGED
					}
				}
				else if (event.xproperty.atom == s_aNetWmIcon)
				{
					if (xactor->bIgnored)  // skip taskbar
						continue;
					// notify everybody
					gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_ICON_CHANGED, actor);
				}
				else if (event.xproperty.atom == s_aWmClass)
				{
					if (xactor->bIgnored)  // skip taskbar
						continue;
					// update the actor
					gchar *cOldClass = actor->cClass, *cOldWmClass = actor->cWmClass;
					gchar *cWmClass = NULL;
					gchar *cNewClass = cairo_dock_get_xwindow_class (Xid, &cWmClass);
					if (! cNewClass || g_strcmp0 (cNewClass, cOldClass) == 0)
						continue;
					actor->cClass = cNewClass;
					actor->cWmClass = cWmClass;
					
					// notify everybody
					gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_CLASS_CHANGED, actor, cOldClass, cOldWmClass);
					
					g_free (cOldClass);
					g_free (cOldWmClass);
				}
			}
			else if (event.type == ConfigureNotify)
			{
				if (xactor->bIgnored)  // skip taskbar
					continue;
				// update the actor
				int x = event.xconfigure.x, y = event.xconfigure.y;
				int w = event.xconfigure.width, h = event.xconfigure.height;
				cairo_dock_get_xwindow_geometry (Xid, &x, &y, &w, &h);
				actor->windowGeometry.width = w;
				actor->windowGeometry.height = h;
				actor->windowGeometry.x = x;
				actor->windowGeometry.y = y;
				
				actor->iViewPortX = x / gldi_get_desktop_width() + g_desktopGeometry.iCurrentViewportX;
				actor->iViewPortY = y / gldi_get_desktop_height() + g_desktopGeometry.iCurrentViewportY;
				
				if (w != actor->windowGeometry.width || h != actor->windowGeometry.height)  // size has changed
				{
					_update_backing_pixmap (xactor);
				}
				
				// notify everybody
				gldi_object_notify (&myWindowsMgr, NOTIFICATION_WINDOW_SIZE_POSITION_CHANGED, actor);
			}
			/*else if (event.type == g_iDamageEvent + XDamageNotify)
			{
				XDamageNotifyEvent *e = (XDamageNotifyEvent *) &event;
				cd_debug ("window %s has been damaged (%d;%d %dx%d)", e->drawable, e->area.x, e->area.y, e->area.width, e->area.height);
				// e->drawable is the window ID of the damaged window
				// e->geometry is the geometry of the damaged window	
				// e->area     is the bounding rect for the damaged area	
				// e->damage   is the damage handle returned by XDamageCreate()
				// Subtract all the damage, repairing the window.
				XDamageSubtract (s_XDisplay, e->damage, None, None);
			}
			else
				cd_debug ("  type : %d (%d); window : %d", event.type, XDamageNotify, Xid);*/
		}  // end of event
	}
	if (XEventsQueued (s_XDisplay, QueuedAlready) != 0)
		XSync (s_XDisplay, True);  // True <=> discard.
	//g_print ("XEventsQueued : %d\n", XEventsQueued (s_XDisplay, QueuedAfterFlush));  // QueuedAlready, QueuedAfterReading, QueuedAfterFlush
	
	return TRUE;
}


  ///////////////////////////////
 /// DESKTOP MANAGER BACKEND ///
///////////////////////////////

static gboolean _show_hide_desktop (gboolean bShow)
{
	cairo_dock_show_hide_desktop (bShow);
	return TRUE;
}

static gboolean _desktop_is_visible (void)
{
	return cairo_dock_desktop_is_visible ();
}

static gchar ** _get_desktops_names (void)
{
	return cairo_dock_get_desktops_names ();
}

static gboolean _set_desktops_names (gchar **cNames)
{
	cairo_dock_set_desktops_names (cNames);
	return TRUE;
}

static gboolean _set_current_desktop (int iDesktopNumber, int iViewportNumberX, int iViewportNumberY)
{
	if (iDesktopNumber >= 0)
		cairo_dock_set_current_desktop (iDesktopNumber);
	if (iViewportNumberX >= 0 && iViewportNumberY >= 0)
		cairo_dock_set_current_viewport (iViewportNumberX, iViewportNumberY);
	return TRUE;
}

static gboolean _set_nb_desktops (int iNbDesktops, int iNbViewportX, int iNbViewportY)
{
	if (iNbDesktops >= 0)
		cairo_dock_set_nb_desktops (iNbDesktops);
	if (iNbViewportX >= 0 && iNbViewportY >= 0)
		cairo_dock_set_nb_viewports (iNbViewportX, iNbViewportY);
	return TRUE;
}

static cairo_surface_t *_get_desktop_bg_surface (void)  // attention : fonction lourde.
{
	//g_print ("+++ %s ()\n", __func__);
	Pixmap iRootPixmapID = cairo_dock_get_window_background_pixmap (cairo_dock_get_root_id ());
	g_return_val_if_fail (iRootPixmapID != 0, NULL);  // Note: depending on the WM, iRootPixmapID might be 0, and a window of type 'Desktop' might be used instead (covering the whole screen). We don't handle this case, as I've never encountered it yet.
	
	cairo_surface_t *pDesktopBgSurface = NULL;
	GdkPixbuf *pBgPixbuf = cairo_dock_get_pixbuf_from_pixmap (iRootPixmapID, FALSE);  // FALSE <=> don't add alpha channel
	if (pBgPixbuf != NULL)
	{
		if (gdk_pixbuf_get_height (pBgPixbuf) == 1 && gdk_pixbuf_get_width (pBgPixbuf) == 1)  // single color
		{
			guchar *pixels = gdk_pixbuf_get_pixels (pBgPixbuf);
			cd_debug ("c'est une couleur unie (%.2f, %.2f, %.2f)", (double) pixels[0] / 255, (double) pixels[1] / 255, (double) pixels[2] / 255);
			
			pDesktopBgSurface = cairo_dock_create_blank_surface (
				gldi_get_desktop_width(),
				gldi_get_desktop_height());
			
			cairo_t *pCairoContext = cairo_create (pDesktopBgSurface);
			cairo_set_source_rgb (pCairoContext,
				(double) pixels[0] / 255,
				(double) pixels[1] / 255,
				(double) pixels[2] / 255);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
			cairo_paint (pCairoContext);
			cairo_destroy (pCairoContext);
		}
		else
		{
			double fWidth, fHeight;
			cairo_surface_t *pBgSurface = cairo_dock_create_surface_from_pixbuf (pBgPixbuf,
				1,
				0,
				0,
				FALSE,
				&fWidth,
				&fHeight,
				NULL, NULL);
			
			if (fWidth < gldi_get_desktop_width() || fHeight < gldi_get_desktop_height())  // pattern/color gradation
			{
				cd_debug ("c'est un degrade ou un motif (%dx%d)", (int) fWidth, (int) fHeight);
				pDesktopBgSurface = cairo_dock_create_blank_surface (
					gldi_get_desktop_width(),
					gldi_get_desktop_height());
				cairo_t *pCairoContext = cairo_create (pDesktopBgSurface);
				
				cairo_pattern_t *pPattern = cairo_pattern_create_for_surface (pBgSurface);
				g_return_val_if_fail (cairo_pattern_status (pPattern) == CAIRO_STATUS_SUCCESS, NULL);
				cairo_pattern_set_extend (pPattern, CAIRO_EXTEND_REPEAT);
				
				cairo_set_source (pCairoContext, pPattern);
				cairo_paint (pCairoContext);
				
				cairo_destroy (pCairoContext);
				cairo_pattern_destroy (pPattern);
				cairo_surface_destroy (pBgSurface);
			}
			else  // image
			{
				cd_debug ("c'est un fond d'ecran de taille %dx%d", (int) fWidth, (int) fHeight);
				pDesktopBgSurface = pBgSurface;
			}
		}
		
		g_object_unref (pBgPixbuf);
	}
	return pDesktopBgSurface;
}


  ///////////////////////////////
 /// WINDOWS MANAGER BACKEND ///
///////////////////////////////


static void _move_to_nth_desktop (GldiWindowActor *actor, int iNumDesktop, int iDeltaViewportX, int iDeltaViewportY)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_move_xwindow_to_nth_desktop (xactor->Xid, iNumDesktop, iDeltaViewportX, iDeltaViewportY);
}

static void _show (GldiWindowActor *actor)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_show_xwindow (xactor->Xid);
}
static void _close (GldiWindowActor *actor)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_close_xwindow (xactor->Xid);
}
static void _kill (GldiWindowActor *actor)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_kill_xwindow (xactor->Xid);
}
static void _minimize (GldiWindowActor *actor)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_minimize_xwindow (xactor->Xid);
}
static void _lower (GldiWindowActor *actor)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_lower_xwindow (xactor->Xid);
}
static void _maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_maximize_xwindow (xactor->Xid, bMaximize);
}
static void _set_fullscreen (GldiWindowActor *actor, gboolean bFullScreen)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_set_xwindow_fullscreen (xactor->Xid, bFullScreen);
}
static void _set_above (GldiWindowActor *actor, gboolean bAbove)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_set_xwindow_above (xactor->Xid, bAbove);
}

static void _set_minimize_position (GldiWindowActor *actor, int x, int y)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_set_xicon_geometry (xactor->Xid, x, y, 1, 1);
}

static void _set_thumbnail_area (GldiWindowActor *actor, int x, int y, int w, int h)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_set_xicon_geometry (xactor->Xid, x, y, w, h);
}

static GldiWindowActor* _get_active_window (void)
{
	if (s_iCurrentActiveWindow == 0)
		return NULL;
	return g_hash_table_lookup (s_hXWindowTable, &s_iCurrentActiveWindow);
}

static void _set_window_border (GldiWindowActor *actor, gboolean bWithBorder)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_set_xwindow_border (xactor->Xid, bWithBorder);
}

static cairo_surface_t* _get_icon_surface (GldiWindowActor *actor, int iWidth, int iHeight)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	return cairo_dock_create_surface_from_xwindow (xactor->Xid, iWidth, iHeight);
}

static cairo_surface_t* _get_thumbnail_surface (GldiWindowActor *actor, int iWidth, int iHeight)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	return cairo_dock_create_surface_from_xpixmap (xactor->iBackingPixmap, iWidth, iHeight);
}

static GLuint _get_texture (GldiWindowActor *actor)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	return cairo_dock_texture_from_pixmap (xactor->Xid, xactor->iBackingPixmap);
}

static GldiWindowActor *_get_transient_for (GldiWindowActor *actor)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	if (xactor->XTransientFor == None)
		return NULL;
	return g_hash_table_lookup (s_hXWindowTable, &xactor->XTransientFor);
}

static void _is_above_or_below (GldiWindowActor *actor, gboolean *bIsAbove, gboolean *bIsBelow)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_xwindow_is_above_or_below (xactor->Xid, bIsAbove, bIsBelow);
}

static gboolean _is_sticky (GldiWindowActor *actor)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	return cairo_dock_xwindow_is_sticky (xactor->Xid);
}

static void _set_sticky (GldiWindowActor *actor, gboolean bSticky)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_set_xwindow_sticky (xactor->Xid, bSticky);
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	//\__________________ init internal data
	s_aNetClientList		= XInternAtom (s_XDisplay, "_NET_CLIENT_LIST_STACKING", False);
	s_aNetActiveWindow		= XInternAtom (s_XDisplay, "_NET_ACTIVE_WINDOW", False);
	s_aNetCurrentDesktop	= XInternAtom (s_XDisplay, "_NET_CURRENT_DESKTOP", False);
	s_aNetDesktopViewport	= XInternAtom (s_XDisplay, "_NET_DESKTOP_VIEWPORT", False);
	s_aNetDesktopGeometry	= XInternAtom (s_XDisplay, "_NET_DESKTOP_GEOMETRY", False);
	s_aNetWorkarea			= XInternAtom (s_XDisplay, "_NET_WORKAREA", False);
	s_aNetShowingDesktop 	= XInternAtom (s_XDisplay, "_NET_SHOWING_DESKTOP", False);
	s_aRootMapID			= XInternAtom (s_XDisplay, "_XROOTPMAP_ID", False);  // Note: ESETROOT_PMAP_ID might be used instead. We don't handle it as it seems quite rare and somewhat deprecated.
	s_aNetNbDesktops		= XInternAtom (s_XDisplay, "_NET_NUMBER_OF_DESKTOPS", False);
	s_aNetDesktopNames		= XInternAtom (s_XDisplay, "_NET_DESKTOP_NAMES", False);
	s_aXKlavierState		= XInternAtom (s_XDisplay, "XKLAVIER_STATE", False);
	s_aNetWmState			= XInternAtom (s_XDisplay, "_NET_WM_STATE", False);
	s_aNetWmName 			= XInternAtom (s_XDisplay, "_NET_WM_NAME", False);
	s_aWmName 				= XInternAtom (s_XDisplay, "WM_NAME", False);
	s_aWmClass 				= XInternAtom (s_XDisplay, "WM_CLASS", False);
	s_aNetWmIcon 			= XInternAtom (s_XDisplay, "_NET_WM_ICON", False);
	s_aWmHints 				= XInternAtom (s_XDisplay, "WM_HINTS", False);
	s_aNetWmDesktop			= XInternAtom (s_XDisplay, "_NET_WM_DESKTOP", False);
	
	s_hXWindowTable = g_hash_table_new_full (g_int_hash,
		g_int_equal,
		g_free,  // Xid
		NULL);  // actor
	
	//\__________________ get the list of windows
	gulong i, iNbWindows = 0;
	Window *pXWindowsList = cairo_dock_get_windows_list (&iNbWindows, FALSE);  // ordered by creation date; this allows us to set the correct age to the icon, which is constant. On the next updates, the z-order (which is dynamic) will be set.
	cd_debug ("got %d X windows", iNbWindows);
	
	Window Xid;
	for (i = 0; i < iNbWindows; i ++)
	{
		Xid = pXWindowsList[i];
		(void)_make_new_actor (Xid);
	}
	if (pXWindowsList != NULL)
		XFree (pXWindowsList);
	
	//\__________________ get the current active window
	if (s_iCurrentActiveWindow == 0)
		s_iCurrentActiveWindow = cairo_dock_get_active_xwindow ();
	
	//\__________________ get the current desktop/viewport
	g_desktopGeometry.iNbDesktops = cairo_dock_get_nb_desktops ();
	cairo_dock_get_nb_viewports (&g_desktopGeometry.iNbViewportX, &g_desktopGeometry.iNbViewportY);
	_cairo_dock_retrieve_current_desktop_and_viewport ();
	
	//\__________________ listen for X events
	Window root = DefaultRootWindow (s_XDisplay);
	cairo_dock_set_xwindow_mask (root, PropertyChangeMask);
	s_iSidPollXEvents = g_timeout_add (CAIRO_DOCK_CHECK_XEVENTS_INTERVAL, (GSourceFunc) _cairo_dock_unstack_Xevents, (gpointer) NULL);  // un g_idle_add () consomme 90% de CPU ! :-/
	
	//\__________________ Register backends
	GldiDesktopManagerBackend dmb;
	memset (&dmb, 0, sizeof (GldiDesktopManagerBackend));
	dmb.show_hide_desktop      = _show_hide_desktop;
	dmb.desktop_is_visible     = _desktop_is_visible;
	dmb.get_desktops_names     = _get_desktops_names;
	dmb.set_desktops_names     = _set_desktops_names;
	dmb.get_desktop_bg_surface = _get_desktop_bg_surface;
	dmb.set_current_desktop    = _set_current_desktop;
	dmb.set_nb_desktops        = _set_nb_desktops;
	gldi_desktop_manager_register_backend (&dmb);
	
	GldiWindowManagerBackend wmb;
	memset (&wmb, 0, sizeof (GldiWindowManagerBackend));
	wmb.get_active_window = _get_active_window;
	wmb.move_to_nth_desktop = _move_to_nth_desktop;
	wmb.show = _show;
	wmb.close = _close;
	wmb.kill = _kill;
	wmb.minimize = _minimize;
	wmb.lower = _lower;
	wmb.maximize = _maximize;
	wmb.set_fullscreen = _set_fullscreen;
	wmb.set_above = _set_above;
	wmb.set_minimize_position = _set_minimize_position;
	wmb.set_thumbnail_area = _set_thumbnail_area;
	wmb.set_window_border = _set_window_border;
	wmb.get_icon_surface = _get_icon_surface;
	wmb.get_thumbnail_surface = _get_thumbnail_surface;
	wmb.get_texture = _get_texture;
	wmb.get_transient_for = _get_transient_for;
	wmb.is_above_or_below = _is_above_or_below;
	wmb.is_sticky = _is_sticky;
	wmb.set_sticky = _set_sticky;
	gldi_windows_manager_register_backend (&wmb);
}


  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	GldiXWindowActor *xactor = (GldiXWindowActor*)obj;
	GldiWindowActor *actor = (GldiWindowActor*)xactor;
	Window *xid = (Window*)attr;
	Window Xid = *xid;
	
	xactor->Xid = Xid;
	
	// get additional properties
	actor->cName = cairo_dock_get_xwindow_name (Xid, TRUE);
	actor->iNumDesktop = cairo_dock_get_xwindow_desktop (Xid);
	
	int iLocalPositionX=0, iLocalPositionY=0, iWidthExtent=0, iHeightExtent=0;
	cairo_dock_get_xwindow_geometry (Xid, &iLocalPositionX, &iLocalPositionY, &iWidthExtent, &iHeightExtent);
	
	actor->iViewPortX = iLocalPositionX / g_desktopGeometry.Xscreen.width + g_desktopGeometry.iCurrentViewportX;
	actor->iViewPortY = iLocalPositionY / g_desktopGeometry.Xscreen.height + g_desktopGeometry.iCurrentViewportY;
	
	actor->windowGeometry.x = iLocalPositionX;
	actor->windowGeometry.y = iLocalPositionY;
	actor->windowGeometry.width = iWidthExtent;
	actor->windowGeometry.height = iHeightExtent;
	actor->bIsTransientFor = (xactor->XTransientFor != None);
	g_print ("%s is transient: %d\n", actor->cName, actor->bIsTransientFor);
	
	actor->iAge = s_iNumWindow ++;
	
	// get window thumbnail
	#ifdef HAVE_XEXTEND
	if (myTaskbarParam.bShowAppli && myTaskbarParam.iMinimizedWindowRenderType == 1)
	{
		XCompositeRedirectWindow (s_XDisplay, Xid, CompositeRedirectAutomatic);  // redirect the window content to the backing pixmap (the WM may or may not already do this).
		xactor->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, Xid);
		/*icon->iDamageHandle = XDamageCreate (s_XDisplay, Xid, XDamageReportNonEmpty);  // XDamageReportRawRectangles
		cd_debug ("backing pixmap : %d ; iDamageHandle : %d", icon->iBackingPixmap, icon->iDamageHandle);*/
	}
	#endif
	
	// add to table
	Window *pXid = g_new (Window, 1);
	*pXid = Xid;
	g_hash_table_insert (s_hXWindowTable, pXid, xactor);
	
	// start watching events
	cairo_dock_set_xwindow_mask (Xid, PropertyChangeMask | StructureNotifyMask);
}

static void reset_object (GldiObject *obj)
{
	g_print ("X reset\n");
	GldiXWindowActor *actor = (GldiXWindowActor*)obj;
	// stop watching events
	cairo_dock_set_xwindow_mask (actor->Xid, None);
	
	cairo_dock_set_xicon_geometry (actor->Xid, 0, 0, 0, 0);
	
	// remove from table
	if (actor->iLastCheckTime != -1)  // if not already removed
		g_hash_table_remove (s_hXWindowTable, &actor->Xid);
	
	// free data
	#ifdef HAVE_XEXTEND	
	if (actor->iBackingPixmap != 0)
	{
		XFreePixmap (s_XDisplay, actor->iBackingPixmap);
		XCompositeUnredirectWindow (s_XDisplay, actor->Xid, CompositeRedirectAutomatic);
	}
	#endif
}

void gldi_register_X_manager (void)
{
	// Manager
	memset (&myXMgr, 0, sizeof (GldiXManager));
	myXMgr.mgr.cModuleName   = "X";
	myXMgr.mgr.init          = init;
	myXMgr.mgr.load          = NULL;
	myXMgr.mgr.unload        = NULL;
	myXMgr.mgr.reload        = (GldiManagerReloadFunc)NULL;
	myXMgr.mgr.get_config    = (GldiManagerGetConfigFunc)NULL;
	myXMgr.mgr.reset_config  = (GldiManagerResetConfigFunc)NULL;
	myXMgr.mgr.init_object    = init_object;
	myXMgr.mgr.reset_object   = reset_object;
	myXMgr.mgr.iObjectSize    = sizeof (GldiXWindowActor);
	// Config
	myXMgr.mgr.pConfig = (GldiManagerConfigPtr)NULL;
	myXMgr.mgr.iSizeOfConfig = 0;
	// data
	myXMgr.mgr.iSizeOfData = 0;
	myXMgr.mgr.pData = (GldiManagerDataPtr)NULL;
	// signals
	gldi_object_install_notifications (&myXMgr, NB_NOTIFICATIONS_X_MANAGER);
	gldi_object_set_manager (GLDI_OBJECT (&myXMgr), GLDI_MANAGER (&myWindowsMgr));
	// connect to X (now, because other modules may need it for their init)
	s_XDisplay = cairo_dock_initialize_X_desktop_support ();  // renseigne la taille de l'ecran.
	// register
	gldi_register_manager (GLDI_MANAGER(&myXMgr));
}

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

#include "gldi-config.h"
#ifdef HAVE_X11

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define __USE_POSIX
#include <signal.h>
#include <sys/select.h>

#include <cairo.h>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>  // GDK_WINDOW_XID, GDK_IS_X11_DISPLAY
#endif
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/XKBlib.h>  // we should check for XkbQueryExtension...

#include "cairo-dock-utils.h"
#include "cairo-dock-log.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.iMinimizedWindowRenderType
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-class-manager.h"  // gldi_class_startup_notify_end
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-container.h"  // GldiContainerManagerBackend
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-manager.h" // gldi_docks_foreach_root
#include "cairo-dock-dock-facility.h" // gldi_dock_get_screen_offset_y
#include "cairo-dock-icon-facility.h" // cairo_dock_get_icon_container
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-task.h"
#include "cairo-dock-glx.h"
#include "cairo-dock-egl.h"
#define _MANAGER_DEF_
#include "cairo-dock-X-manager.h"

// public (manager, config, data)
GldiManager myXMgr;
GldiObjectManager myXObjectMgr;

gboolean g_bX11UseEgl = FALSE;
static gboolean _prefer_egl (void)
{
	#ifndef HAVE_EGL
		return FALSE;
	#endif
	#ifndef HAVE_GLX
		return TRUE;
	#endif
	// the command line option only makes sense if we have compiled both EGL and GLX support
	return g_bX11UseEgl;
}


// dependencies
extern GldiContainer *g_pPrimaryContainer;
//extern int g_iDamageEvent;

// private
static Display *s_XDisplay = NULL;
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
static Atom s_aNetStartupInfoBegin;
static Atom s_aNetStartupInfo;
static GHashTable *s_hXWindowTable = NULL;  // table of (Xid,actor)
static GHashTable *s_hXClientMessageTable = NULL;  // table of (Xid,client-message)
static int s_iTime = 1;  // on peut aller jusqu'a 2^31, soit 17 ans a 4Hz.
static int s_iNumWindow = 1;  // used to order appli icons by age (=creation date).
static Window s_iCurrentActiveWindow = 0;
static guint num_lock_mask=0, caps_lock_mask=0, scroll_lock_mask=0;
static GPollFD s_poll_fd;

typedef enum {
	X_DEMANDS_ATTENTION = (1<<0),
	X_URGENCY_HINT = (1 << 2)
} XAttentionFlag;

// signals
typedef enum {
	NB_NOTIFICATIONS_X_MANAGER = NB_NOTIFICATIONS_WINDOWS
	} CairoXManagerNotifications;

// data
typedef struct _GldiXWindowActor GldiXWindowActor;
struct _GldiXWindowActor {
	GldiWindowActor actor;
	// X-specific
	Window Xid;
	gint iLastCheckTime;
	Pixmap iBackingPixmap;
	Window XTransientFor;
	guint iDemandsAttention;  // a mask of XAttentionFlag
	gboolean bIgnored;
	};


static GldiXWindowActor *_make_new_actor (Window Xid)
{
	GldiXWindowActor *xactor;
	gboolean bShowInTaskbar = FALSE;
	gboolean bNormalWindow = FALSE;
	Window iTransientFor = None;
	gchar *cClass = NULL, *cWmClass = NULL, *cWmName = NULL;
	gboolean bIsHidden = FALSE, bIsFullScreen = FALSE, bIsMaximized = FALSE, bDemandsAttention = FALSE, bIsSticky = FALSE;
	
	//\__________________ see if we should skip it
	// check its 'skip taskbar' property
	bShowInTaskbar = cairo_dock_xwindow_is_fullscreen_or_hidden_or_maximized (Xid, &bIsFullScreen, &bIsHidden, &bIsMaximized, &bDemandsAttention, &bIsSticky);
	
	if (bShowInTaskbar)
	{
		// check its type
		bNormalWindow = cairo_dock_get_xwindow_type (Xid, &iTransientFor);
		if (bNormalWindow || iTransientFor != None)
		{
			// check get its class
			cClass = cairo_dock_get_xwindow_class (Xid, &cWmClass, &cWmName);
			if (cClass == NULL)
			{
				gchar *cName = cairo_dock_get_xwindow_name (Xid, TRUE);
				cd_warning ("this window (%s, %ld) doesn't belong to any class, skip it.\n"
					"Please report this bug to the application's devs.", cName, Xid);
				g_free (cName);
				bShowInTaskbar = FALSE;
			}
		}
		else
		{
			cd_debug ("unwanted type -> ignore this window");
			bShowInTaskbar = FALSE;
		}
	}
	else
	{
		XGetTransientForHint (s_XDisplay, Xid, &iTransientFor);
	}
	
	//\__________________ if the window passed all the tests, make a new actor
	if (bShowInTaskbar)  // make a new actor and fill the properties we got before
	{
		xactor = (GldiXWindowActor*)gldi_object_new (&myXObjectMgr, &Xid);
		GldiWindowActor *actor = (GldiWindowActor*)xactor;
		actor->bDisplayed = bNormalWindow;
		actor->cClass = cClass;
		actor->cWmClass = cWmClass;
		actor->cWmName = cWmName;
		actor->bIsHidden = bIsHidden;
		actor->bIsMaximized = bIsMaximized;
		actor->bIsFullScreen = bIsFullScreen;
		actor->bDemandsAttention = bDemandsAttention;
		actor->bIsSticky = bIsSticky;
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
	xactor->XTransientFor = iTransientFor;
	((GldiWindowActor*)xactor)->bIsTransientFor = (iTransientFor != None);
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

static void _on_change_desktop_geometry (gboolean bIsNetDesktopGeometry)
{
	// check if the number of viewports has changed.
	cairo_dock_get_nb_viewports (&g_desktopGeometry.iNbViewportX, &g_desktopGeometry.iNbViewportY);
	_cairo_dock_retrieve_current_desktop_and_viewport ();  // au cas ou on enleve le viewport courant.
	
	// notify everybody
	// according to LP1901507, the dock ends up on the wrong screen after the screens go to energy-save/off mode (dual-screen/nvidia).
	// as a workaround, we force the flag to true when it's a NetDesktopGeometry message, even if the size hasn't really changed
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, bIsNetDesktopGeometry);
}

static void _update_backing_pixmap (GldiXWindowActor *actor)
{
#ifdef HAVE_XEXTEND
	if (myTaskbarParam.bShowAppli && myTaskbarParam.iMinimizedWindowRenderType == 1 && cairo_dock_xcomposite_is_available ())
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
			gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
		
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
	
	// set the z-order of existing windows, and create actors for new windows
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
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_CREATED, actor);
		}
		else  // just update its check-time
			actor->iLastCheckTime = s_iTime;
		
		// update the z-order
		if (! actor->bIgnored)
			actor->actor.iStackOrder = iStackOrder ++;
	}
	
	// remove old actors for windows that disappeared
	g_hash_table_foreach_remove (s_hXWindowTable, (GHRFunc) _remove_old_applis, GINT_TO_POINTER (s_iTime));
	
	// notify everybody that the stack order has changed
	gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_Z_ORDER_CHANGED, NULL);
	
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
	gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ATTENTION_CHANGED, actor);
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
	gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ATTENTION_CHANGED, actor);
}

static void lookup_ignorable_modifiers (void)
{
	caps_lock_mask = XkbKeysymToModifiers (s_XDisplay, GDK_KEY_Caps_Lock);
	num_lock_mask = XkbKeysymToModifiers (s_XDisplay, GDK_KEY_Num_Lock);
	scroll_lock_mask = XkbKeysymToModifiers (s_XDisplay, GDK_KEY_Scroll_Lock);
}

static gboolean _cairo_dock_unstack_Xevents (G_GNUC_UNUSED gpointer data)
{
	static XEvent event;
	
	if (!g_pPrimaryContainer)  // peut arriver en cours de chargement d'un theme.
		return TRUE;
	
	Window Xid;
	Window root = DefaultRootWindow (s_XDisplay);
	
	// read the messages on the fd, and put them in the event queue
	int i, nb_msg = XEventsQueued (s_XDisplay, QueuedAfterReading);
	//g_print ("%d X msg\n", nb_msg);
	
	for (i = 0; i < nb_msg; i ++)
	{
		// get the next event in the queue
		XNextEvent (s_XDisplay, &event);
		Xid = event.xany.window;
		//g_print (" %d) type : %d; atom : %s; window : %d\n", i, event.type, XGetAtomName (s_XDisplay, event.xproperty.atom), Xid);
		
		// process the event
		if (event.type == ClientMessage)  // inter-client message
		{
			cd_debug ("+ message: %s (%ld/%ld)", XGetAtomName (s_XDisplay, event.xclient.message_type), Xid, root);
			
			// make a new message or get the existing one from previous startup events on this window
			GString *pMsg = NULL;
			if (event.xclient.message_type == s_aNetStartupInfoBegin)
			{
				if (strncmp (&event.xclient.data.b[0], "remove:", 7) == 0)  // ignore 'new:' and 'change:' messages
				{
					pMsg = g_string_sized_new (128);
					
					Window *pXid = g_new (Window, 1);
					*pXid = Xid;
					g_hash_table_insert (s_hXClientMessageTable, pXid, pMsg);
				}
			}
			else if (event.xclient.message_type == s_aNetStartupInfo)
			{
				pMsg = g_hash_table_lookup (s_hXClientMessageTable, &Xid);
			}
			
			// if a startup message is available, take it into account
			if (pMsg)
			{
				// append the new data to the message
				g_string_append_len (pMsg, &event.xclient.data.b[0], 20);
				
				// check if the messge is complete
				int i = 0;
				while (i < 20 && event.xclient.data.b[i] != '\0')
					i ++;
				
				// if it is, parse it
				if (i < 20)  // this event is the end of the message => it's complete; we should have something like: 'remove ID="id-value"'
				{
					cd_debug (" => message: %s", pMsg->str);
					// look for the ID key
					gchar *str = NULL;
					do  // look for "ID *="
					{
						str = strstr (pMsg->str, "ID");
						if (str)
						{
							str += 2;
							while (*str == ' ') str++;
							if (*str == '=')
							{
								str ++;
								break;
							}
						}
						str = NULL;
					}
					while(1);
					
					if (str)
					{
						// extract the ID value
						while (*str == ' ') str++;
						gboolean quoted = (*str == '"');
						if (quoted)
							str ++;
						gchar *id_end = str+1;
						// We can have: ID="gldi-atom\ shell-2", with a whitespace... yes
						if (quoted)
						{
							// we need to remove '\' and '"'
							gchar *withoutChar = id_end;
							while (*id_end != '\0' && *id_end != '"')
							{
								*withoutChar = *id_end;
								if (*withoutChar != '\\')
									withoutChar ++;
								id_end ++;
							}
							/* id_end should be at the '"' char but because we
							 * have moved all char if we found '\', the new end
							 * is at the position of withoutChar
							 */
							*withoutChar = '\0';
						}
						else
						{
							while (*id_end != '\0' && *id_end != ' ')
								id_end ++;
							*id_end = '\0';
						}
						cd_debug (" => ID: %s", str);

						// extract the class if it's one of our ID
						if (strncmp (str, "gldi-", 5) == 0)  // we built this ID => it has the class inside
						{
							str += 5;
							id_end = strrchr (str, '-');
							if (id_end)
								*id_end = '\0';
							cd_debug (" => class: %s", str);
							// notify the class about the end of the launching
							gldi_class_startup_notify_end (str);
						}
					}
					
					// destroy this message
					g_hash_table_remove (s_hXClientMessageTable, &Xid);
				}
			}
		}
		else if (event.type == MappingNotify)  // keymap changed (this event is always sent to all clients)
		{
			gldi_object_notify (&myDesktopMgr, NOTIFICATION_KEYMAP_CHANGED, FALSE);
			lookup_ignorable_modifiers ();
			gldi_object_notify (&myDesktopMgr, NOTIFICATION_KEYMAP_CHANGED, TRUE);
		}
		else if (Xid == root)  // event on the desktop
		{
			if (event.type == PropertyNotify)
			{
				if (event.xproperty.atom == s_aNetClientList)  // the stack order has changed: it's either because a window z-order has changed, or  a window disappeared (destroyed or hidden), or a window appeared.
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
						gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ACTIVATED, xactor && ! xactor->bIgnored ? xactor : NULL);
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
					_on_change_desktop_geometry (event.xproperty.atom == s_aNetDesktopGeometry);  // -> NOTIFICATION_DESKTOP_GEOMETRY_CHANGED
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
			else if (event.type == KeyPress)
			{
				guint event_mods = event.xkey.state & ~(num_lock_mask | caps_lock_mask | scroll_lock_mask);  // remove the lock masks
				gldi_object_notify (&myDesktopMgr, NOTIFICATION_SHORTKEY_PRESSED, event.xkey.keycode, event_mods);
			}
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
					gboolean bIsFullScreen, bIsHidden, bIsMaximized, bDemandsAttention, bIsSticky;
					gboolean bSkipTaskbar = ! cairo_dock_xwindow_is_fullscreen_or_hidden_or_maximized (Xid, &bIsFullScreen, &bIsHidden, &bIsMaximized, &bDemandsAttention, &bIsSticky);
					
					// special case where a window enters/leaves the taskbar
					if (bSkipTaskbar != xactor->bIgnored)
					{
						if (xactor->bIgnored)  // was ignored, simply recreate it
						{
							// remove it from the table, so that the XEvent loop detects it again
							g_hash_table_remove (s_hXWindowTable, &Xid);  // remove it explicitly, because the 'unref' might not free it
							xactor->iLastCheckTime = -1;
							_delete_actor (xactor);  // unref it since we don't need it anymore
						}
						else  // is now ignored
						{
							xactor->bIgnored = bSkipTaskbar;
							gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
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
					gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_STATE_CHANGED, actor, bHiddenChanged, bMaximizedChanged, bFullScreenChanged);
					
					if (actor->bIsSticky != bIsSticky)  // a change in stickyness can be seen as a change in the desktop position
					{
						actor->bIsSticky = bIsSticky;
						gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, actor);
					}
				}
				else if (event.xproperty.atom == s_aNetWmDesktop)
				{
					if (xactor->bIgnored)  // skip taskbar
						continue;
					// update the actor
					actor->iNumDesktop = cairo_dock_get_xwindow_desktop (Xid);
					
					// notify everybody
					gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, actor);
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
					gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_NAME_CHANGED, actor);
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
							gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ICON_CHANGED, actor);
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
					gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ICON_CHANGED, actor);
				}
				else if (event.xproperty.atom == s_aWmClass)
				{
					if (xactor->bIgnored)  // skip taskbar
						continue;
					// update the actor
					gchar *cOldClass = actor->cClass, *cOldWmClass = actor->cWmClass;
					gchar *cWmClass = NULL;
					gchar *cWmName = NULL;
					gchar *cNewClass = cairo_dock_get_xwindow_class (Xid, &cWmClass, &cWmName);
					if (! cNewClass || g_strcmp0 (cNewClass, cOldClass) == 0)
					{
						g_free (cWmClass);
						g_free (cWmName);
						continue;
					}
					actor->cClass = cNewClass;
					actor->cWmClass = cWmClass;
					g_free (actor->cWmName);
					actor->cWmName = cWmName;
					
					// notify everybody
					gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_CLASS_CHANGED, actor, cOldClass, cOldWmClass);
					
					g_free (cOldClass);
					g_free (cOldWmClass);
				}
			}
			else if (event.type == ConfigureNotify)
			{
				if (xactor->bIgnored)  // skip taskbar  /// TODO: don't skip if XTransientFor != 0 ?...
					continue;
				// update the actor
				int x = event.xconfigure.x, y = event.xconfigure.y;
				int w = event.xconfigure.width, h = event.xconfigure.height;
				cairo_dock_get_xwindow_geometry (Xid, &x, &y, &w, &h);
				actor->windowGeometry.width = w / cairo_dock_X_display_scale;
				actor->windowGeometry.height = h / cairo_dock_X_display_scale;
				actor->windowGeometry.x = x / cairo_dock_X_display_scale;
				actor->windowGeometry.y = y / cairo_dock_X_display_scale;
				
				actor->iViewPortX = x / gldi_desktop_get_width() + g_desktopGeometry.iCurrentViewportX;
				actor->iViewPortY = y / gldi_desktop_get_height() + g_desktopGeometry.iCurrentViewportY;
				
				if (w != actor->windowGeometry.width || h != actor->windowGeometry.height)  // size has changed
				{
					_update_backing_pixmap (xactor);
				}
				
				// notify everybody
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_SIZE_POSITION_CHANGED, actor);
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
	
	XFlush (s_XDisplay);  // now that there are no more messages in the input queue, flush the output queue
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

static void _add_workspace (void)
{
	if (g_desktopGeometry.iNbViewportX == 1 && g_desktopGeometry.iNbViewportY == 1)
		cairo_dock_set_nb_desktops (g_desktopGeometry.iNbDesktops + 1);
	else cairo_dock_change_nb_viewports (1, cairo_dock_set_nb_viewports);
}

static void _remove_workspace (void)
{
	if (g_desktopGeometry.iNbViewportX == 1 && g_desktopGeometry.iNbViewportY == 1)
	{
		// note: do not attempt to remove the last desktop
		if (g_desktopGeometry.iNbDesktops > 1)
			cairo_dock_set_nb_desktops (g_desktopGeometry.iNbDesktops - 1);
	}
	else cairo_dock_change_nb_viewports (-1, cairo_dock_set_nb_viewports);
}

static cairo_surface_t *_get_desktop_bg_surface (void)  // attention : fonction lourde.
{
	//g_print ("+++ %s ()\n", __func__);
	Pixmap iRootPixmapID = cairo_dock_get_window_background_pixmap (DefaultRootWindow (s_XDisplay));
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
				gldi_desktop_get_width(),
				gldi_desktop_get_height());
			
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
			
			if (fWidth < gldi_desktop_get_width() || fHeight < gldi_desktop_get_height())  // pattern/color gradation
			{
				cd_debug ("c'est un degrade ou un motif (%dx%d)", (int) fWidth, (int) fHeight);
				pDesktopBgSurface = cairo_dock_create_blank_surface (
					gldi_desktop_get_width(),
					gldi_desktop_get_height());
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


static void _refresh (void)
{
	g_desktopGeometry.iNbDesktops = cairo_dock_get_nb_desktops ();
	cairo_dock_get_nb_viewports (&g_desktopGeometry.iNbViewportX, &g_desktopGeometry.iNbViewportY);
	cd_debug ("desktop refresh -> %dx%dx%d", g_desktopGeometry.iNbDesktops, g_desktopGeometry.iNbViewportX, g_desktopGeometry.iNbViewportY);
}

static gboolean _grab_shortkey (guint keycode, guint modifiers, gboolean grab)
{
	Window root = DefaultRootWindow (s_XDisplay);
	
	guint mod_masks [] = {
		0, /* modifier only */
		num_lock_mask,
		caps_lock_mask,
		scroll_lock_mask,
		num_lock_mask  | caps_lock_mask,
		num_lock_mask  | scroll_lock_mask,
		caps_lock_mask | scroll_lock_mask,
		num_lock_mask  | caps_lock_mask | scroll_lock_mask,
	};  // these 3 modifiers are taken into account by X; so we need to add every possible combinations of them to the modifiers of the shortkey
	
	cairo_dock_reset_X_error_code ();
	guint i;
	for (i = 0; i < G_N_ELEMENTS (mod_masks); i++)
	{
		if (grab)
			XGrabKey (s_XDisplay,
				keycode,
				modifiers | mod_masks [i],
				root,
				False,
				GrabModeAsync,
				GrabModeAsync);
		else
			XUngrabKey (s_XDisplay,
				keycode,
				modifiers | mod_masks [i],
				root);
	}
	
	// sync with the server to get any error feedback
	XSync (s_XDisplay, False);
	int error = cairo_dock_get_X_error_code ();
	
	return (error == 0);
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

static void _set_thumbnail_area (GldiWindowActor *actor, GldiContainer* pContainer, int x, int y, int w, int h)
{
	if (pContainer->bIsHorizontal)
	{
		x += pContainer->iWindowPositionX;
		y += pContainer->iWindowPositionY;
	}
	else
	{
		x += pContainer->iWindowPositionY;
		y += pContainer->iWindowPositionX;
	}
	
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
	return cairo_dock_texture_from_pixmap (xactor->Xid, xactor->iBackingPixmap);  /// TODO: make an EGL version of this...
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

static void _set_sticky (GldiWindowActor *actor, gboolean bSticky)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_set_xwindow_sticky (xactor->Xid, bSticky);
}

static void _can_minimize_maximize_close (GldiWindowActor *actor, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	cairo_dock_xwindow_can_minimize_maximized_close (xactor->Xid, bCanMinimize, bCanMaximize, bCanClose);
}

static guint _get_id (GldiWindowActor *actor)
{
	GldiXWindowActor *xactor = (GldiXWindowActor *)actor;
	return xactor->Xid;
}

static GldiWindowActor *_pick_window (G_GNUC_UNUSED GtkWindow *pParentWindow)
{
	GldiWindowActor *actor = NULL;
	
	// let the user grab the window, and get the result.
	const char * const args[] = {"xwininfo", NULL};
	gchar *cProp = cairo_dock_launch_command_argv_sync_with_stderr (args, TRUE);
	
	// get the corresponding actor
	// look for the window ID in this chain: xwininfo: Window id: 0xc00009 "name-of-the-window"
	
	const gchar *str = g_strstr_len (cProp, -1, "Window id");  
	if (str)
	{
		str += 9;  // skip "Window id"
		while (*str == ' ' || *str == ':')  // skip the ':' and spaces
			str ++;
		Window Xid = strtol (str, NULL, 0);  // XID is an unsigned long; we let the base be 0, so that the function guesses by itself.
		
		actor = g_hash_table_lookup (s_hXWindowTable, &Xid);
	}
	g_free (cProp);
	
	return actor;
}

  /////////////////////////////////
 /// CONTAINER MANAGER BACKEND ///
/////////////////////////////////

#ifdef GDK_WINDOWING_X11
#define _gldi_container_get_Xid(pContainer) GDK_WINDOW_XID (gldi_container_get_gdk_window(pContainer))
#else
_gldi_container_get_Xid(pContainer) 0
#endif

static void _reserve_space (GldiContainer *pContainer, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x)
{
	Window Xid = _gldi_container_get_Xid (pContainer);
	cairo_dock_set_strut_partial (Xid, left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);
}

static int _get_current_desktop_index (GldiContainer *pContainer)
{
	Window Xid = _gldi_container_get_Xid (pContainer);
	
	int iDesktop = cairo_dock_get_xwindow_desktop (Xid);
	
	int iGlobalPositionX, iGlobalPositionY, iWidthExtent, iHeightExtent;
	cairo_dock_get_xwindow_geometry (Xid, &iGlobalPositionX, &iGlobalPositionY, &iWidthExtent, &iHeightExtent);  // relative to the current viewport
	if (iGlobalPositionX < 0)
		iGlobalPositionX += g_desktopGeometry.iNbViewportX * gldi_desktop_get_width() * cairo_dock_X_display_scale;
	if (iGlobalPositionY < 0)
		iGlobalPositionY += g_desktopGeometry.iNbViewportY * gldi_desktop_get_height() * cairo_dock_X_display_scale;
	
	int iViewportX = iGlobalPositionX / (gldi_desktop_get_width() * cairo_dock_X_display_scale);
	int iViewportY = iGlobalPositionY / (gldi_desktop_get_height() * cairo_dock_X_display_scale);
	
	int iCurrentDesktop, iCurrentViewportX, iCurrentViewportY;
	gldi_desktop_get_current (&iCurrentDesktop, &iCurrentViewportX, &iCurrentViewportY);
	
	iViewportX += iCurrentViewportX;
	if (iViewportX >= g_desktopGeometry.iNbViewportX)
		iViewportX -= g_desktopGeometry.iNbViewportX;
	iViewportY += iCurrentViewportY;
	if (iViewportY >= g_desktopGeometry.iNbViewportY)
		iViewportY -= g_desktopGeometry.iNbViewportY;
	//g_print ("position : %d,%d,%d / %d,%d,%d\n", iDesktop, iViewportX, iViewportY, iCurrentDesktop, iCurrentViewportX, iCurrentViewportY);
	
	return iDesktop * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY + iViewportX * g_desktopGeometry.iNbViewportY + iViewportY;
}

static void _move (GldiContainer *pContainer, int iNumDesktop, int iAbsolutePositionX, int iAbsolutePositionY)
{
	Window Xid = _gldi_container_get_Xid (pContainer);
	cairo_dock_move_xwindow_to_absolute_position (Xid, iNumDesktop,
		iAbsolutePositionX * cairo_dock_X_display_scale,
		iAbsolutePositionY * cairo_dock_X_display_scale);
}

static gboolean _is_active (GldiContainer *pContainer)
{
	Window Xid = _gldi_container_get_Xid (pContainer);
	return (Xid == s_iCurrentActiveWindow);
}

static void _present (GldiContainer *pContainer)
{
	Window Xid = _gldi_container_get_Xid (pContainer);
	cairo_dock_show_xwindow (Xid);
	//gtk_window_present_with_time (GTK_WINDOW ((pContainer)->pWidget), gdk_x11_get_server_time (gldi_container_get_gdk_window(pContainer)))  // to avoid the focus steal prevention.
}

static void _set_keep_below (GldiContainer *pContainer, gboolean bKeepBelow)
{
	gtk_window_set_keep_below (GTK_WINDOW (pContainer->pWidget), bKeepBelow);
}

static void _move_resize_dock (CairoDock *pDock)
{
	int iNewWidth = pDock->iMaxDockWidth;
	int iNewHeight = pDock->iMaxDockHeight;
	int iNewPositionX, iNewPositionY;
	cairo_dock_get_window_position_at_balance (pDock, iNewWidth, iNewHeight, &iNewPositionX, &iNewPositionY);
	/* We can't intercept the case where the new dimensions == current ones
	 * because we can have 2 resizes at the "same" time and they will cancel
	 * themselves (remove + insert of one icon). We need 2 configure otherwise
	 * the size will be blocked to the value of the first 'configure'
	 */
	// g_print (" -> %dx%d (%dx%d), %d;%d\n", iNewWidth, iNewHeight, pDock->container.iWidth, pDock->container.iHeight, iNewPositionX, iNewPositionY);

	if (pDock->container.bIsHorizontal)
	{
		gdk_window_move_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pDock)),
				iNewPositionX,
				iNewPositionY,
				iNewWidth,
				iNewHeight);
		/* When we have two gdk_window_move_resize in a row, Compiz will
		 * disturbed and it will block the draw of the dock. It seems Compiz
		 * sends too much 'configure' compare to Metacity. 
		 */
	}
	else
	{
		gdk_window_move_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pDock)),
				iNewPositionY,
				iNewPositionX,
				iNewHeight,
				iNewWidth);
	}
}

typedef struct {
	gboolean bUpToDate;
	gint x;
	gint y;
	gboolean bNoMove;
	gdouble dx;
	gdouble dy;
} CDMousePolling;

#define MOUSE_POLLING_DT 150  // mouse polling delay in ms

static void _cairo_dock_unhide_root_dock_on_mouse_hit (CairoDock *pDock, CDMousePolling *pMouse)
{
	if (! pDock->bAutoHide && pDock->iVisibility != CAIRO_DOCK_VISI_KEEP_BELOW)
		return;
	
	int iScreenWidth = gldi_dock_get_screen_width (pDock);
	int iScreenHeight = gldi_dock_get_screen_height (pDock);
	int iScreenX = gldi_dock_get_screen_offset_x (pDock);
	int iScreenY = gldi_dock_get_screen_offset_y (pDock);
	
	//\________________ On recupere la position du pointeur.
	gint x, y;
	if (! pMouse->bUpToDate)  // pas encore recupere le pointeur.
	{
		pMouse->bUpToDate = TRUE;
		GdkSeat *pSeat = gdk_display_get_default_seat (gdk_display_get_default());
		GdkDevice *pDevice = gdk_seat_get_pointer (pSeat);
		gdk_device_get_position (pDevice, NULL, &x, &y);
		if (x == pMouse->x && y == pMouse->y)  // le pointeur n'a pas bouge, on quitte.
		{
			pMouse->bNoMove = TRUE;
			return ;
		}
		pMouse->bNoMove = FALSE;
		pMouse->dx = (x - pMouse->x);
		pMouse->dy = (y - pMouse->y);
		double d = sqrt (pMouse->dx * pMouse->dx + pMouse->dy * pMouse->dy);
		pMouse->dx /= d;
		pMouse->dy /= d;
		pMouse->x = x;
		pMouse->y = y;
	}
	else  // le pointeur a ete recupere auparavant.
	{
		if (pMouse->bNoMove)  // position inchangee.
			return;
		x = pMouse->x;
		y = pMouse->y;
	}
	
	if (!pDock->container.bIsHorizontal)
	{
		x = pMouse->y;
		y = pMouse->x;
	}
	y -= iScreenY;  // relative to the border of the dock's screen.
	if (pDock->container.bDirectionUp)
	{
		y = iScreenHeight - 1 - y;
		
	}
	
	//\________________ On verifie les conditions.
	int x1, x2;  // coordinates range on the X screen edge.
	gboolean bShow = FALSE;
	int Ws = (pDock->container.bIsHorizontal ? gldi_desktop_get_width() : gldi_desktop_get_height());
	switch (myDocksParam.iCallbackMethod)
	{
		case CAIRO_HIT_SCREEN_BORDER:
		default:
			if (y != 0)
				break;
			if (x < iScreenX || x > iScreenX + iScreenWidth - 1)  // only check the border of the dock's screen.
				break ;
			bShow = TRUE;
		break;
		case CAIRO_HIT_DOCK_PLACE:
			
			if (y != 0)
				break;
			x1 = pDock->container.iWindowPositionX + (pDock->container.iWidth - pDock->iActiveWidth) * pDock->fAlign;
			x2 = x1 + pDock->iActiveWidth;
			if (x1 < 8)  // avoid corners, since this is actually the purpose of this option (corners can be used by the WM to trigger actions).
				x1 = 8;
			if (x2 > Ws - 8)
				x2 = Ws - 8;
			if (x < x1 || x > x2)
				break;
			bShow = TRUE;
		break;
		case CAIRO_HIT_SCREEN_CORNER:
			if (y != 0)
				break;
			if (x > 0 && x < Ws - 1)  // avoid the corners of the X screen (since we can't actually hit the corner of a screen that would be inside the X screen).
				break ;
			bShow = TRUE;
		break;
		case CAIRO_HIT_ZONE:
			if (y > myDocksParam.iZoneHeight)
				break;
			x1 = pDock->container.iWindowPositionX + (pDock->container.iWidth - myDocksParam.iZoneWidth) * pDock->fAlign;
			x2 = x1 + myDocksParam.iZoneWidth;
			if (x < x1 || x > x2)
				break;
			bShow = TRUE;
		break;
	}
	if (! bShow)
	{
		if (pDock->iSidUnhideDelayed != 0)
		{
			g_source_remove (pDock->iSidUnhideDelayed);
			pDock->iSidUnhideDelayed = 0;
		}
		return;
	}
	
	//\________________ On montre ou on programme le montrage du dock.
	int nx, ny;  // normal vector to the screen edge.
	double cost;  // cos (teta), where teta = angle between mouse vector and dock's normal
	double f = 1.;  // delay factor
	if (pDock->container.bIsHorizontal)
	{
		nx = 0;
		ny = (pDock->container.bDirectionUp ? -1 : 1);
	}
	else
	{
		ny = 0;
		nx = (pDock->container.bDirectionUp ? -1 : 1);
	}
	cost = nx * pMouse->dx + ny * pMouse->dy;
	f = 2 + cost;  // so if cost = -1, we arrive straight onto the screen edge, and f = 1, => normal delay. if cost = 0, f = 2 and we have a bigger delay.
	
	int iDelay = f * myDocksParam.iUnhideDockDelay;
	//g_print (" dock will be shown in %dms (%.2f, %d)\n", iDelay, f, pDock->bIsMainDock);
	cairo_dock_unhide_dock_delayed (pDock, iDelay);
}


static gboolean _cairo_dock_poll_screen_edge (G_GNUC_UNUSED gpointer data)  // thanks to Smidgey for the pop-up patch !
{
	static CDMousePolling mouse;
	
	// if the active window is full screen, avoid showing the docks on edge hit
	// some WM will show the dock on top of fullscreen windows, and it's a problem in case of games, for instance
	GldiWindowActor *actor = gldi_windows_get_active();
	if (actor && actor->bIsFullScreen)
		return TRUE;

	mouse.bUpToDate = FALSE;  // mouse position will be updated by the first hidden dock.
	gldi_docks_foreach_root ((GFunc) _cairo_dock_unhide_root_dock_on_mouse_hit, &mouse);
	
	return TRUE;
}

static guint s_iSidPollScreenEdge = 0;
static gboolean s_bShouldPoll = FALSE;

static void _check_should_poll_screen_edge (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->bAutoHide == TRUE) s_bShouldPoll = TRUE;
	else switch (pDock->iVisibility)
	{
		case CAIRO_DOCK_VISI_KEEP_BELOW:
		case CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP:
		case CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY:
		case CAIRO_DOCK_VISI_AUTO_HIDE:
			s_bShouldPoll = TRUE;
			break;
		default:
			break;
	}
}

static void _update_polling_screen_edge (void)
{
	s_bShouldPoll = FALSE;
	gldi_docks_foreach_root ((GFunc) _check_should_poll_screen_edge, NULL);
	if (s_bShouldPoll)
	{
		if (s_iSidPollScreenEdge == 0)
			s_iSidPollScreenEdge = g_timeout_add (MOUSE_POLLING_DT, (GSourceFunc) _cairo_dock_poll_screen_edge, NULL);
	}
	else
	{
		g_source_remove (s_iSidPollScreenEdge);
		s_iSidPollScreenEdge = 0;
	}
}

static inline gboolean _has_multiple_screens_and_on_one_screen(int iNumScreen) {
	return (g_desktopGeometry.iNbScreens > 1) && (iNumScreen > -1);
}

static gboolean _can_reserve_space (int iNumScreen, gboolean bDirectionUp, gboolean bIsHorizontal)
{
	// code moved here from cairo-dock-dock-facility.c, since it is only relevant in the X11 case
	if (!_has_multiple_screens_and_on_one_screen (iNumScreen)) return TRUE;
	if (bDirectionUp)
	{
		if (bIsHorizontal)
		{
			if (cairo_dock_get_screen_position_y (iNumScreen) // y offset
					+ cairo_dock_get_screen_height (iNumScreen)  // height of the current screen
					< gldi_desktop_get_height ())
				return FALSE;
		}
		else
		{
			if (cairo_dock_get_screen_position_x (iNumScreen) // x offset
					+ cairo_dock_get_screen_width (iNumScreen)  // width of the current screen
					< gldi_desktop_get_width ()) // total width
				return FALSE;
		}
	}
	else
	{
		if (bIsHorizontal)
		{
			if (cairo_dock_get_screen_position_y (iNumScreen) > 0) return FALSE;
		}
		else
		{
			if (cairo_dock_get_screen_position_x (iNumScreen) > 0) return FALSE;
		}
	}
	return TRUE;
}

static void _update_mouse_position (GldiContainer *pContainer)
{
	GdkSeat *pSeat = gdk_display_get_default_seat (gdk_display_get_default());
	GdkDevice *pDevice = gdk_seat_get_pointer (pSeat);
	if ((pContainer)->bIsHorizontal)
		gdk_window_get_device_position (gldi_container_get_gdk_window (pContainer), pDevice, &pContainer->iMouseX, &pContainer->iMouseY, NULL);
	else
		gdk_window_get_device_position (gldi_container_get_gdk_window (pContainer), pDevice, &pContainer->iMouseY, &pContainer->iMouseX, NULL);
}

static gboolean _dock_handle_leave (CairoDock *pDock, G_GNUC_UNUSED GdkEventCrossing *pEvent)
{
	// this function is just the _mouse_is_really_outside () function from cairo-dock-dock-factory.c
	// this check only makes sense on X11
	int x1, x2, y1, y2;
	if (pDock->iInputState == CAIRO_DOCK_INPUT_ACTIVE)
	{
		x1 = (pDock->container.iWidth - pDock->iActiveWidth) * pDock->fAlign;
		x2 = x1 + pDock->iActiveWidth;
		if (pDock->container.bDirectionUp)
		{
			y1 = pDock->container.iHeight - pDock->iActiveHeight + 1;
			y2 = pDock->container.iHeight;
		}
		else
		{
			y1 = 0;
			y2 = pDock->iActiveHeight - 1;
		}
	}
	else if (pDock->iInputState == CAIRO_DOCK_INPUT_AT_REST)
	{
		x1 = (pDock->container.iWidth - pDock->iMinDockWidth) * pDock->fAlign;
		x2 = x1 + pDock->iMinDockWidth;
		if (pDock->container.bDirectionUp)
		{
			y1 = pDock->container.iHeight - pDock->iMinDockHeight + 1;
			y2 = pDock->container.iHeight;
		}
		else
		{
			y1 = 0;
			y2 = pDock->iMinDockHeight - 1;
		}		
	}
	else  // hidden
		return TRUE;
	if (pDock->container.iMouseX <= x1
	|| pDock->container.iMouseX >= x2)
		return TRUE;
	if (pDock->container.iMouseY < y1
	|| pDock->container.iMouseY > y2)  // Note: Compiz has a bug: when using the "cube rotation" plug-in, it will reserve 2 pixels for itself on the left and right edges of the screen. So the mouse is not inside the dock when it's at x=0 or x=Ws-1 (no 'enter' event is sent; it's as if the x=0 or x=Ws-1 vertical line of pixels is out of the screen).
		return TRUE;

	return FALSE;
}

// check if the mouse is inside the dock and update iMousePositionType
// moved from cairo-dock-dock-facility.c
void _dock_check_if_mouse_inside_linear (CairoDock *pDock)
{
	CairoDockMousePositionType iMousePositionType;
	int iWidth = pDock->container.iWidth;
	///int iHeight = (pDock->fMagnitudeMax != 0 ? pDock->container.iHeight : pDock->iMinDockHeight);
	int iHeight = pDock->iActiveHeight;
	///int iExtraHeight = (pDock->bAtBottom ? 0 : myIconsParam.iLabelSize);
	// int iExtraHeight = 0;  /// we should check if we have a sub-dock or a dialogue on top of it :-/
	int iMouseX = pDock->container.iMouseX;
	int iMouseY = (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->container.iMouseY : pDock->container.iMouseY);
	//g_print ("%s (%dx%d, %dx%d, %f)\n", __func__, iMouseX, iMouseY, iWidth, iHeight, pDock->fFoldingFactor);

	//\_______________ We check if the cursor is in the dock and we change icons size according to that.
	double offset = (iWidth - pDock->iActiveWidth) * pDock->fAlign + (pDock->iActiveWidth - pDock->fFlatDockWidth) / 2;
	int x_abs = pDock->container.iMouseX - offset;
	///int x_abs = pDock->container.iMouseX + (pDock->fFlatDockWidth - iWidth) * pDock->fAlign;  // abscisse par rapport a la gauche du dock minimal plat.
	gboolean bMouseInsideDock = (x_abs >= 0 && x_abs <= pDock->fFlatDockWidth && iMouseX > 0 && iMouseX < iWidth);
	//g_print ("bMouseInsideDock : %d (%d;%d/%.2f)\n", bMouseInsideDock, pDock->container.bInside, x_abs, pDock->fFlatDockWidth);

	if (iMouseY >= 0 && iMouseY < iHeight) { // inside in the Y axis
		if (! bMouseInsideDock)  // outside of the dock but on the edge.
			iMousePositionType = CAIRO_DOCK_MOUSE_ON_THE_EDGE;
		else
			iMousePositionType = CAIRO_DOCK_MOUSE_INSIDE;
	}
	else
		iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;

	pDock->iMousePositionType = iMousePositionType;
}

static void _adjust_aimed_point (const Icon *pIcon, GtkWidget *pWidget,
	G_GNUC_UNUSED int w, G_GNUC_UNUSED int h, G_GNUC_UNUSED int iMarginPosition,
	G_GNUC_UNUSED gdouble fAlign, int *iAimedX, int *iAimedY)
{
	// we adjust iAimedX and iAimedY to use global coordinates
	GldiContainer *pContainer = (pIcon ? cairo_dock_get_icon_container (pIcon) : NULL);
	if (pIcon && pContainer && CAIRO_DOCK_IS_DOCK (pContainer))
	{
		// in this case, position is relative to pContainer
		if (pContainer->bIsHorizontal)
		{
			*iAimedX += pContainer->iWindowPositionX;
			*iAimedY += pContainer->iWindowPositionY;
		}
		else
		{
			*iAimedX += pContainer->iWindowPositionY;
			*iAimedY += pContainer->iWindowPositionX;
		}
	}
	else
	{
		// in this case, position is relative to pWidget
		int x, y;
		gdk_window_get_position (gtk_widget_get_window (gtk_widget_get_toplevel (pWidget)), &x, &y);
		*iAimedX += x;
		*iAimedY += y;
	}
}

unsigned long gldi_X_manager_get_window_xid (GldiWindowActor *actor)
{
	if (gldi_object_is_manager_child (GLDI_OBJECT(actor), &myXObjectMgr))
	{
		GldiXWindowActor *xactor = (GldiXWindowActor*)actor;
		return xactor->Xid; // Window defined as unsigned long or unsigned int in X.h or Xmd.h
	}
	return 0; // None
}

  ////////////
 /// INIT ///
////////////

static void _string_free (GString *pString)
{
	g_string_free (pString, TRUE);
}

static gboolean _prepare (G_GNUC_UNUSED GSource *source, gint *timeout)
{
	*timeout = -1;  // no timeout for poll()
	return (g_pPrimaryContainer && XEventsQueued (s_XDisplay, QueuedAlready));  // if some events are already in the queue, dispatch them, otherwise poll; if no container yet (maintenance mode or opengl question), keep the event in the queue for later (otherwise it would block the main loop and prevent gtk to draw the widgets)
}
static gboolean _check (G_GNUC_UNUSED GSource *source)
{
	return (g_pPrimaryContainer && (s_poll_fd.revents & G_IO_IN));  // if the fd has something to tell us, go dispatch the events; if no container yet (maintenance mode or opengl question), keep the message in the socket for later (otherwise it would block the main loop and prevent gtk to draw the widgets)
}
static gboolean _dispatch (G_GNUC_UNUSED GSource *source, G_GNUC_UNUSED GSourceFunc callback, G_GNUC_UNUSED gpointer user_data)
{
	return _cairo_dock_unstack_Xevents (NULL);
}


static void init (void)
{
	//\__________________ connect to X
	s_XDisplay = cairo_dock_initialize_X_desktop_support ();  // renseigne la taille de l'ecran.
	
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
	s_aNetStartupInfoBegin 	= XInternAtom (s_XDisplay, "_NET_STARTUP_INFO_BEGIN", False);
	s_aNetStartupInfo 		= XInternAtom (s_XDisplay, "_NET_STARTUP_INFO", False);
	
	s_hXWindowTable = g_hash_table_new_full (g_int_hash,
		g_int_equal,
		g_free,  // Xid
		NULL);  // actor
	
	s_hXClientMessageTable = g_hash_table_new_full (g_int_hash,
		g_int_equal,
		g_free,  // Xid
		(GDestroyNotify)_string_free);  // GString
	
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
	cairo_dock_set_xwindow_mask (root, PropertyChangeMask | KeyPressMask);
	
	static GSourceFuncs source_funcs;
	memset (&source_funcs,0, sizeof (GSourceFuncs));
	source_funcs.prepare = _prepare;
	source_funcs.check = _check;
	source_funcs.dispatch = _dispatch;
	source_funcs.finalize = NULL;
	
	GSource *source = g_source_new (&source_funcs, sizeof(GSource));
	s_poll_fd.fd = ConnectionNumber (s_XDisplay);
	s_poll_fd.events = G_IO_IN;
	g_source_add_poll (source, &s_poll_fd);
	g_source_attach (source, NULL);  // NULL <-> main context
	
	//\__________________ Register backends
	GldiDesktopManagerBackend dmb;
	memset (&dmb, 0, sizeof (GldiDesktopManagerBackend));
	dmb.show_hide_desktop      = _show_hide_desktop;
	dmb.desktop_is_visible     = _desktop_is_visible;
	dmb.get_desktops_names     = _get_desktops_names;
	dmb.set_desktops_names     = _set_desktops_names;
	dmb.get_desktop_bg_surface = _get_desktop_bg_surface;
	dmb.set_current_desktop    = _set_current_desktop;
	dmb.refresh                = _refresh;
	dmb.grab_shortkey          = _grab_shortkey;
	dmb.add_workspace          = _add_workspace;
	dmb.remove_last_workspace  = _remove_workspace;
	gldi_desktop_manager_register_backend (&dmb, "X11");
	
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
	wmb.set_thumbnail_area = _set_thumbnail_area;
	wmb.set_window_border = _set_window_border;
	wmb.get_icon_surface = _get_icon_surface;
	wmb.get_thumbnail_surface = _get_thumbnail_surface;
	wmb.get_texture = _get_texture;
	wmb.get_transient_for = _get_transient_for;
	wmb.is_above_or_below = _is_above_or_below;
	wmb.set_sticky = _set_sticky;
	wmb.can_minimize_maximize_close = _can_minimize_maximize_close;
	wmb.get_id = _get_id;
	wmb.pick_window = _pick_window;
	//!! TODO: figure out GLDI_WM_NO_VIEWPORT_OVERLAP flag (depends on the WM, needs to be done in *-integration.c) !!
	wmb.flags = GINT_TO_POINTER (GLDI_WM_HAVE_WINDOW_GEOMETRY | GLDI_WM_HAVE_WORKSPACES);
	wmb.name = "X11";
	gldi_windows_manager_register_backend (&wmb);
	
	GldiContainerManagerBackend cmb;
	memset (&cmb, 0, sizeof (GldiContainerManagerBackend));
	cmb.reserve_space = _reserve_space;
	cmb.get_current_desktop_index = _get_current_desktop_index;
	cmb.move = _move;
	cmb.is_active = _is_active;
	cmb.present = _present;
	cmb.set_keep_below = _set_keep_below;
	cmb.move_resize_dock = _move_resize_dock;
	cmb.update_polling_screen_edge = _update_polling_screen_edge;
	cmb.can_reserve_space = _can_reserve_space;
	cmb.update_mouse_position = _update_mouse_position;
	cmb.dock_handle_leave = _dock_handle_leave;
	cmb.dock_check_if_mouse_inside_linear = _dock_check_if_mouse_inside_linear;
	cmb.adjust_aimed_point = _adjust_aimed_point;
	gldi_container_manager_register_backend (&cmb);
	
	if (_prefer_egl ()) gldi_register_egl_backend ();
	else gldi_register_glx_backend ();
	
	//\__________________ get modifiers we want to filter
	lookup_ignorable_modifiers ();
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
	
	iLocalPositionX /= cairo_dock_X_display_scale;
	iLocalPositionY /= cairo_dock_X_display_scale;
	
	actor->iViewPortX = iLocalPositionX / g_desktopGeometry.Xscreen.width + g_desktopGeometry.iCurrentViewportX;
	actor->iViewPortY = iLocalPositionY / g_desktopGeometry.Xscreen.height + g_desktopGeometry.iCurrentViewportY;
	
	actor->windowGeometry.x = iLocalPositionX;
	actor->windowGeometry.y = iLocalPositionY;
	actor->windowGeometry.width = iWidthExtent / cairo_dock_X_display_scale;
	actor->windowGeometry.height = iHeightExtent / cairo_dock_X_display_scale;
	
	actor->iAge = s_iNumWindow;
	if (s_iNumWindow == INT_MAX) s_iNumWindow = 1;
	else s_iNumWindow++;
	
	// get window thumbnail
	#ifdef HAVE_XEXTEND
	if (myTaskbarParam.bShowAppli && myTaskbarParam.iMinimizedWindowRenderType == 1 && cairo_dock_xcomposite_is_available ())
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
	GldiXWindowActor *actor = (GldiXWindowActor*)obj;
	cd_debug ("X reset: %s", ((GldiWindowActor*)actor)->cName);
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
	// check if we're in an X session
	#ifdef GDK_WINDOWING_X11  // if GTK doesn't support X, there is no point in trying
	GdkDisplay *dsp = gdk_display_get_default ();  // let's GDK do the guess
	if (! GDK_IS_X11_DISPLAY (dsp))  // if not an X session
	#endif
	{
		cd_message ("Not an X session");
		return;
	}
	
	// Manager
	memset (&myXMgr, 0, sizeof (GldiManager));
	myXMgr.cModuleName   = "X";
	myXMgr.init          = init;
	myXMgr.load          = NULL;
	myXMgr.unload        = NULL;
	myXMgr.reload        = (GldiManagerReloadFunc)NULL;
	myXMgr.get_config    = (GldiManagerGetConfigFunc)NULL;
	myXMgr.reset_config  = (GldiManagerResetConfigFunc)NULL;
	// Config
	myXMgr.pConfig = (GldiManagerConfigPtr)NULL;
	myXMgr.iSizeOfConfig = 0;
	// data
	myXMgr.iSizeOfData = 0;
	myXMgr.pData = (GldiManagerDataPtr)NULL;
	// register
	gldi_object_init (GLDI_OBJECT(&myXMgr), &myManagerObjectMgr, NULL);
	
	// Object Manager
	memset (&myXObjectMgr, 0, sizeof (GldiObjectManager));
	myXObjectMgr.cName   = "X";
	myXObjectMgr.iObjectSize    = sizeof (GldiXWindowActor);
	// interface
	myXObjectMgr.init_object    = init_object;
	myXObjectMgr.reset_object   = reset_object;
	// signals
	gldi_object_install_notifications (&myXObjectMgr, NB_NOTIFICATIONS_X_MANAGER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myXObjectMgr), &myWindowObjectMgr);
}

#else
#include "cairo-dock-log.h"
void gldi_register_X_manager (void)
{
	cd_message ("Cairo-Dock was not built with X support");
}
#endif

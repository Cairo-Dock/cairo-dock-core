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

#include "cairo-dock-container.h"  // CAIRO_DOCK_HORIZONTAL
#include "cairo-dock-notifications.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-desklet-manager.h"  // cairo_dock_foreach_desklet
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dock-manager.h"  // cairo_dock_reposition_root_docks
#include "cairo-dock-class-manager.h"  // cairo_dock_initialize_class_manager
#include "cairo-dock-draw-opengl.h"  // cairo_dock_create_texture_from_surface
#include "cairo-dock-compiz-integration.h"
#include "cairo-dock-kwin-integration.h"
#include "cairo-dock-gnome-shell-integration.h"
#define _MANAGER_DEF_
#include "cairo-dock-X-manager.h"

#define CAIRO_DOCK_CHECK_XEVENTS_INTERVAL 200

// public (manager, config, data)
CairoDockDesktopManager myDesktopMgr;
CairoDockDesktopGeometry g_desktopGeometry;

// dependancies
extern CairoContainer *g_pPrimaryContainer;
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
static Atom s_aXKlavierState;
static CairoDockDesktopBackground *s_pDesktopBg = NULL;  // une fois alloue, le pointeur restera le meme tout le temps.
static CairoDockWMBackend *s_pWMBackend = NULL;

static void _cairo_dock_reload_desktop_background (void);

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
	cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	
	// on gere le cas delicat de X qui nous fait sortir du dock plus tard.
	return FALSE;
}

static void _on_change_nb_desktops (void)
{
	g_desktopGeometry.iNbDesktops = cairo_dock_get_nb_desktops ();
	_cairo_dock_retrieve_current_desktop_and_viewport ();  // au cas ou on enleve le bureau courant.
	
	cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_SCREEN_GEOMETRY_ALTERED);
}

static void _on_change_desktop_geometry (void)
{
	// check if the resolution has changed
	if (cairo_dock_update_screen_geometry ())  // resolution has changed => replace the docks.
	{
		cd_message ("resolution alteree");
		
		cairo_dock_reposition_root_docks (FALSE);  // main dock compris. Se charge de Xinerama.
	}
	
	// check if the number of viewports has changed.
	cairo_dock_get_nb_viewports (&g_desktopGeometry.iNbViewportX, &g_desktopGeometry.iNbViewportY);
	_cairo_dock_retrieve_current_desktop_and_viewport ();  // au cas ou on enleve le viewport courant.
	
	// notify everybody
	cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_SCREEN_GEOMETRY_ALTERED);
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
		//if (event.type == ClientMessage)
		//cd_message ("\n\n\n >>>>>>>>>>>< event.type : %d\n", event.type);
		if (Xid == root)
		{
			if (event.type == PropertyNotify)  // PropertyNotify sur root
			{
				if (event.xproperty.atom == s_aNetClientList)
				{
					cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_WINDOW_CONFIGURED, Xid, NULL);
				}
				else if (event.xproperty.atom == s_aNetActiveWindow)
				{
					Window XActiveWindow = cairo_dock_get_active_xwindow ();
					cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_WINDOW_ACTIVATED, &XActiveWindow);
				}
				else if (event.xproperty.atom == s_aNetCurrentDesktop || event.xproperty.atom == s_aNetDesktopViewport)
				{
					_on_change_current_desktop_viewport ();  // -> NOTIFICATION_DESKTOP_CHANGED
				}
				else if (event.xproperty.atom == s_aNetNbDesktops)
				{
					_on_change_nb_desktops ();  // -> NOTIFICATION_SCREEN_GEOMETRY_ALTERED
				}
				else if (event.xproperty.atom == s_aNetDesktopGeometry || event.xproperty.atom == s_aNetWorkarea)  // check s_aNetWorkarea too, to workaround a bug in Compiz (or X?) : when down-sizing the screen, the _NET_DESKTOP_GEOMETRY atom is not received  (up-sizing is ok though, and changing the viewport makes the atom to be received); but _NET_WORKAREA is correctly sent; since it's only sent when the resolution is changed, or the dock's height (if space is reserved), it's not a big overload to check it too.
				{
					_on_change_desktop_geometry ();  // -> NOTIFICATION_SCREEN_GEOMETRY_ALTERED
				}
				else if (event.xproperty.atom == s_aRootMapID)
				{
					cd_debug ("change wallpaper");
					_cairo_dock_reload_desktop_background ();
					cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_SCREEN_GEOMETRY_ALTERED);
				}
				else if (event.xproperty.atom == s_aNetShowingDesktop)
				{
					cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_DESKTOP_VISIBILITY_CHANGED);
				}
				else if (event.xproperty.atom == s_aXKlavierState)
				{
					cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_KBD_STATE_CHANGED, NULL);
				}
			}  // fin de PropertyNotify sur root.
		}
		else  // evenement sur une fenetre.
		{
			if (event.type == PropertyNotify)  // PropertyNotify sur une fenetre
			{
				if (event.xproperty.atom == s_aXKlavierState)
				{
					cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_KBD_STATE_CHANGED, &Xid);
				}
				else
				{
					cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_WINDOW_PROPERTY_CHANGED, Xid, event.xproperty.atom, event.xproperty.state);
				}
			}
			else if (event.type == ConfigureNotify)  // ConfigureNotify sur une fenetre.
			{
				cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_WINDOW_CONFIGURED, Xid, &event.xconfigure);
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
		}  // fin d'evenement sur une fenetre.
	}
	if (XEventsQueued (s_XDisplay, QueuedAlready) != 0)
		XSync (s_XDisplay, True);  // True <=> discard.
	//g_print ("XEventsQueued : %d\n", XEventsQueued (s_XDisplay, QueuedAfterFlush));  // QueuedAlready, QueuedAfterReading, QueuedAfterFlush
	
	return TRUE;
}


  ////////////////////////
 /// X desktop access ///
////////////////////////

void cairo_dock_get_current_desktop_and_viewport (int *iCurrentDesktop, int *iCurrentViewportX, int *iCurrentViewportY)
{
	*iCurrentDesktop = g_desktopGeometry.iCurrentDesktop;
	*iCurrentViewportX = g_desktopGeometry.iCurrentViewportX;
	*iCurrentViewportY = g_desktopGeometry.iCurrentViewportY;
}


  //////////////////////////////
 /// WINDOW MANAGER BACKEND ///
//////////////////////////////

static gboolean _set_desklets_on_widget_layer (CairoDesklet *pDesklet, G_GNUC_UNUSED gpointer data)
{
	Window Xid = gldi_container_get_Xid (CAIRO_CONTAINER (pDesklet));
	if (pDesklet->iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER)
		cairo_dock_wm_set_on_widget_layer (Xid, TRUE);
	return FALSE;  // continue
}
void cairo_dock_wm_register_backend (CairoDockWMBackend *pBackend)
{
	g_free (s_pWMBackend);
	s_pWMBackend = pBackend;
	
	cd_debug ("new WM backend (%x)", pBackend);
	// since we have a backend, set up the desklets that are supposed to be on the widget layer.
	if (pBackend && pBackend->set_on_widget_layer != NULL)
	{
		cairo_dock_foreach_desklet ((CairoDockForeachDeskletFunc) _set_desklets_on_widget_layer, NULL);
	}
}

gboolean cairo_dock_wm_present_class (const gchar *cClass)  // scale matching class
{
	g_return_val_if_fail (cClass != NULL, FALSE);
	if (s_pWMBackend != NULL && s_pWMBackend->present_class != NULL)
	{
		return s_pWMBackend->present_class (cClass);
	}
	return FALSE;
}

gboolean cairo_dock_wm_present_windows (void)  // scale
{
	if (s_pWMBackend != NULL && s_pWMBackend->present_windows != NULL)
	{
		return s_pWMBackend->present_windows ();
	}
	return FALSE;
}

gboolean cairo_dock_wm_present_desktops (void)  // expose
{
	if (s_pWMBackend != NULL && s_pWMBackend->present_desktops != NULL)
	{
		return s_pWMBackend->present_desktops ();
	}
	return FALSE;
}

gboolean cairo_dock_wm_show_widget_layer (void)  // widget
{
	if (s_pWMBackend != NULL && s_pWMBackend->show_widget_layer != NULL)
	{
		return s_pWMBackend->show_widget_layer ();
	}
	return FALSE;
}

gboolean cairo_dock_wm_set_on_widget_layer (Window Xid, gboolean bOnWidgetLayer)
{
	if (s_pWMBackend != NULL && s_pWMBackend->set_on_widget_layer != NULL)
	{
		return s_pWMBackend->set_on_widget_layer (Xid, bOnWidgetLayer);
	}
	return FALSE;
}

gboolean cairo_dock_wm_can_present_class (void)
{
	return (s_pWMBackend != NULL && s_pWMBackend->present_class != NULL);
}

gboolean cairo_dock_wm_can_present_windows (void)
{
	return (s_pWMBackend != NULL && s_pWMBackend->present_windows != NULL);
}

gboolean cairo_dock_wm_can_present_desktops (void)
{
	return (s_pWMBackend != NULL && s_pWMBackend->present_desktops != NULL);
}

gboolean cairo_dock_wm_can_show_widget_layer (void)
{
	return (s_pWMBackend != NULL && s_pWMBackend->show_widget_layer != NULL);
}

gboolean cairo_dock_wm_can_set_on_widget_layer (void)
{
	return (s_pWMBackend != NULL && s_pWMBackend->set_on_widget_layer != NULL);
}


  //////////////////
 /// DESKTOP BG ///
//////////////////
static cairo_surface_t *_cairo_dock_create_surface_from_desktop_bg (void)  // attention : fonction lourde.
{
	//g_print ("+++ %s ()\n", __func__);
	Pixmap iRootPixmapID = cairo_dock_get_window_background_pixmap (cairo_dock_get_root_id ());
	g_return_val_if_fail (iRootPixmapID != 0, NULL);  // Note: depending on the WM, iRootPixmapID might be 0, and a window of type 'Desktop' might be used instead (covering the whole screen). We don't handle this case, as I've never encounterd it yet.
	
	cairo_surface_t *pDesktopBgSurface = NULL;
	GdkPixbuf *pBgPixbuf = cairo_dock_get_pixbuf_from_pixmap (iRootPixmapID, FALSE);  // FALSE <=> on n'y ajoute pas de transparence.
	if (pBgPixbuf != NULL)
	{
		if (gdk_pixbuf_get_height (pBgPixbuf) == 1 && gdk_pixbuf_get_width (pBgPixbuf) == 1)  // couleur unie.
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
			
			if (fWidth < gldi_get_desktop_width() || fHeight < gldi_get_desktop_height())
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
			else
			{
				cd_debug ("c'est un fond d'ecran de taille %dx%d", (int) fWidth, (int) fHeight);
				pDesktopBgSurface = pBgSurface;
			}
		}
		
		g_object_unref (pBgPixbuf);
	}
	return pDesktopBgSurface;
}

CairoDockDesktopBackground *cairo_dock_get_desktop_background (gboolean bWithTextureToo)
{
	//g_print ("%s (%d, %d)\n", __func__, bWithTextureToo, s_pDesktopBg?s_pDesktopBg->iRefCount:-1);
	if (s_pDesktopBg == NULL)
	{
		s_pDesktopBg = g_new0 (CairoDockDesktopBackground, 1);
	}
	if (s_pDesktopBg->pSurface == NULL)
	{
		s_pDesktopBg->pSurface = _cairo_dock_create_surface_from_desktop_bg ();
	}
	if (s_pDesktopBg->iTexture == 0 && bWithTextureToo)
	{
		s_pDesktopBg->iTexture = cairo_dock_create_texture_from_surface (s_pDesktopBg->pSurface);
	}
	
	s_pDesktopBg->iRefCount ++;
	if (s_pDesktopBg->iSidDestroyBg != 0)
	{
		//g_print ("cancel pending destroy\n");
		g_source_remove (s_pDesktopBg->iSidDestroyBg);
		s_pDesktopBg->iSidDestroyBg = 0;
	}
	return s_pDesktopBg;
}

static gboolean _destroy_bg (CairoDockDesktopBackground *pDesktopBg)
{
	//g_print ("%s ()\n", __func__);
	g_return_val_if_fail (pDesktopBg != NULL, 0);
	if (pDesktopBg->pSurface != NULL)
	{
		cairo_surface_destroy (pDesktopBg->pSurface);
		pDesktopBg->pSurface = NULL;
		//g_print ("--- surface destroyed\n");
	}
	if (pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (pDesktopBg->iTexture);
		pDesktopBg->iTexture = 0;
	}
	pDesktopBg->iSidDestroyBg = 0;
	return FALSE;
}
void cairo_dock_destroy_desktop_background (CairoDockDesktopBackground *pDesktopBg)
{
	//g_print ("%s ()\n", __func__);
	g_return_if_fail (pDesktopBg != NULL);
	if (pDesktopBg->iRefCount > 0)
		pDesktopBg->iRefCount --;
	if (pDesktopBg->iRefCount == 0 && pDesktopBg->iSidDestroyBg == 0)
	{
		//g_print ("add pending destroy\n");
		pDesktopBg->iSidDestroyBg = g_timeout_add_seconds (3, (GSourceFunc)_destroy_bg, pDesktopBg);
	}
}

cairo_surface_t *cairo_dock_get_desktop_bg_surface (CairoDockDesktopBackground *pDesktopBg)
{
	g_return_val_if_fail (pDesktopBg != NULL, NULL);
	return pDesktopBg->pSurface;
}

GLuint cairo_dock_get_desktop_bg_texture (CairoDockDesktopBackground *pDesktopBg)
{
	g_return_val_if_fail (pDesktopBg != NULL, 0);
	return pDesktopBg->iTexture;
}

static void _cairo_dock_reload_desktop_background (void)
{
	//g_print ("%s ()\n", __func__);
	if (s_pDesktopBg == NULL)  // rien a recharger.
		return ;
	if (s_pDesktopBg->pSurface == NULL && s_pDesktopBg->iTexture == 0)  // rien a recharger.
		return ;
	
	if (s_pDesktopBg->pSurface != NULL)
	{
		cairo_surface_destroy (s_pDesktopBg->pSurface);
		//g_print ("--- surface destroyed\n");
	}
	s_pDesktopBg->pSurface = _cairo_dock_create_surface_from_desktop_bg ();
	
	if (s_pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (s_pDesktopBg->iTexture);
		s_pDesktopBg->iTexture = cairo_dock_create_texture_from_surface (s_pDesktopBg->pSurface);
	}
}


  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	/*if (s_pDesktopBg != NULL && s_pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (s_pDesktopBg->iTexture);
		s_pDesktopBg->iTexture = 0;
	}*/
	if (s_pDesktopBg)  // on decharge le desktop-bg de force.
	{
		if (s_pDesktopBg->iSidDestroyBg != 0)
		{
			g_source_remove (s_pDesktopBg->iSidDestroyBg);
			s_pDesktopBg->iSidDestroyBg = 0;
		}
		s_pDesktopBg->iRefCount = 0;
		_destroy_bg (s_pDesktopBg);  // detruit ses ressources immediatement, mais pas le pointeur.
	}
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	//\__________________ On initialise le support de X.
	s_aNetClientList		= XInternAtom (s_XDisplay, "_NET_CLIENT_LIST_STACKING", False);
	s_aNetActiveWindow		= XInternAtom (s_XDisplay, "_NET_ACTIVE_WINDOW", False);
	s_aNetCurrentDesktop	= XInternAtom (s_XDisplay, "_NET_CURRENT_DESKTOP", False);
	s_aNetDesktopViewport	= XInternAtom (s_XDisplay, "_NET_DESKTOP_VIEWPORT", False);
	s_aNetDesktopGeometry	= XInternAtom (s_XDisplay, "_NET_DESKTOP_GEOMETRY", False);
	s_aNetWorkarea			= XInternAtom (s_XDisplay, "_NET_WORKAREA", False);
	s_aNetShowingDesktop 	= XInternAtom (s_XDisplay, "_NET_SHOWING_DESKTOP", False);
	s_aRootMapID			= XInternAtom (s_XDisplay, "_XROOTPMAP_ID", False);  // Note: ESETROOT_PMAP_ID might be used instead. We don't handle it as it seems quite rare and somewhat deprecated.
	s_aNetNbDesktops		= XInternAtom (s_XDisplay, "_NET_NUMBER_OF_DESKTOPS", False);
	s_aXKlavierState		= XInternAtom (s_XDisplay, "XKLAVIER_STATE", False);
	
	//\__________________ On recupere le bureau courant.
	g_desktopGeometry.iNbDesktops = cairo_dock_get_nb_desktops ();
	cairo_dock_get_nb_viewports (&g_desktopGeometry.iNbViewportX, &g_desktopGeometry.iNbViewportY);
	_cairo_dock_retrieve_current_desktop_and_viewport ();
	
	//\__________________ On se met a l'ecoute des evenements X.
	Window root = DefaultRootWindow (s_XDisplay);
	cairo_dock_set_xwindow_mask (root, PropertyChangeMask /*| StructureNotifyMask | SubstructureNotifyMask | ResizeRedirectMask | SubstructureRedirectMask*/);
	
	//\__________________ On lance l'ecoute.
	s_iSidPollXEvents = g_timeout_add (CAIRO_DOCK_CHECK_XEVENTS_INTERVAL, (GSourceFunc) _cairo_dock_unstack_Xevents, (gpointer) NULL);  // un g_idle_add () consomme 90% de CPU ! :-/
	
	//\__________________ Init the Window Manager backends.
	cd_init_compiz_backend ();
	cd_init_kwin_backend ();
	cd_init_gnome_shell_backend ();
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_desktop_manager (void)
{
	// Manager
	memset (&myDesktopMgr, 0, sizeof (CairoDockDesktopManager));
	myDesktopMgr.mgr.cModuleName 	= "Desktop";
	myDesktopMgr.mgr.init 			= init;
	myDesktopMgr.mgr.load 			= NULL;
	myDesktopMgr.mgr.unload 		= unload;
	myDesktopMgr.mgr.reload 		= (GldiManagerReloadFunc)NULL;
	myDesktopMgr.mgr.get_config 	= (GldiManagerGetConfigFunc)NULL;
	myDesktopMgr.mgr.reset_config 	= (GldiManagerResetConfigFunc)NULL;
	// Config
	myDesktopMgr.mgr.pConfig = (GldiManagerConfigPtr)NULL;
	myDesktopMgr.mgr.iSizeOfConfig = 0;
	// data
	myDesktopMgr.mgr.iSizeOfData = 0;
	myDesktopMgr.mgr.pData = (GldiManagerDataPtr)NULL;
	// signals
	cairo_dock_install_notifications_on_object (&myDesktopMgr, NB_NOTIFICATIONS_DESKTOP);
	// connect to X (now, because other modules may need it for their init)
	s_XDisplay = cairo_dock_initialize_X_desktop_support ();  // renseigne la taille de l'ecran.
	// register
	gldi_register_manager (GLDI_MANAGER(&myDesktopMgr));
}

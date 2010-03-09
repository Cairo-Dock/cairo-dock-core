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
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef HAVE_XEXTEND
#include <X11/extensions/Xcomposite.h>
//#include <X11/extensions/Xdamage.h>
#endif

#include "cairo-dock-icons.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-load.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-container.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-X-manager.h"

#define CAIRO_DOCK_TASKBAR_CHECK_INTERVAL 200

extern CairoDock *g_pMainDock;

extern CairoDockDesktopGeometry g_desktopGeometry;

extern gboolean g_bUseOpenGL;
//extern int g_iDamageEvent;

static Display *s_XDisplay = NULL;
static int s_iSidPollXEvents = 0;
static Atom s_aNetClientList;
static Atom s_aNetActiveWindow;
static Atom s_aNetCurrentDesktop;
static Atom s_aNetDesktopViewport;
static Atom s_aNetDesktopGeometry;
static Atom s_aNetShowingDesktop;
static Atom s_aRootMapID;
static Atom s_aNetNbDesktops;
static Atom s_aXKlavierState;

static gboolean _cairo_dock_window_is_on_our_way (Window *Xid, Icon *icon, gpointer *data);

  ///////////////////////
 // X listener : core //
///////////////////////

static inline void _cairo_dock_retrieve_current_desktop_and_viewport (void)
{
	g_desktopGeometry.iCurrentDesktop = cairo_dock_get_current_desktop ();
	cairo_dock_get_current_viewport (&g_desktopGeometry.iCurrentViewportX, &g_desktopGeometry.iCurrentViewportY);
	g_desktopGeometry.iCurrentViewportX /= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
	g_desktopGeometry.iCurrentViewportY /= g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
}

static gboolean _on_change_current_desktop_viewport (void)
{
	CairoDock *pDock = g_pMainDock;
	
	_cairo_dock_retrieve_current_desktop_and_viewport ();
	
	// on propage la notification.
	cairo_dock_notify (CAIRO_DOCK_DESKTOP_CHANGED);
	
	// on gere le cas delicat de X qui nous fait sortir du dock.
	if (! pDock->bIsShrinkingDown && ! pDock->bIsGrowingUp)
	{
		if (pDock->container.bIsHorizontal)
			gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseX, &pDock->container.iMouseY, NULL);
		else
			gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseY, &pDock->container.iMouseX, NULL);
		//g_print ("on met a jour de force\n");
		cairo_dock_calculate_dock_icons (pDock);  // pour faire retrecir le dock si on n'est pas dedans, merci X de nous faire sortir du dock alors que la souris est toujours dedans :-/
		if (pDock->container.bInside)
			return TRUE;
		//g_print (">>> %d;%d, %dx%d\n", pDock->container.iMouseX, pDock->container.iMouseY,pDock->container.iWidth,  pDock->container.iHeight);
	}
	return FALSE;
}

static void _on_change_nb_desktops (void)
{
	g_desktopGeometry.iNbDesktops = cairo_dock_get_nb_desktops ();
	_cairo_dock_retrieve_current_desktop_and_viewport ();  // au cas ou on enleve le bureau courant.
	
	cairo_dock_notify (CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED);
}

static void _on_change_desktop_geometry (void)
{
	if (cairo_dock_update_screen_geometry ())  // modification de la resolution.
	{
		cd_message ("resolution alteree");
		
		/*if (myPosition.bUseXinerama)
		{
			cairo_dock_get_screen_offsets (myPosition.iNumScreen, &g_pMainDock->iScreenOffsetX, &g_pMainDock->iScreenOffsetY);  /// on le fait ici pour avoir g_desktopGeometry.iScreenWidth et g_desktopGeometry.iScreenHeight, mais il faudrait en faire un parametre par dock...
		}*/
		
		cairo_dock_reposition_root_docks (FALSE);  // main dock compris. Se charge de Xinerama.
	}
	
	cairo_dock_get_nb_viewports (&g_desktopGeometry.iNbViewportX, &g_desktopGeometry.iNbViewportY);
	_cairo_dock_retrieve_current_desktop_and_viewport ();  // au cas ou on enleve le viewport courant.
	
	cairo_dock_notify (CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED);
}

static gboolean _cairo_dock_unstack_Xevents (gpointer data)
{
	static XEvent event;
	static gboolean bCheckMouseIsOutside = FALSE;
	
	CairoDock *pDock = g_pMainDock;
	if (!pDock)  // peut arriver en cours de chargement d'un theme.
		return TRUE;
	
	long event_mask = 0xFFFFFFFF;  // on les recupere tous, ca vide la pile au fur et a mesure plutot que tout a la fin.
	Window Xid;
	Window root = DefaultRootWindow (s_XDisplay);
	Icon *icon;
	if (bCheckMouseIsOutside)
	{
		//g_print ("bCheckMouseIsOutside\n");
		bCheckMouseIsOutside = FALSE;
		if (pDock->container.bIsHorizontal)
			gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseX, &pDock->container.iMouseY, NULL);
		else
			gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseY, &pDock->container.iMouseX, NULL);
		cairo_dock_calculate_dock_icons (pDock);  // pour faire retrecir le dock si on n'est pas dedans, merci X de nous faire sortir du dock alors que la souris est toujours dedans :-/
	}
	
	while (XCheckMaskEvent (s_XDisplay, event_mask, &event))
	{
		icon = NULL;
		Xid = event.xany.window;
		//g_print ("  type : %d; atom : %s; window : %d\n", event.type, gdk_x11_get_xatom_name (event.xproperty.atom), Xid);
		//if (event.type == ClientMessage)
		//cd_message ("\n\n\n >>>>>>>>>>>< event.type : %d\n\n", event.type);
		if (Xid == root)
		{
			if (event.type == PropertyNotify)  // PropertyNotify sur root
			{
				if (event.xproperty.atom == s_aNetClientList)
				{
					cairo_dock_notify (CAIRO_DOCK_WINDOW_CONFIGURED, Xid, NULL);
				}
				else if (event.xproperty.atom == s_aNetActiveWindow)
				{
					Window XActiveWindow = cairo_dock_get_active_xwindow ();
					cairo_dock_notify (CAIRO_DOCK_WINDOW_ACTIVATED, &XActiveWindow);
				}
				else if (event.xproperty.atom == s_aNetCurrentDesktop || event.xproperty.atom == s_aNetDesktopViewport)
				{
					bCheckMouseIsOutside = _on_change_current_desktop_viewport ();  // -> CAIRO_DOCK_DESKTOP_CHANGED
				}
				else if (event.xproperty.atom == s_aNetNbDesktops)
				{
					_on_change_nb_desktops ();  // -> CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED
				}
				else if (event.xproperty.atom == s_aNetDesktopGeometry)
				{
					_on_change_desktop_geometry ();  // -> CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED
				}
				else if (event.xproperty.atom == s_aRootMapID)
				{
					cd_debug ("change wallpaper");
					cairo_dock_reload_desktop_background ();
					cairo_dock_notify (CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED);
				}
				else if (event.xproperty.atom == s_aNetShowingDesktop)
				{
					cairo_dock_notify (CAIRO_DOCK_DESKTOP_VISIBILITY_CHANGED);
				}
				else if (event.xproperty.atom == s_aXKlavierState)
				{
					cairo_dock_notify (CAIRO_DOCK_KBD_STATE_CHANGED, NULL);
				}
			}  // fin de PropertyNotify sur root.
		}
		else  // evenement sur une fenetre.
		{
			if (event.type == PropertyNotify)  // PropertyNotify sur une fenetre
			{
				if (event.xproperty.atom == s_aXKlavierState)
				{
					cairo_dock_notify (CAIRO_DOCK_KBD_STATE_CHANGED, &Xid);
				}
				else
				{
					cairo_dock_notify (CAIRO_DOCK_WINDOW_PROPERTY_CHANGED, Xid, event.xproperty.atom, event.xproperty.state);
				}
			}
			else if (event.type == ConfigureNotify)  // ConfigureNotify sur une fenetre.
			{
				cairo_dock_notify (CAIRO_DOCK_WINDOW_CONFIGURED, Xid, &event.xconfigure);
			}
			/*else if (event.type == g_iDamageEvent + XDamageNotify)
			{
				XDamageNotifyEvent *e = (XDamageNotifyEvent *) &event;
				g_print ("window %s has been damaged (%d;%d %dx%d)\n", e->drawable, e->area.x, e->area.y, e->area.width, e->area.height);
				// e->drawable is the window ID of the damaged window
				// e->geometry is the geometry of the damaged window	
				// e->area     is the bounding rect for the damaged area	
				// e->damage   is the damage handle returned by XDamageCreate()
				// Subtract all the damage, repairing the window.
				XDamageSubtract (s_XDisplay, e->damage, None, None);
			}
			else
				g_print ("  type : %d (%d); window : %d\n", event.type, XDamageNotify, Xid);*/
		}  // fin d'evenement sur une fenetre.
	}
	if (XEventsQueued (s_XDisplay, QueuedAlready) != 0)
		XSync (s_XDisplay, True);  // True <=> discard.
	//g_print ("XEventsQueued : %d\n", XEventsQueued (s_XDisplay, QueuedAfterFlush));  // QueuedAlready, QueuedAfterReading, QueuedAfterFlush
	
	return TRUE;
}

static void cairo_dock_initialize_X_manager (Display *pDisplay)
{
	s_XDisplay = pDisplay;

	s_aNetClientList		= XInternAtom (s_XDisplay, "_NET_CLIENT_LIST_STACKING", False);
	s_aNetActiveWindow		= XInternAtom (s_XDisplay, "_NET_ACTIVE_WINDOW", False);
	s_aNetCurrentDesktop	= XInternAtom (s_XDisplay, "_NET_CURRENT_DESKTOP", False);
	s_aNetDesktopViewport	= XInternAtom (s_XDisplay, "_NET_DESKTOP_VIEWPORT", False);
	s_aNetDesktopGeometry	= XInternAtom (s_XDisplay, "_NET_DESKTOP_GEOMETRY", False);
	s_aNetShowingDesktop 	= XInternAtom (s_XDisplay, "_NET_SHOWING_DESKTOP", False);
	s_aRootMapID			= XInternAtom (s_XDisplay, "_XROOTPMAP_ID", False);
	s_aNetNbDesktops		= XInternAtom (s_XDisplay, "_NET_NUMBER_OF_DESKTOPS", False);
	s_aXKlavierState		= XInternAtom (s_XDisplay, "XKLAVIER_STATE", False);
}

void cairo_dock_start_X_manager (void)
{
	g_return_if_fail (s_iSidPollXEvents == 0);
	
	//\__________________ On initialise le support de X.
	Display *pDisplay = cairo_dock_initialize_X_desktop_support ();  // renseigne la taille de l'ecran.
	cairo_dock_initialize_class_manager ();
	cairo_dock_initialize_application_manager (pDisplay);
	cairo_dock_initialize_X_manager (pDisplay);
	
	//\__________________ On recupere le bureau courant.
	g_desktopGeometry.iNbDesktops = cairo_dock_get_nb_desktops ();
	cairo_dock_get_nb_viewports (&g_desktopGeometry.iNbViewportX, &g_desktopGeometry.iNbViewportY);
	_cairo_dock_retrieve_current_desktop_and_viewport ();
	
	//\__________________ On se met a l'ecoute des evenements X.
	Window root = DefaultRootWindow (s_XDisplay);
	cairo_dock_set_xwindow_mask (root, PropertyChangeMask /*| StructureNotifyMask | SubstructureNotifyMask | ResizeRedirectMask | SubstructureRedirectMask*/);
	
	//\__________________ On lance l'ecoute.
	s_iSidPollXEvents = g_timeout_add (CAIRO_DOCK_TASKBAR_CHECK_INTERVAL, (GSourceFunc) _cairo_dock_unstack_Xevents, (gpointer) NULL);  // un g_idle_add () consomme 90% de CPU ! :-/
}

void cairo_dock_stop_X_manager (void)  // le met seulement en pause.
{
	g_return_if_fail (s_iSidPollXEvents != 0);
	
	//\__________________ On arrete l'ecoute.
	g_source_remove (s_iSidPollXEvents);
	s_iSidPollXEvents = 0;
}

  /////////////////////////
 // X listener : access //
/////////////////////////

void cairo_dock_get_current_desktop_and_viewport (int *iCurrentDesktop, int *iCurrentViewportX, int *iCurrentViewportY)
{
	*iCurrentDesktop = g_desktopGeometry.iCurrentDesktop;
	*iCurrentViewportX = g_desktopGeometry.iCurrentViewportX;
	*iCurrentViewportY = g_desktopGeometry.iCurrentViewportY;
}

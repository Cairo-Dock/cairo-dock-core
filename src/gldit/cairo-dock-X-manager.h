/*
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

#ifndef __CAIRO_DOCK_X_MANAGER__
#define  __CAIRO_DOCK_X_MANAGER__

#include <X11/Xlib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-X-manager.h This class manages the interactions with X.
* The X manager will handle signals from X and dispatch them, and manages the screen geometry.
*/

typedef struct _CairoDockDesktopManager CairoDockDesktopManager;

#ifndef _MANAGER_DEF_
extern CairoDockDesktopManager myDesktopMgr;
#endif

// no param

// manager
struct _CairoDockDesktopManager {
	GldiManager mgr;
	} ;

/// signals
typedef enum {
	/// notification called when the user switches to another desktop/viewport. data : NULL
	NOTIFICATION_DESKTOP_CHANGED,
	/// notification called when the geometry of the desktop has changed (number of viewports/desktops, dimensions). data : NULL
	NOTIFICATION_SCREEN_GEOMETRY_ALTERED,
	/// notification called when the desktop is shown/hidden. data:NULL.
	NOTIFICATION_DESKTOP_VISIBILITY_CHANGED,
	/// notification called when the state of the keyboard has changed.
	NOTIFICATION_KBD_STATE_CHANGED,
	/// notification called when a window is resized or moved, or when the z-order of windows has changed. data : {Xid, XConfigureEvent or NULL}.
	NOTIFICATION_WINDOW_CONFIGURED,
	/// notification called when the active window has changed. data : Window* or NULL
	NOTIFICATION_WINDOW_ACTIVATED,
	/// notification called when a window's property has changed. data : {Window, Atom, int}
	NOTIFICATION_WINDOW_PROPERTY_CHANGED,
	/// 
	NB_NOTIFICATIONS_DESKTOP
	} CairoDesktopNotifications;

// data
struct _CairoDockDesktopGeometry {
	int iScreenWidth[2], iScreenHeight[2];  // dimension de l'ecran sur lequel est place le dock.
	int iXScreenWidth[2], iXScreenHeight[2];  // dimension de l'ecran logique compose eventuellement de plusieurs moniteurs.
	int iNbDesktops;
	int iNbViewportX, iNbViewportY;
	int iCurrentDesktop;
	int iCurrentViewportX, iCurrentViewportY;
	};

// X manager : access
/** Get the current workspace (desktop and viewport).
*@param iCurrentDesktop will be filled with the current desktop number
*@param iCurrentViewportX will be filled with the current horizontal viewport number
*@param iCurrentViewportY will be filled with the current vertical viewport number
*/
void cairo_dock_get_current_desktop_and_viewport (int *iCurrentDesktop, int *iCurrentViewportX, int *iCurrentViewportY);

// Desktop background
CairoDockDesktopBackground *cairo_dock_get_desktop_background (gboolean bWithTextureToo);

void cairo_dock_destroy_desktop_background (CairoDockDesktopBackground *pDesktopBg);

cairo_surface_t *cairo_dock_get_desktop_bg_surface (CairoDockDesktopBackground *pDesktopBg);

GLuint cairo_dock_get_desktop_bg_texture (CairoDockDesktopBackground *pDesktopBg);


void gldi_register_desktop_manager (void);

G_END_DECLS
#endif

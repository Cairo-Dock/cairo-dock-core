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
extern CairoDockDesktopGeometry g_desktopGeometry;
#endif

// no param

// manager
struct _CairoDockDesktopManager {
	GldiManager mgr;
	} ;

/// signals
typedef enum {
	/// notification called when the user switches to another desktop/viewport. data : NULL
	NOTIFICATION_DESKTOP_CHANGED = NB_NOTIFICATIONS_OBJECT,
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
	NB_NOTIFICATIONS_DESKTOP
	} CairoDesktopNotifications;

// data
struct _CairoDockDesktopGeometry {
	int iNbScreens;
	GtkAllocation *pScreens;  // liste of all screen devices.
	GtkAllocation Xscreen;  // logical screen, possibly made of several screen devices.
	int iNbDesktops;
	int iNbViewportX, iNbViewportY;
	int iCurrentDesktop;
	int iCurrentViewportX, iCurrentViewportY;
	};


/// Definition of the Window Manager backend.
struct _CairoDockWMBackend {
	gboolean (*present_class) (const gchar *cClass);
	gboolean (*present_windows) (void);
	gboolean (*present_desktops) (void);
	gboolean (*show_widget_layer) (void);
	gboolean (*set_on_widget_layer) (Window Xid, gboolean bOnWidgetLayer);
	};

/// Definition of a Desktop Background Buffer. It has a reference count so that it can be shared across all the lib.
struct _CairoDockDesktopBackground {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	guint iSidDestroyBg;
	gint iRefCount;
	} ;


/** Register a Window Manager backend, overwriting any previous one.
*@param pBackend a Window Manager backend; the function takes ownership of the pointer.
*/
void cairo_dock_wm_register_backend (CairoDockWMBackend *pBackend);

/** Present all the windows of a given class.
*@param cClass the class.
*@return TRUE on success
*/
gboolean cairo_dock_wm_present_class (const gchar *cClass);

/** Present all the windows of the current desktop.
*@return TRUE on success
*/
gboolean cairo_dock_wm_present_windows (void);

/** Present all the desktops.
*@return TRUE on success
*/
gboolean cairo_dock_wm_present_desktops (void);

/** Show the Widget Layer.
*@return TRUE on success
*/
gboolean cairo_dock_wm_show_widget_layer (void);

/** Set a window to be displayed on the Widget Layer.
*@param Xid X ID of the window.
*@param bOnWidgetLayer whether to set or unset the option.
*@return TRUE on success
*/
gboolean cairo_dock_wm_set_on_widget_layer (Window Xid, gboolean bOnWidgetLayer);

gboolean cairo_dock_wm_can_present_class (void);
gboolean cairo_dock_wm_can_present_windows (void);
gboolean cairo_dock_wm_can_present_desktops (void);
gboolean cairo_dock_wm_can_show_widget_layer (void);
gboolean cairo_dock_wm_can_set_on_widget_layer (void);


// X manager : access
/** Get the current workspace (desktop and viewport).
*@param iCurrentDesktop will be filled with the current desktop number
*@param iCurrentViewportX will be filled with the current horizontal viewport number
*@param iCurrentViewportY will be filled with the current vertical viewport number
*/
void cairo_dock_get_current_desktop_and_viewport (int *iCurrentDesktop, int *iCurrentViewportX, int *iCurrentViewportY);

#define GLDI_DEFAULT_SCREEN 0 // it's the first screen. -1 = all screens
#define cairo_dock_get_screen_position_x(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? g_desktopGeometry.pScreens[i].x : 0)
#define cairo_dock_get_screen_position_y(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? g_desktopGeometry.pScreens[i].y : 0)
#define cairo_dock_get_screen_width(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? g_desktopGeometry.pScreens[i].width : g_desktopGeometry.Xscreen.width)
#define cairo_dock_get_screen_height(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? g_desktopGeometry.pScreens[i].height : g_desktopGeometry.Xscreen.height)
#define cairo_dock_get_nth_screen(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? &g_desktopGeometry.pScreens[i] : &g_desktopGeometry.Xscreen)
#define gldi_get_desktop_width() g_desktopGeometry.Xscreen.width
#define gldi_get_desktop_height() g_desktopGeometry.Xscreen.height

// Desktop background
CairoDockDesktopBackground *cairo_dock_get_desktop_background (gboolean bWithTextureToo);

void cairo_dock_destroy_desktop_background (CairoDockDesktopBackground *pDesktopBg);

cairo_surface_t *cairo_dock_get_desktop_bg_surface (CairoDockDesktopBackground *pDesktopBg);

GLuint cairo_dock_get_desktop_bg_texture (CairoDockDesktopBackground *pDesktopBg);


void gldi_register_desktop_manager (void);

G_END_DECLS
#endif

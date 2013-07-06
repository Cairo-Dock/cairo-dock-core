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

#ifndef __GLDI_DESKTOP_MANAGER__
#define  __GLDI_DESKTOP_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-desktop-manager.h This class manages the desktop: screen geometry, current desktop/viewport, etc, and notifies for any change on it.
*/

// manager

#ifndef _MANAGER_DEF_
extern GldiManager myDesktopMgr;
extern GldiDesktopGeometry g_desktopGeometry;
#endif

// no param


/// signals
typedef enum {
	/// notification called when the user switches to another desktop/viewport. data : NULL
	NOTIFICATION_DESKTOP_CHANGED = NB_NOTIFICATIONS_OBJECT,
	/// notification called when the geometry of the desktop has changed (number of viewports/desktops, dimensions). data: resolution-has-changed
	NOTIFICATION_DESKTOP_GEOMETRY_CHANGED,
	/// notification called when the desktop is shown/hidden. data: NULL
	NOTIFICATION_DESKTOP_VISIBILITY_CHANGED,
	/// notification called when the state of the keyboard has changed.
	NOTIFICATION_KBD_STATE_CHANGED,
	/// notification called when the names of the desktops have changed
	NOTIFICATION_DESKTOP_NAMES_CHANGED,
	/// notification called when the wallpaper has changed
	NOTIFICATION_DESKTOP_WALLPAPER_CHANGED,
	NB_NOTIFICATIONS_DESKTOP
	} CairoDesktopNotifications;

// data
struct _GldiDesktopGeometry {
	int iNbScreens;
	GtkAllocation *pScreens;  // liste of all screen devices.
	GtkAllocation Xscreen;  // logical screen, possibly made of several screen devices.
	int iNbDesktops;
	int iNbViewportX, iNbViewportY;
	int iCurrentDesktop;
	int iCurrentViewportX, iCurrentViewportY;
	};

/// Definition of the Desktop Manager backend.
struct _GldiDesktopManagerBackend {
	gboolean (*present_class) (const gchar *cClass);
	gboolean (*present_windows) (void);
	gboolean (*present_desktops) (void);
	gboolean (*show_widget_layer) (void);
	gboolean (*set_on_widget_layer) (GldiContainer *pContainer, gboolean bOnWidgetLayer);
	gboolean (*show_hide_desktop) (gboolean bShow);
	gboolean (*desktop_is_visible) (void);
	gchar** (*get_desktops_names) (void);
	gboolean (*set_desktops_names) (gchar **cNames);
	cairo_surface_t* (*get_desktop_bg_surface) (void);
	gboolean (*set_current_desktop) (int iDesktopNumber, int iViewportNumberX, int iViewportNumberY);
	gboolean (*set_nb_desktops) (int iNbDesktops, int iNbViewportX, int iNbViewportY);
	void (*refresh) (void);
	};

/// Definition of a Desktop Background Buffer. It has a reference count so that it can be shared across all the lib.
struct _GldiDesktopBackground {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	guint iSidDestroyBg;
	gint iRefCount;
	} ;


  /////////////////////
 // Desktop Backend //
/////////////////////

/** Register a Desktop Manager backend. NULL functions do not overwrite existing ones.
*@param pBackend a Desktop Manager backend; can be freeed after.
*/
void gldi_desktop_manager_register_backend (GldiDesktopManagerBackend *pBackend);

/** Present all the windows of a given class.
*@param cClass the class.
*@return TRUE on success
*/
gboolean gldi_desktop_present_class (const gchar *cClass);

/** Present all the windows of the current desktop.
*@return TRUE on success
*/
gboolean gldi_desktop_present_windows (void);

/** Present all the desktops.
*@return TRUE on success
*/
gboolean gldi_desktop_present_desktops (void);

/** Show the Widget Layer.
*@return TRUE on success
*/
gboolean gldi_desktop_show_widget_layer (void);

/** Set a Container to be displayed on the Widget Layer.
*@param pContainer a container.
*@param bOnWidgetLayer whether to set or unset the option.
*@return TRUE on success
*/
gboolean gldi_desktop_set_on_widget_layer (GldiContainer *pContainer, gboolean bOnWidgetLayer);

gboolean gldi_desktop_can_present_class (void);
gboolean gldi_desktop_can_present_windows (void);
gboolean gldi_desktop_can_present_desktops (void);
gboolean gldi_desktop_can_show_widget_layer (void);
gboolean gldi_desktop_can_set_on_widget_layer (void);

gboolean gldi_desktop_show_hide (gboolean bShow);
gboolean gldi_desktop_is_visible (void);
gchar** gldi_desktop_get_names (void);
gboolean gldi_desktop_set_names (gchar **cNames);
gboolean gldi_desktop_set_current (int iDesktopNumber, int iViewportNumberX, int iViewportNumberY);
gboolean gldi_desktop_set_nb_desktops (int iNbDesktops, int iNbViewportX, int iNbViewportY);

void gldi_desktop_refresh (void);

  ////////////////////
 // Desktop access //
////////////////////

/** Get the current workspace (desktop and viewport).
*@param iCurrentDesktop will be filled with the current desktop number
*@param iCurrentViewportX will be filled with the current horizontal viewport number
*@param iCurrentViewportY will be filled with the current vertical viewport number
*/
void gldi_desktop_get_current (int *iCurrentDesktop, int *iCurrentViewportX, int *iCurrentViewportY);

#define GLDI_DEFAULT_SCREEN 0 // it's the first screen. -1 = all screens
#define cairo_dock_get_screen_position_x(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? g_desktopGeometry.pScreens[i].x : 0)
#define cairo_dock_get_screen_position_y(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? g_desktopGeometry.pScreens[i].y : 0)
#define cairo_dock_get_screen_width(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? g_desktopGeometry.pScreens[i].width : g_desktopGeometry.Xscreen.width)
#define cairo_dock_get_screen_height(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? g_desktopGeometry.pScreens[i].height : g_desktopGeometry.Xscreen.height)
#define cairo_dock_get_nth_screen(i) (i >= 0 && i < g_desktopGeometry.iNbScreens ? &g_desktopGeometry.pScreens[i] : &g_desktopGeometry.Xscreen)
#define gldi_desktop_get_width() g_desktopGeometry.Xscreen.width
#define gldi_desktop_get_height() g_desktopGeometry.Xscreen.height

  ////////////////////////
 // Desktop background //
////////////////////////

GldiDesktopBackground *gldi_desktop_background_get (gboolean bWithTextureToo);

void gldi_desktop_background_destroy (GldiDesktopBackground *pDesktopBg);

cairo_surface_t *gldi_desktop_background_get_surface (GldiDesktopBackground *pDesktopBg);

GLuint gldi_desktop_background_get_texture (GldiDesktopBackground *pDesktopBg);


void gldi_register_desktop_manager (void);

G_END_DECLS
#endif

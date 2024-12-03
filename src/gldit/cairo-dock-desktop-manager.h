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
	/// notification called when a shortkey that has been registered by the dock is pressed. data: keycode, modifiers
	NOTIFICATION_SHORTKEY_PRESSED,
	/// notification called when the keymap changed, before and after updating it. data: updated
	NOTIFICATION_KEYMAP_CHANGED,
	/// notification when the user requests the desktop menu to be shown
	NOTIFICATION_MENU_REQUEST,
	NB_NOTIFICATIONS_DESKTOP
	} CairoDesktopNotifications;


/*
Old behavior: separate iNbDesktops + iNbViewportX / iNbViewportY:
  - iNbDesktops: Independent "desktops", no overlap (i.e. each window is on one desktop at maximum). Desktops
      are arranged in one dimension.
  - iNbViewportX / iNbViewportY: each desktop can have multiple viewports, strictly arranged into a rectangle
      that form a continuous space, thus windows can overlap among viewports.
  - These map to distinct APIs in X11: desktops to _NET_NUMBER_OF_DESKTOPS / _NET_CURRENT_DESKTOP and
      viewports to _NET_DESKTOP_GEOMETRY / _NET_DESKTOP_VIEWPORT:
      https://specifications.freedesktop.org/wm-spec/latest/ar01s03.html

Typcial behavior:
  - Compiz (+ also Gnome-shell?): iNbDesktops == 1, "workspaces" correspond to viewports only
  - other WMs: iNbDesktops >= 1, iNbViewportX == iNbViewportY == 1

Questions / issues:
  - Unsure if for any WM / DE, we can have both iNbDesktops > 1 and multiple viewports. In theory, on Wayland
      this will be possible with the ext-workspace protocol if there are multiple workspace groups. On X11,
      this is explicitly supported by the standards, but likely not implemented by WMs.
  - Are there X11 WMs that provide multipe desktops and arranges them in 2D? In theory, this is possible using
      _NET_DESKTOP_LAYOUT, however Cairo-Dock currently does not handle this. Note: according to the
      specification, this is set by the "pager", which is possibly a separate entity from the WM:
      https://specifications.freedesktop.org/wm-spec/latest/ar01s03.html
      So possibly Cairo-Dock could act as a pager in this case and arrange the desktops as it is convenient.
      (TODO: tesk maybe with Openbox or similar)
      However, when not using viewports, this is only relevant if the WM / compositor provides an animation
      when switching (as it can be confusing if that does not match CD's UI).
      Examples:
       - Cinnamon: https://github.com/linuxmint/muffin/commit/4a0a270c260f2272b97473b1897c5d71c746ecc6
	       (set through their config?)
	   - Openbox: https://forums.freebsd.org/threads/how-to-start-openbox-with-2x2-grid-workplaces.84749/
	       https://openbox.org/help/Openbox-session
	       (set by changing _NET_DESKTOP_LAYOUT, e.g. by us)
	   - KWin (X11): https://invent.kde.org/plasma/kwin/-/merge_requests/5065
	       (set in config or DBus)
  - How should things work with multiple monitors? On X11, they are assumed to have the same desktops and
      viewports, but Wayland allows separate workspace groups

Minimal changes for Wayland:
  - workspace groups correspond to desktops, and workspaces to viewports; make sure things work for multiple
      workspace groups
  - allow viewports to be non-overlapping (GLDI_WM_NO_VIEWPORT_OVERLAP -- this is how desktops behave)
  - workspaces are arranged into 1 or 2 dimensions, based on the coordinates (higher dimensions are not
      supported, converted to 1D)
  - update the switcher applet to display desktops / workspace groups more independently
  - change APIs to add / remove individual workspaces, take care to handle to case of X11 rectangular
      viewports though -> partly done
  - move GldiWMBackendFlags and the related capabilities here? -> no, better in window manager
  - X11: special case WMs where desktops can be arranged in 2D? (but has to be no viewports)
      -> "desktops" would be interpreted as independent viewports in this case

Plan for a new API:
  - two-level structure with "desktops" and "vieports"
  - on X11: desktop <-> _NET_NUMBER_OF_DESKTOPS / _NET_CURRENT_DESKTOP / _NET_DESKTOP_LAYOUT
            workspace <-> _NET_DESKTOP_GEOMETRY / _NET_DESKTOP_VIEWPORT
  - on Wayland: desktop <-> workspace-group, viewport <-> workspace (on KWin, only one desktop)
  - both desktops and viewports can be arranged into a 2D grid (for desktops, add this)
  - number of viewports per desktop can be different -> dynamic storage of a new _GldiViewportGeometry for
      each desktop
  - desktops are always "independent": a window can only span one (or all for sticky windows)
  - vieports can be overlapping (a window can span multiple), flag / setting for this that is set by the backend

Next steps:
  - change API for adding and removing "workspaces", these are handled by a backend-specific way (move some of the
      logic from the switcher plugin to core) -> done
  - make the number of viewports per desktop independent, use this to implement the ext-workspace /
      cosmic-workspace protocol
  - implement support for _NET_DESKTOP_LAYOUT for arranging desktops in a 2D grid on X11
*/

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
	void (*refresh) (void);
	gboolean (*grab_shortkey) (guint keycode, guint modifiers, gboolean grab);
	void (*add_workspace) (void); // gldi_desktop_add_workspace ()
	void (*remove_last_workspace) (void); // gldi_desktop_remove_last_workspace ()
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
void gldi_desktop_manager_register_backend (GldiDesktopManagerBackend *pBackend, const gchar *name);

const gchar *gldi_desktop_manager_get_backend_names (void);

/** Present all the windows of a given class.
*@param cClass the class.
*@param pContainer currently active container which might need to be unfocused
*@return TRUE on success
*/
gboolean gldi_desktop_present_class (const gchar *cClass, GldiContainer *pContainer);

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

/** Adds a new workspace, desktop or viewport in an implementation-defined manner.
 * Typically this can mean adding one more workspace / desktop as the "last" one.
 * On X11, this will resize the desktop geometry, and could result in adding
 * multiple viewports.
 * Might not suceed, depending on the capabilities of the backend
 * (NOTIFICATION_DESKTOP_GEOMETRY_CHANGED will be emitted if successful).
 */
void gldi_desktop_add_workspace (void);

/** Remove the "last" workspace desktop or viewport, according to the
 * internal ordering of workspaces. The actual number of workspaces can
 * be > 1, depending on the backend (on X11, if viewports are arranged
 * in a square).
 * Might not suceed, depending on the capabilities of the backend
 * (NOTIFICATION_DESKTOP_GEOMETRY_CHANGED will be emitted if successful).
 */
void gldi_desktop_remove_last_workspace (void);


void gldi_desktop_refresh (void);

gboolean gldi_desktop_grab_shortkey (guint keycode, guint modifiers, gboolean grab);

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

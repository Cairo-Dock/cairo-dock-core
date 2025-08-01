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

#ifndef __GLDI_WINDOWS_MANAGER__
#define  __GLDI_WINDOWS_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-windows-manager.h This class manages the windows actors and notifies for any change on them.
*/

// manager
typedef struct _GldiWindowsManager GldiWindowsManager;

#ifndef _MANAGER_DEF_
extern GldiObjectManager myWindowObjectMgr;
#endif

/// signals
typedef enum {
	NOTIFICATION_WINDOW_CREATED = NB_NOTIFICATIONS_OBJECT,
	NOTIFICATION_WINDOW_DESTROYED,
	NOTIFICATION_WINDOW_NAME_CHANGED,
	NOTIFICATION_WINDOW_ICON_CHANGED,
	NOTIFICATION_WINDOW_ATTENTION_CHANGED,
	NOTIFICATION_WINDOW_SIZE_POSITION_CHANGED,
	NOTIFICATION_WINDOW_STATE_CHANGED,
	NOTIFICATION_WINDOW_CLASS_CHANGED,
	NOTIFICATION_WINDOW_Z_ORDER_CHANGED,
	NOTIFICATION_WINDOW_ACTIVATED,
	NOTIFICATION_WINDOW_DESKTOP_CHANGED,
	NB_NOTIFICATIONS_WINDOWS
	} GldiWindowNotifications;

// data

typedef enum {
	GLDI_WM_GEOM_REL_TO_VIEWPORT = 1, // if present, windows' geometry is relative to the viewport they are present on (and not to the current viewport)
	GLDI_WM_NO_VIEWPORT_OVERLAP = 2, // if present, windows cannot span multiple viewports
	GLDI_WM_HAVE_WINDOW_GEOMETRY = 4, // the WM provides valid window geometry information (in _GldiWindowActor::windowGeometry)
	GLDI_WM_HAVE_WORKSPACES = 8 // the WM tracks which workspace (desktop, viewport) a window is on (in iNumDesktop, iViewPortX, iViewPortY)
	} GldiWMBackendFlags;

/// Definition of the Windows Manager backend.
struct _GldiWindowManagerBackend {
	GldiWindowActor* (*get_active_window) (void);
	void (*move_to_nth_desktop) (GldiWindowActor *actor, int iNumDesktop, int iDeltaViewportX, int iDeltaViewportY);
	void (*show) (GldiWindowActor *actor);
	void (*close) (GldiWindowActor *actor);
	void (*kill) (GldiWindowActor *actor);
	void (*minimize) (GldiWindowActor *actor);
	void (*lower) (GldiWindowActor *actor);
	void (*maximize) (GldiWindowActor *actor, gboolean bMaximize);
	void (*set_fullscreen) (GldiWindowActor *actor, gboolean bFullScreen);
	void (*set_above) (GldiWindowActor *actor, gboolean bAbove);
	void (*set_thumbnail_area) (GldiWindowActor *actor, GldiContainer* pContainer, int x, int y, int w, int h);
	void (*set_window_border) (GldiWindowActor *actor, gboolean bWithBorder);
	cairo_surface_t* (*get_icon_surface) (GldiWindowActor *actor, int iWidth, int iHeight);
	cairo_surface_t* (*get_thumbnail_surface) (GldiWindowActor *actor, int iWidth, int iHeight);
	GLuint (*get_texture) (GldiWindowActor *actor);
	GldiWindowActor* (*get_transient_for) (GldiWindowActor *actor);
	void (*is_above_or_below) (GldiWindowActor *actor, gboolean *bIsAbove, gboolean *bIsBelow);
	void (*set_sticky) (GldiWindowActor *actor, gboolean bSticky);
	void (*can_minimize_maximize_close) (GldiWindowActor *actor, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose);
	guint (*get_id) (GldiWindowActor *actor);
	GldiWindowActor* (*pick_window) (GtkWindow *pParentWindow);  // grab the mouse, wait for a click, then get the clicked window and returns its actor
	const gchar *name; // name of the current backend
	void (*move_to_viewport_abs) (GldiWindowActor *actor, int iNumDesktop, int iViewportX, int iViewportY); // like move_to_nth_desktop, but use absolute viewport coordinates
	gpointer flags; // GldiWMBackendFlags, cast to pointer
	void (*get_menu_address) (GldiWindowActor *actor, char **service_name, char **object_path);
	} ;

/// Definition of a window actor.
struct _GldiWindowActor {
	GldiObject object;
	gboolean bDisplayed;  /// not used yet...
	gboolean bIsHidden;
	gboolean bIsFullScreen;
	gboolean bIsMaximized;
	gboolean bDemandsAttention;
	GtkAllocation windowGeometry;
	gint iNumDesktop;  // can be -1
	gint iViewPortX, iViewPortY;
	gint iStackOrder;
	gchar *cClass; // parsed class of this window (i.e. the result of gldi_window_parse_class ())
	gchar *cWmClass; // original class as reported by the WM (needed in some cases to match it)
	gchar *cName; // window title, displayed as label
	gchar *cWmName; // only used on X11, res_name part of XClassHint (i.e. the first string in WM_CLASS), not parsed
	gchar *cLastAttentionDemand;
	gint iAge;  // age of the window (a mere growing integer).
	gboolean bIsTransientFor;  // TRUE if the window is transient (for a parent window).
	gboolean bIsSticky;
	};


/** Register a Window Manager backend. NULL functions are simply ignored.
*@param pBackend a Window Manager backend
*/
void gldi_windows_manager_register_backend (GldiWindowManagerBackend *pBackend);

const gchar *gldi_windows_manager_get_name ();

/** Run a function on each window actor.
*@param bOrderedByZ TRUE to sort by z-order, FALSE to sort by age
*@param callback the callback
*@param data user data
*/
void gldi_windows_foreach (gboolean bOrderedByZ, GFunc callback, gpointer data);
void gldi_windows_foreach_unordered (GFunc callback, gpointer data);

/** Run a function on each window actor.
*@param callback the callback (takes the actor and the data, returns TRUE to stop)
*@param data user data
*@return the found actor, or NULL
*/
GldiWindowActor *gldi_windows_find (gboolean (*callback) (GldiWindowActor*, gpointer), gpointer data);

/** Get the current active window actor.
*@return the actor, or NULL if no window is currently active
*/
GldiWindowActor* gldi_windows_get_active (void);


void gldi_window_move_to_desktop (GldiWindowActor *actor, int iNumDesktop, int iNumViewportX, int iNumViewportY);

void gldi_window_show (GldiWindowActor *actor);
void gldi_window_close (GldiWindowActor *actor);
void gldi_window_kill (GldiWindowActor *actor);
void gldi_window_minimize (GldiWindowActor *actor);
void gldi_window_lower (GldiWindowActor *actor);
void gldi_window_maximize (GldiWindowActor *actor, gboolean bMaximize);
void gldi_window_set_fullscreen (GldiWindowActor *actor, gboolean bFullScreen);
void gldi_window_set_above (GldiWindowActor *actor, gboolean bAbove);

/** Set the position of this window's icon, to be used by the WM for its minimize animation.
 * Note: coordinates are relative to the passed container's main surface. */
void gldi_window_set_thumbnail_area (GldiWindowActor *actor, GldiContainer* pContainer, int x, int y, int w, int h);

void gldi_window_set_border (GldiWindowActor *actor, gboolean bWithBorder);

cairo_surface_t* gldi_window_get_icon_surface (GldiWindowActor *actor, int iWidth, int iHeight);

cairo_surface_t* gldi_window_get_thumbnail_surface (GldiWindowActor *actor, int iWidth, int iHeight);

GLuint gldi_window_get_texture (GldiWindowActor *actor);

GldiWindowActor* gldi_window_get_transient_for (GldiWindowActor *actor);

void gldi_window_is_above_or_below (GldiWindowActor *actor, gboolean *bIsAbove, gboolean *bIsBelow);

gboolean gldi_window_is_sticky (GldiWindowActor *actor);

void gldi_window_set_sticky (GldiWindowActor *actor, gboolean bSticky);

void gldi_window_can_minimize_maximize_close (GldiWindowActor *actor, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose);


gboolean gldi_window_is_on_current_desktop (GldiWindowActor *actor);

gboolean gldi_window_is_on_desktop (GldiWindowActor *pAppli, int iNumDesktop, int iNumViewportX, int iNumViewportY);

void gldi_window_move_to_current_desktop (GldiWindowActor *pAppli);

guint gldi_window_get_id (GldiWindowActor *pAppli);

/** Get the object path at which this window's app might export its global menus if supported by the backend.
 *@param actor the window whose menu is requested
 *@param service_name return location for the dbus service name
 *@param object_path return location for the object path
 * Note: the returned values in service_name and object_path point to strings owned by this window
 * actor instance and should not be modified or freed by the caller.
 */
void gldi_window_get_menu_address (GldiWindowActor *actor, char **service_name, char **object_path);

GldiWindowActor *gldi_window_pick (GtkWindow *pParentWindow);

/** Get all currently managed windows as mebers of a GPtrArray.
 *@return a newly allocated GPtrArray with all windows (the order is unspecified); the caller should
	free this with g_ptr_array_free()
 */
GPtrArray *gldi_window_manager_get_all (void);

/* WM capabilities -- use cases outside of cairo-dock-windows-manager.c (especially plugins)
 * should check these before using the corresponding fields */

/** Check whether we can track windows' position.
 *@return whether GldiWindowActor::windowGeometry contains the actual
 * window position; if FALSE, this should not be used
 */
gboolean gldi_window_manager_have_coordinates (void);

/** Check whether we can track which workspace / viewport / desktop
 *  windows are present on.
 *@return whether GldiWindowActor::iNumDesktop, iViewPortX and iViewPortY
 * contains valid information; if FALSE, these should not be used
 */
gboolean gldi_window_manager_can_track_workspaces (void);

/** Check how window position coordinates should be interpreted. Result is only
 *  valud if gldi_window_manager_have_coordinates () == TRUE as well.
 *@return whether window coordinates should be interpreted relative to the
 * currently active workspace / viewport; if false, coordinates are relative
 * to the workspace / viewport that the window is currently on
 */
gboolean gldi_window_manager_is_position_relative_to_current_viewport (void);

/** Check whether it is possible to move a window to another desktop / viewport.
 *@return TRUE if it is possible to move a window; if FALSE,
 * gldi_window_move_to_current_desktop () and gldi_window_move_to_desktop ()
 * will do nothing
 */
gboolean gldi_window_manager_can_move_to_desktop (void);

/* utility for parsing special cases in the window class / app ID;
 * used by both the X and Wayland backends
 * 
 * res_class and res_name should be the window class / app ID and window
 * name / title as reported by the windowing system; the return value is
 * a parsed class with some special cases handled; the caller should
 * free it when it is done using it
 * */
gchar* gldi_window_parse_class(const gchar* res_class, const gchar* res_name);

void gldi_register_windows_manager (void);

G_END_DECLS
#endif

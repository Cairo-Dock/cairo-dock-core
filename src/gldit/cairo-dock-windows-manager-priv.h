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

#ifndef __GLDI_WINDOWS_MANAGER_PRIV__
#define  __GLDI_WINDOWS_MANAGER_PRIV__

#include "cairo-dock-windows-manager.h"
G_BEGIN_DECLS

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

/** Register a Window Manager backend. NULL functions are simply ignored.
*@param pBackend a Window Manager backend
*/
void gldi_windows_manager_register_backend (GldiWindowManagerBackend *pBackend);

const gchar *gldi_windows_manager_get_name ();

/** Run a function on each window actor.
*@param callback the callback (takes the actor and the data, returns TRUE to stop)
*@param data user data
*@return the found actor, or NULL
*/
GldiWindowActor *gldi_windows_find (gboolean (*callback) (GldiWindowActor*, gpointer), gpointer data);

/** Set the position of this window's icon, to be used by the WM for its minimize animation.
 * Note: coordinates are relative to the passed container's main surface. */
void gldi_window_set_thumbnail_area (GldiWindowActor *actor, GldiContainer* pContainer, int x, int y, int w, int h);


cairo_surface_t* gldi_window_get_icon_surface (GldiWindowActor *actor, int iWidth, int iHeight);

cairo_surface_t* gldi_window_get_thumbnail_surface (GldiWindowActor *actor, int iWidth, int iHeight);

GLuint gldi_window_get_texture (GldiWindowActor *actor);

GldiWindowActor* gldi_window_get_transient_for (GldiWindowActor *actor);

void gldi_window_is_above_or_below (GldiWindowActor *actor, gboolean *bIsAbove, gboolean *bIsBelow);

gboolean gldi_window_is_sticky (GldiWindowActor *actor);

void gldi_window_set_sticky (GldiWindowActor *actor, gboolean bSticky);

GldiWindowActor *gldi_window_pick (GtkWindow *pParentWindow);

/** Get all currently managed windows as members of a GPtrArray.
 *@return a newly allocated GPtrArray with all windows (the order is unspecified); the caller should
	free this with g_ptr_array_free()
 */
GPtrArray *gldi_window_manager_get_all (void);


/** Utility for parsing special cases in the window class / app ID;
* used by both the X and Wayland backends.
* 
*@param res_class the window class (X11) or app ID (Wayland) reported by
*  the windowing system.
*@param res_name the window title as reported by the windowing system;
*@return a parsed class with some special cases handled; the caller
* should free it when it is done using it
*/
gchar* gldi_window_parse_class(const gchar* res_class, const gchar* res_name);

void gldi_register_windows_manager (void);

G_END_DECLS
#endif


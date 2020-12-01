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
	void (*set_minimize_position) (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y);
	void (*set_thumbnail_area) (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y, int w, int h);
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
	gchar *cClass;
	gchar *cWmClass;
	gchar *cName;
	gchar *cLastAttentionDemand;
	gint iAge;  // age of the window (a mere growing integer).
	gboolean bIsTransientFor;  // TRUE if the window is transient (for a parent window).
	gboolean bIsSticky;
	};


/** Register a Window Manager backend. NULL functions are simply ignored.
*@param pBackend a Window Manager backend
*/
void gldi_windows_manager_register_backend (GldiWindowManagerBackend *pBackend);

/** Run a function on each window actor.
*@param bOrderedByZ TRUE to sort by z-order, FALSE to sort by age
*@param callback the callback
*@param data user data
*/
void gldi_windows_foreach (gboolean bOrderedByZ, GFunc callback, gpointer data);

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

/// note: on Wayland, coordinates are relative to the passed widget's main surface (the wl_surface corresponding to its GdkWindow)
/// on X11, pContainerWidget is ignored
void gldi_window_set_minimize_position (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y);

/// note: on Wayland, coordinates are relative to the passed widget's main surface (the wl_surface corresponding to its GdkWindow)
/// on X11, pContainerWidget is ignored
void gldi_window_set_thumbnail_area (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y, int w, int h);

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

GldiWindowActor *gldi_window_pick (GtkWindow *pParentWindow);


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

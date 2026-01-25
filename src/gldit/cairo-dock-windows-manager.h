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

/// Additional info about DBus interfaces related to a window that might be set
typedef struct _DBusProps {
	// Windows that support the GTK interfaces
	gchar *cGTKBusName; // DBus name
	gchar *cGTKMenuBarPath;
	gchar *cGTKAppMenuPath; // not used
	gchar *cGTKWindowPath;
	gchar *cGTKAppPath;
	
	// Windows that support the com.canonical.dbusmenu interface
	gchar *cKDEServiceName; // DBus name 
	gchar *cKDEObjectPath; // object path
} GldiWindowDBusProperties;

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
	GldiWindowDBusProperties *pDBusProps; // DBus properties (if set, can be NULL)
	};


/** Run a function on each window actor.
*@param bOrderedByZ TRUE to sort by z-order, FALSE to sort by age
*@param callback the callback
*@param data user data
*/
void gldi_windows_foreach (gboolean bOrderedByZ, GFunc callback, gpointer data);
void gldi_windows_foreach_unordered (GFunc callback, gpointer data);

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

void gldi_window_set_border (GldiWindowActor *actor, gboolean bWithBorder);

void gldi_window_can_minimize_maximize_close (GldiWindowActor *actor, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose);

gboolean gldi_window_is_on_current_desktop (GldiWindowActor *actor);

gboolean gldi_window_is_on_desktop (GldiWindowActor *pAppli, int iNumDesktop, int iNumViewportX, int iNumViewportY);

void gldi_window_move_to_current_desktop (GldiWindowActor *pAppli);

guint gldi_window_get_id (GldiWindowActor *pAppli);

/** Get the DBus properties set for this window if set and supported by the backend. Currently,
 * the X11 and KWin backends support filling out the DBus name and object paths required to
 * support global menus.
 *@param actor the window whose menu is requested
 *@return a struct with the relevant DBus properties set or NULL if unset / unknown; any member may
 * be NULL if not set.
 * 
 * Note: members of the returned struct belong to the window and may change at any time. The caller
 * should make copies if it wishes to use them later.
 */
const GldiWindowDBusProperties *gldi_window_get_dbus_props (const GldiWindowActor *actor);


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
 *  valid if gldi_window_manager_have_coordinates () == TRUE as well.
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

G_END_DECLS
#endif


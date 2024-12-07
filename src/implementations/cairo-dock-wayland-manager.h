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

#ifndef __CAIRO_DOCK_WAYLAND_MANAGER__
#define  __CAIRO_DOCK_WAYLAND_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-dock-factory.h"
#include <gdk/gdk.h>

G_BEGIN_DECLS

/*
*@file cairo-dock-wayland-manager.h This class manages the interactions with Wayland.
* The Wayland manager handles signals from Wayland and dispatch them to the Windows manager and the Desktop manager.
* The Wayland manager handles signals from Wayland and dispatch them to the Windows manager and the Desktop manager.
*/

#ifndef _MANAGER_DEF_
extern GldiManager myWaylandMgr;
#endif

typedef enum {
	/// notification called when a new monitor was added, data : the GdkMonitor added
	NOTIFICATION_WAYLAND_MONITOR_ADDED = NB_NOTIFICATIONS_OBJECT,
	/// notification called when a monitor was removed, data : the GdkMonitor removed
	NOTIFICATION_WAYLAND_MONITOR_REMOVED,
	NB_NOTIFICATIONS_WAYLAND_DESKTOP
	} CairoWaylandDesktopNotifications;

void gldi_register_wayland_manager (void);

gboolean gldi_wayland_manager_have_layer_shell ();

/// Get the screen edge this dock should be anchored to
CairoDockPositionType gldi_wayland_get_edge_for_dock (const CairoDock *pDock);

/// Get the GdkMonitor a given dock is on as managed by this class
GdkMonitor *gldi_dock_wayland_get_monitor (CairoDock *pDock);

/// Get the list of monitors currently managed -- caller should not modify the GdkMonitor* pointers stored here
GdkMonitor *const *gldi_wayland_get_monitors (int *iNumMonitors);

/// Functions to try to grab / ungrab the keyboard
///  (via setting the corresponding keyboard-interactivity property in wlr-layer-shell)
/// These are not (yet?) part of the container manager backend, since
/// these work quite differently from X11 and can result in a disorienting
/// user experience. This way, these can be used only in situations where
/// necessary and where their usefulness can be properly tested.

typedef enum {
	GLDI_KEYBOARD_RELEASE_MENU_CLOSED,
	GLDI_KEYBOARD_RELEASE_PRESENT_WINDOWS
} GldiWaylandReleaseKeyboardReason;

void gldi_wayland_grab_keyboard (GldiContainer *pContainer);
void gldi_wayland_release_keyboard (GldiContainer *pContainer, GldiWaylandReleaseKeyboardReason reason);

/// Get the name of the Wayland compositor that was detected
const gchar *gldi_wayland_get_detected_compositor (void);

G_END_DECLS
#endif

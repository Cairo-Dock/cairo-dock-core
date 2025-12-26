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

#ifndef __CAIRO_DOCK_WAYLAND_MANAGER_PRIV__
#define __CAIRO_DOCK_WAYLAND_MANAGER_PRIV__

#include "cairo-dock-wayland-manager.h"

// detect and store the compositor that we are running under
// this can be used to handle quirks
typedef enum {
	WAYLAND_COMPOSITOR_NONE, // we are not running under Wayland
	WAYLAND_COMPOSITOR_UNKNOWN, // unable to detect compositor
	WAYLAND_COMPOSITOR_GENERIC, // compositor which does not require specific quirks (functionally same as UNKNOWN)
	WAYLAND_COMPOSITOR_WAYFIRE,
	WAYLAND_COMPOSITOR_KWIN,
	WAYLAND_COMPOSITOR_COSMIC,
	WAYLAND_COMPOSITOR_NIRI
} GldiWaylandCompositorType;

GldiWaylandCompositorType gldi_wayland_get_compositor_type (void);
void gldi_wayland_set_compositor_type (GldiWaylandCompositorType type);

#endif


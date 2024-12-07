/*
 * cairo-dock-wayland-hotspots.h
 * 
 * Functions to monitor screen edges for recalling a hidden dock.
 * Currently, this supports two cases:
 *   -- on Wayfire, wf-shell is used 
 *   -- as a generic case, a transparent layer-shell surface is used
 * 
 * Copyright 2021-2024 Daniel Kondor <kondor.dani@gmail.com>
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


#ifndef CAIRO_DOCK_WAYLAND_HOTSPOTS_H
#define CAIRO_DOCK_WAYLAND_HOTSPOTS_H

#ifdef HAVE_WAYLAND
#include <wayland-client.h>
#include <stdint.h>
#include <glib.h>

/// Try to match Wayland protocols that we need to use
gboolean gldi_wayland_hotspots_match_protocol (uint32_t id, const char *interface, uint32_t version);

typedef enum {
	GLDI_WAYLAND_HOTSPOTS_NONE, // we could not init support for hotspots
	GLDI_WAYLAND_HOTSPOTS_LAYER_SHELL, // we use a 1 pixel tall layer shell windows at the screen edges
	GLDI_WAYLAND_HOTSPOTS_WAYFIRE // we use wf-shell
} GldiWaylandHotspotsType;

/// Initialize the interface for monitoring hotspots
GldiWaylandHotspotsType gldi_wayland_hotspots_try_init (struct wl_registry *registry);

/// Update the hotspots we are listening to (based on the configuration of all root docks)
void gldi_wayland_hotspots_update (void);

#endif
#endif


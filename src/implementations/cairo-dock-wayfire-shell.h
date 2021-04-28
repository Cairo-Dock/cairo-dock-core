/*
 * cairo-dock-wayfire-shell.h
 * 
 * Functions to interact with Wayfire for screen edge hotspots.
 * 
 * Copyright 2021 Daniel Kondor <kondor.dani@gmail.com>
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


#ifndef CAIRO_DOCK_WAYFIRE_SHELL_H
#define CAIRO_DOCK_WAYFIRE_SHELL_H

#include <wayland-client.h>
#include <stdint.h>
#include <glib.h>

/// Try to bind the wayfire shell manager object (should be called for objects in the wl_registry)
gboolean gldi_zwf_shell_try_bind (struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version);

/// Update the hotspots we are listening to (based on the configuration of all root docks)
void gldi_zwf_shell_update_hotspots (void);

/// Stop listening to all hotspots
void gldi_zwf_manager_stop (void);

#endif



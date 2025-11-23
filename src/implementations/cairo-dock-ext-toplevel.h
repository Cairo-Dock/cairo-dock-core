/*
 * cairo-dock-cosmic-toplevel.h
 * 
 * Interact with Wayland clients via the cosmic_toplevel_info_unstable_v1
 * and cosmic_toplevel_management_unstable_v1 protocol.
 * See for more info: https://github.com/pop-os/cosmic-protocols
 * 
 * Copyright 2020-2024 Daniel Kondor <kondor.dani@gmail.com>
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
 * 
 */


#ifndef CAIRO_DOCK_EXT_TOPLEVEL_H
#define CAIRO_DOCK_EXT_TOPLEVEL_H

#include <wayland-client.h>
#include <stdint.h>

gboolean gldi_ext_toplevel_match_protocol (uint32_t id, const char *interface, uint32_t version);
gboolean gldi_ext_toplevel_try_init (struct wl_registry *registry);

#endif


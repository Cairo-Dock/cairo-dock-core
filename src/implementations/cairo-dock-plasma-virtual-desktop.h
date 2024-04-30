/*
 * cairo-dock-plasma-virtual-desktop.h -- desktop / workspace management
 *  facilities for KWin / KDE Plasma
 * 
 * Copyright 2024 Daniel Kondor <kondor.dani@gmail.com>
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

#ifndef CAIRO_DOCK_PLASMA_VIRTUAL_DESKTOP_H
#define CAIRO_DOCK_PLASMA_VIRTUAL_DESKTOP_H

#include <wayland-client.h>
#include <stdint.h>

gboolean gldi_plasma_virtual_desktop_match_protocol (uint32_t id, const char *interface, uint32_t version);
gboolean gldi_plasma_virtual_desktop_try_init (struct wl_registry *registry);

/// Try to get the index of a virtual desktop with the associated ID
/// (returns -1 if not found)
int gldi_plasma_virtual_desktop_get_index (const char *desktop_id);

/// Get the ID of the virtual desktop with the given index (or NULL if
/// out of range). The returned string is owned by the virtual desktop
/// manager (the caller must no change or free it).
const gchar *gldi_plasma_virtual_desktop_get_id (int ix);

#endif


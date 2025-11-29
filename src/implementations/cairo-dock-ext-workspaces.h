/*
 * cairo-dock-ext-workspaces.h -- desktop / workspace management
 *  facilities based on the ext-workspace Wayland protocol
 * 
 * Copyright 2024-2025 Daniel Kondor <kondor.dani@gmail.com>
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

#ifndef CAIRO_DOCK_EXT_WORKSPACES_H
#define CAIRO_DOCK_EXT_WORKSPACES_H

#include <wayland-client.h>
#include <stdint.h>
#include "wayland-ext-workspace-client-protocol.h"

extern struct wl_output *s_ws_output;

gboolean gldi_ext_workspaces_match_protocol (uint32_t id, const char *interface, uint32_t version);
gboolean gldi_ext_workspaces_try_init (struct wl_registry *registry);
struct ext_workspace_handle_v1 *gldi_ext_workspaces_get_handle (int iDesktop, int x, int y);
gboolean gldi_ext_workspaces_find (struct ext_workspace_handle_v1 *handle, int *iNbDesktop, int *x, int *y);

#endif


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

G_BEGIN_DECLS

/*
*@file cairo-dock-wayland-manager.h This class manages the interactions with Wayland.
* The Wayland manager handles signals from Wayland and dispatch them to the Windows manager and the Desktop manager.
* The Wayland manager handles signals from Wayland and dispatch them to the Windows manager and the Desktop manager.
*/


void gldi_register_wayland_manager (void);

/// Get the screen edge this dock should be anchored to
CairoDockPositionType gldi_wayland_get_edge_for_dock (const CairoDock *pDock);

G_END_DECLS
#endif

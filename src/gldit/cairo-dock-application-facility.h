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

#ifndef __CAIRO_DOCK_APPLICATION_FACILITY__
#define  __CAIRO_DOCK_APPLICATION_FACILITY__

#include <glib.h>
#include <X11/Xlib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cairo_dock_appli_demands_attention (Icon *icon);
void cairo_dock_appli_stops_demanding_attention (Icon *icon);

void cairo_dock_animate_icon_on_active (Icon *icon, CairoDock *pParentDock);


gboolean cairo_dock_appli_covers_dock (Icon *pIcon, CairoDock *pDock);
gboolean cairo_dock_appli_overlaps_dock (Icon *pIcon, CairoDock *pDock);


CairoDock *cairo_dock_insert_appli_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bAnimate);
CairoDock *cairo_dock_detach_appli (Icon *pIcon);


void cairo_dock_set_one_icon_geometry_for_window_manager (Icon *icon, CairoDock *pDock);
void cairo_dock_reserve_one_icon_geometry_for_window_manager (Window *Xid, Icon *icon, CairoDock *pMainDock);


gboolean cairo_dock_appli_is_on_desktop (Icon *pIcon, int iNumDesktop, int iNumViewportX, int iNumViewportY);
gboolean cairo_dock_appli_is_on_current_desktop (Icon *pIcon);

void cairo_dock_move_window_to_desktop (Icon *pIcon, int iNumDesktop, int iNumViewportX, int iNumViewportY);
void cairo_dock_move_window_to_current_desktop (Icon *pIcon);


const CairoDockImageBuffer *cairo_dock_appli_get_image_buffer (Icon *pIcon);

G_END_DECLS
#endif

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

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/*
*@file cairo-dock-application-facility.h A set of utilities for handling appli-icons.
*/

void gldi_appli_icon_demands_attention (Icon *icon);  // applications-manager

void gldi_appli_icon_stop_demanding_attention (Icon *icon);  // applications-manager

void gldi_appli_icon_animate_on_active (Icon *icon, CairoDock *pParentDock);  // applications-manager


CairoDock *gldi_appli_icon_insert_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bAnimate);

CairoDock *gldi_appli_icon_detach (Icon *pIcon);


void gldi_appli_icon_set_geometry_for_window_manager (Icon *icon, CairoDock *pDock);

void gldi_appli_reserve_geometry_for_window_manager (GldiWindowActor *pAppli, Icon *icon, CairoDock *pMainDock);  // applications-manager


const CairoDockImageBuffer *gldi_appli_icon_get_image_buffer (Icon *pIcon);


void gldi_window_inhibitors_set_name (GldiWindowActor *actor, const gchar *cNewName);  // applications-manager

void gldi_window_inhibitors_set_active_state (GldiWindowActor *actor, gboolean bActive);  // applications-manager

void gldi_window_inhibitors_set_hidden_state (GldiWindowActor *actor, gboolean bIsHidden);  // applications-manager


G_END_DECLS
#endif

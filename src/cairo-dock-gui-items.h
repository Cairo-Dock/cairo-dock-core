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


#ifndef __CAIRO_DOCK_GUI_LAUNCHER__
#define  __CAIRO_DOCK_GUI_LAUNCHER__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS


CairoDockGroupKeyWidget *cairo_dock_gui_items_get_widget_from_name (CairoDockModuleInstance *pInstance, const gchar *cGroupName, const gchar *cKeyName);

void cairo_dock_gui_items_update_desklet_params (CairoDesklet *pDesklet);

void cairo_dock_gui_items_update_module_instance_container (CairoDockModuleInstance *pInstance, gboolean bDetached);

void cairo_dock_update_desklet_visibility_params (CairoDesklet *pDesklet);

void cairo_dock_gui_items_reload_current_widget (CairoDockModuleInstance *pInstance, int iShowPage);

void cairo_dock_gui_items_set_status_message_on_gui (const gchar *cMessage);

void cairo_dock_register_default_items_gui_backend (void);


G_END_DECLS
#endif

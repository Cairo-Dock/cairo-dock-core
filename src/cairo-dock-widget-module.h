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


#ifndef __CAIRO_DOCK_WIDGET_MODULE__
#define  __CAIRO_DOCK_WIDGET_MODULE__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
#include "cairo-dock-widget.h"
G_BEGIN_DECLS


typedef struct _ModuleWidget ModuleWidget;

struct _ModuleWidget {
	CDWidget widget;
	CairoDockModule *pModule;
	CairoDockModuleInstance *pModuleInstance;
	gchar *cConfFilePath;
};
#define MODULE_WIDGET(w) ((ModuleWidget*)(w))

#define IS_MODULE_WIDGET(w) (w && CD_WIDGET(w)->iType == WIDGET_MODULE)


ModuleWidget *cairo_dock_module_widget_new (CairoDockModule *pModule, CairoDockModuleInstance *pInstance);


void cairo_dock_module_widget_update_desklet_params (ModuleWidget *pModuleWidget, CairoDesklet *pDesklet);


void cairo_dock_module_widget_update_desklet_visibility_params (ModuleWidget *pModuleWidget, CairoDesklet *pDesklet);


void cairo_dock_module_widget_update_module_instance_container (ModuleWidget *pModuleWidget, CairoDockModuleInstance *pInstance, gboolean bDetached);


void cairo_dock_module_widget_reload_current_widget (ModuleWidget *pModuleWidget, CairoDockModuleInstance *pInstance, int iShowPage);


G_END_DECLS
#endif

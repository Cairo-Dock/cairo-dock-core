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

#ifndef __CAIRO_DOCK_GUI_SWITCH__
#define  __CAIRO_DOCK_GUI_SWITCH__

#include <gtk/gtk.h>
G_BEGIN_DECLS

// Definition of the main GUI interface.
struct _CairoDockMainGuiBackend {
	// Show the main config panel, build it if necessary.
	GtkWidget * (*show_main_gui) (void);
	// Show the config panel of a given module (internal or external), reload it if it was already opened.
	void (*show_module_gui) (const gchar *cModuleName);
	// Show the config panel of a given module instance, reload it if it was already opened. iShowPage is the page that should be displayed in case the module has several pages, -1 means to keep the current page.
	//void (*show_module_instance_gui) (CairoDockModuleInstance *pModuleInstance, int iShowPage);
	// close the config panels.
	void (*close_gui) (void);
	// update the GUI to mark a module as '(in)active'.
	void (*update_module_state) (const gchar *cModuleName, gboolean bActive);
	void (*update_module_instance_container) (CairoDockModuleInstance *pInstance, gboolean bDetached);
	void (*update_desklet_params) (CairoDesklet *pDesklet);
	void (*update_desklet_visibility_params) (CairoDesklet *pDesklet);
	void (*update_modules_list) (void);
	void (*update_shortkeys) (void);
	gboolean bCanManageThemes;
	const gchar *cDisplayedName;
	const gchar *cTooltip;
	} ;
typedef struct _CairoDockMainGuiBackend CairoDockMainGuiBackend;

// Definition of the icons GUI interface.
struct _CairoDockItemsGuiBackend {
	// Show the config panel on a given icon/container, build or reload it if necessary.
	GtkWidget * (*show_gui) (Icon *pIcon, CairoContainer *pContainer, CairoDockModuleInstance *pModuleInstance, int iShowPage);
	// reload the gui and its content, for the case a launcher has changed (image, order, new container, etc).
	void (*reload_items) (void);
	} ;
typedef struct _CairoDockItemsGuiBackend CairoDockItemsGuiBackend;


void cairo_dock_load_user_gui_backend (int iMode);

GtkWidget *cairo_dock_make_switch_gui_button (void);

gboolean cairo_dock_theme_manager_is_integrated (void);



void cairo_dock_gui_trigger_update_desklet_params (CairoDesklet *pDesklet);

void cairo_dock_gui_trigger_update_desklet_visibility (CairoDesklet *pDesklet);

void cairo_dock_gui_trigger_reload_items (void);

void cairo_dock_gui_trigger_update_module_state (const gchar *cModuleName);

void cairo_dock_gui_trigger_update_modules_list (void);

void cairo_dock_gui_trigger_update_module_container (CairoDockModuleInstance *pInstance, gboolean bIsDetached);

void cairo_dock_gui_trigger_reload_shortkeys (void);


void cairo_dock_register_config_gui_backend (CairoDockMainGuiBackend *pBackend);

void cairo_dock_register_items_gui_backend (CairoDockItemsGuiBackend *pBackend);


GtkWidget *cairo_dock_show_main_gui (void);

//void cairo_dock_show_module_instance_gui (CairoDockModuleInstance *pModuleInstance, int iShowPage);

void cairo_dock_show_module_gui (const gchar *cModuleName);

void cairo_dock_close_gui (void);

void cairo_dock_show_items_gui (Icon *pIcon, CairoContainer *pContainer, CairoDockModuleInstance *pModuleInstance, int iShowPage);


G_END_DECLS
#endif

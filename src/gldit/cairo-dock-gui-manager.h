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


#ifndef __CAIRO_DOCK_GUI_FACILITY__
#define  __CAIRO_DOCK_GUI_FACILITY__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"
G_BEGIN_DECLS

/** @file cairo-dock-gui-manager.h This class provides functions to act on configuration windows.
* 
* It also defines the interface that a GUI backend should implement.
* 
* Note: GUIs are built from a .conf file; .conf files are normal group/key files, but with some special indications in the comments. Each key will be represented by a pre-defined widget, that is defined by the first caracter of its comment. The comment also contains a description of the key, and an optionnal tooltip. See cairo-dock-gui-factory.h for the list of pre-defined widgets and a short explanation on how to use them inside a conf file. The file 'cairo-dock.conf' can be an useful example.
*/

// manager
/// Definition of the callback called when the user apply the config panel.
typedef gboolean (* CairoDockApplyConfigFunc) (gpointer data);
typedef void (* CairoDockLoadCustomWidgetFunc) (GtkWidget *pWindow, GKeyFile *pKeyFile, GSList *pWidgetList);
typedef void (* CairoDockSaveCustomWidgetFunc) (GtkWidget *pWindow, GKeyFile *pKeyFile, GSList *pWidgetList);

struct _CairoGuiManager {
	GldiManager mgr;
	CairoDockGroupKeyWidget* (*get_group_key_widget_from_name) (const gchar *cGroupName, const gchar *cKeyName);
	
	GtkWidget* (*build_generic_gui_window) (const gchar *cTitle, int iWidth, int iHeight, CairoDockApplyConfigFunc pAction, gpointer pUserData, GFreeFunc pFreeUserData);
	GtkWidget* (*build_generic_gui_full) (const gchar *cConfFilePath, const gchar *cGettextDomain, const gchar *cTitle, int iWidth, int iHeight, CairoDockApplyConfigFunc pAction, gpointer pUserData, GFreeFunc pFreeUserData, CairoDockLoadCustomWidgetFunc load_custom_widgets, CairoDockSaveCustomWidgetFunc save_custom_widgets);
	void (*reload_generic_gui) (GtkWidget *pWindow);
	} ;

// signals
typedef enum {
	NOTIFICATION_SHOW_MODULE_INSTANCE_GUI = NB_NOTIFICATIONS_OBJECT,
	NOTIFICATION_STATUS_MESSAGE,
	NOTIFICATION_GET_GROUP_KEY_WIDGET,
	NB_NOTIFICATIONS_GUI
	} CairoGuiNotifications;


/// Definition of the GUI interface for modules.
struct _CairoDockGuiBackend {
	/// display a message on the GUI.
	void (*set_status_message_on_gui) (const gchar *cMessage);
	/// Reload the current config window from the conf file. iShowPage is the page that should be displayed in case the module has several pages, -1 means to keep the current page.
	void (*reload_current_widget) (CairoDockModuleInstance *pModuleInstance, int iShowPage);
	void (*show_module_instance_gui) (CairoDockModuleInstance *pModuleInstance, int iShowPage);
	/// retrieve the widgets in the current module window, corresponding to the (group,key) pair in its conf file.
	CairoDockGroupKeyWidget * (*get_widget_from_name) (CairoDockModuleInstance *pModuleInstance, const gchar *cGroupName, const gchar *cKeyName);
	} ;
typedef struct _CairoDockGuiBackend CairoDockGuiBackend;


#define CAIRO_DOCK_FRAME_MARGIN 6


void cairo_dock_register_gui_backend (CairoDockGuiBackend *pBackend);


void cairo_dock_reload_current_widget_full (CairoDockModuleInstance *pModuleInstance, int iShowPage);

/**Reload the widget of a given module instance if it is currently opened (the current page is displayed). This is useful if the module has modified its conf file and wishes to display the changes.
@param pModuleInstance an instance of a module.
*/
#define cairo_dock_reload_current_module_widget(pModuleInstance) cairo_dock_reload_current_widget_full (pModuleInstance, -1)
#define cairo_dock_reload_current_module_widget_full cairo_dock_reload_current_widget_full

void cairo_dock_show_module_instance_gui (CairoDockModuleInstance *pModuleInstance, int iShowPage);

/** Display a message on a given window that has a status-bar. If no window is provided, the current config panel will be used.
@param pWindow window where the message should be displayed, or NULL to target the config panel.
@param cMessage the message.
*/
void cairo_dock_set_status_message (GtkWidget *pWindow, const gchar *cMessage);
/** Display a message on a given window that has a status-bar. If no window is provided, the current config panel will be used.
@param pWindow window where the message should be displayed, or NULL to target the config panel.
@param cFormat the message, in a printf-like format
@param ... arguments of the format.
*/
void cairo_dock_set_status_message_printf (GtkWidget *pWindow, const gchar *cFormat, ...) G_GNUC_PRINTF (2, 3);


G_END_DECLS
#endif

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


#ifndef __CAIRO_DOCK_GUI_MANAGER__
#define  __CAIRO_DOCK_GUI_MANAGER__

#include <gtk/gtk.h>
G_BEGIN_DECLS

/** @file cairo-dock-gui-manager.h This class manages the config panels of Cairo-Dock.
* 
* GUIs are built from a .conf file; .conf files are normal group/key files, but with some special indications in the comments. Each key will be represented by a pre-defined widget, that is defined by the first caracter of its comment. The comment also contains a description of the key, and an optionnal tooltip. See cairo-dock-gui-factory.h for the list of pre-defined widgets and a short explanation on how to use them inside a conf file. The file 'cairo-dock.conf' can be an useful example.
* 
* The class defines the interface that a backend to the main GUI of Cairo-Dock should implement.
* It also provides a useful function to easily build a window from a conf file : \ref cairo_dock_build_generic_gui
* 
*/

/// Definition of the GUI interface.
struct _CairoDockGuiBackend {
	/// display a message on the GUI.
	void (*set_status_message_on_gui) (const gchar *cMessage);
	/// Show the main config panel, build it if necessary.
	GtkWidget * (*show_main_gui) (void);
	/// Show the config panel of a given module (internal or external), reload it if it was already opened.
	void (*show_module_gui) (const gchar *cModuleName);
	/// Show the config panel of a given module instance, reload it if it was already opened. iShowPage is the page that should be displayed in case the module has several pages, -1 means to keep the current page.
	void (*show_module_instance_gui) (CairoDockModuleInstance *pModuleInstance, int iShowPage);
	/// say whether the config of a module instance is currently opened.
	gboolean (*module_is_opened) (CairoDockModuleInstance *pModuleInstance);
	/// update the GUI to mark a module as 'inactive'.
	void (*deactivate_module_in_gui) (const gchar *cModuleName);
	/// retrieve the widgets in the current module window, corresponding to the (group,key) pair in its conf file.
	GSList * (*get_widgets_from_name) (const gchar *cGroupName, const gchar *cKeyName);
	/// close the config panels.
	void (*close_gui) (void);
	} ;
typedef struct _CairoDockGuiBackend CairoDockGuiBackend;

/// Definition of the callback called when the user apply the config panel.
typedef gboolean (* CairoDockApplyConfigFunc) (gpointer data);


int cairo_dock_get_nb_dialog_windows (void);
void cairo_dock_dialog_window_destroyed (void);
void cairo_dock_dialog_window_created (void);

/**Retrieve the widget in the current config panel, corresponding to the (group,key) pair in its conf file
@param cGroupName name of the group in the conf file.
@param cKeyName name of the key in the conf file.
@return the widget that match the group and key, or NULL if none was found.
*/
GtkWidget *cairo_dock_get_widget_from_name (const gchar *cGroupName, const gchar *cKeyName);
/**Reload the widget of a given module instance if it is currently opened. This is useful if the module has modified its conf file and wishes to display the changes.
@param pInstance an instance of a module.
@param iShowPage number of the page to display (it corresponds to the nth group in the conf file), or -1 to display the current page.
*/
void cairo_dock_reload_current_module_widget_full (CairoDockModuleInstance *pInstance, int iShowPage);
/**Reload the widget of a given module instance if it is currently opened (the current page is displayed). This is useful if the module has modified its conf file and wishes to display the changes.
@param pInstance an instance of a module.
*/
#define cairo_dock_reload_current_module_widget(pInstance) cairo_dock_reload_current_module_widget_full (pInstance, -1)

void cairo_dock_deactivate_module_in_gui (const gchar *cModuleName);
void cairo_dock_update_desklet_size_in_gui (CairoDockModuleInstance *pModuleInstance, int iWidth, int iHeight);
void cairo_dock_update_desklet_position_in_gui (CairoDockModuleInstance *pModuleInstance, int x, int y);
void cairo_dock_update_desklet_detached_state_in_gui (CairoDockModuleInstance *pModuleInstance, gboolean bIsDetached);

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


/** Register a GUI backend to be the current one.
@param pBackend the new backend.
*/
void cairo_dock_register_gui_backend (CairoDockGuiBackend *pBackend);

/** Show the main config panel.
*/
GtkWidget *cairo_dock_show_main_gui (void);
/** Show the config panel of a given module instance. Reload the widgets if the panel was already opened.
@param pModuleInstance the module instance
@param iShowPage the specific page to display, or -1 to keep the current one if the instance is already opened.
*/
void cairo_dock_show_module_instance_gui (CairoDockModuleInstance *pModuleInstance, int iShowPage);
/** Show the config panel of a given module (internal or external). Reload the widgets if the panel was already opened.
@param cModuleName name of the module.
*/
void cairo_dock_show_module_gui (const gchar *cModuleName);
/** Close the config panel(s).
*/
void cairo_dock_close_gui (void);


/** Build a generic GUI window, that contains 2 buttons apply and quit, and a statusbar.
@param cTitle title to set to the window.
@param iWidth width of the window.
@param iHeight height of the window.
@param pAction callback to be called when the apply button is pressed, or NULL.
@param pUserData data passed to the previous callback, or NULL.
@param pFreeUserData callback called when the window is destroyed, to free the previous data, or NULL.
@return the new window.
*/
GtkWidget *cairo_dock_build_generic_gui_window (const gchar *cTitle, int iWidth, int iHeight, CairoDockApplyConfigFunc pAction, gpointer pUserData, GFreeFunc pFreeUserData);
/** Load a conf file into a generic window, that contains 2 buttons apply and quit, and a statusbar. If no callback is provided, the window is blocking, until the user press one of the button; in this case, TRUE is returned if ok was pressed.
@param cConfFilePath conf file to load into the window.
@param cGettextDomain translation domain.
@param cTitle title to set to the window.
@param iWidth width of the window.
@param iHeight height of the window.
@param pAction callback to be called when the apply button is pressed, or NULL.
@param pUserData data passed to the previous callback, or NULL.
@param pFreeUserData callback called when the window is destroyed, to free the previous data, or NULL.
@param pWindow pointer to the newly created window.
@return the new window.
*/
gboolean cairo_dock_build_generic_gui (const gchar *cConfFilePath, const gchar *cGettextDomain, const gchar *cTitle, int iWidth, int iHeight, CairoDockApplyConfigFunc pAction, gpointer pUserData, GFreeFunc pFreeUserData, GtkWidget **pWindow);

/** Reload a generic window built upon a conf file, by parsing again the conf file.
@param pWindow the window.
*/
void cairo_dock_reload_generic_gui (GtkWidget *pWindow);



/// Definition of the launcher GUI interface.
struct _CairoDockLauncherGuiBackend {
	/// Show the config panel on a given launcher, build or reload it if necessary.
	GtkWidget * (*show_gui) (Icon *pIcon);
	/// reload the gui and its content, for the case a launcher has changed (image, order, new container, etc).
	void (*refresh_gui) (void);
	} ;
typedef struct _CairoDockLauncherGuiBackend CairoDockLauncherGuiBackend;

/** Register a launcher GUI backend to be the current one.
@param pBackend the new backend.
*/
void cairo_dock_register_launcher_gui_backend (CairoDockLauncherGuiBackend *pBackend);

/** Build and show the launcher GUI for a given launcher.
@param pIcon the launcher.
@return the GUI window.
*/
GtkWidget *cairo_dock_build_launcher_gui (Icon *pIcon);

/** Trigger the refresh of the launcher GUI. The refresh well happen when the main loop gets available.
*/
void cairo_dock_trigger_refresh_launcher_gui (void);


void cairo_dock_load_user_gui_backend (void);
GtkWidget *cairo_dock_make_switch_gui_button (void);


G_END_DECLS
#endif

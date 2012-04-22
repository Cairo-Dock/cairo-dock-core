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

/** @file cairo-dock-gui-manager.h This class provides useful functions to build config panels from keyfiles.
* 
* GUIs are built from a .conf file; .conf files are normal group/key files, but with some special indications in the comments. Each key will be represented by a pre-defined widget, that is defined by the first caracter of its comment. The comment also contains a description of the key, and an optionnal tooltip. See cairo-dock-gui-factory.h for the list of pre-defined widgets and a short explanation on how to use them inside a conf file. The file 'cairo-dock.conf' can be an useful example.
* 
* The class defines the interface that a backend to the main GUI of Cairo-Dock should implement.
* It also provides a useful function to easily build a window from a conf file : \ref cairo_dock_build_generic_gui
*/

// manager
/// Definition of the callback called when the user apply the config panel.
typedef gboolean (* CairoDockApplyConfigFunc) (gpointer data);
typedef void (* CairoDockLoadCustomWidgetFunc) (GtkWidget *pWindow, GKeyFile *pKeyFile);
typedef void (* CairoDockSaveCustomWidgetFunc) (GtkWidget *pWindow, GKeyFile *pKeyFile);

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

/**Retrieve the group-key widget in the current config panel, corresponding to the (group,key) pair in its conf file.
@param pModuleInstance the applet making the demand.
@param cGroupName name of the group in the conf file.
@param cKeyName name of the key in the conf file.
@return the group-key widget that match the group and key, or NULL if none was found.
*/
CairoDockGroupKeyWidget *cairo_dock_get_group_key_widget_from_name (CairoDockModuleInstance *pModuleInstance, const gchar *cGroupName, const gchar *cKeyName);

/** A mere wrapper around the previous function, that returns directly the GTK widget corresponding to the (group,key). Note that empty widgets will return NULL, so you can't you can't distinguish between an empty widget and an inexisant widget.
@param pModuleInstance the applet making the demand.
@param cGroupName name of the group in the conf file.
@param cKeyName name of the key in the conf file.
@return the widget that match the group and key, or NULL if the widget is empty or if none was found.
*/
GtkWidget *cairo_dock_get_widget_from_name (CairoDockModuleInstance *pModuleInstance, const gchar *cGroupName, const gchar *cKeyName);

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

GtkWidget *cairo_dock_build_generic_gui_full (const gchar *cConfFilePath, const gchar *cGettextDomain, const gchar *cTitle, int iWidth, int iHeight, CairoDockApplyConfigFunc pAction, gpointer pUserData, GFreeFunc pFreeUserData, CairoDockLoadCustomWidgetFunc load_custom_widgets, CairoDockSaveCustomWidgetFunc save_custom_widgets);

/** Load a conf file into a generic window, that contains 2 buttons apply and quit, and a statusbar. If no callback is provided, the window is blocking, until the user press one of the button; in this case, TRUE is returned if ok was pressed.
@param cConfFilePath conf file to load into the window.
@param cGettextDomain translation domain.
@param cTitle title to set to the window.
@param iWidth width of the window.
@param iHeight height of the window.
@param pAction callback to be called when the apply button is pressed, or NULL.
@param pUserData data passed to the previous callback, or NULL.
@param pFreeUserData callback called when the window is destroyed, to free the previous data, or NULL.
@return the new window.
*/
#define cairo_dock_build_generic_gui(cConfFilePath, cGettextDomain, cTitle, iWidth, iHeight, pAction, pUserData, pFreeUserData) cairo_dock_build_generic_gui_full (cConfFilePath, cGettextDomain, cTitle, iWidth, iHeight, pAction, pUserData, pFreeUserData, NULL, NULL)

/** Reload a generic window built upon a conf file, by parsing again the conf file.
@param pWindow the window.
*/
void cairo_dock_reload_generic_gui (GtkWidget *pWindow);


G_END_DECLS
#endif

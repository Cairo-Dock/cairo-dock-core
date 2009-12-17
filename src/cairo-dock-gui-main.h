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

#ifndef __CAIRO_DOCK_GUI_MAIN__
#define  __CAIRO_DOCK_GUI_MAIN__

#include <gtk/gtk.h>
#include <cairo-dock-struct.h>
G_BEGIN_DECLS

struct _CairoDockCategoryWidgetTable {
	GtkWidget *pFrame;
	GtkWidget *pTable;
	gint iNbRows;
	gint iNbItemsInCurrentRow;
	GtkToolItem *pCategoryButton;
	} ;

struct _CairoDockGroupDescription {
	gchar *cGroupName;
	gchar *cTitle;
	gint iCategory;
	gchar *cDescription;
	gchar *cPreviewFilePath;
	GtkWidget *pActivateButton;
	GtkWidget *pLabel;
	gchar *cOriginalConfFilePath;
	gchar *cIcon;
	gchar *cConfFilePath;
	const gchar *cGettextDomain;
	void (* load_custom_widget) (CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
	const gchar **cDependencies;
	gboolean bIgnoreDependencies;
	GList *pExternalModules;
	const gchar *cInternalModule;
	gboolean bMatchFilter;
	} ;

typedef struct _CairoDockGroupDescription CairoDockGroupDescription;
typedef struct _CairoDockCategoryWidgetTable CairoDockCategoryWidgetTable;


/**void on_click_category_button (GtkButton *button, gpointer data);
void on_click_all_button (GtkButton *button, gpointer data);
void on_click_back_button (GtkButton *button, gpointer data);

void on_click_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription);
void on_enter_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription);
void on_leave_group_button (GtkButton *button, gpointer *data);


void on_click_apply (GtkButton *button, GtkWidget *pWindow);
void on_click_ok (GtkButton *button, GtkWidget *pWindow);
void on_click_quit (GtkButton *button, GtkWidget *pWindow);
gboolean on_delete_main_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop);

void on_click_activate_given_group (GtkToggleButton *button, CairoDockGroupDescription *pGroupDescription);
void on_click_activate_current_group (GtkToggleButton *button, gpointer *data);*/

/**
void cairo_dock_reset_filter_state (void);
void cairo_dock_activate_filter (GtkEntry *pEntry, gpointer data);
void cairo_dock_toggle_all_words (GtkCheckMenuItem *pMenuItem, gpointer data);
void cairo_dock_toggle_search_in_tooltip (GtkCheckMenuItem *pMenuItem, gpointer data);
void cairo_dock_toggle_highlight_words (GtkCheckMenuItem *pMenuItem, gpointer data);
void cairo_dock_toggle_hide_others (GtkCheckMenuItem *pMenuItem, gpointer data);
void cairo_dock_clear_filter (GtkButton *pButton, GtkEntry *pEntry);*/


GtkWidget *cairo_dock_build_main_ihm (const gchar *cConfFilePath, gboolean bMaintenanceMode);


GtkWidget *cairo_dock_get_preview_image (int *iPreviewWidth);
GtkWidget *cairo_dock_get_main_window (void);
CairoDockGroupDescription *cairo_dock_get_current_group (void);
gpointer cairo_dock_get_current_widget (void);


void cairo_dock_hide_all_categories (void);
void cairo_dock_show_all_categories (void);
void cairo_dock_show_one_category (int iCategory);
void cairo_dock_toggle_category_button (int iCategory);
void cairo_dock_insert_extern_widget_in_gui (GtkWidget *pWidget);
GtkWidget *cairo_dock_present_group_widget (const gchar *cConfFilePath, CairoDockGroupDescription *pGroupDescription, gboolean bSingleGroup, CairoDockModuleInstance *pInstance);
CairoDockGroupDescription *cairo_dock_find_module_description (const gchar *cModuleName);
void cairo_dock_present_module_gui (CairoDockModule *pModule);
void cairo_dock_present_module_instance_gui (CairoDockModuleInstance *pModuleInstance);
void cairo_dock_show_group (CairoDockGroupDescription *pGroupDescription);

void cairo_dock_free_categories (void);

void cairo_dock_write_current_group_conf_file (gchar *cConfFilePath, CairoDockModuleInstance *pInstance);
void cairo_dock_write_extra_group_conf_file (gchar *cConfFilePath, CairoDockModuleInstance *pInstance, int iNumExtraModule);


gpointer cairo_dock_get_previous_widget (void);


void cairo_dock_apply_current_filter (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther);
/** Trigger the filter, according to the current text and filter's options.
*/
void cairo_dock_trigger_current_filter (void);


void cairo_dock_register_main_gui_backend (void);


G_END_DECLS
#endif


#ifndef __CAIRO_DOCK_GUI_MANAGER__
#define  __CAIRO_DOCK_GUI_MANAGER__

#include <gtk/gtk.h>
G_BEGIN_DECLS


struct _CairoDockCategoryWidgetTable {
	GtkWidget *pFrame;
	GtkWidget *pTable;
	gint iNbRows;
	gint iNbItemsInCurrentRow;
	} ;

struct _CairoDockGroupDescription {
	gchar *cGroupName;
	gint iCategory;
	gchar *cDescription;
	gchar *cPreviewFilePath;
	GtkWidget *pActivateButton;
	GtkWidget *pLabel;
	gchar *cOriginalConfFilePath;
	gchar *cIcon;
	gchar *cConfFilePath;
	gchar *cGettextDomain;
	void (* load_custom_widget) (CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
	const gchar **cDependencies;
	gboolean bIgnoreDependencies;
	} ;

typedef struct _CairoDockGroupDescription CairoDockGroupDescription;
typedef struct _CairoDockCategoryWidgetTable CairoDockCategoryWidgetTable;


int cairo_dock_get_nb_config_panels (void);
void cairo_dock_config_panel_destroyed (void);
void cairo_dock_config_panel_created (void);

GtkWidget *cairo_dock_build_main_ihm (gchar *cConfFilePath, gboolean bMaintenanceMode);


GtkWidget *cairo_dock_get_preview_image (int *iPreviewWidth);
GtkWidget *cairo_dock_get_main_window (void);
CairoDockGroupDescription *cairo_dock_get_current_group (void);
GSList *cairo_dock_get_current_widget_list (void);
gpointer cairo_dock_get_current_widget (void);


void cairo_dock_hide_all_categories (void);
void cairo_dock_show_all_categories (void);
void cairo_dock_show_one_category (int iCategory);
void cairo_dock_insert_extern_widget_in_gui (GtkWidget *pWidget);
GtkWidget *cairo_dock_present_group_widget (gchar *cConfFilePath, CairoDockGroupDescription *pGroupDescription, gboolean bSingleGroup, CairoDockModuleInstance *pInstance);
CairoDockGroupDescription *cairo_dock_find_module_description (const gchar *cModuleName);
void cairo_dock_present_module_gui (CairoDockModule *pModule);
void cairo_dock_present_module_instance_gui (CairoDockModuleInstance *pModuleInstance);
void cairo_dock_show_group (CairoDockGroupDescription *pGroupDescription);

void cairo_dock_free_categories (void);

void cairo_dock_write_current_group_conf_file (gchar *cConfFilePath, CairoDockModuleInstance *pInstance);


gboolean cairo_dock_build_normal_gui (gchar *cConfFilePath, const gchar *cGettextDomain, const gchar *cTitle, int iWidth, int iHeight, CairoDockApplyConfigFunc pAction, gpointer pUserData, GFreeFunc pFreeUserData);


gpointer cairo_dock_get_previous_widget (void);


void cairo_dock_reload_current_group_widget (CairoDockModuleInstance *pInstance);

GtkWidget *cairo_dock_get_widget_from_name (const gchar *cGroupName, const gchar *cKeyName);


GList *cairo_dock_search_keywords_in_group_names (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip);

void cairo_dock_apply_current_filter (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther);
void cairo_dock_trigger_current_filter (void);


G_END_DECLS
#endif

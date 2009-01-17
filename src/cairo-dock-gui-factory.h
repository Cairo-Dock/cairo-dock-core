
#ifndef __CAIRO_DOCK_GUI_FACTORY__
#define  __CAIRO_DOCK_GUI_FACTORY__

#include <gtk/gtk.h>
G_BEGIN_DECLS


void cairo_dock_build_renderer_list_for_gui (GHashTable *pHashTable);
void cairo_dock_build_desklet_decorations_list_for_gui (GHashTable *pHashTable);
void cairo_dock_build_desklet_decorations_list_for_applet_gui (GHashTable *pHashTable);
void cairo_dock_build_dialog_decorator_list_for_gui (GHashTable *pHashTable);


GtkWidget *cairo_dock_build_group_widget (GKeyFile *pKeyFile, const gchar *cGroupName, GtkTooltips *pToolTipsGroup, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath);

GtkWidget *cairo_dock_build_conf_file_widget (const gchar *cConfFilePath, GtkTooltips *pToolTipsGroup, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath);


void cairo_dock_update_keyfile_from_widget_list (GKeyFile *pKeyFile, GSList *pWidgetList);


void cairo_dock_free_generated_widget_list (GSList *pWidgetList);


GtkWidget *cairo_dock_find_widget_from_name (GSList *pWidgetList, const gchar *cGroupName, const gchar *cKeyName);

void cairo_dock_build_animations_list_for_gui (GHashTable *pHashTable);


G_END_DECLS
#endif

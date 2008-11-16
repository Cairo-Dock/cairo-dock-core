
#ifndef __CAIRO_DOCK_KEYFILE_MANAGER__
#define  __CAIRO_DOCK_KEYFILE_MANAGER__

#include <glib.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cairo_dock_write_keys_to_file (GKeyFile *pKeyFile, const gchar *cConfFilePath);
void cairo_dock_flush_conf_file_full (GKeyFile *pKeyFile, gchar *cConfFilePath, gchar *cShareDataDirPath, gboolean bUseFileKeys, gchar *cTemplateFileName);
#define cairo_dock_flush_conf_file(pKeyFile, cConfFilePath, cShareDataDirPath, cTemplateFileName) cairo_dock_flush_conf_file_full (pKeyFile, cConfFilePath, cShareDataDirPath, TRUE, cTemplateFileName)

void cairo_dock_replace_key_values (GKeyFile *pOriginalKeyFile, GKeyFile *pReplacementKeyFile, gboolean bUseOriginalKeys, gchar iIdentifier);

gchar *cairo_dock_write_table_content (GHashTable *pHashTable, GHFunc pWritingFunc, gboolean bSortByKey, gboolean bAddEmptyEntry);
void cairo_dock_write_one_name (gchar *cName, gpointer value, GString *pString);
void cairo_dock_write_one_name_description (gchar *cName, gchar *cDescriptionFilePath, GString *pString);
void cairo_dock_write_one_module_name (gchar *cName, CairoDockModule *pModule, GString *pString);
void cairo_dock_write_one_theme_name (gchar *cName, gchar *cThemePath, GString *pString);
void cairo_dock_write_one_renderer_name (gchar *cName, CairoDockRenderer *pRenderer, GString *pString);
void cairo_dock_update_conf_file_with_hash_table (GKeyFile *pOpenedKeyFile, gchar *cConfFile, GHashTable *pModuleTable, gchar *cGroupName, gchar *cKeyName, gchar *cNewUsefullComment, GHFunc pWritingFunc, gboolean bSortByKey, gboolean bAddEmptyEntry);
void cairo_dock_update_conf_file_with_list (GKeyFile *pOpenedKeyFile, gchar *cConfFile, gchar *cList, gchar *cGroupName, gchar *cKeyName, gchar *cNewUsefullComment);

void cairo_dock_replace_values_in_conf_file (gchar *cConfFilePath, GKeyFile *pValidKeyFile, gboolean bUseFileKeys, gchar iIdentifier);
void cairo_dock_replace_keys_by_identifier (gchar *cConfFilePath, gchar *cReplacementConfFilePath, gchar iIdentifier);

void cairo_dock_get_conf_file_version (GKeyFile *pKeyFile, gchar **cConfFileVersion);
gboolean cairo_dock_conf_file_needs_update (GKeyFile *pKeyFile, gchar *cVersion);

void cairo_dock_add_remove_element_to_key (const gchar *cConfFilePath, const gchar *cGroupName, const gchar *cKeyName, gchar *cElementName, gboolean bAdd);


G_END_DECLS
#endif

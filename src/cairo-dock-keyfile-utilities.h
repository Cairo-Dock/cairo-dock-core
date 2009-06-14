
#ifndef __CAIRO_DOCK_KEYFILE_UTILITIES__
#define  __CAIRO_DOCK_KEYFILE_UTILITIES__

#include <glib.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS


GKeyFile *cairo_dock_open_key_file (const gchar *cConfFilePath);

void cairo_dock_write_keys_to_file (GKeyFile *pKeyFile, const gchar *cConfFilePath);

void cairo_dock_flush_conf_file_full (GKeyFile *pKeyFile, const gchar *cConfFilePath, const gchar *cShareDataDirPath, gboolean bUseFileKeys, const gchar *cTemplateFileName);
#define cairo_dock_flush_conf_file(pKeyFile, cConfFilePath, cShareDataDirPath, cTemplateFileName) cairo_dock_flush_conf_file_full (pKeyFile, cConfFilePath, cShareDataDirPath, TRUE, cTemplateFileName)

void cairo_dock_replace_key_values (GKeyFile *pOriginalKeyFile, GKeyFile *pReplacementKeyFile, gboolean bUseOriginalKeys, gchar iIdentifier);

void cairo_dock_replace_values_in_conf_file (gchar *cConfFilePath, GKeyFile *pValidKeyFile, gboolean bUseFileKeys, gchar iIdentifier);
void cairo_dock_replace_keys_by_identifier (gchar *cConfFilePath, gchar *cReplacementConfFilePath, gchar iIdentifier);

void cairo_dock_get_conf_file_version (GKeyFile *pKeyFile, gchar **cConfFileVersion);
gboolean cairo_dock_conf_file_needs_update (GKeyFile *pKeyFile, const gchar *cVersion);

/** Add or remove a value in a list of values to a given (group,key) pair in a conf file.
*/
void cairo_dock_add_remove_element_to_key (const gchar *cConfFilePath, const gchar *cGroupName, const gchar *cKeyName, gchar *cElementName, gboolean bAdd);


G_END_DECLS
#endif


#ifndef __CAIRO_DOCK_THEMES_MANAGER__
#define  __CAIRO_DOCK_THEMES_MANAGER__

#include <glib.h>
#include <gtk/gtk.h>
G_BEGIN_DECLS


/**
*Liste les themes disponibles. Un theme est un repertoire, et tous doivent etre places dans un meme repertoire.
*@param cThemesDir le repertoire contenant les themes.
*@param hProvidedTable une table de hashage (string , string) qui sera remplie, ou NULL pour que la fonction vous la cree.
*@param erreur : erreur positionnee au cas ou le repertoire serait illisible.
*@return la table de hashage contenant les doublets (nom_du_theme , chemin_du_theme). Si une table avait ete fournie en entree, c'est elle qui est retournee, sinon c'est une nouvelle table, a detruire avec 'g_hash_table_destroy' apres utilisation (tous les elements seront liberes).
*/
GHashTable *cairo_dock_list_local_themes (const gchar *cThemesDir, GHashTable *hProvidedTable, GError **erreur);

gchar *cairo_dock_download_file (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, gint iShowActivity, const gchar *cExtractTo, GError **erreur);

gchar *cairo_dock_get_distant_file_content (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, gint iShowActivity, GError **erreur);

GHashTable *cairo_dock_list_net_themes (const gchar *cServerAdress, const gchar *cDirectory, const gchar *cListFileName, GHashTable *hProvidedTable, GError **erreur);

GHashTable *cairo_dock_list_themes (const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir);


gchar *cairo_dock_edit_themes (GHashTable **hThemeTable, gboolean bSafeMode);

gchar *cairo_dock_get_chosen_theme (gchar *cConfFile, gboolean *bUseThemeBehaviour, gboolean *bUseThemeLaunchers);

void cairo_dock_load_current_theme (void);
#define cairo_dock_load_theme(...) cairo_dock_load_current_theme ()

void cairo_dock_mark_theme_as_modified (gboolean bModified);
gboolean cairo_dock_theme_need_save (void);


gboolean cairo_dock_manage_themes (GtkWidget *pWidget, CairoDockStartMode iMode);

#define cairo_dock_update_conf_file_with_themes(pOpenedKeyFile, cAppletConfFilePath, pThemeTable, cGroupName, cKeyName) cairo_dock_update_conf_file_with_hash_table (pOpenedKeyFile, cAppletConfFilePath, pThemeTable, cGroupName, cKeyName, NULL, (GHFunc) cairo_dock_write_one_theme_name, TRUE, FALSE)

gchar *cairo_dock_get_theme_path (const gchar *cThemeName, const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir);

G_END_DECLS
#endif

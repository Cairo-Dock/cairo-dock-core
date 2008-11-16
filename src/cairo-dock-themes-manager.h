
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
GHashTable *cairo_dock_list_themes (gchar *cThemesDir, GHashTable *hProvidedTable, GError **erreur);
GHashTable *cairo_dock_list_net_themes (gchar *cServerAdress, GHashTable *hProvidedTable, GError **erreur);

gchar *cairo_dock_edit_themes (GHashTable **hThemeTable, gboolean bSafeMode);


gchar *cairo_dock_get_chosen_theme (gchar *cConfFile, gboolean *bUseThemeBehaviour, gboolean *bUseThemeLaunchers);


gchar *cairo_dock_get_theme_path (gchar *cThemeName, GHashTable *hThemeTable);


void cairo_dock_load_theme (gchar *cThemePath);


void cairo_dock_mark_theme_as_modified (gboolean bModified);
gboolean cairo_dock_theme_need_save (void);


///int cairo_dock_ask_initial_theme (void);

gboolean cairo_dock_manage_themes (GtkWidget *pWidget, gint iMode);

#define cairo_dock_update_conf_file_with_themes(pOpenedKeyFile, cAppletConfFilePath, pThemeTable, cGroupName, cKeyName) cairo_dock_update_conf_file_with_hash_table (pOpenedKeyFile, cAppletConfFilePath, pThemeTable, cGroupName, cKeyName, NULL, (GHFunc) cairo_dock_write_one_theme_name, TRUE, FALSE)


G_END_DECLS
#endif

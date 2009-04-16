
#ifndef __CAIRO_DOCK_THEMES_MANAGER__
#define  __CAIRO_DOCK_THEMES_MANAGER__

#include <glib.h>
#include <gtk/gtk.h>
G_BEGIN_DECLS

/**@file cairo-dock-themes-manager.h This class provides a convenient way to use themes.
* Themes can be :
*   local   (installed as root in a sub-folder of /usr/share/cairo-dock)
*   user    (located in a sub-folder of ~/.config/cairo-dock)
*   distant (located on the cairo-dock's server)
*/

typedef enum {
	CAIRO_DOCK_LOCAL_THEME=0,
	CAIRO_DOCK_USER_THEME,
	CAIRO_DOCK_DISTANT_THEME,
	CAIRO_DOCK_NB_TYPE_THEME
} CairoDockThemeType;

struct _CairoDockTheme {
	gchar *cThemePath;
	gdouble fSize;
	gchar *cAuthor;
	gchar *cDisplayedName;
	CairoDockThemeType iType;  // installed, user, distant.
};

/** Destroy a theme and free all its allocated memory.
*@param pTheme the theme.
*/
void cairo_dock_free_theme (CairoDockTheme *pTheme);


GHashTable *cairo_dock_list_local_themes (const gchar *cThemesDir, GHashTable *hProvidedTable, GError **erreur);

/** Download a distant file into a given folder, possibly extracting it.
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param iShowActivity 0 : don't show, 1 : show a dialog, 2 : do it in a terminal.
*@param cExtractTo a local path where to extract the file, if this one is a .tar.gz archive, or NULL.
*@param erreur an error.
*@return the local path of the downloaded file. If it was an archive, the path of the extracted file. Free the string after using it.
*/
gchar *cairo_dock_download_file (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, gint iShowActivity, const gchar *cExtractTo, GError **erreur);

/** Retrieve the content of a distant text file.
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param iShowActivity 0 : don't show, 1 : show a dialog, 2 : do it in a terminal.
*@param erreur an error.
*@return the content of the file. Free the string after using it.
*/
gchar *cairo_dock_get_distant_file_content (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, gint iShowActivity, GError **erreur);

GHashTable *cairo_dock_list_net_themes (const gchar *cServerAdress, const gchar *cDirectory, const gchar *cListFileName, GHashTable *hProvidedTable, GError **erreur);

/** Get a list of themes from differente sources.
*@param cShareThemesDir path of a local folder containg themes or NULL.
*@param cUserThemesDir path of a user folder containg themes or NULL.
*@param cDistantThemesDir path of a distant folder containg themes or NULL.
*@return a hash table of (name, #CairoDockTheme).
*/
GHashTable *cairo_dock_list_themes (const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir);


gchar *cairo_dock_edit_themes (GHashTable **hThemeTable, gboolean bSafeMode);


/** Load the current theme. This will (re)load all the parameters of Cairo-Dock and all the plug-ins, as if you just started the dock.
*/
void cairo_dock_load_current_theme (void);
#define cairo_dock_load_theme(...) cairo_dock_load_current_theme ()

void cairo_dock_mark_theme_as_modified (gboolean bModified);
gboolean cairo_dock_theme_need_save (void);


gboolean cairo_dock_manage_themes (GtkWidget *pWidget, CairoDockStartMode iMode);

/** Look for a theme with a given name into differente sources. It is faster than getting the list of themes and looking for the given one.
*@param cThemeName name of the theme.
*@param cShareThemesDir path of a local folder containg themes or NULL.
*@param cUserThemesDir path of a user folder containg themes or NULL.
*@param cDistantThemesDir path of a distant folder containg themes or NULL.
*@return a newly allocated string containing the complete local path of the theme. If the theme is distant, it is downloaded and extracted into the user folder.
*/
gchar *cairo_dock_get_theme_path (const gchar *cThemeName, const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir);

G_END_DECLS
#endif

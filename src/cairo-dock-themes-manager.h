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
* The class offers a high level of abstraction that allows to manipulate themes without having to care their location, version, etc.
*/

#define CAIRO_DOCK_LOCAL_EXTRAS_DIR "extras"
#define CAIRO_DOCK_LAUNCHERS_DIR "launchers"
#define CAIRO_DOCK_PLUG_INS_DIR "plug-ins"
#define CAIRO_DOCK_LOCAL_ICONS_DIR "icons"

/// Types of themes.
typedef enum {
	CAIRO_DOCK_LOCAL_THEME=0,
	CAIRO_DOCK_USER_THEME,
	CAIRO_DOCK_DISTANT_THEME,
	CAIRO_DOCK_NEW_THEME,
	CAIRO_DOCK_UPDATED_THEME,
	CAIRO_DOCK_ANY_THEME,
	CAIRO_DOCK_NB_TYPE_THEME
} CairoDockThemeType;

/// Definition of a generic theme.
struct _CairoDockTheme {
	/// complete path of the theme.
	gchar *cThemePath;
	/// size in Mo
	gdouble fSize;
	/// author(s)
	gchar *cAuthor;
	/// name of the theme
	gchar *cDisplayedName;
	/// type of theme : installed, user, distant.
	CairoDockThemeType iType;
	/// rating of the theme, between 1 and 5.
	gint iRating;
	/// gint sobriety/simplicity of the theme, between 1 and 5.
	gint iSobriety;
	/// date of creation of the theme.
	gint iCreationDate;
	/// date of latest changes in the theme.
	gint iLastModifDate;
};

/** Destroy a theme and free all its allocated memory.
*@param pTheme the theme.
*/
void cairo_dock_free_theme (CairoDockTheme *pTheme);


gchar *cairo_dock_uncompress_file (const gchar *cArchivePath, const gchar *cExtractTo, const gchar *cRealArchiveName);

/** Download a distant file into a given folder, possibly extracting it.
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param cExtractTo a local path where to extract the file, if this one is a .tar.gz/.tar.bz2/.tgz archive, or NULL.
*@param erreur an error.
*@return the local path of the downloaded file. If it was an archive, the path of the extracted file. Free the string after using it.
*/
gchar *cairo_dock_download_file (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, const gchar *cExtractTo, GError **erreur);

/** Asynchronously download a distant file into a given folder, possibly extracting it. This function is non-blocking, you'll get a CairoTask that you can discard at any time, and you'll get the path of the downloaded file as the first argument of the callback (the second being the data you passed to this function).
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param cExtractTo a local path where to extract the file, if this one is a .tar.gz/.tar.bz2/.tgz archive, or NULL.
*@param pCallback function called when the download is finished. It takes the path of the downloaded file (it belongs to the task so don't free it) and the data you've set here.
*@param data data to be passed to the callback.
*@return the Task that is doing the job. Keep it and use \ref cairo_dock_discard_task whenever you want to discard the download (for instance if the user cancels it).
*/
CairoDockTask *cairo_dock_download_file_async (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, const gchar *cExtractTo, GFunc pCallback, gpointer data);

/** Retrieve the content of a distant text file.
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param erreur an error.
*@return the content of the file. Free the string after using it.
*/
gchar *cairo_dock_get_distant_file_content (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, GError **erreur);

/** Asynchronously retrieve the content of a distant text file. This function is non-blocking, you'll get a CairoTask that you can discard at any time, and you'll get the content of the downloaded file as the first argument of the callback (the second being the data you passed to this function).
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param pCallback function called when the download is finished. It takes the content of the downloaded file (it belongs to the task so don't free it) and the data you've set here.
*@param data data to be passed to the callback.
*@return the Task that is doing the job. Keep it and use \ref cairo_dock_discard_task whenever you want to discard the download (for instance if the user cancels it).
*/
CairoDockTask *cairo_dock_get_distant_file_content_async (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, GFunc pCallback, gpointer data);


GHashTable *cairo_dock_list_local_themes (const gchar *cThemesDir, GHashTable *hProvidedTable, gboolean bUpdateThemeValidity, GError **erreur);

GHashTable *cairo_dock_list_net_themes (const gchar *cServerAdress, const gchar *cDirectory, const gchar *cListFileName, GHashTable *hProvidedTable, GError **erreur);

/** Get a list of themes from differente sources.
*@param cShareThemesDir path of a local folder containg themes or NULL.
*@param cUserThemesDir path of a user folder containg themes or NULL.
*@param cDistantThemesDir path of a distant folder containg themes or NULL.
*@return a hash table of (name, #_CairoDockTheme).
*/
GHashTable *cairo_dock_list_themes (const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir);

/** Asynchronously get a list of themes from differente sources. This function is non-blocking, you'll get a CairoTask that you can discard at any time, and you'll get a hash-table of the themes as the first argument of the callback (the second being the data you passed to this function).
*@param cShareThemesDir path of a local folder containg themes or NULL.
*@param cUserThemesDir path of a user folder containg themes or NULL.
*@param cDistantThemesDir path of a distant folder containg themes or NULL.
*@param pCallback function called when the listing is finished. It takes the hash-table of the found themes (it belongs to the task so don't free it) and the data you've set here.
*@param data data to be passed to the callback.
*@return the Task that is doing the job. Keep it and use \ref cairo_dock_discard_task whenever you want to discard the download (for instance if the user cancels it).*/
CairoDockTask *cairo_dock_list_themes_async (const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir, GFunc pCallback, gpointer data);


/** Look for a theme with a given name into differente sources. If the theme is found on the server and is not present on the disk, or is not up to date, then it is downloaded and the local path is returned.
*@param cThemeName name of the theme.
*@param cShareThemesDir path of a local folder containg themes or NULL.
*@param cUserThemesDir path of a user folder containg themes or NULL.
*@param cDistantThemesDir path of a distant folder containg themes or NULL.
*@param iGivenType type of theme, or CAIRO_DOCK_ANY_THEME if any type of theme should be considered.
*@return a newly allocated string containing the complete local path of the theme. If the theme is distant, it is downloaded and extracted into this folder.
*/
gchar *cairo_dock_get_theme_path (const gchar *cThemeName, const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir, CairoDockThemeType iGivenType);


void cairo_dock_mark_theme_as_modified (gboolean bModified);
gboolean cairo_dock_theme_need_save (void);


CairoDockThemeType cairo_dock_extract_theme_type_from_name (const gchar *cThemeName);

/** Export the current theme to a given name. Exported themes can be imported directly from the Theme Manager.
 * @param cNewThemeName name to export the theme to.
 * @param bSaveBehavior whether to save the behavior paremeters too.
 * @param bSaveLaunchers whether to save the launchers too.
 * @return TRUE if the theme could be exported succefuly.
 */
gboolean cairo_dock_export_current_theme (const gchar *cNewThemeName, gboolean bSaveBehavior, gboolean bSaveLaunchers);

/** Create a package of the current theme. Packages can be distributed easily, and imported into the dock by a mere drag and drop into the Theme Manager. The package is placed in the Home.
 * @param cThemeName name of the package.
 * @return TRUE if the theme could be packaged succefuly.
 */
gboolean cairo_dock_package_current_theme (const gchar *cThemeName);

/** Extract a package into the themes folder. Does not load it.
 * @param cPackagePath path of a package. If the package is distant, it is first downoladed.
 * @return the path of the theme folder, or NULL if anerror occured.
 */
gchar * cairo_dock_depackage_theme (const gchar *cPackagePath);

/** Remove some exported themes from the hard-disk.
 * @param cThemesList a list of theme names, NULL-terminated.
 * @return TRUE if the themes has been succefuly deleted.
 */
gboolean cairo_dock_delete_themes (gchar **cThemesList);

/** Import a theme, which can be : a local theme, a user theme, a distant theme, or even the path to a packaged theme.
 * @param cThemeName name of the theme to import.
 * @param bLoadBehavior whether to import the behavior parameters too.
 * @param bLoadLaunchers whether to import the launchers too.
 * @return TRUE if the theme could be imported succefuly.
 */
gboolean cairo_dock_import_theme (const gchar *cThemeName, gboolean bLoadBehavior, gboolean bLoadLaunchers);

/** Asynchronously import a theme, which can be : a local theme, a user theme, a distant theme, or even the path to a packaged theme. This function is non-blocking, you'll get a CairoTask that you can discard at any time, and you'll get the result of the import as the first argument of the callback (the second being the data you passed to this function).
 * @param cThemeName name of the theme to import.
 * @param bLoadBehavior whether to import the behavior parameters too.
 * @param bLoadLaunchers whether to import the launchers too.
 * @param pCallback function called when the download is finished. It takes the result of the import (TRUE for a successful import) and the data you've set here.
 * @param data data to be passed to the callback.
 * @return the Task that is doing the job. Keep it and use \ref cairo_dock_discard_task whenever you want to discard the download (for instance if the user cancels it).

 */
CairoDockTask *cairo_dock_import_theme_async (const gchar *cThemeName, gboolean bLoadBehavior, gboolean bLoadLaunchers, GFunc pCallback, gpointer data);

/** Load the current theme. This will (re)load all the parameters of Cairo-Dock and all the plug-ins, as if you just started the dock.
*/
void cairo_dock_load_current_theme (void);
#define cairo_dock_load_theme(...) cairo_dock_load_current_theme ()


void cairo_dock_set_paths (gchar *cRootDataDirPath, gchar *cExtraDirPath, gchar *cThemesDirPath, gchar *cCurrentThemeDirPath);


G_END_DECLS
#endif

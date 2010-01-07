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


GHashTable *cairo_dock_list_local_themes (const gchar *cThemesDir, GHashTable *hProvidedTable, gboolean bUpdateThemeValidity, GError **erreur);


gchar *cairo_dock_uncompress_file (const gchar *cArchivePath, const gchar *cExtractTo, const gchar *cRealArchiveName);

/** Download a distant file into a given folder, possibly extracting it.
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param iShowActivity 0 : don't show, 1 : show a dialog, 2 : do it in a terminal.
*@param cExtractTo a local path where to extract the file, if this one is a .tar.gz/.tar.bz2/.tgz archive, or NULL.
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
*@return a hash table of (name, #_CairoDockTheme).
*/
GHashTable *cairo_dock_list_themes (const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir);

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
 */
gboolean cairo_dock_export_current_theme (const gchar *cNewThemeName, gboolean bSaveBehavior, gboolean bSaveLaunchers);

/** Create a package of the current theme. Packages can be distributed easily, and imported into the dock by a mere drag and drop into the Theme Manager. The package is placed in the Home.
 */
gboolean cairo_dock_package_current_theme (const gchar *cThemeName);

/** Extract a package into the themes folder. Does not load it.
 */
gchar * cairo_dock_depackage_theme (const gchar *cPackagePath);

/** Remove some exported themes from the hard-disk.
 */
gboolean cairo_dock_delete_themes (gchar **cThemesList);

/** Import a theme, which can be : a local theme, a user theme, a distant theme, or even the path to a packaged theme.
 */
gboolean cairo_dock_import_theme (const gchar *cThemeName, gboolean bLoadBehavior, gboolean bLoadLaunchers);


/** Build and show the Theme Manager window.
 */
void cairo_dock_manage_themes (void);


/** Load the current theme. This will (re)load all the parameters of Cairo-Dock and all the plug-ins, as if you just started the dock.
*/
void cairo_dock_load_current_theme (void);
#define cairo_dock_load_theme(...) cairo_dock_load_current_theme ()


G_END_DECLS
#endif

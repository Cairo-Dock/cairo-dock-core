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

/**@file cairo-dock-themes-manager.h This class allows defines the structure of the global theme of the dock (launchers, icons, plug-ins, configuration files, etc).
* It also provides methods to manage the themes, like exporting the current theme, importing new themes, deleting themes, etc.
*/

void cairo_dock_mark_current_theme_as_modified (gboolean bModified);
gboolean cairo_dock_current_theme_need_save (void);

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
 * @return the Task that is doing the job. Keep it and use \ref cairo_dock_discard_task if you want to discard the download before it's completed (for instance if the user cancels it), or \ref cairo_dock_free_task inside your callback.
 */
CairoDockTask *cairo_dock_import_theme_async (const gchar *cThemeName, gboolean bLoadBehavior, gboolean bLoadLaunchers, GFunc pCallback, gpointer data);

/** Load the current theme. This will (re)load all the parameters of Cairo-Dock and all the plug-ins, as if you just started the dock.
*/
void cairo_dock_load_current_theme (void);


void cairo_dock_set_paths (gchar *cRootDataDirPath, gchar *cExtraDirPath, gchar *cThemesDirPath, gchar *cCurrentThemeDirPath, gchar *cThemeServerAdress);


G_END_DECLS
#endif

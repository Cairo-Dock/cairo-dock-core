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


#ifndef __CAIRO_DOCK_LAUNCHER_FACTORY__
#define  __CAIRO_DOCK_LAUNCHER_FACTORY__

#include <glib.h>
//#include "cairo-dock-icons.h"

G_BEGIN_DECLS

/** *@file cairo-dock-launcher-factory.h This class handles the creation launcher icons, from the desktop files contained inside the 'launchers' folder. The files holding the information are common desktop files, with additionnal keys added by the dock on the launcher creation.
*/

/** Remove the version number from a string. Directly modifies the string.
 * @param cString a string.
 * @return TRUE if a version has been removed.
 */
gboolean cairo_dock_remove_version_from_string (gchar *cString);

/** Set the class of a launcher. You can safely free the paramater 'cStartupWMClass' after calling this function. This function is to be called on a launcher well defined (all other parameters should be already filled).
 * @param icon a launcher.
 * @param cStartupWMClass the class of the launcher defined in its .desktop file, or NULL. You can't expect the resulting class to be the one you provide, because this function makes a lot of guesses.
 */
void cairo_dock_set_launcher_class (Icon *icon, const gchar *cStartupWMClass);


/** Read a desktop file and fetch all its data into an Icon.
 * @param cDesktopFileName name or path of a desktop file. If it's a simple name, it will be taken in the "launchers" folder of the current theme.
 * @param icon the Icon to fill.
 * @param cSubDockRendererName filled with the renderer name of the sub-dock, if the icon will hold one.
 * @return the type of the icon, guessed from the values of the desktop file.
 */
CairoDockIconTrueType cairo_dock_load_icon_info_from_desktop_file (const gchar *cDesktopFileName, Icon *icon, gchar **cSubDockRendererName);

/** Create an Icon from a given desktop file. The resulting icon can directly be used inside a container. Class inhibating is handled.
 * @param cDesktopFileName name of the desktop file, present in the "launchers" folder of the current theme.
 * @param cSubDockRendererName filled with the renderer name of the sub-dock, if the icon will hold one.
 * @return the newly created icon.
 */
G_GNUC_MALLOC Icon * cairo_dock_new_launcher_icon (const gchar *cDesktopFileName, gchar **cSubDockRendererName);


G_END_DECLS
#endif

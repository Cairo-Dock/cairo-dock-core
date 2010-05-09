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


#ifndef __CAIRO_DOCK_LAUNCHER_MANAGER__
#define  __CAIRO_DOCK_LAUNCHER_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/** *@file cairo-dock-launcher-manager.h This class handles the creation, load and reload of launcher icons, from the desktop files contained inside the 'launchers' folder. The files holding the information are common desktop files, with additionnal keys added by the dock on the launcher creation.
*/

/** Search the path of an icon into the defined icons themes. It also handles the '~' caracter in paths.
 * @param cFileName name of the icon file.
 * @return the complete path of the icon, or NULL if not found.
 */
gchar *cairo_dock_search_icon_s_path (const gchar *cFileName);

/** Create an Icon from a given desktop file, and fill its buffers. The resulting icon can directly be used inside a container. Class inhibating is handled.
 * @param cDesktopFileName name of the desktop file, present in the "launchers" folder of the current theme.
 * @return the newly created icon.
*/
Icon * cairo_dock_create_icon_from_desktop_file (const gchar *cDesktopFileName);

/** Load a set of .desktop files that define icons, and build the corresponding tree of docks.
* All the icons are created and placed inside their dock, which is created if necessary.
* @param cDirectory a folder containing some .desktop files.
*/
void cairo_dock_build_docks_tree_with_desktop_files (const gchar *cDirectory);


/** Reload completely a launcher. It handles all the side-effects like modifying the class, the sub-dock's view, the container, etc.
 * @param icon the launcher Icon to reload.
*/
void cairo_dock_reload_launcher (Icon *icon);


gchar *cairo_dock_launch_command_sync (const gchar *cCommand);

gboolean cairo_dock_launch_command_printf (const gchar *cCommandFormat, gchar *cWorkingDirectory, ...) G_GNUC_PRINTF (1, 3);
gboolean cairo_dock_launch_command_full (const gchar *cCommand, gchar *cWorkingDirectory);
#define cairo_dock_launch_command(cCommand) cairo_dock_launch_command_full (cCommand, NULL)

G_END_DECLS
#endif

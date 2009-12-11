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

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-launcher-factory.h This class handles the creation and reload of launcher icons, from the desktop files contained inside the 'launchers' folder. The files holding the information are common desktop files, with additionnal keys added by the dock on the launcher creation.
*/

/** Search the path of an icon into the defined icons themes. It also handles the '~' caracter in paths.
 * @param cFileName name of the icon file.
 * @return the complete path of the icon, or NULL if not found.
 */
gchar *cairo_dock_search_icon_s_path (const gchar *cFileName);


/** Read a desktop file and fetch all its data into an Icon.
 * @param cDesktopFileName name or path of a desktop file. If it's a simple name, it will be taken in the "launchers" folder of the current theme.
 * @param icon the Icon to fill.
*/
void cairo_dock_load_icon_info_from_desktop_file (const gchar *cDesktopFileName, Icon *icon);


/** Create an Icon from a given desktop file, and fill its buffers. The resulting icon can directly be used inside a container. Class inhibating is handled.
 * @param cDesktopFileName name of the desktop file, present in the "launchers" folder of the current theme.
 * @param pSourceContext a drawing context, not altered.
 * @return the newly created icon.
*/
Icon * cairo_dock_create_icon_from_desktop_file (const gchar *cDesktopFileName, cairo_t *pSourceContext);

/** Reload an Icon from a given desktop file, and fill its buffers. It doesn't handle the side-effects like modifying the class, the sub-dock's view, the container, etc.
 * @param cDesktopFileName nameof the desktop file, taken in the "launchers" folder of the current theme.
 * @param pSourceContext a drawing context, not altered.
 * @param icon the Icon to reload.
*/
void cairo_dock_reload_icon_from_desktop_file (const gchar *cDesktopFileName, cairo_t *pSourceContext, Icon *icon);


/** Reload completely a launcher. It handles all the side-effects like modifying the class, the sub-dock's view, the container, etc.
 * @param icon the launcher Icon to reload.
*/
void cairo_dock_reload_launcher (Icon *icon);


G_END_DECLS
#endif

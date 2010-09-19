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


#ifndef __CAIRO_DOCK_DESKTOP_FILE_FACTORY__
#define  __CAIRO_DOCK_DESKTOP_FILE_FACTORY__

#include <glib.h>

#include "cairo-dock-icons.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-desktop-file-factory.h This class handles the creation of desktop files, which are group/key pair files used by Cairo-Dock to store information about icons : launchers and files, separators, sub-docks.
*/

typedef enum {
	CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER = 0,
	CAIRO_DOCK_DESKTOP_FILE_FOR_CONTAINER,
	CAIRO_DOCK_DESKTOP_FILE_FOR_SEPARATOR,
	CAIRO_DOCK_DESKTOP_FILE_FOR_FILE,
	CAIRO_DOCK_NB_DESKTOP_FILES
	} CairoDockDesktopFileType;

/** Replace the %20 by normal spaces into the string. The string is directly modified.
*@param cString the string (it can't be a constant string)
*/
void cairo_dock_remove_html_spaces (gchar *cString);

/** Create, add and fill a desktop file for a given URI. The URI can be either a common desktop file, a script, or a file/folder/mounting point.
*@param cURI URI of a file defining the launcher.
*@param cDockName name of the dock the separator will be added.
*@param fOrder order of the icon inside the dock.
*@param iGroup group of the icon.
*@param erreur an error filled if something went wrong.
*/
gchar *cairo_dock_add_desktop_file_from_uri (const gchar *cURI, const gchar *cDockName, double fOrder, CairoDockIconType iGroup, GError **erreur);

/** Create and add an empty default desktop file for a given type.
*@param iLauncherType type of the icon it will represent : launcher, file, container icon, separator.
*@param cDockName name of the dock the separator will be added.
*@param fOrder order of the icon inside the dock.
*@param iGroup group of the icon.
*@param erreur an error filled if something went wrong.
*/
gchar *cairo_dock_add_desktop_file_from_type (CairoDockDesktopFileType iLauncherType, const gchar *cDockName, double fOrder, CairoDockIconType iGroup, GError **erreur);


void cairo_dock_update_launcher_desktop_file (gchar *cDesktopFilePath, CairoDockDesktopFileType iLauncherType);


void cairo_dock_write_container_name_in_conf_file (Icon *pIcon, const gchar *cParentDockName);

void cairo_dock_write_order_in_conf_file (Icon *pIcon, double fOrder);


G_END_DECLS
#endif


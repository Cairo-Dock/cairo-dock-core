/**
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


#ifndef CAIRO_DOCK_DESKTOP_FILE_DB_H
#define CAIRO_DOCK_DESKTOP_FILE_DB_H

#include <glib.h>
#include <gio/gdesktopappinfo.h>

G_BEGIN_DECLS

/**
*@file cairo-dock-desktop-file-db.h Functions to maintain and query a database of all apps that are installed
* on a system.
*/


/**
 * Start the desktop file DB manager. This will run a background thread to populate the DB with all apps
 * installed on the system. */
void gldi_desktop_file_db_init (void);

/**
 * Stop the desktop file DB manager. This will delete all apps in the DB and free all memory. */
void gldi_desktop_file_db_stop (void);

/**
 * Try to look up an installed app. This function can block the first time it's called if the DB has not been
 * fully populated yet.
 * @param class Desktop file ID, class or app-id of an app to look up (matching is based on the basename of
 * 	its .desktop file, and the content of the StartupWMClass and Exec keys in it).
 * @param bOnlyDesktopID if TRUE, only the .desktop file name is used for matching (can be useful if looking
 * 	for a known .desktop file).
 * @return GDesktopAppInfo corresponding to the app if found. The return value is owned by the DB, the
 *  caller should call g_object_ref () on it if it wants to keep it.
*/
GDesktopAppInfo *gldi_desktop_file_db_lookup (const char *class, gboolean bOnlyDesktopID);

G_END_DECLS

#endif


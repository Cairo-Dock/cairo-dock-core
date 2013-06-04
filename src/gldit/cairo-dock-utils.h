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

#ifndef __CAIRO_DOCK_UTILS__
#define  __CAIRO_DOCK_UTILS__
G_BEGIN_DECLS

/**
*@file cairo-dock-utils.h Some helper functions.
*/


gchar *cairo_dock_generate_unique_filename (const gchar *cBaseName, const gchar *cCairoDockDataDir);


gchar *cairo_dock_cut_string (const gchar *cString, int iNbCaracters);;


/** Replace the %20 by normal spaces into the string. The string is directly modified.
*@param cString the string (it can't be a constant string)
*/
void cairo_dock_remove_html_spaces (gchar *cString);


gchar *cairo_dock_launch_command_sync_with_stderr (const gchar *cCommand, gboolean bPrintStdErr);
#define cairo_dock_launch_command_sync(cCommand) cairo_dock_launch_command_sync_with_stderr (cCommand, TRUE)

gboolean cairo_dock_launch_command_printf (const gchar *cCommandFormat, gchar *cWorkingDirectory, ...) G_GNUC_PRINTF (1, 3);
gboolean cairo_dock_launch_command_full (const gchar *cCommand, gchar *cWorkingDirectory);
#define cairo_dock_launch_command(cCommand) cairo_dock_launch_command_full (cCommand, NULL)

gchar * cairo_dock_get_command_with_right_terminal (const gchar *cCommand);



G_END_DECLS
#endif


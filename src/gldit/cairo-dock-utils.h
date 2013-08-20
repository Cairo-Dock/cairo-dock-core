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

/** Remove the version number from a string. Directly modifies the string.
 * @param cString a string.
 * @return TRUE if a version has been removed.
 */
gboolean cairo_dock_remove_version_from_string (gchar *cString);

/** Replace the %20 by normal spaces into the string. The string is directly modified.
*@param cString the string (it can't be a constant string)
*/
void cairo_dock_remove_html_spaces (gchar *cString);

/** Get the 3 version numbers of a string.
*@param cVersionString the string of the form "x.y.z".
*@param iMajorVersion pointer to the major version.
*@param iMinorVersion pointer to the minor version.
*@param iMicroVersion pointer to the micro version.
*/
void cairo_dock_get_version_from_string (const gchar *cVersionString, int *iMajorVersion, int *iMinorVersion, int *iMicroVersion);


gchar *cairo_dock_launch_command_sync_with_stderr (const gchar *cCommand, gboolean bPrintStdErr);
#define cairo_dock_launch_command_sync(cCommand) cairo_dock_launch_command_sync_with_stderr (cCommand, TRUE)

gboolean cairo_dock_launch_command_printf (const gchar *cCommandFormat, const gchar *cWorkingDirectory, ...) G_GNUC_PRINTF (1, 3);
gboolean cairo_dock_launch_command_full (const gchar *cCommand, const gchar *cWorkingDirectory);
#define cairo_dock_launch_command(cCommand) cairo_dock_launch_command_full (cCommand, NULL)

gchar * cairo_dock_get_command_with_right_terminal (const gchar *cCommand);

/* Like g_strcmp0, but saves a function call.
*/
#define gldi_strings_differ(s1, s2) (!s1 ? s2 != NULL : !s2 ? s1 != NULL : strcmp(s1, s2) != 0)
#define cairo_dock_strings_differ gldi_strings_differ

/** Say if 2 RGBA colors differ.
*/
#define cairo_dock_colors_rvb_differ(c1, c2) ((c1[0] != c2[0]) || (c1[1] != c2[1]) || (c1[2] != c2[2]))

/** Say if 2 RGB colors differ.
*/
#define cairo_dock_colors_differ(c1, c2) (cairo_dock_colors_rvb_differ (c1, c2) || (c1[3] != c2[3]))


#include "gldi-config.h"
#ifdef HAVE_X11

gboolean cairo_dock_property_is_present_on_root (const gchar *cPropertyName);  // env-manager

gboolean cairo_dock_check_xrandr (int major, int minor);  // showDesktop

#endif

G_END_DECLS
#endif


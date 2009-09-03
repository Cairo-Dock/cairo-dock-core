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


#ifndef __CAIRO_DOCK_KEYFILE_UTILITIES__
#define  __CAIRO_DOCK_KEYFILE_UTILITIES__

#include <glib.h>
G_BEGIN_DECLS


/**
*@file cairo-dock-keyfile-utilities.h This class provides useful functions to manipulate the conf files of Cairo-Dock, which are classic group/key pair files.
*/

/** Open a conf file to be read/written. Returns NULL if the file couldn't be found/opened/parsed.
*Free it with g_key_file_free after you're done.
*/
GKeyFile *cairo_dock_open_key_file (const gchar *cConfFilePath);

/** Write a conf file on the disk.
*/
void cairo_dock_write_keys_to_file (GKeyFile *pKeyFile, const gchar *cConfFilePath);


void cairo_dock_flush_conf_file_full (GKeyFile *pKeyFile, const gchar *cConfFilePath, const gchar *cShareDataDirPath, gboolean bUseFileKeys, const gchar *cTemplateFileName);
#define cairo_dock_flush_conf_file(pKeyFile, cConfFilePath, cShareDataDirPath, cTemplateFileName) cairo_dock_flush_conf_file_full (pKeyFile, cConfFilePath, cShareDataDirPath, TRUE, cTemplateFileName)

void cairo_dock_replace_key_values (GKeyFile *pOriginalKeyFile, GKeyFile *pReplacementKeyFile, gboolean bUseOriginalKeys, gchar iIdentifier);

void cairo_dock_replace_values_in_conf_file (const gchar *cConfFilePath, GKeyFile *pValidKeyFile, gboolean bUseFileKeys, gchar iIdentifier);
void cairo_dock_replace_keys_by_identifier (const gchar *cConfFilePath, gchar *cReplacementConfFilePath, gchar iIdentifier);


/** Get the version of a conf file. The version is written on the first line of the file, as a comment.
*/
void cairo_dock_get_conf_file_version (GKeyFile *pKeyFile, gchar **cConfFileVersion);

/** Say if a conf file's version mismatches a giver version.
*/
gboolean cairo_dock_conf_file_needs_update (GKeyFile *pKeyFile, const gchar *cVersion);

/** Add or remove a value in a list of values to a given (group,key) pair of a conf file.
*/
void cairo_dock_add_remove_element_to_key (const gchar *cConfFilePath, const gchar *cGroupName, const gchar *cKeyName, gchar *cElementName, gboolean bAdd);

/** Add a key to a conf file, so that it can be parsed by the GUI manager.
*/
void cairo_dock_add_widget_to_conf_file (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *ckeyName, const gchar *cInitialValue, gchar iWidgetType, const gchar *cDescription, const gchar *cTooltip);


G_END_DECLS
#endif

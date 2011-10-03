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

#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <gtk/gtk.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "gldi-config.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-desktop-file-factory.h"

#define CAIRO_DOCK_LAUNCHER_CONF_FILE "launcher.desktop"
#define CAIRO_DOCK_CONTAINER_CONF_FILE "container.desktop"
#define CAIRO_DOCK_SEPARATOR_CONF_FILE "separator.desktop"

extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentLaunchersPath;


static gchar *_cairo_dock_generate_desktop_filename (const gchar *cBaseName, gchar *cCairoDockDataDir)
{
	int iPrefixNumber = 0;
	GString *sFileName = g_string_new ("");

	do
	{
		iPrefixNumber ++;
		g_string_printf (sFileName, "%s/%02d%s", cCairoDockDataDir, iPrefixNumber, cBaseName);
	} while (iPrefixNumber < 99 && g_file_test (sFileName->str, G_FILE_TEST_EXISTS));

	g_string_free (sFileName, TRUE);
	if (iPrefixNumber == 99)
		return NULL;
	else
		return g_strdup_printf ("%02d%s", iPrefixNumber, cBaseName);
}

static inline const gchar *_cairo_dock_get_launcher_template_conf_file_path (CairoDockDesktopFileType iNewDesktopFileType)
{
	const gchar *cTemplateFile;
	switch (iNewDesktopFileType)
	{
		case CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER :
			cTemplateFile = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_LAUNCHER_CONF_FILE;
		break ;
		case CAIRO_DOCK_DESKTOP_FILE_FOR_CONTAINER :
			cTemplateFile = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_CONTAINER_CONF_FILE;
		break ;
		case CAIRO_DOCK_DESKTOP_FILE_FOR_SEPARATOR :
			cTemplateFile = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_SEPARATOR_CONF_FILE;
		break ;
		default:
			cTemplateFile = NULL;
	}
	return cTemplateFile;
}

static gchar *_add_new_desktop_file (CairoDockDesktopFileType iLauncherType, const gchar *cOrigin, const gchar *cDockName, double fOrder, GError **erreur)
{
	//\__________________ open the template.
	const gchar *cTemplateFile = _cairo_dock_get_launcher_template_conf_file_path (iLauncherType);
	g_return_val_if_fail (cTemplateFile != NULL, NULL);
	
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cTemplateFile);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	//\__________________ fill the parameters
	gchar *cFilePath = NULL;
	if (cOrigin != NULL && *cOrigin != '/')  // transform the origin URI into a path or a file name.
	{
		if (strncmp (cOrigin, "application://", 14) == 0)  // Ubuntu >= 11.04: it's now an "app" URI
			cFilePath = g_strdup (cOrigin + 14);  // in this case we don't have the actual path of the .desktop, but that doesn't matter.
		else
			cFilePath = g_filename_from_uri (cOrigin, NULL, NULL);
	}
	else  // no origin or already a path.
		cFilePath = g_strdup (cOrigin);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Origin", cFilePath?cFilePath:"");
	
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);
	
	//\__________________ in the case of a script, set ourselves a valid name and command.
	if (cFilePath != NULL && g_str_has_suffix (cFilePath, ".sh"))
	{
		gchar *cName = g_path_get_basename (cFilePath);
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cName);
		g_free (cName);
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", cFilePath);
		g_key_file_set_boolean (pKeyFile, "Desktop Entry", "Terminal", TRUE);
	}
	
	//\__________________ in the case of a custom launcher, set a command (the launcher would be invalid without).
	if (cFilePath == NULL && iLauncherType == CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER)
	{
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", _("Enter a command"));
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", _("New launcher"));
	}
	
	//\__________________ generate a unique and readable filename.
	gchar *cBaseName = (cFilePath ?
		*cFilePath == '/' ?
			g_path_get_basename (cFilePath) :
			g_strdup (cFilePath) :
		g_path_get_basename (cTemplateFile));
	gchar *cNewDesktopFileName = _cairo_dock_generate_desktop_filename (cBaseName, g_cCurrentLaunchersPath);
	g_free (cBaseName);
	
	//\__________________ write the keys.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_write_keys_to_file (pKeyFile, cNewDesktopFilePath);
	g_free (cNewDesktopFilePath);
	
	g_free (cFilePath);
	g_key_file_free (pKeyFile);
	return cNewDesktopFileName;
}

gchar *cairo_dock_add_desktop_file_from_uri (const gchar *cURI, const gchar *cDockName, double fOrder, GError **erreur)
{
	if (! (cURI == NULL || g_str_has_suffix (cURI, ".desktop") || g_str_has_suffix (cURI, ".sh")))
		return NULL;
	return _add_new_desktop_file (CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER, cURI, cDockName, fOrder, erreur);
}

gchar *cairo_dock_add_desktop_file_from_type (CairoDockDesktopFileType iLauncherType, const gchar *cDockName, double fOrder, GError **erreur)
{
	return _add_new_desktop_file (iLauncherType, NULL, cDockName, fOrder, erreur);
}


void cairo_dock_update_launcher_key_file (GKeyFile *pKeyFile, const gchar *cDesktopFilePath, CairoDockDesktopFileType iLauncherType)
{
	const gchar *cTemplateFile = _cairo_dock_get_launcher_template_conf_file_path (iLauncherType);
	cd_debug ("%s (%s)", __func__, cTemplateFile);
	
	cairo_dock_upgrade_conf_file (cDesktopFilePath, pKeyFile, cTemplateFile);  // update keys
}


void cairo_dock_write_container_name_in_conf_file (Icon *pIcon, const gchar *cParentDockName)
{
	if (pIcon->cDesktopFileName != NULL)  // lanceur ou separateur.
	{
		gchar *cDesktopFilePath = *pIcon->cDesktopFileName == '/' ? g_strdup (pIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pIcon->cDesktopFileName);
		cairo_dock_update_conf_file (cDesktopFilePath,
			G_TYPE_STRING, "Desktop Entry", "Container", cParentDockName,
			G_TYPE_INVALID);
		g_free (cDesktopFilePath);
	}
	else if (CAIRO_DOCK_IS_APPLET (pIcon))
	{
		cairo_dock_update_conf_file (pIcon->pModuleInstance->cConfFilePath,
			G_TYPE_STRING, "Icon", "dock name", cParentDockName,
			G_TYPE_INVALID);
	}
}

void cairo_dock_write_order_in_conf_file (Icon *pIcon, double fOrder)
{
	if (pIcon->cDesktopFileName != NULL)  // lanceur ou separateur.
	{
		gchar *cDesktopFilePath = *pIcon->cDesktopFileName == '/' ? g_strdup (pIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pIcon->cDesktopFileName);
		cairo_dock_update_conf_file (cDesktopFilePath,
			G_TYPE_DOUBLE, "Desktop Entry", "Order", fOrder,
			G_TYPE_INVALID);
		g_free (cDesktopFilePath);
	}
	else if (CAIRO_DOCK_IS_APPLET (pIcon))
	{
		cairo_dock_update_conf_file (pIcon->pModuleInstance->cConfFilePath,
			G_TYPE_DOUBLE, "Icon", "order", fOrder,
			G_TYPE_INVALID);
	}
}


// should not be here, and probably not be used either (only applets does).
void cairo_dock_remove_html_spaces (gchar *cString)
{
	gchar *str = cString;
	do
	{
		str = g_strstr_len (str, -1, "%20");
		if (str == NULL)
			break ;
		*str = ' ';
		str ++;
		strcpy (str, str+2);
	}
	while (TRUE);
}

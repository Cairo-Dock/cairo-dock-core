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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gldi-config.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"  // g_iDesktopEnv
#include "cairo-dock-desktop-file-factory.h"  // cairo_dock_update_launcher_key_file
#include "cairo-dock-launcher-factory.h"

extern gchar *g_cCurrentLaunchersPath;
extern CairoDockDesktopEnv g_iDesktopEnv;

/*
insert new desktop file -> make a new user desktop file, set the path and set all other params to NULL
load user desktop file

insert new custom launcher -> make a new user desktop file, set path to NULL and set other params to default values
load user desktop file

load a user desktop file -> get the path + internal params (order, etc)
if path not NULL: check path, or search based on the filename
if not found: search for alternative desktop files
 -> class
get common data from the user desktop file
if class is NULL: try to guess the class from Exec+StartupWMClass
 -> class2 -> register -> class
if class not NULL: get data from the class

register class -> get the class from Exec+StartupWMClass -> make a class
get other params (Name, Icon, Exec, Path, MimeType, StartupWMClass, Terminal, Unity Menu)
insert in table
return class
*/

gboolean cairo_dock_remove_version_from_string (gchar *cString)
{
	if (cString == NULL)
		return FALSE;
	int n = strlen (cString);
	gchar *str = cString + n - 1;
	do
	{
		if (g_ascii_isdigit(*str) || *str == '.')
		{
			str --;
			continue;
		}
		if (*str == '-' || *str == ' ')  // 'Glade-2', 'OpenOffice 3.1'
		{
			*str = '\0';
			return TRUE;
		}
		else
			return FALSE;
	}
	while (str != cString);
	return FALSE;
}

#define _print_error(cDesktopFileName, erreur)\
	if (erreur != NULL) {\
		cd_warning ("while trying to load %s : %s", cDesktopFileName, erreur->message);\
		g_error_free (erreur);\
		erreur = NULL; }
CairoDockIconTrueType cairo_dock_load_icon_info_from_desktop_file (const gchar *cDesktopFileName, Icon *icon, gchar **cSubDockRendererName)
{
	cd_debug ("%s (%s)", __func__, cDesktopFileName);
	CairoDockIconTrueType iType = CAIRO_DOCK_ICON_TYPE_LAUNCHER;
	
	//\__________________ open the desktop file
	GError *erreur = NULL;
	gchar *cDesktopFilePath = (*cDesktopFileName == '/' ? g_strdup (cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cDesktopFileName));
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
	g_return_val_if_fail (pKeyFile != NULL, iType);
	
	g_free (icon->cDesktopFileName);
	icon->cDesktopFileName = g_strdup (cDesktopFileName);
	
	gboolean bNeedUpdate = FALSE;
	
	//\__________________ get the type of the icon
	if (g_key_file_has_key (pKeyFile, "Desktop Entry", "Icon Type", NULL))
	{
		iType = g_key_file_get_integer (pKeyFile, "Desktop Entry", "Icon Type", NULL);
	}
	else  // old desktop file
	{
		bNeedUpdate = TRUE;
		
		gchar *cCommand = g_key_file_get_string (pKeyFile, "Desktop Entry", "Exec", NULL);
		gboolean bIsContainer;
		if (g_key_file_has_key (pKeyFile, "Desktop Entry", "Is container", NULL))
			bIsContainer = g_key_file_get_boolean (pKeyFile, "Desktop Entry", "Is container", NULL);
		else if (g_key_file_has_key (pKeyFile, "Desktop Entry", "Nb subicons", NULL))
			bIsContainer = (g_key_file_get_integer (pKeyFile, "Desktop Entry", "Nb subicons", NULL) != 0);
		else
			bIsContainer = (g_key_file_get_integer (pKeyFile, "Desktop Entry", "Type", NULL) == 1);
		
		if (bIsContainer)
		{
			iType = CAIRO_DOCK_ICON_TYPE_CONTAINER;
		}
		else if (cCommand == NULL || *cCommand == '\0')
		{
			iType = CAIRO_DOCK_ICON_TYPE_SEPARATOR;
		}
		else
		{
			iType = CAIRO_DOCK_ICON_TYPE_LAUNCHER;
		}
		g_key_file_set_integer (pKeyFile, "Desktop Entry", "Icon Type", iType);
		g_free (cCommand);
	}
	
	//\__________________ get internal params
	icon->fOrder = g_key_file_get_double (pKeyFile, "Desktop Entry", "Order", &erreur);
	_print_error (cDesktopFileName, erreur);

	g_free (icon->cParentDockName);
	icon->cParentDockName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Container", &erreur);
	_print_error (cDesktopFileName, erreur);
	if (icon->cParentDockName == NULL || *icon->cParentDockName == '\0')
	{
		g_free (icon->cParentDockName);
		icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);
	}
	
	if (iType == CAIRO_DOCK_ICON_TYPE_CONTAINER)
	{
		*cSubDockRendererName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Renderer", NULL);
		icon->iSubdockViewType = g_key_file_get_integer (pKeyFile, "Desktop Entry", "render", NULL);  // on a besoin d'un entier dans le panneau de conf pour pouvoir degriser des options selon le rendu choisi. De plus c'est utile aussi pour Animated Icons...
	}
	else
		*cSubDockRendererName = NULL;
	
	int iSpecificDesktop = g_key_file_get_integer (pKeyFile, "Desktop Entry", "ShowOnViewport", NULL);
	if (iSpecificDesktop != 0)
		cairo_dock_set_specified_desktop_for_icon (icon, iSpecificDesktop);
	
	//\__________________ get common data as defined by the user.
	g_free (icon->cFileName);
	icon->cFileName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Icon", NULL);
	if (icon->cFileName != NULL && *icon->cFileName == '\0')
	{
		g_free (icon->cFileName);
		icon->cFileName = NULL;
	}
	
	g_free (icon->cName);
	icon->cName = g_key_file_get_locale_string (pKeyFile, "Desktop Entry", "Name", NULL, NULL);
	if (icon->cName != NULL && *icon->cName == '\0')
	{
		g_free (icon->cName);
		icon->cName = NULL;
	}
	
	g_free (icon->cCommand);
	icon->cCommand = g_key_file_get_string (pKeyFile, "Desktop Entry", "Exec", NULL);
	if (icon->cCommand != NULL && *icon->cCommand == '\0')
	{
		g_free (icon->cCommand);
		icon->cCommand = NULL;
	}
	
	if (icon->pMimeTypes != NULL)
	{
		g_strfreev (icon->pMimeTypes);
		icon->pMimeTypes = NULL;
	}
	
	g_free (icon->cWorkingDirectory);
	icon->cWorkingDirectory = NULL;
	
	//\__________________ in the case of a launcher, bind it to a class, and get the additionnal params.
	if (iType == CAIRO_DOCK_ICON_TYPE_LAUNCHER)
	{
		gchar *cStartupWMClass = g_key_file_get_string (pKeyFile, "Desktop Entry", "StartupWMClass", NULL);
		if (cStartupWMClass && *cStartupWMClass == '\0')
		{
			g_free (cStartupWMClass);
			cStartupWMClass = NULL;
		}
		
		// get the origin of the desktop file.
		gchar *cClass = NULL;
		gsize length = 0;
		gchar **pOrigins = g_key_file_get_string_list (pKeyFile, "Desktop Entry", "Origin", &length, NULL);
		int iNumOrigin = -1;
		if (pOrigins != NULL)  // some origins are provided, try them one by one.
		{
			int i;
			for (i = 0; pOrigins[i] != NULL; i++)
			{
				cClass = cairo_dock_register_class_full (pOrigins[i], cStartupWMClass, NULL);
				if (cClass != NULL)  // neat, this origin is a valid one, let's use it from now.
				{
					iNumOrigin = i;
					break;
				}
			}
			g_strfreev (pOrigins);
		}

		// if no origin class could be found, try to guess the class
		gchar *cFallbackClass = NULL;
		if (cClass == NULL)  // no class found, maybe an old launcher or a custom one, try to guess from the info in the user desktop file.
		{
			cFallbackClass = cairo_dock_guess_class (icon->cCommand, cStartupWMClass);
			cClass = cairo_dock_register_class_full (cFallbackClass, cStartupWMClass, NULL);
		}
		
		// get common data from the class
		g_free (icon->cClass);
		if (cClass != NULL)
		{
			icon->cClass = cClass;
			g_free (cFallbackClass);
			cairo_dock_set_data_from_class (cClass, icon);
			if (iNumOrigin != 0)  // it's not the first origin that gave us the correct class, so let's write it down to avoid searching the next time.
			{
				g_key_file_set_string (pKeyFile, "Desktop Entry", "Origin", cairo_dock_get_class_desktop_file (cClass));
				if (!bNeedUpdate)  // no update is scheduled, so write it now.
					cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
			}
		}
		else  // no class found, it's maybe an old launcher, take the remaining common params from the user desktop file.
		{
			icon->cClass = cFallbackClass;
			gsize length = 0;
			icon->pMimeTypes = g_key_file_get_string_list (pKeyFile, "Desktop Entry", "MimeType", &length, NULL);
			
			if (icon->cCommand != NULL)
			{
				icon->cWorkingDirectory = g_key_file_get_string (pKeyFile, "Desktop Entry", "Path", NULL);
				if (icon->cWorkingDirectory != NULL && *icon->cWorkingDirectory == '\0')
				{
					g_free (icon->cWorkingDirectory);
					icon->cWorkingDirectory = NULL;
				}
			}
		}
		
		
		// take into account the execution in a terminal.
		gboolean bExecInTerminal = g_key_file_get_boolean (pKeyFile, "Desktop Entry", "Terminal", NULL);
		if (bExecInTerminal)  // on le fait apres la classe puisqu'on change la commande.
		{
			gchar *cOldCommand = icon->cCommand;
			const gchar *cTerm = g_getenv ("COLORTERM");
			if (cTerm != NULL && strlen (cTerm) > 1)  // on filtre les cas COLORTERM=1 ou COLORTERM=y. ce qu'on veut c'est le nom d'un terminal.
				icon->cCommand = g_strdup_printf ("%s -e \"%s\"", cTerm, cOldCommand);
			else if (g_iDesktopEnv == CAIRO_DOCK_GNOME)
				icon->cCommand = g_strdup_printf ("gnome-terminal -e \"%s\"", cOldCommand);
			else if (g_iDesktopEnv == CAIRO_DOCK_XFCE)
				icon->cCommand = g_strdup_printf ("xfce4-terminal -e \"%s\"", cOldCommand);
			else if (g_iDesktopEnv == CAIRO_DOCK_KDE)
				icon->cCommand = g_strdup_printf ("konsole -e \"%s\"", cOldCommand);
			else if (g_getenv ("TERM") != NULL)
				icon->cCommand = g_strdup_printf ("%s -e \"%s\"", g_getenv ("TERM"), cOldCommand);
			else
				icon->cCommand = g_strdup_printf ("xterm -e \"%s\"", cOldCommand);
			g_free (cOldCommand);
		}
		
		gboolean bPreventFromInhibiting = g_key_file_get_boolean (pKeyFile, "Desktop Entry", "prevent inhibate", NULL);  // FALSE si la cle n'existe pas.
		if (bPreventFromInhibiting)
		{
			g_free (icon->cClass);
			icon->cClass = NULL;
		}
		g_free (cStartupWMClass);
	}
	
	//\__________________ update the key file if necessary.
	if (! bNeedUpdate)
		bNeedUpdate = cairo_dock_conf_file_needs_update (pKeyFile, GLDI_VERSION);
	if (bNeedUpdate)
	{
		cairo_dock_update_launcher_key_file (pKeyFile,
			cDesktopFilePath,
			iType == CAIRO_DOCK_ICON_TYPE_LAUNCHER ? CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER : iType == CAIRO_DOCK_ICON_TYPE_CONTAINER ? CAIRO_DOCK_DESKTOP_FILE_FOR_CONTAINER : CAIRO_DOCK_DESKTOP_FILE_FOR_SEPARATOR);
	}
	
	g_key_file_free (pKeyFile);
	g_free (cDesktopFilePath);
	return iType;
}


Icon * cairo_dock_new_launcher_icon (const gchar *cDesktopFileName, gchar **cSubDockRendererName)
{
	//\____________ create an empty icon.
	Icon *icon = cairo_dock_new_icon ();
	icon->iGroup = CAIRO_DOCK_LAUNCHER;
	
	//\____________ fill it with the information from its .desktop file.
	icon->iTrueType = cairo_dock_load_icon_info_from_desktop_file (cDesktopFileName, icon, cSubDockRendererName);
	
	//\____________ check the validity of the icon.
	if (icon->cDesktopFileName == NULL || (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon) && icon->cCommand == NULL))  // either a non readable .desktop file, or a launcher in a theme which does not correspond to any installed program => skip it.
	{
		cd_debug ("this launcher (%s) is not valid or does not correspond to any installed program", cDesktopFileName);
		cairo_dock_free_icon (icon);
		return NULL;
	}
	
	return icon;
}

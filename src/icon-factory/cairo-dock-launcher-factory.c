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

#include "cairo-dock-icons.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-launcher-factory.h"

extern gchar *g_cCurrentLaunchersPath;
extern int g_iNbNonStickyLaunchers;
extern CairoDockDesktopEnv g_iDesktopEnv;

gboolean cairo_dock_remove_version_from_string (gchar *cString)
{
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

void cairo_dock_set_launcher_class (Icon *icon, const gchar *cStartupWMClass)
{
	// plusieurs cas sont possibles :
	// Exec=toto
	// Exec=toto-1.2
	// Exec=toto -x -y
	// Exec=/path/to/toto -x -y
	// Exec=gksu nautilus /
	// Exec=gksu --description /usr/share/applications/synaptic.desktop /usr/sbin/synaptic
	// Exec=wine "C:\Program Files\Starcraft\Starcraft.exe"
	// Exec=wine "/path/to/prog.exe"
	// Exec=env WINEPREFIX="/home/fab/.wine" wine "C:\Program Files\Starcraft\Starcraft.exe"
	g_free (icon->cClass);
	if (icon->cCommand != NULL && icon->cBaseURI == NULL)
	{
		if (cStartupWMClass == NULL || *cStartupWMClass == '\0' || strcmp (cStartupWMClass, "Wine") == 0)  // on force pour wine, car meme si la classe est explicitement definie en tant que "Wine", cette information est inexploitable.
		{
			gchar *cDefaultClass = g_ascii_strdown (icon->cCommand, -1);
			gchar *str, *cClass = cDefaultClass;
			
			if (strncmp (cClass, "gksu", 4) == 0 || strncmp (cClass, "kdesu", 4) == 0)  // on prend la fin .
			{
				while (cClass[strlen(cClass)-1] == ' ')  // par securite on enleve les espaces en fin de ligne.
					cClass[strlen(cClass)-1] = '\0';
				str = strchr (cClass, ' ');  // on cherche le premier espace.
				if (str != NULL)  // on prend apres.
				{
					while (*str == ' ')
						str ++;
					cClass = str;
				}  // la on a vire le gksu.
				if (*cClass == '-')  // option, on prend le dernier argument de la commande.
				{
					str = strrchr (cClass, ' ');  // on cherche le dernier espace.
					if (str != NULL)  // on prend apres.
						cClass = str + 1;
				}
				else  // on prend le premier argument.
				{
					str = strchr (cClass, ' ');  // on cherche le premier espace.
					if (str != NULL)  // on vire apres.
						*str = '\0';
				}
				
				str = strrchr (cClass, '/');  // on cherche le dernier '/'.
				if (str != NULL)  // on prend apres.
					cClass = str + 1;
			}
			else if ((str = g_strstr_len (cClass, -1, "wine ")) != NULL)
			{
				cClass = str;  // on met deja la classe a "wine", c'est mieux que rien.
				*(str+4) = '\0';
				str += 5;
				gchar *exe = g_strstr_len (str, -1, ".exe");  // on cherche a isoler le nom de l'executable, puisque wine l'utilise dans le res_name.
				if (exe)
				{
					*(exe+4) = '\0';
					gchar *slash = strrchr (str, '\\');
					if (slash)
						cClass = slash+1;
					else
					{
						slash = strrchr (str, '/');
						if (slash)
							cClass = slash+1;
					}
				}
				cd_debug ("  special case : wine application => class = '%s'", cClass);
			}
			else
			{
				while (*cClass == ' ')  // par securite on enleve les espaces en debut de ligne.
					cClass ++;
				str = strchr (cClass, ' ');  // on cherche le premier espace.
				if (str != NULL)  // on vire apres.
					*str = '\0';
				str = strrchr (cClass, '/');  // on cherche le dernier '/'.
				if (str != NULL)  // on prend apres.
					cClass = str + 1;
				str = strchr (cClass, '.');  // on vire les .xxx, sinon on ne sait pas detecter l'absence d'extension quand on cherche l'icone (openoffice.org), ou tout simplement ca empeche de trouver l'icone (jbrout.py).
				if (str != NULL)
					*str = '\0';
			}
			
			if (*cClass != '\0')
				icon->cClass = g_strdup (cClass);
			else
				icon->cClass = NULL;
			g_free (cDefaultClass);
		}
		else
		{
			icon->cClass = g_ascii_strdown (cStartupWMClass, -1);
			gchar *str = strchr (icon->cClass, '.');  // on vire les .xxx, sinon on ne sait pas detecter l'absence d'extension quand on cherche l'icone (openoffice.org), ou tout simplement ca empeche de trouver l'icone (jbrout.py).
			if (str != NULL)
				*str = '\0';
		}
		cairo_dock_remove_version_from_string (icon->cClass);
		cd_debug ("class of the launcher %s : '%s'\n", icon->cName, icon->cClass);
	}
	else
		icon->cClass = NULL;
}

#define _print_error(cDesktopFileName, erreur)\
	if (erreur != NULL) {\
		cd_warning ("while trying to load %s : %s", cDesktopFileName, erreur->message);\
		g_error_free (erreur);\
		erreur = NULL; }
void cairo_dock_load_icon_info_from_desktop_file (const gchar *cDesktopFileName, Icon *icon, gchar **cSubDockRendererName)
{
	GError *erreur = NULL;
	gchar *cDesktopFilePath = (*cDesktopFileName == '/' ? g_strdup (cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cDesktopFileName));
	//g_print ("%s (%s)\n", __func__, cDesktopFilePath);
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	g_free (icon->cDesktopFileName);
	icon->cDesktopFileName = g_strdup (cDesktopFileName);

	g_free (icon->cFileName);
	icon->cFileName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Icon", &erreur);
	_print_error( cDesktopFileName, erreur);
	if (icon->cFileName != NULL && *icon->cFileName == '\0')
	{
		g_free (icon->cFileName);
		icon->cFileName = NULL;
	}
	
	g_free (icon->cName);
	icon->cName = g_key_file_get_locale_string (pKeyFile, "Desktop Entry", "Name", NULL, &erreur);
	_print_error( cDesktopFileName, erreur);
	if (icon->cName != NULL && *icon->cName == '\0')
	{
		g_free (icon->cName);
		icon->cName = NULL;
	}

	g_free (icon->cCommand);
	icon->cCommand = g_key_file_get_string (pKeyFile, "Desktop Entry", "Exec", &erreur);
	_print_error( cDesktopFileName, erreur);
	if (icon->cCommand != NULL && *icon->cCommand == '\0')
	{
		g_free (icon->cCommand);
		icon->cCommand = NULL;
	}
	
	if (icon->cCommand != NULL)
	{
		g_free (icon->cWorkingDirectory);
		icon->cWorkingDirectory = g_key_file_get_string (pKeyFile, "Desktop Entry", "Path", NULL);
		if (icon->cWorkingDirectory != NULL && *icon->cWorkingDirectory == '\0')
		{
			g_free (icon->cWorkingDirectory);
			icon->cWorkingDirectory = NULL;
		}
	}
	
	icon->fOrder = g_key_file_get_double (pKeyFile, "Desktop Entry", "Order", &erreur);
	_print_error( cDesktopFileName, erreur);

	/**icon->cBaseURI = g_key_file_get_string (pKeyFile, "Desktop Entry", "Base URI", NULL);
	if (icon->cBaseURI != NULL && *icon->cBaseURI == '\0')
	{
		g_free (icon->cBaseURI);
		icon->cBaseURI = NULL;
	}
	icon->iVolumeID = g_key_file_get_boolean (pKeyFile, "Desktop Entry", "Is mounting point", NULL);*/
	
	g_free (icon->cParentDockName);
	icon->cParentDockName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Container", &erreur);
	_print_error( cDesktopFileName, erreur);
	if (icon->cParentDockName == NULL || *icon->cParentDockName == '\0')
	{
		g_free (icon->cParentDockName);
		icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);
	}
	
	gint iNbSubIcons = g_key_file_get_integer (pKeyFile, "Desktop Entry", "Nb subicons", &erreur);
	if (erreur != NULL)
	{
		gboolean bIsContainer = g_key_file_get_boolean (pKeyFile, "Desktop Entry", "Is container", NULL);
		if (bIsContainer)
			iNbSubIcons = 3;
		g_error_free (erreur);
		erreur = NULL;
	}
	switch (iNbSubIcons)
	{
		case 1 : icon->iNbSubIcons = 5; break;
		case 2 : icon->iNbSubIcons = 10; break;
		case 3 : icon->iNbSubIcons = 20; break;
		case 4 : icon->iNbSubIcons = 30; break;
		case 5 : icon->iNbSubIcons = 1e4; break;
		default : icon->iNbSubIcons = 0; break;
	}
	
	if (icon->iNbSubIcons != 0 && icon->cName != NULL)
	{
		*cSubDockRendererName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Renderer", NULL);
	}
	if (icon->iNbSubIcons != 0)
	{
		icon->iSubdockViewType = g_key_file_get_integer (pKeyFile, "Desktop Entry", "render", NULL);  // on a besoin d'un entier dans le panneau de conf pour pouvoir degriser des options selon le rendu choisi. De plus c'est utile aussi pour Animated Icons...
	}

	gboolean bPreventFromInhibating = g_key_file_get_boolean (pKeyFile, "Desktop Entry", "prevent inhibate", NULL);  // FALSE si la cle n'existe pas.
	if (bPreventFromInhibating)
	{
		g_free (icon->cClass);
		icon->cClass = NULL;
	}
	else
	{
		gchar *cStartupWMClass = g_key_file_get_string (pKeyFile, "Desktop Entry", "StartupWMClass", NULL);
		cairo_dock_set_launcher_class (icon, cStartupWMClass);
		g_free (cStartupWMClass);
	}
	
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
	
	if (g_key_file_has_key (pKeyFile, "Desktop Entry", "group", NULL))
	{
		icon->iType = g_key_file_get_integer (pKeyFile, "Desktop Entry", "group", NULL);
	}
	
	int iSpecificDesktop = g_key_file_get_integer (pKeyFile, "Desktop Entry", "ShowOnViewport", NULL);
	if (iSpecificDesktop != 0 && icon->iSpecificDesktop == 0)
	{
		g_iNbNonStickyLaunchers ++;
	}
	else if (iSpecificDesktop == 0 && icon->iSpecificDesktop != 0)
	{
		g_iNbNonStickyLaunchers --;
	}
	icon->iSpecificDesktop = iSpecificDesktop;
	
	g_key_file_free (pKeyFile);
}


Icon * cairo_dock_new_launcher_icon (const gchar *cDesktopFileName, gchar **cSubDockRendererName)
{
	//\____________ On cree l'icone.
	Icon *icon = g_new0 (Icon, 1);
	icon->iType = CAIRO_DOCK_LAUNCHER;
	
	//\____________ On recupere les infos de son .desktop.
	cairo_dock_load_icon_info_from_desktop_file (cDesktopFileName, icon, cSubDockRendererName);
	g_return_val_if_fail (icon->cDesktopFileName != NULL, NULL);
	
	if (icon->cBaseURI != NULL)
		icon->iTrueType = CAIRO_DOCK_ICON_TYPE_FILE;
	else if (icon->cCommand != NULL)
		icon->iTrueType = CAIRO_DOCK_ICON_TYPE_LAUNCHER;
	else if (icon->iNbSubIcons != 0)
		icon->iTrueType = CAIRO_DOCK_ICON_TYPE_CONTAINER;
	else
		icon->iTrueType = CAIRO_DOCK_ICON_TYPE_SEPARATOR;
	
	return icon;
}

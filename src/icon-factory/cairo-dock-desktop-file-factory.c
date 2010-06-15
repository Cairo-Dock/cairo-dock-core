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

#include "../../config.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-desktop-file-factory.h"

#define CAIRO_DOCK_LAUNCHER_CONF_FILE "launcher.desktop"
#define CAIRO_DOCK_FILE_CONF_FILE "file.desktop"
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


static inline const gchar *_cairo_dock_get_launcher_template_conf_file_path (CairoDockDesktopFileType iNewDesktopFileType)
{
	const gchar *cTemplateFile;
	switch (iNewDesktopFileType)
	{
		case CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER :
			cTemplateFile = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_LAUNCHER_CONF_FILE;
		break ;
		case CAIRO_DOCK_DESKTOP_FILE_FOR_CONTAINER :
			cTemplateFile = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_CONTAINER_CONF_FILE;
		break ;
		case CAIRO_DOCK_DESKTOP_FILE_FOR_SEPARATOR :
			cTemplateFile = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_SEPARATOR_CONF_FILE;
		break ;
		case CAIRO_DOCK_DESKTOP_FILE_FOR_FILE :
			cTemplateFile = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_FILE_CONF_FILE;
		break ;
		default:
			cTemplateFile = NULL;
	}
	return cTemplateFile;
}

static gchar *_cairo_dock_generate_desktop_file_for_launcher (const gchar *cDesktopURI, const gchar *cDockName, double fOrder, CairoDockIconType iGroup, GError **erreur)
{
	g_return_val_if_fail (cDesktopURI != NULL, NULL);
	GError *tmp_erreur = NULL;
	gchar *cFilePath = (*cDesktopURI == '/' ? g_strdup (cDesktopURI) : g_filename_from_uri (cDesktopURI, NULL, &tmp_erreur));
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return NULL;
	}

	//\___________________ On ouvre le patron.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cFilePath);
	if (pKeyFile == NULL)
		return NULL;

	//\___________________ On renseigne nos champs.
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);
	
	//\___________________ On elimine les indesirables.
	g_key_file_remove_key (pKeyFile, "Desktop Entry", "X-Ubuntu-Gettext-Domain", NULL);
	gchar *cCommand = g_key_file_get_string (pKeyFile, "Desktop Entry", "Exec", &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		g_key_file_free (pKeyFile);
		return NULL;
	}
	gchar *str = strchr (cCommand, '%');
	if (str != NULL)
	{
		*str = '\0';
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", cCommand);
	}
	g_free (cCommand);

	gchar *cIconName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Icon", &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		g_key_file_free (pKeyFile);
		return NULL;
	}
	if (*cIconName != '/' && (g_str_has_suffix (cIconName, ".svg") || g_str_has_suffix (cIconName, ".png") || g_str_has_suffix (cIconName, ".xpm")))
	{
		cIconName[strlen(cIconName) - 4] = '\0';
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Icon", cIconName);
	}
	g_free (cIconName);
	
	gchar **pKeyList = g_key_file_get_keys (pKeyFile, "Desktop Entry", NULL, NULL);  // on enleve les cles 'Icon' traduites !
	gchar *cKeyName;
	int i;
	for (i = 0; pKeyList[i] != NULL; i ++)
	{
		cKeyName = pKeyList[i];
		if (strncmp (cKeyName, "Icon[", 5) == 0)
			g_key_file_remove_key (pKeyFile, "Desktop Entry", cKeyName, NULL);
	}
	g_strfreev (pKeyList);
	
	g_key_file_set_integer (pKeyFile, "Desktop Entry", "group", iGroup);
	
	//\___________________ On lui choisit un nom de fichier tel qu'il n'y ait pas de collision.
	gchar *cBaseName = g_path_get_basename (cFilePath);
	gchar *cNewDesktopFileName = _cairo_dock_generate_desktop_filename (cBaseName, g_cCurrentLaunchersPath);
	g_free (cBaseName);

	//\___________________ On ecrit tout ca dans un fichier base sur le template.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_flush_conf_file_full (pKeyFile, cNewDesktopFilePath, CAIRO_DOCK_SHARE_DATA_DIR, FALSE, CAIRO_DOCK_LAUNCHER_CONF_FILE);
	
	g_free (cNewDesktopFilePath);
	g_key_file_free (pKeyFile);
	g_free (cFilePath);

	return cNewDesktopFileName;
}

static gchar *_cairo_dock_generate_desktop_file_for_file (const gchar *cURI, const gchar *cDockName, double fOrder, GError **erreur)
{
	//\___________________ On recupere le type mime du fichier.
	gchar *cIconName = NULL, *cName = NULL, *cRealURI = NULL;
	gboolean bIsDirectory;
	int iVolumeID;
	double fUnusedOrder;
	if (! cairo_dock_fm_get_file_info (cURI, &cName, &cRealURI, &cIconName, &bIsDirectory, &iVolumeID, &fUnusedOrder, 0) || cIconName == NULL)
		return NULL;
	cd_message (" -> cIconName : %s; bIsDirectory : %d; iVolumeID : %d\n", cIconName, bIsDirectory, iVolumeID);

	/**if (bIsDirectory)
	{
		int answer = cairo_dock_ask_general_question_and_wait (_("Do you want to monitor the content of this directory?"));
		if (answer != GTK_RESPONSE_YES)
			bIsDirectory = FALSE;
	}*/

	//\___________________ On ouvre le patron.
	const gchar *cDesktopFileTemplate = _cairo_dock_get_launcher_template_conf_file_path (CAIRO_DOCK_DESKTOP_FILE_FOR_FILE);
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cDesktopFileTemplate);
	if (pKeyFile == NULL)
		return NULL;

	//\___________________ On renseigne ce qu'on peut.
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Base URI", cURI);

	g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cName);
	g_free (cName);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", cRealURI);
	g_free (cRealURI);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Icon", (cIconName != NULL ? cIconName : ""));
	g_free (cIconName);

	g_key_file_set_boolean (pKeyFile, "Desktop Entry", "Is mounting point", (iVolumeID > 0));
	g_key_file_set_integer (pKeyFile, "Desktop Entry", "Nb subicons", (bIsDirectory ? 3 : 0));

	//\___________________ On lui choisit un nom de fichier tel qu'il n'y ait pas de collision.
	gchar *cNewDesktopFileName = _cairo_dock_generate_desktop_filename ("file-launcher.desktop", g_cCurrentLaunchersPath);

	//\___________________ On ecrit tout.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_write_keys_to_file (pKeyFile, cNewDesktopFilePath);
	g_free (cNewDesktopFilePath);
	g_key_file_free (pKeyFile);

	return cNewDesktopFileName;
}

static gchar *_cairo_dock_generate_desktop_file_for_script (const gchar *cURI, const gchar *cDockName, double fOrder, GError **erreur)
{
	//\___________________ On ouvre le patron.
	const gchar *cDesktopFileTemplate = _cairo_dock_get_launcher_template_conf_file_path (CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER);
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cDesktopFileTemplate);
	if (pKeyFile == NULL)
		return NULL;
	
	//\___________________ On renseigne ce qu'on peut.
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);
	gchar *cName = g_path_get_basename (cURI);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cName);
	g_free (cName);
	gchar *cFilePath = (*cURI == '/' ? g_strdup (cURI) : g_filename_from_uri (cURI, NULL, NULL));
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", cFilePath);
	g_free (cFilePath);
	g_key_file_set_boolean (pKeyFile, "Desktop Entry", "Terminal", TRUE);
	
	//\___________________ On lui choisit un nom de fichier tel qu'il n'y ait pas de collision.
	gchar *cNewDesktopFileName = _cairo_dock_generate_desktop_filename ("script-launcher.desktop", g_cCurrentLaunchersPath);
	
	//\___________________ On ecrit tout.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_write_keys_to_file (pKeyFile, cNewDesktopFilePath);
	g_free (cNewDesktopFilePath);
	g_key_file_free (pKeyFile);

	return cNewDesktopFileName;
}


gchar *cairo_dock_add_desktop_file_from_uri (const gchar *cURI, const gchar *cDockName, double fOrder, CairoDockIconType iGroup, GError **erreur)
{
	g_return_val_if_fail (cURI != NULL, NULL);
	cd_message ("%s (%s)", __func__, cURI);
	/**double fEffectiveOrder;
	if (fOrder == CAIRO_DOCK_LAST_ORDER && pDock != NULL)
	{
		Icon *pLastIcon = cairo_dock_get_last_launcher (pDock->icons);
		if (pLastIcon != NULL)
			fEffectiveOrder = pLastIcon->fOrder + 1;
		else
			fEffectiveOrder = 1;
	}
	else
		fEffectiveOrder = fOrder;*/

	//\_________________ On cree determine le type de lanceur et on ajoute un fichier desktop correspondant.
	GError *tmp_erreur = NULL;
	gchar *cNewDesktopFileName;
	if (g_str_has_suffix (cURI, ".desktop"))  // lanceur.
	{
		cNewDesktopFileName = _cairo_dock_generate_desktop_file_for_launcher (cURI, cDockName, fOrder, iGroup, &tmp_erreur);
	}
	else if (g_str_has_suffix (cURI, ".sh"))  // script.
	{
		cd_message ("This file will be treated as a launcher, not as a file.\nIf this doesn't fit you, you should use the Stack applet, which is dedicated to file stacking.");
		cNewDesktopFileName = _cairo_dock_generate_desktop_file_for_script (cURI, cDockName, fOrder, &tmp_erreur);
	}
	else  // fichier, repertoire ou point de montage.
	{
		cNewDesktopFileName = _cairo_dock_generate_desktop_file_for_file (cURI, cDockName, fOrder, &tmp_erreur);
	}

	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
	}
	return cNewDesktopFileName;
}

gchar *cairo_dock_add_desktop_file_from_type (CairoDockDesktopFileType iLauncherType, const gchar *cDockName, double fOrder, CairoDockIconType iGroup, GError **erreur)
{
	const gchar *cTemplateFile = _cairo_dock_get_launcher_template_conf_file_path (iLauncherType);
	return cairo_dock_add_desktop_file_from_uri (cTemplateFile, cDockName, fOrder, iGroup, erreur);
}


void cairo_dock_update_launcher_desktop_file (gchar *cDesktopFilePath, CairoDockDesktopFileType iLauncherType)
{
	GError *erreur = NULL;
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
	if (pKeyFile == NULL)
		return ;

	if (cairo_dock_conf_file_needs_update (pKeyFile, CAIRO_DOCK_VERSION))
	{
		const gchar *cTemplateFile = _cairo_dock_get_launcher_template_conf_file_path (iLauncherType);
		cd_debug ("%s (%s)", __func__, cTemplateFile);
		cairo_dock_flush_conf_file_full (pKeyFile,
			cDesktopFilePath,
			CAIRO_DOCK_SHARE_DATA_DIR,
			FALSE,
			cTemplateFile);
	}
	
	g_key_file_free (pKeyFile);
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
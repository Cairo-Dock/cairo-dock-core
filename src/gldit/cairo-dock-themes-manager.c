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

#include <string.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <sys/stat.h>
#define __USE_POSIX
#include <time.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "gldi-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-launcher-manager.h" // cairo_dock_get_command_with_right_terminal
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-module-manager.h"  // gldi_module_foreach
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-icon-facility.h"  // gldi_icons_get_any_without_dialog
#include "cairo-dock-task.h"
#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_get_command_with_right_terminal
#include "cairo-dock-packages.h"
#include "cairo-dock-core.h"
#include "cairo-dock-applications-manager.h"  // cairo_dock_get_current_active_icon
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-themes-manager.h"

// public data
gchar *g_cCairoDockDataDir = NULL;  // le repertoire racine contenant tout.
gchar *g_cCurrentThemePath = NULL;  // le chemin vers le repertoire du theme courant.
gchar *g_cExtrasDirPath = NULL;  // le chemin vers le repertoire des extra.
gchar *g_cThemesDirPath = NULL;  // le chemin vers le repertoire des themes.
gchar *g_cCurrentLaunchersPath = NULL;  // le chemin vers le repertoire des lanceurs du theme courant.
gchar *g_cCurrentIconsPath = NULL;  // le chemin vers le repertoire des icones du theme courant.
gchar *g_cCurrentImagesPath = NULL;  // le chemin vers le repertoire des images ou autre du theme courant.
gchar *g_cCurrentPlugInsPath = NULL;  // le chemin vers le repertoire des plug-ins du theme courant.
gchar *g_cConfFile = NULL;  // le chemin du fichier de conf.

// private
static gchar *s_cLocalThemeDirPath = NULL;
static gchar *s_cDistantThemeDirName = NULL;

#define CAIRO_DOCK_MODIFIED_THEME_FILE ".cairo-dock-need-save"
// the structure of a theme (including the current theme, except that it doesn't have an extras folders)
#define CAIRO_DOCK_LOCAL_EXTRAS_DIR "extras"
#define CAIRO_DOCK_LAUNCHERS_DIR "launchers"
#define CAIRO_DOCK_PLUG_INS_DIR "plug-ins"
#define CAIRO_DOCK_LOCAL_ICONS_DIR "icons"
#define CAIRO_DOCK_LOCAL_IMAGES_DIR "images"

// dependancies
extern CairoDock *g_pMainDock;


static gchar * _replace_slash_by_underscore (gchar *cName)
{
	g_return_val_if_fail (cName != NULL, NULL);

	for (int i = 0; cName[i] != '\0'; i++)
	{
		if (cName[i] == '/' || cName[i] == '$')
			cName[i] = '_';
	}

	return cName;
}

/** Escapes the special characters '\b', '\f', '\n', '\r', '\t', '\v', '\' and '"' in the string source by inserting a '\' before them and replaces '/' and '$' by '_'
 * @param cOldName name of a filename
 * @return a newly-allocated copy of source with certain characters escaped. See above.
 */
static gchar * _escape_string_for_filename (const gchar *cOldName)
{
	gchar *cNewName = g_strescape (cOldName, NULL);

	return _replace_slash_by_underscore (cNewName);
}

static void cairo_dock_mark_current_theme_as_modified (gboolean bModified)
{
	static int state = -1;
	
	if (state == -1)
		state = cairo_dock_current_theme_need_save ();
	
	if (state != bModified)
	{
		state = bModified;
		gchar *cModifiedFile = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_MODIFIED_THEME_FILE);
		g_file_set_contents (cModifiedFile,
			(bModified ? "1" : "0"),
			-1,
			NULL);
		g_free (cModifiedFile);
	}
}

gboolean cairo_dock_current_theme_need_save (void)
{
	gchar *cModifiedFile = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_MODIFIED_THEME_FILE);
	gsize length = 0;
	gchar *cContent = NULL;
	g_file_get_contents (cModifiedFile,
		&cContent,
		&length,
		NULL);
	g_free (cModifiedFile);
	gboolean bNeedSave;
	if (length > 0)
		bNeedSave = (*cContent == '1');
	else
		bNeedSave = FALSE;
	g_free (cContent);
	return bNeedSave;
}

void cairo_dock_delete_conf_file (const gchar *cConfFilePath)
{
	g_remove (cConfFilePath);
	cairo_dock_mark_current_theme_as_modified (TRUE);
}

gboolean cairo_dock_add_conf_file (const gchar *cOriginalConfFilePath, const gchar *cConfFilePath)
{
	gboolean r = cairo_dock_copy_file (cOriginalConfFilePath, cConfFilePath);
	if (r)
		cairo_dock_mark_current_theme_as_modified (TRUE);
	return r;
}

void cairo_dock_update_conf_file (const gchar *cConfFilePath, GType iFirstDataType, ...)
{
	va_list args;
	va_start (args, iFirstDataType);
	cairo_dock_update_keyfile_va_args (cConfFilePath, iFirstDataType, args);
	va_end (args);
	
	cairo_dock_mark_current_theme_as_modified (TRUE);
}

void cairo_dock_write_keys_to_conf_file (GKeyFile *pKeyFile, const gchar *cConfFilePath)
{
	cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	
	cairo_dock_mark_current_theme_as_modified (TRUE);
}


gboolean cairo_dock_export_current_theme (const gchar *cNewThemeName, gboolean bSaveBehavior, gboolean bSaveLaunchers)
{
	g_return_val_if_fail (cNewThemeName != NULL, FALSE);

	gchar *cNewThemeNameWithoutSlashes = _replace_slash_by_underscore (g_strdup (cNewThemeName));
	
	cairo_dock_extract_package_type_from_name (cNewThemeNameWithoutSlashes);

	gchar *cNewThemeNameEscaped = g_strescape (cNewThemeNameWithoutSlashes, NULL);

	cd_message ("we save in %s", cNewThemeNameWithoutSlashes);
	GString *sCommand = g_string_new ("");
	gboolean bThemeSaved = FALSE;
	int r;
	gchar *cNewThemePath = g_strdup_printf ("%s/%s", g_cThemesDirPath, cNewThemeNameWithoutSlashes);
	gchar *cNewThemePathEscaped = g_strdup_printf ("%s/%s", g_cThemesDirPath, cNewThemeNameEscaped);
	if (g_file_test (cNewThemePath, G_FILE_TEST_EXISTS))  // on ecrase un theme existant.
	{
		cd_debug ("  This theme will be updated");
		gchar *cQuestion = g_strdup_printf (_("Are you sure you want to overwrite theme %s?"), cNewThemeName);
		Icon *pIcon = cairo_dock_get_current_active_icon ();  // it's most probably the icon corresponding to the configuration window
		if (pIcon == NULL || cairo_dock_get_icon_container (pIcon) == NULL)  // if not available, get any icon
			pIcon = gldi_icons_get_any_without_dialog ();
		cd_debug ("%s", pIcon->cName);
		int iClickedButton = gldi_dialog_show_and_wait (cQuestion,
			pIcon, CAIRO_CONTAINER (g_pMainDock),
			GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, NULL);
		g_free (cQuestion);
		if (iClickedButton == 0 || iClickedButton == -1)  // ok button or Enter.
		{
			//\___________________ On traite le fichier de conf global et ceux des docks principaux.
			gchar *cNewConfFilePath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_CONF_FILE);
			if (bSaveBehavior)
			{
				cairo_dock_copy_file (g_cConfFile, cNewConfFilePath);
			}
			else
			{
				cairo_dock_merge_conf_files (cNewConfFilePath, g_cConfFile, '+');
			}
			g_free (cNewConfFilePath);
			
			//\___________________ On traite les lanceurs.
			if (bSaveLaunchers)
			{
				g_string_printf (sCommand, "rm -f \"%s/%s\"/*", cNewThemePathEscaped, CAIRO_DOCK_LAUNCHERS_DIR);
				cd_message ("%s", sCommand->str);
				r = system (sCommand->str);
				if (r < 0)
					cd_warning ("Not able to launch this command: %s", sCommand->str);
				
				g_string_printf (sCommand, "cp \"%s\"/* \"%s/%s\"", g_cCurrentLaunchersPath, cNewThemePathEscaped, CAIRO_DOCK_LAUNCHERS_DIR);
				cd_message ("%s", sCommand->str);
				r = system (sCommand->str);
				if (r < 0)
					cd_warning ("Not able to launch this command: %s", sCommand->str);
			}
			
			//\___________________ On traite tous le reste.
			/// TODO : traiter les .conf des applets comme celui du dock...
			g_string_printf (sCommand, "find \"%s\" -mindepth 1 -maxdepth 1  ! -name '%s' ! -name \"%s\" -exec /bin/cp -r '{}' \"%s\" \\;", g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE, CAIRO_DOCK_LAUNCHERS_DIR, cNewThemePathEscaped);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
			if (r < 0)
				cd_warning ("Not able to launch this command: %s", sCommand->str);

			bThemeSaved = TRUE;
		}
	}
	else  // sinon on sauvegarde le repertoire courant tout simplement.
	{
		cd_debug ("  creation of the new theme (%s)", cNewThemePath);

		if (g_mkdir (cNewThemePath, 7*8*8+7*8+5) == 0)
		{
			g_string_printf (sCommand, "cp -r \"%s\"/* \"%s\"", g_cCurrentThemePath, cNewThemePathEscaped);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
			if (r < 0)
				cd_warning ("Not able to launch this command: %s", sCommand->str);

			bThemeSaved = TRUE;
		}
		else
			cd_warning ("couldn't create %s", cNewThemePath);
	}

	g_free (cNewThemeNameEscaped);
	g_free (cNewThemeNameWithoutSlashes);

	//\___________________ On conserve la date de derniere modif.
	time_t epoch = (time_t) time (NULL);
	struct tm currentTime;
	localtime_r (&epoch, &currentTime);
	char cDateBuffer[50+1];
	strftime (cDateBuffer, 50, "%a %d %b %Y, %R", &currentTime);
	gchar *cMessage = g_strdup_printf ("%s\n %s", _("Last modification on:"), cDateBuffer);
	gchar *cReadmeFile = g_strdup_printf ("%s/%s", cNewThemePath, "readme");
	g_file_set_contents (cReadmeFile,
		cMessage,
		-1,
		NULL);
	g_free (cReadmeFile);
	g_free (cMessage);
	
	g_string_printf (sCommand, "rm -f \"%s/last-modif\"", cNewThemePathEscaped);
	r = system (sCommand->str);
	
	//\___________________ make a preview of the current main dock.
	gchar *cPreviewPath = g_strdup_printf ("%s/preview", cNewThemePath);
	cairo_dock_make_preview (g_pMainDock, cPreviewPath);
	g_free (cPreviewPath);
	
	//\___________________ Le theme n'est plus en etat 'modifie'.
	g_free (cNewThemePath);
	g_free (cNewThemePathEscaped);
	if (bThemeSaved)
	{
		cairo_dock_mark_current_theme_as_modified (FALSE);
	}
	
	g_string_free (sCommand, TRUE);
	
	return bThemeSaved;
}

gboolean cairo_dock_package_current_theme (const gchar *cThemeName, const gchar *cDirPath)
{
	g_return_val_if_fail (cThemeName != NULL, FALSE);
	gboolean bSuccess = FALSE;

	gchar *cNewThemeName = _escape_string_for_filename (cThemeName);
	if (cDirPath == NULL || *cDirPath == '\0'
		|| (g_file_test (cDirPath, G_FILE_TEST_EXISTS) && g_file_test (cDirPath, G_FILE_TEST_IS_REGULAR))) // exist but not a directory
		cDirPath = g_getenv ("HOME");
	
	cairo_dock_extract_package_type_from_name (cNewThemeName);
	
	cd_message ("building theme package ...");
	const gchar *cPackageBuilderPath = GLDI_SHARE_DATA_DIR"/scripts/cairo-dock-package-theme.sh";
	gboolean bScriptFound = g_file_test (cPackageBuilderPath, G_FILE_TEST_EXISTS);
	if (bScriptFound)
	{
		int r;
		gchar *cCommand = g_strdup_printf ("%s '%s' '%s'",
			cPackageBuilderPath, cNewThemeName, cDirPath);
		gchar *cFullCommand = cairo_dock_get_command_with_right_terminal (cCommand);
		r = system (cFullCommand); // we need to wait...
		if (r != 0)
		{
			cd_warning ("Not able to launch this command: %s, retry without external terminal", cFullCommand);
			r = system (cCommand); // relaunch it without the terminal and wait
			if (r != 0)
				cd_warning ("Not able to launch this command: %s", cCommand);
			else
				bSuccess = TRUE;
		}
		else
			bSuccess = TRUE;
		g_free (cCommand);
		g_free (cFullCommand);
	}
	else
		cd_warning ("the package builder script was not found !");

	if (bSuccess)
	{
		gchar *cGeneralMessage = g_strdup_printf ("%s %s", _("Your theme should now be available in this directory:"), cDirPath);
		gldi_dialog_show_general_message (cGeneralMessage, 8000);
		g_free (cGeneralMessage);
	}
	else
		gldi_dialog_show_general_message (_("Error when launching 'cairo-dock-package-theme' script"), 8000);

	g_free (cNewThemeName);
	return bSuccess;
}
gchar *cairo_dock_depackage_theme (const gchar *cPackagePath)
{
	gchar *cNewThemePath = NULL;
	if (*cPackagePath == '/' || strncmp (cPackagePath, "file://", 7) == 0)  // paquet en local.
	{
		cd_debug (" paquet local");
		gchar *cFilePath = (*cPackagePath == '/' ? g_strdup (cPackagePath) : g_filename_from_uri (cPackagePath, NULL, NULL));
		cNewThemePath = cairo_dock_uncompress_file (cFilePath, g_cThemesDirPath, NULL);
		g_free (cFilePath);
	}
	else  // paquet distant.
	{
		cd_debug (" paquet distant");
		cNewThemePath = cairo_dock_download_archive (cPackagePath, g_cThemesDirPath);
		if (cNewThemePath == NULL)
		{
			gldi_dialog_show_temporary_with_icon_printf (_("Could not access remote file %s. Maybe the server is down.\nPlease retry later or contact us at glx-dock.org."), NULL, NULL, 0, NULL, cPackagePath);
		}
	}
	return cNewThemePath;
}

gboolean cairo_dock_delete_themes (gchar **cThemesList)
{
	g_return_val_if_fail (cThemesList != NULL && cThemesList[0] != NULL, FALSE);
	
	GString *sCommand = g_string_new ("");
	gboolean bThemeDeleted = FALSE;
	
	if (cThemesList[1] == NULL)
		g_string_printf (sCommand, _("Are you sure you want to delete theme %s?"), cThemesList[0]);
	else
		g_string_printf (sCommand, _("Are you sure you want to delete these themes?"));
	Icon *pIcon = cairo_dock_get_current_active_icon ();  // it's most probably the icon corresponding to the configuration window
	if (pIcon == NULL || cairo_dock_get_icon_container (pIcon) == NULL)  // if not available, get any icon
		pIcon = gldi_icons_get_any_without_dialog ();
	int iClickedButton = gldi_dialog_show_and_wait (sCommand->str,
		pIcon, cairo_dock_get_icon_container (pIcon),
		GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, NULL);
	if (iClickedButton == 0 || iClickedButton == -1)  // ok button or Enter.
	{
		gchar *cThemeName;
		int i, r;
		for (i = 0; cThemesList[i] != NULL; i ++)
		{
			cThemeName = _escape_string_for_filename (cThemesList[i]);
			if (*cThemeName == '\0')
			{
				g_free (cThemeName);
				continue;
			}
			cairo_dock_extract_package_type_from_name (cThemeName);
			
			bThemeDeleted = TRUE;
			g_string_printf (sCommand, "rm -rf \"%s/%s\"", g_cThemesDirPath, cThemeName);
			r = system (sCommand->str);  // g_rmdir only delete an empty dir...
			if (r < 0)
				cd_warning ("Not able to launch this command: %s", sCommand->str);
			g_free (cThemeName);
		}
	}
	
	g_string_free (sCommand, TRUE);
	return bThemeDeleted;
}

static gboolean _find_module_from_user_data_dir (G_GNUC_UNUSED gchar *cModuleName, GldiModule *pModule, const gchar *cUserDataDirName)
{
	if (pModule->pVisitCard->cUserDataDir && strcmp (cUserDataDirName, pModule->pVisitCard->cUserDataDir) == 0)
		return TRUE;
	return FALSE;
}

static gchar *_cairo_dock_get_theme_path (const gchar *cThemeName)  // a theme name or a package URL, both distant or local
{
	gchar *cNewThemeName = g_strdup (cThemeName);
	gchar *cNewThemePath = NULL;
	
	int length = strlen (cNewThemeName);
	if (cNewThemeName[length-1] == '\n')
		cNewThemeName[--length] = '\0';  // on vire le retour chariot final.
	if (cNewThemeName[length-1] == '\r')
		cNewThemeName[--length] = '\0';
	cd_debug ("cNewThemeName : '%s'", cNewThemeName);
	
	if (g_str_has_suffix (cNewThemeName, ".tar.gz") || g_str_has_suffix (cNewThemeName, ".tar.bz2") || g_str_has_suffix (cNewThemeName, ".tgz"))  // c'est un paquet.
	{
		cd_debug ("it's a tarball");
		cNewThemePath = cairo_dock_depackage_theme (cNewThemeName);
	}
	else  // c'est un theme officiel.
	{
		cd_debug ("it's an official theme");
		cNewThemePath = cairo_dock_get_package_path (cNewThemeName, s_cLocalThemeDirPath, g_cThemesDirPath, s_cDistantThemeDirName, CAIRO_DOCK_ANY_PACKAGE);
	}
	g_free (cNewThemeName);
	return cNewThemePath;
}

static void _launch_cmd (const gchar *cCommand)
{
	cd_debug ("%s", cCommand);
	int r = system (cCommand);
	if (r < 0)
		cd_warning ("Not able to launch this command: %s", cCommand);
}

static gboolean _cairo_dock_import_local_theme (const gchar *cNewThemePath, gboolean bLoadBehavior, gboolean bLoadLaunchers)
{
	g_return_val_if_fail (cNewThemePath != NULL && g_file_test (cNewThemePath, G_FILE_TEST_EXISTS), FALSE);
	
	//\___________________ We load global behaviour parameters for each dock.
	GString *sCommand = g_string_new ("");
	cd_message ("Applying changes ...");
	if (g_pMainDock == NULL || bLoadBehavior)
	{
		g_string_printf (sCommand, "rm -f \"%s\"/*.conf", g_cCurrentThemePath);
		_launch_cmd (sCommand->str);

		g_string_printf (sCommand, "cp \"%s\"/*.conf \"%s\"", cNewThemePath, g_cCurrentThemePath);
		_launch_cmd (sCommand->str);
	}
	else
	{
		GDir *dir = g_dir_open (cNewThemePath, 0, NULL);
		const gchar* cDockConfFile;
		gchar *cThemeDockConfFile, *cUserDockConfFile;
		while ((cDockConfFile = g_dir_read_name (dir)) != NULL)
		{
			if (g_str_has_suffix (cDockConfFile, ".conf"))
			{
				cThemeDockConfFile = g_strdup_printf ("%s/%s", cNewThemePath, cDockConfFile);
				cUserDockConfFile = g_strdup_printf ("%s/%s", g_cCurrentThemePath, cDockConfFile);
				if (g_file_test (cUserDockConfFile, G_FILE_TEST_EXISTS))
				{
					cairo_dock_merge_conf_files (cUserDockConfFile, cThemeDockConfFile, '+');
				}
				else
				{
					cairo_dock_copy_file (cThemeDockConfFile, cUserDockConfFile);
				}
				g_free (cUserDockConfFile);
				g_free (cThemeDockConfFile);
			}
		}
		g_dir_close (dir);
	}
	
	//\___________________ We load icons
	if (bLoadLaunchers)
	{
		g_string_printf (sCommand, "rm -f \"%s\"/*", g_cCurrentIconsPath);
		_launch_cmd (sCommand->str);
		g_string_printf (sCommand, "rm -f \"%s\"/.*", g_cCurrentIconsPath);
		_launch_cmd (sCommand->str);
		
		g_string_printf (sCommand, "rm -f \"%s\"/*", g_cCurrentImagesPath);
		_launch_cmd (sCommand->str);
		g_string_printf (sCommand, "rm -f \"%s\"/.*", g_cCurrentImagesPath);
		_launch_cmd (sCommand->str);
	}
	gchar *cNewLocalIconsPath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
	if (! g_file_test (cNewLocalIconsPath, G_FILE_TEST_IS_DIR))  // it's an old theme: move icons to a new dir 'icons'.
	{
		g_string_printf (sCommand, "find \"%s/%s\" -mindepth 1 ! -name '*.desktop' -exec /bin/cp '{}' '%s' \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentIconsPath);
	}
	else
	{
		g_string_printf (sCommand, "for f in \"%s\"/* ; do rm -f \"%s/`basename \"${f%%.*}\"`\"*; done;", cNewLocalIconsPath, g_cCurrentIconsPath);  // we erase double items because we could have x.png and x.svg and the dock will not know which it has to use.
		_launch_cmd (sCommand->str);
		
		g_string_printf (sCommand, "cp \"%s\"/* \"%s\"", cNewLocalIconsPath, g_cCurrentIconsPath);
	}
	_launch_cmd (sCommand->str);
	g_free (cNewLocalIconsPath);
	
	//\___________________ We load extras.
	g_string_printf (sCommand, "%s/%s", cNewThemePath, CAIRO_DOCK_LOCAL_EXTRAS_DIR);
	if (g_file_test (sCommand->str, G_FILE_TEST_IS_DIR))
	{
		g_string_printf (sCommand, "cp -r \"%s/%s\"/* \"%s\"", cNewThemePath, CAIRO_DOCK_LOCAL_EXTRAS_DIR, g_cExtrasDirPath);
		_launch_cmd (sCommand->str);
	}
	
	//\___________________ We load launcher if needed after having removed old ones.
	if (! g_file_test (g_cCurrentLaunchersPath, G_FILE_TEST_EXISTS))
	{
		gchar *command = g_strdup_printf ("mkdir -p \"%s\"", g_cCurrentLaunchersPath);
		_launch_cmd (sCommand->str);
		g_free (command);
	}
	if (g_pMainDock == NULL || bLoadLaunchers)
	{
		g_string_printf (sCommand, "rm -f \"%s\"/*.desktop", g_cCurrentLaunchersPath);
		_launch_cmd (sCommand->str);

		g_string_printf (sCommand, "cp \"%s/%s\"/*.desktop \"%s\"", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentLaunchersPath);
		_launch_cmd (sCommand->str);
	}
	
	//\___________________ We replace all files by the new ones.
	g_string_printf (sCommand, "find \"%s\" -mindepth 1 -maxdepth 1  ! -name '*.conf' -type f -exec rm -f '{}' \\;", g_cCurrentThemePath);  // remove all ficher of the theme except launchers and plugins.
	_launch_cmd (sCommand->str);

	if (g_pMainDock == NULL || bLoadBehavior)
	{
		g_string_printf (sCommand, "find \"%s\"/* -prune ! -name '*.conf' ! -name %s -exec /bin/cp -r '{}' \"%s\" \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);  // Copy all files of the new theme except launchers and .conf files in the dir of the current theme. Overwrite files with same names
		_launch_cmd (sCommand->str);
	}
	else
	{
		// We copy all files of the new theme except launchers and .conf files (dock and plug-ins).
		g_string_printf (sCommand, "find \"%s\" -mindepth 1 ! -name '*.conf' ! -path '%s/%s*' ! -type d -exec cp -p {} \"%s\" \\;", cNewThemePath, cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);
		_launch_cmd (sCommand->str);
		
		// iterate all .conf files of all plug-ins, then update them and merge them with the current theme.
		gchar *cNewPlugInsDir = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_PLUG_INS_DIR);  // dir of plug-ins of the new theme.
		GDir *dir = g_dir_open (cNewPlugInsDir, 0, NULL);  // NULL if this theme doesn't have any 'plug-ins' dir.
		const gchar* cModuleDirName;
		gchar *cConfFilePath, *cNewConfFilePath, *cUserDataDirPath, *cConfFileName;
		do
		{
			cModuleDirName = g_dir_read_name (dir);  // name of the dir of the theme (maybe != of theme's name)
			if (cModuleDirName == NULL)
				break ;
			
			// we create dir of the plug-in of the current theme.
			cd_debug ("  installing %s's config", cModuleDirName);
			cUserDataDirPath = g_strdup_printf ("%s/%s", g_cCurrentPlugInsPath, cModuleDirName);  // dir of the plug-in in the current theme.
			if (! g_file_test (cUserDataDirPath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			{
				cd_debug ("    directory %s doesn't exist, it will be created.", cUserDataDirPath);
				
				g_string_printf (sCommand, "mkdir -p \"%s\"", cUserDataDirPath);
				_launch_cmd (sCommand->str);
			}
			
			// we find the name and path of the .conf file of the plugin in the new theme.
			cConfFileName = g_strdup_printf ("%s.conf", cModuleDirName);
			cNewConfFilePath = g_strdup_printf ("%s/%s/%s", cNewPlugInsDir, cModuleDirName, cConfFileName);
			if (! g_file_test (cNewConfFilePath, G_FILE_TEST_EXISTS))
			{
				g_free (cConfFileName);
				g_free (cNewConfFilePath);
				GldiModule *pModule = gldi_module_foreach ((GHRFunc) _find_module_from_user_data_dir, (gpointer) cModuleDirName);
				if (pModule == NULL)  // in this case, we don't load non used plugins.
				{
					cd_warning ("couldn't find the module owning '%s', this file will be ignored.");
					continue;
				}
				cConfFileName = g_strdup (pModule->pVisitCard->cConfFileName);
				cNewConfFilePath = g_strdup_printf ("%s/%s/%s", cNewPlugInsDir, cModuleDirName, cConfFileName);
			}
			cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, cConfFileName);  // path of the .conf file of the current theme.
			
			// we merge these 2 .conf files.
			if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
			{
				cd_debug ("    no conf file %s, we will take the theme's one", cConfFilePath);
				cairo_dock_copy_file (cNewConfFilePath, cConfFilePath);
			}
			else
			{
				cairo_dock_merge_conf_files (cConfFilePath, cNewConfFilePath, '+');
			}
			g_free (cNewConfFilePath);
			g_free (cConfFilePath);
			g_free (cUserDataDirPath);
			g_free (cConfFileName);
		}
		while (1);
		g_dir_close (dir);
		g_free (cNewPlugInsDir);
	}
	
	g_string_printf (sCommand, "rm -f \"%s/last-modif\"", g_cCurrentThemePath);
	_launch_cmd (sCommand->str);
	
	// precaution maybe useless.
	g_string_printf (sCommand, "chmod -R 775 \"%s\"", g_cCurrentThemePath);
	_launch_cmd (sCommand->str);
	
	cairo_dock_mark_current_theme_as_modified (FALSE);
	
	g_string_free (sCommand, TRUE);
	return TRUE;
}

gboolean cairo_dock_import_theme (const gchar *cThemeName, gboolean bLoadBehavior, gboolean bLoadLaunchers)
{
	//\___________________ Get the local path of the theme (if necessary, it is downloaded and/or unzipped).
	gchar *cNewThemePath = _cairo_dock_get_theme_path (cThemeName);
	g_return_val_if_fail (cNewThemePath != NULL && g_file_test (cNewThemePath, G_FILE_TEST_EXISTS), FALSE);
	
	//\___________________ import the theme in the current theme.
	gboolean bSuccess = _cairo_dock_import_local_theme (cNewThemePath, bLoadBehavior, bLoadLaunchers);
	g_free (cNewThemePath);
	return bSuccess;
}


static void _import_theme (gpointer *pSharedMemory)  // import the theme on the disk; the actual copy of the files is not done here, because we want to be able to cancel the task.
{
	cd_debug ("dl start");
	gchar *cNewThemePath = _cairo_dock_get_theme_path (pSharedMemory[0]);
	g_free (pSharedMemory[0]);
	pSharedMemory[0] = cNewThemePath;
	cd_debug ("dl over");
}
static gboolean _finish_import (gpointer *pSharedMemory)  // once the theme is local, we can import it in the current theme.
{
	gboolean bSuccess;
	if (! pSharedMemory[0])
	{
		cd_warning ("Couldn't download the theme.");
		bSuccess = FALSE;
	}
	else
	{
		bSuccess = _cairo_dock_import_local_theme (pSharedMemory[0], GPOINTER_TO_INT (pSharedMemory[1]), GPOINTER_TO_INT (pSharedMemory[2]));
	}
	
	GFunc pCallback = pSharedMemory[3];
	pCallback (GINT_TO_POINTER (bSuccess), pSharedMemory[4]);
	return FALSE;
}
static void _discard_import (gpointer *pSharedMemory)
{
	g_free (pSharedMemory[0]);
	g_free (pSharedMemory);
}
GldiTask *cairo_dock_import_theme_async (const gchar *cThemeName, gboolean bLoadBehavior, gboolean bLoadLaunchers, GFunc pCallback, gpointer data)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 5);
	pSharedMemory[0] = g_strdup (cThemeName);
	pSharedMemory[1] = GINT_TO_POINTER (bLoadBehavior);
	pSharedMemory[2] = GINT_TO_POINTER (bLoadLaunchers);
	pSharedMemory[3] = pCallback;
	pSharedMemory[4] = data;
	GldiTask *pTask = gldi_task_new_full (0, (GldiGetDataAsyncFunc) _import_theme, (GldiUpdateSyncFunc) _finish_import, (GFreeFunc) _discard_import, pSharedMemory);
	gldi_task_launch (pTask);
	return pTask;
}


#define _check_dir(cDirPath) \
	if (! g_file_test (cDirPath, G_FILE_TEST_IS_DIR)) {\
		if (g_mkdir (cDirPath, 7*8*8+7*8+7) != 0) {\
			cd_warning ("couldn't create directory %s", cDirPath);\
			g_free (cDirPath);\
			cDirPath = NULL; } }

void cairo_dock_set_paths (gchar *cRootDataDirPath, gchar *cExtraDirPath, gchar *cThemesDirPath, gchar *cCurrentThemeDirPath, gchar *cLocalThemeDirPath, gchar *cDistantThemeDirName, gchar *cThemeServerAdress)
{
	//\___________________ On initialise les chemins de l'appli.
	g_cCairoDockDataDir = cRootDataDirPath;  // le repertoire racine contenant tout.
	_check_dir (g_cCairoDockDataDir);
	g_cCurrentThemePath = cCurrentThemeDirPath;  // le chemin vers le repertoire du theme courant.
	_check_dir (g_cCurrentThemePath);
	g_cExtrasDirPath = cExtraDirPath;  // le chemin vers le repertoire des extra.
	_check_dir (g_cExtrasDirPath);
	g_cThemesDirPath = cThemesDirPath;  // le chemin vers le repertoire des themes.
	_check_dir (g_cThemesDirPath);
	s_cLocalThemeDirPath = cLocalThemeDirPath;
	s_cDistantThemeDirName = cDistantThemeDirName;	
	
	g_cCurrentLaunchersPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
	_check_dir (g_cCurrentLaunchersPath);
	g_cCurrentIconsPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
	g_cCurrentImagesPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_IMAGES_DIR);
	_check_dir (g_cCurrentIconsPath);
	g_cCurrentPlugInsPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_PLUG_INS_DIR);
	_check_dir (g_cCurrentPlugInsPath);
	g_cConfFile = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
	
	//\___________________ On initialise l'adresse du serveur de themes.
	cairo_dock_set_packages_server (cThemeServerAdress);
}

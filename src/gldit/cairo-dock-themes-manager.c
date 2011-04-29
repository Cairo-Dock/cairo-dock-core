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
#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-task.h"
#include "cairo-dock-log.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-core.h"
#include "cairo-dock-themes-manager.h"

#define CAIRO_DOCK_MODIFIED_THEME_FILE ".cairo-dock-need-save"

gchar *g_cCairoDockDataDir = NULL;  // le repertoire racine contenant tout.
gchar *g_cCurrentThemePath = NULL;  // le chemin vers le repertoire du theme courant.
gchar *g_cExtrasDirPath = NULL;  // le chemin vers le repertoire des extra.
gchar *g_cThemesDirPath = NULL;  // le chemin vers le repertoire des themes.
gchar *g_cCurrentLaunchersPath = NULL;  // le chemin vers le repertoire des lanceurs du theme courant.
gchar *g_cCurrentIconsPath = NULL;  // le chemin vers le repertoire des icones du theme courant.
gchar *g_cCurrentImagesPath = NULL;  // le chemin vers le repertoire des images ou autre du theme courant.
gchar *g_cCurrentPlugInsPath = NULL;  // le chemin vers le repertoire des plug-ins du theme courant.
gchar *g_cConfFile = NULL;  // le chemin du fichier de conf.

static gchar *s_cLocalThemeDirPath = NULL;
static gchar *s_cDistantThemeDirName = NULL;

extern CairoDock *g_pMainDock;

#define CAIRO_DOCK_LOCAL_EXTRAS_DIR "extras"
#define CAIRO_DOCK_LAUNCHERS_DIR "launchers"
#define CAIRO_DOCK_PLUG_INS_DIR "plug-ins"
#define CAIRO_DOCK_LOCAL_ICONS_DIR "icons"
#define CAIRO_DOCK_LOCAL_IMAGES_DIR "images"

void cairo_dock_mark_current_theme_as_modified (gboolean bModified)
{
	gchar *cModifiedFile = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_MODIFIED_THEME_FILE);

	g_file_set_contents (cModifiedFile,
		(bModified ? "1" : "0"),
		-1,
		NULL);

	g_free (cModifiedFile);
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


gboolean cairo_dock_export_current_theme (const gchar *cNewThemeName, gboolean bSaveBehavior, gboolean bSaveLaunchers)
{
	g_return_val_if_fail (cNewThemeName != NULL, FALSE);
	
	cairo_dock_extract_package_type_from_name (cNewThemeName);
	
	cd_message ("on sauvegarde dans %s", cNewThemeName);
	GString *sCommand = g_string_new ("");
	gboolean bThemeSaved = FALSE;
	int r;
	gchar *cNewThemePath = g_strdup_printf ("%s/%s", g_cThemesDirPath, cNewThemeName);
	if (g_file_test (cNewThemePath, G_FILE_TEST_EXISTS))  // on ecrase un theme existant.
	{
		cd_debug ("  le theme existant sera mis a jour");
		gchar *cQuestion = g_strdup_printf (_("Are you sure you want to overwrite theme %s?"), cNewThemeName);
		int answer = cairo_dock_ask_general_question_and_wait (cQuestion);
		g_free (cQuestion);
		if (answer == GTK_RESPONSE_YES)
		{
			//\___________________ On traite le fichier de conf global et ceux des docks principaux.
			gchar *cNewConfFilePath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_CONF_FILE);
			if (bSaveBehavior)
			{
				cairo_dock_copy_file (g_cConfFile, cNewConfFilePath);
				/**g_string_printf (sCommand, "/bin/cp \"%s\" \"%s\"", g_cConfFile, cNewConfFilePath);
				cd_message ("%s", sCommand->str);
				r = system (sCommand->str);*/
			}
			else
			{
				///cairo_dock_replace_keys_by_identifier (cNewConfFilePath, g_cConfFile, '+');
				cairo_dock_merge_conf_files (cNewConfFilePath, g_cConfFile, '+');
			}
			g_free (cNewConfFilePath);
			
			//\___________________ On traite les lanceurs.
			if (bSaveLaunchers)
			{
				g_string_printf (sCommand, "rm -f \"%s/%s\"/*", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
				cd_message ("%s", sCommand->str);
				r = system (sCommand->str);
				
				g_string_printf (sCommand, "cp \"%s\"/* \"%s/%s\"", g_cCurrentLaunchersPath, cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
				cd_message ("%s", sCommand->str);
				r = system (sCommand->str);
			}
			
			//\___________________ On traite tous le reste.
			/// TODO : traiter les .conf des applets comme celui du dock...
			g_string_printf (sCommand, "find \"%s\" -mindepth 1 -maxdepth 1  ! -name '%s' ! -name \"%s\" -exec /bin/cp -r '{}' \"%s\" \\;", g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE, CAIRO_DOCK_LAUNCHERS_DIR, cNewThemePath);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);

			bThemeSaved = TRUE;
		}
	}
	else  // sinon on sauvegarde le repertoire courant tout simplement.
	{
		cd_debug ("  creation du nouveau theme (%s)", cNewThemePath);

		if (g_mkdir (cNewThemePath, 7*8*8+7*8+5) == 0)
		{
			g_string_printf (sCommand, "cp -r \"%s\"/* \"%s\"", g_cCurrentThemePath, cNewThemePath);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);

			bThemeSaved = TRUE;
		}
		else
			cd_warning ("couldn't create %s", cNewThemePath);
	}
	
	//\___________________ On conserve la date de derniere modif.
	time_t epoch = (time_t) time (NULL);
	struct tm currentTime;
	localtime_r (&epoch, &currentTime);
	char cDateBuffer[50+1];
	strftime (cDateBuffer, 50, "%a %d %b, %R", &currentTime);
	gchar *cMessage = g_strdup_printf ("%s\n %s", _("Last modification on:"), cDateBuffer);
	gchar *cReadmeFile = g_strdup_printf ("%s/%s", cNewThemePath, "readme");
	g_file_set_contents (cReadmeFile,
		cMessage,
		-1,
		NULL);
	g_free (cReadmeFile);
	g_free (cMessage);
	
	g_string_printf (sCommand, "rm -f \"%s/last-modif\"", cNewThemePath);
	r = system (sCommand->str);
	
	/// TODO: draw the main dock into the "preview" file...
	if (g_pMainDock && g_pMainDock->pRenderer)
	{
		cairo_surface_t *pSurface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
			g_pMainDock->iActiveWidth,
			g_pMainDock->iActiveHeight);
		cairo_t *pCairoContext = cairo_create (pSurface);
		
		if (!g_pMainDock->container.bIsHorizontal)
		{
			cairo_translate (pCairoContext, g_pMainDock->iMaxDockWidth/2, g_pMainDock->iMaxDockHeight/2);
			cairo_rotate (pCairoContext, -G_PI/2);
			
			if (!g_pMainDock->container.bDirectionUp)
			{
				
			}
			cairo_translate (pCairoContext, -g_pMainDock->iMaxDockHeight/2, -g_pMainDock->iMaxDockWidth/2);
		}
		g_pMainDock->pRenderer->render (pCairoContext, g_pMainDock);
		
		gchar *cPreviewPath = g_strdup_printf ("%s/preview", cNewThemePath);
		cairo_surface_write_to_png (pSurface, cPreviewPath);
		g_free (cPreviewPath);
		cairo_destroy (pCairoContext);
		cairo_surface_destroy (pSurface);
	}
	
	//\___________________ Le theme n'est plus en etat 'modifie'.
	g_free (cNewThemePath);
	if (bThemeSaved)
	{
		cairo_dock_mark_current_theme_as_modified (FALSE);
	}
	
	g_string_free (sCommand, TRUE);
	
	return bThemeSaved;
}

gboolean cairo_dock_package_current_theme (const gchar *cThemeName)
{
	g_return_val_if_fail (cThemeName != NULL, FALSE);
	
	cairo_dock_extract_package_type_from_name (cThemeName);
	
	cd_message ("building theme package ...");
	int r;
	if (g_file_test (GLDI_SHARE_DATA_DIR"/../../bin/cairo-dock-package-theme", G_FILE_TEST_EXISTS))
	{
		gchar *cCommand;
		const gchar *cTerm = g_getenv ("TERM");
		if (cTerm == NULL || *cTerm == '\0')
			cCommand = g_strdup_printf ("xterm -e %s \"%s\"", "cairo-dock-package-theme", cThemeName);
		else
			cCommand = g_strdup_printf ("$TERM -e '%s \"%s\"'", "cairo-dock-package-theme", cThemeName);
		r = system (cCommand);
		g_free (cCommand);
		return TRUE;
	}
	else
	{
		cd_warning ("the package builder script was not found !");
		return FALSE;
	}
}
gchar *cairo_dock_depackage_theme (const gchar *cPackagePath)
{
	gchar *cNewThemePath = NULL;
	if (*cPackagePath == '/' || strncmp (cPackagePath, "file://", 7) == 0)  // paquet en local.
	{
		cd_debug (" paquet local\n");
		gchar *cFilePath = (*cPackagePath == '/' ? g_strdup (cPackagePath) : g_filename_from_uri (cPackagePath, NULL, NULL));
		cNewThemePath = cairo_dock_uncompress_file (cFilePath, g_cThemesDirPath, NULL);
		g_free (cFilePath);
	}
	else  // paquet distant.
	{
		cd_debug (" paquet distant\n");
		gchar *str = strrchr (cPackagePath, '/');
		if (str != NULL)
		{
			*str = '\0';
			cNewThemePath = cairo_dock_download_file (cPackagePath, "", str+1, g_cThemesDirPath, NULL);
			if (cNewThemePath == NULL)
			{
				cairo_dock_show_temporary_dialog_with_icon_printf (_("Could not access remote file %s/%s. Maybe the server is down.\nPlease retry later or contact us at glx-dock.org."), NULL, NULL, 0, NULL, cPackagePath, str+1);
			}
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
	int answer = cairo_dock_ask_general_question_and_wait (sCommand->str);
	if (answer == GTK_RESPONSE_YES)
	{
		gchar *cThemeName;
		int i, r;
		for (i = 0; cThemesList[i] != NULL; i ++)
		{
			cThemeName = cThemesList[i];
			if (*cThemeName == '\0')
				continue;
			cairo_dock_extract_package_type_from_name (cThemeName);
			
			bThemeDeleted = TRUE;
			g_string_printf (sCommand, "rm -rf \"%s/%s\"", g_cThemesDirPath, cThemeName);
			r = system (sCommand->str);  // g_rmdir n'efface qu'un repertoire vide.
		}
	}
	
	g_string_free (sCommand, TRUE);
	return bThemeDeleted;
}

static gboolean _find_module_from_user_data_dir (gchar *cModuleName, CairoDockModule *pModule, const gchar *cUserDataDirName)
{
	if (pModule->pVisitCard->cUserDataDir && strcmp (cUserDataDirName, pModule->pVisitCard->cUserDataDir) == 0)
		return TRUE;
	return FALSE;
}
gboolean cairo_dock_import_theme (const gchar *cThemeName, gboolean bLoadBehavior, gboolean bLoadLaunchers)
{
	//\___________________ On obtient le chemin du nouveau theme (telecharge ou decompresse si necessaire).
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
		cd_debug ("c'est un paquet");
		cNewThemePath = cairo_dock_depackage_theme (cNewThemeName);
		
		g_return_val_if_fail (cNewThemePath != NULL, FALSE);
		gchar *tmp = cNewThemeName;
		cNewThemeName = g_path_get_basename (cNewThemePath);
		g_free (tmp);
	}
	else  // c'est un theme officiel.
	{
		cd_debug ("c'est un theme officiel");
		cNewThemePath = cairo_dock_get_package_path (cNewThemeName, s_cLocalThemeDirPath, g_cThemesDirPath, s_cDistantThemeDirName, CAIRO_DOCK_ANY_PACKAGE);
	}
	g_return_val_if_fail (cNewThemePath != NULL && g_file_test (cNewThemePath, G_FILE_TEST_EXISTS), FALSE);
	//g_print ("cNewThemePath : %s ; cNewThemeName : %s\n", cNewThemePath, cNewThemeName);
	
	//\___________________ On charge les parametres de comportement globaux et de chaque dock.
	GString *sCommand = g_string_new ("");
	int r;
	/**cd_message ("Applying changes ...");
	if (g_pMainDock == NULL || bLoadBehavior)
	{
		g_string_printf (sCommand, "/bin/cp \"%s\"/%s \"%s\"", cNewThemePath, CAIRO_DOCK_CONF_FILE, g_cCurrentThemePath);
		cd_message ("%s", sCommand->str);
		r = system (sCommand->str);
	}
	else
	{
		gchar *cNewConfFilePath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_CONF_FILE);
		///cairo_dock_replace_keys_by_identifier (g_cConfFile, cNewConfFilePath, '+');
		cairo_dock_merge_conf_files (g_cConfFile, cNewConfFilePath, '+');
		g_free (cNewConfFilePath);
	}
	
	g_string_printf (sCommand, "find \"%s\" -mindepth 1 -maxdepth 1 -name '*.conf' ! -name '%s' -exec /bin/cp '{}' \"%s\" \\;", cNewThemePath, CAIRO_DOCK_CONF_FILE, g_cCurrentThemePath);
	cd_debug ("%s", sCommand->str);
	r = system (sCommand->str);*/
	
	cd_message ("Applying changes ...");
	if (g_pMainDock == NULL || bLoadBehavior)
	{
		g_string_printf (sCommand, "cp \"%s\"/*.conf \"%s\"", cNewThemePath, g_cCurrentThemePath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
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
	
	//\___________________ On charge les icones.
	if (bLoadLaunchers)
	{
		g_string_printf (sCommand, "rm -f \"%s\"/*", g_cCurrentIconsPath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
		g_string_printf (sCommand, "rm -f \"%s\"/.*", g_cCurrentIconsPath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
		
		g_string_printf (sCommand, "rm -f \"%s\"/*", g_cCurrentImagesPath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
		g_string_printf (sCommand, "rm -f \"%s\"/.*", g_cCurrentImagesPath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
	}
	gchar *cNewLocalIconsPath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
	if (! g_file_test (cNewLocalIconsPath, G_FILE_TEST_IS_DIR))  // c'est un ancien theme, on deplace les icones vers le repertoire 'icons'.
	{
		g_string_printf (sCommand, "find \"%s/%s\" -mindepth 1 ! -name '*.desktop' -exec /bin/cp '{}' '%s' \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentIconsPath);
	}
	else
	{
		g_string_printf (sCommand, "for f in \"%s\"/* ; do rm -f \"%s/`basename \"${f%%.*}\"`\"*; done;", cNewLocalIconsPath, g_cCurrentIconsPath);  // on efface les doublons car sinon on pourrait avoir x.png et x.svg ensemble et le dock ne saurait pas lequel choisir.
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
		
		g_string_printf (sCommand, "cp \"%s\"/* \"%s\"", cNewLocalIconsPath, g_cCurrentIconsPath);
	}
	cd_debug ("%s", sCommand->str);
	r = system (sCommand->str);
	g_free (cNewLocalIconsPath);
	
	//\___________________ On charge les extras.
	g_string_printf (sCommand, "%s/%s", cNewThemePath, CAIRO_DOCK_LOCAL_EXTRAS_DIR);
	if (g_file_test (sCommand->str, G_FILE_TEST_IS_DIR))
	{
		g_string_printf (sCommand, "cp -r \"%s/%s\"/* \"%s\"", cNewThemePath, CAIRO_DOCK_LOCAL_EXTRAS_DIR, g_cExtrasDirPath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
	}
	
	//\___________________ On charge les lanceurs si necessaire, en effacant ceux existants.
	if (! g_file_test (g_cCurrentLaunchersPath, G_FILE_TEST_EXISTS))
	{
		gchar *command = g_strdup_printf ("mkdir -p \"%s\"", g_cCurrentLaunchersPath);
		r = system (command);
		g_free (command);
	}
	if (g_pMainDock == NULL || bLoadLaunchers)
	{
		g_string_printf (sCommand, "rm -f \"%s\"/*.desktop", g_cCurrentLaunchersPath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);

		g_string_printf (sCommand, "cp \"%s/%s\"/*.desktop \"%s\"", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentLaunchersPath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
	}
	
	//\___________________ On remplace tous les autres fichiers par les nouveaux.
	g_string_printf (sCommand, "find \"%s\" -mindepth 1 -maxdepth 1  ! -name '*.conf' -type f -exec rm -f '{}' \\;", g_cCurrentThemePath);  // efface tous les fichiers du theme mais sans toucher aux lanceurs et aux plug-ins.
	cd_debug ("%s", sCommand->str);
	r = system (sCommand->str);

	if (g_pMainDock == NULL || bLoadBehavior)
	{
		g_string_printf (sCommand, "find \"%s\"/* -prune ! -name '*.conf' ! -name %s -exec /bin/cp -r '{}' \"%s\" \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);  // copie tous les fichiers du nouveau theme sauf les lanceurs et le .conf, dans le repertoire du theme courant. Ecrase les fichiers de memes noms.
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
	}
	else
	{
		// on copie tous les fichiers du nouveau theme sauf les lanceurs et les .conf (dock et plug-ins).
		g_string_printf (sCommand, "find \"%s\" -mindepth 1 ! -name '*.conf' ! -path '%s/%s*' ! -type d -exec cp -p {} \"%s\" \\;", cNewThemePath, cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
		
		// on parcours les .conf des plug-ins, on les met a jour, et on fusionne avec le theme courant.
		gchar *cNewPlugInsDir = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_PLUG_INS_DIR);  // repertoire des plug-ins du nouveau theme.
		GDir *dir = g_dir_open (cNewPlugInsDir, 0, NULL);  // NULL si ce theme n'a pas de repertoire 'plug-ins'.
		const gchar* cModuleDirName;
		gchar *cConfFilePath, *cNewConfFilePath, *cUserDataDirPath, *cConfFileName;
		do
		{
			cModuleDirName = g_dir_read_name (dir);  // nom du repertoire du theme (pas forcement egal au nom du theme)
			if (cModuleDirName == NULL)
				break ;
			
			// on cree le repertoire du plug-in dans le theme courant.
			cd_debug ("  installing %s's config", cModuleDirName);
			cUserDataDirPath = g_strdup_printf ("%s/%s", g_cCurrentPlugInsPath, cModuleDirName);  // repertoire du plug-in dans le theme courant.
			if (! g_file_test (cUserDataDirPath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			{
				cd_debug ("    directory %s doesn't exist, it will be created.", cUserDataDirPath);
				
				g_string_printf (sCommand, "mkdir -p \"%s\"", cUserDataDirPath);
				r = system (sCommand->str);
			}
			
			// on recupere le nom et le chemin du .conf du plug-in dans le nouveau theme.
			cConfFileName = g_strdup_printf ("%s.conf", cModuleDirName);
			cNewConfFilePath = g_strdup_printf ("%s/%s/%s", cNewPlugInsDir, cModuleDirName, cConfFileName);
			if (! g_file_test (cNewConfFilePath, G_FILE_TEST_EXISTS))
			{
				g_free (cConfFileName);
				g_free (cNewConfFilePath);
				CairoDockModule *pModule = cairo_dock_foreach_module ((GHRFunc) _find_module_from_user_data_dir, (gpointer) cModuleDirName);
				if (pModule == NULL)  // du coup, dans ce cas-la, on ne charge pas des plug-ins non utilises par l'utilisateur.
				{
					cd_warning ("couldn't find the module owning '%s', this file will be ignored.");
					continue;
				}
				cConfFileName = g_strdup (pModule->pVisitCard->cConfFileName);
				cNewConfFilePath = g_strdup_printf ("%s/%s/%s", cNewPlugInsDir, cModuleDirName, cConfFileName);
			}
			cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, cConfFileName);  // chemin du .conf dans le theme courant.
			
			// on fusionne les 2 .conf.
			if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
			{
				cd_debug ("    no conf file %s, we will take the theme's one", cConfFilePath);
				cairo_dock_copy_file (cNewConfFilePath, cConfFilePath);
				/**g_string_printf (sCommand, "cp \"%s\" \"%s\"", cNewConfFilePath, cConfFilePath);
				r = system (sCommand->str);*/
			}
			else
			{
				///cairo_dock_replace_keys_by_identifier (cConfFilePath, cNewConfFilePath, '+');
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
	r = system (sCommand->str);
	
	// precaution probablement inutile.
	g_string_printf (sCommand, "chmod -R 777 \"%s\"", g_cCurrentThemePath);
	r = system (sCommand->str);
	
	g_string_free (sCommand, TRUE);
	return TRUE;
}


static void _import_theme (gpointer *pSharedMemory)
{
	cd_debug ("dl start");
	
	gchar *cNewThemeName = g_strdup (pSharedMemory[0]);
	gchar *cNewThemePath = NULL;
	
	int length = strlen (cNewThemeName);
	if (cNewThemeName[length-1] == '\n')
		cNewThemeName[--length] = '\0';  // on vire le retour chariot final.
	if (cNewThemeName[length-1] == '\r')
		cNewThemeName[--length] = '\0';
	cd_debug ("cNewThemeName : '%s'", cNewThemeName);
	
	// on recupere le theme en local, mais sans faire l'import dans le theme courant.
	if (g_str_has_suffix (cNewThemeName, ".tar.gz") || g_str_has_suffix (cNewThemeName, ".tar.bz2") || g_str_has_suffix (cNewThemeName, ".tgz"))  // c'est l'URL d'un paquet.
	{
		cd_debug ("c'est un paquet");
		cNewThemePath = cairo_dock_depackage_theme (cNewThemeName);
		
		if (cNewThemePath != NULL)
		{
			gchar *tmp = cNewThemeName;
			cNewThemeName = g_path_get_basename (cNewThemePath);
			g_free (tmp);
		}
		else
		{
			g_free (cNewThemeName);
			cNewThemeName = NULL;
		}
		g_free (pSharedMemory[0]);
		pSharedMemory[0] = cNewThemeName;
	}
	else  // c'est le nom d'un theme officiel.
	{
		cd_debug ("c'est un theme officiel");
		cNewThemePath = cairo_dock_get_package_path (cNewThemeName, s_cLocalThemeDirPath, g_cThemesDirPath, s_cDistantThemeDirName, CAIRO_DOCK_ANY_PACKAGE);
	}
	
	cd_debug ("dl over");
}
static gboolean _finish_import (gpointer *pSharedMemory)
{
	gboolean bSuccess;
	if (! pSharedMemory[0])
	{
		cd_warning ("Couldn't download the theme.");
		bSuccess = FALSE;
	}
	else
	{
		// maintenant que le theme est sur le disque, on l'importe dans le theme courant.
		bSuccess = cairo_dock_import_theme (pSharedMemory[0], GPOINTER_TO_INT (pSharedMemory[1]), GPOINTER_TO_INT (pSharedMemory[2]));
		if (! bSuccess)
			cd_warning ("Couldn't import the theme %s.", pSharedMemory[0]);
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
CairoDockTask *cairo_dock_import_theme_async (const gchar *cThemeName, gboolean bLoadBehavior, gboolean bLoadLaunchers, GFunc pCallback, gpointer data)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 5);
	pSharedMemory[0] = g_strdup (cThemeName);
	pSharedMemory[1] = GINT_TO_POINTER (bLoadBehavior);
	pSharedMemory[2] = GINT_TO_POINTER (bLoadLaunchers);
	pSharedMemory[3] = pCallback;
	pSharedMemory[4] = data;
	CairoDockTask *pTask = cairo_dock_new_task_full (0, (CairoDockGetDataAsyncFunc) _import_theme, (CairoDockUpdateSyncFunc) _finish_import, (GFreeFunc) _discard_import, pSharedMemory);
	cairo_dock_launch_task (pTask);
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
	
	//\___________________ On initialise le gestionnaire de paquets.
	cairo_dock_init_package_manager (cThemeServerAdress);
}

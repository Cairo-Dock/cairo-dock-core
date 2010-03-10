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

#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gauge.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-task.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-themes-manager.h"

#define CAIRO_DOCK_THEME_PANEL_WIDTH 1000
#define CAIRO_DOCK_THEME_PANEL_HEIGHT 550

static GtkWidget *s_pThemeManager = NULL;
static CairoDockTask *s_pImportTask = NULL;


static gchar *cairo_dock_build_temporary_themes_conf_file (void)
{
	//\___________________ On cree un fichier de conf temporaire.
	const gchar *cTmpDir = g_get_tmp_dir ();
	gchar *cTmpConfFile = g_strdup_printf ("%s/cairo-dock-init.XXXXXX", cTmpDir);
	int fds = mkstemp (cTmpConfFile);
	if (fds == -1)
	{
		cd_warning ("can't create a temporary file in %s", cTmpDir);
		g_free (cTmpConfFile);
		return NULL;
	}
	
	//\___________________ On copie le fichier de conf par defaut dedans.
	gchar *cCommand = g_strdup_printf ("cp \"%s\" \"%s\"", CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_THEME_CONF_FILE, cTmpConfFile);
	int r = system (cCommand);
	g_free (cCommand);

	close(fds);
	return cTmpConfFile;
}

static void _load_theme (gboolean bSuccess, gpointer data)
{
	if (s_pThemeManager == NULL)  // si l'utilisateur a ferme la fenetre entre-temps, on considere qu'il a abandonne.
		return ;
	if (bSuccess)
	{
		cairo_dock_load_current_theme ();
		
		cairo_dock_set_status_message (s_pThemeManager, "");
		cairo_dock_reload_generic_gui (s_pThemeManager);
	}
	else
		cairo_dock_set_status_message (s_pThemeManager, _("Could not import the theme."));
}

static void on_theme_destroy (gchar *cInitConfFile)
{
	cd_debug ("");
	g_remove (cInitConfFile);
	g_free (cInitConfFile);
	s_pThemeManager = NULL;
}

static gboolean on_theme_apply (gchar *cInitConfFile)
{
	cd_debug ("%s (%s)", __func__, cInitConfFile);
	GError *erreur = NULL;
	int r;  // resultat de system().
	
	//\___________________ On recupere les donnees de l'IHM apres modification par l'utilisateur.
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cInitConfFile);
	g_return_val_if_fail (pKeyFile != NULL, FALSE);
	
	//\___________________ On sauvegarde le theme actuel.
	GString *sCommand = g_string_new ("");
	gchar *cNewThemeName = g_key_file_get_string (pKeyFile, "Save", "theme name", &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	if (cNewThemeName != NULL && *cNewThemeName == '\0')
	{
		g_free (cNewThemeName);
		cNewThemeName = NULL;
	}
	cd_message ("cNewThemeName : %s", cNewThemeName);

	if (cNewThemeName != NULL)
	{
		cairo_dock_extract_theme_type_from_name (cNewThemeName);
		
		gboolean bSaveBehavior = g_key_file_get_boolean (pKeyFile, "Save", "save current behaviour", NULL);
		gboolean bSaveLaunchers = g_key_file_get_boolean (pKeyFile, "Save", "save current launchers", NULL);
		
		gboolean bThemeSaved = cairo_dock_export_current_theme (cNewThemeName, bSaveBehavior, bSaveLaunchers);
		
		if (g_key_file_get_boolean (pKeyFile, "Save", "package", NULL))
		{
			bThemeSaved |= cairo_dock_package_current_theme (cNewThemeName);
		}
		
		if (bThemeSaved)
		{
			g_key_file_set_string (pKeyFile, "Save", "theme name", "");
			cairo_dock_write_keys_to_file (pKeyFile, cInitConfFile);
		}
		cairo_dock_set_status_message (s_pThemeManager, bThemeSaved ? _("The theme has been saved") :  _("The theme could not be saved"));
		
		g_free (cNewThemeName);
		g_key_file_free (pKeyFile);
		return FALSE;
	}
	
	//\___________________ On efface les themes selectionnes.
	gsize length = 0;
	gchar ** cThemesList = g_key_file_get_string_list (pKeyFile, "Delete", "deleted themes", &length, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	else if (cThemesList != NULL && cThemesList[0] != NULL && *cThemesList[0] != '\0')
	{
		gboolean bThemeDeleted = cairo_dock_delete_themes (cThemesList);
		
		if (bThemeDeleted)
		{
			g_key_file_set_string (pKeyFile, "Delete", "deleted themes", "");
			cairo_dock_write_keys_to_file (pKeyFile, cInitConfFile);
		}
		if (cThemesList[1] == NULL)
			cairo_dock_set_status_message (s_pThemeManager, bThemeDeleted ? _("The theme has been deleted") :  _("The theme could not be deleted"));
		else
			cairo_dock_set_status_message (s_pThemeManager, bThemeDeleted ? _("The themes have been deleted") :  _("The themes could not be deleted"));
		
		g_strfreev (cThemesList);
		g_key_file_free (pKeyFile);
		return FALSE;
	}
	
	//\___________________ On charge le theme selectionne.
	cNewThemeName = g_key_file_get_string (pKeyFile, "Themes", "chosen theme", &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	if (cNewThemeName != NULL && *cNewThemeName == '\0')
	{
		g_free (cNewThemeName);
		cNewThemeName = NULL;
	}
	
	if (cNewThemeName == NULL)
	{
		cNewThemeName = g_key_file_get_string (pKeyFile, "Themes", "package", &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		if (cNewThemeName != NULL && *cNewThemeName == '\0')
		{
			g_free (cNewThemeName);
			cNewThemeName = NULL;
		}
	}
	
	//\___________________ On charge le nouveau theme choisi.
	if (cNewThemeName != NULL)
	{
		gboolean bLoadBehavior = g_key_file_get_boolean (pKeyFile, "Themes", "use theme behaviour", NULL);
		gboolean bLoadLaunchers = g_key_file_get_boolean (pKeyFile, "Themes", "use theme launchers", NULL);
		
		if (s_pImportTask != NULL)
			cairo_dock_discard_task (s_pImportTask);
		
		cairo_dock_set_status_message_printf (s_pThemeManager, _("Importing theme %s ..."), cNewThemeName);
		s_pImportTask = cairo_dock_import_theme_async (cNewThemeName, bLoadBehavior, bLoadLaunchers, (GFunc)_load_theme, NULL);
		g_free (cNewThemeName);
		return TRUE;  // ne recharge pas la fenetre, on le fera nous-memes apres l'import.
	}
	
	return TRUE;
}

void cairo_dock_manage_themes (void)
{
	if (s_pThemeManager != NULL)
	{
		gtk_window_present (GTK_WINDOW (s_pThemeManager));
		return ;
	}
	
	gchar *cInitConfFile = cairo_dock_build_temporary_themes_conf_file ();  // sera supprime a la destruction de la fenetre.
	
	//\___________________ On laisse l'utilisateur l'editer.
	const gchar *cPresentedGroup = (cairo_dock_theme_need_save () ? "Save" : NULL);
	const gchar *cTitle = _("Manage Themes");
	
	cairo_dock_build_generic_gui (cInitConfFile,
		NULL, cTitle,
		CAIRO_DOCK_THEME_PANEL_WIDTH, CAIRO_DOCK_THEME_PANEL_HEIGHT,
		(CairoDockApplyConfigFunc) on_theme_apply,
		cInitConfFile,
		(GFreeFunc) on_theme_destroy,
		&s_pThemeManager);
}

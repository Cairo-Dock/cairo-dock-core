/***********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

************************************************************************************/
#include <string.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "cairo-dock-applet-factory.h"
#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gauge.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-themes-manager.h"

#define CAIRO_DOCK_MODIFIED_THEME_FILE ".cairo-dock-need-save"
#define CAIRO_DOCK_THEME_PANEL_WIDTH 750
#define CAIRO_DOCK_THEME_PANEL_HEIGHT 400
#define CAIRO_DOCK_THEME_SERVER "http://themes.cairo-dock.org"
#define CAIRO_DOCK_BACKUP_THEME_SERVER "http://fabounet03.free.fr/themes"

extern gchar *g_cCairoDockDataDir;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cMainDockDefaultRendererName;
extern gchar *g_cThemeServerAdress;

extern CairoDock *g_pMainDock;
extern int g_iWmHint;


GHashTable *cairo_dock_list_themes (gchar *cThemesDir, GHashTable *hProvidedTable, GError **erreur)
{
	GError *tmp_erreur = NULL;
	GDir *dir = g_dir_open (cThemesDir, 0, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return hProvidedTable;
	}

	GHashTable *pThemeTable = (hProvidedTable != NULL ? hProvidedTable : g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free));

	const gchar* cThemeName;
	gchar *cThemePath;
	do
	{
		cThemeName = g_dir_read_name (dir);
		if (cThemeName == NULL)
			break ;

		cThemePath = g_strdup_printf ("%s/%s", cThemesDir, cThemeName);

		if (g_file_test (cThemePath, G_FILE_TEST_IS_DIR))
			g_hash_table_insert (pThemeTable, g_strdup (cThemeName), cThemePath);
	}
	while (1);
	g_dir_close (dir);
	return pThemeTable;
}

GHashTable *cairo_dock_list_net_themes (gchar *cServerAdress, GHashTable *hProvidedTable, GError **erreur)
{
	g_return_val_if_fail (cServerAdress != NULL && *cServerAdress != '\0', hProvidedTable);
	cd_message ("listing net themes on %s ...", cServerAdress);
	
	gchar *cTmpFilePath = g_strdup ("/tmp/cairo-dock-net-themes.XXXXXX");
	int fds = mkstemp (cTmpFilePath);
	if (fds == -1)
	{
		g_free (cTmpFilePath);
		return hProvidedTable;
	}
	
	gchar *cCommand = g_strdup_printf ("wget \"%s/liste.txt\" -O '%s' -t 2 -T 2", cServerAdress, cTmpFilePath);
	system (cCommand);
	g_free (cCommand);
	close(fds);
	
	gchar *cContent = NULL;
	gsize length = 0;
	g_file_get_contents (cTmpFilePath, &cContent, &length, NULL);
	
	if (cContent == NULL)
	{
		g_set_error (erreur, 1, 1, "couldn't retrieve themes on %s (check that your connection is alive, or retry later)", cServerAdress);
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		return hProvidedTable;
	}
	
	GHashTable *pThemeTable = (hProvidedTable != NULL ? hProvidedTable : g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free));
	
	gchar **cNetThemesList = g_strsplit (cContent, "\n", -1);
	g_free (cContent);
	
	int i;
	gchar *cThemeName, *cThemePath;
	for (i = 0; cNetThemesList[i] != NULL; i ++)
	{
		cThemeName = cNetThemesList[i];
		if (*cThemeName == '#' || *cThemeName == '\0')
		{
			g_print ("+ commentaire : %s\n", cThemeName);
			g_free (cThemeName);
		}
		else
		{
			g_print ("+ theme : %s\n", cThemeName);
			cThemePath = g_strdup_printf ("%s/%s", cServerAdress, cThemeName);
			g_hash_table_insert (pThemeTable, cThemeName, cThemePath);
		}
	}
	
	g_free (cNetThemesList);
	g_remove (cTmpFilePath);
	g_free (cTmpFilePath);
	
	return pThemeTable;
}


gchar *cairo_dock_build_themes_conf_file (GHashTable **hThemeTable)
{
	//\___________________ On recupere la liste des themes existant (pre-installes et utilisateur).
	GError *erreur = NULL;
	gchar *cThemesDir = CAIRO_DOCK_SHARE_THEMES_DIR;
	*hThemeTable = cairo_dock_list_themes (cThemesDir, NULL, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	g_hash_table_insert (*hThemeTable, g_strdup (""), g_strdup (""));
	
	*hThemeTable = cairo_dock_list_net_themes (g_cThemeServerAdress != NULL ? g_cThemeServerAdress : CAIRO_DOCK_THEME_SERVER, *hThemeTable, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		*hThemeTable = cairo_dock_list_net_themes (g_cThemeServerAdress != NULL ? g_cThemeServerAdress : CAIRO_DOCK_BACKUP_THEME_SERVER, *hThemeTable, &erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
	}
	
	cThemesDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR);  // les themes utilisateurs ecraseront les autres themes.
	*hThemeTable = cairo_dock_list_themes (cThemesDir, *hThemeTable, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	
	GHashTable *hUserThemeTable = cairo_dock_list_themes (cThemesDir, NULL, NULL);
	g_free (cThemesDir);

	gchar *cUserThemeNames = cairo_dock_write_table_content (hUserThemeTable, (GHFunc) cairo_dock_write_one_name, TRUE, FALSE);

	//\___________________ On cree un fichier de conf temporaire.
	const gchar *cTmpDir = g_get_tmp_dir ();
	gchar *cTmpConfFile = g_strdup_printf ("%s/cairo-dock-init", cTmpDir);

	gchar *cCommand = g_strdup_printf ("cp %s/%s %s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_THEME_CONF_FILE, cTmpConfFile);
	system (cCommand);
	g_free (cCommand);

	//\___________________ On met a jour ce fichier de conf.
	GKeyFile *pKeyFile = g_key_file_new ();
	g_key_file_load_from_file (pKeyFile, cTmpConfFile, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return NULL;
	}
	
	cairo_dock_update_conf_file_with_themes (pKeyFile, cTmpConfFile, *hThemeTable, "Themes", "chosen theme");
	cairo_dock_update_conf_file_with_hash_table (pKeyFile, cTmpConfFile, hUserThemeTable, "Delete", "wanted themes", NULL, (GHFunc) cairo_dock_write_one_name, TRUE, FALSE);
	cairo_dock_update_conf_file_with_hash_table (pKeyFile, cTmpConfFile, hUserThemeTable, "Save", "theme name", NULL, (GHFunc) cairo_dock_write_one_name, TRUE, FALSE);
	g_hash_table_destroy (hUserThemeTable);
	
	g_key_file_set_string (pKeyFile, "Delete", "wanted themes", cUserThemeNames);  // sThemeNames
	g_free (cUserThemeNames);

	cairo_dock_write_keys_to_file (pKeyFile, cTmpConfFile);
	g_key_file_free (pKeyFile);
	return cTmpConfFile;
}

gchar *cairo_dock_get_chosen_theme (gchar *cConfFile, gboolean *bUseThemeBehaviour, gboolean *bUseThemeLaunchers)
{
	GError *erreur = NULL;
	GKeyFile *pKeyFile = g_key_file_new ();

	g_key_file_load_from_file (pKeyFile, cConfFile, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return NULL;
	}

	gchar *cThemeName = g_key_file_get_string (pKeyFile, "Themes", "chosen theme", &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return NULL;
	}
	if (cThemeName != NULL && *cThemeName == '\0')
	{
		g_free (cThemeName);
		cThemeName = NULL;
	}

	*bUseThemeBehaviour = g_key_file_get_boolean (pKeyFile, "Themes", "use theme behaviour", &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		g_free (cThemeName);
		return NULL;
	}

	*bUseThemeLaunchers = g_key_file_get_boolean (pKeyFile, "Themes", "use theme launchers", &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		g_free (cThemeName);
		return NULL;
	}

	g_key_file_free (pKeyFile);
	return cThemeName;
}


void cairo_dock_load_theme (gchar *cThemePath)
{
	//g_print ("%s (%s)\n", __func__, cThemePath);
	g_return_if_fail (cThemePath != NULL && g_file_test (cThemePath, G_FILE_TEST_IS_DIR));

	//\___________________ On libere toute la memoire allouee pour les docks (stoppe aussi tous les timeout).
	cairo_dock_free_all_docks ();
	cairo_dock_invalidate_gauges_list ();

	//\___________________ On cree le dock principal.
	g_pMainDock = cairo_dock_create_new_dock (g_iWmHint, CAIRO_DOCK_MAIN_DOCK_NAME, NULL);  // on ne lui assigne pas de vues, puisque la vue par defaut des docks principaux sera definie plus tard.
	g_pMainDock->bIsMainDock = TRUE;

	//\___________________ On lit son fichier de conf et on charge tout.
	cairo_dock_read_conf_file (g_cConfFile, g_pMainDock);  // chargera des valeurs par defaut si le fichier de conf fourni est incorrect.
	cairo_dock_mark_theme_as_modified (FALSE);  // le chargement du fichier de conf le marque a 'TRUE'.
}


void cairo_dock_mark_theme_as_modified (gboolean bModified)
{
	gchar *cModifiedFile = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_MODIFIED_THEME_FILE);

	g_file_set_contents (cModifiedFile,
		(bModified ? "1" : "0"),
		-1,
		NULL);

	g_free (cModifiedFile);
}
gboolean cairo_dock_theme_need_save (void)
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


static void _cairo_dock_delete_one_theme (gchar *cThemeName, gchar *cThemePath, gpointer *data)
{
	gchar **cThemesList = data[0];
	GtkWidget *pWidget = data[1];
	gchar *cWantedThemeName;
	int i = 0;
	while (cThemesList[i] != NULL)
	{
		cWantedThemeName = cThemesList[i];
		if (strcmp (cWantedThemeName, cThemeName) == 0)
			break;
		i ++;
	}

	if (cThemesList[i] == NULL)  // le theme ne se trouve pas dans la liste des themes desires.
	{
		gchar *question = g_strdup_printf (_("Are you sure you want to delete theme %s ?"), cThemeName);
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (pWidget),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_YES_NO,
			question);
		g_free (question);
		int answer = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		if (answer == GTK_RESPONSE_YES)
		{
			gchar *cCommand = g_strdup_printf ("rm -rf '%s'", cThemePath);
			system (cCommand);  // g_rmdir n'efface qu'un repertoire vide.
			g_free (cCommand);
		}
	}
}
static void on_theme_destroy (gpointer *user_data)
{
	g_print ("%s ()\n", __func__);
	g_remove (user_data[0]);
	g_free (user_data[0]);
	g_hash_table_destroy (user_data[1]);
	cairo_dock_dialog_unreference (user_data[2]);
	g_free (user_data);
}
static void on_theme_apply (gpointer *user_data)
{
	gchar *cInitConfFile = user_data[0];
	GHashTable *hThemeTable = user_data[1];
	GtkWidget *pWidget = user_data[2];
	g_print ("%s (%s)\n", __func__, cInitConfFile);
		GError *erreur = NULL;
		gboolean bNeedSave = cairo_dock_theme_need_save ();

		//\___________________ On recupere les donnees de l'IHM apres modification par l'utilisateur.
		GKeyFile *pKeyFile = g_key_file_new ();

		g_key_file_load_from_file (pKeyFile, cInitConfFile, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
		//g_remove (cInitConfFile);
		//g_free (cInitConfFile);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			return ;
		}

		//\___________________ On charge le nouveau theme choisi.
		gchar *cNewThemeName = g_key_file_get_string (pKeyFile, "Themes", "chosen theme", &erreur);
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

		GString *sCommand = g_string_new ("");
		if (cNewThemeName != NULL)
		{
			if (bNeedSave)
			{
				int iAnswer = cairo_dock_ask_general_question_and_wait (_("You made some modifications in the current theme.\nYou will loose them if you don't save before choosing a new theme. Continue anyway ?"));
				if (iAnswer != GTK_RESPONSE_YES)
				{
					//g_hash_table_destroy (hThemeTable);
					return ;
				}
				else
					cairo_dock_mark_theme_as_modified (FALSE);
			}

			gchar *cNewThemePath = g_hash_table_lookup (hThemeTable, cNewThemeName);
			gboolean bDistantTheme = FALSE;
			gchar *cRepToDelete = NULL;
			
			//\___________________ On telecharge le theme s'il est distant.
			if (strncmp (cNewThemePath, "http://", 7) == 0 || strncmp (cNewThemePath, "ftp://", 6) == 0)
			{
				gchar *cTmpFilePath = g_strdup ("/tmp/cairo-dock-net-theme.XXXXXX");
				int fds = mkstemp (cTmpFilePath);
				if (fds == -1)
				{
					g_free (cNewThemeName);
					//g_hash_table_destroy (hThemeTable);
					return ;
				}
				
				g_print ("downloading theme from %s ...\n", cNewThemePath);
				g_string_printf (sCommand, "wget \"%s/%s.tar.gz\" -O '%s' -t 2 -T 2", cNewThemePath, cNewThemeName, cTmpFilePath);
				system (sCommand->str);
				close(fds);
				
				g_print ("uncompressing theme %s ...\n", cNewThemeName);
				
				cNewThemePath = g_strdup_printf ("%s-data", cTmpFilePath);
				bDistantTheme = TRUE;
				
				if (! g_file_test (cNewThemePath, G_FILE_TEST_EXISTS))
				{
					if (g_mkdir (cNewThemePath, 7*8*8+7*8+5) != 0)
					{
						cd_warning ("couldn't create directory %s", cNewThemePath);
						g_remove (cTmpFilePath);
						g_free (cTmpFilePath);
						//g_hash_table_destroy (hThemeTable);
						g_free (cNewThemePath);
						return ;
					}
				}
				
				g_string_printf (sCommand, "tar xfz \"%s\" -C \"%s\"", cTmpFilePath, cNewThemePath);
				system (sCommand->str);
				
				//\___________________ On verifie ou les fichiers se sont decompresses.
				GDir *dir = g_dir_open (cNewThemePath, 0, NULL);
				const gchar* cFileName;
				gchar *cPrevFileName = NULL;
				int iNbFiles = 0;
				do
				{
					cFileName = g_dir_read_name (dir);
					if (cFileName == NULL)
						break ;
					cPrevFileName = g_strdup (cFileName);
					iNbFiles ++;
				} while (iNbFiles < 2);
				g_dir_close (dir);
				
				if (iNbFiles == 0)
				{
					cd_warning ("directory %s is empty or unreadable", cNewThemePath);
					g_remove (cTmpFilePath);
					g_free (cTmpFilePath);
					//g_hash_table_destroy (hThemeTable);
					g_free (cNewThemePath);
					return ;
				}
				else if (iNbFiles == 1)
				{
					cRepToDelete = cNewThemePath;
					cNewThemePath = g_strdup_printf ("%s-data/%s", cTmpFilePath, cPrevFileName);
				}
				g_free (cPrevFileName);
				
				g_remove (cTmpFilePath);
				g_free (cTmpFilePath);
				
				g_print ("le theme se trouve temporairement dans %s\n", cNewThemePath);
			}
			
			//\___________________ On charge les parametres de comportement.
			if (g_pMainDock == NULL || g_key_file_get_boolean (pKeyFile, "Themes", "use theme behaviour", NULL))
			{
				g_string_printf (sCommand, "/bin/cp '%s'/%s '%s'", cNewThemePath, CAIRO_DOCK_CONF_FILE, g_cCurrentThemePath);
				cd_message ("%s", sCommand->str);
				system (sCommand->str);
			}
			else
			{
				gchar *cNewConfFilePath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_CONF_FILE);
				cairo_dock_replace_keys_by_identifier (g_cConfFile, cNewConfFilePath, '+');
				g_free (cNewConfFilePath);
			}
			//\___________________ On charge les icones.
			///g_string_printf (sCommand, "find '%s' -mindepth 1 ! -name '*.desktop' ! -name 'container-*' -delete", g_cCurrentLaunchersPath);
			g_string_printf (sCommand, "rm -f '%s/%s'/*", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
			cd_message ("%s", sCommand->str);
			system (sCommand->str);
			///g_string_printf (sCommand, "find '%s/%s' -mindepth 1 ! -name '*.desktop' -exec /bin/cp -p '{}' '%s' \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentLaunchersPath);
			gchar *cNewLocalIconsPath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
			if (! g_file_test (cNewLocalIconsPath, G_FILE_TEST_IS_DIR))
			{
				g_string_printf (sCommand, "sed -i \"s/~\\/.cairo-dock/~\\/.config\\/%s/g\" '%s/%s/%s'", CAIRO_DOCK_DATA_DIR, g_cCairoDockDataDir, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_CONF_FILE);
				cd_message ("%s", sCommand->str);
				system (sCommand->str);
				
				g_string_printf (sCommand, "sed -i \"/default icon directory/ { s/~\\/.config\\/%s\\/%s\\/icons/%s/g }\" '%s/%s'", CAIRO_DOCK_DATA_DIR, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
				cd_message ("%s", sCommand->str);
				system (sCommand->str);
				
				g_string_printf (sCommand, "sed -i \"s/_ThemeDirectory_/%s/g\" '%s/%s'", CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
				cd_message ("%s", sCommand->str);
				system (sCommand->str);
				
				g_string_printf (sCommand, "find '%s/%s' -mindepth 1 ! -name '*.desktop' -exec /bin/cp '{}' '%s/%s' \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
			}
			else
			{
				g_string_printf (sCommand, "cp '%s'/* '%s/%s'", cNewLocalIconsPath, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
			}
			cd_message ("%s", sCommand->str);
			system (sCommand->str);
			g_free (cNewLocalIconsPath);
			
			//\___________________ On charge les extras.
			g_string_printf (sCommand, "%s/%s", cNewThemePath, CAIRO_DOCK_EXTRAS_DIR);
			if (g_file_test (sCommand->str, G_FILE_TEST_IS_DIR))
			{
				g_string_printf (sCommand, "cp -r '%s/%s'/* '%s/%s'", cNewThemePath, CAIRO_DOCK_EXTRAS_DIR, g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR);
				cd_message ("%s", sCommand->str);
				system (sCommand->str);
			}
			
			//\___________________ On charge les lanceurs si necessaire, en effacant ceux existants.
			if (g_pMainDock == NULL || g_key_file_get_boolean (pKeyFile, "Themes", "use theme launchers", NULL))
			{
				g_string_printf (sCommand, "rm -f '%s'/*.desktop", g_cCurrentLaunchersPath);
				cd_message ("%s", sCommand->str);
				system (sCommand->str);

				g_string_printf (sCommand, "cp '%s/%s'/*.desktop '%s'", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentLaunchersPath);
				cd_message ("%s", sCommand->str);
				system (sCommand->str);
			}
			
			//\___________________ On remplace tous les autres fichiers par les nouveaux.
			g_string_printf (sCommand, "find '%s' -mindepth 1 -maxdepth 1  ! -name '*.conf' -type f -exec rm -f '{}' \\;", g_cCurrentThemePath);  // efface tous les fichiers du theme mais sans toucher aux lanceurs et aux plug-ins.
			cd_message ("%s", sCommand->str);
			system (sCommand->str);

			if (g_pMainDock == NULL || g_key_file_get_boolean (pKeyFile, "Themes", "use theme behaviour", NULL))
			{
				g_string_printf (sCommand, "find '%s'/* -prune ! -name '*.conf' ! -name %s -exec /bin/cp -r '{}' '%s' \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);  // copie tous les fichiers du nouveau theme sauf les lanceurs et le .conf, dans le repertoire du theme courant. Ecrase les fichiers de memes noms.
				cd_message ("%s", sCommand->str);
				system (sCommand->str);
			}
			else
			{
				g_string_printf (sCommand, "find '%s' ! -name '*.conf' ! -path '%s/%s*'  -mindepth 1 ! -type d -exec cp -p {} '%s' \\;", cNewThemePath, cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);  // copie tous les fichiers du nouveau theme sauf les lanceurs/icones et les .conf du dock et des plug-ins.
				cd_message ("%s", sCommand->str);
				system (sCommand->str);
				
				gchar *cNewPlugInsDir = g_strdup_printf ("%s/%s", cNewThemePath, "plug-ins");
				GDir *dir = g_dir_open (cNewPlugInsDir, 0, NULL);  // NULL si ce theme n'a pas de repertoire 'plug-ins'.
				const gchar* cModuleName;
				gchar *cConfFilePath, *cNewConfFilePath, *cUserDataDirPath;
				do
				{
					cModuleName = g_dir_read_name (dir);
					if (cModuleName == NULL)
						break ;
					
					CairoDockModule *pModule =  cairo_dock_find_module_from_name (cModuleName);
					if (pModule == NULL || pModule->pVisitCard == NULL)
						continue;

					cd_debug ("  installing %s's config\n", cModuleName);
					cUserDataDirPath = g_strdup_printf ("%s/plug-ins/%s", g_cCurrentThemePath, pModule->pVisitCard->cUserDataDir);
					if (! g_file_test (cUserDataDirPath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
					{
						cd_debug ("    directory %s doesn't exist, it will be created.", cUserDataDirPath);
						
						gchar *command = g_strdup_printf ("mkdir -p %s", cUserDataDirPath);
						system (command);
						g_free (command);
					}
					cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, pModule->pVisitCard->cConfFileName);
					cNewConfFilePath = g_strdup_printf ("%s/%s/%s", cNewPlugInsDir, pModule->pVisitCard->cUserDataDir, pModule->pVisitCard->cConfFileName);
					
					if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
					{
						cd_debug ("    no conf file %s, we will take the theme's one", cConfFilePath);
						gchar *command = g_strdup_printf ("cp '%s' '%s'", cNewConfFilePath, cConfFilePath);
						system (command);
						g_free (command);
					}
					else
					{
						cairo_dock_replace_keys_by_identifier (cConfFilePath, cNewConfFilePath, '+');
					}
					g_free (cNewConfFilePath);
					g_free (cConfFilePath);
					g_free (cUserDataDirPath);
				}
				while (1);
				g_dir_close (dir);
				g_free (cNewPlugInsDir);
			}
			//\___________________ On charge le theme courant.
			cairo_dock_load_theme (g_cCurrentThemePath);

			g_free (cNewThemeName);
			//g_hash_table_destroy (hThemeTable);
			
			if (bDistantTheme)
			{
				g_string_printf (sCommand, "rm -rf \"%s\"", (cRepToDelete != NULL ? cRepToDelete : cNewThemePath));
				system (sCommand->str);
				g_free (cNewThemePath);
				g_free (cRepToDelete);
			}
			
			g_string_free (sCommand, TRUE);
			return ;
		}
		g_free (cNewThemeName);

		//\___________________ On sauvegarde le theme actuel
		cNewThemeName = g_key_file_get_string (pKeyFile, "Save", "theme name", &erreur);
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
			cd_message ("on sauvegarde dans %s", cNewThemeName);
			gboolean bThemeSaved = FALSE;
			gchar *cNewThemePath = g_hash_table_lookup (hThemeTable, cNewThemeName);
			if (cNewThemePath != NULL)  // on ecrase un theme existant.
			{
				cd_message ("  theme existant");
				if (strncmp (cNewThemePath, CAIRO_DOCK_SHARE_THEMES_DIR, strlen (CAIRO_DOCK_SHARE_THEMES_DIR)) == 0)  // c'est un theme pre-installe.
				{
					cd_warning ("You can't overwrite a pre-installed theme");
				}
				else
				{
					gchar *question = g_strdup_printf (_("Are you sure you want to overwrite theme %s ?"), cNewThemeName);
					GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (pWidget),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_QUESTION,
						GTK_BUTTONS_YES_NO,
						question);
					g_free (question);
					int answer = gtk_dialog_run (GTK_DIALOG (dialog));
					gtk_widget_destroy (dialog);
					if (answer == GTK_RESPONSE_YES)
					{
						gchar *cNewConfFilePath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_CONF_FILE);
						if (g_key_file_get_boolean (pKeyFile, "Save", "save current behaviour", NULL))
						{
							g_string_printf (sCommand, "/bin/cp '%s' '%s'", g_cConfFile, cNewConfFilePath);
							cd_message ("%s", sCommand->str);
							system (sCommand->str);
						}
						else
						{
							cairo_dock_replace_keys_by_identifier (cNewConfFilePath, g_cConfFile, '+');
						}
						g_free (cNewConfFilePath);

						if (g_key_file_get_boolean (pKeyFile, "Save", "save current launchers", NULL))
						{
							g_string_printf (sCommand, "rm -f '%s/%s'/*", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
							cd_message ("%s", sCommand->str);
							system (sCommand->str);
							
							g_string_printf (sCommand, "cp '%s'/* '%s/%s'", g_cCurrentLaunchersPath, cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
							cd_message ("%s", sCommand->str);
							system (sCommand->str);
						}

						g_string_printf (sCommand, "find '%s' -mindepth 1 -maxdepth 1  ! -name '*.conf' ! -name '%s' -exec /bin/cp -r '{}' '%s' \\;", g_cCurrentThemePath, CAIRO_DOCK_LAUNCHERS_DIR, cNewThemePath);
						cd_message ("%s", sCommand->str);
						system (sCommand->str);

						bThemeSaved = TRUE;
					}
				}
			}
			else
			{
				cNewThemePath = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR, cNewThemeName);
				cd_message ("  nouveau theme (%s)", cNewThemePath);

				if (g_mkdir (cNewThemePath, 7*8*8+7*8+5) != 0)
					bThemeSaved = FALSE;
				else
				{
					g_string_printf (sCommand, "cp -r '%s'/* '%s'", g_cCurrentThemePath, cNewThemePath);
					cd_message ("%s", sCommand->str);
					system (sCommand->str);

					g_free (cNewThemePath);
					bThemeSaved = TRUE;
				}
			}
			if (bThemeSaved)
				cairo_dock_mark_theme_as_modified (FALSE);
		}

		//\___________________ On efface les themes qui ne sont plus desires.
		gsize length = 0;
		gchar ** cThemesList = g_key_file_get_string_list (pKeyFile, "Delete", "wanted themes", &length, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		else if (cThemesList != NULL)
		{
			gchar *cThemesDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR);
			GHashTable *hUserThemeTable = cairo_dock_list_themes (cThemesDir, NULL, &erreur);
			g_free (cThemesDir);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
			else
			{
				if (cNewThemeName != NULL)
					g_hash_table_remove (hUserThemeTable, cNewThemeName);  // pour ne pas effacer le theme qu'on vient d'enregistrer.
				gpointer data[2] = {cThemesList, pWidget};
				g_hash_table_foreach (hUserThemeTable, (GHFunc) _cairo_dock_delete_one_theme, data);
			}
			g_hash_table_destroy (hUserThemeTable);
		}
		g_strfreev (cThemesList);

		g_free (cNewThemeName);
		g_key_file_free (pKeyFile);
		g_string_free (sCommand, TRUE);
}
gboolean cairo_dock_manage_themes (GtkWidget *pWidget, gint iMode)
{
	GHashTable *hThemeTable = NULL;
	
	gchar *cInitConfFile = cairo_dock_build_themes_conf_file (&hThemeTable);
	
	//\___________________ On laisse l'utilisateur l'editer.
	gchar *cPresentedGroup = (cairo_dock_theme_need_save () ? "Save" : NULL);
	const gchar *cTitle = (iMode == 2 ? _("< Safe Mode >") : _("Manage Themes"));
	
	CairoDialog *pDialog = NULL;
	if (iMode == 2)
	{
		g_print ("show dialog\n");
		pDialog = cairo_dock_show_general_message (_("You are running Cairo-Dock in safe mode.\nWhy ? Probably because a plug-in has messed into your dock,\n or maybe your theme has got corrupted.\nSo, no plug-in will be available, and you can now save your current theme if you want\n before you start using the dock.\nTry with your current theme, if it works, it means a plug-in is wrong.\nOtherwise, try with another theme.\nSave a config that is working, and restart the dock in normal mode.\nThen, activate plug-ins one by one to guess which one is wrong."), 0.);
	}
	
	//gboolean bChoiceOK = cairo_dock_edit_conf_file (NULL, cTmpConfFile, cTitle, CAIRO_DOCK_THEME_PANEL_WIDTH, CAIRO_DOCK_THEME_PANEL_HEIGHT, 0, cPresentedGroup, NULL, NULL, NULL, CAIRO_DOCK_GETTEXT_PACKAGE);
	gpointer *data = g_new0 (gpointer, 3);
	data[0] = cInitConfFile;
	data[1] = hThemeTable;
	data[2] = pDialog;
	if (iMode == 0)
	{
		gboolean bChoiceOK = cairo_dock_build_normal_gui (cInitConfFile, NULL, cTitle, CAIRO_DOCK_THEME_PANEL_WIDTH, CAIRO_DOCK_THEME_PANEL_HEIGHT, on_theme_apply, data, on_theme_destroy);
	}
	else
	{
		gboolean bChoiceOK = cairo_dock_build_normal_gui (cInitConfFile, NULL, cTitle, CAIRO_DOCK_THEME_PANEL_WIDTH, CAIRO_DOCK_THEME_PANEL_HEIGHT, NULL, NULL, NULL);
		g_print ("on applique le theme\n");
		on_theme_apply (data);
		g_print ("on free\n");
		on_theme_destroy (data);
	}
	
	
	return FALSE;
}

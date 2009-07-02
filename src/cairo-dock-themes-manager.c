/***********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

************************************************************************************/
#include <string.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <sys/stat.h>
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
#include "cairo-dock-themes-manager.h"

#define CAIRO_DOCK_MODIFIED_THEME_FILE ".cairo-dock-need-save"
#define CAIRO_DOCK_THEME_PANEL_WIDTH 750
#define CAIRO_DOCK_THEME_PANEL_HEIGHT 400
#define CAIRO_DOCK_THEME_SERVER "http://themes.cairo-dock.org"
#define CAIRO_DOCK_BACKUP_THEME_SERVER "http://fabounet03.free.fr"
#define CAIRO_DOCK_PREFIX_NET_THEME "(Net)   "
#define CAIRO_DOCK_PREFIX_USER_THEME "(User)  "
#define CAIRO_DOCK_PREFIX_LOCAL_THEME "(Local) "
#define CAIRO_DOCK_DEFAULT_THEME_LIST_NAME "liste.txt"
#define CAIRO_DOCK_DL_NB_RETRY 2
#define CAIRO_DOCK_DL_TIMEOUT 5

extern gchar *g_cCairoDockDataDir;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cMainDockDefaultRendererName;
extern gchar *g_cThemeServerAdress;

extern CairoDock *g_pMainDock;
extern gboolean g_bForceOpenGL;
extern gboolean g_bEasterEggs;

static GtkWidget *s_pThemeManager = NULL;

void cairo_dock_free_theme (CairoDockTheme *pTheme)
{
	if (pTheme == NULL)
		return ;
	g_free (pTheme->cThemePath);
	g_free (pTheme->cAuthor);
	g_free (pTheme->cDisplayedName);
	g_free (pTheme);
}

GHashTable *cairo_dock_list_local_themes (const gchar *cThemesDir, GHashTable *hProvidedTable, GError **erreur)
{
	cd_message ("%s (%s)", __func__, cThemesDir);
	GError *tmp_erreur = NULL;
	GDir *dir = g_dir_open (cThemesDir, 0, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return hProvidedTable;
	}

	GHashTable *pThemeTable = (hProvidedTable != NULL ? hProvidedTable : g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_theme));

	gchar *cThemePath;
	const gchar* cThemeName;
	int iType = (strncmp (cThemesDir, "/usr", 4) == 0 ? 0 : 1);
	const gchar *cPrefix = (iType == 0 ? CAIRO_DOCK_PREFIX_LOCAL_THEME : CAIRO_DOCK_PREFIX_USER_THEME);
	CairoDockTheme *pTheme;
	do
	{
		cThemeName = g_dir_read_name (dir);
		if (cThemeName == NULL)
			break ;
		
		cThemePath = g_strdup_printf ("%s/%s", cThemesDir, cThemeName);
		if (g_file_test (cThemePath, G_FILE_TEST_IS_DIR))
		{
			pTheme = g_new0 (CairoDockTheme, 1);
			pTheme->cThemePath = cThemePath;
			pTheme->cDisplayedName = g_strdup_printf ("%s%s", (cPrefix != NULL ? cPrefix : ""), cThemeName);
			pTheme->iType = iType;
			
			g_hash_table_insert (pThemeTable, g_strdup (cThemeName), pTheme);  // donc ecrase un theme installe ayant le meme nom.
		}
	}
	while (1);
	g_dir_close (dir);
	return pThemeTable;
}

gchar *cairo_dock_uncompress_file (const gchar *cArchivePath, const gchar *cExtractTo, const gchar *cRealArchiveName)
{
	if (!g_file_test (cExtractTo, G_FILE_TEST_EXISTS))
	{
		if (g_mkdir (cExtractTo, 7*8*8+7*8+5) != 0)
		{
			cd_warning ("couldn't create directory %s", cExtractTo);
			return NULL;
		}
	}
	gchar *cResultPath;
	gchar *cCommand = g_strdup_printf ("tar xfz \"%s\" -C \"%s\"", cArchivePath, cExtractTo);
	int r = system (cCommand);
	if (r != 0)
	{
		cd_warning ("an error occured while executing '%s'", cCommand);
		cResultPath = NULL;
	}
	else
	{
		gchar *cLocalFileName;  // on construit le nom local du theme apres decompression.
		if (cRealArchiveName == NULL)
			cRealArchiveName = cArchivePath;
		gchar *str = strrchr (cRealArchiveName, '/');
		if (str != NULL)
			cLocalFileName = g_strdup (str+1);
		else
			cLocalFileName = g_strdup (cRealArchiveName);
		
		if (g_str_has_suffix (cLocalFileName, ".tar.gz"))
			cLocalFileName[strlen(cLocalFileName)-7] = '\0';
		
		cResultPath = g_strdup_printf ("%s/%s", cExtractTo, cLocalFileName);
		g_free (cLocalFileName);
	}
	g_free (cCommand);
	return cResultPath;
}

gchar *cairo_dock_download_file (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, gint iShowActivity, const gchar *cExtractTo, GError **erreur)
{
	cairo_dock_set_status_message_printf (s_pThemeManager, _("Downloading file %s ..."), cDistantFileName);
	gchar *cTmpFilePath = g_strdup ("/tmp/cairo-dock-net-file.XXXXXX");
	int fds = mkstemp (cTmpFilePath);
	if (fds == -1)
	{
		g_set_error (erreur, 1, 1, "couldn't create temporary file '%s'", cTmpFilePath);
		g_free (cTmpFilePath);
		return NULL;
	}
	
	CairoDialog *pDialog = NULL;
	if (iShowActivity == 1)
	{
		pDialog = cairo_dock_show_temporary_dialog_with_default_icon ("downloading file %s on %s ...", NULL, NULL, 0, cDistantFileName, cServerAdress);
		cairo_dock_dialog_reference (pDialog);
		g_print ("downloading file ...\n");
		while (gtk_events_pending ())
			gtk_main_iteration ();
	}
	gchar *cCommand = g_strdup_printf ("%s wget \"%s/%s/%s\" -O \"%s\" -t %d -T %d%s", (iShowActivity == 2 ? "$TERM -e '" : ""), cServerAdress, cDistantFilePath, cDistantFileName, cTmpFilePath, CAIRO_DOCK_DL_NB_RETRY, CAIRO_DOCK_DL_TIMEOUT, (iShowActivity == 2 ? "'" : ""));
	g_print ("%s\n", cCommand);
	
	int r = system (cCommand);
	if (r != 0)
	{
		g_set_error (erreur, 1, 1, "an error occured while executing '%s'", cCommand);
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		cTmpFilePath = NULL;
	}
	
	gboolean bOk = (cTmpFilePath != NULL);
	if (cTmpFilePath != NULL)
	{
		struct stat buf;
		stat (cTmpFilePath, &buf);
		bOk = (buf.st_size > 0);
	}
	if (! bOk)
	{
		g_set_error (erreur, 1, 1, "couldn't get distant file %s", cDistantFileName);
		cairo_dock_set_status_message_printf (s_pThemeManager, _("couldn't get distant file %s"), cDistantFileName);
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		cTmpFilePath = NULL;
	}
	else
	{
		cairo_dock_set_status_message (s_pThemeManager, "");
	}
	close(fds);
	g_free (cCommand);
	
	if (cTmpFilePath != NULL && cExtractTo != NULL)
	{
		if (pDialog != NULL)
		{
			cairo_dock_set_dialog_message_printf (pDialog, "uncompressing %s", cTmpFilePath);
			g_print ("uncompressing ...\n");
			while (gtk_events_pending ())
				gtk_main_iteration ();
		}
		
		gchar *cPath = cairo_dock_uncompress_file (cTmpFilePath, cExtractTo, cDistantFileName);
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		cTmpFilePath = cPath;
	}
	
	if (! cairo_dock_dialog_unreference (pDialog))
		cairo_dock_dialog_unreference (pDialog);
	return cTmpFilePath;
}

gchar *cairo_dock_get_distant_file_content (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, gint iShowActivity, GError **erreur)
{
	GError *tmp_erreur = NULL;
	gchar *cTmpFilePath = cairo_dock_download_file (cServerAdress, cDistantFilePath, cDistantFileName, iShowActivity, NULL, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return NULL;
	}

	gchar *cContent = NULL;
	gsize length = 0;
	g_file_get_contents (cTmpFilePath, &cContent, &length, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		cd_warning ("while opening the downloaded file '%s/%s/%s' : %s\n check that your connection is alive, or retry later", cServerAdress, cDistantFilePath, cDistantFileName, tmp_erreur->message);
		g_propagate_error (erreur, tmp_erreur);
	}
	else if (cContent == NULL)
	{
		cd_warning ("couldn't retrieve info from '%s/%s/%s'\n check that your connection is alive, or retry later", cServerAdress, cDistantFilePath, cDistantFileName);
		g_set_error (erreur, 1, 1, "couldn't retrieve info from '%s/%s/%s'\n check that your connection is alive, or retry later", cServerAdress, cDistantFilePath, cDistantFileName);
	}
	
	g_remove (cTmpFilePath);
	g_free (cTmpFilePath);
	return cContent;
}

GHashTable *cairo_dock_list_net_themes (const gchar *cServerAdress, const gchar *cDirectory, const gchar *cListFileName, GHashTable *hProvidedTable, GError **erreur)
{
	g_return_val_if_fail (cServerAdress != NULL && *cServerAdress != '\0', hProvidedTable);
	cd_message ("listing net themes on %s/%s ...", cServerAdress, cDirectory);
	
	GError *tmp_erreur = NULL;
	gchar *cContent = cairo_dock_get_distant_file_content (cServerAdress, cDirectory, cListFileName, 0, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		cd_warning ("couldn't retrieve themes on %s (check that your connection is alive, or retry later)", cServerAdress);
		g_propagate_error (erreur, tmp_erreur);
		return hProvidedTable;
	}
	if (cContent == NULL || (g_bEasterEggs && strncmp (cContent, "#CD", 3) != 0))
	{
		cd_warning ("empty themes list on %s (check that your connection is alive, or retry later)", cServerAdress);
		g_set_error (erreur, 1, 1, "empty themes list on %s", cServerAdress);
		g_free (cContent);
		return hProvidedTable;
	}
	
	GHashTable *pThemeTable = (hProvidedTable != NULL ? hProvidedTable : g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_theme));
	
	gchar **cNetThemesList = g_strsplit (cContent, "\n", -1);
	g_free (cContent);
	
	int i;
	gboolean bFirstComment = FALSE;
	gchar *cThemeName;
	CairoDockTheme *pTheme = NULL;
	for (i = 0; cNetThemesList[i] != NULL; i ++)
	{
		cThemeName = cNetThemesList[i];
		if (*cThemeName == '#' || *cThemeName == '\0')
		{
			if (*cThemeName == '#' && bFirstComment && pTheme != NULL)
			{
				cd_debug ("+ commentaire : %s", cThemeName+1);
				pTheme->fSize = g_ascii_strtod (cThemeName+1, NULL);
				pTheme->cAuthor = g_strdup (strchr (cThemeName, ' '));
				gchar *cDisplayedName = g_strdup_printf ("%s by %s [%.2f MB]", pTheme->cDisplayedName, (pTheme->cAuthor ? pTheme->cAuthor : "---"), pTheme->fSize);
				g_free (pTheme->cDisplayedName);
				pTheme->cDisplayedName = cDisplayedName;
			}
			g_free (cThemeName);
			bFirstComment = FALSE;
		}
		else
		{
			cd_debug ("+ theme : %s", cThemeName);
			pTheme = g_new0 (CairoDockTheme, 1);
			pTheme->iType = CAIRO_DOCK_DISTANT_THEME;
			pTheme->cThemePath = g_strdup_printf ("%s/%s/%s", cServerAdress, cDirectory, cThemeName);
			pTheme->cDisplayedName = g_strconcat (CAIRO_DOCK_PREFIX_NET_THEME, cThemeName, NULL);
			g_hash_table_insert (pThemeTable, cThemeName, pTheme);
			bFirstComment = TRUE;
		}
	}
	
	g_free (cNetThemesList);  // car les noms sont passes dans la hashtable.
	return pThemeTable;
}

GHashTable *cairo_dock_list_themes (const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir)
{
	cd_message ("%s (%s, %s, %s)", __func__, cShareThemesDir, cUserThemesDir, cDistantThemesDir);
	GError *erreur = NULL;
	GHashTable *pThemeTable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_theme);
	
	//\______________ On recupere les themes distants.
	if (cDistantThemesDir != NULL)
		pThemeTable = cairo_dock_list_net_themes (g_cThemeServerAdress != NULL ? g_cThemeServerAdress : CAIRO_DOCK_THEME_SERVER, cDistantThemesDir, CAIRO_DOCK_DEFAULT_THEME_LIST_NAME, pThemeTable, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while loading distant themes in '%s/%s' : %s", g_cThemeServerAdress != NULL ? g_cThemeServerAdress : CAIRO_DOCK_THEME_SERVER, cDistantThemesDir, erreur->message);
		cairo_dock_set_status_message_printf (s_pThemeManager, _("couldn't get the list of themes for %s (no connection ?)"), cDistantThemesDir);
		g_error_free (erreur);
		erreur = NULL;
	}
	
	//\______________ On recupere les themes pre-installes (qui ecrasent donc les precedents).
	if (cShareThemesDir != NULL)
		pThemeTable = cairo_dock_list_local_themes (cShareThemesDir, pThemeTable, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while loading pre-installed themes in '%s' : %s", cShareThemesDir, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	
	//\______________ On recupere les themes utilisateurs (qui ecrasent donc tout le monde).
	if (cUserThemesDir != NULL)
		pThemeTable = cairo_dock_list_local_themes (cUserThemesDir, pThemeTable, NULL);
	
	return pThemeTable;
}



gchar *cairo_dock_build_temporary_themes_conf_file (void)
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
	gchar *cCommand = g_strdup_printf ("cp '%s' '%s'", CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_THEME_CONF_FILE, cTmpConfFile);
	int r = system (cCommand);
	g_free (cCommand);

	close(fds);
	return cTmpConfFile;
}

void cairo_dock_load_current_theme (void)
{
	cd_message ("%s ()", __func__);

	//\___________________ On libere toute la memoire allouee pour les docks (stoppe aussi tous les timeout).
	cairo_dock_free_all_docks ();

	//\___________________ On cree le dock principal.
	g_pMainDock = cairo_dock_create_new_dock (CAIRO_DOCK_MAIN_DOCK_NAME, NULL);  // on ne lui assigne pas de vues, puisque la vue par defaut des docks principaux sera definie plus tard.
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



static void on_theme_destroy (gchar *cInitConfFile)
{
	g_print ("%s ()\n", __func__);
	g_remove (cInitConfFile);
	g_free (cInitConfFile);
	s_pThemeManager = NULL;
}

static gboolean _find_module_from_user_data_dir (gchar *cModuleName, CairoDockModule *pModule, const gchar *cUserDataDirName)
{
	if (pModule->pVisitCard->cUserDataDir && strcmp (cUserDataDirName, pModule->pVisitCard->cUserDataDir) == 0)
		return TRUE;
	return FALSE;
}
static gboolean on_theme_apply (gchar *cInitConfFile)
{
	g_print ("%s (%s)\n", __func__, cInitConfFile);
	GError *erreur = NULL;
	int r;  // resultat de system().
	gboolean bNeedSave = cairo_dock_theme_need_save ();

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
		if (strncmp (cNewThemeName, CAIRO_DOCK_PREFIX_USER_THEME, strlen (CAIRO_DOCK_PREFIX_USER_THEME)) == 0)
		{
			gchar *tmp = cNewThemeName;
			cNewThemeName = g_strdup (cNewThemeName+strlen (CAIRO_DOCK_PREFIX_USER_THEME));
			g_free (tmp);
			g_print (" => cNewThemeName : '%s'\n", cNewThemeName);
		}
		cd_message ("on sauvegarde dans %s", cNewThemeName);
		gboolean bThemeSaved = FALSE;
		gchar *cNewThemePath = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR, cNewThemeName);
		if (g_file_test (cNewThemePath, G_FILE_TEST_EXISTS))  // on ecrase un theme existant.
		{
			cd_debug ("  le theme existant sera mis a jour");
			gchar *cQuestion = g_strdup_printf (_("Are you sure you want to overwrite theme %s ?"), cNewThemeName);
			int answer = cairo_dock_ask_general_question_and_wait (cQuestion);
			g_free (cQuestion);
			if (answer == GTK_RESPONSE_YES)
			{
				//\___________________ On traite le fichier de conf.
				gchar *cNewConfFilePath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_CONF_FILE);
				if (g_key_file_get_boolean (pKeyFile, "Save", "save current behaviour", NULL))
				{
					g_string_printf (sCommand, "/bin/cp '%s' '%s'", g_cConfFile, cNewConfFilePath);
					cd_message ("%s", sCommand->str);
					r = system (sCommand->str);
				}
				else
				{
					cairo_dock_replace_keys_by_identifier (cNewConfFilePath, g_cConfFile, '+');
				}
				g_free (cNewConfFilePath);
				
				//\___________________ On traite les lanceurs.
				if (g_key_file_get_boolean (pKeyFile, "Save", "save current launchers", NULL))
				{
					g_string_printf (sCommand, "rm -f '%s/%s'/*", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
					cd_message ("%s", sCommand->str);
					r = system (sCommand->str);
					
					g_string_printf (sCommand, "cp '%s'/* '%s/%s'", g_cCurrentLaunchersPath, cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
					cd_message ("%s", sCommand->str);
					r = system (sCommand->str);
				}
				
				//\___________________ On traite tous le reste.
				/// TODO : traiter les .conf des applets comme celui du dock...
				g_string_printf (sCommand, "find '%s' -mindepth 1 -maxdepth 1  ! -name '*.conf' ! -name '%s' -exec /bin/cp -r '{}' '%s' \\;", g_cCurrentThemePath, CAIRO_DOCK_LAUNCHERS_DIR, cNewThemePath);
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
				g_string_printf (sCommand, "cp -r '%s'/* '%s'", g_cCurrentThemePath, cNewThemePath);
				cd_message ("%s", sCommand->str);
				r = system (sCommand->str);

				bThemeSaved = TRUE;
			}
			else
				cd_warning ("couldn't create %s", cNewThemePath);
		}
		
		if (g_key_file_get_boolean (pKeyFile, "Save", "package", NULL))
		{
			cd_message ("building theme package ...");
			if (g_file_test (CAIRO_DOCK_SHARE_DATA_DIR"/../../bin/cairo-dock-package-theme.sh", G_FILE_TEST_EXISTS))
			{
				gchar *cCommand = g_strdup_printf ("$TERM -e '%s \"%s\"'", "cairo-dock-package-theme.sh", cNewThemeName);
				r = system (cCommand);
				g_free (cCommand);
			}
			else
			{
				cd_warning ("the package builder script was not found !");
			}
		}	
		
		g_free (cNewThemePath);
		if (bThemeSaved)
			cairo_dock_mark_theme_as_modified (FALSE);		

		g_free (cNewThemeName);
		g_key_file_free (pKeyFile);
		g_string_free (sCommand, TRUE);
		return ! bThemeSaved;
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
		gboolean bThemeDeleted = FALSE;
		int answer;
		gchar *cQuestion;
		int i;
		if (cThemesList[1] == NULL)
			cQuestion = g_strdup_printf (_("Are you sure you want to delete theme %s ?"), cThemesList[0]);
		else
			cQuestion = g_strdup (_("Are you sure you want to delete these themes ?"));
		answer = cairo_dock_ask_general_question_and_wait (cQuestion);
		g_free (cQuestion);
		if (answer == GTK_RESPONSE_YES)
		{
			gchar *cThemeName;
			for (i = 0; cThemesList[i] != NULL; i ++)
			{
				cThemeName = cThemesList[i];
				if (*cThemeName == '\0')
					continue;
				if (cNewThemeName && strcmp (cNewThemeName, cThemeName) == 0)  // precaution probablement superflue.
					continue;

				bThemeDeleted = TRUE;
				g_string_printf (sCommand, "rm -rf '%s/%s/%s'", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR, cThemeName);
				r = system (sCommand->str);  // g_rmdir n'efface qu'un repertoire vide.
			}
		}
		g_strfreev (cThemesList);
		g_key_file_free (pKeyFile);
		g_string_free (sCommand, TRUE);
		return ! bThemeDeleted;
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
		//\___________________ On regarde si le theme courant est modifie.
		if (bNeedSave)
		{
			int iAnswer = cairo_dock_ask_general_question_and_wait (_("You made some modifications in the current theme.\nYou will loose them if you don't save before choosing a new theme. Continue anyway ?"));
			if (iAnswer != GTK_RESPONSE_YES)
			{
				return TRUE;
			}
			else
				cairo_dock_mark_theme_as_modified (FALSE);
		}
		
		//\___________________ On obtient le chemin du nouveau theme (telecharge si necessaire).
		int length = strlen (cNewThemeName);
		if (cNewThemeName[length-1] == '\n')
			cNewThemeName[--length] = '\0';  // on vire le retour chariot final.
		if (cNewThemeName[length-1] == '\r')
			cNewThemeName[--length] = '\0';
		g_print ("cNewThemeName : '%s'\n", cNewThemeName);
		gchar *cUserThemesDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR);
		gchar *cNewThemePath = NULL;
		if (g_str_has_suffix (cNewThemeName, ".tar.gz"))  // c'est un paquet.
		{
			g_print ("c'est un paquet\n");
			if (*cNewThemeName == '/' || strncmp (cNewThemeName, "file://", 7) == 0)  // paquet en local.
			{
				g_print (" paquet local\n");
				//cairo_dock_remove_html_spaces (cNewThemeName);
				cNewThemePath = cairo_dock_uncompress_file (*cNewThemeName == '/' ? cNewThemeName : cNewThemeName+7, cUserThemesDir, NULL);
			}
			else  // paquet distant.
			{
				g_print (" paquet distant\n");
				gchar *str = strrchr (cNewThemeName, '/');
				if (str != NULL)
				{
					*str = '\0';
					cNewThemePath = cairo_dock_download_file (cNewThemeName, "", str+1, 2, cUserThemesDir, NULL);
					if (cNewThemePath == NULL)
					{
						cairo_dock_show_temporary_dialog_with_default_icon (_("couldn't get distant file %s/%s, maybe the server is down.\nPlease retry later or contact us at cairo-dock.org."), NULL, NULL, 0, cNewThemeName, str+1);
					}
				}
			}
			g_return_val_if_fail (cNewThemePath != NULL, TRUE);
			g_print (" => cNewThemePath = '%s'\n", cNewThemePath);
			gchar *tmp = cNewThemeName;
			cNewThemeName = g_path_get_basename (cNewThemePath);
			//cNewThemeName[strlen (cNewThemeName) - 7] = '\0';
			g_free (tmp);
		}
		else  // c'est un theme officiel.
		{
			g_print ("c'est un theme officiel\n");
			cNewThemePath = cairo_dock_get_theme_path (cNewThemeName, CAIRO_DOCK_SHARE_THEMES_DIR, cUserThemesDir, CAIRO_DOCK_THEMES_DIR);
		}
		g_free (cUserThemesDir);
		
		g_return_val_if_fail (cNewThemePath != NULL && g_file_test (cNewThemePath, G_FILE_TEST_EXISTS), TRUE);
		g_print ("cNewThemePath : %s ; cNewThemeName : %s\n", cNewThemePath, cNewThemeName);
		
		//\___________________ On charge les parametres de comportement.
		cairo_dock_set_status_message (s_pThemeManager, _("Applying changes ..."));
		if (g_pMainDock == NULL || g_key_file_get_boolean (pKeyFile, "Themes", "use theme behaviour", NULL))
		{
			g_string_printf (sCommand, "/bin/cp '%s'/%s '%s'", cNewThemePath, CAIRO_DOCK_CONF_FILE, g_cCurrentThemePath);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
		}
		else
		{
			gchar *cNewConfFilePath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_CONF_FILE);
			cairo_dock_replace_keys_by_identifier (g_cConfFile, cNewConfFilePath, '+');
			g_free (cNewConfFilePath);
		}
		
		//\___________________ On charge les .conf des autres docks racines.
		g_string_printf (sCommand, "find '%s' -mindepth 1 -maxdepth 1 -name '*.conf' ! -name '%s' -exec /bin/cp '{}' '%s' \\;", cNewThemePath, CAIRO_DOCK_CONF_FILE, g_cCurrentThemePath);
		cd_message ("%s", sCommand->str);
		r = system (sCommand->str);
		
		//\___________________ On charge les lanceurs.
		if (g_key_file_get_boolean (pKeyFile, "Themes", "use theme launchers", NULL))
		{
			g_string_printf (sCommand, "rm -f '%s/%s'/*", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
			g_string_printf (sCommand, "rm -f '%s/%s'/.*", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
		}
		//\___________________ On charge les icones.
		gchar *cNewLocalIconsPath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
		if (! g_file_test (cNewLocalIconsPath, G_FILE_TEST_IS_DIR))  // c'est un ancien theme, mettons-le vite a jour !
		{
			g_string_printf (sCommand, "sed -i \"s/~\\/.cairo-dock/~\\/.config\\/%s/g\" '%s/%s/%s'", CAIRO_DOCK_DATA_DIR, g_cCairoDockDataDir, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_CONF_FILE);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
			
			g_string_printf (sCommand, "sed -i \"/default icon directory/ { s/~\\/.config\\/%s\\/%s\\/icons/%s/g }\" '%s/%s'", CAIRO_DOCK_DATA_DIR, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
			
			g_string_printf (sCommand, "sed -i \"s/_ThemeDirectory_/%s/g\" '%s/%s'", CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
			
			g_string_printf (sCommand, "find '%s/%s' -mindepth 1 ! -name '*.desktop' -exec /bin/cp '{}' '%s/%s' \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
		}
		else
		{
			g_string_printf (sCommand, "for f in '%s'/* ; do rm -f \"%s/%s/`basename \"${f%%.*}\"`\"*; done;", cNewLocalIconsPath, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);  // on efface les doublons car sinon on pourrait avoir x.png et x.svg ensemble et le dock ne saurait pas lequel choisir.
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
			
			g_string_printf (sCommand, "cp '%s'/* '%s/%s'", cNewLocalIconsPath, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
		}
		cd_message ("%s", sCommand->str);
		r = system (sCommand->str);
		g_free (cNewLocalIconsPath);
		
		//\___________________ On charge les extras.
		g_string_printf (sCommand, "%s/%s", cNewThemePath, CAIRO_DOCK_EXTRAS_DIR);
		if (g_file_test (sCommand->str, G_FILE_TEST_IS_DIR))
		{
			g_string_printf (sCommand, "cp -r '%s/%s'/* '%s/%s'", cNewThemePath, CAIRO_DOCK_EXTRAS_DIR, g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
		}
		
		//\___________________ On charge les lanceurs si necessaire, en effacant ceux existants.
		if (g_pMainDock == NULL || g_key_file_get_boolean (pKeyFile, "Themes", "use theme launchers", NULL))
		{
			g_string_printf (sCommand, "rm -f '%s'/*.desktop", g_cCurrentLaunchersPath);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);

			g_string_printf (sCommand, "cp '%s/%s'/*.desktop '%s'", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentLaunchersPath);
			cd_message ("%s", sCommand->str);
			r = system (sCommand->str);
		}
		
		//\___________________ On remplace tous les autres fichiers par les nouveaux.
		g_string_printf (sCommand, "find '%s' -mindepth 1 -maxdepth 1  ! -name '*.conf' -type f -exec rm -f '{}' \\;", g_cCurrentThemePath);  // efface tous les fichiers du theme mais sans toucher aux lanceurs et aux plug-ins.
		cd_message (sCommand->str);
		r = system (sCommand->str);

		if (g_pMainDock == NULL || g_key_file_get_boolean (pKeyFile, "Themes", "use theme behaviour", NULL))
		{
			g_string_printf (sCommand, "find '%s'/* -prune ! -name '*.conf' ! -name %s -exec /bin/cp -r '{}' '%s' \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);  // copie tous les fichiers du nouveau theme sauf les lanceurs et le .conf, dans le repertoire du theme courant. Ecrase les fichiers de memes noms.
			cd_message (sCommand->str);
			r = system (sCommand->str);
		}
		else
		{
			g_string_printf (sCommand, "find '%s' -mindepth 1 ! -name '*.conf' ! -path '%s/%s*' ! -type d -exec cp -p {} '%s' \\;", cNewThemePath, cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);  // copie tous les fichiers du nouveau theme sauf les lanceurs et les .conf du dock et des plug-ins.
			cd_message (sCommand->str);
			r = system (sCommand->str);
			
			gchar *cNewPlugInsDir = g_strdup_printf ("%s/%s", cNewThemePath, "plug-ins");
			GDir *dir = g_dir_open (cNewPlugInsDir, 0, NULL);  // NULL si ce theme n'a pas de repertoire 'plug-ins'.
			const gchar* cModuleDirName;
			gchar *cConfFilePath, *cNewConfFilePath, *cUserDataDirPath, *cConfFileName;
			do
			{
				cModuleDirName = g_dir_read_name (dir);
				if (cModuleDirName == NULL)
					break ;
				
				/*CairoDockModule *pModule =  cairo_dock_find_module_from_name (cModuleName);
				if (pModule == NULL || pModule->pVisitCard == NULL)
					continue;*/

				cd_debug ("  installing %s's config\n", cModuleDirName);
				cUserDataDirPath = g_strdup_printf ("%s/plug-ins/%s", g_cCurrentThemePath, cModuleDirName);
				if (! g_file_test (cUserDataDirPath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
				{
					cd_debug ("    directory %s doesn't exist, it will be created.", cUserDataDirPath);
					
					gchar *command = g_strdup_printf ("mkdir -p '%s'", cUserDataDirPath);
					r = system (command);
					g_free (command);
				}
				
				cConfFileName = g_strdup_printf ("%s.conf", cModuleDirName);
				cNewConfFilePath = g_strdup_printf ("%s/%s/%s", cNewPlugInsDir, cModuleDirName, cConfFileName);
				if (! g_file_test (cNewConfFilePath, G_FILE_TEST_EXISTS))
				{
					g_free (cConfFileName);
					g_free (cNewConfFilePath);
					CairoDockModule *pModule = cairo_dock_foreach_module ((GHRFunc) _find_module_from_user_data_dir, cModuleDirName);
					if (pModule == NULL)  // du coup, dans ce cas-la, on ne charge pas des plug-ins non utilises par l'utilisateur.
					{
						cd_warning ("couldn't find the module owning '%s', this file will be ignored.");
						continue;
					}
					cConfFileName = g_strdup (pModule->pVisitCard->cConfFileName);
					cNewConfFilePath = g_strdup_printf ("%s/%s/%s", cNewPlugInsDir, cModuleDirName, cConfFileName);
				}
				cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, cConfFileName);
				
				if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
				{
					cd_debug ("    no conf file %s, we will take the theme's one", cConfFilePath);
					gchar *command = g_strdup_printf ("cp '%s' '%s'", cNewConfFilePath, cConfFilePath);
					r = system (command);
					g_free (command);
				}
				else
				{
					cairo_dock_replace_keys_by_identifier (cConfFilePath, cNewConfFilePath, '+');
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
		
		//\___________________ On charge le theme courant.
		cairo_dock_set_status_message (s_pThemeManager, _("Now reloading theme ..."));
		cairo_dock_load_current_theme ();

		g_free (cNewThemeName);
		g_free (cNewThemePath);
		g_string_free (sCommand, TRUE);
		cairo_dock_set_status_message (s_pThemeManager, "");
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
	gchar *cPresentedGroup = (cairo_dock_theme_need_save () ? "Save" : NULL);
	const gchar *cTitle = _("Manage Themes");
	
	cairo_dock_build_normal_gui (cInitConfFile,
		NULL, cTitle,
		CAIRO_DOCK_THEME_PANEL_WIDTH, CAIRO_DOCK_THEME_PANEL_HEIGHT,
		(CairoDockApplyConfigFunc) on_theme_apply,
		cInitConfFile,
		(GFreeFunc) on_theme_destroy,
		&s_pThemeManager);
}

gchar *cairo_dock_get_theme_path (const gchar *cThemeName, const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir)
{
	cd_message ("%s (%s, %s, %s)", __func__, cShareThemesDir, cUserThemesDir, cDistantThemesDir);
	if (cThemeName == NULL || *cThemeName == '\0')
		return NULL;
	gchar *cThemePath = NULL;
	
	if (cUserThemesDir != NULL)
	{
		cThemePath = g_strdup_printf ("%s/%s", cUserThemesDir, cThemeName);
		if (g_file_test (cThemePath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			return cThemePath;
		g_free (cThemePath);
		cThemePath = NULL;
	}
	if (cShareThemesDir != NULL)
	{
		cThemePath = g_strdup_printf ("%s/%s", cShareThemesDir, cThemeName);
		if (g_file_test (cThemePath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			return cThemePath;
		g_free (cThemePath);
		cThemePath = NULL;
	}
	
	if (cDistantThemesDir != NULL)
	{
		gchar *cDistantFileName = g_strdup_printf ("%s/%s.tar.gz", cThemeName, cThemeName);
		GError *erreur = NULL;
		cThemePath = cairo_dock_download_file (g_cThemeServerAdress != NULL ? g_cThemeServerAdress : CAIRO_DOCK_THEME_SERVER, cDistantThemesDir, cDistantFileName, 0, cUserThemesDir, &erreur);
		g_free (cDistantFileName);
		if (erreur != NULL)
		{
			cd_warning ("couldn't retrieve distant theme %s : %s" , cThemeName, erreur->message);
			cairo_dock_set_status_message_printf (s_pThemeManager, _("couldn't retrieve distant theme %s"), cThemeName);  // le message sera repris par une bulle de dialogue, mais on le met la aussi quand meme.
			g_error_free (erreur);
		}
	}
	
	g_print (" ====> cThemePath : %s\n", cThemePath);
	return cThemePath;
}

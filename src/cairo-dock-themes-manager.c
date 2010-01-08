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
#include "cairo-dock-internal-system.h"
#include "cairo-dock-themes-manager.h"

#define CAIRO_DOCK_MODIFIED_THEME_FILE ".cairo-dock-need-save"
#define CAIRO_DOCK_THEME_PANEL_WIDTH 1000
#define CAIRO_DOCK_THEME_PANEL_HEIGHT 530
///#define CAIRO_DOCK_THEME_SERVER "http://themes.cairo-dock.org"
#define CAIRO_DOCK_THEME_SERVER "http://themes.vef.fr"  // en attendant que le nom du domaine revienne.
#define CAIRO_DOCK_BACKUP_THEME_SERVER "http://fabounet03.free.fr"
#define CAIRO_DOCK_DEFAULT_THEME_LIST_NAME "list.conf"

extern gchar *g_cCairoDockDataDir;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cMainDockDefaultRendererName;
extern gchar *g_cThemeServerAdress;
extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;

extern CairoDock *g_pMainDock;
extern gboolean g_bForceOpenGL;

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

static inline int _get_theme_rating (const gchar *cThemesDir, const gchar *cThemeName)
{
	gchar *cRatingFile = g_strdup_printf ("%s/.rating/%s", cThemesDir, cThemeName);
	int iRating = 0;
	gsize length = 0;
	gchar *cContent = NULL;
	g_file_get_contents (cRatingFile,
		&cContent,
		&length,
		NULL);
	if (cContent)
	{
		iRating = atoi (cContent);
		g_free (cContent);
	}
	g_free (cRatingFile);
	return iRating;	
}

GHashTable *cairo_dock_list_local_themes (const gchar *cThemesDir, GHashTable *hProvidedTable, gboolean bUpdateThemeValidity, GError **erreur)
{
	g_print ("%s (%s)\n", __func__, cThemesDir);
	GError *tmp_erreur = NULL;
	GDir *dir = g_dir_open (cThemesDir, 0, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return hProvidedTable;
	}

	GHashTable *pThemeTable = (hProvidedTable != NULL ? hProvidedTable : g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_theme));
	
	CairoDockThemeType iType = (strncmp (cThemesDir, "/usr", 4) == 0 ?
		CAIRO_DOCK_LOCAL_THEME :
		CAIRO_DOCK_USER_THEME);
	GString *sRatingFile = g_string_new (cThemesDir);
	gchar *cThemePath;
	CairoDockTheme *pTheme;
	const gchar *cThemeName;
	while ((cThemeName = g_dir_read_name (dir)) != NULL)
	{
		// on ecarte les fichiers caches.
		if (*cThemeName == '.')
			continue;
		
		// on ecarte les non repertoires.
		cThemePath = g_strdup_printf ("%s/%s", cThemesDir, cThemeName);
		if (! g_file_test (cThemePath, G_FILE_TEST_IS_DIR))
		{
			g_free (cThemePath);
			continue;
		}
		
		// on insere le theme dans la table.
		pTheme = g_new0 (CairoDockTheme, 1);
		pTheme->cThemePath = cThemePath;
		pTheme->cDisplayedName = g_strdup (cThemeName);
		pTheme->iType = iType;
		pTheme->iRating = _get_theme_rating (cThemesDir, cThemeName);
		g_hash_table_insert (pThemeTable, g_strdup (cThemeName), pTheme);  // donc ecrase un theme installe ayant le meme nom.
	}
	g_dir_close (dir);
	return pThemeTable;
}

gchar *cairo_dock_uncompress_file (const gchar *cArchivePath, const gchar *cExtractTo, const gchar *cRealArchiveName)
{
	// on cree le repertoire d'extraction.
	if (!g_file_test (cExtractTo, G_FILE_TEST_EXISTS))
	{
		if (g_mkdir (cExtractTo, 7*8*8+7*8+5) != 0)
		{
			cd_warning ("couldn't create directory %s", cExtractTo);
			return NULL;
		}
	}
	
	// on construit le chemin local du dossier apres son extraction.
	gchar *cLocalFileName;
	if (cRealArchiveName == NULL)
		cRealArchiveName = cArchivePath;
	gchar *str = strrchr (cRealArchiveName, '/');
	if (str != NULL)
		cLocalFileName = g_strdup (str+1);
	else
		cLocalFileName = g_strdup (cRealArchiveName);
	
	if (g_str_has_suffix (cLocalFileName, ".tar.gz"))
		cLocalFileName[strlen(cLocalFileName)-7] = '\0';
	else if (g_str_has_suffix (cLocalFileName, ".tar.bz2"))
		cLocalFileName[strlen(cLocalFileName)-8] = '\0';
	else if (g_str_has_suffix (cLocalFileName, ".tgz"))
		cLocalFileName[strlen(cLocalFileName)-4] = '\0';
	g_return_val_if_fail (cLocalFileName != NULL && *cLocalFileName != '\0', NULL);
	
	gchar *cResultPath = g_strdup_printf ("%s/%s", cExtractTo, cLocalFileName);
	g_free (cLocalFileName);
	
	// on efface un dossier identique prealable.
	if (g_file_test (cResultPath, G_FILE_TEST_EXISTS))
	{
		gchar *cCommand = g_strdup_printf ("rm -rf \"%s\"", cResultPath);
		int r = system (cCommand);
		g_free (cCommand);
	}
	
	// on decompresse l'archive.
	gchar *cCommand = g_strdup_printf ("tar xf%c \"%s\" -C \"%s\"", (g_str_has_suffix (cArchivePath, "bz2") ? 'j' : 'z'), cArchivePath, cExtractTo);
	g_print ("tar : %s\n", cCommand);
	int r = system (cCommand);
	if (r != 0)
	{
		cd_warning ("an error occured while executing '%s'", cCommand);
		g_free (cResultPath);
		cResultPath = NULL;
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
		pDialog = cairo_dock_show_temporary_dialog_with_icon_printf ("downloading file %s on %s ...", NULL, NULL, 0, NULL, cDistantFileName, cServerAdress);
		cairo_dock_dialog_reference (pDialog);
		g_print ("downloading file ...\n");
		while (gtk_events_pending ())
			gtk_main_iteration ();
	}
	//gchar *cCommand = g_strdup_printf ("%s wget \"%s/%s/%s\" -O \"%s\" -t %d -T %d%s", (iShowActivity == 2 ? "$TERM -e '" : ""), cServerAdress, cDistantFilePath, cDistantFileName, cTmpFilePath, CAIRO_DOCK_DL_NB_RETRY, CAIRO_DOCK_DL_TIMEOUT, (iShowActivity == 2 ? "'" : ""));
	gchar *cCommand = g_strdup_printf ("%s curl \"%s/%s/%s\" --output \"%s\" --connect-timeout %d --max-time %d --retry %d%s",
		(iShowActivity == 2 ? "$TERM -e '" : ""),
		cServerAdress, cDistantFilePath, cDistantFileName,
		cTmpFilePath,
		mySystem.iConnectionTimeout, mySystem.iConnectiontMaxTime, mySystem.iConnectiontNbRetries,
		(iShowActivity == 2 ? "'" : ""));
	g_print ("%s\n", cCommand);
	
	int r = system (cCommand);
	if (r != 0)
	{
		cd_warning ("an error occured while executing '%s'", cCommand);
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		cTmpFilePath = NULL;
	}
	
	gboolean bOk = (cTmpFilePath != NULL);
	if (bOk)
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

static inline int _convert_date (int iDate)
{
	int d, m, y;
	y = iDate / 10000;
	m = (iDate - y*10000) / 100;
	d = iDate % 100;
	return (d + m*30 + y*365);
}
static void _cairo_dock_parse_theme_list (GKeyFile *pKeyFile, const gchar *cServerAdress, const gchar *cDirectory, GHashTable *pThemeTable)
{
	// date courante.
	time_t epoch = (time_t) time (NULL);
	struct tm currentTime;
	localtime_r (&epoch, &currentTime);
	int day = currentTime.tm_mday;  // dans l'intervalle 1 a 31.
	int month = currentTime.tm_mon + 1;  // dans l'intervalle 0 a 11.
	int year = 1900 + currentTime.tm_year;  // tm_year = nombre d'annees écoulees depuis 1900.
	int now = day + month * 30 + year * 365;
	
	// liste des themes.
	gsize length=0;
	gchar **pGroupList = g_key_file_get_groups (pKeyFile, &length);
	g_return_if_fail (pGroupList != NULL);  // rien a charger dans la table, on quitte.
	
	// on parcourt la liste.
	gchar *cThemeName, *cDate;
	CairoDockTheme *pTheme;
	CairoDockThemeType iType;
	int iCreationDate, iLastModifDate, iLocalDate, iSobriety;
	int last_modif, creation_date;
	guint i;
	for (i = 0; i < length; i ++)
	{
		cThemeName = pGroupList[i];
		iCreationDate = g_key_file_get_integer (pKeyFile, cThemeName, "creation", NULL);
		iLastModifDate = g_key_file_get_integer (pKeyFile, cThemeName, "last modif", NULL);
		iSobriety = g_key_file_get_integer (pKeyFile, cThemeName, "sobriety", NULL);
		
		// creation < 30j && pas sur le disque -> new
		// sinon last modif < 30j && last use < last modif -> updated
		// sinon -> net
		
		// on surcharge les themes locaux en cas de nouvelle version.
		CairoDockTheme *pSameTheme = g_hash_table_lookup (pThemeTable, cThemeName);
		if (pSameTheme != NULL)  // le theme existe en local.
		{
			// on regarde de quand date cette version locale.
			gchar *cVersionFile = g_strdup_printf ("%s/last-modif", pSameTheme->cThemePath);
			gsize length = 0;
			gchar *cContent = NULL;
			g_file_get_contents (cVersionFile,
				&cContent,
				&length,
				NULL);
			if (cContent == NULL)  // le theme n'a pas encore de fichier de date
			{
				// on ne peut pas savoir quand l'utilisateur a mis a jour le theme pour la derniere fois;
				// on va supposer que l'on ne met pas a jour ses themes tres regulierement, donc il est probable qu'il l'ait fait il y'a au moins 1 mois.
				// de cette facon, les themes mis a jour recemment (il y'a moins d'1 mois) apparaitront "updated".
				if (month > 1)
					iLocalDate = day + (month - 1) * 1e2 + year * 1e4;
				else
					iLocalDate = day + 12 * 1e2 + (year - 1) * 1e4;
				cDate = g_strdup_printf ("%d", iLocalDate);
				g_file_set_contents (cVersionFile,
					cDate,
					-1,
					NULL);
				g_free (cDate);
			}
			else
				iLocalDate = atoi (cContent);
			g_free (cContent);
			
			if (iLocalDate < iLastModifDate)  // la copie locale est plus ancienne.
			{
				iType = CAIRO_DOCK_UPDATED_THEME;
			}
			else  // c'est deja la derniere version disponible, on en reste la.
			{
				g_free (cVersionFile);
				g_free (cThemeName);
				pSameTheme->iSobriety = iSobriety;  // par contre on en profite pour renseigner la sobriete.
				continue;
			}
			g_free (cVersionFile);
			
			pTheme = pSameTheme;
			g_free (pTheme->cThemePath);
			g_free (pTheme->cAuthor);
			g_free (pTheme->cDisplayedName);
		}
		else  // theme encore jamais telecharge.
		{
			last_modif = _convert_date (iLastModifDate);
			creation_date = _convert_date (iCreationDate);
			
			if (now - creation_date < 30)  // les themes restent nouveaux pendant 1 mois.
				iType = CAIRO_DOCK_NEW_THEME;
			else if (now - last_modif < 30)  // les themes restent mis a jour pendant 1 mois.
				iType = CAIRO_DOCK_UPDATED_THEME;
			else
				iType = CAIRO_DOCK_DISTANT_THEME;
			
			pTheme = g_new0 (CairoDockTheme, 1);
			g_hash_table_insert (pThemeTable, cThemeName, pTheme);
			pTheme->iRating = g_key_file_get_integer (pKeyFile, cThemeName, "rating", NULL);  // // par contre on affiche la note que l'utilisateur avait precedemment etablie.
		}
		
		pTheme->cThemePath = g_strdup_printf ("%s/%s/%s", cServerAdress, cDirectory, cThemeName);
		pTheme->iType = iType;
		pTheme->fSize = g_key_file_get_double (pKeyFile, cThemeName, "size", NULL);
		pTheme->cAuthor = g_key_file_get_string (pKeyFile, cThemeName, "author", NULL);
		if (pTheme->cAuthor && *pTheme->cAuthor == '\0')
		{
			g_free (pTheme->cAuthor);
			pTheme->cAuthor = NULL;
		}
		pTheme->iSobriety = iSobriety;
		pTheme->iCreationDate = iCreationDate;
		pTheme->iLastModifDate = iLastModifDate;
		pTheme->cDisplayedName = g_strdup_printf ("%s by %s [%.2f MB]", cThemeName, (pTheme->cAuthor ? pTheme->cAuthor : "---"), pTheme->fSize);
	}
	g_free (pGroupList);  // les noms des themes sont desormais dans la hash-table.
}

GHashTable *cairo_dock_list_net_themes (const gchar *cServerAdress, const gchar *cDirectory, const gchar *cListFileName, GHashTable *hProvidedTable, GError **erreur)
{
	g_return_val_if_fail (cServerAdress != NULL && *cServerAdress != '\0', hProvidedTable);
	cd_message ("listing net themes on %s/%s ...", cServerAdress, cDirectory);
	
	// On recupere la liste des themes distants.
	GError *tmp_erreur = NULL;
	gchar *cContent = cairo_dock_get_distant_file_content (cServerAdress, cDirectory, cListFileName, 0, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		cd_warning ("couldn't retrieve themes on %s (check that your connection is alive, or retry later)", cServerAdress);
		g_propagate_error (erreur, tmp_erreur);
		return hProvidedTable;
	}
	
	// on verifie son integrite.
	if (cContent == NULL || strncmp (cContent, "#!CD", 4) != 0)  // avec une connexion wifi etablie sur un operateur auquel on ne s'est pas logue, il peut nous renvoyer des messages au lieu de juste rien. On filtre ca par un entete dedie.
	{
		cd_warning ("empty themes list on %s (check that your connection is alive, or retry later)", cServerAdress);
		g_set_error (erreur, 1, 1, "empty themes list on %s", cServerAdress);
		g_free (cContent);
		return hProvidedTable;
	}
	
	// petit bonus : on a la derniere version du dock sur la 1ere ligne.
	if (cContent[4] != '\n')
	{
		int iMajorVersion=0, iMinorVersion=0, iMicroVersion=0;
		cairo_dock_get_version_from_string (cContent+4, &iMajorVersion, &iMinorVersion, &iMicroVersion);
		g_print ("%d/%d/%d\n", iMajorVersion, iMinorVersion, iMicroVersion);
		if (iMajorVersion > g_iMajorVersion ||
			(iMajorVersion == g_iMajorVersion &&
				(iMinorVersion > g_iMinorVersion ||
					(iMinorVersion == g_iMinorVersion && iMicroVersion > g_iMicroVersion))))
		{
			g_print ("A new version is available !\n>>>\n%d.%d.%d\n<<<\n", iMajorVersion, iMinorVersion, iMicroVersion);
		}
	}
	
	// on charge la liste dans un fichier de cles.
	GKeyFile *pKeyFile = g_key_file_new ();
	g_key_file_load_from_data (pKeyFile,
		cContent,
		-1,
		G_KEY_FILE_NONE,
		&tmp_erreur);
	g_free (cContent);
	if (tmp_erreur != NULL)
	{
		cd_warning ("invalid list of themes (%s)\n(check that your connection is alive, or retry later)", cServerAdress);
		g_propagate_error (erreur, tmp_erreur);
		g_key_file_free (pKeyFile);
		return hProvidedTable;
	}
	
	// on parse la liste dans une table de hashage.
	GHashTable *pThemeTable = (hProvidedTable != NULL ? hProvidedTable : g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_theme));
	
	_cairo_dock_parse_theme_list (pKeyFile, cServerAdress, cDirectory, pThemeTable);
	
	g_key_file_free (pKeyFile);
	
	return pThemeTable;
}

GHashTable *cairo_dock_list_themes (const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir)
{
	cd_message ("%s (%s, %s, %s)", __func__, cShareThemesDir, cUserThemesDir, cDistantThemesDir);
	GError *erreur = NULL;
	GHashTable *pThemeTable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_theme);
	
	//\______________ On recupere les themes pre-installes.
	if (cShareThemesDir != NULL)
		pThemeTable = cairo_dock_list_local_themes (cShareThemesDir, pThemeTable, cDistantThemesDir != NULL, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while loading pre-installed themes in '%s' : %s", cShareThemesDir, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	
	//\______________ On recupere les themes utilisateurs (qui ecrasent donc les precedents).
	if (cUserThemesDir != NULL)
		pThemeTable = cairo_dock_list_local_themes (cUserThemesDir, pThemeTable, cDistantThemesDir != NULL, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while loading user themes in '%s' : %s", cShareThemesDir, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	
	//\______________ On recupere les themes distants (qui surchargent tous les themes).
	if (cDistantThemesDir != NULL)
		pThemeTable = cairo_dock_list_net_themes (g_cThemeServerAdress != NULL ? g_cThemeServerAdress : CAIRO_DOCK_THEME_SERVER, cDistantThemesDir, CAIRO_DOCK_DEFAULT_THEME_LIST_NAME, pThemeTable, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while loading distant themes in '%s/%s' : %s", g_cThemeServerAdress != NULL ? g_cThemeServerAdress : CAIRO_DOCK_THEME_SERVER, cDistantThemesDir, erreur->message);
		//g_print ("s_pThemeManager:%x\n", s_pThemeManager);
		cairo_dock_set_status_message_printf (s_pThemeManager, _("couldn't get the list of themes for %s (no connection ?)"), cDistantThemesDir);
		g_error_free (erreur);
		erreur = NULL;
	}
	
	return pThemeTable;
}

gchar *cairo_dock_get_theme_path (const gchar *cThemeName, const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir, CairoDockThemeType iGivenType)
{
	cd_message ("%s (%s, %s, %s)", __func__, cShareThemesDir, cUserThemesDir, cDistantThemesDir);
	if (cThemeName == NULL || *cThemeName == '\0')
		return NULL;
	CairoDockThemeType iType = cairo_dock_extract_theme_type_from_name (cThemeName);
	if (iType == CAIRO_DOCK_ANY_THEME)
		iType = iGivenType;
	gchar *cThemePath = NULL;
	
	//g_print ("iType : %d\n", iType);
	if (cUserThemesDir != NULL && /*iType != CAIRO_DOCK_DISTANT_THEME && iType != CAIRO_DOCK_NEW_THEME && */iType != CAIRO_DOCK_UPDATED_THEME)
	{
		cThemePath = g_strdup_printf ("%s/%s", cUserThemesDir, cThemeName);
		
		if (g_file_test (cThemePath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
		{
			return cThemePath;
		}
		
		g_free (cThemePath);
		cThemePath = NULL;
	}
	
	if (cShareThemesDir != NULL && /*iType != CAIRO_DOCK_DISTANT_THEME && iType != CAIRO_DOCK_NEW_THEME && */iType != CAIRO_DOCK_UPDATED_THEME)
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
		else  // on se souvient de la date a laquelle on a mis a jour le theme pour la derniere fois.
		{
			gchar *cVersionFile = g_strdup_printf ("%s/last-modif", cThemePath);
			time_t epoch = (time_t) time (NULL);
			struct tm currentTime;
			localtime_r (&epoch, &currentTime);
			int now = (currentTime.tm_mday+1) + (currentTime.tm_mon+1) * 1e2 + (1900+currentTime.tm_year) * 1e4;
			gchar *cDate = g_strdup_printf ("%d", now);
			g_file_set_contents (cVersionFile,
				cDate,
				-1,
				NULL);
			g_free (cDate);
			g_free (cVersionFile);
		}
	}
	
	g_print (" ====> cThemePath : %s\n", cThemePath);
	return cThemePath;
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

CairoDockThemeType cairo_dock_extract_theme_type_from_name (const gchar *cThemeName)
{
	if (cThemeName == NULL)
		return CAIRO_DOCK_ANY_THEME;
	CairoDockThemeType iType = CAIRO_DOCK_ANY_THEME;
	int l = strlen (cThemeName);
	if (cThemeName[l-1] == ']')
	{
		gchar *str = strrchr (cThemeName, '[');
		if (str != NULL && g_ascii_isdigit (*(str+1)))
		{
			iType = atoi (str+1);
			*str = '\0';
		}
	}
	return iType;
}

gboolean cairo_dock_export_current_theme (const gchar *cNewThemeName, gboolean bSaveBehavior, gboolean bSaveLaunchers)
{
	g_return_val_if_fail (cNewThemeName != NULL, FALSE);
	
	cairo_dock_extract_theme_type_from_name (cNewThemeName);
	
	cd_message ("on sauvegarde dans %s", cNewThemeName);
	GString *sCommand = g_string_new ("");
	gboolean bThemeSaved = FALSE;
	int r;
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
			if (bSaveBehavior)
			{
				g_string_printf (sCommand, "/bin/cp \"%s\" \"%s\"", g_cConfFile, cNewConfFilePath);
				cd_message ("%s", sCommand->str);
				r = system (sCommand->str);
			}
			else
			{
				cairo_dock_replace_keys_by_identifier (cNewConfFilePath, g_cConfFile, '+');
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
			g_string_printf (sCommand, "find \"%s\" -mindepth 1 -maxdepth 1  ! -name '*.conf' ! -name \"%s\" -exec /bin/cp -r '{}' \"%s\" \\;", g_cCurrentThemePath, CAIRO_DOCK_LAUNCHERS_DIR, cNewThemePath);
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
	
	g_free (cNewThemePath);
	if (bThemeSaved)
	{
		cairo_dock_mark_theme_as_modified (FALSE);
	}
	
	g_string_free (sCommand, TRUE);
	
	cairo_dock_set_status_message (s_pThemeManager, bThemeSaved ? _("The theme has been saved") :  _("The theme couldn't be saved"));
	return bThemeSaved;
}

gboolean cairo_dock_package_current_theme (const gchar *cThemeName)
{
	g_return_val_if_fail (cThemeName != NULL, FALSE);
	
	cairo_dock_extract_theme_type_from_name (cThemeName);
	
	cd_message ("building theme package ...");
	int r;
	if (g_file_test (CAIRO_DOCK_SHARE_DATA_DIR"/../../bin/cairo-dock-package-theme", G_FILE_TEST_EXISTS))
	{
		gchar *cCommand;
		const gchar *cTerm = g_getenv ("TERM");
		if (cTerm == NULL || *cTerm == '\0')
			cCommand = g_strdup_printf ("%s \"%s\"", "cairo-dock-package-theme", cThemeName);
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
	gchar *cUserThemesDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR);
	gchar *cNewThemePath = NULL;
	if (*cPackagePath == '/' || strncmp (cPackagePath, "file://", 7) == 0)  // paquet en local.
	{
		g_print (" paquet local\n");
		//cairo_dock_remove_html_spaces (cPackagePath);
		gchar *cFilePath = (*cPackagePath == '/' ? g_strdup (cPackagePath) : g_filename_from_uri (cPackagePath, NULL, NULL));
		cNewThemePath = cairo_dock_uncompress_file (cFilePath, cUserThemesDir, NULL);
		g_free (cFilePath);
	}
	else  // paquet distant.
	{
		g_print (" paquet distant\n");
		gchar *str = strrchr (cPackagePath, '/');
		if (str != NULL)
		{
			*str = '\0';
			cNewThemePath = cairo_dock_download_file (cPackagePath, "", str+1, 2, cUserThemesDir, NULL);
			if (cNewThemePath == NULL)
			{
				cairo_dock_show_temporary_dialog_with_icon_printf (_("couldn't get distant file %s/%s, maybe the server is down.\nPlease retry later or contact us at cairo-dock.org."), NULL, NULL, 0, NULL, cPackagePath, str+1);
			}
		}
	}
	g_free (cUserThemesDir);
	return cNewThemePath;
}

gboolean cairo_dock_delete_themes (gchar **cThemesList)
{
	g_return_val_if_fail (cThemesList != NULL && cThemesList[0] != NULL, FALSE);
	
	GString *sCommand = g_string_new ("");
	gboolean bThemeDeleted = FALSE;
	
	if (cThemesList[1] == NULL)
		g_string_printf (sCommand, _("Are you sure you want to delete theme %s ?"), cThemesList[0]);
	else
		g_string_printf (sCommand, _("Are you sure you want to delete these themes ?"));
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
			cairo_dock_extract_theme_type_from_name (cThemeName);
			
			bThemeDeleted = TRUE;
			g_string_printf (sCommand, "rm -rf \"%s/%s/%s\"", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR, cThemeName);
			r = system (sCommand->str);  // g_rmdir n'efface qu'un repertoire vide.
		}
	}
	if (cThemesList[1] == NULL)
		cairo_dock_set_status_message (s_pThemeManager, bThemeDeleted ? _("The theme has been deleted") :  _("The theme couldn't be deleted"));
	else
		cairo_dock_set_status_message (s_pThemeManager, bThemeDeleted ? _("The themes have been deleted") :  _("The themes couldn't be deleted"));
	
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
	//\___________________ On regarde si le theme courant est modifie.
	gboolean bNeedSave = cairo_dock_theme_need_save ();
	if (bNeedSave)
	{
		int iAnswer = cairo_dock_ask_general_question_and_wait (_("You made some modifications in the current theme.\nYou will loose them if you don't save before choosing a new theme. Continue anyway ?"));
		if (iAnswer != GTK_RESPONSE_YES)
		{
			return FALSE;
		}
		cairo_dock_mark_theme_as_modified (FALSE);
	}
	
	//\___________________ On obtient le chemin du nouveau theme (telecharge ou decompresse si necessaire).
	gchar *cNewThemeName = g_strdup (cThemeName);
	gchar *cNewThemePath = NULL;
	
	int length = strlen (cNewThemeName);
	if (cNewThemeName[length-1] == '\n')
		cNewThemeName[--length] = '\0';  // on vire le retour chariot final.
	if (cNewThemeName[length-1] == '\r')
		cNewThemeName[--length] = '\0';
	g_print ("cNewThemeName : '%s'\n", cNewThemeName);
	
	if (g_str_has_suffix (cNewThemeName, ".tar.gz") || g_str_has_suffix (cNewThemeName, ".tar.bz2") || g_str_has_suffix (cNewThemeName, ".tgz"))  // c'est un paquet.
	{
		g_print ("c'est un paquet\n");
		cNewThemePath = cairo_dock_depackage_theme (cNewThemeName);
		
		g_return_val_if_fail (cNewThemePath != NULL, FALSE);
		gchar *tmp = cNewThemeName;
		cNewThemeName = g_path_get_basename (cNewThemePath);
		g_free (tmp);
	}
	else  // c'est un theme officiel.
	{
		g_print ("c'est un theme officiel\n");
		gchar *cUserThemesDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR);
		cNewThemePath = cairo_dock_get_theme_path (cNewThemeName, CAIRO_DOCK_SHARE_THEMES_DIR, cUserThemesDir, CAIRO_DOCK_THEMES_DIR, CAIRO_DOCK_ANY_THEME);
		g_free (cUserThemesDir);
	}
	g_return_val_if_fail (cNewThemePath != NULL && g_file_test (cNewThemePath, G_FILE_TEST_EXISTS), FALSE);
	g_print ("cNewThemePath : %s ; cNewThemeName : %s\n", cNewThemePath, cNewThemeName);
	
	//\___________________ On charge les parametres de comportement.
	GString *sCommand = g_string_new ("");
	int r;
	cairo_dock_set_status_message (s_pThemeManager, _("Applying changes ..."));
	if (g_pMainDock == NULL || bLoadBehavior)
	{
		g_string_printf (sCommand, "/bin/cp \"%s\"/%s \"%s\"", cNewThemePath, CAIRO_DOCK_CONF_FILE, g_cCurrentThemePath);
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
	g_string_printf (sCommand, "find \"%s\" -mindepth 1 -maxdepth 1 -name '*.conf' ! -name '%s' -exec /bin/cp '{}' \"%s\" \\;", cNewThemePath, CAIRO_DOCK_CONF_FILE, g_cCurrentThemePath);
	cd_message ("%s", sCommand->str);
	r = system (sCommand->str);
	
	//\___________________ On charge les lanceurs.
	if (bLoadLaunchers)
	{
		g_string_printf (sCommand, "rm -f \"%s/%s\"/*", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
		cd_message ("%s", sCommand->str);
		r = system (sCommand->str);
		g_string_printf (sCommand, "rm -f \"%s/%s\"/.*", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
		cd_message ("%s", sCommand->str);
		r = system (sCommand->str);
	}
	
	//\___________________ On charge les icones.
	gchar *cNewLocalIconsPath = g_strdup_printf ("%s/%s", cNewThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
	if (! g_file_test (cNewLocalIconsPath, G_FILE_TEST_IS_DIR))  // c'est un ancien theme, on deplace les icones vers le repertoire 'icons'.
	{
		g_string_printf (sCommand, "find \"%s/%s\" -mindepth 1 ! -name '*.desktop' -exec /bin/cp '{}' '%s/%s' \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
	}
	else
	{
		g_string_printf (sCommand, "for f in \"%s\"/* ; do rm -f \"%s/%s/`basename \"${f%%.*}\"`\"*; done;", cNewLocalIconsPath, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);  // on efface les doublons car sinon on pourrait avoir x.png et x.svg ensemble et le dock ne saurait pas lequel choisir.
		cd_message ("%s", sCommand->str);
		r = system (sCommand->str);
		
		g_string_printf (sCommand, "cp \"%s\"/* \"%s/%s\"", cNewLocalIconsPath, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
	}
	cd_message ("%s", sCommand->str);
	r = system (sCommand->str);
	g_free (cNewLocalIconsPath);
	
	//\___________________ On charge les extras.
	g_string_printf (sCommand, "%s/%s", cNewThemePath, CAIRO_DOCK_EXTRAS_DIR);
	if (g_file_test (sCommand->str, G_FILE_TEST_IS_DIR))
	{
		g_string_printf (sCommand, "cp -r \"%s/%s\"/* \"%s/%s\"", cNewThemePath, CAIRO_DOCK_EXTRAS_DIR, g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR);
		cd_message ("%s", sCommand->str);
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
		cd_message ("%s", sCommand->str);
		r = system (sCommand->str);

		g_string_printf (sCommand, "cp \"%s/%s\"/*.desktop \"%s\"", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentLaunchersPath);
		cd_message ("%s", sCommand->str);
		r = system (sCommand->str);
	}
	
	//\___________________ On remplace tous les autres fichiers par les nouveaux.
	g_string_printf (sCommand, "find \"%s\" -mindepth 1 -maxdepth 1  ! -name '*.conf' -type f -exec rm -f '{}' \\;", g_cCurrentThemePath);  // efface tous les fichiers du theme mais sans toucher aux lanceurs et aux plug-ins.
	cd_message (sCommand->str);
	r = system (sCommand->str);

	if (g_pMainDock == NULL || bLoadBehavior)
	{
		g_string_printf (sCommand, "find \"%s\"/* -prune ! -name '*.conf' ! -name %s -exec /bin/cp -r '{}' \"%s\" \\;", cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);  // copie tous les fichiers du nouveau theme sauf les lanceurs et le .conf, dans le repertoire du theme courant. Ecrase les fichiers de memes noms.
		cd_message (sCommand->str);
		r = system (sCommand->str);
	}
	else
	{
		// on copie tous les fichiers du nouveau theme sauf les lanceurs et les .conf (dock et plug-ins).
		g_string_printf (sCommand, "find \"%s\" -mindepth 1 ! -name '*.conf' ! -path '%s/%s*' ! -type d -exec cp -p {} \"%s\" \\;", cNewThemePath, cNewThemePath, CAIRO_DOCK_LAUNCHERS_DIR, g_cCurrentThemePath);
		cd_message (sCommand->str);
		r = system (sCommand->str);
		
		// on parcours les .conf des plug-ins, on les met a jour, et on fusionne avec le theme courant.
		gchar *cNewPlugInsDir = g_strdup_printf ("%s/%s", cNewThemePath, "plug-ins");  // repertoire des plug-ins du nouveau theme.
		GDir *dir = g_dir_open (cNewPlugInsDir, 0, NULL);  // NULL si ce theme n'a pas de repertoire 'plug-ins'.
		const gchar* cModuleDirName;
		gchar *cConfFilePath, *cNewConfFilePath, *cUserDataDirPath, *cConfFileName;
		do
		{
			cModuleDirName = g_dir_read_name (dir);  // nom du repertoire du theme (pas forcement egal au nom du theme)
			if (cModuleDirName == NULL)
				break ;
			
			// on cree le repertoire du plug-in dans le theme courant.
			cd_debug ("  installing %s's config\n", cModuleDirName);
			cUserDataDirPath = g_strdup_printf ("%s/plug-ins/%s", g_cCurrentThemePath, cModuleDirName);  // repertoire du plug-in dans le theme courant.
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
				g_string_printf (sCommand, "cp \"%s\" \"%s\"", cNewConfFilePath, cConfFilePath);
				r = system (sCommand->str);
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
	
	// precaution probablement inutile.
	g_string_printf (sCommand, "chmod -R 777 \"%s\"", g_cCurrentThemePath);
	r = system (sCommand->str);
	
	g_string_free (sCommand, TRUE);
	return TRUE;
}



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

static gboolean on_theme_apply (gchar *cInitConfFile)
{
	g_print ("%s (%s)\n", __func__, cInitConfFile);
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
		
		g_free (cNewThemeName);
		g_key_file_free (pKeyFile);
		return TRUE;
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
		
		g_strfreev (cThemesList);
		g_key_file_free (pKeyFile);
		return TRUE;
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
		
		gboolean bThemeImported = cairo_dock_import_theme (cNewThemeName, bLoadBehavior, bLoadLaunchers);
		
		//\___________________ On charge le theme courant.
		if (bThemeImported)
		{
			cairo_dock_set_status_message (s_pThemeManager, _("Now reloading theme ..."));
			cairo_dock_load_current_theme ();
		}
		
		g_free (cNewThemeName);
		cairo_dock_set_status_message (s_pThemeManager, "");
		return FALSE;
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
	
	cairo_dock_build_normal_gui (cInitConfFile,
		NULL, cTitle,
		CAIRO_DOCK_THEME_PANEL_WIDTH, CAIRO_DOCK_THEME_PANEL_HEIGHT,
		(CairoDockApplyConfigFunc) on_theme_apply,
		cInitConfFile,
		(GFreeFunc) on_theme_destroy,
		&s_pThemeManager);
}


void cairo_dock_load_current_theme (void)
{
	cd_message ("%s ()", __func__);

	//\___________________ On libere toute la memoire allouee pour les docks (stoppe aussi tous les timeout).
	cairo_dock_free_all_docks ();

	//\___________________ On cree le dock principal.
	g_pMainDock = cairo_dock_create_new_dock (CAIRO_DOCK_MAIN_DOCK_NAME, NULL);  // on ne lui assigne pas de vues, puisque la vue par defaut des docks principaux sera definie plus tard.

	//\___________________ On lit son fichier de conf et on charge tout.
	cairo_dock_read_conf_file (g_cConfFile, g_pMainDock);  // chargera des valeurs par defaut si le fichier de conf fourni est incorrect.
}

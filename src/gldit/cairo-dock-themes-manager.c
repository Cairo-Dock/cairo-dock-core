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
#include <curl/curl.h>

#include "../../config.h"
#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-task.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-themes-manager.h"

#define CAIRO_DOCK_MODIFIED_THEME_FILE ".cairo-dock-need-save"
#define CAIRO_DOCK_DEFAULT_THEME_LIST_NAME "list.conf"

static gchar *s_cThemeServerAdress = NULL;

gchar *g_cCairoDockDataDir = NULL;  // le repertoire racine contenant tout.
gchar *g_cCurrentThemePath = NULL;  // le chemin vers le repertoire du theme courant.
gchar *g_cExtrasDirPath = NULL;  // le chemin vers le repertoire des extra.
gchar *g_cThemesDirPath = NULL;  // le chemin vers le repertoire des themes.
gchar *g_cCurrentLaunchersPath = NULL;  // le chemin vers le repertoire des lanceurs du theme courant.
gchar *g_cCurrentIconsPath = NULL;  // le chemin vers le repertoire des icones du theme courant.
gchar *g_cCurrentPlugInsPath = NULL;  // le chemin vers le repertoire des plug-ins du theme courant.
gchar *g_cConfFile = NULL;  // le chemin du fichier de conf.
int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;  // version de la lib.

extern CairoDock *g_pMainDock;


void cairo_dock_free_theme (CairoDockTheme *pTheme)
{
	if (pTheme == NULL)
		return ;
	g_free (pTheme->cThemePath);
	g_free (pTheme->cAuthor);
	g_free (pTheme->cDisplayedName);
	g_free (pTheme);
}


gchar *cairo_dock_uncompress_file (const gchar *cArchivePath, const gchar *cExtractTo, const gchar *cRealArchiveName)
{
	//\_______________ on cree le repertoire d'extraction.
	if (!g_file_test (cExtractTo, G_FILE_TEST_EXISTS))
	{
		if (g_mkdir (cExtractTo, 7*8*8+7*8+5) != 0)
		{
			cd_warning ("couldn't create directory %s", cExtractTo);
			return NULL;
		}
	}
	
	//\_______________ on construit le chemin local du dossier apres son extraction.
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
	
	//\_______________ on efface un dossier identique prealable.
	if (g_file_test (cResultPath, G_FILE_TEST_EXISTS))
	{
		gchar *cCommand = g_strdup_printf ("rm -rf \"%s\"", cResultPath);
		int r = system (cCommand);
		g_free (cCommand);
	}
	
	//\_______________ on decompresse l'archive.
	gchar *cCommand = g_strdup_printf ("tar xf%c \"%s\" -C \"%s\"", (g_str_has_suffix (cArchivePath, "bz2") ? 'j' : 'z'), cArchivePath, cExtractTo);
	cd_debug ("tar : %s\n", cCommand);
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

static inline CURL *_init_curl_connection (const gchar *cURL)
{
	CURL *handle = curl_easy_init ();
	curl_easy_setopt (handle, CURLOPT_URL, cURL);
	if (mySystem.cConnectionProxy != NULL)
	{
		curl_easy_setopt (handle, CURLOPT_PROXY, mySystem.cConnectionProxy);
		if (mySystem.iConnectionPort != 0)
			curl_easy_setopt (handle, CURLOPT_PROXYPORT, mySystem.iConnectionPort);
		if (mySystem.cConnectionUser != NULL && mySystem.
			cConnectionPasswd != NULL)
		{
			gchar *cUserPwd = g_strdup_printf ("%s:%s", mySystem.cConnectionUser, mySystem.
			cConnectionPasswd);
			curl_easy_setopt (handle, CURLOPT_PROXYUSERPWD, cUserPwd);
			g_free (cUserPwd);
			/*curl_easy_setopt (handle, CURLOPT_PROXYUSERNAME, mySystem.cConnectionUser);
			if (mySystem.cConnectionPasswd != NULL)
				curl_easy_setopt (handle, CURLOPT_PROXYPASSWORD, mySystem.cConnectionPasswd);*/  // a partir de libcurl 7.19.1, donc apres Jaunty
		}
	}
	curl_easy_setopt (handle, CURLOPT_TIMEOUT, mySystem.iConnectionMaxTime);
	curl_easy_setopt (handle, CURLOPT_CONNECTTIMEOUT, mySystem.iConnectionTimeout);
	curl_easy_setopt (handle, CURLOPT_NOSIGNAL, 1);  // With CURLOPT_NOSIGNAL set non-zero, curl will not use any signals; sinon curl se vautre apres le timeout, meme si le download s'est bien passe !
	return handle;
}

static size_t _write_data_to_file (gpointer buffer, size_t size, size_t nmemb, FILE *fd)
{
	return fwrite (buffer, size, nmemb, fd);
}
gchar *cairo_dock_download_file (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, const gchar *cExtractTo, GError **erreur)
{
	//g_print ("%s (%s, %s, %s, %s)\n", __func__, cServerAdress, cDistantFilePath, cDistantFileName, cExtractTo);
	//\_______________ On reserve un fichier temporaire.
	gchar *cTmpFilePath = g_strdup ("/tmp/cairo-dock-net-file.XXXXXX");
	int fds = mkstemp (cTmpFilePath);
	if (fds == -1)
	{
		g_set_error (erreur, 1, 1, "couldn't create temporary file '%s'", cTmpFilePath);
		g_free (cTmpFilePath);
		return NULL;
	}
	
	//\_______________ On lance le download.
	gchar *cURL = (cServerAdress ? g_strdup_printf ("%s/%s/%s", cServerAdress, cDistantFilePath, cDistantFileName) : g_strdup (cDistantFileName));
	cd_debug ("cURL : %s\n", cURL);
	FILE *f = fopen (cTmpFilePath, "wb");
	g_return_val_if_fail (f, NULL);
	
	CURL *handle = _init_curl_connection (cURL);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, _write_data_to_file);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, f);
	
	CURLcode r = curl_easy_perform (handle);
	
	if (r != CURLE_OK)
	{
		cd_warning ("an error occured while downloading '%s'", cURL);
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		cTmpFilePath = NULL;
	}
	
	curl_easy_cleanup (handle);
	g_free (cURL);
	fclose (f);
	/**gchar *cCommand = g_strdup_printf ("curl -s \"%s/%s/%s\" --output \"%s\" --connect-timeout %d --max-time %d --retry %d",
		cServerAdress, cDistantFilePath, cDistantFileName,
		cTmpFilePath,
		mySystem.iConnectionTimeout, mySystem.iConnectiontMaxTime, mySystem.iConnectiontNbRetries);
	cd_debug ("download with '%s'", cCommand);
	int r = system (cCommand);
	if (r != 0)
	{
		cd_warning ("an error occured while executing '%s'", cCommand);
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		cTmpFilePath = NULL;
	}
	g_free (cCommand);*/
	
	//\_______________ On teste que le fichier est non vide.
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
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		cTmpFilePath = NULL;
	}
	close(fds);
	
	//\_______________ On l'extrait si c'est une archive.
	if (cTmpFilePath != NULL && cExtractTo != NULL)
	{
		cd_debug ("uncompressing ...\n");
		gchar *cPath = cairo_dock_uncompress_file (cTmpFilePath, cExtractTo, cDistantFileName);
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		cTmpFilePath = cPath;
	}
	
	return cTmpFilePath;
}

static void _dl_file (gpointer *pSharedMemory)
{
	GError *erreur = NULL;
	pSharedMemory[6] = cairo_dock_download_file (pSharedMemory[0], pSharedMemory[1], pSharedMemory[2], pSharedMemory[3], &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
}
static gboolean _finish_dl (gpointer *pSharedMemory)
{
	if (pSharedMemory[6] == NULL)
		cd_warning ("couldn't get distant file %s", pSharedMemory[2]);
	
	GFunc pCallback = pSharedMemory[4];
	pCallback (pSharedMemory[6], pSharedMemory[5]);
	return FALSE;
}
static void _discard_dl (gpointer *pSharedMemory)
{
	g_free (pSharedMemory[0]);
	g_free (pSharedMemory[1]);
	g_free (pSharedMemory[2]);
	g_free (pSharedMemory[3]);
	g_free (pSharedMemory[6]);
	g_free (pSharedMemory);
}
CairoDockTask *cairo_dock_download_file_async (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, const gchar *cExtractTo, GFunc pCallback, gpointer data)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 7);
	pSharedMemory[0] = g_strdup (cServerAdress);
	pSharedMemory[1] = g_strdup (cDistantFilePath);
	pSharedMemory[2] = g_strdup (cDistantFileName);
	pSharedMemory[3] = g_strdup (cExtractTo);
	pSharedMemory[4] = pCallback;
	pSharedMemory[5] = data;
	CairoDockTask *pTask = cairo_dock_new_task_full (0, (CairoDockGetDataAsyncFunc) _dl_file, (CairoDockUpdateSyncFunc) _finish_dl, (GFreeFunc) _discard_dl, pSharedMemory);
	cairo_dock_launch_task (pTask);
	return pTask;
}


gchar *cairo_dock_get_distant_file_content (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, GError **erreur)
{
	gchar *cURL = g_strdup_printf ("%s/%s/%s", cServerAdress, cDistantFilePath, cDistantFileName);
	gchar *cContent = cairo_dock_get_url_data (cURL, erreur);
	g_free (cURL);
	return cContent;
}

static size_t _write_data_to_buffer (gpointer buffer, size_t size, size_t nmemb, GList **list_ptr)
{
	GList *pList = *list_ptr;
	*list_ptr = g_list_prepend (*list_ptr, g_strndup (buffer, size * nmemb));
	return size * nmemb;
}
gchar *cairo_dock_get_url_data (const gchar *cURL, GError **erreur)
{
	//\_______________ On lance le download.
	cd_debug ("getting data from '%s' ...", cURL);
	CURL *handle = _init_curl_connection (cURL);
	curl_easy_setopt (handle, CURLOPT_WRITEFUNCTION, (curl_write_callback)_write_data_to_buffer);
	gpointer *pointer_to_list = g_new0 (gpointer, 1);
	curl_easy_setopt (handle, CURLOPT_WRITEDATA, pointer_to_list);
	
	CURLcode r = curl_easy_perform (handle);
	
	if (r != CURLE_OK)
	{
		cd_warning ("an error occured while downloading '%s' : %s", cURL, curl_easy_strerror (r));
		g_free (pointer_to_list);
		pointer_to_list = NULL;
	}
	
	curl_easy_cleanup (handle);
	
	//\_______________ On recupere les donnees.
	gchar *cContent = NULL;
	if (pointer_to_list != NULL)
	{
		GList *l, *pList = *pointer_to_list;
		int n = 0;
		for (l = pList; l != NULL; l = l->next)
		{
			if (l->data != NULL)
				n += strlen (l->data);
		}
		
		if (n != 0)
		{
			cContent = g_new0 (char, n+1);
			char *ptr = cContent;
			for (l = g_list_last (pList); l != NULL; l = l->prev)
			{
				if (l->data != NULL)
				{
					n = strlen (l->data);
					memcpy (ptr, l->data, n);
					ptr += n;
					g_free (l->data);
				}
			}
		}
		g_list_free (pList);
		g_free (pointer_to_list);
	}
	
	return cContent;
}


static void _dl_file_readme (gpointer *pSharedMemory)
{
	GError *erreur = NULL;
	pSharedMemory[5] = cairo_dock_get_distant_file_content (pSharedMemory[0], pSharedMemory[1], pSharedMemory[2], &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
}
static gboolean _finish_dl_readme (gpointer *pSharedMemory)
{
	if (pSharedMemory[5] == NULL)
		cd_warning ("couldn't get distant file %s", pSharedMemory[2]);
	
	GFunc pCallback = pSharedMemory[3];
	pCallback (pSharedMemory[5], pSharedMemory[4]);
	return FALSE;
}
static void _discard_dl_readme (gpointer *pSharedMemory)
{
	g_free (pSharedMemory[0]);
	g_free (pSharedMemory[1]);
	g_free (pSharedMemory[2]);
	g_free (pSharedMemory[5]);
	g_free (pSharedMemory);
}
CairoDockTask *cairo_dock_get_distant_file_content_async (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, GFunc pCallback, gpointer data)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 6);
	pSharedMemory[0] = g_strdup (cServerAdress);
	pSharedMemory[1] = g_strdup (cDistantFilePath);
	pSharedMemory[2] = g_strdup (cDistantFileName);
	pSharedMemory[3] = pCallback;
	pSharedMemory[4] = data;
	CairoDockTask *pTask = cairo_dock_new_task_full (0, (CairoDockGetDataAsyncFunc) _dl_file_readme, (CairoDockUpdateSyncFunc) _finish_dl_readme, (GFreeFunc) _discard_dl_readme, pSharedMemory);
	cairo_dock_launch_task (pTask);
	return pTask;
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
	cd_debug ("%s (%s)", __func__, cThemesDir);
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
	gchar *cContent = cairo_dock_get_distant_file_content (cServerAdress, cDirectory, cListFileName, &tmp_erreur);
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
		cd_debug ("%d/%d/%d\n", iMajorVersion, iMinorVersion, iMicroVersion);
		if (iMajorVersion > g_iMajorVersion ||
			(iMajorVersion == g_iMajorVersion &&
				(iMinorVersion > g_iMinorVersion ||
					(iMinorVersion == g_iMinorVersion && iMicroVersion > g_iMicroVersion))))
		{
			cd_debug ("A new version is available !\n>>>\n%d.%d.%d\n<<<\n", iMajorVersion, iMinorVersion, iMicroVersion);
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
	if (cDistantThemesDir != NULL && s_cThemeServerAdress)
	{
		pThemeTable = cairo_dock_list_net_themes (s_cThemeServerAdress, cDistantThemesDir, CAIRO_DOCK_DEFAULT_THEME_LIST_NAME, pThemeTable, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("while loading distant themes in '%s/%s' : %s", s_cThemeServerAdress, cDistantThemesDir, erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
	}
	
	return pThemeTable;
}

static void _list_themes (gpointer *pSharedMemory)
{
	pSharedMemory[5] = cairo_dock_list_themes (pSharedMemory[0], pSharedMemory[1], pSharedMemory[2]);
}
static gboolean _finish_list_themes (gpointer *pSharedMemory)
{
	if (pSharedMemory[5] == NULL)
		cd_warning ("couldn't get distant themes in '%s'", pSharedMemory[2]);
	
	GFunc pCallback = pSharedMemory[3];
	pCallback (pSharedMemory[5], pSharedMemory[4]);
	
	return FALSE;
}
static void _discard_list_themes (gpointer *pSharedMemory)
{
	g_free (pSharedMemory[0]);
	g_free (pSharedMemory[1]);
	g_free (pSharedMemory[2]);
	g_hash_table_unref (pSharedMemory[5]);
	g_free (pSharedMemory);
}
CairoDockTask *cairo_dock_list_themes_async (const gchar *cShareThemesDir, const gchar *cUserThemesDir, const gchar *cDistantThemesDir, GFunc pCallback, gpointer data)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 6);
	pSharedMemory[0] = g_strdup (cShareThemesDir);
	pSharedMemory[1] = g_strdup (cUserThemesDir);
	pSharedMemory[2] = g_strdup (cDistantThemesDir);
	pSharedMemory[3] = pCallback;
	pSharedMemory[4] = data;
	CairoDockTask *pTask = cairo_dock_new_task_full (0, (CairoDockGetDataAsyncFunc) _list_themes, (CairoDockUpdateSyncFunc) _finish_list_themes, (GFreeFunc) _discard_list_themes, pSharedMemory);
	cairo_dock_launch_task (pTask);
	return pTask;
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
	
	if (cDistantThemesDir != NULL && s_cThemeServerAdress)
	{
		gchar *cDistantFileName = g_strdup_printf ("%s/%s.tar.gz", cThemeName, cThemeName);
		GError *erreur = NULL;
		cThemePath = cairo_dock_download_file (s_cThemeServerAdress, cDistantThemesDir, cDistantFileName, cUserThemesDir, &erreur);
		g_free (cDistantFileName);
		if (erreur != NULL)
		{
			cd_warning ("couldn't retrieve distant theme %s : %s" , cThemeName, erreur->message);
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
	
	cd_debug (" ====> cThemePath : %s", cThemePath);
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
	gchar *cNewThemePath = g_strdup_printf ("%s/%s", g_cThemesDirPath, cNewThemeName);
	if (g_file_test (cNewThemePath, G_FILE_TEST_EXISTS))  // on ecrase un theme existant.
	{
		cd_debug ("  le theme existant sera mis a jour");
		gchar *cQuestion = g_strdup_printf (_("Are you sure you want to overwrite theme %s?"), cNewThemeName);
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
			cairo_dock_extract_theme_type_from_name (cThemeName);
			
			bThemeDeleted = TRUE;
			g_string_printf (sCommand, "rm -rf \"%s/%s/%s\"", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR, cThemeName);
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
		cNewThemePath = cairo_dock_get_theme_path (cNewThemeName, CAIRO_DOCK_SHARE_THEMES_DIR, g_cThemesDirPath, CAIRO_DOCK_THEMES_DIR, CAIRO_DOCK_ANY_THEME);
	}
	g_return_val_if_fail (cNewThemePath != NULL && g_file_test (cNewThemePath, G_FILE_TEST_EXISTS), FALSE);
	//g_print ("cNewThemePath : %s ; cNewThemeName : %s\n", cNewThemePath, cNewThemeName);
	
	//\___________________ On charge les parametres de comportement.
	GString *sCommand = g_string_new ("");
	int r;
	cd_message ("Applying changes ...");
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
	cd_debug ("%s", sCommand->str);
	r = system (sCommand->str);
	
	//\___________________ On charge les lanceurs.
	if (bLoadLaunchers)
	{
		g_string_printf (sCommand, "rm -f \"%s\"/*", g_cCurrentIconsPath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
		g_string_printf (sCommand, "rm -f \"%s\"/.*", g_cCurrentIconsPath);
		cd_debug ("%s", sCommand->str);
		r = system (sCommand->str);
	}
	
	//\___________________ On charge les icones.
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


static void _import_theme (gpointer *pSharedMemory)
{
	pSharedMemory[5] = GINT_TO_POINTER (cairo_dock_import_theme (pSharedMemory[0], GPOINTER_TO_INT (pSharedMemory[1]), GPOINTER_TO_INT (pSharedMemory[2])));
}
static gboolean _finish_import (gpointer *pSharedMemory)
{
	if (! pSharedMemory[5])
		cd_warning ("Couldn't import the theme %s.", pSharedMemory[0]);
	
	GFunc pCallback = pSharedMemory[3];
	pCallback (pSharedMemory[5], pSharedMemory[4]);
	return FALSE;
}
static void _discard_import (gpointer *pSharedMemory)
{
	g_free (pSharedMemory[0]);
	g_free (pSharedMemory);
}
CairoDockTask *cairo_dock_import_theme_async (const gchar *cThemeName, gboolean bLoadBehavior, gboolean bLoadLaunchers, GFunc pCallback, gpointer data)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 6);
	pSharedMemory[0] = g_strdup (cThemeName);
	pSharedMemory[1] = GINT_TO_POINTER (bLoadBehavior);
	pSharedMemory[2] = GINT_TO_POINTER (bLoadLaunchers);
	pSharedMemory[3] = pCallback;
	pSharedMemory[4] = data;
	CairoDockTask *pTask = cairo_dock_new_task_full (0, (CairoDockGetDataAsyncFunc) _import_theme, (CairoDockUpdateSyncFunc) _finish_import, (GFreeFunc) _discard_import, pSharedMemory);
	cairo_dock_launch_task (pTask);
	return pTask;
}


void cairo_dock_load_current_theme (void)
{
	cd_message ("%s ()", __func__);

	//\___________________ On libere toute la memoire allouee par tout le monde (stoppe aussi tous les timeout).
	cairo_dock_free_all ();

	//\___________________ On cree le dock principal.
	CairoDock *pMainDock = cairo_dock_create_dock (CAIRO_DOCK_MAIN_DOCK_NAME, NULL);  // on ne lui assigne pas de vues, puisque la vue par defaut des docks principaux sera definie plus tard.

	//\___________________ On lit son fichier de conf et on charge tout.
	cairo_dock_load_config (g_cConfFile, pMainDock);  // chargera des valeurs par defaut si le fichier de conf fourni est incorrect.
}


#define _check_dir(cDirPath) \
	if (! g_file_test (cDirPath, G_FILE_TEST_IS_DIR)) {\
		if (g_mkdir (cDirPath, 7*8*8+7*8+7) != 0) {\
			cd_warning ("couldn't create directory %s", cDirPath);\
			g_free (cDirPath);\
			cDirPath = NULL; } }

void cairo_dock_set_paths (gchar *cRootDataDirPath, gchar *cExtraDirPath, gchar *cThemesDirPath, gchar *cCurrentThemeDirPath, gchar *cThemeServerAdress)
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
	
	g_cCurrentLaunchersPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
	_check_dir (g_cCurrentLaunchersPath);
	g_cCurrentIconsPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
	_check_dir (g_cCurrentIconsPath);
	g_cCurrentPlugInsPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_PLUG_INS_DIR);
	_check_dir (g_cCurrentPlugInsPath);
	g_cConfFile = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
	
	//\___________________ On initialise le gestionnaire de download.
	curl_global_init (CURL_GLOBAL_DEFAULT);
	
	//\___________________ On initialise les numeros de version.
	cairo_dock_get_version_from_string (CAIRO_DOCK_VERSION, &g_iMajorVersion, &g_iMinorVersion, &g_iMicroVersion);
}

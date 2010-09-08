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

#include "../config.h"
#include "cairo-dock-keyfile-utilities.h"
//#include "cairo-dock-dock-manager.h"
//#include "cairo-dock-gui-manager.h"
#include "cairo-dock-task.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-packages.h"

#define CAIRO_DOCK_DEFAULT_PACKAGES_LIST_FILE "list.conf"

static gchar *s_cPackageServerAdress = NULL;

  ////////////////////
 /// DOWNLOAD API ///
////////////////////

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
	
	//\_______________ on deplace un dossier identique prealable.
	gchar *cTempBackup = NULL;
	if (g_file_test (cResultPath, G_FILE_TEST_EXISTS))
	{
		cTempBackup = g_strdup_printf ("%s___cairo-dock-backup", cResultPath);
		g_rename (cResultPath, cTempBackup);
	}
	
	//\_______________ on decompresse l'archive.
	gchar *cCommand = g_strdup_printf ("tar xf%c \"%s\" -C \"%s\"", (g_str_has_suffix (cArchivePath, "bz2") ? 'j' : 'z'), cArchivePath, cExtractTo);
	cd_debug ("tar : %s\n", cCommand);
	int r = system (cCommand);
	
	//\_______________ on verifie le resultat, en remettant l'original en cas d'echec.
	if (r != 0 || !g_file_test (cResultPath, G_FILE_TEST_EXISTS))
	{
		cd_warning ("an error occured while executing '%s'", cCommand);
		if (cTempBackup != NULL)
		{
			g_rename (cTempBackup, cResultPath);
		}
		g_free (cResultPath);
		cResultPath = NULL;
	}
	else if (cTempBackup != NULL)
	{
		gchar *cCommand = g_strdup_printf ("rm -rf \"%s\"", cTempBackup);
		int r = system (cCommand);
		g_free (cCommand);
	}
	
	g_free (cCommand);
	g_free (cTempBackup);
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
	if (mySystem.bForceIPv4)
		curl_easy_setopt (handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);  // la resolution d'adresse en ipv6 peut etre tres lente chez certains.
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
static void _free_dl (gpointer *pSharedMemory)
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
	CairoDockTask *pTask = cairo_dock_new_task_full (0, (CairoDockGetDataAsyncFunc) _dl_file, (CairoDockUpdateSyncFunc) _finish_dl, (GFreeFunc) _free_dl, pSharedMemory);
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

static void _dl_file_content (gpointer *pSharedMemory)
{
	GError *erreur = NULL;
	pSharedMemory[5] = cairo_dock_get_distant_file_content (pSharedMemory[0], pSharedMemory[1], pSharedMemory[2], &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
}
static gboolean _finish_dl_content (gpointer *pSharedMemory)
{
	if (pSharedMemory[5] == NULL)
		cd_warning ("couldn't get distant file %s", pSharedMemory[2]);
	
	GFunc pCallback = pSharedMemory[3];
	pCallback (pSharedMemory[5], pSharedMemory[4]);
	return TRUE;
}
static void _free_dl_content (gpointer *pSharedMemory)
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
	CairoDockTask *pTask = cairo_dock_new_task_full (0, (CairoDockGetDataAsyncFunc) _dl_file_content, (CairoDockUpdateSyncFunc) _finish_dl_content, (GFreeFunc) _free_dl_content, pSharedMemory);
	cairo_dock_launch_task (pTask);
	return pTask;
}

  ////////////////////
 /// PACKAGES API ///
////////////////////

void cairo_dock_free_package (CairoDockPackage *pPackage)
{
	if (pPackage == NULL)
		return ;
	g_free (pPackage->cPackagePath);
	g_free (pPackage->cAuthor);
	g_free (pPackage->cDisplayedName);
	g_free (pPackage);
}

static inline int _get_rating (const gchar *cPackagesDir, const gchar *cPackageName)
{
	gchar *cRatingFile = g_strdup_printf ("%s/.rating/%s", cPackagesDir, cPackageName);
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
GHashTable *cairo_dock_list_local_packages (const gchar *cPackagesDir, GHashTable *hProvidedTable, gboolean bUpdatePackageValidity, GError **erreur)
{
	cd_debug ("%s (%s)", __func__, cPackagesDir);
	GError *tmp_erreur = NULL;
	GDir *dir = g_dir_open (cPackagesDir, 0, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return hProvidedTable;
	}

	GHashTable *pPackageTable = (hProvidedTable != NULL ? hProvidedTable : g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_package));
	
	CairoDockPackageType iType = (strncmp (cPackagesDir, "/usr", 4) == 0 ?
		CAIRO_DOCK_LOCAL_PACKAGE :
		CAIRO_DOCK_USER_PACKAGE);
	GString *sRatingFile = g_string_new (cPackagesDir);
	gchar *cPackagePath;
	CairoDockPackage *pPackage;
	const gchar *cPackageName;
	while ((cPackageName = g_dir_read_name (dir)) != NULL)
	{
		// on ecarte les fichiers caches.
		if (*cPackageName == '.')
			continue;
		
		// on ecarte les non repertoires.
		cPackagePath = g_strdup_printf ("%s/%s", cPackagesDir, cPackageName);
		if (! g_file_test (cPackagePath, G_FILE_TEST_IS_DIR))
		{
			g_free (cPackagePath);
			continue;
		}
		
		// on insere le package dans la table.
		pPackage = g_new0 (CairoDockPackage, 1);
		pPackage->cPackagePath = cPackagePath;
		pPackage->cDisplayedName = g_strdup (cPackageName);
		pPackage->iType = iType;
		pPackage->iRating = _get_rating (cPackagesDir, cPackageName);
		g_hash_table_insert (pPackageTable, g_strdup (cPackageName), pPackage);  // donc ecrase un package installe ayant le meme nom.
	}
	g_dir_close (dir);
	return pPackageTable;
}

static inline int _convert_date (int iDate)
{
	int d, m, y;
	y = iDate / 10000;
	m = (iDate - y*10000) / 100;
	d = iDate % 100;
	return (d + m*30 + y*365);
}
static void _cairo_dock_parse_package_list (GKeyFile *pKeyFile, const gchar *cServerAdress, const gchar *cDirectory, GHashTable *pPackageTable)
{
	// date courante.
	time_t epoch = (time_t) time (NULL);
	struct tm currentTime;
	localtime_r (&epoch, &currentTime);
	int day = currentTime.tm_mday;  // dans l'intervalle 1 a 31.
	int month = currentTime.tm_mon + 1;  // dans l'intervalle 0 a 11.
	int year = 1900 + currentTime.tm_year;  // tm_year = nombre d'annees écoulees depuis 1900.
	int now = day + month * 30 + year * 365;
	
	// liste des packages.
	gsize length=0;
	gchar **pGroupList = g_key_file_get_groups (pKeyFile, &length);
	g_return_if_fail (pGroupList != NULL);  // rien a charger dans la table, on quitte.
	
	// on parcourt la liste.
	gchar *cPackageName, *cDate, *cDisplayedName, *cName, *cAuthor;
	CairoDockPackage *pPackage;
	CairoDockPackageType iType;
	double fSize;
	int iCreationDate, iLastModifDate, iLocalDate, iSobriety, iCategory;
	int last_modif, creation_date;
	guint i;
	for (i = 0; i < length; i ++)
	{
		cPackageName = pGroupList[i];
		iCreationDate = g_key_file_get_integer (pKeyFile, cPackageName, "creation", NULL);
		iLastModifDate = g_key_file_get_integer (pKeyFile, cPackageName, "last modif", NULL);
		iSobriety = g_key_file_get_integer (pKeyFile, cPackageName, "sobriety", NULL);
		iCategory = g_key_file_get_integer (pKeyFile, cPackageName, "category", NULL);
		fSize = g_key_file_get_double (pKeyFile, cPackageName, "size", NULL);
		cAuthor = g_key_file_get_string (pKeyFile, cPackageName, "author", NULL);
		if (cAuthor && *cAuthor == '\0')
		{
			g_free (cAuthor);
			cAuthor = NULL;
		}
		cName = NULL;
		if (g_key_file_has_key (pKeyFile, cPackageName, "name", NULL))
		{
			cName = g_key_file_get_string (pKeyFile, cPackageName, "name", NULL);
		}
		
		// creation < 30j && pas sur le disque -> new
		// sinon last modif < 30j && last use < last modif -> updated
		// sinon -> net
		
		// on surcharge les packages locaux en cas de nouvelle version.
		CairoDockPackage *pSamePackage = g_hash_table_lookup (pPackageTable, cPackageName);
		if (pSamePackage != NULL)  // le package existe en local.
		{
			// on regarde de quand date cette version locale.
			gchar *cVersionFile = g_strdup_printf ("%s/last-modif", pSamePackage->cPackagePath);
			gsize length = 0;
			gchar *cContent = NULL;
			g_file_get_contents (cVersionFile,
				&cContent,
				&length,
				NULL);
			if (cContent == NULL)  // le package n'a pas encore de fichier de date
			{
				// on ne peut pas savoir quand l'utilisateur a mis a jour le package pour la derniere fois;
				// on va supposer que l'on ne met pas a jour ses packages tres regulierement, donc il est probable qu'il l'ait fait il y'a au moins 1 mois.
				// de cette facon, les packages mis a jour recemment (il y'a moins d'1 mois) apparaitront "updated".
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
			g_free (cVersionFile);
			
			
			if (iLocalDate < iLastModifDate)  // la copie locale est plus ancienne.
			{
				iType = CAIRO_DOCK_UPDATED_PACKAGE;
			}
			else  // c'est deja la derniere version disponible, on en reste la.
			{
				pSamePackage->iSobriety = iSobriety;  // par contre on en profite pour renseigner la sobriete.
				g_free (pSamePackage->cDisplayedName);
				pSamePackage->cDisplayedName = g_strdup_printf ("%s by %s", cName ? cName : cPackageName, (cAuthor ? cAuthor : "---"));
				pSamePackage->cAuthor = cAuthor;
				g_free (cName);
				g_free (cPackageName);
				continue;
			}
			
			pPackage = pSamePackage;
			g_free (pPackage->cPackagePath);
			g_free (pPackage->cAuthor);
			g_free (pPackage->cDisplayedName);
		}
		else  // package encore jamais telecharge.
		{
			last_modif = _convert_date (iLastModifDate);
			creation_date = _convert_date (iCreationDate);
			
			if (now - creation_date < 30)  // les packages restent nouveaux pendant 1 mois.
				iType = CAIRO_DOCK_NEW_PACKAGE;
			else if (now - last_modif < 30)  // les packages restent mis a jour pendant 1 mois.
				iType = CAIRO_DOCK_UPDATED_PACKAGE;
			else
				iType = CAIRO_DOCK_DISTANT_PACKAGE;
			
			pPackage = g_new0 (CairoDockPackage, 1);
			g_hash_table_insert (pPackageTable, cPackageName, pPackage);
			pPackage->iRating = g_key_file_get_integer (pKeyFile, cPackageName, "rating", NULL);  // par contre on affiche la note que l'utilisateur avait precedemment etablie.
		}
		
		pPackage->cPackagePath = g_strdup_printf ("%s/%s/%s", cServerAdress, cDirectory, cPackageName);
		pPackage->iType = iType;
		pPackage->fSize = fSize;
		pPackage->cAuthor = cAuthor;
		pPackage->cDisplayedName = g_strdup_printf ("%s by %s [%.2f MB]", cName ? cName : cPackageName, (cAuthor ? cAuthor : "---"), fSize);
		pPackage->iSobriety = iSobriety;
		pPackage->iCategory = iCategory;
		pPackage->iCreationDate = iCreationDate;
		pPackage->iLastModifDate = iLastModifDate;
	}
	g_free (pGroupList);  // les noms des packages sont desormais dans la hash-table.
}
GHashTable *cairo_dock_list_net_packages (const gchar *cServerAdress, const gchar *cDirectory, const gchar *cListFileName, GHashTable *hProvidedTable, GError **erreur)
{
	g_return_val_if_fail (cServerAdress != NULL && *cServerAdress != '\0', hProvidedTable);
	cd_message ("listing net packages on %s/%s ...", cServerAdress, cDirectory);
	
	// On recupere la liste des packages distants.
	GError *tmp_erreur = NULL;
	gchar *cContent = cairo_dock_get_distant_file_content (cServerAdress, cDirectory, cListFileName, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		cd_warning ("couldn't retrieve packages on %s (check that your connection is alive, or retry later)", cServerAdress);
		g_propagate_error (erreur, tmp_erreur);
		return hProvidedTable;
	}
	
	// on verifie son integrite.
	if (cContent == NULL || strncmp (cContent, "#!CD", 4) != 0)  // avec une connexion wifi etablie sur un operateur auquel on ne s'est pas logue, il peut nous renvoyer des messages au lieu de juste rien. On filtre ca par un entete dedie.
	{
		cd_warning ("empty packages list on %s (check that your connection is alive, or retry later)", cServerAdress);
		g_set_error (erreur, 1, 1, "empty packages list on %s", cServerAdress);
		g_free (cContent);
		return hProvidedTable;
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
		cd_warning ("invalid list of packages (%s)\n(check that your connection is alive, or retry later)", cServerAdress);
		g_propagate_error (erreur, tmp_erreur);
		g_key_file_free (pKeyFile);
		return hProvidedTable;
	}
	
	// on parse la liste dans une table de hashage.
	GHashTable *pPackageTable = (hProvidedTable != NULL ? hProvidedTable : g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_package));
	
	_cairo_dock_parse_package_list (pKeyFile, cServerAdress, cDirectory, pPackageTable);
	
	g_key_file_free (pKeyFile);
	
	return pPackageTable;
}

GHashTable *cairo_dock_list_packages (const gchar *cSharePackagesDir, const gchar *cUserPackagesDir, const gchar *cDistantPackagesDir)
{
	cd_message ("%s (%s, %s, %s)", __func__, cSharePackagesDir, cUserPackagesDir, cDistantPackagesDir);
	GError *erreur = NULL;
	GHashTable *pPackageTable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_package);
	
	//\______________ On recupere les packages pre-installes.
	if (cSharePackagesDir != NULL)
		pPackageTable = cairo_dock_list_local_packages (cSharePackagesDir, pPackageTable, cDistantPackagesDir != NULL, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while listing pre-installed packages in '%s' : %s", cSharePackagesDir, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	
	//\______________ On recupere les packages utilisateurs (qui ecrasent donc les precedents).
	if (cUserPackagesDir != NULL)
		pPackageTable = cairo_dock_list_local_packages (cUserPackagesDir, pPackageTable, cDistantPackagesDir != NULL, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while listing user packages in '%s' : %s", cUserPackagesDir, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	
	//\______________ On recupere les packages distants (qui surchargent tous les packages).
	if (cDistantPackagesDir != NULL && s_cPackageServerAdress)
	{
		pPackageTable = cairo_dock_list_net_packages (s_cPackageServerAdress, cDistantPackagesDir, CAIRO_DOCK_DEFAULT_PACKAGES_LIST_FILE, pPackageTable, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("while listing distant packages in '%s/%s' : %s", s_cPackageServerAdress, cDistantPackagesDir, erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
	}
	
	return pPackageTable;
}

static void _list_packages (gpointer *pSharedMemory)
{
	pSharedMemory[5] = cairo_dock_list_packages (pSharedMemory[0], pSharedMemory[1], pSharedMemory[2]);
}
static gboolean _finish_list_packages (gpointer *pSharedMemory)
{
	if (pSharedMemory[5] == NULL)
		cd_warning ("couldn't get distant packages in '%s'", pSharedMemory[2]);
	
	GFunc pCallback = pSharedMemory[3];
	pCallback (pSharedMemory[5], pSharedMemory[4]);
	return TRUE;
}
static void _discard_list_packages (gpointer *pSharedMemory)
{
	g_free (pSharedMemory[0]);
	g_free (pSharedMemory[1]);
	g_free (pSharedMemory[2]);
	if (pSharedMemory[5] != NULL)
		g_hash_table_unref (pSharedMemory[5]);
	g_free (pSharedMemory);
}
CairoDockTask *cairo_dock_list_packages_async (const gchar *cSharePackagesDir, const gchar *cUserPackagesDir, const gchar *cDistantPackagesDir, CairoDockGetPackagesFunc pCallback, gpointer data)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 6);
	pSharedMemory[0] = g_strdup (cSharePackagesDir);
	pSharedMemory[1] = g_strdup (cUserPackagesDir);
	pSharedMemory[2] = g_strdup (cDistantPackagesDir);
	pSharedMemory[3] = pCallback;
	pSharedMemory[4] = data;
	CairoDockTask *pTask = cairo_dock_new_task_full (0, (CairoDockGetDataAsyncFunc) _list_packages, (CairoDockUpdateSyncFunc) _finish_list_packages, (GFreeFunc) _discard_list_packages, pSharedMemory);
	cairo_dock_launch_task (pTask);
	return pTask;
}

gchar *cairo_dock_get_package_path (const gchar *cPackageName, const gchar *cSharePackagesDir, const gchar *cUserPackagesDir, const gchar *cDistantPackagesDir, CairoDockPackageType iGivenType)
{
	cd_message ("%s (%s, %s, %s)", __func__, cSharePackagesDir, cUserPackagesDir, cDistantPackagesDir);
	if (cPackageName == NULL || *cPackageName == '\0')
		return NULL;
	CairoDockPackageType iType = cairo_dock_extract_package_type_from_name (cPackageName);  // juste au cas ou, mais normalement c'est plutot a l'appelant d'extraire le type de theme.
	if (iType == CAIRO_DOCK_ANY_PACKAGE)
		iType = iGivenType;
	gchar *cPackagePath = NULL;
	
	//g_print ("iType : %d\n", iType);
	if (cUserPackagesDir != NULL && /*iType != CAIRO_DOCK_DISTANT_PACKAGE && iType != CAIRO_DOCK_NEW_PACKAGE && */iType != CAIRO_DOCK_UPDATED_PACKAGE)
	{
		cPackagePath = g_strdup_printf ("%s/%s", cUserPackagesDir, cPackageName);
		
		if (g_file_test (cPackagePath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
		{
			return cPackagePath;
		}
		
		g_free (cPackagePath);
		cPackagePath = NULL;
	}
	
	if (cSharePackagesDir != NULL && iType != CAIRO_DOCK_UPDATED_PACKAGE)
	{
		cPackagePath = g_strdup_printf ("%s/%s", cSharePackagesDir, cPackageName);
		if (g_file_test (cPackagePath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			return cPackagePath;
		g_free (cPackagePath);
		cPackagePath = NULL;
	}
	
	if (cDistantPackagesDir != NULL && s_cPackageServerAdress)
	{
		gchar *cDistantFileName = g_strdup_printf ("%s/%s.tar.gz", cPackageName, cPackageName);
		GError *erreur = NULL;
		cPackagePath = cairo_dock_download_file (s_cPackageServerAdress, cDistantPackagesDir, cDistantFileName, cUserPackagesDir, &erreur);
		g_free (cDistantFileName);
		if (erreur != NULL)
		{
			cd_warning ("couldn't retrieve distant package %s : %s" , cPackageName, erreur->message);
			g_error_free (erreur);
		}
		else  // on se souvient de la date a laquelle on a mis a jour le package pour la derniere fois.
		{
			gchar *cVersionFile = g_strdup_printf ("%s/last-modif", cPackagePath);
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
	
	cd_debug (" ====> cPackagePath : %s", cPackagePath);
	return cPackagePath;
}


CairoDockPackageType cairo_dock_extract_package_type_from_name (const gchar *cPackageName)
{
	if (cPackageName == NULL)
		return CAIRO_DOCK_ANY_PACKAGE;
	CairoDockPackageType iType = CAIRO_DOCK_ANY_PACKAGE;
	int l = strlen (cPackageName);
	if (cPackageName[l-1] == ']')
	{
		gchar *str = strrchr (cPackageName, '[');
		if (str != NULL && g_ascii_isdigit (*(str+1)))
		{
			iType = atoi (str+1);
			*str = '\0';
		}
	}
	return iType;
}

#define _check_dir(cDirPath) \
	if (! g_file_test (cDirPath, G_FILE_TEST_IS_DIR)) {\
		if (g_mkdir (cDirPath, 7*8*8+7*8+7) != 0) {\
			cd_warning ("couldn't create directory %s", cDirPath);\
			g_free (cDirPath);\
			cDirPath = NULL; } }

void cairo_dock_init_package_manager (gchar *cPackageServerAdress)
{
	s_cPackageServerAdress = cPackageServerAdress;
	
	//\___________________ On initialise le gestionnaire de download.
	curl_global_init (CURL_GLOBAL_DEFAULT);
}

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

#include "gldi-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-task.h"
#include "cairo-dock-config.h"
#include "cairo-dock-log.h"
#define _MANAGER_DEF_
#include "cairo-dock-packages.h"

// public (manager, config, data)
CairoConnectionParam myConnectionParam;
GldiManager myConnectionMgr;

// dependancies

// private
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
	cd_debug ("tar : %s", cCommand);
	int r = system (cCommand);
	
	//\_______________ on verifie le resultat, en remettant l'original en cas d'echec.
	if (r != 0 || !g_file_test (cResultPath, G_FILE_TEST_EXISTS))
	{
		cd_warning ("Invalid archive file (%s)", cCommand);
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
		if (r < 0)
			cd_warning ("Couldn't remove temporary folder (%s)", cCommand);
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
	if (myConnectionParam.cConnectionProxy != NULL)
	{
		curl_easy_setopt (handle, CURLOPT_PROXY, myConnectionParam.cConnectionProxy);
		if (myConnectionParam.iConnectionPort != 0)
			curl_easy_setopt (handle, CURLOPT_PROXYPORT, myConnectionParam.iConnectionPort);
		if (myConnectionParam.cConnectionUser != NULL && myConnectionParam.
			cConnectionPasswd != NULL)
		{
			gchar *cUserPwd = g_strdup_printf ("%s:%s", myConnectionParam.cConnectionUser, myConnectionParam.
			cConnectionPasswd);
			curl_easy_setopt (handle, CURLOPT_PROXYUSERPWD, cUserPwd);
			g_free (cUserPwd);
			/*curl_easy_setopt (handle, CURLOPT_PROXYUSERNAME, myConnectionParam.cConnectionUser);
			if (myConnectionParam.cConnectionPasswd != NULL)
				curl_easy_setopt (handle, CURLOPT_PROXYPASSWORD, myConnectionParam.cConnectionPasswd);*/  // a partir de libcurl 7.19.1, donc apres Jaunty
		}
	}
	if (myConnectionParam.bForceIPv4)
		curl_easy_setopt (handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);  // la resolution d'adresse en ipv6 peut etre tres lente chez certains.
	curl_easy_setopt (handle, CURLOPT_TIMEOUT, myConnectionParam.iConnectionMaxTime);
	curl_easy_setopt (handle, CURLOPT_CONNECTTIMEOUT, myConnectionParam.iConnectionTimeout);
	curl_easy_setopt (handle, CURLOPT_NOSIGNAL, 1);  // With CURLOPT_NOSIGNAL set non-zero, curl will not use any signals; sinon curl se vautre apres le timeout, meme si le download s'est bien passe !
	curl_easy_setopt (handle, CURLOPT_FOLLOWLOCATION , 1);  // follow redirection
	///curl_easy_setopt (handle, CURLOPT_USERAGENT , "a/5.0 (X11; Linux x86_64; rv:2.0b11) Gecko/20100101 Firefox/4.0b11");
	return handle;
}

static size_t _write_data_to_file (gpointer buffer, size_t size, size_t nmemb, FILE *fd)
{
	return fwrite (buffer, size, nmemb, fd);
}
gboolean cairo_dock_download_file (const gchar *cURL, const gchar *cLocalPath)
{
	g_return_val_if_fail (cLocalPath != NULL && cURL != NULL, FALSE);
	
	// download the file
	FILE *f = fopen (cLocalPath, "wb");
	g_return_val_if_fail (f, FALSE);
	
	CURL *handle = _init_curl_connection (cURL);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, _write_data_to_file);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, f);
	
	CURLcode r = curl_easy_perform (handle);
	fclose (f);
	
	// check the result
	gboolean bOk;
	if (r != CURLE_OK)  // an error occured
	{
		cd_warning ("Couldn't download file '%s' (%s)", cURL, curl_easy_strerror (r));
		g_remove (cLocalPath);
		bOk = FALSE;
	}
	else  // download ok, check the file is not empty.
	{
		struct stat buf;
		stat (cLocalPath, &buf);
		if (buf.st_size > 0)
		{
			bOk = TRUE;
		}
		else
		{
			cd_warning ("Empty file from '%s'", cURL);
			g_remove (cLocalPath);
			bOk = FALSE;
		}
	}
	
	curl_easy_cleanup (handle);
	return bOk;
}

gchar *cairo_dock_download_file_in_tmp (const gchar *cURL)
{
	int fds = 0;
	gchar *cTmpFilePath = g_strdup ("/tmp/cairo-dock-net-file.XXXXXX");
	fds = mkstemp (cTmpFilePath);
	if (fds == -1)
	{
		cd_warning ("Couldn't create temporary file '%s' - check permissions in /tmp", cTmpFilePath);
		g_free (cTmpFilePath);
		return NULL;
	}
	
	gboolean bOk = cairo_dock_download_file (cURL, cTmpFilePath);
	
	if (! bOk)
	{
		g_remove (cTmpFilePath);
		g_free (cTmpFilePath);
		cTmpFilePath = NULL;
	}
	close(fds);
	return cTmpFilePath;
}

gchar *cairo_dock_download_archive (const gchar *cURL, const gchar *cExtractTo)
{
	g_return_val_if_fail (cURL != NULL, NULL);
	
	// download the archive
	gchar *cArchivePath = cairo_dock_download_file_in_tmp (cURL);
	
	// if success, uncompress it.
	gchar *cPath = NULL;
	if (cArchivePath != NULL)
	{
		if (cExtractTo != NULL)
		{
			cd_debug ("uncompressing archive...");
			cPath = cairo_dock_uncompress_file (cArchivePath, cExtractTo, cURL);
			g_remove (cArchivePath);
		}
		else
		{
			cPath = cArchivePath;
			cArchivePath = NULL;
		}
	}
	
	g_free (cArchivePath);
	return cPath;
}


static void _dl_file (gpointer *pSharedMemory)
{
	if (pSharedMemory[1] != NULL)  // download to the given location
	{
		if (cairo_dock_download_file (pSharedMemory[0], pSharedMemory[1]))
		{
			pSharedMemory[4] = pSharedMemory[1];
			pSharedMemory[1] = NULL;
		}
	}
	else  // download in /tmp
	{
		pSharedMemory[4] = cairo_dock_download_file_in_tmp (pSharedMemory[0]);
	}
}
static gboolean _finish_dl (gpointer *pSharedMemory)
{
	GFunc pCallback = pSharedMemory[2];
	pCallback (pSharedMemory[4], pSharedMemory[3]);
	return FALSE;
}
static void _free_dl (gpointer *pSharedMemory)
{
	g_free (pSharedMemory[0]);
	g_free (pSharedMemory[1]);
	g_free (pSharedMemory[4]);
	g_free (pSharedMemory);
}
GldiTask *cairo_dock_download_file_async (const gchar *cURL, const gchar *cLocalPath, GFunc pCallback, gpointer data)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 5);
	pSharedMemory[0] = g_strdup (cURL);
	pSharedMemory[1] = g_strdup (cLocalPath);
	pSharedMemory[2] = pCallback;
	pSharedMemory[3] = data;
	GldiTask *pTask = gldi_task_new_full (0, (GldiGetDataAsyncFunc) _dl_file, (GldiUpdateSyncFunc) _finish_dl, (GFreeFunc) _free_dl, pSharedMemory);
	gldi_task_launch (pTask);
	return pTask;
}



static size_t _write_data_to_buffer (gpointer data, size_t size, size_t nmemb, GString *buffer)
{
	g_string_append_len (buffer, data, size * nmemb);
	return size * nmemb;
}
gchar *cairo_dock_get_url_data_with_post (const gchar *cURL, gboolean bGetOutputHeaders, GError **erreur, const gchar *cFirstProperty, ...)
{
	//\_______________ On lance le download.
	cd_debug ("getting data from '%s' ...", cURL);
	CURL *handle = _init_curl_connection (cURL);
	
	GString *sPostData = NULL;
	if (cFirstProperty != NULL)
	{
		sPostData = g_string_new ("");
		const gchar *cProperty = cFirstProperty;
		gchar *cData;
		gchar *cEncodedData = NULL;
		va_list args;
		va_start (args, cFirstProperty);
		do
		{
			cData = va_arg (args, gchar *);
			if (!cData)
				break;
			if (cEncodedData != NULL)  // we don't access the pointer, we just want to know if we have already looped once or not.
				g_string_append_c (sPostData, '&');
			cEncodedData = curl_easy_escape (handle, cData, 0);
			g_string_append_printf (sPostData, "%s=%s", cProperty, cEncodedData);
			curl_free (cEncodedData);
			cProperty = va_arg (args, gchar *);
		}
		while (cProperty != NULL);
		va_end (args);
		// g_print ("POST data: '%s'\n", sPostData->str);
		
		curl_easy_setopt (handle, CURLOPT_POST, 1);
		curl_easy_setopt (handle, CURLOPT_POSTFIELDS, sPostData->str);
		if (bGetOutputHeaders)
			curl_easy_setopt (handle, CURLOPT_HEADER, 1);
	}
	curl_easy_setopt (handle, CURLOPT_WRITEFUNCTION, (curl_write_callback)_write_data_to_buffer);
	GString *buffer = g_string_sized_new (1024);
	curl_easy_setopt (handle, CURLOPT_WRITEDATA, buffer);
	
	CURLcode r = curl_easy_perform (handle);
	
	if (r != CURLE_OK)
	{
		g_set_error (erreur, 1, 1, "Couldn't download file '%s' (%s)", cURL, curl_easy_strerror (r));
		g_string_free (buffer, TRUE);
		buffer = NULL;
	}
	
	curl_easy_cleanup (handle);
	if (sPostData)
		g_string_free (sPostData, TRUE);
	
	//\_______________ On recupere les donnees.
	gchar *cContent = NULL;
	if (buffer != NULL) cContent = g_string_free_and_steal (buffer);
	return cContent;
}

gchar *cairo_dock_get_url_data_with_headers (const gchar *cURL, gboolean bGetOutputHeaders, GError **erreur, const gchar *cFirstProperty, ...)
{
	//\_______________ init a CURL context
	cd_debug ("getting data from '%s' ...", cURL);
	CURL *handle = _init_curl_connection (cURL);
	
	//\_______________ set the custom headers
	struct curl_slist *headers = NULL;
	if (cFirstProperty != NULL)
	{
		const gchar *cProperty = cFirstProperty;
		gchar *cData;
		va_list args;
		va_start (args, cFirstProperty);
		do
		{
			cData = va_arg (args, gchar *);
			if (!cData)
				break;
			
			gchar *header = g_strdup_printf ("%s: %s", cProperty, cData);
			headers = curl_slist_append (headers, header);
			g_free (header);
			cProperty = va_arg (args, gchar *);
		}
		while (cProperty != NULL);
		va_end (args);
		curl_easy_setopt (handle, CURLOPT_HTTPHEADER, headers);
	}
	
	//\_______________ set the callback
	if (bGetOutputHeaders)
		curl_easy_setopt (handle, CURLOPT_HEADER, 1);
	
	curl_easy_setopt (handle, CURLOPT_WRITEFUNCTION, (curl_write_callback)_write_data_to_buffer);
	GString *buffer = g_string_sized_new (1024);
	curl_easy_setopt (handle, CURLOPT_WRITEDATA, buffer);
	
	//\_______________ perform the request
	CURLcode r = curl_easy_perform (handle);
	
	if (r != CURLE_OK)
	{
		g_set_error (erreur, 1, 1, "Couldn't download file '%s' (%s)", cURL, curl_easy_strerror (r));
		g_string_free (buffer, TRUE);
		buffer = NULL;
	}
	
	curl_slist_free_all (headers);
	curl_easy_cleanup (handle);
	
	//\_______________ On recupere les donnees.
	gchar *cContent = NULL;
	if (buffer != NULL) cContent = g_string_free_and_steal (buffer);
	return cContent;
}

static void _dl_file_content (gpointer *pSharedMemory)
{
	GError *erreur = NULL;
	pSharedMemory[3] = cairo_dock_get_url_data (pSharedMemory[0], &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
}
static gboolean _finish_dl_content (gpointer *pSharedMemory)
{
	GFunc pCallback = pSharedMemory[1];
	pCallback (pSharedMemory[3], pSharedMemory[2]);
	return TRUE;
}
static void _free_dl_content (gpointer *pSharedMemory)
{
	g_free (pSharedMemory[0]);
	g_free (pSharedMemory[3]);
	g_free (pSharedMemory);
}
GldiTask *cairo_dock_get_url_data_async (const gchar *cURL, GFunc pCallback, gpointer data)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 6);
	pSharedMemory[0] = g_strdup (cURL);
	pSharedMemory[1] = pCallback;
	pSharedMemory[2] = data;
	GldiTask *pTask = gldi_task_new_full (0, (GldiGetDataAsyncFunc) _dl_file_content, (GldiUpdateSyncFunc) _finish_dl_content, (GFreeFunc) _free_dl_content, pSharedMemory);
	gldi_task_launch (pTask);
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
	g_free (pPackage->cHint);
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
GHashTable *cairo_dock_list_local_packages (const gchar *cPackagesDir, GHashTable *hProvidedTable, G_GNUC_UNUSED gboolean bUpdatePackageValidity, GError **erreur)
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
	gchar *cPackageName, *cName, *cAuthor;
	CairoDockPackage *pPackage;
	CairoDockPackageType iType;
	double fSize;
	int iCreationDate, iLastModifDate, iLocalDate, iSobriety;
	int last_modif, creation_date;
	gchar *cHint;
	guint i;
	for (i = 0; i < length; i ++)
	{
		cPackageName = pGroupList[i];
		iCreationDate = g_key_file_get_integer (pKeyFile, cPackageName, "creation", NULL);
		iLastModifDate = g_key_file_get_integer (pKeyFile, cPackageName, "last modif", NULL);
		iSobriety = g_key_file_get_integer (pKeyFile, cPackageName, "sobriety", NULL);
		cHint = g_key_file_get_string (pKeyFile, cPackageName, "hint", NULL);
		if (cHint && *cHint == '\0')
		{
			g_free (cHint);
			cHint = NULL;
		}
		fSize = g_key_file_get_double (pKeyFile, cPackageName, "size", NULL);
		cAuthor = g_key_file_get_string (pKeyFile, cPackageName, "author", NULL);
		if (cAuthor && *cAuthor == '\0')
		{
			g_free (cAuthor);
			cAuthor = NULL;
		}
		cName = g_key_file_get_string (pKeyFile, cPackageName, "name", NULL);
		if (cName && *cName == '\0')
		{
			g_free (cName);
			cName = NULL;
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
				gchar *cDate = g_strdup_printf ("%d", iLocalDate);
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
				pSamePackage->cDisplayedName = (cName ? cName : g_strdup (cPackageName));
				pSamePackage->cAuthor = cAuthor;  // et l'auteur original.
				pSamePackage->cHint = cHint;  // et le hint.
				g_free (cPackageName);
				continue;
			}
			
			pPackage = pSamePackage;  // we'll update the characteristics of the package with the one from the server.
			g_free (pPackage->cPackagePath);
			g_free (pPackage->cAuthor);
			g_free (pPackage->cHint);
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
			g_hash_table_insert (pPackageTable, g_strdup (cPackageName), pPackage);
			pPackage->iRating = g_key_file_get_integer (pKeyFile, cPackageName, "rating", NULL);  // par contre on affiche la note que l'utilisateur avait precedemment etablie.
		}
		
		pPackage->cPackagePath = g_strdup_printf ("%s/%s/%s", cServerAdress, cDirectory, cPackageName);
		pPackage->iType = iType;
		pPackage->fSize = fSize;
		pPackage->cAuthor = cAuthor;
		pPackage->cDisplayedName = (cName ? cName : g_strdup (cPackageName));
		pPackage->iSobriety = iSobriety;
		pPackage->cHint = cHint;
		pPackage->iCreationDate = iCreationDate;
		pPackage->iLastModifDate = iLastModifDate;
		g_free (cPackageName);
	}
	g_free (pGroupList);  // les noms des packages sont liberes dans la boucle.
}
GHashTable *cairo_dock_list_net_packages (const gchar *cServerAdress, const gchar *cDirectory, const gchar *cListFileName, GHashTable *hProvidedTable, GError **erreur)
{
	g_return_val_if_fail (cServerAdress != NULL && *cServerAdress != '\0', hProvidedTable);
	cd_message ("listing net packages on %s/%s ...", cServerAdress, cDirectory);
	
	// On recupere la liste des packages distants.
	GError *tmp_erreur = NULL;
	gchar *cURL = g_strdup_printf ("%s/%s/%s", cServerAdress, cDirectory, cListFileName);
	gchar *cContent = cairo_dock_get_url_data (cURL, &tmp_erreur);
	g_free (cURL);
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

GHashTable *cairo_dock_list_packages (const gchar *cSharePackagesDir, const gchar *cUserPackagesDir, const gchar *cDistantPackagesDir, GHashTable *pTable)
{
	cd_message ("%s (%s, %s, %s)", __func__, cSharePackagesDir, cUserPackagesDir, cDistantPackagesDir);
	GError *erreur = NULL;
	GHashTable *pPackageTable = (pTable ? pTable : g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) cairo_dock_free_package));
	
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
	pSharedMemory[5] = cairo_dock_list_packages (pSharedMemory[0], pSharedMemory[1], pSharedMemory[2], pSharedMemory[5]);
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
GldiTask *cairo_dock_list_packages_async (const gchar *cSharePackagesDir, const gchar *cUserPackagesDir, const gchar *cDistantPackagesDir, CairoDockGetPackagesFunc pCallback, gpointer data, GHashTable *pTable)
{
	gpointer *pSharedMemory = g_new0 (gpointer, 6);
	pSharedMemory[0] = g_strdup (cSharePackagesDir);
	pSharedMemory[1] = g_strdup (cUserPackagesDir);
	pSharedMemory[2] = g_strdup (cDistantPackagesDir);
	pSharedMemory[3] = pCallback;
	pSharedMemory[4] = data;
	pSharedMemory[5] = pTable;  // can be NULL
	GldiTask *pTask = gldi_task_new_full (0, (GldiGetDataAsyncFunc)_list_packages, (GldiUpdateSyncFunc)_finish_list_packages, (GFreeFunc) _discard_list_packages, pSharedMemory);
	gldi_task_launch (pTask);
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
	if (cUserPackagesDir != NULL && iType != CAIRO_DOCK_UPDATED_PACKAGE)
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
		gchar *cDistantFileName = g_strdup_printf ("%s/%s/%s/%s.tar.gz", s_cPackageServerAdress, cDistantPackagesDir, cPackageName, cPackageName);
		cPackagePath = cairo_dock_download_archive (cDistantFileName, cUserPackagesDir);
		g_free (cDistantFileName);
		
		if (cPackagePath != NULL)  // on se souvient de la date a laquelle on a mis a jour le package pour la derniere fois.
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


void cairo_dock_set_packages_server (gchar *cPackageServerAdress)
{
	s_cPackageServerAdress = cPackageServerAdress;
}


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoConnectionParam *pSystem)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pSystem->iConnectionTimeout = cairo_dock_get_integer_key_value (pKeyFile, "System", "conn timeout", &bFlushConfFileNeeded, 7, NULL, NULL);
	pSystem->iConnectionMaxTime = cairo_dock_get_integer_key_value (pKeyFile, "System", "conn max time", &bFlushConfFileNeeded, 120, NULL, NULL);
	if (cairo_dock_get_boolean_key_value (pKeyFile, "System", "conn use proxy", &bFlushConfFileNeeded, FALSE, NULL, NULL))
	{
		pSystem->cConnectionProxy = cairo_dock_get_string_key_value (pKeyFile, "System", "conn proxy", &bFlushConfFileNeeded, NULL, NULL, NULL);
		pSystem->iConnectionPort = cairo_dock_get_integer_key_value (pKeyFile, "System", "conn port", &bFlushConfFileNeeded, 0, NULL, NULL);
		pSystem->cConnectionUser = cairo_dock_get_string_key_value (pKeyFile, "System", "conn user", &bFlushConfFileNeeded, NULL, NULL, NULL);
		gchar *cPasswd = cairo_dock_get_string_key_value (pKeyFile, "System", "conn passwd", &bFlushConfFileNeeded, NULL, NULL, NULL);
		cairo_dock_decrypt_string (cPasswd, &pSystem->cConnectionPasswd);
		pSystem->bForceIPv4 = cairo_dock_get_boolean_key_value (pKeyFile, "System", "force ipv4", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	}
	
	return bFlushConfFileNeeded;
}

  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (CairoConnectionParam *pSystem)
{
	g_free (pSystem->cConnectionProxy);
	g_free (pSystem->cConnectionUser);
	g_free (pSystem->cConnectionPasswd);
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	curl_global_init (CURL_GLOBAL_DEFAULT);
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_connection_manager (void)
{
	// Manager
	memset (&myConnectionMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myConnectionMgr), &myManagerObjectMgr, NULL);
	myConnectionMgr.cModuleName  = "Connection";
	// interface
	myConnectionMgr.init         = init;
	myConnectionMgr.load         = NULL;
	myConnectionMgr.unload       = NULL;
	myConnectionMgr.reload       = (GldiManagerReloadFunc)NULL;
	myConnectionMgr.get_config   = (GldiManagerGetConfigFunc)get_config;
	myConnectionMgr.reset_config = (GldiManagerResetConfigFunc)reset_config;
	// Config
	myConnectionMgr.pConfig = (GldiManagerConfigPtr)&myConnectionParam;
	myConnectionMgr.iSizeOfConfig = sizeof (CairoConnectionParam);
	// data
	myConnectionMgr.pData = (GldiManagerDataPtr)NULL;
	myConnectionMgr.iSizeOfData = 0;
	// signals
	gldi_object_install_notifications (&myConnectionMgr, NB_NOTIFICATIONS_CONNECTION);  // we don't have a Connection Object, so let's put the signals here
}

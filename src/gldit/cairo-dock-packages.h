/*
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


#ifndef __CAIRO_DOCK_PACKAGES__
#define  __CAIRO_DOCK_PACKAGES__

#include <glib.h>
#include <gtk/gtk.h>
G_BEGIN_DECLS

/**@file cairo-dock-packages.h This class provides a convenient way to deal with packages. A Package is a tarball (tar.gz) of a folder, located on a distant server, that can be installed locally.
* Packages are listed on the server in a file named "list.conf". It's a group-key file starting with "#!CD" on the first line; each package is described in its own group. Packages are stored on the server in a folder that has the same name, and contains the tarball, a "readme" file, and a "preview" file.
*
* The class offers a high level of abstraction that allows to manipulate packages without having to care their location, version, etc.
* It also provides convenient utility functions to get remote data.
*
* To get the list of available packages, use \ref cairo_dock_list_packages, or its asynchronous version \ref cairo_dock_list_packages_async.
* To get the path of a package, use \ref cairo_dock_get_package_path; it will look for the the best
*/

/// Types of packagess.
typedef enum {
	/// package installed as root on the machine (in a sub-folder /usr).
	CAIRO_DOCK_LOCAL_PACKAGE=0,
	/// package located in the user's home
	CAIRO_DOCK_USER_PACKAGE,
	/// package present on the server
	CAIRO_DOCK_DISTANT_PACKAGE,
	/// package newly present on the server (for less than 1 month)
	CAIRO_DOCK_NEW_PACKAGE,
	/// package present locally but with a more recent version on the server, or distant package that has been updated in the past month.
	CAIRO_DOCK_UPDATED_PACKAGE,
	/// joker (the search path function will search locally first, and on the server then).
	CAIRO_DOCK_ANY_PACKAGE,
	CAIRO_DOCK_NB_TYPE_PACKAGE
} CairoDockPackageType;

/// Definition of a generic package.
struct _CairoDockPackage {
	/// complete path of the package.
	gchar *cPackagePath;
	/// size in Mo
	gdouble fSize;
	/// author(s)
	gchar *cAuthor;
	/// name of the package
	gchar *cDisplayedName;
	/// type of package : installed, user, distant.
	CairoDockPackageType iType;
	/// rating of the package.
	gint iRating;
	/// sobriety/simplicity of the package.
	gint iSobriety;
	/// category of the package.
	gint iCategory;
	/// date of creation of the package.
	gint iCreationDate;
	/// date of latest changes in the package.
	gint iLastModifDate;
};

/// Prototype of the function called when the list of packages is available. Use g_hash_table_ref if you want to keep the table outside of this function.
typedef void (* CairoDockGetPackagesFunc ) (GHashTable *pPackagesTable, gpointer data);

gchar *cairo_dock_uncompress_file (const gchar *cArchivePath, const gchar *cExtractTo, const gchar *cRealArchiveName);

/** Download a distant file into a given folder, possibly extracting it.
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param cExtractTo a local path where to extract the file, if this one is a .tar.gz/.tar.bz2/.tgz archive, or NULL.
*@param erreur an error.
*@return the local path of the downloaded file. If it was an archive, the path of the extracted file. Free the string after using it.
*/
gchar *cairo_dock_download_file (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, const gchar *cExtractTo, GError **erreur);

/** Asynchronously download a distant file into a given folder, possibly extracting it. This function is non-blocking, you'll get a CairoTask that you can discard at any time, and you'll get the path of the downloaded file as the first argument of the callback (the second being the data you passed to this function).
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param cExtractTo a local path where to extract the file, if this one is a .tar.gz/.tar.bz2/.tgz archive, or NULL.
*@param pCallback function called when the download is finished. It takes the path of the downloaded file (it belongs to the task so don't free it) and the data you've set here.
*@param data data to be passed to the callback.
*@return the Task that is doing the job. Keep it and use \ref cairo_dock_discard_task whenever you want to discard the download (for instance if the user cancels it), or \ref cairo_dock_free_task inside your callback.
*/
CairoDockTask *cairo_dock_download_file_async (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, const gchar *cExtractTo, GFunc pCallback, gpointer data);

/** Retrieve the content of a distant text file.
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param erreur an error.
*@return the content of the file. Free the string after using it.
*/
gchar *cairo_dock_get_distant_file_content (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, GError **erreur);

/** Retrieve the data of a distant URL.
*@param cURL distant adress to get data from.
*@param erreur an error.
*@return the data. Free the string after using it.
*/
gchar *cairo_dock_get_url_data (const gchar *cURL, GError **erreur);

/** Asynchronously retrieve the content of a distant text file. This function is non-blocking, you'll get a CairoTask that you can discard at any time, and you'll get the content of the downloaded file as the first argument of the callback (the second being the data you passed to this function).
*@param cServerAdress adress of the server.
*@param cDistantFilePath path of the file on the server.
*@param cDistantFileName name of the file.
*@param pCallback function called when the download is finished. It takes the content of the downloaded file (it belongs to the task so don't free it) and the data you've set here.
*@param data data to be passed to the callback.
*@return the Task that is doing the job. Keep it and use \ref cairo_dock_discard_task whenever you want to discard the download (for instance if the user cancels it), or \ref cairo_dock_free_task inside your callback.
*/
CairoDockTask *cairo_dock_get_distant_file_content_async (const gchar *cServerAdress, const gchar *cDistantFilePath, const gchar *cDistantFileName, GFunc pCallback, gpointer data);


  ////////////////
 // THEMES API //
////////////////

/** Destroy a package and free all its allocated memory.
*@param pPackage the package.
*/
void cairo_dock_free_package (CairoDockPackage *pPackage);

GHashTable *cairo_dock_list_local_packages (const gchar *cPackagesDir, GHashTable *hProvidedTable, gboolean bUpdatePackageValidity, GError **erreur);

GHashTable *cairo_dock_list_net_packages (const gchar *cServerAdress, const gchar *cDirectory, const gchar *cListFileName, GHashTable *hProvidedTable, GError **erreur);

/** Get a list of packages from differente sources.
*@param cSharePackagesDir path of a local folder containg packages or NULL.
*@param cUserPackagesDir path of a user folder containg packages or NULL.
*@param cDistantPackagesDir path of a distant folder containg packages or NULL.
*@return a hash table of (name, #_CairoDockPackage). Free it with g_hash_table_destroy when you're done with it.
*/
GHashTable *cairo_dock_list_packages (const gchar *cSharePackagesDir, const gchar *cUserPackagesDir, const gchar *cDistantPackagesDir);

/** Asynchronously get a list of packages from differente sources. This function is non-blocking, you'll get a CairoTask that you can discard at any time, and you'll get a hash-table of the packages as the first argument of the callback (the second being the data you passed to this function).
*@param cSharePackagesDir path of a local folder containg packages or NULL.
*@param cUserPackagesDir path of a user folder containg packages or NULL.
*@param cDistantPackagesDir path of a distant folder containg packages or NULL.
*@param pCallback function called when the listing is finished. It takes the hash-table of the found packages (it belongs to the task so don't free it) and the data you've set here.
*@param data data to be passed to the callback.
*@return the Task that is doing the job. Keep it and use \ref cairo_dock_discard_task whenever you want to discard the download (for instance if the user cancels it), or \ref cairo_dock_free_task inside your callback.
*/
CairoDockTask *cairo_dock_list_packages_async (const gchar *cSharePackagesDir, const gchar *cUserPackagesDir, const gchar *cDistantPackagesDir, CairoDockGetPackagesFunc pCallback, gpointer data);


/** Look for a package with a given name into differente sources. If the package is found on the server and is not present on the disk, or is not up to date, then it is downloaded and the local path is returned.
*@param cPackageName name of the package.
*@param cSharePackagesDir path of a local folder containing packages or NULL.
*@param cUserPackagesDir path of a user folder containing packages or NULL.
*@param cDistantPackagesDir path of a distant folder containg packages or NULL.
*@param iGivenType type of package, or CAIRO_DOCK_ANY_PACKAGE if any type of package should be considered.
*@return a newly allocated string containing the complete local path of the package. If the package is distant, it is downloaded and extracted into this folder.
*/
gchar *cairo_dock_get_package_path (const gchar *cPackageName, const gchar *cSharePackagesDir, const gchar *cUserPackagesDir, const gchar *cDistantPackagesDir, CairoDockPackageType iGivenType);

CairoDockPackageType cairo_dock_extract_package_type_from_name (const gchar *cPackageName);


void cairo_dock_init_package_manager (gchar *cPackageServerAdress);


G_END_DECLS
#endif

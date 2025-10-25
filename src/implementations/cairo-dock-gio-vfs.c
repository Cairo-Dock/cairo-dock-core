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


#include "cairo-dock-gio-vfs.h"

#include <string.h>
#include <stdlib.h>

#include "cairo-dock-file-manager.h"

#include <glib.h>
// Note: Gio is present from GLib >= 2.16, so we can assume it is always available
#include <gio/gio.h>
//#define G_VFS_DBUS_DAEMON_NAME "org.gtk.vfs.Daemon" -- see comment below


#include <cairo-dock-log.h>
#include <cairo-dock-utils.h>
#include <cairo-dock-container.h>
#include <cairo-dock-class-manager.h>
#include <cairo-dock-icon-facility.h>
#include <cairo-dock-icon-manager.h>

extern GldiContainer *g_pPrimaryContainer;

static void _cairo_dock_gio_vfs_empty_dir (const gchar *cBaseURI);

static GHashTable *s_hMonitorHandleTable = NULL;

static void _gio_vfs_free_monitor_data (gpointer *data)
{
	if (data != NULL)
	{
		GFileMonitor *pHandle = data[2];
		g_file_monitor_cancel (pHandle);
		g_object_unref (pHandle);
		g_free (data);
	}
}

static void _fill_backend (CairoDockDesktopEnvBackend *pVFSBackend);
void gldi_register_gio_vfs_backend (void)
{
	/** TODO: the DBus service might be needed for mounting volumes to
	 * work properly. However, it is unclear, as g_file_mount_mountable ()
	 * does not explicitly depend on this, and newer GLib versions could
	 * work without this being present. Also, we don't have a fallback,
	 * so it is OK to always just try using the GIO interfaces.
	if (bNeedDbus)
	{
		if( !cairo_dock_dbus_is_enabled() ||
			!cairo_dock_dbus_detect_application (G_VFS_DBUS_DAEMON_NAME) )
		{
			cd_warning("VFS Daemon NOT found on DBus !");
		  return FALSE;
		}
		cd_message("VFS Daemon found on DBus.");
	}
	*/
	
	if (s_hMonitorHandleTable != NULL)
		g_hash_table_destroy (s_hMonitorHandleTable);
	
	s_hMonitorHandleTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		(GDestroyNotify) _gio_vfs_free_monitor_data);
	
	CairoDockDesktopEnvBackend backend = { NULL };
	_fill_backend (&backend);
	// FALSE: do not overwrite any functions already registered (by the
	// *-integration plugins, although it is likely we run before them)
	cairo_dock_fm_register_vfs_backend (&backend, FALSE);
}

/**
 * cTargetURI is the URI which is represented by the icon: for cases where
 * the icon is located in the same directory (e.g. CD or DVD)
 */
static gchar *_cd_get_icon_path (GIcon *pIcon, const gchar *cTargetURI)
{
	//g_print ("%s ()\n", __func__);
	gchar *cIconPath = NULL;
	if (G_IS_THEMED_ICON (pIcon))
	{
		const gchar * const *cFileNames = g_themed_icon_get_names (G_THEMED_ICON (pIcon));
		int i;
		for (i = 0; cFileNames[i] != NULL && cIconPath == NULL; i ++)
		{
			gchar *path = cairo_dock_search_icon_s_path (cFileNames[i], CAIRO_DOCK_DEFAULT_ICON_SIZE);
			if (path)
			{
				g_free (path);
				cIconPath = g_strdup (cFileNames[i]);
			}
		}
	}
	else if (G_IS_FILE_ICON (pIcon))
	{
		GFile *pFile = g_file_icon_get_file (G_FILE_ICON (pIcon));
		cIconPath = g_file_get_basename (pFile);
		//g_print (" file_icon => %s\n", cIconPath);
		
		if (cTargetURI && cIconPath && g_str_has_suffix (cIconPath, ".ico"))  // mount of ISO file
		{
			gchar *tmp = cIconPath;
			cIconPath = g_strdup_printf ("%s/%s", cTargetURI, tmp);
			g_free (tmp);
			if (strncmp (cIconPath, "file://", 7) == 0)
			{
				tmp = cIconPath;
				cIconPath = g_filename_from_uri (tmp, NULL, NULL);
				g_free (tmp);
			}
		}
	}
	return cIconPath;
}

static GDrive *_cd_find_drive_from_name (const gchar *cName)
{
	g_return_val_if_fail (cName != NULL, NULL);
	cd_message ("%s (%s)", __func__, cName);
	GVolumeMonitor *pVolumeMonitor = g_volume_monitor_get ();
	GDrive *pFoundDrive = NULL;
	
	gchar *str = strrchr (cName, '-');
	if (str)
		*str = '\0';
	
	//\___________________ We get connected disks (CD reader, etc.) and we list their volumes.
	GList *pDrivesList = g_volume_monitor_get_connected_drives (pVolumeMonitor);
	GList *dl;
	GDrive *pDrive;
	gchar *cDriveName;
	for (dl = pDrivesList; dl != NULL; dl = dl->next)
	{
		pDrive = dl->data;
		if (pFoundDrive == NULL)
		{
			cDriveName = g_drive_get_name  (pDrive);
			cd_message ("  drive '%s'", cDriveName);
			if (cDriveName != NULL && strcmp (cDriveName, cName) == 0)
				pFoundDrive = pDrive;
			else
				g_object_unref (pDrive);
			g_free (cDriveName);
		}
		else
			g_object_unref (pDrive);
	}
	g_list_free (pDrivesList);
	if (str)
		*str = '-';
	return pFoundDrive;
}
static gchar *_cd_find_volume_name_from_drive_name (const gchar *cName)
{
	g_return_val_if_fail (cName != NULL, NULL);
	cd_message ("%s (%s)", __func__, cName);
	GDrive *pDrive = _cd_find_drive_from_name (cName);
	g_return_val_if_fail (pDrive != NULL, NULL);
	
	gchar *cVolumeName = NULL;
	GList *pAssociatedVolumes = g_drive_get_volumes (pDrive);
	g_object_unref (pDrive);
	if (pAssociatedVolumes == NULL)
		return NULL;
	
	int iNumVolume;
	gchar *str = strrchr (cName, '-');
	if (str)
	{
		iNumVolume = atoi (str+1);
	}
	else
		iNumVolume = 0;
	
	GVolume *pVolume = g_list_nth_data (pAssociatedVolumes, iNumVolume);
	if (pVolume != NULL)
	{
		cVolumeName = g_volume_get_name (pVolume);
	}
	cd_debug ("%dth volume -> cVolumeName : %s", iNumVolume, cVolumeName);
	
	/* cd_debug ("List of unavailable volumes on this disc:");
	GList *av;
	gchar *cLog;
	for (av = pAssociatedVolumes; av != NULL; av = av->next)
	{
		pVolume = av->data;
		cLog = g_volume_get_name (pVolume);
		cd_debug ("  - %s", cLog);
		g_free (cLog);
	}*/
	
	g_list_free_full (pAssociatedVolumes, g_object_unref);
	
	return cVolumeName;
}
static gboolean _cd_find_can_eject_from_drive_name (const gchar *cName)
{
	cd_debug ("%s (%s)", __func__, cName);
	GDrive *pDrive = _cd_find_drive_from_name (cName);
	g_return_val_if_fail (pDrive != NULL, FALSE);
	
	gboolean bCanEject = g_drive_can_eject (pDrive);
	g_object_unref (pDrive);
	return bCanEject;
}

static void cairo_dock_gio_vfs_get_file_info (const gchar *cBaseURI, gchar **cName, gchar **cURI, gchar **cIconName, gboolean *bIsDirectory, int *iVolumeID, double *fOrder, CairoDockFMSortType iSortType)
{
	*cName = NULL;
	*cURI = NULL;
	*cIconName = NULL;
	g_return_if_fail (cBaseURI != NULL);
	GError *erreur = NULL;
	cd_message ("%s (%s)", __func__, cBaseURI);
	
	// make it a valid URI.
	gchar *cValidUri;
	if (g_str_has_prefix (cBaseURI, "x-nautilus-desktop://"))  // shortcut on the desktop (nautilus)
	{
		const gchar *cDesktopPath = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
		cValidUri = g_strdup_printf ("file://%s%s", cDesktopPath, cBaseURI+21);
	}
	else  // normal file
	{
		if (*cBaseURI == '/')
			cValidUri = g_filename_to_uri (cBaseURI, NULL, NULL);
		else
			cValidUri = g_strdup (cBaseURI);
		// strange case when unmounting a FTP bookmark when it's not available: crash with DBus
		if (*cBaseURI == ':' || *cValidUri == ':')
		{
			cd_warning ("invalid URI (%s ; %s), skip it", cBaseURI, cValidUri);
			g_free (cValidUri);
			return;
		}
	}
	
	// get its attributes.
	GFile *pFile = g_file_new_for_uri (cValidUri);
	g_return_if_fail (pFile);
	const gchar *cQuery = G_FILE_ATTRIBUTE_STANDARD_TYPE","
		G_FILE_ATTRIBUTE_STANDARD_SIZE","
		G_FILE_ATTRIBUTE_TIME_MODIFIED","
		G_FILE_ATTRIBUTE_TIME_ACCESS","
		G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
		G_FILE_ATTRIBUTE_STANDARD_NAME","
		G_FILE_ATTRIBUTE_STANDARD_ICON","
		G_FILE_ATTRIBUTE_THUMBNAIL_PATH","
		G_FILE_ATTRIBUTE_STANDARD_TARGET_URI;
	GFileInfo *pFileInfo = g_file_query_info (pFile,
		cQuery,
		G_FILE_QUERY_INFO_NONE,  /// G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS
		NULL,
		&erreur);
	if (erreur != NULL)  // can happen if the file point is not mounted.
	{
		cd_debug ("gvfs-integration : %s", erreur->message);  // useless to have a warning.
		g_error_free (erreur);
		g_free (cValidUri);
		g_object_unref (pFile);
		return ;
	}
	
	const gchar *cFileName = g_file_info_get_name (pFileInfo);
	const gchar *cMimeType = g_file_info_get_content_type (pFileInfo);
	GFileType iFileType = g_file_info_get_file_type (pFileInfo);
	const gchar *cTargetURI = g_file_info_get_attribute_string (pFileInfo, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
	
	// find the order of the file
	if (fOrder)
	{
		if (iSortType == CAIRO_DOCK_FM_SORT_BY_DATE)
			*fOrder =  g_file_info_get_attribute_uint64 (pFileInfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		else if (iSortType == CAIRO_DOCK_FM_SORT_BY_ACCESS)
			*fOrder =  g_file_info_get_attribute_uint64 (pFileInfo, G_FILE_ATTRIBUTE_TIME_ACCESS);
		else if (iSortType == CAIRO_DOCK_FM_SORT_BY_SIZE)
			*fOrder = g_file_info_get_size (pFileInfo);
		else if (iSortType == CAIRO_DOCK_FM_SORT_BY_TYPE)
			*fOrder = (cMimeType != NULL ? *((int *) cMimeType) : 0);
		else
			*fOrder = 0;
	}
	
	if (bIsDirectory)
		*bIsDirectory = (iFileType == G_FILE_TYPE_DIRECTORY);
	
	
	// find a readable name if it's a mount point.
	if (iFileType == G_FILE_TYPE_MOUNTABLE)
	{
		if (iVolumeID)
			*iVolumeID = 1;
		
		if (cName)
		{
			*cName = NULL;
			
			cd_message ("  cTargetURI:%s", cTargetURI);
			GMount *pMount = NULL;
			if (cTargetURI != NULL)
			{
				GFile *file = g_file_new_for_uri (cTargetURI);
				pMount = g_file_find_enclosing_mount (file, NULL, NULL);
				g_object_unref (file);
			}
			if (pMount != NULL)
			{
				*cName = g_mount_get_name (pMount);
				cd_message ("a GMount exists (%s)",* cName);
				g_object_unref (pMount);
			}
			else
			{
				gchar *cMountName = g_strdup (cFileName);
				gchar *str = strrchr (cMountName, '.');  // we remove the extension ".volume" or ".drive".
				if (str != NULL)
				{
					*str = '\0';
					if (strcmp (str+1, "link") == 0)  // for the links, we take its name.
					{
						if (strcmp (cMountName, "root") == 0)  // we replace 'root' by a better name.
						{
							*cName = g_strdup (_("File System"));
						}
					}
					else if (strcmp (str+1, "drive") == 0)  // we're looking for a better name if possible.
					{
						gchar *cVolumeName = _cd_find_volume_name_from_drive_name (cMountName);
						if (cVolumeName != NULL)
						{
							*cName = cVolumeName;
						}
					}
				}
				if (*cName == NULL)
					*cName = cMountName;
				else
					g_free (cMountName);
			}
			if (*cName ==  NULL)
				*cName = g_strdup (cFileName);
		}
	}
	else  // for an normal file, just re-use the filename
	{
		if (iVolumeID)
			*iVolumeID = 0;
		if (cName)
			*cName = g_strdup (cFileName);
	}
	
	// find the target URI if it's a mount point
	if (cURI)
	{
		if (cTargetURI)
		{
			*cURI = g_strdup (cTargetURI);
			g_free (cValidUri);
			cValidUri = NULL;
		}
		else
			*cURI = cValidUri;
	}
	
	// find an icon.
	if (cIconName)
	{
		*cIconName = NULL;
		// first use an available thumbnail.
		*cIconName = g_strdup (g_file_info_get_attribute_byte_string (pFileInfo, G_FILE_ATTRIBUTE_THUMBNAIL_PATH));
		// if no thumbnail available and the file is an image, use it directly.
		if (*cIconName == NULL && cMimeType != NULL && strncmp (cMimeType, "image", 5) == 0)
		{
			gchar *cHostname = NULL;
			GError *erreur = NULL;
			gchar *cFilePath = g_filename_from_uri (*cURI, &cHostname, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
			}
			else if (cHostname == NULL || strcmp (cHostname, "localhost") == 0)  // we get thumbnails only for local files.
			{
				*cIconName = cFilePath;
				cairo_dock_remove_html_spaces (*cIconName);
			}
			g_free (cHostname);
		}
		// else, get the icon for the mime-types.
		if (*cIconName == NULL)
		{
			GIcon *pSystemIcon = g_file_info_get_icon (pFileInfo);
			if (pSystemIcon != NULL)
			{
				*cIconName = _cd_get_icon_path (pSystemIcon, cTargetURI ? cTargetURI : *cURI);
			}
		}
		cd_message ("cIconName : %s", *cIconName);
	}
	
	g_object_unref (pFileInfo);
	g_object_unref (pFile);
}

static GList *cairo_dock_gio_vfs_list_directory (const gchar *cBaseURI, CairoDockFMSortType iSortType, int iNewIconsGroup, gboolean bListHiddenFiles, int iNbMaxFiles, gchar **cValidUri)
{
	g_return_val_if_fail (cBaseURI != NULL, NULL);
	cd_message ("%s (%s)", __func__, cBaseURI);
	
	gchar *cURI;
	if (strcmp (cBaseURI, CAIRO_DOCK_FM_VFS_ROOT) == 0)
		cURI = g_strdup ("computer://");
	else if (strcmp (cBaseURI, CAIRO_DOCK_FM_NETWORK) == 0)
		cURI = g_strdup ("network://");
	else
		cURI = (*cBaseURI == '/' ? g_strconcat ("file://", cBaseURI, NULL) : g_strdup (cBaseURI));
	*cValidUri = cURI;
	
	GFile *pFile = g_file_new_for_uri (cURI);
	GError *erreur = NULL;
	const gchar *cAttributes = G_FILE_ATTRIBUTE_STANDARD_TYPE","
		G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
		G_FILE_ATTRIBUTE_STANDARD_NAME","
		G_FILE_ATTRIBUTE_STANDARD_ICON","
		G_FILE_ATTRIBUTE_THUMBNAIL_PATH","
		G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN","
		G_FILE_ATTRIBUTE_STANDARD_TARGET_URI;
	GFileEnumerator *pFileEnum = g_file_enumerate_children (pFile,
		cAttributes,
		G_FILE_QUERY_INFO_NONE,  /// G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS
		NULL,
		&erreur);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration : %s", erreur->message);
		g_error_free (erreur);
		g_object_unref (pFile);
		return NULL;
	}
	
	int iOrder = 0;
	int iNbFiles = 0;
	GList *pIconList = NULL;
	Icon *icon;
	GFileInfo *pFileInfo;
	do
	{
		pFileInfo = g_file_enumerator_next_file (pFileEnum, NULL, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("gvfs-integration : %s", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
			continue;
		}
		if (pFileInfo == NULL)
			break ;
		
		gboolean bIsHidden = g_file_info_get_is_hidden (pFileInfo);
		if (bListHiddenFiles || ! bIsHidden)
		{
			GFileType iFileType = g_file_info_get_file_type (pFileInfo);
			GIcon *pFileIcon = g_file_info_get_icon (pFileInfo);
			if (pFileIcon == NULL)
			{
				cd_message ("No ICON");
				continue;
			}
			const gchar *cFileName = g_file_info_get_name (pFileInfo);
			const gchar *cMimeType = g_file_info_get_content_type (pFileInfo);
			gchar *cName = NULL;
			
			icon = cairo_dock_create_dummy_launcher (NULL, NULL, NULL, NULL, 0);
			icon->iGroup = iNewIconsGroup;
			icon->cBaseURI = g_strconcat (*cValidUri, "/", cFileName, NULL);
			//g_print	 ("+ %s (mime:%s)n", icon->cBaseURI, cMimeType);
			
			if (iFileType == G_FILE_TYPE_MOUNTABLE)
			{
				const gchar *cTargetURI = g_file_info_get_attribute_string (pFileInfo, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
				cd_message ("It's a mount point: %s (%s)", cTargetURI, cFileName);
				
				GMount *pMount = NULL;
				if (cTargetURI != NULL)
				{
					icon->cCommand = g_strdup (cTargetURI);
					GFile *file = g_file_new_for_uri (cTargetURI);
					pMount = g_file_find_enclosing_mount (file, NULL, NULL);
					g_object_unref (file);
				}
				if (pMount != NULL)
				{
					cName = g_mount_get_name (pMount);
					cd_message ("GMount exists (%s)", cName);
					g_object_unref (pMount);
				}
				else
				{
					cName = g_strdup (cFileName);
					gchar *str = strrchr (cName, '.');  // we remove the extension ".volume" or ".drive".
					if (str != NULL)
					{
						*str = '\0';
						if (strcmp (str+1, "link") == 0)
						{
							if (strcmp (cName, "root") == 0)
							{
								g_free (cName);
								cName = g_strdup (_("File System"));
							}
						}
						else if (strcmp (str+1, "drive") == 0)  // we are looking for a better name.
						{
							gchar *cVolumeName = _cd_find_volume_name_from_drive_name (cName);
							if (cVolumeName != NULL)
							{
								g_free (cName);
								cName = cVolumeName;
							}
						}
					}
				}
				icon->iVolumeID = 1;
				cd_message ("The name of this volume is: %s", cName);
			}
			else
			{
				if (iFileType == G_FILE_TYPE_DIRECTORY)
					icon->iVolumeID = -1;
				cName = g_strdup (cFileName);
			}
			
			if (icon->cCommand == NULL)
				icon->cCommand = g_strdup (icon->cBaseURI);
			icon->cName = cName;
			icon->cFileName = g_strdup (g_file_info_get_attribute_byte_string (pFileInfo, G_FILE_ATTRIBUTE_THUMBNAIL_PATH));
			if (cMimeType != NULL && strncmp (cMimeType, "image", 5) == 0)
			{
				gchar *cHostname = NULL;
				gchar *cFilePath = g_filename_from_uri (icon->cBaseURI, &cHostname, &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
					erreur = NULL;
				}
				else if (cHostname == NULL || strcmp (cHostname, "localhost") == 0)  // we get thumbnails only for local files.
				{
					icon->cFileName = g_strdup (cFilePath);
					cairo_dock_remove_html_spaces (icon->cFileName);
				}

				g_free (cHostname);
				g_free (cFilePath);
			}
			if (icon->cFileName == NULL)
			{
				icon->cFileName = _cd_get_icon_path (pFileIcon, icon->cCommand);
				//g_print ("icon->cFileName : %s\n", icon->cFileName);
			}
			
			if (iSortType == CAIRO_DOCK_FM_SORT_BY_SIZE)
				icon->fOrder = g_file_info_get_size (pFileInfo);
			else if (iSortType == CAIRO_DOCK_FM_SORT_BY_DATE)
				icon->fOrder = g_file_info_get_attribute_uint64 (pFileInfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);
			else if (iSortType == CAIRO_DOCK_FM_SORT_BY_TYPE)
				icon->fOrder = (cMimeType != NULL ? *((int *) cMimeType) : 0);
			if (icon->fOrder == 0)  // not so good but better than nothing, no?
				icon->fOrder = iOrder;
			pIconList = g_list_prepend (pIconList, icon);
			cd_debug (" + %s (%s)", icon->cName, icon->cFileName);
			iOrder ++;
			iNbFiles ++;
		}
		g_object_unref (pFileInfo);
	} while (iNbFiles < iNbMaxFiles);
	
	g_object_unref (pFileEnum);
	g_object_unref (pFile);
	
	if (iSortType == CAIRO_DOCK_FM_SORT_BY_NAME)
		pIconList = cairo_dock_sort_icons_by_name (pIconList);
	else
		pIconList = cairo_dock_sort_icons_by_order (pIconList);
	
	return pIconList;
}

static gsize cairo_dock_gio_vfs_measure_directory (const gchar *cBaseURI, gint iCountType, gboolean bRecursive, gint *pCancel)
{
	g_return_val_if_fail (cBaseURI != NULL, 0);
	//cd_debug ("%s (%s)", __func__, cBaseURI);
	
	gchar *cURI = (*cBaseURI == '/' ? g_strconcat ("file://", cBaseURI, NULL) : (gchar*)cBaseURI);  // we free it at the end if needed.
	
	GFile *pFile = g_file_new_for_uri (cURI);
	GError *erreur = NULL;
	const gchar *cAttributes = G_FILE_ATTRIBUTE_STANDARD_TYPE","
		G_FILE_ATTRIBUTE_STANDARD_SIZE","
		G_FILE_ATTRIBUTE_STANDARD_NAME","
		G_FILE_ATTRIBUTE_STANDARD_TARGET_URI;
	GFileEnumerator *pFileEnum = g_file_enumerate_children (pFile,
		cAttributes,
		G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
		NULL,
		&erreur);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration: %s (%s)", erreur->message, cURI);
		g_error_free (erreur);
		g_object_unref (pFile);
		if (cURI != cBaseURI)
			g_free (cURI);
		g_atomic_int_set (pCancel, TRUE);
		return 0;
	}
	
	gsize iMeasure = 0;
	GFileInfo *pFileInfo;
	GString *sFilePath = g_string_new ("");
	do
	{
		pFileInfo = g_file_enumerator_next_file (pFileEnum, NULL, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("gvfs-integration : %s (%s [%s]: %s)", erreur->message,
				g_file_info_get_name (pFileInfo),
				g_file_info_get_display_name (pFileInfo),
				g_file_info_get_content_type (pFileInfo));
			g_error_free (erreur);
			erreur = NULL;
			continue;
		}
		if (pFileInfo == NULL)
			break ;
		
		const gchar *cFileName = g_file_info_get_name (pFileInfo);
		
		g_string_printf (sFilePath, "%s/%s", cURI, cFileName);
		GFileType iFileType = g_file_info_get_file_type (pFileInfo);
		
		if (iFileType == G_FILE_TYPE_DIRECTORY && bRecursive)
		{
			g_string_printf (sFilePath, "%s/%s", cURI, cFileName);
			iMeasure += MAX (1, cairo_dock_gio_vfs_measure_directory (sFilePath->str, iCountType, bRecursive, pCancel));  // an empty dir == 1.
		}
		else
		{
			if (iCountType == 1)  // measure size.
			{
				iMeasure += g_file_info_get_size (pFileInfo);
			}
			else  // measure nb files.
			{
				iMeasure ++;
			}
		}
		g_object_unref (pFileInfo);
	} while (! g_atomic_int_get (pCancel));
	if (*pCancel)
		cd_debug ("measure cancelled");
	
	g_object_unref (pFileEnum);
	g_object_unref (pFile);
	g_string_free (sFilePath, TRUE);
	if (cURI != cBaseURI)
		g_free (cURI);
	
	return iMeasure;
}


/***********************************************************************
 * launch_uri () and helpers -- this function tries to find the default
 * app to open an URI using the GAppInfo functions.
 * To ensure the dock stays responsive, we try to use the async version
 * of these functions if they are available (GLib >= 2.74).
 * However, this complicates the code somewhat as we have two paths and
 * also have to define the callbacks for the async functions.
 * 
 * We do the search in three steps:
 *  1. We look at the URI scheme, using g_app_info_get_default_for_uri_scheme () / 
 * 		g_app_info_get_default_for_uri_scheme_async ()
 *  2. If no app is found, we try g_app_info_get_default_for_type () /
 * 		g_app_info_get_default_for_type_async ()
 *  3. If still no match, we use g_app_info_get_all_for_type ()
 * 		(this does not have an async version, but by this time, all
 * 		relevant information should be cached in Gio anyway).
 * Note: we do not have proper error checking, only whether we got a valid
 * GDesktopAppInfo and we could register a GldiAppInfo from it.
 */

static gchar *_cd_find_target_uri (const gchar *cBaseURI)
{
	GError *erreur = NULL;
	GFile *pFile = g_file_new_for_uri (cBaseURI);
	GFileInfo *pFileInfo = g_file_query_info (pFile,
		G_FILE_ATTRIBUTE_STANDARD_TARGET_URI,
		G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
		NULL,
		&erreur);
	g_object_unref (pFile);
	if (erreur != NULL)
	{
		cd_debug ("%s (%s) : %s", __func__, cBaseURI, erreur->message);  // can have a .mount, no warning.
		g_error_free (erreur);
		return NULL;
	}
	gchar *cTargetURI = g_strdup (g_file_info_get_attribute_string (pFileInfo, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI));
	g_object_unref (pFileInfo);
	return cTargetURI;
}

static gboolean _try_launch_app (GAppInfo *pAppInfo, const gchar *const *vURIs)
{
	if (pAppInfo && G_IS_DESKTOP_APP_INFO (pAppInfo))
	{
		GldiAppInfo *app = gldi_app_info_from_desktop_app_info (G_DESKTOP_APP_INFO (pAppInfo));
		if (app)
		{
			gldi_app_info_launch (app, vURIs);
			gldi_object_unref (GLDI_OBJECT (app));
			// TODO: error checking? (now we always assume success)
			return TRUE;
		}
	}
	return FALSE;
}

static void _got_default_for_type (GAppInfo *app, const gchar *cURI, const gchar *cMimeType)
{
	const gchar *vURIs[] = {cURI, NULL};
	
	gboolean bSuccess = _try_launch_app (app, vURIs);
	if (app) g_object_unref (app);
	
	if (! bSuccess)
	{
		cd_debug ("gvfs-integration : couldn't launch '%s' with its default app", cURI);
		
		GList *pAppsList = g_app_info_get_all_for_type (cMimeType);
		GAppInfo *pAppInfo;
		GList *a;
		for (a = pAppsList; a != NULL; a = a->next)
		{
			pAppInfo = a->data;
			if (_try_launch_app (pAppInfo, vURIs)) break;
		}
		g_list_free_full (pAppsList, g_object_unref);
	}
}

#if GLIB_CHECK_VERSION(2, 74, 0)
struct _AsyncTypeData
{
	gchar *cURI;
	gchar *cMimeType;
};

static void _got_default_for_type_async (G_GNUC_UNUSED GObject* pObj, GAsyncResult *pRes, gpointer ptr)
{
	struct _AsyncTypeData *data = (struct _AsyncTypeData*)ptr;
	GAppInfo *pAppInfo = g_app_info_get_default_for_type_finish (pRes, NULL);
	_got_default_for_type (pAppInfo, data->cURI, data->cMimeType);
	g_free (data->cURI);
	g_free (data->cMimeType);
	g_free (data);
}
#endif

static void _launch_uri_mime_type (gchar *cURI)
{
	// get the mime-type.
	GError *erreur = NULL;
	GFile *pFile = ((*cURI != '/') ? g_file_new_for_uri (cURI) : g_file_new_for_path (cURI));
	const gchar *cQuery = G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;
	GFileInfo *pFileInfo = g_file_query_info (pFile,
		cQuery,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&erreur);
	if (erreur != NULL)  // if no mime-type (can happen with not mounted volumes), abort.
	{
		cd_warning ("gvfs-integration : %s", erreur->message);
		g_error_free (erreur);
		g_object_unref (pFile);
		g_free (cURI);
		return;
	}
	const gchar *cMimeType = g_file_info_get_content_type (pFileInfo);
	
	if (cMimeType)
	{
#if GLIB_CHECK_VERSION(2, 74, 0)
		struct _AsyncTypeData *data = g_new0 (struct _AsyncTypeData, 1);
		data->cURI = (gchar*)cURI;
		data->cMimeType = g_strdup (cMimeType);
		g_app_info_get_default_for_type_async (cMimeType, FALSE, NULL, _got_default_for_type_async, (gpointer)data);
		cURI = NULL;
#else
		_got_default_for_type (g_app_info_get_default_for_type (cMimeType, FALSE), cURI, cMimeType);
#endif
	}

	g_object_unref (pFileInfo);
	g_object_unref (pFile);
	g_free (cURI);
}

#if GLIB_CHECK_VERSION(2, 74, 0)
static void _got_default_for_uri_scheme_async (G_GNUC_UNUSED GObject* pObj, GAsyncResult *pRes, gpointer data)
{
	gchar *cURI = (gchar*)data;
	gboolean bSuccess = FALSE;
	GAppInfo *pAppInfo = g_app_info_get_default_for_uri_scheme_finish (pRes, NULL);
	if (pAppInfo)
	{
		const gchar *vURIs[] = {cURI, NULL};
		bSuccess = _try_launch_app (pAppInfo, vURIs);
		g_object_unref (pAppInfo);
	}
	
	if (! bSuccess) _launch_uri_mime_type (cURI);
	else g_free (cURI);
}
#endif

static void cairo_dock_gio_vfs_launch_uri (const gchar *cURI)
{
	g_return_if_fail (cURI != NULL);
	
	// in case it's a mount point, take the URI that can actually be launched. 
	gchar *cTargetURI = NULL;
	if (strstr (cURI, "://"))
	{
		cTargetURI = _cd_find_target_uri (cURI);
		if (cTargetURI) cURI = cTargetURI;
	}
	
	// now, try to launch it with the default program know by gvfs.
	gchar *cURIScheme = g_uri_parse_scheme (cURI);
	gboolean bSuccess = FALSE;
	
#if GLIB_CHECK_VERSION(2, 74, 0)
	if (cURIScheme && *cURIScheme)
	{
		g_app_info_get_default_for_uri_scheme_async (cURIScheme, NULL, _got_default_for_uri_scheme_async,
			cTargetURI ? cTargetURI : g_strdup (cURI));
		g_free (cURIScheme);
		return;
	}
#else
	if (cURIScheme && *cURIScheme)
	{
		GAppInfo *pAppDefault = g_app_info_get_default_for_uri_scheme (cURIScheme);
		if (pAppDefault)
		{
			const gchar *vURIs[] = {cURI, NULL};
			bSuccess = _try_launch_app (pAppDefault, vURIs);
			g_object_unref (pAppDefault);
		}
	}
#endif
	g_free (cURIScheme);
	
	if (! bSuccess) _launch_uri_mime_type (cTargetURI ? cTargetURI : g_strdup (cURI));
	else g_free (cTargetURI);
}

static GMount *_cd_find_mount_from_uri (const gchar *cURI, gchar **cTargetURI)
{
	cd_message ("%s (%s)", __func__, cURI);
	gchar *_cTargetURI = _cd_find_target_uri (cURI);
	
	GMount *pMount = NULL;
	if (_cTargetURI != NULL)
	{
		cd_debug ("  points to %s", _cTargetURI);
		GFile *file = g_file_new_for_uri (_cTargetURI);
		pMount = g_file_find_enclosing_mount (file, NULL, NULL);
		g_object_unref (file);
	}
	if (cTargetURI != NULL)
		*cTargetURI = _cTargetURI;
	else
		g_free (_cTargetURI);
	return pMount;
}

static gchar *cairo_dock_gio_vfs_is_mounted (const gchar *cURI, gboolean *bIsMounted)
{
	cd_message ("%s (%s)", __func__, cURI);
	gchar *cTargetURI = NULL;
	
	GFile *pFile = g_file_new_for_uri (cURI);
	GFileType iType = g_file_query_file_type (pFile, G_FILE_QUERY_INFO_NONE, NULL);
	g_object_unref (pFile);
	cd_debug ("iType: %d\n", iType);
	
	if (iType == G_FILE_TYPE_MOUNTABLE)  // look for the mount point it targets
	{
		GMount *pMount = _cd_find_mount_from_uri (cURI, &cTargetURI);
		cd_debug (" cTargetURI : %s", cTargetURI);
		if (pMount != NULL)
		{
			*bIsMounted = TRUE;
			g_object_unref (pMount);
		}
		else
		{
			if (cTargetURI != NULL && strcmp (cTargetURI, "file:///") == 0)  // special case?
				*bIsMounted = TRUE;
			else
				*bIsMounted = FALSE;
		}
	}
	else if (iType == G_FILE_TYPE_UNKNOWN)  // probably a remote share folder or whatever that is not yet mounted
	{
		*bIsMounted = FALSE;
	}
	else  // any other types (directory, regular file, etc) that is on (or point to) a mounted volume.
	{
		*bIsMounted = TRUE;
	}
	return cTargetURI;
}

static gchar * _cd_find_drive_name_from_URI (const gchar *cURI)
{
	g_return_val_if_fail (cURI != NULL, NULL);
	if (strncmp (cURI, "computer:///", 12) == 0)
	{
		gchar *cDriveName = g_strdup (cURI+12);
		gchar *str = strrchr (cDriveName, '.');
		if (str != NULL)
		{
			if (strcmp (str+1, "drive") == 0)
			{
				*str = '\0';
				while (1)
				{
					str = strchr (cDriveName, '\\');
					if (str == NULL)
						break;
					*str = '/';
				}
				return cDriveName;
			}
		}
		g_free (cDriveName);
	}
	return NULL;
}
static gboolean cairo_dock_gio_vfs_can_eject (const gchar *cURI)
{
	cd_message ("%s (%s)", __func__, cURI);
	gchar *cDriveName = _cd_find_drive_name_from_URI (cURI);
	if (cDriveName == NULL)
		return FALSE;
	
	gboolean bCanEject = _cd_find_can_eject_from_drive_name (cDriveName);
	//g_free (cDriveName);
	return bCanEject;
}
static gboolean cairo_dock_gio_vfs_eject_drive (const gchar *cURI)
{
	cd_message ("%s (%s)", __func__, cURI);
	gchar *cDriveName = _cd_find_drive_name_from_URI (cURI);
	GDrive *pDrive = _cd_find_drive_from_name (cDriveName);
	if (pDrive != NULL)
	{
		g_drive_eject_with_operation (pDrive,
			G_MOUNT_UNMOUNT_NONE,
			NULL,
			NULL,
			NULL,
			NULL);
	}
	g_object_unref (pDrive);
	g_free (cDriveName);
	return TRUE;
}


static void _gio_vfs_mount_callback (gpointer pObject, GAsyncResult *res, gpointer *data)
{
	cd_message ("%s (%d)", __func__, GPOINTER_TO_INT (data[1]));
	
	CairoDockFMMountCallback pCallback = data[0];
	
	GError *erreur = NULL;
	gboolean bSuccess;
	if (GPOINTER_TO_INT (data[1]) == 1)
	{
		if (data[5])
			bSuccess = (g_file_mount_mountable_finish (G_FILE (pObject), res, &erreur) != NULL);
		else
			bSuccess = g_file_mount_enclosing_volume_finish (G_FILE (pObject), res, &erreur);
	}
	else if (GPOINTER_TO_INT (data[1]) == 0)
		bSuccess = g_mount_unmount_with_operation_finish (G_MOUNT (pObject), res, &erreur);
	else
		bSuccess = g_mount_eject_with_operation_finish (G_MOUNT (pObject), res, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration : %s", erreur->message);
		g_error_free (erreur);
	}
	
	cd_message ("(un)mounted -> %d", bSuccess);
	if (pCallback != NULL)
		pCallback (GPOINTER_TO_INT (data[1]) == 1, bSuccess, data[2], data[3], data[4]);
	g_free (data[2]);
	g_free (data[3]);
	g_free (data);
}

static void cairo_dock_gio_vfs_mount (const gchar *cURI, G_GNUC_UNUSED int iVolumeID, CairoDockFMMountCallback pCallback, gpointer user_data)
{
	g_return_if_fail (cURI != NULL);
	cd_message ("%s (%s)", __func__, cURI);
	
	gchar *cTargetURI = _cd_find_target_uri (cURI);
	GFile *pFile = g_file_new_for_uri (cURI);
	
	gpointer *data = g_new (gpointer, 6);  // freed in the callback.
	data[0] = pCallback;
	data[1] = GINT_TO_POINTER (1);  // mount
	data[2] = (cTargetURI ? g_path_get_basename (cTargetURI) : g_strdup (cURI));
	data[3] = g_strdup (cURI);
	data[4] = user_data;
	
	GMountOperation *mount_op = gtk_mount_operation_new (GTK_WINDOW (g_pPrimaryContainer->pWidget));
	g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
	
	GFileType iType = g_file_query_file_type (pFile, G_FILE_QUERY_INFO_NONE, NULL);
	cd_debug ("iType: %d\n", iType);
	if (iType == G_FILE_TYPE_MOUNTABLE)
	{
		data[5] = GINT_TO_POINTER (1);
		g_file_mount_mountable  (pFile,
			G_MOUNT_MOUNT_NONE,
			mount_op,
			NULL,
			(GAsyncReadyCallback) _gio_vfs_mount_callback,
			data);
	}
	else  // smb share folders typically have an unknown type; we have to use the other mount function for these types of mount points.
	{
		data[5] = GINT_TO_POINTER (0);
		g_file_mount_enclosing_volume (pFile,
			G_MOUNT_MOUNT_NONE,
			mount_op,
			NULL,
			(GAsyncReadyCallback) _gio_vfs_mount_callback,
			data);
	}
	// unref mount_op here - g_file_mount_enclosing_volume() does ref for itself
	g_object_unref (mount_op);
	g_object_unref (pFile);
	g_free (cTargetURI);
}

static void cairo_dock_gio_vfs_unmount (const gchar *cURI, G_GNUC_UNUSED int iVolumeID, CairoDockFMMountCallback pCallback, gpointer user_data)
{
	g_return_if_fail (cURI != NULL);
	cd_message ("%s (%s)", __func__, cURI);
	
	gchar *cTargetURI = NULL;
	GMount *pMount = _cd_find_mount_from_uri (cURI, &cTargetURI);
	if (pMount == NULL || ! G_IS_MOUNT (pMount))
	{
		return ;
	}
	
	if ( ! g_mount_can_unmount (pMount))
		return ;
	
	gboolean bCanEject = g_mount_can_eject (pMount);
	gboolean bCanUnmount = g_mount_can_unmount (pMount);
	cd_message ("eject:%d / unmount:%d", bCanEject, bCanUnmount);
	if (! bCanEject && ! bCanUnmount)
	{
		cd_warning ("can't unmount this volume (%s)", cURI);
		return ;
	}
	
	gpointer *data = g_new (gpointer, 5);
	data[0] = pCallback;
	data[1] = GINT_TO_POINTER (bCanEject ? 2 : 0);
	data[2] = g_mount_get_name (pMount);
	data[3] = g_strdup (cURI);
	data[4] = user_data;
	if (bCanEject)
		g_mount_eject_with_operation (pMount,
			G_MOUNT_UNMOUNT_NONE,
			NULL,
			NULL,
			(GAsyncReadyCallback) _gio_vfs_mount_callback,
			data);
	else
		g_mount_unmount_with_operation (pMount,
			G_MOUNT_UNMOUNT_NONE,
			NULL,
			NULL,
			(GAsyncReadyCallback) _gio_vfs_mount_callback,
			data);
}


static void _on_monitor_changed (G_GNUC_UNUSED GFileMonitor *monitor,
	GFile *file,
	G_GNUC_UNUSED GFile *other_file,
	GFileMonitorEvent event_type,
	gpointer  *data)
{
	CairoDockFMMonitorCallback pCallback = data[0];
	gpointer user_data = data[1];
	cd_message ("%s (%d , data : %x)", __func__, event_type, user_data);
	
	CairoDockFMEventType iEventType;
	switch (event_type)
	{
		///case G_FILE_MONITOR_EVENT_CHANGED :  // ignore this one should avoid most useless signals sent by gvfs.
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT :
		//case G_FILE_MONITOR_EVENT_UNMOUNTED : // pertinent ?...
			iEventType = CAIRO_DOCK_FILE_MODIFIED;
			cd_message ("modification of a file");
		break;
		
		case G_FILE_MONITOR_EVENT_DELETED :
			iEventType = CAIRO_DOCK_FILE_DELETED;
			cd_message ("a file has been removed");
		break;
		
		case G_FILE_MONITOR_EVENT_CREATED :
			iEventType = CAIRO_DOCK_FILE_CREATED;
			cd_message ("creation of a file");
		break;
		
		default :
		return ;
	}
	gchar *cURI = g_file_get_uri (file);
	cd_message (" it's this file: %s", cURI);
	gchar *cPath = NULL;
	if (strncmp (cURI, "computer://", 11) == 0)
	{
		if (event_type == G_FILE_MONITOR_EVENT_CHANGED)
		{
			g_free (cURI);
			return ;
		}
		memcpy (cURI+4, "file", 4);
		cPath = g_filename_from_uri (cURI+4, NULL, NULL);
		cd_debug(" (path:%s)", cPath);
		g_free (cURI);
		cURI = g_strdup_printf ("computer://%s", cPath);
		cd_message ("its complete URI is: %s", cURI);
	}
	
	pCallback (iEventType, cURI, user_data);
	g_free (cURI);
}


static void cairo_dock_gio_vfs_add_monitor (const gchar *cURI, gboolean bDirectory, CairoDockFMMonitorCallback pCallback, gpointer user_data)
{
	g_return_if_fail (cURI != NULL);
	GError *erreur = NULL;
	GFileMonitor *pMonitor;
	GFile *pFile = (*cURI == '/' ? g_file_new_for_path (cURI) : g_file_new_for_uri (cURI));
	if (bDirectory)
		pMonitor = g_file_monitor_directory (pFile,
			G_FILE_MONITOR_WATCH_MOUNTS,
			NULL,
			&erreur);
	else
		pMonitor = g_file_monitor_file (pFile,
			G_FILE_MONITOR_WATCH_MOUNTS,
			NULL,
			&erreur);
	g_object_unref (pFile);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration : couldn't add monitor on '%s' (%d) [%s]", cURI, bDirectory, erreur->message);
		g_error_free (erreur);
		return ;
	}
	
	gpointer *data = g_new0 (gpointer, 3);
	data[0] = pCallback;
	data[1] = user_data;
	data[2] = pMonitor;
	g_signal_connect (G_OBJECT (pMonitor), "changed", G_CALLBACK (_on_monitor_changed), data);
	
	g_hash_table_insert (s_hMonitorHandleTable, g_strdup (cURI), data);
	cd_message (">>> monitor added to %s (%x)", cURI, user_data);
}

static void cairo_dock_gio_vfs_remove_monitor (const gchar *cURI)
{
	if (cURI != NULL)
	{
		cd_message (">>> monitor removed from %s", cURI);
		g_hash_table_remove (s_hMonitorHandleTable, cURI);
	}
}



static gboolean cairo_dock_gio_vfs_delete_file (const gchar *cURI, gboolean bNoTrash)
{
	g_return_val_if_fail (cURI != NULL, FALSE);
	GFile *pFile = (*cURI == '/' ? g_file_new_for_path (cURI) : g_file_new_for_uri (cURI));
	
	GError *erreur = NULL;
	gboolean bSuccess;
	if (bNoTrash)
	{
		const gchar *cQuery = G_FILE_ATTRIBUTE_STANDARD_TYPE;
		GFileInfo *pFileInfo = g_file_query_info (pFile,
			cQuery,
			G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
			NULL,
			&erreur);
		if (erreur != NULL)
		{
			cd_warning ("gvfs-integration : %s", erreur->message);
			g_error_free (erreur);
			g_object_unref (pFile);
			return FALSE;
		}
		
		GFileType iFileType = g_file_info_get_file_type (pFileInfo);
		if (iFileType == G_FILE_TYPE_DIRECTORY)
		{
			_cairo_dock_gio_vfs_empty_dir (cURI);
		}
		
		bSuccess = g_file_delete (pFile, NULL, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("gvfs-integration : %s", erreur->message);
			g_error_free (erreur);
		}
	}
	else
	{
		bSuccess = g_file_trash (pFile, NULL, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("gvfs-integration : %s", erreur->message);
			g_error_free (erreur);
		}
	}
	g_object_unref (pFile);
	return bSuccess;
}

static gboolean cairo_dock_gio_vfs_rename_file (const gchar *cOldURI, const gchar *cNewName)
{
	g_return_val_if_fail (cOldURI != NULL, FALSE);
	GFile *pOldFile = (*cOldURI == '/' ? g_file_new_for_path (cOldURI) : g_file_new_for_uri (cOldURI));
	GError *erreur = NULL;
	GFile *pNewFile = g_file_set_display_name (pOldFile, cNewName, NULL, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration : %s", erreur->message);
		g_error_free (erreur);
	}
	gboolean bSuccess = (pNewFile != NULL);
	if (pNewFile != NULL)
		g_object_unref (pNewFile);
	g_object_unref (pOldFile);
	return bSuccess;
}

static gboolean cairo_dock_gio_vfs_move_file (const gchar *cURI, const gchar *cDirectoryURI)
{
	g_return_val_if_fail (cURI != NULL, FALSE);
	cd_message (" %s -> %s", cURI, cDirectoryURI);
	GFile *pFile = (*cURI == '/' ? g_file_new_for_path (cURI) : g_file_new_for_uri (cURI));
	
	gchar *cFileName = g_file_get_basename (pFile);
	gchar *cNewFileURI = g_strconcat (cDirectoryURI, "/", cFileName, NULL);  // not so good but ok...
	GFile *pDestinationFile = (*cNewFileURI == '/' ? g_file_new_for_path (cNewFileURI) : g_file_new_for_uri (cNewFileURI));
	g_free (cNewFileURI);
	g_free (cFileName);
	
	GError *erreur = NULL;
	gboolean bSuccess = g_file_move (pFile,
		pDestinationFile,
		G_FILE_COPY_NOFOLLOW_SYMLINKS,
		NULL,
		NULL,  // GFileProgressCallback
		NULL,  // data
		&erreur);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration : %s", erreur->message);
		g_error_free (erreur);
	}
	g_object_unref (pFile);
	g_object_unref (pDestinationFile);
	return bSuccess;
}

static gboolean cairo_dock_gio_vfs_create_file (const gchar *cURI, gboolean bDirectory)
{
	g_return_val_if_fail (cURI != NULL, FALSE);
	GFile *pFile = (*cURI == '/' ? g_file_new_for_path (cURI) : g_file_new_for_uri (cURI));
	
	GError *erreur = NULL;
	gboolean bSuccess = TRUE;
	if (bDirectory)
		g_file_make_directory_with_parents (pFile, NULL, &erreur);
	else
		g_file_create (pFile, G_FILE_CREATE_PRIVATE, NULL, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration : %s", erreur->message);
		g_error_free (erreur);
		bSuccess = FALSE;
	}
	g_object_unref (pFile);
	
	return bSuccess;
}

static void cairo_dock_gio_vfs_get_file_properties (const gchar *cURI, guint64 *iSize, time_t *iLastModificationTime, gchar **cMimeType, int *iUID, int *iGID, int *iPermissionsMask)
{
	g_return_if_fail (cURI != NULL);
	GFile *pFile = (*cURI == '/' ? g_file_new_for_path (cURI) : g_file_new_for_uri (cURI));
	GError *erreur = NULL;
	const gchar *cQuery = G_FILE_ATTRIBUTE_STANDARD_SIZE","
		G_FILE_ATTRIBUTE_TIME_MODIFIED","
		G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
		G_FILE_ATTRIBUTE_UNIX_UID","
		G_FILE_ATTRIBUTE_UNIX_GID","
		G_FILE_ATTRIBUTE_ACCESS_CAN_READ","
		G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE","
		G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE;
	GFileInfo *pFileInfo = g_file_query_info (pFile,
		cQuery,
		G_FILE_QUERY_INFO_NONE,  /// G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS
		NULL,
		&erreur);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration : couldn't get file properties for '%s' [%s]", cURI, erreur->message);
		g_error_free (erreur);
	}
	
	*iSize = g_file_info_get_attribute_uint64 (pFileInfo, G_FILE_ATTRIBUTE_STANDARD_SIZE);
	*iLastModificationTime = (time_t) g_file_info_get_attribute_uint64 (pFileInfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	*cMimeType = g_file_info_get_attribute_as_string (pFileInfo, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	*iUID = g_file_info_get_attribute_uint32 (pFileInfo, G_FILE_ATTRIBUTE_UNIX_UID);
	*iGID = g_file_info_get_attribute_uint32 (pFileInfo, G_FILE_ATTRIBUTE_UNIX_GID);
	gboolean r = g_file_info_get_attribute_boolean (pFileInfo, G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
	gboolean w = g_file_info_get_attribute_boolean (pFileInfo, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
	gboolean x = g_file_info_get_attribute_boolean (pFileInfo, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
	*iPermissionsMask = r * 8 * 8 + w * 8 + x;
	
	g_object_unref (pFileInfo);
	g_object_unref (pFile);
}


static gchar *cairo_dock_gio_vfs_get_trash_path (const gchar *cNearURI, gchar **cFileInfoPath)
{
	if (cNearURI == NULL)
		return g_strdup ("trash://");
	gchar *cPath = NULL;
	const gchar *xdgPath = g_getenv ("XDG_DATA_HOME");
	if (xdgPath != NULL)
	{
		cPath = g_strdup_printf ("%s/Trash/files", xdgPath);
		if (cFileInfoPath != NULL)
			*cFileInfoPath = g_strdup_printf ("%s/Trash/info", xdgPath);
	}
	else
	{
		cPath = g_strdup_printf ("%s/.local/share/Trash/files", g_getenv ("HOME"));
		if (cFileInfoPath != NULL)
			*cFileInfoPath = g_strdup_printf ("%s/.local/share/Trash/info", g_getenv ("HOME"));
	}
	return cPath;
}

static gchar *cairo_dock_gio_vfs_get_desktop_path (void)
{
	GFile *pFile = g_file_new_for_uri ("desktop://");
	gchar *cPath = g_file_get_path (pFile);
	g_object_unref (pFile);
	if (cPath == NULL)
		cPath = g_strdup_printf ("%s/Desktop", g_getenv ("HOME"));
	return cPath;
}

static void _cairo_dock_gio_vfs_empty_dir (const gchar *cBaseURI)
{
	if (cBaseURI == NULL)
		return ;
	
	GFile *pFile = (*cBaseURI == '/' ? g_file_new_for_path (cBaseURI) : g_file_new_for_uri (cBaseURI));
	GError *erreur = NULL;
	const gchar *cAttributes = G_FILE_ATTRIBUTE_STANDARD_TYPE","
		G_FILE_ATTRIBUTE_STANDARD_NAME;
	GFileEnumerator *pFileEnum = g_file_enumerate_children (pFile,
		cAttributes,
		G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
		NULL,
		&erreur);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration : %s", erreur->message);
		g_object_unref (pFile);
		g_error_free (erreur);
		return ;
	}
	
	GString *sFileUri = g_string_new ("");
	GFileInfo *pFileInfo;
	GFile *file;
	do
	{
		pFileInfo = g_file_enumerator_next_file (pFileEnum, NULL, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("gvfs-integration : %s", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
			continue;
		}
		if (pFileInfo == NULL)
			break ;
		
		GFileType iFileType = g_file_info_get_file_type (pFileInfo);
		const gchar *cFileName = g_file_info_get_name (pFileInfo);
		
		g_string_printf (sFileUri, "%s/%s", cBaseURI, cFileName);
		if (iFileType == G_FILE_TYPE_DIRECTORY)
		{
			_cairo_dock_gio_vfs_empty_dir (sFileUri->str);
		}
		
		file = (*cBaseURI == '/' ? g_file_new_for_path (sFileUri->str) : g_file_new_for_uri (sFileUri->str));
		g_file_delete (file, NULL, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("gvfs-integration : %s", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		g_object_unref (file);
		
		g_object_unref (pFileInfo);
	} while (1);
	
	g_string_free (sFileUri, TRUE);
	g_object_unref (pFileEnum);
	g_object_unref (pFile);
}

static inline int _convert_base16 (char c)
{
	int x;
	if (c >= '0' && c <= '9')
		x = c - '0';
	else
		x = 10 + (c - 'A');
	return x;
}
static void cairo_dock_gio_vfs_empty_trash (void)
{
	GFile *pFile = g_file_new_for_uri ("trash://");
	GError *erreur = NULL;
	const gchar *cAttributes = G_FILE_ATTRIBUTE_STANDARD_TARGET_URI","
		G_FILE_ATTRIBUTE_STANDARD_NAME","
		G_FILE_ATTRIBUTE_STANDARD_TYPE;
	GFileEnumerator *pFileEnum = g_file_enumerate_children (pFile,
		cAttributes,
		G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
		NULL,
		&erreur);
	if (erreur != NULL)
	{
		cd_warning ("gvfs-integration : %s", erreur->message);
		g_object_unref (pFile);
		g_error_free (erreur);
		return ;
	}
	
	GString *sFileUri = g_string_new ("");
	GFileInfo *pFileInfo;;
	do
	{
		pFileInfo = g_file_enumerator_next_file (pFileEnum, NULL, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("gvfs-integration : %s", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
			continue;
		}
		if (pFileInfo == NULL)
			break ;
		
		const gchar *cFileName = g_file_info_get_name (pFileInfo);
		//g_print (" - %s\n", cFileName);
		
		/* 2 cases: a file in the trash of the home dir or in a trash of
		 * another volume.
		 * name is like that: "\media\Fabounet2\.Trash-1000\files\t%C3%A8st%201"
		 * and the URI "trash:///%5Cmedia%5CFabounet2%5C.Trash-1000%5Cfiles%5Ct%25C3%25A8st%25201"
		 * But this URI doesn't work if there are non ASCII-7 chars in its name
		 * (bug in gio/gvfs ?). So we try to rebuild a path to this file.
		*/
		if (cFileName && *cFileName == '\\')
		{
			g_string_printf (sFileUri, "file://%s", cFileName);
			g_strdelimit (sFileUri->str, "\\", '/');
			//g_print ("   - %s\n", sFileUri->str);
			
			GFileType iFileType = g_file_info_get_file_type (pFileInfo);
			if (iFileType == G_FILE_TYPE_DIRECTORY)  // can't delete a non-empty folder located on a different volume than home.
			{
				_cairo_dock_gio_vfs_empty_dir (sFileUri->str);
			}
			GFile *file = g_file_new_for_uri (sFileUri->str);
			g_file_delete (file, NULL, &erreur);
			g_object_unref (file);
			
			gchar *str = g_strrstr (sFileUri->str, "/files/");
			if (str)
			{
				*str = '\0';
				gchar *cInfo = g_strdup_printf ("%s/info/%s.trashinfo", sFileUri->str, str+7);
				//g_print ("   - %s\n", cInfo);
				file = g_file_new_for_uri (cInfo);
				g_free (cInfo);
				g_file_delete (file, NULL, NULL);
				g_object_unref (file);
			}
		}
		else  // main trash: name like that: "tȿst 1" and the URI: "trash:///t%C3%A8st%201"
		{
			if (strchr (cFileName, '%'))  // if there is a % inside the name, it disturb gio, so let's remove it.
			{
				gchar *cTmpPath = g_strdup_printf ("/%s", cFileName);
				gchar *cEscapedFileName = g_filename_to_uri (cTmpPath, NULL, NULL);
				g_free (cTmpPath);
				g_string_printf (sFileUri, "trash://%s", cEscapedFileName+7);  // replace file:// with trash://
				g_free (cEscapedFileName);
			}
			else  // else it can handle the URI as usual.
				g_string_printf (sFileUri, "trash:///%s", cFileName);
			GFile *file = g_file_new_for_uri (sFileUri->str);
			g_file_delete (file, NULL, &erreur);
			g_object_unref (file);
		}
		if (erreur != NULL)
		{
			cd_warning ("gvfs-integration : %s", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		
		g_object_unref (pFileInfo);
	} while (1);
	
	g_string_free (sFileUri, TRUE);
	g_object_unref (pFileEnum);
	g_object_unref (pFile);
}

static GList *cairo_dock_gio_vfs_list_apps_for_file (const gchar *cBaseURI)
{
	gchar *cValidUri;
	if (*cBaseURI == '/')
		cValidUri = g_filename_to_uri (cBaseURI, NULL, NULL);
	else
		cValidUri = g_strdup (cBaseURI);
	GFile *pFile = g_file_new_for_uri (cValidUri);
	
	GError *erreur = NULL;
	const gchar *cQuery = G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;
	GFileInfo *pFileInfo = g_file_query_info (pFile,
		cQuery,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		&erreur);
	
	if (erreur != NULL)  // can happen if the volume is not mounted but we can notify the user via a warning.
	{
		cd_warning ("gvfs-integration : %s", erreur->message);
		g_error_free (erreur);
		g_free (cValidUri);
		g_object_unref (pFile);
		return NULL;
	}
	
	const gchar *cMimeType = g_file_info_get_content_type (pFileInfo);
	
	return g_app_info_get_all_for_type (cMimeType);
}

static void _fill_backend (CairoDockDesktopEnvBackend *pVFSBackend)
{
	if(pVFSBackend)
	{
		pVFSBackend->get_file_info = cairo_dock_gio_vfs_get_file_info;
		pVFSBackend->get_file_properties = cairo_dock_gio_vfs_get_file_properties;
		pVFSBackend->list_directory = cairo_dock_gio_vfs_list_directory;
		pVFSBackend->measure_directory = cairo_dock_gio_vfs_measure_directory;
		pVFSBackend->launch_uri = cairo_dock_gio_vfs_launch_uri;
		pVFSBackend->is_mounted = cairo_dock_gio_vfs_is_mounted;
		pVFSBackend->can_eject = cairo_dock_gio_vfs_can_eject;
		pVFSBackend->eject = cairo_dock_gio_vfs_eject_drive;
		pVFSBackend->mount = cairo_dock_gio_vfs_mount;
		pVFSBackend->unmount = cairo_dock_gio_vfs_unmount;
		pVFSBackend->add_monitor = cairo_dock_gio_vfs_add_monitor;
		pVFSBackend->remove_monitor = cairo_dock_gio_vfs_remove_monitor;
		pVFSBackend->delete_file = cairo_dock_gio_vfs_delete_file;
		pVFSBackend->rename = cairo_dock_gio_vfs_rename_file;
		pVFSBackend->move = cairo_dock_gio_vfs_move_file;
		pVFSBackend->create = cairo_dock_gio_vfs_create_file;
		pVFSBackend->get_trash_path = cairo_dock_gio_vfs_get_trash_path;
		pVFSBackend->empty_trash = cairo_dock_gio_vfs_empty_trash;
		pVFSBackend->get_desktop_path = cairo_dock_gio_vfs_get_desktop_path;
		pVFSBackend->list_apps_for_file = cairo_dock_gio_vfs_list_apps_for_file;
	}
}



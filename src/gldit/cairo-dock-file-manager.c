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

#define _POSIX_C_SOURCE 200809L // needed for O_CLOEXEC
#include <stdlib.h>      // atoi
#include <string.h>      // memset
#include <sys/stat.h>    // stat
#include <fcntl.h>  // open
#include <sys/sendfile.h>  // sendfile
#include <errno.h>  // errno

#include "gldi-config.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-desklet-factory.h"  // cairo_dock_fm_create_icon_from_URI
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dialog-factory.h"  // gldi_dialog_show_temporary
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-utils.h"  // cairo_dock_property_is_present_on_root
#include "cairo-dock-class-manager.h"  // GldiAppInfo functions
#include "cairo-dock-icon-manager.h"  // cairo_dock_free_icon
#include "cairo-dock-menu.h" // gldi_menu_add_item
#define _MANAGER_DEF_
#include "cairo-dock-file-manager.h"

// public (manager, config, data)
GldiManager myDesktopEnvMgr;
CairoDockDesktopEnv g_iDesktopEnv = CAIRO_DOCK_UNKNOWN_ENV;

// dependancies

// private
static CairoDockDesktopEnvBackend *s_pEnvBackend = NULL;


void cairo_dock_fm_force_desktop_env (CairoDockDesktopEnv iForceDesktopEnv)
{
	g_return_if_fail (s_pEnvBackend == NULL);
	g_iDesktopEnv = iForceDesktopEnv;
}

void cairo_dock_fm_register_vfs_backend (CairoDockDesktopEnvBackend *pVFSBackend)
{
	g_free (s_pEnvBackend);
	s_pEnvBackend = pVFSBackend;
}

gboolean cairo_dock_fm_vfs_backend_is_defined (void)
{
	return (s_pEnvBackend != NULL);
}


GList * cairo_dock_fm_list_directory (const gchar *cURI, CairoDockFMSortType g_fm_iSortType, int iNewIconsType, gboolean bListHiddenFiles, int iNbMaxFiles, gchar **cFullURI)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->list_directory != NULL)
	{
		return s_pEnvBackend->list_directory (cURI, g_fm_iSortType, iNewIconsType, bListHiddenFiles, iNbMaxFiles, cFullURI);
	}
	else
	{
		cFullURI = NULL;
		return NULL;
	}
}

gsize cairo_dock_fm_measure_diretory (const gchar *cBaseURI, gint iCountType, gboolean bRecursive, gint *pCancel)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->measure_directory != NULL)
	{
		return s_pEnvBackend->measure_directory (cBaseURI, iCountType, bRecursive, pCancel);
	}
	else
		return 0;
}

gboolean cairo_dock_fm_get_file_info (const gchar *cBaseURI, gchar **cName, gchar **cURI, gchar **cIconName, gboolean *bIsDirectory, int *iVolumeID, double *fOrder, CairoDockFMSortType iSortType)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->get_file_info != NULL)
	{
		s_pEnvBackend->get_file_info (cBaseURI, cName, cURI, cIconName, bIsDirectory, iVolumeID, fOrder, iSortType);
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_get_file_properties (const gchar *cURI, guint64 *iSize, time_t *iLastModificationTime, gchar **cMimeType, int *iUID, int *iGID, int *iPermissionsMask)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->get_file_properties != NULL)
	{
		s_pEnvBackend->get_file_properties (cURI, iSize, iLastModificationTime, cMimeType, iUID, iGID, iPermissionsMask);
		return TRUE;
	}
	else
		return FALSE;
}

static gpointer _cairo_dock_fm_launch_uri_threaded (gchar *cURI)
{
	cd_debug ("%s (%s)", __func__, cURI);
	s_pEnvBackend->launch_uri (cURI);
	g_free (cURI);
	return NULL;
}
gboolean cairo_dock_fm_launch_uri (const gchar *cURI)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->launch_uri != NULL && cURI != NULL)
	{
		s_pEnvBackend->launch_uri (cURI);
		
		// add it to the recent files.
		GtkRecentManager *rm = gtk_recent_manager_get_default () ;
		if (*cURI == '/')  // not an URI, this is now needed for gtk-recent.
		{
			gchar *cValidURI = g_filename_to_uri (cURI, NULL, NULL);
			if (cValidURI) gtk_recent_manager_add_item (rm, cValidURI);
			g_free (cValidURI);
		}
		else
			gtk_recent_manager_add_item (rm, cURI);
		
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_add_monitor_full (const gchar *cURI, gboolean bDirectory, const gchar *cMountedURI, CairoDockFMMonitorCallback pCallback, gpointer data)
{
	g_return_val_if_fail (cURI != NULL, FALSE);
	if (s_pEnvBackend != NULL && s_pEnvBackend->add_monitor != NULL)
	{
		if (cMountedURI != NULL && strcmp (cMountedURI, cURI) != 0)
		{
			s_pEnvBackend->add_monitor (cURI, FALSE, pCallback, data);
			if (bDirectory)
				s_pEnvBackend->add_monitor (cMountedURI, TRUE, pCallback, data);
		}
		else
		{
			s_pEnvBackend->add_monitor (cURI, bDirectory, pCallback, data);
		}
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_remove_monitor_full (const gchar *cURI, gboolean bDirectory, const gchar *cMountedURI)
{
	g_return_val_if_fail (cURI != NULL, FALSE);
	if (s_pEnvBackend != NULL && s_pEnvBackend->remove_monitor != NULL)
	{
		s_pEnvBackend->remove_monitor (cURI);
		if (cMountedURI != NULL && strcmp (cMountedURI, cURI) != 0 && bDirectory)
		{
			s_pEnvBackend->remove_monitor (cMountedURI);
		}
		return TRUE;
	}
	else
		return FALSE;
}



gboolean cairo_dock_fm_mount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, gpointer user_data)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->mount != NULL && iVolumeID > 0 && cURI != NULL)
	{
		s_pEnvBackend->mount (cURI, iVolumeID, pCallback, user_data);
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_unmount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, gpointer user_data)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->unmount != NULL && iVolumeID > 0 && cURI != NULL)
	{
		s_pEnvBackend->unmount (cURI, iVolumeID, pCallback, user_data);
		return TRUE;
	}
	else
		return FALSE;
}

gchar *cairo_dock_fm_is_mounted (const gchar *cURI, gboolean *bIsMounted)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->is_mounted != NULL)
		return s_pEnvBackend->is_mounted (cURI, bIsMounted);
	else
		return NULL;
}

gboolean cairo_dock_fm_can_eject (const gchar *cURI)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->can_eject != NULL)
		return s_pEnvBackend->can_eject (cURI);
	else
		return FALSE;
}

gboolean cairo_dock_fm_eject_drive (const gchar *cURI)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->eject != NULL)
		return s_pEnvBackend->eject (cURI);
	else
		return FALSE;
}


gboolean cairo_dock_fm_delete_file (const gchar *cURI, gboolean bNoTrash)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->delete_file != NULL)
	{
		return s_pEnvBackend->delete_file (cURI, bNoTrash);
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_rename_file (const gchar *cOldURI, const gchar *cNewName)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->rename != NULL)
	{
		return s_pEnvBackend->rename (cOldURI, cNewName);
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_move_file (const gchar *cURI, const gchar *cDirectoryURI)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->move != NULL)
	{
		return s_pEnvBackend->move (cURI, cDirectoryURI);
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_create_file (const gchar *cURI, gboolean bDirectory)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->create != NULL)
	{
		return s_pEnvBackend->create (cURI, bDirectory);
	}
	else
		return FALSE;
}

GList *cairo_dock_fm_list_apps_for_file (const gchar *cURI)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->list_apps_for_file != NULL)
	{
		return s_pEnvBackend->list_apps_for_file (cURI);
	}
	else
		return NULL;
}

gboolean cairo_dock_fm_empty_trash (void)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->empty_trash != NULL)
	{
		s_pEnvBackend->empty_trash ();
		return TRUE;
	}
	else
		return FALSE;
}

gchar *cairo_dock_fm_get_trash_path (const gchar *cNearURI, gchar **cFileInfoPath)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->get_trash_path != NULL)
	{
		return s_pEnvBackend->get_trash_path (cNearURI, cFileInfoPath);
	}
	else
		return NULL;
}

gchar *cairo_dock_fm_get_desktop_path (void)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->get_desktop_path != NULL)
	{
		return s_pEnvBackend->get_desktop_path ();
	}
	else
		return NULL;
}

gboolean cairo_dock_fm_logout (void)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->logout!= NULL)
	{
		const gchar *sm = g_getenv ("SESSION_MANAGER");
		if (sm == NULL || *sm == '\0')  // if there is no session-manager, the desktop methods are useless.
			return FALSE;
		s_pEnvBackend->logout ();
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_shutdown (void)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->shutdown!= NULL)
	{
		const gchar *sm = g_getenv ("SESSION_MANAGER");
		if (sm == NULL || *sm == '\0')  // idem
			return FALSE;
		s_pEnvBackend->shutdown ();
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_reboot (void)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->reboot!= NULL)
	{
		const gchar *sm = g_getenv ("SESSION_MANAGER");
		if (sm == NULL || *sm == '\0')  // idem
			return FALSE;
		s_pEnvBackend->reboot ();
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_lock_screen (void)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->lock_screen != NULL)
	{
		s_pEnvBackend->lock_screen ();
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_can_setup_time (void)
{
	return (s_pEnvBackend != NULL && s_pEnvBackend->setup_time!= NULL);
}

gboolean cairo_dock_fm_setup_time (void)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->setup_time!= NULL)
	{
		s_pEnvBackend->setup_time ();
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_show_system_monitor (void)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->show_system_monitor!= NULL)
	{
		s_pEnvBackend->show_system_monitor ();
		return TRUE;
	}
	else
		return FALSE;
}

Icon *cairo_dock_fm_create_icon_from_URI (const gchar *cURI, GldiContainer *pContainer, CairoDockFMSortType iFileSortType)
{
	if (s_pEnvBackend == NULL || s_pEnvBackend->get_file_info == NULL)
		return NULL;
	Icon *pNewIcon = cairo_dock_create_dummy_launcher (NULL, NULL, NULL, NULL, 0);  // not a type that the dock can handle => the creator must handle it itself.
	pNewIcon->cBaseURI = g_strdup (cURI);
	gboolean bIsDirectory;
	s_pEnvBackend->get_file_info (cURI, &pNewIcon->cName, &pNewIcon->cCommand, &pNewIcon->cFileName, &bIsDirectory, &pNewIcon->iVolumeID, &pNewIcon->fOrder, iFileSortType);
	if (pNewIcon->cName == NULL)
	{
		gldi_object_unref (GLDI_OBJECT (pNewIcon));
		return NULL;
	}
	if (bIsDirectory)
		pNewIcon->iVolumeID = -1;
	//g_print ("%s -> %s ; %s\n", cURI, pNewIcon->cCommand, pNewIcon->cBaseURI);

	if (iFileSortType == CAIRO_DOCK_FM_SORT_BY_NAME)
	{
		GList *pList = (CAIRO_DOCK_IS_DOCK (pContainer) ? CAIRO_DOCK (pContainer)->icons : CAIRO_DESKLET (pContainer)->icons);
		GList *ic;
		Icon *icon;
		for (ic = pList; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			if (icon->cName != NULL && strcmp (pNewIcon->cName, icon->cName) > 0)  // ordre croissant.
			{
				if (ic->prev != NULL)
				{
					Icon *prev_icon = ic->prev->data;
					pNewIcon->fOrder = (icon->fOrder + prev_icon->fOrder) / 2;
				}
				else
					pNewIcon->fOrder = icon->fOrder - 1;
				break ;
			}
			else if (ic->next == NULL)
			{
				pNewIcon->fOrder = icon->fOrder + 1;
			}
		}
	}
	///cairo_dock_trigger_load_icon_buffers (pNewIcon);

	return pNewIcon;
}


gboolean cairo_dock_fm_move_into_directory (const gchar *cURI, Icon *icon, GldiContainer *pContainer)
{
	g_return_val_if_fail (cURI != NULL && icon != NULL, FALSE);
	cd_message (" -> copie de %s dans %s", cURI, icon->cBaseURI);
	gboolean bSuccess = cairo_dock_fm_move_file (cURI, icon->cBaseURI);
	if (! bSuccess)
	{
		cd_warning ("couldn't copy this file.\nCheck that you have writing rights, and that the new does not already exist.");
		gchar *cMessage = g_strdup_printf ("Warning : couldn't copy %s into %s.\nCheck that you have writing rights, and that the name does not already exist.", cURI, icon->cBaseURI);
		gldi_dialog_show_temporary (cMessage, icon, pContainer, 4000);
		g_free (cMessage);
	}
	return bSuccess;
}


int cairo_dock_get_file_size (const gchar *cFilePath)
{
	struct stat buf;
	if (cFilePath == NULL)
		return 0;
	buf.st_size = 0;
	if (stat (cFilePath, &buf) != -1)
	{
		return buf.st_size;
	}
	else
		return 0;
}

gboolean cairo_dock_copy_file (const gchar *cFilePath, const gchar *cDestPath)
{
	gboolean ret = TRUE;
	// open both files
	int src_fd = open (cFilePath, O_RDONLY | O_CLOEXEC);
	int dest_fd;
	if (g_file_test (cDestPath, G_FILE_TEST_IS_DIR))
	{
		const gchar *cFileName = strrchr(cFilePath, '/');
		gchar *cFileDest = g_strdup_printf("%s/%s", cDestPath, cFileName ? cFileName : cFilePath);
		dest_fd = open (cFileDest, O_CREAT | O_WRONLY | O_CLOEXEC, S_IRUSR|S_IWUSR | S_IRGRP | S_IROTH);  // mode=644
		g_free(cFileDest);
	}
	else
	{
		dest_fd = open (cDestPath, O_CREAT | O_WRONLY | O_CLOEXEC, S_IRUSR|S_IWUSR | S_IRGRP | S_IROTH);  // mode=644
	}
	struct stat stat;
	// get data size to be copied
	if (fstat (src_fd, &stat) < 0)
	{
		cd_warning ("couldn't get info of file '%s' (%s)", cFilePath, strerror(errno));
		ret = FALSE;
	}
	else
	{
		// perform in-kernel transfer (zero copy to user space)
		int size;
		#ifdef __FreeBSD__
		size = sendfile (src_fd, dest_fd, 0, stat.st_size, NULL, NULL, 0);
		#else  // Linux >= 2.6.33 for being able to have a regular file as the output socket
		size = sendfile (dest_fd, src_fd, NULL, stat.st_size);  // note the inversion between both calls ^_^;
		#endif
		if (size < 0)  // error, fallback to a read-write method
		{
			cd_debug ("couldn't fast-copy file '%s' to '%s' (%s)", cFilePath, cDestPath, strerror(errno));
			// read data
			char *buf = g_new (char, stat.st_size);
			size = read (src_fd, buf, stat.st_size);
			if (size < 0)
			{
				cd_warning ("couldn't read file '%s' (%s)", cFilePath, strerror(errno));
				ret = FALSE;
			}
			else
			{
				// copy data
				size = write (dest_fd, buf, stat.st_size);
				if (size < 0)
				{
					cd_warning ("couldn't write to file '%s' (%s)", cDestPath, strerror(errno));
					ret = FALSE;
				}
			}
			g_free (buf);
		}
	}
	close (dest_fd);
	close (src_fd);
	return ret;
}


struct _submenu_data {
	GList *data_list; // struct _launch_with_data
	gchar *cPath;
	gpointer user_data;
	CairoDockFMOpenedWithCallback pCallback;
};
struct _launch_with_data {
	GldiAppInfo *app;
	struct _submenu_data *data;
};

static void _free_submenu_data_item (gpointer ptr)
{
	struct _launch_with_data *data = (struct _launch_with_data*)ptr;
	gldi_object_unref (GLDI_OBJECT (data->app));
	g_free (data);
}
static void _free_submenu_data (gpointer ptr, GObject*)
{
	struct _submenu_data *data = (struct _submenu_data*)ptr;
	g_list_free_full (data->data_list, _free_submenu_data_item);
	g_free (data->cPath);
	g_free (data);
}

static void _submenu_launch_with (GtkMenuItem*, gpointer ptr)
{
	struct _launch_with_data *data = (struct _launch_with_data*)ptr;
	const gchar *cURIs[] = {data->data->cPath, NULL};
	gldi_app_info_launch (data->app, cURIs);
	
	if (data->data->pCallback) data->data->pCallback (data->data->user_data);
}

gboolean cairo_dock_fm_add_open_with_submenu (GList *pAppList, const gchar *cPath, GtkWidget *pMenu, const gchar *cLabel,
	const gchar *cImage, CairoDockFMOpenedWithCallback pCallback, gpointer user_data)
{
	if (! (pAppList && cPath) ) return FALSE;
	struct _submenu_data *data = g_new0 (struct _submenu_data, 1);
	data->cPath = g_strdup (cPath);
	data->user_data = user_data;
	data->pCallback = pCallback;
	
	GtkWidget *pSubMenu = gldi_menu_add_sub_menu (pMenu, cLabel, cImage);
	g_object_weak_ref (G_OBJECT (pSubMenu), _free_submenu_data, data);
	
	GList *a;
	for (a = pAppList; a != NULL; a = a->next)
	{
		GAppInfo *app_info = a->data;
		GDesktopAppInfo *desktop_app = G_DESKTOP_APP_INFO (app_info);
		if (!desktop_app)
		{
			// should not happen, GDesktopAppInfo is the only implementation
			cd_warning ("Unknown GAppInfo type!");
			continue;
		}
		GldiAppInfo *gldi_app = gldi_app_info_from_desktop_app_info (desktop_app);
		if (!gldi_app)
		{
			// warning already shown in gldi_app_info_from_desktop_app_info ()
			continue;
		}

		gchar *cIconPath = NULL;
		GIcon *pIcon = g_app_info_get_icon (app_info);
		if (pIcon)
		{
			gchar *tmp = g_icon_to_string (pIcon);
			cIconPath = cairo_dock_search_icon_s_path (tmp, cairo_dock_search_icon_size (GTK_ICON_SIZE_MENU));
			g_free (tmp);
		}

		struct _launch_with_data *app_data = g_new (struct _launch_with_data, 1);
		app_data->data = data;
		app_data->app = gldi_app;
		data->data_list = g_list_prepend (data->data_list, app_data); // save a reference to the app info

		gldi_menu_add_item (pSubMenu, g_app_info_get_name (app_info), cIconPath, G_CALLBACK(_submenu_launch_with), app_data);
		g_free (cIconPath);
	}
	return TRUE;
}


  ///////////
 /// PID ///
///////////

int cairo_dock_fm_get_pid (const gchar *cProcessName)
{
	int iPID = -1;
	const char * const args[] = {"pidof", cProcessName, NULL};
	gchar *cPID = cairo_dock_launch_command_argv_sync_with_stderr (args, TRUE);

	if (cPID != NULL && *cPID != '\0')
		iPID = atoi (cPID);

	g_free (cPID);

	return iPID;
}

static gboolean _wait_pid (gpointer *pData)
{
	gboolean bCheckSameProcess = GPOINTER_TO_INT (pData[0]);
	gchar *cProcess = pData[1];

	// check if /proc/%d dir exists or the process is running
	if ((bCheckSameProcess && ! g_file_test (cProcess, G_FILE_TEST_EXISTS))
		|| (! bCheckSameProcess && cairo_dock_fm_get_pid (cProcess) == -1))
	{
		GSourceFunc pCallback = pData[2];
		gpointer pUserData = pData[3];

		pCallback (pUserData);

		// free allocated ressources just used for this function
		g_free (cProcess);
		g_free (pData);

		return FALSE;
	}

	return TRUE;
}

gboolean cairo_dock_fm_monitor_pid (const gchar *cProcessName, gboolean bCheckSameProcess, GSourceFunc pCallback, gboolean bAlwaysLaunch, gpointer pUserData)
{
	int iPID = cairo_dock_fm_get_pid (cProcessName);
	if (iPID == -1)
	{
		if (bAlwaysLaunch)
			pCallback (pUserData);
		return FALSE;
	}

	gpointer *pData = g_new (gpointer, 4);
	pData[0] = GINT_TO_POINTER (bCheckSameProcess);
	if (bCheckSameProcess)
		pData[1] = g_strdup_printf ("/proc/%d", iPID);
	else
		pData[1] = g_strdup (cProcessName);
	pData[2] = pCallback;
	pData[3] = pUserData;

	/* It's not easy to be notified when a non child process is stopped...
	 * We can't use waitpid (not a child process) or monitor /proc/PID dir (or a
	 * file into it) with g_file_monitor, poll or inotify => it's not working...
	 * And for apt-get/dpkg, we can't monitor the lock file with fcntl because
	 * we need root rights to do that.
	 * Let's just check every 5 seconds if the PID is still running
	 */
	g_timeout_add_seconds (5, (GSourceFunc)_wait_pid, pData);

	return TRUE;
}

  ////////////
 /// INIT ///
////////////

static CairoDockDesktopEnv _guess_environment (void)
{
	// first, let's try out with XDG_CURRENT_DESKTOP
	const gchar *cEnv = g_getenv ("XDG_CURRENT_DESKTOP");
	if (cEnv != NULL)
	{
		if (strstr(cEnv, "GNOME") != NULL)
			return CAIRO_DOCK_GNOME;
		else if (strstr(cEnv, "XFCE") != NULL)
			return CAIRO_DOCK_XFCE;
		else if (strstr(cEnv, "KDE") != NULL)
			return CAIRO_DOCK_KDE;
		else
		{
			cd_debug ("couldn't interpret XDG_CURRENT_DESKTOP=%s", cEnv);
		}
	}
	
	// else, fall back to some old (less reliable) methods
	cEnv = g_getenv ("GNOME_DESKTOP_SESSION_ID");  // this value is now deprecated, but has been maintained for compatibility, so let's keep using it (Note: a possible alternative would be to check for org.gnome.SessionManager on Dbus)
	if (cEnv != NULL && *cEnv != '\0')
		return CAIRO_DOCK_GNOME;
	
	cEnv = g_getenv ("KDE_FULL_SESSION");
	if (cEnv != NULL && *cEnv != '\0')
		return CAIRO_DOCK_KDE;
	
	cEnv = g_getenv ("KDE_SESSION_UID");
	if (cEnv != NULL && *cEnv != '\0')
		return CAIRO_DOCK_KDE;
	
	#ifdef HAVE_X11
	if (cairo_dock_property_is_present_on_root ("_DT_SAVE_MODE"))
		return CAIRO_DOCK_XFCE;
	#endif
	
	const char * const args[] = {"pgrep", "kwin", NULL}; // will also match kwin_wayland, kwin_x11, etc.
	gchar *cKWin = cairo_dock_launch_command_argv_sync_with_stderr (args, FALSE);
	if (cKWin != NULL && *cKWin != '\0')
	{
		g_free (cKWin);
		return CAIRO_DOCK_KDE;
	}
	g_free (cKWin);
	
	return CAIRO_DOCK_UNKNOWN_ENV;
	
}

const gchar *cairo_dock_fm_get_desktop_name (void)
{
	return (g_iDesktopEnv == CAIRO_DOCK_GNOME ? "GNOME" :
		g_iDesktopEnv == CAIRO_DOCK_XFCE ? "XFCE" :
		g_iDesktopEnv == CAIRO_DOCK_KDE ? "KDE" :
		"unknown");
}

static void init (void)
{
	g_iDesktopEnv = _guess_environment ();
	cd_message ("We found this desktop environment: %s",
		cairo_dock_fm_get_desktop_name ());
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_desktop_environment_manager (void)
{
	// Manager
	memset (&myDesktopEnvMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myDesktopEnvMgr), &myManagerObjectMgr, NULL);
	myDesktopEnvMgr.cModuleName 	= "Desktop Env";
	// interface
	myDesktopEnvMgr.init 			= init;
	myDesktopEnvMgr.load 			= NULL;
	myDesktopEnvMgr.unload 			= NULL;
	myDesktopEnvMgr.reload 			= (GldiManagerReloadFunc)NULL;
	myDesktopEnvMgr.get_config 		= (GldiManagerGetConfigFunc)NULL;
	myDesktopEnvMgr.reset_config	 = (GldiManagerResetConfigFunc)NULL;
	// Config
	myDesktopEnvMgr.pConfig = (GldiManagerConfigPtr)NULL;
	myDesktopEnvMgr.iSizeOfConfig = 0;
	// data
	myDesktopEnvMgr.pData = (GldiManagerDataPtr)NULL;
	myDesktopEnvMgr.iSizeOfData = 0;
}

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
#include <sys/stat.h>

#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-desklet-factory.h"  // cairo_dock_fm_create_icon_from_URI
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-container.h"
#include "cairo-dock-launcher-manager.h"  // cairo_dock_launch_command_sync
#include "cairo-dock-X-utilities.h"  // cairo_dock_property_is_present_on_root
#define _MANAGER_DEF_
#include "cairo-dock-file-manager.h"

// public (manager, config, data)
CairoDesktopEnvManager myDesktopEnvMgr;
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
}
gboolean cairo_dock_fm_launch_uri (const gchar *cURI)
{
	if (s_pEnvBackend != NULL && s_pEnvBackend->launch_uri != NULL && cURI != NULL)
	{
		// launch the URI in a thread.
		//s_pEnvBackend->launch_uri (cURI);
		GError *erreur = NULL;
		gchar *cThreadURI = g_strdup (cURI);
		GThread* pThread = g_thread_create ((GThreadFunc) _cairo_dock_fm_launch_uri_threaded, (gpointer) cThreadURI, FALSE, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
		}
		
		// add it to the recent files.
		GtkRecentManager *rm = gtk_recent_manager_get_default () ;
		gchar *cValidURI = NULL;
		if (*cURI == '/')  // not an URI, this is now needed for gtk-recent.
			cValidURI = g_filename_to_uri (cURI, NULL, NULL);
		gtk_recent_manager_add_item (rm, cValidURI ? cValidURI : cURI);
		g_free (cValidURI);
		
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


static void _cairo_dock_fm_action_after_mounting (gboolean bMounting, gboolean bSuccess, const gchar *cName, const gchar *cUri, gpointer *data)
{
	Icon *icon = data[0];
	CairoContainer *pContainer = data[1];
	cd_message ("%s (%s) : %d", __func__, (bMounting ? "mount" : "unmount"), bSuccess);  // en cas de demontage effectif, l'icone n'est plus valide !
	if ((! bSuccess && pContainer != NULL) || icon == NULL)  // dans l'autre cas (succes), l'icone peut ne plus etre valide ! mais on s'en fout, puisqu'en cas de succes, il y'aura rechargement de l'icone, et donc on pourra balancer le message a ce moment-la.
	{
		///if (icon != NULL)
			cairo_dock_show_temporary_dialog_with_icon_printf (bMounting ? _("failed to mount %s") : _("Failed to unmount %s"), icon, pContainer, 4000, "same icon", cName);
		///else
		///	cairo_dock_show_general_message (cMessage, 4000);
	}
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
		s_pEnvBackend->shutdown ();
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

Icon *cairo_dock_fm_create_icon_from_URI (const gchar *cURI, CairoContainer *pContainer, CairoDockFMSortType iFileSortType)
{
	if (s_pEnvBackend == NULL || s_pEnvBackend->get_file_info == NULL)
		return NULL;
	Icon *pNewIcon = cairo_dock_create_dummy_launcher (NULL, NULL, NULL, NULL, 0);
	pNewIcon->iTrueType = CAIRO_DOCK_ICON_TYPE_FILE;
	pNewIcon->cBaseURI = g_strdup (cURI);
	gboolean bIsDirectory;
	s_pEnvBackend->get_file_info (cURI, &pNewIcon->cName, &pNewIcon->cCommand, &pNewIcon->cFileName, &bIsDirectory, &pNewIcon->iVolumeID, &pNewIcon->fOrder, iFileSortType);
	if (pNewIcon->cName == NULL)
	{
		cairo_dock_free_icon (pNewIcon);
		return NULL;
	}
	if (bIsDirectory)
		pNewIcon->iVolumeID = -1;
	g_print ("%s -> %s ; %s\n", cURI, pNewIcon->cCommand, pNewIcon->cBaseURI);

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
	cairo_dock_trigger_load_icon_buffers (pNewIcon, pContainer);

	return pNewIcon;
}


gboolean cairo_dock_fm_move_into_directory (const gchar *cURI, Icon *icon, CairoContainer *pContainer)
{
	g_return_val_if_fail (cURI != NULL && icon != NULL, FALSE);
	cd_message (" -> copie de %s dans %s", cURI, icon->cBaseURI);
	gboolean bSuccess = cairo_dock_fm_move_file (cURI, icon->cBaseURI);
	if (! bSuccess)
	{
		cd_warning ("couldn't copy this file.\nCheck that you have writing rights, and that the new does not already exist.");
		gchar *cMessage = g_strdup_printf ("Warning : couldn't copy %s into %s.\nCheck that you have writing rights, and that the name does not already exist.", cURI, icon->cBaseURI);
		cairo_dock_show_temporary_dialog (cMessage, icon, pContainer, 4000);
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


  ////////////
 /// INIT ///
////////////

static inline CairoDockDesktopEnv _guess_environment (void)
{
	const gchar * cEnv = g_getenv ("GNOME_DESKTOP_SESSION_ID");
	if (cEnv != NULL && *cEnv != '\0')
		return CAIRO_DOCK_GNOME;
	
	cEnv = g_getenv ("KDE_FULL_SESSION");
	if (cEnv != NULL && *cEnv != '\0')
		return CAIRO_DOCK_KDE;
	
	cEnv = g_getenv ("KDE_SESSION_UID");
	if (cEnv != NULL && *cEnv != '\0')
		return CAIRO_DOCK_KDE;
	
	if (cairo_dock_property_is_present_on_root ("_DT_SAVE_MODE"))
		return CAIRO_DOCK_XFCE;
	
	gchar *cKWin = cairo_dock_launch_command_sync ("pgrep kwin");
	if (cKWin != NULL && *cKWin != '\0')
	{
		g_free (cKWin);
		return CAIRO_DOCK_KDE;
	}
	g_free (cKWin);
	
	return CAIRO_DOCK_UNKNOWN_ENV;
	
}
static void init (void)
{
	g_iDesktopEnv = _guess_environment ();
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_desktop_environment_manager (void)
{
	// Manager
	memset (&myDesktopEnvMgr, 0, sizeof (CairoDesktopEnvManager));
	myDesktopEnvMgr.mgr.cModuleName 	= "Desktop Env";
	myDesktopEnvMgr.mgr.init 			= init;
	myDesktopEnvMgr.mgr.load 			= NULL;
	myDesktopEnvMgr.mgr.unload 			= NULL;
	myDesktopEnvMgr.mgr.reload 			= (GldiManagerReloadFunc)NULL;
	myDesktopEnvMgr.mgr.get_config 		= (GldiManagerGetConfigFunc)NULL;
	myDesktopEnvMgr.mgr.reset_config	 = (GldiManagerResetConfigFunc)NULL;
	// Config
	myDesktopEnvMgr.mgr.pConfig = (GldiManagerConfigPtr*)NULL;
	myDesktopEnvMgr.mgr.iSizeOfConfig = 0;
	// data
	myDesktopEnvMgr.mgr.pData = (GldiManagerDataPtr*)NULL;
	myDesktopEnvMgr.mgr.iSizeOfData = 0;
	// signals
	cairo_dock_install_notifications_on_object (&myDesktopEnvMgr, NB_NOTIFICATIONS_DESKTOP_ENV);
	// register
	gldi_register_manager (GLDI_MANAGER(&myDesktopEnvMgr));
}

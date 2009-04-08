/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <string.h>

#include "cairo-dock-dock-factory.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-container.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-file-manager.h"

extern CairoDockDesktopEnv g_iDesktopEnv;

static CairoDockVFSBackend *s_pVFSBackend = NULL;

void cairo_dock_fm_register_vfs_backend (CairoDockVFSBackend *pVFSBackend)
{
	g_free (s_pVFSBackend);
	s_pVFSBackend = pVFSBackend;
}



GList * cairo_dock_fm_list_directory (const gchar *cURI, CairoDockFMSortType g_fm_iSortType, int iNewIconsType, gboolean bListHiddenFiles, gchar **cFullURI)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->list_directory != NULL)
	{
		return s_pVFSBackend->list_directory (cURI, g_fm_iSortType, iNewIconsType, bListHiddenFiles, cFullURI);
	}
	else
	{
		cFullURI = NULL;
		return NULL;
	}
}

gboolean cairo_dock_fm_get_file_info (const gchar *cBaseURI, gchar **cName, gchar **cURI, gchar **cIconName, gboolean *bIsDirectory, int *iVolumeID, double *fOrder, CairoDockFMSortType iSortType)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->get_file_info != NULL)
	{
		s_pVFSBackend->get_file_info (cBaseURI, cName, cURI, cIconName, bIsDirectory, iVolumeID, fOrder, iSortType);
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_get_file_properties (const gchar *cURI, guint64 *iSize, time_t *iLastModificationTime, gchar **cMimeType, int *iUID, int *iGID, int *iPermissionsMask)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->get_file_properties != NULL)
	{
		s_pVFSBackend->get_file_properties (cURI, iSize, iLastModificationTime, cMimeType, iUID, iGID, iPermissionsMask);
		return TRUE;
	}
	else
		return FALSE;
}

static gpointer _cairo_dock_fm_launch_uri_threaded (gchar *cURI)
{
	s_pVFSBackend->launch_uri (cURI);
}
gboolean cairo_dock_fm_launch_uri (const gchar *cURI)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->launch_uri != NULL)
	{
		//s_pVFSBackend->launch_uri (cURI);
		GError *erreur = NULL;
		GThread* pThread = g_thread_create ((GThreadFunc) _cairo_dock_fm_launch_uri_threaded, (gpointer) cURI, FALSE, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
		}
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_add_monitor_full (const gchar *cURI, gboolean bDirectory, const gchar *cMountedURI, CairoDockFMMonitorCallback pCallback, gpointer data)
{
	g_return_val_if_fail (cURI != NULL, FALSE);
	if (s_pVFSBackend != NULL && s_pVFSBackend->add_monitor != NULL)
	{
		if (cMountedURI != NULL && strcmp (cMountedURI, cURI) != 0)
		{
			s_pVFSBackend->add_monitor (cURI, FALSE, pCallback, data);
			if (bDirectory)
				s_pVFSBackend->add_monitor (cMountedURI, TRUE, pCallback, data);
		}
		else
		{
			s_pVFSBackend->add_monitor (cURI, bDirectory, pCallback, data);
		}
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_remove_monitor_full (const gchar *cURI, gboolean bDirectory, const gchar *cMountedURI)
{
	g_return_val_if_fail (cURI != NULL, FALSE);
	if (s_pVFSBackend != NULL && s_pVFSBackend->remove_monitor != NULL)
	{
		s_pVFSBackend->remove_monitor (cURI);
		if (cMountedURI != NULL && strcmp (cMountedURI, cURI) != 0 && bDirectory)
		{
			s_pVFSBackend->remove_monitor (cMountedURI);
		}
		return TRUE;
	}
	else
		return FALSE;
}



gboolean cairo_dock_fm_mount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->mount != NULL && iVolumeID > 0)
	{
		s_pVFSBackend->mount (cURI, iVolumeID, pCallback, icon, pContainer);
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_unmount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->unmount != NULL && iVolumeID > 0)
	{
		s_pVFSBackend->unmount (cURI, iVolumeID, pCallback, icon, pContainer);
		return TRUE;
	}
	else
		return FALSE;
}

gchar *cairo_dock_fm_is_mounted (const gchar *cURI, gboolean *bIsMounted)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->is_mounted != NULL)
		return s_pVFSBackend->is_mounted (cURI, bIsMounted);
	else
		return NULL;
}

gboolean cairo_dock_fm_can_eject (const gchar *cURI)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->can_eject != NULL)
		return s_pVFSBackend->can_eject (cURI);
	else
		return FALSE;
}

gboolean cairo_dock_fm_eject_drive (const gchar *cURI)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->eject != NULL)
		return s_pVFSBackend->eject (cURI);
	else
		return FALSE;
}


gboolean cairo_dock_fm_delete_file (const gchar *cURI)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->delete_file != NULL)
	{
		return s_pVFSBackend->delete_file (cURI);
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_rename_file (const gchar *cOldURI, const gchar *cNewName)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->rename != NULL)
	{
		return s_pVFSBackend->rename (cOldURI, cNewName);
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_move_file (const gchar *cURI, const gchar *cDirectoryURI)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->move != NULL)
	{
		return s_pVFSBackend->move (cURI, cDirectoryURI);
	}
	else
		return FALSE;
}


gchar *cairo_dock_fm_get_trash_path (const gchar *cNearURI, gchar **cFileInfoPath)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->get_trash_path != NULL)
	{
		return s_pVFSBackend->get_trash_path (cNearURI, cFileInfoPath);
	}
	else
		return NULL;
}

gchar *cairo_dock_fm_get_desktop_path (void)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->get_desktop_path != NULL)
	{
		return s_pVFSBackend->get_desktop_path ();
	}
	else
		return NULL;
}

gboolean cairo_dock_fm_logout (void)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->logout!= NULL)
	{
		s_pVFSBackend->logout ();
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_shutdown (void)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->shutdown!= NULL)
	{
		s_pVFSBackend->shutdown ();
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_setup_time (void)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->setup_time!= NULL)
	{
		s_pVFSBackend->setup_time ();
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cairo_dock_fm_show_system_monitor (void)
{
	if (s_pVFSBackend != NULL && s_pVFSBackend->show_system_monitor!= NULL)
	{
		s_pVFSBackend->show_system_monitor ();
		return TRUE;
	}
	else
		return FALSE;
}

Icon *cairo_dock_fm_create_icon_from_URI (const gchar *cURI, CairoContainer *pContainer)
{
	if (s_pVFSBackend == NULL || s_pVFSBackend->get_file_info == NULL)
		return NULL;
	Icon *pNewIcon = g_new0 (Icon, 1);
	pNewIcon->iType = CAIRO_DOCK_LAUNCHER;
	pNewIcon->cBaseURI = g_strdup (cURI);
	gboolean bIsDirectory;
	s_pVFSBackend->get_file_info (cURI, &pNewIcon->acName, &pNewIcon->acCommand, &pNewIcon->acFileName, &bIsDirectory, &pNewIcon->iVolumeID, &pNewIcon->fOrder, mySystem.iFileSortType);
	if (pNewIcon->acName == NULL)
	{
		cairo_dock_free_icon (pNewIcon);
		return NULL;
	}

	if (bIsDirectory)
	{
		cd_message ("  c'est un sous-repertoire");
	}

	if (mySystem.iFileSortType == CAIRO_DOCK_FM_SORT_BY_NAME)
	{
		GList *pList = (CAIRO_DOCK_IS_DOCK (pContainer) ? CAIRO_DOCK (pContainer)->icons : CAIRO_DESKLET (pContainer)->icons);
		GList *ic;
		Icon *icon;
		for (ic = pList; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			if (icon->acName != NULL && strcmp (pNewIcon->acName, icon->acName) < 0)
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
	cairo_dock_load_one_icon_from_scratch (pNewIcon, pContainer);

	return pNewIcon;
}

void cairo_dock_fm_create_dock_from_directory (Icon *pIcon, CairoDock *pParentDock)
{
	if (s_pVFSBackend == NULL)
		return;
	cd_message ("");
	g_free (pIcon->acCommand);
	GList *pIconList = cairo_dock_fm_list_directory (pIcon->cBaseURI, mySystem.iFileSortType, CAIRO_DOCK_LAUNCHER, mySystem.bShowHiddenFiles, &pIcon->acCommand);
	pIcon->pSubDock = cairo_dock_create_subdock_from_scratch (pIconList, pIcon->acName, pParentDock);

	cairo_dock_update_dock_size (pIcon->pSubDock);  // le 'load_buffer' ne le fait pas.

	cairo_dock_fm_add_monitor (pIcon);
}



static Icon *cairo_dock_fm_alter_icon_if_necessary (Icon *pIcon, CairoContainer *pContainer)
{
	if (s_pVFSBackend == NULL)
		return NULL;
	cd_debug ("%s (%s)", __func__, pIcon->cBaseURI);
	Icon *pNewIcon = cairo_dock_fm_create_icon_from_URI (pIcon->cBaseURI, pContainer);
	g_return_val_if_fail (pNewIcon != NULL && pNewIcon->acName != NULL, NULL);

	//g_print ("%s <-> %s (%s <-> <%s)\n", pIcon->acName, pNewIcon->acName, pIcon->acFileName, pNewIcon->acFileName);
	if (pIcon->acName == NULL || strcmp (pIcon->acName, pNewIcon->acName) != 0 || pNewIcon->acFileName == NULL || strcmp (pIcon->acFileName, pNewIcon->acFileName) != 0 || pIcon->fOrder != pNewIcon->fOrder)
	{
		cd_message ("  on remplace %s", pIcon->acName);
		if (CAIRO_DOCK_IS_DOCK (pContainer))
		{
			pNewIcon->cParentDockName = g_strdup (pIcon->cParentDockName);
			cairo_dock_remove_one_icon_from_dock (CAIRO_DOCK (pContainer), pIcon);
		}
		else
		{
			CAIRO_DESKLET (pContainer)->icons = g_list_remove (CAIRO_DESKLET (pContainer)->icons, pIcon);
		}
		if (pIcon->acDesktopFileName != NULL)
			cairo_dock_fm_remove_monitor (pIcon);

		pNewIcon->acDesktopFileName = g_strdup (pIcon->acDesktopFileName);
		if (pIcon->pSubDock != NULL)
		{
			pNewIcon->pSubDock == pIcon->pSubDock;
			pIcon->pSubDock = NULL;

			if (pNewIcon->acName != NULL && strcmp (pIcon->acName, pNewIcon->acName) != 0)
			{
				cairo_dock_rename_dock (pIcon->acName, pNewIcon->pSubDock, pNewIcon->acName);
			}  // else : detruire le sous-dock.
		}
		pNewIcon->fX = pIcon->fX;
		pNewIcon->fXAtRest = pIcon->fXAtRest;
		pNewIcon->fDrawX = pIcon->fDrawX;

		if (CAIRO_DOCK_IS_DOCK (pContainer))
			cairo_dock_insert_icon_in_dock (pNewIcon, CAIRO_DOCK (pContainer), CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, ! CAIRO_DOCK_INSERT_SEPARATOR);  // on met a jour la taille du dock pour le fXMin/fXMax, et eventuellement la taille de l'icone peut aussi avoir change.
		else
			CAIRO_DESKLET (pContainer)->icons = g_list_insert_sorted (CAIRO_DESKLET (pContainer)->icons,
				pIcon,
				(GCompareFunc) cairo_dock_compare_icons_order);  // on n'utilise pas le pDesklet->pRenderer->load_icons, car on remplace juste une icone par une autre quasi identique, et on ne sait pas si load_icons a ete utilisee.
		cairo_dock_redraw_my_icon (pNewIcon, pContainer);

		if (pNewIcon->acDesktopFileName != NULL)
			cairo_dock_fm_add_monitor (pNewIcon);

		cairo_dock_free_icon (pIcon);
		return pNewIcon;
	}
	else
	{
		cairo_dock_free_icon (pNewIcon);
		return pIcon;
	}
}
void cairo_dock_fm_manage_event_on_file (CairoDockFMEventType iEventType, const gchar *cBaseURI, Icon *pIcon, CairoDockIconType iTypeOnCreation)
{
	g_return_if_fail (cBaseURI != NULL && pIcon != NULL);
	gchar *cURI = (g_strdup (cBaseURI));
	cairo_dock_remove_html_spaces (cURI);
	cd_message ("%s (%d sur %s)", __func__, iEventType, cURI);

	switch (iEventType)
	{
		case CAIRO_DOCK_FILE_DELETED :
		{
			Icon *pConcernedIcon;
			CairoContainer *pParentContainer;
			if (pIcon->cBaseURI != NULL && strcmp (cURI, pIcon->cBaseURI) == 0)
			{
				pConcernedIcon = pIcon;
				pParentContainer = cairo_dock_search_container_from_icon (pIcon);
			}
			else if (pIcon->pSubDock != NULL)
			{
				pConcernedIcon = cairo_dock_get_icon_with_base_uri (pIcon->pSubDock->icons, cURI);
				if (pConcernedIcon == NULL)  // on cherche par nom.
				{
					pConcernedIcon = cairo_dock_get_icon_with_name (pIcon->pSubDock->icons, cURI);
				}
				if (pConcernedIcon == NULL)
					return ;
				pParentContainer = CAIRO_CONTAINER (pIcon->pSubDock);
			}
			else
			{
				cd_warning ("  on n'aurait pas du recevoir cet evenement !");
				return ;
			}
			cd_message ("  %s sera supprimee", pConcernedIcon->acName);
			
			if (CAIRO_DOCK_IS_DOCK (pParentContainer))
			{
				cairo_dock_remove_one_icon_from_dock (CAIRO_DOCK (pParentContainer), pConcernedIcon);  // enleve aussi son moniteur.
				cairo_dock_update_dock_size (CAIRO_DOCK (pParentContainer));
			}
			else if (pConcernedIcon->acDesktopFileName != NULL)  // alors elle a un moniteur.
				cairo_dock_fm_remove_monitor (pConcernedIcon);
			
			cairo_dock_free_icon (pConcernedIcon);
		}
		break ;
		
		case CAIRO_DOCK_FILE_CREATED :
		{
			if ((pIcon->cBaseURI == NULL || strcmp (cURI, pIcon->cBaseURI) != 0) && pIcon->pSubDock != NULL)  // dans des cas foirreux, il se peut que le fichier soit cree alors qu'il existait deja dans le dock.
			{
				CairoContainer *pParentContainer = cairo_dock_search_container_from_icon (pIcon);
				
				if (pIcon->pSubDock != NULL)  // cas d'un signal CREATED sur un fichier deja existant, merci GFVS :-/
				{
					Icon *pSameIcon = cairo_dock_get_icon_with_base_uri (pIcon->pSubDock->icons, cURI);
					if (pSameIcon != NULL)
					{
						cd_message ("ce fichier (%s) existait deja !", pSameIcon->acName);
						return;  // on decide de ne rien faire, c'est surement un signal inutile.
						//cairo_dock_remove_one_icon_from_dock (pIcon->pSubDock, pSameIcon);
						//cairo_dock_free_icon (pSameIcon);
					}
				}
				Icon *pNewIcon = cairo_dock_fm_create_icon_from_URI (cURI, (CAIRO_DOCK_IS_DOCK (pParentContainer) ? CAIRO_CONTAINER (pIcon->pSubDock) : pParentContainer));
				if (pNewIcon == NULL)
					return ;
				pNewIcon->iType = iTypeOnCreation;

				if (CAIRO_DOCK_IS_DOCK (pParentContainer))
					cairo_dock_insert_icon_in_dock (pNewIcon, pIcon->pSubDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, ! CAIRO_DOCK_INSERT_SEPARATOR);
				else
					CAIRO_DESKLET (pParentContainer)->icons = g_list_insert_sorted (CAIRO_DESKLET (pParentContainer)->icons,
						pIcon,
						(GCompareFunc) cairo_dock_compare_icons_order);
				cd_message ("  %s a ete insere(e)", (pNewIcon != NULL ? pNewIcon->acName : "aucune icone n'"));
				
				if (pNewIcon->iVolumeID > 0)
				{
					gboolean bIsMounted;
					gchar *cUri = cairo_dock_fm_is_mounted (pNewIcon->cBaseURI, &bIsMounted);
					g_free (cUri);
					
					cd_message (" c'est un volume, on considere qu'il vient de se faire (de)monter");
					cairo_dock_show_temporary_dialog_with_icon (bIsMounted ? _("%s is now mounted") : _("%s is now unmounted"), pNewIcon, CAIRO_DOCK_IS_DOCK (pParentContainer) ? CAIRO_CONTAINER (pIcon->pSubDock) : pParentContainer, 4000, pNewIcon->acName);
				}
			}
		}
		break ;
		
		case CAIRO_DOCK_FILE_MODIFIED :
		{
			Icon *pConcernedIcon;
			CairoContainer *pParentContainer;
			if (pIcon->cBaseURI != NULL && strcmp (pIcon->cBaseURI, cURI) == 0)  // c'est l'icone elle-meme.
			{
				pConcernedIcon = pIcon;
				pParentContainer = cairo_dock_search_container_from_icon (pIcon);
				g_return_if_fail (pParentContainer != NULL);
			}
			else if (pIcon->pSubDock != NULL)  // c'est a l'interieur du repertoire qu'elle represente.
			{
				pConcernedIcon = cairo_dock_get_icon_with_base_uri (pIcon->pSubDock->icons, cURI);
				g_print ("cURI in sub-dock: %s\n", cURI);
				if (pConcernedIcon == NULL)  // on cherche par nom.
				{
					pConcernedIcon = cairo_dock_get_icon_with_name (pIcon->pSubDock->icons, cURI);
				}
				g_return_if_fail (pConcernedIcon != NULL);
				pParentContainer = CAIRO_CONTAINER (pIcon->pSubDock);
			}
			else
			{
				cd_warning ("  a file has been modified but we couldn't find which one.");
				return ;
			}
			cd_message ("  %s est modifiee", pConcernedIcon->acName);
			
			Icon *pNewIcon = cairo_dock_fm_alter_icon_if_necessary (pConcernedIcon, pParentContainer);  // pConcernedIcon a ete remplacee et n'est donc peut-etre plus valide.
			
			if (pNewIcon != NULL && pNewIcon->iVolumeID > 0)
			{
				cd_message ("ce volume a change");
				gboolean bIsMounted = FALSE;
				if (s_pVFSBackend->is_mounted != NULL)
				{
					gchar *cActivationURI = s_pVFSBackend->is_mounted (pNewIcon->acCommand, &bIsMounted);
					g_free (cActivationURI);
				}
				cairo_dock_show_temporary_dialog_with_icon (bIsMounted ? _("%s is now mounted") : _("%s is now unmounted"), pNewIcon, pParentContainer, 4000, "same icon", pNewIcon->acName);
			}
		}
		break ;
	}
	g_free (cURI);
}

void cairo_dock_fm_action_on_file_event (CairoDockFMEventType iEventType, const gchar *cURI, Icon *pIcon)
{
	cairo_dock_fm_manage_event_on_file (iEventType, cURI, pIcon, CAIRO_DOCK_LAUNCHER);
}

void cairo_dock_fm_action_after_mounting (gboolean bMounting, gboolean bSuccess, const gchar *cName, Icon *icon, CairoContainer *pContainer)
{
	cd_message ("%s (%s) : %d\n", __func__, (bMounting ? "mount" : "unmount"), bSuccess);  // en cas de demontage effectif, l'icone n'est plus valide !
	if ((! bSuccess && pContainer != NULL) || icon == NULL)  // dans l'autre cas (succes), l'icone peut ne plus etre valide ! mais on s'en fout, puisqu'en cas de succes, il y'aura rechargement de l'icone, et donc on pourra balancer le message a ce moment-la.
	{
		///if (icon != NULL)
			cairo_dock_show_temporary_dialog_with_icon (bMounting ? _("failed to mount %s") : _("failed to unmount %s"), icon, pContainer, 4000, "same icon", cName);
		///else
		///	cairo_dock_show_general_message (cMessage, 4000);
	}
}



gboolean cairo_dock_fm_move_into_directory (const gchar *cURI, Icon *icon, CairoContainer *pContainer)
{
	g_return_val_if_fail (cURI != NULL && icon != NULL, FALSE);
	cd_message (" -> copie de %s dans %s", cURI, icon->cBaseURI);
	gboolean bSuccess = cairo_dock_fm_move_file (cURI, icon->cBaseURI);
	if (! bSuccess)
	{
		cd_warning ("couldn't copy this file.\nCheck that you have writing rights, and that the new does not already exist.");
		gchar *cMessage = g_strdup_printf ("Attention : couldn't copy %s into %s.\nCheck that you have writing rights, and that the name does not already exist.", cURI, icon->cBaseURI);
		cairo_dock_show_temporary_dialog (cMessage, icon, pContainer, 4000);
		g_free (cMessage);
	}
	return bSuccess;
}

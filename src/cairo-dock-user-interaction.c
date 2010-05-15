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

#include <math.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <cairo.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "cairo-dock-animations.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-user-interaction.h"

extern gboolean g_bLocked;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentIconsPath;

static int _compare_zorder (Icon *icon1, Icon *icon2)  // classe par z-order decroissant.
{
	if (icon1->iStackOrder < icon2->iStackOrder)
		return -1;
	else if (icon1->iStackOrder > icon2->iStackOrder)
		return 1;
	else
		return 0;
}
static void _cairo_dock_hide_show_in_class_subdock (Icon *icon)
{
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->Xid != 0 && ! pIcon->bIsHidden)  // par defaut on cache tout.
		{
			break;
		}
	}
	
	if (ic != NULL)  // au moins une fenetre est visible, on cache tout.
	{
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->Xid != 0 && ! pIcon->bIsHidden)
			{
				cairo_dock_minimize_xwindow (pIcon->Xid);
			}
		}
	}
	else  // on montre tout, dans l'ordre du z-order.
	{
		GList *pZOrderList = NULL;
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->Xid != 0)
				pZOrderList = g_list_insert_sorted (pZOrderList, pIcon, (GCompareFunc) _compare_zorder);
		}
		for (ic = pZOrderList; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			cairo_dock_show_xwindow (pIcon->Xid);
		}
		g_list_free (pZOrderList);
	}
}

static void _cairo_dock_show_prev_next_in_subdock (Icon *icon, gboolean bNext)
{
	Window xActiveId = cairo_dock_get_current_active_window ();
	GList *ic;
	Icon *pIcon;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->Xid == xActiveId)
			break;
	}
	if (ic == NULL)
		ic = icon->pSubDock->icons;
	
	GList *ic2 = ic;
	do
	{
		ic2 = (bNext ? cairo_dock_get_next_element (ic2, icon->pSubDock->icons) : cairo_dock_get_previous_element (ic2, icon->pSubDock->icons));
		pIcon = ic2->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			cairo_dock_show_xwindow (pIcon->Xid);
			break;
		}
	} while (ic2 != ic);
}

static void _cairo_dock_close_all_in_class_subdock (Icon *icon)
{
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->Xid != 0)
		{
			cairo_dock_close_xwindow (pIcon->Xid);
		}
	}
}

gboolean cairo_dock_notification_click_icon (gpointer pUserData, Icon *icon, CairoDock *pDock, guint iButtonState)
{
	//g_print ("+ %s (%s)\n", __func__, icon ? icon->cName : "no icon");
	if (icon == NULL)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	if (icon->pSubDock != NULL && (myAccessibility.bShowSubDockOnClick || !GTK_WIDGET_VISIBLE (pDock->container.pWidget)) && ! (iButtonState & GDK_SHIFT_MASK))  // icone de sous-dock a montrer au clic.
	{
		cairo_dock_show_subdock (icon, pDock);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_IS_URI_LAUNCHER (icon))  // URI : on lance ou on monte.
	{
		cd_debug (" uri launcher");
		gboolean bIsMounted = FALSE;
		if (icon->iVolumeID > 0)
		{
			gchar *cActivationURI = cairo_dock_fm_is_mounted (icon->cBaseURI, &bIsMounted);
			g_free (cActivationURI);
		}
		if (icon->iVolumeID > 0 && ! bIsMounted)
		{
			int answer = cairo_dock_ask_question_and_wait (_("Do you want to mount this device?"), icon, CAIRO_CONTAINER (pDock));
			if (answer != GTK_RESPONSE_YES)
			{
				return CAIRO_DOCK_LET_PASS_NOTIFICATION;
			}
			cairo_dock_fm_mount (icon, CAIRO_CONTAINER (pDock));
		}
		else
			cairo_dock_fm_launch_uri (icon->cCommand);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_IS_APPLI (icon) && ! ((iButtonState & GDK_SHIFT_MASK) && CAIRO_DOCK_IS_LAUNCHER (icon)) && ! CAIRO_DOCK_IS_APPLET (icon))  // une icone d'appli ou d'inhibiteur (hors applet) mais sans le shift+clic : on cache ou on montre.
	{
		cd_debug (" appli");
		if (cairo_dock_get_current_active_window () == icon->Xid && myTaskBar.bMinimizeOnClick)  // ne marche que si le dock est une fenêtre de type 'dock', sinon il prend le focus.
			cairo_dock_minimize_xwindow (icon->Xid);
		else
			cairo_dock_show_xwindow (icon->Xid);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_IS_LAUNCHER (icon))
	{
		//g_print ("+ launcher\n");
		if (CAIRO_DOCK_IS_MULTI_APPLI (icon) && ! (iButtonState & GDK_SHIFT_MASK))  // un lanceur ayant un sous-dock de classe ou une icone de paille : on cache ou on montre.
		{
			if (! myAccessibility.bShowSubDockOnClick)
			{
				_cairo_dock_hide_show_in_class_subdock (icon);
			}
			return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
		}
		else if (icon->cCommand != NULL && strcmp (icon->cCommand, "none") != 0)  // finalement, on lance la commande.
		{
			if (pDock->iRefCount != 0)
			{
				Icon *pMainIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
				if (CAIRO_DOCK_IS_APPLET (pMainIcon))
					return CAIRO_DOCK_LET_PASS_NOTIFICATION;
			}
			
			gboolean bSuccess = FALSE;
			if (*icon->cCommand == '<')
			{
				bSuccess = cairo_dock_simulate_key_sequence (icon->cCommand);
				if (!bSuccess)
					bSuccess = cairo_dock_launch_command_full (icon->cCommand, icon->cWorkingDirectory);
			}
			else
			{
				bSuccess = cairo_dock_launch_command_full (icon->cCommand, icon->cWorkingDirectory);
				if (! bSuccess)
					bSuccess = cairo_dock_simulate_key_sequence (icon->cCommand);
			}
			if (! bSuccess)
			{
				cairo_dock_request_icon_animation (icon, pDock, "blink", 1);  // 1 clignotement si echec
			}
			return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
		}
	}
	else
		cd_debug ("no action here");
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


gboolean cairo_dock_notification_middle_click_icon (gpointer pUserData, Icon *icon, CairoDock *pDock)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.bCloseAppliOnMiddleClick && ! CAIRO_DOCK_IS_APPLET (icon))
	{
		cairo_dock_close_xwindow (icon->Xid);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	if (CAIRO_DOCK_IS_URI_LAUNCHER (icon) && icon->pSubDock != NULL)  // icone de repertoire.
	{
		cairo_dock_fm_launch_uri (icon->cCommand);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	if (CAIRO_DOCK_IS_MULTI_APPLI (icon))
	{
		// On ferme tout.
		_cairo_dock_close_all_in_class_subdock (icon);
		
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


gboolean cairo_dock_notification_scroll_icon (gpointer pUserData, Icon *icon, CairoDock *pDock, int iDirection)
{
	if (CAIRO_DOCK_IS_MULTI_APPLI (icon) || CAIRO_DOCK_IS_CONTAINER_LAUNCHER (icon))  // on emule un alt+tab sur la liste des applis du sous-dock.
	{
		_cairo_dock_show_prev_next_in_subdock (icon, iDirection == GDK_SCROLL_DOWN);
	}
	else if (CAIRO_DOCK_IS_APPLI (icon) && icon->cClass != NULL)
	{
		Icon *pNextIcon = cairo_dock_get_prev_next_classmate_icon (icon, iDirection == GDK_SCROLL_DOWN);
		if (pNextIcon != NULL)
			cairo_dock_show_xwindow (pNextIcon->Xid);
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


gboolean cairo_dock_notification_drop_data (gpointer pUserData, const gchar *cReceivedData, Icon *icon, double fOrder, CairoContainer *pContainer)
{
	if (! CAIRO_DOCK_IS_DOCK (pContainer))
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	CairoDock *pReceivingDock = pDock;
	if (g_str_has_suffix (cReceivedData, ".desktop"))  // ajout d'un nouveau lanceur si on a lache sur ou entre 2 lanceurs.
	{
		if ((myIcons.iSeparateIcons == 1 || myIcons.iSeparateIcons == 3) && CAIRO_DOCK_IS_NORMAL_APPLI (icon))
			return CAIRO_DOCK_LET_PASS_NOTIFICATION;
		if ((myIcons.iSeparateIcons == 2 || myIcons.iSeparateIcons == 3) && CAIRO_DOCK_IS_APPLET (icon))
			return CAIRO_DOCK_LET_PASS_NOTIFICATION;
		if (fOrder == CAIRO_DOCK_LAST_ORDER && icon && icon->pSubDock != NULL)  // on a lache sur une icone de sous-dock => on l'ajoute dans le sous-dock.
		{
			pReceivingDock = icon->pSubDock;
		}
	}
	else  // c'est un fichier.
	{
		if (fOrder == CAIRO_DOCK_LAST_ORDER)  // on a lache dessus.
		{
			if (CAIRO_DOCK_IS_LAUNCHER (icon))
			{
				if (CAIRO_DOCK_IS_URI_LAUNCHER (icon))
				{
					if (icon->pSubDock != NULL || icon->iVolumeID != 0)  // on le lache sur un repertoire ou un point de montage.
					{
						cairo_dock_fm_move_into_directory (cReceivedData, icon, pContainer);
						return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
					}
					else  // on le lache sur un fichier.
					{
						return CAIRO_DOCK_LET_PASS_NOTIFICATION;
					}
				}
				else if (CAIRO_DOCK_IS_CONTAINER_LAUNCHER (icon))  // on le lache sur un sous-dock de lanceurs.
				{
					pReceivingDock = icon->pSubDock;
				}
				else  // on le lache sur un lanceur.
				{
					gchar *cPath = NULL;
					if (strncmp (cReceivedData, "file://", 7) == 0)  // tous les programmes ne gerent pas les URI; pour parer au cas ou il ne le gererait pas, dans le cas d'un fichier local, on convertit en un chemin
					{
						cPath = g_filename_from_uri (cReceivedData, NULL, NULL);
					}
					gchar *cCommand = g_strdup_printf ("%s \"%s\"", icon->cCommand, cPath ? cPath : cReceivedData);
					cd_message ("will open the file with the command '%s'...\n", cCommand);
					g_spawn_command_line_async (cCommand, NULL);
					g_free (cPath);
					g_free (cCommand);
					cairo_dock_request_icon_animation (icon, pDock, "blink", 2);
					return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
				}
			}
			else if (CAIRO_DOCK_IS_APPLI (icon))  // une appli normale
			{
				gchar *ext = strrchr (cReceivedData, '.');
				if (!ext)
					return CAIRO_DOCK_LET_PASS_NOTIFICATION;
				if (strcmp (ext, ".png") == 0 || strcmp (ext, ".svg") == 0)
				{
					if (!myTaskBar.bOverWriteXIcons)
					{
						myTaskBar.bOverWriteXIcons = TRUE;
						cairo_dock_update_conf_file (g_cConfFile,
							G_TYPE_BOOLEAN, "TaskBar", "overwrite xicon", myTaskBar.bOverWriteXIcons,
							G_TYPE_INVALID);
						cairo_dock_show_temporary_dialog_with_default_icon (_("The option 'overwrite X icons' has been automatically enabled in the config.\nIt is located in the 'Taskbar' module."), icon, pContainer, 6000);
					}
					
					gchar *cPath = NULL;
					if (strncmp (cReceivedData, "file://", 7) == 0)  // tous les programmes ne gerent pas les URI; pour parer au cas ou il ne le gererait pas, dans le cas d'un fichier local, on convertit en un chemin
					{
						cPath = g_filename_from_uri (cReceivedData, NULL, NULL);
					}
					
					gchar *cCommand = g_strdup_printf ("cp '%s' '%s/%s%s'", cPath?cPath:cReceivedData, g_cCurrentIconsPath, icon->cClass, ext);
					int r = system (cCommand);
					g_free (cCommand);
					
					cairo_dock_reload_icon_image (icon, pContainer);
					cairo_dock_redraw_icon (icon, pContainer);
				}
				return CAIRO_DOCK_LET_PASS_NOTIFICATION;
			}
			else  // autre chose.
			{
				return CAIRO_DOCK_LET_PASS_NOTIFICATION;
			}
		}
		else  // on a lache a cote.
		{
			Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
			if (CAIRO_DOCK_IS_URI_LAUNCHER (pPointingIcon))  // on a lache dans un dock qui est un repertoire, on copie donc le fichier dedans.
			{
				cairo_dock_fm_move_into_directory (cReceivedData, icon, pContainer);
				return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
			}
		}
	}

	if (g_bLocked)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	cairo_dock_add_new_launcher_by_uri (cReceivedData, pReceivingDock, fOrder);
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

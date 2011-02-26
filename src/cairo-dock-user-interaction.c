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
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-gui-backend.h"
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
	if (icon->pSubDock == NULL || icon->pSubDock->icons == NULL)
		return;
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
	if (icon->pSubDock == NULL || icon->pSubDock->icons == NULL)
		return;
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
	if (icon->pSubDock == NULL || icon->pSubDock->icons == NULL)
		return;
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


static gboolean _launch_icon_command (Icon *icon, CairoDock *pDock)
{
	if (icon->cCommand == NULL)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	if (pDock->iRefCount != 0)  // let the applets handle their own sub-icons.
	{
		Icon *pMainIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
		if (CAIRO_DOCK_IS_APPLET (pMainIcon))
			return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	}
	
	gboolean bSuccess = FALSE;
	if (*icon->cCommand == '<')  // shortkey
	{
		bSuccess = cairo_dock_simulate_key_sequence (icon->cCommand);
		if (!bSuccess)
			bSuccess = cairo_dock_launch_command_full (icon->cCommand, icon->cWorkingDirectory);
	}
	else  // normal command
	{
		bSuccess = cairo_dock_launch_command_full (icon->cCommand, icon->cWorkingDirectory);
		if (! bSuccess)
			bSuccess = cairo_dock_simulate_key_sequence (icon->cCommand);
	}
	if (! bSuccess)
	{
		cairo_dock_request_icon_animation (icon, pDock, "blink", 1);  // 1 blink if fail.
	}
	return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
}
gboolean cairo_dock_notification_click_icon (gpointer pUserData, Icon *icon, CairoContainer *pContainer, guint iButtonState)
{
	//g_print ("+ %s (%s)\n", __func__, icon ? icon->cName : "no icon");
	if (icon == NULL || ! CAIRO_DOCK_IS_DOCK (pContainer))
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	
	if (iButtonState & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))  // shit or ctrl + click
	{
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)
		|| CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon)
		|| CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
		{
			return _launch_icon_command (icon, pDock);
		}
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	}
	
	/// TODO: compiz/kwin integration ...
	if (CAIRO_DOCK_IS_MULTI_APPLI(icon))
	{
		gboolean bAllHidden = TRUE;
		if (icon->pSubDock != NULL)
		{
			Icon *pOneIcon;
			GList *ic;
			for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
			{
				pOneIcon = ic->data;
				bAllHidden &= pOneIcon->bIsHidden;
			}
		}
		if (! bAllHidden)  // the Expose does not work for hidden windows, so let's skip the case where all the windows of the class are hidden.
		{
			gchar *cCommand = g_strdup_printf ("dbus-send  --type=method_call --dest=org.freedesktop.compiz  /org/freedesktop/compiz/scale/allscreens/initiate_all_key  org.freedesktop.compiz.activate string:'root' int32:%d string:\"match\"  string:'class=.*%s'", cairo_dock_get_root_id (), icon->cClass+1);  /// we need the real class here...
			g_print ("%s\n", cCommand);
			int r = system (cCommand);
			g_free (cCommand);
			if (r == 0)
			{
				if (icon->pSubDock != NULL)
				{
					cairo_dock_emit_leave_signal (CAIRO_CONTAINER (icon->pSubDock));
				}
				return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
			}
		}
	}
	if (icon->pSubDock != NULL && (myDocksParam.bShowSubDockOnClick || !GTK_WIDGET_VISIBLE (icon->pSubDock->container.pWidget)))  // icon pointing to a sub-dock with either "sub-dock activation on click" option enabled, or sub-dock not visible -> open the sub-dock
	{
		cairo_dock_show_subdock (icon, pDock);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_IS_APPLI (icon) && ! CAIRO_DOCK_IS_APPLET (icon))  // icon holding an appli, but not being an applet -> show/hide the window.
	{
		if (cairo_dock_get_current_active_window () == icon->Xid && myTaskbarParam.bMinimizeOnClick)  // ne marche que si le dock est une fenêtre de type 'dock', sinon il prend le focus.
			cairo_dock_minimize_xwindow (icon->Xid);
		else
			cairo_dock_show_xwindow (icon->Xid);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_IS_MULTI_APPLI (icon))  // icon holding a class sub-dock -> show/hide the windows of the class.
	{
		if (! myDocksParam.bShowSubDockOnClick)
		{
			_cairo_dock_hide_show_in_class_subdock (icon);
		}
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon))  // finally, launcher being none of the previous cases -> launch the command
	{
		return _launch_icon_command (icon, pDock);
	}
	else
	{
		cd_debug ("no action here");
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


gboolean cairo_dock_notification_middle_click_icon (gpointer pUserData, Icon *icon, CairoContainer *pContainer)
{
	if (icon == NULL || ! CAIRO_DOCK_IS_DOCK (pContainer))
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	
	if (CAIRO_DOCK_IS_APPLI (icon) && ! CAIRO_DOCK_IS_APPLET (icon) && myTaskbarParam.iActionOnMiddleClick != 0)
	{
		switch (myTaskbarParam.iActionOnMiddleClick)
		{
			case 1:  // close
				cairo_dock_close_xwindow (icon->Xid);
			break;
			case 2:  // minimise
				if (! icon->bIsHidden)
				{
					cairo_dock_minimize_xwindow (icon->Xid);
				}
			break;
			case 3:  // launch new
				if (icon->cCommand != NULL)
				{
					cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
					cairo_dock_notify_on_object (CAIRO_CONTAINER (pDock), NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
				}
			break;
		}
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_IS_MULTI_APPLI (icon) && myTaskbarParam.iActionOnMiddleClick != 0)
	{
		switch (myTaskbarParam.iActionOnMiddleClick)
		{
			case 1:  // close
				_cairo_dock_close_all_in_class_subdock (icon);
			break;
			case 2:  // minimise
				_cairo_dock_hide_show_in_class_subdock (icon);
			break;
			case 3:  // launch new
				if (icon->cCommand != NULL)
				{
					cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
					cairo_dock_notify_on_object (CAIRO_CONTAINER (pDock), NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
				}
			break;
		}
		
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


gboolean cairo_dock_notification_scroll_icon (gpointer pUserData, Icon *icon, CairoContainer *pContainer, int iDirection)
{
	if (CAIRO_DOCK_IS_MULTI_APPLI (icon) || CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))  // on emule un alt+tab sur la liste des applis du sous-dock.
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
	if (g_str_has_suffix (cReceivedData, ".desktop"))  // .desktop -> add a new launcher if dropped on or amongst launchers. 
	{
		if ((myIconsParam.iSeparateIcons == 1 || myIconsParam.iSeparateIcons == 3) && CAIRO_DOCK_IS_NORMAL_APPLI (icon))
			return CAIRO_DOCK_LET_PASS_NOTIFICATION;
		if ((myIconsParam.iSeparateIcons == 2 || myIconsParam.iSeparateIcons == 3) && CAIRO_DOCK_IS_APPLET (icon))
			return CAIRO_DOCK_LET_PASS_NOTIFICATION;
		if (fOrder == CAIRO_DOCK_LAST_ORDER && icon && icon->pSubDock != NULL)  // drop onto a container icon.
		{
			pReceivingDock = icon->pSubDock;  // -> add into the pointed sub-dock.
		}
	}
	else  // file.
	{
		if (fOrder == CAIRO_DOCK_LAST_ORDER)  // dropped on an icon
		{
			if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))  // sub-dock -> propagate to the sub-dock.
			{
				pReceivingDock = icon->pSubDock;
			}
			else if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)
			|| CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon)
			|| CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon)) // launcher/appli -> fire the command with this file.
			{
				if (icon->cCommand == NULL)
					return CAIRO_DOCK_LET_PASS_NOTIFICATION;
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
			else  // skip any other case.
			{
				return CAIRO_DOCK_LET_PASS_NOTIFICATION;
			}
		}  // else: dropped between 2 icons -> try to add it.
	}

	if (g_bLocked || myDocksParam.bLockAll)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	Icon *pNewIcon = cairo_dock_add_new_launcher_by_uri (cReceivedData, pReceivingDock, fOrder);
	
	return (pNewIcon ? CAIRO_DOCK_INTERCEPT_NOTIFICATION : CAIRO_DOCK_LET_PASS_NOTIFICATION);
}


void cairo_dock_set_custom_icon_on_appli (const gchar *cFilePath, Icon *icon, CairoContainer *pContainer)
{
	g_return_if_fail (CAIRO_DOCK_IS_APPLI (icon) && cFilePath != NULL);
	gchar *ext = strrchr (cFilePath, '.');
	if (!ext)
		return;
	cd_debug ("%s (%s)", __func__, cFilePath);
	if ((strcmp (ext, ".png") == 0 || strcmp (ext, ".svg") == 0) && !myDocksParam.bLockAll && ! myDocksParam.bLockIcons)
	{
		if (!myTaskbarParam.bOverWriteXIcons)
		{
			myTaskbarParam.bOverWriteXIcons = TRUE;
			cairo_dock_update_conf_file (g_cConfFile,
				G_TYPE_BOOLEAN, "TaskBar", "overwrite xicon", myTaskbarParam.bOverWriteXIcons,
				G_TYPE_INVALID);
			cairo_dock_show_temporary_dialog_with_default_icon (_("The option 'overwrite X icons' has been automatically enabled in the config.\nIt is located in the 'Taskbar' module."), icon, pContainer, 6000);
		}
		
		gchar *cPath = NULL;
		if (strncmp (cFilePath, "file://", 7) == 0)
		{
			cPath = g_filename_from_uri (cFilePath, NULL, NULL);
		}
		
		gchar *cCommand = g_strdup_printf ("cp \"%s\" \"%s/%s%s\"", cPath?cPath:cFilePath, g_cCurrentIconsPath, icon->cClass, ext);
		cd_debug (" -> '%s'", cCommand);
		int r = system (cCommand);
		g_free (cCommand);
		
		cairo_dock_reload_icon_image (icon, pContainer);
		cairo_dock_redraw_icon (icon, pContainer);
	}
}



static guint s_iSidRefreshGUI = 0;
static gboolean _refresh_gui (gpointer data)
{
	//if (s_pLauncherGuiBackend && s_pLauncherGuiBackend->refresh_gui)
	//	s_pLauncherGuiBackend->refresh_gui ();
	
	s_iSidRefreshGUI = 0;
	return FALSE;
}
void cairo_dock_trigger_refresh_gui (void)
{
	if (s_iSidRefreshGUI != 0)
		return;
	
	s_iSidRefreshGUI = g_idle_add ((GSourceFunc) _refresh_gui, NULL);
}

gboolean cairo_dock_notification_configure_desklet (gpointer pUserData, CairoDesklet *pDesklet)
{
	g_print ("desklet %s configured\n", pDesklet->pIcon?pDesklet->pIcon->cName:"unknown");
	cairo_dock_gui_trigger_update_desklet_params (pDesklet);
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_notification_icon_moved (gpointer pUserData, Icon *pIcon, CairoDock *pDock)
{
	g_print ("icon %s moved\n", pIcon?pIcon->cName:"unknown");
	
	if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
	|| (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon) && pIcon->cDesktopFileName)
	|| CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cairo_dock_gui_trigger_reload_items ();
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_notification_icon_inserted (gpointer pUserData, Icon *pIcon, CairoDock *pDock)
{
	g_print ("icon %s inserted (%.2f)\n", pIcon?pIcon->cName:"unknown", pIcon->fInsertRemoveFactor);
	//if (pIcon->fInsertRemoveFactor == 0)
	//	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	if ( ( (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon)) && pIcon->cDesktopFileName)
	|| CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cairo_dock_gui_trigger_reload_items ();
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_notification_icon_removed (gpointer pUserData, Icon *pIcon, CairoDock *pDock)
{
	g_print ("icon %s removed (%.2f)\n", pIcon?pIcon->cName:"unknown", pIcon->fInsertRemoveFactor);
	//if (pIcon->fInsertRemoveFactor == 0)
	//	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	if ( ( (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon)) && pIcon->cDesktopFileName)
	|| CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cairo_dock_gui_trigger_reload_items ();
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_notification_dock_destroyed (gpointer pUserData, CairoDock *pDock)
{
	g_print ("dock destroyed\n");
		cairo_dock_gui_trigger_reload_items ();
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_notification_module_activated (gpointer pUserData, const gchar *cModuleName, gboolean bActivated)
{
	//g_print ("module %s (de)activated (%d)\n", cModuleName, bActivated);
		cairo_dock_gui_trigger_update_module_state (cModuleName);
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_notification_module_registered (gpointer pUserData, const gchar *cModuleName, gboolean bRegistered)
{
	//g_print ("module %s (un)registered (%d)\n", cModuleName, bRegistered);
		cairo_dock_gui_trigger_update_modules_list ();
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_notification_module_detached (gpointer pUserData, CairoDockModuleInstance *pInstance, gboolean bIsDetached)
{
	g_print ("module %s (de)tached (%d)\n", pInstance->pModule->pVisitCard->cModuleName, bIsDetached);
	cairo_dock_gui_trigger_update_module_container (pInstance, bIsDetached);
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

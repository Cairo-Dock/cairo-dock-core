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

#include "cairo-dock-icon-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-launcher-manager.h"  // gldi_launcher_add_new
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_full
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dialog-factory.h"  // gldi_dialog_show_temporary_with_default_icon
#include "cairo-dock-themes-manager.h"  // cairo_dock_update_conf_file
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keybinder.h"  // cairo_dock_trigger_shortkey
#include "cairo-dock-animations.h"  // gldi_icon_request_animation
#include "cairo-dock-class-manager.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-user-interaction.h"

extern gboolean g_bLocked;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentIconsPath;

static int _compare_zorder (Icon *icon1, Icon *icon2)  // classe par z-order decroissant.
{
	if (icon1->pAppli->iStackOrder < icon2->pAppli->iStackOrder)
		return -1;
	else if (icon1->pAppli->iStackOrder > icon2->pAppli->iStackOrder)
		return 1;
	else
		return 0;
}
static void _cairo_dock_hide_show_in_class_subdock (Icon *icon)
{
	if (icon->pSubDock == NULL || icon->pSubDock->icons == NULL)
		return;
	// if the appli has the focus, we hide all the windows, else we show them all
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->pAppli != NULL && pIcon->pAppli == gldi_windows_get_active ())
		{
			break;
		}
	}
	
	if (ic != NULL)  // one of the windows of the appli has the focus -> hide.
	{
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->pAppli != NULL && ! pIcon->pAppli->bIsHidden)
			{
				gldi_window_minimize (pIcon->pAppli);
			}
		}
	}
	else  // on montre tout, dans l'ordre du z-order.
	{
		GList *pZOrderList = NULL;
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->pAppli != NULL)
				pZOrderList = g_list_insert_sorted (pZOrderList, pIcon, (GCompareFunc) _compare_zorder);
		}
		
		int iNumDesktop, iViewPortX, iViewPortY;
		gldi_desktop_get_current (&iNumDesktop, &iViewPortX, &iViewPortY);
		
		for (ic = pZOrderList; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (gldi_window_is_on_desktop (pIcon->pAppli, iNumDesktop, iViewPortX, iViewPortY))
				break;
		}
		if (pZOrderList && ic == NULL)  // no window on the current desktop -> take the first desktop
		{
			pIcon = pZOrderList->data;
			iNumDesktop = pIcon->pAppli->iNumDesktop;
			iViewPortX = pIcon->pAppli->iViewPortX;
			iViewPortY = pIcon->pAppli->iViewPortY;
		}
		
		for (ic = pZOrderList; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (gldi_window_is_on_desktop (pIcon->pAppli, iNumDesktop, iViewPortX, iViewPortY))
				gldi_window_show (pIcon->pAppli);
		}
		g_list_free (pZOrderList);
	}
}

static void _cairo_dock_show_prev_next_in_subdock (Icon *icon, gboolean bNext)
{
	if (icon->pSubDock == NULL || icon->pSubDock->icons == NULL)
		return;
	GldiWindowActor *pActiveAppli = gldi_windows_get_active ();
	GList *ic;
	Icon *pIcon;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->pAppli == pActiveAppli)
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
			gldi_window_show (pIcon->pAppli);
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
		if (pIcon->pAppli != NULL)
		{
			gldi_window_close (pIcon->pAppli);
		}
	}
}

static void _show_all_windows (GList *pIcons)
{
	Icon *pIcon;
	GList *ic;
	for (ic = pIcons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->pAppli != NULL && pIcon->pAppli->bIsHidden)  // a window is hidden...
		{
			gldi_window_show (pIcon->pAppli);
		}
	}
}


static gboolean _launch_icon_command (Icon *icon)
{
	if (gldi_class_is_starting (icon->cClass) || gldi_icon_is_launching (icon))  // do not launch it twice (avoid wrong double click); so we can't launch an app several times rapidly (must wait until it's launched), but it's probably not a useful feature anyway
		return GLDI_NOTIFICATION_INTERCEPT;
	
	if (! gldi_icon_launch_command (icon))
	{
		gldi_icon_request_animation (icon, "blink", 1);  // 1 blink if fail.
	}
	return GLDI_NOTIFICATION_INTERCEPT;
}
gboolean cairo_dock_notification_click_icon (G_GNUC_UNUSED gpointer pUserData, Icon *icon, GldiContainer *pContainer, guint iButtonState)
{
	if (icon == NULL || ! CAIRO_DOCK_IS_DOCK (pContainer))
		return GLDI_NOTIFICATION_LET_PASS;
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	
	// shit/ctrl + click on an icon that is linked to a program => re-launch this program.
	if (iButtonState & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))  // shit or ctrl + click
	{
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)
		|| CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon)
		|| CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
		{
			_launch_icon_command (icon);
		}
		return GLDI_NOTIFICATION_LET_PASS;
	}
	
	// scale on an icon holding a class sub-dock.
	if (CAIRO_DOCK_IS_MULTI_APPLI(icon))
	{
		if (myTaskbarParam.bPresentClassOnClick // if we want to use this feature
		&& (!myDocksParam.bShowSubDockOnClick  // if sub-docks are shown on mouse over
			|| gldi_container_is_visible (CAIRO_CONTAINER (icon->pSubDock)))  // or this sub-dock is already visible
		&& gldi_desktop_present_class (icon->cClass, pContainer)) // we use the scale plugin if it's possible
		{
			_show_all_windows (icon->pSubDock->icons); // show all windows
			// in case the dock is visible or about to be visible, hide it, as it would confuse the user to have both.
			cairo_dock_emit_leave_signal (CAIRO_CONTAINER (icon->pSubDock));
			return GLDI_NOTIFICATION_INTERCEPT;
		}
	}
	
	// else handle sub-docks showing on click, applis and launchers (not applets).
	if (icon->pSubDock != NULL && (myDocksParam.bShowSubDockOnClick || !gldi_container_is_visible (CAIRO_CONTAINER (icon->pSubDock))))  // icon pointing to a sub-dock with either "sub-dock activation on click" option enabled, or sub-dock not visible -> open the sub-dock
	{
		cairo_dock_show_subdock (icon, pDock);
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	else if (CAIRO_DOCK_IS_APPLI (icon) && ! CAIRO_DOCK_IS_APPLET (icon))  // icon holding an appli, but not being an applet -> show/hide the window.
	{
		GldiWindowActor *pAppli = icon->pAppli;
		if (gldi_windows_get_active () == pAppli && myTaskbarParam.bMinimizeOnClick && ! pAppli->bIsHidden &&
				(!gldi_window_manager_can_track_workspaces () || gldi_window_is_on_current_desktop (pAppli)))  // ne marche que si le dock est une fenêtre de type 'dock', sinon il prend le focus.
			gldi_window_minimize (pAppli);
		else
			gldi_window_show (pAppli);
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	else if (CAIRO_DOCK_IS_MULTI_APPLI (icon))  // icon holding a class sub-dock -> show/hide the windows of the class.
	{
		if (! myDocksParam.bShowSubDockOnClick)
		{
			_cairo_dock_hide_show_in_class_subdock (icon);
		}
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	else if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon))  // finally, launcher being none of the previous cases -> launch the command
	{
		_launch_icon_command (icon);
	}
	else  // for applets and their sub-icons, let the module-instance handles the click; for separators, no action.
	{
		cd_debug ("no action here");
	}
	return GLDI_NOTIFICATION_LET_PASS;
}


gboolean cairo_dock_notification_middle_click_icon (G_GNUC_UNUSED gpointer pUserData, Icon *icon, GldiContainer *pContainer)
{
	if (icon == NULL || ! CAIRO_DOCK_IS_DOCK (pContainer))
		return GLDI_NOTIFICATION_LET_PASS;
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	
	if (CAIRO_DOCK_IS_APPLI (icon) && ! CAIRO_DOCK_IS_APPLET (icon) && myTaskbarParam.iActionOnMiddleClick != CAIRO_APPLI_ACTION_NONE)
	{
		switch (myTaskbarParam.iActionOnMiddleClick)
		{
			case CAIRO_APPLI_ACTION_CLOSE:  // close
				gldi_window_close (icon->pAppli);
			break;
			case CAIRO_APPLI_ACTION_MINIMISE:  // minimise
				if (! icon->pAppli->bIsHidden)
				{
					gldi_window_minimize (icon->pAppli);
				}
			break;
			case CAIRO_APPLI_ACTION_LAUNCH_NEW:  // launch new
				if (icon->pClassApp || icon->pCustomLauncher)
				{
					gldi_object_notify (pDock, NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // emulate a shift+left click
				}
			break;
			default:  // nothing
			break;
		}
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	else if (CAIRO_DOCK_IS_MULTI_APPLI (icon) && myTaskbarParam.iActionOnMiddleClick != CAIRO_APPLI_ACTION_NONE)
	{
		switch (myTaskbarParam.iActionOnMiddleClick)
		{
			case CAIRO_APPLI_ACTION_CLOSE:  // close
				_cairo_dock_close_all_in_class_subdock (icon);
			break;
			case CAIRO_APPLI_ACTION_MINIMISE:  // minimise
				_cairo_dock_hide_show_in_class_subdock (icon);
			break;
			case CAIRO_APPLI_ACTION_LAUNCH_NEW:  // launch new
				if (icon->pClassApp || icon->pCustomLauncher)
				{
					gldi_object_notify (CAIRO_CONTAINER (pDock), NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // emulate a shift+left click
				}
			break;
			default:  // nothing
			break;
		}
		
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	return GLDI_NOTIFICATION_LET_PASS;
}


gboolean cairo_dock_notification_scroll_icon (G_GNUC_UNUSED gpointer pUserData, Icon *icon, G_GNUC_UNUSED GldiContainer *pContainer, int iDirection)
{
	if (CAIRO_DOCK_IS_MULTI_APPLI (icon) || CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))  // emulate an alt+tab on the list of applis of the sub-dock
	{
		_cairo_dock_show_prev_next_in_subdock (icon, iDirection == GDK_SCROLL_DOWN);
	}
	else if (CAIRO_DOCK_IS_APPLI (icon) && icon->cClass != NULL)
	{
		Icon *pNextIcon = cairo_dock_get_prev_next_classmate_icon (icon, iDirection == GDK_SCROLL_DOWN);
		if (pNextIcon != NULL)
			gldi_window_show (pNextIcon->pAppli);
	}
	return GLDI_NOTIFICATION_LET_PASS;
}


gboolean cairo_dock_notification_drop_data_selection (G_GNUC_UNUSED gpointer pUserData,
		GtkSelectionData *selection_data, Icon *icon, double fOrder, GldiContainer *pContainer,
		gboolean *bHandled)
{
	if (! CAIRO_DOCK_IS_DOCK (pContainer))
		return GLDI_NOTIFICATION_LET_PASS;
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	
	gchar **data = NULL;
	data = gtk_selection_data_get_uris (selection_data);
	if (data) cd_debug ("got URIs");
	else
	{
		guchar *tmp = gtk_selection_data_get_text (selection_data);
		if (!tmp) return GLDI_NOTIFICATION_LET_PASS;
		cd_debug ("got text");
		// we will interpret the data as a list of filenames, so we always split by newline
		data = g_strsplit ((gchar*)tmp, "\n", -1);
		g_free (tmp);
		// remove empty entries
		unsigned int i, j = 0;
		for (i = 0; data[i]; i++)
		{
			gboolean bIsSpace = TRUE;
			gchar *tmp2 = data[i];
			for(; *tmp2; ++tmp2)
				if (! (*tmp2 == ' ' || *tmp2 == '\t' || *tmp2 == '\r'))
				{
					bIsSpace = FALSE;
					break;
				}
			if (bIsSpace) g_free (data[i]);
			else
			{
				gchar *tmp3 = NULL;
				if (data[i][0] == '/')
				{
					// convert absolute paths to URI style (if it is not an
					// absolute path, we cannot do much, just hope that the
					// app can interpret it as a command line argument)
					tmp3 = data[i];
					data[j] = g_strdup_printf ("file://%s", tmp3);
					g_free (tmp3);
				}
				else if (i != j) data[j] = data[i];
				j++;
			}
		}
		for (; data[j]; j++) data[j] = NULL;
	}
	
	gboolean ret = GLDI_NOTIFICATION_LET_PASS;
	gboolean bAllDesktop = TRUE;
	{
		gchar **tmp;
		for (tmp = data; *tmp; ++tmp)
			if (! g_str_has_suffix (*tmp, ".desktop"))
			{
				bAllDesktop = FALSE;
				break;
			}
	}
	if (bAllDesktop)
	{
		// all are .desktop files, add as launchers
		CairoDock *pReceivingDock = pDock;
		if (fOrder == CAIRO_DOCK_LAST_ORDER && icon && CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon)
				&& icon->pSubDock != NULL)  // drop onto a container icon.
		{
			pReceivingDock = icon->pSubDock;  // -> add into the pointed sub-dock.
		}
		gchar **tmp;
		for (tmp = data; *tmp; ++tmp)
		{
			// only add launchers if we got a valid .desktop file
			gldi_launcher_add_new_full (*tmp, pReceivingDock, fOrder, TRUE);
		}
		ret = GLDI_NOTIFICATION_INTERCEPT;
		*bHandled = TRUE;
	}
	else if (icon && (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)
			|| CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon)
			|| CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon)))
	{
		GDesktopAppInfo *app = icon->pClassApp ? icon->pClassApp : icon->pCustomLauncher;
		if (app)
		{
			// GdkAppLaunchContext will automatically use startup notify / xdg-activation,
			// allowing e.g. the app to raise itself if necessary
			GList *list = NULL;
			gchar **tmp;
			GdkAppLaunchContext *context = gdk_display_get_app_launch_context (gdk_display_get_default ());
			// we always treat the parameters as URIs, this will work for apps that expect URIs (most cases)
			// and GIO will anyway try to convert to files (and potentially mess things up) for apps that only expect files
			for (tmp = data; *tmp; ++tmp) list = g_list_append (list, *tmp);
			g_app_info_launch_uris (G_APP_INFO (app), list, G_APP_LAUNCH_CONTEXT (context), NULL);
			g_list_free (list);
			g_object_unref (context); // will be kept by GIO if necessary (and we don't care about the "launched" signal in this case)
			ret = GLDI_NOTIFICATION_INTERCEPT;
			*bHandled = TRUE;
		}
	}
	g_strfreev (data);
	return ret;
}


void cairo_dock_set_custom_icon_on_appli (const gchar *cFilePath, Icon *icon, GldiContainer *pContainer)
{
	g_return_if_fail (CAIRO_DOCK_IS_APPLI (icon) && cFilePath != NULL);
	gchar *ext = strrchr (cFilePath, '.');
	if (!ext)
		return;
	cd_debug ("%s (%s - %s)", __func__, cFilePath, icon->cFileName);
	if ((strcmp (ext, ".png") == 0 || strcmp (ext, ".svg") == 0) && !myDocksParam.bLockAll) // && ! myDocksParam.bLockIcons) // or if we have to hide the option...
	{
		if (!myTaskbarParam.bOverWriteXIcons)
		{
			myTaskbarParam.bOverWriteXIcons = TRUE;
			cairo_dock_update_conf_file (g_cConfFile,
				G_TYPE_BOOLEAN, "TaskBar", "overwrite xicon", myTaskbarParam.bOverWriteXIcons,
				G_TYPE_INVALID);
			gldi_dialog_show_temporary_with_default_icon (_("The option 'overwrite X icons' has been automatically enabled in the config.\nIt is located in the 'Taskbar' module."), icon, pContainer, 6000);
		}
		
		gchar *cPath = NULL;
		if (strncmp (cFilePath, "file://", 7) == 0)
		{
			cPath = g_filename_from_uri (cFilePath, NULL, NULL);
		}
		
		const gchar *cClassIcon = cairo_dock_get_class_icon (icon->cClass);
		if (cClassIcon == NULL)
			cClassIcon = icon->cClass;
		
		gchar *cDestPath = g_strdup_printf ("%s/%s%s", g_cCurrentIconsPath, cClassIcon, ext);
		cairo_dock_copy_file (cPath?cPath:cFilePath, cDestPath);
		g_free (cDestPath);
		g_free (cPath);
		
		cairo_dock_reload_icon_image (icon, pContainer);
		cairo_dock_redraw_icon (icon);
	}
}


gboolean cairo_dock_notification_configure_desklet (G_GNUC_UNUSED gpointer pUserData, CairoDesklet *pDesklet)
{
	//g_print ("desklet %s configured\n", pDesklet->pIcon?pDesklet->pIcon->cName:"unknown");
	cairo_dock_gui_update_desklet_params (pDesklet);
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_icon_moved (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon, G_GNUC_UNUSED CairoDock *pDock)
{
	//g_print ("icon %s moved\n", pIcon?pIcon->cName:"unknown");
	
	if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
	|| (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon) && pIcon->cDesktopFileName)
	|| CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_icon_inserted (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon, G_GNUC_UNUSED CairoDock *pDock)
{
	//g_print ("icon %s inserted (%.2f)\n", pIcon?pIcon->cName:"unknown", pIcon->fInsertRemoveFactor);
	//if (pIcon->fInsertRemoveFactor == 0)
	//	return GLDI_NOTIFICATION_LET_PASS;
	
	if ( ( (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon)) && pIcon->cDesktopFileName)
	|| CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_icon_removed (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon, G_GNUC_UNUSED CairoDock *pDock)
{
	//g_print ("icon %s removed (%.2f)\n", pIcon?pIcon->cName:"unknown", pIcon->fInsertRemoveFactor);
	//if (pIcon->fInsertRemoveFactor == 0)
	//	return GLDI_NOTIFICATION_LET_PASS;
	
	if ( ( (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon)) && pIcon->cDesktopFileName)
	|| CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_desklet_added_removed (G_GNUC_UNUSED gpointer pUserData, G_GNUC_UNUSED CairoDesklet *pDesklet)
{
	//Icon *pIcon = pDesklet->pIcon;
	//g_print ("desklet %s removed\n", pIcon?pIcon->cName:"unknown");
	
	cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_dock_destroyed (G_GNUC_UNUSED gpointer pUserData, G_GNUC_UNUSED CairoDock *pDock)
{
	//g_print ("dock destroyed\n");
	cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_module_activated (G_GNUC_UNUSED gpointer pUserData, const gchar *cModuleName, G_GNUC_UNUSED gboolean bActivated)
{
	//g_print ("module %s (de)activated (%d)\n", cModuleName, bActivated);
	cairo_dock_gui_trigger_update_module_state (cModuleName);
	
	cairo_dock_gui_trigger_reload_items ();  // for plug-ins that don't have an applet, like Cairo-Pinguin.
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_module_registered (G_GNUC_UNUSED gpointer pUserData, G_GNUC_UNUSED const gchar *cModuleName, G_GNUC_UNUSED gboolean bRegistered)
{
	//g_print ("module %s (un)registered (%d)\n", cModuleName, bRegistered);
	cairo_dock_gui_trigger_update_modules_list ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_module_detached (G_GNUC_UNUSED gpointer pUserData, GldiModuleInstance *pInstance, gboolean bIsDetached)
{
	//g_print ("module %s (de)tached (%d)\n", pInstance->pModule->pVisitCard->cModuleName, bIsDetached);
	cairo_dock_gui_trigger_update_module_container (pInstance, bIsDetached);
	
	cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_shortkey_added_removed_changed (G_GNUC_UNUSED gpointer pUserData, G_GNUC_UNUSED GldiShortkey *pShortkey)
{
	cairo_dock_gui_trigger_reload_shortkeys ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

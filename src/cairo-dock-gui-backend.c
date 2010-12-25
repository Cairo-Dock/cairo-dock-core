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
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "cairo-dock-config.h"
#include "cairo-dock-gui-main.h"
#include "cairo-dock-gui-simple.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-gui-backend.h"

extern gchar *g_cCairoDockDataDir;
extern CairoDock *g_pMainDock;

static CairoDockMainGuiBackend *s_pMainGuiBackend = NULL;
static CairoDockItemsGuiBackend *s_pItemsGuiBackend = NULL;
static int s_iCurrentMode = 0;

void cairo_dock_load_user_gui_backend (int iMode)  // 0 = simple
{
	if (iMode == 1)
	{
		cairo_dock_register_main_gui_backend ();
	}
	else
	{
		cairo_dock_register_simple_gui_backend ();
	}
	s_iCurrentMode = iMode;
}

static void on_click_switch_mode (GtkButton *button, gpointer data)
{
	cairo_dock_close_gui ();
	
	int iNewMode = (s_iCurrentMode == 1 ? 0 : 1);
	
	gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
	cairo_dock_update_conf_file (cConfFilePath,
		G_TYPE_INT, "Gui", "mode", iNewMode,
		G_TYPE_INVALID);
	g_free (cConfFilePath);
	
	cairo_dock_load_user_gui_backend (iNewMode);
	
	cairo_dock_show_main_gui ();
}
GtkWidget *cairo_dock_make_switch_gui_button (void)
{
	GtkWidget *pSwitchButton = gtk_button_new_with_label (s_pMainGuiBackend->cDisplayedName);
	if (s_pMainGuiBackend->cTooltip)
		gtk_widget_set_tooltip_text (pSwitchButton, s_pMainGuiBackend->cTooltip);
	
	GtkWidget *pImage = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (pSwitchButton), pImage);
	g_signal_connect (G_OBJECT (pSwitchButton), "clicked", G_CALLBACK(on_click_switch_mode), NULL);
	return pSwitchButton;
}

gboolean cairo_dock_theme_manager_is_integrated (void)
{
	return (s_pMainGuiBackend && s_pMainGuiBackend->bCanManageThemes);
}



static guint s_iSidUpdateDesklet = 0;
static CairoDesklet *s_DeskletToUpdate = NULL;
static gboolean _update_desklet_params (gpointer data)
{
	if (s_DeskletToUpdate != NULL)
	{
		if (s_pMainGuiBackend && s_pMainGuiBackend->update_desklet_params)
			s_pMainGuiBackend->update_desklet_params (s_DeskletToUpdate);
		s_DeskletToUpdate = NULL;
	}
	s_iSidUpdateDesklet = 0;
	return FALSE;
}
static gboolean _on_stop_desklet (gpointer pUserData, CairoDesklet *pDesklet)
{
	if (s_DeskletToUpdate == pDesklet)  // the desklet we were about to update has been destroyed, cancel.
	{
		if (s_iSidUpdateDesklet != 0)
		{
			g_source_remove (s_iSidUpdateDesklet);
			s_iSidUpdateDesklet = 0;
		}
		s_DeskletToUpdate = NULL;
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}
void cairo_dock_gui_trigger_update_desklet_params (CairoDesklet *pDesklet)
{
	g_return_if_fail (pDesklet != NULL);
	if (s_DeskletToUpdate != pDesklet)  // new desklet to update, let's forget the previous one.
	{
		if (s_iSidUpdateDesklet != 0)
		{
			g_source_remove (s_iSidUpdateDesklet);
			s_iSidUpdateDesklet = 0;
		}
		s_DeskletToUpdate = NULL;
	}
	if (s_iSidUpdateDesklet == 0)  // no update scheduled yet, let's schedule it.
	{
		s_iSidUpdateDesklet = g_idle_add_full (G_PRIORITY_LOW,
			(GSourceFunc) _update_desklet_params,
			NULL,
			NULL);
		s_DeskletToUpdate = pDesklet;
		cairo_dock_register_notification_on_object (pDesklet,
			NOTIFICATION_STOP_DESKLET,
			(CairoDockNotificationFunc) _on_stop_desklet,
			CAIRO_DOCK_RUN_AFTER, NULL);
	}
}


static guint s_iSidUpdateVisiDesklet = 0;
static gboolean _update_desklet_visibility_params (gpointer data)
{
	if (s_DeskletToUpdate != NULL)
	{
		if (s_pMainGuiBackend && s_pMainGuiBackend->update_desklet_visibility_params)
			s_pMainGuiBackend->update_desklet_visibility_params (s_DeskletToUpdate);
		s_DeskletToUpdate = NULL;
	}
	s_iSidUpdateVisiDesklet = 0;
	return FALSE;
}
void cairo_dock_gui_trigger_update_desklet_visibility (CairoDesklet *pDesklet)
{
	g_return_if_fail (pDesklet != NULL);
	if (s_DeskletToUpdate != pDesklet)  // new desklet to update, let's forget the previous one.
	{
		if (s_iSidUpdateVisiDesklet != 0)
		{
			g_source_remove (s_iSidUpdateVisiDesklet);
			s_iSidUpdateVisiDesklet = 0;
		}
		s_DeskletToUpdate = NULL;
	}
	if (s_iSidUpdateVisiDesklet == 0)  // no update scheduled yet, let's schedule it.
	{
		s_iSidUpdateVisiDesklet = g_idle_add_full (G_PRIORITY_LOW,
			(GSourceFunc) _update_desklet_visibility_params,
			NULL,
			NULL);
		s_DeskletToUpdate = pDesklet;
		cairo_dock_register_notification_on_object (pDesklet,
			NOTIFICATION_STOP_DESKLET,
			(CairoDockNotificationFunc) _on_stop_desklet,
			CAIRO_DOCK_RUN_AFTER, NULL);
	}
}


static guint s_iSidReloadItems = 0;
static gboolean _reload_items (gpointer data)
{
	if (s_pItemsGuiBackend && s_pItemsGuiBackend->reload_items)
		s_pItemsGuiBackend->reload_items ();
	s_iSidReloadItems = 0;
	return FALSE;
}
void cairo_dock_gui_trigger_reload_items (void)
{
	if (s_iSidReloadItems == 0)
	{
		s_iSidReloadItems = g_idle_add_full (G_PRIORITY_LOW,
			(GSourceFunc) _reload_items,
			NULL,
			NULL);
	}
}


static guint s_iSidUpdateModuleState = 0;
static gboolean _update_module_state (gchar *cModuleName)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->update_module_state)
	{
		CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
		if (pModule != NULL)
		{
			s_pMainGuiBackend->update_module_state (cModuleName, pModule->pInstancesList != NULL);
		}
	}
	s_iSidUpdateModuleState = 0;
	return FALSE;
}
void cairo_dock_gui_trigger_update_module_state (const gchar *cModuleName)
{
	if (s_iSidUpdateModuleState == 0)
	{
		s_iSidUpdateModuleState = g_idle_add_full (G_PRIORITY_LOW,
			(GSourceFunc) _update_module_state,
			g_strdup (cModuleName),
			g_free);
	}
}


static guint s_iSidReloadModulesList = 0;
static gboolean _update_modules_list (gpointer data)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->update_module_state)
		s_pMainGuiBackend->update_modules_list ();
	
	s_iSidReloadModulesList = 0;
	return FALSE;
}
void cairo_dock_gui_trigger_update_modules_list (void)
{
	if (s_iSidReloadModulesList == 0)
	{
		s_iSidReloadModulesList = g_idle_add_full (G_PRIORITY_LOW,
			(GSourceFunc) _update_modules_list,
			NULL,
			NULL);
	}
}


void cairo_dock_gui_trigger_update_module_container (CairoDockModuleInstance *pInstance, gboolean bIsDetached)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->update_module_instance_container)
		s_pMainGuiBackend->update_module_instance_container (pInstance, bIsDetached);  // on n'a encore pas de moyen simple d'etre prevenu de la destruction de l'instance, donc on effectue l'action tout de suite. -> si, regarder l'icone ...
}



void cairo_dock_register_config_gui_backend (CairoDockMainGuiBackend *pBackend)
{
	g_free (s_pMainGuiBackend);
	s_pMainGuiBackend = pBackend;
}

void cairo_dock_register_items_gui_backend (CairoDockItemsGuiBackend *pBackend)
{
	g_free (s_pItemsGuiBackend);
	s_pItemsGuiBackend = pBackend;
}


GtkWidget *cairo_dock_show_main_gui (void)
{
	GtkWidget *pWindow = NULL;
	if (s_pMainGuiBackend && s_pMainGuiBackend->show_main_gui)
		pWindow = s_pMainGuiBackend->show_main_gui ();
	if (pWindow && g_pMainDock != NULL)  // evitons d'empieter sur le main dock.
	{
		if (g_pMainDock->container.bIsHorizontal)
		{
			if (g_pMainDock->container.bDirectionUp)
				gtk_window_move (GTK_WINDOW (pWindow), 0, 0);
			else
				gtk_window_move (GTK_WINDOW (pWindow), 0, g_pMainDock->iMinDockHeight+10);
		}
		else
		{
			if (g_pMainDock->container.bDirectionUp)
				gtk_window_move (GTK_WINDOW (pWindow), 0, 0);
			else
				gtk_window_move (GTK_WINDOW (pWindow), g_pMainDock->iMinDockHeight+10, 0);
		}
	}
	
	return pWindow;
}

/*void cairo_dock_show_module_instance_gui (CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->show_module_instance_gui)
		s_pMainGuiBackend->show_module_instance_gui (pModuleInstance, iShowPage);
}*/

void cairo_dock_show_module_gui (const gchar *cModuleName)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->show_module_gui)
		s_pMainGuiBackend->show_module_gui (cModuleName);
}

void cairo_dock_close_gui (void)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->close_gui)
		s_pMainGuiBackend->close_gui ();
}

void cairo_dock_show_items_gui (Icon *pIcon, CairoContainer *pContainer, CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	//g_print ("%s (%x)\n", __func__, pIcon);
	if (s_pItemsGuiBackend && s_pItemsGuiBackend->show_gui)
		s_pItemsGuiBackend->show_gui (pIcon, pContainer, pModuleInstance, iShowPage);
}

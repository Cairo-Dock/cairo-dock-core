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

#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-gui-advanced.h"
#include "cairo-dock-gui-simple.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-gui-simple.h"
#include "cairo-dock-gui-backend.h"

extern gchar *g_cCairoDockDataDir;
extern CairoDock *g_pMainDock;

static CairoDockMainGuiBackend *s_pMainGuiBackend = NULL;
static int s_iCurrentMode = 0;

void cairo_dock_load_user_gui_backend (int iMode)  // 0 = simple
{
	if (iMode == 1)
	{
		cairo_dock_register_advanced_gui_backend ();
	}
	else
	{
		cairo_dock_register_simple_gui_backend ();
	}
	s_iCurrentMode = iMode;
}

int cairo_dock_gui_backend_get_mode ()
{
	return s_iCurrentMode;
}

static void on_click_switch_mode (G_GNUC_UNUSED GtkButton *button, G_GNUC_UNUSED gpointer data)
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



void cairo_dock_gui_update_desklet_params (CairoDesklet *pDesklet)
{
	g_return_if_fail (pDesklet != NULL);
	if (s_pMainGuiBackend && s_pMainGuiBackend->update_desklet_params)
		s_pMainGuiBackend->update_desklet_params (pDesklet);
}


void cairo_dock_gui_update_desklet_visibility (CairoDesklet *pDesklet)
{
	g_return_if_fail (pDesklet != NULL);
	if (s_pMainGuiBackend && s_pMainGuiBackend->update_desklet_visibility_params)
		s_pMainGuiBackend->update_desklet_visibility_params (pDesklet);
}


static guint s_iSidReloadItems = 0;
static gboolean _reload_items (G_GNUC_UNUSED gpointer data)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->reload_items)
		s_pMainGuiBackend->reload_items ();
	
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
static gboolean _update_modules_list (G_GNUC_UNUSED gpointer data)
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


static guint s_iSidReloadShortkeys = 0;
static gboolean _update_shortkeys (G_GNUC_UNUSED gpointer data)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->update_shortkeys)
		s_pMainGuiBackend->update_shortkeys ();
	
	s_iSidReloadShortkeys = 0;
	return FALSE;
}
void cairo_dock_gui_trigger_reload_shortkeys (void)
{
	if (s_iSidReloadShortkeys == 0)
	{
		s_iSidReloadShortkeys = g_idle_add_full (G_PRIORITY_LOW,
			(GSourceFunc) _update_shortkeys,
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

static void _display_window (GtkWidget *pWindow)
{
	// place it on the current desktop, and avoid overlapping the main dock
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
	
	// take focus
	gtk_window_present (GTK_WINDOW (pWindow));
}

GtkWidget * cairo_dock_show_main_gui (void)
{
	// create the window
	GtkWidget *pWindow = NULL;
	if (s_pMainGuiBackend && s_pMainGuiBackend->show_main_gui)
		pWindow = s_pMainGuiBackend->show_main_gui ();
	
	_display_window (pWindow);
	
	return pWindow;
}

void cairo_dock_show_module_gui (const gchar *cModuleName)
{
	GtkWidget *pWindow = NULL;
	if (s_pMainGuiBackend && s_pMainGuiBackend->show_module_gui)
		pWindow = s_pMainGuiBackend->show_module_gui (cModuleName);
		
	_display_window (pWindow);
}

void cairo_dock_close_gui (void)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->close_gui)
		s_pMainGuiBackend->close_gui ();
}

void cairo_dock_show_items_gui (Icon *pIcon, CairoContainer *pContainer, CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	GtkWidget *pWindow = NULL;
	if (s_pMainGuiBackend && s_pMainGuiBackend->show_gui)
		pWindow = s_pMainGuiBackend->show_gui (pIcon, pContainer, pModuleInstance, iShowPage);
		
	_display_window (pWindow);
}

void cairo_dock_reload_gui (void)
{
	if (s_pMainGuiBackend && s_pMainGuiBackend->reload)
		s_pMainGuiBackend->reload ();
}

void cairo_dock_show_themes (void)
{
	GtkWidget *pWindow = NULL;
	if (s_pMainGuiBackend && s_pMainGuiBackend->show_themes)
		pWindow = s_pMainGuiBackend->show_themes ();
		
	_display_window (pWindow);
}

void cairo_dock_show_addons (void)
{
	GtkWidget *pWindow = NULL;
	if (s_pMainGuiBackend && s_pMainGuiBackend->show_addons)
		pWindow = s_pMainGuiBackend->show_addons ();
		
	_display_window (pWindow);
}

gboolean cairo_dock_can_manage_themes (void)
{
	return (s_pMainGuiBackend && s_pMainGuiBackend->show_themes);
}

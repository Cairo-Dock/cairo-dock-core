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

#ifndef __CAIRO_DOCK_WIDGET_ITEMS__
#define  __CAIRO_DOCK_WIDGET_ITEMS__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
#include "cairo-dock-widget.h"
G_BEGIN_DECLS


typedef struct _ItemsWidget ItemsWidget;

struct _ItemsWidget {
	CDWidget widget;
	GtkWidget *pTreeView;
	GtkWidget *pCurrentLauncherWidget;
	Icon *pCurrentIcon;
	GldiContainer *pCurrentContainer;
	GldiModuleInstance *pCurrentModuleInstance;
	gchar *cPrevPath;
	GtkWindow *pMainWindow;  // main window, needed to build other widgets (e.g. to create a file selector and attach it to the main window)
};

#define ITEMS_WIDGET(w) ((ItemsWidget*)(w))

#define IS_ITEMS_WIDGET(w) (w && CD_WIDGET(w)->iType == WIDGET_ITEMS)


ItemsWidget *cairo_dock_items_widget_new (GtkWindow *pMainWindow);


void cairo_dock_items_widget_select_item (ItemsWidget *pItemsWidget, Icon *pIcon, GldiContainer *pContainer, GldiModuleInstance *pModuleInstance, int iNotebookPage);


void cairo_dock_items_widget_update_desklet_params (ItemsWidget *pItemsWidget, CairoDesklet *pDesklet);


void cairo_dock_items_widget_update_desklet_visibility_params (ItemsWidget *pItemsWidget, CairoDesklet *pDesklet);


void cairo_dock_items_widget_update_module_instance_container (ItemsWidget *pItemsWidget, GldiModuleInstance *pInstance, gboolean bDetached);


void cairo_dock_items_widget_reload_current_widget (ItemsWidget *pItemsWidget, GldiModuleInstance *pInstance, int iShowPage);


G_END_DECLS
#endif

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


#ifndef __CAIRO_DOCK_WIDGET_PLUGINS__
#define  __CAIRO_DOCK_WIDGET_PLUGINS__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
#include "cairo-dock-widget.h"
G_BEGIN_DECLS


typedef struct _PluginsWidget PluginsWidget;

struct _PluginsWidget {
	CDWidget widget;
	GtkWidget *pTreeView;
};

#define PLUGINS_WIDGET(w) ((PluginsWidget*)(w))

#define IS_PLUGINS_WIDGET(w) (w && CD_WIDGET(w)->iType == WIDGET_PLUGINS)


PluginsWidget *cairo_dock_plugins_widget_new (void);


void cairo_dock_widget_plugins_update_module_state (PluginsWidget *pPluginsWidget, const gchar *cModuleName, gboolean bActive);


G_END_DECLS
#endif

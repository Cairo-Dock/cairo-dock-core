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


#ifndef __CAIRO_DOCK_WIDGET_CONFIG_GROUP__
#define  __CAIRO_DOCK_WIDGET_CONFIG_GROUP__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
#include "cairo-dock-widget.h"
G_BEGIN_DECLS


typedef struct _ConfigGroupWidget ConfigGroupWidget;

struct _ConfigGroupWidget {
	CDWidget widget;
	gchar *cGroupName;
	GList *pManagers;
	GSList *pExtraWidgets;
};
#define CONFIG_GROUP_WIDGET(w) ((ConfigGroupWidget*)(w))

#define IS_CONFIG_GROUP_WIDGET(w) (w && CD_WIDGET(w)->iType == WIDGET_CONFIG_GROUP)


ConfigGroupWidget *cairo_dock_config_group_widget_new (const gchar *cGroupName, GList *pManagers, const gchar *cTitle, const gchar *cIcon);


G_END_DECLS
#endif

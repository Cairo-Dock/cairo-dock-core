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

#ifndef __CAIRO_DOCK_WIDGET__
#define  __CAIRO_DOCK_WIDGET__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS


typedef struct _CDWidget CDWidget;

typedef enum {
	WIDGET_UNKNOWN,
	WIDGET_CONFIG_GROUP,
	WIDGET_CONFIG,
	WIDGET_ITEMS,
	WIDGET_MODULE,
	WIDGET_PLUGINS,
	WIDGET_SHORTKEYS,
	WIDGET_THEMES,
	WIDGET_NB_TYPES,
} CDWidgetType;

struct _CDWidget {
	CDWidgetType iType;
	GtkWidget *pWidget;
	void (*apply) (CDWidget *pWidget);
	void (*reset) (CDWidget *pWidget);
	gboolean (*represents_module_instance) (CairoDockModuleInstance *pInstance);
	CairoDockGroupKeyWidget* (*get_widget_from_name) (CairoDockModuleInstance *pInstance, const gchar *cGroupName, const gchar *cKeyName);
	GSList *pWidgetList;
	GPtrArray *pDataGarbage;
};

#define CD_WIDGET(w) ((CDWidget*)(w))


void cairo_dock_widget_apply (CDWidget *pCdWidget);


gboolean cairo_dock_widget_can_apply (CDWidget *pCdWidget);


void cairo_dock_widget_free (CDWidget *pCdWidget);


void cairo_dock_widget_destroy_widget (CDWidget *pCdWidget);


G_END_DECLS
#endif

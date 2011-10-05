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


#ifndef __CAIRO_DOCK_GUI_COMMONS__
#define  __CAIRO_DOCK_GUI_COMMONS__

#include <glib.h>
#include <gtk/gtk.h>
G_BEGIN_DECLS


void cairo_dock_make_tree_view_for_delete_themes (GtkWidget *pWindow);


gboolean cairo_dock_save_current_theme (GKeyFile* pKeyFile);


gboolean cairo_dock_delete_user_themes (GKeyFile* pKeyFile);


gboolean cairo_dock_load_theme (GKeyFile* pKeyFile, GFunc pCallback, GtkWidget *pMainWindow);


gboolean cairo_dock_add_module_to_modele (gchar *cModuleName, CairoDockModule *pModule, GtkListStore *pModele);

GtkWidget *cairo_dock_build_modules_treeview (void);


void cairo_dock_update_desklet_widgets (CairoDesklet *pDesklet, GSList *pWidgetList);

void cairo_dock_update_desklet_visibility_widgets (CairoDesklet *pDesklet, GSList *pWidgetList);

void cairo_dock_update_is_detached_widget (gboolean bIsDetached, GSList *pWidgetList);


gchar *cairo_dock_get_third_party_applets_link (void);

G_END_DECLS
#endif

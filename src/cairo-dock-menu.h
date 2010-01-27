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


#ifndef __CAIRO_DOCK_MENU__
#define  __CAIRO_DOCK_MENU__

#include <gtk/gtk.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-menu.h This class manages the menu of Cairo-Dock.
* It is called on a left click on a Container, builds a main menu, and notifies everybody about it, so that the menu is completed.
*/


gboolean cairo_dock_notification_build_container_menu (gpointer *pUserData, CairoContainer *pContainer, GtkWidget *menu);

gboolean cairo_dock_notification_build_icon_menu (gpointer *pUserData, Icon *icon, CairoContainer *pContainer, GtkWidget *menu);


/** Pop-up a menu on a container. In the case of a dock, it prevents this one from shrinking down.
*@param menu the menu.
*@param pContainer the container that was clicked.
*/
void cairo_dock_popup_menu_on_container (GtkWidget *menu, CairoContainer *pContainer);

/** Add an entry to a given menu.
*@param cLabel label of the entry
*@param gtkStock a GTK stock or a path to an image
*@param pFunction callback
*@param pMenu the menu to insert the entry in
*@param pData data to feed the callback with
*/
GtkWidget *cairo_dock_add_in_menu_with_stock_and_data (const gchar *cLabel, const gchar *gtkStock, GFunc pFunction, GtkWidget *pMenu, gpointer pData);


/** Build the main menu of a Container.
*@param icon the icon that was left-clicked, or NULL if none.
*@param pContainer the container that was left-clicked.
*/
GtkWidget *cairo_dock_build_menu (Icon *icon, CairoContainer *pContainer);


G_END_DECLS
#endif

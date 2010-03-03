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


gboolean cairo_dock_notification_build_container_menu (gpointer *pUserData, CairoContainer *pContainer, GtkWidget *menu);

gboolean cairo_dock_notification_build_icon_menu (gpointer *pUserData, Icon *icon, CairoContainer *pContainer, GtkWidget *menu);


G_END_DECLS
#endif

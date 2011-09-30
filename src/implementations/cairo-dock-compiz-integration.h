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


#ifndef __CAIRO_DOCK_COMPIZ_INTEGRATION__
#define  __CAIRO_DOCK_COMPIZ_INTEGRATION__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-compiz-integration.h This class implements the integration of Compiz inside Cairo-Dock.
*/

#define CD_COMPIZ_BUS "org.freedesktop.compiz"
#define CD_COMPIZ_OBJECT "/org/freedesktop/compiz"
#define CD_COMPIZ_INTERFACE "org.freedesktop.compiz"
///#define OLD_WIDGET_LAYER 1  // seems better to rely on the _COMPIZ_WIDGET atom than on a rule.

void cd_init_compiz_backend (void);


gboolean cd_is_the_new_compiz (void);  // this function is exported for the Help applet.


G_END_DECLS
#endif

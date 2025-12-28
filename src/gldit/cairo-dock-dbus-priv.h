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


#ifndef __CAIRO_DOCK_DBUS_PRIV__
#define __CAIRO_DOCK_DBUS_PRIV__

#include "cairo-dock-dbus.h"
G_BEGIN_DECLS

/** Try to own our DBus name (org.cairodock.CairoDock). This is a synchronous
 * function that will block until the name is acquired or rejected. It should
 * be only called during initialization as it will iterate the default context.
 *@return TRUE if the name has been acquired 
 */
gboolean cairo_dock_dbus_own_name (void);

G_END_DECLS
#endif


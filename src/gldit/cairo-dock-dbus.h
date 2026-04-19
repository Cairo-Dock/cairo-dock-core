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


#ifndef __CAIRO_DOCK_DBUS__
#define  __CAIRO_DOCK_DBUS__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/**
*@file cairo-dock-dbus.h This class defines numerous convenient functions to use DBus inside Cairo-Dock.
* DBus is used to communicate and interact with other running applications.
*/ 


/** Say if the bus is available or not.
*@return TRUE if the connection to the bus has been established.
*/
gboolean cairo_dock_dbus_is_enabled (void);

/** Get a connection to the session bus (if connected).
*@return main DBusConnection to the session bus or NULL if unavailable
*/
GDBusConnection *cairo_dock_dbus_get_session_bus (void);

/** Get the DBus well-known name under which Cairo-Dock is registered
* on the session bus. By default, it is "org.CairoDock.cairodock".
*@return the DBus name registered or NULL if we do not own a DBus name
*/
const gchar *cairo_dock_dbus_get_owned_name (void);

G_END_DECLS
#endif


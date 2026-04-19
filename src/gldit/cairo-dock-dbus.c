/**
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

#include <string.h>
#include <glib.h>

#include "cairo-dock-log.h"
#include "cairo-dock-dbus-priv.h"


static GDBusConnection *s_pMainConnection = NULL;
static const gchar *s_cBusName = "org.cairodock.CairoDock";
static gboolean s_bNameOwned = FALSE;


gboolean cairo_dock_dbus_is_enabled (void)
{
	return (s_pMainConnection != NULL);
}

GDBusConnection *cairo_dock_dbus_get_session_bus (void)
{
	return s_pMainConnection;
}

const gchar *cairo_dock_dbus_get_owned_name (void)
{
	return s_bNameOwned ? s_cBusName : NULL;
}

static void _on_bus_acquired (GDBusConnection *pConn, G_GNUC_UNUSED const gchar* cName, G_GNUC_UNUSED gpointer ptr)
{
	s_pMainConnection = pConn;
}


gboolean s_bNameAcquiredOrLost = FALSE;

static void _on_name_acquired (G_GNUC_UNUSED GDBusConnection* pConn, G_GNUC_UNUSED const gchar* cName, G_GNUC_UNUSED gpointer data)
{
	s_bNameOwned = TRUE;
	s_bNameAcquiredOrLost = TRUE;
}

static void _on_name_lost (G_GNUC_UNUSED GDBusConnection* pConn, G_GNUC_UNUSED const gchar* cName, G_GNUC_UNUSED gpointer data)
{
	s_bNameOwned = FALSE;
	s_bNameAcquiredOrLost = TRUE;
}

gboolean cairo_dock_dbus_own_name (void)
{
	//\____________ Register the service name (the service name is registerd once by the first gldi instance).
	g_bus_own_name (G_BUS_TYPE_SESSION, s_cBusName, G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE, _on_bus_acquired, // bus acquired handler
		_on_name_acquired, _on_name_lost, NULL, NULL);
	
	// Wait until we have acquired the bus. This is necessary as we want to know whether we succeeded.
	GMainContext *pContext = g_main_context_default ();
	do g_main_context_iteration (pContext, TRUE);
	while (!s_bNameAcquiredOrLost);
	
	return s_bNameOwned;
}


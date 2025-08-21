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
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
G_BEGIN_DECLS

/**
*@file cairo-dock-dbus.h This class defines numerous convenient functions to use DBus inside Cairo-Dock.
* DBus is used to communicate and interact with other running applications.
*/ 


typedef void (*CairoDockDbusNameOwnerChangedFunc) (const gchar *cName, gboolean bOwned, gpointer data);

/** Get the connection to the 'session' Bus.
*@return the connection to the bus.
*/
DBusGConnection *cairo_dock_get_session_connection (void);
#define cairo_dock_get_dbus_connection cairo_dock_get_session_connection

DBusGProxy *cairo_dock_get_main_proxy (void);

DBusGProxy *cairo_dock_get_main_system_proxy (void);

/**Register a new service on the session bus.
*@param cServiceName name of the service.
*@return TRUE in case of success, false otherwise.
*/
gboolean cairo_dock_register_service_name (const gchar *cServiceName);

/** Say if the bus is available or not.
*@return TRUE if the connection to the bus has been established.
*/
gboolean cairo_dock_dbus_is_enabled (void);

void cairo_dock_watch_dbus_name_owner (const char *cName, CairoDockDbusNameOwnerChangedFunc pCallback, gpointer data);

void cairo_dock_stop_watching_dbus_name_owner (const char *cName, CairoDockDbusNameOwnerChangedFunc pCallback);


/** Create a new proxy for the 'session' connection.
*@param name a name on the bus.
*@param path the path.
*@param interface name of the interface.
*@return the newly created proxy. Use g_object_unref when your done with it.
*/
DBusGProxy *cairo_dock_create_new_session_proxy (const char *name, const char *path, const char *interface);
#define cairo_dock_create_new_dbus_proxy cairo_dock_create_new_session_proxy

/**Create a new proxy for the 'system' connection.
*@param name a name on the bus.
*@param path the path.
*@param interface name of the interface.
*@return the newly created proxy. Use g_object_unref when your done with it.
*/
DBusGProxy *cairo_dock_create_new_system_proxy (const char *name, const char *path, const char *interface);

typedef void (*CairoDockOnAppliPresentOnDbus) (gboolean bPresent, gpointer data);

DBusGProxyCall *cairo_dock_dbus_detect_application_async (const gchar *cName, CairoDockOnAppliPresentOnDbus pCallback, gpointer user_data);

DBusGProxyCall *cairo_dock_dbus_detect_system_application_async (const gchar *cName, CairoDockOnAppliPresentOnDbus pCallback, gpointer user_data);

/** Detect if an application is currently running on Session bus.
*@param cName name of the application.
*@return TRUE if the application is running and has a service on the bus.
*/
gboolean cairo_dock_dbus_detect_application (const gchar *cName);

/** Detect if an application is currently running on System bus.
*@param cName name of the application.
*@return TRUE if the application is running and has a service on the bus.
*/
gboolean cairo_dock_dbus_detect_system_application (const gchar *cName);


gchar **cairo_dock_dbus_get_services (void);

/** Get the value of a 'boolean' parameter on the bus.
*@param pDbusProxy proxy to the connection.
*@param cAccessor name of the accessor.
*@return the value of the parameter.
*/
gboolean cairo_dock_dbus_get_boolean (DBusGProxy *pDbusProxy, const gchar *cAccessor);

/** Get the value of an 'unsigned integer' parameter non signe on the bus.
*@param pDbusProxy proxy to the connection.
*@param cAccessor name of the accessor.
*@return the value of the parameter.
*/
guint cairo_dock_dbus_get_uinteger (DBusGProxy *pDbusProxy, const gchar *cAccessor);

/** Get the value of a 'integer' parameter on the bus.
*@param pDbusProxy proxy to the connection.
*@param cAccessor name of the accessor.
*@return the value of the parameter.
*/
int cairo_dock_dbus_get_integer (DBusGProxy *pDbusProxy, const gchar *cAccessor);

/** Get the value of a 'string' parameter on the bus.
*@param pDbusProxy proxy to the connection.
*@param cAccessor name of the accessor.
*@return the value of the parameter, to be freeed with g_free.
*/
gchar *cairo_dock_dbus_get_string (DBusGProxy *pDbusProxy, const gchar *cAccessor);

/** Get the value of a 'string list' parameter on the bus.
*@param pDbusProxy proxy to the connection.
*@param cAccessor name of the accessor.
*@return the value of the parameter, to be freeed with g_strfreev.
*/
gchar **cairo_dock_dbus_get_string_list (DBusGProxy *pDbusProxy, const gchar *cAccessor);

GPtrArray *cairo_dock_dbus_get_array (DBusGProxy *pDbusProxy, const gchar *cAccessor);

/** Get the value of an 'unsigned char' parameter on the bus.
*@param pDbusProxy proxy to the connection.
*@param cAccessor name of the accessor.
*@return the value of the parameter.
*/
guchar *cairo_dock_dbus_get_uchar (DBusGProxy *pDbusProxy, const gchar *cAccessor);

gdouble cairo_dock_dbus_get_double (DBusGProxy *pDbusProxy, const gchar *cAccessor);

/** Call a command on the bus.
*@param pDbusProxy proxy to the connection.
*@param cCommand name of the commande.
*/
void cairo_dock_dbus_call (DBusGProxy *pDbusProxy, const gchar *cCommand);


void cairo_dock_dbus_get_properties (DBusGProxy *pDbusProxy, const gchar *cCommand, const gchar *cInterface, const gchar *cProperty, GValue *vProperties);  /// deprecated...

#define cairo_dock_dbus_get_property_in_value(pDbusProxy, cInterface, cProperty, pProperties) cairo_dock_dbus_get_property_in_value_with_timeout(pDbusProxy, cInterface, cProperty, pProperties, -1)
void cairo_dock_dbus_get_property_in_value_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, GValue *pProperty, gint iTimeOut);

#define cairo_dock_dbus_get_property_as_boolean(pDbusProxy, cInterface, cProperty) cairo_dock_dbus_get_property_as_boolean_with_timeout(pDbusProxy, cInterface, cProperty, -1)
gboolean cairo_dock_dbus_get_property_as_boolean_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut);

#define cairo_dock_dbus_get_property_as_int(pDbusProxy, cInterface, cProperty) cairo_dock_dbus_get_property_as_int_with_timeout(pDbusProxy, cInterface, cProperty, -1)
gint cairo_dock_dbus_get_property_as_int_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut);

#define cairo_dock_dbus_get_property_as_uint(pDbusProxy, cInterface, cProperty) cairo_dock_dbus_get_property_as_uint_with_timeout(pDbusProxy, cInterface, cProperty, -1)
guint cairo_dock_dbus_get_property_as_uint_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut);

#define cairo_dock_dbus_get_property_as_uchar(pDbusProxy, cInterface, cProperty) cairo_dock_dbus_get_property_as_uchar_with_timeout(pDbusProxy, cInterface, cProperty, -1)
guchar cairo_dock_dbus_get_property_as_uchar_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut);

#define cairo_dock_dbus_get_property_as_double(pDbusProxy, cInterface, cProperty) cairo_dock_dbus_get_property_as_double_with_timeout(pDbusProxy, cInterface, cProperty, -1)
gdouble cairo_dock_dbus_get_property_as_double_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut);

#define cairo_dock_dbus_get_property_as_string(pDbusProxy, cInterface, cProperty) cairo_dock_dbus_get_property_as_string_with_timeout(pDbusProxy, cInterface, cProperty, -1)
gchar *cairo_dock_dbus_get_property_as_string_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut);

#define cairo_dock_dbus_get_property_as_object_path(pDbusProxy, cInterface, cProperty) cairo_dock_dbus_get_property_as_object_path_with_timeout(pDbusProxy, cInterface, cProperty, -1)
gchar *cairo_dock_dbus_get_property_as_object_path_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut);

#define cairo_dock_dbus_get_property_as_boxed(pDbusProxy, cInterface, cProperty) cairo_dock_dbus_get_property_as_boxed_with_timeout(pDbusProxy, cInterface, cProperty, -1)
gpointer cairo_dock_dbus_get_property_as_boxed_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut);

#define cairo_dock_dbus_get_property_as_string_list(pDbusProxy, cInterface, cProperty) cairo_dock_dbus_get_property_as_string_list_with_timeout(pDbusProxy, cInterface, cProperty, -1)
gchar **cairo_dock_dbus_get_property_as_string_list_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut);


#define cairo_dock_dbus_get_all_properties(pDbusProxy, cInterface) cairo_dock_dbus_get_all_properties_with_timeout(pDbusProxy, cInterface, -1)
GHashTable *cairo_dock_dbus_get_all_properties_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, gint iTimeOut);


#define cairo_dock_dbus_set_property(pDbusProxy, cInterface, cProperty, pProperty) cairo_dock_dbus_set_property_with_timeout(pDbusProxy, cInterface, cProperty, pProperty, -1)
void cairo_dock_dbus_set_property_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, GValue *pProperty, gint iTimeOut);

#define cairo_dock_dbus_set_boolean_property(pDbusProxy, cInterface, cProperty, bValue) cairo_dock_dbus_set_boolean_property_with_timeout(pDbusProxy, cInterface, cProperty, bValue, -1)
void cairo_dock_dbus_set_boolean_property_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gboolean bValue, gint iTimeOut);


G_END_DECLS
#endif

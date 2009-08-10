
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

/** Get the connection to the 'session' Bus.
*@return the connection to the bus.
*/
DBusGConnection *cairo_dock_get_session_connection (void);
#define cairo_dock_get_dbus_connection cairo_dock_get_session_connection

/**Register a new service on the bus.
*@param cServiceName name of the service.
*/
void cairo_dock_register_service_name (const gchar *cServiceName);

/** Say if the bus is available or not.
*@return TRUE if the connection to the bus has been established.
*/
gboolean cairo_dock_bdus_is_enabled (void);

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


void cairo_dock_dbus_get_properties (DBusGProxy *pDbusProxy, const gchar *cCommand, const gchar *cInterface, const gchar *cProperty, GValue *vProperties);


G_END_DECLS
#endif

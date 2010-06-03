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
#include "cairo-dock-dbus.h"

static DBusGConnection *s_pSessionConnexion = NULL;
static DBusGConnection *s_pSystemConnexion = NULL;
static DBusGProxy *s_pDBusSessionProxy = NULL;
static DBusGProxy *s_pDBusSystemProxy = NULL;


DBusGConnection *cairo_dock_get_session_connection (void)
{
	if (s_pSessionConnexion == NULL)
	{
		GError *erreur = NULL;
		s_pSessionConnexion = dbus_g_bus_get (DBUS_BUS_SESSION, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			s_pSessionConnexion = NULL;
		}
	}
	return s_pSessionConnexion;
}

DBusGConnection *cairo_dock_get_system_connection (void)
{
	if (s_pSystemConnexion == NULL)
	{
		GError *erreur = NULL;
		s_pSystemConnexion = dbus_g_bus_get (DBUS_BUS_SYSTEM, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			s_pSystemConnexion = NULL;
		}
	}
	return s_pSystemConnexion;
}

DBusGProxy *cairo_dock_get_main_proxy (void)
{
	if (s_pDBusSessionProxy == NULL)
	{
		s_pDBusSessionProxy = cairo_dock_create_new_session_proxy (DBUS_SERVICE_DBUS,
			DBUS_PATH_DBUS,
			DBUS_INTERFACE_DBUS);
	}
	return s_pDBusSessionProxy;
}

DBusGProxy *cairo_dock_get_main_system_proxy (void)
{
	if (s_pDBusSystemProxy == NULL)
	{
		s_pDBusSystemProxy = cairo_dock_create_new_system_proxy (DBUS_SERVICE_DBUS,
			DBUS_PATH_DBUS,
			DBUS_INTERFACE_DBUS);
	} 
	return s_pDBusSystemProxy;
}

void cairo_dock_register_service_name (const gchar *cServiceName)
{
	DBusGProxy *pProxy = cairo_dock_get_main_proxy ();
	if (pProxy == NULL)
		return ;
	GError *erreur = NULL;
	int request_ret;
	org_freedesktop_DBus_request_name (pProxy, cServiceName, 0, &request_ret, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("Unable to register service: %s", erreur->message);
		g_error_free (erreur);
	}
}


gboolean cairo_dock_bdus_is_enabled (void)
{
	return (cairo_dock_get_session_connection () != NULL && cairo_dock_get_system_connection () != NULL);
}


DBusGProxy *cairo_dock_create_new_session_proxy (const char *name, const char *path, const char *interface)
{
	DBusGConnection *pConnection = cairo_dock_get_session_connection ();
	if (pConnection != NULL)
		return dbus_g_proxy_new_for_name (
			pConnection,
			name,
			path,
			interface);
	else
		return NULL;
}

DBusGProxy *cairo_dock_create_new_system_proxy (const char *name, const char *path, const char *interface)
{
	DBusGConnection *pConnection = cairo_dock_get_system_connection ();
	if (pConnection != NULL)
		return dbus_g_proxy_new_for_name (
			pConnection,
			name,
			path,
			interface);
	else
		return NULL;
}

static void _on_detect_application (DBusGProxy *proxy, DBusGProxyCall *call_id, gpointer *data)
{
	CairoDockOnAppliPresentOnDbus pCallback = data[0];
	gpointer user_data = data[1];
	gchar *cName = data[2];
	gchar **name_list = NULL;
	gboolean bSuccess = dbus_g_proxy_end_call (proxy,
		call_id,
		NULL,
		G_TYPE_STRV,
		&name_list,
		G_TYPE_INVALID);
	
	gboolean bPresent = FALSE;
	cd_message ("detection du service %s ...", cName);
	int i;
	for (i = 0; name_list[i] != NULL; i ++)
	{
		if (strcmp (name_list[i], cName) == 0)
		{
			bPresent = TRUE;
			break;
		}
	}
	g_strfreev (name_list);
	g_free (cName);
	data[2] = NULL;
	
	pCallback (bPresent, user_data);
}
static void _free_detect_application (gpointer *data)
{
	cd_debug ("free data\n");
	g_free (data[2]);
	g_free (data);
}
static inline DBusGProxyCall *_dbus_detect_application_async (const gchar *cName, DBusGProxy *pProxy, CairoDockOnAppliPresentOnDbus pCallback, gpointer user_data)
{
	g_return_val_if_fail (cName != NULL && pProxy != NULL, FALSE);
	gpointer *data = g_new0 (gpointer, 3);
	data[0] = pCallback;
	data[1] = user_data;
	data[2] = g_strdup (cName);
	DBusGProxyCall* pCall= dbus_g_proxy_begin_call (pProxy, "ListNames",
		(DBusGProxyCallNotify)_on_detect_application,
		data,
		(GDestroyNotify) _free_detect_application,
		G_TYPE_INVALID);
	return pCall;
}

DBusGProxyCall *cairo_dock_dbus_detect_application_async (const gchar *cName, CairoDockOnAppliPresentOnDbus pCallback, gpointer user_data)
{
	cd_message ("%s (%s)", __func__, cName);
	DBusGProxy *pProxy = cairo_dock_get_main_proxy ();
	return _dbus_detect_application_async (cName, pProxy, pCallback, user_data);
}

DBusGProxyCall *cairo_dock_dbus_detect_system_application_async (const gchar *cName, CairoDockOnAppliPresentOnDbus pCallback, gpointer user_data)
{
	cd_message ("%s (%s)", __func__, cName);
	DBusGProxy *pProxy = cairo_dock_get_main_system_proxy ();
	return _dbus_detect_application_async (cName, pProxy, pCallback, user_data);
}

static inline gboolean _dbus_detect_application (const gchar *cName, DBusGProxy *pProxy)
{
	g_return_val_if_fail (cName != NULL && pProxy != NULL, FALSE);
	
	gchar **name_list = NULL;
	gboolean bPresent = FALSE;
	
	if(dbus_g_proxy_call (pProxy, "ListNames", NULL,
		G_TYPE_INVALID,
		G_TYPE_STRV,
		&name_list,
		G_TYPE_INVALID))
	{
		cd_message ("detection du service %s ...", cName);
		int i;
		for (i = 0; name_list[i] != NULL; i ++)
		{
			if (strcmp (name_list[i], cName) == 0)
			{
				bPresent = TRUE;
				break;
			}
		}
	}
	g_strfreev (name_list);
	return bPresent;
}

gboolean cairo_dock_dbus_detect_application (const gchar *cName)
{
	cd_message ("%s (%s)", __func__, cName);
	DBusGProxy *pProxy = cairo_dock_get_main_proxy ();
	return _dbus_detect_application (cName, pProxy);
}

gboolean cairo_dock_dbus_detect_system_application (const gchar *cName)
{
	cd_message ("%s (%s)", __func__, cName);
	DBusGProxy *pProxy = cairo_dock_get_main_system_proxy ();
	return _dbus_detect_application (cName, pProxy);
}


gchar **cairo_dock_dbus_get_services (void)
{
	DBusGProxy *pProxy = cairo_dock_get_main_proxy ();
	gchar **name_list = NULL;
	if(dbus_g_proxy_call (pProxy, "ListNames", NULL,
		G_TYPE_INVALID,
		G_TYPE_STRV,
		&name_list,
		G_TYPE_INVALID))
		return name_list;
	else
		return NULL;
}



gboolean cairo_dock_dbus_get_boolean (DBusGProxy *pDbusProxy, const gchar *cAccessor)
{
	GError *erreur = NULL;
	gboolean bValue = FALSE;
	dbus_g_proxy_call (pDbusProxy, cAccessor, &erreur,
		G_TYPE_INVALID,
		G_TYPE_BOOLEAN, &bValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
	return bValue;
}

int cairo_dock_dbus_get_integer (DBusGProxy *pDbusProxy, const gchar *cAccessor)
{
	GError *erreur = NULL;
	int iValue = -1;
	dbus_g_proxy_call (pDbusProxy, cAccessor, &erreur,
		G_TYPE_INVALID,
		G_TYPE_INT, &iValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		iValue = -1;
	}
	return iValue;
}

guint cairo_dock_dbus_get_uinteger (DBusGProxy *pDbusProxy, const gchar *cAccessor)
{
	GError *erreur = NULL;
	guint iValue = -1;
	dbus_g_proxy_call (pDbusProxy, cAccessor, &erreur,
		G_TYPE_INVALID,
		G_TYPE_UINT, &iValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		iValue = -1;
	}
	return iValue;
}

gchar *cairo_dock_dbus_get_string (DBusGProxy *pDbusProxy, const gchar *cAccessor)
{
	GError *erreur = NULL;
	gchar *cValue = NULL;
	dbus_g_proxy_call (pDbusProxy, cAccessor, &erreur,
		G_TYPE_INVALID,
		G_TYPE_STRING, &cValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
	return cValue;
}

guchar *cairo_dock_dbus_get_uchar (DBusGProxy *pDbusProxy, const gchar *cAccessor)
{
	GError *erreur = NULL;
	guchar* uValue = NULL;
	
	dbus_g_proxy_call (pDbusProxy, cAccessor, &erreur,
		G_TYPE_INVALID,
		G_TYPE_UCHAR, &uValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
	
	return uValue;
}

gdouble cairo_dock_dbus_get_double (DBusGProxy *pDbusProxy, const gchar *cAccessor)
{
	GError *erreur = NULL;
	gdouble fValue = 0.;
	
	dbus_g_proxy_call (pDbusProxy, cAccessor, &erreur,
		G_TYPE_INVALID,
		G_TYPE_DOUBLE, &fValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
	
	return fValue;
}

gchar **cairo_dock_dbus_get_string_list (DBusGProxy *pDbusProxy, const gchar *cAccessor)
{
	GError *erreur = NULL;
	gchar **cValues = NULL;
	dbus_g_proxy_call (pDbusProxy, cAccessor, &erreur,
		G_TYPE_INVALID,
		G_TYPE_POINTER, &cValues,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
	return cValues;
}

GPtrArray *cairo_dock_dbus_get_array (DBusGProxy *pDbusProxy, const gchar *cAccessor)
{
	GError *erreur = NULL;
	GPtrArray *pArray = NULL;
	dbus_g_proxy_call (pDbusProxy, cAccessor, &erreur,
		G_TYPE_INVALID,
		dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH), &pArray,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
	return pArray;
}



void cairo_dock_dbus_call (DBusGProxy *pDbusProxy, const gchar *cCommand)
{
	dbus_g_proxy_call_no_reply (pDbusProxy, cCommand,
		G_TYPE_INVALID,
		G_TYPE_INVALID);
}


void cairo_dock_dbus_get_properties (DBusGProxy *pDbusProxy, const gchar *cCommand, const gchar *cInterface, const gchar *cProperty, GValue *vProperties)
{
	GError *erreur=NULL;
	
	dbus_g_proxy_call(pDbusProxy, cCommand, &erreur,
		G_TYPE_STRING, cInterface,
		G_TYPE_STRING, cProperty,
		G_TYPE_INVALID,
		G_TYPE_VALUE, vProperties,
		G_TYPE_INVALID);
	
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
}



void cairo_dock_dbus_get_property_in_value (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, GValue *pProperty)
{
	GError *erreur=NULL;
	
	dbus_g_proxy_call(pDbusProxy, "Get", &erreur,
		G_TYPE_STRING, cInterface,
		G_TYPE_STRING, cProperty,
		G_TYPE_INVALID,
		G_TYPE_VALUE, pProperty,
		G_TYPE_INVALID);
	
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
}

gboolean cairo_dock_dbus_get_property_as_boolean (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value (pDbusProxy, cInterface, cProperty, &v);
	if (G_VALUE_HOLDS_BOOLEAN (&v))
		return g_value_get_boolean (&v);
	else
		return FALSE;
}

gint cairo_dock_dbus_get_property_as_int (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value (pDbusProxy, cInterface, cProperty, &v);
	if (G_VALUE_HOLDS_INT (&v))
		return g_value_get_int (&v);
	else
		return 0;
}

guint cairo_dock_dbus_get_property_as_uint (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value (pDbusProxy, cInterface, cProperty, &v);
	if (G_VALUE_HOLDS_UINT (&v))
		return g_value_get_uint (&v);
	else
		return 0;
}

guchar cairo_dock_dbus_get_property_as_uchar (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value (pDbusProxy, cInterface, cProperty, &v);
	if (G_VALUE_HOLDS_UCHAR (&v))
		return g_value_get_uchar (&v);
	else
		return 0;
}

gdouble cairo_dock_dbus_get_property_as_double (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value (pDbusProxy, cInterface, cProperty, &v);
	if (G_VALUE_HOLDS_DOUBLE (&v))
		return g_value_get_double (&v);
	else
		return 0.;
}

gchar *cairo_dock_dbus_get_property_as_string (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value (pDbusProxy, cInterface, cProperty, &v);
	if (G_VALUE_HOLDS_STRING (&v))
	{
		gchar *s = (gchar*)g_value_get_string (&v);  // on recupere directement le contenu de la GValue. Comme on ne libere pas la GValue, la chaine est a liberer soi-meme quand on en n'a plus besoin.
		return s;
	}
	else
		return NULL;
}

gchar *cairo_dock_dbus_get_property_as_object_path (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value (pDbusProxy, cInterface, cProperty, &v);
	if (G_VALUE_HOLDS (&v, DBUS_TYPE_G_OBJECT_PATH))
	{
		gchar *s = (gchar*)g_value_get_string (&v);  // meme remarque.
		return s;
	}
	else
		return NULL;
}

gpointer cairo_dock_dbus_get_property_as_boxed (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value (pDbusProxy, cInterface, cProperty, &v);
	if (G_VALUE_HOLDS_BOXED (&v))
	{
		gpointer p = g_value_get_boxed (&v);  // meme remarque.
		return p;
	}
	else
		return NULL;
}

gchar **cairo_dock_dbus_get_property_as_string_list (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value (pDbusProxy, cInterface, cProperty, &v);
	if (G_VALUE_HOLDS_BOXED(&v))
	{
		gchar **s = (gchar**)g_value_get_boxed (&v);  // meme remarque.
		return s;
	}
	else
		return NULL;
}

GHashTable *cairo_dock_dbus_get_all_properties (DBusGProxy *pDbusProxy, const gchar *cInterface)
{
	GError *erreur=NULL;
	GHashTable *hProperties = NULL;
	
	dbus_g_proxy_call(pDbusProxy, "GetAll", &erreur,
		G_TYPE_STRING, cInterface,
		G_TYPE_INVALID,
		(dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE)), &hProperties,
		G_TYPE_INVALID);
	
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return NULL;
	}
	else
	{
		return hProperties;
	}
}


void cairo_dock_dbus_set_property (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, GValue *pProperty)
{
	GError *erreur=NULL;
	
	dbus_g_proxy_call(pDbusProxy, "Set", &erreur,
		G_TYPE_STRING, cInterface,
		G_TYPE_STRING, cProperty,
		G_TYPE_VALUE, pProperty,
		G_TYPE_INVALID,
		G_TYPE_INVALID);
	
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
	}
}

void cairo_dock_dbus_set_boolean_property (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gboolean bValue)
{
	GValue v = G_VALUE_INIT;
	g_value_init (&v, G_TYPE_BOOLEAN);
	g_value_set_boolean (&v, bValue);
	cairo_dock_dbus_set_property (pDbusProxy, cInterface, cProperty, &v);
}


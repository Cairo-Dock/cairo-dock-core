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

/***********************************************************************
 * New interface */

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


/***********************************************************************
 * Deprecated interface -- to be removed. */

static DBusGConnection *s_pSessionConnexion = NULL;
static DBusGConnection *s_pSystemConnexion = NULL;
static DBusGProxy *s_pDBusSessionProxy = NULL;
static DBusGProxy *s_pDBusSystemProxy = NULL;
static GHashTable *s_pFilterTable = NULL;
static GList *s_pFilterList = NULL;

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

gboolean cairo_dock_register_service_name (const gchar *cServiceName)
{
	DBusGProxy *pProxy = cairo_dock_get_main_proxy ();
	if (pProxy == NULL)
		return FALSE;
	GError *erreur = NULL;
	guint request_ret;
	org_freedesktop_DBus_request_name (pProxy, cServiceName, 0, &request_ret, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("Unable to register service: %s", erreur->message);
		g_error_free (erreur);
		return FALSE;
	}
	return TRUE;
}

static void on_name_owner_changed (G_GNUC_UNUSED DBusGProxy *pProxy, const gchar *cName, G_GNUC_UNUSED const gchar *cPrevOwner, const gchar *cNewOwner, G_GNUC_UNUSED gpointer data)
{
	//g_print ("%s (%s)\n", __func__, cName);
	gboolean bOwned = (cNewOwner != NULL && *cNewOwner != '\0');
	GList *pFilter = g_hash_table_lookup (s_pFilterTable, cName);
	GList *f;
	for (f = pFilter; f != NULL; f = f->next)
	{
		gpointer *p = f->data;
		CairoDockDbusNameOwnerChangedFunc pCallback = p[0];
		pCallback (cName, bOwned, p[1]);
	}
	
	for (f = s_pFilterList; f != NULL; f = f->next)
	{
		gpointer *p = f->data;
		if (strncmp (cName, p[2], GPOINTER_TO_INT (p[3])) == 0)
		{
			CairoDockDbusNameOwnerChangedFunc pCallback = p[0];
			pCallback (cName, bOwned, p[1]);
		}
	}
}
void cairo_dock_watch_dbus_name_owner (const char *cName, CairoDockDbusNameOwnerChangedFunc pCallback, gpointer data)
{
	if (cName == NULL)
		return;
	if (s_pFilterTable == NULL)  // first call to this function.
	{
		// create the table
		s_pFilterTable = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			g_free,
			NULL);
		
		// and register to the 'NameOwnerChanged' signal
		DBusGProxy *pProxy = cairo_dock_get_main_proxy ();
		g_return_if_fail (pProxy != NULL);
		
		dbus_g_proxy_add_signal (pProxy, "NameOwnerChanged",
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_INVALID);
		dbus_g_proxy_connect_signal (pProxy, "NameOwnerChanged",
			G_CALLBACK (on_name_owner_changed),
			NULL, NULL);
	}
	
	// record the callback.
	gpointer *p = g_new0 (gpointer, 4);
	p[0] = pCallback;
	p[1] = data;
	int n = strlen(cName);
	if (cName[n-1] == '*')
	{
		p[2] = g_strdup (cName);
		p[3] = GINT_TO_POINTER (n-1);
		s_pFilterList = g_list_prepend (s_pFilterList, p);
	}
	else
	{
		GList *pFilter = g_hash_table_lookup (s_pFilterTable, cName);
		pFilter = g_list_prepend (pFilter, p);
		g_hash_table_insert (s_pFilterTable, g_strdup (cName), pFilter);
	}
}

void cairo_dock_stop_watching_dbus_name_owner (const char *cName, CairoDockDbusNameOwnerChangedFunc pCallback)
{
	if (cName == NULL || *cName == '\0')
		return;
	int n = strlen(cName);
	if (cName[n-1] == '*')
	{
		GList *f;
		for (f = s_pFilterList; f != NULL; f = f->next)
		{
			gpointer *p = f->data;
			if (strncmp (cName, p[2], strlen (cName)-1) == 0 && pCallback == p[0])
			{
				g_free (p[2]);
				g_free (p);
				s_pFilterList = g_list_delete_link (s_pFilterList, f);
			}
		}
	}
	else
	{
		GList *pFilter = g_hash_table_lookup (s_pFilterTable, cName);
		if (pFilter != NULL)
		{
			GList *f;
			for (f = pFilter; f != NULL; f = f->next)
			{
				gpointer *p = f->data;
				if (pCallback == p[0])
				{
					g_free (p);
					pFilter = g_list_delete_link (pFilter, f);
					g_hash_table_insert (s_pFilterTable, g_strdup (cName), pFilter);
					break;
				}
			}
		}
	}
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
	cd_message ("detection du service %s (%d)...", cName, bSuccess);
	if (bSuccess && name_list)
	{
		int i;
		int n = strlen(cName);
		if (cName[n-1] == '*')
		{
			for (i = 0; name_list[i] != NULL; i ++)
			{
				if (strncmp (name_list[i], cName, n) == 0)
				{
					bPresent = TRUE;
					break;
				}
			}
		}
		else
		{
			for (i = 0; name_list[i] != NULL; i ++)
			{
				if (strcmp (name_list[i], cName) == 0)
				{
					bPresent = TRUE;
					break;
				}
			}
		}
	}
	
	pCallback (bPresent, user_data);
	
	g_strfreev (name_list);
	g_free (cName);
	data[2] = NULL;
}
static void _free_detect_application (gpointer *data)
{
	cd_debug ("free detection data\n");
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
		G_TYPE_STRV, &cValues,
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



void cairo_dock_dbus_get_property_in_value_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, GValue *pProperty, gint iTimeOut)
{
	GError *erreur=NULL;
	
	dbus_g_proxy_call_with_timeout (pDbusProxy, "Get", iTimeOut, &erreur,
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

gboolean cairo_dock_dbus_get_property_as_boolean_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
	if (G_VALUE_HOLDS_BOOLEAN (&v))
		return g_value_get_boolean (&v);
	else
		return FALSE;
}

gint cairo_dock_dbus_get_property_as_int_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
	if (G_VALUE_HOLDS_INT (&v))
		return g_value_get_int (&v);
	else
		return 0;
}

guint cairo_dock_dbus_get_property_as_uint_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
	if (G_VALUE_HOLDS_UINT (&v))
		return g_value_get_uint (&v);
	else
		return 0;
}

guchar cairo_dock_dbus_get_property_as_uchar_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
	if (G_VALUE_HOLDS_UCHAR (&v))
		return g_value_get_uchar (&v);
	else
		return 0;
}

gdouble cairo_dock_dbus_get_property_as_double_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
	if (G_VALUE_HOLDS_DOUBLE (&v))
		return g_value_get_double (&v);
	else
		return 0.;
}

gchar *cairo_dock_dbus_get_property_as_string_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
	if (G_VALUE_HOLDS_STRING (&v))
	{
		gchar *s = (gchar*)g_value_get_string (&v);  // on recupere directement le contenu de la GValue. Comme on ne libere pas la GValue, la chaine est a liberer soi-meme quand on en n'a plus besoin.
		return s;
	}
	else
		return NULL;
}

gchar *cairo_dock_dbus_get_property_as_object_path_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
	if (G_VALUE_HOLDS (&v, DBUS_TYPE_G_OBJECT_PATH))
	{
		gchar *s = (gchar*)g_value_get_string (&v);  // meme remarque.
		return s;
	}
	else
		return NULL;
}

gpointer cairo_dock_dbus_get_property_as_boxed_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
	if (G_VALUE_HOLDS_BOXED (&v))
	{
		gpointer p = g_value_get_boxed (&v);  // meme remarque.
		return p;
	}
	else
		return NULL;
}

gchar **cairo_dock_dbus_get_property_as_string_list_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	cairo_dock_dbus_get_property_in_value_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
	if (G_VALUE_HOLDS_BOXED(&v))
	{
		gchar **s = (gchar**)g_value_get_boxed (&v);  // meme remarque.
		return s;
	}
	else
		return NULL;
}

GHashTable *cairo_dock_dbus_get_all_properties_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, gint iTimeOut)
{
	GError *erreur=NULL;
	GHashTable *hProperties = NULL;
	
	dbus_g_proxy_call_with_timeout (pDbusProxy, "GetAll", iTimeOut, &erreur,
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


void cairo_dock_dbus_set_property_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, GValue *pProperty, gint iTimeOut)
{
	GError *erreur=NULL;
	
	dbus_g_proxy_call_with_timeout (pDbusProxy, "Set", iTimeOut, &erreur,
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

void cairo_dock_dbus_set_boolean_property_with_timeout (DBusGProxy *pDbusProxy, const gchar *cInterface, const gchar *cProperty, gboolean bValue, gint iTimeOut)
{
	GValue v = G_VALUE_INIT;
	g_value_init (&v, G_TYPE_BOOLEAN);
	g_value_set_boolean (&v, bValue);
	cairo_dock_dbus_set_property_with_timeout (pDbusProxy, cInterface, cProperty, &v, iTimeOut);
}


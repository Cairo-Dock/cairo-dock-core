/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Adrien Pilleboue (for any bug report, please mail me to adrien.pilleboue@gmail.com)

******************************************************************************/
#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include "cairo-dock-log.h"
#include "cairo-dock-dbus.h"

static DBusGConnection *s_pSessionConnexion = NULL;
static DBusGConnection *s_pSystemConnexion = NULL;
static DBusGProxy *s_pDBusProxy = NULL;

extern gchar *g_cConfFile;
extern CairoDock *g_pMainDock;


/*void cairo_dock_initialize_dbus_manager (void)
{
	g_return_if_fail (s_pSessionConnexion == NULL && s_pSystemConnexion == NULL);
	cd_message ("");
	GError *erreur = NULL;
	s_pSessionConnexion = dbus_g_bus_get (DBUS_BUS_SESSION, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("Attention : %s", erreur->message);
		g_error_free (erreur);
		s_pSessionConnexion = NULL;
	}
	if (s_pSessionConnexion == NULL)
		return ;
	
	s_pDBusProxy = dbus_g_proxy_new_for_name(s_pSessionConnexion,
		DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS,
		DBUS_INTERFACE_DBUS);
	
	s_pSystemConnexion = dbus_g_bus_get (DBUS_BUS_SYSTEM, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("Attention : %s", erreur->message);
		g_error_free (erreur);
		s_pSystemConnexion = NULL;
	}
	if (s_pSystemConnexion == NULL)
		return ;
}*/


DBusGConnection *cairo_dock_get_session_connection (void)
{
	if (s_pSessionConnexion == NULL)
	{
		GError *erreur = NULL;
		s_pSessionConnexion = dbus_g_bus_get (DBUS_BUS_SESSION, &erreur);
		if (erreur != NULL)
		{
			cd_warning ("Attention : %s", erreur->message);
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
			cd_warning ("Attention : %s", erreur->message);
			g_error_free (erreur);
			s_pSystemConnexion = NULL;
		}
	}
	return s_pSystemConnexion;
}

DBusGProxy *cairo_dock_get_main_proxy (void)
{
	if (s_pDBusProxy == NULL)
	{
		s_pDBusProxy = cairo_dock_create_new_session_proxy (DBUS_SERVICE_DBUS,
			DBUS_PATH_DBUS,
			DBUS_INTERFACE_DBUS);
	}
	return s_pDBusProxy;
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

gboolean cairo_dock_dbus_detect_application (const gchar *cName)
{
	cd_message ("");
	DBusGProxy *pProxy = cairo_dock_get_main_proxy ();
	if (pProxy == NULL)
		return FALSE;
	
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



gboolean cairo_dock_dbus_get_boolean (DBusGProxy *pDbusProxy, const gchar *cParameter)
{
	GError *erreur = NULL;
	gboolean bValue = FALSE;
	dbus_g_proxy_call (pDbusProxy, cParameter, &erreur,
		G_TYPE_INVALID,
		G_TYPE_BOOLEAN, &bValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning ("Attention : %s", erreur->message);
		g_error_free (erreur);
	}
	return bValue;
}

int cairo_dock_dbus_get_integer (DBusGProxy *pDbusProxy, const gchar *cParameter)
{
	GError *erreur = NULL;
	int iValue = 0;
	dbus_g_proxy_call (pDbusProxy, cParameter, &erreur,
		G_TYPE_INVALID,
		G_TYPE_INT, &iValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning ("Attention : %s", erreur->message);
		g_error_free (erreur);
	}
	return iValue;
}

guint cairo_dock_dbus_get_uinteger (DBusGProxy *pDbusProxy, const gchar *cParameter)
{
	GError *erreur = NULL;
	guint iValue = 0;
	dbus_g_proxy_call (pDbusProxy, cParameter, &erreur,
		G_TYPE_INVALID,
		G_TYPE_UINT, &iValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning ("Attention : %s", erreur->message);
		g_error_free (erreur);
	}
	return iValue;
}

gchar *cairo_dock_dbus_get_string (DBusGProxy *pDbusProxy, const gchar *cParameter)
{
	GError *erreur = NULL;
	gchar *cValue = NULL;
	dbus_g_proxy_call (pDbusProxy, cParameter, &erreur,
		G_TYPE_INVALID,
		G_TYPE_STRING, &cValue,
		G_TYPE_INVALID);
	if (erreur != NULL)
	{
		cd_warning ("Attention : %s", erreur->message);
		g_error_free (erreur);
	}
	return cValue;
}

guchar *cairo_dock_dbus_get_uchar (DBusGProxy *pDbusProxy, const gchar *cParameter)
{
	GError *erreur = NULL;
	guchar* uValue = NULL;
	
	dbus_g_proxy_call (pDbusProxy, cParameter, &erreur,
		G_TYPE_INVALID,
		G_TYPE_UCHAR, &uValue,
		G_TYPE_INVALID);
	
	return uValue;
}

void cairo_dock_dbus_call (DBusGProxy *pDbusProxy, const gchar *cCommand)
{
	dbus_g_proxy_call_no_reply (pDbusProxy, cCommand,
		G_TYPE_INVALID,
		G_TYPE_INVALID);
}

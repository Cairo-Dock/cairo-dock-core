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

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dbus.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-launcher-manager.h"  // cairo_dock_launch_command_sync
#include "cairo-dock-config.h"  // cairo_dock_get_version_from_string
#include "cairo-dock-compiz-integration.h"

static DBusGProxy *s_pScaleProxy = NULL;
static DBusGProxy *s_pExposeProxy = NULL;
static DBusGProxy *s_pWidgetLayerProxy = NULL;

#define CD_COMPIZ_BUS "org.freedesktop.compiz"
#define CD_COMPIZ_OBJECT "/org/freedesktop/compiz"
#define CD_COMPIZ_INTERFACE "org.freedesktop.compiz"

static gboolean present_windows (void)
{
	gboolean bSuccess = FALSE;
	if (s_pScaleProxy != NULL)
	{
		GError *erreur = NULL;
		bSuccess = dbus_g_proxy_call (s_pScaleProxy, "activate", &erreur,
			G_TYPE_STRING, "root",
			G_TYPE_INT, cairo_dock_get_root_id (),
			G_TYPE_STRING, "",
			G_TYPE_STRING, "",
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("compiz scale error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static gboolean present_class (const gchar *cClass)
{
	g_print ("%s (%s)\n", __func__, cClass);
	const GList *pIcons = cairo_dock_list_existing_appli_with_class (cClass);
	if (pIcons == NULL)
		return FALSE;
	
	gboolean bAllHidden = TRUE;
	Icon *pOneIcon;
	const GList *ic;
	int i = 0;
	for (ic = pIcons; ic != NULL; ic = ic->next)
	{
		pOneIcon = ic->data;
		bAllHidden &= pOneIcon->bIsHidden;
	}
	if (bAllHidden)
		return FALSE;
	
	gboolean bSuccess = FALSE;
	if (s_pScaleProxy != NULL)
	{
		GError *erreur = NULL;
		gchar *cMatch = g_strdup_printf ("class=.%s*", cClass+1);  /// we need the real class here...
		//g_print ("match %s\n", cMatch);
		bSuccess = dbus_g_proxy_call (s_pScaleProxy, "activate", &erreur,
			G_TYPE_STRING, "root",
			G_TYPE_INT, cairo_dock_get_root_id (),
			G_TYPE_STRING, "match",
			G_TYPE_STRING, cMatch,
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		g_free (cMatch);
		if (erreur)
		{
			cd_warning ("compiz scale error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static gboolean present_desktops (void)
{
	gboolean bSuccess = FALSE;
	if (s_pExposeProxy != NULL)
	{
		GError *erreur = NULL;
		bSuccess = dbus_g_proxy_call (s_pExposeProxy, "activate", &erreur,
			G_TYPE_STRING, "root",
			G_TYPE_INT, cairo_dock_get_root_id (),
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("compiz expo error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static gboolean show_widget_layer (void)
{
	gboolean bSuccess = FALSE;
	if (s_pWidgetLayerProxy != NULL)
	{
		GError *erreur = NULL;
		bSuccess = dbus_g_proxy_call (s_pWidgetLayerProxy, "activate", &erreur,
			G_TYPE_STRING, "root",
			G_TYPE_INT, cairo_dock_get_root_id (),
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("compiz widget layer error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static void _on_got_widget_match_rule (DBusGProxy *proxy, DBusGProxyCall *call_id, gpointer data)
{
	//g_print ("%s ()\n", __func__);
	// get the matching rule.
	GError *error = NULL;
	gchar *cRule = NULL;
	dbus_g_proxy_end_call (proxy,
		call_id,
		&error,
		G_TYPE_STRING,
		&cRule,
		G_TYPE_INVALID);
	if (error)
	{
		cd_warning ("compiz widget match error: %s", error->message);
		g_error_free (error);
		return;
	}
	cd_debug ("got rule : %s", cRule);
	
	// if our rule is not present yet, add it.
	if (cRule == NULL || *cRule == '\0' || (g_strstr_len (cRule, -1, "class=Cairo-dock & type=utility") == NULL && g_strstr_len (cRule, -1, "(class=Cairo-dock) & (type=utility)") == NULL && g_strstr_len (cRule, -1, "name=cairo-dock & type=utility") == NULL))
	{
		gchar *cNewRule = (cRule == NULL || *cRule == '\0' ?
			g_strdup ("(class=Cairo-dock & type=utility)") :
			g_strdup_printf ("(%s) | (class=Cairo-dock & type=utility)", cRule));
		cd_debug ("set rule : %s", cNewRule);
		
		dbus_g_proxy_call_no_reply (proxy,
			"set",
			G_TYPE_STRING,
			cNewRule,
			G_TYPE_INVALID);
		
		g_free (cNewRule);
	}
	g_free (cRule);
}
static gboolean _check_widget_rule (gpointer data)
{
	cd_debug ("%s ()", __func__);
	// get the matching rule.
	DBusGProxy *pWidgetProxy = cairo_dock_create_new_session_proxy (
			CD_COMPIZ_BUS,
			CD_COMPIZ_OBJECT"/widget/screen0/match",
			CD_COMPIZ_INTERFACE);
	dbus_g_proxy_begin_call (pWidgetProxy,
		"get",
		(DBusGProxyCallNotify) _on_got_widget_match_rule,
		NULL,
		NULL,
		G_TYPE_INVALID);
	///g_object_unref (pWidgetProxy);
	return FALSE;
}
static void _on_got_active_plugins (DBusGProxy *proxy, DBusGProxyCall *call_id, gpointer data)
{
	cd_debug ("%s ()", __func__);
	// get the active plug-ins.
	GError *error = NULL;
	gchar **plugins = NULL;
	dbus_g_proxy_end_call (proxy,
		call_id,
		&error,
		G_TYPE_STRV,
		&plugins,
		G_TYPE_INVALID);
	if (error)
	{
		cd_warning ("compiz active plug-ins error: %s", error->message);
		g_error_free (error);
		return;
	}
	g_return_if_fail (plugins != NULL);
	
	// look for the 'widget' plug-in.
	gboolean bFound = FALSE;
	int i;
	for (i = 0; plugins[i] != NULL; i++)
	{
		if (strcmp (plugins[i], "widget") == 0)
		{
			bFound = TRUE;
			break;
		}
	}
	
	// if not present, add it to the list and send it back to Compiz.
	if (!bFound)
	{
		gchar **plugins2 = g_new0 (gchar*, i+2);  // +1 for 'widget' and +1 for NULL
		memcpy (plugins2, plugins, i * sizeof (gchar*));
		plugins2[i] = (gchar*)"widget";  // elements of 'plugins2' are not freed.
		
		dbus_g_proxy_call_no_reply (proxy,
			"set",
			G_TYPE_STRV,
			plugins2,
			G_TYPE_INVALID);
		
		g_free (plugins2);  // elements belong to 'plugins' or are const.
	}
	g_strfreev (plugins);
	
	// now get the matching rule.
	if (bFound)  // the plug-in was already active
	{
		_check_widget_rule (NULL);
	}
	else  // the plug-in was not active yet, let it create its object on the bus.
	{
		g_timeout_add_seconds (2, _check_widget_rule, NULL);
	}
}
static gboolean _check_widget_plugin (gpointer data)
{
	// first get the active plug-ins.
	DBusGProxy *pActivePluginsProxy = cairo_dock_create_new_session_proxy (
		CD_COMPIZ_BUS,
		CD_COMPIZ_OBJECT"/core/allscreens/active_plugins",
		CD_COMPIZ_INTERFACE);
	
	dbus_g_proxy_begin_call (pActivePluginsProxy,
		"get",
		(DBusGProxyCallNotify) _on_got_active_plugins,
		NULL,
		NULL,
		G_TYPE_INVALID);
	///g_object_unref (pActivePluginsProxy);
	
	return FALSE;
}
static gboolean set_on_widget_layer (Window Xid, gboolean bOnWidgetLayer)
{
	g_print ("%s ()\n", __func__);
	static gboolean s_bFirst = TRUE;
	if (bOnWidgetLayer)
	{
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_UTILITY");
		// the first time, trigger a check to ensure the 'widget' plug-in is operationnal.
		if (s_bFirst)
		{
			g_timeout_add_seconds (2, _check_widget_plugin, NULL);
			s_bFirst = FALSE;
		}
	}
	else
	{
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_NORMAL");
	}
	return TRUE;
}


static void _register_compiz_backend (void)
{
	CairoDockWMBackend *p = g_new0 (CairoDockWMBackend, 1);
	
	p->present_class = present_class;
	p->present_windows = present_windows;
	p->present_desktops = present_desktops;
	p->show_widget_layer = show_widget_layer;
	p->set_on_widget_layer = set_on_widget_layer;
	
	cairo_dock_wm_register_backend (p);
}

static void _unregister_compiz_backend (void)
{
	cairo_dock_wm_register_backend (NULL);
}

static void _on_compiz_owner_changed (gboolean bOwned, gpointer data)
{
	cd_debug ("Compiz is on the bus (%d)", bOwned);
	
	if (bOwned)  // set up the proxies
	{
		g_return_if_fail (s_pScaleProxy == NULL);
		
		gboolean bNewCompiz = FALSE;
			
		gchar *cVersion = cairo_dock_launch_command_sync ("compiz --version");
		if (cVersion != NULL)
		{
			gchar *str = strchr (cVersion, ' ');  // "compiz 0.8.6"
			if (str != NULL)
			{
				int iMajorVersion, iMinorVersion, iMicroVersion;
				cairo_dock_get_version_from_string (str+1, &iMajorVersion, &iMinorVersion, &iMicroVersion);
				if (iMajorVersion > 0 || iMinorVersion > 8)
					bNewCompiz = TRUE;
			}
		}
		cd_debug ("NewCompiz: %d", bNewCompiz);
		
		s_pScaleProxy = cairo_dock_create_new_session_proxy (
			CD_COMPIZ_BUS,
			bNewCompiz ?
				CD_COMPIZ_OBJECT"/scale/screen0/initiate_all_key":
				CD_COMPIZ_OBJECT"/scale/allscreens/initiate_all_key",
			CD_COMPIZ_INTERFACE);
		
		s_pExposeProxy = cairo_dock_create_new_session_proxy (
			CD_COMPIZ_BUS,
			bNewCompiz ?
				CD_COMPIZ_OBJECT"/expo/screen0/expo_button":
				CD_COMPIZ_OBJECT"/expo/allscreens/expo_button",
			CD_COMPIZ_INTERFACE);
		
		s_pWidgetLayerProxy = cairo_dock_create_new_session_proxy (
			CD_COMPIZ_BUS,
			bNewCompiz ?
				CD_COMPIZ_OBJECT"/widget/screen0/toggle_button":
				CD_COMPIZ_OBJECT"/widget/allscreens/toggle_button",
			CD_COMPIZ_INTERFACE);
		
		_register_compiz_backend ();
	}
	else if (s_pScaleProxy != NULL)
	{
		g_object_unref (s_pScaleProxy);
		s_pScaleProxy = NULL;
		g_object_unref (s_pExposeProxy);
		s_pExposeProxy = NULL;
		g_object_unref (s_pWidgetLayerProxy);
		s_pWidgetLayerProxy = NULL;
		
		_unregister_compiz_backend ();
	}
}
static void _on_detect_compiz (gboolean bPresent, gpointer data)
{
	cd_debug ("Compiz is present: %d", bPresent);
	if (bPresent)
	{
		_on_compiz_owner_changed (TRUE, NULL);
	}
	cairo_dock_watch_dbus_name_owner (CD_COMPIZ_BUS,
		(CairoDockDbusNameOwnerChangedFunc) _on_compiz_owner_changed,
		NULL);
}
void cd_init_compiz_backend (void)
{
	DBusGProxyCall *call = cairo_dock_dbus_detect_application_async (CD_COMPIZ_BUS,
		(CairoDockOnAppliPresentOnDbus) _on_detect_compiz,
		NULL);
}

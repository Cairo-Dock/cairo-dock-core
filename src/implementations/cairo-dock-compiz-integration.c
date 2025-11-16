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

#include <glib.h>
#include <gio/gio.h>

#include "gldi-config.h"
#ifdef HAVE_X11
#include <X11/Xatom.h>
#include <gdk/gdkx.h>  // GDK_WINDOW_XID
#endif

#include "cairo-dock-log.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"  // bIsHidden
#include "cairo-dock-icon-factory.h"  // pAppli
#include "cairo-dock-container-priv.h"  // gldi_container_get_gdk_window
#include "cairo-dock-class-manager-priv.h"
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_argv_sync_with_stderr
#include "cairo-dock-X-utilities.h"  // cairo_dock_get_X_display, cairo_dock_change_nb_viewports
#include "cairo-dock-compiz-integration.h"

static GDBusProxy *s_pScaleProxy = NULL;
static GDBusProxy *s_pExposeProxy = NULL;
static GDBusProxy *s_pWidgetLayerProxy = NULL;
static GDBusProxy *s_pHSizeProxy = NULL;
static GDBusProxy *s_pVSizeProxy = NULL;

#ifdef HAVE_X11
static inline Window _get_root_Xid (void)
{
	Display *dpy = cairo_dock_get_X_display ();
	return (dpy ? DefaultRootWindow(dpy) : 0);
}
#else
#define _get_root_Xid(...) 0
#endif

static gboolean _present_windows (void)
{
	gint32 root = _get_root_Xid();
	if (! root)
		return FALSE;
	gboolean bSuccess = FALSE;
	if (s_pScaleProxy != NULL)
	{
		GError *erreur = NULL;
		GVariant *res = g_dbus_proxy_call_sync (s_pScaleProxy, "activate",
			g_variant_new ("(siss)", "root", root, "", ""),
			G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &erreur);
		if (res) g_variant_unref (res); // don't care
		
		if (erreur)
		{
			cd_warning ("compiz scale error: %s", erreur->message);
			g_error_free (erreur);
		}
		else bSuccess = TRUE;
	}
	return bSuccess;
}

static gboolean _present_class (const gchar *cClass)
{
	cd_debug ("%s (%s)", __func__, cClass);
	const GList *pIcons = cairo_dock_list_existing_appli_with_class (cClass);
	if (pIcons == NULL)
		return FALSE;
	
	gboolean bAllHidden = TRUE;
	Icon *pOneIcon;
	const GList *ic;
	for (ic = pIcons; ic != NULL; ic = ic->next)
	{
		pOneIcon = ic->data;
		bAllHidden &= pOneIcon->pAppli->bIsHidden;
	}
	if (bAllHidden)
		return FALSE;
	
	int root = _get_root_Xid();
	if (! root)
		return FALSE;
	
	gboolean bSuccess = FALSE;
	if (s_pScaleProxy != NULL)
	{
		GError *erreur = NULL;
		const gchar *cWmClass = cairo_dock_get_class_wm_class (cClass);
		gchar *cMatch;
		if (cWmClass)
			cMatch = g_strdup_printf ("class=%s", cWmClass);
		else
			cMatch = g_strdup_printf ("class=.%s*", cClass+1);
		cd_message ("Compiz: match '%s'", cMatch);
		GVariant *res = g_dbus_proxy_call_sync (s_pScaleProxy, "activate",
			g_variant_new ("(siss)", "root", root, "match", cMatch),
			G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &erreur);
		if (res) g_variant_unref (res); // don't care
		g_free (cMatch);
		if (erreur)
		{
			cd_warning ("compiz scale error: %s", erreur->message);
			g_error_free (erreur);
		}
		else bSuccess = TRUE;
	}
	return bSuccess;
}

static gboolean _present_desktops (void)
{
	int root = _get_root_Xid();
	if (! root)
		return FALSE;
	
	gboolean bSuccess = FALSE;
	if (s_pExposeProxy != NULL)
	{
		GError *erreur = NULL;
		GVariant *res = g_dbus_proxy_call_sync (s_pExposeProxy, "activate",
			g_variant_new ("(si)", "root", root),
			G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &erreur);
		if (res) g_variant_unref (res); // don't care
		if (erreur)
		{
			cd_warning ("compiz expo error: %s", erreur->message);
			g_error_free (erreur);
		}
		else bSuccess = TRUE;
	}
	return bSuccess;
}

static gboolean _show_widget_layer (void)
{
	int root = _get_root_Xid();
	if (! root)
		return FALSE;
	
	gboolean bSuccess = FALSE;
	if (s_pWidgetLayerProxy != NULL)
	{
		GError *erreur = NULL;
		GVariant *res = g_dbus_proxy_call_sync (s_pWidgetLayerProxy, "activate",
			g_variant_new ("(si)", "root", root),
			G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &erreur);
		if (res) g_variant_unref (res); // don't care
		if (erreur)
		{
			cd_warning ("compiz widget layer error: %s", erreur->message);
			g_error_free (erreur);
		}
		bSuccess = TRUE;
	}
	return bSuccess;
}

#ifdef HAVE_X11
static void _on_got_active_plugins (GObject *obj, GAsyncResult* async_res, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("%s ()", __func__);
	GDBusProxy *proxy = (GDBusProxy*)obj;
	g_object_ref (proxy); // we might need the proxy after the end of the call
	// get the active plug-ins.
	GError *error = NULL;
	const gchar **plugins = NULL;
	gsize len = 0;
	GVariant *res = g_dbus_proxy_call_finish (proxy, async_res, &error);
	
	if (error)
	{
		cd_warning ("compiz active plug-ins error: %s", error->message);
		g_object_unref (proxy);
		g_error_free (error);
		return;
	}
	
	if (!res || !g_variant_is_of_type (res, G_VARIANT_TYPE ("(as)")))
	{
		cd_warning ("compiz active plug-ins: unexpected result type");
		if (res) g_variant_unref (res);
		g_object_unref (proxy);
		return;
	}
	
	GVariant *res2 = g_variant_get_child_value (res, 0);
	if (res2) plugins = g_variant_get_strv (res2, &len);
	else
	{
		cd_warning ("compiz active plug-ins: unexpected result type");
		g_variant_unref (res);
		g_object_unref (proxy);
		return;
	}
	
	// look for the 'widget' plug-in.
	gboolean bFound = FALSE;
	gsize i;
	for (i = 0; i < len; i++)
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
		const gchar **plugins2 = g_new0 (const gchar*, len + 2);  // +1 for 'widget' and +1 for NULL
		memcpy (plugins2, plugins, len * sizeof (gchar*));
		plugins2[len] = "widget";  // elements of 'plugins2' are not freed.

		if (cd_is_the_new_compiz ())
		{
			gchar *cPluginsList = g_strjoinv (",", (gchar**)plugins2); // note: should not modify strings
			cd_debug ("Compiz Plugins List: %s", cPluginsList);
			const gchar * const args[] = {SHARE_DATA_DIR"/scripts/help_scripts.sh", "compiz_new_replace_list_plugins", cPluginsList, NULL};
			cairo_dock_launch_command_argv (args);
			g_free (cPluginsList);
		}
		else
		{	// It seems it doesn't work with Compiz 0.9 :-? => compiz (core) - Warn: Can't set Value with type 12 to option "active_plugins" with type 11 (with dbus-send too...)
			g_dbus_proxy_call (proxy, "set", g_variant_new_strv (plugins2, len + 1),
				G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, NULL, NULL);
		}
		
		g_free (plugins2);  // elements belong to 'plugins' or are const.
	}
	g_free (plugins);
	g_variant_unref (res);
	g_variant_unref (res2);
	g_object_unref (proxy);
}

static void _got_widget_plugin_proxy (G_GNUC_UNUSED GObject *obj, GAsyncResult *res, G_GNUC_UNUSED gpointer ptr)
{
	GError *erreur = NULL;
	GDBusProxy *proxy = g_dbus_proxy_new_finish (res, &erreur);
	if (erreur)
	{
		cd_warning ("Error creating Compiz plugin list DBus proxy: %s", erreur->message);
		g_error_free (erreur);
	}
	else
	{
		g_dbus_proxy_call (proxy,
			"get",
			NULL, // no parameters
			G_DBUS_CALL_FLAGS_NO_AUTO_START,
			-1, // timeout
			NULL, // GCancellable
			_on_got_active_plugins,
			NULL);
		g_object_unref (proxy); // ref will be kept by the call
	}
}

static gboolean _check_widget_plugin (G_GNUC_UNUSED gpointer data)
{
	// first get the active plug-ins.
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL, // GDBusInterfaceInfo
		CD_COMPIZ_BUS,
		cd_is_the_new_compiz () ?
			CD_COMPIZ_OBJECT"/core/screen0/active_plugins":
			CD_COMPIZ_OBJECT"/core/allscreens/active_plugins",
		CD_COMPIZ_INTERFACE,
		NULL, // GCancellable
		_got_widget_plugin_proxy,
		NULL);
	
	return FALSE;
}

static gboolean _set_on_widget_layer (GldiContainer *pContainer, gboolean bOnWidgetLayer)
{
	static gboolean s_bChecked = TRUE;
	static Atom s_aCompizWidget = None;
	
	cd_debug ("%s ()", __func__);
	Window Xid = GDK_WINDOW_XID (gldi_container_get_gdk_window(pContainer));
	Display *dpy = cairo_dock_get_X_display ();
	if (! dpy)
		return FALSE;
	if (s_aCompizWidget == None)
		s_aCompizWidget = XInternAtom (dpy, "_COMPIZ_WIDGET", False);
	
	if (bOnWidgetLayer)
	{
		// the first time, trigger a check to ensure the 'widget' plug-in is operationnal.
		if (s_bChecked)
		{
			g_timeout_add_seconds (2, _check_widget_plugin, NULL);
			s_bChecked = FALSE;
		}
		// set the _COMPIZ_WIDGET atom on the window to mark it.
		gulong widget = 1;
		XChangeProperty (dpy,
			Xid,
			s_aCompizWidget,
			XA_WINDOW, 32, PropModeReplace,
			(guchar *) &widget, 1);
	}
	else
	{
		XDeleteProperty (dpy,
			Xid,
			s_aCompizWidget);
	}
	return TRUE;
}
#else
static gboolean _set_on_widget_layer (G_GNUC_UNUSED GldiContainer *pContainer, G_GNUC_UNUSED gboolean bOnWidgetLayer)
{
	return FALSE;
}
#endif // HAVE_X11

/* Only add workspaces with Compiz: We shouldn't add desktops when using Compiz
 * and with this method, Compiz saves the new state
 */
static void _compiz_set_nb_viewports (int X, int Y)
{
	if (s_pHSizeProxy != NULL && s_pVSizeProxy != NULL)
	{
		GError *error = NULL;
		GVariant *res = g_dbus_proxy_call_sync (s_pHSizeProxy, "set",
			g_variant_new ("(i)", (gint32)X),
			G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &error);
		if (res) g_variant_unref (res);
		if (error)
		{
			cd_warning ("compiz HSize error: %s", error->message);
			g_error_free (error);
		}
		error = NULL;
		res = g_dbus_proxy_call_sync (s_pVSizeProxy, "set",
			g_variant_new ("(i)", (gint32)Y),
			G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &error);
		if (res) g_variant_unref (res);
		if (error)
		{
			cd_warning ("compiz VSize error: %s", error->message);
			g_error_free (error);
		}
	}
}

static void _add_workspace (void)
{
	cairo_dock_change_nb_viewports (1, _compiz_set_nb_viewports);
}

static void _remove_workspace (void)
{
	cairo_dock_change_nb_viewports (-1, _compiz_set_nb_viewports);
}

static gboolean bRegistered = FALSE;

static void _register_compiz_backend (void)
{
	if (bRegistered) return;
	bRegistered = TRUE;
	
	GldiDesktopManagerBackend p;
	memset(&p, 0, sizeof (GldiDesktopManagerBackend));
	
	p.present_class = _present_class;
	p.present_windows = _present_windows;
	p.present_desktops = _present_desktops;
	p.show_widget_layer = _show_widget_layer;
	p.set_on_widget_layer = _set_on_widget_layer;
	p.add_workspace = _add_workspace;
	p.remove_last_workspace = _remove_workspace;
	
	
	gldi_desktop_manager_register_backend (&p, "Compiz");
}

static void _unregister_compiz_backend (void)
{
	//cairo_dock_wm_register_backend (NULL);
}


static void _got_compiz_proxy (G_GNUC_UNUSED GObject *obj, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	GDBusProxy *proxy = g_dbus_proxy_new_finish (res, &error);
	if (error)
	{
		cd_warning ("Error connecting to Compiz DBus proxy: %s", error->message);
		g_error_free (error);
		return;
	}
	
	GDBusProxy **target = (GDBusProxy**)data;
	if (*target) g_object_unref (*target); // should not happen, but just to be safe
	*target = proxy;
	
	// we need to call this only once, but it is OK to call multiple times
	_register_compiz_backend ();
}

gboolean cd_is_the_new_compiz (void)
{
	static gboolean s_bNewCompiz = FALSE;
	static gboolean s_bHasBeenChecked = FALSE;  // only check it once, as it's likely to not change.
	if (!s_bHasBeenChecked)
	{
		s_bHasBeenChecked = TRUE;
		const char * const args[] = {"compiz", "--version", NULL};
		gchar *cVersion = cairo_dock_launch_command_argv_sync_with_stderr (args, FALSE);
		if (cVersion != NULL)
		{
			gchar *str = strchr (cVersion, ' ');  // "compiz 0.8.6"
			if (str != NULL)
			{
				int iMajorVersion, iMinorVersion, iMicroVersion;
				cairo_dock_get_version_from_string (str+1, &iMajorVersion, &iMinorVersion, &iMicroVersion);
				if (iMajorVersion > 0 || iMinorVersion > 8)
					s_bNewCompiz = TRUE;
			}
		}
		g_free (cVersion);
		cd_debug ("NewCompiz: %d", s_bNewCompiz);
	}
	return s_bNewCompiz;
}

static void _on_name_appeared (GDBusConnection *connection, const gchar *name,
	G_GNUC_UNUSED const gchar *name_owner, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Compiz is on the bus");
	
	gboolean bNewCompiz = cd_is_the_new_compiz ();
	
	g_dbus_proxy_new (connection, 
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL, // GDBusInterfaceInfo
		name,
		bNewCompiz ?
			CD_COMPIZ_OBJECT"/scale/screen0/initiate_all_key":
			CD_COMPIZ_OBJECT"/scale/allscreens/initiate_all_key",
		CD_COMPIZ_INTERFACE,
		NULL, // GCancellable
		_got_compiz_proxy,
		&s_pScaleProxy);
	
	g_dbus_proxy_new (connection, 
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL, // GDBusInterfaceInfo
		name,
		bNewCompiz ?
			CD_COMPIZ_OBJECT"/expo/screen0/expo_button":
			CD_COMPIZ_OBJECT"/expo/allscreens/expo_button",
		CD_COMPIZ_INTERFACE,
		NULL, // GCancellable
		_got_compiz_proxy,
		&s_pExposeProxy);
	
	g_dbus_proxy_new (connection, 
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL, // GDBusInterfaceInfo
		name,
		bNewCompiz ?
			CD_COMPIZ_OBJECT"/widget/screen0/toggle_button":
			CD_COMPIZ_OBJECT"/widget/allscreens/toggle_button",
		CD_COMPIZ_INTERFACE,
		NULL, // GCancellable
		_got_compiz_proxy,
		&s_pWidgetLayerProxy);
	
	g_dbus_proxy_new (connection, 
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL, // GDBusInterfaceInfo
		name,
		bNewCompiz ?
			CD_COMPIZ_OBJECT"/core/screen0/hsize":
			CD_COMPIZ_OBJECT"/core/allscreens/hsize",
		CD_COMPIZ_INTERFACE,
		NULL, // GCancellable
		_got_compiz_proxy,
		&s_pHSizeProxy);
	
	g_dbus_proxy_new (connection, 
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL, // GDBusInterfaceInfo
		name,
		bNewCompiz ?
			CD_COMPIZ_OBJECT"/core/screen0/vsize":
			CD_COMPIZ_OBJECT"/core/allscreens/vsize",
		CD_COMPIZ_INTERFACE,
		NULL, // GCancellable
		_got_compiz_proxy,
		&s_pVSizeProxy);
}
	
static void _on_name_vanished (G_GNUC_UNUSED GDBusConnection *connection, G_GNUC_UNUSED const gchar *name, G_GNUC_UNUSED gpointer user_data)
{
	if (s_pScaleProxy != NULL)
	{
		g_object_unref (s_pScaleProxy);
		s_pScaleProxy = NULL;
	}
	if (s_pExposeProxy != NULL)
	{
		g_object_unref (s_pExposeProxy);
		s_pExposeProxy = NULL;
	}
	if (s_pWidgetLayerProxy != NULL)
	{
		g_object_unref (s_pWidgetLayerProxy);
		s_pWidgetLayerProxy = NULL;
	}
	if (s_pHSizeProxy != NULL)
	{
		g_object_unref (s_pHSizeProxy);
		s_pHSizeProxy = NULL;
	}
	if (s_pVSizeProxy != NULL)
	{
		g_object_unref (s_pVSizeProxy);
		s_pVSizeProxy = NULL;
	}
	_unregister_compiz_backend ();
}

void cd_init_compiz_backend (void)
{
	g_bus_watch_name (G_BUS_TYPE_SESSION, CD_COMPIZ_BUS, G_BUS_NAME_WATCHER_FLAGS_NONE,
		_on_name_appeared, _on_name_vanished, NULL, NULL);
}

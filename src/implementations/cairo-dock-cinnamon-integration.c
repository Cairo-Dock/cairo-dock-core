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

#include "cairo-dock-log.h"
#include "cairo-dock-class-manager-priv.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-cinnamon-integration.h"

static GDBusProxy *s_pProxy = NULL;

#define CD_CINNAMON_BUS "org.Cinnamon"
#define CD_CINNAMON_OBJECT "/org/Cinnamon"
#define CD_CINNAMON_INTERFACE "org.Cinnamon"


struct eval_cb_data
{
	CairoDockDesktopManagerActionResult cb;
	gpointer user_data;
};

static void _eval_cb (GObject *pObj, GAsyncResult *pRes, gpointer data)
{
	struct eval_cb_data *cb_data = (struct eval_cb_data*)data;
	GError *error = NULL;
	gboolean bSuccess = FALSE;
	
	GVariant *res = g_dbus_proxy_call_finish (G_DBUS_PROXY (pObj), pRes, &error);
	if (error)
	{
		cd_warning ("Error calling Cinnamon method: %s", error->message);
		g_error_free (error);
	}
	else if (res && g_variant_is_of_type (res, G_VARIANT_TYPE ("(bs)")))
	{
		// result is a bool + string
		g_variant_get_child (res, 0, "b", &bSuccess);
		GVariant *str = g_variant_get_child_value (res, 1);
		const gchar *cResult = g_variant_get_string (str, NULL);
		
		cd_debug ("%s", cResult);
		g_variant_unref (str);
	}
	if (res) g_variant_unref (res);
	
	cb_data->cb (bSuccess, cb_data->user_data); // signal success
	g_free (cb_data);
}

static void _eval (const gchar *cmd, CairoDockDesktopManagerActionResult cb, gpointer user_data)
{
	if (s_pProxy != NULL)
	{
		if (cb)
		{
			struct eval_cb_data *cb_data = g_new0 (struct eval_cb_data, 1);
			cb_data->cb = cb;
			cb_data->user_data = user_data;
			g_dbus_proxy_call (s_pProxy, "Eval", g_variant_new ("(s)", cmd),
				G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, _eval_cb, cb_data);
		}
		else g_dbus_proxy_call (s_pProxy, "Eval", g_variant_new ("(s)", cmd),
			G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, NULL, NULL);
	}
	else if (cb) cb (FALSE, user_data);
}

static void present_overview (void)
{
	_eval ("Main.overview.toggle();", NULL, NULL);
}

static void present_expo (void)
{
	_eval ("Main.expo.toggle();", NULL, NULL);
}

static void present_class (const gchar *cClass, CairoDockDesktopManagerActionResult cb, gpointer user_data)
{
	cd_debug ("%s", cClass);
	GList *pIcons = (GList*)cairo_dock_list_existing_appli_with_class (cClass);
	if (pIcons == NULL)
	{
		if (cb) cb (FALSE, user_data);
		return;
	}
	
	if (s_pProxy != NULL)
	{
		const gchar *cWmClass = cairo_dock_get_class_wm_class (cClass);
		int iNumDesktop, iViewPortX, iViewPortY;
		gldi_desktop_get_current (&iNumDesktop, &iViewPortX, &iViewPortY);
		// note: this is only used on X11, all desktops have the same viewport geometry
		int iWorkspace = iNumDesktop * g_desktopGeometry.pViewportsX[0] * g_desktopGeometry.pViewportsY[0] + iViewPortX + iViewPortY * g_desktopGeometry.pViewportsX[0];
		gchar *code = g_strdup_printf ("Main.overview.show(); \
		let windows = global.get_window_actors(); \
		let ws = Main.overview.workspacesView._workspaces[%d]._monitors[0]; \
		for (let i = 0; i < windows.length; i++) \
		{ \
			let win = windows[i]; \
			let metaWin = win.get_meta_window(); \
			let index = ws._lookupIndex(metaWin); \
			if (metaWin.get_wm_class() == '%s') \
				{ if (index == -1) ws._addWindowClone(win); } \
				else \
				{ if (index != -1) { let clone = ws._windows[index]; ws._windows.splice(index, 1);clone.destroy(); } \
			} \
		}", iWorkspace, cWmClass);
		cd_debug ("eval(%s)", code);
		_eval (code, cb, user_data);
		g_free (code);
	}
}

static gboolean bRegistered = FALSE;

static void _register_cinnamon_backend (void)
{
	if (bRegistered) return;
	bRegistered = TRUE;
	
	GldiDesktopManagerBackend p;
	memset(&p, 0, sizeof (GldiDesktopManagerBackend));
	
	p.present_class = present_class;
	p.present_windows = present_overview;
	p.present_desktops = present_expo;
	
	gldi_desktop_manager_register_backend (&p, "Cinnamon");
}

static void _unregister_cinnamon_backend (void)
{
	// we cannot unregister, but there would be no point anyway
	
	//cairo_dock_wm_register_backend (NULL);
}

static void _on_proxy_connected (G_GNUC_UNUSED GObject *obj, GAsyncResult *res, G_GNUC_UNUSED gpointer ptr)
{
	if (s_pProxy)
	{
		// should not happen
		g_object_unref (s_pProxy);
		s_pProxy = NULL;
	}
	
	GError *erreur = NULL;
	s_pProxy = g_dbus_proxy_new_finish (res, &erreur);
	if (erreur)
	{
		cd_warning ("Error creating Cinnamon DBus proxy: %s", erreur->message);
		g_error_free (erreur);
	}
	else _register_cinnamon_backend ();
}

static void _on_name_appeared (GDBusConnection *connection, const gchar *name,
	G_GNUC_UNUSED const gchar *name_owner, G_GNUC_UNUSED gpointer data)
{	
	g_dbus_proxy_new (connection,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
		NULL, // GDBusInterfaceInfo -- we could supply this, but works anyway
		name,
		CD_CINNAMON_OBJECT,
		CD_CINNAMON_INTERFACE,
		NULL,
		_on_proxy_connected,
		NULL);
}

static void _on_name_vanished (G_GNUC_UNUSED GDBusConnection *connection, G_GNUC_UNUSED const gchar *name, G_GNUC_UNUSED gpointer user_data)
{
	if (s_pProxy != NULL)
	{
		g_object_unref (s_pProxy);
		s_pProxy = NULL;
		
		_unregister_cinnamon_backend ();
	}
}

void cd_init_cinnamon_backend (void)
{
	g_bus_watch_name (G_BUS_TYPE_SESSION, CD_CINNAMON_BUS, G_BUS_NAME_WATCHER_FLAGS_NONE,
		_on_name_appeared, _on_name_vanished, NULL, NULL);
}


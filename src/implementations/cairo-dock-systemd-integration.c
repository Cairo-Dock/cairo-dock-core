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


#include <stdio.h>
#include <time.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include "cairo-dock-systemd-integration.h"
#include <cairo-dock-log.h>

#if GLIB_CHECK_VERSION (2, 84, 0)
#define _init_variant_builder g_variant_builder_init_static
#else
#define _init_variant_builder g_variant_builder_init
#endif

static GDBusProxy *s_proxy = NULL;

static guint32 s_iLaunchID = 0; // unique counter to be used as a suffix for systemd unit names
static guint32 s_iLaunchTS = 0; // timestamp of when we were launched, to be used as a prefix for systemd unit names

static void _spawn_end (G_GNUC_UNUSED GObject* pObj, GAsyncResult *res, G_GNUC_UNUSED gpointer ptr)
{
	GError *err = NULL;
	GVariant *ret = g_dbus_proxy_call_finish (s_proxy, res, &err);
	if (ret) g_variant_unref (ret);
	else
	{
		cd_warning ("couldn't launch app: %s", err->message);
		g_error_free (err);
	}
}

static void _spawn_app (const gchar * const *args, const gchar *id, const gchar *desc, const gchar * const *env, const gchar *working_dir)
{
	if (!s_proxy) return;
	if (!(args && *args)) return;
	
	/* Note: systemd unit names must be unique. We ensure this by:
	 *  -- using the "app-cairodock-" prefix to distinguish from other units
	 *  -- adding the app-id to distinguish between apps started by us
	 *  -- adding a @num suffix to distinguish between multiple instances of the same app
	 *  -- adding the time we were started as part of our prefix, to avoid clashes if
	 *     Cairo-Dock is restarted (or started multiple times, e.g. during development;
	 *     this will still not work if two instances of Cairo-Dock are started at the
	 *     exact same time, but we expect this not to occur in normal usage)
	 * Note: systemd will create a separate slice for each service started by us,
	 * and an additional prefix, but this does not really matter.
	 *  */
	s_iLaunchID++;
	// we don't expect a wrap around, but just in case, we reset the time prefix in this case
	if (!s_iLaunchID) s_iLaunchTS = (guint32) time (NULL); 
	char *name;
	const size_t len = strlen (id);
	const size_t max_len =
		255 // length allowed by systemd
		- 25 // length of our prefix + dash + suffix + nul terminator
		- 20; // 2 * max length of a 32-bit integer
	if (len > max_len)
		name = g_strdup_printf ("app-cairodock-%"G_GUINT32_FORMAT"-%.*s@%"G_GUINT32_FORMAT".service", s_iLaunchTS, (int)max_len, id, s_iLaunchID);
	else name = g_strdup_printf ("app-cairodock-%"G_GUINT32_FORMAT"-%s@%"G_GUINT32_FORMAT".service", s_iLaunchTS, id, s_iLaunchID);
	
	GVariantBuilder var_builder;
	_init_variant_builder   (&var_builder, G_VARIANT_TYPE ("(ssa(sv)a(sa(sv)))"));
	g_variant_builder_add_value (&var_builder, g_variant_new_take_string (name));
	g_variant_builder_add   (&var_builder, "s", "fail");
	
	// Unit properties
	g_variant_builder_open  (&var_builder, G_VARIANT_TYPE ("a(sv)"));
	g_variant_builder_add   (&var_builder, "(sv)", "Description", g_variant_new_string (desc));
	const gchar *exec_flags[] = {"no-env-expand", "ignore-failure", NULL};	
	GVariant *tmp = g_variant_new ("(s^as^as)", args[0], args, exec_flags);
	// note: ExecStartEx expects an array of (sasas)
	g_variant_builder_add   (&var_builder, "(sv)", "ExecStartEx", g_variant_new_array (NULL, &tmp, 1));
	if (env && *env) g_variant_builder_add (&var_builder, "(sv)", "Environment", g_variant_new_strv (env, -1));
	if (working_dir) g_variant_builder_add (&var_builder, "(sv)", "WorkingDirectory", g_variant_new_string (working_dir));
	// fail if systemd cannot exec the process binary
	g_variant_builder_add   (&var_builder, "(sv)", "Type", g_variant_new_string ("exec"));
	// clean up failed processes (otherwise, systemd service units remain in the "failed" state)
	g_variant_builder_add   (&var_builder, "(sv)", "CollectMode", g_variant_new_string ("inactive-or-failed"));
	// only consider this service to end if all processes spawned by it have exited (otherwise, systemd would
	// kill any child processes after the main process exited; this is a problem e.g. with cosmic-files)
	g_variant_builder_add   (&var_builder, "(sv)", "ExitType", g_variant_new_string ("cgroup"));
	g_variant_builder_close (&var_builder);
	g_variant_builder_open  (&var_builder, G_VARIANT_TYPE ("a(sa(sv))"));
	g_variant_builder_close (&var_builder);
	
	g_dbus_proxy_call (s_proxy, "StartTransientUnit", g_variant_builder_end (&var_builder), G_DBUS_CALL_FLAGS_NONE, -1, NULL, _spawn_end, NULL);
}



static void _proxy_connected (G_GNUC_UNUSED GObject* pObj, GAsyncResult *res, G_GNUC_UNUSED gpointer ptr)
{
	s_proxy = g_dbus_proxy_new_for_bus_finish (res, NULL);
	if (s_proxy)
	{
		// Now that we have a proxy, register our backend for starting apps
		GldiChildProcessManagerBackend backend;
		backend.spawn_app = _spawn_app;
		gldi_register_process_manager_backend (&backend);
	}
	else cd_warning ("Cannot create DBus proxy for org.freedesktop.systemd1");
}

static void _got_version (GObject* pObj, GAsyncResult *res, G_GNUC_UNUSED gpointer ptr)
{
	GError *erreur = NULL;
	GDBusConnection *pConn = G_DBUS_CONNECTION (pObj);
	GVariant *prop = g_dbus_connection_call_finish (pConn, res, &erreur);
	if (erreur)
	{
		cd_warning ("Cannot get systemd version, not registering (%s)", erreur->message);
		g_error_free (erreur);
		return;
	}
	
	long version = -1L;
	if (g_variant_is_of_type (prop, G_VARIANT_TYPE ("(v)")))
	{
		GVariant *tmp1 = g_variant_get_child_value (prop, 0);
		GVariant *tmp2 = g_variant_get_variant (tmp1);
		if (g_variant_is_of_type (tmp2, G_VARIANT_TYPE ("s")))
		{
			gsize len = 0;
			const gchar *tmp3 = g_variant_get_string (tmp2, &len); // note: return value is never NULL
			if (len > 0)
			{
				char *end;
				version = strtol (tmp3, &end, 10); // e.g. 255.4-1ubuntu8.11, we only want the major version, which is the integer part
				if (end == tmp3) version = -1; // error parsing
			}
		}
		g_variant_unref (tmp1);
		g_variant_unref (tmp2);
	}
	g_variant_unref (prop);
	
	if (version < 0L)
	{
		cd_warning ("Unexpected format for systemd version");
		return;
	}
	
	if (version < 250L)
	{
		cd_message ("Systemd version < 250, not registering");
		return;
	}
	
	// connect to the real proxy
	g_dbus_proxy_new (pConn,
		G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
		NULL, // GDBusInterfaceInfo
		"org.freedesktop.systemd1",
		"/org/freedesktop/systemd1",
		"org.freedesktop.systemd1.Manager",
		NULL, // GCancellable
		_proxy_connected,
		NULL);
}

void cairo_dock_systemd_integration_init (void)
{
	// Note: we don't use g_bus_watch_name () as we expect that systemd will already be
	// on the bus and will not disappear. We just directly make a call to check its
	// version and abort on failure.
	GDBusConnection *pConn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL); // bus should already be connected
	if (!pConn)
	{
		cd_message ("DBus not available, not connecting");
		return;
	}
	
	g_dbus_connection_call (pConn,
		"org.freedesktop.systemd1",
		"/org/freedesktop/systemd1",
		"org.freedesktop.DBus.Properties",
		"Get",
		g_variant_new ("(ss)", "org.freedesktop.systemd1.Manager", "Version"),
		G_VARIANT_TYPE ("(v)"),
		G_DBUS_CALL_FLAGS_NO_AUTO_START,
		-1,
		NULL, // cancellable
		_got_version,
		NULL);
	
	s_iLaunchTS = (guint32) time (NULL); // should be safe to cast and we do not care about the actual value, only that it is unique
}



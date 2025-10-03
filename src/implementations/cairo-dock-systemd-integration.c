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

GDBusProxy *s_proxy = NULL;

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


static GVariantType *s_full_type = NULL;
static GVariantType *s_props_type = NULL;
static GVariantType *s_aux_type = NULL;
static GVariantType *s_args_one_type = NULL;
static GVariantType *s_string_array_type = NULL;

static void _init_variant_types (void)
{
	/*
	       StartTransientUnit(in  s name,
                         in  s mode,
                         in  a(sv) properties,
                         in  a(sa(sv)) aux,
                         out o job);
	*/
	if (!s_full_type) s_full_type = g_variant_type_new ("(ssa(sv)a(sa(sv)))");
	if (!s_props_type) s_props_type = g_variant_type_new ("a(sv)");
	if (!s_aux_type) s_aux_type = g_variant_type_new ("a(sa(sv))");
	// format of args
	if (!s_args_one_type) s_args_one_type = g_variant_type_new ("(sasb)");
	// args and env vectors
	if (!s_string_array_type) s_string_array_type = g_variant_type_new ("as");
	
	// note: all type variables are leaked -- could add a destructor that is called on exit
}

static void _spawn_app (const gchar * const *args, const gchar *id, const gchar *desc, const gchar * const *env, const gchar *working_dir)
{
	if (!s_proxy) return;
	if (!(args && *args)) return;
	
	_init_variant_types ();
	
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
	g_variant_builder_init  (&var_builder, s_full_type);
	g_variant_builder_add_value (&var_builder, g_variant_new_take_string (name));
	g_variant_builder_add   (&var_builder, "s", "fail");
	g_variant_builder_open  (&var_builder, s_props_type);
	g_variant_builder_add   (&var_builder, "(sv)", "Description", g_variant_new_string (desc));
	{
		GVariantBuilder args_builder;
		g_variant_builder_init  (&args_builder, s_args_one_type);
		g_variant_builder_add   (&args_builder, "s", args[0]);
		g_variant_builder_open  (&args_builder, s_string_array_type);
		for(; *args; ++args) g_variant_builder_add (&args_builder, "s", *args);
		g_variant_builder_close (&args_builder);
		g_variant_builder_add   (&args_builder, "b", FALSE);
		GVariant *tmp = g_variant_builder_end (&args_builder);
		g_variant_builder_add   (&var_builder, "(sv)", "ExecStart", g_variant_new_array (NULL, &tmp, 1));
	}
	if (env && *env)
	{
		GVariantBuilder env_builder;
		g_variant_builder_init (&env_builder, s_string_array_type);
		if (env) for (; *env; ++env) g_variant_builder_add (&env_builder, "s", *env);
		g_variant_builder_add (&var_builder, "(sv)", "Environment", g_variant_builder_end (&env_builder));
	}
	if (working_dir) g_variant_builder_add (&var_builder, "(sv)", "WorkingDirectory", working_dir);
	// fail if systemd cannot exec the process binary
	g_variant_builder_add   (&var_builder, "(sv)", "Type", g_variant_new_string ("exec"));
	// clean up failed processes (otherwise, systemd service units remain in the "failed" state)
	g_variant_builder_add   (&var_builder, "(sv)", "CollectMode", g_variant_new_string ("inactive-or-failed"));
	// only consider this service to end if all processes spawned by it have exited (otherwise, systemd would
	// kill any child processes after the main process exited; this is a problem e.g. with cosmic-files)
	g_variant_builder_add   (&var_builder, "(sv)", "ExitType", g_variant_new_string ("cgroup"));
	g_variant_builder_close (&var_builder);
	g_variant_builder_open  (&var_builder, s_aux_type);
	g_variant_builder_close (&var_builder);
	
	g_dbus_proxy_call (s_proxy, "StartTransientUnit", g_variant_builder_end (&var_builder), G_DBUS_CALL_FLAGS_NONE, -1, NULL, _spawn_end, NULL);
}


static void _proxy_connected (G_GNUC_UNUSED GObject* pObj, GAsyncResult *res, G_GNUC_UNUSED gpointer ptr)
{
	s_proxy = g_dbus_proxy_new_for_bus_finish (res, NULL);
	if (s_proxy)
	{
		const char *owner = g_dbus_proxy_get_name_owner (s_proxy);
		if (!owner)
		{
			// we expect that systemd should be on the bus before 
			// TODO: or possibly wait for it to appear later?
			cd_message ("no owner for org.freedesktop.systemd1, not registering");
			g_object_unref (s_proxy);
			s_proxy = NULL;
			return;
		}
		
		GldiChildProcessManagerBackend backend;
		backend.spawn_app = _spawn_app;
		gldi_register_process_manager_backend (&backend);
	}
}


void cairo_dock_systemd_integration_init (void)
{
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
		G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
		NULL, // GDBusInterfaceInfo
		"org.freedesktop.systemd1",
		"/org/freedesktop/systemd1",
		"org.freedesktop.systemd1.Manager",
		NULL, // GCancellable
		_proxy_connected,
		NULL);
	s_iLaunchTS = (guint32) time (NULL); // should be safe to cast and we do not care about the actual value, only that it is unique
}



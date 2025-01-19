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
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include "cairo-dock-systemd-integration.h"
#include <cairo-dock-log.h>

GDBusProxy *s_proxy = NULL;

static void _child_watch_dummy (GPid pid, gint, gpointer)
{
	g_spawn_close_pid (pid); // note: this is a no-op
}

static void _create_scope_end (GObject*, GAsyncResult *res, gpointer ptr)
{
	GError *err = NULL;
	GVariant *ret = g_dbus_proxy_call_finish (s_proxy, res, &err);
	if (ret) g_variant_unref (ret);
	else
	{
		cd_warning ("couldn't set scope for child process: %s", err->message);
		g_error_free (err);
	}
	
	// we only create the child watch after the DBus call to systemd finished
	// hopefully this avoids pid reuse race conditions (i.e. until it has been
	// waited for, the pid should become a zombie if the process exits)
	g_child_watch_add (GPOINTER_TO_INT (ptr), _child_watch_dummy, NULL);
}

static void _create_transient_scope (const char *id, const char *desc, GPid pid)
{
	if (!s_proxy) return;
	
	GVariantBuilder var_builder;
	/*
	       StartTransientUnit(in  s name,
                         in  s mode,
                         in  a(sv) properties,
                         in  a(sa(sv)) aux,
                         out o job);
	*/
	GVariantType *full_type  = g_variant_type_new ("(ssa(sv)a(sa(sv)))");
	GVariantType *props_type = g_variant_type_new ("a(sv)");
	GVariantType *aux_type   = g_variant_type_new ("a(sa(sv))");
	
	char *name;
	size_t len = strlen (id);
	const size_t max_len =
		255 // length allowed by systemd
		- 42 // length of our prefix + dash + suffix
		- 9; // max length of %d specifier (assuming pid_t is 32 bits)
	if (len > max_len)
		name = g_strdup_printf ("cairo-dock-launched-%.*s-%d.scope", (int)max_len, id, pid);
	else name = g_strdup_printf ("cairo-dock-launched-%s-%d.scope", id, pid);
	unsigned int tmp = (unsigned int)pid;
	
	g_variant_builder_init  (&var_builder, full_type);
	g_variant_builder_add   (&var_builder, "s", name);
	g_variant_builder_add   (&var_builder, "s", "fail");
	g_variant_builder_open  (&var_builder, props_type);
	g_variant_builder_add   (&var_builder, "(sv)", "Description", g_variant_new_string (desc));
	g_variant_builder_add   (&var_builder, "(sv)", "PIDs", g_variant_new_fixed_array (G_VARIANT_TYPE_UINT32, &tmp, 1, 4));
	g_variant_builder_close (&var_builder);
	g_variant_builder_open  (&var_builder, aux_type);
	g_variant_builder_close (&var_builder);
	
	GVariant *var = g_variant_ref_sink (g_variant_builder_end (&var_builder));
	g_dbus_proxy_call (s_proxy, "StartTransientUnit", var, G_DBUS_CALL_FLAGS_NONE, -1, NULL, _create_scope_end, GINT_TO_POINTER (pid));
	
	g_variant_unref (var);
	g_free (name);
	g_variant_type_free (full_type);
	g_variant_type_free (props_type);
	g_variant_type_free (aux_type);
}


static void _proxy_connected (GObject*, GAsyncResult *res, gpointer)
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
		backend.new_app_launched = _create_transient_scope;
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
}



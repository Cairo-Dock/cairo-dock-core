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

/**
 * @file cairo-dock-logout-backend.c
 * This file implements generic (DE-independent) functions to log out, shutdown, etc.
 * These were previously part of the logout plugin, but are useful more generally, so
 * they are provided here.
 */

#include "cairo-dock-logout-backend.h"
#include "cairo-dock-task.h"
#include "cairo-dock-utils.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dialog-factory.h"

typedef enum {
	CD_LOGIN_UNKNOWN,
	CD_CONSOLE_KIT,
	CD_LOGIND,
	CD_NB_LOGIN_MANAGER
} CDLoginManager;

typedef enum {
	CD_LOGOUT_UNKNOWN,
	CD_WAYLAND,
	CD_SYSTEMD
	// TODO: old method which kills GDM / other !
} CDLogoutType;
	
static gboolean s_bInitDone = FALSE;
static gboolean s_bCanHibernate = FALSE;
static gboolean s_bCanHybridSleep = FALSE;
static gboolean s_bCanSuspend = FALSE;
static gboolean s_bCanStop = FALSE;
static gboolean s_bCanRestart = FALSE;
static gboolean s_bCanLock = FALSE;
static CDLoginManager s_iLoginManager = CD_LOGIN_UNKNOWN;
static CDLogoutType s_iLogoutType = CD_LOGOUT_UNKNOWN;

static GldiTask *s_pTask = NULL;

// proxy for actions, either to Logind or ConsoleKit
static GDBusProxy *s_proxy = NULL;

// proxy for NetworkManager (for toggling network status)
static GDBusProxy *s_proxy_nm = NULL;
static gboolean s_bNmChecked = FALSE;

static void _console_kit_action (const gchar *cAction)
{
	if (!s_proxy)
	{
		cd_warning ("No proxy to ConsoleKit has been set up!");
		return;
	}
	
	g_dbus_proxy_call (s_proxy, cAction, NULL,
		G_DBUS_CALL_FLAGS_NO_AUTO_START | G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
		-1, NULL, NULL, NULL);
}

static void _logind_action (const gchar *cAction)
{
	if (!s_proxy)
	{
		cd_warning ("No proxy to Logind has been set up!");
		return;
	}
	
	g_dbus_proxy_call (s_proxy, cAction, g_variant_new ("(b)", FALSE), // non-interactive
		G_DBUS_CALL_FLAGS_NO_AUTO_START | G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
		-1, NULL, NULL, NULL);
}


static void _shut_down_real (void)
{
	if (s_bCanStop)
	{
		switch (s_iLoginManager)
		{
			case CD_CONSOLE_KIT:
				_console_kit_action ("Stop");
				break;
			case CD_LOGIND:
				_logind_action ("PowerOff");
				break;
			default:
				break;
		}
	}
}

static void _shut_down (CairoDockFMConfirmationFunc cb_confirm, gpointer data)
{
	if (cb_confirm) cb_confirm (data, _shut_down_real);
	else _shut_down_real ();
}

static void _restart_real (void)
{
	if (s_bCanRestart)
	{
		switch (s_iLoginManager)
		{
			case CD_CONSOLE_KIT:
				_console_kit_action ("Restart");
				break;
			case CD_LOGIND:
				_logind_action ("Reboot");
				break;
			default:
				break;
		}
	}
}

static void _restart (CairoDockFMConfirmationFunc cb_confirm, gpointer data)
{
	if (cb_confirm) cb_confirm (data, _restart_real);
	else _restart_real ();
}

static void _suspend (void)
{
	if (!s_bCanSuspend) return;
	if (s_iLoginManager == CD_LOGIND)
		_logind_action ("Suspend");
}

static void _hibernate (void)
{
	if (!s_bCanHibernate) return;
	if (s_iLoginManager == CD_LOGIND)
		_logind_action ("Hibernate");
}

static void _hybridSleep (void)
{
	if (!s_bCanHybridSleep) return;
	if (s_iLoginManager == CD_LOGIND)
		_logind_action ("HybridSleep");
}

static void _logout_real (void)
{
	switch (s_iLogoutType)
	{
		case CD_WAYLAND:
			cairo_dock_launch_command_single ("wayland-logout");
			break;
		case CD_SYSTEMD:
		{
			if (!s_proxy)
			{
				cd_warning ("No proxy to Logind has been set up!");
				break;
			}
			
			// note: we already checked that we have a valid session ID
			const char *session_id = g_getenv ("XDG_SESSION_ID");
			g_dbus_proxy_call (s_proxy, "TerminateSession", g_variant_new ("(s)", session_id),
				G_DBUS_CALL_FLAGS_NO_AUTO_START | G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
				-1, NULL, NULL, NULL);
			
			break;
		}
		default:
			// should not happen, we only register our backend if one of the methods is known
			cd_warning ("Unknown logout method!");
			break;
	}
}

static void _logout (CairoDockFMConfirmationFunc cb_confirm, gpointer data)
{
	if (cb_confirm) cb_confirm (data, _logout_real);
	else _logout_real ();
}

static void _lock_screen (void) {
	if (!s_bCanLock) return;
	
	if (! gldi_container_is_wayland_backend ())
	{
		// On X11, we prefer xdg-screensaver
		gchar *tmp = g_find_program_in_path ("xdg-screensaver");
		if (tmp)
		{
			const char *args[] = {tmp, "lock", NULL};
			cairo_dock_launch_command_argv_full (args, NULL, GLDI_LAUNCH_SLICE);
			g_free (tmp);
			return;
		}
	}
	
	// Otherwise, we try to use Logind
	const gchar *session_id = g_getenv ("XDG_SESSION_ID");
	// note: we re-check everything to avoid the edge case when xdg-screensaver was uninstalled since we checked
	if (session_id && *session_id && s_proxy)
	{
		g_dbus_proxy_call (s_proxy, "LockSession", g_variant_new ("(s)", session_id),
			G_DBUS_CALL_FLAGS_NO_AUTO_START | G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
			-1, NULL, NULL, NULL);
	}
}


/*
 * Check if the method (cMethod) exists and returns 'yes' or 'challenge':
 * http://www.freedesktop.org/wiki/Software/systemd/logind/
 *  If "yes" is returned the operation is supported and the user may execute the
 *   operation without further authentication.
 *  If "challenge" is returned the operation is available, but only after
 *   authorization.
 * If it exists, *bIsAble is modified and TRUE is returned.
 */
static void _cd_logout_check_capabilities_logind (GDBusProxy *pProxy, const gchar *cMethod, gboolean *bIsAble)
{
	GError *error = NULL;
	GVariant *res = g_dbus_proxy_call_sync (pProxy, cMethod, NULL,
		G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &error);
	
	if (!error)
	{
		if (res && g_variant_is_of_type (res, G_VARIANT_TYPE ("(s)")))
		{
			GVariant *str = g_variant_get_child_value (res, 0);
			const gchar *cResult = g_variant_get_string (str, NULL);
			*bIsAble = (cResult && (strcmp (cResult, "yes") == 0
						|| strcmp (cResult, "challenge") == 0) );
			g_variant_unref (str);
		}
		else cd_warning ("Unexpected result from Logind!");
		
		g_variant_unref (res);
	}
	else
	{
		cd_warning ("Logind error: %s", error->message);
		g_error_free (error);
	}
}

static void _cd_logout_check_capabilities_async (G_GNUC_UNUSED gpointer ptr)
{
	// test first with LoginD
	s_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
		G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		NULL, // GDBusInterfaceInfo
		"org.freedesktop.login1",
		"/org/freedesktop/login1",
		"org.freedesktop.login1.Manager",
		NULL,
		NULL);
	
	if (s_proxy)
	{
		if (!g_dbus_proxy_get_name_owner (s_proxy))
		{
			g_object_unref (s_proxy);
			s_proxy = NULL;
		}
		else
		{
			s_iLoginManager = CD_LOGIND;
			const gchar *cLogindMethods[] = {"CanPowerOff", "CanReboot", "CanSuspend", "CanHibernate", "CanHybridSleep", NULL};
			gboolean *bCapabilities[] = {&s_bCanStop, &s_bCanRestart, &s_bCanSuspend, &s_bCanHibernate, &s_bCanHybridSleep};
			int i;
			for (i = 0; cLogindMethods[i] != NULL; i++)
				_cd_logout_check_capabilities_logind (s_proxy, cLogindMethods[i], bCapabilities[i]);
		}
	}


	if (!s_proxy) // then check with ConsoleKit
	{
		GError *error = NULL;

		// get capabilities from ConsoleKit.: reboot and poweroff
		s_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
			NULL, // GDBusInterfaceInfo
			"org.freedesktop.ConsoleKit",
			"/org/freedesktop/ConsoleKit/Manager",
			"org.freedesktop.ConsoleKit.Manager",
			NULL,
			NULL);
		
		if (s_proxy)
		{
			if (!g_dbus_proxy_get_name_owner (s_proxy))
			{
				g_object_unref (s_proxy);
				s_proxy = NULL;
			}
			else
			{
				GVariant *res = g_dbus_proxy_call_sync (s_proxy,
					"CanRestart",
					NULL, // parameters
					G_DBUS_CALL_FLAGS_NO_AUTO_START,
					-1,
					NULL,
					&error);
				if (error)
				{
					cd_warning ("ConsoleKit error: %s", error->message);
					g_error_free (error);
					g_object_unref (s_proxy);
					s_proxy = NULL;
				}
				else
				{
					s_iLoginManager = CD_CONSOLE_KIT;
					if (res && g_variant_is_of_type (res, G_VARIANT_TYPE ("(b)")))
						g_variant_get_child (res, 0, "b", &s_bCanRestart);
					g_variant_unref (res);
					
					res = g_dbus_proxy_call_sync (s_proxy,
						"CanStop",
						NULL, // parameters
						G_DBUS_CALL_FLAGS_NO_AUTO_START,
						-1,
						NULL,
						&error);
					if (error)
					{
						cd_warning ("ConsoleKit error: %s", error->message);
						g_error_free (error);
					}
					else
					{
						if (res && g_variant_is_of_type (res, G_VARIANT_TYPE ("(b)")))
							g_variant_get_child (res, 0, "b", &s_bCanStop);
						g_variant_unref (res);
					}
				}
			}
		}
	}
	
	// Check whether we can logout
	// first check if we are in a logind session -- we just need the session ID and then can use the DBus interface
	const gchar *session_id = g_getenv ("XDG_SESSION_ID");
	if (s_iLoginManager == CD_LOGIND && session_id && *session_id)
	{
		s_iLogoutType = CD_SYSTEMD;
		s_bCanLock = TRUE; // in this case, we can also lock the screen
	}
	else
	{
		// then check some alternatives
		if (g_getenv ("WAYLAND_DISPLAY") != NULL)
		{
			// check for wayland-logout and assume that it will work
			gchar *tmp = g_find_program_in_path ("wayland-logout");
			if (tmp != NULL)
			{
				g_free (tmp);
				s_iLogoutType = CD_WAYLAND;
			}
		}
		else
		{
			// check whether we can lock the screen -- this will only work on X11
			gchar *tmp = g_find_program_in_path ("xdg-screensaver");
			if (tmp)
			{
				s_bCanLock = TRUE; // we just assume it will work
				g_free (tmp);
			}
		}
	}
}

static void _toggle_network (void)
{
	g_return_if_fail (s_proxy_nm != NULL);
	
	GVariant *v = g_dbus_proxy_get_cached_property (s_proxy_nm, "NetworkingEnabled");
	if (!v) cd_warning ("Cannot read the 'NetworkingEnabled' property");
	else
	{
		if (!g_variant_is_of_type (v, G_VARIANT_TYPE ("b")))
			cd_warning ("Unexpected property type for 'NetworkingEnabled': %s", g_variant_get_type_string (v));
		else
		{
			gboolean bEnabled = g_variant_get_boolean (v);
			g_dbus_proxy_call (s_proxy_nm, "Enable", g_variant_new ("(b)", !bEnabled),
				G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION, -1,
				NULL, NULL, NULL);
		}
		
		g_variant_unref (v);
	}
}

static void _toggle_wifi (void)
{
	g_return_if_fail (s_proxy_nm != NULL);
	
	GVariant *v = g_dbus_proxy_get_cached_property (s_proxy_nm, "WirelessEnabled");
	if (!v) cd_warning ("Cannot read the 'WirelessEnabled' property");
	else
	{
		if (!g_variant_is_of_type (v, G_VARIANT_TYPE ("b")))
			cd_warning ("Unexpected property type for 'WirelessEnabled': %s", g_variant_get_type_string (v));
		else
		{
			gboolean bEnabled = g_variant_get_boolean (v);
			g_dbus_proxy_call (s_proxy_nm, "org.freedesktop.DBus.Properties.Set",
				g_variant_new ("(ssv)", "org.freedesktop.NetworkManager", "WirelessEnabled", g_variant_new_boolean(!bEnabled)),
				G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION, -1,
				NULL, NULL, NULL);
		}
		
		g_variant_unref (v);
	}
}

static void _register_backend (void)
{
	gboolean bAnyValid = FALSE;
	
	CairoDockDesktopEnvBackend backend = { NULL };
	if (s_bCanStop) { backend.shutdown = _shut_down; bAnyValid = TRUE; }
	if (s_bCanRestart) { backend.reboot = _restart; bAnyValid = TRUE; }
	if (s_bCanSuspend) { backend.suspend = _suspend; bAnyValid = TRUE; }
	if (s_bCanHibernate) { backend.hibernate = _hibernate; bAnyValid = TRUE; }
	if (s_bCanHybridSleep) { backend.hybrid_sleep = _hybridSleep; bAnyValid = TRUE; }
	if (s_iLogoutType != CD_LOGOUT_UNKNOWN) { backend.logout = _logout; bAnyValid = TRUE; }
	if (s_bCanLock) { backend.lock_screen = _lock_screen; bAnyValid = TRUE; }
	
	if (s_proxy_nm)
	{
		bAnyValid = TRUE;
		backend.toggle_network = _toggle_network;
		backend.toggle_wifi = _toggle_wifi;
	}
	
	if (!bAnyValid) return;
	
	// FALSE -- we do not want to overwrite the DE-specific functions already
	// registered by the *-integration plugins (since we run asynchronously,
	// they might be registered earlier)
	cairo_dock_fm_register_vfs_backend (&backend, FALSE);
}

static gboolean _cd_logout_got_capabilities (G_GNUC_UNUSED gpointer ptr)
{
	// fetch the capabilities.
	gldi_task_discard (s_pTask);
	s_pTask = NULL;
	
	if (s_bNmChecked) _register_backend ();
	
	return FALSE;
}

static void _got_nm_proxy (G_GNUC_UNUSED GObject *pObj, GAsyncResult *pRes, G_GNUC_UNUSED gpointer ptr)
{
	s_bNmChecked = TRUE;
	
	GError *err = NULL;
	s_proxy_nm = g_dbus_proxy_new_for_bus_finish (pRes, &err);
	if (err)
	{
		cd_warning ("Cannot create NetworkManager DBus proxy: %s", err->message);
		g_error_free (err);
	}
	else
	{
		gchar *cName = g_dbus_proxy_get_name_owner (s_proxy_nm);
		if (!cName)
		{
			// no NetworkManager, will not use it (it should be available already when the session is started, no need to watch the name)
			g_object_unref (G_OBJECT (s_proxy_nm));
			s_proxy_nm = NULL;
		}
		else g_free (cName);
	}
	
	if (!s_pTask) _register_backend ();
}


void gldi_logout_backend_init (void)
{
	if (s_bInitDone) return;
	s_bInitDone = TRUE;
	
	// Note: we check capabilities by looking at DBus interfaces which
	// can have a small latency. In theory, it would be nicer to just
	// use the async DBus API, but since we have to do many checks in
	// series, it is more convenient to use the sync functions and run
	// things in a task.
	s_pTask = gldi_task_new_full (0,
		(GldiGetDataAsyncFunc) _cd_logout_check_capabilities_async,
		(GldiUpdateSyncFunc) _cd_logout_got_capabilities,
		(GFreeFunc) NULL,
		NULL);
	// Note: we add 500 ms delay in case some services are not available
	// right away.
	gldi_task_launch_delayed (s_pTask, 500);
	
	// create NetworkManager proxy
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		NULL, // interface info
		"org.freedesktop.NetworkManager",
		"/org/freedesktop/NetworkManager",
		"org.freedesktop.NetworkManager",
		NULL, // cancellable
		_got_nm_proxy,
		NULL);
}


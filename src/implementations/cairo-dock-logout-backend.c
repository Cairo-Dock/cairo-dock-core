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
#include "cairo-dock-dbus.h"
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



static void _console_kit_action (const gchar *cAction)
{
	GError *error = NULL;
	DBusGProxy *pProxy = cairo_dock_create_new_system_proxy (
		"org.freedesktop.ConsoleKit",
		"/org/freedesktop/ConsoleKit/Manager",
		"org.freedesktop.ConsoleKit.Manager");
	
	dbus_g_proxy_call (pProxy, cAction, &error,
		G_TYPE_INVALID,
		G_TYPE_INVALID);
	if (error)
	{
		cd_warning ("ConsoleKit error: %s", error->message);
		g_error_free (error);
	}
	g_object_unref (pProxy);
}

static void _logind_action (const gchar *cAction)
{
	GError *error = NULL;
	DBusGProxy *pProxy = cairo_dock_create_new_system_proxy (
		"org.freedesktop.login1",
		"/org/freedesktop/login1",
		"org.freedesktop.login1.Manager");
	
	dbus_g_proxy_call (pProxy, cAction, &error,
		G_TYPE_BOOLEAN, FALSE,  // non-interactive
		G_TYPE_INVALID,
		G_TYPE_INVALID);
	if (error)
	{
		cd_warning ("Logind error: %s", error->message);
		gchar *cMessage = g_strdup_printf ("%s %s\n%s",
			_("Logind has returned this error:"),
			error->message,
			_("Please check that you can do this action\n"
				"(e.g. you can't power the computer off if another session is active)"));
		gldi_dialog_show_general_message (cMessage, 15e3);
		g_free (cMessage);
		g_error_free (error);
	}
	g_object_unref (pProxy);
}


static void _shut_down (void)
{
	// gldi_object_notify (&myModuleObjectMgr, NOTIFICATION_LOGOUT); -- TODO: move to fm !!
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

static void _restart (void)
{
	// gldi_object_notify (&myModuleObjectMgr, NOTIFICATION_LOGOUT); -- TODO: move to fm !!
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

static void _logout (void)
{
	switch (s_iLogoutType)
	{
		case CD_WAYLAND:
			cairo_dock_launch_command_single ("wayland-logout");
			break;
		case CD_SYSTEMD:
		{
			// TODO: we should use the DBus interface directly!
			const char * const cmd[] = {"systemctl", "--user", "stop", "graphical-session.target", NULL};
			cairo_dock_launch_command_argv (cmd);
			break;
		}
		default:
			// should not happen, we only register our backend if one of the methods is known
			cd_warning ("Unknown logout method!");
			break;
	}
}

static void _lock_screen (void) {
	if (!s_bCanLock) return;
	const char *args[] = {NULL, NULL, NULL};
	if (gldi_container_is_wayland_backend ())
	{
		// xdg-screensaver does not work on (most) Wayland compositors
		// we use loginctl and hope it works
		args[0] = "loginctl";
		args[1] = "lock-session";
		// will be transient (basically just calls the DBus method, we could do it ourselves as well)
		cairo_dock_launch_command_argv (args);
	}
	else
	{
		args[0] = "xdg-screensaver";
		args[1] = "lock";
		cairo_dock_launch_command_argv_full (args, NULL, GLDI_LAUNCH_SLICE);
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
static gboolean _cd_logout_check_capabilities_logind (DBusGProxy *pProxy, const gchar *cMethod, gboolean *bIsAble)
{
	GError *error = NULL;
	gchar *cResult = NULL;
	dbus_g_proxy_call (pProxy, cMethod, &error,
		G_TYPE_INVALID,
		G_TYPE_STRING, &cResult,
		G_TYPE_INVALID);
	if (!error)
	{
		*bIsAble = (cResult && (strcmp (cResult, "yes") == 0
		            || strcmp (cResult, "challenge") == 0));
		g_free (cResult);
	}
	else
	{
		cd_debug ("Logind error: %s", error->message);
		g_error_free (error);
		return FALSE;
	}
	return TRUE;
}

static void _cd_logout_check_capabilities_async (G_GNUC_UNUSED gpointer ptr)
{
	// test first with LoginD
	DBusGProxy *pProxy = cairo_dock_create_new_system_proxy (
		"org.freedesktop.login1",
		"/org/freedesktop/login1",
		"org.freedesktop.login1.Manager");

	const gchar *cLogindMethods[] = {"CanPowerOff", "CanReboot", "CanSuspend", "CanHibernate", "CanHybridSleep", NULL};
	gboolean *bCapabilities[] = {&s_bCanStop,
		&s_bCanRestart, &s_bCanSuspend,
		&s_bCanHibernate, &s_bCanHybridSleep};

	if (pProxy && _cd_logout_check_capabilities_logind (pProxy, cLogindMethods[0], bCapabilities[0]))
	{
		s_iLoginManager = CD_LOGIND;
		for (int i = 1; cLogindMethods[i] != NULL; i++)
			_cd_logout_check_capabilities_logind (pProxy, cLogindMethods[i], bCapabilities[i]);

		g_object_unref (pProxy);
	}
	else // then check with ConsoleKit
	{
		GError *error = NULL;

		// get capabilities from ConsoleKit.: reboot and poweroff
		pProxy = cairo_dock_create_new_system_proxy (
			"org.freedesktop.ConsoleKit",
			"/org/freedesktop/ConsoleKit/Manager",
			"org.freedesktop.ConsoleKit.Manager");
		
		dbus_g_proxy_call (pProxy, "CanRestart", &error,
			G_TYPE_INVALID,
			G_TYPE_BOOLEAN, &s_bCanRestart,
			G_TYPE_INVALID);
		if (!error)
		{
			s_iLoginManager = CD_CONSOLE_KIT;
			
			dbus_g_proxy_call (pProxy, "CanStop", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &s_bCanStop,
				G_TYPE_INVALID);
			if (error)
			{
				cd_warning ("ConsoleKit error: %s", error->message);
				g_error_free (error);
			}
		}
		else
		{
			cd_debug ("ConsoleKit error: %s", error->message);
			g_error_free (error);
		}
		g_object_unref (pProxy);
	}
	
	
	// Check whether we can logout
	
	const char *args[] = {NULL, NULL, NULL, NULL, NULL};
	// 1. check if we are in a systemd graphical session -- note: this should actually be handled by logind above, we just would need to get the session ID
	gchar *tmp2 = g_find_program_in_path ("systemctl");
	if (tmp2 != NULL)
	{
		g_free (tmp2);
		args[0] = "systemctl";
		args[1] = "--user";
		args[2] = "is-active";
		args[3] = "graphical-session.target";
		tmp2 = cairo_dock_launch_command_argv_sync_with_stderr (args, FALSE);
		if (tmp2 != NULL)
		{
			int r = strcmp(tmp2, "active");
			g_free (tmp2);
			if (!r) s_iLogoutType = CD_SYSTEMD;
		}
	}
	// 2. Wayland specific
	if (s_iLogoutType == CD_LOGOUT_UNKNOWN && g_getenv ("WAYLAND_DISPLAY") != NULL)
	{
		// check for wayland-logout and assume that it will work
		gchar *tmp = g_find_program_in_path ("wayland-logout");
		if (tmp != NULL)
		{
			g_free (tmp);
			s_iLogoutType = CD_WAYLAND;
		}
	}
	
	// check whether we can lock the screen
	if (g_getenv ("WAYLAND_DISPLAY") != NULL)
	{
		if (s_iLogoutType == CD_SYSTEMD)
		{
			gchar *tmp = g_find_program_in_path ("loginctl");
			if (tmp)
			{
				s_bCanLock = TRUE; // we just assume it will work (often not the case though)
				g_free (tmp);
			}
		}
	}
	else
	{
		gchar *tmp = g_find_program_in_path ("xdg-screensaver");
		if (tmp)
		{
			s_bCanLock = TRUE; // we just assume it will work
			g_free (tmp);
		}
	}
}

static gboolean _cd_logout_got_capabilities (G_GNUC_UNUSED gpointer ptr)
{
	// fetch the capabilities.
	gldi_task_discard (s_pTask);
	s_pTask = NULL;
	
	CairoDockDesktopEnvBackend backend = { NULL };
	if (s_bCanStop) backend.shutdown = _shut_down;
	if (s_bCanRestart) backend.reboot = _restart;
	if (s_bCanSuspend) backend.suspend = _suspend;
	if (s_bCanHibernate) backend.hibernate = _hibernate;
	if (s_bCanHybridSleep) backend.hybrid_sleep = _hybridSleep;
	if (s_iLogoutType != CD_LOGOUT_UNKNOWN) backend.logout = _logout;
	if (s_bCanLock) backend.lock_screen = _lock_screen;
	
	// FALSE -- we do not want to overwrite the DE-specific functions already
	// registered by the *-integration plugins (since we run asynchronously,
	// they might be registered earlier)
	cairo_dock_fm_register_vfs_backend (&backend, FALSE);
	
	return FALSE;
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
}


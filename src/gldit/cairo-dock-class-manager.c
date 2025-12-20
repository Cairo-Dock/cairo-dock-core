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

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <cairo.h>

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_remove_version_from_string
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_notify_startup
#include "cairo-dock-module-manager.h"  // GldiModule
#include "cairo-dock-module-instance-manager.h"  // GldiModuleInstance
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-indicator-manager.h"  // myIndicatorsParam.bUseClassIndic
#include "cairo-dock-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-desktop-file-db.h"
#include "cairo-dock-class-manager.h"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopEnv g_iDesktopEnv;

// global config (for debugging only)
gboolean g_bDisableDbusActivation = FALSE;
gboolean g_bGioLaunch = FALSE;

static GldiObjectManager myAppInfoObjectMgr;

/// Definition of a Class of application.
struct _CairoDockClassAppli {
	/// TRUE if the appli must use the icon provided by X instead the one from the theme.
	gboolean bUseXIcon;
	/// TRUE if the appli doesn't group togather with its class.
	gboolean bExpand;
	/// List of the inhibitors of the class.
	GList *pIconsOfClass;
	/// List of the appli icons of this class.
	GList *pAppliOfClass;
	gboolean bSearchedAttributes;
	/// Class of the app as reported by the WM / compositor without parsing or any changes to it
	/// or alternatively, the custom class set for our launchers
	gchar *cStartupWMClass;
	gchar *cIcon;
	gchar *cName;
	gint iAge;  // age of the first created window of this class
	gchar *cDockName;  // unique name of the class sub-dock
	guint iSidOpeningTimeout;  // timeout to stop the launching, if not stopped by the application before
	gboolean bIsLaunching;  // flag to mark a class as being launched
	gboolean bHasStartupNotify;  // TRUE if the application sends a "remove" event when its launch is complete (not used yet)
	GldiAppInfo *app; // contains the desktop file opened by us
};

typedef struct _CairoDockClassAppli CairoDockClassAppli;


static GHashTable *s_hClassTable = NULL;
static GHashTable *s_hAltClass = NULL; // we store alternative class / app-ids here

static gchar *_cairo_dock_register_class_full (const gchar *cSearchTerm, const gchar *cFallbackClass, const gchar *cWmClass,
	gboolean bUseWmClass, gboolean bCreateAlways, gboolean bIsDesktopFile, GDesktopAppInfo *app, CairoDockClassAppli **pResult);

static void _cairo_dock_free_class_appli (CairoDockClassAppli *pClassAppli)
{
	g_list_free (pClassAppli->pIconsOfClass);
	g_list_free (pClassAppli->pAppliOfClass);
	g_free (pClassAppli->cName);
	g_free (pClassAppli->cIcon);
	g_free (pClassAppli->cStartupWMClass);
	if (pClassAppli->iSidOpeningTimeout != 0)
		g_source_remove (pClassAppli->iSidOpeningTimeout);
	if (pClassAppli->app) gldi_object_unref (GLDI_OBJECT (pClassAppli->app));
	g_free (pClassAppli);
}

static inline CairoDockClassAppli *_cairo_dock_lookup_class_appli (const gchar *cClass)
{
	gpointer ret = NULL;
	if (cClass)
	{
		ret = g_hash_table_lookup (s_hClassTable, cClass);
		if (!ret) ret = g_hash_table_lookup (s_hAltClass, cClass);
	}
	return ret;
}

static gboolean _on_window_created (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	gldi_class_startup_notify_end (actor->cClass);

	return GLDI_NOTIFICATION_LET_PASS;
}
static gboolean _on_window_activated (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	if (! actor)
		return GLDI_NOTIFICATION_LET_PASS;

	gldi_class_startup_notify_end (actor->cClass);

	return GLDI_NOTIFICATION_LET_PASS;
}


/***********************************************************************
 * GldiAppInfo */

/** Helper object for launching apps. This is needed as unfortunately
 * GDesktopAppInfo does not provide all the functionality we need.
 * Note: typedefed to GldiAppInfo in cairo-dock-struct.h, but all
 * fields are private and should be used via accessors.
 */
struct _GldiAppInfo {
	GldiObject object;
	GAppInfo *app; // the desktop app info -- this object holds one reference to it
	const gchar* const *actions; // additional actions supported by this app (belongs to app, no need to free)
	gchar ***action_args; // parsed Exec key to launch actions
	gchar **args; // parsed Exec key for launching the main app
	int args_file_pos; // position of files / URIs in args or -1 if not found
	gchar *cWorkingDir; // working directory to start the app in (need to store here since we cannot grab this from app)
	gboolean bNeedsTerminal; // whether this app needs to be launched in a terminal
	gboolean bCustomCmd; // if TRUE, args[0] is a full command line, to be run in a shell (i.e. with sh -c) after appending filenames
	//!! TODO: each action can have its own icon
};


struct _GldiTerminal {
	const char * const args[3];
	CairoDockDesktopEnv iDesktopEnv;
	gboolean bOnlyWayland;
};

static const struct _GldiTerminal s_vTerminals[] = {
	{ { "kgx", "-e", NULL }, CAIRO_DOCK_GNOME, FALSE }, // new replacement for gnome-terminal
	{ { "gnome-terminal", "--", NULL }, CAIRO_DOCK_GNOME, FALSE },
	{ { "konsole", "-e", NULL }, CAIRO_DOCK_KDE, FALSE },
	{ { "xfce4-terminal", "-x", NULL }, CAIRO_DOCK_XFCE, FALSE },
	{ { "mate-terminal", "-x", NULL }, CAIRO_DOCK_GNOME, FALSE }, // note: MATE likely sets desktop environment as GNOME
	{ { "foot", NULL, NULL }, CAIRO_DOCK_UNKNOWN_ENV, TRUE },
	{ { "sakura", "-e", NULL }, CAIRO_DOCK_UNKNOWN_ENV, FALSE },
	{ { "alacritty", "-e", NULL }, CAIRO_DOCK_UNKNOWN_ENV, FALSE },
	{ { "kitty", NULL, NULL }, CAIRO_DOCK_UNKNOWN_ENV, FALSE },
	{ { "rxvt", "-e", NULL }, CAIRO_DOCK_UNKNOWN_ENV, FALSE },
	{ { "xterm", "-e", NULL }, CAIRO_DOCK_UNKNOWN_ENV, FALSE },
	{ { "x-terminal-emulator", "-e", NULL }, CAIRO_DOCK_UNKNOWN_ENV, FALSE }, // last one, since the "-e" option might not work
	{ { NULL, NULL, NULL }, CAIRO_DOCK_UNKNOWN_ENV, FALSE }
};

// which terminal to use when an app needs one, selected the first time it is needed
// (note: should be made a user-visible setting)
int s_iTerminal = -1;

static void _choose_terminal (void)
{
	if (s_iTerminal >= 0) return;
	
	int iFirst = -1; // first valid terminal found, regardless of the desktop env
	for (s_iTerminal = 0; s_vTerminals[s_iTerminal].args[0] != NULL; ++s_iTerminal)
	{
		if (s_vTerminals[s_iTerminal].bOnlyWayland && ! gldi_container_is_wayland_backend ())
			continue;
		gchar *tmp = g_find_program_in_path (s_vTerminals[s_iTerminal].args[0]);
		if (tmp)
		{
			g_free (tmp);
			// if we don't know the environment, we take any terminal
			if (g_iDesktopEnv == CAIRO_DOCK_UNKNOWN_ENV) break;
			
			if (iFirst == -1) iFirst = s_iTerminal;
			
			if (s_vTerminals[s_iTerminal].iDesktopEnv == g_iDesktopEnv)
			{
				iFirst = -1;
				break;
			}
		}
	}
	
	if (iFirst >= 0) s_iTerminal = iFirst; // reset to the first one found if there is no better match
}


static gchar **_process_cmdline (const gchar *cCmdline, gboolean bKeepFiles, int *pFilePos, GAppInfo *app)
{
	int args_len;
	int file_pos = -1;
	gchar **tmp = NULL;
	GError *err = NULL;
	
	gboolean res = g_shell_parse_argv (cCmdline, &args_len, &tmp, &err);
	if (!res)
	{
		cd_warning ("cannot parse command line: %s\n", err->message);
		g_error_free (err);
		return NULL;
	}
	
	// process useful substitutions and remove the rest
	int j = 0, k = 0;
	for (; j < args_len; j++)
	{
		gboolean bKeep = TRUE;
		gboolean bParsed = FALSE;
		/*
		 * Note: we assume that a useful escape is always preceded by whitespace,
		 * so we need to only check the first character of each argument. This is
		 * consistent with the standard that says here
		 * https://specifications.freedesktop.org/desktop-entry-spec/latest/exec-variables.html:
		 * "Field codes must not be used inside a quoted argument, the result of
		 * field code expansion inside a quoted argument is undefined. The %F
		 * and %U field codes may only be used as an argument on their own."
		 * 
		 * However, this is not fully adhered to, as e.g. Zotero has the following:
		 * Exec=bash -c "$(dirname $(realpath $(echo %k | sed -e 's/^file:\\/\\///')))/zotero -url %U"
		 * Currently, this will not work, but eventually we would want to support
		 * replacing some escapes in the middle of arguments. While this specific
		 * form is likely rare, other apps might also use "bash -c" or similar,
		 * with the file / URI arguments quoted.
		 */
		if (tmp[j][0] == '%' && tmp[j][1] != '%') switch (tmp[j][1])
		{
			case 'f':
			case 'F':
			case 'u':
			case 'U':
				// files or URIs, will be processed when launching the app
				if (file_pos == -1)
				{
					bKeep = bKeepFiles;
					bParsed = TRUE;
					file_pos = j;
				}
				else bKeep = FALSE; // only one %f / %F / %u / %U argument supported
				break;
			case 'd':
			case 'D':
			case 'n':
			case 'N':
			case 'v':
			case 'm':
				// deprecated escapes, we should remove them
				bKeep = FALSE;
				break;
			case 'i':
				// should expand to two arguments: --icon [iconname],
				// but does not seem to be used, so maybe it's OK to ignore
				bKeep = FALSE;
				break;
			case 'c':
				// name of the app
				g_free (tmp[j]);
				tmp[j] = g_strdup (g_app_info_get_name (app));
				bKeep = (tmp[j] != NULL); // note: we should always have a name
				bParsed = TRUE;
				break;
			case 'k':
				// path to the .desktop file (if available)
				bKeep = FALSE;
				if (G_IS_DESKTOP_APP_INFO (app))
				{
					GDesktopAppInfo *desktop_app = G_DESKTOP_APP_INFO (app);
					const gchar *path = g_desktop_app_info_get_filename (desktop_app);
					if (path)
					{
						g_free (tmp[j]);
						tmp[j] = g_strdup (path);
						bKeep = TRUE;
						bParsed = TRUE;
					}
				}
				break;
			default:
				// according to the spec, having an unknown field is an error -- we silently remove it
				bKeep = FALSE;
				break;
		}
		
		if (bKeep)
		{
			if (!bParsed)
			{
				// convert "%%" to "%"
				int x = 0, y = 0;
				for (; tmp[j][y]; x++, y++)
				{
					if (tmp[j][y] == '%' && tmp[j][y+1] == '%') y++;
					if (x != y) tmp[j][x] = tmp[j][y];
				}
				tmp[j][x] = 0;
			}
			if (j != k) tmp[k] = tmp[j];
			k++;
		}
		else g_free (tmp[j]); // delete this arg
	}
	
	if (!k)
	{
		// no valid argument
		cd_warning ("cannot parse command line: empty or invalid arguments\n");
		g_free (tmp); // in this case, all elements in ret were freed or were originally NULL
		return NULL;
	}
	
	// mark the deleted arguments as NULL
	for (; k < args_len; k++) tmp[k] = NULL;
	
	if (pFilePos) *pFilePos = file_pos;
	
	return tmp;
}

struct _GldiAppInfoAttr {
	GAppInfo *app;
	const gchar *cCmdline;
	gboolean bNeedsTerminal;
	const gchar *cWorkingDir;
};

static void _init_appinfo (GldiObject *obj, gpointer attr)
{
	GldiAppInfo *info = (GldiAppInfo*)obj;
	struct _GldiAppInfoAttr *params = (struct _GldiAppInfoAttr*)attr;
	info->app = params->app;
	g_object_ref (info->app);
	
	GDesktopAppInfo *desktop_app = NULL;
	gboolean bDbusActivatable = FALSE;
	
	//\__________________ check for the additional actions supported by this app
	if (G_IS_DESKTOP_APP_INFO (info->app))
	{
		desktop_app = G_DESKTOP_APP_INFO (info->app);
		info->actions = g_desktop_app_info_list_actions (desktop_app);
		if (!g_bDisableDbusActivation)
			bDbusActivatable = g_desktop_app_info_get_boolean (desktop_app, "DBusActivatable");
	}
	if (g_bGioLaunch) bDbusActivatable = TRUE; // this will skip parsing the command line and force using the Gio function to launch this app
	
	if (params->bNeedsTerminal) info->bNeedsTerminal = TRUE;
	else if (desktop_app) info->bNeedsTerminal = g_desktop_app_info_get_boolean (desktop_app, "Terminal");
	
	if (params->cWorkingDir) info->cWorkingDir = g_strdup (params->cWorkingDir);
	else if (desktop_app) info->cWorkingDir = g_desktop_app_info_get_string (desktop_app, "Path");
	
	if (! bDbusActivatable)
	{
		// parse the command lines to launch this app if not DBus activated
		if (params->cCmdline)
		{
			// this is a user supplied command, we just run it with sh -c
			info->args = g_new0 (char*, 2);
			info->args[0] = g_strdup (params->cCmdline);
			info->bCustomCmd = TRUE;
		}
		else
		{
			const gchar *cCmdline = g_app_info_get_commandline (info->app);
			if (!cCmdline)
			{
				const char *id = g_app_info_get_id (info->app);
				cd_warning ("Cannot get value of Exec= key for app: %s", id ? id : "(unknown)");
				g_object_unref (info->app);
				info->app = NULL;
				return;
			}
			info->args = _process_cmdline (cCmdline, TRUE, &(info->args_file_pos), info->app);
			if (!info->args)
			{
				// warning already shown in _process_cmdline, just bail out
				// we mark app as empty for the caller to detect failure
				g_object_unref (info->app);
				info->app = NULL;
				return;
			}
		}
		
		// parse additional actions
		if (info->actions)
		{
			// note: we need to reopen the .desktop file ourselves, as g_desktop_app_info_get_string () does not allow specifying a section
			const char *cDesktopFilePath = g_desktop_app_info_get_filename (desktop_app); // in this case, desktop_app != NULL
			GKeyFile *keyfile = g_key_file_new ();
			GError *err = NULL;
			if (!g_key_file_load_from_file (keyfile, cDesktopFilePath, G_KEY_FILE_NONE, &err))
			{
				cd_warning ("cannot read extra actions from desktop file: %s (%s)\n", cDesktopFilePath, err->message);
				g_error_free (err);
				info->actions = NULL;
			}
			else
			{
				unsigned int i, n_actions = g_strv_length ((gchar**)info->actions);
				info->action_args = g_new0 (gchar**, n_actions + 1);
				
				for (i = 0; i < n_actions; i++)
				{
					gchar *group = g_strdup_printf ("Desktop Action %s", info->actions[i]);
					gchar *exec  = g_key_file_get_string (keyfile, group, "Exec", &err);
					g_free (group);
					if (!exec)
					{
						cd_warning ("cannot read extra action \"%s\" from desktop file: %s (%s)\n",
							info->actions[i], cDesktopFilePath, err->message);
						g_error_free (err);
						info->actions = NULL;
						break;
					}
					
					info->action_args[i] = _process_cmdline (exec, FALSE, NULL, info->app);
					g_free (exec);
					if (!info->action_args[i])
					{
						// warning was already shown in _process_cmdline
						// no need to free action_args (it will be done when info is freed),
						// just mark that we do not have desktop actions
						info->actions = NULL;
						break;
					}
				}
			}
		}
	}
}

static void _reset_appinfo (GldiObject *obj)
{
	GldiAppInfo *info = (GldiAppInfo*)obj;
	if (info->app) g_object_unref (info->app);
	if (info->action_args)
	{
		gchar ***tmp = info->action_args;
		for (; *tmp; ++tmp) g_strfreev (*tmp);
		g_free (info->action_args);
	}
	g_strfreev (info->args);
	g_free (info->cWorkingDir);
}

GldiAppInfo *gldi_app_info_new_from_commandline (const gchar *cCmdline, const gchar *name, const gchar *cWorkingDir, gboolean bNeedsTerminal)
{
	GError *err = NULL;
	struct _GldiAppInfoAttr attr;
	attr.app = g_app_info_create_from_commandline (cCmdline, name, G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION |
		(bNeedsTerminal ? G_APP_INFO_CREATE_NEEDS_TERMINAL : 0), &err);
	if (!attr.app)
	{
		cd_warning (err->message);
		g_error_free (err);
		return NULL;
	}
	
	attr.cCmdline = cCmdline;
	attr.bNeedsTerminal = bNeedsTerminal;
	attr.cWorkingDir = cWorkingDir;
	GldiAppInfo *ret = (GldiAppInfo*) gldi_object_new (&myAppInfoObjectMgr, &attr); // will ref app
	g_object_unref (attr.app);
	if (!ret->app)
	{
		// failed to parse commandline, return NULL
		gldi_object_unref (GLDI_OBJECT (ret));
		return NULL;
	}
	return ret;
}

void gldi_app_info_launch_action (GldiAppInfo *app, const gchar *cAction)
{
	g_return_if_fail (app && app->actions && cAction);
	if (app->action_args)
	{
		// find the action and launch ourselves
		unsigned int i;
		for (i = 0; app->actions[i] && app->action_args[i]; i++)
		{
			if (!strcmp (app->actions[i], cAction))
			{
				cairo_dock_launch_command_argv_full2 ((const gchar * const*)app->action_args[i], app->cWorkingDir,
					GLDI_LAUNCH_GUI | GLDI_LAUNCH_SLICE, app->app);
				break;
			}
		}
	}
	else
	{
		g_return_if_fail (app->app && G_IS_DESKTOP_APP_INFO (app->app));
		// this app supports DBus activation, use it directly
		GdkAppLaunchContext *context = gdk_display_get_app_launch_context (gdk_display_get_default ());
		g_desktop_app_info_launch_action (G_DESKTOP_APP_INFO (app->app), cAction, G_APP_LAUNCH_CONTEXT (context));
		g_object_unref (context);
	}
}

static const gchar *_uri_to_file (const gchar *cUri, gchar **to_free)
{
	if (strstr (cUri, "://"))
	{
		gchar *tmp = NULL;
		GFile *file = g_file_new_for_uri (cUri);
		if (file)
		{
			tmp = g_file_get_path (file);
			g_object_unref (file);
			
			if (tmp)
			{
				*to_free = tmp;
				return tmp;
			}
		}
	}
	return cUri;
}

void gldi_app_info_launch (GldiAppInfo *app, const gchar* const *uris)
{
	g_return_if_fail (app && (app->args || app->app));
	if (app->args)
	{
		int n_args = 0;
		int n_uris = 0;
		int n_term = 0;
		
		{
			gchar **tmp1;
			const gchar * const *tmp2;
			for (tmp1 = app->args; *tmp1; ++tmp1) ++n_args;
			if (uris) for (tmp2 = uris; *tmp2; ++tmp2) ++n_uris;
		}
		
		if (app->bNeedsTerminal)
		{
			_choose_terminal ();
			if (s_vTerminals[s_iTerminal].args[0] != NULL)
			{
				const char * const *tmp2;
				for (tmp2 = s_vTerminals[s_iTerminal].args; *tmp2; ++tmp2) n_term++;
			}
		}
		
		if (app->bCustomCmd)
		{
			// command specified by the user, we launch it with just sh -c
			const char *cmd = app->args[0];
			char *to_free = NULL;
			int i;
			
			if (n_uris)
			{
				// append URIs at the end of the command -- this might not work, but we cannot do any better
				GString *str = g_string_new (cmd);
				for (i = 0; i < n_uris; i++)
				{
					char *tmp = g_shell_quote (uris[i]);
					g_string_append_printf (str, " %s", tmp);
					g_free (tmp);
				}
				to_free = g_string_free (str, FALSE);
				cmd = to_free;
			}
			
			const char **args = g_new0 (const char*, 4 + n_term);
			for (i = 0; i < n_term; i++)
				args[i] = s_vTerminals[s_iTerminal].args[i];
			args[i] = "sh";
			args[i+1] = "-c";
			args[i+2] = cmd;
			// note: there will still be a NULL-terminator left in args
			cairo_dock_launch_command_argv_full2 (args, app->cWorkingDir, GLDI_LAUNCH_GUI | GLDI_LAUNCH_SLICE, app->app);
			
			g_free (args);
			g_free (to_free);
			return;
		}
		
		// slight optimization: if the last arg is for files, we do not need to allocate a new array
		if (!n_term && n_uris <= 1 && app->args_file_pos > 0 && app->args_file_pos == n_args - 1)
		{
			// note: n_args > 0, checked when creating app->args
			int x = n_args - 1;
			char *tmp2 = app->args[x];
			char *to_free = NULL;
			if (uris) 
			{
				if (app->args[x][1] == 'f' || app->args[x][1] == 'F')
					app->args[x] = (char*)_uri_to_file (uris[0], &to_free); // try to convert URI to path
				else app->args[x] = (char*)uris[0];
			}
			else app->args[x] = NULL;
			cairo_dock_launch_command_argv_full2 ((const gchar * const*)app->args, app->cWorkingDir,
				GLDI_LAUNCH_GUI | GLDI_LAUNCH_SLICE, app->app);
			
			app->args[x] = tmp2;
			g_free (to_free);
		}
		else
		{
			const char **args = g_new0 (const char*, n_term + n_args + n_uris + 1);
			char **args_to_free = g_new0 (char*, n_uris + 1);
			gboolean bConvertUris = FALSE;
			
			int i = 0, j, k = 0;
			for (; i < n_term; i++)
				args[i] = s_vTerminals[s_iTerminal].args[i];
			
			for (j = 0; j < n_args; j++)
			{
				gboolean bKeep = TRUE;
				if (app->args_file_pos > 0 && app->args_file_pos == j)
				{
					const gchar *tmp = app->args[j];
					if (tmp[0] != '%')
						cd_warning ("Unexpected argument: %s", tmp);
					else switch (tmp[1])
					{
						case 'f':
							if (uris && *uris)
							{
								args[i] = _uri_to_file (*uris, args_to_free + k);
								if (args_to_free[k]) k++;
								i++;
								uris++;
							}
							bKeep = FALSE;
							bConvertUris = TRUE;
							break;
						case 'u':
							// one URI
							if (uris && *uris)
							{
								args[i] = *uris;
								i++;
								uris++;
							}
							bKeep = FALSE;
							break;
						case 'F':
							if (uris) for (; *uris; ++uris)
							{
								args[i] = _uri_to_file (*uris, args_to_free + k);
								if (args_to_free[k]) k++;
								i++;
							}
							bKeep = FALSE;
							break;
						case 'U':
							// take all URIs
							if (uris) for (; *uris; ++uris)
							{
								args[i] = *uris;
								i++;
							}
							bKeep = FALSE;
							break;
						default:
							cd_warning ("Unexpected argument: %s", tmp);
							break;
					}
				}
				if (bKeep)
				{
					args[i] = app->args[j];
					i++;
				}
			}
			
			// add remaining files / uris at the end
			if (uris) for (; *uris; ++uris, ++i)
			{
				if (bConvertUris)
				{
					args[i] = _uri_to_file (*uris, args_to_free + k);
					if (args_to_free[k]) k++;
				}
				else args[i] = *uris;
			}
			// note: there will still be a NULL-terminator left in args
			cairo_dock_launch_command_argv_full2 (args, app->cWorkingDir, GLDI_LAUNCH_GUI | GLDI_LAUNCH_SLICE, app->app);
			g_free (args);
			g_strfreev (args_to_free);
		}
	}
	else
	{
		// this is a DBus activated app, we can just use the GAppInfo functions for starting it
		GList *list = NULL;
		GList *list_to_free = NULL;
		if (uris) 
		{
			const gchar * const *tmp;
			for (tmp = uris; *tmp; ++tmp)
			{
				// note: we need to convert filenames to URIs (this is what g_desktop_app_info_launch () would do as well)
				if (tmp[0][0] == '/' || (strstr(*tmp, "://") == NULL))
				{
					GFile *file = g_file_new_for_path (*tmp);
					gchar *cURI = file ? g_file_get_uri (file) : NULL;
					if (cURI)
					{
						list = g_list_prepend (list, (void*)cURI);
						list_to_free = g_list_prepend (list_to_free, (void*)cURI);
					}
					// TODO: add the original value as that might still be useful?
					if (file) g_object_unref (file);
				}
				else list = g_list_prepend (list, (void*)*tmp);
			}
			list = g_list_reverse (list);
		}
		GdkAppLaunchContext *context = gdk_display_get_app_launch_context (gdk_display_get_default ());
		g_app_info_launch_uris (app->app, list, G_APP_LAUNCH_CONTEXT (context), NULL);
		g_list_free (list);
		g_list_free_full (list_to_free, g_free);
		g_object_unref (context);
	}
}

const gchar * const *gldi_app_info_get_desktop_actions (GldiAppInfo *app)
{
	return app ? ((const gchar * const *)app->actions) : NULL;
}

gchar *gldi_app_info_get_desktop_action_name (GldiAppInfo *app, const gchar *cAction)
{
	g_return_val_if_fail (app && app->actions && G_IS_DESKTOP_APP_INFO (app->app), NULL);
	return g_desktop_app_info_get_action_name (G_DESKTOP_APP_INFO (app->app), cAction);
}

const gchar** gldi_app_info_get_supported_types (GldiAppInfo *app)
{
	return (app && app->app) ? g_app_info_get_supported_types (app->app) : NULL;
}

GldiAppInfo *gldi_app_info_from_desktop_app_info (GDesktopAppInfo *pDesktopAppInfo)
{
	CairoDockClassAppli *pClass = NULL;
	gchar* cClass = _cairo_dock_register_class_full (NULL, NULL, NULL, FALSE, FALSE, TRUE, pDesktopAppInfo, &pClass);
	if (!cClass)
	{
		cd_warning ("Error processing desktop app!");
		return NULL;
	}
	
	if (!pClass->app)
	{
		cd_warning ("Class %s already registered, but has no GldiAppInfo");
	}
	else gldi_object_ref (GLDI_OBJECT (pClass->app));
	
	g_free (cClass); // TODO: find a way to avoid useless alloc and dealloc here
	return pClass->app;
}

void gldi_launch_desktop_app_info (GDesktopAppInfo *pDesktopAppInfo, const gchar* const *uris)
{
	GldiAppInfo *app = gldi_app_info_from_desktop_app_info (pDesktopAppInfo);
	if (app)
	{
		gldi_app_info_launch (app, uris);
		gldi_object_unref (GLDI_OBJECT (app));
	}
	else
	{
		// warning already shown in gldi_app_info_from_desktop_app_info ()
		// TODO: should we use g_app_info_launch () as a fallback?
	}
}

void gldi_app_info_set_run_in_terminal (GldiAppInfo *app, gboolean bNeedsTerminal)
{
	app->bNeedsTerminal = bNeedsTerminal;
}

/***********************************************************************
 * rest of class manager */
void cairo_dock_initialize_class_manager (void)
{
	gldi_desktop_file_db_init ();
	if (s_hClassTable == NULL)
		s_hClassTable = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			g_free,
			(GDestroyNotify) _cairo_dock_free_class_appli);
	if (s_hAltClass == NULL)
		s_hAltClass = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			g_free,
			NULL);
	// register to events to detect the ending of a launching
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_CREATED,
		(GldiNotificationFunc) _on_window_created,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_ACTIVATED,
		(GldiNotificationFunc) _on_window_activated,
		GLDI_RUN_AFTER, NULL);  // some applications don't open a new window, but rather take the focus; 
}

const GList *cairo_dock_list_existing_appli_with_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL ? pClassAppli->pAppliOfClass : NULL);
}

static gboolean _cairo_dock_add_inhibitor_to_class (const gchar *cClass, Icon *pIcon)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, FALSE);

	g_return_val_if_fail (g_list_find (pClassAppli->pIconsOfClass, pIcon) == NULL, TRUE);
	pClassAppli->pIconsOfClass = g_list_prepend (pClassAppli->pIconsOfClass, pIcon);

	return TRUE;
}

CairoDock *cairo_dock_get_class_subdock (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	return gldi_dock_get (pClassAppli->cDockName);
}

CairoDock* cairo_dock_create_class_subdock (const gchar *cClass, CairoDock *pParentDock)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	CairoDock *pDock = gldi_dock_get (pClassAppli->cDockName);
	if (pDock == NULL)  // cDockName not yet defined, or previous class subdock no longer exists
	{
		g_free (pClassAppli->cDockName);
		pClassAppli->cDockName = cairo_dock_get_unique_dock_name (cClass);
		pDock = gldi_subdock_new (pClassAppli->cDockName, NULL, pParentDock, NULL);
	}

	return pDock;
}

static void cairo_dock_destroy_class_subdock (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_if_fail (pClassAppli != NULL);

	CairoDock *pDock = gldi_dock_get (pClassAppli->cDockName);
	if (pDock)
	{
		gldi_object_unref (GLDI_OBJECT(pDock));
	}

	g_free (pClassAppli->cDockName);
	pClassAppli->cDockName = NULL;
}

gboolean cairo_dock_add_appli_icon_to_class (Icon *pIcon)
{
	g_return_val_if_fail (CAIRO_DOCK_ICON_TYPE_IS_APPLI (pIcon) && pIcon->pAppli, FALSE);
	cd_debug ("%s (%s)", __func__, pIcon->cClass);

	if (pIcon->cClass == NULL)
	{
		cd_message (" %s doesn't have any class, not good!", pIcon->cName);
		return FALSE;
	}

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	if (pClassAppli == NULL && pIcon->pAppli->cWmClass)
		pClassAppli = _cairo_dock_lookup_class_appli (pIcon->pAppli->cWmClass);
	if (!pClassAppli)
	{
		/* not seen before, register this class
		 * notes:
		 * (1) this will always create pClassAppli, even if not found
		 * (2) the return value is the same as pIcon->cClass
		 * (3) on X11, we use the cWmName as a fallback for the class; this comes from
		 * 	   the WM_CLASS property as well: the res_name part, see e.g.
		 * https://www.x.org/releases/X11R7.6/doc/libX11/specs/libX11/libX11.html#Setting_and_Reading_the_WM_CLASS_Property
		 *     while it is recommended to use the res_class part as the "class",
		 *     some apps do otherwise, e.g. exfalso sets
		 * WM_CLASS(STRING) = "io.github.quodlibet.ExFalso", "Exfalso"
		 * (where the first string is wm_name and corresponds to the correct desktop file ID)
		 */
		char *cClass = _cairo_dock_register_class_full (pIcon->cClass, pIcon->pAppli->cWmName,
			pIcon->pAppli->cWmClass, FALSE, TRUE, FALSE, NULL, &pClassAppli);
		if (cClass) g_free (cClass);
		if (!pClassAppli) return FALSE; // should not happen
	}

	if (pIcon->pAppInfo) gldi_object_unref (GLDI_OBJECT (pIcon->pAppInfo));
	pIcon->pAppInfo = pClassAppli->app; // can be null if no .desktop file was found
	if (pIcon->pAppInfo) gldi_object_ref (GLDI_OBJECT (pIcon->pAppInfo));
	//!! TODO: handle handle the case when the original pIcon->pAppInfo contained
	//!! a custom command!
	//!! this is needed for shift + click and similar to use the correct command if 
	//!! multiple icons are added (if only one, the launcher icon will take over)

	if (pClassAppli->pAppliOfClass == NULL)  // the first appli of a class defines the age of the class.
		pClassAppli->iAge = pIcon->pAppli->iAge;

	g_return_val_if_fail (g_list_find (pClassAppli->pAppliOfClass, pIcon) == NULL, TRUE);
	pClassAppli->pAppliOfClass = g_list_prepend (pClassAppli->pAppliOfClass, pIcon);

	return TRUE;
}

gboolean cairo_dock_remove_appli_from_class (Icon *pIcon)
{
	g_return_val_if_fail (pIcon!= NULL, FALSE);
	cd_debug ("%s (%s, %s)", __func__, pIcon->cClass, pIcon->cName);

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_val_if_fail (pClassAppli != NULL, FALSE);

	pClassAppli->pAppliOfClass = g_list_remove (pClassAppli->pAppliOfClass, pIcon);

	return TRUE;
}

gboolean cairo_dock_set_class_use_xicon (const gchar *cClass, gboolean bUseXIcon)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, FALSE);

	if (pClassAppli->bUseXIcon == bUseXIcon)  // nothing to do.
		return FALSE;

	GList *pElement;
	Icon *pAppliIcon;
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL; pElement = pElement->next)
	{
		pAppliIcon = pElement->data;
		if (bUseXIcon)
		{
			cd_message ("%s: take X icon", pAppliIcon->cName);
		}
		else
		{
			cd_message ("%s: doesn't use X icon", pAppliIcon->cName);
		}

		cairo_dock_reload_icon_image (pAppliIcon, cairo_dock_get_icon_container (pAppliIcon));
	}

	return TRUE;
}


static void _cairo_dock_set_same_indicator_on_sub_dock (Icon *pInhibhatorIcon)
{
	CairoDock *pInhibatorDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibhatorIcon));
	if (GLDI_OBJECT_IS_DOCK(pInhibatorDock) && pInhibatorDock->iRefCount > 0)  // the inhibitor is in a sub-dock.
	{
		gboolean bSubDockHasIndicator = FALSE;
		if (pInhibhatorIcon->bHasIndicator)
		{
			bSubDockHasIndicator = TRUE;
		}
		else
		{
			GList* ic;
			Icon *icon;
			for (ic = pInhibatorDock->icons ; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if (icon->bHasIndicator)
				{
					bSubDockHasIndicator = TRUE;
					break;
				}
			}
		}
		CairoDock *pParentDock = NULL;
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pInhibatorDock, &pParentDock);
		if (pPointingIcon != NULL && pPointingIcon->bHasIndicator != bSubDockHasIndicator)
		{
			cd_message ("  for the sub-dock %s : indicator <- %d", pPointingIcon->cName, bSubDockHasIndicator);
			pPointingIcon->bHasIndicator = bSubDockHasIndicator;
			if (pParentDock != NULL)
				cairo_dock_redraw_icon (pPointingIcon);
		}
	}
}

static GldiWindowActor *_gldi_appli_icon_detach_of_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, 0);

	const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
	Icon *pIcon;
	const GList *pElement;
	///gboolean bNeedsRedraw = FALSE;
	CairoDock *pParentDock;
	GldiWindowActor *pFirstFoundActor = NULL;
	for (pElement = pList; pElement != NULL; pElement = pElement->next)
	{
		pIcon = pElement->data;
		pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pIcon));
		if (pParentDock == NULL)  // not in a dock => nothing to do.
			continue;

		cd_debug ("detachment of the icon %s (%p)", pIcon->cName, pFirstFoundActor);
		gldi_icon_detach (pIcon);

		// if the icon was in the class sub-dock, check if it became empty
		if (pParentDock == cairo_dock_get_class_subdock (cClass))  // the icon was in the class sub-dock
		{
			if (pParentDock->icons == NULL)  // and it's now empty -> destroy it (and the class-icon pointing on it as well)
			{
				CairoDock *pMainDock = NULL;
				Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pParentDock, &pMainDock);
				/// TODO: register to the destroy event of the class sub-dock...
				cairo_dock_destroy_class_subdock (cClass);  // destroy it before the class-icon, since it will destroy the sub-dock
				if (pMainDock && CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pPointingIcon))
				{
					gldi_icon_detach (pPointingIcon);
					gldi_object_unref (GLDI_OBJECT(pPointingIcon));
				}
			}
		}

		if (pFirstFoundActor == NULL)  // we grab the 1st app of this class.
		{
			pFirstFoundActor = pIcon->pAppli;
		}
	}
	return pFirstFoundActor;
}
gboolean cairo_dock_inhibite_class (const gchar *cClass, Icon *pInhibitorIcon)
{
	g_return_val_if_fail (cClass != NULL, FALSE);
	cd_message ("%s (%s)", __func__, cClass);

	// add inhibitor to class (first, so that applis can find it and take its surface if necessary)
	if (! _cairo_dock_add_inhibitor_to_class (cClass, pInhibitorIcon))
		return FALSE;

	// set class name on the inhibitor if not already done.
	if (pInhibitorIcon && pInhibitorIcon->cClass != cClass)
	{
		g_free (pInhibitorIcon->cClass);
		pInhibitorIcon->cClass = g_strdup (cClass);
	}

	// if launchers are mixed with applis, steal applis icons.
	if (!myTaskbarParam.bMixLauncherAppli)
		return TRUE;
	GldiWindowActor *pFirstFoundActor = _gldi_appli_icon_detach_of_class (cClass);  // detach existing applis, and then retach them to the inhibitor.
	if (pInhibitorIcon != NULL)
	{
		// inhibitor takes control of the first existing appli of the class.
		gldi_icon_set_appli (pInhibitorIcon, pFirstFoundActor);
		pInhibitorIcon->bHasIndicator = (pFirstFoundActor != NULL);
		_cairo_dock_set_same_indicator_on_sub_dock (pInhibitorIcon);
		
		if (pFirstFoundActor)
		{
			// update the name of the icon
			if (pInhibitorIcon->cInitialName == NULL)
				pInhibitorIcon->cInitialName = pInhibitorIcon->cName;
			else
				g_free (pInhibitorIcon->cName);
			pInhibitorIcon->cName = g_strdup (pFirstFoundActor->cName ? pFirstFoundActor->cName : cClass);
		}

		// other applis icons are retached to the inhibitor.
		const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
		Icon *pIcon;
		const GList *pElement;
		for (pElement = pList; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			cd_debug (" an app is detached (%s)", pIcon->cName);
			if (pIcon->pAppli != pFirstFoundActor && cairo_dock_get_icon_container (pIcon) == NULL)  // detached and has to be re-attached.
				gldi_appli_icon_insert_in_dock (pIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
		}
	}

	return TRUE;
}

gboolean cairo_dock_class_is_inhibited (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->pIconsOfClass != NULL);
}

gboolean cairo_dock_class_is_using_xicon (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	// if pClassAppli == NULL, there is no launcher able to give its icon but we can found one in icons theme of the system.
	return (pClassAppli != NULL && pClassAppli->bUseXIcon);
}

gboolean cairo_dock_class_is_expanded (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->bExpand);
}

gboolean cairo_dock_prevent_inhibited_class (Icon *pIcon)
{
	g_return_val_if_fail (pIcon != NULL, FALSE);
	//g_print ("%s (%s)\n", __func__, pIcon->cClass);

	gboolean bToBeInhibited = FALSE;
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	if (pClassAppli != NULL)
	{
		Icon *pInhibitorIcon;
		GList *pElement;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			if (pInhibitorIcon != NULL)  // an inhibitor is present.
			{
				if (pInhibitorIcon->pAppli == NULL && pInhibitorIcon->pSubDock == NULL)  // this icon inhibits this class but doesn't control any apps yet
				{
					gldi_icon_set_appli (pInhibitorIcon, pIcon->pAppli);
					cd_message (">>> %s will take an indicator during the next redraw ! (pAppli : %p)", pInhibitorIcon->cName, pInhibitorIcon->pAppli);
					pInhibitorIcon->bHasIndicator = TRUE;
					_cairo_dock_set_same_indicator_on_sub_dock (pInhibitorIcon);
				/**}

				if (pInhibitorIcon->pAppli == pIcon->pAppli)  // this icon controls us.
				{*/
					CairoDock *pInhibatorDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibitorIcon));
					//\______________ We place the icon for X.
					if (! bToBeInhibited)  // we put the thumbnail only on the 1st one.
					{
						if (pInhibatorDock != NULL)
						{
							//g_print ("we move the thumbnail on the inhibitor %s\n", pInhibitorIcon->cName);
							gldi_appli_icon_set_geometry_for_window_manager (pInhibitorIcon, pInhibatorDock);
						}
					}
					//\______________ We update inhibitor's label.
					if (pInhibatorDock != NULL && pIcon->cName != NULL)
					{
						if (pInhibitorIcon->cInitialName == NULL)
							pInhibitorIcon->cInitialName = pInhibitorIcon->cName;
						else
							g_free (pInhibitorIcon->cName);
						pInhibitorIcon->cName = NULL;
						gldi_icon_set_name (pInhibitorIcon, pIcon->cName);
					}
				}
				bToBeInhibited = (pInhibitorIcon->pAppli == pIcon->pAppli);
			}
		}
	}
	return bToBeInhibited;
}


static void _cairo_dock_remove_icon_from_class (Icon *pInhibitorIcon)
{
	g_return_if_fail (pInhibitorIcon != NULL);
	cd_message ("%s (%s)", __func__, pInhibitorIcon->cClass);

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pInhibitorIcon->cClass);
	if (pClassAppli != NULL)
	{
		pClassAppli->pIconsOfClass = g_list_remove (pClassAppli->pIconsOfClass, pInhibitorIcon);
	}
}

void cairo_dock_deinhibite_class (const gchar *cClass, Icon *pInhibitorIcon)
{
	cd_message ("%s (%s)", __func__, cClass);
	_cairo_dock_remove_icon_from_class (pInhibitorIcon);

	if (pInhibitorIcon != NULL && pInhibitorIcon->pSubDock != NULL && pInhibitorIcon->pSubDock == cairo_dock_get_class_subdock (cClass))  // the launcher is controlling several appli icons, place them back in the taskbar.
	{
		// first destroy the class sub-dock, so that the appli icons won't go inside again.
		// we empty the sub-dock then destroy it, then re-insert the appli icons
		GList *icons = pInhibitorIcon->pSubDock->icons;
		pInhibitorIcon->pSubDock->icons = NULL;  // empty the sub-dock
		cairo_dock_destroy_class_subdock (cClass);  // destroy the sub-dock without destroying its icons
		pInhibitorIcon->pSubDock = NULL;  // since the inhibitor can already be detached, the sub-dock can't find it

		Icon *pAppliIcon;
		GList *ic;
		for (ic = icons; ic != NULL; ic = ic->next)
		{
			pAppliIcon = ic->data;
			cairo_dock_set_icon_container (pAppliIcon, NULL);  // manually "detach" it
		}

		// then re-insert the appli icons.
		for (ic = icons; ic != NULL; ic = ic->next)
		{
			pAppliIcon = ic->data;
			gldi_appli_icon_insert_in_dock (pAppliIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
		}
		g_list_free (icons);

		cairo_dock_trigger_load_icon_buffers (pInhibitorIcon);  // in case the inhibitor was drawn with an emblem or a stack of the applis
	}

	if (pInhibitorIcon == NULL || pInhibitorIcon->pAppli != NULL)  // the launcher is controlling 1 appli icon, or we deinhibate all the inhibitors.
	{
		const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
		Icon *pIcon;
		///gboolean bNeedsRedraw = FALSE;
		///CairoDock *pParentDock;
		const GList *pElement;
		for (pElement = pList; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			if (pInhibitorIcon == NULL || pIcon->pAppli == pInhibitorIcon->pAppli)
			{
				cd_message ("re-add the icon previously inhibited (pAppli:%p)", pIcon->pAppli);
				pIcon->fInsertRemoveFactor = 0;
				pIcon->fScale = 1.;
				/**pParentDock = */
				gldi_appli_icon_insert_in_dock (pIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
				///bNeedsRedraw = (pParentDock != NULL && pParentDock->bIsMainDock);
			}
			///cairo_dock_reload_icon_image (pIcon, cairo_dock_get_icon_container (pIcon));  /// question : why should we do that for all icons?...
		}
		///if (bNeedsRedraw)
		///	gtk_widget_queue_draw (g_pMainDock->container.pWidget);  /// pDock->pRenderer->calculate_icons (pDock); ?...
	}
	if (pInhibitorIcon != NULL)
	{
		cd_message (" the inhibitor has lost everything");
		gldi_icon_unset_appli (pInhibitorIcon);
		pInhibitorIcon->bHasIndicator = FALSE;
		g_free (pInhibitorIcon->cClass);
		pInhibitorIcon->cClass = NULL;
		cd_debug ("  no more classes");
	}
}


void gldi_window_detach_from_inhibitors (GldiWindowActor *pAppli)
{
	const gchar *cClass = pAppli->cClass;
	cd_message ("%s (%s)", __func__, cClass);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GldiWindowActor *pNextAppli = NULL;  // next window that will be inhibited.
		gboolean bFirstSearch = TRUE;
		Icon *pSameClassIcon = NULL;
		Icon *pIcon;
		GList *pElement;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			if (pIcon->pAppli == pAppli)  // this inhibitor controls the given window -> make it control another (possibly none).
			{
				// find the next inhibited appli
				if (bFirstSearch)  // we didn't search the next window yet, do it now.
				{
					bFirstSearch = FALSE;
					Icon *pOneIcon;
					GList *ic;
					for (ic = g_list_last (pClassAppli->pAppliOfClass); ic != NULL; ic = ic->prev)  // reverse order, to take the oldest window of this class.
					{
						pOneIcon = ic->data;
						if (pOneIcon != NULL
						&& pOneIcon->pAppli != NULL
						&& pOneIcon->pAppli != pAppli  // not the window we precisely want to avoid
						&& (! myTaskbarParam.bAppliOnCurrentDesktopOnly || gldi_window_is_on_current_desktop (pOneIcon->pAppli)))  // can actually be displayed
						{
							pSameClassIcon = pOneIcon;
							break ;
						}
					}
					pNextAppli = (pSameClassIcon != NULL ? pSameClassIcon->pAppli : NULL);
					if (pSameClassIcon != NULL)  // this icon will be inhibited, we need to detach it if needed
					{
						cd_message ("  it's %s which will replace it", pSameClassIcon->cName);
						gldi_icon_detach (pSameClassIcon);  // it can't be the class sub-dock, because pIcon had the window actor, so it doesn't hold the class sub-dock and the class is not grouped (otherwise they would all be in the class sub-dock).
					}
				}

				// make the icon inhibits the next appli (possibly none)
				gldi_icon_set_appli (pIcon, pNextAppli);
				pIcon->bHasIndicator = (pNextAppli != NULL);
				_cairo_dock_set_same_indicator_on_sub_dock (pIcon);
				if (pNextAppli == NULL)
					gldi_icon_set_name (pIcon, pIcon->cInitialName);
				cd_message (" %s : bHasIndicator <- %d, pAppli <- %p", pIcon->cName, pIcon->bHasIndicator, pIcon->pAppli);

				// redraw
				GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
				if (pContainer)
					gtk_widget_queue_draw (pContainer->pWidget);
			}
		}
	}
}

static void _cairo_dock_remove_all_applis_from_class (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, G_GNUC_UNUSED gpointer data)
{
	g_list_free (pClassAppli->pAppliOfClass);
	pClassAppli->pAppliOfClass = NULL;

	Icon *pInhibitorIcon;
	GList *pElement;
	for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
	{
		pInhibitorIcon = pElement->data;
		pInhibitorIcon->bHasIndicator = FALSE;
		gldi_icon_unset_appli (pInhibitorIcon);
		_cairo_dock_set_same_indicator_on_sub_dock (pInhibitorIcon);
	}
}
void cairo_dock_remove_all_applis_from_class_table (void)  // for the stop_application_manager
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_remove_all_applis_from_class, NULL);
}

void cairo_dock_reset_class_table (void)
{
	g_hash_table_remove_all (s_hClassTable);
	g_hash_table_remove_all (s_hAltClass);
	// gldi_desktop_file_db_stop (); -- TODO: call this when exiting, but not when loading a new theme !!
}


cairo_surface_t *cairo_dock_create_surface_from_class (const gchar *cClass, int iWidth, int iHeight)
{
	cd_debug ("%s (%s)", __func__, cClass);
	// first we try to get an icon from one of the inhibitor.
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		cd_debug ("bUseXIcon:%d", pClassAppli->bUseXIcon);
		if (pClassAppli->bUseXIcon)
			return NULL;

		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			cd_debug ("  %s", pInhibitorIcon->cName);
			if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon))
			{
				if (pInhibitorIcon->pSubDock == NULL || myIndicatorsParam.bUseClassIndic)  // in the case where a launcher has more than one instance of its class and which represents the stack, we doesn't take the icon.
				{
					cd_debug ("%s will give its surface", pInhibitorIcon->cName);
					return cairo_dock_duplicate_surface (pInhibitorIcon->image.pSurface,
						pInhibitorIcon->image.iWidth,
						pInhibitorIcon->image.iHeight,
						iWidth,
						iHeight);
				}
				else if (pInhibitorIcon->cFileName != NULL)
				{
					gchar *cIconFilePath = cairo_dock_search_icon_s_path (pInhibitorIcon->cFileName, MAX (iWidth, iHeight));
					if (cIconFilePath != NULL)
					{
						cd_debug ("we replace X icon by %s", cIconFilePath);
						cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
							iWidth,
							iHeight);
						g_free (cIconFilePath);
						if (pSurface)
							return pSurface;
					}
				}
			}
		}
	}

	// if we didn't find one, we use the icon defined in the class.
	if (pClassAppli != NULL && pClassAppli->cIcon != NULL)
	{
		cd_debug ("get the class icon (%s)", pClassAppli->cIcon);
		gchar *cIconFilePath = cairo_dock_search_icon_s_path (pClassAppli->cIcon, MAX (iWidth, iHeight));
		cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
			iWidth,
			iHeight);
		g_free (cIconFilePath);
		if (pSurface)
			return pSurface;
	}
	else
	{
		cd_debug ("no icon for the class %s", cClass);
	}

	// if not found or not defined, try to find an icon based on the name class.
	gchar *cIconFilePath = cairo_dock_search_icon_s_path (cClass, MAX (iWidth, iHeight));
	if (cIconFilePath != NULL)
	{
		cd_debug ("we replace the X icon by %s", cIconFilePath);
		cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
			iWidth,
			iHeight);
		g_free (cIconFilePath);
		if (pSurface)
			return pSurface;
	}

	cd_debug ("class %s will take the X icon", cClass);
	return NULL;
}

/**
void cairo_dock_update_visibility_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli, gboolean bIsHidden)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == pAppli)
			{
				cd_debug (" %s also %s itself", pInhibitorIcon->cName, (bIsHidden ? "hide" : "show"));
				if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon) && myTaskbarParam.fVisibleAppliAlpha != 0)
				{
					pInhibitorIcon->fAlpha = 1;  // we cheat a bit.
					cairo_dock_redraw_icon (pInhibitorIcon);
				}
			}
		}
	}
}

void cairo_dock_update_activity_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == pAppli)
			{
				cd_debug (" %s becomes active too", pInhibitorIcon->cName);
				CairoDock *pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibitorIcon));
				if (pParentDock != NULL)
					gldi_appli_icon_animate_on_active (pInhibitorIcon, pParentDock);
			}
		}
	}
}

void cairo_dock_update_inactivity_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == pAppli)
			{
				cairo_dock_redraw_icon (pInhibitorIcon);
			}
		}
	}
}

void cairo_dock_update_name_on_inhibitors (const gchar *cClass, GldiWindowActor *actor, gchar *cNewName)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == actor)
			{
				if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon))
				{
					cd_debug (" %s change its name to %s", pInhibitorIcon->cName, cNewName);
					if (pInhibitorIcon->cInitialName == NULL)
					{
						pInhibitorIcon->cInitialName = pInhibitorIcon->cName;
						cd_debug ("pInhibitorIcon->cInitialName <- %s", pInhibitorIcon->cInitialName);
					}
					else
						g_free (pInhibitorIcon->cName);
					pInhibitorIcon->cName = NULL;

					gldi_icon_set_name (pInhibitorIcon, (cNewName != NULL ? cNewName : pInhibitorIcon->cInitialName));
				}
				cairo_dock_redraw_icon (pInhibitorIcon);
			}
		}
	}
}
*/
void gldi_window_foreach_inhibitor (GldiWindowActor *actor, GldiIconRFunc callback, gpointer data)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (actor->cClass);
	if (pClassAppli != NULL)
	{
		Icon *pInhibitorIcon;
		GList *ic;
		for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
		{
			pInhibitorIcon = ic->data;
			if (pInhibitorIcon->pAppli == actor)
			{
				if (! callback (pInhibitorIcon, data))
					break;
			}
		}
	}
}


Icon *cairo_dock_get_classmate (Icon *pIcon)  // gets an icon of the same class, that is inside a dock (or will be for an inhibitor), but not inside the class sub-dock
{
	cd_debug ("%s (%s)", __func__, pIcon->cClass);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	Icon *pFriendIcon = NULL;
	GList *pElement;
	for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
	{
		pFriendIcon = pElement->data;
		/// TODO: this is ugly... maybe an inhibitor shouldn't inhibit when not yet in a dock...
		if (pFriendIcon == NULL || (cairo_dock_get_icon_container(pFriendIcon) == NULL && pFriendIcon->cParentDockName == NULL))  // if not inside a dock (for instance a detached applet, but not a hidden launcher), ignore.
			continue ;
		cd_debug (" friend : %s", pFriendIcon->cName);
		if (pFriendIcon->pAppli != NULL || pFriendIcon->pSubDock != NULL)  // is linked to a window, 1 or several times (window actor or class sub-dock).
			return pFriendIcon;
	}

	GldiContainer *pClassSubDock = CAIRO_CONTAINER(cairo_dock_get_class_subdock (pIcon->cClass));
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL; pElement = pElement->next)
	{
		pFriendIcon = pElement->data;
		if (pFriendIcon == pIcon)  // skip ourselves
			continue ;

		if (cairo_dock_get_icon_container (pFriendIcon) != NULL && cairo_dock_get_icon_container (pFriendIcon) != pClassSubDock)  // inside a dock, but not the class sub-dock
			return pFriendIcon;
	}

	return NULL;
}



gboolean cairo_dock_check_class_subdock_is_empty (CairoDock *pDock, const gchar *cClass)
{
	cd_debug ("%s (%s, %d)", __func__, cClass, g_list_length (pDock->icons));
	if (pDock->iRefCount == 0)
		return FALSE;
	if (pDock->icons == NULL)  // shouldn't happen, handle this case and make some noise.
	{
		cd_warning ("the %s class sub-dock has no element, which is probably an error !\nit will be destroyed.", cClass);
		Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
		cairo_dock_destroy_class_subdock (cClass);
		pFakeClassIcon->pSubDock = NULL;
		if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pFakeClassIcon))
		{
			gldi_icon_detach (pFakeClassIcon);
			gldi_object_unref (GLDI_OBJECT(pFakeClassIcon));
		}
		return TRUE;
	}
	else if (pDock->icons->next == NULL)  // only 1 icon left in the sub-dock -> destroy it.
	{
		cd_debug ("   the sub-dock of the class %s has now only one item in it: will be destroyed", cClass);
		Icon *pLastClassIcon = pDock->icons->data;

		CairoDock *pFakeParentDock = NULL;
		Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
		g_return_val_if_fail (pFakeClassIcon != NULL, TRUE);

		// detach the last icon from the class sub-dock
		gboolean bLastIconIsRemoving = cairo_dock_icon_is_being_removed (pLastClassIcon);  // keep the removing state because when we detach the icon, it returns to normal state.
		gldi_icon_detach (pLastClassIcon);
		pLastClassIcon->fOrder = pFakeClassIcon->fOrder;  // if re-inserted in a dock, insert at the same place

		// destroy the class sub-dock
		cairo_dock_destroy_class_subdock (cClass);
		pFakeClassIcon->pSubDock = NULL;

		if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pFakeClassIcon))  // the class sub-dock is pointed by a class-icon
		{
			// destroy the class-icon
			gldi_icon_detach (pFakeClassIcon);
			gldi_object_unref (GLDI_OBJECT(pFakeClassIcon));

			// re-insert the last icon in place of it, or destroy it if it was being removed
			if (! bLastIconIsRemoving)
			{
				gldi_icon_insert_in_container (pLastClassIcon, CAIRO_CONTAINER(pFakeParentDock), ! CAIRO_DOCK_ANIMATE_ICON);
			}
			else  // the last icon is being removed, no need to re-insert it (e.g. when we close all classes in one hit)
			{
				cd_debug ("no need to re-insert the last icon");
				gldi_object_unref (GLDI_OBJECT(pLastClassIcon));
			}
		}
		else  // the class sub-dock is pointed by a launcher/applet
		{
			// re-inhibit the last icon or destroy it if it was being removed
			if (! bLastIconIsRemoving)
			{
				gldi_appli_icon_insert_in_dock (pLastClassIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);  // Note that we could optimize and manually set the appli and the name...
				///cairo_dock_update_name_on_inhibitors (cClass, pLastClassIcon->pAppli, pLastClassIcon->cName);
			}
			else  // the last icon is being removed, no need to re-insert it
			{
				pFakeClassIcon->bHasIndicator = FALSE;
				gldi_object_unref (GLDI_OBJECT(pLastClassIcon));
			}
			cairo_dock_redraw_icon (pFakeClassIcon);
		}
		
		// trigger a leave event on the parent dock as it might need to shrink down now
		gldi_dock_leave_synthetic (pFakeParentDock);
		cd_debug ("no more dock");
		return TRUE;
	}
	return FALSE;
}

static void _cairo_dock_update_class_subdock_name (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, gconstpointer *data)
{
	const CairoDock *pDock = data[0];
	const gchar *cNewName = data[1];
	if (g_strcmp0 (pClassAppli->cDockName, pDock->cDockName) == 0)  // this class uses this dock
	{
		g_free (pClassAppli->cDockName);
		pClassAppli->cDockName = g_strdup (cNewName);
	}
}
void cairo_dock_update_class_subdock_name (const CairoDock *pDock, const gchar *cNewName)  // update the subdock name of the class that holds 'pDock' as its class subdock
{
	gconstpointer data[2] = {pDock, cNewName};
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_update_class_subdock_name, data);
}


static void _cairo_dock_reset_overwrite_exceptions (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, G_GNUC_UNUSED gpointer data)
{
	pClassAppli->bUseXIcon = FALSE;
}
void cairo_dock_set_overwrite_exceptions (const gchar *cExceptions)
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_reset_overwrite_exceptions, NULL);
	if (cExceptions == NULL)
		return;

	gchar **cClassList = g_strsplit (cExceptions, ";", -1);
	if (cClassList == NULL || cClassList[0] == NULL || *cClassList[0] == '\0')
	{
		g_strfreev (cClassList);
		return;
	}
	CairoDockClassAppli *pClassAppli;
	int i;
	for (i = 0; cClassList[i] != NULL; i ++)
	{
		pClassAppli = _cairo_dock_lookup_class_appli (cClassList[i]);
		if (pClassAppli) pClassAppli->bUseXIcon = TRUE;
	}

	g_strfreev (cClassList);
}

static void _cairo_dock_reset_group_exceptions (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, G_GNUC_UNUSED gpointer data)
{
	pClassAppli->bExpand = FALSE;
}
void cairo_dock_set_group_exceptions (const gchar *cExceptions)
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_reset_group_exceptions, NULL);
	if (cExceptions == NULL)
		return;

	gchar **cClassList = g_strsplit (cExceptions, ";", -1);
	if (cClassList == NULL || cClassList[0] == NULL || *cClassList[0] == '\0')
	{
		g_strfreev (cClassList);
		return;
	}
	CairoDockClassAppli *pClassAppli;
	int i;
	for (i = 0; cClassList[i] != NULL; i ++)
	{
		pClassAppli = _cairo_dock_lookup_class_appli (cClassList[i]);
		if (pClassAppli) pClassAppli->bExpand = TRUE;
	}

	g_strfreev (cClassList);
}


Icon *cairo_dock_get_prev_next_classmate_icon (Icon *pIcon, gboolean bNext)
{
	cd_debug ("%s (%s, %s)", __func__, pIcon->cClass, pIcon->cName);
	g_return_val_if_fail (pIcon->cClass != NULL, NULL);

	Icon *pActiveIcon = cairo_dock_get_current_active_icon ();
	if (pActiveIcon == NULL || pActiveIcon->cClass == NULL || strcmp (pActiveIcon->cClass, pIcon->cClass) != 0)  // the active window is not from our class, we active the icon given in parameter.
	{
		cd_debug ("Active icon's class: %s", pIcon->cClass);
		return pIcon;
	}

	//\________________ We are looking in the class of the active window and take the next or previous one.
	Icon *pNextIcon = NULL;
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	//\________________ We are looking in icons of apps.
	Icon *pClassmateIcon;
	GList *pElement, *ic;
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL && pNextIcon == NULL; pElement = pElement->next)
	{
		pClassmateIcon = pElement->data;
		cd_debug (" %s is it active?", pClassmateIcon->cName);
		if (pClassmateIcon->pAppli == pActiveIcon->pAppli)  // the active window.
		{
			cd_debug ("  found an active window (%s; %p)", pClassmateIcon->cName, pClassmateIcon->pAppli);
			if (bNext)  // take the 1st non null after that.
			{
				ic = pElement;
				do
				{
					ic = cairo_dock_get_next_element (ic, pClassAppli->pAppliOfClass);
					if (ic == pElement)
					{
						cd_debug ("  found nothing!");
						break ;
					}
					pClassmateIcon = ic->data;
					if (pClassmateIcon != NULL && pClassmateIcon->pAppli != NULL)
					{
						cd_debug ("  we take this one (%s; %p)", pClassmateIcon->cName, pClassmateIcon->pAppli);
						pNextIcon = pClassmateIcon;
						break ;
					}
				}
				while (1);
			}
			else  // we take the first non null before it.
			{
				ic = pElement;
				do
				{
					ic = cairo_dock_get_previous_element (ic, pClassAppli->pAppliOfClass);
					if (ic == pElement)
						break ;
					pClassmateIcon = ic->data;
					if (pClassmateIcon != NULL && pClassmateIcon->pAppli != NULL)
					{
						pNextIcon = pClassmateIcon;
						break ;
					}
				}
				while (1);
			}
			break ;
		}
	}
	return pNextIcon;
}



Icon *cairo_dock_get_inhibitor (Icon *pIcon, gboolean bOnlyInDock)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == pIcon->pAppli)
			{
				if (! bOnlyInDock || cairo_dock_get_icon_container (pInhibitorIcon) != NULL)
					return pInhibitorIcon;
			}
		}
	}
	return NULL;
}

static inline double _get_previous_order (GList *ic)
{
	if (ic == NULL)
		return 0;
	double fOrder;
	Icon *icon = ic->data;
	Icon *prev_icon = (ic->prev ? ic->prev->data : NULL);
	if (prev_icon != NULL && cairo_dock_get_icon_order (prev_icon) == cairo_dock_get_icon_order (icon))

		fOrder = (icon->fOrder + prev_icon->fOrder) / 2;
	else
		fOrder = icon->fOrder - 1;
	return fOrder;
}
static inline double _get_next_order (GList *ic)
{
	if (ic == NULL)
		return 0;
	double fOrder;
	Icon *icon = ic->data;
	Icon *next_icon = (ic->next ? ic->next->data : NULL);
	if (next_icon != NULL && cairo_dock_get_icon_order (next_icon) == cairo_dock_get_icon_order (icon))
		fOrder = (icon->fOrder + next_icon->fOrder) / 2;
	else
		fOrder = icon->fOrder + 1;
	return fOrder;
}
static inline double _get_first_appli_order (CairoDock *pDock, GList *first_launcher_ic, GList *last_launcher_ic)
{
	double fOrder;
	switch (myTaskbarParam.iIconPlacement)
	{
		case CAIRO_APPLI_BEFORE_FIRST_ICON:
			fOrder = _get_previous_order (pDock->icons);
		break;

		case CAIRO_APPLI_BEFORE_FIRST_LAUNCHER:
			if (first_launcher_ic != NULL)
			{
				//g_print (" go just before the first launcher (%s)\n", ((Icon*)first_launcher_ic->data)->cName);
				fOrder = _get_previous_order (first_launcher_ic);  // 'first_launcher_ic' includes the separators, so we can just take the previous order.
			}
			else  // no launcher, go to the beginning of the dock.
			{
				fOrder = _get_previous_order (pDock->icons);
			}
		break;

		case CAIRO_APPLI_AFTER_ICON:
		{
			Icon *icon;
			GList *ic = NULL;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if ((icon->cDesktopFileName != NULL && g_strcmp0 (icon->cDesktopFileName, myTaskbarParam.cRelativeIconName) == 0)
				|| (icon->pModuleInstance && g_strcmp0 (icon->pModuleInstance->cConfFilePath, myTaskbarParam.cRelativeIconName) == 0))
					break;
			}

			if (ic != NULL)  // icon found
			{
				fOrder = _get_next_order (ic);
				break;
			}  // else don't break, and go to the 'CAIRO_APPLI_AFTER_LAST_LAUNCHER' case, which will be the fallback.
		}
		/* fall through */

		case CAIRO_APPLI_AFTER_LAST_LAUNCHER:
		default:
			if (last_launcher_ic != NULL)
			{
				//g_print (" go just after the last launcher (%s)\n", ((Icon*)last_launcher_ic->data)->cName);
				fOrder = _get_next_order (last_launcher_ic);
			}
			else  // no launcher, go to the beginning of the dock.
			{
				fOrder = _get_previous_order (pDock->icons);
			}
		break;

		case CAIRO_APPLI_AFTER_LAST_ICON:
			fOrder = _get_next_order (g_list_last (pDock->icons));
		break;
	}
	return fOrder;
}
static inline int _get_class_age (CairoDockClassAppli *pClassAppli)
{
	if (pClassAppli->pAppliOfClass == NULL)
		return 0;
	return pClassAppli->iAge;
}
// Set the order of an appli when they are mixed amongst launchers and no class sub-dock exists (because either they are not grouped by class, or just it's the first appli of this class in the dock)
// First try to see if an inhibitor is present in the dock; if not, see if an appli of the same class is present in the dock.
// -> if yes, place it next to it, ordered by age (go to the right until our age is greater)
// -> if no, place it amongst the other appli icons, ordered by age (search the last launcher, skip any automatic separator, and then go to the right until our age is greater or there is no more appli).
void cairo_dock_set_class_order_in_dock (Icon *pIcon, CairoDock *pDock)
{
	//g_print ("%s (%s, %d)\n", __func__, pIcon->cClass, pIcon->iAge);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_if_fail (pClassAppli != NULL);

	// Look for an icon of the same class in the dock, to place ourself relatively to it.
	Icon *pSameClassIcon = NULL;
	GList *same_class_ic = NULL;

	// First look for an inhibitor of this class, preferably a launcher.
	CairoDock *pParentDock;
	Icon *pInhibitorIcon;
	GList *ic;
	for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
	{
		pInhibitorIcon = ic->data;

		pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibitorIcon));
		if (! GLDI_OBJECT_IS_DOCK(pParentDock))  // not inside a dock, for instance a desklet (or a hidden launcher) -> skip
			continue;
		pSameClassIcon = pInhibitorIcon;
		same_class_ic = ic;
		//g_print (" found an inhibitor of this class: %s (%d)\n", pSameClassIcon->cName, pSameClassIcon->iAge);
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pSameClassIcon))  // it's a launcher, we wont't find better -> quit
			break ;
	}

	// if no inhibitor found, look for an appli of this class in the dock.
	if (pSameClassIcon == NULL)
	{
		Icon *pAppliIcon;
		for (ic = g_list_last (pClassAppli->pAppliOfClass); ic != NULL; ic = ic->prev)  // check the older icons first (prepend), because then we'll place ourself to their right.
		{
			pAppliIcon = ic->data;
			if (pAppliIcon == pIcon)  // skip ourself
				continue;
			pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pAppliIcon));
			if (pParentDock != NULL)
			{
				pSameClassIcon = pAppliIcon;
				same_class_ic = ic;
				//g_print (" found an appli of this class: %s (%d)\n", pSameClassIcon->cName, pSameClassIcon->iAge);
				break ;
			}
		}
		pIcon->iGroup = (myTaskbarParam.bSeparateApplis ? CAIRO_DOCK_APPLI : CAIRO_DOCK_LAUNCHER);  // no inhibitor, so we'll go in the taskbar group.
	}
	else  // an inhibitor is present, we'll go next to it, so we'll be in its group.
	{
		pIcon->iGroup = pSameClassIcon->iGroup;
	}

	// if we found one, place next to it, ordered by age amongst the other appli of this class already in the dock.
	if (pSameClassIcon != NULL)
	{
		same_class_ic = g_list_find (pDock->icons, pSameClassIcon);
		g_return_if_fail (same_class_ic != NULL);
		Icon *pNextIcon = NULL;  // the next icon after all the icons of our class, or NULL if we reach the end of the dock.
		for (ic = same_class_ic->next; ic != NULL; ic = ic->next)
		{
			pNextIcon = ic->data;
			//g_print ("  next icon: %s (%d)\n", pNextIcon->cName, pNextIcon->iAge);
			if (!pNextIcon->cClass || strcmp (pNextIcon->cClass, pIcon->cClass) != 0)  // not our class any more, quit.
				break;

			if (pIcon->pAppli->iAge > pNextIcon->pAppli->iAge)  // we are more recent than this icon -> place on its right -> continue
			{
				pSameClassIcon = pNextIcon;  // 'pSameClassIcon' will be the last icon of our class older than us.
				pNextIcon = NULL;
			}
			else  // we are older than it -> go just before it -> quit
			{
				break;
			}
		}
		//g_print (" pNextIcon: %s (%d)\n", pNextIcon?pNextIcon->cName:"none", pNextIcon?pNextIcon->iAge:-1);

		if (pNextIcon != NULL && cairo_dock_get_icon_order (pNextIcon) == cairo_dock_get_icon_order (pSameClassIcon))  // the next icon is in thge09e same group as us: place it between this icon and pSameClassIcon.
			pIcon->fOrder = (pNextIcon->fOrder + pSameClassIcon->fOrder) / 2;
		else  // no icon after our class or in a different grou: we place just after pSameClassIcon.
			pIcon->fOrder = pSameClassIcon->fOrder + 1;

		return;
	}

	// if no icon of our class is present in the dock, place it amongst the other appli icons, after the first appli or after the launchers, and ordered by age.
	// search the last launcher and the first appli.
	Icon *icon;
	Icon *pFirstLauncher = NULL;
	GList *first_appli_ic = NULL, *last_launcher_ic = NULL, *first_launcher_ic = NULL;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)  // launcher, even without class
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon)  // container icon (likely to contain some launchers)
		|| (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && icon->pModuleInstance->pModule->pVisitCard->bActAsLauncher)  // applet acting like a launcher
		/**|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon)*/)  // separator (user or auto).
		{
			// pLastLauncher = icon;
			last_launcher_ic = ic;
			if (pFirstLauncher == NULL)
			{
				pFirstLauncher = icon;
				first_launcher_ic = ic;
			}
		}
		else if ((CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) || CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
		&& ! cairo_dock_class_is_inhibited (icon->cClass))  // an appli not placed next to its inhibitor.
		{
			// pFirstAppli = icon;
			first_appli_ic = ic;
			break ;
		}
	}
	//g_print (" last launcher: %s\n", pLastLauncher?pLastLauncher->cName:"none");
	//g_print (" first appli: %s\n", pFirstAppli?pFirstAppli->cName:"none");

	// place amongst the other applis, or after the last launcher.
	if (first_appli_ic != NULL)  // if an appli exists in the dock, use it as an anchor.
	{
		int iAge = _get_class_age (pClassAppli);  // the age of our class.

		GList *last_appli_ic = NULL;  // last appli whose class is older than ours => we'll go just after.
		for (ic = first_appli_ic; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			if (! CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) && ! CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
				break;

			// get the age of this class (= age of the oldest icon of this class)
			CairoDockClassAppli *pOtherClassAppli = _cairo_dock_lookup_class_appli (icon->cClass);
			if (! pOtherClassAppli || ! pOtherClassAppli->pAppliOfClass)  // should never happen
				continue;

			int iOtherClassAge = _get_class_age (pOtherClassAppli);
			//g_print (" age of class %s: %d\n", icon->cClass, iOtherClassAge);

			// compare to our class.
			if (iOtherClassAge < iAge)  // it's older than our class -> skip this whole class, we'll go after.
			{
				Icon *next_icon;
				while (ic->next != NULL)
				{
					next_icon = ic->next->data;
					if (next_icon->cClass && strcmp (next_icon->cClass, icon->cClass) == 0)  // next icon is of the same class -> skip
						ic = ic->next;
					else
						break;
				}
				last_appli_ic = ic;
			}
			else  // we are older -> discard and quit.
			{
				break;
			}
		}

		if (last_appli_ic == NULL)  // we are the oldest class -> go just before the first appli
		{
			//g_print (" we are the oldest class\n");
			pIcon->fOrder = _get_previous_order (first_appli_ic);
		}
		else  // go just after the last one
		{
			//g_print (" go just after %s\n", ((Icon*)last_appli_ic->data)->cName);
			pIcon->fOrder = _get_next_order (last_appli_ic);
		}
	}
	else  // no appli yet in the dock -> place it at the taskbar position defined in conf.
	{
		pIcon->fOrder = _get_first_appli_order (pDock, first_launcher_ic, last_launcher_ic);
	}
}

void cairo_dock_set_class_order_amongst_applis (Icon *pIcon, CairoDock *pDock)  // set the order of an appli amongst the other applis of a given dock (class sub-dock or main dock).
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_if_fail (pClassAppli != NULL);

	// place the icon amongst the other appli icons of this class, or after the last appli if none.
	if (myTaskbarParam.bSeparateApplis)
		pIcon->iGroup = CAIRO_DOCK_APPLI;
	else
		pIcon->iGroup = CAIRO_DOCK_LAUNCHER;
	Icon *icon;
	GList *ic, *last_ic = NULL, *first_appli_ic = NULL;
	GList *last_launcher_ic = NULL, *first_launcher_ic = NULL;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon))
		{
			if (! first_appli_ic)
				first_appli_ic = ic;
			if (icon->cClass && strcmp (icon->cClass, pIcon->cClass) == 0)  // this icon is in our class.
			{
				if (!icon->pAppli || icon->pAppli->iAge < pIcon->pAppli->iAge)  // it's older than us => we are more recent => go after => continue. (Note: icon->pAppli can be NULL if the icon in the dock is being removed)
				{
					last_ic = ic;  // remember the last item of our class.
				}
				else  // we are older than it => go just before it.
				{
					pIcon->fOrder = _get_previous_order (ic);
					return ;
				}
			}
		}
		else if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)  // launcher, even without class
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon)  // container icon (likely to contain some launchers)
		|| (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && icon->cClass != NULL && icon->pModuleInstance->pModule->pVisitCard->bActAsLauncher)  // applet acting like a launcher
		|| (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon)))  // separator (user or auto).
		{
			last_launcher_ic = ic;
			if (first_launcher_ic == NULL)
			{
				first_launcher_ic = ic;
			}
		}
	}

	if (last_ic != NULL)  // there are some applis of our class, but none are more recent than us, so we are the most recent => go just after the last one we found previously.
	{
		pIcon->fOrder = _get_next_order (last_ic);
	}
	else  // we didn't find a single icon of our class => place amongst the other applis from age.
	{
		if (first_appli_ic != NULL)  // if an appli exists in the dock, use it as an anchor.
		{
			Icon *pOldestAppli = g_list_last (pClassAppli->pAppliOfClass)->data;  // prepend
			int iAge = pOldestAppli->pAppli->iAge;  // the age of our class.

			GList *last_appli_ic = NULL;  // last appli whose class is older than ours => we'll go just after.
			for (ic = first_appli_ic; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if (! CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) && ! CAIRO_DOCK_IS_MULTI_APPLI (icon))
					break;

				// get the age of this class (= age of the oldest icon of this class)
				CairoDockClassAppli *pOtherClassAppli = _cairo_dock_lookup_class_appli (icon->cClass);
				if (! pOtherClassAppli || ! pOtherClassAppli->pAppliOfClass)  // should never happen
					continue;

				Icon *pOldestAppli = g_list_last (pOtherClassAppli->pAppliOfClass)->data;  // prepend

				// compare to our class.
				if (pOldestAppli->pAppli->iAge < iAge)  // it's older than our class -> skip this whole class, we'll go after.
				{
					while (ic->next != NULL)
					{
						icon = ic->next->data;
						if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) && icon->cClass && strcmp (icon->cClass, pOldestAppli->cClass) == 0)  // next icon is an appli of the same class -> skip
							ic = ic->next;
						else
							break;
					}
					last_appli_ic = ic;
				}
				else  // we are older -> discard and quit.
				{
					break;
				}
			}

			if (last_appli_ic == NULL)  // we are the oldest class -> go just before the first appli
			{
				pIcon->fOrder = _get_previous_order (first_appli_ic);
			}
			else  // go just after the last one
			{
				pIcon->fOrder = _get_next_order (last_appli_ic);
			}
		}
		else  // no appli, use the defined placement.
		{
			pIcon->fOrder = _get_first_appli_order (pDock, first_launcher_ic, last_launcher_ic);
		}
	}
}


const gchar *cairo_dock_get_class_name (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return pClassAppli ? pClassAppli->cName : NULL;
}
const gchar *cairo_dock_get_class_desktop_file (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli && pClassAppli->app && G_IS_DESKTOP_APP_INFO (pClassAppli->app->app))
		return g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (pClassAppli->app->app));
	return NULL;
}

const gchar *cairo_dock_get_class_icon (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return pClassAppli ? pClassAppli->cIcon : NULL;
}

const gchar *cairo_dock_get_class_wm_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	if (pClassAppli->cStartupWMClass == NULL)  // if the WMClass has not been retrieved beforehand, do it now
	{
		cd_debug ("retrieve WMClass for %s...", cClass);
		Icon *pIcon;
		GList *ic;
		for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->pAppli && pIcon->pAppli->cWmClass)
			{
				pClassAppli->cStartupWMClass = g_strdup (pIcon->pAppli->cWmClass);
				break;
			}
		}
	}

	return pClassAppli->cStartupWMClass;
}

GldiAppInfo *cairo_dock_get_class_app_info (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return pClassAppli ? pClassAppli->app : NULL;
}

const gchar* const *cairo_dock_get_class_actions (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli && pClassAppli->app) ? pClassAppli->app->actions : NULL;
}

const CairoDockImageBuffer *cairo_dock_get_class_image_buffer (const gchar *cClass)
{
	static CairoDockImageBuffer image;
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);
	Icon *pIcon;
	GList *ic;
	for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon) && pIcon->image.pSurface)  // avoid applets
		{
			memcpy (&image, &pIcon->image, sizeof (CairoDockImageBuffer));
			return &image;
		}
	}
	for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->image.pSurface)
		{
			memcpy (&image, &pIcon->image, sizeof (CairoDockImageBuffer));
			return &image;
		}
	}

	return NULL;
}


/**
 * Attempt to find a desktop file for an app given its app-id, class, or desktop file path.
 * 
 * If cDesktopFile is a valid path, try to open it and return the
 * corresponding GDesktopAppInfo. If the return value is non-NULL,
 * it's reference count has been incremented already and the caller
 * must call g_object_unref() on it if it is no longer needed.
 * 
 * Alternatively, search our database of system-wide .desktop files,
 * including workarounds for several known inconsistencies (by adding
 * prefices). All comparisons are case-insensitive.
 * 
 * If bOnlyExact, only the given string is searched (both among filenames and
 * StartupWMClass keys), and heuristics are not attempted.
 * 
 * If bIsDesktopFile, cDekstopFile can be an actual filename and is processed accordingly.
 */
static GDesktopAppInfo *_search_desktop_file (const gchar *cDesktopFile, gboolean bOnlyExact, gboolean bIsDesktopFile)
{
	if (cDesktopFile == NULL)
		return NULL;
	gchar *tmp_to_free = NULL;
	gboolean bPath = FALSE;
	if (*cDesktopFile == '/') 
	{
		if (!bIsDesktopFile)
		{
			cd_warning ("Got absolute path, but bIsDesktopFile == FALSE!\n");
			bIsDesktopFile = TRUE;
		}
		if (g_file_test (cDesktopFile, G_FILE_TEST_EXISTS))  // it's a path and it exists.
		{
			GDesktopAppInfo *app = g_desktop_app_info_new (cDesktopFile);
			if (app) return app;
		}
		// if not found, try searching based on the basename
		tmp_to_free = g_path_get_basename (cDesktopFile);
		cDesktopFile = tmp_to_free;
		bPath = TRUE;
	}
	
	if (bIsDesktopFile)
		{
		// remove the .desktop suffix and convert to lower case if it is present
		// note: if cDesktopFile does not end with ".desktop", then it has been
		// processed (and converted to lower case) before
		gchar *suffix = g_strrstr (cDesktopFile, ".desktop");
		if (suffix)
		{
			gchar *to_free2 = tmp_to_free;
			tmp_to_free = g_ascii_strdown (cDesktopFile, suffix - cDesktopFile);
			cDesktopFile = tmp_to_free;
			g_free (to_free2);
		}
	}
	
	// normal case: we have the correct name
	// (if bPath == FALSE, we want to match a class, which can happen based on the .desktop file contents)
	GDesktopAppInfo *app = gldi_desktop_file_db_lookup (cDesktopFile, bPath);
	if (app || bPath || bOnlyExact)
	{
		// if it was found return it; also if the query was an absolute
		// path, we don't try the heuristics (we are only interested if
		// the exact same file is found in some other location)
		g_free (tmp_to_free); // can be NULL
		if (app) g_object_ref (app);
		return app; // can be NULL
	}
	
	// handle potential partial matches and special cases
	// #0: special casing for Gnome Terminal, required at least on Ubuntu 22.04 and 24.04
	// (should be fixed in newer versions, see e.g. here: https://gitlab.gnome.org/GNOME/gnome-terminal/-/issues/8033)
	if (!strcmp (cDesktopFile, "gnome-terminal-server"))
	{
		const char *tmpkey = "org.gnome.terminal";
		app = gldi_desktop_file_db_lookup (tmpkey, TRUE); // we want exact match for org.gnome.terminal.desktop
		if (app)
		{
			g_free (tmp_to_free); // can be NULL
			return g_object_ref (app);
		}
	}
	
	GString *sID = g_string_new (NULL);
	
	/* #1: add common prefices
	 * e.g. org.gnome.Evince.desktop, but app-id is only evince on Ubuntu 22.04 and 24.04
	 * More generally, this can happen with GTK+3 apps that have only "partially" migrated
	 * to using the "new" (reverse DNS style) .desktop format.
	 * See e.g.
	 * https://gitlab.gnome.org/GNOME/gtk/-/issues/2822
	 * https://gitlab.gnome.org/GNOME/gtk/-/issues/2034
	 * https://honk.sigxcpu.org/con/GTK__and_the_application_id.html
	 * https://docs.gtk.org/gtk4/migrating-3to4.html#set-a-proper-application-id
	 */
	const char *prefices[] = {"org.gnome.", "org.kde.", "org.freedesktop.", NULL};
	int j;
	
	for (j = 0; prefices[j]; j++)
	{
		g_string_printf (sID, "%s%s", prefices[j], cDesktopFile);
		app = gldi_desktop_file_db_lookup (sID->str, TRUE); // we want exact match for the file name
		if (app) break;
	}
	
	if (!app)
	{
		// #2: snap "namespaced" names -- these could be anything, we just handle the "common" case where
		// simply the app-id is duplicated (e.g. "firefox_firefox.desktop" as on Ubuntu 22.04 and 24.04)
		g_string_printf (sID, "%s_%s", cDesktopFile, cDesktopFile);
		app = gldi_desktop_file_db_lookup (sID->str, TRUE); // we want exact match for the file name
	}
	
	g_free (tmp_to_free); // can be null
	g_string_free(sID, TRUE);
	if (app) g_object_ref (app);
	return app;
}

gchar *cairo_dock_guess_class (const gchar *cCommand, const gchar *cStartupWMClass)
{
	// Several cases are possible:
	// Exec=toto
	// Exec=toto-1.2
	// Exec=toto -x -y
	// Exec=/path/to/toto -x -y
	// Exec=gksu nautilus /  (or kdesu)
	// Exec=su-to-root -X -c /usr/sbin/synaptic
	// Exec=gksu --description /usr/share/applications/synaptic.desktop /usr/sbin/synaptic
	// Exec=wine "C:\Program Files\Starcraft\Starcraft.exe"
	// Exec=wine "/path/to/prog.exe"
	// Exec=env WINEPREFIX="/home/fab/.wine" wine "C:\Program Files\Starcraft\Starcraft.exe"

	cd_debug ("%s (%s, '%s')", __func__, cCommand, cStartupWMClass);
	gchar *cResult = NULL;
	if (cStartupWMClass == NULL || *cStartupWMClass == '\0' || strcmp (cStartupWMClass, "Wine") == 0)  // special case for wine, because even if the class is defined as "Wine", this information is non-exploitable.
	{
		if (cCommand == NULL || *cCommand == '\0')
			return NULL;
		gchar *cDefaultClass = g_ascii_strdown (cCommand, -1);
		gchar *str;
		const gchar *cClass = cDefaultClass;  // pointer to the current class.

		if (strncmp (cClass, "gksu", 4) == 0 || strncmp (cClass, "kdesu", 5) == 0 || strncmp (cClass, "su-to-root", 10) == 0)  // we take the end
		{
			str = (gchar*)cClass + strlen(cClass) - 1;  // last char.
			while (*str == ' ')  // by security, we remove spaces at the end of the line.
				*(str--) = '\0';
			str = strchr (cClass, ' ');  // first whitespace.
			if (str != NULL)  // we are looking after that.
			{
				while (*str == ' ')
					str ++;
				cClass = str;
			}  // we remove gksu, kdesu, etc..
			if (*cClass == '-')  // if it's an option: we need the last param.
			{
				str = strrchr (cClass, ' ');  // last whitespace.
				if (str != NULL)  // we are looking after that.
					cClass = str + 1;
			}
			else  // we can use the first param
			{
				str = strchr (cClass, ' ');  // first whitespace.
				if (str != NULL)  // we remove everything after that
					*str = '\0';
			}

			str = strrchr (cClass, '/');  // last '/'.
			if (str != NULL)  // remove after that.
				cClass = str + 1;
		}
		else if ((str = g_strstr_len (cClass, -1, "wine ")) != NULL)
		{
			cClass = str;  // class = wine, better than nothing.
			*(str+4) = '\0';
			str += 5;
			while (*str == ' ')  // we remove extra whitespaces.
				str ++;
			// we try to find the executable which is used by wine as res_name.
			gchar *exe = g_strstr_len (str, -1, ".exe");
			if (!exe)
				exe = g_strstr_len (str, -1, ".EXE");
			if (exe)
			{
				*exe = '\0';  // remove the extension.
				gchar *slash = strrchr (str, '\\');
				if (slash)
					cClass = slash+1;
				else
				{
					slash = strrchr (str, '/');
					if (slash)
						cClass = slash+1;
					else
						cClass = str;
				}
			}
			cd_debug ("  special case : wine application => class = '%s'", cClass);
		}
		else
		{
			if (!strncmp (cClass, "env ", 4))
			{
				// e.g. env BAMF_DESKTOP_FILE_HINT=/var/lib/snapd/desktop/applications/firefox_firefox.desktop /snap/bin/firefox %u
				// we want to extract "firefox" from this
				gint argc;
				gchar **argv = NULL;
				gboolean parsed = g_shell_parse_argv (cClass, &argc, &argv, NULL);
				cClass = NULL;
				
				if (parsed)
				{
					int i;
					gboolean have_var = FALSE;
					for (i = 1; i < argc; i++)
					{
						if (strchr (argv[i], '=')) {
							// first argument that is likely actually setting an environment variable
							if (argv[i][0] != '-') have_var = TRUE;
						}
						else if (have_var)
						{
							// first argument which is not an environment variable
							// and likely not a command line option
							g_free (cDefaultClass);
							cDefaultClass = g_strdup (argv[i]);
							cClass = cDefaultClass;
							break;
						}
					}
				}
				
				g_strfreev (argv);
			}
			else if (!strncmp (cClass, "python3 ", 8) || !strncmp (cClass, "python ", 7))
			{
				cClass += 7;
				while (*cClass == ' ') cClass++;
				if (*cClass == 0 || *cClass == '-')
					cClass = NULL; // we cannot parse Python's arguments, do not attempt to do so
			}
			
			// TODO: handle sh -c cases as well?
			
			if (cClass)
			{
				while (*cClass == ' ')  // by security, remove extra whitespaces.
					cClass ++;
				str = strchr (cClass, ' ');  // first whitespace.
				if (str != NULL)  // remove everything after that
					*str = '\0';
				str = strrchr (cClass, '/');  // last '/'.
				if (str != NULL)  // we take after that.
					cClass = str + 1;
				str = strchr (cClass, '.');  // we remove all .xxx otherwise we can't detect the lack of extension when looking for an icon (openoffice.org) or it's a problem when looking for an icon (jbrout.py).
				if (str != NULL && str != cClass)
					*str = '\0';
			}
		}

		if (cClass && *cClass != '\0')
		{
			// handle the cases of programs where command != class.
			if (strncmp (cClass, "oo", 2) == 0)
			{
				if (strcmp (cClass, "ooffice") == 0 || strcmp (cClass, "oowriter") == 0 || strcmp (cClass, "oocalc") == 0 || strcmp (cClass, "oodraw") == 0 || strcmp (cClass, "ooimpress") == 0)  // openoffice poor design: there is no way to bind its windows to the launcher without this trick.
					cClass = "openoffice";
			}
			else if (strncmp (cClass, "libreoffice", 11) == 0)  // libreoffice has different classes according to the launching option (--writer => libreoffice-writer, --calc => libreoffice-calc, etc)
			{
				gchar *str = strchr (cCommand, ' ');
				if (str && *(str+1) == '-')
				{
					g_free (cDefaultClass);
					cDefaultClass = g_strdup_printf ("%s%s", "libreoffice", str+2);
					str = strchr (cDefaultClass, ' ');  // remove the additionnal params of the command.
					if (str)
						*str = '\0';
					cClass = cDefaultClass;  // "libreoffice-writer"
				}
			}
			
			// final result
			cResult = g_strdup (cClass);
		}
		g_free (cDefaultClass);
	}
	else
		cResult = g_ascii_strdown (cStartupWMClass, -1);
	
	if (cResult)
	{
		// remove some suffices that can be problematic: .exe, .py (note: cResult is already lowercase here)
		if (g_str_has_suffix (cResult, ".exe")) cResult[strlen (cResult) - 4] = 0;
		else if (g_str_has_suffix (cResult, ".py")) cResult[strlen (cResult) - 3] = 0;
		cairo_dock_remove_version_from_string (cResult);
	}
	cd_debug (" -> '%s'", cResult);

	return cResult;
}



/** Register an application class from apps installed on the system -- internal version.
* @param cSearchTerm class name to search for; can be desktop file path or class name or app-id;
* 	preprocessed accordingly with cairo_dock_guess_class () or gldi_window_parse_class ()
* @param cFallbackClass alternate class name to try looking up if cClassName is not found
*   (e.g. on X11 the "name" part of WM_CLASS as some apps put the proper desktop file ID there). Should be
*   converted to lowercase by the caller.
* @param cWmClass StartupWMClass key from a custom launcher or the class / app-id as reported by the WM
* 	without any processing
* @param bUseWmClass if TRUE, and cWmClass != NULL, cWmClass will also added as a key
* @param bCreateAlways if TRUE, a dummy class will be created if not found
* @param bIsDesktopFile if TRUE, cSearchTerm is assumed to be the name of a .desktop file (and processed accordingly)
* @param pResult store the found / created CairoDockClassAppli here for further processing
* @return the class ID in a newly allocated string (can be used to retrieve class properties later).
* 
* Exactly one of cDesktopFile or cClassName should be supplied. These are processed in the exact same
* way when searching for apps, meaning:
*  - first, it is checked if a registered class exists with either of these or cWmClass as a key
*  - next, they are supplied to _search_desktop_file which tries to find a matching .desktop file
*     using a set of heuristics
* 
* The difference is that if cDesktopFile is given and not found, this function fails: no new class is
* registered and returns NULL. In contrast, if cClassName is given and not found, an new, empty class
* is registered and (a copy of) cClassName is returned. Also, if an app is found, cClassName is always
* added as a key (so if cClassName != NULL, the return value is always a copy of it).
*/
static gchar *_cairo_dock_register_class_full (const gchar *cSearchTerm, const gchar *cFallbackClass, const gchar *cWmClass,
	gboolean bUseWmClass, gboolean bCreateAlways, gboolean bIsDesktopFile, GDesktopAppInfo *app, CairoDockClassAppli **pResult)
{
	g_return_val_if_fail ((cSearchTerm || app), NULL);
	
	gchar *cClass = NULL;
	CairoDockClassAppli *pClassAppli = NULL;
	
	if (!cSearchTerm)
	{
		cSearchTerm = g_app_info_get_id (G_APP_INFO (app));
		if (!cSearchTerm) return NULL;
	}
	
	//\__________________ if the class is already registered and filled, quit.
	// note: in many cases, this will be non-NULL (if we encountered cClass before but did not register it)
	pClassAppli = _cairo_dock_lookup_class_appli (cSearchTerm);
	if (!pClassAppli && cFallbackClass)
	{
		pClassAppli = _cairo_dock_lookup_class_appli (cFallbackClass);
		if (pClassAppli)
		{
			// save the original class key as well (takes ownership of cClass)
			g_hash_table_insert (s_hAltClass, g_strdup (cSearchTerm), pClassAppli);
			// adjust the return value (for line 2114)
			cClass = g_strdup (cFallbackClass);
		}
	}

	if (pClassAppli)  // we already searched this class (we only add to the hash table in this function)
	{
		//g_print ("class %s already known (%s)\n", cClass?cClass:cDesktopFile, pClassAppli->cDesktopFile);
		if (pClassAppli->cStartupWMClass == NULL && cWmClass != NULL)  // if the cStartupWMClass was not stored before, do it now.
			pClassAppli->cStartupWMClass = g_strdup (cWmClass);
		if (bUseWmClass && cWmClass != NULL && !g_hash_table_contains (s_hAltClass, cWmClass))
			g_hash_table_insert (s_hAltClass, g_strdup (cWmClass), pClassAppli);
		//g_print ("%s --> %s\n", cClass, pClassAppli->cStartupWMClass);
		if (pResult) *pResult = pClassAppli;
		return cClass ? cClass : g_strdup (cSearchTerm);
	}
	
	if (cWmClass)
	{
		pClassAppli = _cairo_dock_lookup_class_appli (cWmClass);
		if (pClassAppli && pClassAppli->app)
		{
			if (pResult) *pResult = pClassAppli;
			return g_strdup (cWmClass);
		}
	}

	if (app) g_object_ref (app); // will be unrefed later

	//\__________________ search the desktop file's path.
	if (!app && cFallbackClass)
	{
		// in this case, we do a two-stage search: first we try exact matches, then using heuristics
		// this is to avoid edge cases where cFallbackClass would be an exact match
		app = _search_desktop_file (cSearchTerm, TRUE, bIsDesktopFile);
		if (!app)
		{
			app = _search_desktop_file (cFallbackClass, TRUE, FALSE);
			if (!app && cWmClass)
			{
				// note: cWmClass can contain upper-case characters, but we only
				// store lower-case versions
				gchar *tmp = g_ascii_strdown (cWmClass, -1);
				app = _search_desktop_file (tmp, TRUE, FALSE);
				g_free (tmp);
			}
		}
	}
	
	if (!app)
	{
		//!! TODO: maybe we should not allow heuristics for .desktop file names?
		/// (e.g. should "terminal.desktop" match "org.gnome.Terminal.desktop" ?)
		app = _search_desktop_file (cSearchTerm, FALSE, bIsDesktopFile);
		if (!app)
		{
			if (cFallbackClass)
			{
				app = _search_desktop_file (cFallbackClass, FALSE, FALSE);
			}
			else if (cWmClass)
			{
				// if cFallbackClass != NULL, we did this search already above
				gchar *tmp = g_ascii_strdown (cWmClass, -1); // note: will already be lowercase if read from a launcher
				app = _search_desktop_file (tmp, TRUE, FALSE);
				g_free (tmp);
			}
		}
	}
	
	if (!app)  // couldn't find the .desktop
	{
		if (bCreateAlways)  // make a class anyway to store the few info we have.
		{
			if (!pClassAppli)
			{
				pClassAppli = g_new0 (CairoDockClassAppli, 1);
				g_hash_table_insert (s_hClassTable, g_strdup (cSearchTerm), pClassAppli);
			}
			else g_hash_table_insert (s_hAltClass, g_strdup (cSearchTerm), pClassAppli);

			if (pClassAppli->cStartupWMClass == NULL && cWmClass != NULL)
				pClassAppli->cStartupWMClass = g_strdup (cWmClass);
			//g_print ("%s ---> %s\n", cClass, pClassAppli->cStartupWMClass);
			pClassAppli->bSearchedAttributes = TRUE;
			if (bUseWmClass && cWmClass != NULL && !g_hash_table_contains (s_hAltClass, cWmClass))
				g_hash_table_insert (s_hAltClass, g_strdup (cWmClass), pClassAppli);
			if (pResult) *pResult = pClassAppli;
			return g_strdup (cSearchTerm);
		}
		cd_debug ("couldn't find the desktop file %s", cSearchTerm);
		return NULL;
	}
	
	//\__________________ open it. -- TODO: use g_app_info_get_id () instead?
	const gchar *cDesktopFilePath = g_desktop_app_info_get_filename (app);
	cd_debug ("+ parsing class desktop file %s...", cDesktopFilePath);

	//\__________________ guess the class name.
	const gchar *cCommand = g_app_info_get_commandline (G_APP_INFO(app));
	const gchar *cStartupWMClass = g_desktop_app_info_get_startup_wm_class (app);
	if (cStartupWMClass && *cStartupWMClass == '\0')
		cStartupWMClass = NULL;

	/* We have four potential sources for the "class" of an application:
	 * (1) cSearchTerm -- the app-id / class reported for an open app
	 * 		(this is already parsed by gldi_window_parse_class () as opposed to the
	 * 		 cWmClass variable which is the "raw" value reported by the WM
	 * 		 or the StartupWMClass key read from our own config file)
	 * (2) the basename of the desktop file from cDesktopFilePath
	 * 		always available if we are here (either from cDesktopFile
	 * 		or matched above)
	 * (3) the StartupWMClass or Exec key from the desktop file -- note:
	 * 		this is not necessarily unique, multiple desktop files can
	 * 		have the same key or command
	 * (4) the StartupWMClass key in a launcher's .desktop file -> this
	 * 		is given here as the cWmClass variable if bUseWmClass == TRUE
	 * 		and cClassName == NULL => cClass == NULL
	 * Obviously, if we are loading a launcher, (1) is not available.
	 * Note: all of these will be lowercase ((1) and (3) converted when
	 * parsing, (2) converted below).
	 * 
	 * On X11, (1) and (3) should be the same (if (3) is given). On
	 * Wayland, (1) and (2) should be the same. However, there are
	 * exceptions. Also, running XWayland apps can complicate things. In
	 * theory, having three different values is possible if running the
	 * app-id / class does not match the StartupWMClass, but also does
	 * not match the desktop file name because it does not include the
	 * reverse DNS of the publisher, which was however correctly guessed
	 * by us (see _search_desktop_file () above).
	 * We deal with this by storing (1) or (2) as the "class" in
	 * s_hClassTable, and the others in s_hAltClass, so no matter how an
	 * app identifies itself, it will be matched to its icon.
	 * Note: (3) is always put in s_hAltClass since it can be non-unique
	 * and it is ignored if it is already present when registering a new
	 * app that has (1) or (2) not matching a previously registered app. */
	gchar *cDesktopFileID = NULL; // (2)
	gchar *cAltClass2 = NULL; // (3)
	{
		gchar *tmp = g_path_get_basename (cDesktopFilePath);
		gchar *tmp2 = g_ascii_strdown (tmp, -1);
		g_free (tmp);
		if (g_str_has_suffix (tmp2, ".desktop"))
		{
			int len = strlen (tmp2);
			tmp2[len - 8] = 0; // here, len >= 8
		}
		cDesktopFileID = tmp2;
	}
	// potential "class" from the desktop file (3)
	if (cCommand || cStartupWMClass)
		cAltClass2 = cairo_dock_guess_class (cCommand, cStartupWMClass);
	// take care of duplicates
	if (cAltClass2 && !strcmp(cDesktopFileID, cAltClass2))
	{
		g_free (cAltClass2);
		cAltClass2 = NULL;
	}
	if (!bIsDesktopFile)
	{
		// cDesktopFileID != NULL here
		if (!strcmp (cSearchTerm, cDesktopFileID))
		{
			// nothing to do, cClass can be NULL, we use cDesktopFileID instead
		}
		else
		{
			if (cAltClass2 && !strcmp (cSearchTerm, cAltClass2))
			{
				cClass = cAltClass2;
				cAltClass2 = NULL;
			}
			else cClass = g_strdup (cSearchTerm);
		}
	}
	
	//\__________________ make a new class or get the existing one.
	{
		CairoDockClassAppli *pDesktopIDAppli = _cairo_dock_lookup_class_appli (cDesktopFileID);
		
		/*
		 * Note: here cDesktopFileID != NULL, while cClass can be NULL.
		 * If cClass != NULL, it was searched before and not found.
		 * pDesktopIDAppli can be non-NULL if this app was found before; in this
		 * case, it will have bSearchedAttributed == TRUE.
		 */
		
		if (pDesktopIDAppli)
		{
			pClassAppli = pDesktopIDAppli;
			if (cClass)
			{
				g_hash_table_insert (s_hAltClass, g_strdup (cClass), pClassAppli);
				g_free (cDesktopFileID);
			}
			else cClass = cDesktopFileID;
		}
		else
		{
			if (!cClass)
			{
				cClass = cDesktopFileID;
				cDesktopFileID = NULL;
			}
			// need to create a new class
			pClassAppli = g_new0 (CairoDockClassAppli, 1);
			struct _GldiAppInfoAttr attr;
			attr.app = G_APP_INFO (app);
			attr.cCmdline = NULL;
			attr.bNeedsTerminal = FALSE; // will be looked up from app
			attr.cWorkingDir = NULL; // will be looked up from app
			pClassAppli->app = (GldiAppInfo*) gldi_object_new (&myAppInfoObjectMgr, &attr); // will ref app
			g_hash_table_insert (s_hClassTable, g_strdup (cClass), pClassAppli);
			if (cDesktopFileID) g_hash_table_insert (s_hAltClass, cDesktopFileID, pClassAppli);
		}
		
		/* here, pClassAppli != NULL
		 * 
		 * We need to check cAltClass2 which can only be stored in s_hAltClass
		 * (it is only potentially added here). If it is found, it is likely a
		 * spurious match for another app that has the same command line or the
		 * same StartupWMClass key, we can ignore it. If it is not found, we
		 * add as a potential alternative ID for this app.
		 */
		if (cAltClass2 && !g_hash_table_contains (s_hAltClass, cAltClass2))
			g_hash_table_insert (s_hAltClass, cAltClass2, pClassAppli);
		else g_free (cAltClass2);
		
		if (bUseWmClass && cWmClass != NULL && !g_hash_table_contains (s_hAltClass, cWmClass))
			g_hash_table_insert (s_hAltClass, g_strdup (cWmClass), pClassAppli);
		// by here, we freed or added as a hash table key both
		// cAltClass2 and cDesktopFileID
	}
	g_object_unref (app); // g_object_ref () was called above if making a new copy
	
	// here, we have a valid pClassAppli, store it if required
	if (pResult) *pResult = pClassAppli;
	
	// we store the WM class (class or app_id as reported by the WM without
	// any processing) if it was not stored before
	// (note: this is always the class we get from the WM and NOT the
	// StartupWMClass in the .desktop file as that might
	// not match what we have in reality)
	if (cWmClass != NULL &&
			(pClassAppli->cStartupWMClass == NULL || // no class was stored before
			bIsDesktopFile)) // or, we are reading a launcher and there was a custom StartupWMClass set
	{
		g_free (pClassAppli->cStartupWMClass);
		pClassAppli->cStartupWMClass = g_strdup (cWmClass);
	}

	//\__________________ if we already searched and found the attributes beforehand, quit.
	if (pClassAppli->bSearchedAttributes)
	{
		//g_print ("%s ----> %s\n", cClass, pClassAppli->cStartupWMClass);
		return cClass;
	}
	pClassAppli->bSearchedAttributes = TRUE;

	//\__________________ get the attributes.
	pClassAppli->cName = g_strdup (g_app_info_get_name (G_APP_INFO (app)));

	// TODO: use g_app_info_get_icon () instead of this?
	pClassAppli->cIcon = g_desktop_app_info_get_string (app, "Icon");
	if (pClassAppli->cIcon != NULL && *pClassAppli->cIcon != '/')  // remove any extension.
	{
		gchar *str = strrchr (pClassAppli->cIcon, '.');
		if (str && (strcmp (str+1, "png") == 0 || strcmp (str+1, "svg") == 0 || strcmp (str+1, "xpm") == 0))
			*str = '\0';
	}
	
	cd_debug (" -> class '%s'", cClass);
	return cClass;
}

gchar *cairo_dock_register_class2 (const gchar *cSearchTerm, const gchar *cWmClass, gboolean bCreateAlways, gboolean bIsDesktopFile)
{
	return _cairo_dock_register_class_full (cSearchTerm, NULL, cWmClass, TRUE, bCreateAlways, bIsDesktopFile, NULL, NULL);
}

gchar *cairo_dock_register_class (const gchar *cSearchTerm)
{
	return cairo_dock_register_class2 (cSearchTerm, NULL, FALSE, TRUE); // for compatibility, cSearchTerm can be a .desktop file name
}


void cairo_dock_set_data_from_class (const gchar *cClass, Icon *pIcon)
{
	g_return_if_fail (cClass != NULL && pIcon != NULL);
	cd_debug ("%s (%s)", __func__, cClass);

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli == NULL || ! pClassAppli->bSearchedAttributes)
	{
		cd_debug ("no class %s or no attributes", cClass);
		return;
	}

	if (pIcon->cName == NULL)
		pIcon->cName = g_strdup (pClassAppli->cName);

	if (pIcon->cFileName == NULL)
		pIcon->cFileName = g_strdup (pClassAppli->cIcon);
	
	if (pIcon->pAppInfo)
	{
		// should not happen, this function is only called for newly created icons
		cd_warning ("app corresponding to this icon was already set!");
		gldi_object_unref (GLDI_OBJECT (pIcon->pAppInfo));
	}
	if (pClassAppli->app)
	{
		pIcon->pAppInfo = pClassAppli->app;
		gldi_object_ref (GLDI_OBJECT (pIcon->pAppInfo));
	}
	else pIcon->pAppInfo = NULL;
}


static void _gldi_class_appli_startup_notify_end (CairoDockClassAppli *pClassAppli);

static gboolean _stop_opening_timeout (CairoDockClassAppli *pClassAppli)
{
	pClassAppli->iSidOpeningTimeout = 0;
	_gldi_class_appli_startup_notify_end (pClassAppli);
	return FALSE;
}
void gldi_class_startup_notify (Icon *pIcon)
{
	const gchar *cClass = pIcon->cClass;
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (! pClassAppli || pClassAppli->bIsLaunching)
		return;

	// mark the class as launching and set a timeout
	pClassAppli->bIsLaunching = TRUE;
	if (pClassAppli->iSidOpeningTimeout == 0)
		pClassAppli->iSidOpeningTimeout = g_timeout_add_seconds (15,  // 15 seconds, for applications that take a really long time to start
		(GSourceFunc) _stop_opening_timeout, pClassAppli);  // we can give pClassAppli as parameter, as we would remove the timeout if it is destroyed

	// mark the icon as launching (this is just for convenience for the animations)
	gldi_icon_mark_as_launching (pIcon);
}

static void _gldi_class_appli_startup_notify_end (CairoDockClassAppli *pClassAppli)
{
	// unset the icons as launching
	GList* ic;
	Icon *icon;
	for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		gldi_icon_stop_marking_as_launching (icon);
	}
	for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		gldi_icon_stop_marking_as_launching (icon);
	}
	if (pClassAppli->cDockName != NULL)  // also unset the class-icon, if any
	{
		CairoDock *pClassSubDock = gldi_dock_get (pClassAppli->cDockName);
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pClassSubDock, NULL);
		if (pPointingIcon != NULL)
			gldi_icon_stop_marking_as_launching (pPointingIcon);
	}

	// unset the class as launching and stop a timeout
	pClassAppli->bIsLaunching = FALSE;
	if (pClassAppli->iSidOpeningTimeout != 0)
	{
		g_source_remove (pClassAppli->iSidOpeningTimeout);
		pClassAppli->iSidOpeningTimeout = 0;
	}
}

void gldi_class_startup_notify_end (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (! pClassAppli || ! pClassAppli->bIsLaunching)
		return;
	_gldi_class_appli_startup_notify_end (pClassAppli);
}


gboolean gldi_class_is_starting (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->iSidOpeningTimeout != 0);
}


void gldi_register_class_manager (void)
{
	// Object Manager
	memset (&myAppInfoObjectMgr, 0, sizeof (GldiObjectManager));
	myAppInfoObjectMgr.cName          = "AppInfo";
	myAppInfoObjectMgr.iObjectSize    = sizeof (GldiAppInfo);
	// interface
	myAppInfoObjectMgr.init_object    = _init_appinfo;
	myAppInfoObjectMgr.reset_object   = _reset_appinfo;
	// signals
	gldi_object_install_notifications (GLDI_OBJECT (&myAppInfoObjectMgr), NB_NOTIFICATIONS_OBJECT);
}


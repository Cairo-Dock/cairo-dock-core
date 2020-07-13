
/*
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
/*****************************************************************************************************
**
** Program:
**    cairo-dock
**
** License :
**    This program is released under the terms of the GNU General Public License, version 3 or above.
**    If you don't know what that means take a look at:
**       http://www.gnu.org/licenses/licenses.html#GPL
**
*********************** VERSION 0 (2006) *********************
**
** Original idea :
**    Mirco "MacSlow" Mueller <macslow@bangang.de>, June 2006.
** With the help of:
**    Behdad Esfahbod <behdad@behdad.org>
**    David Reveman <davidr@novell.com>
**    Karl Lattimer <karl@qdh.org.uk>
** Originally conceived as a stress-test for cairo, librsvg, and glitz.
**
*********************** VERSION 0.1 and above (2007-2014) *********************
**
** author(s) :
**     Fabrice Rey <fabounet@glx-dock.org>
** With the help of:
**     A lot of people !!! (see the About menu and the 'copyright' file)
**
*******************************************************************************/

#include <unistd.h> // sleep, execl
#include <signal.h>

#define __USE_POSIX
#include <time.h>

#include <glib/gstdio.h>
#include <dbus/dbus-glib.h>  // dbus_g_thread_init

#include "config.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_get_first_icon
#include "cairo-dock-module-manager.h"  // gldi_modules_new_from_directory
#include "cairo-dock-module-instance-manager.h"  // GldiModuleInstance
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialog-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-utils.h"  // cairo_dock_launch_command
#include "cairo-dock-core.h"

#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-user-interaction.h"
#include "cairo-dock-user-menu.h"

//#define CAIRO_DOCK_THEME_SERVER "http://themes.glx-dock.org"
#define CAIRO_DOCK_THEME_SERVER "http://download.tuxfamily.org/glxdock/themes"
#define CAIRO_DOCK_BACKUP_THEME_SERVER "http://fabounet03.free.fr"
// Nom du repertoire racine du theme courant.
#define CAIRO_DOCK_CURRENT_THEME_NAME "current_theme"
// Nom du repertoire des themes extras.
#define CAIRO_DOCK_EXTRAS_DIR "extras"

extern gchar *g_cCairoDockDataDir;
extern gchar *g_cCurrentThemePath;

extern gchar *g_cConfFile;
extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;

extern CairoDock *g_pMainDock;
extern CairoDockGLConfig g_openglConfig;

extern gboolean g_bUseOpenGL;
extern gboolean g_bEasterEggs;

#ifdef HAVE_GTK_LAYER_SHELL
extern gboolean g_bDisableLayerShell;
#endif

extern GldiModuleInstance *g_pCurrentModule;
extern GtkWidget *cairo_dock_build_simple_gui_window (void);

gboolean g_bForceCairo = FALSE;
gboolean g_bLocked;

extern gboolean g_bForceWayland;
extern gboolean g_bForceX11;

static gboolean s_bSucessfulLaunch = FALSE;
static GString *s_pLaunchCommand = NULL;
static gchar *s_cLastVersion = NULL;
static gchar *s_cDefaulBackend = NULL;
static gint s_iGuiMode = 0;  // 0 = simple mode, 1 = advanced mode
static gint s_iLastYear = 0;
static gint s_iNbCrashes = 0;
static gboolean s_bPingServer = TRUE;
static gboolean s_bCDSessionLaunched = FALSE; // session CD already launched?


static void _on_got_server_answer (const gchar *data, G_GNUC_UNUSED gpointer user_data)
{
	if (data != NULL)
	{
		s_bPingServer = TRUE;  // after we got the answer, we can write in the global file to not try any more.
		gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_BOOLEAN, "Launch", "ping server", s_bPingServer,
			G_TYPE_INVALID);
		g_free (cConfFilePath);
	}
}
static gboolean _cairo_dock_successful_launch (gpointer data)
{
	s_bSucessfulLaunch = TRUE;
	// new year greetings.
	time_t t = time (NULL);
	struct tm st;
	localtime_r (&t, &st);
	
	if (!data && st.tm_mday <= 15 && st.tm_mon == 0 && s_iLastYear < st.tm_year + 1900)  // first 2 weeks of january + not first launch
	{
		s_iLastYear = st.tm_year + 1900;
		gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_INT, "Launch", "last year", s_iLastYear,
			G_TYPE_INVALID);
		g_free (cConfFilePath);
		
		Icon *pIcon = gldi_icons_get_any_without_dialog ();
		gchar *cMessage = g_strdup_printf (_("Happy new year %d !!!"), s_iLastYear);
		gchar *cMessageFull = g_strdup_printf ("\n%s :-)\n", cMessage);
		gldi_dialog_show_temporary_with_icon (cMessageFull, pIcon, CAIRO_CONTAINER (g_pMainDock), 15000., CAIRO_DOCK_SHARE_DATA_DIR"/icons/balloons.png");
		g_free (cMessageFull);
		g_free (cMessage);
	}
	
	if (! s_bPingServer && g_str_has_suffix (g_cCairoDockDataDir, CAIRO_DOCK_DATA_DIR))  // the server (which hosts themes, third-party applets and packages) has never been accessed yet, ping it once
	{
		s_bPingServer = TRUE;
		cairo_dock_get_url_data_async (CAIRO_DOCK_THEME_SERVER"/ping.txt", (GFunc)_on_got_server_answer, NULL);
	}
	
	return FALSE;
}
static gboolean _cairo_dock_first_launch_setup (G_GNUC_UNUSED gpointer data)
{
	cairo_dock_launch_command (CAIRO_DOCK_SHARE_DATA_DIR"/scripts/initial-setup.sh");
	return FALSE;
}
static void _cairo_dock_quit (G_GNUC_UNUSED int signal)
{
	gtk_main_quit ();
}
/* Crash at startup:
 *  - First 2 crashes: retry with a delay of 2 sec (maybe due to a problem at startup)
 *  - 3th crash: remove the applet and restart the dock
 *  - 4th crash: show the maintenance mode
 *  - 5th crash: quit
 */
static void _cairo_dock_intercept_signal (int signal)
{
	cd_warning ("Cairo-Dock has crashed (sig %d).\nIt will be restarted now.\nFeel free to report this bug on glx-dock.org to help improving the dock!", signal);
	g_print ("info on the system :\n");
	int r = system ("uname -a");
	if (r < 0)
		cd_warning ("Not able to launch this command: uname");
	
	// if a module is responsible, expose it to public shame.
	if (g_pCurrentModule != NULL)
	{
		g_print ("The applet '%s' may be the culprit", g_pCurrentModule->pModule->pVisitCard->cModuleName);
		if (! s_bSucessfulLaunch)  // else, be quiet.
			g_string_append_printf (s_pLaunchCommand, " -x \"%s\"", g_pCurrentModule->pModule->pVisitCard->cModuleName);
	}
	else
	{
		g_print ("Couldn't guess if it was an applet's fault or not. It may have crashed inside the core or inside a thread\n");
	}
	
	// if the crash occurs on startup, take additionnal measures; else just respawn quietly.
	if (! s_bSucessfulLaunch)  // a crash on startup,
	{
		if (s_iNbCrashes < 2)  // the first 2 crashes, restart with a delay (in case we were launched too early on startup).
			g_string_append (s_pLaunchCommand, " -w 2");  // 2s delay.
		else if (g_pCurrentModule == NULL || s_iNbCrashes == 3)  // crash and no culprit or 4th crash => start in maintenance mode.
			g_string_append (s_pLaunchCommand, " -m");
		g_string_append_printf (s_pLaunchCommand, " -q %d", s_iNbCrashes + 1);  // increment the first-crash counter.
	}  // else a random crash, respawn quietly.
	
	g_print ("restarting with '%s'...\n", s_pLaunchCommand->str);
	execl ("/bin/sh", "/bin/sh", "-c", s_pLaunchCommand->str, (char *)NULL);  // on ne revient pas de cette fonction.
	//execlp ("cairo-dock", "cairo-dock", s_pLaunchCommand->str, (char *)0);
	cd_warning ("Sorry, couldn't restart the dock");
}
static void _cairo_dock_set_signal_interception (void)
{
	signal (SIGSEGV, _cairo_dock_intercept_signal);  // Segmentation violation
	signal (SIGFPE, _cairo_dock_intercept_signal);  // Floating-point exception
	signal (SIGILL, _cairo_dock_intercept_signal);  // Illegal instruction
	signal (SIGABRT, _cairo_dock_intercept_signal);  // Abort // kill -6
}

static gboolean on_delete_maintenance_gui (G_GNUC_UNUSED GtkWidget *pWidget, GMainLoop *pBlockingLoop)
{
	cd_debug ("%s ()", __func__);
	if (pBlockingLoop != NULL && g_main_loop_is_running (pBlockingLoop))
	{
		g_main_loop_quit (pBlockingLoop);
	}
	return FALSE;  // TRUE <=> ne pas detruire la fenetre.
}

static void PrintMuteFunc (G_GNUC_UNUSED const gchar *string) {}

static void _cairo_dock_get_global_config (const gchar *cCairoDockDataDir)
{
	gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", cCairoDockDataDir);
	GKeyFile *pKeyFile = g_key_file_new ();
	if (g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
	{
		g_key_file_load_from_file (pKeyFile, cConfFilePath, 0, NULL);
		s_cLastVersion = g_key_file_get_string (pKeyFile, "Launch", "last version", NULL);
		s_cDefaulBackend = g_key_file_get_string (pKeyFile, "Launch", "default backend", NULL);
		if (s_cDefaulBackend && *s_cDefaulBackend == '\0')
		{
			g_free (s_cDefaulBackend);
			s_cDefaulBackend = NULL;
		}
		s_iGuiMode = g_key_file_get_integer (pKeyFile, "Gui", "mode", NULL);  // 0 by default
		s_iLastYear = g_key_file_get_integer (pKeyFile, "Launch", "last year", NULL);  // 0 by default
		s_bPingServer = g_key_file_get_boolean (pKeyFile, "Launch", "ping server", NULL);  // FALSE by default
		s_bCDSessionLaunched = g_key_file_get_boolean (pKeyFile, "Launch", "cd session", NULL);  // FALSE by default
	}
	else  // first launch or old version, the file doesn't exist yet.
	{
		gchar *cLastVersionFilePath = g_strdup_printf ("%s/.cairo-dock-last-version", cCairoDockDataDir);
		if (g_file_test (cLastVersionFilePath, G_FILE_TEST_EXISTS))
		{
			gsize length = 0;
			g_file_get_contents (cLastVersionFilePath,
				&s_cLastVersion,
				&length,
				NULL);
		}
		g_remove (cLastVersionFilePath);
		g_free (cLastVersionFilePath);
		g_key_file_set_string (pKeyFile, "Launch", "last version", s_cLastVersion?s_cLastVersion:"");
		
		g_key_file_set_string (pKeyFile, "Launch", "default backend", "");
		
		g_key_file_set_integer (pKeyFile, "Gui", "mode", s_iGuiMode);
		
		g_key_file_set_integer (pKeyFile, "Launch", "last year", s_iLastYear);
		
		cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	}
	g_key_file_free (pKeyFile);
	g_free (cConfFilePath);
}


int main (int argc, char** argv)
{
	//\___________________ build the command line used to respawn, and check if we have been launched from another life.
	s_pLaunchCommand = g_string_new (argv[0]);
	int i;
	for (i = 1; i < argc; i ++)
	{
		//g_print ("'%s'\n", argv[i]);
		if (strcmp (argv[i], "-q") == 0)  // this option is only set by the dock itself, at the end of the command line.
		{
			s_iNbCrashes = atoi (argv[i+1]);
			argc = i;  // remove this option, as it doesn't belong to the options table.
			argv[i] = NULL;
			break;
		}
		else if (strcmp (argv[i], "-w") == 0 || strcmp (argv[i], "-x") == 0)  // skip this option and its argument.
		{
			i ++;
		}
		else if (strcmp (argv[i], "-m") == 0)  // skip this option
		{
			// nothing else to do.
		}
		else  // keep this option in the command line.
		{
			g_string_append_printf (s_pLaunchCommand, " %s", argv[i]);
		}
	}
	/* Crash: 5th crash: an applet has already been removed and the maintenance
	 * mode has already been displayed => stop
	 */
	if (s_iNbCrashes > 4)
	{
		g_print ("Sorry, Cairo-Dock has encoutered some problems, and will quit.\n");
		return 1;
	}

	// mute all output messages if CD is not launched from a terminal
	if (getenv("TERM") == NULL)  /// why not isatty(stdout) ?...
		g_set_print_handler(PrintMuteFunc);
	
	
	// init lib
	#if !defined (GLIB_VERSION_2_36) // no longer needed now... (>= 2.35)
	g_type_init (); // should initialise threads too on new versions of GLIB (>= 2.24)
	#endif
	#if (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 24)
	if (!g_thread_supported ())
		g_thread_init (NULL);
	#endif

	dbus_g_thread_init (); // it's a wrapper: it will use dbus_threads_init_default ();
	
	GError *erreur = NULL;
	
	//\___________________ internationalize the app.
	bindtextdomain (CAIRO_DOCK_GETTEXT_PACKAGE, CAIRO_DOCK_LOCALE_DIR);
	bind_textdomain_codeset (CAIRO_DOCK_GETTEXT_PACKAGE, "UTF-8");
	textdomain (CAIRO_DOCK_GETTEXT_PACKAGE);
	
	//\___________________ get app's options.
	gboolean bSafeMode = FALSE, bMaintenance = FALSE, bNoSticky = FALSE, bCappuccino = FALSE, bPrintVersion = FALSE, bTesting = FALSE, bForceOpenGL = FALSE, bToggleIndirectRendering = FALSE, bKeepAbove = FALSE, bForceColors = FALSE, bAskBackend = FALSE, bMetacityWorkaround = FALSE;
	gchar *cEnvironment = NULL, *cUserDefinedDataDir = NULL, *cVerbosity = 0, *cUserDefinedModuleDir = NULL, *cExcludeModule = NULL, *cThemeServerAdress = NULL;
	int iDelay = 0;
	GOptionEntry pOptionsTable[] =
	{
		// GLDI options: cairo, opengl, indirect-opengl, env, keep-above, no-sticky
		{"cairo", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bForceCairo,
			_("Use Cairo backend."), NULL},
		{"opengl", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bForceOpenGL,
			_("Use OpenGL backend."), NULL},
		{"indirect-opengl", 'O', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bToggleIndirectRendering,
			_("Use OpenGL backend with indirect rendering. There are very few case where this option should be used."), NULL},
		{"ask-backend", 'A', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bAskBackend,
			_("Ask again on startup which backend to use."), NULL},
		{"env", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cEnvironment,
			_("Force the dock to consider this environnement - use it with care."), NULL},
		{"dir", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cUserDefinedDataDir,
			_("Force the dock to load from this directory, instead of ~/.config/cairo-dock."), NULL},
		{"server", 'S', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cThemeServerAdress,
			_("Address of a server containing additional themes. This will overwrite the default server address."), NULL},
		{"wait", 'w', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
			&iDelay,
			_("Wait for N seconds before starting; this is useful if you notice some problems when the dock starts with the session."), NULL},
		{"maintenance", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bMaintenance,
			_("Allow to edit the config before the dock is started and show the config panel on start."), NULL},
		{"exclude", 'x', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cExcludeModule,
			_("Exclude a given plug-in from activating (it is still loaded though)."), NULL},
		{"safe-mode", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bSafeMode,
			_("Don't load any plug-ins."), NULL},
		{"metacity-workaround", 'W', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bMetacityWorkaround,
			_("Work around some bugs in Metacity Window-Manager (invisible dialogs or sub-docks)"), NULL},
		{"log", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cVerbosity,
			_("Log verbosity (debug,message,warning,critical,error); default is warning."), NULL},
		{"colors", 'F', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bForceColors,
			_("Force to display some output messages with colors."), NULL},
		{"version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bPrintVersion,
			_("Print version and quit."), NULL},
		{"locked", 'k', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bLocked,
			_("Lock the dock so that any modification is impossible for users."), NULL},
		// below options are probably useless for most of people.
		{"keep-above", 'a', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bKeepAbove,
			_("Keep the dock above other windows."), NULL},
		{"no-sticky", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bNoSticky,
			_("Don't make the dock appear on all desktops."), NULL},
		{"capuccino", 'C', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bCappuccino,
			_("Cairo-Dock makes anything, including coffee !"), NULL},
		{"modules-dir", 'M', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cUserDefinedModuleDir,
			_("Ask the dock to load additionnal modules contained in this directory (though it is unsafe for your dock to load unnofficial modules)."), NULL},
		{"testing", 'T', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bTesting,
			_("For debugging purpose only. The crash manager will not be started to hunt down the bugs."), NULL},
		{"easter-eggs", 'E', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bEasterEggs,
			_("For debugging purpose only. Some hidden and still unstable options will be activated."), NULL},
		{"wayland", 'L', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bForceWayland,
			_("Force using the Wayland backends (disable X11 backends)."), NULL},
		{"x11", 'X', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bForceX11,
			_("Force using the X11 backend (disable any Wayland functionality)."), NULL},
#ifdef HAVE_GTK_LAYER_SHELL
		{"no-layer-shell", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bDisableLayerShell,
			_("For debugging purpose only. Disable gtk-layer-shell support."), NULL},
#endif
		{NULL, 0, 0, 0,
			NULL,
			NULL, NULL}
	};

	GOptionContext *context = g_option_context_new ("Cairo-Dock");
	g_option_context_add_main_entries (context, pOptionsTable, NULL);
	g_option_context_parse (context, &argc, &argv, &erreur);
	if (erreur != NULL)
	{
		cd_error ("ERROR in options: %s", erreur->message);
		return 1;
	}
	if (g_bForceWayland && g_bForceX11)
	{
		cd_error ("Both Wayland and X11 backends cannot be requested (use only one of the -L and -X options)!\n");
		return 1;
	}
	if (g_bForceWayland)
		gdk_set_allowed_backends ("wayland");
	if (g_bForceX11)
		gdk_set_allowed_backends ("x11");
	
	gtk_init (&argc, &argv);
	
	if (bPrintVersion)
	{
		g_print ("%s\n", CAIRO_DOCK_VERSION);
		return 0;
	}
	
	if (g_bLocked)
		cd_warning ("Cairo-Dock will be locked.");
	
	if (cVerbosity != NULL)
	{
		cd_log_set_level_from_name (cVerbosity);
		g_free (cVerbosity);
	}
	
	if (bForceColors)
		cd_log_force_use_color ();
	
	CairoDockDesktopEnv iDesktopEnv = CAIRO_DOCK_UNKNOWN_ENV;
	if (cEnvironment != NULL)
	{
		if (strcmp (cEnvironment, "gnome") == 0)
			iDesktopEnv = CAIRO_DOCK_GNOME;
		else if (strcmp (cEnvironment, "kde") == 0)
			iDesktopEnv = CAIRO_DOCK_KDE;
		else if (strcmp (cEnvironment, "xfce") == 0)
			iDesktopEnv = CAIRO_DOCK_XFCE;
		else if (strcmp (cEnvironment, "none") == 0)
			iDesktopEnv = CAIRO_DOCK_UNKNOWN_ENV;
		else
			cd_warning ("Unknown environment '%s'", cEnvironment);
		g_free (cEnvironment);
	}
	
	if (bCappuccino)
	{
		const gchar *cCappuccino = _("Cairo-Dock makes anything, including coffee !");
		g_print ("%s\n", cCappuccino);
		return 0;
	}
	
	//\___________________ get global config.
	gboolean bFirstLaunch = FALSE;
	gchar *cRootDataDirPath;
	// if the user has specified the '-d' parameter.
	if (cUserDefinedDataDir != NULL)
	{
		// if 'cRootDataDirPath' is not a full path, we will have a few problems with image in the config panel without a full path (e.g. 'bg.svg' in the default theme)
		if (*cUserDefinedDataDir == '/')
			cRootDataDirPath = cUserDefinedDataDir;
		else if (*cUserDefinedDataDir == '~')
			cRootDataDirPath = g_strdup_printf ("%s%s", getenv("HOME"), cUserDefinedDataDir + 1);
		else
			cRootDataDirPath = g_strdup_printf ("%s/%s", g_get_current_dir(), cUserDefinedDataDir);
		cUserDefinedDataDir = NULL;
	}
	else
	{
		cRootDataDirPath = g_strdup_printf ("%s/.config/%s", getenv("HOME"), CAIRO_DOCK_DATA_DIR);
	}
	bFirstLaunch = ! g_file_test (cRootDataDirPath, G_FILE_TEST_IS_DIR);
	_cairo_dock_get_global_config (cRootDataDirPath);
	if (bAskBackend)
	{
		g_free (s_cDefaulBackend);
		s_cDefaulBackend = NULL;
	}
	
	//\___________________ delay the startup if specified.
	if (iDelay > 0)
	{
		sleep (iDelay);
	}
	
	//\___________________ initialize libgldi.
	GldiRenderingMethod iRendering = (bForceOpenGL ? GLDI_OPENGL : g_bForceCairo ? GLDI_CAIRO : GLDI_DEFAULT);
	gldi_init (iRendering);
	
	//\___________________ set custom user options.
	if (bKeepAbove)
		cairo_dock_force_docks_above ();
	
	if (bNoSticky)
		cairo_dock_set_containers_non_sticky ();
	
	if (bMetacityWorkaround)
		cairo_dock_disable_containers_opacity ();
	
	if (iDesktopEnv != CAIRO_DOCK_UNKNOWN_ENV)
		cairo_dock_fm_force_desktop_env (iDesktopEnv);
	
	if (bToggleIndirectRendering)
		gldi_gl_backend_force_indirect_rendering ();
	
	gchar *cExtraDirPath = g_strconcat (cRootDataDirPath, "/"CAIRO_DOCK_EXTRAS_DIR, NULL);
	gchar *cThemesDirPath = g_strconcat (cRootDataDirPath, "/"CAIRO_DOCK_THEMES_DIR, NULL);
	gchar *cCurrentThemeDirPath = g_strconcat (cRootDataDirPath, "/"CAIRO_DOCK_CURRENT_THEME_NAME, NULL);
	
	cairo_dock_set_paths (cRootDataDirPath, cExtraDirPath, cThemesDirPath, cCurrentThemeDirPath, (gchar*)CAIRO_DOCK_SHARE_THEMES_DIR, (gchar*)CAIRO_DOCK_DISTANT_THEMES_DIR, cThemeServerAdress ? cThemeServerAdress : g_strdup (CAIRO_DOCK_THEME_SERVER));
	
	//\___________________ Check that OpenGL is safely usable, if not ask the user what to do.
	if (bAskBackend || (g_bUseOpenGL && ! bForceOpenGL && ! bToggleIndirectRendering && ! gldi_gl_backend_is_safe ()))  // opengl disponible sans l'avoir force mais pas safe => on demande confirmation.
	{
		if (s_cDefaulBackend == NULL)  // pas de backend par defaut defini.
		{
			GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Use OpenGL in Cairo-Dock"),
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				_("Yes"),
				GTK_RESPONSE_YES,
				_("No"),
				GTK_RESPONSE_NO,
				NULL);
			GtkWidget *label = gtk_label_new (_("OpenGL allows you to use the hardware acceleration, reducing the CPU load to the minimum.\nIt also allows some pretty visual effects similar to Compiz.\nHowever, some cards and/or their drivers don't fully support it, which may prevent the dock from running correctly.\nDo you want to activate OpenGL ?\n (To not show this dialog, launch the dock from the Application menu,\n  or with the -o option to force OpenGL and -c to force cairo.)"));
			GtkWidget *pContentBox = gtk_dialog_get_content_area (GTK_DIALOG(dialog));

			gtk_box_pack_start (GTK_BOX (pContentBox), label, FALSE, FALSE, 0);

			GtkWidget *pAskBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
			gtk_box_pack_start (GTK_BOX (pContentBox), pAskBox, FALSE, FALSE, 0);
			label = gtk_label_new (_("Remember this choice"));
			GtkWidget *pCheckBox = gtk_check_button_new ();
			gtk_box_pack_end (GTK_BOX (pAskBox), pCheckBox, FALSE, FALSE, 0);
			gtk_box_pack_end (GTK_BOX (pAskBox), label, FALSE, FALSE, 0);
			
			gtk_widget_show_all (dialog);
			
			gint iAnswer = gtk_dialog_run (GTK_DIALOG (dialog));  // lance sa propre main loop, c'est pourquoi on peut le faire avant le gtk_main().
			gboolean bRememberChoice = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pCheckBox));
			gtk_widget_destroy (dialog);
			if (iAnswer == GTK_RESPONSE_NO)
			{
				gldi_gl_backend_deactivate ();
			}
			
			if (bRememberChoice)  // l'utilisateur a defini le choix par defaut.
			{
				s_cDefaulBackend = g_strdup (iAnswer == GTK_RESPONSE_NO ? "cairo" : "opengl");
				gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
				cairo_dock_update_conf_file (cConfFilePath,
					G_TYPE_STRING, "Launch", "default backend", s_cDefaulBackend,
					G_TYPE_INVALID);
				g_free (cConfFilePath);
			}
		}
		else if (strcmp (s_cDefaulBackend, "opengl") != 0)  // un backend par defaut qui n'est pas OpenGL.
		{
			gldi_gl_backend_deactivate ();
		}
	}
	g_print ("\n"
	" ============================================================================\n"
	"\tCairo-Dock version : %s\n"
	"\tCompiled date      : %s %s\n"
	"\tBuilt with GTK     : %d.%d\n"
	"\tRunning with OpenGL: %d\n"
	" ============================================================================\n\n",
		CAIRO_DOCK_VERSION,
		__DATE__, __TIME__,
		GTK_MAJOR_VERSION, GTK_MINOR_VERSION,
		g_bUseOpenGL);
	
	//\___________________ load plug-ins (must be done after everything is initialized).
	if (! bSafeMode)
	{
		gldi_modules_new_from_directory (NULL, &erreur);  // load gldi-based plug-ins
		if (erreur != NULL)
		{
			cd_warning ("%s\n  no module will be available", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		
		if (cUserDefinedModuleDir != NULL)
		{
			gldi_modules_new_from_directory (cUserDefinedModuleDir, &erreur);  // load user plug-ins
			if (erreur != NULL)
			{
				cd_warning ("%s\n  no additionnal module will be available", erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
			g_free (cUserDefinedModuleDir);
			cUserDefinedModuleDir = NULL;
		}
	}
	
	//\___________________ define GUI backend.
	cairo_dock_load_user_gui_backend (s_iGuiMode);
	
	//\___________________ register to the useful notifications.
	gldi_object_register_notification (&myContainerObjectMgr,
		NOTIFICATION_DROP_DATA,
		(GldiNotificationFunc) cairo_dock_notification_drop_data,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myContainerObjectMgr,
		NOTIFICATION_CLICK_ICON,
		(GldiNotificationFunc) cairo_dock_notification_click_icon,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myContainerObjectMgr,
		NOTIFICATION_MIDDLE_CLICK_ICON,
		(GldiNotificationFunc) cairo_dock_notification_middle_click_icon,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myContainerObjectMgr,
		NOTIFICATION_SCROLL_ICON,
		(GldiNotificationFunc) cairo_dock_notification_scroll_icon,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myContainerObjectMgr,
		NOTIFICATION_BUILD_CONTAINER_MENU,
		(GldiNotificationFunc) cairo_dock_notification_build_container_menu,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myContainerObjectMgr,
		NOTIFICATION_BUILD_ICON_MENU,
		(GldiNotificationFunc) cairo_dock_notification_build_icon_menu,
		GLDI_RUN_AFTER, NULL);
	
	gldi_object_register_notification (&myDeskletObjectMgr,
		NOTIFICATION_CONFIGURE_DESKLET,
		(GldiNotificationFunc) cairo_dock_notification_configure_desklet,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDockObjectMgr,
		NOTIFICATION_ICON_MOVED,
		(GldiNotificationFunc) cairo_dock_notification_icon_moved,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDockObjectMgr,
		NOTIFICATION_DESTROY,
		(GldiNotificationFunc) cairo_dock_notification_dock_destroyed,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myModuleObjectMgr,
		NOTIFICATION_MODULE_ACTIVATED,
		(GldiNotificationFunc) cairo_dock_notification_module_activated,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myModuleObjectMgr,
		NOTIFICATION_MODULE_REGISTERED,
		(GldiNotificationFunc) cairo_dock_notification_module_registered,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myModuleInstanceObjectMgr,
		NOTIFICATION_MODULE_INSTANCE_DETACHED,
		(GldiNotificationFunc) cairo_dock_notification_module_detached,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDockObjectMgr,
		NOTIFICATION_INSERT_ICON,
		(GldiNotificationFunc) cairo_dock_notification_icon_inserted,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDockObjectMgr,
		NOTIFICATION_REMOVE_ICON,
		(GldiNotificationFunc) cairo_dock_notification_icon_removed,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDeskletObjectMgr,
		NOTIFICATION_DESTROY,
		(GldiNotificationFunc) cairo_dock_notification_desklet_added_removed,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDeskletObjectMgr,
		NOTIFICATION_NEW,
		(GldiNotificationFunc) cairo_dock_notification_desklet_added_removed,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myShortkeyObjectMgr,
		NOTIFICATION_NEW,
		(GldiNotificationFunc) cairo_dock_notification_shortkey_added_removed_changed,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myShortkeyObjectMgr,
		NOTIFICATION_DESTROY,
		(GldiNotificationFunc) cairo_dock_notification_shortkey_added_removed_changed,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myShortkeyObjectMgr,
		NOTIFICATION_SHORTKEY_CHANGED,
		(GldiNotificationFunc) cairo_dock_notification_shortkey_added_removed_changed,
		GLDI_RUN_AFTER, NULL);
	
	//\___________________ handle crashes.
	if (! bTesting)
		_cairo_dock_set_signal_interception ();
	
	//\___________________ handle terminate signals to quit properly (especially when the system shuts down).
	signal (SIGTERM, _cairo_dock_quit);  // Term // kill -15 (system)
	signal (SIGHUP,  _cairo_dock_quit);  // sent to a process when its controlling terminal is closed

	//\___________________ Disable modules that have crashed
	if (cExcludeModule != NULL && (s_iNbCrashes > 2 || bMaintenance)) // 3th crash or 4th (with -m)
	{
		gchar *cCommand = g_strdup_printf ("sed -i \"/modules/ s/%s//g\" \"%s\"",
			cExcludeModule, g_cConfFile);
		int r = system (cCommand);
		if (r < 0)
			cd_warning ("Not able to launch this command: %s", cCommand);
		else
			cd_warning (_("The module '%s' has been deactivated because it may "
			"have caused some problems.\nYou can reactivate it, if it happens "
			"again thanks to report it at http://glx-dock.org"), cExcludeModule);
		g_free (cCommand);
	}
	
	//\___________________ maintenance mode -> show the main config panel.
	if (bMaintenance)
	{
		cairo_dock_load_user_gui_backend (1);  // force the advanced GUI, it can display the config before the theme is loaded.
		
		GtkWidget *pWindow = cairo_dock_show_main_gui ();
		gtk_window_set_title (GTK_WINDOW (pWindow), _("< Maintenance mode >"));
		if (cExcludeModule != NULL)
			cairo_dock_set_status_message_printf (pWindow, "%s '%s'...", _("Something went wrong with this applet:"), cExcludeModule);
		gtk_window_set_modal (GTK_WINDOW (pWindow), TRUE);
		GMainLoop *pBlockingLoop = g_main_loop_new (NULL, FALSE);
		g_signal_connect (G_OBJECT (pWindow),
			"destroy",
			G_CALLBACK (on_delete_maintenance_gui),
			pBlockingLoop);
		
		cd_warning ("showing the maintenance mode ...");
		g_main_loop_run (pBlockingLoop);  // pas besoin de GDK_THREADS_LEAVE/ENTER vu qu'on est pas encore dans la main loop de GTK. En fait cette boucle va jouer le role de la main loop GTK.
		cd_warning ("end of the maintenance mode.");
		
		g_main_loop_unref (pBlockingLoop);
		cairo_dock_load_user_gui_backend (s_iGuiMode);  // go back to the user GUI.
	}
	
	//\___________________ load the current theme.
	cd_message ("loading theme ...");
	const gchar *cDesktopSessionEnv = g_getenv ("DESKTOP_SESSION");
	if (! g_file_test (g_cConfFile, G_FILE_TEST_EXISTS))  // no theme yet, copy the default theme first.
	{
		const gchar *cThemeName;
		if (g_strcmp0 (cDesktopSessionEnv, "cairo-dock") == 0)
		{
			cThemeName = "Default-Panel";
			// We're using the CD session for the first time
			s_bCDSessionLaunched = TRUE;
			gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
			cairo_dock_update_conf_file (cConfFilePath,
				G_TYPE_BOOLEAN, "Launch", "cd session", s_bCDSessionLaunched,
				G_TYPE_INVALID);
			g_free (cConfFilePath);
		}
		else
			cThemeName = "Default-Single";
		gchar *cCommand = g_strdup_printf ("cp -r \"%s/%s\"/* \"%s\"", CAIRO_DOCK_SHARE_DATA_DIR"/themes", cThemeName, g_cCurrentThemePath);
		cd_message (cCommand);
		int r = system (cCommand);
		if (r < 0)
			cd_warning ("Not able to launch this command: %s", cCommand);
		g_free (cCommand);
	}
	/* The first time the Cairo-Dock session is used but not the first time the
	 *  dock is launched: propose to use the Default-Panel theme if a second
	 *  dock is not used (case: the user has already launched the dock and he
	 *  wants to test the Cairo-Dock session: propose a theme with two docks)
	 */
	else if (! s_bCDSessionLaunched && g_strcmp0 (cDesktopSessionEnv, "cairo-dock") == 0)
	{
		gchar *cSecondDock = g_strdup_printf ("%s/"CAIRO_DOCK_MAIN_DOCK_NAME"-2.conf", g_cCurrentThemePath);
		if (! g_file_test (cSecondDock, G_FILE_TEST_EXISTS))
		{
			GtkWidget *pDialog = gtk_dialog_new_with_buttons (_("You're using our Cairo-Dock session"),
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				_("Yes"), GTK_RESPONSE_YES,
				_("No"), GTK_RESPONSE_NO,
				NULL);
			GtkWidget *pLabel = gtk_label_new (_("It can be interesting to use an adapted theme for this session.\n\nDo you want to load our \"Default-Panel\" theme?\n\nNote: your current theme will be saved and can be reimported later from the Themes manager"));

			GtkWidget *pContentBox = gtk_dialog_get_content_area (GTK_DIALOG (pDialog));
			gtk_box_pack_start (GTK_BOX (pContentBox), pLabel, FALSE, FALSE, 0);
			gtk_widget_show_all (pDialog);

			gint iAnswer = gtk_dialog_run (GTK_DIALOG (pDialog)); // will block the main loop
			gtk_widget_destroy (pDialog);
			if (iAnswer == GTK_RESPONSE_YES)
			{
				time_t epoch = (time_t) time (NULL);
				struct tm currentTime;
				localtime_r (&epoch, &currentTime);
				char cDateBuffer[22+1];
				strftime (cDateBuffer, 22, "Theme_%Y%m%d_%H%M%S", &currentTime);

				cairo_dock_export_current_theme (cDateBuffer, TRUE, TRUE);
				cairo_dock_import_theme ("Default-Panel", TRUE, TRUE);
			}

			// Force init script
			g_timeout_add_seconds (4, _cairo_dock_first_launch_setup, NULL);
		}
		g_free (cSecondDock);

		// Update 'cd session' key: we already check that the user is using an adapted theme for this session
		s_bCDSessionLaunched = TRUE;
		gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_BOOLEAN, "Launch", "cd session", s_bCDSessionLaunched,
			G_TYPE_INVALID);
		g_free (cConfFilePath);
	}
	cairo_dock_load_current_theme ();
	
	//\___________________ lock mode.
	if (g_bLocked)  // comme on ne pourra pas ouvrir le panneau de conf, ces 2 variables resteront tel quel.
	{
		myDocksParam.bLockIcons = TRUE;
		myDocksParam.bLockAll = TRUE;
	}
	
	if (!bSafeMode && gldi_module_get_nb () <= 1)  // 1 en comptant l'aide
	{
		Icon *pIcon = gldi_icons_get_any_without_dialog ();
		gldi_dialog_show_temporary_with_icon (_("No plug-in were found.\nPlug-ins provide most of the functionalities (animations, applets, views, etc).\nSee http://glx-dock.org for more information.\nThere is almost no meaning in running the dock without them and it's probably due to a problem with the installation of these plug-ins.\nBut if you really want to use the dock without these plug-ins, you can launch the dock with the '-f' option to no longer have this message.\n"), pIcon, CAIRO_CONTAINER (g_pMainDock), 0., CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON);
	}
	
	//\___________________ display the changelog in case of a new version.
	gboolean bNewVersion = (s_cLastVersion == NULL || strcmp (s_cLastVersion, CAIRO_DOCK_VERSION) != 0);
	if (bNewVersion)
	{
		gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_STRING, "Launch", "last version", CAIRO_DOCK_VERSION,
			G_TYPE_INVALID);
		g_free (cConfFilePath);

		/// If any operation must be done on the user theme (like activating a module by default, or disabling an option), it should be done here once (when CAIRO_DOCK_VERSION matches the new version).
	}
	
	//g_print ("bFirstLaunch: %d; bNewVersion: %d\n", bFirstLaunch, bNewVersion);
	if (bFirstLaunch)  // first launch => set up config
	{
		g_timeout_add_seconds (4, _cairo_dock_first_launch_setup, NULL);
	}
	else if (bNewVersion)  // new version -> changelog (if it's the first launch, useless to display what's new, we already have the Welcome message).
	{
		gchar *cChangeLogFilePath = g_strdup_printf ("%s/ChangeLog.txt", CAIRO_DOCK_SHARE_DATA_DIR);
		GKeyFile *pKeyFile = cairo_dock_open_key_file (cChangeLogFilePath);
		if (pKeyFile != NULL)
		{
			gchar *cKeyName = g_strdup_printf ("%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion);  // version without "alpha", "beta", "rc", etc.
			gchar *cChangeLogMessage = g_key_file_get_string (pKeyFile, "ChangeLog", cKeyName, NULL);
			g_free (cKeyName);
			if (cChangeLogMessage != NULL)
			{
				GString *sChangeLogMessage = g_string_new (gettext (cChangeLogMessage));
				g_free (cChangeLogMessage);

				// changelog message is now split (by line): to not re-translate all the message when there is a modification
				int i = 0;
				while (TRUE)
				{
					cKeyName = g_strdup_printf ("%d.%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion, i);
					cChangeLogMessage = g_key_file_get_string (pKeyFile, "ChangeLog", cKeyName, NULL);
					g_free (cKeyName);

					if (cChangeLogMessage == NULL) // no more message
						break;

					g_string_append_printf (sChangeLogMessage, "\n %s", gettext (cChangeLogMessage));

					g_free (cChangeLogMessage);
					i++;
				}

				Icon *pFirstIcon = cairo_dock_get_first_icon (g_pMainDock->icons);
				
				CairoDialogAttr attr;
				memset (&attr, 0, sizeof (CairoDialogAttr));
				attr.cText = sChangeLogMessage->str;
				attr.cImageFilePath = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON;
				attr.bUseMarkup = TRUE;
				attr.pIcon = pFirstIcon;
				attr.pContainer = CAIRO_CONTAINER (g_pMainDock);
				gldi_dialog_new (&attr);
				g_string_free (sChangeLogMessage, TRUE);
			}
			g_key_file_free (pKeyFile);
		}
		// In case something has changed in Compiz/Gtk/others, we also run the script on a new version of the dock.
		g_timeout_add_seconds (4, _cairo_dock_first_launch_setup, NULL);
	}
	else if (cExcludeModule != NULL && ! bMaintenance && s_iNbCrashes > 1)
	{
		gchar *cMessage;
		if (s_iNbCrashes == 2) // <=> second crash: display a dialogue
			cMessage = g_strdup_printf (_("The module '%s' may have encountered a problem.\nIt has been restored successfully, but if it happens again, please report it at http://glx-dock.org"), cExcludeModule);
		else // since the 3th crash: the applet has been disabled
			cMessage = g_strdup_printf (_("The module '%s' has been deactivated because it may have caused some problems.\nYou can reactivate it, if it happens again thanks to report it at http://glx-dock.org"), cExcludeModule);
		
		GldiModule *pModule = gldi_module_get (cExcludeModule);
		Icon *icon = gldi_icons_get_any_without_dialog ();
		gldi_dialog_show_temporary_with_icon (cMessage, icon, CAIRO_CONTAINER (g_pMainDock), 15000., (pModule ? pModule->pVisitCard->cIconFilePath : NULL));
		g_free (cMessage);
	}
	
	if (! bTesting)
		g_timeout_add_seconds (5, _cairo_dock_successful_launch, GINT_TO_POINTER (bFirstLaunch));

	// Start Mainloop
	gtk_main ();
	
	signal (SIGSEGV, NULL);  // Segmentation violation
	signal (SIGFPE, NULL);  // Floating-point exception
	signal (SIGILL, NULL);  // Illegal instruction
	signal (SIGABRT, NULL);
	signal (SIGTERM, NULL);
	signal (SIGHUP, NULL);

	gldi_free_all ();

	#if (LIBRSVG_MAJOR_VERSION == 2 && LIBRSVG_MINOR_VERSION < 36)
	rsvg_term ();
	#endif
	xmlCleanupParser ();
	g_string_free (s_pLaunchCommand, TRUE);
	
	cd_message ("Bye bye !");
	g_print ("\033[0m\n");

	return 0;
}

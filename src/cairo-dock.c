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
*********************** VERSION 0.1 and above (2007-2011) *********************
**
** author(s) :
**     Fabrice Rey <fabounet@glx-dock.org>
** With the help of:
**     A lot of people !!!
**
*******************************************************************************/

/// http://www.siteduzero.com/tutoriel-3-307309-le-systeme-de-micro-paiement-flattr.html
/// http://www.cyber-junk.de/wp-content/cache/supercache/cyber-junk.de/entwickelt/flattr-button-im-eigenbau-mittels-curl-und-mini-api/index.html

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h> 
#include <unistd.h>

#define __USE_POSIX
#include <time.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <gtk/gtkgl.h>

#include "config.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_get_first_icon
#include "cairo-dock-module-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-menu.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-launcher-manager.h"  // cairo_dock_launch_command

#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-items.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-user-interaction.h"
#include "help/cairo-dock-help.h"
#include "cairo-dock-core.h"

//#define CAIRO_DOCK_THEME_SERVER "http://themes.glx-dock.org"
#define CAIRO_DOCK_THEME_SERVER "http://download.tuxfamily.org/glxdock/themes"
#define CAIRO_DOCK_BACKUP_THEME_SERVER "http://fabounet03.free.fr"
// Nom du repertoire racine du theme courant.
#define CAIRO_DOCK_CURRENT_THEME_NAME "current_theme"
// Nom du repertoire des themes extras.
#define CAIRO_DOCK_EXTRAS_DIR "extras"
// Nom du repertoire des themes de dock.
#define CAIRO_DOCK_THEMES_DIR "themes"
// Nom du repertoire des themes de dock sur le serveur
#define CAIRO_DOCK_DISTANT_THEMES_DIR "themes2.4"

extern gchar *g_cCairoDockDataDir;
extern gchar *g_cCurrentThemePath;

extern gchar *g_cConfFile;
extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;

extern CairoDock *g_pMainDock;
extern CairoDockGLConfig g_openglConfig;

extern gboolean g_bUseGlitz;
extern gboolean g_bUseOpenGL;
extern gboolean g_bEasterEggs;

extern CairoDockModuleInstance *g_pCurrentModule;

gboolean g_bForceCairo = FALSE;
gboolean g_bLocked;

static gboolean s_bSucessfulLaunch = FALSE;
static GString *s_pLaunchCommand = NULL;
static gchar *s_cLastVersion = NULL;
static gchar *s_cDefaulBackend = NULL;
static gint s_iGuiMode = 0;  // 0 = simple mode, 1 = advanced mode
static gint s_iLastYear = 0;
static void (*s_activate_composite) (gboolean) = NULL;
static gint s_iNbCrashes = 0;


static gboolean _cairo_dock_successful_launch (gpointer data)
{
	// successful launch, remove the maintenance mode from the command line.
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
		
		Icon *pIcon = cairo_dock_get_dialogless_icon ();
		gchar *cMessage = g_strdup_printf (_("Happy new year %d !!!"), s_iLastYear);
		gchar *cMessageFull = g_strdup_printf ("\n%s :-)\n", cMessage);
		cairo_dock_show_temporary_dialog_with_icon (cMessageFull, pIcon, CAIRO_CONTAINER (g_pMainDock), 15000., CAIRO_DOCK_SHARE_DATA_DIR"/icons/balloons.png");
		g_free (cMessageFull);
		g_free (cMessage);
	}
	return FALSE;
}
static gboolean _cairo_dock_first_launch_setup (gpointer data)
{
	cairo_dock_launch_command (CAIRO_DOCK_SHARE_DATA_DIR"/scripts/initial-setup.sh");
	return FALSE;
}
static void _cairo_dock_quit (int signal)
{
	gtk_main_quit ();
}
static void _cairo_dock_intercept_signal (int signal)
{
	cd_warning ("Cairo-Dock has crashed (sig %d).\nIt will be restarted now.\nFeel free to report this bug on glx-dock.org to help improving the dock!", signal);
	g_print ("info on the system :\n");
	int r = system ("uname -a");
	
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
		else if (g_pCurrentModule == NULL)  // several crashes, and no culprit => start in maintenance mode.
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
	signal (SIGABRT, _cairo_dock_intercept_signal);  // Abort
	signal (SIGTERM, _cairo_dock_quit);  // Abort
}

static gboolean on_delete_maintenance_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	cd_debug ("%s ()", __func__);
	if (pBlockingLoop != NULL && g_main_loop_is_running (pBlockingLoop))
	{
		g_main_loop_quit (pBlockingLoop);
	}
	return FALSE;  // TRUE <=> ne pas detruire la fenetre.
}

static void PrintFunc(const gchar *string) {}

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
		g_key_file_set_string (pKeyFile, "Launch", "last version", s_cLastVersion);
		
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
	gchar *cDisableApplet = NULL;
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
	if (s_iNbCrashes > 3)
	{
		g_print ("Sorry, Cairo-Dock has encoutered some problems, and will quit.\n");
		return 1;
	}

	// muted output messages if CD is not launched from a terminal
	if (getenv("TERM") == NULL)
		g_set_print_handler(PrintFunc);

	g_print (">>> restart cmd line: %s\n", s_pLaunchCommand->str);
	
	gtk_init (&argc, &argv);
	gtk_gl_init (&argc, &argv);
	
	GError *erreur = NULL;
	
	//\___________________ get app's options.
	gboolean bSafeMode = FALSE, bMaintenance = FALSE, bNoSticky = FALSE, bNormalHint = FALSE, bCappuccino = FALSE, bPrintVersion = FALSE, bTesting = FALSE, bForceIndirectRendering = FALSE, bForceOpenGL = FALSE, bToggleIndirectRendering = FALSE, bKeepAbove = FALSE, bForceColors = FALSE;
	gchar *cEnvironment = NULL, *cUserDefinedDataDir = NULL, *cVerbosity = 0, *cUserDefinedModuleDir = NULL, *cExcludeModule = NULL, *cThemeServerAdress = NULL;
	int iDelay = 0;
	GOptionEntry pOptionsTable[] =
	{
		{"log", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cVerbosity,
			_("Log verbosity (debug,message,warning,critical,error); default is warning."), NULL},
		{"colors", 'A', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bForceColors,
			_("Force to display some output messages with colors."), NULL},
		{"wait", 'w', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
			&iDelay,
			_("Wait for N seconds before starting; this is useful if you notice some problems when the dock starts with the session."), NULL},
#ifdef HAVE_GLITZ
		{"glitz", 'g', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bUseGlitz,
			_("Force Glitz backend (hardware acceleration for cairo, needs a glitz-enabled libcairo)."), NULL},
#endif
		{"cairo", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bForceCairo,
			_("Use Cairo backend."), NULL},
		{"opengl", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bForceOpenGL,
			_("Use OpenGL backend."), NULL},
		{"indirect-opengl", 'O', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bToggleIndirectRendering,
			_("Use OpenGL backend with indirect rendering. There are very few case where this option should be used."), NULL},
		{"indirect", 'i', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bForceIndirectRendering,
			_("Deprecated - see -O."), NULL},
		{"keep-above", 'a', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bKeepAbove,
			_("Keep the dock above other windows whatever."), NULL},
		{"no-sticky", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bNoSticky,
			_("Don't make the dock appear on all desktops."), NULL},
		{"env", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cEnvironment,
			_("Force the dock to consider this environnement - use it with care."), NULL},
		{"dir", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cUserDefinedDataDir,
			_("Force the dock to load from this directory, instead of ~/.config/cairo-dock."), NULL},
		{"maintenance", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bMaintenance,
			_("Allow to edit the config before the dock is started and show the config panel on start."), NULL},
		{"exclude", 'x', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cExcludeModule,
			_("Exclude a given plug-in from activating (it is still loaded though)."), NULL},
		{"safe-mode", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bSafeMode,
			_("Don't load any plug-ins."), NULL},
		{"capuccino", 'C', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bCappuccino,
			_("Cairo-Dock makes anything, including coffee !"), NULL},
		{"version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bPrintVersion,
			_("Print version and quit."), NULL},
		{"modules-dir", 'M', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cUserDefinedModuleDir,
			_("Ask the dock to load additionnal modules contained in this directory (though it is unsafe for your dock to load unnofficial modules)."), NULL},
		{"testing", 'T', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bTesting,
			_("For debugging purpose only. The crash manager will not be started to hunt down the bugs."), NULL},
		{"easter-eggs", 'E', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bEasterEggs,
			_("For debugging purpose only. Some hidden and still unstable options will be activated."), NULL},
		{"server", 'S', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cThemeServerAdress,
			_("Address of a server containing additional themes. This will overwrite the default server address."), NULL},
		{"locked", 'k', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bLocked,
			_("Lock the dock so that any modification is impossible for users."), NULL},
		{NULL, 0, 0, 0,
			NULL,
			NULL, NULL}
	};

	GOptionContext *context = g_option_context_new ("Cairo-Dock");
	g_option_context_add_main_entries (context, pOptionsTable, NULL);
	g_option_context_parse (context, &argc, &argv, &erreur);
	if (erreur != NULL)
	{
		g_print ("ERROR in options : %s\n", erreur->message);
		return 1;
	}
	
	if (bPrintVersion)
	{
		g_print ("%s\n", CAIRO_DOCK_VERSION);
		return 0;
	}
	
	if (g_bLocked)
		g_print ("Cairo-Dock will be locked.\n");
	
	if (cVerbosity != NULL)
	{
		cd_log_set_level_from_name (cVerbosity);
		g_free (cVerbosity);
	}
	
	cd_log_set_force_color (bForceColors);
	
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
			cd_warning ("unknown environnment '%s'", cEnvironment);
		g_free (cEnvironment);
	}
#ifdef HAVE_GLITZ
	g_print ("Compiled with Glitz (hardware acceleration support for cairo)\n");
#endif
	
	if (bCappuccino)
	{
		g_print ("Cairo-Dock does anything, including coffee !.\n");
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
	
	//\___________________ internationalize the app.
	bindtextdomain (CAIRO_DOCK_GETTEXT_PACKAGE, CAIRO_DOCK_LOCALE_DIR);
	bind_textdomain_codeset (CAIRO_DOCK_GETTEXT_PACKAGE, "UTF-8");
	textdomain (CAIRO_DOCK_GETTEXT_PACKAGE);
	
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
	
	if (iDesktopEnv != CAIRO_DOCK_UNKNOWN_ENV)
		cairo_dock_fm_force_desktop_env (iDesktopEnv);
	
	if (bToggleIndirectRendering)
		cairo_dock_force_indirect_rendering ();
	
	gchar *cExtraDirPath = g_strconcat (cRootDataDirPath, "/"CAIRO_DOCK_EXTRAS_DIR, NULL);
	gchar *cThemesDirPath = g_strconcat (cRootDataDirPath, "/"CAIRO_DOCK_THEMES_DIR, NULL);
	gchar *cCurrentThemeDirPath = g_strconcat (cRootDataDirPath, "/"CAIRO_DOCK_CURRENT_THEME_NAME, NULL);
	
	cairo_dock_set_paths (cRootDataDirPath, cExtraDirPath, cThemesDirPath, cCurrentThemeDirPath, (gchar*)CAIRO_DOCK_SHARE_THEMES_DIR, (gchar*)CAIRO_DOCK_DISTANT_THEMES_DIR, cThemeServerAdress ? cThemeServerAdress : g_strdup (CAIRO_DOCK_THEME_SERVER));
	
	//\___________________ Check that OpenGL is safely usable, if not ask the user what to do.
	if (g_bUseOpenGL && ! bForceOpenGL && ! bToggleIndirectRendering && ! cairo_dock_opengl_is_safe ())  // opengl disponible sans l'avoir force mais pas safe => on demande confirmation.
	{
		if (s_cDefaulBackend == NULL)  // pas de backend par defaut defini.
		{
			GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Use OpenGL in Cairo-Dock"),
				NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_YES,
				GTK_RESPONSE_YES,
				GTK_STOCK_NO,
				GTK_RESPONSE_NO,
				NULL);
			GtkWidget *label = gtk_label_new (_("OpenGL allows you to use the hardware acceleration, reducing the CPU load to the minimum.\nIt also allows some pretty visual effects similar to Compiz.\nHowever, some cards and/or their drivers don't fully support it, which may prevent the dock from running correctly.\nDo you want to activate OpenGL ?\n (To not show this dialog, launch the dock from the Application menu,\n  or with the -o option to force OpenGL and -c to force cairo.)"));
			gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), label, FALSE, FALSE, 0);
			
			GtkWidget *pAskBox = gtk_hbox_new (FALSE, 3);
			gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), pAskBox, FALSE, FALSE, 0);
			label = gtk_label_new (_("Remember this choice"));
			GtkWidget *pCheckBox = gtk_check_button_new ();
			gtk_box_pack_end (GTK_BOX (pAskBox), pCheckBox, FALSE, FALSE, 0);
			gtk_box_pack_end (GTK_BOX (pAskBox), label, FALSE, FALSE, 0);
			///g_signal_connect (G_OBJECT (pCheckBox), "toggled", G_CALLBACK(_toggle_remember_choice), dialog);
			
			gtk_widget_show_all (dialog);
			
			gint iAnswer = gtk_dialog_run (GTK_DIALOG (dialog));  // lance sa propre main loop, c'est pourquoi on peut le faire avant le gtk_main().
			///gboolean bRememberChoice = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "remember"));
			gboolean bRememberChoice = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pCheckBox));
			gtk_widget_destroy (dialog);
			if (iAnswer == GTK_RESPONSE_NO)
			{
				cairo_dock_deactivate_opengl ();
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
			cairo_dock_deactivate_opengl ();
		}
	}
	g_print ("\n ============================================================================ \n\tCairo-Dock version: %s\n\tCompiled date:  %s %s\n\tRunning with OpenGL: %d\n ============================================================================\n\n",
		CAIRO_DOCK_VERSION,
		__DATE__, __TIME__,
		g_bUseOpenGL);
	
	//\___________________ load plug-ins (must be done after everything is initialized).
	if (! bSafeMode)
	{
		GError *erreur = NULL;
		cairo_dock_load_modules_in_directory (NULL, &erreur);  // load gldi-based plug-ins
		if (erreur != NULL)
		{
			cd_warning ("%s\n  no module will be available", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		
		if (cUserDefinedModuleDir != NULL)
		{
			cairo_dock_load_modules_in_directory (cUserDefinedModuleDir, &erreur);  // load user plug-ins
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
	cairo_dock_register_help_module ();  // this applet is made for Cairo-Dock, not gldi; therefore, it's not installed as a separate library, so we manually register it.
	
	//\___________________ define GUI backend.
	cairo_dock_load_user_gui_backend (s_iGuiMode);
	cairo_dock_register_default_items_gui_backend ();
	
	//\___________________ register to the useful notifications.
	cairo_dock_register_notification_on_object (&myContainersMgr,
		NOTIFICATION_DROP_DATA,
		(CairoDockNotificationFunc) cairo_dock_notification_drop_data,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myContainersMgr,
		NOTIFICATION_CLICK_ICON,
		(CairoDockNotificationFunc) cairo_dock_notification_click_icon,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myContainersMgr,
		NOTIFICATION_MIDDLE_CLICK_ICON,
		(CairoDockNotificationFunc) cairo_dock_notification_middle_click_icon,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myContainersMgr,
		NOTIFICATION_SCROLL_ICON,
		(CairoDockNotificationFunc) cairo_dock_notification_scroll_icon,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myContainersMgr,
		NOTIFICATION_BUILD_CONTAINER_MENU,
		(CairoDockNotificationFunc) cairo_dock_notification_build_container_menu,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification_on_object (&myContainersMgr,
		NOTIFICATION_BUILD_ICON_MENU,
		(CairoDockNotificationFunc) cairo_dock_notification_build_icon_menu,
		CAIRO_DOCK_RUN_AFTER, NULL);
	
	cairo_dock_register_notification_on_object (&myDeskletsMgr,
		NOTIFICATION_CONFIGURE_DESKLET,
		(CairoDockNotificationFunc) cairo_dock_notification_configure_desklet,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myDocksMgr,
		NOTIFICATION_ICON_MOVED,
		(CairoDockNotificationFunc) cairo_dock_notification_icon_moved,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myDocksMgr,
		NOTIFICATION_STOP_DOCK,
		(CairoDockNotificationFunc) cairo_dock_notification_dock_destroyed,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myModulesMgr,
		NOTIFICATION_MODULE_ACTIVATED,
		(CairoDockNotificationFunc) cairo_dock_notification_module_activated,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myModulesMgr,
		NOTIFICATION_MODULE_REGISTERED,
		(CairoDockNotificationFunc) cairo_dock_notification_module_registered,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myModulesMgr,
		NOTIFICATION_MODULE_INSTANCE_DETACHED,
		(CairoDockNotificationFunc) cairo_dock_notification_module_detached,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myDocksMgr,
		NOTIFICATION_INSERT_ICON,
		(CairoDockNotificationFunc) cairo_dock_notification_icon_inserted,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myDocksMgr,
		NOTIFICATION_REMOVE_ICON,
		(CairoDockNotificationFunc) cairo_dock_notification_icon_removed,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myDeskletsMgr,
		NOTIFICATION_STOP_DESKLET,
		(CairoDockNotificationFunc) cairo_dock_notification_desklet_destroyed,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myDeskletsMgr,
		NOTIFICATION_NEW_DESKLET,
		(CairoDockNotificationFunc) cairo_dock_notification_desklet_destroyed,
		CAIRO_DOCK_RUN_AFTER, NULL);
	
	//\___________________ handle crashes.
	if (! bTesting)
		_cairo_dock_set_signal_interception ();
	
	//\___________________ maintenance mode -> show the main config panel.
	if (bMaintenance)
	{
		if (cExcludeModule != NULL)
		{
			cd_warning ("The module '%s' has been deactivated because it may have caused some problems.\nYou can reactivate it, if it happens again thanks to report it at http://glx-dock.org\n", cExcludeModule);
			gchar *cCommand = g_strdup_printf ("sed -i \"/modules/ s/%s//g\" \"%s\"", cExcludeModule, g_cConfFile);
			int r = system (cCommand);
			g_free (cCommand);
		}
		
		GtkWidget *pWindow = cairo_dock_show_main_gui ();
		gtk_window_set_title (GTK_WINDOW (pWindow), _("< Maintenance mode >"));
		if (cExcludeModule != NULL)
			cairo_dock_set_status_message_printf (pWindow, "Something went wrong with the applet '%s'...", cExcludeModule);
		gtk_window_set_modal (GTK_WINDOW (pWindow), TRUE);
		GMainLoop *pBlockingLoop = g_main_loop_new (NULL, FALSE);
		g_signal_connect (G_OBJECT (pWindow),
			"delete-event",
			G_CALLBACK (on_delete_maintenance_gui),
			pBlockingLoop);
		
		g_print ("showing the maintenance mode ...\n");
		g_main_loop_run (pBlockingLoop);  // pas besoin de GDK_THREADS_LEAVE/ENTER vu qu'on est pas encore dans la main loop de GTK. En fait cette boucle va jouer le role de la main loop GTK.
		g_print ("end of the maintenance mode.\n");
		
		g_main_loop_unref (pBlockingLoop);
	}
	
	//\___________________ load the current theme.
	cd_message ("loading theme ...");
	if (! g_file_test (g_cConfFile, G_FILE_TEST_EXISTS))  // no theme yet, copy the default theme first.
	{
		gchar *cCommand = g_strdup_printf ("/bin/cp -r \"%s\"/* \"%s\"", CAIRO_DOCK_SHARE_DATA_DIR"/themes/_default_", g_cCurrentThemePath);
		cd_message (cCommand);
		int r = system (cCommand);
		g_free (cCommand);
		
		cairo_dock_mark_current_theme_as_modified (FALSE);  // on ne proposera pas de sauvegarder ce theme.
	}
	cairo_dock_load_current_theme ();
	
	//\___________________ lock mode.
	if (g_bLocked)  // comme on ne pourra pas ouvrir le panneau de conf, ces 2 variables resteront tel quel.
	{
		myDocksParam.bLockIcons = TRUE;
		myDocksParam.bLockAll = TRUE;
	}
	
	if (!bSafeMode && cairo_dock_get_nb_modules () <= 1)  // 1 en comptant l'aide
	{
		Icon *pIcon = cairo_dock_get_dialogless_icon ();
		cairo_dock_ask_question_and_wait (("No plug-in were found.\nPlug-ins provide most of the functionnalities of Cairo-Dock (animations, applets, views, etc).\nSee http://glx-dock.org for more information.\nSince there is almost no meaning in running the dock without them, the application will quit now."), pIcon, CAIRO_CONTAINER (g_pMainDock));
		return 0;
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
	}
	
	g_print ("bFirstLaunch: %d; bNewVersion: %d\n", bFirstLaunch, bNewVersion);
	if (bFirstLaunch)  // first launch => set up config
	{
		g_timeout_add_seconds (4, _cairo_dock_first_launch_setup, NULL);
	}
	else if (bNewVersion)  // nouvelle version -> changelog (si c'est le 1er lancement, inutile de dire ce qui est nouveau, et de plus on a deja le message de bienvenue).
	{
		gchar *cChangeLogFilePath = g_strdup_printf ("%s/ChangeLog.txt", CAIRO_DOCK_SHARE_DATA_DIR);
		GKeyFile *pKeyFile = cairo_dock_open_key_file (cChangeLogFilePath);
		if (pKeyFile != NULL)
		{
			gchar *cKeyName = g_strdup_printf ("%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion);  // version sans les "alpha", "beta", "rc", etc.
			gchar *cChangeLogMessage = g_key_file_get_string (pKeyFile, "ChangeLog", cKeyName, NULL);
			g_free (cKeyName);
			if (cChangeLogMessage != NULL)
			{
				Icon *pFirstIcon = cairo_dock_get_first_icon (g_pMainDock->icons);
				myDialogsParam.dialogTextDescription.bUseMarkup = TRUE;
				cairo_dock_show_temporary_dialog_with_default_icon (gettext (cChangeLogMessage), pFirstIcon, CAIRO_CONTAINER (g_pMainDock), 0);
				myDialogsParam.dialogTextDescription.bUseMarkup = FALSE;
				g_free (cChangeLogMessage);
			}
			g_key_file_free (pKeyFile);
		}
		// In case something has changed in Compiz/Gtk/others, we also run the script on a new version of the dock.
		g_timeout_add_seconds (4, _cairo_dock_first_launch_setup, NULL);
	}
	else if (cExcludeModule != NULL && ! bMaintenance)
	{
		gchar *cMessage;
		if (s_iNbCrashes < 2)
			cMessage = g_strdup_printf (_("The module '%s' may have encountered a problem.\nIt has been restored successfully, but if it happens again, please report it at http://glx-dock.org"), cExcludeModule);
		else
			cMessage = g_strdup_printf ("The module '%s' has been deactivated because it may have caused some problems.\nYou can reactivate it, if it happens again thanks to report it at http://glx-dock.org", cExcludeModule);
		
		CairoDockModule *pModule = cairo_dock_find_module_from_name (cExcludeModule);
		Icon *icon = cairo_dock_get_dialogless_icon ();
		cairo_dock_show_temporary_dialog_with_icon (cMessage, icon, CAIRO_CONTAINER (g_pMainDock), 15000., (pModule ? pModule->pVisitCard->cIconFilePath : NULL));
		g_free (cMessage);
	}
	
	if (! bTesting)
		g_timeout_add_seconds (5, _cairo_dock_successful_launch, GINT_TO_POINTER (bFirstLaunch));
	
	g_print ("\n\nTODO:\n"
	"- test drop (Shortcuts, between applets or applis, Panel view, etc).\n"
	"- draw a preview of the dock in opengl\n"
	"- handle icon path in .desktop files.\n"
	"- kde integration ++\n"
	"- find Kwin config tool for Composite-manager\n"
	"- review Help hints\n"
	"- display Help GUI in simple mode\n"
	"- drop folder: wrong icon order\n"
	"- Drop2Share: Uppix\n"
	"- clean traces (especially from the class manager)\n"
	"- add actions for the Help module\n"
	"\n");
	
	gtk_main ();
	
	signal (SIGSEGV, NULL);  // Segmentation violation
	signal (SIGFPE, NULL);  // Floating-point exception
	signal (SIGILL, NULL);  // Illegal instruction
	signal (SIGABRT, NULL);
	signal (SIGTERM, NULL);
	gldi_free_all ();
	
	rsvg_term ();
	xmlCleanupParser ();
	g_string_free (s_pLaunchCommand, TRUE);
	
	cd_message ("Bye bye !");
	g_print ("\033[0m\n");

	return 0;
}

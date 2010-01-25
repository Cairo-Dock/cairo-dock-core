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
** Original idea :
**    Mirco Mueller, June 2006.
**
*********************** VERSION 0 (2006)*********************
** author(s):
**    Mirco "MacSlow" Mueller <macslow@bangang.de>
**    Behdad Esfahbod <behdad@behdad.org>
**    David Reveman <davidr@novell.com>
**    Karl Lattimer <karl@qdh.org.uk>
**
** notes :
**    Originally conceived as a stress-test for cairo, librsvg, and glitz.
**
** notes from original author:
**
**    I just know that some folks will bug me regarding this, so... yes there's
**    nearly everything hard-coded, it is not nice, it is not very usable for
**    easily (without any hard work) making a full dock-like application out of
**    this, please don't email me asking to me to do so... for everybody else
**    feel free to make use of it beyond this being a small but heavy stress
**    test. I've written this on an Ubuntu-6.06 box running Xgl/compiz. The
**    icons used are from the tango-project...
**
**        http://tango-project.org/
**
**    Over the last couple of days Behdad and David helped me (MacSlow) out a
**    great deal by sending me additional tweaked and optimized versions. I've
**    now merged all that with my recent additions.
**
*********************** VERSION 0.1.0 and above (2007-2009)*********************
**
** author(s) :
**     Fabrice Rey <fabounet@users.berlios.de>
**
** notes :
**     I've completely rewritten the calculation part, and the callback system.
**     Plus added a conf file that allows to dynamically modify most of the parameters.
**     Plus a visible zone that make the hiding/showing more friendly.
**     Plus a menu and the drag'n'drop ability.
**     Also I've separated functions in several files in order to make the code more readable.
**     Now it sems more like a real dock !
**
**     Edit : plus a taskbar, plus an applet system,
**            plus the container ability, plus different views, plus the top and vertical position, ...
**
**
*******************************************************************************/

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h> 
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <gtk/gtkgl.h>

#include "cairo-dock-icons.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-menu.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-dbus.h"
#include "cairo-dock-load.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-gauge.h"
#include "cairo-dock-graph.h"
#include "cairo-dock-internal-system.h"

CairoDock *g_pMainDock;  // pointeur sur le dock principal.
GdkWindowTypeHint g_iWmHint = GDK_WINDOW_TYPE_HINT_DOCK;  // hint pour la fenetre du dock principal.

int g_iScreenWidth[2];  // dimensions de l'ecran physique sur lequel reside le dock
int g_iScreenHeight[2];
int g_iXScreenWidth[2];  // dimensions de l'ecran logique X.
int g_iXScreenHeight[2];

gchar *g_cCurrentThemePath = NULL;  // le chemin vers le repertoire du theme courant.
gchar *g_cCurrentLaunchersPath = NULL;  // le chemin vers le repertoire des lanceurs/icones du theme courant.
gchar *g_cConfFile = NULL;  // le chemin du fichier de conf.
gchar *g_cCairoDockDataDir = NULL;  // le repertoire ou on va chercher la config.

cairo_surface_t *g_pVisibleZoneSurface = NULL;  // surface de la zone de rappel.
double g_fBackgroundImageWidth = 0, g_fBackgroundImageHeight = 0;  // sa taille reelle.
cairo_surface_t *g_pBackgroundSurface = NULL;  // surface associee a l'image du fond, de la taille de l'image du fond.
cairo_surface_t *g_pBackgroundSurfaceFull = NULL;  // surface associee aux decorations, de 2 fois la taille de la fenetre.

int g_iNbDesktops;  // nombre de bureaux.
int g_iNbViewportX, g_iNbViewportY;  // nombre de "faces du cube".
int g_iNbNonStickyLaunchers = 0;
cairo_surface_t *g_pActiveIndicatorSurface = NULL;
double g_fActiveIndicatorWidth, g_fActiveIndicatorHeight;

cairo_surface_t *g_pIconBackgroundImageSurface =  NULL;  // Surface cairo de l'image de fond pour les icones des launchers et applis.
double g_iIconBackgroundImageWidth, g_iIconBackgroundImageHeight;

gboolean g_bKeepAbove = FALSE;
gboolean g_bSkipPager = TRUE;
gboolean g_bSkipTaskbar = TRUE;
gboolean g_bSticky = TRUE;

gboolean g_bUseGlitz = FALSE;
gboolean g_bVerbose = FALSE;

int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;
CairoDockDesktopEnv g_iDesktopEnv = CAIRO_DOCK_UNKNOWN_ENV;

cairo_surface_t *g_pIndicatorSurface = NULL;
double g_fIndicatorWidth, g_fIndicatorHeight;

cairo_surface_t *g_pClassIndicatorSurface = NULL;
double g_fClassIndicatorWidth, g_fClassIndicatorHeight;

CairoDockDesktopBackground *g_pFakeTransparencyDesktopBg = NULL;
//int g_iDamageEvent = 0;

gchar *g_cThemeServerAdress = NULL;
gboolean g_bEasterEggs = FALSE;
gboolean g_bLocked = FALSE;

gboolean g_bUseOpenGL = FALSE;
gboolean g_bForceOpenGL = FALSE;
gboolean g_bForcedOpenGL = FALSE;
gboolean g_bForceCairo = FALSE;
gboolean g_bIndirectRendering = FALSE;
GdkGLConfig* g_pGlConfig = NULL;
GLuint g_iBackgroundTexture=0;
GLuint g_iIndicatorTexture=0;
GLuint g_iActiveIndicatorTexture=0;
GLuint g_iClassIndicatorTexture=0;
GLuint g_iIconBackgroundTexture=0;
GLuint g_iVisibleZoneTexture=0;
GLuint g_pGradationTexture[2]={0, 0};

CairoDockModuleInstance *g_pCurrentModule = NULL;

static gchar *cLaunchCommand = NULL;

static gboolean _cairo_dock_successful_launch (gpointer data)
{
	if (g_str_has_suffix (cLaunchCommand, " -m"))
		cLaunchCommand[strlen (cLaunchCommand)-3] = '\0';  // on enleve le mode maintenance.
	return FALSE;
}
static void _cairo_dock_intercept_signal (int signal)
{
	cd_warning ("Cairo-Dock has crashed (sig %d).\nIt will be restarted now (%s).\nFeel free to report this bug on cairo-dock.org to help improving the dock !", signal, cLaunchCommand);
	g_print ("info on the system :\n");
	int r = system ("uname -a");
	if (g_pCurrentModule != NULL)
	{
		g_print ("The applet '%s' may be the culprit", g_pCurrentModule->pModule->pVisitCard->cModuleName);
		cLaunchCommand = g_strdup_printf ("%s -x \"%s\"", cLaunchCommand, g_pCurrentModule->pModule->pVisitCard->cModuleName);
	}
	else
	{
		g_print ("Couldn't guess if it was an applet's fault or not. It may have crashed inside the core or inside a thread\n");
	}
	execl ("/bin/sh", "/bin/sh", "-c", cLaunchCommand, (char *)NULL);  // on ne revient pas de cette fonction.
	//execlp ("cairo-dock", "cairo-dock", cLaunchCommand, (char *)0);
	cd_warning ("Sorry, couldn't restart the dock");
}
static void _cairo_dock_set_signal_interception (void)
{
	signal (SIGSEGV, _cairo_dock_intercept_signal);  // Segmentation violation
	signal (SIGFPE, _cairo_dock_intercept_signal);  // Floating-point exception
	signal (SIGILL, _cairo_dock_intercept_signal);  // Illegal instruction
	signal (SIGABRT, _cairo_dock_intercept_signal);  // Abort
}

static void _accept_metacity_composition (int iClickedButton, GtkWidget *pInteractiveWidget, gpointer data, CairoDialog *pDialog)
{
	if (iClickedButton == 1)  // clic explicite sur "cancel"
	{
		int r = system ("gconftool-2 -s '/apps/metacity/general/compositing_manager' --type bool false");
	}
}

static gboolean on_delete_maintenance_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	g_print ("%s ()\n", __func__);
	if (pBlockingLoop != NULL && g_main_loop_is_running (pBlockingLoop))
	{
		g_main_loop_quit (pBlockingLoop);
	}
	return FALSE;  // TRUE <=> ne pas detruire la fenetre.
}

#define _create_dir_or_die(cDirPath) do {\
	if (g_mkdir (cDirPath, 7*8*8+7*8+7) != 0) {\
		cd_warning ("couldn't create directory %s", cDirPath);\
		return 1; } } while (0)

int main (int argc, char** argv)
{
	int i, iNbMaintenance=0;
	GString *sCommandString = g_string_new (argv[0]);
	gchar *cDisableApplet = NULL;
	for (i = 1; i < argc; i ++)
	{
		g_print ("'%s'\n", argv[i]);
		if (strcmp (argv[i], "-m") == 0)
			iNbMaintenance ++;
		
		g_string_append_printf (sCommandString, " %s", argv[i]);
	}
	if (iNbMaintenance > 1)
	{
		g_print ("Sorry, Cairo-Dock has encoutered some problems, and will quit.\n");
		return 1;
	}
	g_string_append (sCommandString, " -m");  // on relance avec le mode maintenance.
	cLaunchCommand = sCommandString->str;
	g_string_free (sCommandString, FALSE);
	
	cd_log_init(FALSE);
	//No log
	cd_log_set_level(0);
	
	gtk_init (&argc, &argv);
	
	GError *erreur = NULL;
	
	//\___________________ On recupere quelques options.
	gboolean bSafeMode = FALSE, bMaintenance = FALSE, bNoSkipPager = FALSE, bNoSkipTaskbar = FALSE, bNoSticky = FALSE, bToolBarHint = FALSE, bNormalHint = FALSE, bCappuccino = FALSE, bExpresso = FALSE, bCafeLatte = FALSE, bPrintVersion = FALSE, bTesting = FALSE;
	gchar *cEnvironment = NULL, *cUserDefinedDataDir = NULL, *cVerbosity = 0, *cUserDefinedModuleDir = NULL, *cExcludeModule = NULL;
	GOptionEntry TableDesOptions[] =
	{
		{"log", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cVerbosity,
			"log verbosity (debug,message,warning,critical,error) default is warning", NULL},
#ifdef HAVE_GLITZ
		{"glitz", 'g', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bUseGlitz,
			"force Glitz backend (hardware acceleration for cairo, needs a glitz-enabled libcairo)", NULL},
#endif
		{"cairo", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bForceCairo,
			"force cairo backend", NULL},
		{"opengl", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bForceOpenGL,
			"force OpenGL backend", NULL},
		{"indirect", 'i', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bIndirectRendering,
			"use Indirect Rendering mode for OpenGL", NULL},
		{"keep-above", 'a', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bKeepAbove,
			"keep the dock above other windows whatever", NULL},
		{"no-skip-pager", 'p', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bNoSkipPager,
			"show the dock in pager", NULL},
		{"no-skip-taskbar", 'b', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bNoSkipTaskbar,
			"show the dock in taskbar", NULL},
		{"no-sticky", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bNoSticky,
			"don't make the dock appear on all desktops", NULL},
		{"toolbar-hint", 't', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bToolBarHint,
			"force the window manager to consider cairo-dock as a toolbar instead of a dock", NULL},
		{"normal-hint", 'n', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bNormalHint,
			"force the window manager to consider cairo-dock as a normal appli instead of a dock", NULL},
		{"env", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cEnvironment,
			"force the dock to consider this environnement - it may crash the dock if not set properly.", NULL},
		{"dir", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cUserDefinedDataDir,
			"force the dock to load this directory, instead of ~/.config/cairo-dock.", NULL},
		{"maintenance", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bMaintenance,
			"allow to edit the config before the dock is started and show the config panel on start", NULL},
		{"exclude", 'x', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cExcludeModule,
			"exclude a given plug-in from activating (it is still loaded though)", NULL},
		{"safe-mode", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bSafeMode,
			"don't load any plug-ins", NULL},
		{"capuccino", 'C', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bCappuccino,
			"Cairo-Dock makes anything, including coffee !", NULL},
		{"expresso", 'X', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bExpresso,
			"Cairo-Dock makes anything, including coffee !", NULL},
		{"cafe-latte", 'L', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bCafeLatte,
			"Cairo-Dock makes anything, including coffee !", NULL},
		{"version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bPrintVersion,
			"print version and quit.", NULL},
		{"modules-dir", 'M', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cUserDefinedModuleDir,
			"ask the dock to load additionnal modules contained in this directory (though it is unsafe for your dock to load unnofficial modules).", NULL},
		{"testing", 'T', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bTesting,
			"for debugging purpose only. The crash manager will not be started to hunt down the bugs.", NULL},
		{"easter-eggs", 'E', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bEasterEggs,
			"for debugging purpose only. Some hidden and still unstable options will be activated.", NULL},
		{"server", 'S', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&g_cThemeServerAdress,
			"address of a server containing additional themes. This will overwrite the default server address.", NULL},
		{"locked", 'k', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bLocked,
			"lock the dock so that any modification is impossible for users.", NULL},
		{NULL}
	};

	GOptionContext *context = g_option_context_new ("Cairo-Dock");
	g_option_context_add_main_entries (context, TableDesOptions, NULL);
	g_option_context_parse (context, &argc, &argv, &erreur);
	if (erreur != NULL)
	{
		g_print ("ERROR in options : %s\n", erreur->message);
		return 1;
	}

/* FIXME: utiliser l'option --enable-verbose du configure, l'idee etant que les fonctions de log sont non definies dans les versions officielles, histoire de pas faire le test tout le temps.
#ifndef CAIRO_DOCK_VERBOSE
	if (cVerbosity != NULL)
	{
		g_print ("Cairo-Dock was not compiled with verbose, configure it with --enable-verbose for that\n");
		g_free (cVerbosity);
		cVerbosity = NULL;
	}
#endif */
	if (bPrintVersion)
	{
		g_print ("%s\n", CAIRO_DOCK_VERSION);
		return 0;
	}
	
	if (g_bLocked)
		g_print ("Cairo-Dock will be locked.\n");
	
	cd_log_set_level_from_name (cVerbosity);
	g_free (cVerbosity);

	g_bSkipPager = ! bNoSkipPager;
	g_bSkipTaskbar = ! bNoSkipTaskbar;
	g_bSticky = ! bNoSticky;

	if (bToolBarHint)
		g_iWmHint = GDK_WINDOW_TYPE_HINT_TOOLBAR;
	if (bNormalHint)
		g_iWmHint = GDK_WINDOW_TYPE_HINT_NORMAL;
	if (cEnvironment != NULL)
	{
		if (strcmp (cEnvironment, "gnome") == 0)
			g_iDesktopEnv = CAIRO_DOCK_GNOME;
		else if (strcmp (cEnvironment, "kde") == 0)
			g_iDesktopEnv = CAIRO_DOCK_KDE;
		else if (strcmp (cEnvironment, "xfce") == 0)
			g_iDesktopEnv = CAIRO_DOCK_XFCE;
		else if (strcmp (cEnvironment, "none") == 0)
			g_iDesktopEnv = CAIRO_DOCK_UNKNOWN_ENV;
		else
			cd_warning ("unknown environnment '%s'", cEnvironment);
		g_free (cEnvironment);
	}
#ifdef HAVE_GLITZ
	cd_message ("Compiled with Glitz (hardware acceleration support for cairo)n");
#endif
	
	if (bCappuccino)
	{
		g_print ("Please insert one coin into your PC.\n");
		return 0;
	}
	if (bExpresso)
	{
		g_print ("Sorry, no more sugar; please try again later.\n");
		return 0;
	}
	if (bCafeLatte)
	{
		g_print ("Honestly, you trust someone who includes such options in his code ?\n");
		return 0;
	}
	
	//\___________________ On internationalise l'appli.
	bindtextdomain (CAIRO_DOCK_GETTEXT_PACKAGE, CAIRO_DOCK_LOCALE_DIR);
	bind_textdomain_codeset (CAIRO_DOCK_GETTEXT_PACKAGE, "UTF-8");
	textdomain (CAIRO_DOCK_GETTEXT_PACKAGE);

	//\___________________ On teste l'existence du repertoire des donnees ~/.config/cairo-dock.
	gboolean bFirstLaunch = FALSE;
	int r;
	if (cUserDefinedDataDir != NULL)
	{
		g_cCairoDockDataDir = cUserDefinedDataDir;
		cUserDefinedDataDir = NULL;
		if (! g_file_test (g_cCairoDockDataDir, G_FILE_TEST_IS_DIR))
		{
			_create_dir_or_die (g_cCairoDockDataDir);
		}
	}
	else
	{
		g_cCairoDockDataDir = g_strdup_printf ("%s/.config/%s", getenv("HOME"), CAIRO_DOCK_DATA_DIR);
		if (! g_file_test (g_cCairoDockDataDir, G_FILE_TEST_IS_DIR))
		{
			gchar *cOldDataDir = g_strdup_printf ("%s/.cairo-dock", getenv("HOME"));
			if (g_file_test (cOldDataDir, G_FILE_TEST_IS_DIR))  // l'ancien rep existe, on le deplace.
			{
				cd_warning ("Cairo-Dock's data dir is now located in ~/.config, it will be moved there");
				gchar *cCommand = g_strdup_printf ("mkdir \"%s/.config\" > /dev/null", getenv("HOME"));
				cd_message (cCommand);
				r = system (cCommand);
				g_free (cCommand);
					
				cCommand = g_strdup_printf ("mv \"%s\" \"%s\"", cOldDataDir, g_cCairoDockDataDir);
				cd_message (cCommand);
				r = system (cCommand);
				g_free (cCommand);
				
				cCommand = g_strdup_printf ("sed -i \"s/~\\/.cairo-dock/~\\/.config\\/%s/g\" \"%s/%s/%s\"", CAIRO_DOCK_DATA_DIR, g_cCairoDockDataDir, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_CONF_FILE);
				cd_message (cCommand);
				r = system (cCommand);
				g_free (cCommand);
			}
			else
			{
				_create_dir_or_die (g_cCairoDockDataDir);
				bFirstLaunch = TRUE;
			}
			g_free (cOldDataDir);
		}
	}
	gchar *cThemesDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR);
	if (! g_file_test (cThemesDir, G_FILE_TEST_IS_DIR))
	{
		_create_dir_or_die (cThemesDir);
	}
	g_free (cThemesDir);
	gchar *cExtrasDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR);
	if (! g_file_test (cExtrasDir, G_FILE_TEST_IS_DIR))
	{
		_create_dir_or_die (cExtrasDir);
	}
	g_free (cExtrasDir);
	g_cCurrentThemePath = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_CURRENT_THEME_NAME);
	if (! g_file_test (g_cCurrentThemePath, G_FILE_TEST_IS_DIR))
	{
		_create_dir_or_die (g_cCurrentThemePath);
	}
	g_cCurrentLaunchersPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
	if (! g_file_test (g_cCurrentLaunchersPath, G_FILE_TEST_IS_DIR))
	{
		_create_dir_or_die (g_cCurrentLaunchersPath);
	}
	gchar *cLocalIconsPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
	if (! g_file_test (cLocalIconsPath, G_FILE_TEST_IS_DIR))
	{
		_create_dir_or_die (cLocalIconsPath);
		if (! bFirstLaunch)
		{
			cd_warning ("Cairo-Dock's local icons are now located in the 'icons' folder, they will be moved there");
			gchar *cCommand = g_strdup_printf ("cd \"%s\" && mv *.svg *.png *.xpm *.jpg *.bmp *.gif \"%s\" > /dev/null", g_cCurrentLaunchersPath, cLocalIconsPath);
			cd_message (cCommand);
			r = system (cCommand);
			g_free (cCommand);
		}
	}
	g_free (cLocalIconsPath);
	
	//\___________________ On initialise les numeros de version.
	cairo_dock_get_version_from_string (CAIRO_DOCK_VERSION, &g_iMajorVersion, &g_iMinorVersion, &g_iMicroVersion);

	//\___________________ On initialise le gestionnaire de docks (a faire en 1er).
	cairo_dock_initialize_dock_manager ();
	
	//\___________________ On initialise le gestionnaire de vues.
	cairo_dock_initialize_renderer_manager ();
	
	//\___________________ On initialise le multi-threading.
	if (!g_thread_supported ())
		g_thread_init (NULL);
	
	//\___________________ On initialise le support de X.
	cairo_dock_initialize_X_support ();
	cairo_dock_start_listening_X_events ();
	
	//\___________________ On initialise le keybinder.
	cd_keybinder_init();
	
	//\___________________ On detecte l'environnement de bureau (apres les applis et avant les modules).
	if (g_iDesktopEnv == CAIRO_DOCK_UNKNOWN_ENV)
		g_iDesktopEnv = cairo_dock_guess_environment ();
	cd_debug ("environnement de bureau : %d", g_iDesktopEnv);
	
	//\___________________ On initialise le support d'OpenGL.
	if (! g_bForceCairo && ! g_bUseGlitz)
		g_pGlConfig = cairo_dock_get_opengl_config (g_bForceOpenGL, &g_bForcedOpenGL);
	g_bUseOpenGL = (g_pGlConfig != NULL);
	
	//\___________________ On enregistre les rendus de donnees.
	cairo_dock_register_data_renderer_entry_point ("gauge", (CairoDataRendererNewFunc) cairo_dock_new_gauge);
	cairo_dock_register_data_renderer_entry_point ("graph", (CairoDataRendererNewFunc) cairo_dock_new_graph);
	
	//\___________________ On initialise le gestionnaire de modules et on pre-charge les modules existant.
	if (g_module_supported () && ! bSafeMode)
	{
		cairo_dock_initialize_module_manager (CAIRO_DOCK_MODULES_DIR);
		
		if (cUserDefinedModuleDir != NULL)
		{
			cairo_dock_initialize_module_manager (cUserDefinedModuleDir);
			g_free (cUserDefinedModuleDir);
			cUserDefinedModuleDir = NULL;
		}
	}
	else
		cairo_dock_initialize_module_manager (NULL);
	
	//\___________________ On definit le backend des GUI.
	cairo_dock_load_user_gui_backend ();
	
	//\___________________ On enregistre nos notifications.
	cairo_dock_register_notification (CAIRO_DOCK_BUILD_MENU,
		(CairoDockNotificationFunc) cairo_dock_notification_build_menu,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_DROP_DATA,
		(CairoDockNotificationFunc) cairo_dock_notification_drop_data,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_CLICK_ICON,
		(CairoDockNotificationFunc) cairo_dock_notification_click_icon,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_MIDDLE_CLICK_ICON,
		(CairoDockNotificationFunc) cairo_dock_notification_middle_click_icon,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_SCROLL_ICON,
		(CairoDockNotificationFunc) cairo_dock_notification_scroll_icon,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_RENDER_DOCK,
		(CairoDockNotificationFunc) cairo_dock_render_dock_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_RENDER_ICON,
		(CairoDockNotificationFunc) cairo_dock_render_icon_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_RENDER_ICON,
		(CairoDockNotificationFunc) cairo_dock_render_icon_notification_cairo,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_RENDER_DESKLET,
		(CairoDockNotificationFunc) cairo_dock_render_desklet_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_INSERT_ICON,
		(CairoDockNotificationFunc) cairo_dock_on_insert_remove_icon_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_REMOVE_ICON,
		(CairoDockNotificationFunc) cairo_dock_on_insert_remove_icon_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_UPDATE_ICON,
		(CairoDockNotificationFunc) cairo_dock_update_inserting_removing_icon_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_STOP_ICON,
		(CairoDockNotificationFunc) cairo_dock_stop_inserting_removing_icon_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_UPDATE_FLYING_CONTAINER,
		(CairoDockNotificationFunc) cairo_dock_update_flying_container_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_RENDER_FLYING_CONTAINER,
		(CairoDockNotificationFunc) cairo_dock_render_flying_container_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
	
	//\___________________ On initialise la gestion des crash.
	if (! bTesting)
		_cairo_dock_set_signal_interception ();
	
	//\___________________ On charge le dernier theme ou on demande a l'utilisateur d'en choisir un.
	g_cConfFile = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
	
	if (bMaintenance)
	{
		if (cExcludeModule != NULL)
		{
			cd_warning ("The module '%s' has been deactivated because it may have caused some problems.\nYou can reactivate it, if it happens again thanks to report it at http://cairo-dock.org\n", cExcludeModule);
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
		///g_object_set_data (G_OBJECT (pWindow), "loop", pBlockingLoop);
		g_signal_connect (G_OBJECT (pWindow),
			"delete-event",
			G_CALLBACK (on_delete_maintenance_gui),
			pBlockingLoop);

		g_print ("debut de boucle bloquante ...\n");
		GDK_THREADS_LEAVE ();
		g_main_loop_run (pBlockingLoop);
		GDK_THREADS_ENTER ();
		g_print ("fin de boucle bloquante\n");
		
		g_main_loop_unref (pBlockingLoop);
		g_print ("plop\n");
	}
	
	if (cExcludeModule != NULL)
	{
		g_print ("on enleve %s de '%s'\n", cExcludeModule, cLaunchCommand);
		gchar *str = g_strstr_len (cLaunchCommand, -1, " -x ");
		if (str)
		{
			*str = '\0';  // enleve le module de la ligne de commande, ainsi que le le -m courant.
			g_print ("cLaunchCommand <- '%s'\n", cLaunchCommand);
		}
		else
		{
			g_free (cExcludeModule);
			cExcludeModule = NULL;
		}
	}
	
	cd_message ("loading theme ...");
	if (! g_file_test (g_cConfFile, G_FILE_TEST_EXISTS))
	{
		gchar *cCommand = g_strdup_printf ("/bin/cp -r \"%s\"/* \"%s/%s\"", CAIRO_DOCK_SHARE_DATA_DIR"/themes/_default_", g_cCairoDockDataDir, CAIRO_DOCK_CURRENT_THEME_NAME);
		cd_message (cCommand);
		r = system (cCommand);
		g_free (cCommand);
		
		cairo_dock_mark_theme_as_modified (FALSE);  // on ne proposera pas de sauvegarder ce theme.
		cairo_dock_load_current_theme ();
	}
	else
		cairo_dock_load_current_theme ();
	
	//\___________________ On affiche un petit message de bienvenue.
	if (bFirstLaunch)
	{
		cairo_dock_show_general_message (_("Welcome in Cairo-Dock2 !\nA default and simple theme has been loaded.\nYou can either familiarize yourself with the dock or choose another theme with right-click -> Cairo-Dock -> Manage themes.\nA useful help is available by right-click -> Cairo-Dock -> Help.\nIf you have any question/request/remark, please pay us a visit at http://cairo-dock.org.\nHope you will enjoy this soft !\n  (you can now click on this dialog to close it)"), 0);
		
		GdkScreen *pScreen = gdk_screen_get_default ();
		if (! mySystem.bUseFakeTransparency && ! gdk_screen_is_composited (pScreen))
		{
			cd_warning ("no composite manager found");
			// Si l'utilisateur utilise Metacity, on lui propose d'activer le composite.
			gchar *cPsef = cairo_dock_launch_command_sync ("pgrep metacity");  // 'ps' ne marche pas, il faut le lancer dans un script :-/
			if (cPsef != NULL && *cPsef != '\0')  // "metacity" a ete trouve.
			{
				Icon *pIcon = cairo_dock_get_dialogless_icon ();
				
				int iAnswer= cairo_dock_ask_question_and_wait (_("To remove the black rectangle around the dock, you need to activate a composite manager.\nFor instance, it can be done by activating the desktop effects, launching Compiz, or activating the composition in Metacity.\nI can do this lattest operation for you, do you want to proceed ?"), pIcon, CAIRO_CONTAINER (g_pMainDock));
				if (iAnswer == GTK_RESPONSE_YES)
				{
					int r = system ("gconftool-2 -s '/apps/metacity/general/compositing_manager' --type bool true");
					cairo_dock_show_dialog_with_question (_("Do you want to keep this setting ?"), pIcon, CAIRO_CONTAINER (g_pMainDock), NULL, (CairoDockActionOnAnswerFunc) _accept_metacity_composition, NULL, NULL);
				}
			}
			else  // sinon il a droit a un "message a caractere informatif".
			{
				cairo_dock_show_general_message (_("To remove the black rectangle around the dock, you need to activate a composite manager.\nFor instance, it can be done by activating the desktop effects, launching Compiz, or activating the composition in Metacity.\nIf your machine can't support composition, Cairo-Dock can emulate it; this option is in the 'System' module of the configuration, at the bottom of the page."), 0);
			}
			g_free (cPsef);
		}
	}
	else if (cExcludeModule != NULL && ! bMaintenance)
	{
		gchar *cMessage = g_strdup_printf (_("The module '%s' may have encountered a problem.\nIt has been restared successfully, but if it happens again, thanks to report it to us at http://cairo-dock.org"), cExcludeModule);
		
		CairoDockModule *pModule = cairo_dock_find_module_from_name (cExcludeModule);
		Icon *icon = cairo_dock_get_dialogless_icon ();
		cairo_dock_show_temporary_dialog_with_icon (cMessage, icon, CAIRO_CONTAINER (g_pMainDock), 15000., (pModule ? pModule->pVisitCard->cIconFilePath : NULL));
		g_free (cMessage);
	}
	
	if (cairo_dock_get_nb_modules () <= 1)  // le module Help est inclus de base.
		cairo_dock_show_general_message (_("No plug-in were found.\nTo make the most of Cairo-Dock, please consider installing its plug-ins.\nThey provide a lot of interesting functionnalities, animations, etc,\n that you certainly don't want to miss !"), 0);
	
	//\___________________ On affiche le changelog en cas de nouvelle version.
	gchar *cLastVersionFilePath = g_strdup_printf ("%s/.cairo-dock-last-version", g_cCairoDockDataDir);
	gboolean bWriteChangeLog;
	if (! g_file_test (cLastVersionFilePath, G_FILE_TEST_EXISTS))
	{
		bWriteChangeLog = TRUE;
	}
	else
	{
		gsize length = 0;
		gchar *cContent = NULL;
		g_file_get_contents (cLastVersionFilePath,
			&cContent,
			&length,
			NULL);
		if (length > 0 && strcmp (cContent, CAIRO_DOCK_VERSION) == 0)
			bWriteChangeLog = FALSE;
		else
			bWriteChangeLog = TRUE;
		g_free (cContent);
	}

	g_file_set_contents (cLastVersionFilePath,
		CAIRO_DOCK_VERSION,
		-1,
		NULL);
	g_free (cLastVersionFilePath);

	if (bWriteChangeLog && ! bFirstLaunch)
	{
		gchar *cChangeLogFilePath = g_strdup_printf ("%s/ChangeLog.txt", CAIRO_DOCK_SHARE_DATA_DIR);
		GKeyFile *pKeyFile = g_key_file_new ();
		g_key_file_load_from_file (pKeyFile, cChangeLogFilePath, 0, &erreur);  // pas de commentaire utile.
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		else
		{
			gchar *cKeyName = g_strdup_printf ("%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion);  // version sans les "alpha", "beta", "rc", etc.
			gchar *cChangeLogMessage = g_key_file_get_string (pKeyFile, "ChangeLog", cKeyName, &erreur);
			g_free (cKeyName);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
			}
			else
			{
				Icon *pFirstIcon = cairo_dock_get_first_icon (g_pMainDock->icons);
				cairo_dock_show_temporary_dialog_with_default_icon (gettext (cChangeLogMessage), pFirstIcon, CAIRO_CONTAINER (g_pMainDock), 0);
			}
			g_free (cChangeLogMessage);
		}
	}
	
	if (! bTesting)
		g_timeout_add_seconds (5, _cairo_dock_successful_launch, NULL);
	
	g_print ("\n2eme DOCK PRINCIPAL AVEC 1 ICONE : BORD TRONQUE EN VERTICAL\n\n");
	g_print ("\nPLAN 3D NE PREND PAS TOUT L'ECRAN\n\n");
	g_print ("\nBATTERIE VERS 100%% (?)\n\n");
	
	g_print ("\nINDICATEURS EN HAUT (RETOURNES) ET CAIRO\n\n");
	g_print ("=> AUTO-HIDE WHEN SWITCHING DESKTOP IN CAIRO MODE\n");
	g_print ("PREVUE EN VUE PARABOLIQUE\n");
	g_print ("MP AVEC CERTAINS NOMS D'ALBUM/ARTISTE ?\n");
	
	g_print ("\nTEXTURE FROM PIXMAP\n\n");
	g_print ("\nNOUVELLE INSTANCE COPIEE SUR LA 1ERE\n\n");
	g_print ("\nJAUGES : LOGO\n\n");
	
	/*if (strcmp((cairo_dock_launch_command_sync ("date +%m%d")), "0101") == 0)
	{
		Icon *pFirstIcon = cairo_dock_get_first_icon (g_pMainDock->icons);
		cairo_dock_show_temporary_dialog_with_default_icon ("Happy New Year every body !\n\tThe Cairo-Dock Team", pFirstIcon, CAIRO_CONTAINER (g_pMainDock), 0);
	}*/
	
	gtk_main ();
	
	signal (SIGSEGV, NULL);  // Segmentation violation
	signal (SIGFPE, NULL);  // Floating-point exception
	signal (SIGILL, NULL);  // Illegal instruction
	signal (SIGABRT, NULL);
	cairo_dock_free_all_docks ();
	
	rsvg_term ();
	
	cd_message ("Bye bye !");
	g_print ("\033[0m\n");

	return 0;
}

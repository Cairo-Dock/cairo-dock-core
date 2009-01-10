/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
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
*********************** VERSION 0.1.0 and above (2007-2008)*********************
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
#include "cairo-dock-animations.h"

CairoDock *g_pMainDock;  // pointeur sur le dock principal.
int g_iWmHint = GDK_WINDOW_TYPE_HINT_DOCK;  // hint pour la fenetre du dock principal.

int g_iScreenWidth[2];  // dimensions de l'ecran physique sur lequel reside le dock
int g_iScreenHeight[2];
int g_iXScreenWidth[2];  // dimensions de l'ecran logique X.
int g_iXScreenHeight[2];
int g_iScreenOffsetX = 0;
int g_iScreenOffsetY = 0;

gchar *g_cCurrentThemePath = NULL;  // le chemin vers le repertoire du theme courant.
gchar *g_cCurrentLaunchersPath = NULL;  // le chemin vers le repertoire des lanceurs/icones du theme courant.
gchar *g_cConfFile = NULL;  // le chemin du fichier de conf.
gchar *g_cCairoDockDataDir = NULL;  // le repertoire ou on va chercher les .desktop.

cairo_surface_t *g_pVisibleZoneSurface = NULL;  // surface de la zone de rappel.
double g_fBackgroundImageWidth = 0, g_fBackgroundImageHeight = 0;  // sa taille reelle.
cairo_surface_t *g_pBackgroundSurface = NULL;  // surface associee a l'image du fond, de la taille de l'image du fond.
cairo_surface_t *g_pBackgroundSurfaceFull = NULL;  // surface associee aux decorations, de 2 fois la taille de la fenetre.

int g_iNbDesktops;  // nombre de bureaux.
int g_iNbViewportX, g_iNbViewportY;  // nombre de "faces du cube".
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

cairo_surface_t *g_pIndicatorSurface[2] = {NULL, NULL};
double g_fIndicatorWidth, g_fIndicatorHeight;

cairo_surface_t *g_pDropIndicatorSurface = NULL;
double g_fDropIndicatorWidth, g_fDropIndicatorHeight;

cairo_surface_t *g_pDesktopBgSurface = NULL;  // image en fond d'ecran.
//int g_iDamageEvent = 0;

gboolean g_bDisplayDropEmblem = FALSE; // indicateur de drop
gchar *g_cThemeServerAdress = NULL;
gboolean g_bEasterEggs = FALSE;
gboolean g_bLocked = FALSE;

gboolean g_bUseOpenGL = FALSE;
gboolean g_bUseCairo = FALSE;
gboolean g_bIndirectRendering = FALSE;
GdkGLConfig* g_pGlConfig = NULL;
GLuint g_iBackgroundTexture=0;
GLuint g_iIndicatorTexture=0;
GLuint g_iActiveIndicatorTexture=0;
GLuint g_pVisibleZoneTexture=0;
GLuint g_pGradationTexture[2]={0, 0};

static gchar *cLaunchCommand = NULL;

static void _cairo_dock_set_verbosity(gchar *cVerbosity)
{
  if (!cVerbosity)
    cd_log_set_level(G_LOG_LEVEL_WARNING);
  else if (!strcmp(cVerbosity, "debug"))
    cd_log_set_level(G_LOG_LEVEL_DEBUG);
  else if (!strcmp(cVerbosity, "message"))
    cd_log_set_level(G_LOG_LEVEL_MESSAGE);
  else if (!strcmp(cVerbosity, "warning"))
    cd_log_set_level(G_LOG_LEVEL_WARNING);
  else if (!strcmp(cVerbosity, "critical"))
    cd_log_set_level(G_LOG_LEVEL_CRITICAL);
  else if (!strcmp(cVerbosity, "error"))
    cd_log_set_level(G_LOG_LEVEL_ERROR);
  else {
    cd_log_set_level(G_LOG_LEVEL_WARNING);
    cd_warning("bad verbosity option: default to warning");
	}
}

static gboolean _cairo_dock_successful_launch (gpointer data)
{
	cLaunchCommand[strlen (cLaunchCommand)-3] = '\0';  // on enleve le mode maintenance.
	return FALSE;
}
static void _cairo_dock_intercept_signal (int signal)
{
	cd_warning ("Attention : Cairo-Dock has crashed (sig %d).\nIt will be restarted now.\nFeel free to report this bug on cairo-dock.org to help improving the dock !", signal);
	execl ("/bin/sh", "/bin/sh", "-c", cLaunchCommand, NULL);  // on ne revient pas de cette fonction.
	cd_warning ("Sorry, couldn't restart the dock");
}
static void _cairo_dock_set_signal_interception (void)
{
	signal (SIGSEGV, _cairo_dock_intercept_signal);  // Segmentation violation
	signal (SIGFPE, _cairo_dock_intercept_signal);  // Floating-point exception
	signal (SIGILL, _cairo_dock_intercept_signal);  // Illegal instruction
	signal (SIGABRT, _cairo_dock_intercept_signal);  // Abort
}


int main (int argc, char** argv)
{
	int i;
	GString *sCommandString = g_string_new (argv[0]);
	for (i = 1; i < argc; i ++)
	{
		g_string_append_printf (sCommandString, " %s", argv[i]);
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
	gboolean bSafeMode = FALSE, bMaintenance = FALSE, bNoSkipPager = FALSE, bNoSkipTaskbar = FALSE, bNoSticky = FALSE, bToolBarHint = FALSE, bNormalHint = FALSE, bCappuccino = FALSE, bExpresso = FALSE, bCafeLatte = FALSE, bPrintVersion = FALSE, bTesting = FALSE, bForceCairo = FALSE, bForceOpenGL = FALSE;
	gchar *cEnvironment = NULL, *cUserDefinedDataDir = NULL, *cVerbosity = 0, *cUserDefinedModuleDir = NULL;
	GOptionEntry TableDesOptions[] =
	{
		{"log", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cVerbosity,
			"log verbosity (debug,message,warning,critical,error) default is warning", NULL},
		{"glitz", 'g', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bUseGlitz,
			"force Glitz backend (hardware acceleration for cairo, needs a glitz-enabled libcairo)", NULL},
		{"cairo", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bForceCairo,
			"force cairo backend", NULL},
		{"opengl", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bForceOpenGL,
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
			"force the dock to load this directory, instead of ~/.cairo-dock.", NULL},
		{"maintenance", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bMaintenance,
			"allow to edit the config before the dock is started and show the config panel on start", NULL},
		{"safe-mode", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bSafeMode,
			"don't load any plug-ins and show the theme manager on start", NULL},
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
			"adress of a server containing additional themes. This will overwrite the default server adress.", NULL},
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
		g_print ("ERREUR : %s\n", erreur->message);
		exit (-1);
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
	
	_cairo_dock_set_verbosity(cVerbosity);
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
	cd_message ("Compiled with Glitz (hardware acceleration support)n");
#else
	if (g_bUseGlitz)
	{
		cd_warning ("Attention : Cairo-Dock was not compiled with glitz");
		g_bUseGlitz = FALSE;
	}
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

	//\___________________ On teste l'existence du repertoire des donnees .cairo-dock.
	if (cUserDefinedDataDir != NULL)
	{
		g_cCairoDockDataDir = cUserDefinedDataDir;
		cUserDefinedDataDir = NULL;
		if (! g_file_test (g_cCairoDockDataDir, G_FILE_TEST_IS_DIR))
		{
			if (g_mkdir (g_cCairoDockDataDir, 7*8*8+7*8+5) != 0)
				cd_warning ("couldn't create directory %s", g_cCairoDockDataDir);
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
				gchar *cCommand = g_strdup_printf ("mkdir '%s/.config' > /dev/null", getenv("HOME"));
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
					
				cCommand = g_strdup_printf ("mv '%s' '%s'", cOldDataDir, g_cCairoDockDataDir);
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
				
				cCommand = g_strdup_printf ("sed -i \"s/~\\/.cairo-dock/~\\/.config\\/%s/g\" '%s/%s/%s'", CAIRO_DOCK_DATA_DIR, g_cCairoDockDataDir, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_CONF_FILE);
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
				
				cCommand = g_strdup_printf ("sed -i \"/default icon directory/ { s/~\\/.config\\/%s\\/%s\\/icons/%s/g }\" '%s/%s'", CAIRO_DOCK_DATA_DIR, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
			}
			else
			{
				if (g_mkdir (g_cCairoDockDataDir, 7*8*8+7*8+5) != 0)
					cd_warning ("couldn't create directory %s", g_cCairoDockDataDir);
			}
			g_free (cOldDataDir);
		}
	}
	gchar *cThemesDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_THEMES_DIR);
	if (! g_file_test (cThemesDir, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir (cThemesDir, 7*8*8+7*8+5) != 0)
			cd_warning ("couldn't create directory %s", cThemesDir);
	}
	g_free (cThemesDir);
	gchar *cExtrasDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR);
	if (! g_file_test (cExtrasDir, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir (cExtrasDir, 7*8*8+7*8+5) != 0)
			cd_warning ("couldn't create directory %s", cExtrasDir);
	}
	g_free (cExtrasDir);
	g_cCurrentThemePath = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_CURRENT_THEME_NAME);
	if (! g_file_test (g_cCurrentThemePath, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir (g_cCurrentThemePath, 7*8*8+7*8+5) != 0)
			cd_warning ("couldn't create directory %s", g_cCurrentThemePath);
	}
	g_cCurrentLaunchersPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LAUNCHERS_DIR);
	if (! g_file_test (g_cCurrentLaunchersPath, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir (g_cCurrentLaunchersPath, 7*8*8+7*8+5) != 0)
			cd_warning ("couldn't create directory %s", g_cCurrentLaunchersPath);
	}
	gchar *cLocalIconsPath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
	if (! g_file_test (cLocalIconsPath, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir (cLocalIconsPath, 7*8*8+7*8+5) != 0)
			cd_warning ("couldn't create directory %s", cLocalIconsPath);
		else
		{
			cd_warning ("Cairo-Dock's local icons are now located in the 'icons' folder, they will be moved there");
			gchar *cCommand = g_strdup_printf ("cd '%s' && mv *.svg *.png *.xpm *.jpg *.bmp *.gif '%s' > /dev/null", g_cCurrentLaunchersPath, cLocalIconsPath);
			cd_message ("%s", cCommand);
			system (cCommand);
			g_free (cCommand);
			
			cCommand = g_strdup_printf ("sed -i \"s/_ThemeDirectory_/%s/g\" '%s/%s'", CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
			cd_message ("%s", cCommand);
			system (cCommand);
			g_free (cCommand);
			
			cCommand = g_strdup_printf ("sed -i \"/default icon directory/ { s/~\\/.config\\/%s\\/%s\\/icons/%s/g }\" '%s/%s'", CAIRO_DOCK_DATA_DIR, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
			cd_message ("%s", cCommand);
			system (cCommand);
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
	
	//\___________________ On initialise le keybinder
	cd_keybinder_init();
	
	//\___________________ On detecte l'environnement de bureau (apres les applis et avant les modules).
	if (g_iDesktopEnv == CAIRO_DOCK_UNKNOWN_ENV)
		g_iDesktopEnv = cairo_dock_guess_environment ();
	cd_message ("environnement de bureau : %d", g_iDesktopEnv);
	
	//\___________________ On initialise le support d'OpenGL.
	if (! bForceCairo && ! g_bUseGlitz)
		g_pGlConfig = cairo_dock_get_opengl_config (bForceOpenGL);
	g_bUseOpenGL = (g_pGlConfig != NULL);
	
	
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
	
	//\___________________ On initialise la gestion des crash.
	if (! bTesting)
		_cairo_dock_set_signal_interception ();
	
	//\___________________ On charge le dernier theme ou on demande a l'utilisateur d'en choisir un.
	g_cConfFile = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
	
	gboolean config_ok;
	if (bMaintenance)
	{
		cairo_dock_build_main_ihm (g_cConfFile, TRUE);
	}
	
	cd_message ("loading theme ...");
	if (! g_file_test (g_cConfFile, G_FILE_TEST_EXISTS) || bSafeMode)
	{
		if (! g_file_test (g_cConfFile, G_FILE_TEST_EXISTS))
			cairo_dock_mark_theme_as_modified (FALSE);  // le fichier n'existe pas, on ne proposera pas de sauvegarder ce theme.
		do
		{
			cairo_dock_manage_themes (NULL, bSafeMode ? CAIRO_DOCK_START_SAFE : CAIRO_DOCK_START_MAINTENANCE);
		}
		while (g_pMainDock == NULL);
	}
	else
		cairo_dock_load_theme (g_cCurrentThemePath);
	
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

	if (bWriteChangeLog)
	{
		gchar *cChangeLogFilePath = g_strdup_printf ("%s/ChangeLog.txt", CAIRO_DOCK_SHARE_DATA_DIR);
		GKeyFile *pKeyFile = g_key_file_new ();
		g_key_file_load_from_file (pKeyFile, cChangeLogFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		else
		{
			gchar *cKeyName = g_strdup_printf ("%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion);
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
	else
	{
		//\___________________ Message a caractere informatif (ou pas).
		gchar *cSillyMessageFilePath = g_strdup_printf ("%s/.cairo-dock-silly-question", g_cCairoDockDataDir);
		//const gchar *cSillyMessage = "Le saviez-vous ?\nUtiliser cairo-dock vous rendra beau et intelligent !";
		//const gchar *cSillyMessage = "Le saviez-vous ?\nUtiliser cairo-dock augmentera votre popularité auprès de la gente féminine !";
		//const gchar *cSillyMessage = "Le saviez-vous ?\nCairo-Dock contribue à réduire le trou de la couche d'ozone !";
		//const gchar *cSillyMessage = "Montrer Cairo-Dock à un utilisateur de Mac est le meilleur moyen de s'en faire un ennemi;\nN'oubliez pas qu'il a payé 129$ pour avoir la même chose !";  // 7500
		//const gchar *cSillyMessage = "Petite annonce :\n  Projet sérieux recherche secrétaire pour rédiger documentation.\n  Niveau d'étude exigé : 95C.";  // 7500
		//const gchar *cSillyMessage = "Cairo-Dock fait même le café ! Au choix :\n cairo-dock --capuccino , cairo-dock --expresso , cairo-dock --cafe_latte";  // 8000
		//const gchar *cSillyMessage = "Veuillez rentrer un compliment élogieux à la gloire Fab pour pouvoir utiliser cairo-dock.";
		//const gchar *cSillyMessage = "Sondage :\n Combien cairo-dock c'est trop bien :";
		//const gchar *cSillyMessage = "Cairo-Dock : just launch it !";  // 4000
		//const gchar *cSillyMessage = "Cairo-Dock lave plus blanc que blanc.";  // 4000
		//const gchar *cSillyMessage = "Sondage :\nVoulez-vous voir plus de filles nues dans Cairo-Dock ?";
		//const gchar *cSillyMessage = "C'est les soldes !\n Pour tout sous-dock acheté, un sous-dock offert !";
		//const gchar *cSillyMessage = "J-2 avant la 1.5, la tension monte !";
		//const gchar *cSillyMessage = "Cairo-Dock : sans danger si l'on se conforme au mode d'emploi.";
		//const gchar *cSillyMessage = "Nochka, ton home a disparu !";
		//const gchar *cSillyMessage = "La nouvelle sauce Cairo-Dock rehaussera le goût de tous vos plats !";
		//const gchar *cSillyMessage = "Avec Cairo-Dock c'est vous qui avez la plus grosse (barre de lancement) !";
		const gchar *cSillyMessage = "\n   Bonne Année 2009 !!!\n";
		const gchar *cNumSilllyMessage = "20";
		gboolean bWriteSillyMessage;
		if (! g_file_test (cSillyMessageFilePath, G_FILE_TEST_EXISTS))
		{
			bWriteSillyMessage = TRUE;
		}
		else
		{
			gsize length = 0;
			gchar *cContent = NULL;
			g_file_get_contents (cSillyMessageFilePath,
				&cContent,
				&length,
				NULL);
			if (length > 0 && strcmp (cContent, cNumSilllyMessage) == 0)
				bWriteSillyMessage = FALSE;
			else
				bWriteSillyMessage = TRUE;
			g_free (cContent);
		}
	
		g_file_set_contents (cSillyMessageFilePath,
			cNumSilllyMessage,
			-1,
			NULL);
		g_free (cSillyMessageFilePath);
	
		if (bWriteSillyMessage && cSillyMessage != NULL)
		{
			cairo_dock_show_general_message (cSillyMessage, 6000);
			/*double fAnswer = cairo_dock_show_value_and_wait (cSillyMessage, pFirstIcon, g_pMainDock, 1.);
			cd_message (" ==> %.2f\n", fAnswer);
			if (fAnswer == 0)
				cd_message ("Cela sera consigné et utilisé contre vous le moment venu ;-)\n");
			else if (fAnswer == 1)
				cd_message ("je suis aussi d'accord ! ;-)\n");*/
	
			/*int iAnswer = cairo_dock_ask_question_and_wait (cSillyMessage, pFirstIcon, g_pMainDock);
			if (iAnswer == GTK_RESPONSE_YES)
				cd_message ("c'est bien ce que je pensais ;-)\n");
			else
				cd_message ("allez on ne me la fais pas ! ;-)\n");*/
	
			/*gchar *cAnswer = cairo_dock_show_demand_and_wait ("Test :", NULL, g_pMainDock, "pouet");
			cd_message (" -> %s\n", cAnswer);*/
			/*double fAnswer = cairo_dock_show_value_and_wait ("Test :", cairo_dock_get_first_appli (g_pMainDock->icons), g_pMainDock, .7);
			cd_message (" ==> %.2f\n", fAnswer);*/
		}
	}
	
	if (! bTesting)
		g_timeout_add_seconds (5, _cairo_dock_successful_launch, NULL);
	
	gtk_main ();

	rsvg_term ();
	
	cd_message ("Bye bye !");
	g_print ("\033[0m\n");

	return 0;
}

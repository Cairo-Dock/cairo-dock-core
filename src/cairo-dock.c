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
*********************** VERSION 0.1.0 and above (2007-2010)*********************
**
** author(s) :
**     Fabrice Rey <fabounet@glx-dock.org>
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

#include "config.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-menu.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-launcher.h"
#include "cairo-dock-gui-switch.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-dbus.h"
#include "cairo-dock-load.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-gauge.h"
#include "cairo-dock-graph.h"
#include "cairo-dock-default-view.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-dialogs.h"
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-user-interaction.h"
#include "cairo-dock-hiding-effect.h"
#include "cairo-dock-icon-container.h"
#include "cairo-dock-data-renderer.h"

#define CAIRO_DOCK_THEME_SERVER "http://themes.glx-dock.org"
#define CAIRO_DOCK_BACKUP_THEME_SERVER "http://fabounet03.free.fr"
// Nom du repertoire racine du theme courant.
#define CAIRO_DOCK_CURRENT_THEME_NAME "current_theme"
// Nom du repertoire des themes extras.
#define CAIRO_DOCK_EXTRAS_DIR "extras"
// Nom du repertoire des themes de dock.
#define CAIRO_DOCK_THEMES_DIR "themes"

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

static gchar *s_cLaunchCommand = NULL;
static gchar *s_cLastVersion = NULL;
gboolean g_bEnterHelpOnce = FALSE;
static gchar *s_cDefaulBackend = NULL;
static gboolean s_bTestComposite = TRUE;
static gint s_iGuiMode = 0;  // 0 = simple mode, 1 = advanced mode

static void _accept_metacity_composition (int iClickedButton, GtkWidget *pInteractiveWidget, gpointer data, CairoDialog *pDialog)
{
	if (iClickedButton == 1)  // clic explicite sur "cancel"
	{
		int r = system ("gconftool-2 -s '/apps/metacity/general/compositing_manager' --type bool false");
	}
}
static void _toggle_remember_choice (GtkCheckButton *pButton, GtkWidget *pDialog)
{
	g_object_set_data (G_OBJECT (pDialog), "remember", GINT_TO_POINTER (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pButton))));
}
static gboolean _cairo_dock_successful_launch (gpointer data)
{
	if (g_str_has_suffix (s_cLaunchCommand, " -m"))
		s_cLaunchCommand[strlen (s_cLaunchCommand)-3] = '\0';  // on enleve le mode maintenance.
	
	//\___________________ On teste le composite maintenant (au demarrage, le composite manager peut ne pas etre lance).
	if (s_bTestComposite)
	{
		GdkScreen *pScreen = gdk_screen_get_default ();
		if (! mySystem.bUseFakeTransparency && ! gdk_screen_is_composited (pScreen))
		{
			cd_warning ("no composite manager found");
			// Si l'utilisateur utilise Metacity, on lui propose d'activer le composite.
			gchar *cPsef = cairo_dock_launch_command_sync ("pgrep metacity");  // 'ps' ne marche pas, il faut le lancer dans un script :-/
			if (cPsef != NULL && *cPsef != '\0')  // "metacity" a ete trouve.
			{
				Icon *pIcon = cairo_dock_get_dialogless_icon ();
				GtkWidget *pAskBox = gtk_hbox_new (FALSE, 3);
				GtkWidget *label = gtk_label_new (_("Don't ask me any more"));
				GtkWidget *pCheckBox = gtk_check_button_new ();
				gtk_box_pack_end (GTK_BOX (pAskBox), pCheckBox, FALSE, FALSE, 0);
				gtk_box_pack_end (GTK_BOX (pAskBox), label, FALSE, FALSE, 0);
				g_signal_connect (G_OBJECT (pCheckBox), "toggled", G_CALLBACK(_toggle_remember_choice), pAskBox);
				int iClickedButton = cairo_dock_show_dialog_and_wait (_("To remove the black rectangle around the dock, you will need to activate a composite manager.\nFor instance, this can be done by activating desktop effects, launching Compiz, or activating the composition in Metacity.\nI can perform this last operation for you. Do you want to proceed ?"), pIcon, CAIRO_CONTAINER (g_pMainDock), 0., NULL, pAskBox);
				
				gboolean bRememberChoice = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pAskBox), "remember"));
				gtk_widget_destroy (pAskBox);
				if (bRememberChoice)
				{
					s_bTestComposite = FALSE;
				}
				///int iAnswer= cairo_dock_ask_question_and_wait (_("To remove the black rectangle around the dock, you will need to activate a composite manager.\nFor instance, this can be done by activating desktop effects, launching Compiz, or activating the composition in Metacity.\nI can perform this last operation for you. Do you want to proceed ?"), pIcon, CAIRO_CONTAINER (g_pMainDock));
				if (iClickedButton == 0 || iClickedButton == -1)
				{
					int r = system ("gconftool-2 -s '/apps/metacity/general/compositing_manager' --type bool true");
					cairo_dock_show_dialog_with_question (_("Do you want to keep this setting?"), pIcon, CAIRO_CONTAINER (g_pMainDock), NULL, (CairoDockActionOnAnswerFunc) _accept_metacity_composition, NULL, NULL);
				}
				
			}
			else  // sinon il a droit a un "message a caractere informatif".
			{
				cairo_dock_show_general_message (_("To remove the black rectangle around the dock, you will need to activate a composite manager.\nFor instance, this can be done by activating desktop effects, launching Compiz, or activating the composition in Metacity.\nIf your machine can't support composition, Cairo-Dock can emulate it. This option is in the 'System' module of the configuration, at the bottom of the page."), 0);
			}
			g_free (cPsef);
		}
		else  // a priori peu utile de le retester, surtout que le composite risque de ne pas etre actif tout de suite au demarrage.
		{
			s_bTestComposite = FALSE;
		}
		
		if (! s_bTestComposite)
		{
			gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
			cairo_dock_update_conf_file (cConfFilePath,
				G_TYPE_BOOLEAN, "Launch", "test composite", s_bTestComposite,
				G_TYPE_INVALID);
			g_free (cConfFilePath);
		}
	}
	return FALSE;
}
static void _cairo_dock_intercept_signal (int signal)
{
	cd_warning ("Cairo-Dock has crashed (sig %d).\nIt will be restarted now (%s).\nFeel free to report this bug on glx-dock.org to help improving the dock !", signal, s_cLaunchCommand);
	g_print ("info on the system :\n");
	int r = system ("uname -a");
	if (g_pCurrentModule != NULL)
	{
		g_print ("The applet '%s' may be the culprit", g_pCurrentModule->pModule->pVisitCard->cModuleName);
		s_cLaunchCommand = g_strdup_printf ("%s -x \"%s\"", s_cLaunchCommand, g_pCurrentModule->pModule->pVisitCard->cModuleName);
	}
	else
	{
		g_print ("Couldn't guess if it was an applet's fault or not. It may have crashed inside the core or inside a thread\n");
	}
	execl ("/bin/sh", "/bin/sh", "-c", s_cLaunchCommand, (char *)NULL);  // on ne revient pas de cette fonction.
	//execlp ("cairo-dock", "cairo-dock", s_cLaunchCommand, (char *)0);
	cd_warning ("Sorry, couldn't restart the dock");
}
static void _cairo_dock_set_signal_interception (void)
{
	signal (SIGSEGV, _cairo_dock_intercept_signal);  // Segmentation violation
	signal (SIGFPE, _cairo_dock_intercept_signal);  // Floating-point exception
	signal (SIGILL, _cairo_dock_intercept_signal);  // Illegal instruction
	signal (SIGABRT, _cairo_dock_intercept_signal);  // Abort
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

static void _entered_help_once (CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile)
{
	if (!g_bEnterHelpOnce)
	{
		g_bEnterHelpOnce = TRUE;
		gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_BOOLEAN, "Help", "entered once", g_bEnterHelpOnce,
			G_TYPE_INVALID);
		g_free (cConfFilePath);
	}
}
static void _register_help_module (void)
{
	//\________________ ceci est un vilain hack ... mais je trouvais ca lourd de compiler un truc qui n'a aucun code, et puis comme ca on a l'aide meme sans les plug-ins.
	CairoDockModule *pHelpModule = g_new0 (CairoDockModule, 1);
	CairoDockVisitCard *pVisitCard = g_new0 (CairoDockVisitCard, 1);
	pHelpModule->pVisitCard = pVisitCard;
	pVisitCard->cModuleName = "Help";
	pVisitCard->cTitle = _("Help");
	pVisitCard->iMajorVersionNeeded = 2;
	pVisitCard->iMinorVersionNeeded = 0;
	pVisitCard->iMicroVersionNeeded = 0;
	pVisitCard->cPreviewFilePath = NULL;
	pVisitCard->cGettextDomain = NULL;
	pVisitCard->cDockVersionOnCompilation = CAIRO_DOCK_VERSION;
	pVisitCard->cUserDataDir = "help";
	pVisitCard->cShareDataDir = CAIRO_DOCK_SHARE_DATA_DIR;
	pVisitCard->cConfFileName = "help.conf";
	pVisitCard->cModuleVersion = "0.1.1";
	pVisitCard->iCategory = CAIRO_DOCK_CATEGORY_BEHAVIOR;
	pVisitCard->cIconFilePath = CAIRO_DOCK_SHARE_DATA_DIR"/icon-help.svg";
	pVisitCard->iSizeOfConfig = 0;
	pVisitCard->iSizeOfData = 0;
	pVisitCard->cDescription = N_("A useful FAQ which also contains a lot of hints.\nRoll your mouse over a sentence to make helpful popups appear.");
	pVisitCard->cAuthor = "Fabounet";
	pHelpModule->pInterface = g_new0 (CairoDockModuleInterface, 1);
	pHelpModule->pInterface->load_custom_widget = _entered_help_once;
	cairo_dock_register_module (pHelpModule);
	cairo_dock_activate_module (pHelpModule, NULL);
	pHelpModule->fLastLoadingTime = time (NULL) + 1e7;  // pour ne pas qu'il soit desactive lors d'un reload general, car il n'est pas dans la liste des modules actifs du fichier de conf.
}

static void _cairo_dock_get_global_config (void)
{
	gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
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
		s_bTestComposite = g_key_file_get_boolean (pKeyFile, "Launch", "test composite", NULL);
		g_bEnterHelpOnce = g_key_file_get_boolean (pKeyFile, "Help", "entered once", NULL);
		s_iGuiMode = g_key_file_get_integer (pKeyFile, "Gui", "mode", NULL);  // 0 si la cle n'est pas presente.
	}
	else  // ancienne methode.
	{
		gchar *cLastVersionFilePath = g_strdup_printf ("%s/.cairo-dock-last-version", g_cCairoDockDataDir);
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
		g_key_file_set_boolean (pKeyFile, "Launch", "test composite", TRUE);
		
		gchar *cHelpHistory = g_strdup_printf ("%s/.help/entered-once", g_cCairoDockDataDir);
		if (g_file_test (cHelpHistory, G_FILE_TEST_EXISTS))
		{
			g_bEnterHelpOnce = TRUE;
		}
		gchar *cCommand = g_strdup_printf ("rm -rf \"%s/.help\"", g_cCairoDockDataDir);
		int r = system (cCommand);
		g_free (cCommand);
		g_free (cHelpHistory);
		g_key_file_set_boolean (pKeyFile, "Help", "entered once", g_bEnterHelpOnce);
		
		s_iGuiMode = 0;
		g_key_file_set_integer (pKeyFile, "Gui", "mode", s_iGuiMode);
		
		cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	}
	g_key_file_free (pKeyFile);
	g_free (cConfFilePath);
}

int main (int argc, char** argv)
{
	int i, iNbMaintenance=0;
	GString *sCommandString = g_string_new (argv[0]);
	gchar *cDisableApplet = NULL;
	for (i = 1; i < argc; i ++)
	{
		//g_print ("'%s'\n", argv[i]);
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
	s_cLaunchCommand = sCommandString->str;
	g_string_free (sCommandString, FALSE);
	
	cd_log_init(FALSE);  // no log by default.
	
	gtk_init (&argc, &argv);
	
	GError *erreur = NULL;
	
	//\___________________ On recupere quelques options.
	gboolean bSafeMode = FALSE, bMaintenance = FALSE, bNoSticky = FALSE, bNormalHint = FALSE, bCappuccino = FALSE, bPrintVersion = FALSE, bTesting = FALSE, bForceIndirectRendering = FALSE, bForceOpenGL = FALSE, bToggleIndirectRendering = FALSE, bKeepAbove = FALSE;
	gchar *cEnvironment = NULL, *cUserDefinedDataDir = NULL, *cVerbosity = 0, *cUserDefinedModuleDir = NULL, *cExcludeModule = NULL, *cThemeServerAdress = NULL;
	GOptionEntry TableDesOptions[] =
	{
		{"log", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cVerbosity,
			"log verbosity (debug,message,warning,critical,error); default is warning", NULL},
#ifdef HAVE_GLITZ
		{"glitz", 'g', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bUseGlitz,
			"force Glitz backend (hardware acceleration for cairo, needs a glitz-enabled libcairo)", NULL},
#endif
		{"cairo", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bForceCairo,
			"use Cairo backend", NULL},
		{"opengl", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bForceOpenGL,
			"use OpenGL backend", NULL},
		{"indirect-opengl", 'O', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bToggleIndirectRendering,
			"use OpenGL backend with indirect rendering. There are very few case where this option should be used.", NULL},
		{"indirect", 'i', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bForceIndirectRendering,
			"deprecated - see -O", NULL},
		{"keep-above", 'a', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bKeepAbove,
			"keep the dock above other windows whatever", NULL},
		{"no-sticky", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&bNoSticky,
			"don't make the dock appear on all desktops", NULL},
		{"env", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cEnvironment,
			"force the dock to consider this environnement - use it with care.", NULL},
		{"dir", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&cUserDefinedDataDir,
			"force the dock to load from this directory, instead of ~/.config/cairo-dock.", NULL},
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
			&cThemeServerAdress,
			"address of a server containing additional themes. This will overwrite the default server address.", NULL},
		{"locked", 'k', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&g_bLocked,
			"lock the dock so that any modification is impossible for users.", NULL},
		{NULL, 0, 0, 0,
			NULL,
			NULL, NULL}
	};

	GOptionContext *context = g_option_context_new ("Cairo-Dock");
	g_option_context_add_main_entries (context, TableDesOptions, NULL);
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
	cd_message ("Compiled with Glitz (hardware acceleration support for cairo)n");
#endif
	
	if (bCappuccino)
	{
		g_print ("Cairo-Dock does anything, including coffee !.\n");
		return 0;
	}
	
	//\___________________ On internationalise l'appli.
	bindtextdomain (CAIRO_DOCK_GETTEXT_PACKAGE, CAIRO_DOCK_LOCALE_DIR);
	bind_textdomain_codeset (CAIRO_DOCK_GETTEXT_PACKAGE, "UTF-8");
	textdomain (CAIRO_DOCK_GETTEXT_PACKAGE);

	  /////////////
	 //// LIB ////
	/////////////
	
	//\___________________ On definit les repertoires des donnees.
	gchar *cRootDataDirPath;
	if (cUserDefinedDataDir != NULL)
	{
		cRootDataDirPath = cUserDefinedDataDir;
		cUserDefinedDataDir = NULL;
	}
	else
	{
		cRootDataDirPath = g_strdup_printf ("%s/.config/%s", getenv("HOME"), CAIRO_DOCK_DATA_DIR);
	}
	gchar *cExtraDirPath = g_strconcat (cRootDataDirPath, "/"CAIRO_DOCK_EXTRAS_DIR, NULL);
	gchar *cThemesDirPath = g_strconcat (cRootDataDirPath, "/"CAIRO_DOCK_THEMES_DIR, NULL);
	gchar *cCurrentThemeDirPath = g_strconcat (cRootDataDirPath, "/"CAIRO_DOCK_CURRENT_THEME_NAME, NULL);
	cairo_dock_set_paths (cRootDataDirPath, cExtraDirPath, cThemesDirPath, cCurrentThemeDirPath, cThemeServerAdress ? cThemeServerAdress : g_strdup (CAIRO_DOCK_THEME_SERVER));
	
	//\___________________ On initialise le gestionnaire de docks (a faire en 1er).
	cairo_dock_init_dock_manager ();
	
	//\___________________ On initialise le gestionnaire de desklets.
	cairo_dock_init_desklet_manager ();
	
	//\___________________ On initialise le gestionnaire de dialogues.
	cairo_dock_init_dialog_manager ();
	
	//\___________________ On initialise le gestionnaire de backends (vues, etc).
	cairo_dock_init_backends_manager ();
	
	//\___________________ On initialise le gestionnaire des indicateurs.
	cairo_dock_init_indicator_manager ();
	
	//\___________________ On initialise le multi-threading.
	if (!g_thread_supported ())
		g_thread_init (NULL);
	
	//\___________________ On demarre le support de X.
	cairo_dock_start_X_manager ();
	
	//\___________________ On initialise le keybinder.
	cd_keybinder_init();
	
	//\___________________ On detecte l'environnement de bureau (apres X et avant les modules).
	cairo_dock_init_desktop_environment_manager (iDesktopEnv);
	
	//\___________________ On enregistre les implementations.
	cairo_dock_register_built_in_data_renderers ();
	cairo_dock_register_hiding_effects ();
	
	cairo_dock_register_default_renderer ();
	
	cairo_dock_register_icon_container_renderers ();
	
	//\___________________ On enregistre les notifications de base (avant les modules).
	cairo_dock_register_notification (CAIRO_DOCK_RENDER_ICON,
		(CairoDockNotificationFunc) cairo_dock_render_icon_notification,
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
	
	  /////////////
	 //// APP ////
	/////////////
	
	//\___________________ On lit la config globale de l'appli.
	_cairo_dock_get_global_config ();
	
	//\___________________ Options generales.
	if (bKeepAbove)
		cairo_dock_force_docks_above ();
	
	if (bNoSticky)
		cairo_dock_set_containers_non_sticky ();
	
	//\___________________ On initialise le support d'OpenGL.
	gboolean bOpenGLok = FALSE;
	if (bForceOpenGL || (! g_bForceCairo && ! g_bUseGlitz))
		bOpenGLok = cairo_dock_initialize_opengl_backend (bToggleIndirectRendering, bForceOpenGL);
	if (bOpenGLok && ! bForceOpenGL && ! bToggleIndirectRendering && ! cairo_dock_opengl_is_safe ())  // opengl disponible sans l'avoir force mais pas safe => on demande confirmation.
	{
		if (s_cDefaulBackend == NULL) // pas de backend par defaut defini.
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
			g_signal_connect (G_OBJECT (pCheckBox), "toggled", G_CALLBACK(_toggle_remember_choice), dialog);
			
			gtk_widget_show_all (dialog);
			
			gint iAnswer = gtk_dialog_run (GTK_DIALOG (dialog));  // lance sa propre main loop, c'est pourquoi on peut le faire avant le gtk_main().
			gboolean bRememberChoice = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "remember"));
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
	
	//\___________________ On initialise le gestionnaire de modules et on pre-charge les modules existant (il faut le faire apres savoir si on utilise l'OpenGL).
	if (g_module_supported () && ! bSafeMode)
	{
		cairo_dock_initialize_module_manager (cairo_dock_get_modules_dir ());
		
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
	cairo_dock_load_user_gui_backend (s_iGuiMode);
	cairo_dock_register_default_launcher_gui_backend ();
	
	//\___________________ On enregistre nos notifications.
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
	cairo_dock_register_notification (CAIRO_DOCK_BUILD_CONTAINER_MENU,
		(CairoDockNotificationFunc) cairo_dock_notification_build_container_menu,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_BUILD_ICON_MENU,
		(CairoDockNotificationFunc) cairo_dock_notification_build_icon_menu,
		CAIRO_DOCK_RUN_AFTER, NULL);
	
	//\___________________ On initialise la gestion des crash.
	if (! bTesting)
		_cairo_dock_set_signal_interception ();
	
	//\___________________ mode maintenance.
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
	
	if (cExcludeModule != NULL)
	{
		//g_print ("on enleve %s de '%s'\n", cExcludeModule, s_cLaunchCommand);
		gchar *str = g_strstr_len (s_cLaunchCommand, -1, " -x ");
		if (str)
		{
			*str = '\0';  // enleve le module de la ligne de commande, ainsi que le le -m courant.
			g_print ("s_cLaunchCommand <- '%s'\n", s_cLaunchCommand);
		}
		else
		{
			g_free (cExcludeModule);
			cExcludeModule = NULL;
		}
	}
	
	//\___________________ On charge le dernier theme.
	cd_message ("loading theme ...");
	if (! g_file_test (g_cConfFile, G_FILE_TEST_EXISTS))  // aucun theme, on copie le theme par defaut.
	{
		gchar *cCommand = g_strdup_printf ("/bin/cp -r \"%s\"/* \"%s\"", CAIRO_DOCK_SHARE_DATA_DIR"/themes/_default_", g_cCurrentThemePath);
		cd_message (cCommand);
		int r = system (cCommand);
		g_free (cCommand);
		
		cairo_dock_mark_current_theme_as_modified (FALSE);  // on ne proposera pas de sauvegarder ce theme.
	}
	cairo_dock_load_current_theme ();
	
	if (g_bLocked)
	{
		myAccessibility.bLockIcons = TRUE;
		myAccessibility.bLockAll = TRUE;
	}
	
	if (!bSafeMode && cairo_dock_get_nb_modules () == 0)
	{
		Icon *pIcon = cairo_dock_get_dialogless_icon ();
		cairo_dock_ask_question_and_wait (("No plug-in were found.\nPlug-ins provide most of the functionnalities of Cairo-Dock (animations, applets, views, etc).\nSee http://glx-dock.org for more information.\nSince there is almost no meaning in running the dock without them, the application will quit now."), pIcon, CAIRO_CONTAINER (g_pMainDock));
		exit (0);
	}
	
	_register_help_module ();
	
	
	//\___________________ On affiche un petit message de bienvenue ou de changelog ou d'erreur.
	gboolean bNewVersion = (s_cLastVersion == NULL || strcmp (s_cLastVersion, CAIRO_DOCK_VERSION) != 0);
	if (bNewVersion)
	{
		gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_STRING, "Launch", "last version", CAIRO_DOCK_VERSION,
			G_TYPE_INVALID);
		g_free (cConfFilePath);
	}
	
	gboolean bFirstLaunch = ! g_file_test (cRootDataDirPath, G_FILE_TEST_IS_DIR);
	if (bFirstLaunch)  // tout premier lancement -> bienvenue !
	{
		cairo_dock_show_general_message (_("Welcome in Cairo-Dock2 !\nA default and simple theme has been loaded.\nYou can either familiarize yourself with the dock or choose another theme with right-click -> Cairo-Dock -> Manage themes.\nA useful help is available by right-click -> Cairo-Dock -> Help.\nIf you have any question/request/remark, please pay us a visit at http://glx-dock.org.\nHope you will enjoy this soft !\n  (you can now click on this dialog to close it)"), 0);
	}
	else if (bNewVersion)  // nouvelle version -> changelog (si c'est le 1er lancement, inutile de dire ce qui est nouveau, et de plus on a deja le message de bienvenue).
	{
		gchar *cChangeLogFilePath = g_strdup_printf ("%s/ChangeLog.txt", CAIRO_DOCK_SHARE_DATA_DIR);
		GKeyFile *pKeyFile = cairo_dock_open_key_file (cChangeLogFilePath);
		g_key_file_load_from_file (pKeyFile, cChangeLogFilePath, 0, &erreur);  // pas de commentaire utile.
		if (pKeyFile != NULL)
		{
			gchar *cKeyName = g_strdup_printf ("%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion);  // version sans les "alpha", "beta", "rc", etc.
			gchar *cChangeLogMessage = g_key_file_get_string (pKeyFile, "ChangeLog", cKeyName, NULL);
			g_free (cKeyName);
			if (cChangeLogMessage != NULL)
			{
				Icon *pFirstIcon = cairo_dock_get_first_icon (g_pMainDock->icons);
				myDialogs.dialogTextDescription.bUseMarkup = TRUE;
				cairo_dock_show_temporary_dialog_with_default_icon (gettext (cChangeLogMessage), pFirstIcon, CAIRO_CONTAINER (g_pMainDock), 0);
				myDialogs.dialogTextDescription.bUseMarkup = FALSE;
				g_free (cChangeLogMessage);
			}
		}
	}
	else if (cExcludeModule != NULL && ! bMaintenance)
	{
		gchar *cMessage = g_strdup_printf (_("The module '%s' may have encountered a problem.\nIt has been restored successfully, but if it happens again, please report it at http://glx-dock.org"), cExcludeModule);
		
		CairoDockModule *pModule = cairo_dock_find_module_from_name (cExcludeModule);
		Icon *icon = cairo_dock_get_dialogless_icon ();
		cairo_dock_show_temporary_dialog_with_icon (cMessage, icon, CAIRO_CONTAINER (g_pMainDock), 15000., (pModule ? pModule->pVisitCard->cIconFilePath : NULL));
		g_free (cMessage);
	}
	
	if (! bTesting)
		g_timeout_add_seconds (5, _cairo_dock_successful_launch, NULL);
	
	gtk_main ();
	
	signal (SIGSEGV, NULL);  // Segmentation violation
	signal (SIGFPE, NULL);  // Floating-point exception
	signal (SIGILL, NULL);  // Illegal instruction
	signal (SIGABRT, NULL);
	cairo_dock_free_all ();
	
	rsvg_term ();
	xmlCleanupParser ();
	
	cd_message ("Bye bye !");
	g_print ("\033[0m\n");

	return 0;
}

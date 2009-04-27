/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gdk/gdkx.h>

#include "cairo-dock-config.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-load.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-menu.h"

#define CAIRO_DOCK_CONF_PANEL_WIDTH 800
#define CAIRO_DOCK_CONF_PANEL_HEIGHT 600
#define CAIRO_DOCK_LAUNCHER_PANEL_WIDTH 600
#define CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT 350
#define CAIRO_DOCK_FILE_HOST_URL "https://developer.berlios.de/project/showfiles.php?group_id=8724"
#define CAIRO_DOCK_HELP_URL "http://www.cairo-dock.org"

extern struct tm *localtime_r (time_t *timer, struct tm *tp);

extern CairoDock *g_pMainDock;

extern gchar *g_cConfFile;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cCurrentThemePath;

extern int g_iNbDesktops;
extern int g_iNbViewportX,g_iNbViewportY ;
extern int g_iScreenWidth[2], g_iScreenHeight[2];
extern gboolean g_bLocked;
extern gboolean g_bEasterEggs;
extern gboolean g_bForceCairo;


static void _cairo_dock_edit_and_reload_conf (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];

	GtkWidget *pWindow = cairo_dock_build_main_ihm (g_cConfFile, FALSE);
}

static void _cairo_dock_initiate_theme_management (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];

	gboolean bRefreshGUI;
	do
	{
		bRefreshGUI = cairo_dock_manage_themes (pDock->pWidget, 0);
	} while (bRefreshGUI);
}


static void _cairo_dock_add_about_page (GtkWidget *pNoteBook, const gchar *cPageLabel, const gchar *cAboutText)
{
	GtkWidget *pVBox, *pScrolledWindow;
	GtkWidget *pPageLabel, *pAboutLabel;
	
	pPageLabel = gtk_label_new (cPageLabel);
	pVBox = gtk_vbox_new (FALSE, 0);
	pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pVBox);
	gtk_notebook_append_page (GTK_NOTEBOOK (pNoteBook), pScrolledWindow, pPageLabel);
	
	pAboutLabel = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (pAboutLabel), TRUE);
	gtk_box_pack_start (GTK_BOX (pVBox),
		pAboutLabel,
		FALSE,
		FALSE,
		0);
	gtk_label_set_markup (GTK_LABEL (pAboutLabel), cAboutText);
}
static void _cairo_dock_lock_icons (GtkMenuItem *pMenuItem, gpointer *data)
{
	myAccessibility.bLockIcons = ! myAccessibility.bLockIcons;
	cairo_dock_update_conf_file (g_cConfFile,
		G_TYPE_BOOLEAN, "Accessibility", "lock icons", myAccessibility.bLockIcons,
		G_TYPE_INVALID);
}
static void _cairo_dock_about (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	const gchar *cTitle = "\nCairo-Dock (2007-2009)\n version "CAIRO_DOCK_VERSION;
	GtkWidget *pDialog = gtk_message_dialog_new (GTK_WINDOW (pDock->pWidget),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_CLOSE,
		cTitle);
	
	gchar *cImagePath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_LOGO);
	GtkWidget *pImage = gtk_image_new_from_file (cImagePath);
	g_free (cImagePath);
#if GTK_MINOR_VERSION >= 12
	gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (pDialog), pImage);
#endif
	GtkWidget *pNoteBook = gtk_notebook_new ();
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (pNoteBook), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (pNoteBook));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(pDialog)->vbox), pNoteBook);
	
	_cairo_dock_add_about_page (pNoteBook,
		_("Development"),
		"<b>Main developer :</b>\n  Fabounet (Fabrice Rey)\n\
<b>Original idea/first development :</b>\n  Mac Slow\n\
<b>Applets :</b>\n  Fabounet\n  Necropotame\n  Ctaf\n  ChAnGFu\n  Tofe\n  Paradoxxx_Zero\n\
<b>Patchs :</b>\n  Special thanks to Augur for his great help with OpenGL\n  Ctaf\n  M.Tasaka\n  Necropotame\n  Robrob\n  Smidgey\n  Tshirtman\n");
	_cairo_dock_add_about_page (pNoteBook,
		_("Artwork"),
		"<b>Themes :</b>\n  Fabounet\n  Chilperik\n  Djoole\n  Glattering\n  Vilraleur\n  Lord Northam\n  Paradoxxx_Zero\n  Coz\n  Benoit2600\n  Nochka85\n\
<b>Translations :</b>\n  Fabounet\n  Ppmt \n  Jiro Kawada (Kawaji)\n  BiAji\n  Mattia Tavernini (Maathias)\n  Peter Thornqvist\n  Yannis Kaskamanidis");
	_cairo_dock_add_about_page (pNoteBook,
		_("Support"),
		"<b>Installation script and web hosting :</b>\n  Mav\n\
<b>Site (cairo-dock.org) :</b>\n  Necropotame\n  Tdey\n\
<b>Suggestions/Comments/Bêta-Testers :</b>\n  AuraHxC\n  Chilperik\n  Cybergoll\n  Damster\n  Djoole\n  Glattering\n  Mav\n  Necropotame\n  Nochka85\n  Ppmt\n  RavanH\n  Rhinopierroce\n  Sombrero\n  Vilraleur");
	
	cairo_dock_config_panel_created ();
	gtk_widget_show_all (pDialog);
	gtk_window_set_position (GTK_WINDOW (pDialog), GTK_WIN_POS_CENTER_ALWAYS);  // un GTK_WIN_POS_CENTER simple ne marche pas, probablement parceque la fenetre n'est pas encore realisee. le 'always' ne pose pas de probleme, puisqu'on ne peut pas redimensionner le dialogue.
	gtk_window_set_keep_above (GTK_WINDOW (pDialog), TRUE);
	gtk_dialog_run (GTK_DIALOG (pDialog));
	gtk_widget_destroy (pDialog);
	cairo_dock_config_panel_destroyed ();
}

static void _launch_url (const gchar *cURL)
{
	if  (! cairo_dock_fm_launch_uri (cURL))
	{
		gchar *cCommand = g_strdup_printf ("\
which xdg-open > /dev/null && xdg-open %s || \
which firefox > /dev/null && firefox %s || \
which konqueror > /dev/null && konqueror %s || \
which opera > /dev/null && opera %s ",
			cURL,
			cURL,
			cURL,
			cURL);  // pas super beau mais efficace ^_^
		system (cCommand);
		g_free (cCommand);
	}
}

static void _cairo_dock_check_for_updates (GtkMenuItem *pMenuItem, gpointer *data)
{
	//g_print ("launching %s...\n", CAIRO_DOCK_FILE_HOST_URL);
	_launch_url (CAIRO_DOCK_FILE_HOST_URL);
	//system ("xterm -e cairo-dock-update.sh &");
}

static void _cairo_dock_help (GtkMenuItem *pMenuItem, gpointer *data)
{
	//g_print ("launching %s...\n", CAIRO_DOCK_HELP_URL);
	_launch_url (CAIRO_DOCK_HELP_URL);
}

static void _cairo_dock_present_help (GtkMenuItem *pMenuItem, gpointer *data)
{
	CairoDockModule *pModule = cairo_dock_find_module_from_name ("Help");
	//g_print ("%x\n", pModule);
	g_return_if_fail (pModule != NULL);
	cairo_dock_build_main_ihm (g_cConfFile, FALSE);
	cairo_dock_present_module_gui (pModule);
}

static void _cairo_dock_quick_hide (GtkMenuItem *pMenuItem, gpointer *data)
{
	CairoDock *pDock = data[1];
	//g_print ("%s ()\n", __func__);
	pDock->bMenuVisible = FALSE;
	cairo_dock_quick_hide_all_docks ();
}

static void _cairo_dock_add_autostart (GtkMenuItem *pMenuItem, gpointer *data)
{
	gchar *cCommand = g_strdup_printf ("cp '/usr/share/applications/cairo-dock%s.desktop' '%s/.config/autostart'", (g_bForceCairo ? "-cairo" : ""), g_getenv ("HOME"));
	system (cCommand);
	g_free (cCommand);
}

static void _cairo_dock_quit (GtkMenuItem *pMenuItem, gpointer *data)
{
	CairoDock *pDock = data[1];
	cairo_dock_on_delete (pDock->pWidget, NULL, pDock);
}


static void _cairo_dock_on_user_remove_icon (Icon *icon, CairoDock *pDock)
{
	cd_debug ("%s (%s)", __func__, icon->acName);
	
	if (icon->pSubDock != NULL)
	{
		gboolean bDestroyIcons = ! CAIRO_DOCK_IS_APPLI (icon);
		if (! CAIRO_DOCK_IS_URI_LAUNCHER (icon) && ! CAIRO_DOCK_IS_APPLI (icon) && icon->pSubDock->icons != NULL)  // alors on propose de repartir les icones de son sous-dock dans le dock principal.
		{
			int answer = cairo_dock_ask_question_and_wait (_("Do you want to re-dispatch the icons contained inside this container into the dock ?\n (otherwise they will be destroyed)"), icon, CAIRO_CONTAINER (pDock));
			g_return_if_fail (answer != GTK_RESPONSE_NONE);
			if (answer == GTK_RESPONSE_YES)
				bDestroyIcons = FALSE;
		}
		cairo_dock_destroy_dock (icon->pSubDock, (CAIRO_DOCK_IS_APPLI (icon) && icon->cClass != NULL ? icon->cClass : icon->acName), (bDestroyIcons ? NULL : g_pMainDock), (bDestroyIcons ? NULL : CAIRO_DOCK_MAIN_DOCK_NAME));
		icon->pSubDock = NULL;
	}
	
	cairo_dock_stop_icon_animation (icon);
	icon->fPersonnalScale = 1.0;
	cairo_dock_notify (CAIRO_DOCK_REMOVE_ICON, icon, pDock);
	cairo_dock_start_icon_animation (icon, pDock);
	
	cairo_dock_mark_theme_as_modified (TRUE);
}
static void _cairo_dock_remove_launcher (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];

	gchar *question = g_strdup_printf (_("You're about removing this icon (%s) from the dock. Sure ?"), (icon->cInitialName != NULL ? icon->cInitialName : icon->acName));
	int answer = cairo_dock_ask_question_and_wait (question, icon, CAIRO_CONTAINER (pDock));
	g_free (question);
	if (answer == GTK_RESPONSE_YES)
	{
		_cairo_dock_on_user_remove_icon (icon, pDock);
	}
	//else
	//	g_print ("ok on la garde\n");
}

static void _cairo_dock_create_launcher (GtkMenuItem *pMenuItem, Icon *icon, CairoDock *pDock, CairoDockNewLauncherType iLauncherType)
{
	//\___________________ On determine l'ordre d'insertion suivant l'endroit du clique.
	GError *erreur = NULL;
	double fOrder;
	if (CAIRO_DOCK_IS_LAUNCHER (icon))
	{
		if (pDock->iMouseX < icon->fDrawX + icon->fWidth * icon->fScale / 2)  // a gauche.
		{
			Icon *prev_icon = cairo_dock_get_previous_icon (pDock->icons, icon);
			fOrder = (prev_icon != NULL ? (icon->fOrder + prev_icon->fOrder) / 2 : icon->fOrder - 1);
		}
		else
		{
			Icon *next_icon = cairo_dock_get_next_icon (pDock->icons, icon);
			fOrder = (next_icon != NULL ? (icon->fOrder + next_icon->fOrder) / 2 : icon->fOrder + 1);
		}
	}
	else
		fOrder = CAIRO_DOCK_LAST_ORDER;

	//\___________________ On cree un fichier de lanceur avec des valeurs par defaut.
	const gchar *cDockName = cairo_dock_search_dock_name (pDock);
	gchar *cNewDesktopFileName;
	if (iLauncherType == 1)  // conteneur.
		cNewDesktopFileName = cairo_dock_add_desktop_file_for_container (cDockName, fOrder, pDock, &erreur);
	else if (iLauncherType == 0)  // lanceur vide.
		cNewDesktopFileName = cairo_dock_add_desktop_file_from_uri (NULL, cDockName, fOrder, pDock, &erreur);
	else if (iLauncherType == 2)  // separateur.
		cNewDesktopFileName = cairo_dock_add_desktop_file_for_separator (cDockName, fOrder, pDock, &erreur);
	else
		return ;
	if (erreur != NULL)
	{
		cd_message ("Attention : while trying to create a new launcher : %s", erreur->message);
		g_error_free (erreur);
		return ;
	}

	//\___________________ On ouvre automatiquement l'IHM pour permettre de modifier ses champs.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_update_launcher_desktop_file (cNewDesktopFilePath, iLauncherType);

	gboolean config_ok;
	if (iLauncherType != CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR)  // inutile pour un separateur.
		config_ok = cairo_dock_build_normal_gui (cNewDesktopFilePath, NULL, _("Fill this launcher"), CAIRO_DOCK_LAUNCHER_PANEL_WIDTH, CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT, NULL, NULL, NULL);
		//config_ok = cairo_dock_edit_conf_file (GTK_WINDOW (pDock->pWidget), cNewDesktopFilePath, _("Fill this launcher"), CAIRO_DOCK_LAUNCHER_PANEL_WIDTH, CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT, 0, NULL, NULL, NULL, NULL, CAIRO_DOCK_GETTEXT_PACKAGE);
	else
		config_ok = TRUE;
	if (config_ok)
	{
		cairo_t* pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
		Icon *pNewIcon = cairo_dock_create_icon_from_desktop_file (cNewDesktopFileName, pCairoContext);

		if (iLauncherType = CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR)
			pNewIcon->iType = icon->iType;
		else if (pNewIcon->acName == NULL)
			pNewIcon->acName = g_strdup (_("Undefined"));

		CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
		cairo_dock_insert_icon_in_dock (pNewIcon, pParentDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);

		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
		cairo_dock_mark_theme_as_modified (TRUE);
	}
	else
	{
		g_remove (cNewDesktopFilePath);
	}

	g_free (cNewDesktopFilePath);
	g_free (cNewDesktopFileName);
}

static void cairo_dock_add_launcher (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	
	_cairo_dock_create_launcher (pMenuItem, icon, pDock, CAIRO_DOCK_LAUNCHER_FROM_DESKTOP_FILE);
}

static void cairo_dock_add_container (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];

	_cairo_dock_create_launcher (pMenuItem, icon, pDock, CAIRO_DOCK_LAUNCHER_FOR_CONTAINER);
}

static void cairo_dock_add_separator (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon != NULL)
		_cairo_dock_create_launcher (pMenuItem, icon, pDock, CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR);
}

static void _on_modify_launcher (Icon *icon)
{
	//g_print ("%s (%s)\n", __func__, icon->acName);
	GError *erreur = NULL;
	//\_____________ On detache l'icone.
	gchar *cPrevDockName = icon->cParentDockName;
	CairoDock *pDock = cairo_dock_search_dock_from_name (cPrevDockName);
	icon->cParentDockName = NULL;  // astuce.
	cairo_dock_detach_icon_from_dock (icon, pDock, TRUE);  // il va falloir la recreer, car tous ses parametres peuvent avoir change; neanmoins, on ne souhaite pas detruire son .desktop.

	//\_____________ On recharge l'icone.
	Window Xid = icon->Xid;
	CairoDock *pSubDock = icon->pSubDock;
	icon->pSubDock = NULL;
	gchar *cClass = icon->cClass;
	icon->cClass = NULL;
	gchar *cDesktopFileName = icon->acDesktopFileName;
	icon->acDesktopFileName = NULL;
	gchar *cName = icon->acName;
	icon->acName = NULL;
	gchar *cRendererName = NULL;
	if (pSubDock != NULL)
	{
		cRendererName = pSubDock->cRendererName;
		pSubDock->cRendererName = NULL;
	}
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	cairo_dock_reload_icon_from_desktop_file (cDesktopFileName, pCairoContext, icon);

	icon->Xid = Xid;
	//\_____________ On gere le sous-dock.
	if (Xid != 0)
	{
		if (icon->pSubDock == NULL)
			icon->pSubDock = pSubDock;
		else  // ne devrait pas arriver (une icone de container n'est pas un lanceur pouvant prendre un Xid).
			cairo_dock_destroy_dock (pSubDock, cName, g_pMainDock, CAIRO_DOCK_MAIN_DOCK_NAME);
	}
	else
	{
		if (pSubDock != icon->pSubDock)  // ca n'est plus le meme container, on transvase ou on detruit.
		{
			cairo_dock_destroy_dock (pSubDock, cName, icon->pSubDock, icon->acName);
		}
	}

	if (icon->pSubDock != NULL && pSubDock == icon->pSubDock)  // c'est le meme sous-dock, son rendu a pu change.
	{
		if ((cRendererName != NULL && icon->pSubDock->cRendererName == NULL)
		 || (cRendererName == NULL && icon->pSubDock->cRendererName != NULL)
		 || (cRendererName != NULL && icon->pSubDock->cRendererName != NULL && strcmp (cRendererName, icon->pSubDock->cRendererName) != 0))
			cairo_dock_update_dock_size (icon->pSubDock);
	}

	//\_____________ On l'insere dans le dock auquel elle appartient maintenant.
	CairoDock *pNewContainer = cairo_dock_search_dock_from_name (icon->cParentDockName);
	g_return_if_fail (pNewContainer != NULL);

	if (pDock != pNewContainer && icon->fOrder > g_list_length (pNewContainer->icons) + 1)
		icon->fOrder = CAIRO_DOCK_LAST_ORDER;

	cairo_dock_insert_icon_in_dock (icon, pNewContainer, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);  // on n'empeche pas les bouclages.

	if (pDock != pNewContainer)
		cairo_dock_update_dock_size (pDock);

	//\_____________ On gere l'inhibition de sa classe.
	gchar *cNowClass = icon->cClass;
	if (cClass != NULL && (cNowClass == NULL || strcmp (cNowClass, cClass) != 0))
	{
		icon->cClass = cClass;
		cairo_dock_deinhibate_class (cClass, icon);
		cClass = NULL;  // libere par la fonction precedente.
		icon->cClass = cNowClass;
	}
	if (cNowClass != NULL && (cClass == NULL || strcmp (cNowClass, cClass) != 0))
		cairo_dock_inhibate_class (cNowClass, icon);

	//\_____________ On redessine les docks impactes.
	cairo_dock_calculate_dock_icons (pDock);
	gtk_widget_queue_draw (pDock->pWidget);
	if (pNewContainer != pDock)
	{
		cairo_dock_calculate_dock_icons (pNewContainer);
		gtk_widget_queue_draw (pNewContainer->pWidget);

		if (pDock->icons == NULL)
		{
			cd_message ("dock %s vide => a la poubelle", cPrevDockName);
			cairo_dock_destroy_dock (pDock, cPrevDockName, NULL, NULL);
		}
	}

	g_free (cPrevDockName);
	g_free (cClass);
	g_free (cDesktopFileName);
	g_free (cName);
	g_free (cRendererName);
	cairo_destroy (pCairoContext);
	cairo_dock_mark_theme_as_modified (TRUE);
}
static void _cairo_dock_modify_launcher (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];

	gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->acDesktopFileName);

	cairo_dock_update_launcher_desktop_file (cDesktopFilePath, CAIRO_DOCK_IS_SEPARATOR (icon) ? CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR : (icon->pSubDock != NULL && icon->Xid == 0) ? CAIRO_DOCK_LAUNCHER_FOR_CONTAINER : CAIRO_DOCK_LAUNCHER_FROM_DESKTOP_FILE);

	
	gboolean config_ok = cairo_dock_build_normal_gui (cDesktopFilePath, NULL, _("Modify this launcher"), CAIRO_DOCK_LAUNCHER_PANEL_WIDTH, CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT, (CairoDockApplyConfigFunc)NULL, NULL, NULL);
	g_free (cDesktopFilePath);
	
	if (config_ok)
	{
		_on_modify_launcher (icon);
	}
	
	if (! pDock->bInside)
	{
		//g_print ("on force a quitter\n");
		pDock->bInside = TRUE;
		pDock->bAtBottom = FALSE;
		cairo_dock_on_leave_notify (pDock->pWidget,
			NULL,
			pDock);
	}
}

static void _cairo_dock_show_file_properties (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	//g_print ("%s (%s)\n", __func__, icon->acName);

	guint64 iSize = 0;
	time_t iLastModificationTime = 0;
	gchar *cMimeType = NULL;
	int iUID=0, iGID=0, iPermissionsMask=0;
	if (cairo_dock_fm_get_file_properties (icon->acCommand, &iSize, &iLastModificationTime, &cMimeType, &iUID, &iGID, &iPermissionsMask))
	{
		GtkWidget *pDialog = gtk_message_dialog_new (GTK_WINDOW (pDock->pWidget),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"Properties :");

		GString *sInfo = g_string_new ("");
		g_string_printf (sInfo, "<b>%s</b>", icon->acName);

		GtkWidget *pLabel= gtk_label_new (NULL);
		gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
		gtk_label_set_markup (GTK_LABEL (pLabel), sInfo->str);

		GtkWidget *pFrame = gtk_frame_new (NULL);
		gtk_container_set_border_width (GTK_CONTAINER (pFrame), 3);
		gtk_frame_set_label_widget (GTK_FRAME (pFrame), pLabel);
		gtk_frame_set_shadow_type (GTK_FRAME (pFrame), GTK_SHADOW_OUT);
		gtk_container_add (GTK_CONTAINER (GTK_DIALOG (pDialog)->vbox), pFrame);

		GtkWidget *pVBox = gtk_vbox_new (FALSE, 3);
		gtk_container_add (GTK_CONTAINER (pFrame), pVBox);

		pLabel = gtk_label_new (NULL);
		gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
		g_string_printf (sInfo, "<u>Size</u> : %d bytes", iSize);
		if (iSize > 1024*1024)
			g_string_append_printf (sInfo, " (%.1f Mo)", 1. * iSize / 1024 / 1024);
		else if (iSize > 1024)
			g_string_append_printf (sInfo, " (%.1f Ko)", 1. * iSize / 1024);
		gtk_label_set_markup (GTK_LABEL (pLabel), sInfo->str);
		gtk_container_add (GTK_CONTAINER (pVBox), pLabel);

		pLabel = gtk_label_new (NULL);
		gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
		struct tm epoch_tm;
		localtime_r (&iLastModificationTime, &epoch_tm);  // et non pas gmtime_r.
		gchar *cTimeChain = g_new0 (gchar, 100);
		strftime (cTimeChain, 100, "%F, %T", &epoch_tm);
		g_string_printf (sInfo, "<u>Last Modification</u> : %s", cTimeChain);
		g_free (cTimeChain);
		gtk_label_set_markup (GTK_LABEL (pLabel), sInfo->str);
		gtk_container_add (GTK_CONTAINER (pVBox), pLabel);

		if (cMimeType != NULL)
		{
			pLabel = gtk_label_new (NULL);
			gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
			g_string_printf (sInfo, "<u>Mime Type</u> : %s", cMimeType);
			gtk_label_set_markup (GTK_LABEL (pLabel), sInfo->str);
			gtk_container_add (GTK_CONTAINER (pVBox), pLabel);
		}

		GtkWidget *pSeparator = gtk_hseparator_new ();
		gtk_container_add (GTK_CONTAINER (pVBox), pSeparator);

		pLabel = gtk_label_new (NULL);
		gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
		g_string_printf (sInfo, "<u>User ID</u> : %d / <u>Group ID</u> : %d", iUID, iGID);
		gtk_label_set_markup (GTK_LABEL (pLabel), sInfo->str);
		gtk_container_add (GTK_CONTAINER (pVBox), pLabel);

		pLabel = gtk_label_new (NULL);
		gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
		int iOwnerPermissions = iPermissionsMask >> 6;  // 8*8.
		int iGroupPermissions = (iPermissionsMask - (iOwnerPermissions << 6)) >> 3;
		int iOthersPermissions = (iPermissionsMask % 8);
		g_string_printf (sInfo, "<u>Permissions</u> : %d / %d / %d", iOwnerPermissions, iGroupPermissions, iOthersPermissions);
		gtk_label_set_markup (GTK_LABEL (pLabel), sInfo->str);
		gtk_container_add (GTK_CONTAINER (pVBox), pLabel);

		gtk_widget_show_all (GTK_DIALOG (pDialog)->vbox);
		gtk_window_set_position (GTK_WINDOW (pDialog), GTK_WIN_POS_CENTER_ALWAYS);
		int answer = gtk_dialog_run (GTK_DIALOG (pDialog));
		gtk_widget_destroy (pDialog);

		g_string_free (sInfo, TRUE);
		g_free (cMimeType);
	}
}

static void _cairo_dock_mount_unmount (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	cd_message ("%s (%s / %s)\n", __func__, icon->acName, icon->acCommand);

	gboolean bIsMounted = FALSE;
	gchar *cActivationURI = cairo_dock_fm_is_mounted (icon->acCommand, &bIsMounted);
	cd_message ("  cActivationURI : %s; bIsMounted : %d\n", cActivationURI, bIsMounted);
	g_free (cActivationURI);

	if (! bIsMounted)
	{
		cairo_dock_fm_mount (icon, CAIRO_CONTAINER (pDock));
	}
	else
		cairo_dock_fm_unmount (icon, CAIRO_CONTAINER (pDock));
}

static void _cairo_dock_eject (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	cd_message ("%s (%s / %s)\n", __func__, icon->acName, icon->acCommand);
	cairo_dock_fm_eject_drive (icon->acCommand);
}


static void _cairo_dock_delete_file (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	cd_message ("%s (%s)\n", __func__, icon->acName);

	gchar *question = g_strdup_printf (_("You're about deleting this file\n  (%s)\nfrom your hard-disk. Sure ?"), icon->acCommand);
	int answer = cairo_dock_ask_question_and_wait (question, icon, CAIRO_CONTAINER (pDock));
	g_free (question);
	if (answer == GTK_RESPONSE_YES)
	{
		gboolean bSuccess = cairo_dock_fm_delete_file (icon->acCommand);
		if (! bSuccess)
		{
			cd_message ("Attention : couldn't delete this file.\nCheck that you have writing rights on this file.\n");
			gchar *cMessage = g_strdup_printf (_("Attention : couldn't delete this file.\nCheck that you have writing rights on it."));
			cairo_dock_show_temporary_dialog_with_default_icon (cMessage, icon, CAIRO_CONTAINER (pDock), 4000);
			g_free (cMessage);
		}
		///cairo_dock_remove_icon_from_dock (pDock, icon);
		///cairo_dock_update_dock_size (pDock);

		if (icon->acDesktopFileName != NULL)
		{
			gchar *icon_path = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->acDesktopFileName);
			g_remove (icon_path);
			g_free (icon_path);
		}

		///cairo_dock_free_icon (icon);
	}
}

static void _cairo_dock_rename_file (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	cd_message ("%s (%s)", __func__, icon->acName);

	gchar *cNewName = cairo_dock_show_demand_and_wait (_("Rename to :"), icon, CAIRO_CONTAINER (pDock), icon->acName);
	if (cNewName != NULL && *cNewName != '\0')
	{
		gboolean bSuccess = cairo_dock_fm_rename_file (icon->acCommand, cNewName);
		if (! bSuccess)
		{
			cd_warning ("couldn't rename this file.\nCheck that you have writing rights, and that the new name does not already exist.");
			cairo_dock_show_temporary_dialog (_("Attention : couldn't rename %s.\nCheck that you have writing rights,\n and that the new name does not already exist."), icon, CAIRO_CONTAINER (pDock), 5000, icon->acCommand);
		}
	}
	g_free (cNewName);
}



static void _cairo_dock_initiate_config_module (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoContainer *pContainer= data[1];
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module !
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	cairo_dock_build_main_ihm (g_cConfFile, FALSE);
	cairo_dock_present_module_instance_gui (icon->pModuleInstance);
}

static void _cairo_dock_detach_module (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoContainer *pContainer= data[1];
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module !
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
		G_TYPE_BOOLEAN, "Desklet", "initially detached", CAIRO_DOCK_IS_DOCK (pContainer),
		G_TYPE_INVALID);

	cairo_dock_reload_module_instance (icon->pModuleInstance, TRUE);
	if (icon->pModuleInstance->pDesklet)  // on a detache l'applet.
		cairo_dock_zoom_out_desklet (icon->pModuleInstance->pDesklet);
}

static void _cairo_dock_remove_module_instance (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoContainer *pContainer= data[1];
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module !
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	gchar *question = g_strdup_printf (_("You're about removing this applet (%s) from the dock. Sure ?"), icon->pModuleInstance->pModule->pVisitCard->cModuleName);
	int answer = cairo_dock_ask_question_and_wait (question, icon, CAIRO_CONTAINER (pContainer));
	if (answer == GTK_RESPONSE_YES)
	{
		cairo_dock_remove_module_instance (icon->pModuleInstance);
	}
}

static void _cairo_dock_add_module_instance (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoContainer *pContainer= data[1];
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module !
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	cairo_dock_add_module_instance (icon->pModuleInstance->pModule);
}

static void _cairo_dock_close_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];

	if (CAIRO_DOCK_IS_APPLI (icon))
		cairo_dock_close_xwindow (icon->Xid);
}
static void _cairo_dock_kill_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];

	if (CAIRO_DOCK_IS_APPLI (icon))
		cairo_dock_kill_xwindow (icon->Xid);
}
static void _cairo_dock_minimize_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon->Xid > 0)
	{
		cairo_dock_minimize_xwindow (icon->Xid);
	}
}

static void _cairo_dock_show_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon->Xid > 0)
	{
		cairo_dock_show_xwindow (icon->Xid);
	}
}

static void _cairo_dock_launch_new (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon->acCommand != NULL)
	{
		cairo_dock_notify (CAIRO_DOCK_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
	}
}

static void _cairo_dock_make_launcher_from_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	g_return_if_fail (icon->Xid != 0);
	
	// on trouve le .desktop du programme.
	gchar *cDesktopFilePath = g_strdup_printf ("/usr/share/applications/%s.desktop", icon->cClass);
	
	// on cree un nouveau lanceur a partir.
	if (! g_file_test (cDesktopFilePath, G_FILE_TEST_EXISTS))
	{
	   // chercher un desktop qui contiennent StartupWMClass=class.
	   
	}
	{
		cairo_dock_add_new_launcher_by_uri (cDesktopFilePath, g_pMainDock, CAIRO_DOCK_LAST_ORDER);
	}
	g_free (cDesktopFilePath);
}

static void _cairo_dock_close_class (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon->pSubDock != NULL)
	{
		Icon *pIcon;
		GList *ic;
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->Xid != 0)
			{
				cairo_dock_close_xwindow (pIcon->Xid);
			}
		}
	}
}

static void _cairo_dock_maximize_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon->Xid > 0)
	{
		gboolean bIsMaximized = cairo_dock_window_is_maximized (icon->Xid);
		cairo_dock_maximize_xwindow (icon->Xid, ! bIsMaximized);
	}
}

static void _cairo_dock_set_appli_fullscreen (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon->Xid > 0)
	{
		gboolean bIsFullScreen = cairo_dock_window_is_fullscreen (icon->Xid);
		cairo_dock_set_xwindow_fullscreen (icon->Xid, ! bIsFullScreen);
	}
}

static void _cairo_dock_move_appli_to_current_desktop (GtkMenuItem *pMenuItem, gpointer *data)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon->Xid > 0)
	{
		int iCurrentDesktop = cairo_dock_get_current_desktop ();
		cairo_dock_move_xwindow_to_nth_desktop (icon->Xid, iCurrentDesktop, 0, 0);  // on ne veut pas decaler son viewport par rapport a nous.
	}
}

static void _cairo_dock_move_appli_to_desktop (GtkMenuItem *pMenuItem, gpointer *user_data)
{
	gpointer *data = user_data[0];
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	int iDesktopNumber = GPOINTER_TO_INT (user_data[1]);
	int iViewPortNumberY = GPOINTER_TO_INT (user_data[2]);
	int iViewPortNumberX = GPOINTER_TO_INT (user_data[3]);
	cd_message ("%s (%d;%d;%d)", __func__, iDesktopNumber, iViewPortNumberX, iViewPortNumberY);
	if (icon->Xid > 0)
	{
		int iCurrentDesktopNumber = cairo_dock_get_window_desktop (icon->Xid);
		
		int iCurrentViewPortX, iCurrentViewPortY;
		cairo_dock_get_current_viewport (&iCurrentViewPortX, &iCurrentViewPortY);
		cd_debug (" current_viewport : %d;%d", iCurrentViewPortX, iCurrentViewPortY);
		
		cairo_dock_move_xwindow_to_nth_desktop (icon->Xid, iDesktopNumber, iViewPortNumberX * g_iScreenWidth[CAIRO_DOCK_HORIZONTAL] - iCurrentViewPortX, iViewPortNumberY * g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] - iCurrentViewPortY);
	}
}

static void _cairo_dock_change_window_above (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon->Xid > 0)
	{
		gboolean bIsAbove=FALSE, bIsBelow=FALSE;
		cairo_dock_window_is_above_or_below (icon->Xid, &bIsAbove, &bIsBelow);
		cairo_dock_set_xwindow_above (icon->Xid, ! bIsAbove);
	}
}


/*static void cairo_dock_swap_with_prev_icon (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	Icon *prev_icon = cairo_dock_get_previous_icon (pDock->icons, icon);
	cairo_dock_swap_icons (pDock, icon, prev_icon);
}

static void cairo_dock_swap_with_next_icon (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	Icon *next_icon = cairo_dock_get_next_icon (pDock->icons, icon);
	cairo_dock_swap_icons (pDock, icon, next_icon);
}

static void cairo_dock_move_icon_to_beginning (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	cairo_dock_move_icon_after_icon (pDock, icon, NULL);
}

static void cairo_dock_move_icon_to_end (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	Icon* pLastIcon = cairo_dock_get_last_icon_of_type (pDock->icons, icon->iType);
	if (pLastIcon != NULL && pLastIcon != icon)
		cairo_dock_move_icon_after_icon (pDock, icon, pLastIcon);
}*/

static void _cairo_dock_keep_window_in_state (gpointer *data, gboolean bAbove, gboolean bBelow)
{
	Icon *icon = data[0];
	CairoDesklet *pDesklet = data[1];
	if (icon == NULL)  // cas de l'applet Terminal par exemple.
		icon = pDesklet->pIcon;
	
	gtk_window_set_keep_below (GTK_WINDOW(pDesklet->pWidget), bBelow);
	gtk_window_set_keep_above (GTK_WINDOW(pDesklet->pWidget), bAbove);
	if (CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_BOOLEAN, "Desklet", "keep below", bBelow,
			G_TYPE_BOOLEAN, "Desklet", "keep above", bAbove,
			G_TYPE_INVALID);
}
static void _cairo_dock_keep_above(GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	cd_debug ("");
	if (gtk_check_menu_item_get_active (pMenuItem))
		_cairo_dock_keep_window_in_state (data, TRUE, FALSE);
}

static void _cairo_dock_keep_normal(GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	cd_debug ("");
	if (gtk_check_menu_item_get_active (pMenuItem))
		_cairo_dock_keep_window_in_state (data, FALSE, FALSE);
}

static void _cairo_dock_keep_below(GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	cd_debug ("");
	if (gtk_check_menu_item_get_active (pMenuItem))
		_cairo_dock_keep_window_in_state (data, FALSE, TRUE);
}

//for compiz fusion "widget layer"
//set behaviour in compiz to: (name=cairo-dock & type=utility)
static void _cairo_dock_keep_on_widget_layer (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDesklet *pDesklet = data[1];

	cairo_dock_hide_desklet (pDesklet);
	Window Xid = GDK_WINDOW_XID (pDesklet->pWidget->window);

	gboolean bOnCompizWidgetLayer = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem));
	cd_debug (" bOnCompizWidgetLayer : %d", bOnCompizWidgetLayer);
	if (bOnCompizWidgetLayer)
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_UTILITY");
		//gtk_window_set_type_hint(GTK_WINDOW(pDock->pWidget), GDK_WINDOW_TYPE_HINT_UTILITY);
	else
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_NORMAL");
		//gtk_window_set_type_hint(GTK_WINDOW(pDock->pWidget), GDK_WINDOW_TYPE_HINT_NORMAL);
	cairo_dock_show_desklet (pDesklet);

	if (CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_BOOLEAN, "Desklet", "on widget layer", bOnCompizWidgetLayer,
			G_TYPE_INVALID);
}

static void _cairo_dock_lock_position (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDesklet *pDesklet = data[1];
	
	pDesklet->bPositionLocked = ! pDesklet->bPositionLocked;
	if (CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_BOOLEAN, "Desklet", "locked", pDesklet->bPositionLocked,
			G_TYPE_INVALID);
}


static void _cairo_dock_configure_root_dock_position (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock* pDock = data[1];
	g_return_if_fail (pDock->iRefCount == 0 && ! pDock->bIsMainDock);
	
	const gchar *cDockName = cairo_dock_search_dock_name (pDock);
	g_return_if_fail (cDockName != NULL);
	cd_message ("%s (%s)", __func__, cDockName);
	
	gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
	{
		gchar *cCommand = g_strdup_printf ("cp %s/%s %s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_MAIN_DOCK_CONF_FILE, cConfFilePath);
		system (cCommand);
		g_free (cCommand);
	}
	
	gchar *cTitle = g_strdup_printf (_("Set position for the dock '%s'"), cDockName);
	//gboolean config_ok = cairo_dock_edit_conf_file (GTK_WINDOW (pDock->pWidget), cConfFilePath, cTitle, CAIRO_DOCK_CONF_PANEL_WIDTH, CAIRO_DOCK_CONF_PANEL_HEIGHT, 0, NULL, NULL, NULL, NULL, CAIRO_DOCK_GETTEXT_PACKAGE);
	gboolean config_ok = cairo_dock_build_normal_gui (cConfFilePath, NULL, cTitle, CAIRO_DOCK_CONF_PANEL_WIDTH, CAIRO_DOCK_CONF_PANEL_HEIGHT, NULL, NULL, NULL);
	g_free (cTitle);
	
	if (config_ok)
	{
		cairo_dock_get_root_dock_position (cDockName, pDock);
		
		cairo_dock_load_buffers_in_one_dock (pDock);  // recharge les icones et les applets.
		cairo_dock_synchronize_sub_docks_position (pDock, TRUE);
		
		cairo_dock_update_dock_size (pDock);
		cairo_dock_calculate_dock_icons (pDock);
		
		cairo_dock_place_root_dock (pDock);
		gtk_widget_queue_draw (pDock->pWidget);
	}
	
	g_free (cConfFilePath);
}


void cairo_dock_delete_menu (GtkMenuShell *menu, CairoDock *pDock)
{
	cd_debug ("");
	pDock->bMenuVisible = FALSE;
	if (CAIRO_DOCK_IS_DOCK (pDock)/* && ! pDock->bInside*/)
	{
		cd_message ("on force a quitter");
		pDock->bInside = TRUE;
		pDock->bAtBottom = FALSE;
		///cairo_dock_disable_entrance ();  // trop violent, il faudrait trouver un autre truc.
		cairo_dock_on_leave_notify (pDock->pWidget,
			NULL,
			pDock);
	}
}


#define _add_entry_in_menu(cLabel, gtkStock, pSubMenu, pCallBack) CAIRO_DOCK_ADD_IN_MENU_WITH_STOCK_AND_DATA (cLabel, gtkStock, pSubMenu, pCallBack, data)

GtkWidget *cairo_dock_build_menu (Icon *icon, CairoContainer *pContainer)
{
	static gpointer *data = NULL;
	static GtkWidget *s_pMenu = NULL;
	
	if (s_pMenu != NULL)
	{
		gtk_widget_destroy (s_pMenu);
		s_pMenu = NULL;
	}
	
	//\_________________________ On construit le menu et les donnees passees en callback.
	GtkWidget *menu = gtk_menu_new ();
	s_pMenu = menu;
	
	if (CAIRO_DOCK_IS_DOCK (pContainer))
		g_signal_connect (G_OBJECT (menu),
			"deactivate",
			G_CALLBACK (cairo_dock_delete_menu),
			pContainer);

	if (data == NULL)
		data = g_new (gpointer, 3);
	data[0] = icon;
	data[1] = pContainer;
	data[2] = menu;

	//\_________________________ On ajoute le sous-menu Cairo-Dock, toujours present.
	GtkWidget *pMenuItem, *image;
	pMenuItem = gtk_image_menu_item_new_with_label ("Cairo-Dock");
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, 32, 32, NULL);
	image = gtk_image_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);
	gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);

	GtkWidget *pSubMenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenu);

	if (! g_bLocked)
	{
		_add_entry_in_menu (_("Configure"), GTK_STOCK_PREFERENCES, _cairo_dock_edit_and_reload_conf, pSubMenu);
	
		pMenuItem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (pSubMenu), pMenuItem);
	
		if (CAIRO_DOCK_IS_DOCK (pContainer) && ! CAIRO_DOCK (pContainer)->bIsMainDock && CAIRO_DOCK (pContainer)->iRefCount == 0)
		{
			_add_entry_in_menu (_("Set up this dock"), GTK_STOCK_EXECUTE, _cairo_dock_configure_root_dock_position, pSubMenu);
		}
		_add_entry_in_menu (_("Manage themes"), GTK_STOCK_EXECUTE, _cairo_dock_initiate_theme_management, pSubMenu);
	}
	
	pMenuItem = gtk_image_menu_item_new_with_label (myAccessibility.bLockIcons ? _("unlock icons") : _("lock icons"));
	pixbuf = gdk_pixbuf_new_from_file_at_size (CAIRO_DOCK_SHARE_DATA_DIR"/icon-lock-icons.svg", 16, 16, NULL);
	image = gtk_image_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);
	gtk_menu_shell_append  (GTK_MENU_SHELL (pSubMenu), pMenuItem);
	g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK(_cairo_dock_lock_icons), data);
	
	if (CAIRO_DOCK_IS_DOCK (pContainer) && ! CAIRO_DOCK (pContainer)->bAutoHide)
	{
		_add_entry_in_menu (_("Quick-Hide"), GTK_STOCK_GOTO_BOTTOM, _cairo_dock_quick_hide, pSubMenu);
	}
	
	gchar *cCairoAutoStartDirPath = g_strdup_printf ("%s/.config/autostart", g_getenv ("HOME"));
	gchar *cCairoAutoStartEntryPath = g_strdup_printf ("%s/cairo-dock.desktop", cCairoAutoStartDirPath);
	gchar *cCairoAutoStartEntryPath2 = g_strdup_printf ("%s/cairo-dock-cairo.desktop", cCairoAutoStartDirPath);
	if (g_file_test (cCairoAutoStartDirPath, G_FILE_TEST_IS_DIR) && ! g_file_test (cCairoAutoStartEntryPath, G_FILE_TEST_EXISTS) && ! g_file_test (cCairoAutoStartEntryPath2, G_FILE_TEST_EXISTS))
	{
		_add_entry_in_menu (_("Launch Cairo-Dock on startup"), GTK_STOCK_ADD, _cairo_dock_add_autostart, pSubMenu);
	}
	g_free (cCairoAutoStartEntryPath);
	g_free (cCairoAutoStartEntryPath2);
	g_free (cCairoAutoStartDirPath);
	
	_add_entry_in_menu (_("Development's site"), GTK_STOCK_DIALOG_WARNING, _cairo_dock_check_for_updates, pSubMenu);

	_add_entry_in_menu (_("Community's site"), GTK_STOCK_DIALOG_INFO, _cairo_dock_help, pSubMenu);

	_add_entry_in_menu (_("Help"), GTK_STOCK_HELP, _cairo_dock_present_help, pSubMenu);

	_add_entry_in_menu (_("About"), GTK_STOCK_ABOUT, _cairo_dock_about, pSubMenu);

	if (! g_bLocked)
	{
		_add_entry_in_menu (_("Quit"), GTK_STOCK_QUIT, _cairo_dock_quit, pSubMenu);
	}

	//\_________________________ On passe la main a ceux qui veulent y rajouter des choses.
	cairo_dock_notify (CAIRO_DOCK_BUILD_MENU, icon, pContainer, menu);

	return menu;
}


gboolean cairo_dock_notification_build_menu (gpointer *pUserData, Icon *icon, CairoContainer *pContainer, GtkWidget *menu)
{
	static gpointer *data = NULL;
	//g_print ("%x;%x;%x\n", icon, pContainer, menu);
	if (data == NULL)
		data = g_new (gpointer, 3);
	data[0] = icon;
	data[1] = pContainer;
	data[2] = menu;
	GtkWidget *pMenuItem, *image;
	static gpointer *pDesktopData = NULL;

	//\_________________________ Si pas d'icone dans un dock, on s'arrete la.
	if (! g_bLocked && CAIRO_DOCK_IS_DOCK (pContainer) && (icon == NULL || CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon)))
	{
		pMenuItem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
		
		_add_entry_in_menu (_("Add a manual launcher"), GTK_STOCK_ADD, cairo_dock_add_launcher, menu);
		gtk_widget_set_tooltip_text (pMenuItem, _("Don't forget you can drag a launcher from the menu and drop it in the dock !"));
		
		_add_entry_in_menu (_("Add a sub-dock"), GTK_STOCK_ADD, cairo_dock_add_container, menu);
		
		if (icon != NULL)
		{
			_add_entry_in_menu (_("Add a separator"), GTK_STOCK_ADD, cairo_dock_add_separator, menu);
		}

		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	}

	//\_________________________ On rajoute des actions suivant le type de l'icone.
	if (CAIRO_DOCK_IS_LAUNCHER (icon) || CAIRO_DOCK_IS_USER_SEPARATOR (icon) || (icon != NULL && icon->acDesktopFileName != NULL))  // le dernier cas est la pour les cas ou l'utilisateur aurait par megarde efface la commande du lanceur.
	{
		//\_________________________ On rajoute les actions sur les icones de fichiers.
		if (CAIRO_DOCK_IS_URI_LAUNCHER (icon))
		{
			if (icon->iVolumeID > 0)
			{
				gboolean bIsMounted = FALSE;
				cd_message ("%s (%s / %s)\n", __func__, icon->acName, icon->acCommand);
				gchar *cActivationURI = cairo_dock_fm_is_mounted  (icon->acCommand, &bIsMounted);
				cd_message ("  cActivationURI : %s; bIsMounted : %d\n", cActivationURI, bIsMounted);
				g_free (cActivationURI);

				pMenuItem = gtk_menu_item_new_with_label (bIsMounted ? _("Unmount") : _("Mount"));
				gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
				g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK(_cairo_dock_mount_unmount), data);
				
				if (cairo_dock_fm_can_eject (icon->acCommand))
				{
					pMenuItem = gtk_menu_item_new_with_label (_("Eject"));
					gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
					g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK(_cairo_dock_eject), data);
				}
			}
			else
			{
				pMenuItem = gtk_menu_item_new_with_label (_("Delete this file"));
				gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
				g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK(_cairo_dock_delete_file), data);

				pMenuItem = gtk_menu_item_new_with_label (_("Rename this file"));
				gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
				g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK(_cairo_dock_rename_file), data);

				pMenuItem = gtk_menu_item_new_with_label (_("Properties"));
				gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
				g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK(_cairo_dock_show_file_properties), data);
			}
			if (icon->acDesktopFileName == NULL)  // un lanceur sans fichier .desktop, on est donc dans un sous-dock ou l'on ne peut pas faire d'autre action.
				return CAIRO_DOCK_LET_PASS_NOTIFICATION;
		}
		
		//\_________________________ On rajoute des actions de modifications sur le dock.
		if (! g_bLocked && CAIRO_DOCK_IS_DOCK (pContainer))
		{
			pMenuItem = gtk_separator_menu_item_new ();
			gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
			
			_add_entry_in_menu (_("Add a manual launcher"), GTK_STOCK_ADD, cairo_dock_add_launcher, menu);
			gtk_widget_set_tooltip_text (pMenuItem, _("Don't forget you can drag a launcher from the menu and drop it in the dock !"));
	
			_add_entry_in_menu (_("Add a sub-dock"), GTK_STOCK_ADD, cairo_dock_add_container, menu);
			
			if (icon != NULL)
			{
				_add_entry_in_menu (_("Add a separator"), GTK_STOCK_ADD, cairo_dock_add_separator, menu);
			}
			
			///if (CAIRO_DOCK_IS_NORMAL_LAUNCHER (icon) || CAIRO_DOCK_IS_USER_SEPARATOR (icon))  // possede un .desktop.
			if (icon->acDesktopFileName != NULL)
			{
				pMenuItem = gtk_separator_menu_item_new ();
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
			
				_add_entry_in_menu (CAIRO_DOCK_IS_USER_SEPARATOR (icon) ? _("Remove this separator") : _("Remove this launcher"), GTK_STOCK_REMOVE, _cairo_dock_remove_launcher, menu);
			
				_add_entry_in_menu (CAIRO_DOCK_IS_USER_SEPARATOR (icon) ? _("Modify this separator") : _("Modify this launcher"), GTK_STOCK_EDIT, _cairo_dock_modify_launcher, menu);
			}
		}
	}
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		pMenuItem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		
		//\_________________________ On rajoute les actions supplementaires sur les icones d'applis.
		pMenuItem = gtk_menu_item_new_with_label (_("Other actions"));
		gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
		GtkWidget *pSubMenuOtherActions = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenuOtherActions);
		
		_add_entry_in_menu (_("Move to this desktop"), GTK_STOCK_JUMP_TO, _cairo_dock_move_appli_to_current_desktop, pSubMenuOtherActions);
		
		gboolean bIsFullScreen = cairo_dock_window_is_fullscreen (icon->Xid);
		_add_entry_in_menu (bIsFullScreen ? _("Not Fullscreen") : _("Fullscreen"), bIsFullScreen ? GTK_STOCK_LEAVE_FULLSCREEN : GTK_STOCK_FULLSCREEN, _cairo_dock_set_appli_fullscreen, pSubMenuOtherActions);
		
		gboolean bIsAbove=FALSE, bIsBelow=FALSE;
		cairo_dock_window_is_above_or_below (icon->Xid, &bIsAbove, &bIsBelow);
		_add_entry_in_menu (bIsAbove ? _("Don't keep above") : _("Keep above"), bIsAbove ? GTK_STOCK_GOTO_BOTTOM : GTK_STOCK_GOTO_TOP, _cairo_dock_change_window_above, pSubMenuOtherActions);
		cd_debug ("g_iNbDesktops : %d ; g_iNbViewportX : %d ; g_iNbViewportY : %d", g_iNbDesktops, g_iNbViewportX, g_iNbViewportY);
		if (g_iNbDesktops > 1 || g_iNbViewportX > 1 || g_iNbViewportY > 1)
		{
			int i, j, k, iDesktopCode;
			const gchar *cLabel;
			if (g_iNbDesktops > 1 && (g_iNbViewportX > 1 || g_iNbViewportY > 1))
				cLabel = _("Move to desktop %d - face %d");
			else if (g_iNbDesktops > 1)
				cLabel = _("Move to desktop %d");
			else
				cLabel = _("Move to face %d");
			GString *sDesktop = g_string_new ("");
			g_free (pDesktopData);
			pDesktopData = g_new0 (gpointer, 4 * g_iNbDesktops * g_iNbViewportX * g_iNbViewportY);
			gpointer *user_data;
			
			for (i = 0; i < g_iNbDesktops; i ++)  // on range par bureau.
			{
				for (j = 0; j < g_iNbViewportY; j ++)  // puis par rangee.
				{
					for (k = 0; k < g_iNbViewportX; k ++)
					{
						if (g_iNbDesktops > 1 && (g_iNbViewportX > 1 || g_iNbViewportY > 1))
							g_string_printf (sDesktop, cLabel, i+1, j*g_iNbViewportX+k+1);
						else if (g_iNbDesktops > 1)
							g_string_printf (sDesktop, cLabel, i+1);
						else
							g_string_printf (sDesktop, cLabel, j*g_iNbViewportX+k+1);
						iDesktopCode = i * g_iNbViewportY * g_iNbViewportX + j * g_iNbViewportY + k;
						user_data = &pDesktopData[4*iDesktopCode];
						user_data[0] = data;
						user_data[1] = GINT_TO_POINTER (i);
						user_data[2] = GINT_TO_POINTER (j);
						user_data[3] = GINT_TO_POINTER (k);
						
						CAIRO_DOCK_ADD_IN_MENU_WITH_STOCK_AND_DATA (sDesktop->str, NULL, _cairo_dock_move_appli_to_desktop, pSubMenuOtherActions, user_data);
					}
				}
			}
			g_string_free (sDesktop, TRUE);
			
			_add_entry_in_menu (_("Kill"), GTK_STOCK_CANCEL, _cairo_dock_kill_appli, pSubMenuOtherActions);
		}
		
		//\_________________________ On rajoute les actions courantes sur les icones d'applis.
		if (icon->acDesktopFileName != NULL)  // c'est un lanceur inhibiteur.
		{
			_add_entry_in_menu (_("Launch new"), GTK_STOCK_ADD, _cairo_dock_launch_new, menu);
		}
		
		if (! cairo_dock_class_is_inhibated (icon->cClass))
		{
			_add_entry_in_menu (_("Make it a launcher"), GTK_STOCK_CONVERT, _cairo_dock_make_launcher_from_appli, menu);
		}
		
		gboolean bIsMaximized = cairo_dock_window_is_maximized (icon->Xid);
		_add_entry_in_menu (bIsMaximized ? _("Unmaximize") : _("Maximize"), GTK_STOCK_GO_UP, _cairo_dock_maximize_appli, menu);
		
		_add_entry_in_menu (_("Show"), GTK_STOCK_FIND, _cairo_dock_show_appli, menu);

		_add_entry_in_menu (_("Minimize"), GTK_STOCK_GO_DOWN, _cairo_dock_minimize_appli, menu);

		_add_entry_in_menu (_("Close"), GTK_STOCK_CLOSE, _cairo_dock_close_appli, menu);
	}
	else if (CAIRO_DOCK_IS_LAUNCHER (icon) && icon->pSubDock != NULL && icon->cClass != NULL)  // inhibiteur avec sous-dock de classe.
	{
		pMenuItem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		
		if (icon->acDesktopFileName != NULL)  // c'est un lanceur inhibiteur.
		{
			_add_entry_in_menu (_("Launch new"), GTK_STOCK_ADD, _cairo_dock_launch_new, menu);
		}
		
		_add_entry_in_menu (_("Close all"), GTK_STOCK_CLOSE, _cairo_dock_close_class, menu);
	}
	
	if (CAIRO_DOCK_IS_APPLET (icon) || CAIRO_DOCK_IS_DESKLET (pContainer))  // on regarde si pModule != NULL de facon a le faire que pour l'icone qui detient effectivement le module.
	{
		Icon *pIconModule;
		if (CAIRO_DOCK_IS_APPLET (icon))
			pIconModule = icon;
		else if (CAIRO_DOCK_IS_DESKLET (pContainer))
			pIconModule = CAIRO_DESKLET (pContainer)->pIcon;
		else
			pIconModule = NULL;
		
		//\_________________________ On rajoute les actions propres a un module.
		if (! g_bLocked && CAIRO_DOCK_IS_APPLET (pIconModule))
		{
			pMenuItem = gtk_separator_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
			
			_add_entry_in_menu (_("Configure this applet"), GTK_STOCK_PROPERTIES, _cairo_dock_initiate_config_module, menu);
	
			if (pIconModule->pModuleInstance->bCanDetach)
			{
				_add_entry_in_menu (CAIRO_DOCK_IS_DOCK (pContainer) ? _("Detach this applet") : _("Return to dock"), CAIRO_DOCK_IS_DOCK (pContainer) ? GTK_STOCK_DISCONNECT : GTK_STOCK_CONNECT, _cairo_dock_detach_module, menu);
			}
			
			_add_entry_in_menu (_("Remove this applet"), GTK_STOCK_REMOVE, _cairo_dock_remove_module_instance, menu);
			
			if (pIconModule->pModuleInstance->pModule->pVisitCard->bMultiInstance)
			{
				_add_entry_in_menu (_("Launch another instance of this applet"), GTK_STOCK_ADD, _cairo_dock_add_module_instance, menu);
			}
		}
	}

	//\_________________________ On rajoute les actions de positionnement d'un desklet.
	if (! g_bLocked && CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		pMenuItem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);

		GSList *group = NULL;

		gboolean bIsAbove=FALSE, bIsBelow=FALSE;
		Window Xid = GDK_WINDOW_XID (pContainer->pWidget->window);
		cd_debug ("Xid : %d", Xid);
		cairo_dock_window_is_above_or_below (Xid, &bIsAbove, &bIsBelow);  // gdk_window_get_state bugue.
		cd_debug (" -> %d;%d", bIsAbove, bIsBelow);

		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Always on top"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), pMenuItem);
		if (bIsAbove)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_above), data);

		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Normal"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), pMenuItem);
		if (! bIsAbove && ! bIsBelow)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_normal), data);

		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Always below"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), pMenuItem);
		if (bIsBelow)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_below), data);

		pMenuItem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);

		pMenuItem = gtk_check_menu_item_new_with_label("Compiz Fusion Widget");
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), pMenuItem);
		if (cairo_dock_window_is_utility (Xid))  // gtk_window_get_type_hint me renvoie toujours 0 !
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_on_widget_layer), data);
		gtk_widget_set_tooltip_text (pMenuItem, _("set behaviour in Compiz to: (name=cairo-dock & type=utility)"));
		
		pMenuItem = gtk_check_menu_item_new_with_label("Lock position");
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		if (CAIRO_DESKLET (pContainer)->bPositionLocked)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_lock_position), data);
	}

	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

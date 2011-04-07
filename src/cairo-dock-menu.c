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
#include <stdlib.h>
#define __USE_POSIX 1
#include <time.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gdk/gdkx.h>

#include "config.h"
#include "cairo-dock-config.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-gui-themes.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-user-interaction.h"  // set_custom_icon_on_appli
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-gui-commons.h"
#include "cairo-dock-menu.h"

#define CAIRO_DOCK_CONF_PANEL_WIDTH 1000
#define CAIRO_DOCK_CONF_PANEL_HEIGHT 600
#define CAIRO_DOCK_ABOUT_WIDTH 600
#define CAIRO_DOCK_ABOUT_HEIGHT 600
#define CAIRO_DOCK_FILE_HOST_URL "https://launchpad.net/cairo-dock"  // https://developer.berlios.de/project/showfiles.php?group_id=8724
#define CAIRO_DOCK_SITE_URL "http://glx-dock.org"  // http://cairo-dock.vef.fr
#define CAIRO_DOCK_FORUM_URL "http://forum.glx-dock.org"  // http://cairo-dock.vef.fr/bg_forumlist.php
#define CAIRO_DOCK_PLUGINS_EXTRAS_URL "http://extras.glx-dock.org"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopGeometry g_desktopGeometry;

extern gchar *g_cConfFile;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentIconsPath;

extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;
extern gboolean g_bLocked;
extern gboolean g_bForceCairo;
extern gboolean g_bEasterEggs;

#define cairo_dock_icons_are_locked(...) (myDocksParam.bLockIcons || myDocksParam.bLockAll || g_bLocked)
#define cairo_dock_is_locked(...) (myDocksParam.bLockAll || g_bLocked)


static void _cairo_dock_edit_and_reload_conf (GtkMenuItem *pMenuItem, gpointer data)
{
	cairo_dock_show_main_gui ();
}

static GtkWidget *s_pRootDockConfigWindow = NULL;
static gboolean on_apply_config_root_dock (const gchar *cDockName)
{
	CairoDock *pDock = cairo_dock_search_dock_from_name (cDockName);
	cairo_dock_reload_one_root_dock (cDockName, pDock);
	return FALSE;  // FALSE <=> ne pas recharger.
}
static void on_destroy_root_dock (gchar *cInitConfFile)
{
	s_pRootDockConfigWindow = NULL;
}
static void _cairo_dock_configure_root_dock (GtkMenuItem *pMenuItem, CairoDock *pDock)
{
	g_return_if_fail (pDock->iRefCount == 0 && ! pDock->bIsMainDock);
	
	cairo_dock_show_items_gui (NULL, CAIRO_CONTAINER (pDock), NULL, 0);
	/**const gchar *cDockName = cairo_dock_search_dock_name (pDock);
	g_return_if_fail (cDockName != NULL);
	cd_message ("%s (%s)", __func__, cDockName);
	
	gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))  // ne devrait pas arriver mais au cas ou.
	{
		cairo_dock_add_root_dock_config_for_name (cDockName);
	}
	g_free (cConfFilePath);*/
}

static void _cairo_dock_initiate_theme_management (GtkMenuItem *pMenuItem, gpointer data)
{
	cairo_dock_manage_themes ();
}

static void _cairo_dock_add_about_page (GtkWidget *pNoteBook, const gchar *cPageLabel, const gchar *cAboutText)
{
	GtkWidget *pVBox, *pScrolledWindow;
	GtkWidget *pPageLabel, *pAboutLabel;
	
	pPageLabel = gtk_label_new (cPageLabel);
	pVBox = gtk_vbox_new (FALSE, 0);
	pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
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
static void _cairo_dock_lock_icons (GtkMenuItem *pMenuItem, gpointer data)
{
	myDocksParam.bLockIcons = ! myDocksParam.bLockIcons;
	cairo_dock_update_conf_file (g_cConfFile,
		G_TYPE_BOOLEAN, "Accessibility", "lock icons", myDocksParam.bLockIcons,
		G_TYPE_INVALID);
}
static void _cairo_dock_lock_all (GtkMenuItem *pMenuItem, gpointer data)
{
	myDocksParam.bLockAll = ! myDocksParam.bLockAll;
	cairo_dock_update_conf_file (g_cConfFile,
		G_TYPE_BOOLEAN, "Accessibility", "lock all", myDocksParam.bLockAll,
		G_TYPE_INVALID);
}
static void _cairo_dock_about (GtkMenuItem *pMenuItem, CairoContainer *pContainer)
{
	// build dialog
	GtkWidget *pDialog = gtk_dialog_new_with_buttons (_("About Cairo-Dock"),
		GTK_WINDOW (pContainer->pWidget),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		"gtk-close",
		GTK_RESPONSE_CLOSE,
		NULL);
	
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 14)
	GtkWidget *pContentBox = gtk_dialog_get_content_area (GTK_DIALOG(pDialog));
#else
	GtkWidget *pContentBox =  GTK_DIALOG(pDialog)->vbox;
#endif
	
	// logo + links
	GtkWidget *pHBox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pContentBox), pHBox, FALSE, FALSE, 0);
	
	const gchar *cImagePath = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_LOGO;
	GtkWidget *pImage = gtk_image_new_from_file (cImagePath);
	gtk_box_pack_start (GTK_BOX (pHBox), pImage, FALSE, FALSE, 0);
	
	GtkWidget *pVBox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pHBox), pVBox, FALSE, FALSE, 0);
	
	GtkWidget *pLink = gtk_link_button_new_with_label (CAIRO_DOCK_SITE_URL, "Cairo-Dock (2007-2011)\n version "CAIRO_DOCK_VERSION);
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	pLink = gtk_link_button_new_with_label (CAIRO_DOCK_FORUM_URL, _("Community site"));
	gtk_widget_set_tooltip_text (pLink, _("Problems? Suggestions? Just want to talk to us? Come on over!"));
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	pLink = gtk_link_button_new_with_label (CAIRO_DOCK_FILE_HOST_URL, _("Development site"));
	gtk_widget_set_tooltip_text (pLink, _("Find the latest version of Cairo-Dock here !"));
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	gchar *cAdress = g_strdup_printf (CAIRO_DOCK_PLUGINS_EXTRAS_URL"/%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion);
	pLink = gtk_link_button_new_with_label (cAdress, _("More applets"));
	g_free (cAdress);
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	// notebook
	GtkWidget *pNoteBook = gtk_notebook_new ();
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (pNoteBook), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (pNoteBook));
	gtk_box_pack_start (GTK_BOX (pContentBox), pNoteBook, TRUE, TRUE, 0);
	
	_cairo_dock_add_about_page (pNoteBook,
		_("Development"),
		"<b>Main developer :</b>\n  Fabounet (Fabrice Rey)\n\
<b>Original idea/first development :</b>\n  Mac Slow\n\
<b>Applets :</b>\n  Fabounet\n  Necropotame\n  Ctaf\n  ChAnGFu\n  Tofe\n  Paradoxxx_Zero\n  Mav\n  Nochka85\n  Ours_en_pluche\n  Eduardo Mucelli\n\
<b>Patchs :</b>\n  Special thanks to Augur for his great help with OpenGL\n  Ctaf\n  M.Tasaka\n  Matttbe\n  Necropotame\n  Robrob\n  Smidgey\n  Tshirtman\n");
	_cairo_dock_add_about_page (pNoteBook,
		_("Artwork"),
		"<b>Themes :</b>\n  Fabounet\n  Chilperik\n  Djoole\n  Glattering\n  Vilraleur\n  Lord Northam\n  Paradoxxx_Zero\n  Coz\n  Benoit2600\n  Nochka85\n  Taiebot65\n  Lylambda\n  MastroPino\n  Matttbe\n\
<b>Translations :</b>\n  Fabounet\n  Ppmt \n  Jiro Kawada (Kawaji)\n  BiAji\n  Mattia Tavernini (Maathias)\n  Peter Thornqvist\n  Yannis Kaskamanidis\n  Eduardo Mucelli\n");
	_cairo_dock_add_about_page (pNoteBook,
		_("Support"),
		"<b>Installation script and web hosting :</b>\n  Mav\n\
<b>Site (glx-dock.org) :</b>\n  Necropotame\n  Matttbe\n  Tdey\n\
<b>LaunchPad :</b>\n  Matttbe\n  Mav\n\
<b>Suggestions/Comments/Beta-Testers :</b>\n  AuraHxC\n  Chilperik\n  Cybergoll\n  Damster\n  Djoole\n  Glattering\n  Franksuse64\n  Mav\n  Necropotame\n  Nochka85\n  Ppmt\n  RavanH\n  Rhinopierroce\n  Rom1\n  Sombrero\n  Vilraleur");
	
	gtk_widget_show_all (pDialog);
	///gtk_window_set_position (GTK_WINDOW (pDialog), GTK_WIN_POS_CENTER_ALWAYS);  // un GTK_WIN_POS_CENTER simple ne marche pas, probablement parceque la fenetre n'est pas encore realisee. le 'always' ne pose pas de probleme, puisqu'on ne peut pas redimensionner le dialogue.
	
	gtk_window_resize (GTK_WINDOW (pDialog),
		MIN (CAIRO_DOCK_ABOUT_WIDTH, g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]),
		MIN (CAIRO_DOCK_ABOUT_HEIGHT, g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - (g_pMainDock && g_pMainDock->container.bIsHorizontal ? g_pMainDock->iMaxDockHeight : 0)));
	
	gtk_window_set_keep_above (GTK_WINDOW (pDialog), TRUE);
	gtk_dialog_run (GTK_DIALOG (pDialog));
	gtk_widget_destroy (pDialog);
}

static void _launch_url (const gchar *cURL)
{
	if  (! cairo_dock_fm_launch_uri (cURL))
	{
		gchar *cCommand = g_strdup_printf ("\
which xdg-open > /dev/null && xdg-open %s || \
which firefox > /dev/null && firefox %s || \
which konqueror > /dev/null && konqueror %s || \
which iceweasel > /dev/null && konqueror %s || \
which opera > /dev/null && opera %s ",
			cURL,
			cURL,
			cURL,
			cURL,
			cURL);  // pas super beau mais efficace ^_^
		int r = system (cCommand);
		g_free (cCommand);
	}
}
static void _cairo_dock_show_third_party_applets (GtkMenuItem *pMenuItem, gpointer data)
{
	gchar *cAdress = g_strdup_printf (CAIRO_DOCK_PLUGINS_EXTRAS_URL"/%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion);
	_launch_url (cAdress);
	g_free (cAdress);
}

static void _cairo_dock_present_help (GtkMenuItem *pMenuItem, gpointer data)
{
	cairo_dock_show_module_gui ("Help");
}

static void _cairo_dock_quick_hide (GtkMenuItem *pMenuItem, CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	pDock->bMenuVisible = FALSE;
	cairo_dock_quick_hide_all_docks ();
}

static void _cairo_dock_add_autostart (GtkMenuItem *pMenuItem, gpointer data)
{
	gchar *cCairoAutoStartDirPath = g_strdup_printf ("%s/.config/autostart", g_getenv ("HOME"));
	if (! g_file_test (cCairoAutoStartDirPath, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir (cCairoAutoStartDirPath, 7*8*8+5*8+5) != 0)
		{
			cd_warning ("couldn't create directory %s", cCairoAutoStartDirPath);
			g_free (cCairoAutoStartDirPath);
			return ;
		}
	}
	gchar *cCommand = g_strdup_printf ("cp '/usr/share/applications/cairo-dock%s.desktop' '%s'", (g_bForceCairo ? "-cairo" : ""), cCairoAutoStartDirPath);
	int r = system (cCommand);
	g_free (cCommand);
	g_free (cCairoAutoStartDirPath);
}

static void _cairo_dock_quit (GtkMenuItem *pMenuItem, CairoContainer *pContainer)
{
	//cairo_dock_on_delete (pDock->container.pWidget, NULL, pDock);
	Icon *pIcon = NULL;
	if (CAIRO_DOCK_IS_DOCK (pContainer))
		pIcon = cairo_dock_get_dialogless_icon (CAIRO_DOCK (pContainer));
	else if (CAIRO_DOCK_IS_DESKLET (pContainer))
		pIcon = CAIRO_DESKLET (pContainer)->pIcon;
	
	int answer = cairo_dock_ask_question_and_wait (_("Quit Cairo-Dock?"), pIcon, pContainer);
	cd_debug ("quit : %d (yes:%d)\n", answer, GTK_RESPONSE_YES);
	if (answer == GTK_RESPONSE_YES)
		gtk_main_quit ();
}


gboolean cairo_dock_notification_build_container_menu (gpointer *pUserData, Icon *icon, CairoContainer *pContainer, GtkWidget *menu, gboolean *bDiscardMenu)
{
	if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->iRefCount > 0)  // pas sur les sous-docks
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	if (CAIRO_DOCK_IS_DESKLET (pContainer) && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))  // pas sur les icones d'un desklet qui sont des sous-icones d'une applet (sur un desklet, on peut facilement cliquer a cote des icones)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	//\_________________________ On ajoute le sous-menu Cairo-Dock.
	GtkWidget *pMenuItem, *image;
	GdkPixbuf *pixbuf;
	pMenuItem = gtk_image_menu_item_new_with_label ("Cairo-Dock");
	pixbuf = gdk_pixbuf_new_from_file_at_size (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, 32, 32, NULL);
	image = gtk_image_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);
	gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
	
	GtkWidget *pSubMenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenu);
	
	if (! cairo_dock_is_locked ())
	{
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Configure"),
			GTK_STOCK_PREFERENCES,
			(GFunc)_cairo_dock_edit_and_reload_conf,
			pSubMenu,
			NULL);
		gtk_widget_set_tooltip_text (pMenuItem, _("Configure behaviour, appearance, and applets."));
		
		if (CAIRO_DOCK_IS_DOCK (pContainer) && ! CAIRO_DOCK (pContainer)->bIsMainDock && CAIRO_DOCK (pContainer)->iRefCount == 0)
		{
			pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Configure this dock"),
				GTK_STOCK_EXECUTE,
				(GFunc)_cairo_dock_configure_root_dock,
				pSubMenu,
				CAIRO_DOCK (pContainer));
			gtk_widget_set_tooltip_text (pMenuItem, _("Customize the position, visibility and appearance of this main dock."));
		}
		
		if (! cairo_dock_theme_manager_is_integrated ())
		{
			pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Manage themes"),
				CAIRO_DOCK_SHARE_DATA_DIR"/icon-appearance.svg",
				(GFunc)_cairo_dock_initiate_theme_management,
				pSubMenu,
				NULL);
			gtk_widget_set_tooltip_text (pMenuItem, _("Choose from amongst many themes on the server or save your current theme."));
		}
		
		pMenuItem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (pSubMenu), pMenuItem);
		
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (myDocksParam.bLockIcons ? _("Unlock icons") : _("Lock icons"),
			CAIRO_DOCK_SHARE_DATA_DIR"/icon-lock-icons.svg",
			(GFunc)_cairo_dock_lock_icons,
			pSubMenu, NULL);
		gtk_widget_set_tooltip_text (pMenuItem, _("This will (un)lock the position of the icons."));
	}
	if (! g_bLocked)
	{
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (myDocksParam.bLockAll ? _("Unlock dock") : _("Lock dock"),
			CAIRO_DOCK_SHARE_DATA_DIR"/icon-lock-icons.svg",
			(GFunc)_cairo_dock_lock_all,
			pSubMenu,
			NULL);
			gtk_widget_set_tooltip_text (pMenuItem, _("This will (un)lock the whole dock."));
	}
	
	if (CAIRO_DOCK_IS_DOCK (pContainer) && ! CAIRO_DOCK (pContainer)->bAutoHide)
	{
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Quick-Hide"),
			GTK_STOCK_GOTO_BOTTOM,
			(GFunc)_cairo_dock_quick_hide,
			pSubMenu,
			CAIRO_DOCK (pContainer));
		gtk_widget_set_tooltip_text (pMenuItem, _("This will hide the dock until you hover over it with the mouse."));
	}
	
	gchar *cCairoAutoStartDirPath = g_strdup_printf ("%s/.config/autostart", g_getenv ("HOME"));
	gchar *cCairoAutoStartEntryPath = g_strdup_printf ("%s/cairo-dock.desktop", cCairoAutoStartDirPath);
	gchar *cCairoAutoStartEntryPath2 = g_strdup_printf ("%s/cairo-dock-cairo.desktop", cCairoAutoStartDirPath);
	if (! g_file_test (cCairoAutoStartEntryPath, G_FILE_TEST_EXISTS) && ! g_file_test (cCairoAutoStartEntryPath2, G_FILE_TEST_EXISTS))
	{
		cairo_dock_add_in_menu_with_stock_and_data (_("Launch Cairo-Dock on startup"),
			GTK_STOCK_ADD,
			(GFunc)_cairo_dock_add_autostart,
			pSubMenu,
			NULL);
	}
	g_free (cCairoAutoStartEntryPath);
	g_free (cCairoAutoStartEntryPath2);
	g_free (cCairoAutoStartDirPath);
	
	pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Help"),
		GTK_STOCK_HELP,
		(GFunc)_cairo_dock_present_help,
		pSubMenu,
		NULL);
	gtk_widget_set_tooltip_text (pMenuItem, _("There are no problems, only solutions (and a lot of useful hints!)"));
	
	pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Get more applets!"),
		GTK_STOCK_ADD,
		(GFunc)_cairo_dock_show_third_party_applets,
		pSubMenu,
		NULL);
	gtk_widget_set_tooltip_text (pMenuItem, _("Third-party applets provide integration with many programs, like Pidgin"));
	
	cairo_dock_add_in_menu_with_stock_and_data (_("About"),
		GTK_STOCK_ABOUT,
		(GFunc)_cairo_dock_about,
		pSubMenu,
		pContainer);
	
	if (! g_bLocked)
	{
		cairo_dock_add_in_menu_with_stock_and_data (_("Quit"),
			GTK_STOCK_QUIT,
			(GFunc)_cairo_dock_quit,
			pSubMenu,
			pContainer);
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


  ///////////////////////////////////////////////////////////////////
 /////////// LES OPERATIONS SUR LES LANCEURS ///////////////////////
///////////////////////////////////////////////////////////////////

#define _add_entry_in_menu(cLabel, gtkStock, pCallBack, pSubMenu) cairo_dock_add_in_menu_with_stock_and_data (cLabel, gtkStock, (GFunc) (pCallBack), pSubMenu, data)

static void _cairo_dock_remove_launcher (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	
	const gchar *cName = (icon->cInitialName != NULL ? icon->cInitialName : icon->cName);
	if (cName == NULL)
	{
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
			cName = _("separator");
		else
			cName = "no name";
	}
	gchar *question = g_strdup_printf (_("You're about to remove this icon (%s) from the dock. Are you sure?"), cName);
	int answer = cairo_dock_ask_question_and_wait (question, icon, CAIRO_CONTAINER (pDock));
	g_free (question);
	if (answer != GTK_RESPONSE_YES)
		return ;
	
	if (icon->pSubDock != NULL)  // on bazarde le sous-dock.
	{
		gboolean bDestroyIcons = TRUE;
		if (icon->pSubDock->icons != NULL)  // on propose de repartir les icones de son sous-dock dans le dock principal.
		{
			int answer = cairo_dock_ask_question_and_wait (_("Do you want to re-dispatch the icons contained inside this container into the dock?\n(otherwise they will be destroyed)"), icon, CAIRO_CONTAINER (pDock));
			g_return_if_fail (answer != GTK_RESPONSE_NONE);
			if (answer == GTK_RESPONSE_YES)
				bDestroyIcons = FALSE;
		}
		if (!bDestroyIcons)
		{
			const gchar *cDockName = cairo_dock_search_dock_name (pDock);
			cairo_dock_remove_icons_from_dock (icon->pSubDock, pDock, cDockName);
		}
		cairo_dock_destroy_dock (icon->pSubDock, (CAIRO_DOCK_IS_APPLI (icon) && icon->cClass != NULL ? icon->cClass : icon->cName));
		icon->pSubDock = NULL;
	}
	
	cairo_dock_trigger_icon_removal_from_dock (icon);
}

static void _cairo_dock_create_launcher (Icon *icon, CairoDock *pDock, CairoDockDesktopFileType iLauncherType)
{
	//\___________________ On determine l'ordre d'insertion suivant l'endroit du clique.
	double fOrder;
	if (icon != NULL)
	{
		if (pDock->container.iMouseX < icon->fDrawX + icon->fWidth * icon->fScale / 2)  // a gauche.
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
	
	//\___________________ On cree et on charge l'icone a partir d'un des templates.
	Icon *pNewIcon = cairo_dock_add_new_launcher_by_type (iLauncherType, pDock, fOrder, (icon ? icon->iGroup : CAIRO_DOCK_LAUNCHER));
	if (pNewIcon == NULL)
	{
		cd_warning ("Couldn't create create the icon.\nCheck that you have writing permissions on ~/.config/cairo-dock and its sub-folders");
		return ;
	}
	
	//\___________________ On ouvre automatiquement l'IHM pour permettre de modifier ses champs.
	if (iLauncherType != CAIRO_DOCK_DESKTOP_FILE_FOR_SEPARATOR)  // inutile pour un separateur.
		cairo_dock_show_items_gui (pNewIcon, NULL, NULL, -1);
}

static void cairo_dock_add_launcher (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	_cairo_dock_create_launcher (icon, pDock, CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER);
}

static void cairo_dock_add_sub_dock (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	_cairo_dock_create_launcher (icon, pDock, CAIRO_DOCK_DESKTOP_FILE_FOR_CONTAINER);
}

static void cairo_dock_add_main_dock (GtkMenuItem *pMenuItem, gpointer *data)
{
	gchar *cDockName = cairo_dock_add_root_dock_config ();
	CairoDock *pDock = cairo_dock_create_dock (cDockName, NULL);
	cairo_dock_reload_one_root_dock (cDockName, pDock);
	
	cairo_dock_gui_trigger_reload_items ();  // pas de signal "new_dock"
	
	cairo_dock_show_general_message (_("The new dock has been created.\nNow move some launchers or applets into it by right-clicking on the icon -> move to another dock"), 10000);  // on le place pas sur le nouveau dock, car sa fenetre n'est pas encore bien placee (0,0).
}

static void cairo_dock_add_separator (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	_cairo_dock_create_launcher (icon, pDock, CAIRO_DOCK_DESKTOP_FILE_FOR_SEPARATOR);
}

static void _cairo_dock_modify_launcher (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	
	if (icon->cDesktopFileName == NULL || strcmp (icon->cDesktopFileName, "none") == 0)
	{
		cairo_dock_show_temporary_dialog_with_icon (_("Sorry, this icon doesn't have a configuration file."), icon, CAIRO_CONTAINER (pDock), 4000, "same icon");
		return ;
	}
	
	cairo_dock_show_items_gui (icon, NULL, NULL, -1);
}

static void _cairo_dock_move_launcher_to_dock (GtkMenuItem *pMenuItem, const gchar *cDockName)
{
	Icon *pIcon = g_object_get_data (G_OBJECT (pMenuItem), "launcher");
	
	//\_________________________ on cree si besoin le fichier de conf d'un nouveau dock racine.
	gchar *cValidDockName;
	if (cDockName == NULL)  // nouveau dock
	{
		cValidDockName = cairo_dock_add_root_dock_config ();
	}
	else
	{
		cValidDockName = g_strdup (cDockName);
	}
	
	//\_________________________ on met a jour le fichier de conf de l'icone.
	gchar *cCurrentDockName = pIcon->cParentDockName;
	pIcon->cParentDockName = NULL;
	cairo_dock_update_icon_s_container_name (pIcon, cValidDockName);
	g_free (pIcon->cParentDockName);
	pIcon->cParentDockName = cCurrentDockName;
	
	//\_________________________ on recharge l'icone, ce qui va creer le dock.
	if ((CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
		|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
	&& pIcon->cDesktopFileName != NULL)  // user icon.
	{
		cairo_dock_reload_launcher (pIcon);
	}
	else if (CAIRO_DOCK_IS_APPLET (pIcon))
	{
		cairo_dock_reload_module_instance (pIcon->pModuleInstance, TRUE);  // TRUE <=> reload config.
	}
	
	CairoDock *pNewDock = cairo_dock_search_dock_from_name (cValidDockName);
	if (pNewDock && pNewDock->iRefCount == 0 && pNewDock->icons && pNewDock->icons->next == NULL)  // le dock vient d'etre cree avec cette icone.
		cairo_dock_show_general_message (_("The new dock has been created.\nYou can customize it by right-clicking on it -> cairo-dock -> configure this dock."), 8000);  // on le place pas sur le nouveau dock, car sa fenetre n'est pas encore bien placee (0,0).
	g_free (cValidDockName);
}

static void _add_one_dock_to_menu (const gchar *cName, CairoDock *pDock, GtkWidget *pMenu)
{
	// on ne prend que les sous-docks utilisateur.
	Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
	if (pPointingIcon && ! CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pPointingIcon))
		return;
	// on elimine le dock courant.
	Icon *pIcon = g_object_get_data (G_OBJECT (pMenu), "launcher");
	if (strcmp (pIcon->cParentDockName, cName) == 0)
		return;
	// on elimine le sous-dock.
	if (pIcon->pSubDock != NULL && pIcon->pSubDock == pDock)
		return;
	// On definit un nom plus parlant.
	gchar *cUserName = cairo_dock_get_readable_name_for_fock (pDock);
	// on rajoute une entree pour le dock.
	GtkWidget *pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (cUserName ? cUserName : cName, NULL, (GFunc)_cairo_dock_move_launcher_to_dock, pMenu, (gpointer)cName);
	g_object_set_data (G_OBJECT (pMenuItem), "launcher", pIcon);
	g_free (cUserName);
}

  //////////////////////////////////////////////////////////////////
 /////////// LES OPERATIONS SUR LES APPLETS ///////////////////////
//////////////////////////////////////////////////////////////////

static void _cairo_dock_initiate_config_module (GtkMenuItem *pMenuItem, gpointer *data)
{
	cd_debug ("%s ()\n", __func__);
	Icon *icon = data[0];
	CairoContainer *pContainer= data[1];
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module.
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	//cairo_dock_show_module_instance_gui (icon->pModuleInstance, -1);
	cairo_dock_show_items_gui (icon, NULL, NULL, -1);
}

static void _cairo_dock_detach_module (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoContainer *pContainer= data[1];
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module !
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	cairo_dock_detach_module_instance (icon->pModuleInstance);
}

static void _cairo_dock_remove_module_instance (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoContainer *pContainer= data[1];
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module !
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	gchar *question = g_strdup_printf (_("You're about to remove this applet (%s) from the dock. Are you sure?"), icon->pModuleInstance->pModule->pVisitCard->cModuleName);
	int answer = cairo_dock_ask_question_and_wait (question, icon, CAIRO_CONTAINER (pContainer));
	if (answer == GTK_RESPONSE_YES)
	{
		///if (icon->pModuleInstance->pModule->pInstancesList->next == NULL)  // plus d'instance du module apres ca.
		///	cairo_dock_deactivate_module_in_gui (icon->pModuleInstance->pModule->pVisitCard->cModuleName);
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

  /////////////////////////////////////////////////////////////////
 /////////// LES OPERATIONS SUR LES APPLIS ///////////////////////
/////////////////////////////////////////////////////////////////

static void _cairo_dock_close_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (CAIRO_DOCK_IS_APPLI (icon))
		cairo_dock_close_xwindow (icon->Xid);
}
static void _show_image_preview (GtkFileChooser *pFileChooser, GtkImage *pPreviewImage)
{
	gchar *cFileName = gtk_file_chooser_get_preview_filename (pFileChooser);
	if (cFileName == NULL)
		return ;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cFileName, 64, 64, NULL);
	g_free (cFileName);
	if (pixbuf != NULL)
	{
		gtk_image_set_from_pixbuf (pPreviewImage, pixbuf);
		gdk_pixbuf_unref (pixbuf);
		gtk_file_chooser_set_preview_widget_active (pFileChooser, TRUE);
	}
	else
		gtk_file_chooser_set_preview_widget_active (pFileChooser, FALSE);
}
static void _cairo_dock_set_custom_appli_icon (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (! CAIRO_DOCK_IS_APPLI (icon))
		return;
	
	GtkWidget* pFileChooserDialog = gtk_file_chooser_dialog_new (
		"Pick up an image",
		GTK_WINDOW (pDock->container.pWidget),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_OK,
		GTK_RESPONSE_OK,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_CANCEL,
		NULL);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (pFileChooserDialog), "~/.config/cairo-dock/current_theme/icons");
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (pFileChooserDialog), FALSE);
	
	GtkWidget *pPreviewImage = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (pFileChooserDialog), pPreviewImage);
	g_signal_connect (GTK_FILE_CHOOSER (pFileChooserDialog), "update-preview", G_CALLBACK (_show_image_preview), pPreviewImage);
	
	gtk_widget_show (pFileChooserDialog);
	int answer = gtk_dialog_run (GTK_DIALOG (pFileChooserDialog));
	if (answer == GTK_RESPONSE_OK)
	{
		if (myTaskbarParam.cOverwriteException != NULL && icon->cClass != NULL)  // si cette classe etait definie pour ne pas ecraser l'icone X, on le change, sinon l'action utilisateur n'aura aucun impact, ce sera troublant.
		{
			gchar **pExceptions = g_strsplit (myTaskbarParam.cOverwriteException, ";", -1);
			
			int i, j = -1;
			for (i = 0; pExceptions[i] != NULL; i ++)
			{
				if (j == -1 && strcmp (pExceptions[i], icon->cClass) == 0)
				{
					g_free (pExceptions[i]);
					pExceptions[i] = NULL;
					j = i;
				}
			}  // apres la boucle, i = nbre d'elements, j = l'element qui a ete enleve.
			if (j != -1)  // un element a ete enleve.
			{
				cd_warning ("The class '%s' was explicitely set up to use the X icon, we'll change this behavior automatically.", icon->cClass);
				if (j < i - 1)  // ce n'est pas le dernier
				{
					pExceptions[j] = pExceptions[i-1];
					pExceptions[i-1] = NULL;
				}
				
				myTaskbarParam.cOverwriteException = g_strjoinv (";", pExceptions);
				cairo_dock_set_overwrite_exceptions (myTaskbarParam.cOverwriteException);
				
				GKeyFile *pKeyFile = cairo_dock_open_key_file (g_cConfFile);
				if (pKeyFile != NULL)
				{
					g_key_file_set_string (pKeyFile, "TaskBar", "overwrite exception", myTaskbarParam.cOverwriteException);
					cairo_dock_write_keys_to_file (pKeyFile, g_cConfFile);
					
					g_key_file_free (pKeyFile);
				}
			}
			g_strfreev (pExceptions);
		}
		
		gchar *cFilePath = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pFileChooserDialog));
		cairo_dock_set_custom_icon_on_appli (cFilePath, icon, CAIRO_CONTAINER (pDock));
		g_free (cFilePath);
	}
	gtk_widget_destroy (pFileChooserDialog);
}
static void _cairo_dock_remove_custom_appli_icon (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (! CAIRO_DOCK_IS_APPLI (icon))
		return;
	gchar *cCustomIcon = g_strdup_printf ("%s/%s.png", g_cCurrentIconsPath, icon->cClass);
	if (!g_file_test (cCustomIcon, G_FILE_TEST_EXISTS))
	{
		g_free (cCustomIcon);
		cCustomIcon = g_strdup_printf ("%s/%s.svg", g_cCurrentIconsPath, icon->cClass);
		if (!g_file_test (cCustomIcon, G_FILE_TEST_EXISTS))
		{
			g_free (cCustomIcon);
			cCustomIcon = NULL;
		}
	}
	if (cCustomIcon != NULL)
	{
		gchar *cCommand = g_strdup_printf ("rm -f \"%s\"", cCustomIcon);
		int r = system (cCommand);
		g_free (cCommand);
		cairo_dock_reload_icon_image (icon, CAIRO_CONTAINER (pDock));
		cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pDock));
	}
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
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cairo_dock_minimize_xwindow (icon->Xid);
	}
}

static void _cairo_dock_show_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cairo_dock_show_xwindow (icon->Xid);
	}
}

static void _cairo_dock_launch_new (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (icon->cCommand != NULL)
	{
		cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
		cairo_dock_notify_on_object (CAIRO_CONTAINER (pDock), NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
	}
}

static void _cairo_dock_make_launcher_from_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	g_return_if_fail (icon->Xid != 0 && icon->cClass != NULL);
	
	// on trouve le .desktop du programme.
	cd_debug ("%s (%s)\n", __func__, icon->cClass);
	gchar *cDesktopFilePath = g_strdup_printf ("/usr/share/applications/%s.desktop", icon->cClass);
	if (! g_file_test (cDesktopFilePath, G_FILE_TEST_EXISTS))  // on n'a pas trouve la, on cherche chez KDE.
	{
		g_free (cDesktopFilePath);
		cDesktopFilePath = g_strdup_printf ("/usr/share/applications/kde4/%s.desktop", icon->cClass);
		if (! g_file_test (cDesktopFilePath, G_FILE_TEST_EXISTS))  // toujours rien, on utilise locate.
		{
			g_free (cDesktopFilePath);
			cDesktopFilePath = NULL;
			
			gchar *cCommand = g_strdup_printf ("locate /%s.desktop --limit=1 -i", icon->cClass);
			gchar *cResult = cairo_dock_launch_command_sync (cCommand);
			if (cResult != NULL && *cResult != '\0')
			{
				if (cResult[strlen (cResult) - 1] == '\n')
					cResult[strlen (cResult) - 1] = '\0';
				cDesktopFilePath = cResult;
				cResult = NULL;
			}
			// else chercher un desktop qui contiennent command=class...
			g_free (cResult);
		}
	}
	
	// on cree un nouveau lanceur a partir de la classe.
	if (cDesktopFilePath != NULL)
	{
		cd_message ("found desktop file : %s\n", cDesktopFilePath);
		cairo_dock_add_new_launcher_by_uri (cDesktopFilePath, g_pMainDock, CAIRO_DOCK_LAST_ORDER);  // on l'ajoute dans le main dock.
	}
	else
	{
		cairo_dock_show_temporary_dialog_with_default_icon (_("Sorry, couldn't find the corresponding description file.\nConsider dragging and dropping the launcher from the Applications Menu."), icon, CAIRO_CONTAINER (pDock), 8000);
	}
	g_free (cDesktopFilePath);
}

static void _cairo_dock_maximize_appli (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cairo_dock_maximize_xwindow (icon->Xid, ! icon->bIsMaximized);
	}
}

static void _cairo_dock_set_appli_fullscreen (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cairo_dock_set_xwindow_fullscreen (icon->Xid, ! icon->bIsFullScreen);
	}
}

static void _cairo_dock_move_appli_to_current_desktop (GtkMenuItem *pMenuItem, gpointer *data)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cairo_dock_move_window_to_current_desktop (icon);
		if (!icon->bIsHidden)
			cairo_dock_show_xwindow (icon->Xid);
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
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cairo_dock_move_window_to_desktop (icon, iDesktopNumber, iViewPortNumberX, iViewPortNumberY);
	}
}

static void _cairo_dock_change_window_above (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		gboolean bIsAbove=FALSE, bIsBelow=FALSE;
		cairo_dock_xwindow_is_above_or_below (icon->Xid, &bIsAbove, &bIsBelow);
		cairo_dock_set_xwindow_above (icon->Xid, ! bIsAbove);
	}
}

  //////////////////////////////////////////////////////////////////
 ///////////// LES OPERATIONS SUR LES CLASSES /////////////////////
//////////////////////////////////////////////////////////////////

static void _cairo_dock_show_class (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	g_return_if_fail (icon->pSubDock != NULL);
	
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			cairo_dock_show_xwindow (pIcon->Xid);
		}
	}
}

static void _cairo_dock_minimize_class (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	g_return_if_fail (icon->pSubDock != NULL);
	
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			cairo_dock_minimize_xwindow (pIcon->Xid);
		}
	}
}

static void _cairo_dock_close_class (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	g_return_if_fail (icon->pSubDock != NULL);
	
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			cairo_dock_close_xwindow (pIcon->Xid);
		}
	}
}

static void _cairo_dock_move_class_to_desktop (GtkMenuItem *pMenuItem, gpointer *user_data)
{
	gpointer *data = user_data[0];
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	g_return_if_fail (icon->pSubDock != NULL);
	int iDesktopNumber = GPOINTER_TO_INT (user_data[1]);
	int iViewPortNumberY = GPOINTER_TO_INT (user_data[2]);
	int iViewPortNumberX = GPOINTER_TO_INT (user_data[3]);
	cd_message ("%s (%d;%d;%d)", __func__, iDesktopNumber, iViewPortNumberX, iViewPortNumberY);
	
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			cairo_dock_move_window_to_desktop (pIcon, iDesktopNumber, iViewPortNumberX, iViewPortNumberY);
		}
	}
}

static void _cairo_dock_move_class_to_current_desktop (GtkMenuItem *pMenuItem, gpointer *data)
{
	Icon *icon = data[0];
	CairoDock *pDock = data[1];
	g_return_if_fail (icon->pSubDock != NULL);
	
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			cairo_dock_move_window_to_current_desktop (pIcon);
		}
	}
}

  ///////////////////////////////////////////////////////////////////
 ///////////////// LES OPERATIONS SUR LES DESKLETS /////////////////
///////////////////////////////////////////////////////////////////

static inline void _cairo_dock_set_desklet_accessibility (CairoDesklet *pDesklet, CairoDeskletVisibility iVisibility)
{
	cairo_dock_set_desklet_accessibility (pDesklet, iVisibility, TRUE);  // TRUE <=> save state in conf.
	cairo_dock_gui_trigger_update_desklet_visibility (pDesklet);
}
static void _cairo_dock_keep_below (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	CairoDesklet *pDesklet = data[1];
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_KEEP_BELOW);
}

static void _cairo_dock_keep_normal (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	CairoDesklet *pDesklet = data[1];
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_NORMAL);
}

static void _cairo_dock_keep_above (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	CairoDesklet *pDesklet = data[1];
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_KEEP_ABOVE);
}

static void _cairo_dock_keep_on_widget_layer (GtkMenuItem *pMenuItem, gpointer *data)
{
	CairoDesklet *pDesklet = data[1];
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_ON_WIDGET_LAYER);
}

static void _cairo_dock_keep_space (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	CairoDesklet *pDesklet = data[1];
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_RESERVE_SPACE);
}

static void _cairo_dock_set_on_all_desktop (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	CairoDesklet *pDesklet = data[1];
	gboolean bSticky = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem));
	cairo_dock_set_desklet_sticky (pDesklet, bSticky);
	cairo_dock_gui_trigger_update_desklet_visibility (pDesklet);
}

static void _cairo_dock_lock_position (GtkMenuItem *pMenuItem, gpointer *data)
{
	CairoDesklet *pDesklet = data[1];
	gboolean bLocked = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem));
	cairo_dock_lock_desklet_position (pDesklet, bLocked);
	cairo_dock_gui_trigger_update_desklet_visibility (pDesklet);
}


static void _add_desktops_entry (GtkWidget *pMenu, gboolean bAll, gpointer *data)
{
	static gpointer *s_pDesktopData = NULL;
	GtkWidget *pMenuItem, *image;
	
	if (g_desktopGeometry.iNbDesktops > 1 || g_desktopGeometry.iNbViewportX > 1 || g_desktopGeometry.iNbViewportY > 1)
	{
		int i, j, k, iDesktopCode;
		const gchar *cLabel;
		if (g_desktopGeometry.iNbDesktops > 1 && (g_desktopGeometry.iNbViewportX > 1 || g_desktopGeometry.iNbViewportY > 1))
			cLabel = bAll ? _("Move all to desktop %d - face %d") : _("Move to desktop %d - face %d");
		else if (g_desktopGeometry.iNbDesktops > 1)
			cLabel = bAll ? _("Move all to desktop %d") : _("Move to desktop %d");
		else
			cLabel = bAll ? _("Move all to face %d") : _("Move to face %d");
		GString *sDesktop = g_string_new ("");
		g_free (s_pDesktopData);
		s_pDesktopData = g_new0 (gpointer, 4 * g_desktopGeometry.iNbDesktops * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY);
		gpointer *user_data;
		Icon *icon = data[0];
		Icon *pAppli = cairo_dock_get_icon_with_Xid (icon->Xid);  // un inhibiteur ne contient pas les donnees, mais seulement la reference a l'appli, donc on recupere celle-ci pour avoir son bureau.
		
		for (i = 0; i < g_desktopGeometry.iNbDesktops; i ++)  // on range par bureau.
		{
			for (j = 0; j < g_desktopGeometry.iNbViewportY; j ++)  // puis par rangee.
			{
				for (k = 0; k < g_desktopGeometry.iNbViewportX; k ++)
				{
					if (g_desktopGeometry.iNbDesktops > 1 && (g_desktopGeometry.iNbViewportX > 1 || g_desktopGeometry.iNbViewportY > 1))
						g_string_printf (sDesktop, cLabel, i+1, j*g_desktopGeometry.iNbViewportX+k+1);
					else if (g_desktopGeometry.iNbDesktops > 1)
						g_string_printf (sDesktop, cLabel, i+1);
					else
						g_string_printf (sDesktop, cLabel, j*g_desktopGeometry.iNbViewportX+k+1);
					iDesktopCode = i * g_desktopGeometry.iNbViewportY * g_desktopGeometry.iNbViewportX + j * g_desktopGeometry.iNbViewportX + k;
					user_data = &s_pDesktopData[4*iDesktopCode];
					user_data[0] = data;
					user_data[1] = GINT_TO_POINTER (i);
					user_data[2] = GINT_TO_POINTER (j);
					user_data[3] = GINT_TO_POINTER (k);
					
					pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (sDesktop->str, NULL, (GFunc)(bAll ? _cairo_dock_move_class_to_desktop : _cairo_dock_move_appli_to_desktop), pMenu, user_data);
					if (pAppli && cairo_dock_appli_is_on_desktop (pAppli, i, k, j))
						gtk_widget_set_sensitive (pMenuItem, FALSE);
				}
			}
		}
		g_string_free (sDesktop, TRUE);
	}
}
static void _add_add_entry (GtkWidget *pMenu, gpointer *data, gboolean bAddSeparator, gboolean bAddLauncher)
{
	GtkWidget *pMenuItem = _add_entry_in_menu (_("Add"), GTK_STOCK_ADD, NULL, pMenu);
	GtkWidget *pSubMenuAdd = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenuAdd);
	
	_add_entry_in_menu (_("Sub-dock"), GTK_STOCK_ADD, cairo_dock_add_sub_dock, pSubMenuAdd);
	
	_add_entry_in_menu (_("Main dock"), GTK_STOCK_ADD, cairo_dock_add_main_dock, pSubMenuAdd);
	
	if (bAddSeparator)
		_add_entry_in_menu (_("Separator"), GTK_STOCK_ADD, cairo_dock_add_separator, pSubMenuAdd);
	
	if (bAddLauncher)
	{
		pMenuItem = _add_entry_in_menu (_("Custom launcher"), GTK_STOCK_ADD, cairo_dock_add_launcher, pSubMenuAdd);
		gtk_widget_set_tooltip_text (pMenuItem, _("Usually you would drag a launcher from the menu and drop it on the dock."));
	}
}
gboolean cairo_dock_notification_build_icon_menu (gpointer *pUserData, Icon *icon, CairoContainer *pContainer, GtkWidget *menu)
{
	static gpointer *data = NULL;
	//g_print ("%x;%x;%x\n", icon, pContainer, menu);
	if (data == NULL)
		data = g_new (gpointer, 3);
	data[0] = icon;
	data[1] = pContainer;
	data[2] = menu;
	GtkWidget *pMenuItem, *image;
	
	gchar *cLabel;
	gboolean bAddSeparator = ! (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->iRefCount > 0);  // pas sur les sous-docks.
	
	//\_________________________ Si pas d'icone dans un dock, on s'arrete la.
	if (CAIRO_DOCK_IS_DOCK (pContainer) && (icon == NULL || CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon)))
	{
		if (! cairo_dock_is_locked ())
		{
			Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (CAIRO_DOCK (pContainer), NULL);
			if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pPointingIcon))
				_add_add_entry (menu, data, TRUE, TRUE);
		}
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	}

	//\_________________________ On rajoute des actions de modifications sur le dock.
	gboolean bAddNewEntries = FALSE;
	if (! cairo_dock_is_locked () && CAIRO_DOCK_IS_DOCK (pContainer) && icon && (cairo_dock_get_icon_order (icon) == cairo_dock_get_group_order (CAIRO_DOCK_LAUNCHER) || cairo_dock_get_icon_order (icon) == cairo_dock_get_group_order (CAIRO_DOCK_APPLET)))
	{
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (CAIRO_DOCK (pContainer), NULL);
		if (!pPointingIcon || CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pPointingIcon))  // the clicked dock is a main dock or user sub-dock.
		{
			bAddNewEntries = TRUE;
			
			if ((CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)
				|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon)
				|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
			&& icon->cDesktopFileName != NULL)  // user icon
			///if (icon->cDesktopFileName != NULL && icon->cParentDockName != NULL)  // possede un .desktop.
			{
				if (bAddSeparator)
				{
					pMenuItem = gtk_separator_menu_item_new ();
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
				}
				bAddSeparator = TRUE;
				
				_add_entry_in_menu (CAIRO_DOCK_IS_USER_SEPARATOR (icon) ? _("Modify this separator") : _("Modify this launcher"), GTK_STOCK_EDIT, _cairo_dock_modify_launcher, menu);
				
				pMenuItem = _add_entry_in_menu (CAIRO_DOCK_IS_USER_SEPARATOR (icon) ? _("Remove this separator") : _("Remove this launcher"), GTK_STOCK_REMOVE, _cairo_dock_remove_launcher, menu);
				gtk_widget_set_tooltip_text (pMenuItem, _("You can remove a launcher by dragging it out of the dock with the mouse ."));
				
				pMenuItem = _add_entry_in_menu (_("Move to another dock"), GTK_STOCK_JUMP_TO, NULL, menu);
				GtkWidget *pSubMenuDocks = gtk_menu_new ();
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenuDocks);
				g_object_set_data (G_OBJECT (pSubMenuDocks), "launcher", icon);
				pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("New main dock"), GTK_STOCK_NEW, (GFunc)_cairo_dock_move_launcher_to_dock, pSubMenuDocks, NULL);
				g_object_set_data (G_OBJECT (pMenuItem), "launcher", icon);
				cairo_dock_foreach_docks ((GHFunc) _add_one_dock_to_menu, pSubMenuDocks);
			}
		}
	}
	
	//\_________________________ On rajoute les actions sur les applis.
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		if (bAddSeparator)
		{
			pMenuItem = gtk_separator_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		}
		bAddSeparator = TRUE;
		
		//\_________________________ On rajoute les actions supplementaires sur les icones d'applis.
		Icon *pAppli = cairo_dock_get_icon_with_Xid (icon->Xid);  // un inhibiteur ne contient pas les donnees, mais seulement la reference a l'appli, donc on recupere celle-ci pour avoir son etat.
		
		pMenuItem = gtk_menu_item_new_with_label (_("Other actions"));
		gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
		GtkWidget *pSubMenuOtherActions = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenuOtherActions);
		
		pMenuItem = _add_entry_in_menu (_("Move to this desktop"), GTK_STOCK_JUMP_TO, _cairo_dock_move_appli_to_current_desktop, pSubMenuOtherActions);
		if (pAppli && cairo_dock_appli_is_on_current_desktop (pAppli))
			gtk_widget_set_sensitive (pMenuItem, FALSE);
		
		if (pAppli)
		{
			icon->bIsMaximized = pAppli->bIsMaximized;
			icon->bIsFullScreen = pAppli->bIsFullScreen;
		}
		else
		{
			icon->bIsMaximized = cairo_dock_xwindow_is_maximized (icon->Xid);
			icon->bIsFullScreen = cairo_dock_xwindow_is_fullscreen (icon->Xid);
		}
		
		_add_entry_in_menu (icon->bIsFullScreen ? _("Not Fullscreen") : _("Fullscreen"), icon->bIsFullScreen ? GTK_STOCK_LEAVE_FULLSCREEN : GTK_STOCK_FULLSCREEN, _cairo_dock_set_appli_fullscreen, pSubMenuOtherActions);
		
		gboolean bIsAbove=FALSE, bIsBelow=FALSE;
		cairo_dock_xwindow_is_above_or_below (icon->Xid, &bIsAbove, &bIsBelow);
		_add_entry_in_menu (bIsAbove ? _("Don't keep above") : _("Keep above"), bIsAbove ? GTK_STOCK_GOTO_BOTTOM : GTK_STOCK_GOTO_TOP, _cairo_dock_change_window_above, pSubMenuOtherActions);
		
		_add_desktops_entry (pSubMenuOtherActions, FALSE, data);
		
		if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon))
		{
			if (myTaskbarParam.bOverWriteXIcons)
			{
				gchar *cCustomIcon = g_strdup_printf ("%s/%s.png", g_cCurrentIconsPath, icon->cClass);
				if (!g_file_test (cCustomIcon, G_FILE_TEST_EXISTS))
				{
					g_free (cCustomIcon);
					cCustomIcon = g_strdup_printf ("%s/%s.svg", g_cCurrentIconsPath, icon->cClass);
					if (!g_file_test (cCustomIcon, G_FILE_TEST_EXISTS))
					{
						g_free (cCustomIcon);
						cCustomIcon = NULL;
					}
				}
				if (cCustomIcon != NULL)
				{
					_add_entry_in_menu (_("Remove custom icon"), GTK_STOCK_REMOVE, _cairo_dock_remove_custom_appli_icon, pSubMenuOtherActions);
				}
			}
			
			_add_entry_in_menu (_("Set a custom icon"), GTK_STOCK_SELECT_COLOR, _cairo_dock_set_custom_appli_icon, pSubMenuOtherActions);
		}
		
		_add_entry_in_menu (_("Kill"), GTK_STOCK_CANCEL, _cairo_dock_kill_appli, pSubMenuOtherActions);
		
		//\_________________________ On rajoute les actions courantes sur les icones d'applis.
		if (cairo_dock_icons_are_locked () && ! cairo_dock_class_is_inhibited (icon->cClass))  // if the class doesn't already have an inhibator somewhere.
		{
			_add_entry_in_menu (_("Make it a launcher"), GTK_STOCK_CONVERT, _cairo_dock_make_launcher_from_appli, menu);
		}
		
		_add_entry_in_menu (_("Launch a new (Shift+clic)"), GTK_STOCK_ADD, _cairo_dock_launch_new, menu);
		
		_add_entry_in_menu (_("Show"), GTK_STOCK_FIND, _cairo_dock_show_appli, menu);
		
		_add_entry_in_menu (icon->bIsMaximized ? _("Unmaximise") : _("Maximise"), icon->bIsMaximized ? CAIRO_DOCK_SHARE_DATA_DIR"/icon-restore.png" : CAIRO_DOCK_SHARE_DATA_DIR"/icon-maximize.png", _cairo_dock_maximize_appli, menu);
		
		if (! icon->bIsHidden)
		{
			if (myTaskbarParam.iActionOnMiddleClick == 2)  // minimize
				cLabel = g_strdup_printf ("%s (%s)", _("Minimise"), _("middle-click"));
			else
				cLabel = g_strdup (_("Minimise"));
			_add_entry_in_menu (cLabel, CAIRO_DOCK_SHARE_DATA_DIR"/icon-minimize.png", _cairo_dock_minimize_appli, menu);
			g_free (cLabel);
		}
		
		if (myTaskbarParam.iActionOnMiddleClick == 1)  // close
			cLabel = g_strdup_printf ("%s (%s)", _("Close"), _("middle-click"));
		else
			cLabel = g_strdup (_("Close"));
		_add_entry_in_menu (cLabel, CAIRO_DOCK_SHARE_DATA_DIR"/icon-close.png", _cairo_dock_close_appli, menu);
		g_free (cLabel);
	}
	else if (CAIRO_DOCK_IS_MULTI_APPLI (icon))
	{
		if (bAddSeparator)
		{
			pMenuItem = gtk_separator_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		}
		bAddSeparator = TRUE;
		
		//\_________________________ On rajoute les actions supplementaires sur la classe.
		pMenuItem = gtk_menu_item_new_with_label (_("Other actions"));
		gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
		GtkWidget *pSubMenuOtherActions = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenuOtherActions);
		
		_add_entry_in_menu (_("Move all to this desktop"), GTK_STOCK_JUMP_TO, _cairo_dock_move_class_to_current_desktop, pSubMenuOtherActions);
		
		_add_desktops_entry (pSubMenuOtherActions, TRUE, data);
		
		_add_entry_in_menu (_("Launch a new (Shift+clic)"), GTK_STOCK_ADD, _cairo_dock_launch_new, menu);
		
		_add_entry_in_menu (_("Show all"), GTK_STOCK_FIND, _cairo_dock_show_class, menu);

		_add_entry_in_menu (_("Minimise all"), CAIRO_DOCK_SHARE_DATA_DIR"/icon-minimize.png", _cairo_dock_minimize_class, menu);
		
		_add_entry_in_menu (_("Close all"), CAIRO_DOCK_SHARE_DATA_DIR"/icon-close.png", _cairo_dock_close_class, menu);
	}
	
	//\_________________________ On rajoute les actions sur les applets/desklets.
	if (CAIRO_DOCK_IS_APPLET (icon) || CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		Icon *pIconModule;
		if (CAIRO_DOCK_IS_APPLET (icon))
			pIconModule = icon;
		else if (CAIRO_DOCK_IS_DESKLET (pContainer))
			pIconModule = CAIRO_DESKLET (pContainer)->pIcon;
		else
			pIconModule = NULL;
		
		//\_________________________ On rajoute les actions propres a un module.
		if (! cairo_dock_is_locked () && CAIRO_DOCK_IS_APPLET (pIconModule))
		{
			if (bAddSeparator)
			{
				pMenuItem = gtk_separator_menu_item_new ();
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
			}
			bAddSeparator = TRUE;
			
			_add_entry_in_menu (_("Configure this applet"), GTK_STOCK_PROPERTIES, _cairo_dock_initiate_config_module, menu);
	
			if (pIconModule->pModuleInstance->bCanDetach)
			{
				_add_entry_in_menu (CAIRO_DOCK_IS_DOCK (pContainer) ? _("Detach this applet") : _("Return to the dock"), CAIRO_DOCK_IS_DOCK (pContainer) ? GTK_STOCK_DISCONNECT : GTK_STOCK_CONNECT, _cairo_dock_detach_module, menu);
			}
			
			_add_entry_in_menu (_("Remove this applet"), GTK_STOCK_REMOVE, _cairo_dock_remove_module_instance, menu);
			
			if (pIconModule->pModuleInstance->pModule->pVisitCard->bMultiInstance)
			{
				_add_entry_in_menu (_("Launch another instance of this applet"), GTK_STOCK_ADD, _cairo_dock_add_module_instance, menu);
			}
			
			if (CAIRO_DOCK_IS_DOCK (pContainer) && pIconModule->cParentDockName != NULL)  // sinon bien sur ca n'est pas la peine de presenter l'option (Cairo-Penguin par exemple)
			{
				pMenuItem = _add_entry_in_menu (_("Move to another dock"), GTK_STOCK_JUMP_TO, NULL, menu);
				GtkWidget *pSubMenuDocks = gtk_menu_new ();
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenuDocks);
				g_object_set_data (G_OBJECT (pSubMenuDocks), "launcher", pIconModule);
				pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("New main dock"), GTK_STOCK_NEW, (GFunc)_cairo_dock_move_launcher_to_dock, pSubMenuDocks, NULL);
				g_object_set_data (G_OBJECT (pMenuItem), "launcher", pIconModule);
				cairo_dock_foreach_docks ((GHFunc) _add_one_dock_to_menu, pSubMenuDocks);
			}
		}
	}

	//\_________________________ On rajoute les actions de positionnement d'un desklet.
	if (! cairo_dock_is_locked () && CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		if (bAddSeparator)
		{
			pMenuItem = gtk_separator_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		}
		bAddSeparator = TRUE;
		
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Visibility"), GTK_STOCK_FIND, NULL, menu, NULL);
		GtkWidget *pSubMenuAccessibility = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenuAccessibility);
		
		GSList *group = NULL;

		Window Xid = GDK_WINDOW_XID (pContainer->pWidget->window);
		/*gboolean bIsAbove=FALSE, bIsBelow=FALSE;
		cairo_dock_xwindow_is_above_or_below (Xid, &bIsAbove, &bIsBelow);  // gdk_window_get_state bugue.
		gboolean bIsUtility = cairo_dock_window_is_utility (Xid);  // gtk_window_get_type_hint me renvoie toujours 0 !
		gboolean bIsDock = (CAIRO_DESKLET (pContainer)->bSpaceReserved);
		gboolean bIsNormal = (!bIsAbove && !bIsBelow && !bIsUtility && !bIsDock);*/
		gboolean bIsSticky = cairo_dock_xwindow_is_sticky (Xid);
		CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
		CairoDeskletVisibility iVisibility = pDesklet->iVisibility;
		
		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Normal"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), iVisibility == CAIRO_DESKLET_NORMAL/*bIsNormal*/);  // on coche celui-ci par defaut, il sera decoche par les suivants eventuellement.
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_normal), data);
		
		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Always on top"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
		if (iVisibility == CAIRO_DESKLET_KEEP_ABOVE/*bIsAbove*/)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_above), data);
		
		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Always below"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
		if (iVisibility == CAIRO_DESKLET_KEEP_BELOW/*bIsBelow*/)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_below), data);
		
		if (cairo_dock_wm_can_set_on_widget_layer ())
		{
			pMenuItem = gtk_radio_menu_item_new_with_label(group, "Widget Layer");
			group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
			gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
			if (iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER/*bIsUtility*/)
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
			g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_on_widget_layer), data);
			gtk_widget_set_tooltip_text (pMenuItem, _("Set behaviour in Compiz to: (name=cairo-dock & type=utility)"));
		}
		
		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Reserve space"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
		if (iVisibility == CAIRO_DESKLET_RESERVE_SPACE/*bIsDock*/)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_space), data);
		
		pMenuItem = gtk_check_menu_item_new_with_label(_("On all desktops"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		if (bIsSticky)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_set_on_all_desktop), data);
		
		pMenuItem = gtk_check_menu_item_new_with_label(_("Lock position"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		if (CAIRO_DESKLET (pContainer)->bPositionLocked)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_lock_position), data);
	}
	
	//\_________________________ On rajoute les actions d'ajout de nouveaux elements.
	if (bAddNewEntries)
	{
		if (bAddSeparator)
		{
			pMenuItem = gtk_separator_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		}
		_add_add_entry (menu, data, TRUE, cairo_dock_get_icon_order (icon) == cairo_dock_get_group_order (CAIRO_DOCK_LAUNCHER));
	}

	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

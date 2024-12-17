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
#include <glib/gstdio.h>  // g_mkdir/g_remove

#include "config.h"
#include "gldi-icon-names.h"
#include "cairo-dock-animations.h"  // cairo_dock_trigger_icon_removal_from_dock
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-module-instance-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_update_conf_file
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_sync
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dialog-factory.h"  // gldi_dialog_show_*
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-user-interaction.h"  // set_custom_icon_on_appli
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-gui-commons.h"
#include "cairo-dock-applet-facility.h"  // cairo_dock_pop_up_about_applet
#include "cairo-dock-menu.h"
#include "cairo-dock-user-menu.h"
#include "cairo-dock-core.h"

#define CAIRO_DOCK_CONF_PANEL_WIDTH 1000
#define CAIRO_DOCK_CONF_PANEL_HEIGHT 600
#define CAIRO_DOCK_ABOUT_WIDTH 400
#define CAIRO_DOCK_ABOUT_HEIGHT 500
#define CAIRO_DOCK_FILE_HOST_URL "https://github.com/Cairo-Dock/cairo-dock-core" // "https://launchpad.net/cairo-dock"  // https://developer.berlios.de/project/showfiles.php?group_id=8724
#define CAIRO_DOCK_SITE_URL "https://github.com/Cairo-Dock/cairo-dock-core" // "http://glx-dock.org"  // http://cairo-dock.vef.fr
#define CAIRO_DOCK_FORUM_URL "http://forum.glx-dock.org"  // http://cairo-dock.vef.fr/bg_forumlist.php
#define CAIRO_DOCK_PAYPAL_URL "https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=UWQ3VVRB2ZTZS&lc=GB&item_name=Support%20Cairo%2dDock&currency_code=EUR&bn=PP%2dDonationsBF%3abtn_donate_LG%2egif%3aNonHosted"
#define CAIRO_DOCK_FLATTR_URL "http://flattr.com/thing/370779/Support-Cairo-Dock-development"

extern CairoDock *g_pMainDock;
extern GldiDesktopGeometry g_desktopGeometry;

extern gchar *g_cConfFile;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentIconsPath;

extern gboolean g_bLocked;
extern gboolean g_bUseOpenGL;

#define cairo_dock_icons_are_locked(...) (myDocksParam.bLockIcons || myDocksParam.bLockAll || g_bLocked)
#define cairo_dock_is_locked(...) (myDocksParam.bLockAll || g_bLocked)

#define _add_entry_in_menu(cLabel, gtkStock, pCallBack, pSubMenu, params) cairo_dock_add_in_menu_with_stock_and_data (cLabel, gtkStock, G_CALLBACK (pCallBack), pSubMenu, (gpointer)params)

struct _MenuParams {
	Icon *pIcon;
	GldiContainer *pContainer;
	// no need to store menu, it is given to all callbacks anyway
};

void _menu_destroy_notify (gpointer data, GObject*)
{
	g_free (data); // data is struct _MenuParams
}


  //////////////////////////////////////////////
 /////////// CAIRO-DOCK SUB-MENU //////////////
//////////////////////////////////////////////

static void _cairo_dock_edit_and_reload_conf (G_GNUC_UNUSED GtkMenuItem *pMenuItem, G_GNUC_UNUSED gpointer data)
{
	cairo_dock_show_main_gui ();
}

static void _cairo_dock_configure_root_dock (G_GNUC_UNUSED GtkMenuItem *pMenuItem, CairoDock *pDock)
{
	g_return_if_fail (pDock->iRefCount == 0 && ! pDock->bIsMainDock);
	
	cairo_dock_show_items_gui (NULL, CAIRO_CONTAINER (pDock), NULL, 0);
}
static void _on_answer_delete_dock (int iClickedButton, G_GNUC_UNUSED GtkWidget *pInteractiveWidget, CairoDock *pDock, G_GNUC_UNUSED CairoDialog *pDialog)
{
	if (iClickedButton == 0 || iClickedButton == -1)  // ok button or Enter.
	{
		gldi_object_delete (GLDI_OBJECT(pDock));
	}
}
static void _cairo_dock_delete_dock (G_GNUC_UNUSED GtkMenuItem *pMenuItem, CairoDock *pDock)
{
	g_return_if_fail (pDock->iRefCount == 0 && ! pDock->bIsMainDock);
	
	Icon *pIcon = cairo_dock_get_pointed_icon (pDock->icons);
	
	gldi_dialog_show_with_question (_("Delete this dock?"),
		pIcon, CAIRO_CONTAINER (pDock),
		CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON,
		(CairoDockActionOnAnswerFunc)_on_answer_delete_dock, pDock, (GFreeFunc)NULL);
}
static void _cairo_dock_initiate_theme_management (G_GNUC_UNUSED GtkMenuItem *pMenuItem, G_GNUC_UNUSED gpointer data)
{
	cairo_dock_show_themes ();
}

static void _cairo_dock_add_about_page_with_widget (GtkWidget *pNoteBook, const gchar *cPageLabel, GtkWidget *pWidget)
{
	GtkWidget *pVBox, *pScrolledWindow;
	GtkWidget *pPageLabel;
	
	pPageLabel = gtk_label_new (cPageLabel);
	pVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (pScrolledWindow), pVBox);
	gtk_notebook_append_page (GTK_NOTEBOOK (pNoteBook), pScrolledWindow, pPageLabel);
	
	gtk_box_pack_start (GTK_BOX (pVBox),
		pWidget,
		FALSE,
		FALSE,
		15);
}

static void _cairo_dock_add_about_page_with_markup (GtkWidget *pNoteBook, const gchar *cPageLabel, const gchar *cAboutText)
{
	GtkWidget *pAboutLabel = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (pAboutLabel), TRUE);
	gtk_widget_set_halign (pAboutLabel, GTK_ALIGN_START);
	gtk_widget_set_valign (pAboutLabel, GTK_ALIGN_START);
	gtk_widget_set_margin_start (pAboutLabel, 30);
	gtk_label_set_markup (GTK_LABEL (pAboutLabel), cAboutText);
	_cairo_dock_add_about_page_with_widget (pNoteBook, cPageLabel, pAboutLabel);
}
static void _cairo_dock_lock_icons (G_GNUC_UNUSED GtkMenuItem *pMenuItem, G_GNUC_UNUSED gpointer data)
{
	myDocksParam.bLockIcons = ! myDocksParam.bLockIcons;
	cairo_dock_update_conf_file (g_cConfFile,
		G_TYPE_BOOLEAN, "Accessibility", "lock icons", myDocksParam.bLockIcons,
		G_TYPE_INVALID);
}

/* Not used
static void _cairo_dock_lock_all (GtkMenuItem *pMenuItem, gpointer data)
{
	myDocksParam.bLockAll = ! myDocksParam.bLockAll;
	cairo_dock_update_conf_file (g_cConfFile,
		G_TYPE_BOOLEAN, "Accessibility", "lock all", myDocksParam.bLockAll,
		G_TYPE_INVALID);
}
*/
static void _cairo_dock_about (G_GNUC_UNUSED GtkMenuItem *pMenuItem, GldiContainer *pContainer)
{
	// build dialog
	GtkWidget *pDialog = gtk_dialog_new_with_buttons (_("About Cairo-Dock"),
		GTK_WINDOW (pContainer->pWidget),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("Close"),
		GTK_RESPONSE_CLOSE,
		NULL);

	// the dialog box is destroyed when the user responds
	g_signal_connect_swapped (pDialog,
		"response",
		G_CALLBACK (gtk_widget_destroy),
		pDialog);

	GtkWidget *pContentBox = gtk_dialog_get_content_area (GTK_DIALOG(pDialog));

	// logo + links
	GtkWidget *pHBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (pContentBox), pHBox, FALSE, FALSE, 0);

	const gchar *cImagePath = CAIRO_DOCK_SHARE_DATA_DIR"/images/"CAIRO_DOCK_LOGO;
	GtkWidget *pImage = gtk_image_new_from_file (cImagePath);
	gtk_box_pack_start (GTK_BOX (pHBox), pImage, FALSE, FALSE, 0);

	GtkWidget *pVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (pHBox), pVBox, FALSE, FALSE, 0);
	
	GtkWidget *pLink = gtk_link_button_new_with_label (CAIRO_DOCK_SITE_URL, "Cairo-Dock (2007-2024)\n version "CAIRO_DOCK_VERSION);
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	//~ pLink = gtk_link_button_new_with_label (CAIRO_DOCK_FORUM_URL, _("Community site"));
	//~ gtk_widget_set_tooltip_text (pLink, _("Problems? Suggestions? Just want to talk to us? Come on over!"));
	//~ gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	pLink = gtk_link_button_new_with_label (CAIRO_DOCK_FILE_HOST_URL, _("Development site"));
	gtk_widget_set_tooltip_text (pLink, _("Find the latest version of Cairo-Dock here !"));
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	gchar *cLink = cairo_dock_get_third_party_applets_link ();
	pLink = gtk_link_button_new_with_label (cLink, _("Get more applets!"));
	g_free (cLink);
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	gchar *cLabel = g_strdup_printf ("%s (Flattr)", _("Donate"));
	pLink = gtk_link_button_new_with_label (CAIRO_DOCK_FLATTR_URL, cLabel);
	g_free (cLabel);
	gtk_widget_set_tooltip_text (pLink, _("Support the people who spend countless hours to bring you the best dock ever."));
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	cLabel = g_strdup_printf ("%s (Paypal)", _("Donate"));
	pLink = gtk_link_button_new_with_label (CAIRO_DOCK_PAYPAL_URL, cLabel);
	g_free (cLabel);
	gtk_widget_set_tooltip_text (pLink, _("Support the people who spend countless hours to bring you the best dock ever."));
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	
	// notebook
	GtkWidget *pNoteBook = gtk_notebook_new ();
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (pNoteBook), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (pNoteBook));
	gtk_box_pack_start (GTK_BOX (pContentBox), pNoteBook, TRUE, TRUE, 0);

	// About
	/* gchar *text = g_strdup_printf ("\n\n<b>%s</b>\n\n\n"
		"<a href=\"http://glx-dock.org\">http://glx-dock.org</a>",
		_("<b>Cairo-Dock is a pretty, light and convenient interface\n"
			" to your desktop, able to replace advantageously your system panel!</b>"));
	_cairo_dock_add_about_page (pNoteBook,
		_("About"),
		text);*/
	// Development
	gchar *text = g_strdup_printf ("%s\n\n"
	"<span size=\"larger\" weight=\"bold\">%s</span>\n\n"
		"  Fabounet (Fabrice Rey)\n"
		"\t<span size=\"smaller\">%s</span>\n\n"
		"  Matttbe (Matthieu Baerts)\n\n"
		"  Daniel Kondor\n"
		"\n\n<span size=\"larger\" weight=\"bold\">%s</span>\n\n"
		"  Eduardo Mucelli\n"
		"  Jesuisbenjamin\n"
		"  SQP\n",
		_("Here is a list of the current developers and contributors"),
		_("Developers"),
		_("Main developer and project leader"),
		_("Contributors / Hackers"));
	_cairo_dock_add_about_page_with_markup (pNoteBook,
		_("Development"),
		text);
	g_free (text);
	// Support
		text = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n\n"
		"  Matttbe\n"
		"  Mav\n"
		"  Necropotame\n"
		"\n\n<span size=\"larger\" weight=\"bold\">%s</span>\n\n"
		"  BobH\n"
		"  Franksuse64\n"
		"  Lylambda\n"
		"  Ppmt\n"
		"  Taiebot65\n"
		"\n\n<span size=\"larger\" weight=\"bold\">%s</span>\n\n"
		"%s",
		_("Website"),
		_("Beta-testing / Suggestions / Forum animation"),
		_("Translators for this language"),
		_("translator-credits"));
	_cairo_dock_add_about_page_with_markup (pNoteBook,
		_("Support"),
		text);
	g_free (text);
	// Thanks
		text = g_strdup_printf ("%s\n"
		"<a href=\"http://glx-dock.org/ww_page.php?p=How to help us\">%s</a>: %s\n\n"
		"\n<span size=\"larger\" weight=\"bold\">%s</span>\n\n"
		"  Augur\n"
		"  ChAnGFu\n"
		"  Ctaf\n"
		"  Mav\n"
		"  Necropotame\n"
		"  Nochka85\n"
		"  Paradoxxx_Zero\n"
		"  Rom1\n"
		"  Tofe\n"
		"  Mac Slow (original idea)\n"
		"\t<span size=\"smaller\">%s</span>\n"
		"\n\n<span size=\"larger\" weight=\"bold\">%s</span>\n\n"
		"\t<a href=\"http://glx-dock.org/userlist_messages.php\">%s</a>\n"
		"\n\n<span size=\"larger\" weight=\"bold\">%s</span>\n\n"
		"  Benoit2600\n"
		"  Coz\n"
		"  Fabounet\n"
		"  Lord Northam\n"
		"  Lylambda\n"
		"  MastroPino\n"
		"  Matttbe\n"
		"  Nochka85\n"
		"  Paradoxxx_Zero\n"
		"  Taiebot65\n",
		_("Thanks to all people that help us to improve the Cairo-Dock project.\n"
			"Thanks to all current, former and future contributors."),
		_("How to help us?"),
		_("Don't hesitate to join the project, we need you ;)"),
		_("Former contributors"),
		_("For a complete list, please have a look to BZR logs"),
		_("Users of our forum"),
		_("List of our forum's members"),
		_("Artwork"));
	_cairo_dock_add_about_page_with_markup (pNoteBook,
		_("Thanks"),
		text);
	g_free (text);

	text = gldi_get_diag_msg ();
	GtkTextView *textview = GTK_TEXT_VIEW(gtk_text_view_new ());
	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer (textview);
	gtk_text_buffer_set_text (textbuffer, text, -1);
	gtk_text_view_set_monospace (textview, TRUE);
	gtk_text_view_set_editable (textview, FALSE);

	_cairo_dock_add_about_page_with_widget (pNoteBook,
		_("Technical info"),
		GTK_WIDGET(textview));
	
	g_free (text);
	
	gtk_window_resize (GTK_WINDOW (pDialog),
		MIN (CAIRO_DOCK_ABOUT_WIDTH, gldi_desktop_get_width()),
		MIN (CAIRO_DOCK_ABOUT_HEIGHT, gldi_desktop_get_height() - (g_pMainDock && g_pMainDock->container.bIsHorizontal ? g_pMainDock->iMaxDockHeight : 0)));

	gtk_widget_show_all (pDialog);

	gtk_window_set_keep_above (GTK_WINDOW (pDialog), TRUE);
	//don't use gtk_dialog_run(), as we don't want to block the dock
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
		if (r < 0)
			cd_warning ("Not able to launch this command: %s", cCommand);
		g_free (cCommand);
	}
}
static void _cairo_dock_show_third_party_applets (G_GNUC_UNUSED GtkMenuItem *pMenuItem, G_GNUC_UNUSED gpointer data)
{
	gchar *cLink = cairo_dock_get_third_party_applets_link ();
	_launch_url (cLink);
	g_free (cLink);
}

static void _cairo_dock_present_help (G_GNUC_UNUSED GtkMenuItem *pMenuItem, G_GNUC_UNUSED gpointer data)
{
	int iMode = cairo_dock_gui_backend_get_mode ();
	if (iMode == 0)
		cairo_dock_load_user_gui_backend (1); // load the advanced mode (it seems it's currently not possible to open the Help with the Simple mode)
	cairo_dock_show_module_gui ("Help");
	if (iMode == 0)
		cairo_dock_load_user_gui_backend (0);
}

static void _cairo_dock_quick_hide (G_GNUC_UNUSED GtkMenuItem *pMenuItem, G_GNUC_UNUSED CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	///pDock->bHasModalWindow = FALSE;
	cairo_dock_quick_hide_all_docks ();
}

static void _cairo_dock_add_autostart (G_GNUC_UNUSED GtkMenuItem *pMenuItem, G_GNUC_UNUSED gpointer data)
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
	cairo_dock_copy_file ("/usr/share/applications/cairo-dock.desktop", cCairoAutoStartDirPath);
	g_free (cCairoAutoStartDirPath);
}

static void _on_answer_quit (int iClickedButton, G_GNUC_UNUSED GtkWidget *pInteractiveWidget, G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED CairoDialog *pDialog)
{
	if (iClickedButton == 0 || iClickedButton == -1)  // ok button or Enter.
	{
		gtk_main_quit ();
	}
}
static void _cairo_dock_quit (G_GNUC_UNUSED GtkMenuItem *pMenuItem, GldiContainer *pContainer)
{
	Icon *pIcon = NULL;
	if (CAIRO_DOCK_IS_DOCK (pContainer))
		pIcon = cairo_dock_get_pointed_icon (CAIRO_DOCK (pContainer)->icons);
	else if (CAIRO_DOCK_IS_DESKLET (pContainer))
		pIcon = CAIRO_DESKLET (pContainer)->pIcon;
	
	gldi_dialog_show_with_question (_("Quit Cairo-Dock?"),
		pIcon, pContainer,
		CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON,
		(CairoDockActionOnAnswerFunc) _on_answer_quit, NULL, (GFreeFunc)NULL);
}


  ////////////////////////////////////////
 /////////// ITEM SUB-MENU //////////////
////////////////////////////////////////

GtkWidget *_add_item_sub_menu (Icon *icon, GtkWidget *pMenu)
{
	const gchar *cName = NULL;
	if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon) || CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))
	{
		cName = (icon->cInitialName ? icon->cInitialName : icon->cName);
	}
	else if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) || CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
	{
		cName = cairo_dock_get_class_name (icon->cClass);  // better than the current window title.
		if (cName == NULL)
			cName = icon->cClass;
	}
	else if (CAIRO_DOCK_IS_APPLET (icon))
	{
		cName = icon->pModuleInstance->pModule->pVisitCard->cTitle;
	}
	else if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		cName = _("Separator");
	}
	else
		cName = icon->cName;

	gchar *cIconFile = NULL;
	if (CAIRO_DOCK_IS_APPLET (icon))
	{
		if (icon->cFileName != NULL)  // if possible, use the actual icon
			cIconFile = cairo_dock_search_icon_s_path (icon->cFileName, cairo_dock_search_icon_size (GTK_ICON_SIZE_LARGE_TOOLBAR));
		if (!cIconFile)  // else, use the default applet's icon.
			cIconFile = cairo_dock_search_icon_s_path (icon->pModuleInstance->pModule->pVisitCard->cIconFilePath, cairo_dock_search_icon_size (GTK_ICON_SIZE_LARGE_TOOLBAR));
	}
	else if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		if (myIconsParam.cSeparatorImage)
			cIconFile = cairo_dock_search_image_s_path (myIconsParam.cSeparatorImage);
	}
	else if (icon->cFileName != NULL)
	{
		cIconFile = cairo_dock_search_icon_s_path (icon->cFileName, cairo_dock_search_icon_size (GTK_ICON_SIZE_LARGE_TOOLBAR));
	}
	if (cIconFile == NULL && icon->cClass != NULL)
	{
		const gchar *cClassIcon = cairo_dock_get_class_icon (icon->cClass);
		if (cClassIcon)
			cIconFile = cairo_dock_search_icon_s_path (cClassIcon, cairo_dock_search_icon_size (GTK_ICON_SIZE_LARGE_TOOLBAR));
	}
	
	GtkWidget *pItemSubMenu;
	GdkPixbuf *pixbuf = NULL;
	
	if (!cIconFile)  // no icon file (for instance a class that has no icon defined in its desktop file, like gnome-setting-daemon) => use its buffer directly.
	{
		pixbuf = cairo_dock_icon_buffer_to_pixbuf (icon);
	}
	
	if (pixbuf)
	{
		GtkWidget *pMenuItem = NULL;
		pItemSubMenu = gldi_menu_add_sub_menu_full (pMenu, cName, "", &pMenuItem);
		
		GtkWidget *image = gtk_image_new_from_pixbuf (pixbuf);
		gldi_menu_item_set_image (pMenuItem, image);
		g_object_unref (pixbuf);
	}
	else
	{
		pItemSubMenu = cairo_dock_create_sub_menu (cName, pMenu, cIconFile);
	}
	
	g_free (cIconFile);
	return pItemSubMenu;
}


static double _get_next_order (Icon *icon, CairoDock *pDock)
{
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
	return fOrder;
}

static void _cairo_dock_add_launcher (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	CairoDock *pDock = CAIRO_DOCK (params->pContainer);
	double fOrder = _get_next_order (icon, pDock);
	Icon *pNewIcon = gldi_launcher_add_new (NULL, pDock, fOrder);
	if (pNewIcon == NULL)
		cd_warning ("Couldn't create create the icon.\nCheck that you have writing permissions on ~/.config/cairo-dock and its sub-folders");
	else
		cairo_dock_show_items_gui (pNewIcon, NULL, NULL, -1);  // open the config so that the user can complete its fields
}

static void _cairo_dock_add_sub_dock (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	CairoDock *pDock = CAIRO_DOCK (params->pContainer);
	double fOrder = _get_next_order (icon, pDock);
	Icon *pNewIcon = gldi_stack_icon_add_new (pDock, fOrder);
	if (pNewIcon == NULL)
		cd_warning ("Couldn't create create the icon.\nCheck that you have writing permissions on ~/.config/cairo-dock and its sub-folders");
}

static gboolean _show_new_dock_msg (gchar *cDockName)
{
	CairoDock *pDock = gldi_dock_get (cDockName);
	if (pDock)
		gldi_dialog_show_temporary_with_default_icon (_("The new dock has been created.\nNow move some launchers or applets into it by right-clicking on the icon -> move to another dock"), NULL, CAIRO_CONTAINER (pDock), 10000);
	g_free (cDockName);
	return FALSE;
}
static void _cairo_dock_add_main_dock (G_GNUC_UNUSED GtkMenuItem *pMenuItem, G_GNUC_UNUSED gpointer *data)
{
	gchar *cDockName = gldi_dock_add_conf_file ();
	gldi_dock_new (cDockName);
	
	cairo_dock_gui_trigger_reload_items ();  // we could also connect to the signal "new-object" on docks...
	
	g_timeout_add_seconds (1, (GSourceFunc)_show_new_dock_msg, cDockName);  // delai, car sa fenetre n'est pas encore bien placee (0,0).
}

static void _cairo_dock_add_separator (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	CairoDock *pDock = CAIRO_DOCK (params->pContainer);
	double fOrder = _get_next_order (icon, pDock);
	Icon *pNewIcon = gldi_separator_icon_add_new (pDock, fOrder);
	if (pNewIcon == NULL)
		cd_warning ("Couldn't create create the icon.\nCheck that you have writing permissions on ~/.config/cairo-dock and its sub-folders");
}

static void _cairo_dock_add_applet (G_GNUC_UNUSED GtkMenuItem *pMenuItem, G_GNUC_UNUSED gpointer *data)
{
	cairo_dock_show_addons ();
}

static void _add_add_entry (GtkWidget *pMenu, struct _MenuParams *params)
{
	GtkWidget *pSubMenuAdd = cairo_dock_create_sub_menu (_("Add"), pMenu, GLDI_ICON_NAME_ADD);
	
	_add_entry_in_menu (_("Sub-dock"), GLDI_ICON_NAME_ADD, _cairo_dock_add_sub_dock, pSubMenuAdd, params);
	
	_add_entry_in_menu (_("Main dock"), GLDI_ICON_NAME_ADD, _cairo_dock_add_main_dock, pSubMenuAdd, NULL);
	
	_add_entry_in_menu (_("Separator"), GLDI_ICON_NAME_ADD, _cairo_dock_add_separator, pSubMenuAdd, params);
	
	GtkWidget *pMenuItem = _add_entry_in_menu (_("Custom launcher"), GLDI_ICON_NAME_ADD, _cairo_dock_add_launcher, pSubMenuAdd, params);
	gtk_widget_set_tooltip_text (pMenuItem, _("Usually you would drag a launcher from the menu and drop it on the dock."));
	
	_add_entry_in_menu (_("Applet"), GLDI_ICON_NAME_ADD, _cairo_dock_add_applet, pSubMenuAdd, NULL);
}


  ///////////////////////////////////////////
 /////////// LAUNCHER ACTIONS //////////////
///////////////////////////////////////////

static void _on_answer_remove_icon (int iClickedButton, G_GNUC_UNUSED GtkWidget *pInteractiveWidget, Icon *icon, G_GNUC_UNUSED CairoDialog *pDialog)
{
	if (iClickedButton == 0 || iClickedButton == -1)  // ok button or Enter.
	{
		if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon)
		&& icon->pSubDock != NULL)  // remove the sub-dock's content from the theme or dispatch it in the main dock.
		{
			if (icon->pSubDock->icons != NULL)  // on propose de repartir les icones de son sous-dock dans le dock principal.
			{
				int iClickedButton = gldi_dialog_show_and_wait (_("Do you want to re-dispatch the icons contained inside this container into the dock?\n(otherwise they will be destroyed)"),
					icon, CAIRO_CONTAINER (icon->pContainer),
					CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, NULL);
				if (iClickedButton == 0 || iClickedButton == -1)  // ok button or Enter.
				{
					CairoDock *pDock = CAIRO_DOCK (icon->pContainer);
					cairo_dock_remove_icons_from_dock (icon->pSubDock, pDock);
				}
			}
		}
		cairo_dock_trigger_icon_removal_from_dock (icon);
	}
}
static void _cairo_dock_remove_launcher (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	CairoDock *pDock = CAIRO_DOCK (params->pContainer);
	
	const gchar *cName = (icon->cInitialName != NULL ? icon->cInitialName : icon->cName);
	if (cName == NULL)
	{
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
			cName = _("separator");
		else
			cName = "no name";
	}
	gchar *question = g_strdup_printf (_("You're about to remove this icon (%s) from the dock. Are you sure?"), cName);
	gldi_dialog_show_with_question (question,
			icon, CAIRO_CONTAINER (pDock),
			"same icon",
			(CairoDockActionOnAnswerFunc) _on_answer_remove_icon, icon, (GFreeFunc)NULL);
	g_free (question);
}

static void _cairo_dock_modify_launcher (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	CairoDock *pDock = CAIRO_DOCK (params->pContainer);
	
	if (icon->cDesktopFileName == NULL || strcmp (icon->cDesktopFileName, "none") == 0)
	{
		gldi_dialog_show_temporary_with_icon (_("Sorry, this icon doesn't have a configuration file."), icon, CAIRO_CONTAINER (pDock), 4000, "same icon");
		return ;
	}
	
	cairo_dock_show_items_gui (icon, NULL, NULL, -1);
}

static void _cairo_dock_move_launcher_to_dock (GtkMenuItem *pMenuItem, const gchar *cDockName)
{
	Icon *pIcon = g_object_get_data (G_OBJECT (pMenuItem), "icon-item");
	
	//\_________________________ on cree si besoin le fichier de conf d'un nouveau dock racine.
	gchar *cValidDockName;
	if (cDockName == NULL)  // nouveau dock
	{
		cValidDockName = gldi_dock_add_conf_file ();
	}
	else
	{
		cValidDockName = g_strdup (cDockName);
	}
	
	//\_________________________ on met a jour le fichier de conf de l'icone.
	gldi_theme_icon_write_container_name_in_conf_file (pIcon, cValidDockName);
	
	//\_________________________ on recharge l'icone, ce qui va creer le dock.
	if ((CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
		|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
	&& pIcon->cDesktopFileName != NULL)  // user icon.
	{
		gldi_object_reload (GLDI_OBJECT(pIcon), TRUE);  // TRUE <=> reload config.
	}
	else if (CAIRO_DOCK_IS_APPLET (pIcon))
	{
		gldi_object_reload (GLDI_OBJECT(pIcon->pModuleInstance), TRUE);  // TRUE <=> reload config.
	}
	
	CairoDock *pNewDock = gldi_dock_get (cValidDockName);
	if (pNewDock && pNewDock->iRefCount == 0 && pNewDock->icons && pNewDock->icons->next == NULL)  // le dock vient d'etre cree avec cette icone.
		gldi_dialog_show_general_message (_("The new dock has been created.\nYou can customize it by right-clicking on it -> cairo-dock -> configure this dock."), 8000);  // on le place pas sur le nouveau dock, car sa fenetre n'est pas encore bien placee (0,0).
	g_free (cValidDockName);
}

static void _cairo_dock_add_docks_sub_menu (GtkWidget *pMenu, Icon *pIcon)
{
	GtkWidget *pSubMenuDocks = cairo_dock_create_sub_menu (_("Move to another dock"), pMenu, GLDI_ICON_NAME_JUMP_TO);
	g_object_set_data (G_OBJECT (pSubMenuDocks), "icon-item", pIcon);
	GtkWidget *pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("New main dock"), GLDI_ICON_NAME_NEW, G_CALLBACK (_cairo_dock_move_launcher_to_dock), pSubMenuDocks, NULL);
	g_object_set_data (G_OBJECT (pMenuItem), "icon-item", pIcon);
	
	GList *pDocks = cairo_dock_get_available_docks_for_icon (pIcon);
	const gchar *cName;
	gchar *cUserName;
	CairoDock *pDock;
	GList *d;
	for (d = pDocks; d != NULL; d = d->next)
	{
		pDock = d->data;
		cName = gldi_dock_get_name (pDock);
		cUserName = gldi_dock_get_readable_name (pDock);
		
		GtkWidget *pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (cUserName ? cUserName : cName, NULL, G_CALLBACK (_cairo_dock_move_launcher_to_dock), pSubMenuDocks, (gpointer)cName);
		g_object_set_data (G_OBJECT (pMenuItem), "icon-item", pIcon);
		g_free (cUserName);
	}
	g_list_free (pDocks);
}

static void _cairo_dock_make_launcher_from_appli (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	CairoDock *pDock = CAIRO_DOCK (params->pContainer);
	g_return_if_fail (icon->cClass != NULL);
	
	// look for the .desktop file of the program
	cd_debug ("%s (%s)", __func__, icon->cClass);
	const gchar *cDesktopFilePath = cairo_dock_get_class_desktop_file (icon->cClass);
	Icon *result = NULL;
	
	// make a new launcher from this desktop file
	if (cDesktopFilePath != NULL)
	{
		cd_message ("found desktop file : %s", cDesktopFilePath);
		// place it after the last launcher, since the user will probably want to move this new launcher amongst the already existing ones.
		double fOrder = CAIRO_DOCK_LAST_ORDER;
		Icon *pIcon;
		GList *ic, *last_launcher_ic = NULL;
		for (ic = g_pMainDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
			|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon))
			{
				last_launcher_ic = ic;
			}
		}
		if (last_launcher_ic != NULL)
		{
			ic = last_launcher_ic;
			pIcon = ic->data;
			Icon *next_icon = (ic->next ? ic->next->data : NULL);
			if (next_icon != NULL && cairo_dock_get_icon_order (next_icon) == cairo_dock_get_icon_order (pIcon))
				fOrder = (pIcon->fOrder + next_icon->fOrder) / 2;
			else
				fOrder = pIcon->fOrder + 1;
		}
		// add in the main dock
		result = gldi_launcher_add_new_full (cDesktopFilePath, g_pMainDock, fOrder, TRUE);
	}
	if (!result)
	{
		// if a .desktop file has not been found so far, we cannot create a launcher
		// (no point in searching and getting inconsistent results, all "heuristics" for
		// finding .desktop files should be in cairo-dock-class-manager.c)
		gldi_dialog_show_temporary_with_default_icon (_("Sorry, couldn't find the corresponding description file.\nConsider dragging and dropping the launcher from the Applications Menu."), icon, CAIRO_CONTAINER (pDock), 8000);
	}
}

  //////////////////////////////////////////////////////////////////
 /////////// LES OPERATIONS SUR LES APPLETS ///////////////////////
//////////////////////////////////////////////////////////////////

static void _cairo_dock_initiate_config_module (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	cd_debug ("%s ()", __func__);
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	GldiContainer *pContainer= params->pContainer;
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module.
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	cairo_dock_show_items_gui (icon, NULL, NULL, -1);
}

static void _cairo_dock_detach_module (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	GldiContainer *pContainer= params->pContainer;
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module !
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	gldi_module_instance_detach (icon->pModuleInstance);
}

static void _on_answer_remove_module_instance (int iClickedButton, G_GNUC_UNUSED GtkWidget *pInteractiveWidget, Icon *icon, G_GNUC_UNUSED CairoDialog *pDialog)
{
	if (iClickedButton == 0 || iClickedButton == -1)  // ok button or Enter.
	{
		gldi_object_delete (GLDI_OBJECT(icon->pModuleInstance));
	}
}
static void _cairo_dock_remove_module_instance (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	GldiContainer *pContainer= params->pContainer;
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module !
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	gchar *question = g_strdup_printf (_("You're about to remove this applet (%s) from the dock. Are you sure?"), icon->pModuleInstance->pModule->pVisitCard->cTitle);
	gldi_dialog_show_with_question (question,
		icon, CAIRO_CONTAINER (pContainer),
		"same icon",
		(CairoDockActionOnAnswerFunc) _on_answer_remove_module_instance, icon, (GFreeFunc)NULL);
	g_free (question);
}

static void _cairo_dock_add_module_instance (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	GldiContainer *pContainer= params->pContainer;
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
		icon = (CAIRO_DESKLET (pContainer))->pIcon;  // l'icone cliquee du desklet n'est pas forcement celle qui contient le module !
	g_return_if_fail (CAIRO_DOCK_IS_APPLET (icon));
	
	gldi_module_add_instance (icon->pModuleInstance->pModule);
}

static void _cairo_dock_set_sensitive_quit_menu (G_GNUC_UNUSED GtkWidget *pMenuItem, GdkEventKey *pKey, GtkWidget *pQuitEntry)
{
	// pMenuItem not used because we want to only modify one entry
	if (pKey->type == GDK_KEY_PRESS &&
		(pKey->keyval == GDK_KEY_Shift_L || 
		pKey->keyval == GDK_KEY_Shift_R)) // pressed
		gtk_widget_set_sensitive (pQuitEntry, TRUE); // unlocked
	else if (pKey->state & GDK_SHIFT_MASK) // released
		gtk_widget_set_sensitive (pQuitEntry, FALSE); // locked)
}

static void _cairo_dock_launch_new (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	if (icon->pClassApp || icon->pCustomLauncher)
	{
		gldi_object_notify (params->pContainer, NOTIFICATION_CLICK_ICON, icon,
			params->pContainer, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
	}
}


  /////////////////////////////////////////
 /// BUILD CONTAINER MENU NOTIFICATION ///
/////////////////////////////////////////

static void _show_image_preview (GtkFileChooser *pFileChooser, GtkImage *pPreviewImage)
{
	gchar *cFileName = gtk_file_chooser_get_preview_filename (pFileChooser);
	if (cFileName == NULL)
		return ;
	GdkPixbuf *pixbuf = cairo_dock_load_gdk_pixbuf (cFileName, 64, 64);
	g_free (cFileName);
	if (pixbuf != NULL)
	{
		gtk_image_set_from_pixbuf (pPreviewImage, pixbuf);
		g_object_unref (pixbuf);
		gtk_file_chooser_set_preview_widget_active (pFileChooser, TRUE);
	}
	else
		gtk_file_chooser_set_preview_widget_active (pFileChooser, FALSE);
}
static void _cairo_dock_set_custom_appli_icon (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	GldiContainer *pContainer= params->pContainer;
	
	if (! CAIRO_DOCK_IS_APPLI (icon))
		return;
	
	GtkWidget* pFileChooserDialog = gtk_file_chooser_dialog_new (
		_("Pick up an image"),
		GTK_WINDOW (pContainer->pWidget),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("Ok"),
		GTK_RESPONSE_OK,
		_("Cancel"),
		GTK_RESPONSE_CANCEL,
		NULL);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (pFileChooserDialog), "/usr/share/icons");  // we could also use 'xdg-user-dir PICTURES' or /usr/share/icons or ~/.icons, but actually we have no idea where the user will want to pick the image, so let's not try to be smart.
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (pFileChooserDialog), FALSE);
	
	GtkWidget *pPreviewImage = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (pFileChooserDialog), pPreviewImage);
	g_signal_connect (GTK_FILE_CHOOSER (pFileChooserDialog), "update-preview", G_CALLBACK (_show_image_preview), pPreviewImage);

	// a filter
	GtkFileFilter *pFilter = gtk_file_filter_new ();
	gtk_file_filter_set_name (pFilter, _("Image"));
	gtk_file_filter_add_pixbuf_formats (pFilter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (pFileChooserDialog), pFilter);
	
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
				cd_warning ("The class '%s' was explicitly set up to use the X icon, we'll change this behavior automatically.", icon->cClass);
				if (j < i - 1)  // ce n'est pas le dernier
				{
					pExceptions[j] = pExceptions[i-1];
					pExceptions[i-1] = NULL;
				}
				
				myTaskbarParam.cOverwriteException = g_strjoinv (";", pExceptions);
				cairo_dock_set_overwrite_exceptions (myTaskbarParam.cOverwriteException);
				
				cairo_dock_update_conf_file (g_cConfFile,
					G_TYPE_STRING, "TaskBar", "overwrite exception", myTaskbarParam.cOverwriteException,
					G_TYPE_INVALID);
			}
			g_strfreev (pExceptions);
		}
		
		gchar *cFilePath = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pFileChooserDialog));
		cairo_dock_set_custom_icon_on_appli (cFilePath, icon, pContainer);
		g_free (cFilePath);
	}
	gtk_widget_destroy (pFileChooserDialog);
}
static void _cairo_dock_remove_custom_appli_icon (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	GldiContainer *pContainer= params->pContainer;
	
	if (! CAIRO_DOCK_IS_APPLI (icon))
		return;
	
	const gchar *cClassIcon = cairo_dock_get_class_icon (icon->cClass);
	if (cClassIcon == NULL)
		cClassIcon = icon->cClass;
	
	gchar *cCustomIcon = g_strdup_printf ("%s/%s.png", g_cCurrentIconsPath, cClassIcon);
	if (!g_file_test (cCustomIcon, G_FILE_TEST_EXISTS))
	{
		g_free (cCustomIcon);
		cCustomIcon = g_strdup_printf ("%s/%s.svg", g_cCurrentIconsPath, cClassIcon);
		if (!g_file_test (cCustomIcon, G_FILE_TEST_EXISTS))
		{
			g_free (cCustomIcon);
			cCustomIcon = NULL;
		}
	}
	if (cCustomIcon != NULL)
	{
		g_remove (cCustomIcon);
		cairo_dock_reload_icon_image (icon, pContainer);
		cairo_dock_redraw_icon (icon);
	}
}

gboolean cairo_dock_notification_build_container_menu (G_GNUC_UNUSED gpointer *pUserData, Icon *icon, GldiContainer *pContainer, GtkWidget *menu, G_GNUC_UNUSED gboolean *bDiscardMenu)
{
	struct _MenuParams *params = g_new0 (struct _MenuParams, 1);
	params->pContainer = pContainer;
	g_object_weak_ref (G_OBJECT (menu), _menu_destroy_notify, (gpointer)params);
	

	if (CAIRO_DOCK_IS_DESKLET (pContainer) && icon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))  // not on the icons of a desklet, except the applet icon (on a desklet, it's easy to click out of any icon).
		return GLDI_NOTIFICATION_LET_PASS;

	if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->iRefCount > 0)  // not on the sub-docks, except user sub-docks.
	{
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (CAIRO_DOCK (pContainer), NULL);
		if (pPointingIcon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pPointingIcon))
			return GLDI_NOTIFICATION_LET_PASS;
	}

	GtkWidget *pMenuItem;

	//\_________________________ First item is the Cairo-Dock sub-menu.
	GtkWidget *pSubMenu = cairo_dock_create_sub_menu ("Cairo-Dock", menu, CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON);

	// theme settings
	if (! cairo_dock_is_locked ())
	{
		// global settings
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Configure"),
			GLDI_ICON_NAME_PREFERENCES,
			G_CALLBACK (_cairo_dock_edit_and_reload_conf),
			pSubMenu,
			NULL);
		gtk_widget_set_tooltip_text (pMenuItem, _("Configure behaviour, appearance, and applets."));

		// root dock settings
		if (CAIRO_DOCK_IS_DOCK (pContainer) && ! CAIRO_DOCK (pContainer)->bIsMainDock && CAIRO_DOCK (pContainer)->iRefCount == 0)
		{
			pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Configure this dock"),
				GLDI_ICON_NAME_EXECUTE,
				G_CALLBACK (_cairo_dock_configure_root_dock),
				pSubMenu,
				CAIRO_DOCK (pContainer));
			gtk_widget_set_tooltip_text (pMenuItem, _("Customize the position, visibility and appearance of this main dock."));
			
			cairo_dock_add_in_menu_with_stock_and_data (_("Delete this dock"),
				GLDI_ICON_NAME_DELETE,
				G_CALLBACK (_cairo_dock_delete_dock),
				pSubMenu,
				CAIRO_DOCK (pContainer));
		}

		// themes
		if (cairo_dock_can_manage_themes ())
		{
			pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Manage themes"),
				CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-appearance.svg",
				G_CALLBACK (_cairo_dock_initiate_theme_management),
				pSubMenu,
				NULL);
			gtk_widget_set_tooltip_text (pMenuItem, _("Choose from amongst many themes on the server or save your current theme."));
		}

		// add new item
		if (CAIRO_DOCK_IS_DOCK (pContainer))
		{
			_add_add_entry (pSubMenu, params);
		}

		gldi_menu_add_separator (pSubMenu);

		// lock icons position
		pMenuItem = gtk_check_menu_item_new_with_label (_("Lock icons position"));
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (pMenuItem), myDocksParam.bLockIcons);
		gtk_menu_shell_append  (GTK_MENU_SHELL (pSubMenu), pMenuItem);
		g_signal_connect (G_OBJECT (pMenuItem), "toggled", G_CALLBACK (_cairo_dock_lock_icons), NULL);
		gtk_widget_set_tooltip_text (pMenuItem, _("This will (un)lock the position of the icons."));
	}

	// quick-hide
	if (CAIRO_DOCK_IS_DOCK (pContainer) && ! CAIRO_DOCK (pContainer)->bAutoHide)
	{
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Quick-Hide"),
			GLDI_ICON_NAME_GOTO_BOTTOM,
			G_CALLBACK (_cairo_dock_quick_hide),
			pSubMenu,
			CAIRO_DOCK (pContainer));
		gtk_widget_set_sensitive (pMenuItem, gldi_container_can_poll_screen_edge ());
		gtk_widget_set_tooltip_text (pMenuItem, _("This will hide the dock until you hover over it with the mouse."));
	}

	const gchar *cDesktopSession = g_getenv ("DESKTOP_SESSION");
	gboolean bIsCairoDockSession = cDesktopSession && g_str_has_prefix (cDesktopSession, "cairo-dock");
	if (! g_bLocked)
	{
		// auto-start
		gchar *cCairoAutoStartDirPath = g_strdup_printf ("%s/.config/autostart", g_getenv ("HOME"));
		gchar *cCairoAutoStartEntryPath = g_strdup_printf ("%s/cairo-dock.desktop", cCairoAutoStartDirPath);
		gchar *cCairoAutoStartEntryPath2 = g_strdup_printf ("%s/cairo-dock-cairo.desktop", cCairoAutoStartDirPath);
		if (! bIsCairoDockSession && ! g_file_test (cCairoAutoStartEntryPath, G_FILE_TEST_EXISTS) && ! g_file_test (cCairoAutoStartEntryPath2, G_FILE_TEST_EXISTS))
		{
			cairo_dock_add_in_menu_with_stock_and_data (_("Launch Cairo-Dock on startup"),
				GLDI_ICON_NAME_ADD,
				G_CALLBACK (_cairo_dock_add_autostart),
				pSubMenu,
				NULL);
		}
		g_free (cCairoAutoStartEntryPath);
		g_free (cCairoAutoStartEntryPath2);
		g_free (cCairoAutoStartDirPath);
		
		// third-party applets (are here to give them more visibility).
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Get more applets!"),
			GLDI_ICON_NAME_ADD,
			G_CALLBACK (_cairo_dock_show_third_party_applets),
			pSubMenu,
			NULL);
		gtk_widget_set_tooltip_text (pMenuItem, _("Third-party applets provide integration with many programs, like Pidgin"));
		
		// Help (we don't present the help if locked, because it would open the configuration window).
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Help"),
			GLDI_ICON_NAME_HELP,
			G_CALLBACK (_cairo_dock_present_help),
			pSubMenu,
			NULL);
		gtk_widget_set_tooltip_text (pMenuItem, _("There are no problems, only solutions (and a lot of useful hints!)"));
	}

	// About
	cairo_dock_add_in_menu_with_stock_and_data (_("About"),
		GLDI_ICON_NAME_ABOUT,
		G_CALLBACK (_cairo_dock_about),
		pSubMenu,
		pContainer);

	// quit
	if (! g_bLocked)
	{
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Quit"),
			GLDI_ICON_NAME_QUIT,
			G_CALLBACK (_cairo_dock_quit),
			pSubMenu,
			pContainer);
		// if we're using a Cairo-Dock session and we quit the dock we have... nothing to relaunch it!
		if (bIsCairoDockSession)
		{
			gtk_widget_set_sensitive (pMenuItem, FALSE); // locked
			gtk_widget_set_tooltip_text (pMenuItem, _("You're using a Cairo-Dock Session!\nIt's not advised to quit the dock but you can press Shift to unlock this menu entry."));
			// signal to unlock the entry (signal monitored only in the submenu)
			g_signal_connect (pSubMenu, "key-press-event", G_CALLBACK (_cairo_dock_set_sensitive_quit_menu), pMenuItem);
			g_signal_connect (pSubMenu, "key-release-event", G_CALLBACK (_cairo_dock_set_sensitive_quit_menu), pMenuItem);
		}
	}

	//\_________________________ Second item is the Icon sub-menu.
	Icon *pIcon = icon;
	if (pIcon == NULL && CAIRO_DOCK_IS_DESKLET (pContainer))  // on a desklet, the main applet icon may not be drawn; therefore we add the applet sub-menu if we clicked outside of an icon.
	{
		pIcon = CAIRO_DESKLET (pContainer)->pIcon;
	}
	params->pIcon = pIcon;

	if (pIcon != NULL && ! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pIcon))
	{
		GtkWidget *pItemSubMenu = _add_item_sub_menu (pIcon, menu);
		
		if (cairo_dock_is_locked ())
		{
			gboolean bSensitive = FALSE;
			if ( (CAIRO_DOCK_IS_APPLI (icon) || CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pIcon))
				&& (icon->pClassApp || icon->pCustomLauncher))
			{
				_add_entry_in_menu (_("Launch a new (Shift+clic)"), GLDI_ICON_NAME_ADD, _cairo_dock_launch_new, pItemSubMenu, params);
				bSensitive = TRUE;
			}
			if (CAIRO_DOCK_IS_APPLET (pIcon))
			{
				cairo_dock_add_in_menu_with_stock_and_data (_("Applet's handbook"), GLDI_ICON_NAME_ABOUT, G_CALLBACK (cairo_dock_pop_up_about_applet), pItemSubMenu, pIcon->pModuleInstance);
				bSensitive = TRUE;
			}
			gtk_widget_set_sensitive (pItemSubMenu, bSensitive);
		}
		else
		{
			if ( (CAIRO_DOCK_IS_APPLI (icon) || CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pIcon))
					&& (icon->pClassApp || icon->pCustomLauncher))
				_add_entry_in_menu (_("Launch a new (Shift+clic)"), GLDI_ICON_NAME_ADD, _cairo_dock_launch_new, pItemSubMenu, params);
			
			if ((CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
				 || CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
				 || CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
				&& icon->cDesktopFileName != NULL)  // user icon
			{
				_add_entry_in_menu (_("Edit"), GLDI_ICON_NAME_EDIT, _cairo_dock_modify_launcher, pItemSubMenu, params);
				
				pMenuItem = _add_entry_in_menu (_("Remove"), GLDI_ICON_NAME_REMOVE, _cairo_dock_remove_launcher, pItemSubMenu, params);
				gtk_widget_set_tooltip_text (pMenuItem, _("You can remove a launcher by dragging it out of the dock with the mouse ."));
				
				_cairo_dock_add_docks_sub_menu (pItemSubMenu, pIcon);
			}
			else if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (pIcon)
				|| CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pIcon))  // appli with no launcher
			{
				if (! cairo_dock_class_is_inhibited (pIcon->cClass))  // if the class doesn't already have an inhibator somewhere.
				{
					_add_entry_in_menu (_("Make it a launcher"), GLDI_ICON_NAME_NEW, _cairo_dock_make_launcher_from_appli, pItemSubMenu, params);
					
					if (!myDocksParam.bLockAll && CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon))
					{
						if (myTaskbarParam.bOverWriteXIcons)
						{
							const gchar *cClassIcon = cairo_dock_get_class_icon (icon->cClass);
							if (cClassIcon == NULL)
								cClassIcon = icon->cClass;
							
							gchar *cCustomIcon = g_strdup_printf ("%s/%s.png", g_cCurrentIconsPath, cClassIcon);
							if (!g_file_test (cCustomIcon, G_FILE_TEST_EXISTS))
							{
								g_free (cCustomIcon);
								cCustomIcon = g_strdup_printf ("%s/%s.svg", g_cCurrentIconsPath, cClassIcon);
								if (!g_file_test (cCustomIcon, G_FILE_TEST_EXISTS))
								{
									g_free (cCustomIcon);
									cCustomIcon = NULL;
								}
							}
							if (cCustomIcon != NULL)
							{
								_add_entry_in_menu (_("Remove custom icon"), GLDI_ICON_NAME_REMOVE, _cairo_dock_remove_custom_appli_icon, pItemSubMenu, params);
							}
						}
						
						_add_entry_in_menu (_("Set a custom icon"), GLDI_ICON_NAME_SELECT_COLOR, _cairo_dock_set_custom_appli_icon, pItemSubMenu, params);
					}
				}
			}
			else if (CAIRO_DOCK_IS_APPLET (pIcon))  // applet (icon or desklet) (the sub-icons have been filtered before and won't have this menu).
			{
				_add_entry_in_menu (_("Edit"), GLDI_ICON_NAME_EDIT, _cairo_dock_initiate_config_module, pItemSubMenu, params);
				
				if (CAIRO_DOCK_IS_DETACHABLE_APPLET (pIcon))
				{
					_add_entry_in_menu (CAIRO_DOCK_IS_DOCK (pContainer) ? _("Detach") : _("Return to the dock"), CAIRO_DOCK_IS_DOCK (pContainer) ? GLDI_ICON_NAME_GOTO_TOP : GLDI_ICON_NAME_GOTO_BOTTOM, _cairo_dock_detach_module, pItemSubMenu, params);
				}
				
				_add_entry_in_menu (_("Remove"), GLDI_ICON_NAME_REMOVE, _cairo_dock_remove_module_instance, pItemSubMenu, params);
				
				if (pIcon->pModuleInstance->pModule->pVisitCard->bMultiInstance)
				{
					_add_entry_in_menu (_("Duplicate"), GLDI_ICON_NAME_ADD, _cairo_dock_add_module_instance, pItemSubMenu, params);  // Launch another instance of this applet
				}
				
				if (CAIRO_DOCK_IS_DOCK (pContainer) && cairo_dock_get_icon_container(pIcon) != NULL)  // sinon bien sur ca n'est pas la peine de presenter l'option (Cairo-Penguin par exemple)
				{
					_cairo_dock_add_docks_sub_menu (pItemSubMenu, pIcon);
				}
				
				gldi_menu_add_separator (pItemSubMenu);
				
				cairo_dock_add_in_menu_with_stock_and_data (_("Applet's handbook"), GLDI_ICON_NAME_ABOUT, G_CALLBACK (cairo_dock_pop_up_about_applet), pItemSubMenu, pIcon->pModuleInstance);
			}
		}
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}



  /////////////////////////////////////////////////////////////////
 /////////// LES OPERATIONS SUR LES APPLIS ///////////////////////
/////////////////////////////////////////////////////////////////

static void _cairo_dock_close_appli (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	if (CAIRO_DOCK_IS_APPLI (params->pIcon))
		gldi_window_close (params->pIcon->pAppli);
}
static void _cairo_dock_kill_appli (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	if (CAIRO_DOCK_IS_APPLI (params->pIcon))
		gldi_window_kill (params->pIcon->pAppli);
}
static void _cairo_dock_minimize_appli (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	if (CAIRO_DOCK_IS_APPLI (params->pIcon))
	{
		gldi_window_minimize (params->pIcon->pAppli);
	}
}
static void _cairo_dock_lower_appli (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	if (CAIRO_DOCK_IS_APPLI (params->pIcon))
	{
		gldi_window_lower (params->pIcon->pAppli);
	}
}

static void _cairo_dock_show_appli (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	if (CAIRO_DOCK_IS_APPLI (params->pIcon))
	{
		gldi_window_show (params->pIcon->pAppli);
	}
}

static void _cairo_dock_maximize_appli (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	if (CAIRO_DOCK_IS_APPLI (params->pIcon))
	{
		gldi_window_maximize (params->pIcon->pAppli,
			! params->pIcon->pAppli->bIsMaximized);
	}
}

static void _cairo_dock_set_appli_fullscreen (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	if (CAIRO_DOCK_IS_APPLI (params->pIcon))
	{
		gldi_window_set_fullscreen (params->pIcon->pAppli,
			! params->pIcon->pAppli->bIsFullScreen);
	}
}

static void _cairo_dock_move_appli_to_current_desktop (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		gldi_window_move_to_current_desktop (icon->pAppli);
		if (!icon->pAppli->bIsHidden)
			gldi_window_show (icon->pAppli);
	}
}

static void _cairo_dock_move_appli_to_desktop (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *user_data)
{
	struct _MenuParams *params = (struct _MenuParams*) user_data[0];
	Icon *icon = params->pIcon;
	// CairoDock *pDock = data[1];
	int iDesktopNumber = GPOINTER_TO_INT (user_data[1]);
	int iViewPortNumberY = GPOINTER_TO_INT (user_data[2]);
	int iViewPortNumberX = GPOINTER_TO_INT (user_data[3]);
	cd_message ("%s (%d;%d;%d)", __func__, iDesktopNumber, iViewPortNumberX, iViewPortNumberY);
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		gldi_window_move_to_desktop (icon->pAppli, iDesktopNumber, iViewPortNumberX, iViewPortNumberY);
	}
}

static void _cairo_dock_change_window_above (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		gboolean bIsAbove=FALSE, bIsBelow=FALSE;
		gldi_window_is_above_or_below (icon->pAppli, &bIsAbove, &bIsBelow);
		gldi_window_set_above (icon->pAppli, ! bIsAbove);
	}
}

static void _cairo_dock_change_window_sticky (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	if (CAIRO_DOCK_IS_APPLI (params->pIcon))
	{
		gboolean bIsSticky = gldi_window_is_sticky (params->pIcon->pAppli);
		gldi_window_set_sticky (params->pIcon->pAppli, ! bIsSticky);
	}
}

static void _cairo_dock_move_class_to_desktop (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *user_data)
{
	struct _MenuParams *params = (struct _MenuParams*) user_data[0];
	Icon *icon = params->pIcon;
	// CairoDock *pDock = data[1];
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
			gldi_window_move_to_desktop (pIcon->pAppli, iDesktopNumber, iViewPortNumberX, iViewPortNumberY);
		}
	}
}

static void _free_desktops_user_data (gpointer data, GObject*)
{
	g_free (data);
}

static void _add_desktops_entry (GtkWidget *pMenu, gboolean bAll, struct _MenuParams *params)
{
	GtkWidget *pMenuItem;
	
	if (!gldi_window_manager_can_move_to_desktop ()) return;
	
	if (g_desktopGeometry.iNbDesktops > 1 || g_desktopGeometry.iNbViewportX > 1 || g_desktopGeometry.iNbViewportY > 1)
	{
		// add separator
		pMenuItem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);

		int i, j, k, iDesktopCode;
		const gchar *cLabel;
		if (g_desktopGeometry.iNbDesktops > 1 && (g_desktopGeometry.iNbViewportX > 1 || g_desktopGeometry.iNbViewportY > 1))
			cLabel = bAll ? _("Move all to desktop %d - face %d") : _("Move to desktop %d - face %d");
		else if (g_desktopGeometry.iNbDesktops > 1)
			cLabel = bAll ? _("Move all to desktop %d") : _("Move to desktop %d");
		else
			cLabel = bAll ? _("Move all to face %d") : _("Move to face %d");
		GString *sDesktop = g_string_new ("");
		gpointer *pDesktopData = g_new0 (gpointer, 4 * g_desktopGeometry.iNbDesktops * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY);
		g_object_weak_ref (G_OBJECT (pMenu), _free_desktops_user_data, pDesktopData);
		gpointer *user_data;
		Icon *icon = params->pIcon;
		GldiWindowActor *pAppli = icon->pAppli;
		
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
					user_data = &pDesktopData[4*iDesktopCode];
					user_data[0] = params;
					user_data[1] = GINT_TO_POINTER (i);
					user_data[2] = GINT_TO_POINTER (j);
					user_data[3] = GINT_TO_POINTER (k);
					
					pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (sDesktop->str, NULL, G_CALLBACK (bAll ? _cairo_dock_move_class_to_desktop : _cairo_dock_move_appli_to_desktop), pMenu, user_data);
					if (pAppli && gldi_window_is_on_desktop (pAppli, i, k, j))
						gtk_widget_set_sensitive (pMenuItem, FALSE);
				}
			}
		}
		g_string_free (sDesktop, TRUE);
	}
}


  ///////////////////////////////////////////////////////////////
 ///////////// LES OPERATIONS SUR LES CLASSES //////////////////
///////////////////////////////////////////////////////////////


struct _AppAction {
	GDesktopAppInfo *app;
	const gchar *action;
	gchar *action_name;
};

static void _menu_item_destroyed (gpointer data, GObject*)
{
	if (data)
	{
		struct _AppAction *pAction = (struct _AppAction*)data;
		if (pAction->action_name) g_free (pAction->action_name);
		g_free (pAction);
	}
}

static void _cairo_dock_launch_class_action (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer data)
{
	if (!data) return;
	struct _AppAction *pAction = (struct _AppAction*)data;
	
	// GdkAppLaunchContext will automatically use startup notify / xdg-activation,
	// allowing e.g. the app to raise itself if necessary
	GdkAppLaunchContext *context = gdk_display_get_app_launch_context (gdk_display_get_default ());
	g_desktop_app_info_launch_action (pAction->app, pAction->action, G_APP_LAUNCH_CONTEXT (context));
	g_object_unref (context); // will be kept by GIO if necessary (and we don't care about the "launched" signal in this case)
}

static void _cairo_dock_show_class (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	
	g_return_if_fail (icon->pSubDock != NULL);
	
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			gldi_window_show (pIcon->pAppli);
		}
	}
}

static void _cairo_dock_minimize_class (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	
	g_return_if_fail (icon->pSubDock != NULL);
	
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			gldi_window_minimize (pIcon->pAppli);
		}
	}
}

static void _cairo_dock_close_class (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	
	g_return_if_fail (icon->pSubDock != NULL);
	
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			gldi_window_close (pIcon->pAppli);
		}
	}
}

static void _cairo_dock_move_class_to_current_desktop (G_GNUC_UNUSED GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	Icon *icon = params->pIcon;
	
	g_return_if_fail (icon->pSubDock != NULL);
	
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			gldi_window_move_to_current_desktop (pIcon->pAppli);
		}
	}
}

  ///////////////////////////////////////////////////////////////////
 ///////////////// LES OPERATIONS SUR LES DESKLETS /////////////////
///////////////////////////////////////////////////////////////////

static inline void _cairo_dock_set_desklet_accessibility (CairoDesklet *pDesklet, CairoDeskletVisibility iVisibility)
{
	gldi_desklet_set_accessibility (pDesklet, iVisibility, TRUE);  // TRUE <=> save state in conf.
	cairo_dock_gui_update_desklet_visibility (pDesklet);
}
static void _cairo_dock_keep_below (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	CairoDesklet *pDesklet = CAIRO_DESKLET (params->pContainer);
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_KEEP_BELOW);
}

static void _cairo_dock_keep_normal (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	CairoDesklet *pDesklet = CAIRO_DESKLET (params->pContainer);
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_NORMAL);
}

static void _cairo_dock_keep_above (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	CairoDesklet *pDesklet = CAIRO_DESKLET (params->pContainer);
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_KEEP_ABOVE);
}

static void _cairo_dock_keep_on_widget_layer (GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	CairoDesklet *pDesklet = CAIRO_DESKLET (params->pContainer);
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_ON_WIDGET_LAYER);
}

static void _cairo_dock_keep_space (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	CairoDesklet *pDesklet = CAIRO_DESKLET (params->pContainer);
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem)))
		_cairo_dock_set_desklet_accessibility (pDesklet, CAIRO_DESKLET_RESERVE_SPACE);
}

static void _cairo_dock_set_on_all_desktop (GtkCheckMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	CairoDesklet *pDesklet = CAIRO_DESKLET (params->pContainer);
	gboolean bSticky = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem));
	gldi_desklet_set_sticky (pDesklet, bSticky);
	cairo_dock_gui_update_desklet_visibility (pDesklet);
}

static void _cairo_dock_lock_position (GtkMenuItem *pMenuItem, gpointer *data)
{
	struct _MenuParams *params = (struct _MenuParams*) data;
	CairoDesklet *pDesklet = CAIRO_DESKLET (params->pContainer);
	gboolean bLocked = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (pMenuItem));
	gldi_desklet_lock_position (pDesklet, bLocked);
	cairo_dock_gui_update_desklet_visibility (pDesklet);
}


  ////////////////////////////////////
 /// BUILD ICON MENU NOTIFICATION ///
////////////////////////////////////

// stuff for the buttons inside a menu-item
static gboolean _on_press_menu_item (GtkWidget* pWidget, GdkEventButton *pEvent, G_GNUC_UNUSED gpointer data)
{
	GtkWidget *hbox = gtk_bin_get_child (GTK_BIN (pWidget));
	GList *children = gtk_container_get_children (GTK_CONTAINER (hbox));
	int x = pEvent->x, y = pEvent->y;  // position of the mouse relatively to the menu-item
	int xb, yb;  // position of the top-left corner of the button relatively to the menu-item
	GtkWidget* pButton;
	GList* c;
	for (c = children->next; c != NULL; c = c->next)
	{
		pButton = GTK_WIDGET(c->data);
		GtkAllocation alloc;
		gtk_widget_get_allocation (pButton, &alloc);
		gtk_widget_translate_coordinates (pButton, pWidget,
			0, 0, &xb, &yb);
		if (x >= xb && x < (xb + alloc.width)
		&& y >= yb && y < (yb + alloc.height))
		{
			gtk_widget_set_state_flags (pButton, GTK_STATE_FLAG_ACTIVE, TRUE);
			gtk_widget_set_state_flags (
				gtk_bin_get_child(GTK_BIN(pButton)),
				GTK_STATE_FLAG_ACTIVE, TRUE);
			gtk_button_clicked (GTK_BUTTON (pButton));
		}
		else
		{
			gtk_widget_set_state_flags (pButton, GTK_STATE_FLAG_NORMAL, TRUE);
			gtk_widget_set_state_flags (
				gtk_bin_get_child(GTK_BIN(pButton)),
				GTK_STATE_FLAG_NORMAL, TRUE);
		}
	}
	g_list_free (children);
	gtk_widget_queue_draw (pWidget);
	return TRUE;
}
static gboolean _draw_menu_item (GtkWidget* pWidget, cairo_t *cr)
{
    gtk_container_propagate_draw (GTK_CONTAINER (pWidget),
		gtk_bin_get_child (GTK_BIN (pWidget)),
		cr);  // skip the drawing of the menu-item, just propagate to its child; there is no need to make anything else, the child hbox will draw its child as usual.
	return TRUE;  // intercept
}
gboolean _on_motion_notify_menu_item (GtkWidget* pWidget,
	GdkEventMotion* pEvent,
	G_GNUC_UNUSED gpointer data)
{
	GtkWidget *hbox = gtk_bin_get_child (GTK_BIN (pWidget));
	GList *children = gtk_container_get_children (GTK_CONTAINER (hbox));
	int x = pEvent->x, y = pEvent->y;  // position of the mouse relatively to the menu-item
	int xb, yb;  // position of the top-left corner of the button relatively to the menu-item
	GtkWidget* pButton;
	GList* c;
	for (c = children->next; c != NULL; c = c->next)  // skip the label
	{
		pButton = GTK_WIDGET (c->data);
		GtkAllocation alloc;
		gtk_widget_get_allocation (pButton, &alloc);
		gtk_widget_translate_coordinates (pButton, pWidget,
			0, 0, &xb, &yb);
		if (x >= xb && x < (xb + alloc.width)
		&& y >= yb && y < (yb + alloc.height))  // the mouse is inside the button -> select it
		{
			gtk_widget_set_state_flags (pButton, GTK_STATE_FLAG_PRELIGHT, TRUE);
			gtk_widget_set_state_flags (
				gtk_bin_get_child(GTK_BIN(pButton)),
				GTK_STATE_FLAG_PRELIGHT, TRUE);
		}
		else  // else deselect it, in case it was selected
		{
			gtk_widget_set_state_flags (pButton, GTK_STATE_FLAG_NORMAL, TRUE);
			gtk_widget_set_state_flags (
				gtk_bin_get_child(GTK_BIN(pButton)),
				GTK_STATE_FLAG_NORMAL, TRUE);
		}
	}
	GtkWidget *pLabel = children->data;  // force the label to be in a normal state
	gtk_widget_set_state_flags (pLabel, GTK_STATE_FLAG_NORMAL, TRUE);
	g_list_free (children);
	gtk_widget_queue_draw (pWidget);  // and redraw everything
	return FALSE;
}
static gboolean _on_leave_menu_item (GtkWidget* pWidget,
	G_GNUC_UNUSED GdkEventCrossing* pEvent,
	G_GNUC_UNUSED gpointer data)
{
	GtkWidget *hbox = gtk_bin_get_child (GTK_BIN (pWidget));
	GList *children = gtk_container_get_children (GTK_CONTAINER (hbox));
	GtkWidget* pButton;
	GList* c;
	for (c = children->next; c != NULL; c = c->next)
	{
		pButton = GTK_WIDGET(c->data);
		gtk_widget_set_state_flags (pButton, GTK_STATE_FLAG_NORMAL, TRUE);
		gtk_widget_set_state_flags(
			gtk_bin_get_child (GTK_BIN(pButton)),
			GTK_STATE_FLAG_NORMAL, TRUE);
	}
	g_list_free (children);
	gtk_widget_queue_draw (pWidget);
	return FALSE;
}
static gboolean _on_enter_menu_item (GtkWidget* pWidget,
	G_GNUC_UNUSED GdkEventCrossing* pEvent,
	G_GNUC_UNUSED gpointer data)
{
	GtkWidget *hbox = gtk_bin_get_child (GTK_BIN (pWidget));
	GList *children = gtk_container_get_children (GTK_CONTAINER (hbox));
	GtkWidget* pLabel = children->data;  // force the label to be in a normal state
	gtk_widget_set_state_flags (pLabel, GTK_STATE_FLAG_NORMAL, TRUE);
	g_list_free (children);
	gtk_widget_queue_draw (pWidget);
	return FALSE;
}
static GtkWidget *_add_new_button_to_hbox (const gchar *gtkStock, const gchar *cTooltip, GCallback pFunction, GtkWidget *hbox, gpointer data)
{
	GtkWidget *pButton = gtk_button_new ();
	/*
		GtkStyleContext *ctx = gtk_widget_get_style_context (pButton);
	// or we can just remove the style of a button but it's still a button :)
		gtk_style_context_remove_class (ctx, GTK_STYLE_CLASS_BUTTON);
	 // a style like a menuitem but it's a button...
		gtk_style_context_add_class (ctx, GTK_STYLE_CLASS_MENUITEM);
	*/
	
	if (gtkStock)
	{
		GtkWidget *pImage = NULL;
		if (*gtkStock == '/')
		{
			int size = cairo_dock_search_icon_size (GTK_ICON_SIZE_MENU);
			GdkPixbuf *pixbuf = cairo_dock_load_gdk_pixbuf (gtkStock, size, size);
			pImage = gtk_image_new_from_pixbuf (pixbuf);
			g_object_unref (pixbuf);
		}
		else
		{
			//!! TODO: uses GdkPixbuf internally !!
			pImage = gtk_image_new_from_icon_name (gtkStock, GTK_ICON_SIZE_MENU);
		}
		gtk_button_set_image (GTK_BUTTON (pButton), pImage);  // don't unref the image
	}
	gtk_widget_set_tooltip_text (pButton, cTooltip);
	g_signal_connect (G_OBJECT (pButton), "clicked", G_CALLBACK(pFunction), data);
	gtk_box_pack_end (GTK_BOX (hbox), pButton, FALSE, FALSE, 0);
	return pButton;
}
static GtkWidget *_add_menu_item_with_buttons (GtkWidget *menu)
{
	GtkWidget *pMenuItem = gtk_menu_item_new ();
	gtk_menu_shell_append  (GTK_MENU_SHELL (menu), pMenuItem);
	
	g_signal_connect (G_OBJECT (pMenuItem), "button-press-event",
		G_CALLBACK(_on_press_menu_item),
		NULL);  // the press event on the menu will close it, which is fine for us.
	g_signal_connect (G_OBJECT (pMenuItem), "motion-notify-event",
		G_CALLBACK(_on_motion_notify_menu_item),
		NULL);  // we need to manually higlight the currently pointed button
	g_signal_connect (G_OBJECT (pMenuItem),
		"leave-notify-event",
		G_CALLBACK (_on_leave_menu_item),
		NULL);  // to turn off the highlighted button when we leave the menu-item (if we leave it quickly, a motion event won't be generated)
	g_signal_connect (G_OBJECT (pMenuItem),
		"enter-notify-event",
		G_CALLBACK (_on_enter_menu_item),
		NULL);  // to force the label to not highlight (it gets highlighted, even if we overwrite the motion_notify_event callback)
	g_signal_connect (G_OBJECT (pMenuItem),
		"draw",
		G_CALLBACK (_draw_menu_item),
		NULL);  // we don't want the whole menu-item to be higlighted, but only the currently pointed button; so we draw the menu-item ourselves.
	
	GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);
	gtk_container_add (GTK_CONTAINER (pMenuItem), hbox);
	return hbox;
}


gboolean cairo_dock_notification_build_icon_menu (G_GNUC_UNUSED gpointer *pUserData, Icon *icon, GldiContainer *pContainer, GtkWidget *menu)
{
	struct _MenuParams *params = g_new0 (struct _MenuParams, 1);
	params->pIcon = icon;
	params->pContainer = pContainer;
	g_object_weak_ref (G_OBJECT (menu), _menu_destroy_notify, (gpointer)params);
	
	GtkWidget *pMenuItem;
	gchar *cLabel;
	
	//\_________________________ Clic en-dehors d'une icone utile => on s'arrete la.
	if (CAIRO_DOCK_IS_DOCK (pContainer) && (icon == NULL || CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon)))
	{
		return GLDI_NOTIFICATION_LET_PASS;
	}
	
	gboolean bAddSeparator = TRUE;
	if (CAIRO_DOCK_IS_DESKLET (pContainer) && icon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))  // not on the icons of a desklet, except the applet icon (on a desklet, it's easy to click out of any icon).
		bAddSeparator = FALSE;
	
	if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->iRefCount > 0)  // not on the sub-docks, except user sub-docks.
	{
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (CAIRO_DOCK (pContainer), NULL);
		if (pPointingIcon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pPointingIcon))
			bAddSeparator = FALSE;
	}
	
	//\_________________________ class actions.
	if (icon && icon->cClass != NULL && ! icon->bIgnoreQuicklist)
	{
		GDesktopAppInfo *app = icon->pClassApp;
		if (app != NULL)
		{
			const gchar* const* actions = g_desktop_app_info_list_actions (app);
			if (actions && *actions)
			{
				if (bAddSeparator)
				{
					pMenuItem = gtk_separator_menu_item_new ();
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
				}
				bAddSeparator = TRUE;
				for (; *actions; ++actions)
				{
					struct _AppAction *pAction = g_new0 (struct _AppAction, 1);
					pAction->app = app;
					pAction->action = *actions;
					pAction->action_name = g_desktop_app_info_get_action_name (app, *actions);
					
					// note: app is guaranteed to live as long as icon (it holds a ref to it)
					// and our menu is destroyed with the icon, so we can use the strings here directly
					pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (pAction->action_name,
						NULL,
						G_CALLBACK (_cairo_dock_launch_class_action),
						menu, (gpointer)pAction);
					g_object_weak_ref (G_OBJECT (pMenuItem), _menu_item_destroyed, (gpointer)pAction);
				}
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
		
		GldiWindowActor *pAppli = icon->pAppli;
		
		gboolean bCanMinimize, bCanMaximize, bCanClose;
		gldi_window_can_minimize_maximize_close (pAppli, &bCanMinimize, &bCanMaximize, &bCanClose);

		//\_________________________ Window Management
		GtkWidget *hbox = _add_menu_item_with_buttons (menu);
		
		GtkWidget *pLabel = gtk_label_new (_("Window"));
		gtk_box_pack_start (GTK_BOX (hbox), pLabel, FALSE, FALSE, 0);

		if (bCanClose)
		{
			if (myTaskbarParam.iActionOnMiddleClick == CAIRO_APPLI_ACTION_CLOSE && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))  // close
				cLabel = g_strdup_printf ("%s (%s)", _("Close"), _("middle-click"));
			else
				cLabel = g_strdup (_("Close"));
			_add_new_button_to_hbox (CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-close.svg",
				cLabel,
				G_CALLBACK(_cairo_dock_close_appli),
				hbox, (gpointer)params);
			g_free (cLabel);
		}
		
		if (! pAppli->bIsHidden)
		{
			if (bCanMaximize)
			{
				_add_new_button_to_hbox (pAppli->bIsMaximized ? CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-restore.svg" : CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-maximize.svg",
					pAppli->bIsMaximized ? _("Unmaximise") : _("Maximise"),
					G_CALLBACK(_cairo_dock_maximize_appli),
					hbox, (gpointer)params);
			}

			if (bCanMinimize)
			{
				if (myTaskbarParam.iActionOnMiddleClick == CAIRO_APPLI_ACTION_MINIMISE && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))  // minimize
					cLabel = g_strdup_printf ("%s (%s)", _("Minimise"), _("middle-click"));
				else
					cLabel = g_strdup (_("Minimise"));
				_add_new_button_to_hbox (CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-minimize.svg",
					cLabel,
					G_CALLBACK(_cairo_dock_minimize_appli),
					hbox, (gpointer)params);
				g_free (cLabel);
			}
		}
		
		if (pAppli->bIsHidden
		    || pAppli != gldi_windows_get_active ()
		    || !gldi_window_is_on_current_desktop (pAppli))
		{
			_add_new_button_to_hbox (GLDI_ICON_NAME_FIND,
				_("Show"),
				G_CALLBACK(_cairo_dock_show_appli),
				hbox, (gpointer)params);
		}
		
		//\_________________________ Other actions
		GtkWidget *pSubMenuOtherActions = cairo_dock_create_sub_menu (_("Other actions"), menu, NULL);

		// Move
		pMenuItem = _add_entry_in_menu (_("Move to this desktop"), GLDI_ICON_NAME_JUMP_TO, _cairo_dock_move_appli_to_current_desktop, pSubMenuOtherActions, params);
		// if it is not possible to move the window, we still keep this menu item, but make it inactive
		if (!gldi_window_manager_can_move_to_desktop () || gldi_window_is_on_current_desktop (pAppli))
			gtk_widget_set_sensitive (pMenuItem, FALSE);

		// Fullscreen
		_add_entry_in_menu (pAppli->bIsFullScreen ? _("Not Fullscreen") : _("Fullscreen"), pAppli->bIsFullScreen ? GLDI_ICON_NAME_LEAVE_FULLSCREEN : GLDI_ICON_NAME_FULLSCREEN, _cairo_dock_set_appli_fullscreen, pSubMenuOtherActions, params);

		// Below
		if (! pAppli->bIsHidden)  // this could be a button in the menu, if we find an icon that doesn't look too much like the "minimise" one
		{
			if (myTaskbarParam.iActionOnMiddleClick == CAIRO_APPLI_ACTION_LOWER && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))  // lower
				cLabel = g_strdup_printf ("%s (%s)", _("Below other windows"), _("middle-click"));
			else
				cLabel = g_strdup (_("Below other windows"));
			_add_entry_in_menu (cLabel, CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-lower.svg", _cairo_dock_lower_appli, pSubMenuOtherActions, params);
			g_free (cLabel);
		}

		// Above
		gboolean bIsAbove=FALSE, bIsBelow=FALSE;
		gldi_window_is_above_or_below (pAppli, &bIsAbove, &bIsBelow);
		_add_entry_in_menu (bIsAbove ? _("Don't keep above") : _("Keep above"), bIsAbove ? GLDI_ICON_NAME_GOTO_BOTTOM : GLDI_ICON_NAME_GOTO_TOP, _cairo_dock_change_window_above, pSubMenuOtherActions, params);

		// Sticky
		gboolean bIsSticky = gldi_window_is_sticky (pAppli);
		_add_entry_in_menu (bIsSticky ? _("Visible only on this desktop") : _("Visible on all desktops"), GLDI_ICON_NAME_JUMP_TO, _cairo_dock_change_window_sticky, pSubMenuOtherActions, params);

		if (!bIsSticky)  // if the window is sticky, it's on all desktops/viewports, so you can't move it to another
			_add_desktops_entry (pSubMenuOtherActions, FALSE, params);
		
		// add separator
		pMenuItem = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (pSubMenuOtherActions), pMenuItem);

		_add_entry_in_menu (_("Kill"), GLDI_ICON_NAME_CLOSE, _cairo_dock_kill_appli, pSubMenuOtherActions, params);
	}
	else if (CAIRO_DOCK_IS_MULTI_APPLI (icon))
	{
		if (bAddSeparator)
		{
			pMenuItem = gtk_separator_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		}
		bAddSeparator = TRUE;

		//\_________________________ Window Management
		GtkWidget *hbox = _add_menu_item_with_buttons (menu);
		
		GtkWidget *pLabel = gtk_label_new (_("Windows"));
		gtk_box_pack_start (GTK_BOX (hbox), pLabel, FALSE, FALSE, 0);
		
		if (myTaskbarParam.iActionOnMiddleClick == CAIRO_APPLI_ACTION_CLOSE && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))  // close
			cLabel = g_strdup_printf ("%s (%s)", _("Close all"), _("middle-click"));
		else
			cLabel = g_strdup (_("Close all"));
		_add_new_button_to_hbox (CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-close.svg",
			cLabel,
			G_CALLBACK(_cairo_dock_close_class),
			hbox, (gpointer)params);
		g_free (cLabel);
		
		if (myTaskbarParam.iActionOnMiddleClick == CAIRO_APPLI_ACTION_MINIMISE && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))  // minimize
			cLabel = g_strdup_printf ("%s (%s)", _("Minimise all"), _("middle-click"));
		else
			cLabel = g_strdup (_("Minimise all"));
		_add_new_button_to_hbox (CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-minimize.svg",
			cLabel,
			G_CALLBACK(_cairo_dock_minimize_class),
			hbox, (gpointer)params);
		g_free (cLabel);
		
		_add_new_button_to_hbox (GLDI_ICON_NAME_FIND,
			_("Show all"),
			G_CALLBACK(_cairo_dock_show_class),
			hbox, (gpointer)params);
		
		//\_________________________ Other actions
		GtkWidget *pSubMenuOtherActions = cairo_dock_create_sub_menu (_("Other actions"), menu, NULL);
		
		pMenuItem = _add_entry_in_menu (_("Move all to this desktop"), GLDI_ICON_NAME_JUMP_TO, _cairo_dock_move_class_to_current_desktop, pSubMenuOtherActions, params);
		// if it is not possible to move the windows, we still keep this menu item, but make it inactive
		if (!gldi_window_manager_can_move_to_desktop ())
			gtk_widget_set_sensitive (pMenuItem, FALSE);

		
		_add_desktops_entry (pSubMenuOtherActions, TRUE, params);
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
		
		GtkWidget *pSubMenuAccessibility = cairo_dock_create_sub_menu (_("Visibility"), menu, GLDI_ICON_NAME_FIND);
		
		GSList *group = NULL;

		gboolean bIsSticky = gldi_desklet_is_sticky (CAIRO_DESKLET (pContainer));
		CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
		CairoDeskletVisibility iVisibility = pDesklet->iVisibility;
		
		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Normal"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), iVisibility == CAIRO_DESKLET_NORMAL/*bIsNormal*/);  // on coche celui-ci par defaut, il sera decoche par les suivants eventuellement.
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_normal), (gpointer)params);
		
		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Always on top"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
		if (iVisibility == CAIRO_DESKLET_KEEP_ABOVE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_above), (gpointer)params);
		
		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Always below"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
		if (iVisibility == CAIRO_DESKLET_KEEP_BELOW)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_below), (gpointer)params);
		
		if (gldi_desktop_can_set_on_widget_layer ())
		{
			pMenuItem = gtk_radio_menu_item_new_with_label(group, "Widget Layer");
			group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
			gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
			if (iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER)
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
			g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_on_widget_layer), (gpointer)params);
		}
		
		pMenuItem = gtk_radio_menu_item_new_with_label(group, _("Reserve space"));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(pMenuItem));
		gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenuAccessibility), pMenuItem);
		if (iVisibility == CAIRO_DESKLET_RESERVE_SPACE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_keep_space), (gpointer)params);
		
		pMenuItem = gtk_check_menu_item_new_with_label(_("On all desktops"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		if (bIsSticky)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_set_on_all_desktop), (gpointer)params);
		
		pMenuItem = gtk_check_menu_item_new_with_label(_("Lock position"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), pMenuItem);
		if (pDesklet->bPositionLocked)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pMenuItem), TRUE);
		g_signal_connect(G_OBJECT(pMenuItem), "toggled", G_CALLBACK(_cairo_dock_lock_position), (gpointer)params);
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

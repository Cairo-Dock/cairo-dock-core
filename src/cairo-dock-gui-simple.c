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

#include <string.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "config.h"
#include "gldi-config.h"
#include "gldi-icon-names.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-module-instance-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-icon-manager.h"  // cairo_dock_search_icon_size
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_get_width
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-widget-items.h"
#include "cairo-dock-widget-themes.h"
#include "cairo-dock-widget-config.h"
#include "cairo-dock-widget-plugins.h"
#include "cairo-dock-widget.h"
#include "cairo-dock-gui-simple.h"

#define CAIRO_DOCK_PREVIEW_WIDTH 200
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_SIMPLE_PANEL_WIDTH 1200 // matttbe: 800
#define CAIRO_DOCK_SIMPLE_PANEL_HEIGHT 700 // matttbe: 500

typedef struct {
	const gchar *cName;
	const gchar *cIcon;
	const gchar *cTooltip;
	CDWidget* (*build_widget) (void);
	GtkToolItem *pCategoryButton;
	GtkWidget *pMainWindow;
	CDWidget *pCdWidget;
} CDCategory;

typedef enum {
	CD_CATEGORY_ITEMS=0,
	CD_CATEGORY_PLUGINS,
	CD_CATEGORY_CONFIG,
	CD_CATEGORY_THEMES,
	CD_NB_CATEGORIES
} CDCategoryEnum;

static GtkWidget *s_pSimpleConfigWindow = NULL;
static CDCategoryEnum s_iCurrentCategory = 0;

extern gchar *g_cCairoDockDataDir;
extern CairoDock *g_pMainDock;

static CDWidget *_build_items_widget (void);
static CDWidget *_build_config_widget (void);
static CDWidget *_build_themes_widget (void);
static CDWidget *_build_plugins_widget (void);

static void cairo_dock_enable_apply_button (GtkWidget *pMainWindow, gboolean bEnable);
static void cairo_dock_select_category (GtkWidget *pMainWindow, CDCategoryEnum iCategory);

static CDCategory s_pCategories[CD_NB_CATEGORIES] = {
	{
		N_("Current items"),
		CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-all.svg",
		N_("Current items"),
		_build_items_widget,
		NULL,
		NULL,
		NULL
	},{
		N_("Add-ons"),
		CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-plug-ins.svg",
		N_("Add-ons"),
		_build_plugins_widget,
		NULL,
		NULL,
		NULL
	},{
		N_("Configuration"),
		"preferences-system",
		N_("Configuration"),
		_build_config_widget,
		NULL,
		NULL,
		NULL
	},{
		N_("Themes"),
		CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-appearance.svg",
		N_("Themes"),
		_build_themes_widget,
		NULL,
		NULL,
		NULL
	}
};


static inline CDCategory *_get_category (CDCategoryEnum iCategory)
{
	return &s_pCategories[iCategory];
}

static inline void _set_current_category (CDCategoryEnum iCategory)
{
	s_iCurrentCategory = iCategory;
}

static inline CDCategory *_get_current_category (void)
{
	return _get_category (s_iCurrentCategory);
}

static CDWidget *_build_items_widget (void)
{
	CDWidget *pCdWidget = CD_WIDGET (cairo_dock_items_widget_new (GTK_WINDOW (s_pSimpleConfigWindow)));
	
	return pCdWidget;
}

static CDWidget *_build_config_widget (void)
{
	CDWidget *pCdWidget = CD_WIDGET (cairo_dock_config_widget_new (GTK_WINDOW (s_pSimpleConfigWindow)));
	
	return pCdWidget;
}

static CDWidget *_build_themes_widget (void)
{
	CDWidget *pCdWidget = CD_WIDGET (cairo_dock_themes_widget_new (GTK_WINDOW (s_pSimpleConfigWindow)));
	
	return pCdWidget;
}

static CDWidget *_build_plugins_widget (void)
{
	CDWidget *pCdWidget = CD_WIDGET (cairo_dock_plugins_widget_new ());
	
	return pCdWidget;
}

static void on_click_quit (G_GNUC_UNUSED GtkButton *button, GtkWidget *pMainWindow)
{
	gtk_widget_destroy (pMainWindow);
}
static void on_click_apply (G_GNUC_UNUSED GtkButton *button, G_GNUC_UNUSED GtkWidget *pMainWindow)
{
	CDCategory *pCategory = _get_current_category ();
	CDWidget *pCdWidget = pCategory->pCdWidget;
	if (pCdWidget)
		cairo_dock_widget_apply (pCdWidget);
}
GtkWidget *cairo_dock_build_generic_gui_window2 (const gchar *cTitle, int iWidth, int iHeight)
{
	//\_____________ make a new window.
	GtkWidget *pMainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon_from_file (GTK_WINDOW (pMainWindow), GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, NULL);
	if (cTitle != NULL)
		gtk_window_set_title (GTK_WINDOW (pMainWindow), cTitle);
	
	GtkWidget *pMainVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0*CAIRO_DOCK_FRAME_MARGIN);  // all elements will be packed in a VBox
	gtk_container_add (GTK_CONTAINER (pMainWindow), pMainVBox);
	
	//\_____________ add apply/quit buttons.
	GtkWidget *pButtonsHBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_FRAME_MARGIN*2);
	gtk_box_pack_end (GTK_BOX (pMainVBox),
		pButtonsHBox,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pQuitButton = gtk_button_new_with_label (_("Close"));
	g_signal_connect (G_OBJECT (pQuitButton), "clicked", G_CALLBACK(on_click_quit), pMainWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		pQuitButton,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pApplyButton = gtk_button_new_with_label (_("Apply"));
	g_signal_connect (G_OBJECT (pApplyButton), "clicked", G_CALLBACK(on_click_apply), pMainWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		pApplyButton,
		FALSE,
		FALSE,
		0);
	g_object_set_data (G_OBJECT (pMainWindow), "apply-button", pApplyButton);
	
	GtkWidget *pSwitchButton = cairo_dock_make_switch_gui_button ();
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		pSwitchButton,
		FALSE,
		FALSE,
		0);
	
	//\_____________ add a status-bar.
	GtkWidget *pStatusBar = gtk_statusbar_new ();
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),  // pMainVBox
		pStatusBar,
		FALSE,
		FALSE,
		0);
	g_object_set_data (G_OBJECT (pMainWindow), "status-bar", pStatusBar);
	
	//\_____________ resize the window (placement is done later).
	gtk_window_resize (GTK_WINDOW (pMainWindow),
		MIN (iWidth, gldi_desktop_get_width()),
		MIN (iHeight, gldi_desktop_get_height() - (g_pMainDock && g_pMainDock->container.bIsHorizontal ? g_pMainDock->iMaxDockHeight : 0)));
	
	gtk_widget_show_all (pMainWindow);
	
	return pMainWindow;
}


/**static inline GtkWidget *_make_image (const gchar *cImage, int iSize)
{
	g_return_val_if_fail (cImage != NULL, NULL);
	GtkWidget *pImage = NULL;
	if (strncmp (cImage, "gtk-", 4) == 0)
	{
		
		if (iSize >= 48)
			iSize = GTK_ICON_SIZE_DIALOG;
		else if (iSize >= 32)
			iSize = GTK_ICON_SIZE_LARGE_TOOLBAR;
		else
			iSize = GTK_ICON_SIZE_BUTTON;
		pImage = gtk_image_new_from_stock (cImage, iSize);
	}
	else
	{
		gchar *cIconPath = NULL;
		if (*cImage != '/')
		{
			cIconPath = g_strconcat (g_cCairoDockDataDir, "/config-panel/", cImage, NULL);
			if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS))
			{
				g_free (cIconPath);
				cIconPath = g_strconcat (CAIRO_DOCK_SHARE_DATA_DIR"/icons/", cImage, NULL);
			}
		}
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cIconPath ? cIconPath : cImage, iSize, iSize, NULL);
		g_free (cIconPath);
		if (pixbuf != NULL)
		{
			pImage = gtk_image_new_from_pixbuf (pixbuf);
			g_object_unref (pixbuf);
		}
	}
	return pImage;
}*/
static GtkWidget *_make_notebook_label (const gchar *cLabel, const gchar *cImage, int iSize)
{
	GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_FRAME_MARGIN);
	
	GtkWidget *pImage = _gtk_image_new_from_file (cImage, iSize);
	gtk_widget_set_margin_start (pImage, CAIRO_DOCK_FRAME_MARGIN);
	gtk_box_pack_start (GTK_BOX (hbox),
		pImage,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pLabel = gtk_label_new (cLabel);
	gtk_widget_set_margin_start (pLabel, CAIRO_DOCK_FRAME_MARGIN);
	gtk_box_pack_start (GTK_BOX (hbox),
		pLabel,
		FALSE,
		FALSE,
		0);	
	
	gtk_widget_show_all (hbox);
	return hbox;
}

static void _build_category_widget (CDCategory *pCategory)
{
	pCategory->pCdWidget = pCategory->build_widget ();
	gtk_widget_show_all (pCategory->pCdWidget->pWidget);
}

static void _on_switch_page (G_GNUC_UNUSED GtkNotebook *pNoteBook, GtkWidget *page, guint page_num, G_GNUC_UNUSED gpointer user_data)
{
	CDCategory *pCategory = _get_category (page_num);
	g_return_if_fail (pCategory != NULL);
	if (pCategory->pCdWidget == NULL)
	{
		_build_category_widget (pCategory);
		gtk_box_pack_start (GTK_BOX (page), pCategory->pCdWidget->pWidget, TRUE, TRUE, 0);
		gtk_widget_show (pCategory->pCdWidget->pWidget);
	}
	
	cairo_dock_enable_apply_button (pCategory->pMainWindow, cairo_dock_widget_can_apply (pCategory->pCdWidget));
	
	_set_current_category (page_num);
}

static void _on_window_destroyed (G_GNUC_UNUSED GtkWidget *pMainWindow, G_GNUC_UNUSED gpointer data)
{
	CDCategory *pCategory;
	int i;
	for (i = 0; i < CD_NB_CATEGORIES; i ++)
	{
		pCategory = _get_category (i);
		pCategory->pCategoryButton = NULL;
		pCategory->pMainWindow = NULL;
		cairo_dock_widget_free (pCategory->pCdWidget);
		pCategory->pCdWidget = NULL;
	}
	s_pSimpleConfigWindow = NULL;
}


GtkWidget *cairo_dock_build_simple_gui_window (void)
{
	if (s_pSimpleConfigWindow != NULL)
	{
		return s_pSimpleConfigWindow;
	}
	
	//\_____________ build a new config window
	GtkWidget *pMainWindow = cairo_dock_build_generic_gui_window2 (_("Cairo-Dock configuration"),
		CAIRO_DOCK_SIMPLE_PANEL_WIDTH, CAIRO_DOCK_SIMPLE_PANEL_HEIGHT);
	s_pSimpleConfigWindow = pMainWindow;
	g_signal_connect (G_OBJECT (pMainWindow), "destroy", G_CALLBACK(_on_window_destroyed), NULL);
	
	//\_____________ add categories
	GtkWidget *pNoteBook = gtk_notebook_new ();
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (pNoteBook), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (pNoteBook));
	
	GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (pMainWindow));
	gtk_box_pack_start (GTK_BOX (pMainVBox),
		pNoteBook,
		TRUE,
		TRUE,
		0);
	g_object_set_data (G_OBJECT (pMainWindow), "notebook", pNoteBook);
	
	GtkSizeGroup *pSizeGroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);  // make all tabs the same size (actually I'd like them to expand and fill the whole window...)
	CDCategory *pCategory;
	int i;
	for (i = 0; i < CD_NB_CATEGORIES; i ++)
	{
		pCategory = _get_category (i);
		
		GtkWidget *hbox = _make_notebook_label (gettext (pCategory->cName),
			pCategory->cIcon,
			GTK_ICON_SIZE_LARGE_TOOLBAR);
		gtk_size_group_add_widget (pSizeGroup, hbox);
		GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_FRAME_MARGIN);
		gtk_notebook_append_page (GTK_NOTEBOOK (pNoteBook),
			vbox,
			hbox);
		pCategory->pMainWindow = pMainWindow;
	}
	gtk_widget_show_all (pNoteBook);
	
	g_signal_connect (pNoteBook, "switch-page", G_CALLBACK (_on_switch_page), NULL);
	
	return pMainWindow;
}


static void cairo_dock_enable_apply_button (GtkWidget *pMainWindow, gboolean bEnable)
{
	GtkWidget *pApplyButton = g_object_get_data (G_OBJECT (pMainWindow), "apply-button");
	if (bEnable)
		gtk_widget_show (pApplyButton);
	else
		gtk_widget_hide (pApplyButton);
}


static void cairo_dock_select_category (GtkWidget *pMainWindow, CDCategoryEnum iCategory)
{
	GtkNotebook *pNoteBook = g_object_get_data (G_OBJECT (pMainWindow), "notebook");
	gtk_notebook_set_current_page (pNoteBook, iCategory);  // will first emit a 'switch-page' signal, which will build the widget if necessary.
	g_signal_emit_by_name (pNoteBook, "switch-page", gtk_notebook_get_nth_page(pNoteBook, iCategory), iCategory, NULL);  // see the comment on the previous line ? well, it's not true anymore...
}


  ///////////////
 /// BACKEND ///
///////////////

static GtkWidget *show_main_gui (void)
{
	cairo_dock_build_simple_gui_window ();
	
	cairo_dock_select_category (s_pSimpleConfigWindow, CD_CATEGORY_CONFIG);
	return s_pSimpleConfigWindow;
}

static GtkWidget *show_module_gui (G_GNUC_UNUSED const gchar *cModuleName)
{
	cairo_dock_build_simple_gui_window ();
	
	cairo_dock_select_category (s_pSimpleConfigWindow, CD_CATEGORY_ITEMS);
	/// TODO: find a way to present a module that is not activated...
	
	return s_pSimpleConfigWindow;
}

static void close_gui (void)
{
	if (s_pSimpleConfigWindow != NULL)
		gtk_widget_destroy (s_pSimpleConfigWindow);
}

static void update_module_state (const gchar *cModuleName, gboolean bActive)
{
	if (s_pSimpleConfigWindow == NULL)
		return;
	CDCategory *pCategory = _get_category (CD_CATEGORY_PLUGINS);
	if (pCategory->pCdWidget != NULL)  // category is built
	{
		cairo_dock_widget_plugins_update_module_state (PLUGINS_WIDGET (pCategory->pCdWidget), cModuleName, bActive);
	}
}

static void update_modules_list (void)
{
	if (s_pSimpleConfigWindow == NULL)
		return;
	CDCategory *pCategory = _get_category (CD_CATEGORY_PLUGINS);
	if (pCategory->pCdWidget != NULL)  // category is built
	{
		cairo_dock_widget_reload (pCategory->pCdWidget);
	}
}

static void update_shortkeys (void)
{
	if (s_pSimpleConfigWindow == NULL)
		return;
	CDCategory *pCategory = _get_category (CD_CATEGORY_CONFIG);
	if (pCategory->pCdWidget != NULL)  // category is built
	{
		cairo_dock_widget_config_update_shortkeys (CONFIG_WIDGET (pCategory->pCdWidget));
	}
}

static void update_desklet_params (CairoDesklet *pDesklet)
{
	if (s_pSimpleConfigWindow == NULL || pDesklet == NULL || pDesklet->pIcon == NULL)
		return;
	
	CDCategory *pCategory = _get_category (CD_CATEGORY_ITEMS);
	if (pCategory->pCdWidget != NULL)  // category is built
	{
		cairo_dock_items_widget_update_desklet_params (ITEMS_WIDGET (pCategory->pCdWidget), pDesklet);
	}
}

static void update_desklet_visibility_params (CairoDesklet *pDesklet)
{
	if (s_pSimpleConfigWindow == NULL || pDesklet == NULL || pDesklet->pIcon == NULL)
		return;
	
	CDCategory *pCategory = _get_category (CD_CATEGORY_ITEMS);
	if (pCategory->pCdWidget != NULL)  // category is built
	{
		cairo_dock_items_widget_update_desklet_visibility_params (ITEMS_WIDGET (pCategory->pCdWidget), pDesklet);
	}
}

static void update_module_instance_container (GldiModuleInstance *pInstance, gboolean bDetached)
{
	if (s_pSimpleConfigWindow == NULL || pInstance == NULL)
		return;
	
	CDCategory *pCategory = _get_category (CD_CATEGORY_ITEMS);
	if (pCategory->pCdWidget != NULL)  // category is built
	{
		cairo_dock_items_widget_update_module_instance_container (ITEMS_WIDGET (pCategory->pCdWidget), pInstance, bDetached);
	}
}

static GtkWidget *show_gui (Icon *pIcon, GldiContainer *pContainer, GldiModuleInstance *pModuleInstance, int iShowPage)
{
	cairo_dock_build_simple_gui_window ();
	
	cairo_dock_select_category (s_pSimpleConfigWindow, CD_CATEGORY_ITEMS);  // will build the GTK widget
	
	CDCategory *pCategory = _get_category (CD_CATEGORY_ITEMS);
	cairo_dock_items_widget_select_item (ITEMS_WIDGET (pCategory->pCdWidget), pIcon, pContainer, pModuleInstance, iShowPage);
	
	return s_pSimpleConfigWindow;
}

static GtkWidget *show_addons (void)
{
	cairo_dock_build_simple_gui_window ();
	
	cairo_dock_select_category (s_pSimpleConfigWindow, CD_CATEGORY_PLUGINS);  // will build the GTK widget
	
	return s_pSimpleConfigWindow;
}

static void reload_items (void)
{
	if (s_pSimpleConfigWindow == NULL)
		return;
	
	CDCategory *pCategory = _get_category (CD_CATEGORY_ITEMS);
	if (pCategory->pCdWidget != NULL)  // category is built
	{
		cairo_dock_widget_reload (pCategory->pCdWidget);
	}
}

static void _reload_category_widget (CDCategoryEnum iCategory)
{
	CDCategory *pCategory = _get_category (iCategory);
	g_return_if_fail (pCategory != NULL);
	if (pCategory->pCdWidget != NULL)  // the category is built, reload it
	{
		GtkWidget *pPrevWidget = pCategory->pCdWidget->pWidget;
		cairo_dock_widget_reload (pCategory->pCdWidget);
		cd_debug ("%s (%p -> %p)", __func__, pPrevWidget, pCategory->pCdWidget->pWidget);
		
		if (pPrevWidget != pCategory->pCdWidget->pWidget)  // the widget has been rebuilt, let's re-pack it in its container
		{
			GtkWidget *pNoteBook = g_object_get_data (G_OBJECT (s_pSimpleConfigWindow), "notebook");
			GtkWidget *page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (pNoteBook), iCategory);
			gtk_box_pack_start (GTK_BOX (page), pCategory->pCdWidget->pWidget, TRUE, TRUE, 0);
			gtk_widget_show_all (pCategory->pCdWidget->pWidget);
		}
	}
}

static void reload (void)
{
	if (s_pSimpleConfigWindow == NULL)
		return;
	
	_reload_category_widget (CD_CATEGORY_ITEMS);
	
	_reload_category_widget (CD_CATEGORY_CONFIG);
	
	_reload_category_widget (CD_CATEGORY_PLUGINS);
}

////////////////////
/// CORE BACKEND ///
////////////////////

static void set_status_message_on_gui (const gchar *cMessage)
{
	if (s_pSimpleConfigWindow == NULL)
		return;
	GtkWidget *pStatusBar = g_object_get_data (G_OBJECT (s_pSimpleConfigWindow), "status-bar");
	gtk_statusbar_pop (GTK_STATUSBAR (pStatusBar), 0);  // clear any previous message, underflow is allowed.
	gtk_statusbar_push (GTK_STATUSBAR (pStatusBar), 0, cMessage);
}

static void reload_current_widget (GldiModuleInstance *pInstance, int iShowPage)
{
	g_return_if_fail (s_pSimpleConfigWindow != NULL);
	
	CDCategory *pCategory = _get_category (CD_CATEGORY_ITEMS);
	if (pCategory->pCdWidget != NULL)  // category is built
	{
		cairo_dock_items_widget_reload_current_widget (ITEMS_WIDGET (pCategory->pCdWidget), pInstance, iShowPage);
	}
}

static void show_module_instance_gui (GldiModuleInstance *pModuleInstance, int iShowPage)
{
	show_gui (pModuleInstance->pIcon, NULL, pModuleInstance, iShowPage);
}

static CairoDockGroupKeyWidget *get_widget_from_name (G_GNUC_UNUSED GldiModuleInstance *pInstance, const gchar *cGroupName, const gchar *cKeyName)
{
	g_return_val_if_fail (s_pSimpleConfigWindow != NULL, NULL);
	cd_debug ("%s (%s, %s)", __func__, cGroupName, cKeyName);
	CDCategory *pCategory = _get_current_category ();
	CDWidget *pCdWidget = pCategory->pCdWidget;
	if (pCdWidget)  /// check that the widget represents the given instance...
		return cairo_dock_gui_find_group_key_widget_in_list (pCdWidget->pWidgetList, cGroupName, cKeyName);
	else
		return NULL;
}


void cairo_dock_register_simple_gui_backend (void)
{
	CairoDockMainGuiBackend *pBackend = g_new0 (CairoDockMainGuiBackend, 1);
	
	pBackend->show_main_gui 					= show_main_gui;
	pBackend->show_module_gui 					= show_module_gui;
	pBackend->close_gui 						= close_gui;
	pBackend->update_module_state 				= update_module_state;
	pBackend->update_module_instance_container 	= update_module_instance_container;
	pBackend->update_desklet_params 			= update_desklet_params;
	pBackend->update_desklet_visibility_params 	= update_desklet_visibility_params;
	pBackend->update_modules_list 				= update_modules_list;
	pBackend->update_shortkeys 					= update_shortkeys;
	pBackend->show_gui 							= show_gui;
	pBackend->show_addons 						= show_addons;
	pBackend->reload_items 						= reload_items;
	pBackend->reload 							= reload;
	pBackend->cDisplayedName 					= _("Advanced Mode");  // name of the other backend.
	pBackend->cTooltip 							= _("The advanced mode lets you tweak every single parameter of the dock. It is a powerful tool to customise your current theme.");
	
	cairo_dock_register_config_gui_backend (pBackend);
	
	CairoDockGuiBackend *pConfigBackend = g_new0 (CairoDockGuiBackend, 1);
	
	pConfigBackend->set_status_message_on_gui 	= set_status_message_on_gui;
	pConfigBackend->reload_current_widget 		= reload_current_widget;
	pConfigBackend->show_module_instance_gui 	= show_module_instance_gui;
	pConfigBackend->get_widget_from_name 		= get_widget_from_name;
	
	cairo_dock_register_gui_backend (pConfigBackend);
}

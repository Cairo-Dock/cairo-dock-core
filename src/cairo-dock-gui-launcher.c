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
#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-load.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-launcher.h"

#define CAIRO_DOCK_FRAME_MARGIN 6
#define CAIRO_DOCK_LAUNCHER_PANEL_WIDTH 1000
#define CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT 500

extern gchar *g_cCurrentLaunchersPath;

static GtkWidget *s_pLauncherWindow = NULL;
static GtkWidget *s_pCurrentLauncherWidget = NULL;
static GtkWidget *s_pLauncherTreeView = NULL;
static GtkWidget *s_pLauncherPane = NULL;
static GtkWidget *s_pLauncherScrolledWindow = NULL;
static guint s_iSidRefreshGUI = 0;

  ///////////////
 // CALLBACKS //
///////////////

static void _free_launcher_gui (void)
{
	s_pLauncherWindow = NULL;
	s_pCurrentLauncherWidget = NULL;
	s_pLauncherPane = NULL;
	s_pLauncherTreeView = NULL;
	s_pLauncherScrolledWindow = NULL;
}

static void _delete_current_launcher_widget (void)
{
	g_return_if_fail (s_pLauncherWindow != NULL);
	
	if (s_pCurrentLauncherWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentLauncherWidget);
		s_pCurrentLauncherWidget = NULL;
	}
	GSList *pWidgetList = g_object_get_data (G_OBJECT (s_pLauncherWindow), "widget-list");
	if (pWidgetList != NULL)
	{
		cairo_dock_free_generated_widget_list (pWidgetList);
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "widget-list", NULL);
	}
	GPtrArray *pDataGarbage = g_object_get_data (G_OBJECT (s_pLauncherWindow), "garbage");
	if (pDataGarbage != NULL)
	{
		/// nettoyer ...
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "garbage", NULL);
	}
	
	g_object_set_data (G_OBJECT (s_pLauncherWindow), "current-icon", NULL);
}


static gboolean on_delete_launcher_gui (GtkWidget *pWidget, GdkEvent *event, gpointer data)
{
	GSList *pWidgetList = g_object_get_data (G_OBJECT (pWidget), "widget-list");
	cairo_dock_free_generated_widget_list (pWidgetList);
	
	GPtrArray *pDataGarbage = g_object_get_data (G_OBJECT (pWidget), "garbage");
	/// nettoyer.
	
	cairo_dock_dialog_window_destroyed ();
	
	_free_launcher_gui ();
	
	return FALSE;
}

static void on_click_launcher_apply (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	
	Icon *pIcon = g_object_get_data (G_OBJECT (pWindow), "current-icon");
	if (pIcon == NULL)  // ligne correspondante a un dock principal.
		return;
	GSList *pWidgetList = g_object_get_data (G_OBJECT (pWindow), "widget-list");
	
	gchar *cConfFilePath;
	if (pIcon->cDesktopFileName != NULL)
		cConfFilePath = (*pIcon->cDesktopFileName == '/' ? g_strdup (pIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pIcon->cDesktopFileName));
	else if (CAIRO_DOCK_IS_APPLET (pIcon))
		cConfFilePath = g_strdup (pIcon->pModuleInstance->cConfFilePath);
	else
		return ;
	
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	cairo_dock_update_keyfile_from_widget_list (pKeyFile, pWidgetList);
	cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	g_key_file_free (pKeyFile);
	g_free (cConfFilePath);
	
	if (pIcon->cDesktopFileName != NULL)
		cairo_dock_reload_launcher (pIcon);  // prend tout en compte, y compris le redessin et le rechargement de l'IHM.
	else
		cairo_dock_reload_module_instance (pIcon->pModuleInstance, TRUE);  // idem
}

static void on_click_launcher_quit (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	on_delete_launcher_gui (pWindow, NULL, NULL);
	gtk_widget_destroy (pWindow);
}


  ////////////
 // MODELE //
////////////

static gboolean _search_icon_in_line (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer *data)
{
	Icon *pIcon = NULL;
	gtk_tree_model_get (model, iter,
		2, &pIcon, -1);
	if (pIcon == data[0])
	{
		//g_print (" found !\n");
		memcpy (data[1], iter, sizeof (GtkTreeIter));
		data[2] = GINT_TO_POINTER (TRUE);
		return TRUE;  // stop iterating.
	}
	return FALSE;
}
static gboolean _search_icon_in_model (GtkWidget *pTreeView, Icon *pIcon, GtkTreeIter *iter)
{
	if (pIcon == NULL)
		return FALSE;
	//g_print ("%s (%s)\n", __func__, pIcon->cName);
	GtkTreeModel * model = gtk_tree_view_get_model (GTK_TREE_VIEW (pTreeView));
	gpointer data[3] = {pIcon, iter, GINT_TO_POINTER (FALSE)};
	gtk_tree_model_foreach (model,
		(GtkTreeModelForeachFunc) _search_icon_in_line,
		data);
	return GPOINTER_TO_INT (data[2]);
}

static gboolean _select_one_launcher_in_tree (GtkTreeSelection * selection, GtkTreeModel * model, GtkTreePath * path, gboolean path_currently_selected, gpointer data)
{
	//g_print ("%s (path_currently_selected:%d)\n", __func__, path_currently_selected);
	if (path_currently_selected)
		return TRUE;
	g_object_set_data (G_OBJECT (s_pLauncherWindow), "current-icon", NULL);
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter (model, &iter, path))
		return FALSE;
	
	_delete_current_launcher_widget ();
	
	gchar *cName = NULL;
	Icon *pIcon = NULL;
	gtk_tree_model_get (model, &iter,
		0, &cName,
		2, &pIcon, -1);
	gtk_window_set_title (GTK_WINDOW (s_pLauncherWindow), cName);
	g_free (cName);
	if (pIcon == NULL)  // ligne correspondante a un dock principal.
		return TRUE;
	
	// on charge son .conf
	if (pIcon->cDesktopFileName != NULL)
	{
		//g_print ("on presente %s...\n", pIcon->cDesktopFileName);
		gchar *cConfFilePath = (*pIcon->cDesktopFileName == '/' ? g_strdup (pIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pIcon->cDesktopFileName));
		
		CairoDockDesktopFileType iLauncherType;
		if (CAIRO_DOCK_IS_URI_LAUNCHER (pIcon))
			iLauncherType = CAIRO_DOCK_DESKTOP_FILE_FOR_FILE;
		else if (CAIRO_DOCK_IS_SEPARATOR (pIcon))
			iLauncherType = CAIRO_DOCK_DESKTOP_FILE_FOR_SEPARATOR;
		else if (pIcon->pSubDock != NULL && pIcon->cClass == NULL)
			iLauncherType = CAIRO_DOCK_DESKTOP_FILE_FOR_CONTAINER;
		else 
			iLauncherType = CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER;
		cairo_dock_update_launcher_desktop_file (cConfFilePath, iLauncherType);
		
		GSList *pWidgetList = NULL;
		GPtrArray *pDataGarbage = g_ptr_array_new ();
		s_pCurrentLauncherWidget = cairo_dock_build_conf_file_widget (cConfFilePath,
			NULL,
			s_pLauncherWindow,
			&pWidgetList,
			pDataGarbage,
			NULL);
		
		gtk_paned_pack2 (GTK_PANED (s_pLauncherPane), s_pCurrentLauncherWidget, TRUE, FALSE);
		
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "widget-list", pWidgetList);
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "garbage", pDataGarbage);
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "current-icon", pIcon);
		
		g_free (cConfFilePath);
		gtk_widget_show_all (s_pCurrentLauncherWidget);
	}
	else if (CAIRO_DOCK_IS_APPLET (pIcon))
	{
		GSList *pWidgetList = NULL;
		GPtrArray *pDataGarbage = g_ptr_array_new ();
		
		GKeyFile* pKeyFile = cairo_dock_open_key_file (pIcon->pModuleInstance->cConfFilePath);
		g_return_val_if_fail (pKeyFile != NULL, FALSE);
		
		gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", pIcon->pModuleInstance->pModule->pVisitCard->cShareDataDir, pIcon->pModuleInstance->pModule->pVisitCard->cConfFileName);
		s_pCurrentLauncherWidget = cairo_dock_build_group_widget (pKeyFile, "Icon", pIcon->pModuleInstance->pModule->pVisitCard->cGettextDomain, s_pLauncherWindow, &pWidgetList, pDataGarbage, cOriginalConfFilePath);
		g_free (cOriginalConfFilePath);
		
		gtk_paned_pack2 (GTK_PANED (s_pLauncherPane), s_pCurrentLauncherWidget, TRUE, FALSE);
		
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "widget-list", pWidgetList);
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "garbage", pDataGarbage);
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "current-icon", pIcon);
		gtk_widget_show_all (s_pCurrentLauncherWidget);
	}
	return TRUE;
}

static void _add_one_sub_dock_to_model (CairoDock *pDock, GtkTreeStore *model, GtkTreeIter *pParentIter)
{
	GtkTreeIter iter;
	GList *ic;
	Icon *pIcon;
	GError *erreur = NULL;
	GdkPixbuf *pixbuf = NULL;
	gchar *cImagePath = NULL;
	const gchar *cName;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (! CAIRO_DOCK_IS_LAUNCHER (pIcon) && ! CAIRO_DOCK_IS_USER_SEPARATOR (pIcon) && ! CAIRO_DOCK_IS_APPLET (pIcon))
			continue;
		
		if (cairo_dock_icon_is_being_removed (pIcon))
			continue;
		
		// set an image.
		if (pIcon->cFileName != NULL)
		{
			cImagePath = cairo_dock_search_icon_s_path (pIcon->cFileName);
		}
		if (cImagePath == NULL || ! g_file_test (cImagePath, G_FILE_TEST_EXISTS))
		{
			g_free (cImagePath);
			if (CAIRO_DOCK_IS_SEPARATOR (pIcon))
			{
				cImagePath = cairo_dock_search_image_s_path (myIcons.cSeparatorImage);
			}
			else if (CAIRO_DOCK_IS_APPLET (pIcon))
			{
				cImagePath = g_strdup (pIcon->pModuleInstance->pModule->pVisitCard->cIconFilePath);
			}
			else
			{
				cImagePath = cairo_dock_search_image_s_path (CAIRO_DOCK_DEFAULT_ICON_NAME);
				if (cImagePath == NULL || ! g_file_test (cImagePath, G_FILE_TEST_EXISTS))
				{
					g_free (cImagePath);
					cImagePath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_ICON_NAME);
				}
			}
		}
		
		if (cImagePath != NULL)
		{
			pixbuf = gdk_pixbuf_new_from_file_at_size (cImagePath, 32, 32, &erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
		}
		
		// set a name
		if (CAIRO_DOCK_IS_USER_SEPARATOR (pIcon))  // separator
			cName = "separator";
		else if (CAIRO_DOCK_IS_APPLET (pIcon))  // applet
			cName = dgettext (pIcon->pModuleInstance->pModule->pVisitCard->cGettextDomain, pIcon->pModuleInstance->pModule->pVisitCard->cTitle);
		else  // launcher
			cName = (pIcon->cInitialName ? pIcon->cInitialName : pIcon->cName);
		
		// add an entry in the tree view.
		gtk_tree_store_append (model, &iter, pParentIter);
		gtk_tree_store_set (model, &iter,
			0, cName,
			1, pixbuf,
			2, pIcon,
			-1);
		
		if (CAIRO_DOCK_IS_LAUNCHER (pIcon) && pIcon->pSubDock != NULL && ! CAIRO_DOCK_IS_URI_LAUNCHER (pIcon))
		{
			_add_one_sub_dock_to_model (pIcon->pSubDock, model, &iter);
		}
		
		// reset all for the next round.
		g_free (cImagePath);
		cImagePath = NULL;
		if (pixbuf)
			g_object_unref (pixbuf);
		pixbuf = NULL;
	}
}
static void _add_one_root_dock_to_model (const gchar *cName, CairoDock *pDock, GtkTreeStore *model)
{
	if (pDock->iRefCount != 0)
		return ;
	GtkTreeIter iter;
	
	// on ajoute une ligne pour le dock.
	gtk_tree_store_append (model, &iter, NULL);
    gtk_tree_store_set (model, &iter,
			0, cName,
			2, NULL,
			-1);
	
	// on ajoute chaque lanceur.
	_add_one_sub_dock_to_model (pDock, model, &iter);
}
static GtkTreeModel *_build_tree_model (void)
{
	GtkTreeStore *model = gtk_tree_store_new (3,
		G_TYPE_STRING,
		GDK_TYPE_PIXBUF,
		G_TYPE_POINTER);  // nom du lanceur, image, Icon.
	cairo_dock_foreach_docks ((GHFunc) _add_one_root_dock_to_model, model);
	return GTK_TREE_MODEL (model);
}

static inline gboolean _select_item (Icon *pIcon)
{
	GtkTreeIter iter;
	if (_search_icon_in_model (s_pLauncherTreeView, pIcon, &iter))
	{
		GtkTreeModel * model = gtk_tree_view_get_model (GTK_TREE_VIEW (s_pLauncherTreeView));
		GtkTreePath *path =  gtk_tree_model_get_path (model, &iter);
		gtk_tree_view_expand_to_path (GTK_TREE_VIEW (s_pLauncherTreeView), path);
		gtk_tree_path_free (path);
		
		GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (s_pLauncherTreeView));
		gtk_tree_selection_select_iter (pSelection, &iter);
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "current-icon", pIcon);
		return TRUE;
	}
	else
	{
		gtk_window_set_title (GTK_WINDOW (s_pLauncherWindow), _("Launcher configuration"));
		g_object_set_data (G_OBJECT (s_pLauncherWindow), "current-icon", NULL);
		return FALSE;
	}
}


  /////////
 // GUI //
/////////

static GtkWidget *show_gui (Icon *pIcon)
{
	//g_print ("%s ()\n", __func__);
	//\_____________ On construit la fenetre.
	if (s_pLauncherWindow != NULL)
	{
		_delete_current_launcher_widget ();
		
		_select_item (pIcon);
		
		gtk_window_present (GTK_WINDOW (s_pLauncherWindow));
		return s_pLauncherWindow;
	}
	s_pLauncherWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon_from_file (GTK_WINDOW (s_pLauncherWindow), CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, NULL);
	
	GtkWidget *pMainVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_FRAME_MARGIN);
	gtk_container_add (GTK_CONTAINER (s_pLauncherWindow), pMainVBox);
	
	s_pLauncherPane = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (pMainVBox),
		s_pLauncherPane,
		TRUE,
		TRUE,
		0);
	
	//\_____________ On construit l'arbre des launceurs.
	GtkTreeModel *model = _build_tree_model();
	
	//\_____________ On construit le tree-view avec.
	s_pLauncherTreeView = gtk_tree_view_new_with_model (model);
	g_object_unref (model);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (s_pLauncherTreeView), FALSE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (s_pLauncherTreeView), TRUE);
	
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (s_pLauncherTreeView));
	gtk_tree_selection_set_mode (pSelection, GTK_SELECTION_SINGLE);
	gtk_tree_selection_set_select_function (pSelection,
		(GtkTreeSelectionFunc) _select_one_launcher_in_tree,
		NULL,
		NULL);
	
	GtkCellRenderer *rend;
	rend = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (s_pLauncherTreeView), -1, NULL, rend, "text", 0, NULL);
	rend = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (s_pLauncherTreeView), -1, NULL, rend, "pixbuf", 1, NULL);
	
	s_pLauncherScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set (s_pLauncherScrolledWindow, "height-request", CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT - 30, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (s_pLauncherScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (s_pLauncherScrolledWindow), s_pLauncherTreeView);
	gtk_paned_pack1 (GTK_PANED (s_pLauncherPane), s_pLauncherScrolledWindow, TRUE, FALSE);
	
	//\_____________ On ajoute les boutons.
	GtkWidget *pButtonsHBox = gtk_hbox_new (FALSE, CAIRO_DOCK_FRAME_MARGIN*2);
	gtk_box_pack_end (GTK_BOX (pMainVBox),
		pButtonsHBox,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pQuitButton = gtk_button_new_from_stock (GTK_STOCK_QUIT);
	g_signal_connect (G_OBJECT (pQuitButton), "clicked", G_CALLBACK(on_click_launcher_quit), s_pLauncherWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		pQuitButton,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pApplyButton = gtk_button_new_from_stock (GTK_STOCK_APPLY);
	g_signal_connect (G_OBJECT (pApplyButton), "clicked", G_CALLBACK(on_click_launcher_apply), s_pLauncherWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		pApplyButton,
		FALSE,
		FALSE,
		0);
	
	//\_____________ On ajoute la barre d'etat.
	GtkWidget *pStatusBar = gtk_statusbar_new ();
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),  // pMainVBox
		pStatusBar,
		FALSE,
		FALSE,
		0);
	g_object_set_data (G_OBJECT (s_pLauncherWindow), "status-bar", pStatusBar);
	
	gtk_window_resize (GTK_WINDOW (s_pLauncherWindow), CAIRO_DOCK_LAUNCHER_PANEL_WIDTH, CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT);
	
	gtk_widget_show_all (s_pLauncherWindow);
	cairo_dock_dialog_window_created ();
	
	g_signal_connect (G_OBJECT (s_pLauncherWindow),
		"delete-event",
		G_CALLBACK (on_delete_launcher_gui),
		NULL);
	
	//\_____________ On selectionne l'entree courante.
	_select_item (pIcon);
	
	return s_pLauncherWindow;
}


static void refresh_gui (void)
{
	if (s_pLauncherWindow == NULL)
		return ;
	Icon *pCurrentIcon = g_object_get_data (G_OBJECT (s_pLauncherWindow), "current-icon");
	
	_delete_current_launcher_widget ();
	
	GtkTreeModel *model = _build_tree_model();
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (s_pLauncherTreeView), GTK_TREE_MODEL (model));
	g_object_unref (model);
	
	_select_item (pCurrentIcon);
	
	gtk_widget_show_all (s_pLauncherWindow);
}


void cairo_dock_register_default_launcher_gui_backend (void)
{
	CairoDockLauncherGuiBackend *pBackend = g_new0 (CairoDockLauncherGuiBackend, 1);
	
	pBackend->show_gui 	= show_gui;
	pBackend->refresh_gui 	= refresh_gui;
	
	cairo_dock_register_launcher_gui_backend (pBackend);
}

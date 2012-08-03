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
#include <sys/time.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "config.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-commons.h"
#include "cairo-dock-widget-items.h"

#define CAIRO_DOCK_LAUNCHER_PANEL_WIDTH 1200 // matttbe: 800
#define CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT 700 // matttbe: 500
#define CAIRO_DOCK_LEFT_PANE_MIN_WIDTH 100
#define CAIRO_DOCK_LEFT_PANE_DEFAULT_WIDTH 340 // matttbe: 200
#define CAIRO_DOCK_RIGHT_PANE_MIN_WIDTH 800 // matttbe: 500

#define CAIRO_DOCK_ITEM_ICON_SIZE 32

extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cCurrentThemePath;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern CairoDock *g_pMainDock;

static void _add_one_dock_to_model (CairoDock *pDock, GtkTreeStore *model, GtkTreeIter *pParentIter);
static void on_row_inserted (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, ItemsWidget *pItemsWidget);
static void on_row_deleted (GtkTreeModel *model, GtkTreePath *path, ItemsWidget *pItemsWidget);
static void _items_widget_apply (CDWidget *pCdWidget);

typedef enum {
	CD_MODEL_NAME = 0,  // displayed name
	CD_MODEL_PIXBUF,  // icon image
	CD_MODEL_ICON,  // Icon (for launcher/separator/sub-dock/applet)
	CD_MODEL_CONTAINER,  // CairoContainer (for main docks)
	CD_MODEL_MODULE,  // CairoDockModuleInstance (for plug-ins with no icon)
	CD_MODEL_NB_COLUMNS
	} CDModelColumns;


  ///////////
 // MODEL //
///////////

static gboolean _search_item_in_line (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer *data)
{
	gpointer pItem = NULL;
	gtk_tree_model_get (model, iter,
		GPOINTER_TO_INT (data[1]), &pItem, -1);
	if (pItem == data[0])
	{
		//g_print (" found !\n");
		memcpy (data[2], iter, sizeof (GtkTreeIter));
		data[3] = GINT_TO_POINTER (TRUE);
		return TRUE;  // stop iterating.
	}
	return FALSE;
}
static gboolean _search_item_in_model (GtkWidget *pTreeView, gpointer pItem, CDModelColumns iItemType, GtkTreeIter *iter)
{
	if (pItem == NULL)
		return FALSE;
	//g_print ("%s (%s)\n", __func__, ((Icon*)pItem)->cName);
	GtkTreeModel * model = gtk_tree_view_get_model (GTK_TREE_VIEW (pTreeView));
	gpointer data[4] = {pItem, GINT_TO_POINTER (iItemType), iter, GINT_TO_POINTER (FALSE)};
	gtk_tree_model_foreach (model,
		(GtkTreeModelForeachFunc) _search_item_in_line,
		data);
	return GPOINTER_TO_INT (data[3]);
}

static void _delete_current_launcher_widget (ItemsWidget *pItemsWidget)
{
	g_return_if_fail (pItemsWidget != NULL);
	
	if (pItemsWidget->pCurrentLauncherWidget != NULL)
	{
		gtk_widget_destroy (pItemsWidget->pCurrentLauncherWidget);
		pItemsWidget->pCurrentLauncherWidget = NULL;
	}
	GSList *pWidgetList = pItemsWidget->widget.pWidgetList;
	if (pWidgetList != NULL)
	{
		cairo_dock_free_generated_widget_list (pWidgetList);
		pItemsWidget->widget.pWidgetList = NULL;
	}
	GPtrArray *pDataGarbage = pItemsWidget->widget.pDataGarbage;
	if (pDataGarbage != NULL)
	{
		g_ptr_array_free (pDataGarbage, TRUE);
		pItemsWidget->widget.pDataGarbage = NULL;
	}
	g_free (pItemsWidget->cPrevPath);
	pItemsWidget->cPrevPath = NULL;
	pItemsWidget->pCurrentIcon = NULL;
	pItemsWidget->pCurrentContainer = NULL;
	pItemsWidget->pCurrentModuleInstance = NULL;
}

static gboolean _on_select_one_item_in_tree (GtkTreeSelection * selection, GtkTreeModel * model, GtkTreePath * path, gboolean path_currently_selected, ItemsWidget *pItemsWidget)
{
	cd_debug ("%s (path_currently_selected:%d, %s)", __func__, path_currently_selected, gtk_tree_path_to_string(path));
	if (path_currently_selected)
		return TRUE;
	
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter (model, &iter, path))
		return FALSE;
	gchar *cPath = gtk_tree_path_to_string (path);
	if (cPath && pItemsWidget->cPrevPath && strcmp (pItemsWidget->cPrevPath, cPath) == 0)
	{
		g_free (cPath);
		return TRUE;
	}
	g_free (pItemsWidget->cPrevPath);
	pItemsWidget->cPrevPath = cPath;
	
	cd_debug ("load widgets");
	// delete previous widgets
	_delete_current_launcher_widget (pItemsWidget);  // reset all data
	
	// get new current item
	gchar *cName = NULL;
	Icon *pIcon = NULL;
	CairoContainer *pContainer = NULL;
	CairoDockModuleInstance *pInstance = NULL;
	gtk_tree_model_get (model, &iter,
		CD_MODEL_NAME, &cName,
		CD_MODEL_ICON, &pIcon,
		CD_MODEL_CONTAINER, &pContainer,
		CD_MODEL_MODULE, &pInstance, -1);
	if (CAIRO_DOCK_IS_APPLET (pIcon))
		pInstance = pIcon->pModuleInstance;
	///gtk_window_set_title (GTK_WINDOW (pLauncherWindow), cName);
	g_free (cName);
	
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = NULL;
	
	// load conf file.
	if (pInstance != NULL)
	{
		// open applet's conf file.
		GKeyFile* pKeyFile = cairo_dock_open_key_file (pInstance->cConfFilePath);
		g_return_val_if_fail (pKeyFile != NULL, FALSE);
		
		// build applet's widgets.
		pDataGarbage = g_ptr_array_new ();
		gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", pInstance->pModule->pVisitCard->cShareDataDir, pInstance->pModule->pVisitCard->cConfFileName);
		pItemsWidget->pCurrentLauncherWidget = cairo_dock_build_key_file_widget (pKeyFile,
			pInstance->pModule->pVisitCard->cGettextDomain,
			pItemsWidget->widget.pWidget,
			&pWidgetList,
			pDataGarbage,
			cOriginalConfFilePath);
		g_free (cOriginalConfFilePath);
		
		// load custom widgets
		if (pInstance->pModule->pInterface->load_custom_widget != NULL)
		{
			pItemsWidget->widget.pWidgetList = pWidgetList;  // must set that here so that the call to load_custom_widget() can find the loaded widgets
			pItemsWidget->widget.pDataGarbage = pDataGarbage;
			pInstance->pModule->pInterface->load_custom_widget (pInstance, pKeyFile);  /// TODO: we need to pass 'pWidgetList' to the function...
		}
		
		if (pIcon != NULL)
			pItemsWidget->pCurrentIcon = pIcon;
		else
			pItemsWidget->pCurrentModuleInstance = pInstance;
		
		g_key_file_free (pKeyFile);
	}
	else if (CAIRO_DOCK_IS_DOCK (pContainer))  // ligne correspondante a un dock principal
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		if (!pDock->bIsMainDock)  // pour l'instant le main dock n'a pas de fichier de conf
		{
			// build dock's widgets
			const gchar *cDockName = cairo_dock_search_dock_name (pDock);  // CD_MODEL_NAME contient le nom affiche, qui peut differer.
			g_return_val_if_fail (cDockName != NULL, FALSE);
			cd_message ("%s (%s)", __func__, cDockName);
			
			gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
			if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))  // ne devrait pas arriver mais au cas ou.
			{
				cairo_dock_add_root_dock_config_for_name (cDockName);
			}
			
			pDataGarbage = g_ptr_array_new ();
			pItemsWidget->pCurrentLauncherWidget = cairo_dock_build_conf_file_widget (cConfFilePath,
				NULL,
				pItemsWidget->widget.pWidget,
				&pWidgetList,
				pDataGarbage,
				NULL);
			
			pItemsWidget->pCurrentContainer = CAIRO_CONTAINER (pDock);
			
			g_free (cConfFilePath);
		}
		else  // main dock, we display a message
		{
			pDataGarbage = g_ptr_array_new ();
			gchar *cDefaultMessage = g_strdup_printf ("<b><span font_desc=\"Sans 14\">%s</span></b>", _("Main dock's parameters are available in the main configuration window."));
			pItemsWidget->pCurrentLauncherWidget = cairo_dock_gui_make_preview_box (pItemsWidget->widget.pWidget,
				NULL,  // no selection widget
				FALSE,  // vertical packaging
				0,  // no info bar
				cDefaultMessage,
				CAIRO_DOCK_SHARE_DATA_DIR"/images/"CAIRO_DOCK_LOGO,
				pDataGarbage);
			g_free (cDefaultMessage);
		}
	}
	else if (pIcon && pIcon->cDesktopFileName != NULL)
	{
		//g_print ("on presente %s...\n", pIcon->cDesktopFileName);
		gchar *cConfFilePath = (*pIcon->cDesktopFileName == '/' ? g_strdup (pIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pIcon->cDesktopFileName));
		
		// build launcher's widgets
		pDataGarbage = g_ptr_array_new ();
		pItemsWidget->pCurrentLauncherWidget = cairo_dock_build_conf_file_widget (cConfFilePath,
			NULL,
			pItemsWidget->widget.pWidget,
			&pWidgetList,
			pDataGarbage,
			NULL);
		
		pItemsWidget->pCurrentIcon = pIcon;
		
		g_free (cConfFilePath);
	}
	
	// set widgets in the main window.
	if (pItemsWidget->pCurrentLauncherWidget != NULL)
	{
		gtk_paned_pack2 (GTK_PANED (pItemsWidget->widget.pWidget), pItemsWidget->pCurrentLauncherWidget, TRUE, FALSE);
		
		pItemsWidget->widget.pWidgetList = pWidgetList;
		pItemsWidget->widget.pDataGarbage = pDataGarbage;
		
		gtk_widget_show_all (pItemsWidget->pCurrentLauncherWidget);
	}
	return TRUE;
}

static void _add_one_icon_to_model (Icon *pIcon, GtkTreeStore *model, GtkTreeIter *pParentIter)
{
	if (!pIcon->cDesktopFileName && ! CAIRO_DOCK_IS_APPLET (pIcon))
		return;
	
	if (cairo_dock_icon_is_being_removed (pIcon))
		return;
	
	GtkTreeIter iter;
	GError *erreur = NULL;
	GdkPixbuf *pixbuf = NULL;
	gchar *cImagePath = NULL;
	const gchar *cName;
	
	// set an image.
	if (pIcon->cFileName != NULL)
	{
		cImagePath = cairo_dock_search_icon_s_path (pIcon->cFileName, CAIRO_DOCK_ITEM_ICON_SIZE);
	}
	if (cImagePath == NULL || ! g_file_test (cImagePath, G_FILE_TEST_EXISTS))
	{
		g_free (cImagePath);
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
		{
			if (myIconsParam.cSeparatorImage)
				cImagePath = cairo_dock_search_image_s_path (myIconsParam.cSeparatorImage);
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
				cImagePath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/icons/"CAIRO_DOCK_DEFAULT_ICON_NAME);
			}
		}
	}
	
	if (cImagePath != NULL)
	{
		pixbuf = gdk_pixbuf_new_from_file_at_size (cImagePath, CAIRO_DOCK_ITEM_ICON_SIZE, CAIRO_DOCK_ITEM_ICON_SIZE, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
	}
	
	// set a name
	if (CAIRO_DOCK_IS_USER_SEPARATOR (pIcon))  // separator
		cName = "–––––––––––––––";
	else if (CAIRO_DOCK_IS_APPLET (pIcon))  // applet
		cName = pIcon->pModuleInstance->pModule->pVisitCard->cTitle;
	else  // launcher
		cName = (pIcon->cInitialName ? pIcon->cInitialName : pIcon->cName);
	
	// add an entry in the tree view.
	gtk_tree_store_append (model, &iter, pParentIter);
	gtk_tree_store_set (model, &iter,
		CD_MODEL_NAME, cName,
		CD_MODEL_PIXBUF, pixbuf,
		CD_MODEL_ICON, pIcon,
		-1);
	
	// recursively add the sub-icons.
	if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon) && pIcon->pSubDock != NULL)
	{
		_add_one_dock_to_model (pIcon->pSubDock, model, &iter);
	}
	
	// reset all.
	g_free (cImagePath);
	if (pixbuf)
		g_object_unref (pixbuf);
}
static void _add_one_dock_to_model (CairoDock *pDock, GtkTreeStore *model, GtkTreeIter *pParentIter)
{
	GList *ic;
	Icon *pIcon;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)  // add each icons.
	{
		pIcon = ic->data;
		_add_one_icon_to_model (pIcon, model, pParentIter);
	}
}

static void _add_one_root_dock_to_model (const gchar *cName, CairoDock *pDock, GtkTreeStore *model)
{
	if (pDock->iRefCount != 0)
		return ;
	GtkTreeIter iter;
	
	// on ajoute une ligne pour le dock.
	gtk_tree_store_append (model, &iter, NULL);
	gchar *cUserName = cairo_dock_get_readable_name_for_fock (pDock);
	gtk_tree_store_set (model, &iter,
		CD_MODEL_NAME, cUserName ? cUserName : cName,
		CD_MODEL_CONTAINER, pDock,
		-1);
	g_free (cUserName);
	
	// on ajoute chaque lanceur.
	_add_one_dock_to_model (pDock, model, &iter);
}

static gboolean _add_one_desklet_to_model (CairoDesklet *pDesklet, GtkTreeStore *model)
{
	if (pDesklet->pIcon == NULL)
		return FALSE;
	
	// on ajoute l'icone du desklet.
	_add_one_icon_to_model (pDesklet->pIcon, model, NULL);  // les sous-icones du desklet ne nous interessent pas.
	
	return FALSE; // FALSE => keep going
}

static inline void _add_one_module (const gchar *cModuleName, CairoDockModuleInstance *pModuleInstance, GtkTreeStore *model)
{
	GtkTreeIter iter;
	gtk_tree_store_append (model, &iter, NULL);
	
	GdkPixbuf *pixbuf = NULL;
	gchar *cImagePath = g_strdup (pModuleInstance->pModule->pVisitCard->cIconFilePath);
	if (cImagePath != NULL)
		pixbuf = gdk_pixbuf_new_from_file_at_size (cImagePath, CAIRO_DOCK_ITEM_ICON_SIZE, CAIRO_DOCK_ITEM_ICON_SIZE, NULL);
	gtk_tree_store_set (model, &iter,
		CD_MODEL_NAME, cModuleName,
		CD_MODEL_PIXBUF, pixbuf,
		CD_MODEL_MODULE, pModuleInstance,
		-1);
	g_free (cImagePath);
	if (pixbuf)
		g_object_unref (pixbuf);
}
static gboolean _add_one_module_to_model (const gchar *cModuleName, CairoDockModule *pModule, GtkTreeStore *model)
{
	if (pModule->pVisitCard->iCategory != CAIRO_DOCK_CATEGORY_BEHAVIOR && pModule->pVisitCard->iCategory != CAIRO_DOCK_CATEGORY_THEME && ! cairo_dock_module_is_auto_loaded (pModule) && pModule->pInstancesList != NULL)
	{
		CairoDockModuleInstance *pModuleInstance = pModule->pInstancesList->data;
		if (pModuleInstance->pIcon == NULL || (pModuleInstance->pDock && !pModuleInstance->pIcon->cParentDockName))
		{
			// on ajoute une ligne pour l'applet.
			_add_one_module (cModuleName, pModuleInstance, model);
		}
	}
	return FALSE; // FALSE => keep going
}

static void _select_item (ItemsWidget *pItemsWidget, Icon *pIcon, CairoContainer *pContainer, CairoDockModuleInstance *pModuleInstance)
{
	g_free (pItemsWidget->cPrevPath);
	pItemsWidget->cPrevPath = NULL;
	GtkTreeIter iter;
	if (_search_item_in_model (pItemsWidget->pTreeView, pIcon ? (gpointer)pIcon : pContainer ? (gpointer)pContainer : (gpointer)pModuleInstance, pIcon ? CD_MODEL_ICON : pContainer ? CD_MODEL_CONTAINER : CD_MODEL_MODULE, &iter))
	{
		GtkTreeModel * model = gtk_tree_view_get_model (GTK_TREE_VIEW (pItemsWidget->pTreeView));
		GtkTreePath *path =  gtk_tree_model_get_path (model, &iter);
		gtk_tree_view_expand_to_path (GTK_TREE_VIEW (pItemsWidget->pTreeView), path);
		gtk_tree_path_free (path);
		
		GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pItemsWidget->pTreeView));
		gtk_tree_selection_unselect_all (pSelection);
		gtk_tree_selection_select_iter (pSelection, &iter);
	}
	else
	{
		cd_warning ("item not found");
		_delete_current_launcher_widget (pItemsWidget);
		///gtk_window_set_title (GTK_WINDOW (pItemsWidget), _("Launcher configuration"));
	}
}

static GtkTreeModel *_build_tree_model (ItemsWidget *pItemsWidget)
{
	GtkTreeStore *model = gtk_tree_store_new (CD_MODEL_NB_COLUMNS,
		G_TYPE_STRING,  // displayed name
		GDK_TYPE_PIXBUF,  // displayed icon
		G_TYPE_POINTER,  // Icon
		G_TYPE_POINTER,  // Container
		G_TYPE_POINTER);  // Module
	cairo_dock_foreach_docks ((GHFunc) _add_one_root_dock_to_model, model);  // on n'utilise pas cairo_dock_foreach_root_docks(), de facon a avoir le nom du dock.
	cairo_dock_foreach_desklet ((CairoDockForeachDeskletFunc) _add_one_desklet_to_model, model);
	cairo_dock_foreach_module ((GHRFunc)_add_one_module_to_model, model);
	CairoDockModule *pModule = cairo_dock_find_module_from_name ("Help");
	if (pModule != NULL)
	{
		if (pModule->pInstancesList == NULL)  // Help is not active, so is not already in the icons list.
		{
			///CairoDockModuleInstance *pModuleInstance = pModule->pInstancesList->data;
			///_add_one_module ("Help", pModuleInstance, model);
			/// add it ...
		}
	}
	g_signal_connect (G_OBJECT (model), "row-inserted", G_CALLBACK (on_row_inserted), pItemsWidget);
	g_signal_connect (G_OBJECT (model), "row-deleted", G_CALLBACK (on_row_deleted), pItemsWidget);
	return GTK_TREE_MODEL (model);
}


  //////////////////
 // USER ACTIONS //
//////////////////

static GtkTreeIter lastInsertedIter;
static gboolean s_bHasPendingInsertion = FALSE;
static struct timeval lastTime = {0, 0};

static void on_row_inserted (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, ItemsWidget *pItemsWidget)
{
	// we only receive this event from an intern drag'n'drop
	// when we receive this event, the row is still empty, so we can't perform any task here.
	// so we just remember the event, and treat it later (when we receive the "row-deleted" signal).
	memcpy (&lastInsertedIter, iter, sizeof (GtkTreeIter));
	gettimeofday (&lastTime, NULL);
	s_bHasPendingInsertion = TRUE;
}
static void on_row_deleted (GtkTreeModel *model, GtkTreePath *path, ItemsWidget *pItemsWidget)
{
	g_print ("- row\n");
	// when drag'n'droping a row, the "row-deleted" signal is emitted after the "row-inserted"
	// however the row pointed by 'path' is already invalid, but the previously inserted iter is now filled, and we remembered it, so we can use it now.
	if (s_bHasPendingInsertion)
	{
		struct timeval current_time;
		gettimeofday (&current_time, NULL);
		if ((current_time.tv_sec + current_time.tv_usec / 1.e6) - (lastTime.tv_sec + lastTime.tv_usec / 1.e6) < .3)  // just to be sure we don't get a "row-deleted" that has no link with a drag'n'drop (it's a rough precaution, I've never seen this case happen).
		{
			// get the item that has been moved.
			gchar *cName = NULL;
			Icon *pIcon = NULL;
			CairoContainer *pContainer = NULL;
			CairoDockModuleInstance *pInstance = NULL;
			gtk_tree_model_get (model, &lastInsertedIter,
				CD_MODEL_NAME, &cName,
				CD_MODEL_ICON, &pIcon,
				CD_MODEL_CONTAINER, &pContainer,
				CD_MODEL_MODULE, &pInstance, -1);
			g_print ("+ row %s\n", cName);
			
			if (pIcon)  // launcher/separator/sub-dock-icon or applet
			{
				// get the icon and container.
				/**if (pIcon == NULL)
					pIcon = pInstance->pIcon;*/
				if (pContainer == NULL)
				{
					if (pInstance != NULL)
						pContainer = pInstance->pContainer;
					else if (pIcon != NULL)
					{
						pContainer = CAIRO_CONTAINER (cairo_dock_search_dock_from_name (pIcon->cParentDockName));
					}
				}
				
				// get the new parent in the tree, hence the possibly new container
				GtkTreeIter parent_iter;
				if (CAIRO_DOCK_IS_DOCK (pContainer)  // with this, we prevent from moving desklets; we may add this possibility it later.
				&& gtk_tree_model_iter_parent (model, &parent_iter, &lastInsertedIter))
				{
					gchar *cParentName = NULL;
					Icon *pParentIcon = NULL;
					CairoContainer *pParentContainer = NULL;
					gtk_tree_model_get (model, &parent_iter,
						CD_MODEL_NAME, &cParentName,
						CD_MODEL_ICON, &pParentIcon,
						CD_MODEL_CONTAINER, &pParentContainer, -1);
					g_print (" parent: %s, %p, %p\n", cParentName, pParentIcon, pParentContainer);
					
					if (pParentContainer == NULL && pParentIcon != NULL)  // dropped on an icon, if it's a sub-dock icon, insert into the sub-dock, else do as if we dropped next to the icon.
					{
						if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pParentIcon))  // put our item in the sub-dock
						{
							pParentContainer = CAIRO_CONTAINER (pParentIcon->pSubDock);
						}
						else  // not an icon that can contain our item, so place it next to it.
						{
							pParentContainer = CAIRO_CONTAINER (cairo_dock_search_dock_from_name (pParentIcon->cParentDockName));
							// we'll search the parent instead.
							lastInsertedIter = parent_iter;
							gtk_tree_model_iter_parent (model, &parent_iter, &lastInsertedIter);
							g_print (" search parent %s\n", pParentIcon->cParentDockName);
						}
					}
					if (CAIRO_DOCK_IS_DOCK (pParentContainer))  // not nul and dock-type.
					{
						// if it has changed, update the conf file and the icon.
						if (pParentContainer != pContainer)
						{
							g_print (" parent has changed\n");
							const gchar *cNewParentDockName = cairo_dock_search_dock_name (CAIRO_DOCK (pParentContainer));
							if (cNewParentDockName != NULL)
							{
								cairo_dock_write_container_name_in_conf_file (pIcon, cNewParentDockName);
							}
							
							g_print (" reload the icon...\n");
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
						}
						
						// find the new order of the row in the tree
						double fOrder = 0;
						GtkTreeIter it;
						Icon *pLeftIcon = NULL;
						gchar *last_iter_s = gtk_tree_model_get_string_from_iter (model, &lastInsertedIter);
						g_print ("search for '%s'\n", last_iter_s);
						
						///gtk_tree_model_get_iter_first (model, &iter);
						if (gtk_tree_model_iter_children (model, &it, &parent_iter))  // point on the first iter.
						{
							g_print (" got first iter\n");
							// iterate on the rows until we reach our row.
							do
							{
								gchar *iter_s = gtk_tree_model_get_string_from_iter (model, &it);
								g_print (" test iter %s / %s\n", iter_s, last_iter_s);
								if (strcmp (last_iter_s, iter_s) == 0)  // it's our row
								{
									g_free (iter_s);
									g_print (" reached our row, break\n");
									break;
								}
								else  // not yet our row, let's remember the left icon.
								{
									gchar *name=NULL;
									gtk_tree_model_get (model, &it,
										CD_MODEL_NAME, &name,
										CD_MODEL_ICON, &pLeftIcon, -1);
									g_print ("  (%s)\n", name);
								}
								g_free (iter_s);
							}
							while (gtk_tree_model_iter_next (model, &it));
						}
						g_free (last_iter_s);
						
						// move the icon (will update the conf file and trigger the signal to reload the GUI).
						g_print (" move the icon...\n");
						cairo_dock_move_icon_after_icon (CAIRO_DOCK (pParentContainer), pIcon, pLeftIcon);
					}
					else  // the row may have been dropped on a launcher or a desklet, in which case we must reload the model because this has no meaning.
					{
						cairo_dock_items_widget_reload (pItemsWidget);
					}
					
				}  // else this row has no parent, so it was either a main dock or a desklet, and we have nothing to do.
				else
				{
					cairo_dock_items_widget_reload (pItemsWidget);
				}
			}
			else  // no icon (for instance a plug-in like Remote-control)
			{
				cairo_dock_items_widget_reload (pItemsWidget);
			}
		}
		
		s_bHasPendingInsertion = FALSE;
	}
}

static void _on_select_remove_item (GtkMenuItem *pMenuItem, GtkWidget *pTreeView)
{
	// get the selected line
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pTreeView));
	GtkTreeModel *pModel = NULL;
	GList *paths = gtk_tree_selection_get_selected_rows (pSelection, &pModel);
	g_return_if_fail (paths != NULL && pModel != NULL);
	GtkTreePath *path = paths->data;
	
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter (pModel, &iter, path))
		return;
	
	g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (paths);
	
	// get the corresponding item, and the next line.
	Icon *pIcon = NULL;
	CairoContainer *pContainer = NULL;
	CairoDockModuleInstance *pInstance = NULL;
	gtk_tree_model_get (pModel, &iter,
		CD_MODEL_ICON, &pIcon,
		CD_MODEL_CONTAINER, &pContainer,
		CD_MODEL_MODULE, &pInstance, -1);
	
	if (!gtk_tree_model_iter_next (pModel, &iter))
		gtk_tree_model_get_iter_first (pModel, &iter);
	
	// remove it.
	if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
		|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))  // launcher/separator/sub-dock
	{
		cairo_dock_trigger_icon_removal_from_dock (pIcon);
	}
	else if (CAIRO_DOCK_IS_APPLET (pIcon))  // applet
	{
		cairo_dock_trigger_icon_removal_from_dock (pIcon);
	}
	else if (pInstance != NULL)  // plug-in
	{
		cairo_dock_remove_module_instance (pInstance);
	}
	else if (CAIRO_DOCK_IS_DOCK (pContainer))  // main-dock
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		if (! pDock->bIsMainDock)
		{
			cairo_dock_remove_icons_from_dock (pDock, NULL, NULL);
		
			const gchar *cDockName = cairo_dock_search_dock_name (pDock);
			cairo_dock_destroy_dock (pDock, cDockName);
		}
	}
	
	// select the next item to avoid having no selection
	gtk_tree_selection_unselect_all (pSelection);
	gtk_tree_selection_select_iter (pSelection, &iter);
}


static void _items_widget_reset (CDWidget *pCdWidget)
{
	ItemsWidget *pItemsWidget = ITEMS_WIDGET (pCdWidget);
	g_free (pItemsWidget->cPrevPath);  // internal widgets will be destroyed with the parent.
}


static gboolean on_button_press_event (GtkWidget *pTreeView,
	GdkEventButton *pButton,
	gpointer data)
{
	if (pButton->button == 3)  // clic droit.
	{
		GtkWidget *pMenu = gtk_menu_new ();
			
		GtkWidget *pMenuItem;
		
		/// TODO: check that we can actually remove it (ex.: not the main dock), and maybe display the item's name...
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Remove this item"), GTK_STOCK_REMOVE, G_CALLBACK (_on_select_remove_item), pMenu, pTreeView);
		
		gtk_widget_show_all (pMenu);
		
		gtk_menu_popup (GTK_MENU (pMenu),
			NULL,
			NULL,
			NULL,
			NULL,
			pButton->button,
			gtk_get_current_event_time ());
	}
	return FALSE;
}

ItemsWidget *cairo_dock_items_widget_new (void)
{
	ItemsWidget *pItemsWidget = g_new0 (ItemsWidget, 1);
	pItemsWidget->widget.iType = WIDGET_ITEMS;
	
	GtkWidget *pLauncherPane;
	#if (GTK_MAJOR_VERSION < 3)
	pLauncherPane = gtk_hpaned_new ();
	#else
	pLauncherPane = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	#endif
	pItemsWidget->widget.pWidget = pLauncherPane;
	pItemsWidget->widget.apply = _items_widget_apply;
	pItemsWidget->widget.reset = _items_widget_reset;
	
	//\_____________ On construit l'arbre des launceurs.
	GtkTreeModel *model = _build_tree_model (pItemsWidget);
	
	//\_____________ On construit le tree-view avec.
	pItemsWidget->pTreeView = gtk_tree_view_new_with_model (model);
	g_object_unref (model);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (pItemsWidget->pTreeView), FALSE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (pItemsWidget->pTreeView), TRUE);
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (pItemsWidget->pTreeView), TRUE);  // enables drag and drop of rows -> row-inserted and row-deleted signals
	g_signal_connect (G_OBJECT (pItemsWidget->pTreeView),
		"button-release-event",  // on release, so that the clicked line is already selected
		G_CALLBACK (on_button_press_event),
		NULL);
	
	// line selection
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pItemsWidget->pTreeView));
	gtk_tree_selection_set_mode (pSelection, GTK_SELECTION_SINGLE);
	gtk_tree_selection_set_select_function (pSelection,
		(GtkTreeSelectionFunc) _on_select_one_item_in_tree,
		pItemsWidget,
		NULL);
	
	GtkCellRenderer *rend;
	// column icon
	rend = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pItemsWidget->pTreeView), -1, NULL, rend, "pixbuf", 1, NULL);
	// column name
	rend = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pItemsWidget->pTreeView), -1, NULL, rend, "text", 0, NULL);
	
	GtkWidget *pLauncherWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pLauncherWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pLauncherWindow), pItemsWidget->pTreeView);
	gtk_paned_pack1 (GTK_PANED (pLauncherPane), pLauncherWindow, TRUE, FALSE);
	
	//\_____________ On essaie de definir une taille correcte.
	int w = MIN (CAIRO_DOCK_LAUNCHER_PANEL_WIDTH, g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]);
	if (g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] < CAIRO_DOCK_LAUNCHER_PANEL_WIDTH)  // ecran trop petit, on va essayer de reserver au moins R pixels pour le panneau de droite (avec un minimum de L pixels pour celui de gauche).
		gtk_paned_set_position (GTK_PANED (pLauncherPane), MAX (CAIRO_DOCK_LEFT_PANE_MIN_WIDTH, w - CAIRO_DOCK_RIGHT_PANE_MIN_WIDTH));
	else  // we set a default width rather than letting GTK guess the best, because the right panel is more important than the left one.
		gtk_paned_set_position (GTK_PANED (pLauncherPane), CAIRO_DOCK_LEFT_PANE_DEFAULT_WIDTH);
	g_object_set_data (G_OBJECT (pLauncherPane), "frame-width", GINT_TO_POINTER (250));
	
	return pItemsWidget;
}


static void _items_widget_apply (CDWidget *pCdWidget)
{
	ItemsWidget *pItemsWidget = ITEMS_WIDGET (pCdWidget);
	Icon *pIcon = pItemsWidget->pCurrentIcon;
	CairoContainer *pContainer = pItemsWidget->pCurrentContainer;
	CairoDockModuleInstance *pModuleInstance = pItemsWidget->pCurrentModuleInstance;
	
	if (CAIRO_DOCK_IS_APPLET (pIcon))
		pModuleInstance = pIcon->pModuleInstance;
	
	GSList *pWidgetList = pItemsWidget->widget.pWidgetList;
	
	if (pModuleInstance)
	{
		// open the conf file.
		GKeyFile *pKeyFile = cairo_dock_open_key_file (pModuleInstance->cConfFilePath);
		g_return_if_fail (pKeyFile != NULL);
		
		// update the keys with the widgets.
		cairo_dock_update_keyfile_from_widget_list (pKeyFile, pWidgetList);
		
		// if the parent dock doesn't exist (new dock), add a conf file for it with a nominal name.
		if (g_key_file_has_key (pKeyFile, "Icon", "dock name", NULL))
		{
			gchar *cDockName = g_key_file_get_string (pKeyFile, "Icon", "dock name", NULL);
			gboolean bIsDetached = g_key_file_get_boolean (pKeyFile, "Desklet", "initially detached", NULL);
			if (!bIsDetached)
			{
				CairoDock *pDock = cairo_dock_search_dock_from_name (cDockName);
				if (pDock == NULL)
				{
					gchar *cNewDockName = cairo_dock_add_root_dock_config ();
					g_key_file_set_string (pKeyFile, "Icon", "dock name", cNewDockName);
					g_free (cNewDockName);
				}
			}
			g_free (cDockName);
		}
		
		if (pModuleInstance->pModule->pInterface->save_custom_widget != NULL)
			pModuleInstance->pModule->pInterface->save_custom_widget (pModuleInstance, pKeyFile);
		
		// write everything in the conf file.
		cairo_dock_write_keys_to_file (pKeyFile, pModuleInstance->cConfFilePath);
		g_key_file_free (pKeyFile);
		
		// reload module.
		cairo_dock_reload_module_instance (pModuleInstance, TRUE);
	}
	else if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		if (!pDock->bIsMainDock)  // pour l'instant le main dock n'a pas de fichier de conf
		{
			const gchar *cDockName = cairo_dock_search_dock_name (pDock);  // CD_MODEL_NAME contient le nom affiche, qui peut differer.
			g_return_if_fail (cDockName != NULL);
			
			gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
			
			// open the conf file.
			GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
			g_return_if_fail (pKeyFile != NULL);
			
			// update the keys with the widgets.
			cairo_dock_update_keyfile_from_widget_list (pKeyFile, pWidgetList);
			
			// write everything in the conf file.
			cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
			g_key_file_free (pKeyFile);
			g_free (cConfFilePath);
			
			// reload dock's config.
			cairo_dock_reload_one_root_dock (cDockName, pDock);
		}
	}
	else if (pIcon)
	{
		gchar *cConfFilePath = (*pIcon->cDesktopFileName == '/' ? g_strdup (pIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pIcon->cDesktopFileName));
		
		// open the conf file.
		GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
		g_return_if_fail (pKeyFile != NULL);
		
		// update the keys with the widgets.
		cairo_dock_update_keyfile_from_widget_list (pKeyFile, pWidgetList);
		
		// if the parent dock doesn't exist (new dock), add a conf file for it with a nominal name.
		if (g_key_file_has_key (pKeyFile, "Desktop Entry", "Container", NULL))
		{
			gchar *cDockName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Container", NULL);
			CairoDock *pDock = cairo_dock_search_dock_from_name (cDockName);
			if (pDock == NULL)
			{
				gchar *cNewDockName = cairo_dock_add_root_dock_config ();
				g_key_file_set_string (pKeyFile, "Icon", "dock name", cNewDockName);
				g_free (cNewDockName);
			}
			g_free (cDockName);
		}
		
		// write everything in the conf file.
		cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
		g_key_file_free (pKeyFile);
		g_free (cConfFilePath);
		
		// reload widgets.
		cairo_dock_reload_launcher (pIcon);  // prend tout en compte, y compris le redessin et declenche le rechargement de l'IHM.
	}
	cairo_dock_items_widget_reload (pItemsWidget);  // we reload in case the items place has changed (icon's container, dock orientation, etc).
}


  //////////////////
 /// WIDGET API ///
//////////////////

void cairo_dock_items_widget_select_item (ItemsWidget *pItemsWidget, Icon *pIcon, CairoContainer *pContainer, CairoDockModuleInstance *pModuleInstance, int iNotebookPage)
{
	_delete_current_launcher_widget (pItemsWidget);  // pItemsWidget->pCurrentLauncherWidget <- 0
	
	_select_item (pItemsWidget, pIcon, pContainer, pModuleInstance);  // set pItemsWidget->pCurrentLauncherWidget
	
	if (pItemsWidget->pCurrentLauncherWidget && GTK_IS_NOTEBOOK (pItemsWidget->pCurrentLauncherWidget) && iNotebookPage != -1)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (pItemsWidget->pCurrentLauncherWidget), iNotebookPage);
	
	gtk_widget_show_all (pItemsWidget->pCurrentLauncherWidget);
}


void cairo_dock_items_widget_reload (ItemsWidget *pItemsWidget)
{
	g_print ("%s ()\n", __func__);
	// remember the current item and page
	int iNotebookPage;
	if (pItemsWidget->pCurrentLauncherWidget && GTK_IS_NOTEBOOK (pItemsWidget->pCurrentLauncherWidget))
		iNotebookPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (pItemsWidget->pCurrentLauncherWidget));
	else
		iNotebookPage = -1;
	
	// reload the tree-view
	GtkTreeModel *model = _build_tree_model (pItemsWidget);
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (pItemsWidget->pTreeView), GTK_TREE_MODEL (model));
	g_object_unref (model);
	
	// reload the current icon/container's widgets by reselecting the current line.
	Icon *pCurrentIcon = pItemsWidget->pCurrentIcon;
	CairoContainer *pCurrentContainer = pItemsWidget->pCurrentContainer;
	CairoDockModuleInstance *pCurrentModuleInstance = pItemsWidget->pCurrentModuleInstance;
	
	_delete_current_launcher_widget (pItemsWidget);  // pItemsWidget->pCurrentLauncherWidget <- 0
	
	_select_item (pItemsWidget, pCurrentIcon, pCurrentContainer, pCurrentModuleInstance);  // set pItemsWidget->pCurrentLauncherWidget
	
	if (pItemsWidget->pCurrentLauncherWidget && GTK_IS_NOTEBOOK (pItemsWidget->pCurrentLauncherWidget) && iNotebookPage != -1)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (pItemsWidget->pCurrentLauncherWidget), iNotebookPage);
	
	gtk_widget_show_all (pItemsWidget->widget.pWidget);
}

void cairo_dock_items_widget_update_desklet_params (ItemsWidget *pItemsWidget, CairoDesklet *pDesklet)
{
	g_return_if_fail (pItemsWidget != NULL);
	// check that it's about the current item
	if (pDesklet == NULL || pDesklet->pIcon == NULL)
		return;
	
	Icon *pIcon = pItemsWidget->pCurrentIcon;
	if (pIcon != pDesklet->pIcon)
		return;
	
	// update the corresponding widgets
	GSList *pWidgetList = pItemsWidget->widget.pWidgetList;
	cairo_dock_update_desklet_widgets (pDesklet, pWidgetList);
}

void cairo_dock_items_widget_update_desklet_visibility_params (ItemsWidget *pItemsWidget, CairoDesklet *pDesklet)
{
	g_return_if_fail (pItemsWidget != NULL);
	// check that it's about the current item
	if (pDesklet == NULL || pDesklet->pIcon == NULL)
		return;
	
	Icon *pIcon = pItemsWidget->pCurrentIcon;
	if (pIcon != pDesklet->pIcon)
		return;
	
	// update the corresponding widgets
	GSList *pWidgetList = pItemsWidget->widget.pWidgetList;
	cairo_dock_update_desklet_visibility_widgets (pDesklet, pWidgetList);
}

void cairo_dock_items_widget_update_module_instance_container (ItemsWidget *pItemsWidget, CairoDockModuleInstance *pInstance, gboolean bDetached)
{
	g_return_if_fail (pItemsWidget != NULL);
	// check that it's about the current item
	if (pInstance == NULL)
		return;
	
	Icon *pIcon = pItemsWidget->pCurrentIcon;
	if (pIcon != pInstance->pIcon)  // pour un module qui se detache, il suffit de chercher parmi les applets.
		return;
	
	// update the corresponding widgets
	GSList *pWidgetList = pItemsWidget->widget.pWidgetList;
	cairo_dock_update_is_detached_widget (bDetached, pWidgetList);
}

void cairo_dock_items_widget_reload_current_widget (ItemsWidget *pItemsWidget, CairoDockModuleInstance *pInstance, int iShowPage)
{
	g_return_if_fail (pItemsWidget != NULL && pItemsWidget->pTreeView != NULL);
	cd_debug ("%s ()", __func__);
	
	// check that it's about the current item
	Icon *pIcon = pItemsWidget->pCurrentIcon;
	CairoDockModuleInstance *pModuleInstance = pItemsWidget->pCurrentModuleInstance;
	if (pInstance)
	{
		if (pIcon)
		{
			if (pIcon->pModuleInstance != pInstance)
				return;
		}
		else if (pModuleInstance)
		{
			if (pModuleInstance != pInstance)
				return;
		}
	}
	
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pItemsWidget->pTreeView));
	GtkTreeModel *pModel = NULL;
	GList *paths = gtk_tree_selection_get_selected_rows (pSelection, &pModel);
	g_return_if_fail (paths != NULL && pModel != NULL);
	GtkTreePath *path = paths->data;
	
	int iNotebookPage;
	if ((GTK_IS_NOTEBOOK (pItemsWidget->pCurrentLauncherWidget)))
	{
		if (iShowPage == -1)
			iNotebookPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (pItemsWidget->pCurrentLauncherWidget));
		else
			iNotebookPage = iShowPage;
	}
	else
		iNotebookPage = -1;
	
	g_free (pItemsWidget->cPrevPath);
	pItemsWidget->cPrevPath = NULL;
	_on_select_one_item_in_tree (pSelection, pModel, path, FALSE, pItemsWidget);  // on appelle la callback nous-memes, car la ligne est deja selectionnee, donc le parametre 'path_currently_selected' sera a TRUE. de plus on veut pouvoir mettre la page du notebook.
	
	if (iNotebookPage != -1)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (pItemsWidget->pCurrentLauncherWidget), iNotebookPage);
	}
	
	g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (paths);
}

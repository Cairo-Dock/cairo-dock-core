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
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-gui-items.h"

#define CAIRO_DOCK_LAUNCHER_PANEL_WIDTH 1200
#define CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT 700
#define CAIRO_DOCK_LEFT_PANE_MIN_WIDTH 100
#define CAIRO_DOCK_LEFT_PANE_DEFAULT_WIDTH 340
#define CAIRO_DOCK_RIGHT_PANE_MIN_WIDTH 800

extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cCurrentThemePath;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern CairoDock *g_pMainDock;

static GtkWidget *s_pLauncherWindow = NULL;  // partage avec le backend 'simple'.
static GtkWidget *s_pCurrentLauncherWidget = NULL;
static GtkWidget *s_pLauncherTreeView = NULL;
static GtkWidget *s_pLauncherPane = NULL;
static gchar *cPrevPath = NULL;

static void _add_one_dock_to_model (CairoDock *pDock, GtkTreeStore *model, GtkTreeIter *pParentIter);
static void reload_items (void);
static void on_row_inserted (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void on_row_deleted (GtkTreeModel *model, GtkTreePath *path, gpointer data);

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

static void _delete_current_launcher_widget (GtkWidget *pLauncherWindow)
{
	g_return_if_fail (pLauncherWindow != NULL);
	
	if (s_pCurrentLauncherWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentLauncherWidget);
		s_pCurrentLauncherWidget = NULL;
	}
	GSList *pWidgetList = g_object_get_data (G_OBJECT (pLauncherWindow), "widget-list");
	if (pWidgetList != NULL)
	{
		cairo_dock_free_generated_widget_list (pWidgetList);
		g_object_set_data (G_OBJECT (pLauncherWindow), "widget-list", NULL);
	}
	GPtrArray *pDataGarbage = g_object_get_data (G_OBJECT (pLauncherWindow), "garbage");
	if (pDataGarbage != NULL)
	{
		cd_debug ("free garbage...");
		g_ptr_array_free (pDataGarbage, TRUE);
		g_object_set_data (G_OBJECT (pLauncherWindow), "garbage", NULL);
	}
	
	g_object_set_data (G_OBJECT (pLauncherWindow), "current-icon", NULL);
	g_object_set_data (G_OBJECT (pLauncherWindow), "current-container", NULL);
	g_object_set_data (G_OBJECT (pLauncherWindow), "current-module", NULL);
}

static gboolean _on_select_one_item_in_tree (GtkTreeSelection * selection, GtkTreeModel * model, GtkTreePath * path, gboolean path_currently_selected, GtkWidget *pLauncherWindow)
{
	cd_debug ("%s (path_currently_selected:%d, %s)", __func__, path_currently_selected, gtk_tree_path_to_string(path));
	if (path_currently_selected)
		return TRUE;
	
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter (model, &iter, path))
		return FALSE;
	gchar *cPath = gtk_tree_path_to_string (path);
	if (cPath && cPrevPath && strcmp (cPrevPath, cPath) == 0)
	{
		g_free (cPath);
		return TRUE;
	}
	g_free (cPrevPath);
	cPrevPath = cPath;
	
	cd_debug ("load widgets");
	// delete previous widgets
	_delete_current_launcher_widget (pLauncherWindow);  // remove all gobject data
	
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
	gtk_window_set_title (GTK_WINDOW (pLauncherWindow), cName);
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
		s_pCurrentLauncherWidget = cairo_dock_build_key_file_widget (pKeyFile,
			pInstance->pModule->pVisitCard->cGettextDomain,
			pLauncherWindow,
			&pWidgetList,
			pDataGarbage,
			cOriginalConfFilePath);
		g_free (cOriginalConfFilePath);
		
		// load custom widgets
		if (pInstance->pModule->pInterface->load_custom_widget != NULL)
		{
			g_object_set_data (G_OBJECT (pLauncherWindow), "widget-list", pWidgetList);
			g_object_set_data (G_OBJECT (pLauncherWindow), "garbage", pDataGarbage);
			pInstance->pModule->pInterface->load_custom_widget (pInstance, pKeyFile);
		}
		
		if (pIcon != NULL)
			g_object_set_data (G_OBJECT (pLauncherWindow), "current-icon", pIcon);
		else
			g_object_set_data (G_OBJECT (pLauncherWindow), "current-module", pInstance);
		
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
			s_pCurrentLauncherWidget = cairo_dock_build_conf_file_widget (cConfFilePath,
				NULL,
				pLauncherWindow,
				&pWidgetList,
				pDataGarbage,
				NULL);
			
			g_object_set_data (G_OBJECT (pLauncherWindow), "current-container", pDock);
			
			g_free (cConfFilePath);
		}
		else  // main dock, we display a message
		{
			pDataGarbage = g_ptr_array_new ();
			gchar *cDefaultMessage = g_strdup_printf ("<b><big>%s</big></b>", _("Main dock's parameters are available in the main configuration window."));
			s_pCurrentLauncherWidget = cairo_dock_gui_make_preview_box (pLauncherWindow,
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
		s_pCurrentLauncherWidget = cairo_dock_build_conf_file_widget (cConfFilePath,
			NULL,
			pLauncherWindow,
			&pWidgetList,
			pDataGarbage,
			NULL);
		
		g_object_set_data (G_OBJECT (pLauncherWindow), "current-icon", pIcon);
		
		g_free (cConfFilePath);
	}
	
	// set widgets in the main window.
	if (s_pCurrentLauncherWidget != NULL)
	{
		gtk_paned_pack2 (GTK_PANED (s_pLauncherPane), s_pCurrentLauncherWidget, TRUE, FALSE);
		
		g_object_set_data (G_OBJECT (pLauncherWindow), "widget-list", pWidgetList);
		g_object_set_data (G_OBJECT (pLauncherWindow), "garbage", pDataGarbage);
		
		gtk_widget_show_all (s_pCurrentLauncherWidget);
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
	gint iDesiredIconSize = cairo_dock_search_icon_size (GTK_ICON_SIZE_DND); // default is 32
	
	// set an image.
	if (pIcon->cFileName != NULL)
	{
		cImagePath = cairo_dock_search_icon_s_path (pIcon->cFileName, iDesiredIconSize);
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
		pixbuf = gdk_pixbuf_new_from_file_at_size (cImagePath, iDesiredIconSize, iDesiredIconSize, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
	}
	
	// set a name
	if (CAIRO_DOCK_IS_USER_SEPARATOR (pIcon))  // separator
		cName = "---------";
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
		pixbuf = gdk_pixbuf_new_from_file_at_size (cImagePath, 32, 32, NULL);
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

static void _select_item (Icon *pIcon, CairoContainer *pContainer, CairoDockModuleInstance *pModuleInstance, GtkWidget *pLauncherWindow)
{
	g_free (cPrevPath);
	cPrevPath = NULL;
	GtkTreeIter iter;
	if (_search_item_in_model (s_pLauncherTreeView, pIcon ? (gpointer)pIcon : pContainer ? (gpointer)pContainer : (gpointer)pModuleInstance, pIcon ? CD_MODEL_ICON : pContainer ? CD_MODEL_CONTAINER : CD_MODEL_MODULE, &iter))
	{
		GtkTreeModel * model = gtk_tree_view_get_model (GTK_TREE_VIEW (s_pLauncherTreeView));
		GtkTreePath *path =  gtk_tree_model_get_path (model, &iter);
		gtk_tree_view_expand_to_path (GTK_TREE_VIEW (s_pLauncherTreeView), path);
		gtk_tree_path_free (path);
		
		GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (s_pLauncherTreeView));
		gtk_tree_selection_unselect_all (pSelection);
		gtk_tree_selection_select_iter (pSelection, &iter);
	}
	else
	{
		cd_warning ("item not found");
		_delete_current_launcher_widget (pLauncherWindow);
		
		gtk_window_set_title (GTK_WINDOW (pLauncherWindow), _("Launcher configuration"));
		g_object_set_data (G_OBJECT (pLauncherWindow), "current-icon", NULL);
		g_object_set_data (G_OBJECT (pLauncherWindow), "current-container", NULL);
		g_object_set_data (G_OBJECT (pLauncherWindow), "current-module", NULL);
	}
}

static GtkTreeModel *_build_tree_model (void)
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
	g_signal_connect (G_OBJECT (model), "row-inserted", G_CALLBACK (on_row_inserted), NULL);
	g_signal_connect (G_OBJECT (model), "row-deleted", G_CALLBACK (on_row_deleted), NULL);
	return GTK_TREE_MODEL (model);
}


  //////////////////
 // USER ACTIONS //
//////////////////

static GtkTreeIter lastInsertedIter;
static gboolean s_bHasPendingInsertion = FALSE;
static struct timeval lastTime = {0, 0};

static void on_row_inserted (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	// we only receive this event from an intern drag'n'drop
	// when we receive this event, the row is still empty, so we can't perform any task here.
	// so we just remember the event, and treat it later (when we receive the "row-deleted" signal).
	memcpy (&lastInsertedIter, iter, sizeof (GtkTreeIter));
	gettimeofday (&lastTime, NULL);
	s_bHasPendingInsertion = TRUE;
}
static void on_row_deleted (GtkTreeModel *model, GtkTreePath *path, gpointer data)
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
							}
							while (gtk_tree_model_iter_next (model, &it));
						}
						
						// move the icon (will update the conf file and trigger the signal to reload the GUI).
						g_print (" move the icon...\n");
						cairo_dock_move_icon_after_icon (CAIRO_DOCK (pParentContainer), pIcon, pLeftIcon);
					}
					else  // the row may have been dropped on a launcher or a desklet, in which case we must reload the model because this has no meaning.
					{
						reload_items ();
					}
					
				}  // else this row has no parent, so it was either a main dock or a desklet, and we have nothing to do.
				else
				{
					reload_items ();
				}
			}
			else  // no icon (for instance a plug-in like Remote-control)
			{
				reload_items ();
			}
		}
		
		s_bHasPendingInsertion = FALSE;
	}
}

static void _cairo_dock_remove_item (GtkMenuItem *pMenuItem, GtkWidget *pTreeView)
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


static void _free_launcher_gui (void)
{
	s_pLauncherWindow = NULL;
	s_pCurrentLauncherWidget = NULL;
	s_pLauncherPane = NULL;
	s_pLauncherTreeView = NULL;
}
static gboolean on_delete_launcher_gui (GtkWidget *pWidget, GdkEvent *event, gpointer data)
{
	GSList *pWidgetList = g_object_get_data (G_OBJECT (pWidget), "widget-list");
	cairo_dock_free_generated_widget_list (pWidgetList);
	
	GPtrArray *pDataGarbage = g_object_get_data (G_OBJECT (pWidget), "garbage");
	/// nettoyer.
	
	_free_launcher_gui ();
	
	return FALSE;
}

static void on_click_launcher_apply (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	
	Icon *pIcon = g_object_get_data (G_OBJECT (pWindow), "current-icon");
	CairoContainer *pContainer = g_object_get_data (G_OBJECT (pWindow), "current-container");
	CairoDockModuleInstance *pModuleInstance = g_object_get_data (G_OBJECT (pWindow), "current-module");
	
	if (CAIRO_DOCK_IS_APPLET (pIcon))
		pModuleInstance = pIcon->pModuleInstance;
	
	GSList *pWidgetList = g_object_get_data (G_OBJECT (pWindow), "widget-list");
	
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
	reload_items ();
}

static void on_click_launcher_quit (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	on_delete_launcher_gui (pWindow, NULL, NULL);
	gtk_widget_destroy (pWindow);
}


  /////////////////
 // GUI BACKEND //
/////////////////

static gboolean on_button_press_event (GtkWidget *pWidget,
	GdkEventButton *pButton,
	gpointer data)
{
	if (pButton->button == 3)  // clic droit.
	{
		GtkWidget *pMenu = gtk_menu_new ();
			
		GtkWidget *pMenuItem;
		
		/// TODO: check that we can actually remove it (ex.: not the main dock), and maybe display the item's name...
		pMenuItem = cairo_dock_add_in_menu_with_stock_and_data (_("Remove this item"), GTK_STOCK_REMOVE, G_CALLBACK (_cairo_dock_remove_item), pMenu, pWidget);
		
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
static GtkWidget *show_gui (Icon *pIcon, CairoContainer *pContainer, CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	//g_print ("%s (%x)\n", __func__, pIcon);
	//\_____________ On recharge la fenetre si elle existe deja.
	if (s_pLauncherWindow != NULL)
	{
		_delete_current_launcher_widget (s_pLauncherWindow);
		
		_select_item (pIcon, pContainer, pModuleInstance, s_pLauncherWindow);
		
		gtk_window_present (GTK_WINDOW (s_pLauncherWindow));
		return s_pLauncherWindow;
	}
	
	//\_____________ On construit la fenetre.
	s_pLauncherWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon_from_file (GTK_WINDOW (s_pLauncherWindow), CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, NULL);

	GtkWidget *pMainVBox = _gtk_vbox_new (CAIRO_DOCK_FRAME_MARGIN);
	gtk_container_add (GTK_CONTAINER (s_pLauncherWindow), pMainVBox);
	
	#if (GTK_MAJOR_VERSION < 3)
	s_pLauncherPane = gtk_hpaned_new ();
	#else
	s_pLauncherPane = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	#endif
	
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
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (s_pLauncherTreeView), TRUE);  // enables drag and drop of rows -> row-inserted and row-deleted signals
	g_signal_connect (G_OBJECT (s_pLauncherTreeView),
		"button-release-event",  // on release, so that the clicked line is already selected
		G_CALLBACK (on_button_press_event),
		NULL);
	
	// line selection
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (s_pLauncherTreeView));
	gtk_tree_selection_set_mode (pSelection, GTK_SELECTION_SINGLE);
	gtk_tree_selection_set_select_function (pSelection,
		(GtkTreeSelectionFunc) _on_select_one_item_in_tree,
		s_pLauncherWindow,
		NULL);
	
	GtkCellRenderer *rend;
	// column icon
	rend = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (s_pLauncherTreeView), -1, NULL, rend, "pixbuf", 1, NULL);
	// column name
	rend = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (s_pLauncherTreeView), -1, NULL, rend, "text", 0, NULL);
	
	GtkWidget *pLauncherWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pLauncherWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pLauncherWindow), s_pLauncherTreeView);
	gtk_paned_pack1 (GTK_PANED (s_pLauncherPane), pLauncherWindow, TRUE, FALSE);
	
	//\_____________ We add boutons.
	GtkWidget *pButtonsHBox = _gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN*2);
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
		TRUE,
		TRUE,
		0);
	g_object_set_data (G_OBJECT (s_pLauncherWindow), "status-bar", pStatusBar);
	g_object_set_data (G_OBJECT (s_pLauncherWindow), "frame-width", GINT_TO_POINTER (250));
	
	//\_____________ On essaie de definir une taille correcte.
	int w = MIN (CAIRO_DOCK_LAUNCHER_PANEL_WIDTH, g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]);
	gtk_window_resize (GTK_WINDOW (s_pLauncherWindow),
		w,
		MIN (CAIRO_DOCK_LAUNCHER_PANEL_HEIGHT, g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - (g_pMainDock && g_pMainDock->container.bIsHorizontal ? g_pMainDock->iMaxDockHeight : 0)));
	
	if (g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] < CAIRO_DOCK_LAUNCHER_PANEL_WIDTH)  // ecran trop petit, on va essayer de reserver au moins R pixels pour le panneau de droite (avec un minimum de L pixels pour celui de gauche).
		gtk_paned_set_position (GTK_PANED (s_pLauncherPane), MAX (CAIRO_DOCK_LEFT_PANE_MIN_WIDTH, w - CAIRO_DOCK_RIGHT_PANE_MIN_WIDTH));
	else  // we set a default width rather than letting GTK guess the best, because the right panel is more important than the left one.
		gtk_paned_set_position (GTK_PANED (s_pLauncherPane), CAIRO_DOCK_LEFT_PANE_DEFAULT_WIDTH);
	
	gtk_widget_show_all (s_pLauncherWindow);
	
	g_signal_connect (G_OBJECT (s_pLauncherWindow),
		"delete-event",
		G_CALLBACK (on_delete_launcher_gui),
		NULL);
	
	//\_____________ On selectionne l'entree courante.
	_select_item (pIcon, pContainer, pModuleInstance, s_pLauncherWindow);
	
	return s_pLauncherWindow;
}

static void reload_items (void)
{
	if (s_pLauncherWindow == NULL)
		return ;
	
	g_print ("%s ()\n", __func__);
	int iNotebookPage;
	if (s_pCurrentLauncherWidget && GTK_IS_NOTEBOOK (s_pCurrentLauncherWidget))
		iNotebookPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (s_pCurrentLauncherWidget));
	else
		iNotebookPage = -1;
	
	// reload the tree-view
	GtkTreeModel *model = _build_tree_model();
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (s_pLauncherTreeView), GTK_TREE_MODEL (model));
	g_object_unref (model);
	
	// reload the current icon/container's widgets by reselecting the current line.
	Icon *pCurrentIcon = g_object_get_data (G_OBJECT (s_pLauncherWindow), "current-icon");
	CairoContainer *pCurrentContainer = g_object_get_data (G_OBJECT (s_pLauncherWindow), "current-container");
	CairoDockModuleInstance *pCurrentModuleInstance = g_object_get_data (G_OBJECT (s_pLauncherWindow), "current-module");
	
	_delete_current_launcher_widget (s_pLauncherWindow);  // s_pCurrentLauncherWidget <- 0
	
	_select_item (pCurrentIcon, pCurrentContainer, pCurrentModuleInstance, s_pLauncherWindow);  // set s_pCurrentLauncherWidget
	
	if (s_pCurrentLauncherWidget && GTK_IS_NOTEBOOK (s_pCurrentLauncherWidget) && iNotebookPage != -1)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (s_pCurrentLauncherWidget), iNotebookPage);
	
	gtk_widget_show_all (s_pLauncherWindow);
}

void cairo_dock_register_default_items_gui_backend (void)
{
	CairoDockItemsGuiBackend *pBackend = g_new0 (CairoDockItemsGuiBackend, 1);
	
	pBackend->show_gui 			= show_gui;
	pBackend->reload_items 		= reload_items;
	
	cairo_dock_register_items_gui_backend (pBackend);
}


  ////////////////////
 // ACTIONS ON GUI //
////////////////////

CairoDockGroupKeyWidget *cairo_dock_gui_items_get_widget_from_name (CairoDockModuleInstance *pInstance, const gchar *cGroupName, const gchar *cKeyName)
{
	g_return_val_if_fail (s_pLauncherWindow != NULL, NULL);
	cd_debug ("%s (%s, %s)", __func__, cGroupName, cKeyName);
	return cairo_dock_gui_find_group_key_widget (s_pLauncherWindow, cGroupName, cKeyName);
}

void cairo_dock_gui_items_update_desklet_params (CairoDesklet *pDesklet)
{
	if (s_pLauncherWindow == NULL || pDesklet == NULL || pDesklet->pIcon == NULL)
		return;
	
	Icon *pIcon = g_object_get_data (G_OBJECT (s_pLauncherWindow), "current-icon");
	if (pIcon != pDesklet->pIcon)
		return;
	
	GSList *pWidgetList = g_object_get_data (G_OBJECT (s_pLauncherWindow), "widget-list");
	cairo_dock_update_desklet_widgets (pDesklet, pWidgetList);
}

void cairo_dock_update_desklet_visibility_params (CairoDesklet *pDesklet)
{
	if (s_pLauncherWindow == NULL || pDesklet == NULL || pDesklet->pIcon == NULL)
		return;
	
	Icon *pIcon = g_object_get_data (G_OBJECT (s_pLauncherWindow), "current-icon");
	if (pIcon != pDesklet->pIcon)
		return;
	
	GSList *pWidgetList = g_object_get_data (G_OBJECT (s_pLauncherWindow), "widget-list");
	cairo_dock_update_desklet_visibility_widgets (pDesklet, pWidgetList);
}

void cairo_dock_gui_items_update_module_instance_container (CairoDockModuleInstance *pInstance, gboolean bDetached)
{
	if (s_pLauncherWindow == NULL || pInstance == NULL)
		return;
	
	Icon *pIcon = g_object_get_data (G_OBJECT (s_pLauncherWindow), "current-icon");
	if (pIcon != pInstance->pIcon)  // pour un module qui se detache, il suffit de chercher parmi les applets.
		return;
	
	GSList *pWidgetList = g_object_get_data (G_OBJECT (s_pLauncherWindow), "widget-list");
	cairo_dock_update_is_detached_widget (bDetached, pWidgetList);
}

void cairo_dock_gui_items_reload_current_widget (CairoDockModuleInstance *pInstance, int iShowPage)
{
	g_return_if_fail (s_pLauncherWindow != NULL && s_pLauncherTreeView != NULL);
	cd_debug ("%s ()", __func__);
	
	Icon *pIcon = g_object_get_data (G_OBJECT (s_pLauncherWindow), "current-icon");
	CairoDockModuleInstance *pModuleInstance = g_object_get_data (G_OBJECT (s_pLauncherWindow), "current-module");
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
	
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (s_pLauncherTreeView));
	GtkTreeModel *pModel = NULL;
	GList *paths = gtk_tree_selection_get_selected_rows (pSelection, &pModel);
	g_return_if_fail (paths != NULL && pModel != NULL);
	GtkTreePath *path = paths->data;
	
	int iNotebookPage;
	if ((GTK_IS_NOTEBOOK (s_pCurrentLauncherWidget)))
	{
		if (iShowPage == -1)
			iNotebookPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (s_pCurrentLauncherWidget));
		else
			iNotebookPage = iShowPage;
	}
	else
		iNotebookPage = -1;
	
	g_free (cPrevPath);
	cPrevPath = NULL;
	_on_select_one_item_in_tree (pSelection, pModel, path, FALSE, s_pLauncherWindow);  // on appelle la callback nous-memes, car la ligne est deja selectionnee, donc le parametre 'path_currently_selected' sera a TRUE. de plus on veut pouvoir mettre la page du notebook.
	
	if (iNotebookPage != -1)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (s_pCurrentLauncherWidget), iNotebookPage);
	}
	
	g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (paths);
}

void cairo_dock_gui_items_set_status_message_on_gui (const gchar *cMessage)
{
	GtkWidget *pStatusBar = NULL;
	if (s_pLauncherWindow != NULL)
	{
		pStatusBar = g_object_get_data (G_OBJECT (s_pLauncherWindow), "status-bar");
	}
	if (pStatusBar == NULL)
		return ;
	cd_debug ("%s (%s)", __func__, cMessage);
	gtk_statusbar_pop (GTK_STATUSBAR (pStatusBar), 0);  // clear any previous message, underflow is allowed.
	gtk_statusbar_push (GTK_STATUSBAR (pStatusBar), 0, cMessage);
}

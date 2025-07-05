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

#include "config.h"
#include "gldi-icon-names.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-icon-manager.h"  // cairo_dock_search_icon_s_path
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_get_width
#include "cairo-dock-gui-manager.h"  // cairo_dock_show_module_instance_gui
#include "cairo-dock-gui-backend.h"  // cairo_dock_show_module_gui
#include "cairo-dock-gui-commons.h"  // cairo_dock_get_third_party_applets_link
#include "cairo-dock-widget-plugins.h"

#define CAIRO_DOCK_PREVIEW_HEIGHT 250 // matttbe: 200

extern gchar *g_cConfFile;
extern GldiContainer *g_pPrimaryContainer;

static void _widget_plugins_reload (CDWidget *pCdWidget);

static void _cairo_dock_activate_one_module (G_GNUC_UNUSED GtkCellRendererToggle * cell_renderer, gchar * path, GtkTreeModel * model)
{
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter_from_string (model, &iter, path))
		return ;
	gchar *cModuleName = NULL;
	gboolean bState;
	gtk_tree_model_get (model, &iter,
		CAIRO_DOCK_MODEL_RESULT, &cModuleName,
		CAIRO_DOCK_MODEL_ACTIVE, &bState, -1);
	
	bState = !bState;
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, CAIRO_DOCK_MODEL_ACTIVE, bState, -1);
	
	GldiModule *pModule = gldi_module_get (cModuleName);
	if (g_pPrimaryContainer == NULL)
	{
		cairo_dock_add_remove_element_to_key (g_cConfFile, "System", "modules", cModuleName, bState);
	}
	else if (pModule->pInstancesList == NULL)
	{
		gldi_module_activate (pModule);
	}
	else
	{
		gldi_module_deactivate (pModule);
	}  // la ligne passera en gras automatiquement.
	
	g_free (cModuleName);
}
static void _cairo_dock_initiate_config_module (G_GNUC_UNUSED GtkMenuItem *pMenuItem, GldiModule *pModule)
{
	GldiModuleInstance *pModuleInstance = (pModule->pInstancesList ? pModule->pInstancesList->data : NULL);
	if (pModuleInstance)
		cairo_dock_show_module_instance_gui (pModuleInstance, -1);
	else
		cairo_dock_show_module_gui (pModule->pVisitCard->cModuleName);
}
static gboolean _on_click_module_tree_view (GtkTreeView *pTreeView, GdkEventButton* pButton, G_GNUC_UNUSED gpointer data)
{
	if ((pButton->button == 3 && pButton->type == GDK_BUTTON_RELEASE)  // right click
	|| (pButton->button == 1 && pButton->type == GDK_2BUTTON_PRESS))  // double click
	{
		GtkTreeSelection *pSelection = gtk_tree_view_get_selection (pTreeView);
		GtkTreeModel *pModel;
		GtkTreeIter iter;
		if (! gtk_tree_selection_get_selected (pSelection, &pModel, &iter))
			return FALSE;
		
		gchar *cModuleName = NULL;
		gtk_tree_model_get (pModel, &iter,
			CAIRO_DOCK_MODEL_RESULT, &cModuleName, -1);
		GldiModule *pModule = gldi_module_get (cModuleName);
		if (pModule == NULL)
			return FALSE;
		
		if (pModule->pInstancesList == NULL)  // on ne gere pas la config d'un module non actif, donc inutile de presenter le menu dans ce cas-la.
			return FALSE;
		
		if (pButton->button == 3)
		{
			GtkWidget *pMenu = gtk_menu_new ();
			cairo_dock_gui_menu_item_add (pMenu, _("Configure this applet"), GLDI_ICON_NAME_PROPERTIES, G_CALLBACK (_cairo_dock_initiate_config_module), pModule);
			gtk_widget_show_all (pMenu);
			gtk_menu_popup_at_pointer (GTK_MENU (pMenu), NULL);
		}
		else
		{
			_cairo_dock_initiate_config_module (NULL, pModule);
		}
	}
	return FALSE;
}

static void _cairo_dock_render_module_name (G_GNUC_UNUSED GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
{
	gboolean bActive = FALSE;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_ACTIVE, &bActive, -1);
	
	if (bActive)
		g_object_set (cell, "weight", 800, "weight-set", TRUE, NULL);
	else
		g_object_set (cell, "weight", 400, "weight-set", FALSE, NULL);
}

static void _cairo_dock_render_category (G_GNUC_UNUSED GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
{
	const gchar *cCategory=NULL;
	gint iCategory = 0;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_STATE, &iCategory, -1);
	switch (iCategory)
	{
		case CAIRO_DOCK_CATEGORY_APPLET_FILES:
			cCategory = _("Files");
			g_object_set (cell, "foreground", "#004EA1", NULL);  // blue
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_INTERNET:
			cCategory = _("Internet");
			g_object_set (cell, "foreground", "#FF5555", NULL);  // orange
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_DESKTOP:
			cCategory = _("Desktop");
			g_object_set (cell, "foreground", "#116E08", NULL);  // green
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_ACCESSORY:
			cCategory = _("Accessory");
			g_object_set (cell, "foreground", "#900009", NULL);  // red
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_SYSTEM:
			cCategory = _("System");
			g_object_set (cell, "foreground", "#A58B0D", NULL);  // yellow
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_FUN:
			cCategory = _("Fun");
			g_object_set (cell, "foreground", "#FF55FF", NULL);  // purple
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_BEHAVIOR: // help applet
			cCategory = _("Behaviour");
			g_object_set (cell, "foreground", "#000066", NULL);  // dark blue
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		default:
			cd_warning ("incorrect category (%d)", iCategory);
		break;
	}
	if (cCategory != NULL)
	{
		g_object_set (cell, "text", cCategory, NULL);
	}
}

static gboolean _cairo_dock_add_module_to_modele (gchar *cModuleName, GldiModule *pModule, GtkListStore *pModel)
{
	if (pModule->pVisitCard->iCategory != CAIRO_DOCK_CATEGORY_THEME  // don't display the animations plug-ins
		&& ! gldi_module_is_auto_loaded (pModule))  // don't display modules that can't be disabled
	{
		//g_print (" + %s\n",  pModule->pVisitCard->cIconFilePath);
		int iSize = cairo_dock_search_icon_size (GTK_ICON_SIZE_LARGE_TOOLBAR);
		cairo_surface_t *surface = cairo_dock_create_surface_from_icon (
			pModule->pVisitCard->cIconFilePath, iSize, iSize);

		GtkTreeIter iter;
		memset (&iter, 0, sizeof (GtkTreeIter));
		gtk_list_store_append (GTK_LIST_STORE (pModel), &iter);
		gtk_list_store_set (GTK_LIST_STORE (pModel), &iter,
			CAIRO_DOCK_MODEL_NAME, pModule->pVisitCard->cTitle,
			CAIRO_DOCK_MODEL_RESULT, cModuleName,
			CAIRO_DOCK_MODEL_DESCRIPTION_FILE, dgettext (pModule->pVisitCard->cGettextDomain, pModule->pVisitCard->cDescription),
			CAIRO_DOCK_MODEL_IMAGE, pModule->pVisitCard->cPreviewFilePath,
			CAIRO_DOCK_MODEL_ICON, surface,
			CAIRO_DOCK_MODEL_STATE, pModule->pVisitCard->iCategory,
			CAIRO_DOCK_MODEL_ACTIVE, (pModule->pInstancesList != NULL), -1);
		cairo_surface_destroy (surface);
	}
	return FALSE;
}

static GtkWidget *_cairo_dock_build_modules_treeview (void)
{
	//\______________ On construit le treeview des modules.
	GtkWidget *pOneWidget = cairo_dock_gui_make_tree_view (FALSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (pOneWidget), TRUE);
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (pOneWidget), TRUE);
	g_signal_connect (G_OBJECT (pOneWidget), "button-release-event", G_CALLBACK (_on_click_module_tree_view), NULL);  // pour le menu du clic droit
	g_signal_connect (G_OBJECT (pOneWidget), "button-press-event", G_CALLBACK (_on_click_module_tree_view), NULL);  // pour le menu du clic droit
	
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pOneWidget));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	//\______________ On remplit le modele avec les modules de la categorie.
	GtkTreeModel *pModel = gtk_tree_view_get_model (GTK_TREE_VIEW (pOneWidget));
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (pModel), CAIRO_DOCK_MODEL_STATE, GTK_SORT_ASCENDING);
	gldi_module_foreach ((GHRFunc) _cairo_dock_add_module_to_modele, pModel);
	
	//\______________ On definit l'affichage du modele dans le tree-view.
	GtkTreeViewColumn* col;
	GtkCellRenderer *rend;
	// case a cocher
	rend = gtk_cell_renderer_toggle_new ();
	col = gtk_tree_view_column_new_with_attributes (NULL, rend, "active", CAIRO_DOCK_MODEL_ACTIVE, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_ACTIVE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	g_signal_connect (G_OBJECT (rend), "toggled", (GCallback) _cairo_dock_activate_one_module, pModel);
	// icone
	rend = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "surface", CAIRO_DOCK_MODEL_ICON, NULL);
	// nom
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Plug-in"), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
	gtk_tree_view_column_set_cell_data_func (col, rend, (GtkTreeCellDataFunc)_cairo_dock_render_module_name, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_NAME);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	// categorie
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Category"), rend, "text", CAIRO_DOCK_MODEL_STATE, NULL);
	gtk_tree_view_column_set_cell_data_func (col, rend, (GtkTreeCellDataFunc)_cairo_dock_render_category, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_STATE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	
	return pOneWidget;
}


static void _build_plugins_widget (PluginsWidget *pPluginsWidget)
{
	//\_____________ On construit le tree-view.
	pPluginsWidget->pTreeView = _cairo_dock_build_modules_treeview ();
	
	//\_____________ On l'ajoute a la fenetre.
	GtkWidget *pKeyBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
	GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (pScrolledWindow, "height-request", MIN (2*CAIRO_DOCK_PREVIEW_HEIGHT, gldi_desktop_get_height() - 210), NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (pScrolledWindow), pPluginsWidget->pTreeView);
	gtk_box_pack_start (GTK_BOX (pKeyBox), pScrolledWindow, TRUE, TRUE, 0);
	
	//\______________ On construit le widget de prevue et on le rajoute a la suite.
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	gchar *cDefaultMessage = g_strdup_printf ("<b><span font_desc=\"Sans 14\">%s</span></b>", _("Click on an applet in order to have a preview and a description for it."));
	GtkWidget *pPreviewBox = cairo_dock_gui_make_preview_box (pKeyBox, pPluginsWidget->pTreeView, FALSE, 1, cDefaultMessage, CAIRO_DOCK_SHARE_DATA_DIR"/images/"CAIRO_DOCK_LOGO, pDataGarbage);  // vertical packaging.
	GtkWidget *pScrolledWindow2 = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (pScrolledWindow, "height-request", MIN (2*CAIRO_DOCK_PREVIEW_HEIGHT, gldi_desktop_get_height() - 210), NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow2), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (pScrolledWindow2), pPreviewBox);
	gtk_box_pack_start (GTK_BOX (pKeyBox), pScrolledWindow2, FALSE, FALSE, 0);
	g_free (cDefaultMessage);
	
	GtkWidget *pVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN);
	gtk_box_pack_start (GTK_BOX (pVBox), pKeyBox, TRUE, TRUE, 0);
	
	//\______________ Add a link to the third-party applet (Note: it's somewhere around here that we could add a third-party addons selector).
	gchar *cLink = cairo_dock_get_third_party_applets_link ();
	GtkWidget *pLink = gtk_link_button_new_with_label (cLink, _("Get more applets!"));
	g_free (cLink);
	gtk_box_pack_start (GTK_BOX (pVBox), pLink, FALSE, FALSE, 0);
	
	pPluginsWidget->widget.pWidget = pVBox;
	pPluginsWidget->widget.pDataGarbage = pDataGarbage;
}

PluginsWidget *cairo_dock_plugins_widget_new (void)
{
	PluginsWidget *pPluginsWidget = g_new0 (PluginsWidget, 1);
	pPluginsWidget->widget.iType = WIDGET_PLUGINS;
	pPluginsWidget->widget.apply = NULL;  // no apply button
	pPluginsWidget->widget.reset = NULL;  // nothing special to clean
	pPluginsWidget->widget.reload = _widget_plugins_reload;
	
	_build_plugins_widget (pPluginsWidget);
	
	return pPluginsWidget;
}


static gboolean _update_module_checkbox (GtkTreeModel *pModel, G_GNUC_UNUSED GtkTreePath *path, GtkTreeIter *iter, gpointer *data)
{
	gchar *cWantedModuleName = data[0];
	gchar *cModuleName = NULL;
	gtk_tree_model_get (pModel, iter,
		CAIRO_DOCK_MODEL_RESULT, &cModuleName, -1);
	if (cModuleName && strcmp (cModuleName, cWantedModuleName) == 0)
	{
		gtk_list_store_set (GTK_LIST_STORE (pModel), iter, CAIRO_DOCK_MODEL_ACTIVE, data[1], -1);
		g_free (cModuleName);
		return TRUE;
	}
	g_free (cModuleName);
	return FALSE;
}
void cairo_dock_widget_plugins_update_module_state (PluginsWidget *pPluginsWidget, const gchar *cModuleName, gboolean bActive)
{
	GtkTreeModel *pModel = gtk_tree_view_get_model (GTK_TREE_VIEW (pPluginsWidget->pTreeView));
	gpointer data[2] = {(gpointer)cModuleName, GINT_TO_POINTER (bActive)};
	gtk_tree_model_foreach (GTK_TREE_MODEL (pModel), (GtkTreeModelForeachFunc) _update_module_checkbox, data);
}


static void _widget_plugins_reload (CDWidget *pCdWidget)
{
	PluginsWidget *pPluginsWidget = PLUGINS_WIDGET (pCdWidget);
	
	GtkTreeModel *pModel = gtk_tree_view_get_model (GTK_TREE_VIEW (pPluginsWidget->pTreeView));
	g_return_if_fail (pModel != NULL);
	gtk_list_store_clear (GTK_LIST_STORE (pModel));
	
	gldi_module_foreach ((GHRFunc) _cairo_dock_add_module_to_modele, pModel);
}

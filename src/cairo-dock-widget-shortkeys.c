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
#include "cairo-dock-struct.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-widget-shortkeys.h"

#define CAIRO_DOCK_PREVIEW_HEIGHT 250
extern CairoDockDesktopGeometry g_desktopGeometry;


typedef enum {
	CD_SHORTKEY_MODEL_NAME = 0,  // demander
	CD_SHORTKEY_MODEL_DESCRIPTION,  // description
	CD_SHORTKEY_MODEL_PIXBUF,  // icon image
	CD_SHORTKEY_MODEL_SHORTKEY,  // shortkey
	CD_SHORTKEY_MODEL_STATE,  // grabbed or not
	CD_SHORTKEY_MODEL_BINDING,  // the binding
	CD_SHORTKEY_MODEL_NB_COLUMNS
	} CDModelColumns;
static void _on_key_grab_cb (GtkWidget *pInputDialog, GdkEventKey *event, GtkTreeView *pTreeView)
{
	gchar *key;
	cd_debug ("key pressed");
	if (gtk_accelerator_valid (event->keyval, event->state))
	{
		/* This lets us ignore all ignorable modifier keys, including
		* NumLock and many others. :)
		*
		* The logic is: keep only the important modifiers that were pressed
		* for this event. */
		event->state &= gtk_accelerator_get_default_mod_mask();

		/* Generate the correct name for this key */
		key = gtk_accelerator_name (event->keyval, event->state);

		cd_debug ("KEY GRABBED: '%s'", key);

		// get the key binding
		GtkTreeSelection *pSelection = gtk_tree_view_get_selection (pTreeView);
		GtkTreeModel *pModel;
		GtkTreeIter iter;
		if (gtk_tree_selection_get_selected (pSelection, &pModel, &iter))  // we retrieve it from the current selection, this way if the binding has been destroyed meanwhile (very unlikely), we're safe.
		{
			// Bind the new shortkey
			CairoKeyBinding *binding = NULL;
			gtk_tree_model_get (pModel, &iter,
				CD_SHORTKEY_MODEL_BINDING, &binding, -1);
			
			cd_keybinder_rebind (binding, key, NULL);
			
			// update the model
			gtk_list_store_set (GTK_LIST_STORE (pModel), &iter,
				CD_SHORTKEY_MODEL_SHORTKEY, binding->keystring,
				CD_SHORTKEY_MODEL_STATE, binding->bSuccess, -1);
			
			// store the new shortkey
			if (binding->cConfFilePath != NULL)
			{
				cairo_dock_update_conf_file (binding->cConfFilePath,
					G_TYPE_STRING, binding->cGroupName, binding->cKeyName, key,
					G_TYPE_INVALID);
			}
		}
			
		// Close the window
		g_free (key);
		gtk_widget_destroy (pInputDialog);
	}
}
static void on_cancel_shortkey (GtkButton *button, GtkWidget *pInputDialog)
{
	gtk_widget_destroy (pInputDialog);
}
static void _cairo_dock_initiate_change_shortkey (GtkMenuItem *pMenuItem, GtkTreeView *pTreeView)
{
	// ensure a line is selected
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (pTreeView);
	GtkTreeModel *pModel;
	GtkTreeIter iter;
	if (! gtk_tree_selection_get_selected (pSelection, &pModel, &iter))
		return ;

	// build a small modal input dialog, mainly to prevent any other interaction.
	GtkWidget *pInputDialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_decorated (GTK_WINDOW (pInputDialog), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (pInputDialog), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (pInputDialog), TRUE);
	//gtk_window_set_transient_for (GTK_WINDOW (pInputDialog), GTK_WINDOW (pMainWindow));
	gtk_window_set_modal (GTK_WINDOW (pInputDialog), TRUE);

	gtk_widget_add_events (pInputDialog, GDK_KEY_PRESS_MASK);
	g_signal_connect (GTK_WIDGET(pInputDialog), "key-press-event", G_CALLBACK(_on_key_grab_cb), pTreeView);

	GtkWidget *pMainVBox = _gtk_vbox_new (CAIRO_DOCK_FRAME_MARGIN);
	gtk_container_add (GTK_CONTAINER (pInputDialog), pMainVBox);

	GtkWidget *pLabel = gtk_label_new (_("Press the shortkey"));
	gtk_box_pack_start(GTK_BOX (pMainVBox), pLabel, FALSE, FALSE, 0);

	GtkWidget *pCancelButton = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	g_signal_connect (G_OBJECT (pCancelButton), "clicked", G_CALLBACK(on_cancel_shortkey), pInputDialog);
	gtk_box_pack_start (GTK_BOX (pMainVBox), pCancelButton, FALSE, FALSE, 0);

	gtk_widget_show_all (pInputDialog);
}
static gboolean _on_click_shortkey_tree_view (GtkTreeView *pTreeView, GdkEventButton* pButton, gpointer data)
{
	GtkWidget *pMainWindow = NULL;
	if ((pButton->button == 3 && pButton->type == GDK_BUTTON_RELEASE)  // right click
	|| (pButton->button == 1 && pButton->type == GDK_2BUTTON_PRESS))  // double click
	{
		if (pButton->button == 3)
		{
			GtkWidget *pMenu = gtk_menu_new ();
			
			cairo_dock_add_in_menu_with_stock_and_data (_("Change the shortkey"), GTK_STOCK_PROPERTIES, G_CALLBACK (_cairo_dock_initiate_change_shortkey), pMenu, pTreeView);
			
			gtk_widget_show_all (pMenu);
			gtk_menu_popup (GTK_MENU (pMenu),
				NULL,
				NULL,
				NULL,
				NULL,
				1,
				gtk_get_current_event_time ());
		}
		else
		{
			_cairo_dock_initiate_change_shortkey (NULL, pTreeView);
		}
	}
	return FALSE;
}
static void _cairo_dock_render_shortkey (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, gpointer data)
{
	gboolean bActive = FALSE;
	gchar *cShortkey = NULL;
	gtk_tree_model_get (model, iter,
		CD_SHORTKEY_MODEL_STATE, &bActive,
		CD_SHORTKEY_MODEL_SHORTKEY, &cShortkey, -1);
	
	if (bActive)
	{
		g_object_set (cell, "foreground", "#108000", NULL);  // vert
		g_object_set (cell, "foreground-set", TRUE, NULL);
	}
	else if (cShortkey != NULL)
	{
		g_object_set (cell, "foreground", "#B00000", NULL);  // rouge
		g_object_set (cell, "foreground-set", TRUE, NULL);
	}
	else
	{
		g_object_set (cell, "foreground-set", FALSE, NULL);
	}
	g_free (cShortkey);
}
void cairo_dock_add_shortkey_to_model (CairoKeyBinding *binding, GtkListStore *pModel)
{
	//g_print (" + %s\n",  pModule->pVisitCard->cIconFilePath);
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (binding->cIconFilePath, 32, 32, NULL);
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModel), &iter);
	g_print (" + %s (%s, %d)\n", binding->keystring, binding->cDemander, binding->bSuccess);
	gtk_list_store_set (GTK_LIST_STORE (pModel), &iter,
		CD_SHORTKEY_MODEL_NAME, binding->cDemander,
		CD_SHORTKEY_MODEL_SHORTKEY, binding->keystring,
		CD_SHORTKEY_MODEL_DESCRIPTION, binding->cDescription,
		CD_SHORTKEY_MODEL_PIXBUF, pixbuf,
		CD_SHORTKEY_MODEL_STATE, binding->bSuccess,
		CD_SHORTKEY_MODEL_BINDING, binding, -1);
	g_object_unref (pixbuf);
}
static GtkWidget *cairo_dock_build_shortkeys_widget (void)
{
	// fill the model
	GtkListStore *pModel = gtk_list_store_new (CD_SHORTKEY_MODEL_NB_COLUMNS,
		G_TYPE_STRING,  // demander
		G_TYPE_STRING,  // description
		GDK_TYPE_PIXBUF,  // icon
		G_TYPE_STRING,  // shortkey
		G_TYPE_BOOLEAN,  // grabbed or not
		G_TYPE_POINTER);  // binding
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (pModel), CD_SHORTKEY_MODEL_NAME, GTK_SORT_ASCENDING);
	cd_keybinder_foreach ((GFunc) cairo_dock_add_shortkey_to_model, pModel);
	
	// make the treeview
	GtkWidget *pOneWidget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (pModel));
	g_object_unref (pModel);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (pOneWidget), TRUE);
	g_signal_connect (G_OBJECT (pOneWidget), "button-press-event", G_CALLBACK (_on_click_shortkey_tree_view), NULL);
	g_signal_connect (G_OBJECT (pOneWidget), "button-release-event", G_CALLBACK (_on_click_shortkey_tree_view), NULL);
	
	// define the rendering of the treeview
	GtkTreeViewColumn* col;
	GtkCellRenderer *rend;
	// icon
	rend = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "pixbuf", CD_SHORTKEY_MODEL_PIXBUF, NULL);
	// demander
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Origin"), rend, "text", CD_SHORTKEY_MODEL_NAME, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CD_SHORTKEY_MODEL_NAME);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	// action description
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Action"), rend, "text", CD_SHORTKEY_MODEL_DESCRIPTION, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	// shortkey
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Shortkey"), rend, "text", CD_SHORTKEY_MODEL_SHORTKEY, NULL);
	gtk_tree_view_column_set_cell_data_func (col, rend, (GtkTreeCellDataFunc)_cairo_dock_render_shortkey, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CD_SHORTKEY_MODEL_SHORTKEY);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	
	return pOneWidget;
}


ShortkeysWidget *cairo_dock_shortkeys_widget_new (void)
{
	ShortkeysWidget *pShortkeysWidget = g_new0 (ShortkeysWidget, 1);
	pShortkeysWidget->widget.iType = WIDGET_SHORTKEYS;
	
	pShortkeysWidget->pShortKeysTreeView = cairo_dock_build_shortkeys_widget ();
	
	GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (pScrolledWindow, "height-request", MIN (2*CAIRO_DOCK_PREVIEW_HEIGHT, g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - 175), NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pShortkeysWidget->pShortKeysTreeView);
	
	pShortkeysWidget->widget.pWidget = pScrolledWindow;
	
	return pShortkeysWidget;
}


void cairo_dock_shortkeys_widget_reload (ShortkeysWidget *pShortkeysWidget)
{
	GtkTreeModel *pModel = gtk_tree_view_get_model (GTK_TREE_VIEW (pShortkeysWidget->pShortKeysTreeView));
	g_return_if_fail (pModel != NULL);
	gtk_list_store_clear (GTK_LIST_STORE (pModel));
	cd_keybinder_foreach ((GFunc) cairo_dock_add_shortkey_to_model, pModel);
}

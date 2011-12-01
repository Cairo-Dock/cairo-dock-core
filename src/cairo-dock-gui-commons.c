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
#include <sys/stat.h>
#define __USE_POSIX
#include <time.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>

#include "config.h"
#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-task.h"
#include "cairo-dock-log.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-gui-commons.h"

#define CAIRO_DOCK_PLUGINS_EXTRAS_URL "http://extras.glx-dock.org"

static CairoDockTask *s_pImportTask = NULL;

extern gchar *g_cThemesDirPath;
extern gchar *g_cConfFile;
extern CairoContainer *g_pPrimaryContainer;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;


static void _cairo_dock_fill_model_with_themes (const gchar *cThemeName, CairoDockPackage *pTheme, GtkListStore *pModel)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModel), &iter);
	
	gtk_list_store_set (GTK_LIST_STORE (pModel), &iter,
		CAIRO_DOCK_MODEL_NAME, pTheme->cDisplayedName,
		CAIRO_DOCK_MODEL_RESULT, cThemeName, -1);
}
static void _cairo_dock_activate_one_element (GtkCellRendererToggle * cell_renderer, gchar * path, GtkTreeModel * model)
{
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter_from_string (model, &iter, path))
		return ;
	gboolean bState;
	gtk_tree_model_get (model, &iter, CAIRO_DOCK_MODEL_ACTIVE, &bState, -1);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, CAIRO_DOCK_MODEL_ACTIVE, !bState, -1);
}
void cairo_dock_make_tree_view_for_delete_themes (GtkWidget *pWindow)
{
	const gchar *cGroupName = "Delete";
	//\______________ On recupere le widget de la cle.
	CairoDockGroupKeyWidget *myWidget = cairo_dock_gui_find_group_key_widget (pWindow, cGroupName, "deleted themes");
	g_return_if_fail (myWidget != NULL);
	
	//\______________ On construit le treeview des themes.
	GtkWidget *pOneWidget = cairo_dock_gui_make_tree_view (FALSE);
	GtkTreeModel *pModel = gtk_tree_view_get_model (GTK_TREE_VIEW (pOneWidget));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (pOneWidget), FALSE);
	
	GtkCellRenderer *rend = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "active", CAIRO_DOCK_MODEL_ACTIVE, NULL);
	g_signal_connect (G_OBJECT (rend), "toggled", (GCallback) _cairo_dock_activate_one_element, pModel);

	rend = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
	
	GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pOneWidget));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (pScrolledWindow, "height-request", 160, NULL);  // environ 8 lignes.
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pOneWidget);
	
	//\______________ On recupere les themes utilisateurs.
	GHashTable *pThemeTable = cairo_dock_list_packages (NULL, g_cThemesDirPath, NULL, NULL);

	g_hash_table_foreach (pThemeTable, (GHFunc)_cairo_dock_fill_model_with_themes, pModel);
	g_hash_table_destroy (pThemeTable);
	
	//\______________ On insere le widget.
	myWidget->pSubWidgetList = g_slist_append (myWidget->pSubWidgetList, pOneWidget);
	gtk_box_pack_end (GTK_BOX (myWidget->pKeyBox), pScrolledWindow, FALSE, FALSE, 0);
}


gboolean cairo_dock_save_current_theme (GKeyFile* pKeyFile)
{
	const gchar *cGroupName = "Save";
	//\______________ On recupere le nom du theme.
	gchar *cNewThemeName = g_key_file_get_string (pKeyFile, cGroupName, "theme name", NULL);
	if (cNewThemeName != NULL && *cNewThemeName == '\0')
	{
		g_free (cNewThemeName);
		cNewThemeName = NULL;
	}
	cd_message ("cNewThemeName : %s", cNewThemeName);
	
	g_return_val_if_fail (cNewThemeName != NULL, FALSE);
	
	//\___________________ On sauve le theme courant sous ce nom.
	cairo_dock_extract_package_type_from_name (cNewThemeName);
	
	gboolean bSaveBehavior = g_key_file_get_boolean (pKeyFile, cGroupName, "save current behaviour", NULL);
	gboolean bSaveLaunchers = g_key_file_get_boolean (pKeyFile, cGroupName, "save current launchers", NULL);
	
	gboolean bThemeSaved = cairo_dock_export_current_theme (cNewThemeName, bSaveBehavior, bSaveLaunchers);
	
	if (g_key_file_get_boolean (pKeyFile, cGroupName, "package", NULL))
	{
		bThemeSaved |= cairo_dock_package_current_theme (cNewThemeName);
	}
	
	return bThemeSaved;
}


gboolean cairo_dock_delete_user_themes (GKeyFile* pKeyFile)
{
	const gchar *cGroupName = "Delete";
	//\___________________ On recupere les themes selectionnes.
	gsize length = 0;
	gchar ** cThemesList = g_key_file_get_string_list (pKeyFile, cGroupName, "deleted themes", &length, NULL);
	
	g_return_val_if_fail (cThemesList != NULL && cThemesList[0] != NULL && *cThemesList[0] != '\0', FALSE);
	
	//\___________________ On efface les themes.
	gboolean bThemeDeleted = cairo_dock_delete_themes (cThemesList);
	
	return bThemeDeleted;
}


static void on_cancel_dl (GtkButton *button, GtkWidget *pWaitingDialog)
{
	gtk_widget_destroy (pWaitingDialog);
	cairo_dock_discard_task (s_pImportTask);
	s_pImportTask = NULL;
}
static gboolean _pulse_bar (GtkWidget *pBar)
{
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pBar));
	return TRUE;
}
static gboolean on_destroy_waiting_dialog (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	guint iSidPulse = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pWidget), "pulse-id"));
	cd_debug ("*** stop pulse");
	g_source_remove (iSidPulse);
	return FALSE;
}
gboolean cairo_dock_load_theme (GKeyFile* pKeyFile, GFunc pCallback, GtkWidget *pMainWindow)
{
	const gchar *cGroupName = "Themes";
	//\___________________ On recupere le theme selectionne.
	gchar *cNewThemeName = g_key_file_get_string (pKeyFile, cGroupName, "chosen theme", NULL);
	if (cNewThemeName != NULL && *cNewThemeName == '\0')
	{
		g_free (cNewThemeName);
		cNewThemeName = NULL;
	}
	
	if (cNewThemeName == NULL)
	{
		cNewThemeName = g_key_file_get_string (pKeyFile, cGroupName, "package", NULL);
		if (cNewThemeName != NULL && *cNewThemeName == '\0')
		{
			g_free (cNewThemeName);
			cNewThemeName = NULL;
		}
	}
	
	g_return_val_if_fail (cNewThemeName != NULL, FALSE);
	
	//\___________________ On recupere les options de chargement.
	gboolean bLoadBehavior = g_key_file_get_boolean (pKeyFile, cGroupName, "use theme behaviour", NULL);
	gboolean bLoadLaunchers = g_key_file_get_boolean (pKeyFile, cGroupName, "use theme launchers", NULL);
	
	if (s_pImportTask != NULL)
	{
		cairo_dock_discard_task (s_pImportTask);
		s_pImportTask = NULL;
	}
	//\___________________ On regarde si le theme courant est modifie.
	gboolean bNeedSave = cairo_dock_current_theme_need_save ();
	if (bNeedSave)
	{
		int iAnswer = cairo_dock_ask_general_question_and_wait (_("You have made some changes to the current theme.\nYou will lose them if you don't save before choosing a new theme. Continue anyway?"));
		if (iAnswer != GTK_RESPONSE_YES)
		{
			return FALSE;
		}
		cairo_dock_mark_current_theme_as_modified (FALSE);
	}
	
	//\___________________ On charge le nouveau theme choisi.
	gchar *tmp = g_strdup (cNewThemeName);
	CairoDockPackageType iType = cairo_dock_extract_package_type_from_name (tmp);
	g_free (tmp);
	
	gboolean bThemeImported = FALSE;
	if (pCallback != NULL && (iType != CAIRO_DOCK_LOCAL_PACKAGE && iType != CAIRO_DOCK_USER_PACKAGE))
	{
		GtkWidget *pWaitingDialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_decorated (GTK_WINDOW (pWaitingDialog), FALSE);
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (pWaitingDialog), TRUE);
		gtk_window_set_skip_pager_hint (GTK_WINDOW (pWaitingDialog), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW (pWaitingDialog), GTK_WINDOW (pMainWindow));
		gtk_window_set_modal (GTK_WINDOW (pWaitingDialog), TRUE);

		#if (GTK_MAJOR_VERSION < 3)
		GtkWidget *pMainVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_FRAME_MARGIN);
		#else
		GtkWidget *pMainVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_FRAME_MARGIN);
		#endif
		gtk_container_add (GTK_CONTAINER (pWaitingDialog), pMainVBox);
		
		GtkWidget *pLabel = gtk_label_new (_("Please wait while importing the theme..."));
		gtk_box_pack_start(GTK_BOX (pMainVBox), pLabel, FALSE, FALSE, 0);
		
		GtkWidget *pBar = gtk_progress_bar_new ();
		gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pBar));
		gtk_box_pack_start (GTK_BOX (pMainVBox), pBar, FALSE, FALSE, 0);
		guint iSidPulse = g_timeout_add (100, (GSourceFunc)_pulse_bar, pBar);
		g_object_set_data (G_OBJECT (pWaitingDialog), "pulse-id", GINT_TO_POINTER (iSidPulse));
		g_signal_connect (G_OBJECT (pWaitingDialog),
			"destroy",
			G_CALLBACK (on_destroy_waiting_dialog),
			NULL);
		
		GtkWidget *pCancelButton = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
		g_signal_connect (G_OBJECT (pCancelButton), "clicked", G_CALLBACK(on_cancel_dl), pWaitingDialog);
		gtk_box_pack_start (GTK_BOX (pMainVBox), pCancelButton, FALSE, FALSE, 0);
		
		gtk_widget_show_all (pWaitingDialog);
		
		cd_debug ("start importation...");
		s_pImportTask = cairo_dock_import_theme_async (cNewThemeName, bLoadBehavior, bLoadLaunchers, (GFunc)pCallback, pWaitingDialog);
	}
	else
	{
		bThemeImported = cairo_dock_import_theme (cNewThemeName, bLoadBehavior, bLoadLaunchers);
		
		//\_______________ On le charge.
		if (bThemeImported)
		{
			cairo_dock_load_current_theme ();
		}
	}
	g_free (cNewThemeName);
	return bThemeImported;
}


gboolean cairo_dock_add_module_to_modele (gchar *cModuleName, CairoDockModule *pModule, GtkListStore *pModel)
{
	if (pModule->pVisitCard->iCategory != CAIRO_DOCK_CATEGORY_BEHAVIOR && pModule->pVisitCard->iCategory != CAIRO_DOCK_CATEGORY_THEME && ! cairo_dock_module_is_auto_loaded (pModule))
	{
		//g_print (" + %s\n",  pModule->pVisitCard->cIconFilePath);
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (pModule->pVisitCard->cIconFilePath, 32, 32, NULL);
		GtkTreeIter iter;
		memset (&iter, 0, sizeof (GtkTreeIter));
		gtk_list_store_append (GTK_LIST_STORE (pModel), &iter);
		gtk_list_store_set (GTK_LIST_STORE (pModel), &iter,
			CAIRO_DOCK_MODEL_NAME, pModule->pVisitCard->cTitle,
			CAIRO_DOCK_MODEL_RESULT, cModuleName,
			CAIRO_DOCK_MODEL_DESCRIPTION_FILE, dgettext (pModule->pVisitCard->cGettextDomain, pModule->pVisitCard->cDescription),
			CAIRO_DOCK_MODEL_IMAGE, pModule->pVisitCard->cPreviewFilePath,
			CAIRO_DOCK_MODEL_ICON, pixbuf,
			CAIRO_DOCK_MODEL_STATE, pModule->pVisitCard->iCategory,
			CAIRO_DOCK_MODEL_ACTIVE, (pModule->pInstancesList != NULL), -1);
		g_object_unref (pixbuf);
	}
	return FALSE;
}

static void _cairo_dock_activate_one_module (GtkCellRendererToggle * cell_renderer, gchar * path, GtkTreeModel * model)
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
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	if (g_pPrimaryContainer == NULL)
	{
		cairo_dock_add_remove_element_to_key (g_cConfFile, "System", "modules", cModuleName, bState);
	}
	else if (pModule->pInstancesList == NULL)
	{
		cairo_dock_activate_module_and_load (cModuleName);
	}
	else
	{
		cairo_dock_deactivate_module_and_unload (cModuleName);
	}  // la ligne passera en gras automatiquement.
	
	g_free (cModuleName);
}
static void _cairo_dock_initiate_config_module (GtkMenuItem *pMenuItem, CairoDockModule *pModule)
{
	CairoDockModuleInstance *pModuleInstance = (pModule->pInstancesList ? pModule->pInstancesList->data : NULL);
	if (pModuleInstance)
		cairo_dock_show_module_instance_gui (pModuleInstance, -1);
	else
		cairo_dock_show_module_gui (pModule->pVisitCard->cModuleName);
}
static gboolean _on_click_module_tree_view (GtkTreeView *pTreeView, GdkEventButton* pButton, gpointer data)
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
		CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
		if (pModule == NULL)
			return FALSE;
		
		if (pModule->pInstancesList == NULL)  // on ne gere pas la config d'un module no nactif, donc inutile de presenter le menu dans ce cas-la.
			return FALSE;
		
		if (pButton->button == 3)
		{
			GtkWidget *pMenu = gtk_menu_new ();
			
			cairo_dock_add_in_menu_with_stock_and_data (_("Configure this applet"), GTK_STOCK_PROPERTIES, G_CALLBACK (_cairo_dock_initiate_config_module), pMenu, pModule);
			
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
			_cairo_dock_initiate_config_module (NULL, pModule);
		}
	}
	return FALSE;
}
static void _cairo_dock_render_module_name (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, gpointer data)
{
	gboolean bActive = FALSE;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_ACTIVE, &bActive, -1);
	
	if (bActive)
		g_object_set (cell, "weight", 800, "weight-set", TRUE, NULL);
	else
		g_object_set (cell, "weight", 400, "weight-set", FALSE, NULL);
}

static void _cairo_dock_render_category (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, gpointer data)
{
	const gchar *cCategory=NULL;
	gint iCategory = 0;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_STATE, &iCategory, -1);
	switch (iCategory)
	{
		case CAIRO_DOCK_CATEGORY_APPLET_FILES:
			cCategory = _("Files");
			g_object_set (cell, "foreground", "#004EA1", NULL);  // bleu
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_INTERNET:
			cCategory = _("Internet");
			g_object_set (cell, "foreground", "#FF5555", NULL);  // orange
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_DESKTOP:
			cCategory = _("Desktop");
			g_object_set (cell, "foreground", "#116E08", NULL);  // vert
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_ACCESSORY:
			cCategory = _("Accessory");
			g_object_set (cell, "foreground", "#900009", NULL);  // rouge
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_SYSTEM:
			cCategory = _("System");
			g_object_set (cell, "foreground", "#A58B0D", NULL);  // jaune
			g_object_set (cell, "foreground-set", TRUE, NULL);
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_FUN:
			cCategory = _("Fun");
			g_object_set (cell, "foreground", "#FF55FF", NULL);  // rose
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
GtkWidget *cairo_dock_build_modules_treeview (void)
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
	cairo_dock_foreach_module ((GHRFunc) cairo_dock_add_module_to_modele, pModel);
	
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
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "pixbuf", CAIRO_DOCK_MODEL_ICON, NULL);
	// nom
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("plug-in"), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
	gtk_tree_view_column_set_cell_data_func (col, rend, (GtkTreeCellDataFunc)_cairo_dock_render_module_name, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_NAME);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	// categorie
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("category"), rend, "text", CAIRO_DOCK_MODEL_STATE, NULL);
	gtk_tree_view_column_set_cell_data_func (col, rend, (GtkTreeCellDataFunc)_cairo_dock_render_category, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_STATE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	
	return pOneWidget;
}


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

	#if (GTK_MAJOR_VERSION < 3)
	GtkWidget *pMainVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_FRAME_MARGIN);
	#else
	GtkWidget *pMainVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_FRAME_MARGIN);
	#endif
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
		g_object_set (cell, "foreground", "#A00000", NULL);  // rouge
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
GtkWidget *cairo_dock_build_shortkeys_widget (void)
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

void cairo_dock_update_desklet_widgets (CairoDesklet *pDesklet, GSList *pWidgetList)
{
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GtkWidget *pOneWidget;
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "locked");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), pDesklet->bPositionLocked);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "size");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	g_signal_handlers_block_matched (pOneWidget,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), pDesklet->container.iWidth);
	g_signal_handlers_unblock_matched (pOneWidget,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
	if (pGroupKeyWidget->pSubWidgetList->next != NULL)
	{
		pOneWidget = pGroupKeyWidget->pSubWidgetList->next->data;
		g_signal_handlers_block_matched (pOneWidget,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), pDesklet->container.iHeight);
		g_signal_handlers_unblock_matched (pOneWidget,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
	}
	
	int iRelativePositionX = (pDesklet->container.iWindowPositionX + pDesklet->container.iWidth/2 <= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]/2 ? pDesklet->container.iWindowPositionX : pDesklet->container.iWindowPositionX - g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]);
	int iRelativePositionY = (pDesklet->container.iWindowPositionY + pDesklet->container.iHeight/2 <= g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]/2 ? pDesklet->container.iWindowPositionY : pDesklet->container.iWindowPositionY - g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "x position");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), iRelativePositionX);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "y position");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), iRelativePositionY);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "rotation");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_range_set_value (GTK_RANGE (pOneWidget), pDesklet->fRotation/G_PI*180.);
}

void cairo_dock_update_desklet_visibility_widgets (CairoDesklet *pDesklet, GSList *pWidgetList)
{
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GtkWidget *pOneWidget;
	gboolean bIsSticky = cairo_dock_desklet_is_sticky (pDesklet);
	CairoDeskletVisibility iVisibility = pDesklet->iVisibility;
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "accessibility");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_combo_box_set_active (GTK_COMBO_BOX (pOneWidget), iVisibility);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "sticky");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), bIsSticky);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "locked");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), pDesklet->bPositionLocked);
}

void cairo_dock_update_is_detached_widget (gboolean bIsDetached, GSList *pWidgetList)
{
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GtkWidget *pOneWidget;
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "initially detached");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), bIsDetached);
}


gchar *cairo_dock_get_third_party_applets_link (void)
{
	return g_strdup_printf (CAIRO_DOCK_PLUGINS_EXTRAS_URL"/%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion);
}

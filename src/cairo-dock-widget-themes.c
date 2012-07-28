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
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "config.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-packages.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-dialog-manager.h"  // cairo_dock_ask_general_question_and_wait
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-task.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_export_current_theme
#include "cairo-dock-config.h"  // cairo_dock_load_current_theme
#include "cairo-dock-gui-manager.h"  // cairo_dock_set_status_message
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-widget-themes.h"

extern gchar *g_cThemesDirPath;

static void _themes_widget_apply (CDWidget *pCdWidget);
static void _themes_widget_reset (CDWidget *pCdWidget);
static void _themes_widget_reload (ThemesWidget *pThemesWidget);


static gchar *cairo_dock_build_temporary_themes_conf_file (void)
{
	//\___________________ On cree un fichier de conf temporaire.
	const gchar *cTmpDir = g_get_tmp_dir ();
	gchar *cTmpConfFile = g_strdup_printf ("%s/cairo-dock-init.XXXXXX", cTmpDir);
	int fds = mkstemp (cTmpConfFile);
	if (fds == -1)
	{
		cd_warning ("can't create a temporary file in %s", cTmpDir);
		g_free (cTmpConfFile);
		return NULL;
	}
	
	//\___________________ On copie le fichier de conf par defaut dedans.
	cairo_dock_copy_file (CAIRO_DOCK_SHARE_DATA_DIR"/themes.conf", cTmpConfFile);
	
	close(fds);
	return cTmpConfFile;
}


static void _load_theme (gboolean bSuccess, ThemesWidget *pThemesWidget)
{
	g_print ("%s ()\n", __func__);
	if (bSuccess)
	{
		cairo_dock_load_current_theme ();
		
		cairo_dock_set_status_message (NULL, "");
		
		_themes_widget_reload (pThemesWidget);
		cairo_dock_reload_gui ();
	}
	else
		cairo_dock_set_status_message (NULL, _("Could not import the theme."));
	gtk_widget_destroy (pThemesWidget->pWaitingDialog);
	pThemesWidget->pWaitingDialog = NULL;
}

static gboolean _cairo_dock_save_current_theme (GKeyFile* pKeyFile)
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


static gboolean _cairo_dock_delete_user_themes (GKeyFile* pKeyFile)
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


static void on_cancel_dl (GtkButton *button, ThemesWidget *pThemesWidget)
{
	cairo_dock_discard_task (pThemesWidget->pImportTask);
	pThemesWidget->pImportTask = NULL;
	gtk_widget_destroy (pThemesWidget->pWaitingDialog);  // stop the pulse too
	pThemesWidget->pWaitingDialog = NULL;
}
static gboolean _pulse_bar (GtkWidget *pBar)
{
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pBar));
	return TRUE;
}
static void on_waiting_dialog_destroyed (GtkWidget *pWidget, ThemesWidget *pThemesWidget)
{
	pThemesWidget->pWaitingDialog = NULL;
	g_source_remove (pThemesWidget->iSidPulse);
	pThemesWidget->iSidPulse = 0;
}
static gboolean _cairo_dock_load_theme (GKeyFile* pKeyFile, GFunc pCallback, ThemesWidget *pThemesWidget)
{
	GtkWindow *pMainWindow = pThemesWidget->pMainWindow;
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
	
	if (pThemesWidget->pImportTask != NULL)
	{
		cairo_dock_discard_task (pThemesWidget->pImportTask);
		pThemesWidget->pImportTask = NULL;
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
		pThemesWidget->pWaitingDialog = pWaitingDialog;
		gtk_window_set_decorated (GTK_WINDOW (pWaitingDialog), FALSE);
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (pWaitingDialog), TRUE);
		gtk_window_set_skip_pager_hint (GTK_WINDOW (pWaitingDialog), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW (pWaitingDialog), pMainWindow);
		gtk_window_set_modal (GTK_WINDOW (pWaitingDialog), TRUE);

		GtkWidget *pMainVBox = _gtk_vbox_new (CAIRO_DOCK_FRAME_MARGIN);
		gtk_container_add (GTK_CONTAINER (pWaitingDialog), pMainVBox);
		
		GtkWidget *pLabel = gtk_label_new (_("Please wait while importing the theme..."));
		gtk_box_pack_start(GTK_BOX (pMainVBox), pLabel, FALSE, FALSE, 0);
		
		GtkWidget *pBar = gtk_progress_bar_new ();
		gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pBar));
		gtk_box_pack_start (GTK_BOX (pMainVBox), pBar, FALSE, FALSE, 0);
		pThemesWidget->iSidPulse = g_timeout_add (100, (GSourceFunc)_pulse_bar, pBar);
		g_signal_connect (G_OBJECT (pWaitingDialog),
			"destroy",
			G_CALLBACK (on_waiting_dialog_destroyed),
			pThemesWidget);
		
		GtkWidget *pCancelButton = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
		g_signal_connect (G_OBJECT (pCancelButton), "clicked", G_CALLBACK(on_cancel_dl), pWaitingDialog);
		gtk_box_pack_start (GTK_BOX (pMainVBox), pCancelButton, FALSE, FALSE, 0);
		
		gtk_widget_show_all (pWaitingDialog);
		
		cd_debug ("start importation...");
		pThemesWidget->pImportTask = cairo_dock_import_theme_async (cNewThemeName, bLoadBehavior, bLoadLaunchers, (GFunc)pCallback, pThemesWidget);
	}
	else
	{
		bThemeImported = cairo_dock_import_theme (cNewThemeName, bLoadBehavior, bLoadLaunchers);
		
		//\_______________ On le charge.
		if (bThemeImported)
		{
			cairo_dock_load_current_theme ();
			cairo_dock_reload_gui ();
		}
	}
	g_free (cNewThemeName);
	return bThemeImported;
}


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
static void _make_tree_view_for_delete_themes (GSList *pWidgetList, GPtrArray *pDataGarbage, GKeyFile* pKeyFile)
{
	const gchar *cGroupName = "Delete";
	//\______________ On recupere le widget de la cle.
	CairoDockGroupKeyWidget *myWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, cGroupName, "deleted themes");
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
	g_object_set (pScrolledWindow, "height-request", 200, NULL);  // environ 10 lignes.
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

static void _build_themes_widget (ThemesWidget *pThemesWidget)
{
	GKeyFile* pKeyFile = cairo_dock_open_key_file (pThemesWidget->cInitConfFile);
	
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	if (pKeyFile != NULL)
	{
		pThemesWidget->widget.pWidget = cairo_dock_build_key_file_widget (pKeyFile,
			NULL,  // gettext domain
			NULL,  // main window
			&pWidgetList,
			pDataGarbage,
			NULL);
		if (cairo_dock_current_theme_need_save ())
			gtk_notebook_set_current_page (GTK_NOTEBOOK (pThemesWidget->widget.pWidget), 1);  // "Save"
	}
	pThemesWidget->widget.pWidgetList = pWidgetList;
	pThemesWidget->widget.pDataGarbage = pDataGarbage;
	
	// complete the widget
	_make_tree_view_for_delete_themes (pWidgetList, pDataGarbage, pKeyFile);
	
	g_key_file_free (pKeyFile);
}

ThemesWidget *cairo_dock_themes_widget_new (GtkWindow *pMainWindow)
{
	ThemesWidget *pThemesWidget = g_new0 (ThemesWidget, 1);
	pThemesWidget->widget.iType = WIDGET_THEMES;
	pThemesWidget->widget.apply = _themes_widget_apply;
	pThemesWidget->widget.reset = _themes_widget_reset;
	
	// build the widget from a .conf file
	pThemesWidget->cInitConfFile = cairo_dock_build_temporary_themes_conf_file ();  // sera supprime a la destruction de la fenetre.
	
	_build_themes_widget (pThemesWidget);
	
	return pThemesWidget;
}


static void _themes_widget_apply (CDWidget *pCdWidget)
{
	ThemesWidget *pThemesWidget = THEMES_WIDGET (pCdWidget);
	GError *erreur = NULL;
	int r;  // resultat de system().
	
	//\_______________ open and update the conf file.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (pThemesWidget->cInitConfFile);
	g_return_if_fail (pKeyFile != NULL);
	
	cairo_dock_update_keyfile_from_widget_list (pKeyFile, pThemesWidget->widget.pWidgetList);
	
	cairo_dock_write_keys_to_file (pKeyFile, pThemesWidget->cInitConfFile);
	
	//\_______________ take the actions relatively to the current page.
	int iNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (pThemesWidget->widget.pWidget));
	gboolean bReloadWindow = FALSE;
	switch (iNumPage)
	{
		case 0:  // load a theme
			cairo_dock_set_status_message (NULL, _("Importing theme ..."));
			_cairo_dock_load_theme (pKeyFile, (GFunc) _load_theme, pThemesWidget);  // bReloadWindow reste a FALSE, on ne rechargera la fenetre que lorsque le theme aura ete importe.
		break;
		
		case 1:  // save current theme
			bReloadWindow = _cairo_dock_save_current_theme (pKeyFile);
			if (bReloadWindow)
				cairo_dock_set_status_message (NULL, _("Theme has been saved"));
		break;
		
		case 2:  // delete some themes
			bReloadWindow = _cairo_dock_delete_user_themes (pKeyFile);
			if (bReloadWindow)
				cairo_dock_set_status_message (NULL, _("Themes have been deleted"));
		break;
	}
	g_key_file_free (pKeyFile);
	
	if (bReloadWindow)
	{
		_themes_widget_reload (pThemesWidget);
	}
}


static void _themes_widget_reset (CDWidget *pCdWidget)
{
	ThemesWidget *pThemesWidget = THEMES_WIDGET (pCdWidget);
	
	g_remove (pThemesWidget->cInitConfFile);
	g_free (pThemesWidget->cInitConfFile);
	
	cairo_dock_discard_task (pThemesWidget->pImportTask);
	
	if (pThemesWidget->iSidPulse != 0)
		g_source_remove (pThemesWidget->iSidPulse);
	
	if (pThemesWidget->pWaitingDialog != NULL)
		gtk_widget_destroy (pThemesWidget->pWaitingDialog);
	
	memset (pCdWidget+1, 0, sizeof (ThemesWidget) - sizeof (CDWidget));  // reset all our parameters.
}


static void _themes_widget_reload (ThemesWidget *pThemesWidget)
{
	cairo_dock_widget_destroy_widget (CD_WIDGET (pThemesWidget));
	
	_build_themes_widget (pThemesWidget);
}

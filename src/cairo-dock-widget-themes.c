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
#include "cairo-dock-dock-manager.h"  // cairo_dock_search_dock_from_name
#include "cairo-dock-applications-manager.h"  // cairo_dock_get_current_active_icon
#include "cairo-dock-themes-manager.h"  // cairo_dock_export_current_theme
#include "cairo-dock-config.h"  // cairo_dock_load_current_theme
#include "cairo-dock-gui-manager.h"  // cairo_dock_set_status_message
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-widget-themes.h"

// Nom du repertoire des themes de dock.
#define CAIRO_DOCK_THEMES_DIR "themes"  /// TODO: define that somewhere
// Nom du repertoire des themes de dock sur le serveur
#define CAIRO_DOCK_DISTANT_THEMES_DIR "themes3.0"  /// TODO: define that somewhere

extern gchar *g_cThemesDirPath;

static void _themes_widget_apply (CDWidget *pCdWidget);
static void _themes_widget_reset (CDWidget *pCdWidget);
static void _themes_widget_reload (CDWidget *pThemesWidget);
static void _fill_treeview_with_themes (ThemesWidget *pThemesWidget);


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
		
		_fill_treeview_with_themes (pThemesWidget);
		cairo_dock_reload_gui ();
	}
	else
		cairo_dock_set_status_message (NULL, _("Could not import the theme."));
	gtk_widget_destroy (pThemesWidget->pWaitingDialog);
	pThemesWidget->pWaitingDialog = NULL;
	cairo_dock_discard_task (pThemesWidget->pImportTask);
	pThemesWidget->pImportTask = NULL;
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
static gboolean _cairo_dock_load_theme (GKeyFile* pKeyFile, ThemesWidget *pThemesWidget)
{
	GtkWindow *pMainWindow = pThemesWidget->pMainWindow;
	const gchar *cGroupName = "Load theme";
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
	if (iType != CAIRO_DOCK_LOCAL_PACKAGE && iType != CAIRO_DOCK_USER_PACKAGE)
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
		pThemesWidget->pImportTask = cairo_dock_import_theme_async (cNewThemeName, bLoadBehavior, bLoadLaunchers, (GFunc)_load_theme, pThemesWidget);  // we don't need to set this task as "cd-task" on the widget, because the progressbar prevents any action from the user, so the widget can't be destroyed before the task.
	}
	else  // if the theme is already local and uptodate, there is really no need to show a progressbar, because ony the download/unpacking is done asynchonously (and the copy of the files is fast enough).
	{
		bThemeImported = cairo_dock_import_theme (cNewThemeName, bLoadBehavior, bLoadLaunchers);
		
		//\_______________ load it.
		if (bThemeImported)
		{
			cairo_dock_load_current_theme ();
			cairo_dock_reload_gui ();
		}
	}
	g_free (cNewThemeName);
	return bThemeImported;
}


#define CD_MAX_RATING 5
static inline void _render_rating (GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *iter, int iColumnIndex)
{
	gint iRating = 0;
	gtk_tree_model_get (model, iter, iColumnIndex, &iRating, -1);
	if (iRating > CD_MAX_RATING)
		iRating = CD_MAX_RATING;
	if (iRating > 0)
	{
		GString *s = g_string_sized_new (CD_MAX_RATING*4+1);
		int i;
		for (i= 0; i < iRating; i ++)
			g_string_append (s, "★");
		for (;i < CD_MAX_RATING; i ++)
			g_string_append (s, "☆");
		g_object_set (cell, "text", s->str, NULL);  // markup
		g_string_free (s, TRUE);
	}
	else
	{
		gchar *cRateMe = NULL;
		if (iColumnIndex == CAIRO_DOCK_MODEL_ORDER)  // note, peut etre changee (la sobriete ne peut pas).
			cRateMe = g_strconcat ("<small><i>", _("Rate me"), "</i></small>", NULL);
		g_object_set (cell, "markup", cRateMe ? cRateMe : "   -", NULL);  // pour la sobriete d'un theme utilisateur, plutot que d'avoir une case vide, on met un tiret dedans.
		g_free (cRateMe);
	}
}
static void _cairo_dock_render_sobriety (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, gpointer data)
{
	_render_rating (cell, model, iter, CAIRO_DOCK_MODEL_ORDER2);
}
static void _cairo_dock_render_rating (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, gpointer data)
{
	/// ignorer les themes "default" qui sont en lecture seule...
	_render_rating (cell, model, iter, CAIRO_DOCK_MODEL_ORDER);
}

static GtkListStore *_make_rate_list_store (void)
{
	GString *s = g_string_sized_new (CD_MAX_RATING*4+1);
	GtkListStore *note_list = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
	GtkTreeIter iter;
	int i, j;
	for (i = 1; i <= 5; i ++)
	{
		g_string_assign (s, "");
		for (j= 0; j < i; j ++)
			g_string_append (s, "★");
		for (;j < CD_MAX_RATING; j ++)
			g_string_append (s, "☆");
		
		memset (&iter, 0, sizeof (GtkTreeIter));
		gtk_list_store_append (GTK_LIST_STORE (note_list), &iter);
		gtk_list_store_set (GTK_LIST_STORE (note_list), &iter,
			0, i,
			1, s->str, -1);
	}
	g_string_free (s, TRUE);
	return note_list;
}

static void _change_rating (GtkCellRendererText * cell, gchar * path_string, gchar * new_text, GtkTreeModel * model)
{
	//g_print ("%s (%s : %s)\n", __func__, path_string, new_text);
	g_return_if_fail (new_text != NULL && *new_text != '\0');
	
	GtkTreeIter it;
	if (! gtk_tree_model_get_iter_from_string (model, &it, path_string))
		return ;
	
	int iRating = 0;
	gchar *str = new_text;
	do
	{
		if (strncmp (str, "★", strlen ("★")) == 0)
		{
			str += strlen ("★");
			iRating ++;
		}
		else
			break ;
	} while (1);
	//g_print ("iRating : %d\n", iRating);
	
	gchar *cThemeName = NULL;
	gint iState;
	gtk_tree_model_get (model, &it,
		CAIRO_DOCK_MODEL_RESULT, &cThemeName,
		CAIRO_DOCK_MODEL_STATE, &iState, -1);
	g_return_if_fail (cThemeName != NULL);
	cairo_dock_extract_package_type_from_name (cThemeName);
	//g_print ("theme : %s / %s\n", cThemeName, cDisplayedName);
	
	gchar *cRatingDir = g_strdup_printf ("%s/.rating", g_cThemesDirPath);  // il y'a un probleme ici, on ne connait pas le repertoire utilisateur des themes. donc ce code ne marche que pour les themes du dock (et n'est utilise que pour ca)
	gchar *cRatingFile = g_strdup_printf ("%s/%s", cRatingDir, cThemeName);
	//g_print ("on ecrit dans %s\n", cRatingFile);
	if (iState == CAIRO_DOCK_USER_PACKAGE || iState == CAIRO_DOCK_LOCAL_PACKAGE || g_file_test (cRatingFile, G_FILE_TEST_EXISTS))  // ca n'est pas un theme distant, ou l'utilisateur a deja vote auparavant pour ce theme.
	{
		if (!g_file_test (cRatingDir, G_FILE_TEST_IS_DIR))
		{
			if (g_mkdir (cRatingDir, 7*8*8+7*8+5) != 0)
			{
				cd_warning ("couldn't create directory %s", cRatingDir);
				return ;
			}
		}
		gchar *cContent = g_strdup_printf ("%d", iRating);
		g_file_set_contents (cRatingFile,
			cContent,
			-1,
			NULL);
		g_free (cContent);
		
		gtk_list_store_set (GTK_LIST_STORE (model), &it, CAIRO_DOCK_MODEL_ORDER, iRating, -1);
	}
	else
	{
		Icon *pIcon = cairo_dock_get_current_active_icon ();
		CairoDock *pDock = NULL;
		if (pIcon != NULL)
			pDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
		if (pDock != NULL)
			cairo_dock_show_temporary_dialog_with_icon (_("You must try the theme before you can rate it."), pIcon, CAIRO_CONTAINER (pDock), 3000, "same icon");
		else
			cairo_dock_show_general_message (_("You must try the theme before you can rate it."), 3000);
	}
	g_free (cThemeName);
	g_free (cRatingFile);
	g_free (cRatingDir);
}

static void _got_themes_list (GHashTable *pThemeTable, ThemesWidget *pThemesWidget)
{
	if (pThemeTable == NULL)
	{
		cairo_dock_set_status_message (GTK_WIDGET (pThemesWidget->pMainWindow), "Couldn't list online themes (is connection alive ?)");
		return ;
	}
	else
		cairo_dock_set_status_message (GTK_WIDGET (pThemesWidget->pMainWindow), "");
	
	cairo_dock_discard_task (pThemesWidget->pListTask);
	pThemesWidget->pListTask = NULL;
	
	GtkListStore *pModel = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (pThemesWidget->pTreeView)));
	g_return_if_fail (pModel != NULL);
	
	gtk_list_store_clear (GTK_LIST_STORE (pModel));  // the 2nd time we pass here, the table is full of all themes, so we have to clear it to avoid double local and user themes.
	cairo_dock_fill_model_with_themes (pModel, pThemeTable, NULL);
}

static void _on_delete_theme (GtkMenuItem *pMenuItem, ThemesWidget *pThemesWidget)
{
	// get the selected theme
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pThemesWidget->pTreeView));
	GtkTreeModel *pModel;
	GtkTreeIter iter;
	if (! gtk_tree_selection_get_selected (pSelection, &pModel, &iter))
		return ;
	gchar *cThemeName = NULL;
	gtk_tree_model_get (pModel, &iter,
		CAIRO_DOCK_MODEL_RESULT, &cThemeName, -1);
	cairo_dock_extract_package_type_from_name (cThemeName);
	
	// delete it
	gchar *cThemesList[2] = {cThemeName, NULL};
	gboolean bSuccess = cairo_dock_delete_themes (cThemesList);
	
	// reload the themes list
	if (bSuccess)
	{
		cairo_dock_set_status_message (NULL, _("The theme has been deleted"));
		
		_fill_treeview_with_themes (pThemesWidget);
	}
	g_free (cThemeName);
}
static gboolean _on_click_tree_view (GtkTreeView *pTreeView, GdkEventButton* pButton, ThemesWidget *pThemesWidget)
{
	if ((pButton->button == 3 && pButton->type == GDK_BUTTON_RELEASE)  // right click
	|| (pButton->button == 1 && pButton->type == GDK_2BUTTON_PRESS))  // double click
	{
		GtkTreeSelection *pSelection = gtk_tree_view_get_selection (pTreeView);
		GtkTreeModel *pModel;
		GtkTreeIter iter;
		if (! gtk_tree_selection_get_selected (pSelection, &pModel, &iter))
			return FALSE;
		
		if (pButton->button == 3)  // show the menu if needed (ie, if the theme can be deleted).
		{
			gchar *cThemeName = NULL;
			gtk_tree_model_get (pModel, &iter,
				CAIRO_DOCK_MODEL_RESULT, &cThemeName, -1);
			CairoDockPackageType iType = cairo_dock_extract_package_type_from_name (cThemeName);  // the type is encoded inside the result; one could also see if the theme folder is present on the disk.
			g_free (cThemeName);
			if (iType == CAIRO_DOCK_USER_PACKAGE || iType == CAIRO_DOCK_UPDATED_PACKAGE)
			{
				GtkWidget *pMenu = gtk_menu_new ();
				
				cairo_dock_add_in_menu_with_stock_and_data (_("Delete this theme"), GTK_STOCK_DELETE, G_CALLBACK (_on_delete_theme), pMenu, pThemesWidget);
				
				gtk_widget_show_all (pMenu);
				gtk_menu_popup (GTK_MENU (pMenu),
					NULL,
					NULL,
					NULL,
					NULL,
					1,
					gtk_get_current_event_time ());
			}
		}
		else  // load the theme
		{
			
		}
	}
	return FALSE;
}

static void _fill_treeview_with_themes (ThemesWidget *pThemesWidget)
{
	const gchar *cShareThemesDir = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_THEMES_DIR;
	const gchar *cUserThemesDir = g_cThemesDirPath;
	const gchar *cDistantThemesDir = CAIRO_DOCK_DISTANT_THEMES_DIR;
	
	// list local packages first
	GHashTable *pThemeTable = cairo_dock_list_packages (cShareThemesDir, cUserThemesDir, NULL, NULL);
	_got_themes_list (pThemeTable, pThemesWidget);
	
	// and list distant packages asynchronously.
	cairo_dock_set_status_message_printf (GTK_WIDGET (pThemesWidget->pMainWindow), _("Listing themes in '%s' ..."), cDistantThemesDir);
	pThemesWidget->pListTask = cairo_dock_list_packages_async (NULL, NULL, cDistantThemesDir, (CairoDockGetPackagesFunc) _got_themes_list, pThemesWidget, pThemeTable);  // the table will be freed along with the task.
}

static void _make_tree_view_for_themes (ThemesWidget *pThemesWidget, GPtrArray *pDataGarbage, GKeyFile* pKeyFile)
{
	//\______________ get the group/key widget
	GSList *pWidgetList = pThemesWidget->widget.pWidgetList;
	GtkWindow *pMainWindow = pThemesWidget->pMainWindow;
	CairoDockGroupKeyWidget *myWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Load theme", "chosen theme");
	g_return_if_fail (myWidget != NULL);
	
	//\______________ build the treeview.
	GtkWidget *pOneWidget = cairo_dock_gui_make_tree_view (TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (pOneWidget), TRUE);
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (pOneWidget), TRUE);
	GtkTreeModel *pModel = gtk_tree_view_get_model (GTK_TREE_VIEW (pOneWidget));
	GtkTreeViewColumn* col;
	GtkCellRenderer *rend;
	// state
	rend = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "pixbuf", CAIRO_DOCK_MODEL_ICON, NULL);
	// nom du theme
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Theme"), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_NAME);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	// rating
	GtkListStore *note_list = _make_rate_list_store ();
	rend = gtk_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (rend),
		"text-column", 1,
		"model", note_list,
		"has-entry", FALSE,
		"editable", TRUE,
		NULL);
	g_signal_connect (G_OBJECT (rend), "edited", (GCallback) _change_rating, pModel);
	col = gtk_tree_view_column_new_with_attributes (_("Rating"), rend, "text", CAIRO_DOCK_MODEL_ORDER, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_ORDER);
	gtk_tree_view_column_set_cell_data_func (col, rend, (GtkTreeCellDataFunc)_cairo_dock_render_rating, NULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	// soberty
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes (_("Sobriety"), rend, "text", CAIRO_DOCK_MODEL_ORDER2, NULL);
	gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_ORDER2);
	gtk_tree_view_column_set_cell_data_func (col, rend, (GtkTreeCellDataFunc)_cairo_dock_render_sobriety, NULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
	// vertical scrollbar
	pThemesWidget->pTreeView = pOneWidget;
	GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pOneWidget);
	// menu
	g_signal_connect (G_OBJECT (pOneWidget), "button-release-event", G_CALLBACK (_on_click_tree_view), pThemesWidget);
	g_signal_connect (G_OBJECT (pOneWidget), "button-press-event", G_CALLBACK (_on_click_tree_view), pThemesWidget);
	
	//\______________ add a preview widget next to the treeview
	GtkWidget *pPreviewBox = cairo_dock_gui_make_preview_box (NULL, pOneWidget, FALSE, 2, NULL, CAIRO_DOCK_SHARE_DATA_DIR"/images/"CAIRO_DOCK_LOGO, pDataGarbage);  // vertical packaging.
	GtkWidget *pWidgetBox = _gtk_hbox_new (CAIRO_DOCK_GUI_MARGIN);
	gtk_box_pack_start (GTK_BOX (pWidgetBox), pScrolledWindow, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (pWidgetBox), pPreviewBox, TRUE, TRUE, 0);
	
	//\______________ get the local, shared and distant themes.
	_fill_treeview_with_themes (pThemesWidget);
	
	//\______________ insert the widget.
	myWidget->pSubWidgetList = g_slist_append (myWidget->pSubWidgetList, pOneWidget);
	gtk_box_pack_start (GTK_BOX (myWidget->pKeyBox), pWidgetBox, FALSE, FALSE, 0);
}

static void _build_themes_widget (ThemesWidget *pThemesWidget)
{
	GtkWindow *pMainWindow = pThemesWidget->pMainWindow;
	GKeyFile* pKeyFile = cairo_dock_open_key_file (pThemesWidget->cInitConfFile);
	
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	if (pKeyFile != NULL)
	{
		pThemesWidget->widget.pWidget = cairo_dock_build_key_file_widget (pKeyFile,
			NULL,  // gettext domain
			GTK_WIDGET (pMainWindow),  // main window
			&pWidgetList,
			pDataGarbage,
			NULL);
		if (cairo_dock_current_theme_need_save ())
			gtk_notebook_set_current_page (GTK_NOTEBOOK (pThemesWidget->widget.pWidget), 1);  // "Save"
	}
	pThemesWidget->widget.pWidgetList = pWidgetList;
	pThemesWidget->widget.pDataGarbage = pDataGarbage;
	
	// complete the widget
	_make_tree_view_for_themes (pThemesWidget, pDataGarbage, pKeyFile);
	
	g_key_file_free (pKeyFile);
}

ThemesWidget *cairo_dock_themes_widget_new (GtkWindow *pMainWindow)
{
	ThemesWidget *pThemesWidget = g_new0 (ThemesWidget, 1);
	pThemesWidget->widget.iType = WIDGET_THEMES;
	pThemesWidget->widget.apply = _themes_widget_apply;
	pThemesWidget->widget.reset = _themes_widget_reset;
	pThemesWidget->widget.reload = _themes_widget_reload;
	pThemesWidget->pMainWindow = pMainWindow;
	
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
			_cairo_dock_load_theme (pKeyFile, pThemesWidget);  // bReloadWindow reste a FALSE, on ne rechargera la fenetre que lorsque le theme aura ete importe.
		break;
		
		case 1:  // save current theme
			bReloadWindow = _cairo_dock_save_current_theme (pKeyFile);
			if (bReloadWindow)
				cairo_dock_set_status_message (NULL, _("Theme has been saved"));
		break;
	}
	g_key_file_free (pKeyFile);
	
	if (bReloadWindow)
	{
		_fill_treeview_with_themes (pThemesWidget);
		
		/// TODO: also reload the combo's model ... and for that we should build it ourselves ...
		
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


static void _themes_widget_reload (CDWidget *pCdWidget)
{
	ThemesWidget *pThemesWidget = THEMES_WIDGET (pCdWidget);
	
	cairo_dock_widget_destroy_widget (pCdWidget);
	
	_build_themes_widget (pThemesWidget);
}

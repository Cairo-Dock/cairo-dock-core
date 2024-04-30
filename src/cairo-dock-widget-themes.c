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
#include "gldi-icon-names.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-packages.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-dialog-factory.h"  // gldi_dialog_show_and_wait
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-icon-facility.h"  // gldi_icons_get_any_without_dialog
#include "cairo-dock-task.h"
#include "cairo-dock-dock-manager.h"  // gldi_dock_get
#include "cairo-dock-applications-manager.h"  // cairo_dock_get_current_active_icon
#include "cairo-dock-themes-manager.h"  // cairo_dock_export_current_theme
#include "cairo-dock-config.h"  // cairo_dock_load_current_theme
#include "cairo-dock-gui-manager.h"  // cairo_dock_set_status_message
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-widget-themes.h"

extern gchar *g_cThemesDirPath;
extern CairoDock *g_pMainDock;

static void _themes_widget_apply (CDWidget *pCdWidget);
static void _themes_widget_reset (CDWidget *pCdWidget);
static void _themes_widget_reload (CDWidget *pThemesWidget);
static void _fill_treeview_with_themes (ThemesWidget *pThemesWidget);
static void _fill_combo_with_user_themes (ThemesWidget *pThemesWidget);


static gchar *_cairo_dock_build_temporary_themes_conf_file (void)
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

static void _load_theme (gboolean bSuccess, gpointer data)
{
	ThemesWidget *pThemesWidget = (ThemesWidget*)data;
	
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
	gldi_task_discard (pThemesWidget->pImportTask);
	pThemesWidget->pImportTask = NULL;
}

static gchar * _cairo_dock_save_current_theme (GKeyFile* pKeyFile)
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
		gchar *cDirPath = g_key_file_get_string (pKeyFile, cGroupName, "package dir", NULL);
		bThemeSaved |= cairo_dock_package_current_theme (cNewThemeName, cDirPath);
		g_free (cDirPath);
	}
	
	if (bThemeSaved)
	{
		return cNewThemeName;
	}
	else
	{
		g_free (cNewThemeName);
		return NULL;
	}
}


static void on_cancel_dl (G_GNUC_UNUSED GtkButton *button, ThemesWidget *pThemesWidget)
{
	gldi_task_discard (pThemesWidget->pImportTask);
	pThemesWidget->pImportTask = NULL;
	gtk_widget_destroy (pThemesWidget->pWaitingDialog);  // stop the pulse too
	pThemesWidget->pWaitingDialog = NULL;
}
static gboolean _pulse_bar (GtkWidget *pBar)
{
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pBar));
	return TRUE;
}
static void on_waiting_dialog_destroyed (G_GNUC_UNUSED GtkWidget *pWidget, ThemesWidget *pThemesWidget)
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
		gldi_task_discard (pThemesWidget->pImportTask);
		pThemesWidget->pImportTask = NULL;
	}
	//\___________________ On regarde si le theme courant est modifie.
	gboolean bNeedSave = cairo_dock_current_theme_need_save ();
	if (bNeedSave)
	{
		Icon *pIcon = cairo_dock_get_current_active_icon ();  // it's most probably the icon corresponding to the configuration window
		if (pIcon == NULL || cairo_dock_get_icon_container (pIcon) == NULL)  // if not available, get any icon
			pIcon = gldi_icons_get_any_without_dialog ();
		int iClickedButton = gldi_dialog_show_and_wait (_("You have made some changes to the current theme.\nYou will lose them if you don't save before choosing a new theme. Continue anyway?"),
			pIcon, CAIRO_CONTAINER (g_pMainDock),
			CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, NULL);
		if (iClickedButton != 0 && iClickedButton != -1)  // cancel button or Escape.
		{
			return FALSE;
		}
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
		if (!gldi_container_is_wayland_backend ()) // gtk_window_set_decorated is broken on Wayland
			gtk_window_set_decorated (GTK_WINDOW (pWaitingDialog), FALSE);
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (pWaitingDialog), TRUE);
		gtk_window_set_skip_pager_hint (GTK_WINDOW (pWaitingDialog), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW (pWaitingDialog), pMainWindow);
		gtk_window_set_modal (GTK_WINDOW (pWaitingDialog), TRUE);

		GtkWidget *pMainVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_FRAME_MARGIN);
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
		
		GtkWidget *pCancelButton = gtk_button_new_with_label (_("Cancel"));
		g_signal_connect (G_OBJECT (pCancelButton), "clicked", G_CALLBACK(on_cancel_dl), pWaitingDialog);
		gtk_box_pack_start (GTK_BOX (pMainVBox), pCancelButton, FALSE, FALSE, 0);
		
		gtk_widget_show_all (pWaitingDialog);
		
		cd_debug ("start importation...");
		pThemesWidget->pImportTask = cairo_dock_import_theme_async (cNewThemeName, bLoadBehavior, bLoadLaunchers, _load_theme, pThemesWidget);  // if 'pThemesWidget' is destroyed, the 'reset' callback will be called and will cancel the task.
	}
	else  // if the theme is already local and uptodate, there is really no need to show a progressbar, because only the download/unpacking is done asynchonously (and the copy of the files is fast enough).
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
		if (iColumnIndex == CAIRO_DOCK_MODEL_ORDER)  // note, peut etre changee (la sobriete ne peut pas).
		{
			gchar *cRateMe = g_strconcat ("<small><i>", _("Rate me"), "</i></small>", NULL);
			g_object_set (cell, "markup", cRateMe, NULL);
			g_free (cRateMe);
		}
		else 
			g_object_set (cell, "markup", "   -", NULL);  // pour la sobriete d'un theme utilisateur, plutot que d'avoir une case vide, on met un tiret dedans.
	}
}
static void _cairo_dock_render_sobriety (G_GNUC_UNUSED GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
{
	_render_rating (cell, model, iter, CAIRO_DOCK_MODEL_ORDER2);
}
static void _cairo_dock_render_rating (G_GNUC_UNUSED GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
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

static void _change_rating (G_GNUC_UNUSED GtkCellRendererText * cell, gchar * path_string, gchar * new_text, GtkTreeModel * model)
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
		Icon *pIcon = cairo_dock_get_current_active_icon ();  // most probably the appli-icon representing the config window.
		GldiContainer *pContainer = (pIcon != NULL ? cairo_dock_get_icon_container (pIcon) : NULL);
		if (pContainer != NULL)
			gldi_dialog_show_temporary_with_icon (_("You must try the theme before you can rate it."), pIcon, pContainer, 3000, "same icon");
		else
			gldi_dialog_show_general_message (_("You must try the theme before you can rate it."), 3000);
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
	
	gldi_task_discard (pThemesWidget->pListTask);
	pThemesWidget->pListTask = NULL;
	
	GtkListStore *pModel = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (pThemesWidget->pTreeView)));
	g_return_if_fail (pModel != NULL);
	
	gtk_list_store_clear (GTK_LIST_STORE (pModel));  // the 2nd time we pass here, the table is full of all themes, so we have to clear it to avoid double local and user themes.
	cairo_dock_fill_model_with_themes (pModel, pThemeTable, NULL);
}

static void _on_delete_theme (G_GNUC_UNUSED GtkMenuItem *pMenuItem, ThemesWidget *pThemesWidget)
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
		
		_fill_combo_with_user_themes (pThemesWidget);
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
				cairo_dock_gui_menu_item_add (pMenu, _("Delete this theme"), GLDI_ICON_NAME_DELETE, G_CALLBACK (_on_delete_theme), pThemesWidget);
				gtk_widget_show_all (pMenu);
				gtk_menu_popup_at_pointer (GTK_MENU (pMenu), NULL);
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

static void _make_tree_view_for_themes (ThemesWidget *pThemesWidget, GPtrArray *pDataGarbage, G_GNUC_UNUSED GKeyFile* pKeyFile)
{
	//\______________ get the group/key widget
	GSList *pWidgetList = pThemesWidget->widget.pWidgetList;
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
	gtk_container_add (GTK_CONTAINER (pScrolledWindow), pOneWidget);
	// menu
	g_signal_connect (G_OBJECT (pOneWidget), "button-release-event", G_CALLBACK (_on_click_tree_view), pThemesWidget);
	g_signal_connect (G_OBJECT (pOneWidget), "button-press-event", G_CALLBACK (_on_click_tree_view), pThemesWidget);
	
	//\______________ add a preview widget next to the treeview
	GtkWidget *pPreviewBox = cairo_dock_gui_make_preview_box (GTK_WIDGET (pThemesWidget->pMainWindow), pOneWidget, FALSE, 2, NULL, CAIRO_DOCK_SHARE_DATA_DIR"/images/"CAIRO_DOCK_LOGO, pDataGarbage);
	GtkWidget *pWidgetBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
	gtk_box_pack_start (GTK_BOX (pWidgetBox), pScrolledWindow, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (pWidgetBox), pPreviewBox, TRUE, TRUE, 0);
	
	//\______________ get the user, shared and distant themes.
	_fill_treeview_with_themes (pThemesWidget);
	
	//\______________ insert the widget.
	myWidget->pSubWidgetList = g_slist_append (myWidget->pSubWidgetList, pOneWidget);
	gtk_box_pack_start (GTK_BOX (myWidget->pKeyBox), pWidgetBox, FALSE, FALSE, 0);
}

static gboolean _ignore_server_themes (G_GNUC_UNUSED const gchar *cThemeName, CairoDockPackage *pTheme, G_GNUC_UNUSED gpointer data)
{
	gchar *cVersionFile = g_strdup_printf ("%s/last-modif", pTheme->cPackagePath);
	gboolean bRemove = g_file_test (cVersionFile, G_FILE_TEST_EXISTS);
	g_free (cVersionFile);
	return bRemove;
}
static void _fill_combo_with_user_themes (ThemesWidget *pThemesWidget)
{
	const gchar *cUserThemesDir = g_cThemesDirPath;
	GHashTable *pThemeTable = cairo_dock_list_packages (NULL, cUserThemesDir, NULL, NULL);
	g_hash_table_foreach_remove (pThemeTable, (GHRFunc)_ignore_server_themes, NULL);  // ignore themes coming from the server
	GtkTreeModel *pModel = gtk_combo_box_get_model (GTK_COMBO_BOX (pThemesWidget->pCombo));
	gtk_list_store_clear (GTK_LIST_STORE (pModel));  // for the reload
	cairo_dock_fill_model_with_themes (GTK_LIST_STORE (pModel), pThemeTable, NULL);
	g_hash_table_destroy (pThemeTable);
}
static void _make_combo_for_user_themes (ThemesWidget *pThemesWidget, GPtrArray *pDataGarbage, G_GNUC_UNUSED GKeyFile* pKeyFile)
{
	//\______________ get the group/key widget
	GSList *pWidgetList = pThemesWidget->widget.pWidgetList;
	CairoDockGroupKeyWidget *myWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Save", "theme name");
	g_return_if_fail (myWidget != NULL);
	
	//\______________ build the combo-box.
	GtkWidget *pOneWidget = cairo_dock_gui_make_combo (TRUE);
	pThemesWidget->pCombo = pOneWidget;
	
	//\______________ get the user themes.
	_fill_combo_with_user_themes (pThemesWidget);
	
	//\______________ insert the widget.
	GtkWidget *pHBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
	GtkWidget *pLabel = gtk_label_new (_("Save as:"));
	
	//\______________ add a preview widget under to the combo
	GtkWidget *pPreviewBox = cairo_dock_gui_make_preview_box (GTK_WIDGET (pThemesWidget->pMainWindow), pOneWidget, FALSE, 1, NULL, NULL, pDataGarbage);
	GtkWidget *pWidgetBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN);
	
	GtkWidget *pAlign = gtk_alignment_new (0, 0, 1, 0);
	gtk_container_add (GTK_CONTAINER (pAlign), pLabel);
	gtk_box_pack_start (GTK_BOX (pHBox), pAlign, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (pHBox), pWidgetBox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pWidgetBox), pOneWidget, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pWidgetBox), pPreviewBox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (myWidget->pKeyBox), pHBox, TRUE, TRUE, 0);
	
	myWidget->pSubWidgetList = g_slist_append (myWidget->pSubWidgetList, pOneWidget);
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
	_make_combo_for_user_themes (pThemesWidget, pDataGarbage, pKeyFile);
	
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
	pThemesWidget->cInitConfFile = _cairo_dock_build_temporary_themes_conf_file ();  // sera supprime a la destruction de la fenetre.
	
	_build_themes_widget (pThemesWidget);
	
	return pThemesWidget;
}


static void _themes_widget_apply (CDWidget *pCdWidget)
{
	ThemesWidget *pThemesWidget = THEMES_WIDGET (pCdWidget);
	
	//\_______________ open and update the conf file.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (pThemesWidget->cInitConfFile);
	g_return_if_fail (pKeyFile != NULL);
	
	cairo_dock_update_keyfile_from_widget_list (pKeyFile, pThemesWidget->widget.pWidgetList);
	
	cairo_dock_write_keys_to_file (pKeyFile, pThemesWidget->cInitConfFile);
	
	//\_______________ take the actions relatively to the current page.
	int iNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (pThemesWidget->widget.pWidget));
	gchar *cNewThemeName = NULL;
	switch (iNumPage)
	{
		case 0:  // load a theme
			cairo_dock_set_status_message (NULL, _("Importing theme ..."));
			_cairo_dock_load_theme (pKeyFile, pThemesWidget);  // we don't reload the window in this case, we'll do it once the theme has been imported successfully
		break;
		
		case 1:  // save current theme
			cNewThemeName = _cairo_dock_save_current_theme (pKeyFile);
			if (cNewThemeName != NULL)
			{
				cairo_dock_set_status_message (NULL, _("Theme has been saved"));
				
				_fill_treeview_with_themes (pThemesWidget);
				
				_fill_combo_with_user_themes (pThemesWidget);
				
				cairo_dock_gui_select_in_combo_full (pThemesWidget->pCombo, cNewThemeName, TRUE);
			}
		break;
	}
	g_key_file_free (pKeyFile);
}


static void _themes_widget_reset (CDWidget *pCdWidget)
{
	ThemesWidget *pThemesWidget = THEMES_WIDGET (pCdWidget);
	
	g_remove (pThemesWidget->cInitConfFile);
	g_free (pThemesWidget->cInitConfFile);
	
	gldi_task_discard (pThemesWidget->pImportTask);
	
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

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

#include "config.h"
#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-task.h"
#include "cairo-dock-log.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-gui-commons.h"

static CairoDockTask *s_pImportTask = NULL;

extern gchar *g_cThemesDirPath;

static void _cairo_dock_fill_model_with_themes (const gchar *cThemeName, CairoDockPackage *pTheme, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
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
	gtk_widget_set (pScrolledWindow, "height-request", 160, NULL);  // environ 8 lignes.
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pOneWidget);
	
	//\______________ On recupere les themes utilisateurs.
	GHashTable *pThemeTable = cairo_dock_list_packages (NULL, g_cThemesDirPath, NULL);

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


gboolean cairo_dock_load_theme (GKeyFile* pKeyFile, GFunc pCallback)
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
	gboolean bThemeImported = FALSE;
	if (pCallback != NULL)
	{
		s_pImportTask = cairo_dock_import_theme_async (cNewThemeName, bLoadBehavior, bLoadLaunchers, (GFunc)pCallback, NULL);
		bThemeImported = TRUE;
	}
	else
	{
		gboolean bThemeImported = cairo_dock_import_theme (cNewThemeName, bLoadBehavior, bLoadLaunchers);
		
		//\_______________ On le charge.
		if (bThemeImported)
		{
			cairo_dock_load_current_theme ();
		}
	}
	g_free (cNewThemeName);
	return bThemeImported;
}

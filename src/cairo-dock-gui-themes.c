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
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-gui-commons.h"
#include "cairo-dock-gui-themes.h"

#define CAIRO_DOCK_THEME_PANEL_WIDTH 1000
#define CAIRO_DOCK_THEME_PANEL_HEIGHT 550

extern gchar *g_cThemesDirPath;

static GtkWidget *s_pThemeManager = NULL;
static CairoDockTask *s_pImportTask = NULL;


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
	gchar *cCommand = g_strdup_printf ("cp \"%s\" \"%s\"", CAIRO_DOCK_SHARE_DATA_DIR"/themes.conf", cTmpConfFile);
	int r = system (cCommand);
	g_free (cCommand);

	close(fds);
	return cTmpConfFile;
}


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
static void _make_widgets (GtkWidget *pThemeManager, GKeyFile *pKeyFile)
{
	cairo_dock_make_tree_view_for_delete_themes (pThemeManager);
}


static void _load_theme (gboolean bSuccess, gpointer data)
{
	if (s_pThemeManager == NULL)  // si l'utilisateur a ferme la fenetre entre-temps, on considere qu'il a abandonne.
		return ;
	if (bSuccess)
	{
		cairo_dock_load_current_theme ();
		
		cairo_dock_set_status_message (s_pThemeManager, "");
		cairo_dock_reload_generic_gui (s_pThemeManager);
	}
	else
		cairo_dock_set_status_message (s_pThemeManager, _("Could not import the theme."));
}

static void on_theme_destroy (gchar *cInitConfFile)
{
	cd_debug ("");
	g_remove (cInitConfFile);
	g_free (cInitConfFile);
	s_pThemeManager = NULL;
}

static gboolean on_theme_apply (gchar *cInitConfFile)
{
	cd_debug ("%s (%s)", __func__, cInitConfFile);
	GError *erreur = NULL;
	int r;  // resultat de system().
	
	//\___________________ On recupere l'onglet courant.
	GtkWidget * pMainVBox= gtk_bin_get_child (GTK_BIN (s_pThemeManager));
	GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
	g_return_val_if_fail (children != NULL, FALSE);
	GtkWidget *pGroupWidget = children->data;
	g_list_free (children);
	int iNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (pGroupWidget));
	
	//\_______________ On recupere l'onglet courant.
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cInitConfFile);
	g_return_val_if_fail (pKeyFile != NULL, FALSE);
	
	//\_______________ On effectue les actions correspondantes.
	gboolean bReloadWindow = FALSE;
	switch (iNumPage)
	{
		case 0:  // load a theme
			cairo_dock_set_status_message (s_pThemeManager, _("Importing theme ..."));
			cairo_dock_load_theme (pKeyFile, (GFunc) _load_theme);  // bReloadWindow reste a FALSE, on ne rechargera la fenetre que lorsque le theme aura ete importe.
		break;
		
		case 1:  // save current theme
			bReloadWindow = cairo_dock_save_current_theme (pKeyFile);
			if (bReloadWindow)
				cairo_dock_set_status_message (s_pThemeManager, _("Theme has been saved"));
		break;
		
		case 2:  // delete some themes
			bReloadWindow = cairo_dock_delete_user_themes (pKeyFile);
			if (bReloadWindow)
				cairo_dock_set_status_message (s_pThemeManager, _("Themes have been deleted"));
		break;
	}
	g_key_file_free (pKeyFile);
	
	return !bReloadWindow;
}

void cairo_dock_manage_themes (void)
{
	if (s_pThemeManager != NULL)
	{
		gtk_window_present (GTK_WINDOW (s_pThemeManager));
		return ;
	}
	
	gchar *cInitConfFile = cairo_dock_build_temporary_themes_conf_file ();  // sera supprime a la destruction de la fenetre.
	
	//\___________________ On laisse l'utilisateur l'editer.
	const gchar *cPresentedGroup = (cairo_dock_current_theme_need_save () ? "Save" : NULL);
	const gchar *cTitle = _("Manage Themes");
	
	s_pThemeManager = cairo_dock_build_generic_gui_full (cInitConfFile,
		NULL, cTitle,
		CAIRO_DOCK_THEME_PANEL_WIDTH, CAIRO_DOCK_THEME_PANEL_HEIGHT,
		(CairoDockApplyConfigFunc) on_theme_apply,
		cInitConfFile,
		(GFreeFunc) on_theme_destroy,
		_make_widgets,
		NULL);
}

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
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gui-simple.h"

#define CAIRO_DOCK_PREVIEW_WIDTH 200
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_SIMPLE_PANEL_WIDTH 1000
#define CAIRO_DOCK_SIMPLE_PANEL_HEIGHT 800

static GtkWidget *s_pSimpleConfigWindow = NULL;
static GtkWidget *s_pSimpleConfigModuleWindow = NULL;
static const  gchar *s_cCurrentModuleName = NULL;
// GSList *s_pCurrentWidgetList;  // liste des widgets du main panel.
// GSList *s_pCurrentModuleWidgetList;  // liste des widgets du module courant.
static int s_iIconSize;
static int s_iTaskbarType;

extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern CairoDock *g_pMainDock;
extern gchar *g_cCairoDockDataDir;

#define cd_reload(module_name) do {\
	pInternalModule = cairo_dock_find_internal_module_from_name (module_name);\
	if (pInternalModule != NULL) \
		cairo_dock_reload_internal_module_from_keyfile (pInternalModule, pKeyFile);\
	} while (0)

static gboolean on_apply_config_simple (gpointer data)
{
	//\_____________ On actualise le fichier de conf principal.
	// on ouvre les 2 fichiers (l'un en lecture, l'autre en ecriture).
	gchar *cConfFilePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_SIMPLE_CONF_FILE);
	GKeyFile* pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_free (cConfFilePath);
	g_return_val_if_fail (pSimpleKeyFile != NULL, TRUE);
	GKeyFile* pKeyFile = cairo_dock_open_key_file (g_cConfFile);
	g_return_val_if_fail (pKeyFile != NULL, TRUE);
	
	// comportement
	CairoDockPositionType iScreenBorder = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "screen border", NULL);
	g_key_file_set_integer (pKeyFile, "Position", "screen border", iScreenBorder);
	
	int iVisibility = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "visibility", NULL);
	g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
	
	gboolean bShowOnClick = g_key_file_get_boolean (pSimpleKeyFile, "Behavior", "show on click", NULL);
	g_key_file_set_boolean (pKeyFile, "Accessibility", "show on click", bShowOnClick);
	
	int iTaskbarType = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "taskbar", NULL);
	if (iTaskbarType != s_iTaskbarType)
	{
		gboolean bShowAppli = TRUE, bHideVisible, bCurrentDesktopOnly, bMixLauncherAppli, bGroupAppliByClass;
		switch (iTaskbarType)
		{
			case 0:
				bShowAppli = FALSE;
			break;
			case 1:
				bMixLauncherAppli = TRUE;
				bHideVisible = TRUE;
				bGroupAppliByClass = FALSE;
				bCurrentDesktopOnly = FALSE;
			break;
			case 2:
				bMixLauncherAppli = TRUE;
				bGroupAppliByClass = TRUE;
				bHideVisible = FALSE;
				bCurrentDesktopOnly = FALSE;
			break;
			case 3:
			default:
				bCurrentDesktopOnly = TRUE;
				bMixLauncherAppli = FALSE;
				bGroupAppliByClass = FALSE;
				bHideVisible = FALSE;
			break;
		}
		g_key_file_set_boolean (pKeyFile, "TaskBar", "show applications", bShowAppli);
		if (bShowAppli)  // sinon on inutile de modifier les autres parametres.
		{
			g_key_file_set_boolean (pKeyFile, "TaskBar", "hide visible", bHideVisible);
			g_key_file_set_boolean (pKeyFile, "TaskBar", "current desktop only", bCurrentDesktopOnly);
			g_key_file_set_boolean (pKeyFile, "TaskBar", "mix launcher appli", bMixLauncherAppli);
			g_key_file_set_boolean (pKeyFile, "TaskBar", "group by class", bGroupAppliByClass);
		}
	}
	
	// apparence
	gchar *cIconTheme = g_key_file_get_string (pSimpleKeyFile, "Appearance", "default icon directory", NULL);
	g_key_file_set_string (pKeyFile, "Icons", "default icon directory", cIconTheme);
	g_free (cIconTheme);
	
	int iIconSize = g_key_file_get_integer (pSimpleKeyFile, "Appearance", "icon size", NULL);
	if (iIconSize != s_iIconSize)
	{
		int iLauncherSize, iIconGap;
		double fMaxScale, fReflectSize;
		switch (iIconSize)
		{
			case 0:  // tres petites
				iLauncherSize = 42;
				fMaxScale = 2.2;
				iIconGap = 5;
				fReflectSize = .4;
			break;
			case 1:  // petites
				iLauncherSize = 44;
				fMaxScale = 2.;
				iIconGap = 4;
				fReflectSize = .4;
			break;
			case 2:  // moyennes
				iLauncherSize = 48;
				fMaxScale = 1.7;
				iIconGap = 2;
				fReflectSize = .5;
			break;
			case 3:  // grandes
			default:
				iLauncherSize = 52;
				fMaxScale = 1.5;
				iIconGap = 2;
				fReflectSize = .6;
			break;
			case 4:  // tres grandes
				iLauncherSize = 56;
				fMaxScale = 1.3;
				iIconGap = 2;
				fReflectSize = .6;
			break;
			
		}
		gint tab[2] = {iLauncherSize, iLauncherSize};
		g_key_file_set_integer_list (pKeyFile, "Icons", "launcher size", tab, 2);
		g_key_file_set_integer_list (pKeyFile, "Icons", "appli size", tab, 2);
		g_key_file_set_integer_list (pKeyFile, "Icons", "applet size", tab, 2);
		/// hauteur des separateurs ?...
		g_key_file_set_double (pKeyFile, "Icons", "zoom max", fMaxScale);
		g_key_file_set_double (pKeyFile, "Icons", "field depth", fReflectSize);
		g_key_file_set_integer (pKeyFile, "Icons", "icon gap", iIconGap);
		s_iIconSize = iIconSize;
	}
	
	gchar *cIconOrder = g_key_file_get_string (pSimpleKeyFile, "Appearance", "icon's type order", NULL);
	g_key_file_set_string (pKeyFile, "Icons", "icon's type order", cIconOrder);
	g_free (cIconOrder);
	
	gboolean bMixAppletsAndLaunchers = g_key_file_get_boolean (pSimpleKeyFile, "Appearance", "mix applets with launchers", NULL);
	g_key_file_set_boolean (pKeyFile, "Icons", "mix applets with launchers", bMixAppletsAndLaunchers);
	
	gchar *cMainDockDefaultRendererName = g_key_file_get_string (pSimpleKeyFile, "Appearance", "main dock view", NULL);
	g_key_file_set_string (pKeyFile, "Views", "main dock view", cMainDockDefaultRendererName);
	g_free (cMainDockDefaultRendererName);
	
	gchar *cSubDockDefaultRendererName = g_key_file_get_string (pSimpleKeyFile, "Appearance", "sub-dock view", NULL);
	g_key_file_set_string (pKeyFile, "Views", "sub-dock view", cSubDockDefaultRendererName);
	g_free (cSubDockDefaultRendererName);
	
	cairo_dock_write_keys_to_file (pKeyFile, g_cConfFile);
	
	//\_____________ On recharge les modules concernes.
	CairoDockInternalModule *pInternalModule;
	cd_reload ("Position");
	cd_reload ("Accessibility");
	cd_reload ("TaskBar");
	cd_reload ("Icons");
	cd_reload ("Views");
	
	return TRUE;
}

static void on_destroy_config_simple (gpointer data)
{
	s_pSimpleConfigWindow = NULL;
}

static GtkWidget * show_main_gui (void)
{
	if (s_pSimpleConfigWindow != NULL)
	{
		gtk_window_present (GTK_WINDOW (s_pSimpleConfigWindow));
		return s_pSimpleConfigWindow;
	}
	
	//\_____________ On actualise le fichier de conf simple.
	// on cree le fichier au besoin, et on l'ouvre.
	gchar *cConfFilePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_SIMPLE_CONF_FILE);
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
	{
		gchar *cCommand = g_strdup_printf ("cp \"%s/%s\" \"%s\"", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_SIMPLE_CONF_FILE, cConfFilePath);
		int r = system (cCommand);
		g_free (cCommand);
	}
	
	GKeyFile* pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_val_if_fail (pSimpleKeyFile != NULL, NULL);
	
	if (cairo_dock_conf_file_needs_update (pSimpleKeyFile, CAIRO_DOCK_VERSION))
	{
		cairo_dock_flush_conf_file (pSimpleKeyFile, cConfFilePath, CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_SIMPLE_CONF_FILE);
		g_key_file_free (pSimpleKeyFile);
		pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
		g_return_val_if_fail (pSimpleKeyFile != NULL, NULL);
	}
	
	// comportement
	CairoDockPositionType iScreenBorder = (g_pMainDock ? ((! g_pMainDock->container.bIsHorizontal) << 1) | (! g_pMainDock->container.bDirectionUp) : 0);
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "screen border", iScreenBorder);
	
	gboolean iVisibility;
	if (myAccessibility.bReserveSpace)
		iVisibility = 1;
	else if (myAccessibility.bPopUp)
		iVisibility = 2;
	else if (myAccessibility.bAutoHide)
		iVisibility = 3;
	else if (myAccessibility.bAutoHideOnMaximized)
		iVisibility = 4;
	else if (myAccessibility.cRaiseDockShortcut)
		iVisibility = 5;
	else
		iVisibility = 0;
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "visibility", iVisibility);
	
	g_key_file_set_boolean (pSimpleKeyFile, "Behavior", "show on click", myAccessibility.bShowSubDockOnClick);
	
	int iTaskbarType;
	if (! myTaskBar.bShowAppli)
		iTaskbarType = 0;
	else if (myTaskBar.bHideVisibleApplis)
		iTaskbarType = 1;
	else if (myTaskBar.bGroupAppliByClass)
		iTaskbarType = 2;
	else
		iTaskbarType = 3;
	s_iTaskbarType = iTaskbarType;
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "taskbar", iTaskbarType);
	
	// apparence
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "default icon directory", myIcons.cIconTheme);
	
	int iIconSize;
	if (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] <= 42)  // icones toutes petites.
		iIconSize = 0;
	else if (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] >= 54)  // icones tres grandes.
		iIconSize = 4;
	else if (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] <= 48)
	{
		if (myIcons.fAmplitude >= 2)  // icones petites.
			iIconSize = 1;
		else
			iIconSize = 3;  // moyennes.
	}
	else  // grandes.
		iIconSize = 4;
	
	g_key_file_set_integer (pSimpleKeyFile, "Appearance", "icon size", iIconSize);
	s_iIconSize = iIconSize;	
	
	g_key_file_set_integer_list (pSimpleKeyFile, "Appearance", "icon's type order", myIcons.iIconsTypesList, 3);
	
	g_key_file_set_boolean (pSimpleKeyFile, "Appearance", "mix applets with launchers", myIcons.bMixAppletsAndLaunchers);
	
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "main dock view", myViews.cMainDockDefaultRendererName);
	
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "sub-dock view", myViews.cSubDockDefaultRendererName);
	
	cairo_dock_write_keys_to_file (pSimpleKeyFile, cConfFilePath);
	
	//\_____________ On construit la fenetre.
	cairo_dock_build_normal_gui (cConfFilePath,
		NULL,
		_("Configuration of Cairo-Dock"),
		CAIRO_DOCK_SIMPLE_PANEL_WIDTH, CAIRO_DOCK_SIMPLE_PANEL_HEIGHT,
		(CairoDockApplyConfigFunc) on_apply_config_simple,
		NULL,
		(GFreeFunc) on_destroy_config_simple,
		&s_pSimpleConfigWindow);
	
	//\_____________ On ajoute un bouton "mode avance".
	GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (s_pSimpleConfigWindow));
	GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
	GList *w = g_list_last (children);
	GtkWidget *pButtonsHBox = w->data;
	GtkWidget *pSwitchButton = cairo_dock_make_switch_gui_button ();
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		pSwitchButton,
		FALSE,
		FALSE,
		0);
	gtk_box_reorder_child (GTK_BOX (pButtonsHBox), pSwitchButton, 1);
	gtk_widget_show_all (pSwitchButton);
	return s_pSimpleConfigWindow;
}


static gboolean on_apply_config_module_simple (gpointer data)
{
	g_print ("%s (%s)\n", __func__, s_cCurrentModuleName);
	CairoDockModule *pModule = cairo_dock_find_module_from_name (s_cCurrentModuleName);
	if (pModule != NULL)
	{
		if (pModule->pInstancesList != NULL)
		{
			gchar *cConfFilePath = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "conf-file");
			CairoDockModuleInstance *pModuleInstance = NULL;
			GList *pElement;
			for (pElement = pModule->pInstancesList; pElement != NULL; pElement= pElement->next)
			{
				pModuleInstance = pElement->data;
				if (strcmp (pModuleInstance->cConfFilePath, cConfFilePath) == 0)
					break ;
			}
			g_return_val_if_fail (pModuleInstance != NULL, TRUE);
			
			cairo_dock_reload_module_instance (pModuleInstance, TRUE);
		}
	}
	return TRUE;
}

static void on_destroy_config_module_simple (gpointer data)
{
	s_pSimpleConfigModuleWindow = NULL;
}


static inline GtkWidget * _present_module_widget (GtkWidget *pWindow, CairoDockModuleInstance *pInstance, const gchar *cConfFilePath, const gchar *cGettextDomain, const gchar *cOriginalConfFilePath)
{
	//\_____________ On construit l'IHM du fichier de conf.
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	GtkWidget *pNoteBook = cairo_dock_build_key_file_widget (pKeyFile, cGettextDomain, pWindow, &pWidgetList, pDataGarbage, cOriginalConfFilePath);
	
	g_object_set_data (G_OBJECT (pWindow), "conf-file", g_strdup (cConfFilePath));
	g_object_set_data (G_OBJECT (pWindow), "widget-list", pWidgetList);
	g_object_set_data (G_OBJECT (pWindow), "garbage", pDataGarbage);
	
	if (pInstance != NULL && pInstance->pModule->pInterface->load_custom_widget != NULL)
		pInstance->pModule->pInterface->load_custom_widget (pInstance, pKeyFile);
	
	g_key_file_free (pKeyFile);
	
	//\_____________ On l'insere dans la fenetre.
	GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (pWindow));
	gtk_box_pack_start (GTK_BOX (pMainVBox),
		pNoteBook,
		TRUE,
		TRUE,
		0);
	
	return pNoteBook;
}

static int _reset_current_module_widget (void)
{
	if (s_pSimpleConfigModuleWindow == NULL)
		return -1;
	GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (s_pSimpleConfigModuleWindow));
	GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
	GtkWidget *pGroupWidget = children->data;
	
	int iNotebookPage = 0;
	if (GTK_IS_NOTEBOOK (pGroupWidget))
		iNotebookPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (pGroupWidget));
	
	gtk_widget_destroy (pGroupWidget);
	GSList *pWidgetList = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "widget-list");
	cairo_dock_free_generated_widget_list (pWidgetList);
	g_object_set_data (G_OBJECT (s_pSimpleConfigModuleWindow), "widget-list", NULL);
	GPtrArray *pDataGarbage = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "garbage");
	if (pDataGarbage != NULL)
		g_ptr_array_free (pDataGarbage, TRUE);
	g_object_set_data (G_OBJECT (s_pSimpleConfigModuleWindow), "garbage", NULL);
	
	return iNotebookPage;
}

static void show_module_instance_gui (CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	int iNotebookPage = -1;
	if (s_pSimpleConfigModuleWindow == NULL)
	{
		s_pSimpleConfigModuleWindow = cairo_dock_build_normal_gui_window (dgettext (pModuleInstance->pModule->pVisitCard->cGettextDomain, pModuleInstance->pModule->pVisitCard->cTitle),
			CAIRO_DOCK_SIMPLE_PANEL_WIDTH, CAIRO_DOCK_SIMPLE_PANEL_HEIGHT,
			(CairoDockApplyConfigFunc) on_apply_config_module_simple,
			NULL,
			(GFreeFunc) on_destroy_config_module_simple);
		iNotebookPage = iShowPage;
	}
	else
	{
		iNotebookPage = _reset_current_module_widget ();
		if (iShowPage >= 0)
			iNotebookPage = iShowPage;
	}
	
	gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", pModuleInstance->pModule->pVisitCard->cShareDataDir, pModuleInstance->pModule->pVisitCard->cConfFileName);
	
	GtkWidget *pGroupWidget = _present_module_widget (s_pSimpleConfigModuleWindow,
		pModuleInstance,
		pModuleInstance->cConfFilePath,
		pModuleInstance->pModule->pVisitCard->cGettextDomain,
		cOriginalConfFilePath);
	
	g_free (cOriginalConfFilePath);
	s_cCurrentModuleName = pModuleInstance->pModule->pVisitCard->cModuleName;
	
	gtk_widget_show_all (s_pSimpleConfigModuleWindow);
	if (iNotebookPage != -1 && GTK_IS_NOTEBOOK (pGroupWidget))
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (pGroupWidget), iNotebookPage);
	}
}

static void show_module_gui (const gchar *cModuleName)
{
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	g_return_if_fail (pModule != NULL);
	
	if (s_pSimpleConfigModuleWindow == NULL)
	{
		s_pSimpleConfigModuleWindow = cairo_dock_build_normal_gui_window (cModuleName,
			CAIRO_DOCK_SIMPLE_PANEL_WIDTH, CAIRO_DOCK_SIMPLE_PANEL_HEIGHT,
			(CairoDockApplyConfigFunc) on_apply_config_module_simple,
			NULL,
			(GFreeFunc) on_destroy_config_module_simple);
	}
	else
	{
		_reset_current_module_widget ();
	}
	
	CairoDockModuleInstance *pModuleInstance = (pModule->pInstancesList != NULL ? pModule->pInstancesList->data : NULL);
	gchar *cConfFilePath = (pModuleInstance != NULL ? pModuleInstance->cConfFilePath : pModule->cConfFilePath);
	gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", pModule->pVisitCard->cShareDataDir, pModule->pVisitCard->cConfFileName);
	
	//\_____________ On insere l'IHM du fichier de conf.
	_present_module_widget (s_pSimpleConfigModuleWindow,
		pModuleInstance,
		cConfFilePath,
		pModule->pVisitCard->cGettextDomain,
		cOriginalConfFilePath);
	
	gtk_widget_show_all (s_pSimpleConfigModuleWindow);
	g_free (cOriginalConfFilePath);
	s_cCurrentModuleName = pModule->pVisitCard->cModuleName;
}

static void set_status_message_on_gui (const gchar *cMessage)
{
	GtkWidget *pStatusBar = NULL;
	if (s_pSimpleConfigModuleWindow != NULL)
	{
		pStatusBar = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "status-bar");
	}
	else if (s_pSimpleConfigWindow != NULL)
	{
		pStatusBar = g_object_get_data (G_OBJECT (s_pSimpleConfigWindow), "status-bar");
	}
	if (pStatusBar == NULL)
		return ;
	gtk_statusbar_pop (GTK_STATUSBAR (pStatusBar), 0);  // clear any previous message, underflow is allowed.
	gtk_statusbar_push (GTK_STATUSBAR (pStatusBar), 0, cMessage);
}

static gboolean module_is_opened (CairoDockModuleInstance *pModuleInstance)
{
	if (s_pSimpleConfigModuleWindow == NULL || s_cCurrentModuleName == NULL || pModuleInstance == NULL)
		return FALSE;
	
	if (strcmp (s_cCurrentModuleName, pModuleInstance->pModule->pVisitCard->cModuleName) != 0)
		return FALSE;
	
	gchar *cConfFilePath = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "conf-file");
	if (cConfFilePath == NULL)
		return FALSE;
	CairoDockModuleInstance *pInstance;
	GList *pElement;
	for (pElement = pModuleInstance->pModule->pInstancesList; pElement != NULL; pElement= pElement->next)
	{
		pInstance = pElement->data;
		if (pInstance->cConfFilePath && strcmp (pModuleInstance->cConfFilePath, pInstance->cConfFilePath) == 0)
			return TRUE;
	}
	return FALSE;
}

static gboolean _test_one_module_name (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer *data)
{
	gchar *cResult = NULL;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_RESULT, &cResult, -1);
	g_print ("- %s !\n", cResult
	);
	if (cResult && strcmp (data[0], cResult) == 0)
	{
		GtkTreeIter *iter_to_fill = data[1];
		memcpy (iter_to_fill, iter, sizeof (GtkTreeIter));
		gboolean *bFound = data[2];
		*bFound = TRUE;
		g_free (cResult);
		return TRUE;
	}
	g_free (cResult);
	return FALSE;
}
static void deactivate_module_in_gui (const gchar *cModuleName)
{
	if (s_pSimpleConfigWindow == NULL || cModuleName == NULL)
		return ;
	
	g_print ("TOTO\n");
	/// desactiver la ligne qui va bien dans le gtk_list_store...
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	g_return_if_fail (pModule != NULL);
	
	GSList *pCurrentWidgetList = g_object_get_data (G_OBJECT (s_pSimpleConfigWindow), "widget-list");
	g_return_if_fail (pCurrentWidgetList != NULL);
	
	GSList *pWidgetList = NULL;
	switch (pModule->pVisitCard->iCategory)
	{
		case CAIRO_DOCK_CATEGORY_APPLET_ACCESSORY:
			pWidgetList = cairo_dock_find_widgets_from_name (pCurrentWidgetList, "Add-ons", "accessories");
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_DESKTOP:
			pWidgetList = cairo_dock_find_widgets_from_name (pCurrentWidgetList, "Add-ons", "desktops");
		break;
		case CAIRO_DOCK_CATEGORY_APPLET_CONTROLER:
			pWidgetList = cairo_dock_find_widgets_from_name (pCurrentWidgetList, "Add-ons", "controlers");
		break;
		case CAIRO_DOCK_CATEGORY_PLUG_IN:
			pWidgetList = cairo_dock_find_widgets_from_name (pCurrentWidgetList, "Add-ons", "plug-ins");
		break;
		default:
		break;
	}
	if (pWidgetList != NULL)
	{
		GtkWidget *pTreeView = pWidgetList->data;
		
		GtkTreeModel *pModele = gtk_tree_view_get_model (GTK_TREE_VIEW (pTreeView));
		gboolean bFound = FALSE;
		GtkTreeIter iter;
		memset (&iter, 0, sizeof (GtkTreeIter));
		gconstpointer data[3] = {cModuleName, &iter, &bFound};
		gtk_tree_model_foreach (GTK_TREE_MODEL (pModele), (GtkTreeModelForeachFunc) _test_one_module_name, data);
		
		if (bFound)
		{
			gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
				CAIRO_DOCK_MODEL_ACTIVE, FALSE, -1);
		}
	}
}

static GSList *get_widgets_from_name (const gchar *cGroupName, const gchar *cKeyName)
{
	g_return_val_if_fail (s_pSimpleConfigModuleWindow != NULL, NULL);
	GSList *pCurrentModuleWidgetList = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "widget-list");
	g_return_val_if_fail (pCurrentModuleWidgetList != NULL, NULL);
	return cairo_dock_find_widgets_from_name (pCurrentModuleWidgetList, cGroupName, cKeyName);
}

static void close_gui (void)
{
	if (s_pSimpleConfigWindow != NULL)
	{
		GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (s_pSimpleConfigWindow));
		GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
		GList *w = g_list_last (children);
		GtkWidget *pButtonsHBox = w->data;
		
		children = gtk_container_get_children (GTK_CONTAINER (pButtonsHBox));
		w = g_list_last (children);
		GtkWidget *pQuitButton = w->data;
		
		gboolean bReturn;
		g_signal_emit_by_name (pQuitButton, "clicked", NULL, &bReturn);
	}
	if (s_pSimpleConfigModuleWindow != NULL)
	{
		GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (s_pSimpleConfigModuleWindow));
		GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
		GList *w = g_list_last (children);
		GtkWidget *pButtonsHBox = w->data;
		
		children = gtk_container_get_children (GTK_CONTAINER (pButtonsHBox));
		w = g_list_last (children);
		GtkWidget *pQuitButton = w->data;
		
		gboolean bReturn;
		g_signal_emit_by_name (pQuitButton, "clicked", NULL, &bReturn);
	}
}

void cairo_dock_register_simple_gui_backend (void)
{
	CairoDockGuiBackend *pBackend = g_new0 (CairoDockGuiBackend, 1);
	
	pBackend->show_main_gui 			= show_main_gui;
	pBackend->show_module_instance_gui 	= show_module_instance_gui;
	pBackend->show_module_gui 			= show_module_gui;
	pBackend->set_status_message_on_gui = set_status_message_on_gui;
	pBackend->module_is_opened 			= module_is_opened;
	pBackend->deactivate_module_in_gui 	= deactivate_module_in_gui;
	pBackend->get_widgets_from_name 	= get_widgets_from_name;
	pBackend->close_gui 				= close_gui;
	
	cairo_dock_register_gui_backend (pBackend);
}

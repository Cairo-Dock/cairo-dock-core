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
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_write_keys_to_conf_file
#include "cairo-dock-module-instance-manager.h"  // gldi_module_instance_reload
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-widget-config-group.h"

#define CAIRO_DOCK_ICON_MARGIN 6
extern gchar *g_cConfFile;


static void _config_group_widget_apply (CDWidget *pCdWidget)
{
	ConfigGroupWidget *pConfigGroupWidget = CONFIG_GROUP_WIDGET (pCdWidget);
	
	// update the conf file.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (g_cConfFile);
	g_return_if_fail (pKeyFile != NULL);

	cairo_dock_update_keyfile_from_widget_list (pKeyFile, pCdWidget->pWidgetList);
	cairo_dock_write_keys_to_conf_file (pKeyFile, g_cConfFile);
	g_key_file_free (pKeyFile);
	
	// reload the associated managers.
	const gchar *cManagerName, *cModuleName;
	GldiModule *pModule;
	GldiModuleInstance *pExtraInstance;
	GldiManager *pManager;
	GSList *pExtraWidgetList;
	GKeyFile* pExtraKeyFile;
	GList *m, *e;
	GSList *w = pConfigGroupWidget->pExtraWidgets;
	for (m = pConfigGroupWidget->pManagers; m != NULL; m = m->next)
	{
		cManagerName = m->data;
		pManager = gldi_get_manager (cManagerName);
		g_return_if_fail (pManager != NULL);
		gldi_reload_manager (pManager, g_cConfFile);
		
		// reload the extensions too
		for (e = pManager->pExternalModules; e != NULL && w != NULL; e = e->next)
		{
			// get the extension
			cModuleName = e->data;
			pModule = gldi_module_get (cModuleName);
			if (!pModule)
				continue;
			
			pExtraInstance = pModule->pInstancesList->data;
			if (pExtraInstance == NULL)
				continue;
			
			// update its conf file
			pExtraKeyFile = cairo_dock_open_key_file (pExtraInstance->cConfFilePath);
			if (pExtraKeyFile == NULL)
				continue;
			
			pExtraWidgetList = w->data;
			w = w->next;
			
			cairo_dock_update_keyfile_from_widget_list (pExtraKeyFile, pExtraWidgetList);
			if (pModule->pInterface->save_custom_widget != NULL)
				pModule->pInterface->save_custom_widget (pExtraInstance, pKeyFile, pExtraWidgetList);
			cairo_dock_write_keys_to_conf_file (pExtraKeyFile, pExtraInstance->cConfFilePath);
			g_key_file_free (pExtraKeyFile);
			
			// reload it
			gldi_object_reload (GLDI_OBJECT(pExtraInstance), TRUE);
		}
	}
}

static void _config_group_widget_reset (CDWidget *pCdWidget)
{
	ConfigGroupWidget *pConfigGroupWidget = CONFIG_GROUP_WIDGET (pCdWidget);
	g_free (pConfigGroupWidget->cGroupName);
	g_slist_foreach (pConfigGroupWidget->pExtraWidgets, (GFunc)cairo_dock_free_generated_widget_list, NULL);
	g_slist_free (pConfigGroupWidget->pExtraWidgets);
	memset (pCdWidget+1, 0, sizeof (ConfigGroupWidget) - sizeof (CDWidget));  // reset all our parameters.
}

ConfigGroupWidget *cairo_dock_config_group_widget_new (const gchar *cGroupName, GList *pManagers, const gchar *cTitle, const gchar *cIcon)
{
	ConfigGroupWidget *pConfigGroupWidget = g_new0 (ConfigGroupWidget, 1);
	pConfigGroupWidget->widget.iType = WIDGET_CONFIG_GROUP;
	pConfigGroupWidget->widget.apply = _config_group_widget_apply;
	pConfigGroupWidget->widget.reset = _config_group_widget_reset;
	pConfigGroupWidget->cGroupName = g_strdup (cGroupName);
	pConfigGroupWidget->pManagers = pManagers;
	
	// build its widget based on its config file.
	GKeyFile* pKeyFile = cairo_dock_open_key_file (g_cConfFile);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	GtkWidget *pWidget = cairo_dock_build_group_widget (pKeyFile,
		cGroupName,
		NULL,  // gettext domain
		NULL,  // main window
		&pWidgetList,
		pDataGarbage,
		g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_CONF_FILE));
	pConfigGroupWidget->widget.pWidgetList = pWidgetList;
	pConfigGroupWidget->widget.pDataGarbage = pDataGarbage;
	
	// build the widgets of the extensions
	GtkWidget *pNoteBook = NULL;
	GKeyFile* pExtraKeyFile;
	GldiModule *pModule;
	GldiModuleInstance *pExtraInstance;
	GSList *pExtraWidgetList;
	gchar *cOriginalConfFilePath;
	GldiManager *pManager;
	const gchar *cManagerName, *cModuleName;
	GList *m, *e;
	for (m = pManagers; m != NULL; m = m->next)
	{
		cManagerName = m->data;
		pManager = gldi_get_manager (cManagerName);
		if (!pManager)
			continue;
		
		for (e = pManager->pExternalModules; e != NULL; e = e->next)
		{
			cModuleName = e->data;
			pModule = gldi_module_get (cModuleName);
			if (!pModule)
				continue;
			
			pExtraInstance = pModule->pInstancesList->data;
			if (pExtraInstance == NULL)
				continue;
			
			pExtraKeyFile = cairo_dock_open_key_file (pExtraInstance->cConfFilePath);
			if (pExtraKeyFile == NULL)
				continue;
			
			pExtraWidgetList = NULL;
			cOriginalConfFilePath = g_strdup_printf ("%s/%s", pModule->pVisitCard->cShareDataDir, pModule->pVisitCard->cConfFileName);
			pNoteBook = cairo_dock_build_key_file_widget (pExtraKeyFile,
				pModule->pVisitCard->cGettextDomain,
				NULL,
				&pExtraWidgetList,
				pDataGarbage,  // the garbage array can be mutualized with 'pConfigGroupWidget'
				cOriginalConfFilePath);  /// TODO : fournir pNoteBook a la fonction pour fusionner les differents modules extra...
			
			pConfigGroupWidget->pExtraWidgets = g_slist_append (pConfigGroupWidget->pExtraWidgets, pExtraWidgetList);  // append, so that we can parse the list in the same order again.
			
			if (pModule->pInterface->load_custom_widget != NULL)
				pModule->pInterface->load_custom_widget (pExtraInstance, pExtraKeyFile, pExtraWidgetList);
			
			///g_free (cOriginalConfFilePath);
			g_key_file_free (pExtraKeyFile);
		}
	}
	
	// on rajoute la page du module interne en 1er dans le notebook.
	if (pNoteBook != NULL)
	{
		GtkWidget *pLabel = gtk_label_new (cTitle);
		GtkWidget *pLabelContainer = NULL;
		GtkWidget *pAlign = NULL;
		if (cIcon != NULL && *cIcon != '\0')
		{
			pLabelContainer = _gtk_hbox_new (CAIRO_DOCK_ICON_MARGIN);
			pAlign = gtk_alignment_new (0., 0.5, 0., 0.);
			gtk_container_add (GTK_CONTAINER (pAlign), pLabelContainer);

			GtkWidget *pImage = _gtk_image_new_from_file (cIcon, GTK_ICON_SIZE_BUTTON);
			gtk_container_add (GTK_CONTAINER (pLabelContainer),
				pImage);
			gtk_container_add (GTK_CONTAINER (pLabelContainer), pLabel);
			gtk_widget_show_all (pLabelContainer);
		}
		
		GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);  // add scrollbars on the widget before putting it into the notebook.
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		#if GTK_CHECK_VERSION (3, 8, 0)
		gtk_container_add (GTK_CONTAINER (pScrolledWindow), pWidget);
		#else
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pWidget);
		#endif
		
		gtk_notebook_prepend_page (GTK_NOTEBOOK (pNoteBook), pScrolledWindow, (pAlign != NULL ? pAlign : pLabel));
		pWidget = pNoteBook;
	}
	
	pConfigGroupWidget->widget.pWidget = pWidget;
	
	g_key_file_free (pKeyFile);
	return pConfigGroupWidget;
}


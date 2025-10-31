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
#include "cairo-dock-module-manager.h"
#include "cairo-dock-module-instance-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-themes-manager.h"  // cairo_dock_write_keys_to_conf_file
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-commons.h"
#include "cairo-dock-widget-module.h"

static gboolean _on_instance_destroyed (ModuleWidget *pModuleWidget, G_GNUC_UNUSED GldiModuleInstance *pInstance);
static gboolean _on_module_activated (ModuleWidget *pModuleWidget, G_GNUC_UNUSED const gchar *cModuleName, gboolean bActive);

static gchar *_get_valid_module_conf_file (GldiModule *pModule)
{
	if (pModule->pVisitCard->cConfFileName != NULL)  // not instantiated yet, take a conf-file in the module's user dir, or the default conf-file.
	{
		// open the module's user dir.
		gchar *cUserDataDirPath = gldi_module_get_config_dir (pModule);
		cd_debug ("cUserDataDirPath: %s", cUserDataDirPath);
		GDir *dir = g_dir_open (cUserDataDirPath, 0, NULL);
		if (dir == NULL)
		{
			g_free (cUserDataDirPath);
			return NULL;
		}
		
		// look for a conf-file.
		const gchar *cFileName;
		gchar *cInstanceFilePath = NULL;
		while ((cFileName = g_dir_read_name (dir)) != NULL)
		{
			gchar *str = strstr (cFileName, ".conf");
			if (!str)
				continue;
			if (*(str+5) != '-' && *(str+5) != '\0')  // xxx.conf or xxx.conf-i
				continue;
			cInstanceFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, cFileName);
			break;
		}
		g_dir_close (dir);
		// if no conf-file, copy the default one into the folder and take this one.
		if (cInstanceFilePath == NULL)  // no conf file present yet.
		{
			gboolean r = cairo_dock_copy_file (pModule->cConfFilePath, cUserDataDirPath);
			if (r)  // copy ok
				cInstanceFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, pModule->pVisitCard->cConfFileName);
		}
		g_free (cUserDataDirPath);
		return cInstanceFilePath;
	}
	return NULL;
}

static void _module_widget_apply (CDWidget *pCdWidget)
{
	ModuleWidget *pModuleWidget = MODULE_WIDGET (pCdWidget);
	GldiModule *pModule = pModuleWidget->pModule;
	
	// update the conf file.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (pModuleWidget->cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);

	cairo_dock_update_keyfile_from_widget_list (pKeyFile, pCdWidget->pWidgetList);
	if (pModule->pInterface->save_custom_widget != NULL)
		pModule->pInterface->save_custom_widget (pModuleWidget->pModuleInstance, pKeyFile, pCdWidget->pWidgetList);  // the instance can be NULL
	cairo_dock_write_keys_to_conf_file (pKeyFile, pModuleWidget->cConfFilePath);
	g_key_file_free (pKeyFile);
	
	// reload the module instance
	if (pModuleWidget->pModuleInstance != NULL)
	{
		gldi_object_reload (GLDI_OBJECT(pModuleWidget->pModuleInstance), TRUE);
	}
}

static void _module_widget_reset (CDWidget *pCdWidget)
{
	ModuleWidget *pModuleWidget = MODULE_WIDGET (pCdWidget);
	
	if (pModuleWidget->pModuleInstance)
	{
		gldi_object_remove_notification (pModuleWidget->pModuleInstance,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_instance_destroyed,
			pModuleWidget);
	}
	gldi_object_remove_notification (pModuleWidget->pModule,
		NOTIFICATION_MODULE_ACTIVATED,
		(GldiNotificationFunc) _on_module_activated,
		pModuleWidget);
	
	g_free (pModuleWidget->cConfFilePath);
	memset (pCdWidget+1, 0, sizeof (ModuleWidget) - sizeof (CDWidget));  // reset all our parameters.
}

static void _build_module_widget (ModuleWidget *pModuleWidget)
{
	GKeyFile* pKeyFile = cairo_dock_open_key_file (pModuleWidget->cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", pModuleWidget->pModule->pVisitCard->cShareDataDir, pModuleWidget->pModule->pVisitCard->cConfFileName);
	pModuleWidget->widget.pWidget = cairo_dock_build_key_file_widget (pKeyFile,
		pModuleWidget->pModule->pVisitCard->cGettextDomain,
		pModuleWidget->pMainWindow,
		&pWidgetList,
		pDataGarbage,
		cOriginalConfFilePath);  // cOriginalConfFilePath is taken by the function
	pModuleWidget->widget.pWidgetList = pWidgetList;
	pModuleWidget->widget.pDataGarbage = pDataGarbage;
	
	if (pModuleWidget->pModule->pInterface->load_custom_widget != NULL)
	{
		pModuleWidget->pModule->pInterface->load_custom_widget (pModuleWidget->pModuleInstance, pKeyFile, pWidgetList);
	}
	
	g_key_file_free (pKeyFile);
}

static void _set_module_instance (ModuleWidget *pModuleWidget, GldiModuleInstance *pModuleInstance)
{
	pModuleWidget->pModuleInstance = pModuleInstance;
	if (pModuleInstance)
	{
		gldi_object_register_notification (pModuleInstance,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_instance_destroyed,
			GLDI_RUN_AFTER, pModuleWidget);
	}
	
	g_free (pModuleWidget->cConfFilePath);
	pModuleWidget->cConfFilePath = (pModuleWidget->pModuleInstance ? g_strdup (pModuleWidget->pModuleInstance->cConfFilePath) : _get_valid_module_conf_file (pModuleWidget->pModule));
}
static void _reload_widget_and_insert (ModuleWidget *pModuleWidget)
{
	GtkWidget *pParentBox = gtk_widget_get_parent (pModuleWidget->widget.pWidget);
	cairo_dock_module_widget_reload_current_widget (pModuleWidget);
	gtk_box_pack_start (GTK_BOX (pParentBox),
		pModuleWidget->widget.pWidget,
		TRUE,
		TRUE,
		CAIRO_DOCK_FRAME_MARGIN);
	gtk_widget_show_all (pModuleWidget->widget.pWidget);
}
static gboolean _on_instance_destroyed (ModuleWidget *pModuleWidget, G_GNUC_UNUSED GldiModuleInstance *pInstance)
{
	// the instance we were linked to is done, we need to forget it and reload the config file with either another instance, or the default conf file.
	if (pModuleWidget->pModule->pInstancesList != NULL)  // the instance is already no more in the list
		_set_module_instance (pModuleWidget, pModuleWidget->pModule->pInstancesList->data);
	else
		_set_module_instance (pModuleWidget, NULL);
	
	_reload_widget_and_insert (pModuleWidget);
	
	return GLDI_NOTIFICATION_LET_PASS;
}
static gboolean _on_module_activated (ModuleWidget *pModuleWidget, G_GNUC_UNUSED const gchar *cModuleName, gboolean bActive)
{
	if (bActive && pModuleWidget->pModuleInstance == NULL)  // we're not linked to any instance yet, and a new one appears -> link to it.
	{
		_set_module_instance (pModuleWidget, pModuleWidget->pModule->pInstancesList->data);
		
		_reload_widget_and_insert (pModuleWidget);
	}
	return GLDI_NOTIFICATION_LET_PASS;
}
ModuleWidget *cairo_dock_module_widget_new (GldiModule *pModule, GldiModuleInstance *pInstance, GtkWidget *pMainWindow)
{
	g_return_val_if_fail (pModule != NULL, NULL);
	
	GldiModuleInstance *pModuleInstance = (pInstance ? pInstance : pModule->pInstancesList != NULL ? pModule->pInstancesList->data : NULL);  // can be NULL if the module is not yet activated.
	ModuleWidget *pModuleWidget = g_new0 (ModuleWidget, 1);
	pModuleWidget->widget.iType = WIDGET_MODULE;
	pModuleWidget->widget.apply = _module_widget_apply;
	pModuleWidget->widget.reset = _module_widget_reset;
	pModuleWidget->pModule = pModule;
	_set_module_instance (pModuleWidget, pModuleInstance);
	pModuleWidget->pMainWindow = pMainWindow;
	
	gldi_object_register_notification (pModule,
		NOTIFICATION_MODULE_ACTIVATED,
		(GldiNotificationFunc) _on_module_activated,
		GLDI_RUN_AFTER, pModuleWidget);
	
	// build its widget based on its config file.
	_build_module_widget (pModuleWidget);
	
	return pModuleWidget;
}


void cairo_dock_module_widget_update_desklet_params (ModuleWidget *pModuleWidget, CairoDesklet *pDesklet)
{
	g_return_if_fail (pModuleWidget != NULL);
	// check that it's about the current module
	if (pDesklet == NULL || pDesklet->pIcon == NULL)
		return;
	
	if (pDesklet->pIcon->pModuleInstance != pModuleWidget->pModuleInstance)
		return;
	
	// update the corresponding widgets
	GSList *pWidgetList = pModuleWidget->widget.pWidgetList;
	cairo_dock_update_desklet_widgets (pDesklet, pWidgetList);
}


void cairo_dock_module_widget_update_desklet_visibility_params (ModuleWidget *pModuleWidget, CairoDesklet *pDesklet)
{
	g_return_if_fail (pModuleWidget != NULL);
	// check that it's about the current module
	if (pDesklet == NULL || pDesklet->pIcon == NULL)
		return;
	
	if (pDesklet->pIcon->pModuleInstance != pModuleWidget->pModuleInstance)
		return;
	
	// update the corresponding widgets
	GSList *pWidgetList = pModuleWidget->widget.pWidgetList;
	cairo_dock_update_desklet_visibility_widgets (pDesklet, pWidgetList);
}


void cairo_dock_module_widget_update_module_instance_container (ModuleWidget *pModuleWidget, GldiModuleInstance *pInstance, gboolean bDetached)
{
	g_return_if_fail (pModuleWidget != NULL);
	// check that it's about the current module
	if (pInstance == NULL)
		return;
	if (pInstance != pModuleWidget->pModuleInstance)
		return;
	
	// update the corresponding widgets
	GSList *pWidgetList = pModuleWidget->widget.pWidgetList;
	cairo_dock_update_is_detached_widget (bDetached, pWidgetList);
}


void cairo_dock_module_widget_reload_current_widget (ModuleWidget *pModuleWidget)
{
	cairo_dock_widget_destroy_widget (CD_WIDGET (pModuleWidget));
	pModuleWidget->widget.pWidget = NULL;
	
	_build_module_widget (pModuleWidget);
}

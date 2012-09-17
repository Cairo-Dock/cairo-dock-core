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
#include "cairo-dock-module-factory.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-X-manager.h"
#include "cairo-dock-gui-commons.h"
#include "cairo-dock-widget-module.h"

static gchar *_get_valid_module_conf_file (CairoDockModule *pModule)
{
	if (pModule->pInstancesList != NULL)  // module is already instanciated, take the first instance's conf-file.
	{
		CairoDockModuleInstance *pModuleInstance = pModule->pInstancesList->data;
		return g_strdup (pModuleInstance->cConfFilePath);
	}
	else if (pModule->pVisitCard->cConfFileName != NULL)  // not instanciated yet, take a conf-file in the module's user dir, or the default conf-file.
	{
		// open the module's user dir.
		gchar *cUserDataDirPath = cairo_dock_check_module_conf_dir (pModule);
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
	CairoDockModule *pModule = pModuleWidget->pModule;
	
	// update the conf file.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (pModuleWidget->cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);

	cairo_dock_update_keyfile_from_widget_list (pKeyFile, pCdWidget->pWidgetList);
	if (pModule->pInterface->save_custom_widget != NULL)
		pModule->pInterface->save_custom_widget (pModuleWidget->pModuleInstance, pKeyFile, pCdWidget->pWidgetList);  // the instance can be NULL
	cairo_dock_write_keys_to_file (pKeyFile, pModuleWidget->cConfFilePath);
	g_key_file_free (pKeyFile);
	
	// reload the module instance
	if (pModuleWidget->pModuleInstance != NULL)
	{
		cairo_dock_reload_module_instance (pModuleWidget->pModuleInstance, TRUE);
	}
}

static void _module_widget_reset (CDWidget *pCdWidget)
{
	ModuleWidget *pModuleWidget = MODULE_WIDGET (pCdWidget);
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
		NULL,  // main window
		&pWidgetList,
		pDataGarbage,
		cOriginalConfFilePath);
	///g_free (cOriginalConfFilePath);
	pModuleWidget->widget.pWidgetList = pWidgetList;
	pModuleWidget->widget.pDataGarbage = pDataGarbage;
	
	if (pModuleWidget->pModule->pInterface->load_custom_widget != NULL)
	{
		pModuleWidget->pModule->pInterface->load_custom_widget (pModuleWidget->pModuleInstance, pKeyFile, pWidgetList);
	}
	
	g_key_file_free (pKeyFile);
}

ModuleWidget *cairo_dock_module_widget_new (CairoDockModule *pModule, CairoDockModuleInstance *pInstance)
{
	g_return_val_if_fail (pModule != NULL, NULL);
	
	CairoDockModuleInstance *pModuleInstance = (pInstance ? pInstance : pModule->pInstancesList != NULL ? pModule->pInstancesList->data : NULL);  // can be NULL if the module is not yet activated.
	gchar *cConfFilePath = (pInstance ? pInstance->cConfFilePath : _get_valid_module_conf_file (pModule));
	
	ModuleWidget *pModuleWidget = g_new0 (ModuleWidget, 1);
	pModuleWidget->widget.iType = WIDGET_MODULE;
	pModuleWidget->widget.apply = _module_widget_apply;
	pModuleWidget->widget.reset = _module_widget_reset;
	pModuleWidget->cConfFilePath = cConfFilePath;
	pModuleWidget->pModule = pModule;
	pModuleWidget->pModuleInstance = pModuleInstance;
	
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


void cairo_dock_module_widget_update_module_instance_container (ModuleWidget *pModuleWidget, CairoDockModuleInstance *pInstance, gboolean bDetached)
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

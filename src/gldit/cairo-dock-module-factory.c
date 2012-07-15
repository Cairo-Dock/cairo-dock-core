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

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <dlfcn.h>

#include <cairo.h>

#include "gldi-config.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_conf_file_needs_update
#include "cairo-dock-log.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-X-manager.h"  // g_desktopGeometry
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialog-manager.h"  // cairo_dock_show_temporary_dialog_with_icon
#include "cairo-dock-config.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-data-renderer.h"
#include "cairo-dock-module-factory.h"

// dependancies
extern gchar *g_cCurrentThemePath;
extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;
extern gboolean g_bEasterEggs;

static int s_iMaxOrder = 0;


  /////////////////////
 /// MODULE LOADER ///
/////////////////////

static gchar *_cairo_dock_extract_default_module_name_from_path (gchar *cSoFilePath)
{
	gchar *ptr = g_strrstr (cSoFilePath, "/");
	if (ptr == NULL)
		ptr = cSoFilePath;
	else
		ptr ++;
	if (strncmp (ptr, "lib", 3) == 0)
		ptr += 3;

	if (strncmp (ptr, "cd-", 3) == 0)
		ptr += 3;
	else if (strncmp (ptr, "cd_", 3) == 0)
		ptr += 3;

	gchar *cModuleName = g_strdup (ptr);

	ptr = g_strrstr (cModuleName, ".so");
	if (ptr != NULL)
		*ptr = '\0';

	//ptr = cModuleName;
	//while ((ptr = g_strrstr (ptr, "-")) != NULL)
	//	*ptr = '_';

	return cModuleName;
}

static void _cairo_dock_close_module (CairoDockModule *module)
{
	/**if (module->pModule)
		g_module_close (module->pModule);*/
	if (module->handle)
		dlclose (module->handle);
	
	g_free (module->pInterface);
	module->pInterface = NULL;
	
	cairo_dock_free_visit_card (module->pVisitCard);
	module->pVisitCard = NULL;
	
	g_free (module->cConfFilePath);
	module->cConfFilePath = NULL;
}

static void _cairo_dock_open_module (CairoDockModule *pCairoDockModule, GError **erreur)
{
	//\__________________ On ouvre le .so.
	/**GModule *module = g_module_open (pCairoDockModule->cSoFilePath, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
	if (!module)
	{
		g_set_error (erreur, 1, 1, "while opening module '%s' : (%s)", pCairoDockModule->cSoFilePath, g_module_error ());
		return ;
	}
	pCairoDockModule->pModule = module;*/
	pCairoDockModule->handle = dlopen (pCairoDockModule->cSoFilePath, RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
	if (! pCairoDockModule->handle)
	{
		g_set_error (erreur, 1, 1, "while opening module '%s' : (%s)", pCairoDockModule->cSoFilePath, dlerror ());
		return ;
	}
	//\__________________ On identifie le module.
	gboolean bSymbolFound;
	CairoDockModulePreInit function_pre_init = NULL;
	function_pre_init = dlsym (pCairoDockModule->handle, "pre_init");
	/**bSymbolFound = g_module_symbol (module, "pre_init", (gpointer) &function_pre_init);
	if (bSymbolFound && function_pre_init != NULL)*/
	if (function_pre_init != NULL)
	{
		pCairoDockModule->pVisitCard = g_new0 (CairoDockVisitCard, 1);
		pCairoDockModule->pInterface = g_new0 (CairoDockModuleInterface, 1);
		gboolean bModuleLoaded = function_pre_init (pCairoDockModule->pVisitCard, pCairoDockModule->pInterface);
		if (! bModuleLoaded)
		{
			_cairo_dock_close_module (pCairoDockModule);
			cd_debug ("module '%s' has not been loaded", pCairoDockModule->cSoFilePath);  // peut arriver a xxx-integration ou icon-effect par exemple.
			return ;
		}
	}
	else
	{
		pCairoDockModule->pVisitCard = NULL;
		g_set_error (erreur, 1, 1, "this module ('%s') does not have the common entry point 'pre_init', it may be broken or icompatible with cairo-dock", pCairoDockModule->cSoFilePath);
		return ;
	}
	
	//\__________________ On verifie sa compatibilite.
	CairoDockVisitCard *pVisitCard = pCairoDockModule->pVisitCard;
	if (! g_bEasterEggs &&
		(pVisitCard->iMajorVersionNeeded > g_iMajorVersion || (pVisitCard->iMajorVersionNeeded == g_iMajorVersion && pVisitCard->iMinorVersionNeeded > g_iMinorVersion) || (pVisitCard->iMajorVersionNeeded == g_iMajorVersion && pVisitCard->iMinorVersionNeeded == g_iMinorVersion && pVisitCard->iMicroVersionNeeded > g_iMicroVersion)))
	{
		g_set_error (erreur, 1, 1, "this module ('%s') needs at least Cairo-Dock v%d.%d.%d, but Cairo-Dock is in v%d.%d.%d (%s)\n  It will be ignored", pCairoDockModule->cSoFilePath, pVisitCard->iMajorVersionNeeded, pVisitCard->iMinorVersionNeeded, pVisitCard->iMicroVersionNeeded, g_iMajorVersion, g_iMinorVersion, g_iMicroVersion, GLDI_VERSION);
		cairo_dock_free_visit_card (pCairoDockModule->pVisitCard);
		pCairoDockModule->pVisitCard = NULL;
		return ;
	}
	if (! g_bEasterEggs &&
		pVisitCard->cDockVersionOnCompilation != NULL && strcmp (pVisitCard->cDockVersionOnCompilation, GLDI_VERSION) != 0)  // separation des versions en easter egg.
	{
		g_set_error (erreur, 1, 1, "this module ('%s') was compiled with Cairo-Dock v%s, but Cairo-Dock is in v%s\n  It will be ignored", pCairoDockModule->cSoFilePath, pVisitCard->cDockVersionOnCompilation, GLDI_VERSION);
		cairo_dock_free_visit_card (pCairoDockModule->pVisitCard);
		pCairoDockModule->pVisitCard = NULL;
		return ;
	}

	if (pVisitCard->cModuleName == NULL)
		pVisitCard->cModuleName = _cairo_dock_extract_default_module_name_from_path (pCairoDockModule->cSoFilePath);
}

CairoDockModule *cairo_dock_new_module (const gchar *cSoFilePath, GError **erreur)
{
	CairoDockModule *pCairoDockModule = g_new0 (CairoDockModule, 1);
	
	if (cSoFilePath != NULL)
	{
		pCairoDockModule->cSoFilePath = g_strdup (cSoFilePath);
		GError *tmp_erreur = NULL;
		_cairo_dock_open_module (pCairoDockModule, &tmp_erreur);
		if (tmp_erreur != NULL)
		{
			g_propagate_error (erreur, tmp_erreur);
			g_free (pCairoDockModule->cSoFilePath);
			g_free (pCairoDockModule);
			return NULL;
		}
		if (pCairoDockModule->pVisitCard == NULL)
		{
			g_free (pCairoDockModule);
			return NULL;
		}
	}
	return pCairoDockModule;
}

void cairo_dock_free_module (CairoDockModule *module)
{
	if (module == NULL)
		return ;
	cd_debug ("%s (%s)", __func__, module->pVisitCard->cModuleName);

	cairo_dock_deactivate_module (module);

	_cairo_dock_close_module (module);

	g_free (module->cSoFilePath);
	g_free (module);
}

gchar *cairo_dock_check_module_conf_dir (CairoDockModule *pModule)
{
	CairoDockVisitCard *pVisitCard = pModule->pVisitCard;
	if (pVisitCard->cConfFileName == NULL)
		return NULL;
	
	gchar *cUserDataDirPath = g_strdup_printf ("%s/plug-ins/%s", g_cCurrentThemePath, pVisitCard->cUserDataDir);
	if (! g_file_test (cUserDataDirPath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
	{
		cd_message ("directory %s doesn't exist, it will be added.", cUserDataDirPath);
		
		gchar *command = g_strdup_printf ("mkdir -p \"%s\"", cUserDataDirPath);
		int r = system (command);
		g_free (command);
		
		if (r != 0)
		{
			cd_warning ("couldn't create a directory for applet '%s' in '%s/plug-ins'\n check writing permissions", pVisitCard->cModuleName, g_cCurrentThemePath);
			g_free (cUserDataDirPath);
			g_free (pModule->cConfFilePath);
			pModule->cConfFilePath = NULL;
			return NULL;
		}
	}
	
	if (pModule->cConfFilePath == NULL)
		pModule->cConfFilePath = g_strdup_printf ("%s/%s", pVisitCard->cShareDataDir, pVisitCard->cConfFileName);
	
	return cUserDataDirPath;
}

void cairo_dock_free_visit_card (CairoDockVisitCard *pVisitCard)
{
	g_free (pVisitCard);  // toutes les chaines sont statiques.
}


  ///////////////////////
 /// MODULE INSTANCE ///
///////////////////////

static void _cairo_dock_read_module_config (GKeyFile *pKeyFile, CairoDockModuleInstance *pInstance)
{
	CairoDockModuleInterface *pInterface = pInstance->pModule->pInterface;
	CairoDockVisitCard *pVisitCard = pInstance->pModule->pVisitCard;
	
	gboolean bFlushConfFileNeeded = FALSE;
	if (pInterface->read_conf_file != NULL)
	{
		if (pInterface->reset_config != NULL)
			pInterface->reset_config (pInstance);
		if (pVisitCard->iSizeOfConfig != 0)
			memset (((gpointer)pInstance)+sizeof(CairoDockModuleInstance), 0, pVisitCard->iSizeOfConfig);
		
		bFlushConfFileNeeded = pInterface->read_conf_file (pInstance, pKeyFile);
	}
	if (! bFlushConfFileNeeded)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, pVisitCard->cModuleVersion);
	if (bFlushConfFileNeeded)
	{
		///cairo_dock_flush_conf_file (pKeyFile, pInstance->cConfFilePath, pVisitCard->cShareDataDir, pVisitCard->cConfFileName);
		gchar *cTemplate = g_strdup_printf ("%s/%s", pVisitCard->cShareDataDir, pVisitCard->cConfFileName);
		cairo_dock_upgrade_conf_file_full (pInstance->cConfFilePath, pKeyFile, cTemplate, FALSE);  // keep private keys.
		g_free (cTemplate);
	}
}

GKeyFile *cairo_dock_pre_read_module_instance_config (CairoDockModuleInstance *pInstance, CairoDockMinimalAppletConfig *pMinimalConfig)
{
	g_return_val_if_fail (pInstance != NULL, NULL);
	//\____________________ on ouvre son fichier de conf.
	if (pInstance->cConfFilePath == NULL)  // aucun fichier de conf (xxx-integration par exemple).
		return NULL;
	gchar *cInstanceConfFilePath = pInstance->cConfFilePath;
	CairoDockModule *pModule = pInstance->pModule;
	
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cInstanceConfFilePath);
	if (pKeyFile == NULL)  // fichier illisible.
		return NULL;
	
	if (pInstance->pModule->pVisitCard->iContainerType == CAIRO_DOCK_MODULE_IS_PLUGIN)  // ce module n'a pas d'icone (ce n'est pas une applet).
	{
		return pKeyFile;
	}
	
	//\____________________ on recupere les parametres de l'icone.
	if (pInstance->pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DOCK)  // l'applet peut aller dans un dock.
	{
		gboolean bUnused;
		cairo_dock_get_size_key_value_helper (pKeyFile, "Icon", "icon ", bUnused, pMinimalConfig->iDesiredIconWidth, pMinimalConfig->iDesiredIconHeight);  // for a dock, if 0, will just get the default size; for a desklet, unused.
		/**if (pMinimalConfig->iDesiredIconWidth == 0)
			pMinimalConfig->iDesiredIconWidth = myIconsParam.iIconWidth;
		if (pMinimalConfig->iDesiredIconWidth == 0)
			pMinimalConfig->iDesiredIconWidth = 48;
		if (pMinimalConfig->iDesiredIconHeight == 0)
			pMinimalConfig->iDesiredIconHeight = myIconsParam.iIconHeight;
		if (pMinimalConfig->iDesiredIconHeight == 0)
			pMinimalConfig->iDesiredIconHeight = 48;*/
		
		pMinimalConfig->cLabel = cairo_dock_get_string_key_value (pKeyFile, "Icon", "name", NULL, NULL, NULL, NULL);
		if (pMinimalConfig->cLabel == NULL && !pInstance->pModule->pVisitCard->bAllowEmptyTitle)
		{
			pMinimalConfig->cLabel = g_strdup (pInstance->pModule->pVisitCard->cTitle);
		}
		else if (pMinimalConfig->cLabel && strcmp (pMinimalConfig->cLabel, "none") == 0)
		{
			g_free (pMinimalConfig->cLabel);
			pMinimalConfig->cLabel = NULL;
		}
		
		pMinimalConfig->cIconFileName = cairo_dock_get_string_key_value (pKeyFile, "Icon", "icon", NULL, NULL, NULL, NULL);
		pMinimalConfig->fOrder = cairo_dock_get_double_key_value (pKeyFile, "Icon", "order", NULL, CAIRO_DOCK_LAST_ORDER, NULL, NULL);
		if (pMinimalConfig->fOrder == CAIRO_DOCK_LAST_ORDER)
		{
			pMinimalConfig->fOrder = ++ s_iMaxOrder;
			g_key_file_set_double (pKeyFile, "Icon", "order", pMinimalConfig->fOrder);
			cd_debug ("set order to %.1f\n", pMinimalConfig->fOrder);
			cairo_dock_write_keys_to_file (pKeyFile, cInstanceConfFilePath);
		}
		else
		{
			s_iMaxOrder = MAX (s_iMaxOrder, pMinimalConfig->fOrder);
		}
		pMinimalConfig->cDockName = cairo_dock_get_string_key_value (pKeyFile, "Icon", "dock name", NULL, NULL, NULL, NULL);
		int iBgColorType;
		if (g_key_file_has_key (pKeyFile, "Icon", "always_visi", NULL))
		{
			iBgColorType = g_key_file_get_integer (pKeyFile, "Icon", "always_visi", NULL);
		}
		else  // old param
		{
			iBgColorType = (g_key_file_get_boolean (pKeyFile, "Icon", "always visi", NULL) ? 2 : 0);  // keep the existing custom color
			g_key_file_set_integer (pKeyFile, "Icon", "always_visi", iBgColorType);
		}
		pMinimalConfig->bAlwaysVisible = (iBgColorType != 0);
		pMinimalConfig->pHiddenBgColor = NULL;
		if (iBgColorType == 2)  // custom bg color
		{
			gsize length;
			pMinimalConfig->pHiddenBgColor = g_key_file_get_double_list (pKeyFile, "Icon", "bg color", &length, NULL);
			if (length < 4)  // invalid rgba color => use the default one.
			{
				g_free (pMinimalConfig->pHiddenBgColor);
				pMinimalConfig->pHiddenBgColor = NULL;
			}
		}
	}
	
	//\____________________ on recupere les parametres de son desklet.
	if (pInstance->pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DESKLET)  // l'applet peut aller dans un desklet.
	{
		CairoDeskletAttribute *pDeskletAttribute = &pMinimalConfig->deskletAttribute;
		if (pInstance->pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DOCK)
			pMinimalConfig->bIsDetached = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "initially detached", NULL, FALSE, NULL, NULL);
		else
			pMinimalConfig->bIsDetached = TRUE;
		
		pDeskletAttribute->bDeskletUseSize = ! pInstance->pModule->pVisitCard->bStaticDeskletSize;
		
		gboolean bUseless;
		cairo_dock_get_size_key_value_helper (pKeyFile, "Desklet", "", bUseless, pDeskletAttribute->iDeskletWidth, pDeskletAttribute->iDeskletHeight);
		//g_print ("desklet : %dx%d\n", pDeskletAttribute->iDeskletWidth, pDeskletAttribute->iDeskletHeight);
		if (pDeskletAttribute->iDeskletWidth == 0)
			pDeskletAttribute->iDeskletWidth = 96;
		if (pDeskletAttribute->iDeskletHeight == 0)
			pDeskletAttribute->iDeskletHeight = 96;
		
		pDeskletAttribute->iDeskletPositionX = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "x position", NULL, 0, NULL, NULL);
		pDeskletAttribute->iDeskletPositionY = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "y position", NULL, 0, NULL, NULL);
		pDeskletAttribute->iVisibility = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "accessibility", NULL, CAIRO_DESKLET_NORMAL, NULL, NULL);
		pDeskletAttribute->bOnAllDesktops = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "sticky", NULL, TRUE, NULL, NULL);
		pDeskletAttribute->iNumDesktop = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "num desktop", NULL, -1, NULL, NULL);
		pDeskletAttribute->bPositionLocked = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "locked", NULL, FALSE, NULL, NULL);
		pDeskletAttribute->bNoInput = cairo_dock_get_boolean_key_value (pKeyFile, "Desklet", "no input", NULL, FALSE, NULL, NULL);
		pDeskletAttribute->iRotation = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "rotation", NULL, 0, NULL, NULL);
		pDeskletAttribute->iDepthRotationY = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "depth rotation y", NULL, 0, NULL, NULL);
		pDeskletAttribute->iDepthRotationX = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "depth rotation x", NULL, 0, NULL, NULL);
		
		// on recupere les decorations du desklet.
		gchar *cDecorationTheme = cairo_dock_get_string_key_value (pKeyFile, "Desklet", "decorations", NULL, NULL, NULL, NULL);
		if (cDecorationTheme == NULL || strcmp (cDecorationTheme, "personnal") == 0)
		{
			//g_print ("on recupere les decorations personnelles au desklet\n");
			CairoDeskletDecoration *pUserDeskletDecorations = g_new0 (CairoDeskletDecoration, 1);
			pDeskletAttribute->pUserDecoration = pUserDeskletDecorations;
			
			pUserDeskletDecorations->cBackGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklet", "bg desklet", NULL, NULL, NULL, NULL);
			pUserDeskletDecorations->cForeGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklet", "fg desklet", NULL, NULL, NULL, NULL);
			pUserDeskletDecorations->iLoadingModifier = CAIRO_DOCK_FILL_SPACE;
			pUserDeskletDecorations->fBackGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "bg alpha", NULL, 1.0, NULL, NULL);
			pUserDeskletDecorations->fForeGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklet", "fg alpha", NULL, 1.0, NULL, NULL);
			pUserDeskletDecorations->iLeftMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "left offset", NULL, 0, NULL, NULL);
			pUserDeskletDecorations->iTopMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "top offset", NULL, 0, NULL, NULL);
			pUserDeskletDecorations->iRightMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "right offset", NULL, 0, NULL, NULL);
			pUserDeskletDecorations->iBottomMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "bottom offset", NULL, 0, NULL, NULL);
			g_free (cDecorationTheme);
		}
		else
		{
			//g_print ("decorations : %s\n", cDecorationTheme);
			pDeskletAttribute->cDecorationTheme = cDecorationTheme;
		}
	}
	
	return pKeyFile;
}

void cairo_dock_free_minimal_config (CairoDockMinimalAppletConfig *pMinimalConfig)
{
	if (pMinimalConfig == NULL)
		return;
	g_free (pMinimalConfig->cLabel);
	g_free (pMinimalConfig->cIconFileName);
	g_free (pMinimalConfig->cDockName);
	g_free (pMinimalConfig->deskletAttribute.cDecorationTheme);
	cairo_dock_free_desklet_decoration (pMinimalConfig->deskletAttribute.pUserDecoration);
	g_free (pMinimalConfig->pHiddenBgColor);
	g_free (pMinimalConfig);
}


CairoDockModuleInstance *cairo_dock_instanciate_module (CairoDockModule *pModule, gchar *cConfFilePath)  // prend possession de 'cConfFilePah'.
{
	g_return_val_if_fail (pModule != NULL, NULL);
	cd_message ("%s (%s)", __func__, cConfFilePath);
	
	//\____________________ On cree une instance du module.
	CairoDockModuleInstance *pInstance = g_malloc0 (sizeof (CairoDockModuleInstance) + pModule->pVisitCard->iSizeOfConfig + pModule->pVisitCard->iSizeOfData);  // we allocate everything at once, since config and data will anyway live as long as the instance itself.
	pInstance->pModule = pModule;
	pInstance->cConfFilePath = cConfFilePath;
	if (pModule->pVisitCard->iSizeOfConfig > 0)
		pInstance->pConfig = ( ((gpointer)pInstance) + sizeof(CairoDockModuleInstance) );
	if (pModule->pVisitCard->iSizeOfData > 0)
		pInstance->pData = ( ((gpointer)pInstance) + sizeof(CairoDockModuleInstance) + pModule->pVisitCard->iSizeOfConfig);
	
	CairoDockMinimalAppletConfig *pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
	GKeyFile *pKeyFile = cairo_dock_pre_read_module_instance_config (pInstance, pMinimalConfig);
	if (cConfFilePath != NULL && pKeyFile == NULL)  // we have a conf file, but it was unreadable -> cancel
	{
		cd_warning ("unreadable config file (%s) for applet %s", cConfFilePath, pModule->pVisitCard->cModuleName);
		g_free (pMinimalConfig);
		free (pInstance);
		return NULL;
	}
	pModule->pInstancesList = g_list_prepend (pModule->pInstancesList, pInstance);
	
	//\____________________ On cree le container de l'instance, ainsi que son icone.
	CairoContainer *pContainer = NULL;
	CairoDock *pDock = NULL;
	CairoDesklet *pDesklet = NULL;
	Icon *pIcon = NULL;
	
	if (pInstance->pModule->pVisitCard->iContainerType != CAIRO_DOCK_MODULE_IS_PLUGIN)  // le module a une icone (c'est une applet).
	{
		pInstance->bCanDetach = pMinimalConfig->deskletAttribute.iDeskletWidth > 0;
		pModule->bCanDetach = pInstance->bCanDetach;  // pas encore clair ...
		
		// create the icon.
		pIcon = cairo_dock_create_icon_for_applet (pMinimalConfig,
			pInstance);
		
		// create/find its container and insert the icon inside.
		if (pModule->bCanDetach && pMinimalConfig->bIsDetached)
		{
			pDesklet = cairo_dock_create_desklet (pIcon, &pMinimalConfig->deskletAttribute);
			pContainer = CAIRO_CONTAINER (pDesklet);
		}
		else
		{
			const gchar *cDockName = (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
			pDock = cairo_dock_search_dock_from_name (cDockName);
			if (pDock == NULL)
			{
				pDock = cairo_dock_create_dock (cDockName);
			}
			pContainer = CAIRO_CONTAINER (pDock);
			
			if (pDock)
			{
				cairo_dock_insert_icon_in_dock (pIcon, pDock, ! cairo_dock_is_loading ());  // animate the icon if it's instanciated by the user, not during the initial loading.
				
				// we need to load the icon's buffer before we init the module, because the applet may need it. no ned to do it in desklet mode, since the desklet doesn't have a renderer yet (so buffer can't be loaded).
				cairo_dock_load_icon_buffers (pIcon, pContainer);  // ne cree rien si w ou h < 0 (par exemple si l'applet est detachee).
			}
		}

		pInstance->pIcon = pIcon;
		pInstance->pDock = pDock;
		pInstance->pDesklet = pDesklet;
		pInstance->pContainer = pContainer;
	}
	cairo_dock_free_minimal_config (pMinimalConfig);
	
	//\____________________ initialise the instance.
	if (pKeyFile)
		_cairo_dock_read_module_config (pKeyFile, pInstance);
	
	if (pModule->pInterface->initModule)
		pModule->pInterface->initModule (pInstance, pKeyFile);
	
	if (pDesklet && pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0)  // peut arriver si le desklet a fini de se redimensionner avant l'init.
		gtk_widget_queue_draw (pDesklet->container.pWidget);
	
	if (pKeyFile != NULL)
		g_key_file_free (pKeyFile);
	return pInstance;
}

/* Detruit une instance de module et libere les resources associees.
*/
static void _cairo_dock_free_module_instance (CairoDockModuleInstance *pInstance)
{
	g_free (pInstance->cConfFilePath);
	/*g_free (pInstance->pConfig);
	g_free (pInstance->pData);*/
	g_free (pInstance);
}

/* Stoppe une instance d'un module en vue de la detruire.
*/
static void _cairo_dock_stop_module_instance (CairoDockModuleInstance *pInstance)
{
	if (pInstance->pModule->pInterface->stopModule != NULL)
		pInstance->pModule->pInterface->stopModule (pInstance);
	
	if (pInstance->pModule->pInterface->reset_data != NULL)
		pInstance->pModule->pInterface->reset_data (pInstance);
	
	if (pInstance->pModule->pInterface->reset_config != NULL)
		pInstance->pModule->pInterface->reset_config (pInstance);
	
	cairo_dock_release_data_slot (pInstance);
	
	if (pInstance->pDesklet)
		cairo_dock_destroy_desklet (pInstance->pDesklet);
	if (pInstance->pDrawContext != NULL)
		cairo_destroy (pInstance->pDrawContext);
	
	if (pInstance->pIcon != NULL)
	{
		if (pInstance->pIcon->pSubDock != NULL)
		{
			cairo_dock_destroy_dock (pInstance->pIcon->pSubDock, pInstance->pIcon->cName);
			pInstance->pIcon->pSubDock = NULL;
		}
		pInstance->pIcon->pModuleInstance = NULL;
	}
}

void cairo_dock_deinstanciate_module (CairoDockModuleInstance *pInstance)  // stop an instance of a module
{
	_cairo_dock_stop_module_instance (pInstance);
	
	pInstance->pModule->pInstancesList = g_list_remove (pInstance->pModule->pInstancesList, pInstance);
	if (pInstance->pModule->pInstancesList == NULL)
		cairo_dock_notify_on_object (&myModulesMgr, NOTIFICATION_MODULE_ACTIVATED, pInstance->pModule->pVisitCard->cModuleName, FALSE);
	
	_cairo_dock_free_module_instance (pInstance);
}

void cairo_dock_reload_module_instance (CairoDockModuleInstance *pInstance, gboolean bReloadAppletConf)
{
	g_return_if_fail (pInstance != NULL);
	CairoDockModule *module = pInstance->pModule;
	cd_message ("%s (%s, %d)", __func__, module->pVisitCard->cModuleName, bReloadAppletConf);
	
	GError *erreur = NULL;
	CairoContainer *pCurrentContainer = pInstance->pContainer;
	pInstance->pContainer = NULL;
	CairoDock *pCurrentDock = pInstance->pDock;
	pInstance->pDock = NULL;
	CairoDesklet *pCurrentDesklet = pInstance->pDesklet;
	pInstance->pDesklet = NULL;
	gchar *cCurrentSubDockName = NULL;
	
	CairoContainer *pNewContainer = NULL;
	CairoDock *pNewDock = NULL;
	CairoDesklet *pNewDesklet = NULL;
	
	//\______________ On recharge la config minimale.
	Icon *pIcon = pInstance->pIcon;
	GKeyFile *pKeyFile = NULL;
	CairoDockMinimalAppletConfig *pMinimalConfig = NULL;
	if (bReloadAppletConf && pInstance->cConfFilePath != NULL)
	{
		pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
		pKeyFile = cairo_dock_pre_read_module_instance_config (pInstance, pMinimalConfig);
		
		if (pInstance->pModule->pVisitCard->iContainerType != CAIRO_DOCK_MODULE_IS_PLUGIN)  // c'est une applet.
		{
			//\______________ On met a jour les champs 'nom' et 'image' de l'icone.
			if (pIcon != NULL)
			{
				if (pCurrentDock && ! pIcon->pContainer)  // icon already detached (by drag and drop)
					pCurrentDock = NULL;
				cCurrentSubDockName = g_strdup (pIcon->cName);
				
				// on gere le changement de nom de son sous-dock.
				if (pIcon->cName != NULL && pIcon->pSubDock != NULL && cairo_dock_strings_differ (pIcon->cName, pMinimalConfig->cLabel))
				{
					gchar *cNewName = cairo_dock_get_unique_dock_name (pMinimalConfig->cLabel);
					cd_debug ("* le sous-dock %s prend le nom '%s'", pIcon->cName, cNewName);
					if (strcmp (pIcon->cName, cNewName) != 0)
						cairo_dock_rename_dock (pIcon->cName, NULL, cNewName);
					g_free (pMinimalConfig->cLabel);
					pMinimalConfig->cLabel = cNewName;
				}
				
				g_free (pIcon->cName);
				pIcon->cName = pMinimalConfig->cLabel;
				pMinimalConfig->cLabel = NULL;  // we won't need it any more, so skip a duplication.
				g_free (pIcon->cFileName);
				pIcon->cFileName = pMinimalConfig->cIconFileName;
				pMinimalConfig->cIconFileName = NULL;  // idem
				pIcon->bAlwaysVisible = pMinimalConfig->bAlwaysVisible;
				pIcon->bHasHiddenBg = pMinimalConfig->bAlwaysVisible;  // if were going to see the applet all the time, let's add a background. if the user doesn't want it, he can always set a transparent bg color.
				pIcon->pHiddenBgColor = pMinimalConfig->pHiddenBgColor;
				pMinimalConfig->pHiddenBgColor = NULL;
			}
			
			// on recupere son dock (cree au besoin).
			if (!pMinimalConfig->bIsDetached)  // elle est desormais dans un dock.
			{
				const gchar *cDockName = (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
				pNewDock = cairo_dock_search_dock_from_name (cDockName);
				if (pNewDock == NULL)  // c'est un nouveau dock.
				{
					cairo_dock_add_root_dock_config_for_name (cDockName);
					pNewDock = cairo_dock_create_dock (cDockName);
				}
				pNewContainer = CAIRO_CONTAINER (pNewDock);
			}
			
			// detach the icon from its container if it has changed
			if (pCurrentDock != NULL && (pMinimalConfig->bIsDetached || pNewDock != pCurrentDock))  // was in a dock, now is in another dock or in a desklet
			{
				cd_message ("le container a change (%s -> %s)", pIcon->cParentDockName, pMinimalConfig->bIsDetached ? "desklet" : pMinimalConfig->cDockName);
				cairo_dock_detach_icon_from_dock (pIcon, pCurrentDock);
			}
			else if (pCurrentDesklet != NULL && ! pMinimalConfig->bIsDetached)  // was in a desklet, now is in a dock
			{
				cairo_dock_destroy_desklet (pCurrentDesklet);
				pCurrentDesklet = NULL;
			}
			
			// on recupere son desklet (cree au besoin).
			if (pMinimalConfig->bIsDetached)
			{
				if (pCurrentDesklet == NULL)  // c'est un nouveau desklet.
				{
					pNewDesklet = cairo_dock_create_desklet (pIcon, &pMinimalConfig->deskletAttribute);
				}
				else  // on reconfigure le desklet courant.
				{
					pNewDesklet = pCurrentDesklet;
					cairo_dock_configure_desklet (pNewDesklet, &pMinimalConfig->deskletAttribute);
				}
				pNewContainer = CAIRO_CONTAINER (pNewDesklet);
			}
		}
	}
	else
	{
		pNewContainer = pCurrentContainer;
		pNewDock = pCurrentDock;
		pNewDesklet = pCurrentDesklet;
	}
	pInstance->pContainer = pNewContainer;
	pInstance->pDock = pNewDock;
	pInstance->pDesklet = pNewDesklet;
	
	//\_______________________ On insere l'icone dans son nouveau dock, et on s'assure que sa taille respecte les tailles par defaut.
	if (pNewDock != NULL && pIcon != NULL)  // l'icone est desormais dans un dock.
	{
		// on recupere la taille voulue.
		if (pMinimalConfig == NULL)  // on recupere sa taille, car elle peut avoir change (si c'est la taille par defaut, ou si elle est devenue trop grande).
		{
			pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
			pKeyFile = cairo_dock_pre_read_module_instance_config (pInstance, pMinimalConfig);
			g_key_file_free (pKeyFile);
			pKeyFile = NULL;
		}
		pIcon->fWidth = pMinimalConfig->iDesiredIconWidth;  // requested size
		pIcon->fHeight = pMinimalConfig->iDesiredIconHeight;
		pIcon->iImageWidth = 0;
		pIcon->iImageHeight = 0;
		
		// on insere l'icone dans le dock ou on met a jour celui-ci.
		if (pNewDock != pCurrentDock)  // insert in its new dock.
		{
			cairo_dock_insert_icon_in_dock (pIcon, pNewDock, CAIRO_DOCK_ANIMATE_ICON);
			pIcon->cParentDockName = g_strdup (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
			cairo_dock_load_icon_buffers (pIcon, pNewContainer);  // do it now, since the applet may need it. no ned to do it in desklet mode, since the desklet doesn't have a renderer yet (so buffer can't be loaded).
		}
		else  // same dock, just update its size.
		{
			cairo_dock_resize_icon_in_dock (pIcon, pNewDock);
			if (bReloadAppletConf)
				cairo_dock_load_icon_text (pIcon);
		}
	}
	
	//\_______________________ On recharge la config.
	gboolean bCanReload = TRUE;
	if (pKeyFile != NULL)
	{
		_cairo_dock_read_module_config (pKeyFile, pInstance);
	}
	
	//\_______________________ On recharge l'instance.
	if (bCanReload && module->pInterface->reloadModule != NULL)
		module->pInterface->reloadModule (pInstance, pCurrentContainer, pKeyFile);
	
	if (pNewDock != NULL && pNewDock->iRefCount != 0)  // on redessine l'icone pointant sur le sous-dock contenant l'applet, au cas ou son image aurait change.
	{
		cairo_dock_redraw_subdock_content (pNewDock);
	}
	
	//\_______________________ On nettoie derriere nous.
	cairo_dock_free_minimal_config (pMinimalConfig);
	if (pKeyFile != NULL)
		g_key_file_free (pKeyFile);
	
	if (pCurrentDesklet != NULL && pCurrentDesklet != pNewDesklet)
		cairo_dock_destroy_desklet (pCurrentDesklet);
	if (pCurrentDock != NULL && pCurrentDock != pNewDock)
	{
		if (pCurrentDock->iRefCount == 0 && pCurrentDock->icons == NULL && !pCurrentDock->bIsMainDock)  // dock principal vide.
		{
			pCurrentDock = NULL;  // se fera detruire automatiquement.
		}
		else
		{
			cairo_dock_update_dock_size (pCurrentDock);
			gtk_widget_queue_draw (pCurrentContainer->pWidget);
		}
	}
	if (pNewDesklet != NULL && pIcon && pIcon->pSubDock != NULL)
	{
		cairo_dock_destroy_dock (pIcon->pSubDock, cCurrentSubDockName);
		pIcon->pSubDock = NULL;
	}
	g_free (cCurrentSubDockName);
	
	if (! bReloadAppletConf && cairo_dock_get_icon_data_renderer (pIcon) != NULL)  // reload the data-renderer at the new size
		cairo_dock_reload_data_renderer_on_icon (pIcon, pNewContainer);
}


  ///////////////
 /// MODULES ///
///////////////

void cairo_dock_activate_module (CairoDockModule *module, GError **erreur)
{
	g_return_if_fail (module != NULL && module->pVisitCard != NULL);
	cd_debug ("%s (%s)", __func__, module->pVisitCard->cModuleName);
	
	if (module->pInstancesList != NULL)
	{
		cd_warning ("module %s already activated", module->pVisitCard->cModuleName);
		g_set_error (erreur, 1, 1, "%s () : module %s is already active !", __func__, module->pVisitCard->cModuleName);
		return ;
	}
	
	if (module->pVisitCard->cConfFileName != NULL)  // the module has a conf file -> create an instance for each of them.
	{
		// check that the module's config dir exists or create it.
		gchar *cUserDataDirPath = cairo_dock_check_module_conf_dir (module);
		if (cUserDataDirPath == NULL)
		{
			g_set_error (erreur, 1, 1, "No instance of module %s could be created", __func__, module->pVisitCard->cModuleName);
			return;
		}
		
		// look for conf files inside this folder, and create an instance for each of them.
		int n = 0;
		if (module->pVisitCard->bMultiInstance)  // possibly several conf files.
		{
			// open it
			GError *tmp_erreur = NULL;
			GDir *dir = g_dir_open (cUserDataDirPath, 0, &tmp_erreur);
			if (tmp_erreur != NULL)
			{
				g_free (cUserDataDirPath);
				g_propagate_error (erreur, tmp_erreur);
				return ;
			}
			
			// for each conf file inside, instanciate the module with it.
			const gchar *cFileName;
			gchar *cInstanceFilePath;
			
			while ((cFileName = g_dir_read_name (dir)) != NULL)
			{
				gchar *str = strstr (cFileName, ".conf");
				if (!str)
					continue;
				if (*(str+5) != '-' && *(str+5) != '\0')  // xxx.conf or xxx.conf-i
					continue;
				cInstanceFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, cFileName);
				cairo_dock_instanciate_module (module, cInstanceFilePath);  // prend possession de 'cInstanceFilePath'.
				n ++;
			}
			g_dir_close (dir);
		}
		else  // only 1 conf file possible.
		{
			gchar *cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, module->pVisitCard->cConfFileName);
			if (g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
			{
				cairo_dock_instanciate_module (module, cConfFilePath);
				n = 1;
			}
			else
			{
				g_free (cConfFilePath);
			}
		}
		
		// if no conf file was present, copy the default one and instanciate the module with it.
		if (n == 0)  // no conf file was present.
		{
			gchar *cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, module->pVisitCard->cConfFileName);
			gboolean r = cairo_dock_copy_file (module->cConfFilePath, cConfFilePath);
			if (! r)  // the copy failed.
			{
				g_set_error (erreur, 1, 1, "couldn't copy %s into %s; check permissions and file's existence", module->cConfFilePath, cUserDataDirPath);
				g_free (cConfFilePath);
				g_free (cUserDataDirPath);
				return;
			}
			else
			{
				cairo_dock_instanciate_module (module, cConfFilePath);
			}
		}
		
		g_free (cUserDataDirPath);
	}
	else  // the module has no conf file, just instanciate it once.
	{
		cairo_dock_instanciate_module (module, NULL);
	}
	
	cairo_dock_notify_on_object (&myModulesMgr, NOTIFICATION_MODULE_ACTIVATED, module->pVisitCard->cModuleName, TRUE);
}

void cairo_dock_deactivate_module (CairoDockModule *module)  // stop all instances of a module
{
	g_return_if_fail (module != NULL);
	cd_debug ("%s (%s, %s)", __func__, module->pVisitCard->cModuleName, module->cConfFilePath);
	g_list_foreach (module->pInstancesList, (GFunc) _cairo_dock_stop_module_instance, NULL);
	g_list_foreach (module->pInstancesList, (GFunc) _cairo_dock_free_module_instance, NULL);
	g_list_free (module->pInstancesList);
	module->pInstancesList = NULL;
	cairo_dock_notify_on_object (&myModulesMgr, NOTIFICATION_MODULE_ACTIVATED, module->pVisitCard->cModuleName, FALSE);
}

void cairo_dock_reload_module (CairoDockModule *pModule, gboolean bReloadAppletConf)
{
	GList *pElement;
	CairoDockModuleInstance *pInstance;
	for (pElement = pModule->pInstancesList; pElement != NULL; pElement = pElement->next)
	{
		pInstance = pElement->data;
		cairo_dock_reload_module_instance (pInstance, bReloadAppletConf);
	}
}


void cairo_dock_popup_module_instance_description (CairoDockModuleInstance *pModuleInstance)
{
	gchar *cDescription = g_strdup_printf ("%s (v%s) by %s\n%s",
		pModuleInstance->pModule->pVisitCard->cModuleName,
		pModuleInstance->pModule->pVisitCard->cModuleVersion,
		pModuleInstance->pModule->pVisitCard->cAuthor,
		dgettext (pModuleInstance->pModule->pVisitCard->cGettextDomain,
		pModuleInstance->pModule->pVisitCard->cDescription));
	
	myDialogsParam.dialogTextDescription.bUseMarkup = TRUE;
	cairo_dock_show_temporary_dialog_with_icon (cDescription, pModuleInstance->pIcon, pModuleInstance->pContainer, 0, pModuleInstance->pModule->pVisitCard->cIconFilePath);
	g_free (cDescription);
	myDialogsParam.dialogTextDescription.bUseMarkup = FALSE;
}

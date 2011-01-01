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

#include <cairo.h>

#include "gldi-config.h"
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_conf_file_needs_update
#include "cairo-dock-log.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-X-manager.h"  // g_desktopGeometry
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialog-manager.h"  // cairo_dock_show_temporary_dialog_with_icon
#include "cairo-dock-config.h"
///#include "cairo-dock-gui-manager.h"  // cairo_dock_trigger_refresh_launcher_gui
#include "cairo-dock-module-manager.h"
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
	if (module->pModule)
		g_module_close (module->pModule);
	
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
	GModule *module = g_module_open (pCairoDockModule->cSoFilePath, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
	if (!module)
	{
		g_set_error (erreur, 1, 1, "while opening module '%s' : (%s)", pCairoDockModule->cSoFilePath, g_module_error ());
		return ;
	}
	pCairoDockModule->pModule = module;

	//\__________________ On identifie le module.
	gboolean bSymbolFound;
	CairoDockModulePreInit function_pre_init = NULL;
	bSymbolFound = g_module_symbol (module, "pre_init", (gpointer) &function_pre_init);
	if (bSymbolFound && function_pre_init != NULL)
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

gchar *cairo_dock_check_module_conf_file (CairoDockVisitCard *pVisitCard)
{
	if (pVisitCard->cConfFileName == NULL)
		return NULL;
	
	int r;
	gchar *cUserDataDirPath = g_strdup_printf ("%s/plug-ins/%s", g_cCurrentThemePath, pVisitCard->cUserDataDir);
	if (! g_file_test (cUserDataDirPath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
	{
		cd_message ("directory %s doesn't exist, it will be added.", cUserDataDirPath);
		
		gchar *command = g_strdup_printf ("mkdir -p \"%s\"", cUserDataDirPath);
		r = system (command);
		g_free (command);
	}
	
	gchar *cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, pVisitCard->cConfFileName);
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
	{
		cd_message ("no conf file %s, we will take the default one", cConfFilePath);
		gchar *command = g_strdup_printf ("cp \"%s/%s\" \"%s\"", pVisitCard->cShareDataDir, pVisitCard->cConfFileName, cConfFilePath);
		r = system (command);
		g_free (command);
	}
	
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))  // la copie ne s'est pas bien passee.
	{
		cd_warning ("couldn't copy %s/%s in %s; check permissions and file's existence", pVisitCard->cShareDataDir, pVisitCard->cConfFileName, cUserDataDirPath);
		g_free (cUserDataDirPath);
		g_free (cConfFilePath);
		return NULL;
	}
	
	g_free (cUserDataDirPath);
	return cConfFilePath;
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
		
		bFlushConfFileNeeded = g_key_file_has_group (pKeyFile, "Desklet") && ! g_key_file_has_key (pKeyFile, "Desklet", "accessibility", NULL);  // petit hack des familles ^_^
		bFlushConfFileNeeded |= pInterface->read_conf_file (pInstance, pKeyFile);
	}
	if (! bFlushConfFileNeeded)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, pVisitCard->cModuleVersion);
	if (bFlushConfFileNeeded)
		cairo_dock_flush_conf_file (pKeyFile, pInstance->cConfFilePath, pVisitCard->cShareDataDir, pVisitCard->cConfFileName);
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
		gboolean bUseless;
		cairo_dock_get_size_key_value_helper (pKeyFile, "Icon", "icon ", bUseless, pMinimalConfig->iDesiredIconWidth, pMinimalConfig->iDesiredIconHeight);
		if (pMinimalConfig->iDesiredIconWidth == 0)
			pMinimalConfig->iDesiredIconWidth = myIconsParam.tIconAuthorizedWidth[CAIRO_DOCK_APPLET];
		if (pMinimalConfig->iDesiredIconWidth == 0)
			pMinimalConfig->iDesiredIconWidth = 48;
		if (pMinimalConfig->iDesiredIconHeight == 0)
			pMinimalConfig->iDesiredIconHeight = myIconsParam.tIconAuthorizedHeight[CAIRO_DOCK_APPLET];
		if (pMinimalConfig->iDesiredIconHeight == 0)
			pMinimalConfig->iDesiredIconHeight = 48;
		
		pMinimalConfig->cLabel = cairo_dock_get_string_key_value (pKeyFile, "Icon", "name", NULL, NULL, NULL, NULL);
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
		pMinimalConfig->bAlwaysVisible = g_key_file_get_boolean (pKeyFile, "Icon", "always visi", NULL);
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
	g_free (pMinimalConfig);
}


CairoDockModuleInstance *cairo_dock_instanciate_module (CairoDockModule *pModule, gchar *cConfFilePath)  // prend possession de 'cConfFilePah'.
{
	g_return_val_if_fail (pModule != NULL, NULL);
	cd_message ("%s (%s)", __func__, cConfFilePath);
	
	//\____________________ On cree une instance du module.
	//CairoDockModuleInstance *pInstance = g_new0 (CairoDockModuleInstance, 1);
	CairoDockModuleInstance *pInstance = calloc (1, sizeof (CairoDockModuleInstance) + pModule->pVisitCard->iSizeOfConfig + pModule->pVisitCard->iSizeOfData);
	pInstance->pModule = pModule;
	pInstance->cConfFilePath = cConfFilePath;
	/*if (pModule->pVisitCard->iSizeOfConfig > 0)
		pInstance->pConfig = g_new0 (gpointer, pModule->pVisitCard->iSizeOfConfig);
	if (pModule->pVisitCard->iSizeOfData > 0)
		pInstance->pData = g_new0 (gpointer, pModule->pVisitCard->iSizeOfData);*/
	
	CairoDockMinimalAppletConfig *pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
	GKeyFile *pKeyFile = cairo_dock_pre_read_module_instance_config (pInstance, pMinimalConfig);
	g_return_val_if_fail (cConfFilePath == NULL || pKeyFile != NULL, NULL);  // protection en cas de fichier de conf illisible.
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
		
		// on trouve/cree son container.
		if (pModule->bCanDetach && pMinimalConfig->bIsDetached)
		{
			pDesklet = cairo_dock_create_desklet (NULL, &pMinimalConfig->deskletAttribute);
			/*cd_debug ("transparence du desklet...\n");
			while (gtk_events_pending ())  // pour la transparence initiale.
				gtk_main_iteration ();*/
			pContainer = CAIRO_CONTAINER (pDesklet);
		}
		else
		{
			const gchar *cDockName = (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
			pDock = cairo_dock_search_dock_from_name (cDockName);
			if (pDock == NULL)
			{
				pDock = cairo_dock_create_dock (cDockName, NULL);
			}
			pContainer = CAIRO_CONTAINER (pDock);
		}
		
		// on cree son icone.
		pIcon = cairo_dock_create_icon_for_applet (pMinimalConfig,
			pInstance,
			pContainer);
		if (pDesklet)
		{
			pDesklet->pIcon = pIcon;
			gtk_window_set_title (GTK_WINDOW(pContainer->pWidget), pInstance->pModule->pVisitCard->cModuleName);
			///gtk_widget_queue_draw (pContainer->pWidget);
		}
		cairo_dock_free_minimal_config (pMinimalConfig);
	}

	//\____________________ On initialise l'instance.
	if (pDock)  //  on met la taille qu'elle aura une fois dans le dock.
	{
		pIcon->fWidth *= pDock->container.fRatio;
		pIcon->fHeight *= pDock->container.fRatio;
	}
	
	pInstance->pIcon = pIcon;
	pInstance->pDock = pDock;
	pInstance->pDesklet = pDesklet;
	pInstance->pContainer = pContainer;
	
	if (pKeyFile)
		_cairo_dock_read_module_config (pKeyFile, pInstance);
	
	gboolean bCanInit = TRUE;
	pInstance->pDrawContext = NULL;
	if (pDock && pIcon)  // applet dans un dock (dans un desklet, il faut attendre que l'applet ait mis une vue pour que l'icone soit chargee).
	{
		if (pIcon->pIconBuffer == NULL)
		{
			cd_warning ("icon's buffer is NULL, applet won't be able to draw to it !");
			pInstance->pDrawContext = NULL;
			bCanInit = FALSE;
		}
		else
		{
			pInstance->pDrawContext = cairo_create (pIcon->pIconBuffer);
			if (!pInstance->pDrawContext || cairo_status (pInstance->pDrawContext) != CAIRO_STATUS_SUCCESS)
			{
				cd_warning ("couldn't initialize drawing context, applet won't be able to draw itself !");
				pInstance->pDrawContext = NULL;
				bCanInit = FALSE;
			}
		}
	}
	
	if (bCanInit && pModule->pInterface->initModule)
		pModule->pInterface->initModule (pInstance, pKeyFile);
	
	if (pDock)
	{
		pIcon->fWidth /= pDock->container.fRatio;
		pIcon->fHeight /= pDock->container.fRatio;
		if (cairo_dock_is_loading ())
		{
			cairo_dock_insert_icon_in_dock (pIcon, pDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
		}
		else
		{
			cairo_dock_insert_icon_in_dock (pIcon, pDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
		}
	}
	else if (pDesklet && pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0)  // peut arriver si le desklet a fini de se redimensionner avant l'init.
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

void cairo_dock_deinstanciate_module (CairoDockModuleInstance *pInstance)
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
	gchar *cOldDockName = NULL;
	gchar *cCurrentSubDockName = NULL;
	
	CairoContainer *pNewContainer = NULL;
	CairoDock *pNewDock = NULL;
	CairoDesklet *pNewDesklet = NULL;
	
	//\______________ On recharge la config minimale.
	gboolean bModuleReloaded = FALSE;
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
				pMinimalConfig->cLabel = NULL;  // astuce.
				g_free (pIcon->cFileName);
				pIcon->cFileName = pMinimalConfig->cIconFileName;
				pMinimalConfig->cIconFileName = NULL;
				pIcon->bAlwaysVisible = pMinimalConfig->bAlwaysVisible;
			}
			
			// on recupere son dock (cree au besoin).
			if (!pMinimalConfig->bIsDetached)  // elle est desormais dans un dock.
			{
				const gchar *cDockName = (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
				pNewDock = cairo_dock_search_dock_from_name (cDockName);
				if (pNewDock == NULL)  // c'est un nouveau dock.
				{
					cairo_dock_add_root_dock_config_for_name (cDockName);
					pNewDock = cairo_dock_create_dock (cDockName, NULL);
				}
				pNewContainer = CAIRO_CONTAINER (pNewDock);
			}
			
			// on la detache de son dock si son container a change.
			if (pCurrentDock != NULL && (pMinimalConfig->bIsDetached || pNewDock != pCurrentDock))
			{
				cd_message ("le container a change (%s -> %s)", pIcon->cParentDockName, pMinimalConfig->bIsDetached ? "desklet" : pMinimalConfig->cDockName);
				cOldDockName = g_strdup (pIcon->cParentDockName);
				cairo_dock_detach_icon_from_dock (pIcon, pCurrentDock, myIconsParam.iSeparateIcons);
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
		pIcon->fWidth = pMinimalConfig->iDesiredIconWidth;
		pIcon->fHeight = pMinimalConfig->iDesiredIconHeight;
		
		// on charge l'icone a la bonne taille.
		cairo_dock_set_icon_size (pNewContainer, pIcon);
		cairo_dock_load_icon_buffers (pIcon, pNewContainer);
		
		// on insere l'icone dans le dock ou on met a jour celui-ci.
		if (pNewDock != pCurrentDock)  // on l'insere dans son nouveau dock.
		{
			cairo_dock_insert_icon_in_dock (pIcon, pNewDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
			pIcon->cParentDockName = g_strdup (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
			cairo_dock_start_icon_animation (pIcon, pNewDock);
		}
		else  // le dock n'a pas change, on le met a jour.
		{
			pIcon->fWidth *= pNewContainer->fRatio;
			pIcon->fHeight *= pNewContainer->fRatio;
			
			if (bReloadAppletConf)
			{
				cairo_dock_update_dock_size (pNewDock);
				cairo_dock_calculate_dock_icons (pNewDock);
				gtk_widget_queue_draw (pNewContainer->pWidget);
			}
		}
	}
	
	//\_______________________ On recharge la config.
	gboolean bCanReload = TRUE;
	if (pKeyFile != NULL)
	{
		_cairo_dock_read_module_config (pKeyFile, pInstance);
	}
	
	if (pInstance->pDrawContext != NULL)
	{
		cairo_destroy (pInstance->pDrawContext);
		pInstance->pDrawContext = NULL;
	}
	if (pIcon && pIcon->pIconBuffer)  // applet, on lui associe un contexte de dessin avant le reload.
	{
		pInstance->pDrawContext = cairo_create (pIcon->pIconBuffer);
		if (!pInstance->pDrawContext || cairo_status (pInstance->pDrawContext) != CAIRO_STATUS_SUCCESS)
		{
			cd_warning ("couldn't initialize drawing context, applet won't be reloaded !");
			bCanReload = FALSE;
			pInstance->pDrawContext = NULL;
		}
	}

	//\_______________________ On recharge l'instance.
	if (bCanReload && module->pInterface->reloadModule != NULL)
		bModuleReloaded = module->pInterface->reloadModule (pInstance, pCurrentContainer, pKeyFile);
	
	if (pNewContainer != pCurrentContainer && pNewDock != NULL && pCurrentDock != NULL && pIcon != NULL && pIcon->pSubDock != NULL)
	{
		cairo_dock_synchronize_one_sub_dock_orientation (pIcon->pSubDock, pNewDock, TRUE);
	}
	
	if (pNewDock != NULL && pNewDock->iRefCount != 0)  // on redessine l'icone pointant sur le sous-dock contenant l'applet, au cas ou son image aurait change.
	{
		cairo_dock_redraw_subdock_content (pNewDock);
	}
	
	///cairo_dock_trigger_refresh_launcher_gui ();  /// voir s'il y'a besoin de qqch ici...
	
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
			///cairo_dock_destroy_dock (pCurrentDock, cOldDockName);
			pCurrentDock = NULL;  // se fera detruire automatiquement.
		}
		else
		{
			cairo_dock_update_dock_size (pCurrentDock);
			cairo_dock_calculate_dock_icons (pCurrentDock);
			gtk_widget_queue_draw (pCurrentContainer->pWidget);
		}
	}
	if (pNewDesklet != NULL && pIcon && pIcon->pSubDock != NULL)
	{
		cairo_dock_destroy_dock (pIcon->pSubDock, cCurrentSubDockName);
		pIcon->pSubDock = NULL;
	}
	g_free (cOldDockName);
	g_free (cCurrentSubDockName);
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

	g_free (module->cConfFilePath);
	module->cConfFilePath = cairo_dock_check_module_conf_file (module->pVisitCard);
	
	gchar *cInstanceFilePath = NULL;
	int j = 0;
	do
	{
		if (j == 0)
			cInstanceFilePath = g_strdup (module->cConfFilePath);  // NULL si cConfFilePath l'est.
		else
			cInstanceFilePath = g_strdup_printf ("%s-%d",  module->cConfFilePath, j);
		
		if (cInstanceFilePath != NULL && ! g_file_test (cInstanceFilePath, G_FILE_TEST_EXISTS))  // la 1ere condition est pour xxx-integration par exemple .
		{
			g_free (cInstanceFilePath);
			break ;
		}
		
		cairo_dock_instanciate_module (module, cInstanceFilePath);  // prend possession de 'cInstanceFilePath'.
		
		j ++;
	} while (cInstanceFilePath != NULL);
	
	if (j == 0)
	{
		g_set_error (erreur, 1, 1, "%s () : no instance of module %s could be created", __func__, module->pVisitCard->cModuleName);
		return ;
	}
	cairo_dock_notify_on_object (&myModulesMgr, NOTIFICATION_MODULE_ACTIVATED, module->pVisitCard->cModuleName, TRUE);
}

void cairo_dock_deactivate_module (CairoDockModule *module)
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

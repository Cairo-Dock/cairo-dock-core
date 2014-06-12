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


#include "gldi-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_conf_file_needs_update
#include "cairo-dock-log.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-dialog-factory.h"  // gldi_dialog_new
#include "cairo-dock-config.h"  // cairo_dock_get_string_key_value
#include "cairo-dock-icon-facility.h"  // cairo_dock_set_icon_container
#include "cairo-dock-dock-facility.h"  // cairo_dock_resize_icon_in_dock
#include "cairo-dock-data-renderer.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_update_conf_file
#include "cairo-dock-module-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-module-instance-manager.h"

// public (manager, config, data)
GldiObjectManager myModuleInstanceObjectMgr;

// dependancies
extern CairoDock *g_pMainDock;
extern gchar *g_cCurrentThemePath;

// private
static int s_iNbUsedSlots = 0;
static GldiModuleInstance *s_pUsedSlots[CAIRO_DOCK_NB_DATA_SLOT+1];


GldiModuleInstance *gldi_module_instance_new (GldiModule *pModule, gchar *cConfFilePah)  // The module-instance takes ownership of the path
{
	GldiModuleInstanceAttr attr = {pModule, cConfFilePah};
	
	GldiModuleInstance *pInstance = g_malloc0 (sizeof (GldiModuleInstance) + pModule->pVisitCard->iSizeOfConfig + pModule->pVisitCard->iSizeOfData);  // we allocate everything at once, since config and data will anyway live as long as the instance itself.
	gldi_object_init (GLDI_OBJECT(pInstance), &myModuleInstanceObjectMgr, &attr);
	return pInstance;
}


static void _read_module_config (GKeyFile *pKeyFile, GldiModuleInstance *pInstance)
{
	GldiModuleInterface *pInterface = pInstance->pModule->pInterface;
	GldiVisitCard *pVisitCard = pInstance->pModule->pVisitCard;
	
	gboolean bFlushConfFileNeeded = FALSE;
	if (pInterface->read_conf_file != NULL)
	{
		if (pInterface->reset_config != NULL)
			pInterface->reset_config (pInstance);
		if (pVisitCard->iSizeOfConfig != 0)
			memset (((gpointer)pInstance)+sizeof(GldiModuleInstance), 0, pVisitCard->iSizeOfConfig);
		
		bFlushConfFileNeeded = pInterface->read_conf_file (pInstance, pKeyFile);
	}
	if (! bFlushConfFileNeeded)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, pVisitCard->cModuleVersion);
	if (bFlushConfFileNeeded)
	{
		gchar *cTemplate = g_strdup_printf ("%s/%s", pVisitCard->cShareDataDir, pVisitCard->cConfFileName);
		cairo_dock_upgrade_conf_file_full (pInstance->cConfFilePath, pKeyFile, cTemplate, FALSE);  // keep private keys.
		g_free (cTemplate);
	}
}

GKeyFile *gldi_module_instance_open_conf_file (GldiModuleInstance *pInstance, CairoDockMinimalAppletConfig *pMinimalConfig)
{
	g_return_val_if_fail (pInstance != NULL, NULL);
	//\____________________ on ouvre son fichier de conf.
	if (pInstance->cConfFilePath == NULL)  // aucun fichier de conf (xxx-integration par exemple).
		return NULL;
	gchar *cInstanceConfFilePath = pInstance->cConfFilePath;
	
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
		pMinimalConfig->cDockName = cairo_dock_get_string_key_value (pKeyFile, "Icon", "dock name", NULL, NULL, NULL, NULL);
		pMinimalConfig->fOrder = cairo_dock_get_double_key_value (pKeyFile, "Icon", "order", NULL, CAIRO_DOCK_LAST_ORDER, NULL, NULL);
		if (pMinimalConfig->fOrder == CAIRO_DOCK_LAST_ORDER)  // no order is defined (1st activation) => place it after the last launcher/applet
		{
			GList *ic, *next_ic;
			Icon *icon, *icon1 = NULL, *icon2 = NULL;
			if (g_pMainDock != NULL)  // the dock is not defined too => it goes into the main dock.
			{
				for (ic = g_pMainDock->icons; ic != NULL; ic = ic->next)
				{
					icon = ic->data;
					if (GLDI_OBJECT_IS_LAUNCHER_ICON (icon) || GLDI_OBJECT_IS_STACK_ICON (icon) || GLDI_OBJECT_IS_APPLET_ICON (icon))
					{
						icon1 = icon;
						next_ic = ic->next;
						icon2 = (next_ic?next_ic->data:NULL);
					}
				}
			}
			if (icon1 != NULL)
			{
				pMinimalConfig->fOrder = (icon2 && icon2->iGroup == CAIRO_DOCK_LAUNCHER ? (icon1->fOrder + icon2->fOrder) / 2 : icon1->fOrder + 1);
			}
			else
			{
				pMinimalConfig->fOrder = 0;
			}
			g_key_file_set_double (pKeyFile, "Icon", "order", pMinimalConfig->fOrder);
			cd_debug ("set order to %.1f", pMinimalConfig->fOrder);
			cairo_dock_write_keys_to_file (pKeyFile, cInstanceConfFilePath);
		}
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
			double *col = g_key_file_get_double_list (pKeyFile, "Icon", "bg color", &length, NULL);
			if (length >= 4)
			{
				pMinimalConfig->pHiddenBgColor = g_new0 (GldiColor, 1);
				memcpy (&pMinimalConfig->pHiddenBgColor->rgba, col, sizeof(GdkRGBA));
			}
			g_free (col);
		}
	}
	
	//\____________________ on recupere les parametres de son desklet.
	if (pInstance->pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DESKLET)  // l'applet peut aller dans un desklet.
	{
		CairoDeskletAttr *pDeskletAttribute = &pMinimalConfig->deskletAttribute;
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
		if (cDecorationTheme != NULL && strcmp (cDecorationTheme, "personnal") == 0)
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

void gldi_module_instance_free_generic_config (CairoDockMinimalAppletConfig *pMinimalConfig)
{
	if (pMinimalConfig == NULL)
		return;
	g_free (pMinimalConfig->cLabel);
	g_free (pMinimalConfig->cIconFileName);
	g_free (pMinimalConfig->cDockName);
	g_free (pMinimalConfig->deskletAttribute.cDecorationTheme);
	gldi_desklet_decoration_free (pMinimalConfig->deskletAttribute.pUserDecoration);
	g_free (pMinimalConfig->pHiddenBgColor);
	g_free (pMinimalConfig);
}


void gldi_module_instance_detach (GldiModuleInstance *pInstance)
{
	//\__________________ open the conf file.
	gboolean bIsDetached = (pInstance->pDesklet != NULL);
	if ((bIsDetached && pInstance->pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DOCK) ||
		(!bIsDetached && pInstance->pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DESKLET))
	{
		//\__________________ update the conf file of the applet with the new state.
		cairo_dock_update_conf_file (pInstance->cConfFilePath,
			G_TYPE_BOOLEAN, "Desklet", "initially detached", !bIsDetached,
			G_TYPE_INT, "Desklet", "accessibility", CAIRO_DESKLET_NORMAL,
			G_TYPE_INVALID);
		
		//\__________________ reload the applet.
		gldi_object_reload (GLDI_OBJECT(pInstance), TRUE);
		
		//\__________________ notify everybody.
		gldi_object_notify (pInstance, NOTIFICATION_MODULE_INSTANCE_DETACHED, pInstance, !bIsDetached);
	}
}

void gldi_module_instance_detach_at_position (GldiModuleInstance *pInstance, int iCenterX, int iCenterY)
{
	g_return_if_fail (pInstance->pDesklet == NULL);
	//\__________________ open the conf file (we need the future size of the desklet).
	GKeyFile *pKeyFile = cairo_dock_open_key_file (pInstance->cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	//\__________________ compute coordinates of the center of the desklet.
	int iDeskletWidth = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "width", NULL, 92, NULL, NULL);
	int iDeskletHeight = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "height", NULL, 92, NULL, NULL);
	
	int iDeskletPositionX = iCenterX - iDeskletWidth/2;
	int iDeskletPositionY = iCenterY - iDeskletHeight/2;
	
	//\__________________ update the conf file of the applet with the new state.
	g_key_file_set_double (pKeyFile, "Desklet", "x position", iDeskletPositionX);
	g_key_file_set_double (pKeyFile, "Desklet", "y position", iDeskletPositionY);
	g_key_file_set_boolean (pKeyFile, "Desklet", "initially detached", TRUE);
	g_key_file_set_double (pKeyFile, "Desklet", "locked", FALSE);  // we usually will want to adjust the position of the new desklet.
	g_key_file_set_double (pKeyFile, "Desklet", "no input", FALSE);  // we usually will want to adjust the position of the new desklet.
	g_key_file_set_double (pKeyFile, "Desklet", "accessibility", CAIRO_DESKLET_NORMAL);  // prevent "unforseen consequences".
	
	cairo_dock_write_keys_to_file (pKeyFile, pInstance->cConfFilePath);
	g_key_file_free (pKeyFile);
	
	//\__________________ reload the applet.
	gldi_object_reload (GLDI_OBJECT(pInstance), TRUE);
	
	//\__________________ notify everybody.
	gldi_object_notify (pInstance, NOTIFICATION_MODULE_INSTANCE_DETACHED, pInstance, TRUE);  // inutile de notifier du changement de taille, le configure-event du desklet s'en chargera.
}


void gldi_module_instance_popup_description (GldiModuleInstance *pModuleInstance)
{
	gchar *cDescription = g_strdup_printf ("%s (v%s) %s %s\n%s",
		pModuleInstance->pModule->pVisitCard->cModuleName, // let the original name
		pModuleInstance->pModule->pVisitCard->cModuleVersion,
		_("by"),
		pModuleInstance->pModule->pVisitCard->cAuthor,
		dgettext (pModuleInstance->pModule->pVisitCard->cGettextDomain,
		pModuleInstance->pModule->pVisitCard->cDescription));
	
	CairoDialogAttr attr;
	memset (&attr, 0, sizeof (CairoDialogAttr));
	attr.cText = cDescription;
	attr.cImageFilePath = pModuleInstance->pModule->pVisitCard->cIconFilePath;
	attr.bUseMarkup = TRUE;
	attr.pIcon = pModuleInstance->pIcon;
	attr.pContainer = pModuleInstance->pContainer;
	gldi_dialog_new (&attr);
	
	g_free (cDescription);
}


gboolean gldi_module_instance_reserve_data_slot (GldiModuleInstance *pInstance)
{
	g_return_val_if_fail (s_iNbUsedSlots < CAIRO_DOCK_NB_DATA_SLOT, FALSE);
	if (s_iNbUsedSlots == 0)
		memset (s_pUsedSlots, 0, (CAIRO_DOCK_NB_DATA_SLOT+1) * sizeof (GldiModuleInstance*));
	
	if (pInstance->iSlotID == 0)
	{
		s_iNbUsedSlots ++;
		if (s_pUsedSlots[s_iNbUsedSlots] == NULL)
		{
			pInstance->iSlotID = s_iNbUsedSlots;
			s_pUsedSlots[s_iNbUsedSlots] = pInstance;
		}
		else
		{
			int i;
			for (i = 1; i < s_iNbUsedSlots; i ++)
			{
				if (s_pUsedSlots[i] == NULL)
				{
					pInstance->iSlotID = i;
					s_pUsedSlots[i] = pInstance;
					break ;
				}
			}
		}
	}
	return TRUE;
}

void gldi_module_instance_release_data_slot (GldiModuleInstance *pInstance)
{
	if (pInstance->iSlotID == 0)
		return;
	s_iNbUsedSlots --;
	s_pUsedSlots[pInstance->iSlotID] = NULL;
	pInstance->iSlotID = 0;
}


  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	GldiModuleInstance *pInstance = (GldiModuleInstance*)obj;
	GldiModuleInstanceAttr *mattr = (GldiModuleInstanceAttr*)attr;
	GldiModule *pModule = mattr->pModule;
	
	pInstance->pModule = mattr->pModule;
	pInstance->cConfFilePath = mattr->cConfFilePath;
	if (pInstance->pModule->pVisitCard->iSizeOfConfig > 0)
		pInstance->pConfig = ( ((gpointer)pInstance) + sizeof(GldiModuleInstance) );
	if (pInstance->pModule->pVisitCard->iSizeOfData > 0)
		pInstance->pData = ( ((gpointer)pInstance) + sizeof(GldiModuleInstance) + pInstance->pModule->pVisitCard->iSizeOfConfig);
	
	//\____________________ open the conf file.
	CairoDockMinimalAppletConfig *pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
	GKeyFile *pKeyFile = gldi_module_instance_open_conf_file (pInstance, pMinimalConfig);
	if (pInstance->cConfFilePath != NULL && pKeyFile == NULL)  // we have a conf file, but it was unreadable -> cancel
	{
		cd_warning ("unreadable config file (%s) for applet %s", pInstance->cConfFilePath, pModule->pVisitCard->cModuleName);
		g_free (pMinimalConfig);
		return;
	}
	
	//\____________________ create the icon and its container.
	GldiContainer *pContainer = NULL;
	CairoDock *pDock = NULL;
	CairoDesklet *pDesklet = NULL;
	Icon *pIcon = NULL;
	
	if (pInstance->pModule->pVisitCard->iContainerType != CAIRO_DOCK_MODULE_IS_PLUGIN)  // le module a une icone (c'est une applet).
	{
		// create the icon.
		pIcon = gldi_applet_icon_new (pMinimalConfig,
			pInstance);
		
		// create/find its container and insert the icon inside.
		if ((pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DESKLET) && pMinimalConfig->bIsDetached)
		{
			pMinimalConfig->deskletAttribute.pIcon = pIcon;
			pDesklet = gldi_desklet_new (&pMinimalConfig->deskletAttribute);
			pContainer = CAIRO_CONTAINER (pDesklet);
		}
		else
		{
			const gchar *cDockName = (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
			pDock = gldi_dock_get (cDockName);
			if (pDock == NULL)
			{
				pDock = gldi_dock_new (cDockName);
			}
			pContainer = CAIRO_CONTAINER (pDock);
			
			if (pDock)
			{
				gldi_icon_insert_in_container (pIcon, CAIRO_CONTAINER(pDock), ! cairo_dock_is_loading ());  // animate the icon if it's instanciated by the user, not during the initial loading.
				
				// we need to load the icon's buffer before we init the module, because the applet may need it. no need to do it in desklet mode, since the desklet doesn't have a renderer yet (so buffer can't be loaded).
				cairo_dock_load_icon_buffers (pIcon, pContainer);  // ne cree rien si w ou h < 0 (par exemple si l'applet est detachee).
			}
		}

		pInstance->pIcon = pIcon;
		pInstance->pDock = pDock;
		pInstance->pDesklet = pDesklet;
		pInstance->pContainer = pContainer;
	}
	
	//\____________________ initialise the instance.
	if (pKeyFile)
		_read_module_config (pKeyFile, pInstance);
	
	if (pModule->pInterface->initModule)
		pModule->pInterface->initModule (pInstance, pKeyFile);
	
	if (pDesklet && pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0)  // peut arriver si le desklet a fini de se redimensionner avant l'init.
		gtk_widget_queue_draw (pDesklet->container.pWidget);
	
	gldi_module_instance_free_generic_config (pMinimalConfig);
	if (pKeyFile != NULL)
		g_key_file_free (pKeyFile);
	
	//\____________________ add to the module.
	pModule->pInstancesList = g_list_prepend (pModule->pInstancesList, pInstance);
	if (pModule->pInstancesList->next == NULL)  // pInstance has been inserted in first, so it means it was the first instance
	{
		gldi_object_notify (pInstance->pModule, NOTIFICATION_MODULE_ACTIVATED, pModule->pVisitCard->cModuleName, TRUE);
		if (! cairo_dock_is_loading ())
			gldi_modules_write_active ();
	}
}

static void reset_object (GldiObject *obj)
{
	GldiModuleInstance *pInstance = (GldiModuleInstance*)obj;
	
	// stop the instance
	if (pInstance->pModule->pInterface->stopModule != NULL)
		pInstance->pModule->pInterface->stopModule (pInstance);
	
	if (pInstance->pModule->pInterface->reset_data != NULL)
		pInstance->pModule->pInterface->reset_data (pInstance);
	
	if (pInstance->pModule->pInterface->reset_config != NULL)
		pInstance->pModule->pInterface->reset_config (pInstance);
	
	// destroy icon/container
	if (pInstance->pDesklet)
	{
		gldi_object_unref (GLDI_OBJECT(pInstance->pDesklet));
		pInstance->pDesklet = NULL;
	}
	if (pInstance->pDrawContext != NULL)
		cairo_destroy (pInstance->pDrawContext);
	
	if (pInstance->pIcon != NULL)
	{
		Icon *pIcon = pInstance->pIcon;
		pInstance->pIcon = NULL;  // we will destroy the icon, so avoid its 'destroy' notification
		pIcon->pModuleInstance = NULL;
		
		if (pIcon->pSubDock != NULL)
		{
			gldi_object_unref (GLDI_OBJECT(pIcon->pSubDock));
			pIcon->pSubDock = NULL;
		}
		
		gldi_icon_detach (pIcon);
		gldi_object_unref (GLDI_OBJECT(pIcon));
	}
	
	gldi_module_instance_release_data_slot (pInstance);
	
	g_free (pInstance->cConfFilePath);
	
	// remove from the module
	if (pInstance->pModule->pInstancesList != NULL)
	{
		pInstance->pModule->pInstancesList = g_list_remove (pInstance->pModule->pInstancesList, pInstance);
		if (pInstance->pModule->pInstancesList == NULL)
		{
			gldi_object_notify (pInstance->pModule, NOTIFICATION_MODULE_ACTIVATED, pInstance->pModule->pVisitCard->cModuleName, FALSE);
			gldi_modules_write_active ();
		}
	}
}

static gboolean delete_object (GldiObject *obj)
{
	GldiModuleInstance *pInstance = (GldiModuleInstance*)obj;
	cd_message ("%s (%s)", __func__, pInstance->cConfFilePath);
	g_return_val_if_fail (pInstance->pModule->pInstancesList != NULL, FALSE);
	
	//\_________________ remove this instance from the current theme
	if (pInstance->pModule->pInstancesList->next != NULL)  // not the last one -> delete its conf file
	{
		cd_debug ("We remove %s", pInstance->cConfFilePath);
		cairo_dock_delete_conf_file (pInstance->cConfFilePath);
		
		// We also remove the cConfFilePath (=> this conf file no longer exist during the 'stop' callback)
		g_free (pInstance->cConfFilePath);
		pInstance->cConfFilePath = NULL;
	}
	else  // last one -> we keep its conf file, so if the parent dock has been deleted, put it back in the main dock the next time the module is activated
	{
		if (pInstance->pIcon && pInstance->pDock && ! pInstance->pDock->bIsMainDock)  // it's an applet in a dock
		{
			const gchar *cDockName = gldi_dock_get_name (pInstance->pDock);
			gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
			if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))  // the parent dock has been deleted -> put it back in the main dock the next time the module is activated
				gldi_theme_icon_write_container_name_in_conf_file (pInstance->pIcon, CAIRO_DOCK_MAIN_DOCK_NAME);
		}
	}
	return TRUE;
}

static GKeyFile* reload_object (GldiObject *obj, gboolean bReadConfig, GKeyFile *pKeyFile)
{
	GldiModuleInstance *pInstance = (GldiModuleInstance*)obj;
	GldiModule *module = pInstance->pModule;
	cd_message ("%s (%s, %d)", __func__, module->pVisitCard->cModuleName, bReadConfig);
	
	GldiContainer *pCurrentContainer = pInstance->pContainer;
	pInstance->pContainer = NULL;
	CairoDock *pCurrentDock = pInstance->pDock;
	pInstance->pDock = NULL;
	CairoDesklet *pCurrentDesklet = pInstance->pDesklet;
	pInstance->pDesklet = NULL;
	gchar *cCurrentSubDockName = NULL;
	
	GldiContainer *pNewContainer = NULL;
	CairoDock *pNewDock = NULL;
	CairoDesklet *pNewDesklet = NULL;
	
	//\______________ update the icon/container.
	Icon *pIcon = pInstance->pIcon;
	CairoDockMinimalAppletConfig *pMinimalConfig = NULL;
	if (bReadConfig && pInstance->cConfFilePath != NULL)
	{
		pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
		if (!pKeyFile)
			pKeyFile = gldi_module_instance_open_conf_file (pInstance, pMinimalConfig);
		
		if (pInstance->pModule->pVisitCard->iContainerType != CAIRO_DOCK_MODULE_IS_PLUGIN)  // c'est une applet.
		{
			// update the name, image and visibility of the icon
			if (pIcon != NULL)
			{
				if (pCurrentDock && ! pIcon->pContainer)  // icon already detached (by drag and drop)
					pCurrentDock = NULL;
				cCurrentSubDockName = g_strdup (pIcon->cName);
				
				// on gere le changement de nom de son sous-dock.
				if (pIcon->cName != NULL && pIcon->pSubDock != NULL && g_strcmp0 (pIcon->cName, pMinimalConfig->cLabel) != 0)
				{
					gchar *cNewName = cairo_dock_get_unique_dock_name (pMinimalConfig->cLabel);
					cd_debug ("* le sous-dock %s prend le nom '%s'", pIcon->cName, cNewName);
					if (strcmp (pIcon->cName, cNewName) != 0)
						gldi_dock_rename (pIcon->pSubDock, cNewName);
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
			
			// get its new dock
			if (!pMinimalConfig->bIsDetached)  // elle est desormais dans un dock.
			{
				const gchar *cDockName = (pMinimalConfig->cDockName != NULL ? pMinimalConfig->cDockName : CAIRO_DOCK_MAIN_DOCK_NAME);
				pNewDock = gldi_dock_get (cDockName);
				if (pNewDock == NULL)  // c'est un nouveau dock.
				{
					gldi_dock_add_conf_file_for_name (cDockName);
					pNewDock = gldi_dock_new (cDockName);
				}
				pNewContainer = CAIRO_CONTAINER (pNewDock);
			}
			
			// detach the icon from its container if it has changed
			if (pCurrentDock != NULL && (pMinimalConfig->bIsDetached || pNewDock != pCurrentDock))  // was in a dock, now is in another dock or in a desklet
			{
				cd_message ("le container a change (%s -> %s)", gldi_dock_get_name(pCurrentDock), pMinimalConfig->bIsDetached ? "desklet" : pMinimalConfig->cDockName);
				gldi_icon_detach (pIcon);
			}
			else if (pCurrentDesklet != NULL && ! pMinimalConfig->bIsDetached)  // was in a desklet, now is in a dock
			{
				pCurrentDesklet->pIcon = NULL;
				cairo_dock_set_icon_container (pIcon, NULL);
			}
			
			// get its desklet
			if (pMinimalConfig->bIsDetached)
			{
				if (pCurrentDesklet == NULL)  // c'est un nouveau desklet.
				{
					pMinimalConfig->deskletAttribute.pIcon = pIcon;
					pNewDesklet = gldi_desklet_new (&pMinimalConfig->deskletAttribute);
				}
				else  // on reconfigure le desklet courant.
				{
					pNewDesklet = pCurrentDesklet;
					gldi_desklet_configure (pNewDesklet, &pMinimalConfig->deskletAttribute);
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
	
	if (pNewDock != NULL && pIcon != NULL)  // the icon is now in a dock, update its size and insert it
	{
		// on recupere la taille voulue.
		if (pMinimalConfig == NULL)  // on recupere sa taille, car elle peut avoir change (si c'est la taille par defaut, ou si elle est devenue trop grande).
		{
			pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
			pKeyFile = gldi_module_instance_open_conf_file (pInstance, pMinimalConfig);
			g_key_file_free (pKeyFile);
			pKeyFile = NULL;
		}
		cairo_dock_icon_set_requested_display_size (pIcon, pMinimalConfig->iDesiredIconWidth, pMinimalConfig->iDesiredIconHeight);
		
		// on insere l'icone dans le dock ou on met a jour celui-ci.
		if (pNewDock != pCurrentDock)  // insert in its new dock.
		{
			gldi_icon_insert_in_container (pIcon, CAIRO_CONTAINER(pNewDock), CAIRO_DOCK_ANIMATE_ICON);
			cairo_dock_load_icon_buffers (pIcon, pNewContainer);  // do it now, since the applet may need it. no ned to do it in desklet mode, since the desklet doesn't have a renderer yet (so buffer can't be loaded).
		}
		else  // same dock, just update its size.
		{
			cairo_dock_resize_icon_in_dock (pIcon, pNewDock);
			if (bReadConfig)
				cairo_dock_load_icon_text (pIcon);
		}
	}
	
	//\_______________________ read the config.
	gboolean bCanReload = TRUE;
	if (pKeyFile != NULL)
	{
		_read_module_config (pKeyFile, pInstance);
	}
	
	//\_______________________ reload the instance.
	if (bCanReload && module && module->pInterface && module->pInterface->reloadModule != NULL)
		module->pInterface->reloadModule (pInstance, pCurrentContainer, pKeyFile);
	
	if (pNewDock != NULL && pNewDock->iRefCount != 0)  // on redessine l'icone pointant sur le sous-dock contenant l'applet, au cas ou son image aurait change.
	{
		cairo_dock_redraw_subdock_content (pNewDock);
	}
	
	//\_______________________ clean up.
	gldi_module_instance_free_generic_config (pMinimalConfig);
	
	if (pCurrentDesklet != NULL && pCurrentDesklet != pNewDesklet)
		gldi_object_unref (GLDI_OBJECT(pCurrentDesklet));
	if (pNewDesklet != NULL && pIcon && pIcon->pSubDock != NULL)
	{
		gldi_object_unref (GLDI_OBJECT(pIcon->pSubDock));
		pIcon->pSubDock = NULL;
	}  // no need to destroy the dock where the applet was, it will be done automatically
	g_free (cCurrentSubDockName);
	
	if (! bReadConfig && cairo_dock_get_icon_data_renderer (pIcon) != NULL)  // reload the data-renderer at the new size
		cairo_dock_reload_data_renderer_on_icon (pIcon, pNewContainer);
	
	return pKeyFile;
}

void gldi_register_module_instances_manager (void)
{
	// Object Manager
	memset (&myModuleInstanceObjectMgr, 0, sizeof (GldiObjectManager));
	myModuleInstanceObjectMgr.cName         = "ModuleInstance";
	myModuleInstanceObjectMgr.iObjectSize   = sizeof (GldiModuleInstance);
	// interface
	myModuleInstanceObjectMgr.init_object   = init_object;
	myModuleInstanceObjectMgr.reset_object  = reset_object;
	myModuleInstanceObjectMgr.delete_object = delete_object;
	myModuleInstanceObjectMgr.reload_object = reload_object;
	// signals
	gldi_object_install_notifications (GLDI_OBJECT(&myModuleInstanceObjectMgr), NB_NOTIFICATIONS_MODULE_INSTANCES);
}

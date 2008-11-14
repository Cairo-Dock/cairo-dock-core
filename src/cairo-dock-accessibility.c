/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/

#include "cairo-dock-config.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-accessibility.h"


CairoConfigAccessibility myAccessibility;
CairoConfigAccessibility prevAccessibility;


GET_GROUP_CONFIG_BEGIN (Accessibility)
	g_bReserveSpace = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "reserve space", &bFlushConfFileNeeded, FALSE, "Position", NULL);

	cairo_dock_deactivate_temporary_auto_hide ();
	pDock->bAutoHide = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "auto-hide", &bFlushConfFileNeeded, FALSE, "Position", "auto-hide");
	
	g_bPopUp = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "pop-up", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	g_bPopUpOnScreenBorder = ! cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "pop in corner only", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	
	g_iMaxAuthorizedWidth = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "max autorized width", &bFlushConfFileNeeded, 0, "Position", NULL);
	
	if (g_cRaiseDockShortcut != NULL)
	{
		cd_keybinder_unbind (g_cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_keyboard);
		g_free (g_cRaiseDockShortcut);
	}
	g_cRaiseDockShortcut = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "raise shortcut", &bFlushConfFileNeeded, NULL, "Position", NULL);
	
	g_iLeaveSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "leaving delay", &bFlushConfFileNeeded, 330, "System", NULL);
	g_iShowSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "show delay", &bFlushConfFileNeeded, 300, "System", NULL);
	bShowSubDockOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "show on click", &bFlushConfFileNeeded, FALSE, "System", NULL);
GET_GROUP_CONFIG_END


void reset_config_Accessibility (void)
{
	if (myAccessibility.cRaiseDockShortcut != NULL)
	{
		if (! cd_keybinder_bind (myAccessibility.cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_keyboard, NULL))
		{
			g_free (myAccessibility.cRaiseDockShortcut);
			myAccessibility.cRaiseDockShortcut = NULL;
		}
	}
}


gboolean pre_init (CairoDockVisitCard *pVisitCard, CairoDockModuleInterface *pInterface) \
{
	pVisitCard->cModuleName = g_strdup ("Accessibility");
	pVisitCard->cReadmeFilePath = NULL;
	pVisitCard->iMajorVersionNeeded = g_iMajorVersion;
	pVisitCard->iMinorVersionNeeded = g_iMinorVersion;
	pVisitCard->iMicroVersionNeeded = g_iMicroVersion;
	pVisitCard->cPreviewFilePath = NULL;
	pVisitCard->cGettextDomain = NULL;
	pVisitCard->cDockVersionOnCompilation = CAIRO_DOCK_VERSION;
	pVisitCard->cUserDataDir = g_cCurrentThemePath;
	pVisitCard->cShareDataDir = CAIRO_DOCK_SHARE_DATA_DIR
	pVisitCard->cConfFileName = g_cConfFile;
	pVisitCard->cModuleVersion = CAIRO_DOCK_VERSION;
	pVisitCard->iCategory = CAIRO_DOCK_CATEGORY_SYSTEM;
	pVisitCard->cIconFilePath = NULL;
	pVisitCard->iSizeOfConfig = sizeof (CairoConfigAccessibility);
	pVisitCard->iSizeOfData = 0;
	
	pInterface->initModule = NULL;
	pInterface->stopModule = NULL;
	pInterface->reloadModule = reload;
	pInterface->reset_config = reset_config;
	pInterface->reset_data = NULL;
	pInterface->read_conf_file = read_conf_file;
	
	return TRUE ;
}

gboolean reload (CairoDockModuleInstance *myApplet, CairoContainer *pOldContainer, GKeyFile *pKeyFile) 
{
	
}


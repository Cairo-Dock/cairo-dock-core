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


void cairo_dock_pre_init_position (CairoDockInternalModule *pModule)
{
	pModule->cModuleName = "Accessibility";
	pModule->cTitle = "Accessibility";
	pModule->cIcon = "gtk-fullscreen";
	pModule->cDescription = "Set the position of the main dock.";
	pModule->iCategory = CAIRO_DOCK_CATEGORY_SYSTEM;
	
	pModule->reload = reload;
	pModule->get_config = get_config;
	pModule->reset_data = NULL;
	
	pModule->pConfigPtr = &myPosition;
	pModule->pPrevConfigPtr = &prevPosition;
	pModule->pDataPtr = NULL;
}

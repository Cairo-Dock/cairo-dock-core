/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/

#include <string.h>
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-callbacks.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-accessibility.h"


CairoConfigAccessibility myAccessibility;
extern CairoDock *g_pMainDock;
extern gint g_iScreenWidth[2];

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigAccessibility *pAccessibility)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pAccessibility->bReserveSpace = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "reserve space", &bFlushConfFileNeeded, FALSE, "Position", NULL);

	pAccessibility->bAutoHide = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "auto-hide", &bFlushConfFileNeeded, FALSE, "Position", "auto-hide");
	
	pAccessibility->bPopUp = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "pop-up", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	pAccessibility->bPopUpOnScreenBorder = ! cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "pop in corner only", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	
	pAccessibility->iMaxAuthorizedWidth = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "max autorized width", &bFlushConfFileNeeded, 0, "Position", NULL);
	
	pAccessibility->cRaiseDockShortcut = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "raise shortcut", &bFlushConfFileNeeded, NULL, "Position", NULL);
	
	pAccessibility->iLeaveSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "leaving delay", &bFlushConfFileNeeded, 330, "System", NULL);
	pAccessibility->iShowSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "show delay", &bFlushConfFileNeeded, 300, "System", NULL);
	pAccessibility->bShowSubDockOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "show on click", &bFlushConfFileNeeded, FALSE, "System", NULL);

	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigAccessibility *pAccessibility)
{
	g_free (pAccessibility->cRaiseDockShortcut);
}


#define _bind_key(cShortcut) \
	do { if (! cd_keybinder_bind (cShortcut, (CDBindkeyHandler) cairo_dock_raise_from_keyboard, NULL)) { \
		g_free (cShortcut); \
		cShortcut = NULL; } } while (0)

static void reload (CairoConfigAccessibility *pPrevAccessibility, CairoConfigAccessibility *pAccessibility)
{
	CairoDock *pDock = g_pMainDock;
	
	//\_______________ Shortcut.
	if (pAccessibility->cRaiseDockShortcut != NULL)
	{
		if (pPrevAccessibility->cRaiseDockShortcut == NULL || strcmp (pAccessibility->cRaiseDockShortcut, pPrevAccessibility->cRaiseDockShortcut) != 0)
		{
			if (pPrevAccessibility->cRaiseDockShortcut != NULL)
				cd_keybinder_unbind (pPrevAccessibility->cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_keyboard);
			if (! cd_keybinder_bind (pAccessibility->cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_keyboard, NULL))
			{
				g_free (pAccessibility->cRaiseDockShortcut);
				pAccessibility->cRaiseDockShortcut = NULL;
			}
		}
	}
	else
	{
		if (pPrevAccessibility->cRaiseDockShortcut != NULL)
		{
			cd_keybinder_unbind (pPrevAccessibility->cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_keyboard);
			cairo_dock_place_root_dock (pDock);
			gtk_widget_show (pDock->pWidget);
		}
	}
	
	//\_______________ Max Size.
	if (pAccessibility->iMaxAuthorizedWidth == 0 || pAccessibility->iMaxAuthorizedWidth > g_iScreenWidth[pDock->bHorizontalDock])
		pAccessibility->iMaxAuthorizedWidth = g_iScreenWidth[pDock->bHorizontalDock];
	if (pAccessibility->iMaxAuthorizedWidth != pPrevAccessibility->iMaxAuthorizedWidth)
	{
		/// le faire pour tous les docks racine...
		cairo_dock_update_dock_size (pDock);  // met a jour les icones et le fond aussi.
	}
	
	//\_______________ Reserve Spave.
	pAccessibility->bReserveSpace = pAccessibility->bReserveSpace && (pAccessibility->cRaiseDockShortcut == NULL);
	if (pAccessibility->bReserveSpace != pPrevAccessibility->bReserveSpace)
		cairo_dock_reserve_space_for_all_root_docks (pAccessibility->bReserveSpace);
	
	if (pAccessibility->bPopUp)
		cairo_dock_start_polling_screen_edge (pDock);
	else
		cairo_dock_stop_polling_screen_edge ();
	if (! pAccessibility->bPopUp && pPrevAccessibility->bPopUp)
	{
		cairo_dock_set_root_docks_on_top_layer ();
	}
	else if (pAccessibility->bPopUp && ! pPrevAccessibility->bPopUp)
		gtk_window_set_keep_below (GTK_WINDOW (pDock->pWidget), TRUE);  // le main dock ayant ete cree avant, il n'a pas herite de ce parametre.
	
	//\_______________ Auto-Hide
	pDock->bAutoHide = pAccessibility->bAutoHide;
	if (! pAccessibility->bAutoHide && pPrevAccessibility->bAutoHide)
		cairo_dock_deactivate_temporary_auto_hide ();
	else
		cairo_dock_place_root_dock (pDock);
}



DEFINE_PRE_INIT (Accessibility)
{
	g_print ("%s (%s)\n", __func__, "Accessibility");
	pModule->cModuleName = "Accessibility";
	pModule->cTitle = "Accessibility";
	pModule->cIcon = "gtk-help";
	pModule->cDescription = "How do you access to your docks ?";
	pModule->iCategory = CAIRO_DOCK_CATEGORY_SYSTEM;
	pModule->iSizeOfConfig = sizeof (CairoConfigAccessibility);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = &myAccessibility;
	pModule->pData = NULL;
}

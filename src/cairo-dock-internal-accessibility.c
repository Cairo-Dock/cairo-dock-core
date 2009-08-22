/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/

#include <string.h>
#include "cairo-dock-modules.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-applications-manager.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-dock-facility.h"


CairoConfigAccessibility myAccessibility;
extern CairoDock *g_pMainDock;

#define _append_warning(w) do {\
	if (sWarning == NULL)\
		sWarning = g_string_new ("");\
	else\
		g_string_append_c (sWarning, '\n');\
	g_string_append (sWarning, w);\
	cd_warning (w); } while (0)

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigAccessibility *pAccessibility)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pAccessibility->bReserveSpace = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "reserve space", &bFlushConfFileNeeded, FALSE, "Position", NULL);

	pAccessibility->bAutoHide = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "auto-hide", &bFlushConfFileNeeded, FALSE, "Position", "auto-hide");
	pAccessibility->bAutoHideOnFullScreen = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "auto quick hide", &bFlushConfFileNeeded, FALSE, "TaskBar", NULL);
	pAccessibility->bAutoHideOnMaximized = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "auto quick hide on max", &bFlushConfFileNeeded, FALSE, "TaskBar", NULL);
	
	pAccessibility->bPopUp = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "pop-up", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	pAccessibility->bPopUpOnScreenBorder = ! cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "pop in corner only", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	
	pAccessibility->iMaxAuthorizedWidth = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "max autorized width", &bFlushConfFileNeeded, 0, "Position", NULL);
	pAccessibility->bExtendedMode = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "extended", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	pAccessibility->cRaiseDockShortcut = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "raise shortcut", &bFlushConfFileNeeded, NULL, "Position", NULL);
	
	pAccessibility->iLeaveSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "leaving delay", &bFlushConfFileNeeded, 330, "System", NULL);
	pAccessibility->iShowSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "show delay", &bFlushConfFileNeeded, 300, "System", NULL);
	pAccessibility->bShowSubDockOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "show on click", &bFlushConfFileNeeded, FALSE, "System", NULL);
	pAccessibility->bLockIcons = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "lock icons", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	GString *sWarning = NULL;
	if (pAccessibility->cRaiseDockShortcut != NULL)
	{
		if (pAccessibility->bPopUp)
		{
			_append_warning ("The option 'keep the dock below' is in conflict with the 'raise on shortcuts' option,\n it will be ignored");
			pAccessibility->bPopUp = FALSE;
		}
		if (pAccessibility->bReserveSpace)
		{
			_append_warning ("The option 'reserve space' is in conflict with the 'raise on shortcuts' option,\n it will be ignored");
			pAccessibility->bReserveSpace = FALSE;
		}
		if (pAccessibility->bAutoHide)
		{
			_append_warning ("The option 'auto-hide' is in conflict with the 'raise on shortcuts' option,\n it will be ignored");
			pAccessibility->bAutoHide = FALSE;
		}
		if (pAccessibility->bAutoHideOnFullScreen)
		{
			_append_warning ("The option 'auto-hide on fullscreen window' is in conflict with the 'raise on shortcuts' option,\n it will be ignored");
			pAccessibility->bAutoHideOnFullScreen = FALSE;
		}
		if (pAccessibility->bAutoHideOnMaximized)
		{
			_append_warning ("The option 'auto-hide on maximized window' is in conflict with the 'raise on shortcuts' option,\n it will be ignored");
			pAccessibility->bAutoHideOnMaximized = FALSE;
		}
	}
	
	if (pAccessibility->bPopUp)
	{
		if (pAccessibility->bReserveSpace)
		{
			_append_warning ("The option 'reserve space for dock' is in conflict with the 'keep the dock below' option,\n it will be ignored");
			pAccessibility->bReserveSpace = FALSE;
		}
		if (pAccessibility->bAutoHide)
		{
			_append_warning ("The option 'auto-hide' is in conflict with the 'keep the dock below' option,\n it will be ignored");
			pAccessibility->bAutoHide = FALSE;
		}
		if (pAccessibility->bAutoHideOnFullScreen)
		{
			_append_warning ("The option 'auto-hide on fullscreen window' is in conflict with the 'keep the dock below' option,\n it will be ignored");
			pAccessibility->bAutoHideOnFullScreen = FALSE;
		}
		if (pAccessibility->bAutoHideOnMaximized)
		{
			_append_warning ("The option 'auto-hide on maximized window' is in conflict with the 'keep the dock below' option,\n it will be ignored");
			pAccessibility->bAutoHideOnMaximized = FALSE;
		}
	}  // par contre on peut avoir reserve space avec auto-hide.
	
	
	
	if (pAccessibility->bReserveSpace)
	{
		if (pAccessibility->bAutoHideOnFullScreen)
		{
			cd_warning ("The option 'auto-hide on fullscreen window' is in conflict with the option 'reserve space for dock',\n it will be ignored");
			pAccessibility->bAutoHideOnFullScreen = FALSE;
		}
		if (pAccessibility->bAutoHideOnMaximized)
		{
			cd_warning ("The option 'auto-hide on maximized window' is in conflict with the option 'reserve space for dock',\n it will be ignored");
			pAccessibility->bAutoHideOnMaximized = FALSE;
		}
	}  // par contre l'auto-hide est une propriete pour chaque dock principal, donc il n'y a pas de redondance avec l'auto-hide de la barre des taches.
	
	if (sWarning != NULL)
	{
		if (g_pMainDock)
			cairo_dock_show_general_message (sWarning->str, 12000.);
		g_string_free (sWarning, TRUE);
	}
	
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
	if (pAccessibility->cRaiseDockShortcut != NULL)
	{
		pAccessibility->bReserveSpace = FALSE;
		pAccessibility->bPopUp = FALSE;
		pAccessibility->bAutoHide = FALSE;
	}
	
	//\_______________ Max Size.
	if (pAccessibility->iMaxAuthorizedWidth != pPrevAccessibility->iMaxAuthorizedWidth ||
		pAccessibility->bExtendedMode != pPrevAccessibility->bExtendedMode)
	{
		/// le faire pour tous les docks racine...
		cairo_dock_update_dock_size (pDock);  // met a jour les icones et le fond aussi.
	}
	
	//\_______________ Reserve Spave.
	if (pAccessibility->bReserveSpace != pPrevAccessibility->bReserveSpace)
		cairo_dock_reserve_space_for_all_root_docks (pAccessibility->bReserveSpace);
	
	//\_______________ Pop-up.
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
	if (pDock)
	{
		pDock->bAutoHide = pAccessibility->bAutoHide;
		if (! pAccessibility->bAutoHide && pPrevAccessibility->bAutoHide)
			cairo_dock_deactivate_temporary_auto_hide ();
		else
			cairo_dock_place_root_dock (pDock);
		
		if (pAccessibility->bAutoHideOnFullScreen != pPrevAccessibility->bAutoHideOnFullScreen ||
			pAccessibility->bAutoHideOnMaximized != pPrevAccessibility->bAutoHideOnMaximized)
		{
			if (cairo_dock_search_window_on_our_way (pAccessibility->bAutoHideOnMaximized, pAccessibility->bAutoHideOnFullScreen) == NULL)
			{
				if (cairo_dock_quick_hide_is_activated ())
				{
					cd_message (" => aucune fenetre n'est desormais genante");
					cairo_dock_deactivate_temporary_auto_hide ();
				}
			}
			else
			{
				if (! cairo_dock_quick_hide_is_activated ())
				{
					cd_message (" => une fenetre desormais genante");
					cairo_dock_activate_temporary_auto_hide ();
				}
			}
		}
	}
}



DEFINE_PRE_INIT (Accessibility)
{
	pModule->cModuleName = "Accessibility";
	pModule->cTitle = N_("Accessibility");
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-accessibility.svg";
	pModule->cDescription = N_("How do you access to your docks ?");
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

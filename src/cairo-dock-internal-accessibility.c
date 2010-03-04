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

#include <string.h>
#include "cairo-dock-modules.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-applications-manager.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-dock-facility.h"

CairoConfigAccessibility myAccessibility;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigAccessibility *pAccessibility)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	//\____________________ Visibilite
	int iAccessibility = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "visibility", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 pour pouvoir intercepter le cas ou la cle n'existe pas.
	gboolean bRaiseOnShortcut = FALSE;
	
	gchar *cShortkey = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "raise shortcut", &bFlushConfFileNeeded, NULL, "Position", NULL);
	
	if (iAccessibility == -1)  // option nouvelle.
	{
		pAccessibility->bReserveSpace = g_key_file_get_boolean (pKeyFile, "Accessibility", "reserve space", NULL);
		if (! pAccessibility->bReserveSpace)
		{
			pAccessibility->bPopUp = g_key_file_get_boolean (pKeyFile, "Accessibility", "pop-up", NULL);
			if (! pAccessibility->bPopUp)
			{
				pAccessibility->bAutoHide = g_key_file_get_boolean (pKeyFile, "Accessibility", "auto-hide", NULL);
				if (! pAccessibility->bAutoHide)
				{
					pAccessibility->bAutoHideOnMaximized = g_key_file_get_boolean (pKeyFile, "Accessibility", "auto quick hide on max", NULL);
					if (pAccessibility->bAutoHideOnMaximized)
						iAccessibility = 4;
					else if (cShortkey)
					{
						iAccessibility = 5;
						pAccessibility->cRaiseDockShortcut = cShortkey;
						cShortkey = NULL;
					}
				}
				else
					iAccessibility = 3;
			}
			else
				iAccessibility = 2;
		}
		else
			iAccessibility = 1;
		g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iAccessibility);
	}
	else
	{
		switch (iAccessibility)
		{
			case 0 :
			default :
			break;
			case 1:
				pAccessibility->bReserveSpace = TRUE;
			break;
			case 2 :
				pAccessibility->bPopUp = TRUE;
			break;
			case 3 :
				pAccessibility->bAutoHide = TRUE;
			break;
			case 4 :
				pAccessibility->bAutoHideOnMaximized = TRUE;
			break;
			case 5 :
				pAccessibility->cRaiseDockShortcut = cShortkey;
				cShortkey = NULL;
			break;
			
		}
	}
	g_free (cShortkey);
	
	//\____________________ Autres parametres.
	pAccessibility->iMaxAuthorizedWidth = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "max_authorized_width", &bFlushConfFileNeeded, 0, "Position", NULL);  // obsolete, cache en conf.
	pAccessibility->bExtendedMode = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "extended", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	pAccessibility->bAutoHideOnFullScreen = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "auto quick hide", &bFlushConfFileNeeded, FALSE, "TaskBar", NULL);
	
	cairo_dock_get_size_key_value (pKeyFile, "Accessibility", "zone size", &bFlushConfFileNeeded, 0, "Hidden dock", "zone size", &pAccessibility->iVisibleZoneWidth, &pAccessibility->iVisibleZoneHeight);
	if (pAccessibility->iVisibleZoneWidth == 0)
	{
		pAccessibility->iVisibleZoneWidth = g_key_file_get_integer (pKeyFile, "Hidden dock", "zone width", NULL);
		pAccessibility->iVisibleZoneHeight = g_key_file_get_integer (pKeyFile, "Hidden dock", "zone height", NULL);
		if (pAccessibility->iVisibleZoneWidth == 0)
		{
			pAccessibility->iVisibleZoneWidth = g_key_file_get_integer (pKeyFile, "Background", "zone width", NULL);
			pAccessibility->iVisibleZoneHeight = g_key_file_get_integer (pKeyFile, "Background", "zone height", NULL);
		}
		int iSize[2] = {pAccessibility->iVisibleZoneWidth, pAccessibility->iVisibleZoneHeight};
		g_key_file_set_integer_list (pKeyFile, "Accessibility", "zone size", iSize, 2);
	}
	if (pAccessibility->iVisibleZoneWidth < 50)
		pAccessibility->iVisibleZoneWidth = 50;
	if (pAccessibility->iVisibleZoneHeight == 0)
		pAccessibility->iVisibleZoneHeight = 2;
	
	pAccessibility->bPopUpOnScreenBorder = ! cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "pop in corner only", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	
	//\____________________ sous-docks.
	pAccessibility->iLeaveSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "leaving delay", &bFlushConfFileNeeded, 330, "System", NULL);
	pAccessibility->iShowSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "show delay", &bFlushConfFileNeeded, 300, "System", NULL);
	pAccessibility->bShowSubDockOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "show on click", &bFlushConfFileNeeded, FALSE, "System", NULL);
	
	//\____________________ lock
	pAccessibility->bLockAll = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "lock all", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	pAccessibility->bLockIcons = pAccessibility->bLockAll || cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "lock icons", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigAccessibility *pAccessibility)
{
	g_free (pAccessibility->cRaiseDockShortcut);
}


#define _bind_key(cShortcut) \
	do { if (! cd_keybinder_bind (cShortcut, (CDBindkeyHandler) cairo_dock_raise_from_shortcut, NULL)) { \
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
				cd_keybinder_unbind (pPrevAccessibility->cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_shortcut);
			if (! cd_keybinder_bind (pAccessibility->cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_shortcut, NULL))
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
			cd_keybinder_unbind (pPrevAccessibility->cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_shortcut);
			cairo_dock_place_root_dock (pDock);
			gtk_widget_show (pDock->container.pWidget);
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
		cairo_dock_set_all_views_to_default (1);  // 1 <=> tous les docks racines. met a jour la taille et reserve l'espace.
	}
	
	//\_______________ Zone de rappel.
	if (pAccessibility->iVisibleZoneWidth != pPrevAccessibility->iVisibleZoneWidth ||
		pAccessibility->iVisibleZoneHeight != pPrevAccessibility->iVisibleZoneHeight)
	{
		// on replace tous les docks racines (c'est bourrin, on pourrait juste le faire pour ceux qui ont l'auto-hide).
		cairo_dock_reposition_root_docks (FALSE);  // FALSE <=> main dock inclus.
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
		cairo_dock_set_docks_on_top_layer (FALSE);  // FALSE <=> all docks.
	}
	else if (pAccessibility->bPopUp && ! pPrevAccessibility->bPopUp)
		gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), TRUE);  // le main dock ayant ete cree avant, il n'a pas herite de ce parametre.
	
	//\_______________ Auto-Hide
	if (pDock)
	{
		pDock->bAutoHide = pAccessibility->bAutoHide;
		if (! pAccessibility->bAutoHide && pPrevAccessibility->bAutoHide)
		{
			cairo_dock_start_showing (pDock);
		}
		else if (pAccessibility->bAutoHide && ! pPrevAccessibility->bAutoHide)
		{
			cairo_dock_start_hiding (pDock);
		}
		
		if (pAccessibility->bAutoHideOnFullScreen != pPrevAccessibility->bAutoHideOnFullScreen ||
			pAccessibility->bAutoHideOnMaximized != pPrevAccessibility->bAutoHideOnMaximized)
		{
			if ((!pAccessibility->bAutoHideOnMaximized && ! pAccessibility->bAutoHideOnFullScreen) || cairo_dock_search_window_on_our_way (pDock, pAccessibility->bAutoHideOnMaximized, pAccessibility->bAutoHideOnFullScreen) == NULL)
			{
				if (cairo_dock_quick_hide_is_activated () && !pDock->bAutoHide)
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
	pModule->cTitle = N_("Visibility");
	pModule->cIcon = "icon-visibility.svg";
	pModule->cDescription = N_("Do you like your dock to be always visible,\n or on the contrary unobstrusive ?\nConfigure the way you access to your docks and sub-docks !");  // How do you access to your docks ?
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

/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/

#define _INTERNAL_MODULE_
#include "cairo-dock-position.h"

CairoConfigPosition myPosition;
CairoConfigPosition prevPosition;


GET_GROUP_CONFIG_BEGIN (Position)
	gboolean bFlushConfFileNeeded = FALSE;
	myPosition.iGapX = cairo_dock_get_integer_key_value (pKeyFile, "Position", "x gap", &bFlushConfFileNeeded, 0, NULL, NULL);
	myPosition.iGapY = cairo_dock_get_integer_key_value (pKeyFile, "Position", "y gap", &bFlushConfFileNeeded, 0, NULL, NULL);

	myPosition.iScreenBorder = cairo_dock_get_integer_key_value (pKeyFile, "Position", "screen border", &bFlushConfFileNeeded, 0, NULL, NULL);
	if (myPosition.ScreenBorder < 0 || myPosition.iScreenBorder >= CAIRO_DOCK_NB_POSITIONS)
		myPosition.iScreenBorder = 0;

	myPosition.fAlign = cairo_dock_get_double_key_value (pKeyFile, "Position", "alignment", &bFlushConfFileNeeded, 0.5, NULL, NULL);
GET_GROUP_CONFIG_END


static void reload (CairoConfigPosition *pPosition, CairoConfigPosition *pPrevPosition)
{
	CairoDock *pDock = g_pMainDock;
	if (pPosition->iScreenBorder != pPrevPosition->iScreenBorder)
	{
		switch (s_iScreenBorder)
		{
			case CAIRO_DOCK_BOTTOM :
				pDock->bHorizontalDock = CAIRO_DOCK_HORIZONTAL;
				pDock->bDirectionUp = TRUE;
			break;
			case CAIRO_DOCK_TOP :
				pDock->bHorizontalDock = CAIRO_DOCK_HORIZONTAL;
				pDock->bDirectionUp = FALSE;
			break;
			case CAIRO_DOCK_RIGHT :
				pDock->bHorizontalDock = CAIRO_DOCK_VERTICAL;
				pDock->bDirectionUp = TRUE;
			break;
			case CAIRO_DOCK_LEFT :
				pDock->bHorizontalDock = CAIRO_DOCK_VERTICAL;
				pDock->bDirectionUp = FALSE;
			break;
		}
		cairo_dock_synchronize_sub_docks_position (pDock, FALSE);
		cairo_dock_reload_buffers_in_all_docks (TRUE);
	}
	pDock->calculate_icons (pDock);
	gtk_widget_queue_draw (pDock->pWidget);
	cairo_dock_place_root_dock (pDock);
}

void cairo_dock_pre_init_position (CairoDockInternalModule *pModule)
{
	pModule->cModuleName = "Position";
	pModule->cTitle = "Position";
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

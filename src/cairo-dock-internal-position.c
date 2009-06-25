/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/

#include "cairo-dock-modules.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-internal-accessibility.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-position.h"

CairoConfigPosition myPosition;
extern CairoDock *g_pMainDock;
extern int g_iScreenOffsetX, g_iScreenOffsetY;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigPosition *pPosition)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pPosition->iGapX = cairo_dock_get_integer_key_value (pKeyFile, "Position", "x gap", &bFlushConfFileNeeded, 0, NULL, NULL);
	pPosition->iGapY = cairo_dock_get_integer_key_value (pKeyFile, "Position", "y gap", &bFlushConfFileNeeded, 0, NULL, NULL);

	pPosition->iScreenBorder = cairo_dock_get_integer_key_value (pKeyFile, "Position", "screen border", &bFlushConfFileNeeded, 0, NULL, NULL);
	if (pPosition->iScreenBorder < 0 || pPosition->iScreenBorder >= CAIRO_DOCK_NB_POSITIONS)
		pPosition->iScreenBorder = 0;

	pPosition->fAlign = cairo_dock_get_double_key_value (pKeyFile, "Position", "alignment", &bFlushConfFileNeeded, 0.5, NULL, NULL);
	
	pPosition->bUseXinerama = cairo_dock_get_boolean_key_value (pKeyFile, "Position", "xinerama", &bFlushConfFileNeeded, 0, NULL, NULL);
	if (pPosition->bUseXinerama && ! cairo_dock_xinerama_is_available ())
	{
		cd_warning ("Sorry but either your X server does not have the Xinerama extension, or your version of Cairo-Dock was not built with the support of Xinerama.\n You can't place the dock on a particular screen");
		pPosition->bUseXinerama = FALSE;
	}
	pPosition->iNumScreen = cairo_dock_get_integer_key_value (pKeyFile, "Position", "num screen", &bFlushConfFileNeeded, 0, NULL, NULL);

	return bFlushConfFileNeeded;
}


static void reload (CairoConfigPosition *pPrevPosition, CairoConfigPosition *pPosition)
{
	//g_print ("%s (%d;%d)\n", __func__, pPosition->iGapX, pPosition->iGapY);
	if (pPosition->bUseXinerama)
		cairo_dock_get_screen_offsets (myPosition.iNumScreen);
	else
		g_iScreenOffsetX = g_iScreenOffsetY = 0;
	
	CairoDock *pDock = g_pMainDock;
	gboolean bWasHorizontal = pDock->bHorizontalDock;
	if (pPosition->iScreenBorder != pPrevPosition->iScreenBorder)
	{
		switch (pPosition->iScreenBorder)
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
		cairo_dock_update_dock_size (pDock);  // si bHorizonalDock a change, la taille max a change aussi.
		cairo_dock_synchronize_sub_docks_position (pDock, FALSE);
		cairo_dock_reload_buffers_in_all_docks (TRUE);
	}
	else
		cairo_dock_update_dock_size (pDock);  // si l'ecran a change, la taille max a change aussi.
	
	pDock->iGapX = pPosition->iGapX;
	pDock->iGapY = pPosition->iGapY;
	pDock->fAlign = pPosition->fAlign;
	cairo_dock_calculate_dock_icons (pDock);
	cairo_dock_place_root_dock (pDock);
	if (bWasHorizontal != pDock->bHorizontalDock)
		pDock->iCurrentWidth --;  // la taille dans le referentiel du dock ne change pas meme si on change d'horizontalite, par contre la taille de la fenetre change. On introduit donc un biais ici pour forcer le configure-event a faire son travail, sinon ca fausse le redraw.
	gtk_widget_queue_draw (pDock->pWidget);
}


DEFINE_PRE_INIT (Position)
{
	pModule->cModuleName = "Position";
	pModule->cTitle = N_("Position");
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-position.png";
	pModule->cDescription = N_("Set the position of the main dock.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_SYSTEM;
	pModule->iSizeOfConfig = sizeof (CairoConfigPosition);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = NULL;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myPosition;
	pModule->pData = NULL;
}

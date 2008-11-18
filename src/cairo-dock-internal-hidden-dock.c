/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/


#include "cairo-dock-load.h"
#include "cairo-dock-dock-factory.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-hidden-dock.h"

CairoConfigHiddenDock myHiddenDock;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigHiddenDock *pHiddenDock)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pHiddenDock->cVisibleZoneImageFile = cairo_dock_get_string_key_value (pKeyFile, "Hidden dock", "callback image", &bFlushConfFileNeeded, NULL, "Background", "background image");

	pHiddenDock->iVisibleZoneWidth = cairo_dock_get_integer_key_value (pKeyFile, "Hidden dock", "zone width", &bFlushConfFileNeeded, 150, "Background", NULL);
	if (pHiddenDock->iVisibleZoneWidth < 10)
		pHiddenDock->iVisibleZoneWidth = 10;
	pHiddenDock->iVisibleZoneHeight = cairo_dock_get_integer_key_value (pKeyFile, "Hidden dock", "zone height", &bFlushConfFileNeeded, 25, "Background", NULL);
	if (pHiddenDock->iVisibleZoneHeight < 1)
		pHiddenDock->iVisibleZoneHeight = 1;

	pHiddenDock->fVisibleZoneAlpha = cairo_dock_get_double_key_value (pKeyFile, "Hidden dock", "alpha", &bFlushConfFileNeeded, 0.5, "Background", NULL);
	pHiddenDock->bReverseVisibleImage = cairo_dock_get_boolean_key_value (pKeyFile, "Hidden dock", "reverse visible image", &bFlushConfFileNeeded, TRUE, "Background", NULL);

	return bFlushConfFileNeeded;
}


static void reload (CairoConfigHiddenDock *pPrevHiddenDock, CairoConfigHiddenDock *pHiddenDock)
{
	CairoDock *pDock = g_pMainDock;
	
	if ((pPrevHiddenDock->cVisibleZoneImageFile == NULL && pHiddenDock->cVisibleZoneImageFile != NULL) || (pPrevHiddenDock->cVisibleZoneImageFile != NULL && pHiddenDock->cVisibleZoneImageFile != NULL) || (pPrevHiddenDock->cVisibleZoneImageFile != NULL && pHiddenDock->cVisibleZoneImageFile != NULL && strcmp (pPrevHiddenDock->cVisibleZoneImageFile, pHiddenDock->cVisibleZoneImageFile) != 0))
	{
		cairo_dock_load_visible_zone (pDock, pHiddenDock->cVisibleZoneImageFile, pHiddenDock->iVisibleZoneWidth, pHiddenDock->iVisibleZoneHeight, pHiddenDock->fVisibleZoneAlpha);
	}
	
	cairo_dock_place_root_dock (pDock);
	
	gtk_widget_queue_draw (pDock->pWidget);  // le 'gdk_window_move_resize' ci-dessus ne provoquera pas le redessin si la taille n'a pas change.
}


DEFINE_PRE_INIT (HiddenDock)
{
	pModule->cModuleName = "HiddenDock";
	pModule->cTitle = "HiddenDock";
	pModule->cIcon = "gtk-goto-bottom";
	pModule->cDescription = "Define the appearance of the dock when it's hidden.";
	pModule->iCategory = CAIRO_DOCK_CATEGORY_THEME;
	pModule->iSizeOfConfig = sizeof (CairoConfigHiddenDock);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = NULL;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myHiddenDock;
	pModule->pData = NULL;
}

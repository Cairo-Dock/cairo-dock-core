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
#include "cairo-dock-load.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-hidden-dock.h"

CairoConfigHiddenDock myHiddenDock;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigHiddenDock *pHiddenDock)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	
	cairo_dock_get_size_key_value (pKeyFile, "Hidden dock", "zone size", &bFlushConfFileNeeded, 0, NULL, NULL, &pHiddenDock->iVisibleZoneWidth, &pHiddenDock->iVisibleZoneHeight);
	if (pHiddenDock->iVisibleZoneWidth == 0)
	{
		pHiddenDock->iVisibleZoneWidth = g_key_file_get_integer (pKeyFile, "Hidden dock", "zone width", NULL);
		pHiddenDock->iVisibleZoneHeight = g_key_file_get_integer (pKeyFile, "Hidden dock", "zone height", NULL);
		if (pHiddenDock->iVisibleZoneWidth == 0)
		{
			pHiddenDock->iVisibleZoneWidth = g_key_file_get_integer (pKeyFile, "Background", "zone width", NULL);
			pHiddenDock->iVisibleZoneHeight = g_key_file_get_integer (pKeyFile, "Background", "zone height", NULL);
		}
		int iSize[2] = {pHiddenDock->iVisibleZoneWidth, pHiddenDock->iVisibleZoneHeight};
		g_key_file_set_integer_list (pKeyFile, "Hidden dock", "zone size", iSize, 2);
	}
	if (pHiddenDock->iVisibleZoneWidth < 20)
		pHiddenDock->iVisibleZoneWidth = 20;
	if (pHiddenDock->iVisibleZoneHeight == 0)
		pHiddenDock->iVisibleZoneHeight = 2;
	
	pHiddenDock->cVisibleZoneImageFile = cairo_dock_get_string_key_value (pKeyFile, "Hidden dock", "callback image", &bFlushConfFileNeeded, NULL, "Background", "background image");
	
	pHiddenDock->fVisibleZoneAlpha = cairo_dock_get_double_key_value (pKeyFile, "Hidden dock", "alpha", &bFlushConfFileNeeded, 0.5, "Background", NULL);
	pHiddenDock->bReverseVisibleImage = cairo_dock_get_boolean_key_value (pKeyFile, "Hidden dock", "reverse visible image", &bFlushConfFileNeeded, TRUE, "Background", NULL);

	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigHiddenDock *pHiddenDock)
{
	g_free (pHiddenDock->cVisibleZoneImageFile);
}

static void reload (CairoConfigHiddenDock *pPrevHiddenDock, CairoConfigHiddenDock *pHiddenDock)
{
	CairoDock *pDock = g_pMainDock;
	
	if ((pPrevHiddenDock->cVisibleZoneImageFile == NULL && pHiddenDock->cVisibleZoneImageFile != NULL) || (pPrevHiddenDock->cVisibleZoneImageFile != NULL && pHiddenDock->cVisibleZoneImageFile != NULL) || (pPrevHiddenDock->cVisibleZoneImageFile != NULL && pHiddenDock->cVisibleZoneImageFile != NULL && strcmp (pPrevHiddenDock->cVisibleZoneImageFile, pHiddenDock->cVisibleZoneImageFile) != 0))
	{
		cairo_dock_load_visible_zone (pDock, pHiddenDock->cVisibleZoneImageFile, pHiddenDock->iVisibleZoneWidth, pHiddenDock->iVisibleZoneHeight, pHiddenDock->fVisibleZoneAlpha);
	}
	
	cairo_dock_place_root_dock (pDock);
	
	gtk_widget_queue_draw (pDock->container.pWidget);  // le 'gdk_window_move_resize' ci-dessus ne provoquera pas le redessin si la taille n'a pas change.
}


DEFINE_PRE_INIT (HiddenDock)
{
	pModule->cModuleName = "Hidden dock";
	pModule->cTitle = N_("Hidden Dock");
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-hidden-dock.png";
	pModule->cDescription = N_("Define the appearance of the dock when it's hidden.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_THEME;
	pModule->iSizeOfConfig = sizeof (CairoConfigHiddenDock);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myHiddenDock;
	pModule->pData = NULL;
}

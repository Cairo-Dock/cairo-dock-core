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
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-container.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-indicators.h"

CairoConfigIndicators myIndicators;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigIndicators *pIndicators)
{
	gboolean bFlushConfFileNeeded = FALSE;
	gchar *cIndicatorImageName;
	
	//\__________________ On recupere l'indicateur d'appli lancee.
	cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "indicator image", &bFlushConfFileNeeded, NULL, "Icons", NULL);
	if (cIndicatorImageName != NULL)
	{
		pIndicators->cIndicatorImagePath = cairo_dock_generate_file_path (cIndicatorImageName);
		g_free (cIndicatorImageName);
	}
	else
		pIndicators->cIndicatorImagePath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_INDICATOR_NAME);
	
	pIndicators->bIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "indicator above", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	
	pIndicators->fIndicatorRatio = cairo_dock_get_double_key_value (pKeyFile, "Indicators", "indicator ratio", &bFlushConfFileNeeded, 1., "Icons", NULL);
	
	pIndicators->bLinkIndicatorWithIcon = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "link indicator", &bFlushConfFileNeeded, TRUE, "Icons", NULL);
	
	pIndicators->iIndicatorDeltaY = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "indicator deltaY", &bFlushConfFileNeeded, 2, "Icons", NULL);
	
	pIndicators->bRotateWithDock = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "rotate indicator", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	//\__________________ On recupere l'indicateur de fenetre active.
	int iIndicType = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active indic type", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 pou rpouvoir intercepter le cas ou la cle n'existe pas.
	
	cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "active indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
	if (iIndicType == -1)  // nouvelle cle.
	{
		iIndicType = (cIndicatorImageName != NULL ? 0 : 1);
		g_key_file_set_integer (pKeyFile, "Indicators", "active indic type", iIndicType);
	}
	else
	{
		if (iIndicType != 0)
		{
			g_free (cIndicatorImageName);
			cIndicatorImageName = NULL;
		}
	}
	
	if (cIndicatorImageName != NULL)
	{
		pIndicators->cActiveIndicatorImagePath = cairo_dock_generate_file_path (cIndicatorImageName);
		g_free (cIndicatorImageName);
	}
	else
		pIndicators->cActiveIndicatorImagePath = NULL;
	
	double couleur_active[4] = {0., 0.4, 0.8, 0.5};
	cairo_dock_get_double_list_key_value (pKeyFile, "Indicators", "active color", &bFlushConfFileNeeded, pIndicators->fActiveColor, 4, couleur_active, "Icons", NULL);
	pIndicators->iActiveLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active line width", &bFlushConfFileNeeded, 3, "Icons", NULL);
	pIndicators->iActiveCornerRadius = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active corner radius", &bFlushConfFileNeeded, 6, "Icons", NULL);
	
	pIndicators->bActiveIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "active frame position", &bFlushConfFileNeeded, TRUE, "Icons", NULL);
	
	//\__________________ On recupere l'indicateur de classe groupee.
	pIndicators->bUseClassIndic = (cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "use class indic", &bFlushConfFileNeeded, 0, NULL, NULL) == 0);
	if (pIndicators->bUseClassIndic)
	{
		cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "class indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
		if (cIndicatorImageName != NULL)
		{
			pIndicators->cClassIndicatorImagePath = cairo_dock_generate_file_path (cIndicatorImageName);
			g_free (cIndicatorImageName);
		}
		else
		{
			pIndicators->cClassIndicatorImagePath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_DEFAULT_CLASS_INDICATOR_NAME);
		}
		pIndicators->bZoomClassIndicator = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "zoom class", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	}
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigIndicators *pIndicators)
{
	g_free (pIndicators->cIndicatorImagePath);
	g_free (pIndicators->cActiveIndicatorImagePath);
	g_free (pIndicators->cClassIndicatorImagePath);
}


static void reload (CairoConfigIndicators *pPrevIndicators, CairoConfigIndicators *pIndicators)
{
	CairoDock *pDock = g_pMainDock;
	double fMaxScale = cairo_dock_get_max_scale (pDock);
	cairo_t* pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDock));
	
	if (cairo_dock_strings_differ (pPrevIndicators->cIndicatorImagePath, pIndicators->cIndicatorImagePath) ||
		pPrevIndicators->bLinkIndicatorWithIcon != pIndicators->bLinkIndicatorWithIcon ||
		pPrevIndicators->fIndicatorRatio != pIndicators->fIndicatorRatio)
	{
		cairo_dock_load_task_indicator (myTaskBar.bShowAppli && (myTaskBar.bMixLauncherAppli || myTaskBar.bDrawIndicatorOnAppli) ? pIndicators->cIndicatorImagePath : NULL, fMaxScale, pIndicators->fIndicatorRatio);
	}
	
	if (cairo_dock_strings_differ (pPrevIndicators->cActiveIndicatorImagePath, pIndicators->cActiveIndicatorImagePath) ||
		pPrevIndicators->iActiveCornerRadius != pIndicators->iActiveCornerRadius ||
		pPrevIndicators->iActiveLineWidth != pIndicators->iActiveLineWidth ||
		cairo_dock_colors_differ (pPrevIndicators->fActiveColor, pIndicators->fActiveColor))
	{
		cairo_dock_load_active_window_indicator (pIndicators->cActiveIndicatorImagePath,
			fMaxScale,
			pIndicators->iActiveCornerRadius,
			pIndicators->iActiveLineWidth,
			pIndicators->fActiveColor);
	}
	
	if (cairo_dock_strings_differ (pPrevIndicators->cClassIndicatorImagePath, pIndicators->cClassIndicatorImagePath) ||
		pPrevIndicators->bUseClassIndic != pIndicators->bUseClassIndic)
	{
		cairo_dock_load_class_indicator (myTaskBar.bShowAppli && myTaskBar.bGroupAppliByClass ? pIndicators->cClassIndicatorImagePath : NULL, fMaxScale);
	}
	
	cairo_destroy (pCairoContext);
	
	cairo_dock_redraw_root_docks (FALSE);  // main dock inclus.
}


DEFINE_PRE_INIT (Indicators)
{
	pModule->cModuleName = "Indicators";
	pModule->cTitle = N_("Indicators");
	pModule->cIcon = "icon-indicators.svg";
	pModule->cDescription = N_("Indicators are additional markers for your icons.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_THEME;
	pModule->iSizeOfConfig = sizeof (CairoConfigIndicators);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myIndicators;
	pModule->pData = NULL;
}

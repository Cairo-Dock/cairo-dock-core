/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>

#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-icons.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-indicators.h"

CairoConfigIndicators myIndicators;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigIndicators *pIndicators)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	double couleur_active[4] = {0., 0.4, 0.8, 0.25};
	cairo_dock_get_double_list_key_value (pKeyFile, "Indicators", "active color", &bFlushConfFileNeeded, pIndicators->fActiveColor, 4, couleur_active, "Icons", NULL);
	
	pIndicators->iActiveLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active line width", &bFlushConfFileNeeded, 3, "Icons", NULL);
	pIndicators->iActiveCornerRadius = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active corner radius", &bFlushConfFileNeeded, 6, "Icons", NULL);
	pIndicators->bActiveIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "active frame position", &bFlushConfFileNeeded, TRUE, "Icons", NULL);
	gchar *cActiveIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "active indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
	if (cActiveIndicatorImageName != NULL)
	{
		pIndicators->cActiveIndicatorImagePath = cairo_dock_generate_file_path (cActiveIndicatorImageName);
		g_free (cActiveIndicatorImageName);
	}
	else
		pIndicators->cActiveIndicatorImagePath = NULL;
	
	gchar *cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "indicator image", &bFlushConfFileNeeded, NULL, "Icons", NULL);
	if (cIndicatorImageName != NULL)
	{
		pIndicators->cIndicatorImagePath = cairo_dock_generate_file_path (cIndicatorImageName);
		g_free (cIndicatorImageName);
	}
	else
	{
		pIndicators->cIndicatorImagePath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_DEFAULT_INDICATOR_NAME);
	}
	
	pIndicators->bIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "indicator above", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	
	pIndicators->fIndicatorRatio = cairo_dock_get_double_key_value (pKeyFile, "Indicators", "indicator ratio", &bFlushConfFileNeeded, 1., "Icons", NULL);
	
	pIndicators->bLinkIndicatorWithIcon = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "link indicator", &bFlushConfFileNeeded, TRUE, "Icons", NULL);
	
	pIndicators->iIndicatorDeltaY = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "indicator deltaY", &bFlushConfFileNeeded, 2, "Icons", NULL);

	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigIndicators *pIndicators)
{
	g_free (pIndicators->cActiveIndicatorImagePath);
	g_free (pIndicators->cIndicatorImagePath);
}


static void reload (CairoConfigIndicators *pPrevIndicators, CairoConfigIndicators *pIndicators)
{
	CairoDock *pDock = g_pMainDock;
	double fMaxScale = cairo_dock_get_max_scale (pDock);
	cairo_t* pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	
	if (cairo_dock_strings_differ (pPrevIndicators->cIndicatorImagePath, pIndicators->cIndicatorImagePath))
	{
		cairo_dock_load_task_indicator (myTaskBar.bShowAppli && myTaskBar.bMixLauncherAppli ? pIndicators->cIndicatorImagePath : NULL, pCairoContext, fMaxScale, pIndicators->fIndicatorRatio);
	}
	
	if (cairo_dock_strings_differ (pPrevIndicators->cActiveIndicatorImagePath, pIndicators->cActiveIndicatorImagePath))
	{
		cairo_dock_load_active_window_indicator (pCairoContext,
			pPrevIndicators->cActiveIndicatorImagePath,
			fMaxScale,
			pPrevIndicators->iActiveCornerRadius,
			pPrevIndicators->iActiveLineWidth,
			pPrevIndicators->fActiveColor);
	}
	cairo_destroy (pCairoContext);
	
	cairo_dock_redraw_root_docks (FALSE);  // main dock inclus.
}


DEFINE_PRE_INIT (Indicators)
{
	pModule->cModuleName = "Indicators";
	pModule->cTitle = "Indicators";
	pModule->cIcon = "gtk-select-color";
	pModule->cDescription = "Indicators are extra indications on your icons.";
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

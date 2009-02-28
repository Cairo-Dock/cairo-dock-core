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
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-desklet.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-desklets.h"

CairoConfigDesklets myDesklets;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigDesklets *pDesklets)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pDesklets->cDeskletDecorationsName = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "decorations", &bFlushConfFileNeeded, "dark", NULL, NULL);
	CairoDeskletDecoration *pUserDeskletDecorations = cairo_dock_get_desklet_decoration ("personnal");
	if (pUserDeskletDecorations == NULL)
	{
		pUserDeskletDecorations = g_new0 (CairoDeskletDecoration, 1);
		cairo_dock_register_desklet_decoration ("personnal", pUserDeskletDecorations);
	}
	if (pDesklets->cDeskletDecorationsName == NULL || strcmp (pDesklets->cDeskletDecorationsName, "personnal") == 0)
	{
		g_free (pUserDeskletDecorations->cBackGroundImagePath);
		pUserDeskletDecorations->cBackGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "bg desklet", &bFlushConfFileNeeded, NULL, NULL, NULL);
		g_free (pUserDeskletDecorations->cForeGroundImagePath);
		pUserDeskletDecorations->cForeGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "fg desklet", &bFlushConfFileNeeded, NULL, NULL, NULL);
		pUserDeskletDecorations->iLoadingModifier = CAIRO_DOCK_FILL_SPACE;
		pUserDeskletDecorations->fBackGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklets", "bg alpha", &bFlushConfFileNeeded, 1.0, NULL, NULL);
		pUserDeskletDecorations->fForeGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklets", "fg alpha", &bFlushConfFileNeeded, 1.0, NULL, NULL);
		pUserDeskletDecorations->iLeftMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "left offset", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
		pUserDeskletDecorations->iTopMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "top offset", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
		pUserDeskletDecorations->iRightMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "right offset", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
		pUserDeskletDecorations->iBottomMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "bottom offset", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
	}
	pDesklets->iDeskletButtonSize = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "button size", &bFlushConfFileNeeded, 16, NULL, NULL);
	pDesklets->cRotateButtonImage = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "rotate image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	pDesklets->cRetachButtonImage = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "retach image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	pDesklets->cDepthRotateButtonImage = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "depth rotate image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigDesklets *pDesklets)
{
	g_free (pDesklets->cDeskletDecorationsName);
	g_free (pDesklets->cRotateButtonImage);
	g_free (pDesklets->cRetachButtonImage);
	g_free (pDesklets->cDepthRotateButtonImage);
}


static void reload (CairoConfigDesklets *pPrevDesklets, CairoConfigDesklets *pDesklets)
{
	CairoDock *pDock = g_pMainDock;
	cairo_t* pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	if (cairo_dock_strings_differ (pPrevDesklets->cRotateButtonImage, pDesklets->cRotateButtonImage) ||
		cairo_dock_strings_differ (pPrevDesklets->cRetachButtonImage, pDesklets->cRetachButtonImage) ||
		cairo_dock_strings_differ (pPrevDesklets->cDepthRotateButtonImage, pDesklets->cDepthRotateButtonImage))
	{
		cairo_dock_load_desklet_buttons (pCairoContext);
		
	}
	if (cairo_dock_strings_differ (pPrevDesklets->cDeskletDecorationsName, pDesklets->cDeskletDecorationsName))
	{
		cairo_dock_reload_desklets_decorations (TRUE, pCairoContext);  // TRUE <=> bDefaultThemeOnly
	}
	cairo_destroy (pCairoContext);
}


DEFINE_PRE_INIT (Desklets)
{
	pModule->cModuleName = "Desklets";
	pModule->cTitle = "Desklets";
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-desklets.png";
	pModule->cDescription = N_("The applets can be set on your desktop as widgets.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_THEME;
	pModule->iSizeOfConfig = sizeof (CairoConfigDesklets);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myDesklets;
	pModule->pData = NULL;
}

/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>

#include "cairo-dock-load.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-internal-accessibility.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-views.h"

CairoConfigViews myViews;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigViews *pViews)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pViews->cMainDockDefaultRendererName = cairo_dock_get_string_key_value (pKeyFile, "Views", "main dock view", &bFlushConfFileNeeded, CAIRO_DOCK_DEFAULT_RENDERER_NAME, "Cairo Dock", NULL);
	if (pViews->cMainDockDefaultRendererName == NULL)
		pViews->cMainDockDefaultRendererName = g_strdup (CAIRO_DOCK_DEFAULT_RENDERER_NAME);
	cd_message ("cMainDockDefaultRendererName <- %s", pViews->cMainDockDefaultRendererName);

	pViews->cSubDockDefaultRendererName = cairo_dock_get_string_key_value (pKeyFile, "Views", "sub-dock view", &bFlushConfFileNeeded, CAIRO_DOCK_DEFAULT_RENDERER_NAME, "Sub-Docks", NULL);
	if (pViews->cSubDockDefaultRendererName == NULL)
		pViews->cSubDockDefaultRendererName = g_strdup (CAIRO_DOCK_DEFAULT_RENDERER_NAME);

	pViews->bSameHorizontality = cairo_dock_get_boolean_key_value (pKeyFile, "Views", "same horizontality", &bFlushConfFileNeeded, TRUE, "Sub-Docks", NULL);

	pViews->fSubDockSizeRatio = cairo_dock_get_double_key_value (pKeyFile, "Views", "relative icon size", &bFlushConfFileNeeded, 0.8, "Sub-Docks", NULL);

	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigViews *pViews)
{
	g_free (pViews->cMainDockDefaultRendererName);
	g_free (pViews->cSubDockDefaultRendererName);
}


static void reload (CairoConfigViews *pPrevViews, CairoConfigViews *pViews)
{
	CairoDock *pDock = g_pMainDock;
	
	if (cairo_dock_strings_differ (pPrevViews->cMainDockDefaultRendererName, pViews->cMainDockDefaultRendererName))
	{
		cairo_dock_set_all_views_to_default (1);  // met a jour la taille des docks.
		cairo_dock_redraw_root_docks (FALSE);  // FALSE <=> main dock inclus.
		cairo_dock_reserve_space_for_all_root_docks (myAccessibility.bReserveSpace);
	}
	
	if (cairo_dock_strings_differ (pPrevViews->cSubDockDefaultRendererName, pViews->cSubDockDefaultRendererName) || pPrevViews->bSameHorizontality != pViews->bSameHorizontality || pPrevViews->fSubDockSizeRatio != pViews->fSubDockSizeRatio)
	{
		if (pPrevViews->bSameHorizontality != pViews->bSameHorizontality || pPrevViews->fSubDockSizeRatio != pViews->fSubDockSizeRatio)
		{
			cairo_dock_synchronize_sub_docks_position (pDock, FALSE);
			cairo_dock_reload_buffers_in_all_docks (TRUE);  // y compris les applets.
		}
		cairo_dock_set_all_views_to_default (2);  // met a jour la taille des docks.
	}
}


DEFINE_PRE_INIT (Views)
{
	static const gchar *cDependencies[3] = {"dock rendering", N_("It provides different views to Cairo-Dock. Activate it first if you want to select a different view for your docks."), NULL};
	pModule->cModuleName = "Views";
	pModule->cTitle = "Views";
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-views.svg";
	pModule->cDescription = N_("Select a view for each of your docks.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_THEME;
	pModule->iSizeOfConfig = sizeof (CairoConfigViews);
	pModule->iSizeOfData = 0;
	pModule->cDependencies = cDependencies;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myViews;
	pModule->pData = NULL;
}

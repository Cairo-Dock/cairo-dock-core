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

#include <math.h>
#include "cairo-dock-modules.h"
#include "cairo-dock-load.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-file-manager.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-system.h"

CairoConfigSystem mySystem;
extern CairoDock *g_pMainDock;
extern gboolean g_bForcedOpenGL;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigSystem *pSystem)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pSystem->bLabelForPointedIconOnly = cairo_dock_get_boolean_key_value (pKeyFile, "System", "pointed icon only", &bFlushConfFileNeeded, FALSE, "Labels", NULL);
	pSystem->fLabelAlphaThreshold = cairo_dock_get_double_key_value (pKeyFile, "System", "alpha threshold", &bFlushConfFileNeeded, 10., "Labels", NULL);
	pSystem->fLabelAlphaThreshold = (pSystem->fLabelAlphaThreshold + 10.) / 10.;  // [0;50] -> [1;6]
	pSystem->bTextAlwaysHorizontal = cairo_dock_get_boolean_key_value (pKeyFile, "System", "always horizontal", &bFlushConfFileNeeded, FALSE, "Labels", NULL);
	
	//double fUserValue = cairo_dock_get_double_key_value (pKeyFile, "System", "unfold factor", &bFlushConfFileNeeded, 8., "Cairo Dock", NULL);
	//pSystem->fUnfoldAcceleration = 1 - pow (2, - fUserValue);
	pSystem->iUnfoldingDuration = cairo_dock_get_integer_key_value (pKeyFile, "System", "unfold duration", &bFlushConfFileNeeded, 400, NULL, NULL);

	pSystem->bAnimateOnAutoHide = cairo_dock_get_boolean_key_value (pKeyFile, "System", "animate on auto-hide", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	int iNbSteps = cairo_dock_get_integer_key_value (pKeyFile, "System", "grow nb steps", &bFlushConfFileNeeded, 10, "Cairo Dock", NULL);
	iNbSteps = MAX (iNbSteps, 1);
	pSystem->iGrowUpInterval = MAX (1, CAIRO_DOCK_NB_MAX_ITERATIONS / iNbSteps);
	iNbSteps = cairo_dock_get_integer_key_value (pKeyFile, "System", "shrink nb steps", &bFlushConfFileNeeded, 8, "Cairo Dock", NULL);
	iNbSteps = MAX (iNbSteps, 1);
	pSystem->iShrinkDownInterval = MAX (1, CAIRO_DOCK_NB_MAX_ITERATIONS / iNbSteps);
	pSystem->fMoveUpSpeed = cairo_dock_get_double_key_value (pKeyFile, "System", "move up speed", &bFlushConfFileNeeded, 0.35, "Cairo Dock", NULL);
	pSystem->fMoveUpSpeed = MAX (0.01, MIN (pSystem->fMoveUpSpeed, 1));
	pSystem->fMoveDownSpeed = cairo_dock_get_double_key_value (pKeyFile, "System", "move down speed", &bFlushConfFileNeeded, 0.35, "Cairo Dock", NULL);
	pSystem->fMoveDownSpeed = MAX (0.01, MIN (pSystem->fMoveDownSpeed, 1));
	
	int iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "refresh frequency", &bFlushConfFileNeeded, 25, "Cairo Dock", NULL);
	pSystem->fRefreshInterval = 1000. / iRefreshFrequency;
	pSystem->bDynamicReflection = cairo_dock_get_boolean_key_value (pKeyFile, "System", "dynamic reflection", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "opengl anim freq", &bFlushConfFileNeeded, 33, NULL, NULL);
	pSystem->iGLAnimationDeltaT = 1000. / iRefreshFrequency;
	iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "cairo anim freq", &bFlushConfFileNeeded, 25, NULL, NULL);
	pSystem->iCairoAnimationDeltaT = 1000. / iRefreshFrequency;
	
	pSystem->bAnimateSubDock = cairo_dock_get_boolean_key_value (pKeyFile, "System", "animate subdocks", &bFlushConfFileNeeded, TRUE, "Sub-Docks", NULL);
	
	pSystem->fStripesSpeedFactor = cairo_dock_get_double_key_value (pKeyFile, "System", "scroll speed factor", &bFlushConfFileNeeded, 1.0, "Background", NULL);
	pSystem->fStripesSpeedFactor = MIN (1., pSystem->fStripesSpeedFactor);
	pSystem->bDecorationsFollowMouse = cairo_dock_get_boolean_key_value (pKeyFile, "System", "decorations enslaved", &bFlushConfFileNeeded, TRUE, "Background", NULL);
	
	pSystem->iFileSortType = cairo_dock_get_integer_key_value (pKeyFile, "System", "sort files", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
	pSystem->bShowHiddenFiles = cairo_dock_get_boolean_key_value (pKeyFile, "System", "show hidden files", &bFlushConfFileNeeded, FALSE, NULL, NULL);

	pSystem->bUseFakeTransparency = cairo_dock_get_boolean_key_value (pKeyFile, "System", "fake transparency", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	if (g_bForcedOpenGL)
			pSystem->bUseFakeTransparency = TRUE;
	pSystem->bConfigPanelTransparency = cairo_dock_get_boolean_key_value (pKeyFile, "System", "config transparency", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	
	gsize length=0;
	pSystem->cActiveModuleList = cairo_dock_get_string_list_key_value (pKeyFile, "System", "modules", &bFlushConfFileNeeded, &length, NULL, "Applets", "modules_0");
	
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigSystem *pSystem)
{
	g_free (pSystem->cActiveModuleList);
}


static void reload (CairoConfigSystem *pPrevSystem, CairoConfigSystem *pSystem)
{
	CairoDock *pDock = g_pMainDock;
	
	//\_______________ Fake Transparency.
	if (pSystem->bUseFakeTransparency && ! pPrevSystem->bUseFakeTransparency)
	{
		gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), TRUE);
		cairo_dock_get_desktop_bg_surface ();
	}
	else if (! pSystem->bUseFakeTransparency && pPrevSystem->bUseFakeTransparency)
		gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), FALSE);
	
	if (pSystem->bTextAlwaysHorizontal != pPrevSystem->bTextAlwaysHorizontal)
	{
		cairo_dock_reload_buffers_in_all_docks (TRUE);  // les modules aussi.
	}
	
	if (pSystem->iFileSortType != pPrevSystem->iFileSortType || pSystem->bShowHiddenFiles != pPrevSystem->bShowHiddenFiles)
	{
		/// re-ordonner les icones ayant un cURI != NULL;
	}
}


DEFINE_PRE_INIT (System)
{
	pModule->cModuleName = "System";
	pModule->cTitle = N_("System");
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-system.png";
	pModule->cDescription = N_("All the parameters you will never want to tweak.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_SYSTEM;
	pModule->iSizeOfConfig = sizeof (CairoConfigSystem);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = NULL;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &mySystem;
	pModule->pData = NULL;
}

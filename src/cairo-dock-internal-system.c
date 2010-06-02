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
#include "../config.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-load.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-file-manager.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-system.h"

CairoConfigSystem mySystem;
extern CairoDockGLConfig g_openglConfig;
extern gboolean g_bUseOpenGL;
extern CairoDockDesktopBackground *g_pFakeTransparencyDesktopBg;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigSystem *pSystem)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	// lisibilite des labels
	pSystem->fLabelAlphaThreshold = cairo_dock_get_double_key_value (pKeyFile, "System", "alpha threshold", &bFlushConfFileNeeded, 10., "Labels", NULL);
	pSystem->fLabelAlphaThreshold = (pSystem->fLabelAlphaThreshold + 10.) / 10.;  // [0;50] -> [1;6]
	pSystem->bTextAlwaysHorizontal = cairo_dock_get_boolean_key_value (pKeyFile, "System", "always horizontal", &bFlushConfFileNeeded, FALSE, "Labels", NULL);
	
	// vitesse des animations.
	///pSystem->bAnimateOnAutoHide = cairo_dock_get_boolean_key_value (pKeyFile, "System", "animate on auto-hide", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	pSystem->bAnimateSubDock = cairo_dock_get_boolean_key_value (pKeyFile, "System", "animate subdocks", &bFlushConfFileNeeded, TRUE, "Sub-Docks", NULL);
	
	pSystem->iUnfoldingDuration = cairo_dock_get_integer_key_value (pKeyFile, "System", "unfold duration", &bFlushConfFileNeeded, 400, NULL, NULL);
	
	int iNbSteps = cairo_dock_get_integer_key_value (pKeyFile, "System", "grow nb steps", &bFlushConfFileNeeded, 10, NULL, NULL);
	iNbSteps = MAX (iNbSteps, 1);
	pSystem->iGrowUpInterval = MAX (1, CAIRO_DOCK_NB_MAX_ITERATIONS / iNbSteps);
	
	iNbSteps = cairo_dock_get_integer_key_value (pKeyFile, "System", "shrink nb steps", &bFlushConfFileNeeded, 8, NULL, NULL);
	iNbSteps = MAX (iNbSteps, 1);
	pSystem->iShrinkDownInterval = MAX (1, CAIRO_DOCK_NB_MAX_ITERATIONS / iNbSteps);
	
	pSystem->iUnhideNbSteps = cairo_dock_get_integer_key_value (pKeyFile, "System", "move up nb steps", &bFlushConfFileNeeded, 10, NULL, NULL);
	
	pSystem->iHideNbSteps = cairo_dock_get_integer_key_value (pKeyFile, "System", "move down nb steps", &bFlushConfFileNeeded, 12, NULL, NULL);
	
	// frequence de rafraichissement.
	int iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "refresh frequency", &bFlushConfFileNeeded, 25, NULL, NULL);
	pSystem->fRefreshInterval = 1000. / iRefreshFrequency;
	pSystem->bDynamicReflection = cairo_dock_get_boolean_key_value (pKeyFile, "System", "dynamic reflection", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "opengl anim freq", &bFlushConfFileNeeded, 33, NULL, NULL);
	pSystem->iGLAnimationDeltaT = 1000. / iRefreshFrequency;
	iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "cairo anim freq", &bFlushConfFileNeeded, 25, NULL, NULL);
	pSystem->iCairoAnimationDeltaT = 1000. / iRefreshFrequency;
	
	pSystem->bShowHiddenFiles = cairo_dock_get_boolean_key_value (pKeyFile, "System", "show hidden files", &bFlushConfFileNeeded, FALSE, NULL, NULL);

	pSystem->bUseFakeTransparency = cairo_dock_get_boolean_key_value (pKeyFile, "System", "fake transparency", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	if (g_bUseOpenGL && ! g_openglConfig.bAlphaAvailable)
		pSystem->bUseFakeTransparency = TRUE;
	pSystem->bConfigPanelTransparency = cairo_dock_get_boolean_key_value (pKeyFile, "System", "config transparency", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	gsize length=0;
	pSystem->cActiveModuleList = cairo_dock_get_string_list_key_value (pKeyFile, "System", "modules", &bFlushConfFileNeeded, &length, NULL, "Applets", "modules_0");
	
	pSystem->iConnectionTimeout = cairo_dock_get_integer_key_value (pKeyFile, "System", "conn timeout", &bFlushConfFileNeeded, 7, NULL, NULL);
	pSystem->iConnectionMaxTime = cairo_dock_get_integer_key_value (pKeyFile, "System", "conn max time", &bFlushConfFileNeeded, 120, NULL, NULL);
	if (cairo_dock_get_boolean_key_value (pKeyFile, "System", "conn use proxy", &bFlushConfFileNeeded, FALSE, NULL, NULL))
	{
		pSystem->cConnectionProxy = cairo_dock_get_string_key_value (pKeyFile, "System", "conn proxy", &bFlushConfFileNeeded, NULL, NULL, NULL);
		pSystem->iConnectionPort = cairo_dock_get_integer_key_value (pKeyFile, "System", "conn port", &bFlushConfFileNeeded, 0, NULL, NULL);
		pSystem->cConnectionUser = cairo_dock_get_string_key_value (pKeyFile, "System", "conn user", &bFlushConfFileNeeded, NULL, NULL, NULL);
		gchar *cPasswd = cairo_dock_get_string_key_value (pKeyFile, "System", "conn passwd", &bFlushConfFileNeeded, NULL, NULL, NULL);
		cairo_dock_decrypt_string (cPasswd, &pSystem->cConnectionPasswd);
	}
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigSystem *pSystem)
{
	g_free (pSystem->cActiveModuleList);
	g_free (pSystem->cConnectionProxy);
	g_free (pSystem->cConnectionUser);
	g_free (pSystem->cConnectionPasswd);
}


static void _set_below (CairoDock *pDock, gpointer data)
{
	gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), GPOINTER_TO_INT (data));
}
static void reload (CairoConfigSystem *pPrevSystem, CairoConfigSystem *pSystem)
{
	//\_______________ Fake Transparency.
	if (pSystem->bUseFakeTransparency && ! pPrevSystem->bUseFakeTransparency)
	{
		cairo_dock_foreach_root_docks ((GFunc)_set_below, GINT_TO_POINTER (TRUE));
		g_pFakeTransparencyDesktopBg = cairo_dock_get_desktop_background (g_bUseOpenGL);
	}
	else if (! pSystem->bUseFakeTransparency && pPrevSystem->bUseFakeTransparency)
	{
		cairo_dock_foreach_root_docks ((GFunc)_set_below, GINT_TO_POINTER (FALSE));
		cairo_dock_destroy_desktop_background (g_pFakeTransparencyDesktopBg);
		g_pFakeTransparencyDesktopBg = NULL;
	}
	
	if (pSystem->bTextAlwaysHorizontal != pPrevSystem->bTextAlwaysHorizontal)
	{
		cairo_dock_reload_buffers_in_all_docks (TRUE);  // les modules aussi.
	}
	
	if (pSystem->bShowHiddenFiles != pPrevSystem->bShowHiddenFiles)
	{
		/// re-ordonner les icones ayant un cURI != NULL;
	}
}


DEFINE_PRE_INIT (System)
{
	pModule->cModuleName = "System";
	pModule->cTitle = N_("System");
	pModule->cIcon = "icon-system.svg";
	pModule->cDescription = N_("All of the parameters you will never want to tweak.");
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

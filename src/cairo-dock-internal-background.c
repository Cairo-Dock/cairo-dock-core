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
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-container.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-background.h"

CairoConfigBackground myBackground;
extern CairoDock *g_pMainDock;
extern double g_fBackgroundImageWidth, g_fBackgroundImageHeight;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigBackground *pBackground)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	// cadre.
	pBackground->iDockRadius = cairo_dock_get_integer_key_value (pKeyFile, "Background", "corner radius", &bFlushConfFileNeeded, 12, NULL, NULL);

	pBackground->iDockLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Background", "line width", &bFlushConfFileNeeded, 2, NULL, NULL);

	pBackground->iFrameMargin = cairo_dock_get_integer_key_value (pKeyFile, "Background", "frame margin", &bFlushConfFileNeeded, 2, NULL, NULL);

	double couleur[4] = {0., 0., 0.6, 0.4};
	cairo_dock_get_double_list_key_value (pKeyFile, "Background", "line color", &bFlushConfFileNeeded, pBackground->fLineColor, 4, couleur, NULL, NULL);

	pBackground->bRoundedBottomCorner = cairo_dock_get_boolean_key_value (pKeyFile, "Background", "rounded bottom corner", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	
	gchar *cBgImage = cairo_dock_get_string_key_value (pKeyFile, "Background", "background image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	int iFillBg = cairo_dock_get_integer_key_value (pKeyFile, "Background", "fill bg", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 pour intercepter le cas ou la cle n'existe pas.
	if (iFillBg == -1)
	{
		iFillBg = (cBgImage != NULL ? 0 : 1);
		g_key_file_set_integer (pKeyFile, "Background", "fill bg", iFillBg);
	}
	else
	{
		if (iFillBg != 0)
		{
			g_free (cBgImage);
			cBgImage = NULL;
		}
	}
	
	// image de fond.
	pBackground->cBackgroundImageFile = cBgImage;
	pBackground->fBackgroundImageAlpha = cairo_dock_get_double_key_value (pKeyFile, "Background", "image alpha", &bFlushConfFileNeeded, 0.5, NULL, NULL);

	pBackground->bBackgroundImageRepeat = cairo_dock_get_boolean_key_value (pKeyFile, "Background", "repeat image", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	// degrade du fond.
	if (pBackground->cBackgroundImageFile == NULL)
	{
		pBackground->iNbStripes = cairo_dock_get_integer_key_value (pKeyFile, "Background", "number of stripes", &bFlushConfFileNeeded, 10, NULL, NULL);
		
		if (pBackground->iNbStripes != 0)
		{
			pBackground->fStripesWidth = MAX (.01, MIN (.99, cairo_dock_get_double_key_value (pKeyFile, "Background", "stripes width", &bFlushConfFileNeeded, 0.2, NULL, NULL))) / pBackground->iNbStripes;
		}
		double couleur3[4] = {.7, .7, 1., .7};
		cairo_dock_get_double_list_key_value (pKeyFile, "Background", "stripes color dark", &bFlushConfFileNeeded, pBackground->fStripesColorDark, 4, couleur3, NULL, NULL);
		
		double couleur2[4] = {.7, .9, .7, .4};
		cairo_dock_get_double_list_key_value (pKeyFile, "Background", "stripes color bright", &bFlushConfFileNeeded, pBackground->fStripesColorBright, 4, couleur2, NULL, NULL);
		
		pBackground->fStripesAngle = cairo_dock_get_double_key_value (pKeyFile, "Background", "stripes angle", &bFlushConfFileNeeded, 90., NULL, NULL);
	}
	
	// zone de rappel.
	pBackground->cVisibleZoneImageFile = cairo_dock_get_string_key_value (pKeyFile, "Background", "callback image", &bFlushConfFileNeeded, NULL, "Hidden dock", "callback image");
	
	pBackground->fVisibleZoneAlpha = cairo_dock_get_double_key_value (pKeyFile, "Background", "callback alpha", &bFlushConfFileNeeded, 0.5, "Hidden dock", "alpha");
	pBackground->bReverseVisibleImage = cairo_dock_get_boolean_key_value (pKeyFile, "Background", "callback reverse", &bFlushConfFileNeeded, TRUE, "Hidden dock", "reverse visible image");
	
	// mouvements.
	int iMovementType;
	if (! g_key_file_has_key (pKeyFile, "Background", "move bg", NULL))  // anciennes valeurs.
	{
		pBackground->fDecorationSpeed = g_key_file_get_double (pKeyFile, "System", "scroll speed factor", NULL);
		if (pBackground->fDecorationSpeed != 0)
		{
			if (g_key_file_get_boolean (pKeyFile, "System", "decorations enslaved", NULL))
				iMovementType = 2;
			else
				iMovementType = 1;
			g_key_file_set_double (pKeyFile, "Background", "decorations speed", pBackground->fDecorationSpeed);
		}
		else
			iMovementType = 0;
		g_key_file_set_integer (pKeyFile, "Background", "move bg", iMovementType);
		bFlushConfFileNeeded = TRUE;
	}
	iMovementType = cairo_dock_get_integer_key_value (pKeyFile, "Background", "move bg", &bFlushConfFileNeeded, 0, NULL, NULL);
	if (iMovementType != 0)
	{
		pBackground->fDecorationSpeed = cairo_dock_get_double_key_value (pKeyFile, "Background", "decorations speed", &bFlushConfFileNeeded, 0.5, NULL, NULL);
		pBackground->bDecorationsFollowMouse = (iMovementType == 2);
	}
	
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigBackground *pBackground)
{
	g_free (pBackground->cBackgroundImageFile);
	g_free (pBackground->cVisibleZoneImageFile);
}


static void reload (CairoConfigBackground *pPrevBackground, CairoConfigBackground *pBackground)
{
	CairoDock *pDock = g_pMainDock;
	
	if (cairo_dock_strings_differ (pPrevBackground->cVisibleZoneImageFile, pBackground->cVisibleZoneImageFile))
	{
		cairo_dock_load_visible_zone (pDock, pBackground->cVisibleZoneImageFile, myAccessibility.iVisibleZoneWidth, myAccessibility.iVisibleZoneHeight, pBackground->fVisibleZoneAlpha);
	}
	
	g_fBackgroundImageWidth = g_fBackgroundImageHeight = 0.;
	cairo_dock_set_all_views_to_default (0);  // met a jour la taille (decorations incluses) de tous les docks.
	cairo_dock_calculate_dock_icons (pDock);
	cairo_dock_redraw_root_docks (FALSE);  // main dock inclus.
}


DEFINE_PRE_INIT (Background)
{
	pModule->cModuleName = "Background";
	pModule->cTitle = N_("Background");
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-background.svg";
	pModule->cDescription = N_("Set a background to your dock.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_THEME;
	pModule->iSizeOfConfig = sizeof (CairoConfigBackground);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myBackground;
	pModule->pData = NULL;
}

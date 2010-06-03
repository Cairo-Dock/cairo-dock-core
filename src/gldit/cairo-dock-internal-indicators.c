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

#include "../../config.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-indicator-manager.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-indicators.h"

CairoConfigIndicators myIndicators;

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
		pIndicators->cIndicatorImagePath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/default-indicator.png");
	
	pIndicators->bIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "indicator above", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	
	pIndicators->fIndicatorRatio = cairo_dock_get_double_key_value (pKeyFile, "Indicators", "indicator ratio", &bFlushConfFileNeeded, 1., "Icons", NULL);
	
	pIndicators->bIndicatorOnIcon = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "indicator on icon", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	pIndicators->fIndicatorDeltaY = cairo_dock_get_double_key_value (pKeyFile, "Indicators", "indicator offset", &bFlushConfFileNeeded, 11, NULL, NULL);
	if (pIndicators->fIndicatorDeltaY > 10)  // nouvelle option.
	{
		double iIndicatorDeltaY = g_key_file_get_integer (pKeyFile, "Indicators", "indicator deltaY", NULL);
		double z = g_key_file_get_double (pKeyFile, "Icons", "zoom max", NULL);
		if (z != 0)
			iIndicatorDeltaY /= z;
		pIndicators->bIndicatorOnIcon = g_key_file_get_boolean (pKeyFile, "Indicators", "link indicator", NULL);
		if (iIndicatorDeltaY > 6)  // en general cela signifie que l'indicateur est sur le dock.
			pIndicators->bIndicatorOnIcon = FALSE;
		else if (iIndicatorDeltaY < 3)  // en general cela signifie que l'indicateur est sur l'icone.
			pIndicators->bIndicatorOnIcon = TRUE;  // sinon on garde le comportement d'avant.
		
		int w, hi=0;  // on va se baser sur la taille des lanceurs.
		cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "launcher ", bFlushConfFileNeeded, w, hi);  
		if (hi < 1)
			hi = 48;
		if (pIndicators->bIndicatorOnIcon)  // decalage vers le haut et zoom avec l'icone.
		{
			// on la recupere comme ca car on n'est pas forcement encore passe dans le groupe "Icons".
			pIndicators->fIndicatorDeltaY = (double)iIndicatorDeltaY / hi;
			g_print ("icones : %d, deltaY : %d\n", hi, (int)iIndicatorDeltaY);
		}
		else  // decalage vers le bas sans zoom.
		{
			double hr, hb, l;
			hr = hi * g_key_file_get_double (pKeyFile, "Icons", "field depth", NULL);
			hb = g_key_file_get_integer (pKeyFile, "Background", "frame margin", NULL);
			l = g_key_file_get_integer (pKeyFile, "Background", "line width", NULL);
			pIndicators->fIndicatorDeltaY = (double)iIndicatorDeltaY / (hr + hb + l/2);
		}
		g_print ("recuperation de l'indicateur : %.3f, %d\n", pIndicators->fIndicatorDeltaY, pIndicators->bIndicatorOnIcon);
		g_key_file_set_double (pKeyFile, "Indicators", "indicator offset", pIndicators->fIndicatorDeltaY);
		g_key_file_set_boolean (pKeyFile, "Indicators", "indicator on icon", pIndicators->bIndicatorOnIcon);
	}
	
	pIndicators->bRotateWithDock = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "rotate indicator", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	pIndicators->bDrawIndicatorOnAppli = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "indic on appli", &bFlushConfFileNeeded, FALSE, "TaskBar", NULL);
	
	//\__________________ On recupere l'indicateur de fenetre active.
	int iIndicType = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active indic type", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 pour pouvoir intercepter le cas ou la cle n'existe pas.
	
	cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "active indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
	if (iIndicType == -1)  // nouvelle cle.
	{
		iIndicType = (cIndicatorImageName != NULL ? 0 : 1);
		g_key_file_set_integer (pKeyFile, "Indicators", "active indic type", iIndicType);
	}
	else
	{
		if (iIndicType != 0)  // pas d'image.
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
	
	if (iIndicType == 1)  // cadre
	{
		double couleur_active[4] = {0., 0.4, 0.8, 0.5};
		cairo_dock_get_double_list_key_value (pKeyFile, "Indicators", "active color", &bFlushConfFileNeeded, pIndicators->fActiveColor, 4, couleur_active, "Icons", NULL);
		pIndicators->iActiveLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active line width", &bFlushConfFileNeeded, 3, "Icons", NULL);
		pIndicators->iActiveCornerRadius = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active corner radius", &bFlushConfFileNeeded, 6, "Icons", NULL);
	}
	
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
			pIndicators->cClassIndicatorImagePath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/default-class-indicator.svg");
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
	cairo_dock_reload_indicators (pPrevIndicators, pIndicators);
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

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
#include "cairo-dock-config.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-internal-dialogs.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-container.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-labels.h"

CairoConfigLabels myLabels;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigLabels *pLabels)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	gchar *cFontDescription = cairo_dock_get_string_key_value (pKeyFile, "Labels", "police", &bFlushConfFileNeeded, "sans 12", "Icons", NULL);
	
	PangoFontDescription *fd = pango_font_description_from_string (cFontDescription);
	pLabels->iconTextDescription.cFont = g_strdup (pango_font_description_get_family (fd));
	pLabels->iconTextDescription.iSize = pango_font_description_get_size (fd);
	g_print ("font : %s => %d\n", cFontDescription, pLabels->iconTextDescription.iSize);
	if (!pango_font_description_get_size_is_absolute (fd))
		pLabels->iconTextDescription.iSize /= PANGO_SCALE;
	g_print (" => %d\n", pLabels->iconTextDescription.iSize);
	if (pLabels->iconTextDescription.iSize == 0)
		pLabels->iconTextDescription.iSize = 14;
	pLabels->iconTextDescription.iWeight = pango_font_description_get_weight (fd);
	pLabels->iconTextDescription.iStyle = pango_font_description_get_style (fd);
	
	if (g_key_file_has_key (pKeyFile, "Labels", "size", NULL))
	{
		pLabels->iconTextDescription.iSize = g_key_file_get_integer (pKeyFile, "Labels", "size", NULL);
		int iLabelWeight = g_key_file_get_integer (pKeyFile, "Labels", "weight", NULL);
		pLabels->iconTextDescription.iWeight = cairo_dock_get_pango_weight_from_1_9 (iLabelWeight);
		gboolean bLabelStyleItalic = g_key_file_get_boolean (pKeyFile, "Labels", "italic", NULL);
		if (bLabelStyleItalic)
			pLabels->iconTextDescription.iStyle = PANGO_STYLE_ITALIC;
		else
			pLabels->iconTextDescription.iStyle = PANGO_STYLE_NORMAL;
		
		pango_font_description_set_size (fd, pLabels->iconTextDescription.iSize * PANGO_SCALE);
		pango_font_description_set_weight (fd, pLabels->iconTextDescription.iWeight);
		pango_font_description_set_style (fd, pLabels->iconTextDescription.iStyle);
		
		g_free (cFontDescription);
		cFontDescription = pango_font_description_to_string (fd);
		g_key_file_set_string (pKeyFile, "Labels", "police", cFontDescription);
		bFlushConfFileNeeded = TRUE;
	}
	pango_font_description_free (fd);
	g_free (cFontDescription);
	
	gboolean bShow = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "show labels", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	if (! bShow)
	{
		g_free (pLabels->iconTextDescription.cFont);
		pLabels->iconTextDescription.cFont = NULL;
		pLabels->iconTextDescription.iSize = 0;
	}
	
	pLabels->iconTextDescription.bOutlined = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "text oulined", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	/*pLabels->iconTextDescription.iSize = cairo_dock_get_integer_key_value (pKeyFile, "Labels", "size", &bFlushConfFileNeeded, 14, "Icons", NULL);
	int iLabelWeight = cairo_dock_get_integer_key_value (pKeyFile, "Labels", "weight", &bFlushConfFileNeeded, 5, "Icons", NULL);
	pLabels->iconTextDescription.iWeight = cairo_dock_get_pango_weight_from_1_9 (iLabelWeight);  // on se ramene aux intervalles definit par Pango.
	gboolean bLabelStyleItalic = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "italic", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	if (bLabelStyleItalic)
		pLabels->iconTextDescription.iStyle = PANGO_STYLE_ITALIC;
	else
		pLabels->iconTextDescription.iStyle = PANGO_STYLE_NORMAL;
	

	if (pLabels->iconTextDescription.cFont == NULL)
		pLabels->iconTextDescription.iSize = 0;

	if (pLabels->iconTextDescription.iSize == 0)
	{
		g_free (pLabels->iconTextDescription.cFont);
		pLabels->iconTextDescription.cFont = NULL;
	}*/
	
	double couleur_label[3] = {1., 1., 1.};
	cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text color start", &bFlushConfFileNeeded, pLabels->iconTextDescription.fColorStart, 3, couleur_label, "Icons", NULL);
	
	cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text color stop", &bFlushConfFileNeeded, pLabels->iconTextDescription.fColorStop, 3, couleur_label, "Icons", NULL);
	
	pLabels->iconTextDescription.bVerticalPattern = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "vertical label pattern", &bFlushConfFileNeeded, TRUE, "Icons", NULL);

	double couleur_backlabel[4] = {0., 0., 0., 0.5};
	cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text background color", &bFlushConfFileNeeded, pLabels->iconTextDescription.fBackgroundColor, 4, couleur_backlabel, "Icons", NULL);
	
	pLabels->iconTextDescription.iMargin = cairo_dock_get_integer_key_value (pKeyFile, "Labels", "text margin", &bFlushConfFileNeeded, 4, NULL, NULL);
	
	memcpy (&pLabels->quickInfoTextDescription, &pLabels->iconTextDescription, sizeof (CairoDockLabelDescription));
	pLabels->quickInfoTextDescription.cFont = g_strdup (pLabels->iconTextDescription.cFont);
	pLabels->quickInfoTextDescription.iSize = 12;
	pLabels->quickInfoTextDescription.iWeight = PANGO_WEIGHT_HEAVY;
	pLabels->quickInfoTextDescription.iStyle = PANGO_STYLE_NORMAL;
	myDialogs.dialogTextDescription.iMargin = 0;
	
	gboolean bUseBackgroundForLabel = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "background for label", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	if (! bUseBackgroundForLabel)
		pLabels->iconTextDescription.fBackgroundColor[3] = 0;  // ne sera pas dessine.
	
	if (myDialogs.bHomogeneous)
	{
		myDialogs.dialogTextDescription.iSize = pLabels->iconTextDescription.iSize;
		if (myDialogs.dialogTextDescription.iSize == 0)
			myDialogs.dialogTextDescription.iSize = 14;
		g_free (myDialogs.dialogTextDescription.cFont);
		myDialogs.dialogTextDescription.cFont = g_strdup (pLabels->iconTextDescription.cFont);
		myDialogs.dialogTextDescription.iWeight = pLabels->iconTextDescription.iWeight;
		myDialogs.dialogTextDescription.iStyle = pLabels->iconTextDescription.iStyle;
		myDialogs.dialogTextDescription.fBackgroundColor[3] = 0;
	}
	
	pLabels->iLabelSize = (pLabels->iconTextDescription.iSize != 0 ?
		pLabels->iconTextDescription.iSize +
		(pLabels->iconTextDescription.bOutlined ? 2 : 0) +
		2 * pLabels->iconTextDescription.iMargin : 0);
	
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigLabels *pLabels)
{
	g_free (pLabels->iconTextDescription.cFont);
	g_free (pLabels->quickInfoTextDescription.cFont);
}


static void _reload_one_label (Icon *pIcon, CairoContainer *pContainer, gpointer *data)
{
	CairoConfigLabels *pLabels = data[0];
	cairo_t* pSourceContext = data[1];
	cairo_dock_fill_one_text_buffer (pIcon, pSourceContext, &pLabels->iconTextDescription);
	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	cairo_dock_fill_one_quick_info_buffer (pIcon, pSourceContext, &pLabels->quickInfoTextDescription, fMaxScale);
}
static void _cairo_dock_resize_one_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_update_dock_size (pDock);
}
static void reload (CairoConfigLabels *pPrevLabels, CairoConfigLabels *pLabels)
{
	CairoDock *pDock = g_pMainDock;
	cairo_t* pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	gpointer data[2] = {pLabels, pCairoContext};
	cairo_dock_foreach_icons ((CairoDockForeachIconFunc) _reload_one_label, data);
	cairo_destroy (pCairoContext);
	
	if (pPrevLabels->iLabelSize != pLabels->iLabelSize)
	{
		cairo_dock_foreach_docks ((GHFunc) _cairo_dock_resize_one_dock, NULL);
	}
}


DEFINE_PRE_INIT (Labels)
{
	pModule->cModuleName = "Labels";
	pModule->cTitle = N_("Labels");
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-labels.png";
	pModule->cDescription = N_("Define the style of the icons' labels and quick-info.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_THEME;
	pModule->iSizeOfConfig = sizeof (CairoConfigLabels);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myLabels;
	pModule->pData = NULL;
}

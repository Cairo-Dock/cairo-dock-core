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
#include "cairo-dock-config.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-background.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-dialogs.h"

extern CairoDock *g_pMainDock;
CairoConfigDialogs myDialogs;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigDialogs *pDialogs)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pDialogs->cButtonOkImage = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "button_ok image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	pDialogs->cButtonCancelImage = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "button_cancel image", &bFlushConfFileNeeded, NULL, NULL, NULL);

	cairo_dock_get_size_key_value_helper (pKeyFile, "Dialogs", "button ", bFlushConfFileNeeded, pDialogs->iDialogButtonWidth, pDialogs->iDialogButtonHeight);

	double couleur_bulle[4] = {1.0, 1.0, 1.0, 0.7};
	cairo_dock_get_double_list_key_value (pKeyFile, "Dialogs", "background color", &bFlushConfFileNeeded, pDialogs->fDialogColor, 4, couleur_bulle, NULL, NULL);
	pDialogs->iDialogIconSize = MAX (16, cairo_dock_get_integer_key_value (pKeyFile, "Dialogs", "icon size", &bFlushConfFileNeeded, 48, NULL, NULL));

	gchar *cFontDescription = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "message police", &bFlushConfFileNeeded, NULL, "Icons", NULL);
	if (cFontDescription == NULL)
		cFontDescription = cairo_dock_get_default_system_font ();
	
	PangoFontDescription *fd = pango_font_description_from_string (cFontDescription);
	pDialogs->dialogTextDescription.cFont = g_strdup (pango_font_description_get_family (fd));
	pDialogs->dialogTextDescription.iSize = pango_font_description_get_size (fd);
	if (!pango_font_description_get_size_is_absolute (fd))
		pDialogs->dialogTextDescription.iSize /= PANGO_SCALE;
	if (pDialogs->dialogTextDescription.iSize == 0)
		pDialogs->dialogTextDescription.iSize = 14;
	pDialogs->dialogTextDescription.iWeight = pango_font_description_get_weight (fd);
	pDialogs->dialogTextDescription.iStyle = pango_font_description_get_style (fd);
	
	if (g_key_file_has_key (pKeyFile, "Dialogs", "message size", NULL))  // anciens parametres.
	{
		pDialogs->dialogTextDescription.iSize = g_key_file_get_integer (pKeyFile, "Dialogs", "message size", NULL);
		int iLabelWeight = g_key_file_get_integer (pKeyFile, "Dialogs", "message weight", NULL);
		pDialogs->dialogTextDescription.iWeight = cairo_dock_get_pango_weight_from_1_9 (iLabelWeight);
		gboolean bLabelStyleItalic = g_key_file_get_boolean (pKeyFile, "Dialogs", "message italic", NULL);
		if (bLabelStyleItalic)
			pDialogs->dialogTextDescription.iStyle = PANGO_STYLE_ITALIC;
		else
			pDialogs->dialogTextDescription.iStyle = PANGO_STYLE_NORMAL;
		
		pango_font_description_set_size (fd, pDialogs->dialogTextDescription.iSize * PANGO_SCALE);
		pango_font_description_set_weight (fd, pDialogs->dialogTextDescription.iWeight);
		pango_font_description_set_style (fd, pDialogs->dialogTextDescription.iStyle);
		
		g_free (cFontDescription);
		cFontDescription = pango_font_description_to_string (fd);
		g_key_file_set_string (pKeyFile, "Dialogs", "message police", cFontDescription);
		bFlushConfFileNeeded = TRUE;
	}
	pango_font_description_free (fd);
	g_free (cFontDescription);
	pDialogs->dialogTextDescription.bOutlined = cairo_dock_get_boolean_key_value (pKeyFile, "Dialogs", "outlined", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	pDialogs->dialogTextDescription.iMargin = 0;
	
	double couleur_dtext[3] = {0., 0., 0.};
	cairo_dock_get_double_list_key_value (pKeyFile, "Dialogs", "text color", &bFlushConfFileNeeded, pDialogs->dialogTextDescription.fColorStart, 3, couleur_dtext, NULL, NULL);
	memcpy (&pDialogs->dialogTextDescription.fColorStop, &pDialogs->dialogTextDescription.fColorStart, 3*sizeof (double));
	
	pDialogs->cDecoratorName = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "decorator", &bFlushConfFileNeeded, "comics", NULL, NULL);

	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigDialogs *pDialogs)
{
	g_free (pDialogs->cButtonOkImage);
	g_free (pDialogs->cButtonCancelImage);
	g_free (pDialogs->dialogTextDescription.cFont);
	g_free (pDialogs->cDecoratorName);
}


static void reload (CairoConfigDialogs *pPrevDialogs, CairoConfigDialogs *pDialogs)
{
	CairoDock *pDock = g_pMainDock;
	
	if (cairo_dock_strings_differ (pPrevDialogs->cButtonOkImage, pDialogs->cButtonOkImage) ||
		cairo_dock_strings_differ (pPrevDialogs->cButtonCancelImage, pDialogs->cButtonCancelImage) ||
		pPrevDialogs->iDialogIconSize != pDialogs->iDialogIconSize)
	{
		cairo_dock_load_dialog_buttons (CAIRO_CONTAINER (pDock), pDialogs->cButtonOkImage, pDialogs->cButtonCancelImage);
	}
	/*if (pDialogs->bHomogeneous)
	{
		pDialogs->dialogTextDescription.iSize = myLabels.iconTextDescription.iSize;
		if (pDialogs->dialogTextDescription.iSize == 0)
			pDialogs->dialogTextDescription.iSize = 14;
		pDialogs->dialogTextDescription.cFont = g_strdup (myLabels.iconTextDescription.cFont);
		pDialogs->dialogTextDescription.iWeight = myLabels.iconTextDescription.iWeight;
		pDialogs->dialogTextDescription.iStyle = myLabels.iconTextDescription.iStyle;
	}*/
}


DEFINE_PRE_INIT (Dialogs)
{
	static const gchar *cDependencies[3] = {"dialog rendering", N_("It provides different window decorators. Activate it first if you want to select a different decorator for your dialogs."), NULL};
	pModule->cModuleName = "Dialogs";
	pModule->cTitle = N_("Dialogs");
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-dialogs.svg";
	pModule->cDescription = N_("Configure the look of the dialog bubbles.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_THEME;
	pModule->iSizeOfConfig = sizeof (CairoConfigDialogs);
	pModule->iSizeOfData = 0;
	pModule->cDependencies = cDependencies;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myDialogs;
	pModule->pData = NULL;
}

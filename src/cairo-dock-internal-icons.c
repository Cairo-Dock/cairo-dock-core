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
#include <stdlib.h>

#include "cairo-dock-modules.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-log.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-container.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-class-manager.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-icons.h"

CairoConfigIcons myIcons;
extern CairoDock *g_pMainDock;
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;
extern CairoDockImageBuffer g_pDockBackgroundBuffer;


static const gchar * s_cIconTypeNames[(CAIRO_DOCK_NB_TYPES+1)/2] = {"launchers", "applications", "applets"};

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigIcons *pIcons)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	//\___________________ Ordre des icones.
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_TYPES; i ++)
		pIcons->tIconTypeOrder[i] = i;
	gsize length=0;
	
	int iSeparateIcons = 0;
	if (! g_key_file_has_key (pKeyFile, "Icons", "separate_icons", NULL))  // old parameters.
	{
		if (!g_key_file_get_boolean (pKeyFile, "Icons", "mix applets with launchers", NULL))
		{
			if (!g_key_file_get_boolean (pKeyFile, "Icons", "mix applis with launchers", NULL))
				iSeparateIcons = 3;
			else
				iSeparateIcons = 2;
		}
		else if (!g_key_file_get_boolean (pKeyFile, "Icons", "mix applis with launchers", NULL))
			iSeparateIcons = 1;
	}
	pIcons->iSeparateIcons = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "separate_icons", &bFlushConfFileNeeded, iSeparateIcons , NULL, NULL);
	
	cairo_dock_get_integer_list_key_value (pKeyFile, "Icons", "icon's type order", &bFlushConfFileNeeded, pIcons->iIconsTypesList, 3, NULL, "Cairo Dock", NULL);  // on le recupere meme si on ne separe pas les icones, pour le panneau de conf simple.
	if (pIcons->iIconsTypesList[0] == 0 && pIcons->iIconsTypesList[1] == 0)  // old format.
	{
		g_print ("icon's type order : old format\n");
		gchar **cIconsTypesList = cairo_dock_get_string_list_key_value (pKeyFile, "Icons", "icon's type order", &bFlushConfFileNeeded, &length, NULL, "Cairo Dock", NULL);
		
		if (cIconsTypesList != NULL && length > 0)
		{
			g_print (" conversion ...\n");
			unsigned int i, j;
			for (i = 0; i < length; i ++)
			{
				g_print (" %d) %s\n", i, cIconsTypesList[i]);
				for (j = 0; j < ((CAIRO_DOCK_NB_TYPES + 1) / 2); j ++)
				{
					if (strcmp (cIconsTypesList[i], s_cIconTypeNames[j]) == 0)
					{
						g_print ("   => %d\n", j);
						pIcons->tIconTypeOrder[2*j] = 2 * i;
					}
				}
			}
		}
		g_strfreev (cIconsTypesList);
		
		pIcons->iIconsTypesList[0] = pIcons->tIconTypeOrder[2*0]/2;
		pIcons->iIconsTypesList[1] = pIcons->tIconTypeOrder[2*1]/2;
		pIcons->iIconsTypesList[2] = pIcons->tIconTypeOrder[2*2]/2;
		g_print ("mise a jour avec {%d;%d;%d}\n", pIcons->iIconsTypesList[0], pIcons->iIconsTypesList[1], pIcons->iIconsTypesList[2]);
		g_key_file_set_integer_list (pKeyFile, "Icons", "icon's type order", pIcons->iIconsTypesList, 3);
		bFlushConfFileNeeded = TRUE;
	}
	
	for (i = 0; i < 3; i ++)
		pIcons->tIconTypeOrder[2*pIcons->iIconsTypesList[i]] = 2*i;
	if (pIcons->iSeparateIcons == 0)
	{
		for (i = 0; i < 3; i ++)
			pIcons->tIconTypeOrder[2*i] = 0;
	}
	else if (pIcons->iSeparateIcons == 1)
	{
		pIcons->tIconTypeOrder[CAIRO_DOCK_APPLET] = pIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER];
	}
	else if (pIcons->iSeparateIcons == 2)
	{
		pIcons->tIconTypeOrder[CAIRO_DOCK_APPLI] = pIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER];
	}
	
	//\___________________ Reflets.
	pIcons->fFieldDepth = cairo_dock_get_double_key_value (pKeyFile, "Icons", "field depth", &bFlushConfFileNeeded, 0.7, NULL, NULL);

	pIcons->fAlbedo = cairo_dock_get_double_key_value (pKeyFile, "Icons", "albedo", &bFlushConfFileNeeded, .6, NULL, NULL);

	double fMaxScale = cairo_dock_get_double_key_value (pKeyFile, "Icons", "zoom max", &bFlushConfFileNeeded, 0., NULL, NULL);
	if (fMaxScale == 0)
	{
		pIcons->fAmplitude = g_key_file_get_double (pKeyFile, "Icons", "amplitude", NULL);
		fMaxScale = 1 + pIcons->fAmplitude;
		g_key_file_set_double (pKeyFile, "Icons", "zoom max", fMaxScale);
	}
	else
		pIcons->fAmplitude = fMaxScale - 1;

	pIcons->iSinusoidWidth = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "sinusoid width", &bFlushConfFileNeeded, 250, NULL, NULL);
	pIcons->iSinusoidWidth = MAX (1, pIcons->iSinusoidWidth);

	pIcons->iIconGap = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "icon gap", &bFlushConfFileNeeded, 0, NULL, NULL);

	//\___________________ Ficelle.
	pIcons->iStringLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "string width", &bFlushConfFileNeeded, 0, NULL, NULL);

	gdouble couleur[4];
	cairo_dock_get_double_list_key_value (pKeyFile, "Icons", "string color", &bFlushConfFileNeeded, pIcons->fStringColor, 4, couleur, NULL, NULL);

	pIcons->fAlphaAtRest = cairo_dock_get_double_key_value (pKeyFile, "Icons", "alpha at rest", &bFlushConfFileNeeded, 1., NULL, NULL);
	
	//\___________________ Theme d'icone.
	pIcons->cIconTheme = cairo_dock_get_string_key_value (pKeyFile, "Icons", "default icon directory", &bFlushConfFileNeeded, NULL, "Launchers", NULL);
	if (g_key_file_has_key (pKeyFile, "Icons", "local icons", NULL))  // anciens parametres.
	{
		bFlushConfFileNeeded = TRUE;
		gboolean bUseLocalIcons = g_key_file_get_boolean (pKeyFile, "Icons", "local icons", NULL);
		if (bUseLocalIcons)
		{
			g_free (pIcons->cIconTheme);
			pIcons->cIconTheme = g_strdup ("_Custom Icons_");
			g_key_file_set_string (pKeyFile, "Icons", "default icon directory", pIcons->cIconTheme);
		}
	}
	pIcons->pDefaultIconDirectory = g_new0 (gpointer, 2 * 4);  // theme d'icone + theme local + theme default + NULL final.
	int j = 0;
	gboolean bLocalIconsUsed = FALSE, bDefaultThemeUsed = FALSE;
	
	if (pIcons->cIconTheme == NULL || *pIcons->cIconTheme == '\0')  // theme systeme.
	{
		j ++;
		bDefaultThemeUsed = TRUE;
	}
	else if (pIcons->cIconTheme[0] == '~')
	{
		pIcons->pDefaultIconDirectory[2*j] = g_strdup_printf ("%s%s", getenv ("HOME"), pIcons->cIconTheme+1);
		j ++;
	}
	else if (pIcons->cIconTheme[0] == '/')
	{
		pIcons->pDefaultIconDirectory[2*j] = g_strdup (pIcons->cIconTheme);
		j ++;
	}
	else if (strncmp (pIcons->cIconTheme, "_LocalTheme_", 12) == 0 || strncmp (pIcons->cIconTheme, "_ThemeDirectory_", 16) == 0 || strncmp (pIcons->cIconTheme, "_Custom Icons_", 14) == 0)
	{
		pIcons->pDefaultIconDirectory[2*j] = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
		j ++;
		bLocalIconsUsed = TRUE;
	}
	else
	{
		pIcons->pDefaultIconDirectory[2*j+1] = gtk_icon_theme_new ();
		gtk_icon_theme_set_custom_theme (pIcons->pDefaultIconDirectory[2*j+1], pIcons->cIconTheme);
		j ++;
	}
	
	if (! bLocalIconsUsed)
	{
		pIcons->pDefaultIconDirectory[2*j] = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
		j ++;
	}
	if (! bDefaultThemeUsed)
	{
		j ++;
	}
	pIcons->iNbIconPlaces = j;
	
	gchar *cLauncherBackgroundImageName = cairo_dock_get_string_key_value (pKeyFile, "Icons", "icons bg", &bFlushConfFileNeeded, NULL, NULL, NULL);
	if (cLauncherBackgroundImageName != NULL)
	{
		pIcons->cBackgroundImagePath = cairo_dock_generate_file_path (cLauncherBackgroundImageName);
		g_free (cLauncherBackgroundImageName);
	}
		
	//\___________________ Parametres des lanceurs.
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "launcher ", bFlushConfFileNeeded, pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER]);
	if (pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] == 0)
		pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] = 48;
	if (pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] == 0)
		pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] = 48;
	
	//\___________________ Parametres des applis.
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "appli ", bFlushConfFileNeeded, pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI], pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI]);
	if (pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] == 0)
		pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] = pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
	if (pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI] == 0)
		pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] = pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER];
	
	//\___________________ Parametres des applets.
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "applet ", bFlushConfFileNeeded, pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET], pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET]);
	if (pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] == 0)
		pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] = pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
	if (pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] == 0)
		pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] = pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER];
	
	//\___________________ Parametres des separateurs.
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "separator ", bFlushConfFileNeeded, pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12], pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12]);
	pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR23] = pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12];
	pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR23] = pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12];
	
	pIcons->iSeparatorType = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "separator type", &bFlushConfFileNeeded, -1, NULL, NULL);
	if (pIcons->iSeparatorType == -1)  // nouveau parametre, avant il etait dans dock-rendering.
	{
		pIcons->iSeparatorType = 0;  // ce qui suit est tres moche, mais c'est pour eviter d'avoir a repasser derriere tous les themes.
		gchar *cRenderingConfFile = g_strdup_printf ("%s/plug-ins/rendering/rendering.conf", g_cCurrentThemePath);
		gchar *cMainDockDefaultRendererName = g_key_file_get_string (pKeyFile, "Views", "main dock view", NULL);
		if (cMainDockDefaultRendererName && (strcmp (cMainDockDefaultRendererName, "3D plane") == 0 || strcmp (cMainDockDefaultRendererName, "curve") == 0))
		{
			GKeyFile *keyfile = cairo_dock_open_key_file (cRenderingConfFile);
			g_free (cRenderingConfFile);
			if (keyfile == NULL)
				pIcons->iSeparatorType = 0;
			else
			{
				gsize length=0;
				pIcons->iSeparatorType = g_key_file_get_integer (keyfile, "Inclinated Plane", "draw separator", NULL);
				double *color = g_key_file_get_double_list (keyfile, "Inclinated Plane", "separator color", &length, NULL);
				memcpy (pIcons->fSeparatorColor, color, 4*sizeof (gdouble));
				g_key_file_free (keyfile);
			}
		}
		g_key_file_set_integer (pKeyFile, "Icons", "separator type", pIcons->iSeparatorType);
	}
	else
	{
		double couleur[4] = {0.9,0.9,1.0,1.0};
		cairo_dock_get_double_list_key_value (pKeyFile, "Icons", "separator color", &bFlushConfFileNeeded, pIcons->fSeparatorColor, 4, couleur, NULL, NULL);
	}
	pIcons->cSeparatorImage = cairo_dock_get_string_key_value (pKeyFile, "Icons", "separator image", &bFlushConfFileNeeded, NULL, "Separators", NULL);

	pIcons->bRevolveSeparator = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "revolve separator image", &bFlushConfFileNeeded, TRUE, "Separators", NULL);

	pIcons->bConstantSeparatorSize = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "force size", &bFlushConfFileNeeded, TRUE, "Separators", NULL);
	
	
	pIcons->fReflectSize = 0;
	for (i = 0; i < CAIRO_DOCK_NB_TYPES; i ++)
	{
		if (pIcons->tIconAuthorizedHeight[i] > 0)
			pIcons->fReflectSize = MAX (pIcons->fReflectSize, pIcons->tIconAuthorizedHeight[i]);
	}
	if (pIcons->fReflectSize == 0)  // on n'a pas trouve de hauteur, on va essayer avec la largeur.
	{
		for (i = 0; i < CAIRO_DOCK_NB_TYPES; i ++)
		{
			if (pIcons->tIconAuthorizedWidth[i] > 0)
				pIcons->fReflectSize = MAX (pIcons->fReflectSize, pIcons->tIconAuthorizedWidth[i]);
		}
		if (pIcons->fReflectSize > 0)
			pIcons->fReflectSize = MIN (48, pIcons->fReflectSize);
		else
			pIcons->fReflectSize = 48;
	}
	pIcons->fReflectSize *= pIcons->fFieldDepth;
	
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigIcons *pIcons)
{
	if (pIcons->pDefaultIconDirectory != NULL)
	{
		gpointer data;
		int i;
		for (i = 0; i < pIcons->iNbIconPlaces; i ++)
		{
			if (pIcons->pDefaultIconDirectory[2*i] != NULL)
				g_free (pIcons->pDefaultIconDirectory[2*i]);
			else if (pIcons->pDefaultIconDirectory[2*i+1] != NULL)
				g_object_unref (pIcons->pDefaultIconDirectory[2*i+1]);
		}
		g_free (pIcons->pDefaultIconDirectory);
	}
	g_free (pIcons->cSeparatorImage);
	g_free (pIcons->cBackgroundImagePath);
	g_free (pIcons->cIconTheme);
}


static void reload (CairoConfigIcons *pPrevIcons, CairoConfigIcons *pIcons)
{
	CairoDock *pDock = g_pMainDock;
	double fMaxScale = cairo_dock_get_max_scale (pDock);
	cairo_t* pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDock));
	gboolean bInsertSeparators = FALSE;
	
	gboolean bGroupOrderChanged;
	if (pPrevIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER] != pIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconTypeOrder[CAIRO_DOCK_APPLI] != pIcons->tIconTypeOrder[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconTypeOrder[CAIRO_DOCK_APPLET] != pIcons->tIconTypeOrder[CAIRO_DOCK_APPLET] ||
		pPrevIcons->iSeparateIcons != pIcons->iSeparateIcons)
		bGroupOrderChanged = TRUE;
	else
		bGroupOrderChanged = FALSE;
	
	if (bGroupOrderChanged)
	{
		bInsertSeparators = TRUE;  // on enleve les separateurs avant de re-ordonner.
		cairo_dock_remove_automatic_separators (pDock);
		
		if (pPrevIcons->iSeparateIcons && ! pIcons->iSeparateIcons)
		{
			cairo_dock_reorder_classes ();  // on re-ordonne les applis a cote des lanceurs/applets.
		}
		
		pDock->icons = g_list_sort (pDock->icons, (GCompareFunc) cairo_dock_compare_icons_order);
	}
	
	if ((pPrevIcons->iSeparateIcons && ! pIcons->iSeparateIcons) ||
		pPrevIcons->cSeparatorImage != pIcons->cSeparatorImage ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude)
	{
		bInsertSeparators = TRUE;
		cairo_dock_remove_automatic_separators (pDock);
	}
	
	gboolean bThemeChanged = cairo_dock_strings_differ (pIcons->cIconTheme, pPrevIcons->cIconTheme);
	
	gboolean bIconBackgroundImagesChanged = FALSE;
	// if background images are different, reload them and trigger the reload of all icons
	if (cairo_dock_strings_differ (pPrevIcons->cBackgroundImagePath, pIcons->cBackgroundImagePath) ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude)
	{
		bIconBackgroundImagesChanged = TRUE;
		cairo_dock_load_icons_background_surface (pIcons->cBackgroundImagePath, fMaxScale);
	}
	
	///cairo_dock_create_icon_pbuffer ();
	cairo_dock_create_icon_fbo ();
	
	if (pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude ||
		(!g_bUseOpenGL && pPrevIcons->fFieldDepth != pIcons->fFieldDepth) ||
		(!g_bUseOpenGL && pPrevIcons->fAlbedo != pIcons->fAlbedo) ||
		bThemeChanged ||
		bIconBackgroundImagesChanged)  // oui on ne fait pas dans la finesse.
	{
		g_pDockBackgroundBuffer.iWidth = g_pDockBackgroundBuffer.iHeight = 0.;  // pour mettre a jour les decorations.
		cairo_dock_reload_buffers_in_all_docks (TRUE);  // TRUE <=> y compris les applets.
	}
	
	if (bInsertSeparators)
	{
		cairo_dock_insert_separators_in_dock (pDock);
	}
	
	if (pPrevIcons->iSeparatorType != pIcons->iSeparatorType)
	{
		cairo_dock_update_dock_size (pDock);  // le chargement des separateurs plats se fait dans le calcul de max dock size.
	}
	
	if (pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude)
	{
		cairo_dock_load_active_window_indicator (myIndicators.cActiveIndicatorImagePath,
			fMaxScale,
			myIndicators.iActiveCornerRadius,
			myIndicators.iActiveLineWidth,
			myIndicators.fActiveColor);
		if (myTaskBar.bShowAppli && myTaskBar.bGroupAppliByClass)
			cairo_dock_load_class_indicator (myIndicators.cClassIndicatorImagePath,
				fMaxScale);
		if (myTaskBar.bShowAppli && myTaskBar.bMixLauncherAppli)
			cairo_dock_load_task_indicator (myIndicators.cIndicatorImagePath,
				fMaxScale,
				myIndicators.fIndicatorRatio);
	}
	
	g_pDockBackgroundBuffer.iWidth = g_pDockBackgroundBuffer.iHeight = 0.;
	cairo_dock_set_all_views_to_default (0);  // met a jour la taille (decorations incluses) de tous les docks.
	cairo_dock_calculate_dock_icons (pDock);
	cairo_dock_redraw_root_docks (FALSE);  // main dock inclus.
	
	cairo_destroy (pCairoContext);
}


DEFINE_PRE_INIT (Icons)
{
	static const gchar *cDependencies[3] = {"Animated icons", N_("Provides various animations for your icons."), NULL};
	pModule->cModuleName = "Icons";
	pModule->cTitle = N_("Icons");
	pModule->cIcon = "icon-icons.svg";
	pModule->cDescription = N_("All about icons:\n size, reflection, icon theme,...");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_THEME;
	pModule->iSizeOfConfig = sizeof (CairoConfigIcons);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myIcons;
	pModule->pData = NULL;
}

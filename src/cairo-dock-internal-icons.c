/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>
#include <stdlib.h>

#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-internal-taskbar.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-icons.h"

CairoConfigIcons myIcons;
extern CairoDock *g_pMainDock;
extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentLaunchersPath;
extern gboolean g_bUseOpenGL;
extern double g_fBackgroundImageWidth, g_fBackgroundImageHeight;

static const gchar * s_cIconTypeNames[(CAIRO_DOCK_NB_TYPES+1)/2] = {"launchers", "applications", "applets"};

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigIcons *pIcons)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_TYPES; i ++)
		pIcons->tIconTypeOrder[i] = i;
	gsize length=0;
	gchar **cIconsTypesList = cairo_dock_get_string_list_key_value (pKeyFile, "Icons", "icon's type order", &bFlushConfFileNeeded, &length, NULL, "Cairo Dock", NULL);
	if (cIconsTypesList != NULL && length > 0)
	{
		unsigned int i, j;
		for (i = 0; i < length; i ++)
		{
			for (j = 0; j < ((CAIRO_DOCK_NB_TYPES + 1) / 2); j ++)
			{
				if (strcmp (cIconsTypesList[i], s_cIconTypeNames[j]) == 0)
				{
					pIcons->tIconTypeOrder[2*j] = 2 * i;
				}
			}
		}
	}
	g_strfreev (cIconsTypesList);
	
	pIcons->bMixAppletsAndLaunchers = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "mix applets with launchers", &bFlushConfFileNeeded, FALSE , NULL, NULL);
	if (pIcons->bMixAppletsAndLaunchers)
		pIcons->tIconTypeOrder[CAIRO_DOCK_APPLET] = pIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER];
	
	pIcons->fFieldDepth = cairo_dock_get_double_key_value (pKeyFile, "Icons", "field depth", &bFlushConfFileNeeded, 0.7, NULL, NULL);

	pIcons->fAlbedo = cairo_dock_get_double_key_value (pKeyFile, "Icons", "albedo", &bFlushConfFileNeeded, .6, NULL, NULL);

	pIcons->fAmplitude = cairo_dock_get_double_key_value (pKeyFile, "Icons", "amplitude", &bFlushConfFileNeeded, 1.0, NULL, NULL);

	pIcons->iSinusoidWidth = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "sinusoid width", &bFlushConfFileNeeded, 250, NULL, NULL);
	pIcons->iSinusoidWidth = MAX (1, pIcons->iSinusoidWidth);

	pIcons->iIconGap = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "icon gap", &bFlushConfFileNeeded, 0, NULL, NULL);

	pIcons->iStringLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "string width", &bFlushConfFileNeeded, 0, NULL, NULL);

	gdouble couleur[4];
	cairo_dock_get_double_list_key_value (pKeyFile, "Icons", "string color", &bFlushConfFileNeeded, pIcons->fStringColor, 4, couleur, NULL, NULL);

	pIcons->fAlphaAtRest = cairo_dock_get_double_key_value (pKeyFile, "Icons", "alpha at rest", &bFlushConfFileNeeded, 1., NULL, NULL);
	
	
	pIcons->pDirectoryList = cairo_dock_get_string_list_key_value (pKeyFile, "Icons", "default icon directory", &bFlushConfFileNeeded, &length, NULL, "Launchers", NULL);
	if (pIcons->pDirectoryList == NULL)
	{
		pIcons->pDefaultIconDirectory = NULL;
	}
	else
	{
		pIcons->pDefaultIconDirectory = g_new0 (gpointer, 2 * length + 2);  // +2 pour les NULL final.
		i = 0;
		int j = 0;
		while (pIcons->pDirectoryList[i] != NULL)
		{
			if (pIcons->pDirectoryList[i][0] == '~')
			{
				pIcons->pDefaultIconDirectory[j] = g_strdup_printf ("%s%s", getenv ("HOME"), pIcons->pDirectoryList[i]+1);
				cd_message (" utilisation du repertoire %s", pIcons->pDefaultIconDirectory[j]);
			}
			else if (pIcons->pDirectoryList[i][0] == '/')
			{
				pIcons->pDefaultIconDirectory[j] = g_strdup (pIcons->pDirectoryList[i]);
				cd_message (" utilisation du repertoire %s", pIcons->pDefaultIconDirectory[j]);
			}
			else if (strncmp (pIcons->pDirectoryList[i], CAIRO_DOCK_LOCAL_THEME_KEYWORD, strlen (CAIRO_DOCK_LOCAL_THEME_KEYWORD)) == 0)
			{
				pIcons->pDefaultIconDirectory[j] = g_strdup_printf ("%s/%s%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR, pIcons->pDirectoryList[i]+strlen (CAIRO_DOCK_LOCAL_THEME_KEYWORD));
				cd_message (" utilisation du repertoire local %s", pIcons->pDefaultIconDirectory[j]);
			}
			else if (strncmp (pIcons->pDirectoryList[i], "_ThemeDirectory_", 16) == 0)
			{
				pIcons->pDefaultIconDirectory[j] = g_strdup_printf ("%s/%s%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR, pIcons->pDirectoryList[i]+16);
				cd_warning ("Cairo-Dock's local icons are now located in the 'icons' folder, they will be moved there");
				gchar *cCommand = g_strdup_printf ("cd '%s' && mv *.svg *.png *.xpm *.jpg *.bmp *.gif '%s/%s' > /dev/null", g_cCurrentLaunchersPath, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
				cCommand = g_strdup_printf ("sed -i \"s/_ThemeDirectory_/%s/g\" '%s/%s'", CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
				cCommand = g_strdup_printf ("sed -i \"/default icon directory/ { s/~\\/.config\\/%s\\/%s\\/icons/%s/g }\" '%s/%s'", CAIRO_DOCK_DATA_DIR, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
			}
			else
			{
				cd_message (" utilisation du theme %s", pIcons->pDirectoryList[i]);
				pIcons->pDefaultIconDirectory[j+1] = gtk_icon_theme_new ();
				gtk_icon_theme_set_custom_theme (pIcons->pDefaultIconDirectory[j+1], pIcons->pDirectoryList[i]);
			}
			i ++;
			j += 2;
		}
	}

	//\___________________ Parametres des lanceurs.
	pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "launcher width", &bFlushConfFileNeeded, 48, "Launchers", "max icon size");
	
	pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "launcher height", &bFlushConfFileNeeded, 48, "Launchers", "max icon size");
		
	gchar *cLauncherBackgroundImageName = cairo_dock_get_string_key_value (pKeyFile, "Icons", "icons bg", &bFlushConfFileNeeded, NULL, NULL, NULL);
	if (cLauncherBackgroundImageName != NULL)
	{
		pIcons->cBackgroundImagePath = cairo_dock_generate_file_path (cLauncherBackgroundImageName);
		g_free (cLauncherBackgroundImageName);
		pIcons->bBgForApplets = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "bg for applets", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	}

	//\___________________ Parametres des applis.
	pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "appli width", &bFlushConfFileNeeded, 48, "Applications", "max icon size");

	pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "appli height", &bFlushConfFileNeeded, 48, "Applications", "max icon size");
	
	//\___________________ Parametres des applets.
	pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "applet width", &bFlushConfFileNeeded, 48, "Applets", "max icon size");

	pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "applet height", &bFlushConfFileNeeded, 48, "Applets", "max icon size");
	
	//\___________________ Parametres des separateurs.
	pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "separator width", &bFlushConfFileNeeded, 48, "Separators", "min icon size");
	pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR23] = pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12];

	pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "separator height", &bFlushConfFileNeeded, 48, "Separators", "min icon size");
	pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR23] = pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12];

	pIcons->bUseSeparator = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "use separator", &bFlushConfFileNeeded, TRUE, "Separators", NULL);

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
		for (i = 0; (pIcons->pDefaultIconDirectory[2*i] != NULL || pIcons->pDefaultIconDirectory[2*i+1] != NULL); i ++)
		{
			if (pIcons->pDefaultIconDirectory[2*i] != NULL)
				g_free (pIcons->pDefaultIconDirectory[2*i]);
			else
				g_object_unref (pIcons->pDefaultIconDirectory[2*i+1]);
		}
	}
	g_free (pIcons->cSeparatorImage);
	g_free (pIcons->cBackgroundImagePath);
	g_strfreev (pIcons->pDirectoryList);
}


static void reload (CairoConfigIcons *pPrevIcons, CairoConfigIcons *pIcons)
{
	CairoDock *pDock = g_pMainDock;
	double fMaxScale = cairo_dock_get_max_scale (pDock);
	cairo_t* pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	
	gboolean bGroupOrderChanged;
	if (pPrevIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER] != pIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconTypeOrder[CAIRO_DOCK_APPLI] != pIcons->tIconTypeOrder[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconTypeOrder[CAIRO_DOCK_APPLET] != pIcons->tIconTypeOrder[CAIRO_DOCK_APPLET])
		bGroupOrderChanged = TRUE;
	else
		bGroupOrderChanged = FALSE;
	
	if (bGroupOrderChanged || pPrevIcons->bMixAppletsAndLaunchers != pIcons->bMixAppletsAndLaunchers)
		pDock->icons = g_list_sort (pDock->icons, (GCompareFunc) cairo_dock_compare_icons_order);

	if ((pPrevIcons->bUseSeparator && ! pIcons->bUseSeparator) ||
		(! pPrevIcons->bMixAppletsAndLaunchers && pIcons->bMixAppletsAndLaunchers) ||
		pPrevIcons->cSeparatorImage != pIcons->cSeparatorImage ||
		bGroupOrderChanged)
		cairo_dock_remove_all_separators (pDock);
	
	
	int i = 0;
	while (pIcons->pDirectoryList[i] != NULL && pPrevIcons->pDirectoryList[i] != NULL)
	{
		if (strcmp (pPrevIcons->pDirectoryList[i], pIcons->pDirectoryList[i])== 0)
			break ;
		i ++;
	}

	gboolean bIconBackgroundImagesChanged = FALSE;
	// if background images are different, reload them and trigger the reload of all icons
	if (cairo_dock_strings_differ (pPrevIcons->cBackgroundImagePath, pIcons->cBackgroundImagePath))
	{
		bIconBackgroundImagesChanged = TRUE;
		cairo_dock_load_icons_background_surface (pIcons->cBackgroundImagePath, pCairoContext, fMaxScale);
	}
	if (pPrevIcons->bBgForApplets != pIcons->bBgForApplets && pIcons->cBackgroundImagePath != NULL)
	{
		bIconBackgroundImagesChanged = TRUE;  // on pourrait faire plus fin en ne rechargeant que les applets mais bon.
	}
	
	if (pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude ||
		(!g_bUseOpenGL && pPrevIcons->fFieldDepth != pIcons->fFieldDepth) ||
		(!g_bUseOpenGL && pPrevIcons->fAlbedo != pIcons->fAlbedo) ||
		pIcons->pDirectoryList[i] != NULL ||
		pPrevIcons->pDirectoryList[i] != NULL ||
		bIconBackgroundImagesChanged)
	{
		g_fBackgroundImageWidth = 0.;  // pour mettre a jour les decorations.
		g_fBackgroundImageHeight = 0.;
		cairo_dock_reload_buffers_in_all_docks (TRUE);  // TRUE <=> y compris les applets.
	}
	
	if ((pIcons->bUseSeparator && ! pPrevIcons->bUseSeparator) ||
		(pPrevIcons->bMixAppletsAndLaunchers != pIcons->bMixAppletsAndLaunchers) || 
		pPrevIcons->cSeparatorImage != pIcons->cSeparatorImage ||
		bGroupOrderChanged)
	{
		cairo_dock_insert_separators_in_dock (pDock);
	}
	
	
	g_fBackgroundImageWidth = g_fBackgroundImageHeight = 0.;
	cairo_dock_set_all_views_to_default (0);  // met a jour la taille (decorations incluses) de tous les docks.
	cairo_dock_calculate_dock_icons (pDock);
	cairo_dock_redraw_root_docks (FALSE);  // main dock inclus.
	
	cairo_destroy (pCairoContext);
}


DEFINE_PRE_INIT (Icons)
{
	pModule->cModuleName = "Icons";
	pModule->cTitle = "Icons";
	pModule->cIcon = CAIRO_DOCK_SHARE_DATA_DIR"/icon-icons.svg";
	pModule->cDescription = "All about the icons.";
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

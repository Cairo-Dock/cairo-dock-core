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
#include <string.h>

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>

#include "../config.h"
#ifdef HAVE_LIBCRYPT
/* libC crypt */
#include <crypt.h>

static char DES_crypt_key[64] =
{
    1,0,0,1,1,1,0,0, 1,0,1,1,1,0,1,1, 1,1,0,1,0,1,0,1, 1,1,0,0,0,0,0,1,
    0,0,0,1,0,1,1,0, 1,1,1,0,1,1,1,0, 1,1,1,0,0,1,0,0, 1,0,1,0,1,0,1,1
}; 
#endif

#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-load.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-internal-position.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-dialogs.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-desklets.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-config.h"

CairoDockDesktopBackground *g_pFakeTransparencyDesktopBg = NULL;
gboolean g_bEasterEggs = FALSE;

extern gchar *g_cCurrentLaunchersPath;
extern gboolean g_bUseOpenGL;
extern CairoDockDesktopEnv g_iDesktopEnv;
extern CairoDockHidingEffect *g_pHidingBackend;

static gboolean s_bLoading = FALSE;


gboolean cairo_dock_get_boolean_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, gboolean bDefaultValue, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	gboolean bValue = g_key_file_get_boolean (pKeyFile, cGroupName, cKeyName, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		bValue = g_key_file_get_boolean (pKeyFile, cGroupNameUpperCase, cKeyName, &erreur);
		g_free (cGroupNameUpperCase);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			bValue = g_key_file_get_boolean (pKeyFile, "Cairo Dock", cKeyName, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				bValue = g_key_file_get_boolean (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
					bValue = bDefaultValue;
				}
				else
					cd_message (" (recuperee)");
			}
			else
				cd_message (" (recuperee)");
		}

		g_key_file_set_boolean (pKeyFile, cGroupName, cKeyName, bValue);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	return bValue;
}

int cairo_dock_get_integer_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, int iDefaultValue, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	int iValue = g_key_file_get_integer (pKeyFile, cGroupName, cKeyName, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		iValue = g_key_file_get_integer (pKeyFile, cGroupNameUpperCase, cKeyName, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			iValue = g_key_file_get_integer (pKeyFile, "Cairo Dock", cKeyName, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				iValue = g_key_file_get_integer (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
					iValue = iDefaultValue;
				}
				else
					cd_message (" (recuperee)");
			}
			else
				cd_message (" (recuperee)");
		}
		g_free (cGroupNameUpperCase);

		g_key_file_set_integer (pKeyFile, cGroupName, cKeyName, iValue);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	return iValue;
}

double cairo_dock_get_double_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, double fDefaultValue, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	double fValue = g_key_file_get_double (pKeyFile, cGroupName, cKeyName, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		fValue = g_key_file_get_double (pKeyFile, cGroupNameUpperCase, cKeyName, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			fValue = g_key_file_get_double (pKeyFile, "Cairo Dock", cKeyName, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				fValue = g_key_file_get_double (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
					fValue = fDefaultValue;
				}
				else
					cd_message (" (recuperee)");
			}
			else
				cd_message (" (recuperee)");
		}
		g_free (cGroupNameUpperCase);

		g_key_file_set_double (pKeyFile, cGroupName, cKeyName, fValue);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	return fValue;
}

gchar *cairo_dock_get_string_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultValue, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	gchar *cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		cValue = g_key_file_get_string (pKeyFile, cGroupNameUpperCase, cKeyName, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			cValue = g_key_file_get_string (pKeyFile, "Cairo Dock", cKeyName, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				cValue = g_key_file_get_string (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
					cValue = g_strdup (cDefaultValue);
				}
				else
					cd_message (" (recuperee)");
			}
			else
				cd_message (" (recuperee)");
		}
		g_free (cGroupNameUpperCase);

		g_key_file_set_string (pKeyFile, cGroupName, cKeyName, (cValue != NULL ? cValue : ""));
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	if (cValue != NULL && *cValue == '\0')
	{
		g_free (cValue);
		cValue = NULL;
	}
	return cValue;
}

void cairo_dock_get_integer_list_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, int *iValueBuffer, guint iNbElements, int *iDefaultValues, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	gsize length = 0;
	if (iDefaultValues != NULL)
		memcpy (iValueBuffer, iDefaultValues, iNbElements * sizeof (int));

	int *iValuesList = g_key_file_get_integer_list (pKeyFile, cGroupName, cKeyName, &length, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		iValuesList = g_key_file_get_integer_list (pKeyFile, cGroupNameUpperCase, cKeyName, &length, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			iValuesList = g_key_file_get_integer_list (pKeyFile, "Cairo Dock", cKeyName, &length, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				iValuesList = g_key_file_get_integer_list (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &length, &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
				}
				else
				{
					cd_message (" (recuperee)");
					if (length > 0)
						memcpy (iValueBuffer, iValuesList, MIN (iNbElements, length) * sizeof (int));
				}
			}
			else
			{
				cd_message (" (recuperee)");
				if (length > 0)
					memcpy (iValueBuffer, iValuesList, MIN (iNbElements, length) * sizeof (int));
			}
		}
		else
		{
			if (length > 0)
				memcpy (iValueBuffer, iValuesList, MIN (iNbElements, length) * sizeof (int));
		}
		g_free (cGroupNameUpperCase);
		
		if (iDefaultValues != NULL)  // on ne modifie les valeurs actuelles que si on a explicitement passe des valeurs par defaut en entree; sinon on considere que l'on va traiter le cas en aval.
			g_key_file_set_integer_list (pKeyFile, cGroupName, cKeyName, iValueBuffer, iNbElements);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	else
	{
		if (length > 0)
			memcpy (iValueBuffer, iValuesList, MIN (iNbElements, length) * sizeof (int));
	}
	g_free (iValuesList);
}

void cairo_dock_get_double_list_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, double *fValueBuffer, guint iNbElements, double *fDefaultValues, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	gsize length = 0;
	if (fDefaultValues != NULL)
		memcpy (fValueBuffer, fDefaultValues, iNbElements * sizeof (double));

	double *fValuesList = g_key_file_get_double_list (pKeyFile, cGroupName, cKeyName, &length, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		fValuesList = g_key_file_get_double_list (pKeyFile, cGroupNameUpperCase, cKeyName, &length, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			fValuesList = g_key_file_get_double_list (pKeyFile, "Cairo Dock", cKeyName, &length, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				fValuesList = g_key_file_get_double_list (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &length, &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
				}
				else
				{
					cd_message (" (recuperee)");
					if (length > 0)
						memcpy (fValueBuffer, fValuesList, MIN (iNbElements, length) * sizeof (double));
				}
			}
			else
			{
				cd_message (" (recuperee)");
				if (length > 0)
					memcpy (fValueBuffer, fValuesList, MIN (iNbElements, length) * sizeof (double));
			}
		}
		else
		{
			if (length > 0)
				memcpy (fValueBuffer, fValuesList, MIN (iNbElements, length) * sizeof (double));
		}
		g_free (cGroupNameUpperCase);

		g_key_file_set_double_list (pKeyFile, cGroupName, cKeyName, fValueBuffer, iNbElements);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	else
	{
		if (length > 0)
			memcpy (fValueBuffer, fValuesList, MIN (iNbElements, length) * sizeof (double));
	}
	g_free (fValuesList);
}

gchar **cairo_dock_get_string_list_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, gsize *length, const gchar *cDefaultValues, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	*length = 0;
	gchar **cValuesList = g_key_file_get_string_list (pKeyFile, cGroupName, cKeyName, length, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		cValuesList = g_key_file_get_string_list (pKeyFile, cGroupNameUpperCase, cKeyName, length, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			cValuesList = g_key_file_get_string_list (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), length, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				cValuesList = g_strsplit (cDefaultValues, ";", -1);  // "" -> NULL.
				int i = 0;
				if (cValuesList != NULL)
				{
					while (cValuesList[i] != NULL)
						i ++;
				}
				*length = i;
			}
		}
		g_free (cGroupNameUpperCase);

		if (*length > 0)
			g_key_file_set_string_list (pKeyFile, cGroupName, cKeyName, (const gchar **)cValuesList, *length);
		else
			g_key_file_set_string (pKeyFile, cGroupName, cKeyName, "");
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	if (cValuesList != NULL && (cValuesList[0] == NULL || (*(cValuesList[0]) == '\0' && *length == 1)))
	{
		g_strfreev (cValuesList);
		cValuesList = NULL;
		*length = 0;
	}
	return cValuesList;
}

gchar *cairo_dock_get_file_path_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName, const gchar *cDefaultDir, const gchar *cDefaultFileName)
{
	gchar *cFileName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, NULL, cDefaultGroupName, cDefaultKeyName);
	gchar *cFilePath = NULL;
	if (cFileName != NULL)
		cFilePath = cairo_dock_generate_file_path (cFileName);
	else if (cDefaultFileName != NULL && cDefaultDir != NULL)
		cFilePath = g_strdup_printf ("%s/%s", cDefaultDir, cDefaultFileName);
	return cFilePath;
}
void cairo_dock_get_size_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, gint iDefaultSize, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName, int *iWidth, int *iHeight)
{
	int iSize[2];
	int iDefaultValues[2] = {iDefaultSize, iDefaultSize};
	cairo_dock_get_integer_list_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, iSize, 2, iDefaultValues, cDefaultGroupName, cDefaultKeyName);
	*iWidth = iSize[0];
	*iHeight = iSize[1];
}


void cairo_dock_load_config (const gchar *cConfFilePath, CairoDock *pMainDock)
{
	//\___________________ On ouvre le fichier de conf.
	gboolean bFlushConfFileNeeded = FALSE;  // si un champ n'existe pas, on le rajoute au fichier de conf.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	s_bLoading = TRUE;
	
	//\___________________ On recupere la conf de tous les modules.
	bFlushConfFileNeeded = cairo_dock_get_global_config (pKeyFile);
	
	//\___________________ Post-initialisation : parametres impactant le main dock.
	pMainDock->iGapX = myPosition.iGapX;
	pMainDock->iGapY = myPosition.iGapY;
	pMainDock->fAlign = myPosition.fAlign;
	if (myPosition.bUseXinerama)
	{
		pMainDock->iNumScreen = myPosition.iNumScreen;
		cairo_dock_get_screen_offsets (myPosition.iNumScreen, &pMainDock->iScreenOffsetX, &pMainDock->iScreenOffsetY);
	}
	
	cairo_dock_set_dock_orientation (pMainDock, myPosition.iScreenBorder);
	cairo_dock_move_resize_dock (pMainDock);
	
	//\___________________ fausse transparence.
	if (mySystem.bUseFakeTransparency)
	{
		g_pFakeTransparencyDesktopBg = cairo_dock_get_desktop_background (g_bUseOpenGL);
	}
	
	//\___________________ On charge les images dont on aura besoin tout de suite.
	cairo_dock_load_icon_textures ();
	cairo_dock_load_indicator_textures ();
	
	cairo_dock_load_visible_zone (myAccessibility.cZoneImage, myAccessibility.iZoneWidth, myAccessibility.iZoneHeight, myAccessibility.fZoneAlpha);
	
	cairo_dock_create_icon_fbo ();
	
	//\___________________ On charge les lanceurs.
	pMainDock->fFlatDockWidth = - myIcons.iIconGap;  // car on ne le connaissait pas encore au moment de sa creation .
	cairo_dock_build_docks_tree_with_desktop_files (g_cCurrentLaunchersPath);
	
	cairo_dock_hide_show_launchers_on_other_desktops (pMainDock);
	
	//\___________________ On charge les applets.
	GTimeVal time_val;
	g_get_current_time (&time_val);  // on pourrait aussi utiliser un compteur statique a la fonction ...
	double fTime = time_val.tv_sec + time_val.tv_usec * 1e-6;
	cairo_dock_activate_modules_from_list (mySystem.cActiveModuleList, fTime);
	
	//\___________________ On lance la barre des taches.
	if (myTaskBar.bShowAppli)
	{
		cairo_dock_start_application_manager (pMainDock);  // va inserer le separateur si necessaire.
	}
	
	//\___________________ On dessine tout.
	cairo_dock_draw_subdock_icons ();
	
	cairo_dock_set_all_views_to_default (0);  // met a jour la taille de tous les docks, maintenant qu'ils sont tous remplis.
	cairo_dock_redraw_root_docks (FALSE);  // FALSE <=> main dock inclus.
	
	//\___________________ On charge le comportement d'accessibilite.
	g_pHidingBackend = cairo_dock_get_hiding_effect (myAccessibility.cHideEffect);
	
	cairo_dock_set_dock_visibility (pMainDock, myAccessibility.iVisibility);
	
	//\___________________ On charge les decorations des desklets.
	if (myDesklets.cDeskletDecorationsName != NULL)  // chargement initial, on charge juste ceux qui n'ont pas encore leur deco et qui ont atteint leur taille definitive.
	{
		cairo_dock_reload_desklets_decorations (FALSE);
	}
	
	//\___________________ On charge les listes de backends.
	cairo_dock_update_renderer_list_for_gui ();
	cairo_dock_update_desklet_decorations_list_for_gui ();
	cairo_dock_update_desklet_decorations_list_for_applet_gui ();
	cairo_dock_update_animations_list_for_gui ();
	cairo_dock_update_dialog_decorator_list_for_gui ();
	
	//\___________________ On met a jour le fichier sur le disque si necessaire.
	if (! bFlushConfFileNeeded)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, CAIRO_DOCK_VERSION);
	if (bFlushConfFileNeeded)
	{
		cairo_dock_flush_conf_file (pKeyFile, cConfFilePath, CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_CONF_FILE);
	}
	
	g_key_file_free (pKeyFile);

	s_bLoading = FALSE;
	
	cairo_dock_trigger_refresh_launcher_gui ();
}

void cairo_dock_read_conf_file (const gchar *cConfFilePath, CairoDock *pDock)
{
	//g_print ("%s (%s)\n", __func__, cConfFilePath);
	GError *erreur = NULL;
	gsize length;
	gboolean bFlushConfFileNeeded = FALSE, bFlushConfFileNeeded2 = FALSE;  // si un champ n'existe pas, on le rajoute au fichier de conf.

	//\___________________ On ouvre le fichier de conf.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	if (pKeyFile == NULL)
		return ;
	
	s_bLoading = TRUE;
	
	//\___________________ On garde une trace de certains parametres.
	gchar *cRaiseDockShortcutOld = myAccessibility.cRaiseDockShortcut;
	myAccessibility.cRaiseDockShortcut = NULL;
	///gboolean bPopUpOld = myAccessibility.bPopUp;  // FALSE initialement.
	gboolean bUseFakeTransparencyOld = mySystem.bUseFakeTransparency;  // FALSE initialement.
	gboolean bGroupAppliByClassOld = myTaskBar.bGroupAppliByClass;  // FALSE initialement.
	gboolean bHideVisibleApplisOld = myTaskBar.bHideVisibleApplis;
	gboolean bAppliOnCurrentDesktopOnlyOld = myTaskBar.bAppliOnCurrentDesktopOnly;
	gboolean bMixLauncherAppliOld = myTaskBar.bMixLauncherAppli;
	gboolean bOverWriteXIconsOld = myTaskBar.bOverWriteXIcons;  // TRUE initialement.
	gint iMinimizedWindowRenderTypeOld = myTaskBar.iMinimizedWindowRenderType;
	gchar *cDeskletDecorationsNameOld = myDesklets.cDeskletDecorationsName;
	myDesklets.cDeskletDecorationsName = NULL;
	int iSeparateIconsOld = myIcons.iSeparateIcons;
	CairoDockIconType tIconTypeOrderOld[CAIRO_DOCK_NB_TYPES];
	memcpy (tIconTypeOrderOld, myIcons.tIconTypeOrder, sizeof (tIconTypeOrderOld));
	
	//\___________________ On recupere la conf de tous les modules.
	bFlushConfFileNeeded = cairo_dock_get_global_config (pKeyFile);
	
	//\___________________ Post-initialisation : parametres impactant le main dock.
	pDock->iGapX = myPosition.iGapX;
	pDock->iGapY = myPosition.iGapY;
	
	pDock->fAlign = myPosition.fAlign;
	
	gboolean bGroupOrderChanged;
	if (tIconTypeOrderOld[CAIRO_DOCK_LAUNCHER] != myIcons.tIconTypeOrder[CAIRO_DOCK_LAUNCHER] ||
		tIconTypeOrderOld[CAIRO_DOCK_APPLI] != myIcons.tIconTypeOrder[CAIRO_DOCK_APPLI] ||
		tIconTypeOrderOld[CAIRO_DOCK_APPLET] != myIcons.tIconTypeOrder[CAIRO_DOCK_APPLET])
		bGroupOrderChanged = TRUE;
	else
		bGroupOrderChanged = FALSE;
	
	if (myPosition.bUseXinerama)
	{
		pDock->iNumScreen = myPosition.iNumScreen;
		cairo_dock_get_screen_offsets (myPosition.iNumScreen, &pDock->iScreenOffsetX, &pDock->iScreenOffsetY);
	}
	else
	{
		pDock->iNumScreen = pDock->iScreenOffsetX = pDock->iScreenOffsetY = 0;
	}
	
	switch (myPosition.iScreenBorder)
	{
		case CAIRO_DOCK_BOTTOM :
		default :
			pDock->container.bIsHorizontal = CAIRO_DOCK_HORIZONTAL;
			pDock->container.bDirectionUp = TRUE;
		break;
		case CAIRO_DOCK_TOP :
			pDock->container.bIsHorizontal = CAIRO_DOCK_HORIZONTAL;
			pDock->container.bDirectionUp = FALSE;
		break;
		case CAIRO_DOCK_RIGHT :
			pDock->container.bIsHorizontal = CAIRO_DOCK_VERTICAL;
			pDock->container.bDirectionUp = TRUE;
		break;
		case CAIRO_DOCK_LEFT :
			pDock->container.bIsHorizontal = CAIRO_DOCK_VERTICAL;
			pDock->container.bDirectionUp = FALSE;
		break;
	}
	
	//\___________________ On (re)charge tout, car n'importe quel parametre peut avoir change.
	// fausse transparence.
	if (mySystem.bUseFakeTransparency)
	{
		if (g_pFakeTransparencyDesktopBg == NULL)
		{
			g_pFakeTransparencyDesktopBg = cairo_dock_get_desktop_background (g_bUseOpenGL);
		}
		else if (g_bUseOpenGL)
		{
			CairoDockDesktopBackground *dummy = cairo_dock_get_desktop_background (g_bUseOpenGL);  // pour recharger la texture.
			cairo_dock_destroy_desktop_background (dummy);
		}
	}
	else if (g_pFakeTransparencyDesktopBg != NULL)
	{
		cairo_dock_destroy_desktop_background (g_pFakeTransparencyDesktopBg);
		g_pFakeTransparencyDesktopBg = NULL;
	}
	
	//\___________________ on recharge les buffers d'images.
	cairo_dock_unload_dialog_buttons ();  // on se contente de remettre a zero ces buffers,
	cairo_dock_unload_desklet_buttons ();  // qui seront charges lorsque necessaire.
	
	cairo_dock_load_icon_textures ();
	cairo_dock_load_indicator_textures ();
	
	cairo_dock_load_visible_zone (myAccessibility.cZoneImage, myAccessibility.iZoneWidth, myAccessibility.iZoneHeight, myAccessibility.fZoneAlpha);
	
	///cairo_dock_create_icon_pbuffer ();
	cairo_dock_create_icon_fbo ();
	
	//\___________________ On recharge les lanceurs, les applis, et les applets.
	if (bGroupAppliByClassOld != myTaskBar.bGroupAppliByClass ||
		bHideVisibleApplisOld != myTaskBar.bHideVisibleApplis ||
		bAppliOnCurrentDesktopOnlyOld != myTaskBar.bAppliOnCurrentDesktopOnly ||
		bMixLauncherAppliOld != myTaskBar.bMixLauncherAppli ||
		bOverWriteXIconsOld != myTaskBar.bOverWriteXIcons ||
		iMinimizedWindowRenderTypeOld != myTaskBar.iMinimizedWindowRenderType ||
		(cairo_dock_application_manager_is_running () && ! myTaskBar.bShowAppli))  // on ne veut plus voir les applis, il faut donc les enlever.
	{
		cairo_dock_stop_application_manager ();
	}
	
	if (bGroupOrderChanged || myIcons.iSeparateIcons != iSeparateIconsOld)
		pDock->icons = g_list_sort (pDock->icons, (GCompareFunc) cairo_dock_compare_icons_order);
	
	if ((iSeparateIconsOld && ! myIcons.iSeparateIcons) || bGroupOrderChanged)
		cairo_dock_remove_automatic_separators (pDock);
		
	if (pDock->icons == NULL)
	{
		pDock->fFlatDockWidth = - myIcons.iIconGap;  // car on ne le connaissait pas encore au moment de la creation du dock.
		cairo_dock_build_docks_tree_with_desktop_files (g_cCurrentLaunchersPath);
	}
	else
	{
		cairo_dock_synchronize_sub_docks_orientation (pDock, FALSE);
		cairo_dock_reload_buffers_in_all_docks (FALSE);  // tout sauf les applets, qui seront rechargees en bloc juste apres.
	}
	
	GTimeVal time_val;
	g_get_current_time (&time_val);  // on pourrait aussi utiliser un compteur statique a la fonction ...
	double fTime = time_val.tv_sec + time_val.tv_usec * 1e-6;
	cairo_dock_activate_modules_from_list (mySystem.cActiveModuleList, fTime);
	cairo_dock_deactivate_old_modules (fTime);
	
	if (! cairo_dock_application_manager_is_running () && myTaskBar.bShowAppli)  // maintenant on veut voir les applis !
	{
		cairo_dock_start_application_manager (pDock);  // va inserer le separateur si necessaire.
	}

	if (myIcons.iSeparateIcons && (! iSeparateIconsOld || bGroupOrderChanged))
	{
		cairo_dock_insert_separators_in_dock (pDock);
	}
	
	cairo_dock_draw_subdock_icons ();
	
	cairo_dock_set_all_views_to_default (0);  // met a jour la taille de tous les docks, maintenant qu'ils sont tous remplis.
	cairo_dock_redraw_root_docks (TRUE);  // TRUE <=> sauf le main dock.
	
	if (myAccessibility.cRaiseDockShortcut != NULL)
	{
		if (cRaiseDockShortcutOld == NULL || strcmp (myAccessibility.cRaiseDockShortcut, cRaiseDockShortcutOld) != 0)
		{
			if (cRaiseDockShortcutOld != NULL)
				cd_keybinder_unbind (cRaiseDockShortcutOld, (CDBindkeyHandler) cairo_dock_raise_from_shortcut);
			if (! cd_keybinder_bind (myAccessibility.cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_shortcut, NULL))
			{
				g_free (myAccessibility.cRaiseDockShortcut);
				myAccessibility.cRaiseDockShortcut = NULL;
			}
		}
	}
	else
	{
		if (cRaiseDockShortcutOld != NULL)
		{
			cd_keybinder_unbind (cRaiseDockShortcutOld, (CDBindkeyHandler) cairo_dock_raise_from_shortcut);
			cairo_dock_move_resize_dock (pDock);
			gtk_widget_show (pDock->container.pWidget);
		}
	}
	g_free (cRaiseDockShortcutOld);
	
	//\___________________ On gere le changement dans la visibilite.
	g_pHidingBackend = cairo_dock_get_hiding_effect (myAccessibility.cHideEffect);
	
	if (pDock->bAutoHide)
	{
		pDock->iInputState = CAIRO_DOCK_INPUT_HIDDEN;  // le 'configure' mettra a jour la zone d'input.
		pDock->fHideOffset = 1.;
	}
	else
	{
		cairo_dock_start_showing (pDock);
	}
	
	cairo_dock_hide_show_launchers_on_other_desktops (pDock);
	
	cairo_dock_set_dock_visibility (pDock, myAccessibility.iVisibility);
	
	///cairo_dock_load_background_decorations (pDock);

	cairo_dock_move_resize_dock (pDock);
	
	pDock->container.iMouseX = 0;  // on se place hors du dock initialement.
	pDock->container.iMouseY = 0;
	cairo_dock_calculate_dock_icons (pDock);
	gtk_widget_queue_draw (pDock->container.pWidget);  // le 'gdk_window_move_resize' ci-dessous ne provoquera pas le redessin si la taille n'a pas change.
	
	//\___________________ On recharge les decorations des desklets.
	if (cDeskletDecorationsNameOld == NULL && myDesklets.cDeskletDecorationsName != NULL)  // chargement initial, on charge juste ceux qui n'ont pas encore leur deco et qui ont atteint leur taille definitive.
	{
		cairo_dock_reload_desklets_decorations (FALSE);
	}
	else if (cDeskletDecorationsNameOld != NULL && (myDesklets.cDeskletDecorationsName == NULL || strcmp (cDeskletDecorationsNameOld, myDesklets.cDeskletDecorationsName) != 0))  // le theme par defaut a change, on recharge les desklets qui utilisent le theme "default".
	{
		cairo_dock_reload_desklets_decorations (TRUE);
	}
	else if (myDesklets.cDeskletDecorationsName != NULL && strcmp (myDesklets.cDeskletDecorationsName, "personnal") == 0)  // on a configure le theme personnel, il peut avoir change.
	{
		cairo_dock_reload_desklets_decorations (TRUE);
	}
	else  // on charge juste ceux qui n'ont pas encore leur deco et qui ont atteint leur taille definitive.
	{
		cairo_dock_reload_desklets_decorations (FALSE);
	}
	g_free (cDeskletDecorationsNameOld);
	
	cairo_dock_update_renderer_list_for_gui ();
	cairo_dock_update_desklet_decorations_list_for_gui ();
	cairo_dock_update_desklet_decorations_list_for_applet_gui ();
	cairo_dock_update_animations_list_for_gui ();
	cairo_dock_update_dialog_decorator_list_for_gui ();
	
	//\___________________ On ecrit sur le disque si necessaire.
	if (! bFlushConfFileNeeded)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, CAIRO_DOCK_VERSION);
	if (bFlushConfFileNeeded)
	{
		cairo_dock_flush_conf_file (pKeyFile, cConfFilePath, CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_CONF_FILE);
	}
	
	g_key_file_free (pKeyFile);

	s_bLoading = FALSE;
	
	cairo_dock_trigger_refresh_launcher_gui ();
}

gboolean cairo_dock_is_loading (void)
{
	return s_bLoading;
}


void cairo_dock_update_conf_file (const gchar *cConfFilePath, GType iFirstDataType, ...)  // type, groupe, cle, valeur, etc. finir par G_TYPE_INVALID.
{
	cd_message ("%s (%s)", __func__, cConfFilePath);
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	va_list args;
	va_start (args, iFirstDataType);
	
	GType iType = iFirstDataType;
	gboolean bValue;
	gint iValue;
	double fValue;
	gchar *cValue;
	gchar *cGroupName, *cGroupKey;
	while (iType != G_TYPE_INVALID)
	{
		cGroupName = va_arg (args, gchar *);
		cGroupKey = va_arg (args, gchar *);

		switch (iType)
		{
			case G_TYPE_BOOLEAN :
				bValue = va_arg (args, gboolean);
				g_key_file_set_boolean (pKeyFile, cGroupName, cGroupKey, bValue);
			break ;
			case G_TYPE_INT :
				iValue = va_arg (args, gint);
				g_key_file_set_integer (pKeyFile, cGroupName, cGroupKey, iValue);
			break ;
			case G_TYPE_DOUBLE :
				fValue = va_arg (args, gdouble);
				g_key_file_set_double (pKeyFile, cGroupName, cGroupKey, fValue);
			break ;
			case G_TYPE_STRING :
				cValue = va_arg (args, gchar *);
				g_key_file_set_string (pKeyFile, cGroupName, cGroupKey, cValue);
			break ;
			default :
			break ;
		}

		iType = va_arg (args, GType);
	}

	cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	g_key_file_free (pKeyFile);

	va_end (args);
}


void cairo_dock_update_conf_file_with_position (const gchar *cConfFilePath, int x, int y)
{
	//g_print ("%s (%s ; %d;%d)\n", __func__, cConfFilePath, x, y);
	cairo_dock_update_conf_file (cConfFilePath,
		G_TYPE_INT, "Position", "x gap", x,
		G_TYPE_INT, "Position", "y gap", y,
		G_TYPE_INVALID);
}


void cairo_dock_get_version_from_string (const gchar *cVersionString, int *iMajorVersion, int *iMinorVersion, int *iMicroVersion)
{
	gchar **cVersions = g_strsplit (cVersionString, ".", -1);
	if (cVersions[0] != NULL)
	{
		*iMajorVersion = atoi (cVersions[0]);
		if (cVersions[1] != NULL)
		{
			*iMinorVersion = atoi (cVersions[1]);
			if (cVersions[2] != NULL)
				*iMicroVersion = atoi (cVersions[2]);
		}
	}
	g_strfreev (cVersions);
}


void cairo_dock_decrypt_string( const gchar *cEncryptedString,  gchar **cDecryptedString )
{
	g_return_if_fail (cDecryptedString != NULL);
	if( !cEncryptedString || *cEncryptedString == '\0' )
	{
		*cDecryptedString = g_strdup( "" );
		return;
	}
#ifdef HAVE_LIBCRYPT
	guchar *input = g_strdup(cEncryptedString);
	guchar *shifted_input = input;
	guchar **output = (guchar **)cDecryptedString;
	
	guchar *current_output = NULL;
	
	*output = g_malloc( (strlen(input)+1)/3+1 );
	current_output = *output;

  guchar *last_char_in_input = input + strlen(input);
//  g_print( "Password (before decrypt): %s\n", input );

  for( ; shifted_input < last_char_in_input; shifted_input += 16+8, current_output += 8 )
  {
    guint block[8];
    guchar txt[64];
    gint i = 0, j = 0;
    guchar current_letter = 0;
    
    memset( txt, 0, 64 );

    shifted_input[16+8-1] = 0; // cut the string

    sscanf( shifted_input, "%X-%X-%X-%X-%X-%X-%X-%X",
    &block[0], &block[1], &block[2], &block[3], &block[4], &block[5], &block[6], &block[7] );

    // process the eight first characters of "input"
    for( i = 0; i < 8 ; i++ )
      for ( j = 0; j < 8; j++ )
        txt[i*8+j] = block[i] >> j & 1;
    
    setkey( DES_crypt_key );
    encrypt( txt, 1 );  // decrypt

    for ( i = 0; i < 8; i++ )
    {
      current_output[i] = 0;
      for ( j = 0; j < 8; j++ )
      {
        current_output[i] |= txt[i*8+j] << j;
      }
    }
  }

  *current_output = 0;

//  g_print( "Password (after decrypt): %s\n", *output );

	g_free( input );

#else
	*cDecryptedString = g_strdup( cEncryptedString );
#endif
}

void cairo_dock_encrypt_string( const gchar *cDecryptedString,  gchar **cEncryptedString )
{
	g_return_if_fail (cEncryptedString != NULL);
	if( !cDecryptedString || *cDecryptedString == '\0' )
	{
		*cEncryptedString = g_strdup( "" );
		return;
	}
	
#ifdef HAVE_LIBCRYPT
	const guchar *input = (guchar *)cDecryptedString;
	guchar **output = (guchar **)cEncryptedString;
	guint input_length = 0;
	
	guchar *current_output = NULL;
	// for each block of 8 characters, we need 24 bytes.
	guint nbBlocks = strlen(input)/8+1;
	*output = g_malloc( nbBlocks*24+1 );
	current_output = *output;

  const guchar *last_char_in_input = input + strlen(input);

//  g_print( "Password (before encrypt): %s\n", input );

  for( ; input < last_char_in_input; input += 8, current_output += 16+8 )
  {
    guchar txt[64];
    guint i = 0, j = 0;
    guchar current_letter = 0;
    
    memset( txt, 0, 64 );
    
    // process the eight first characters of "input"
    for( i = 0; i < strlen(input) && i < 8 ; i++ )
      for ( j = 0; j < 8; j++ )
        txt[i*8+j] = input[i] >> j & 1;
    
    setkey( DES_crypt_key );
    encrypt( txt, 0 );  // encrypt

    for ( i = 0; i < 8; i++ )
    {
      current_letter = 0;
      for ( j = 0; j < 8; j++ )
      {
        current_letter |= txt[i*8+j] << j;
      }
      snprintf( current_output + i*3, 4, "%02X-", current_letter );
    }
  }

  *(current_output-1) = 0;

//  g_print( "Password (after encrypt): %s\n", *output );
#else
	*cEncryptedString = g_strdup( cDecryptedString );
#endif
}


xmlDocPtr cairo_dock_open_xml_file (const gchar *cDataFilePath, const gchar *cRootNodeName, xmlNodePtr *root_node, GError **erreur)
{
	if (cairo_dock_get_file_size (cDataFilePath) == 0)
	{
		g_set_error (erreur, 1, 1, "file '%s' doesn't exist or is empty", cDataFilePath);
		*root_node = NULL;
		return NULL;
	}
	xmlInitParser ();
	
	xmlDocPtr doc = xmlParseFile (cDataFilePath);
	if (doc == NULL)
	{
		g_set_error (erreur, 1, 1, "file '%s' is incorrect", cDataFilePath);
		*root_node = NULL;
		return NULL;
	}
	
	xmlNodePtr noeud = xmlDocGetRootElement (doc);
	if (noeud == NULL || xmlStrcmp (noeud->name, (const xmlChar *) cRootNodeName) != 0)
	{
		g_set_error (erreur, 1, 2, "xml file '%s' is not well formed", cDataFilePath);
		*root_node = NULL;
		return doc;
	}
	*root_node = noeud;
	return doc;
}

void cairo_dock_close_xml_file (xmlDocPtr doc)
{
	if (doc)
		xmlFreeDoc (doc);  // ne pas utiliser xmlCleanupParser(), cela peut affecter les autres threads utilisant libxml !
}



gchar *cairo_dock_get_default_system_font (void)
{
	static gchar *s_cFontName = NULL;
	if (s_cFontName == NULL)
	{
		if (g_iDesktopEnv == CAIRO_DOCK_GNOME)
			s_cFontName = cairo_dock_launch_command_sync ("gconftool-2 -g /desktop/gnome/interface/font_name");  /// ou document_font_name ?...
		else
			s_cFontName = g_strdup ("Sans 10");
	}
	return g_strdup (s_cFontName);
}

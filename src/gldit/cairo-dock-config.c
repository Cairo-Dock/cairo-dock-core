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
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-icon-manager.h"  // cairo_dock_hide_show_launchers_on_other_desktops
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-module-factory.h"
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
#include "cairo-dock-container.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-core.h"
#include "cairo-dock-config.h"

gboolean g_bEasterEggs = FALSE;

extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cConfFile;
extern gboolean g_bUseOpenGL;
extern CairoDockDesktopEnv g_iDesktopEnv;
//extern CairoDockHidingEffect *g_pHidingBackend;
//extern CairoDockHidingEffect *g_pKeepingBelowBackend;
extern CairoDockDesktopBackground *g_pFakeTransparencyDesktopBg;

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
		cFilePath = cairo_dock_search_image_s_path (cFileName);
	if (cFilePath == NULL && cDefaultFileName != NULL && cDefaultDir != NULL)  // pas d'image specifiee, ou image introuvable => on prend l'image par defaut fournie.
		cFilePath = g_strdup_printf ("%s/%s", cDefaultDir, cDefaultFileName);
	g_free (cFileName);
	return cFilePath;
}

gchar *cairo_dock_get_icon_path_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName, const gchar *cDefaultDir, const gchar *cDefaultFileName)
{
	gchar *cFileName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, NULL, cDefaultGroupName, cDefaultKeyName);
	gchar *cFilePath = NULL;
	if (cFileName != NULL)
	{
		cFilePath = cairo_dock_search_image_s_path (cFileName);
		if (cFilePath == NULL)  // pas trouve, on cherche une icone
		{
			if (*cFileName != '/' && *cFileName != '~')  // pas un chemin, donc possibilite de trouver autre chose en cherchant dans les icones.
				cFilePath = cairo_dock_search_icon_s_path (cFileName);
		}
	}
	if (cFilePath == NULL && cDefaultFileName != NULL && cDefaultDir != NULL)  // pas d'image specifiee, ou image introuvable => on prend l'image par defaut fournie.
		cFilePath = g_strdup_printf ("%s/%s", cDefaultDir, cDefaultFileName);
	g_free (cFileName);
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


void cairo_dock_load_current_theme (void)
{
	cd_message ("%s ()", __func__);
	s_bLoading = TRUE;
	
	//\___________________ Free everything.
	static gboolean bFirstCall = TRUE;
	if (!bFirstCall)
	{
		gldi_free_all ();
		bFirstCall = FALSE;
	}
	
	//\___________________ Get all managers config.
	gldi_get_managers_config (g_cConfFile, GLDI_VERSION);  /// en fait, CAIRO_DOCK_VERSION ...
	
	//\___________________ Create the primary container (needed to have a cairo/opengl context).
	CairoDock *pMainDock = cairo_dock_create_dock (CAIRO_DOCK_MAIN_DOCK_NAME, NULL);  // on ne lui assigne pas de vues, puisque la vue par defaut des docks principaux sera definie plus tard.
	
	//\___________________ Load all managers data.
	gldi_load_managers ();
	
	//\___________________ Now load the launchers.
	cairo_dock_build_docks_tree_with_desktop_files (g_cCurrentLaunchersPath);
	
	cairo_dock_hide_show_launchers_on_other_desktops (pMainDock);
	
	//\___________________ Load the applets.
	cairo_dock_activate_modules_from_list (myModulesParam.cActiveModuleList);
	
	//\___________________ Start the applications manager (will load the icons if the option is enabled).
	cairo_dock_start_applications_manager (pMainDock);
	
	//\___________________ Draw everything.
	cairo_dock_draw_subdock_icons ();  // maintenant que les sous-docks sont tous definis.
	
	cairo_dock_set_all_views_to_default (0);  // met a jour la taille de tous les docks, maintenant qu'ils sont tous remplis.
	cairo_dock_redraw_root_docks (FALSE);  // FALSE <=> main dock inclus.
	
	//\___________________ On charge les decorations des desklets.
	/**if (myDeskletsParam.cDeskletDecorationsName != NULL)  // chargement initial, on charge juste ceux qui n'ont pas encore leur deco et qui ont atteint leur taille definitive.
	{
		cairo_dock_reload_desklets_decorations (FALSE);
	}*/
	
	s_bLoading = FALSE;
	
	///cairo_dock_trigger_refresh_launcher_gui (); // a faire par l'app
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

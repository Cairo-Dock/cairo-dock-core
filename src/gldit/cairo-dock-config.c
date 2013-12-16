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

#include "gldi-config.h"

#ifdef HAVE_LIBCRYPT
#ifdef __FreeBSD__
#include <unistd.h>  // on BSD, there is no crypt.h
#else
#include <crypt.h>
#endif
static char DES_crypt_key[64] =
{
    1,0,0,1,1,1,0,0, 1,0,1,1,1,0,1,1, 1,1,0,1,0,1,0,1, 1,1,0,0,0,0,0,1,
    0,0,0,1,0,1,1,0, 1,1,1,0,1,1,1,0, 1,1,1,0,0,1,0,0, 1,0,1,0,1,0,1,1
}; 
#endif

#include "cairo-dock-log.h"
#include "cairo-dock-icon-manager.h"  // cairo_dock_hide_show_launchers_on_other_desktops
#include "cairo-dock-applications-manager.h"  // cairo_dock_start_applications_manager
#include "cairo-dock-module-manager.h"  // gldi_modules_activate_from_list
#include "cairo-dock-themes-manager.h"  // cairo_dock_update_conf_file
#include "cairo-dock-dock-factory.h"  // gldi_dock_new
#include "cairo-dock-file-manager.h"  // cairo_dock_get_file_size
#include "cairo-dock-user-icon-manager.h"  // gldi_user_icons_new_from_directory
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_sync
#include "cairo-dock-core.h"  // gldi_free_all
#include "cairo-dock-config.h"

gboolean g_bEasterEggs = FALSE;

extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cConfFile;
extern gboolean g_bUseOpenGL;
extern CairoDockDesktopEnv g_iDesktopEnv;

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
	gldi_free_all ();  // do nothing if there is nothing to unload.
		
	//\___________________ Get all managers config.
	gldi_managers_get_config (g_cConfFile, GLDI_VERSION);  /// en fait, CAIRO_DOCK_VERSION ...
	
	//\___________________ Create the primary container (needed to have a cairo/opengl context).
	CairoDock *pMainDock = gldi_dock_new (CAIRO_DOCK_MAIN_DOCK_NAME);
	
	//\___________________ Load all managers data.
	gldi_managers_load ();
	gldi_modules_activate_from_list (NULL);  // load auto-loaded modules before loading anything (views, etc)
	
	//\___________________ Now load the user icons (launchers, etc).
	gldi_user_icons_new_from_directory (g_cCurrentLaunchersPath);
	
	cairo_dock_hide_show_launchers_on_other_desktops ();
	
	//\___________________ Load the applets.
	gldi_modules_activate_from_list (myModulesParam.cActiveModuleList);
	
	//\___________________ Start the applications manager (will load the icons if the option is enabled).
	cairo_dock_start_applications_manager (pMainDock);
	
	s_bLoading = FALSE;
}


gboolean cairo_dock_is_loading (void)
{
	return s_bLoading;
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
	guchar *input = (guchar *)g_strdup (cEncryptedString);
	guchar *shifted_input = input;
	guchar **output = (guchar **)cDecryptedString;
	
	guchar *current_output = NULL;
	
	*output = g_malloc( (strlen((char *)input)+1)/3+1 );
	current_output = *output;

  guchar *last_char_in_input = input + strlen((char *)input);
//  g_print( "Password (before decrypt): %s\n", input );

  for( ; shifted_input < last_char_in_input; shifted_input += 16+8, current_output += 8 )
  {
    guint block[8];
    guchar txt[64];
    gint i = 0, j = 0;
    
    memset( txt, 0, 64 );

    shifted_input[16+8-1] = 0; // cut the string

    sscanf( (char *)shifted_input, "%X-%X-%X-%X-%X-%X-%X-%X",
    &block[0], &block[1], &block[2], &block[3], &block[4], &block[5], &block[6], &block[7] );

    // process the eight first characters of "input"
    for( i = 0; i < 8 ; i++ )
      for ( j = 0; j < 8; j++ )
        txt[i*8+j] = block[i] >> j & 1;
    
    setkey( DES_crypt_key );
    encrypt( (gchar *)txt, 1 );  // decrypt

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
	
	guchar *current_output = NULL;
	// for each block of 8 characters, we need 24 bytes.
	guint nbBlocks = strlen((char *)input)/8+1;
	*output = g_malloc( nbBlocks*24+1 );
	current_output = *output;

  const guchar *last_char_in_input = input + strlen((char *)input);

//  g_print( "Password (before encrypt): %s\n", input );

  for( ; input < last_char_in_input; input += 8, current_output += 16+8 )
  {
    guchar txt[64];
    guint i = 0, j = 0;
    guchar current_letter = 0;
    
    memset( txt, 0, 64 );
    
    // process the eight first characters of "input"
    for( i = 0; i < strlen((char *)input) && i < 8 ; i++ )
      for ( j = 0; j < 8; j++ )
        txt[i*8+j] = input[i] >> j & 1;
    
    setkey( DES_crypt_key );
    encrypt( (char *)txt, 0 );  // encrypt

    for ( i = 0; i < 8; i++ )
    {
      current_letter = 0;
      for ( j = 0; j < 8; j++ )
      {
        current_letter |= txt[i*8+j] << j;
      }
      snprintf( (char *)current_output + i*3, 4, "%02X-", current_letter );
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
		{
			s_cFontName = cairo_dock_launch_command_sync ("gconftool-2 -g /desktop/gnome/interface/font_name");  // GTK2
			if (! s_cFontName)
				s_cFontName = cairo_dock_launch_command_sync ("gsettings get org.gnome.desktop.interface font-name");  // GTK3
		}
		if (! s_cFontName)
			s_cFontName = g_strdup ("Sans 10");
	}
	return g_strdup (s_cFontName);
}

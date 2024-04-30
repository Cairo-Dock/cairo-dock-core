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

#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"


GKeyFile *cairo_dock_open_key_file (const gchar *cConfFilePath)
{
	GKeyFile *pKeyFile = g_key_file_new ();
	GError *erreur = NULL;
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_debug ("while trying to load %s : %s", cConfFilePath, erreur->message);  // on ne met pas de warning car un fichier de conf peut ne pas exister la 1ere fois.
		g_error_free (erreur);
		g_key_file_free (pKeyFile);
		return NULL;
	}
	return pKeyFile;
}

void cairo_dock_write_keys_to_file_full (GKeyFile *pKeyFile, const gchar *cConfFilePath, gboolean bAllowEmpty)
{
	cd_debug ("%s (%s)", __func__, cConfFilePath);
	GError *erreur = NULL;

	gchar *cDirectory = g_path_get_dirname (cConfFilePath);
	if (! g_file_test (cDirectory, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE))
	{
		g_mkdir_with_parents (cDirectory, 7*8*8+7*8+5);
	}
	g_free (cDirectory);


	gsize length=0;
	gchar *cNewConfFileContent = g_key_file_to_data (pKeyFile, &length, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("Error while fetching data : %s", erreur->message);
		g_error_free (erreur);
		return ;
	}
	if (! bAllowEmpty) g_return_if_fail (cNewConfFileContent != NULL && *cNewConfFileContent != '\0');

	g_file_set_contents (cConfFilePath, cNewConfFileContent, length, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("Error while writing data to %s : %s", cConfFilePath, erreur->message);
		g_error_free (erreur);
		return ;
	}
	g_free (cNewConfFileContent);
}


// pOriginalKeyFile is an up-to-date key-file
// pReplacementKeyFile is an old key-file containing values we want to use
// keys are filtered by the identifier on the original key-file.
// old keys not present in pOriginalKeyFile are added
// new keys in pOriginalKeyFile not present in pReplacementKeyFile and having valid comment are removed
static void cairo_dock_merge_key_files (GKeyFile *pOriginalKeyFile, GKeyFile *pReplacementKeyFile, gchar iIdentifier)
{
	// get the groups of the remplacement key-file.
	GError *erreur = NULL;
	gsize length = 0;
	gchar **pKeyList;
	gchar **pGroupList = g_key_file_get_groups (pReplacementKeyFile, &length);
	g_return_if_fail (pGroupList != NULL);
	gchar *cGroupName, *cKeyName, *cKeyValue, *cComment;
	int i, j;
	
	for (i = 0; pGroupList[i] != NULL; i ++)
	{
		cGroupName = pGroupList[i];
		
		// get the keys of the remplacement key-file.
		length = 0;
		pKeyList = g_key_file_get_keys (pReplacementKeyFile, cGroupName, NULL, NULL);
		g_return_if_fail (pKeyList != NULL);
		
		for (j = 0; pKeyList[j] != NULL; j ++)
		{
			cKeyName = pKeyList[j];
			
			// check that the original identifier matches with the provided one.
			if (iIdentifier != 0)
			{
				if (g_key_file_has_key (pOriginalKeyFile, cGroupName, cKeyName, NULL))  // if the key doesn't exist in the original key-file, don't check the identifier, and add it to the key-file; it probably means it's an old key that will be taken care of by the applet.
				{
					cComment = g_key_file_get_comment (pOriginalKeyFile, cGroupName, cKeyName, NULL);
					if (cComment == NULL || cComment[0] == '\0' || cComment[1] != iIdentifier)
					{
						g_free (cComment);
						continue ;
					}
					g_free (cComment);
				}
			}
			
			// get the replacement value and set it to the key-file, creating it if it didn't exist (in this case, no need to add the comment, since the key will be removed again by the applet).
			cKeyValue =  g_key_file_get_string (pReplacementKeyFile, cGroupName, cKeyName, &erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
			else
			{
				if (cKeyValue && cKeyValue[strlen(cKeyValue) - 1] == '\n')
					cKeyValue[strlen(cKeyValue) - 1] = '\0';
				g_key_file_set_string (pOriginalKeyFile, cGroupName, cKeyName, (cKeyValue != NULL ? cKeyValue : ""));
			}
			g_free (cKeyValue);
		}
		g_strfreev (pKeyList);
	}
	g_strfreev (pGroupList);
	
	// remove keys from the original key-file which are not in the remplacement key-file, except hidden and persistent keys.
	pGroupList = g_key_file_get_groups (pOriginalKeyFile, &length);
	g_return_if_fail (pGroupList != NULL);
	for (i = 0; pGroupList[i] != NULL; i ++)
	{
		cGroupName = pGroupList[i];
		
		// get the keys of the original key-file.
		length = 0;
		pKeyList = g_key_file_get_keys (pOriginalKeyFile, cGroupName, NULL, NULL);
		g_return_if_fail (pKeyList != NULL);
		
		for (j = 0; pKeyList[j] != NULL; j ++)
		{
			cKeyName = pKeyList[j];
			if (! g_key_file_has_key (pReplacementKeyFile, cGroupName, cKeyName, NULL))
			{
				cComment = g_key_file_get_comment (pOriginalKeyFile, cGroupName, cKeyName, NULL);
				if (cComment != NULL && cComment[0] != '\0' && cComment[1] != '0')  // not hidden nor peristent
				{
					g_key_file_remove_comment (pOriginalKeyFile, cGroupName, cKeyName, NULL);
					g_key_file_remove_key (pOriginalKeyFile, cGroupName, cKeyName, NULL);
				}
			}
		}
		g_strfreev (pKeyList);
	}
	g_strfreev (pGroupList);
}

void cairo_dock_merge_conf_files (const gchar *cConfFilePath, gchar *cReplacementConfFilePath, gchar iIdentifier)
{
	GKeyFile *pOriginalKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_if_fail (pOriginalKeyFile != NULL);
	GKeyFile *pReplacementKeyFile = cairo_dock_open_key_file (cReplacementConfFilePath);
	g_return_if_fail (pReplacementKeyFile != NULL);
	
	cairo_dock_merge_key_files (pOriginalKeyFile, pReplacementKeyFile, iIdentifier);
	cairo_dock_write_keys_to_file (pOriginalKeyFile, cConfFilePath);
	
	g_key_file_free (pOriginalKeyFile);
	g_key_file_free (pReplacementKeyFile);
}


// update launcher key-file: use ukf keys (= template) => remove old, add news
// update applet key-file: use vkf keys (= user) if exist in template or NULL/0 comment => keep user keys.

// pValuesKeyFile is a key-file with correct values, but old comments and possibly missing or old keys.
// pUptodateKeyFile is a template key-file with default values.
// bUpdateKeys is TRUE to use up-to-date keys.
static void _cairo_dock_replace_key_values (GKeyFile *pValuesKeyFile, GKeyFile *pUptodateKeyFile, gboolean bUpdateKeys)
{
	GKeyFile *pKeysKeyFile = (bUpdateKeys ? pUptodateKeyFile : pValuesKeyFile);
	
	// get the groups.
	GError *erreur = NULL;
	gsize length = 0;
	gchar **pKeyList;
	gchar **pGroupList = g_key_file_get_groups (pKeysKeyFile, &length);
	g_return_if_fail (pGroupList != NULL);
	gchar *cGroupName, *cKeyName, *cKeyValue, *cComment;
	int i, j;
	
	for (i = 0; pGroupList[i] != NULL; i ++)
	{
		cGroupName = pGroupList[i];
		
		// get the keys.
		length = 0;
		pKeyList = g_key_file_get_keys (pKeysKeyFile, cGroupName, NULL, NULL);
		g_return_if_fail (pKeyList != NULL);
		
		for (j = 0; pKeyList[j] != NULL; j ++)
		{
			cKeyName = pKeyList[j];
			cComment = NULL;
			
			// don't add old keys, except if they are hidden or persistent.
			if (!g_key_file_has_key (pUptodateKeyFile, cGroupName, cKeyName, NULL))  // old key
			{
				cComment = g_key_file_get_comment (pValuesKeyFile, cGroupName, cKeyName, NULL);
				if (cComment != NULL && cComment[0] != '\0' && cComment[1] != '0')  // not hidden nor persistent => skip it.
				{
					g_free (cComment);
					continue;
				}
			}
			
			// get the replacement value and set it to the key-file, creating it if it didn't exist.
			cKeyValue = g_key_file_get_string (pValuesKeyFile, cGroupName, cKeyName, &erreur);
			if (erreur != NULL)  // key doesn't exist
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
			else
			{
				g_key_file_set_string (pUptodateKeyFile, cGroupName, cKeyName, (cKeyValue != NULL ? cKeyValue : ""));
				if (cComment != NULL)  // if we got the comment, it means the key doesn't exist in the up-to-date key-file, so add it.
					g_key_file_set_comment (pUptodateKeyFile, cGroupName, cKeyName, cComment, NULL);
			}
			g_free (cKeyValue);
			g_free (cComment);
		}
		
		g_strfreev (pKeyList);
	}
	g_strfreev (pGroupList);
	
}

void cairo_dock_upgrade_conf_file_full (const gchar *cConfFilePath, GKeyFile *pKeyFile, const gchar *cDefaultConfFilePath, gboolean bUpdateKeys)
{
	GKeyFile *pUptodateKeyFile = cairo_dock_open_key_file (cDefaultConfFilePath);
	g_return_if_fail (pUptodateKeyFile != NULL);
	
	_cairo_dock_replace_key_values (pKeyFile, pUptodateKeyFile, bUpdateKeys);
	
	cairo_dock_write_keys_to_file (pUptodateKeyFile, cConfFilePath);
	
	g_key_file_free (pUptodateKeyFile);
}


void cairo_dock_get_conf_file_version (GKeyFile *pKeyFile, gchar **cConfFileVersion)
{
	*cConfFileVersion = NULL;
	
	gchar *cFirstComment =  g_key_file_get_comment (pKeyFile, NULL, NULL, NULL);
	
	if (cFirstComment != NULL && *cFirstComment != '\0')
	{
		gchar *str = strchr (cFirstComment, '\n');
		if (str != NULL)
			*str = '\0';
		
		str = strchr (cFirstComment, ';');  // le 1er est pour la langue (obsolete).
		if (str != NULL)
		{
			*cConfFileVersion = g_strdup (str+1);
		}
		else
		{
			*cConfFileVersion = g_strdup (cFirstComment + (*cFirstComment == '!'));  // le '!' est obsolete.
		}
	}
	g_free (cFirstComment);
}

gboolean cairo_dock_conf_file_needs_update (GKeyFile *pKeyFile, const gchar *cVersion)
{
	gchar *cPreviousVersion = NULL;
	cairo_dock_get_conf_file_version (pKeyFile, &cPreviousVersion);
	gboolean bNeedsUpdate = (cPreviousVersion == NULL || strcmp (cPreviousVersion, cVersion) != 0);
	
	g_free (cPreviousVersion);
	return bNeedsUpdate;
}


void cairo_dock_add_remove_element_to_key (const gchar *cConfFilePath, const gchar *cGroupName, const gchar *cKeyName, gchar *cElementName, gboolean bAdd)
{
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	if (pKeyFile == NULL)
		return ;
	
	gchar *cElementList = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL), *cNewElementList = NULL;
	if (cElementList != NULL && *cElementList == '\0')
	{
		g_free (cElementList);
		cElementList= NULL;
	}
	
	if (bAdd)
	{
		//g_print ("on rajoute %s\n", cElementName);
		if (cElementList != NULL)
			cNewElementList = g_strdup_printf ("%s;%s", cElementList, cElementName);
		else
			cNewElementList = g_strdup (cElementName);
	}
	else
	{
		//g_print ("on enleve %s\n", cElementName);
		gchar *str = g_strstr_len (cElementList, strlen (cElementList), cElementName);
		g_return_if_fail (str != NULL);
		if (str == cElementList)
		{
			if (str[strlen (cElementName)] == '\0')
				cNewElementList = g_strdup ("");
			else
				cNewElementList = g_strdup (str + strlen (cElementName) + 1);
		}
		else
		{
			*(str-1) = '\0';
			if (str[strlen (cElementName)] == '\0')
				cNewElementList = g_strdup (cElementList);
			else
				cNewElementList = g_strdup_printf ("%s;%s", cElementList, str + strlen (cElementName) + 1);
		}
	}
	g_key_file_set_string (pKeyFile, cGroupName, cKeyName, cNewElementList);
	cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	g_free (cElementList);
	g_free (cNewElementList);
	g_key_file_free (pKeyFile);
}


void cairo_dock_add_widget_to_conf_file (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *ckeyName, const gchar *cInitialValue, CairoDockGUIWidgetType iWidgetType, const gchar *cAuthorizedValues, const gchar *cDescription, const gchar *cTooltip)
{
	g_key_file_set_string (pKeyFile, cGroupName, ckeyName, cInitialValue);
	gchar *Comment = g_strdup_printf ("%c0%s %s%s%s%s", iWidgetType, cAuthorizedValues ? cAuthorizedValues : "", cDescription, cTooltip ? "\n{" : "", cTooltip ? cTooltip : "", cTooltip ? "}" : "");
	g_key_file_set_comment (pKeyFile, cGroupName, ckeyName, Comment, NULL);
	g_free (Comment);
}

void cairo_dock_remove_group_key_from_conf_file (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *ckeyName)
{
	g_key_file_remove_comment (pKeyFile, cGroupName, ckeyName, NULL);
	g_key_file_remove_key (pKeyFile, cGroupName, ckeyName, NULL);
}

gboolean cairo_dock_rename_group_in_conf_file (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cNewGroupName)
{
	if (! g_key_file_has_group (pKeyFile, cGroupName))
		return FALSE;
	
	gchar **pKeyList = g_key_file_get_keys (pKeyFile, cGroupName, NULL, NULL);
	g_return_val_if_fail (pKeyList != NULL, FALSE);
	gchar *cValue;
	int i;
	for (i = 0; pKeyList[i] != NULL; i ++)
	{
		cValue = g_key_file_get_value (pKeyFile, cGroupName, pKeyList[i], NULL);
		g_key_file_set_value (pKeyFile, cNewGroupName, pKeyList[i], cValue);
		g_free (cValue);
	}
	g_strfreev (pKeyList);
	
	g_key_file_remove_group (pKeyFile, cGroupName, NULL);
	
	return TRUE;
}

gchar * cairo_dock_get_locale_string_from_conf_file (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, const gchar *cLocale)
{
	gchar *cKeyValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
	// if the string is empty, gettext mays return a non empty string (e.g. on OpenSUSE we get the .po header)
	if (cKeyValue == NULL || *cKeyValue == '\0')
	{
		g_free (cKeyValue);
		return NULL;
	}

	g_free (cKeyValue);
	return g_key_file_get_locale_string (pKeyFile, cGroupName, cKeyName, cLocale, NULL);
}

void cairo_dock_update_keyfile_va_args (const gchar *cConfFilePath, GType iFirstDataType, va_list args)
{
	cd_message ("%s (%s)", __func__, cConfFilePath);
	
	GKeyFile *pKeyFile = g_key_file_new ();  // if the key-file doesn't exist, it will be created.
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
	
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
}

void cairo_dock_update_keyfile (const gchar *cConfFilePath, GType iFirstDataType, ...)  // type, groupe, cle, valeur, etc. finir par G_TYPE_INVALID.
{
	va_list args;
	va_start (args, iFirstDataType);
	cairo_dock_update_keyfile_va_args (cConfFilePath, iFirstDataType, args);
	va_end (args);
}


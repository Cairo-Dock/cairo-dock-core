/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
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
		cd_warning (erreur->message);
		g_error_free (erreur);
		g_key_file_free (pKeyFile);
		return NULL;
	}
	return pKeyFile;
}

void cairo_dock_write_keys_to_file (GKeyFile *pKeyFile, const gchar *cConfFilePath)
{
	cd_debug ("%s (%s)", __func__, cConfFilePath);
	GError *erreur = NULL;

	gchar *cDirectory = g_path_get_dirname (cConfFilePath);
	if (! g_file_test (cDirectory, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE))
	{
		g_mkdir_with_parents (cDirectory, 7*8*8+7*8+5);
	}
	g_free (cDirectory);


	gsize length;
	gchar *cNewConfFilePath = g_key_file_to_data (pKeyFile, &length, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("Error while fetching data : %s", erreur->message);
		g_error_free (erreur);
		return ;
	}

	g_file_set_contents (cConfFilePath, cNewConfFilePath, length, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("Error while writing data : %s", erreur->message);
		g_error_free (erreur);
		return ;
	}
}


void cairo_dock_flush_conf_file_full (GKeyFile *pKeyFile, gchar *cConfFilePath, gchar *cShareDataDirPath, gboolean bUseFileKeys, gchar *cTemplateFileName)
{
	gchar *cTemplateConfFilePath = g_strdup_printf ("%s/%s", cShareDataDirPath, cTemplateFileName);
	cd_message ("%s (%s)", __func__, cTemplateConfFilePath);
	
	if (! g_file_test (cTemplateConfFilePath, G_FILE_TEST_EXISTS))
	{
		cd_warning ("Couldn't find any installed conf file");
	}
	else
	{
		gchar *cCommand = g_strdup_printf ("/bin/cp %s %s", cTemplateConfFilePath, cConfFilePath);
		system (cCommand);
		g_free (cCommand);
		
		cairo_dock_replace_values_in_conf_file (cConfFilePath, pKeyFile, bUseFileKeys, 0);
	}
	g_free (cTemplateConfFilePath);
}



void cairo_dock_replace_key_values (GKeyFile *pOriginalKeyFile, GKeyFile *pReplacementKeyFile, gboolean bUseOriginalKeys, gchar iIdentifier)
{
	cd_debug ("%s (%d)", __func__, iIdentifier);
	GError *erreur = NULL;
	gsize length = 0;
	gchar **pKeyList;
	gchar **pGroupList = g_key_file_get_groups ((bUseOriginalKeys ? pOriginalKeyFile : pReplacementKeyFile), &length);
	gchar *cGroupName, *cKeyName, *cKeyValue, *cComment;
	int i, j;

	i = 0;
	while (pGroupList[i] != NULL)
	{
		cGroupName = pGroupList[i];

		length = 0;
		pKeyList = g_key_file_get_keys ((bUseOriginalKeys ? pOriginalKeyFile : pReplacementKeyFile), cGroupName, NULL, NULL);

		j = 0;
		while (pKeyList[j] != NULL)
		{
			cKeyName = pKeyList[j];
			//g_print ("%s\n  %s", cKeyName, g_key_file_get_comment (pOriginalKeyFile, cGroupName, cKeyName, NULL));

			if (iIdentifier != 0)
			{
				cComment = g_key_file_get_comment (pReplacementKeyFile, cGroupName, cKeyName, NULL);

				if (cComment == NULL || strlen (cComment) < 2 || cComment[1] != iIdentifier)
				{
					//g_print ("  on saute %s;%s (%s)\n", cGroupName, cKeyName, cComment);
					g_free (cComment);
					j ++;
					continue ;
				}
				g_free (cComment);
			}

			cKeyValue =  g_key_file_get_string (pReplacementKeyFile, cGroupName, cKeyName, &erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
			else
			{
				//g_print (" -> %s\n", cKeyValue);
				if (cKeyValue[strlen(cKeyValue) - 1] == '\n')
					cKeyValue[strlen(cKeyValue) - 1] = '\0';
				g_key_file_set_string (pOriginalKeyFile, cGroupName, cKeyName, (cKeyValue != NULL ? cKeyValue : ""));
			}
			g_free (cKeyValue);
			j ++;
		}

		g_strfreev (pKeyList);
		i ++;
	}
	g_strfreev (pGroupList);
	
	if (bUseOriginalKeys)
	{
		pGroupList = g_key_file_get_groups (pReplacementKeyFile, &length);
		i = 0;
		while (pGroupList[i] != NULL)
		{
			cGroupName = pGroupList[i];

			length = 0;
			pKeyList = g_key_file_get_keys (pReplacementKeyFile, cGroupName, NULL, NULL);

			j = 0;
			while (pKeyList[j] != NULL)
			{
				cKeyName = pKeyList[j];
				//g_print ("%s\n  %s", cKeyName, g_key_file_get_comment (pOriginalKeyFile, cGroupName, cKeyName, NULL));

				cComment = g_key_file_get_comment (pReplacementKeyFile, cGroupName, cKeyName, NULL);
				if (cComment == NULL || strlen (cComment) < 3 || (cComment[1] != '0' && cComment[2] != '0'))
				{
					g_free (cComment);
					j ++;
					continue ;
				}
				if (iIdentifier != 0)
				{
					if (cComment == NULL || strlen (cComment) < 2 || cComment[1] != iIdentifier)
					{
						//g_print ("  on saute %s;%s (%s)\n", cGroupName, cKeyName, cComment);
						g_free (cComment);
						j ++;
						continue ;
					}
				}

				cKeyValue =  g_key_file_get_string (pReplacementKeyFile, cGroupName, cKeyName, &erreur);
				if (erreur != NULL)
				{
					cd_warning (erreur->message);
					g_error_free (erreur);
					erreur = NULL;
				}
				else
				{
					//g_print (" -> %s\n", cKeyValue);
					if (cKeyValue[strlen(cKeyValue) - 1] == '\n')
						cKeyValue[strlen(cKeyValue) - 1] = '\0';
					g_key_file_set_string (pOriginalKeyFile, cGroupName, cKeyName, (cKeyValue != NULL ? cKeyValue : ""));
					if (cComment != NULL)
					{
						g_key_file_set_comment (pOriginalKeyFile, cGroupName, cKeyName, cComment, &erreur);
						if (erreur != NULL)
						{
							cd_warning (erreur->message);
							g_error_free (erreur);
							erreur = NULL;
						}
					}
				}
				g_free (cKeyValue);
				g_free (cComment);
				j ++;
			}

			g_strfreev (pKeyList);
			i ++;
		}
		g_strfreev (pGroupList);
	}
}


void cairo_dock_replace_values_in_conf_file (gchar *cConfFilePath, GKeyFile *pValidKeyFile, gboolean bUseFileKeys, gchar iIdentifier)
{
	GKeyFile *pConfKeyFile = cairo_dock_open_key_file (cConfFilePath);
	if (pConfKeyFile == NULL)
		return ;

	cairo_dock_replace_key_values (pConfKeyFile, pValidKeyFile, bUseFileKeys, iIdentifier);

	cairo_dock_write_keys_to_file (pConfKeyFile, cConfFilePath);

	g_key_file_free (pConfKeyFile);
}

void cairo_dock_replace_keys_by_identifier (gchar *cConfFilePath, gchar *cReplacementConfFilePath, gchar iIdentifier)
{
	cd_debug ("%s (%s <- %s, '%c')", __func__, cConfFilePath, cReplacementConfFilePath, iIdentifier);
	GKeyFile *pReplacementKeyFile = cairo_dock_open_key_file (cReplacementConfFilePath);
	if (pReplacementKeyFile == NULL)
		return ;
	
	cairo_dock_replace_values_in_conf_file (cConfFilePath, pReplacementKeyFile, TRUE, iIdentifier);

	g_key_file_free (pReplacementKeyFile);
}



void cairo_dock_get_conf_file_version (GKeyFile *pKeyFile, gchar **cConfFileVersion)
{
	*cConfFileVersion = NULL;
	
	gchar *cFirstComment =  g_key_file_get_comment (pKeyFile, NULL, NULL, NULL);
	
	if (cFirstComment != NULL && *cFirstComment == '!')
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
			*cConfFileVersion = g_strdup (cFirstComment+1);
		}
	}
	g_free (cFirstComment);
}

gboolean cairo_dock_conf_file_needs_update (GKeyFile *pKeyFile, gchar *cVersion)
{
	gchar *cPreviousVersion = NULL;
	cairo_dock_get_conf_file_version (pKeyFile, &cPreviousVersion);
	
	gboolean bNeedsUpdate;
	if (cPreviousVersion == NULL || strcmp (cPreviousVersion, cVersion) != 0)
		bNeedsUpdate = TRUE;
	else
		bNeedsUpdate = FALSE;
	
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
		g_print ("on rajoute %s\n", cElementName);
		if (cElementList != NULL)
			cNewElementList = g_strdup_printf ("%s;%s", cElementList, cElementName);
		else
			cNewElementList = g_strdup (cElementName);
	}
	else
	{
		g_print ("on enleve %s\n", cElementName);
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

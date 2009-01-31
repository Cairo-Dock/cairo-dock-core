/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <string.h>
#include <stdlib.h>

#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"


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


int _cairo_dock_compare_key_names (gpointer *data1, gpointer *data2)
{
	if (data1[0] == NULL)
	{
		if (data2[0] == NULL)
			return 0;
		else
			return -1;
	}
	else if (data2[0] == NULL)
		return 1;
	else
		return strcmp ((gchar *) data1[0], (gchar *) data2[0]);
}
void _cairo_dock_extract_sorted_table_content (gchar *cName, gpointer value, GList **pList)
{
	gpointer *data = g_new (gpointer, 2);
	data[0] = cName;
	data[1] = value;
	*pList = g_list_insert_sorted (*pList, data, (GCompareFunc) _cairo_dock_compare_key_names);
}
gchar *cairo_dock_write_table_content (GHashTable *pHashTable, GHFunc pWritingFunc, gboolean bSortByKey, gboolean bAddEmptyEntry)
{
	GString *pString = g_string_new ("");

	if (bAddEmptyEntry)
		pWritingFunc ("", NULL, pString);

	if (! bSortByKey)
		g_hash_table_foreach (pHashTable, pWritingFunc, pString);
	else
	{
		GList *pList = NULL;
		g_hash_table_foreach (pHashTable, (GHFunc) _cairo_dock_extract_sorted_table_content, &pList);

		GList *ic;
		gpointer *data;
		for (ic = pList; ic != NULL; ic = ic->next)
		{
			data = ic->data;
			pWritingFunc (data[0], data[1], pString);
			g_free (data);
		}
		g_list_free (pList);
	}
	if (pString->len > 0 && pString->str[pString->len-1] == ';')  // peut etre faux si aucune valeur n'a ete ecrite.
		pString->str[pString->len-1] = '\0';

	gchar *cContent = pString->str;
	g_string_free (pString, FALSE);
	return cContent;
}

void cairo_dock_write_one_name (gchar *cName, gpointer value, GString *pString)
{
	g_string_append_printf (pString, "%s;", cName);
}
void cairo_dock_write_one_name_description (gchar *cName, gchar *cDescriptionFilePath, GString *pString)
{
	g_string_append_printf (pString, "%s;%s;", cName, cDescriptionFilePath);
}
void cairo_dock_write_one_module_name (gchar *cName, CairoDockModule *pModule, GString *pString)
{
	g_string_append_printf (pString, "%s;%s;%s;%s;", cName, (pModule != NULL && pModule->pVisitCard->cReadmeFilePath != NULL ? pModule->pVisitCard->cReadmeFilePath : "none"), (pModule != NULL && pModule->pVisitCard->cPreviewFilePath != NULL ? pModule->pVisitCard->cPreviewFilePath : "none"), (pModule != NULL && pModule->pVisitCard->cIconFilePath != NULL ? pModule->pVisitCard->cIconFilePath : "none"));
}
void cairo_dock_write_one_theme_name (gchar *cName, CairoDockTheme *pTheme, GString *pString)
{
	g_string_append_printf (pString, "%s;%s/readme;%s/preview;", cName, pTheme->cThemePath, pTheme->cThemePath);
}
void cairo_dock_write_one_renderer_name (gchar *cName, CairoDockRenderer *pRenderer, GString *pString)
{
	//g_print ("%s (%s)\n", __func__, cName);
	g_string_append_printf (pString, "%s;%s;%s;", cName, (pRenderer != NULL && pRenderer->cReadmeFilePath != NULL ? pRenderer->cReadmeFilePath : "none"), (pRenderer != NULL && pRenderer->cPreviewFilePath != NULL ? pRenderer->cPreviewFilePath : "none"));
}


void cairo_dock_update_conf_file_with_hash_table (GKeyFile *pOpenedKeyFile, gchar *cConfFile, GHashTable *pModuleTable, gchar *cGroupName, gchar *cKeyName, gchar *cNewUsefullComment, GHFunc pWritingFunc, gboolean bSortByKey, gboolean bAddEmptyEntry)
{
	gchar *cTableContent = cairo_dock_write_table_content (pModuleTable, (pWritingFunc != NULL ? pWritingFunc : (GHFunc) cairo_dock_write_one_name), bSortByKey, bAddEmptyEntry);
	
	cairo_dock_update_conf_file_with_list (pOpenedKeyFile, cConfFile, cTableContent, cGroupName, cKeyName, cNewUsefullComment);
	
	g_free (cTableContent);
}

void cairo_dock_update_conf_file_with_list (GKeyFile *pOpenedKeyFile, gchar *cConfFile, gchar *cList, gchar *cGroupName, gchar *cKeyName, gchar *cNewUsefullComment)
{
	g_return_if_fail (pOpenedKeyFile != NULL || cConfFile != NULL);
	cd_debug ("%s (%s, %s, %s)", __func__, cConfFile, cGroupName, cKeyName);
	GError *erreur = NULL;

	//\___________________ On ouvre le fichier de conf.
	GKeyFile *pKeyFile = pOpenedKeyFile;
	if (pKeyFile == NULL)
	{
		pKeyFile = g_key_file_new ();
		g_key_file_load_from_file (pKeyFile, cConfFile, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			g_key_file_free (pKeyFile);
			return ;
		}
	}
	
	gchar *cUsefullComment;
	gchar *cOldComment = g_key_file_get_comment (pKeyFile, cGroupName, cKeyName, NULL);
	if (cOldComment == NULL)
	{
		if (pOpenedKeyFile == NULL)
			g_key_file_free (pKeyFile);
		return ;
	}
	cOldComment[strlen (cOldComment) - 1] = '\0';

	gchar *cPrefix= cOldComment;
	while (*cPrefix == ' ')
		cPrefix ++;
	gchar *str = strchr (cPrefix, '[');
	if (str != NULL)
	{
		cPrefix = g_strndup (cPrefix, str - cPrefix + 1);
	}
	else
	{
		cPrefix = g_strdup ("s99[");  // par defaut.
	}

	//\___________________ On recupere le commentaire explicatif.
	if (cNewUsefullComment == NULL)
	{
		cUsefullComment = strchr (cOldComment, ']');
		if (cUsefullComment == NULL)
		{
			cUsefullComment = cOldComment;
			while (*cUsefullComment == ' ')
				cUsefullComment ++;
		}
		if (*cUsefullComment != '\0')
			cUsefullComment ++;  // on saute le caractere de type ou le crochet.
		else
			cUsefullComment = NULL;
		if (cUsefullComment != NULL)
			cUsefullComment = g_strdup (cUsefullComment);
	}
	else
	{
		cUsefullComment = g_strdup (cNewUsefullComment);
	}
	g_free (cOldComment);

	//\___________________ On ecrit la liste des possibilites.
	GString *sComment = g_string_new (cPrefix);
	g_free (cPrefix);
	g_string_append_printf (sComment, "%s] %s", cList, (cUsefullComment != NULL ? cUsefullComment : ""));
	g_key_file_set_comment (pKeyFile, cGroupName, cKeyName, sComment->str, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	g_string_free (sComment, TRUE);
	
	if (cConfFile != NULL)
		cairo_dock_write_keys_to_file (pKeyFile, cConfFile);
	g_free (cUsefullComment);
	if (pOpenedKeyFile == NULL)
		g_key_file_free (pKeyFile);
}


void cairo_dock_replace_values_in_conf_file (gchar *cConfFilePath, GKeyFile *pValidKeyFile, gboolean bUseFileKeys, gchar iIdentifier)
{
	GKeyFile *pConfKeyFile = g_key_file_new ();

	GError *erreur = NULL;
	g_key_file_load_from_file (pConfKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		g_key_file_free (pConfKeyFile);
		return ;
	}

	cairo_dock_replace_key_values (pConfKeyFile, pValidKeyFile, bUseFileKeys, iIdentifier);

	cairo_dock_write_keys_to_file (pConfKeyFile, cConfFilePath);

	g_key_file_free (pConfKeyFile);
}

void cairo_dock_replace_keys_by_identifier (gchar *cConfFilePath, gchar *cReplacementConfFilePath, gchar iIdentifier)
{
	cd_debug ("%s (%s <- %s, '%c')\n", __func__, cConfFilePath, cReplacementConfFilePath, iIdentifier);
	GError *erreur = NULL;
	GKeyFile *pReplacementKeyFile = g_key_file_new ();
	g_key_file_load_from_file (pReplacementKeyFile, cReplacementConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return ;
	}

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
	GError *erreur = NULL;
	GKeyFile* pKeyFile = g_key_file_new ();
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cConfFilePath, erreur->message);
		g_error_free (erreur);
		g_key_file_free (pKeyFile);
		return ;
	}
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

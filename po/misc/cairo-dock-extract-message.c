/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet_03@yahoo.fr)

******************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <cairo-dock.h>

#define write_message(m) fprintf (f, "_(\"%s\")\n\n", g_strescape (m, NULL))

static gchar *_parse_key_comment (gchar *cKeyComment, char *iElementType, guint *iNbElements, gchar ***pAuthorizedValuesList, gboolean *bAligned, gchar **cTipString)
{
	if (cKeyComment == NULL || *cKeyComment == '\0')
		return NULL;
	
	gchar *cUsefulComment = cKeyComment;
	while (*cUsefulComment == '#' || *cUsefulComment == ' ' || *cUsefulComment == '\n')  // on saute les # et les espaces
		cUsefulComment ++;
	
	int length = strlen (cUsefulComment);
	while (cUsefulComment[length-1] == '\n')
	{
		cUsefulComment[length-1] = '\0';
		length --;
	}
	
	
	//\______________ On recupere le type du widget.
	*iElementType = *cUsefulComment;
	cUsefulComment ++;
	if (*cUsefulComment == '-' || *cUsefulComment == '+')
		cUsefulComment ++;
	if (*cUsefulComment == '*' || *cUsefulComment == '&')
		cUsefulComment ++;

	//\______________ On recupere le nombre d'elements du widget.
	*iNbElements = atoi (cUsefulComment);
	if (*iNbElements == 0)
		*iNbElements = 1;
	while (g_ascii_isdigit (*cUsefulComment))  // on saute les chiffres.
			cUsefulComment ++;
	while (*cUsefulComment == ' ')  // on saute les espaces.
		cUsefulComment ++;

	//\______________ On recupere les valeurs autorisees.
	if (*cUsefulComment == '[')
	{
		cUsefulComment ++;
		gchar *cAuthorizedValuesChain = cUsefulComment;

		while (*cUsefulComment != '\0' && *cUsefulComment != ']')
			cUsefulComment ++;
		g_return_val_if_fail (*cUsefulComment != '\0', NULL);
		*cUsefulComment = '\0';
		cUsefulComment ++;
		while (*cUsefulComment == ' ')  // on saute les espaces.
			cUsefulComment ++;
		
		if (*cAuthorizedValuesChain == '\0')  // rien, on prefere le savoir plutot que d'avoir une entree vide.
			*pAuthorizedValuesList = g_new0 (gchar *, 1);
		else
			*pAuthorizedValuesList = g_strsplit (cAuthorizedValuesChain, ";", 0);
	}
	else
	{
		*pAuthorizedValuesList = NULL;
	}
	
	//\______________ On recupere l'alignement.
	int len = strlen (cUsefulComment);
	if (cUsefulComment[len - 1] == '\n')
	{
		len --;
		cUsefulComment[len] = '\0';
	}
	if (cUsefulComment[len - 1] == '/')
	{
		cUsefulComment[len - 1] = '\0';
		*bAligned = FALSE;
	}
	else
	{
		*bAligned = TRUE;
	}

	//\______________ On recupere la bulle d'aide.
	gchar *str = strchr (cUsefulComment, '{');
	if (str != NULL && str != cUsefulComment)
	{
		if (*(str-1) == '\n')
			*(str-1) ='\0';
		else
			*str = '\0';

		str ++;
		*cTipString = str;

		str = strrchr (*cTipString, '}');
		if (str != NULL)
			*str = '\0';
	}
	else
	{
		*cTipString = NULL;
	}
	
	return cUsefulComment;
}

int
main (int argc, char** argv)
{
	if (argc < 2)
		g_error ("il manque le chemin du fichier !\n");
	gchar *cConfFilePath = argv[1];
	
	GKeyFile *pKeyFile = g_key_file_new ();
	
	GError *erreur = NULL;
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
		g_error ("/!\\ [%s] %s\n", cConfFilePath, erreur->message);
	
	int iNbBuffers = 0;
	gsize length = 0;
	gchar **pKeyList;
	gchar **pGroupList = g_key_file_get_groups (pKeyFile, &length);
	
	gchar *cGroupName, *cKeyName, *cKeyComment, *cUsefulComment, *cAuthorizedValuesChain, *cTipString, **pAuthorizedValuesList;
	int i, j, k, iNbElements;
	char iElementType;
	gboolean bIsAligned;
	gboolean bValue, *bValueList;
	int iValue, iMinValue, iMaxValue, *iValueList;
	double fValue, fMinValue, fMaxValue, *fValueList;
	gchar *cValue, **cValueList;
	
	gchar *cDirPath = g_path_get_dirname (cConfFilePath);
	gchar *cMessagesFilePath = g_strconcat (cDirPath, "/messages", NULL);
	FILE *f = fopen (cMessagesFilePath, "a");
	if (!f)
		g_error ("impossible d'ouvrir %s", cMessagesFilePath);
	
	i = 0;
	cGroupName = pGroupList[0];
	if (cGroupName != NULL && strcmp (cGroupName, "ChangeLog") == 0)
	{
		pKeyList = g_key_file_get_keys (pKeyFile, cGroupName, NULL, NULL);
		j = 0;
		while (pKeyList[j] != NULL)
		{
			cKeyName = pKeyList[j];
			cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
			write_message (cValue);
			g_free (cValue);
			j ++;
		}
		g_strfreev (pKeyList);
	}
	else
	while (pGroupList[i] != NULL)
	{
		cGroupName = pGroupList[i];
		write_message (cGroupName);
		
		pKeyList = g_key_file_get_keys (pKeyFile, cGroupName, NULL, NULL);
		
		j = 0;
		while (pKeyList[j] != NULL)
		{
			cKeyName = pKeyList[j];

			cKeyComment =  g_key_file_get_comment (pKeyFile, cGroupName, cKeyName, NULL);
			//g_print ("%s -> %s\n", cKeyName, cKeyComment);
			if (cKeyComment != NULL && strcmp (cKeyComment, "") != 0)
			{
				pAuthorizedValuesList = NULL;
				cTipString = NULL;
				iNbElements = 0;
				cKeyComment =  g_key_file_get_comment (pKeyFile, cGroupName, cKeyName, NULL);
				cUsefulComment = _parse_key_comment (cKeyComment, &iElementType, &iNbElements, &pAuthorizedValuesList, &bIsAligned, &cTipString);
				
				if (cTipString != NULL)
				{
					write_message (cTipString);
				}

				if (*cUsefulComment != '\0' && strcmp (cUsefulComment, "...") != 0 && iElementType != 'F' && iElementType != 'X')
				{
					write_message (cUsefulComment);
				}
				
				switch (iElementType)
				{
					case CAIRO_DOCK_WIDGET_CHECK_BUTTON :
					case CAIRO_DOCK_WIDGET_CHECK_CONTROL_BUTTON :
					
					case CAIRO_DOCK_WIDGET_SPIN_INTEGER :  
					case CAIRO_DOCK_WIDGET_SIZE_INTEGER :  
					
					case CAIRO_DOCK_WIDGET_SPIN_DOUBLE :
					case CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGB : 
					case CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGBA :
					
					case CAIRO_DOCK_WIDGET_STRING_ENTRY :
					case CAIRO_DOCK_WIDGET_CLASS_SELECTOR :
					case CAIRO_DOCK_WIDGET_PASSWORD_ENTRY :
					case CAIRO_DOCK_WIDGET_FILE_SELECTOR :
					case CAIRO_DOCK_WIDGET_FOLDER_SELECTOR :  
					case CAIRO_DOCK_WIDGET_SOUND_SELECTOR :
					case CAIRO_DOCK_WIDGET_FONT_SELECTOR :
					case CAIRO_DOCK_WIDGET_SHORTKEY_SELECTOR :
					
					case CAIRO_DOCK_WIDGET_TREE_VIEW_SORT_AND_MODIFY :  // rien a faire car les choix sont modifiables.
					
					case CAIRO_DOCK_WIDGET_VIEW_LIST :
					case CAIRO_DOCK_WIDGET_THEME_LIST :
					case CAIRO_DOCK_WIDGET_ANIMATION_LIST :
					case CAIRO_DOCK_WIDGET_DIALOG_DECORATOR_LIST :
					case CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST :
					case CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST_WITH_DEFAULT :
					case CAIRO_DOCK_WIDGET_DOCK_LIST :
					case CAIRO_DOCK_WIDGET_ICONS_LIST :
					case CAIRO_DOCK_WIDGET_ICON_THEME_LIST :
					//case CAIRO_DOCK_WIDGET_MODULE_LIST :
					case CAIRO_DOCK_WIDGET_JUMP_TO_MODULE :
					case CAIRO_DOCK_WIDGET_JUMP_TO_MODULE_IF_EXISTS :
					
					case CAIRO_DOCK_WIDGET_EMPTY_WIDGET :
					case CAIRO_DOCK_WIDGET_EMPTY_FULL :
					case CAIRO_DOCK_WIDGET_TEXT_LABEL :
					case CAIRO_DOCK_WIDGET_HANDBOOK :
					case CAIRO_DOCK_WIDGET_SEPARATOR :
					
					case CAIRO_DOCK_WIDGET_LAUNCH_COMMAND :
					case CAIRO_DOCK_WIDGET_LAUNCH_COMMAND_IF_CONDITION :
					break;
					
					case CAIRO_DOCK_WIDGET_HSCALE_INTEGER :
					case CAIRO_DOCK_WIDGET_HSCALE_DOUBLE :
						if (pAuthorizedValuesList != NULL)
						{
							for (k = 0; pAuthorizedValuesList[k] != NULL; k ++)
							{
								if (! g_ascii_isdigit (pAuthorizedValuesList[k][0]) && pAuthorizedValuesList[k][0] != '.' && pAuthorizedValuesList[k][0] != '+' && pAuthorizedValuesList[k][0] != '-')
									write_message (pAuthorizedValuesList[k]);
							}
						}
					break;
					
					case CAIRO_DOCK_WIDGET_LINK :
					case CAIRO_DOCK_WIDGET_LIST :
					case CAIRO_DOCK_WIDGET_NUMBERED_LIST :
					case CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST :
					case CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY :
					case CAIRO_DOCK_WIDGET_TREE_VIEW_SORT :
					case CAIRO_DOCK_WIDGET_TREE_VIEW_MULTI_CHOICE :
						if (pAuthorizedValuesList != NULL)
						{
							for (k = 0; pAuthorizedValuesList[k] != NULL; k ++)
							{
								if (pAuthorizedValuesList[k][0] != '\0')
									write_message (pAuthorizedValuesList[k]);
							}
						}
					break;
					
					case CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST_SELECTIVE :
						if (pAuthorizedValuesList != NULL)
						{
							for (k = 0; pAuthorizedValuesList[k] != NULL; k += 3)
							{
								write_message (pAuthorizedValuesList[k]);
							}
						}
					break;
					case CAIRO_DOCK_WIDGET_FRAME :
					case CAIRO_DOCK_WIDGET_EXPANDER :
						if (pAuthorizedValuesList != NULL)
						{
							if (pAuthorizedValuesList[0] == NULL || *pAuthorizedValuesList[0] == '\0')
								cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
							else
								cValue = pAuthorizedValuesList[0];
							write_message (cValue);
						}
					break;
					
					default :
						g_print ("this conf file seems to be incorrect ! (type : %c)\n", iElementType);
					break ;
				}
				g_strfreev (pAuthorizedValuesList);
				g_free (cKeyComment);
			}

			j ++;
		}
		g_strfreev (pKeyList);

		i ++;
	}
	
	g_strfreev (pGroupList);
	fclose (f);
	return 0;
}

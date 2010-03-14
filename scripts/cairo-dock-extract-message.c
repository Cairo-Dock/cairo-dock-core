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
	
	gchar *cGroupName, *cKeyName, *cKeyComment, *cUsefulComment, *cAuthorizedValuesChain, *pTipString, **pAuthorizedValuesList;
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
				cUsefulComment = cKeyComment;
				while (*cUsefulComment == '#' || *cUsefulComment == ' ')  // on saute les # et les espaces.
					cUsefulComment ++;

				iElementType = *cUsefulComment;
				cUsefulComment ++;

				if (! g_ascii_isdigit (*cUsefulComment) && *cUsefulComment != '[')
				{
					cUsefulComment ++;
				}
				
				if (g_ascii_isdigit (*cUsefulComment))
				{
					iNbElements = atoi (cUsefulComment);
					iNbElements = MAX (iNbElements, 1);  // cas 0.
					while (g_ascii_isdigit (*cUsefulComment))
						cUsefulComment ++;
				}
				else
				{
					iNbElements = 1;
				}
				//g_print ("%d element(s)\n", iNbElements);

				while (*cUsefulComment == ' ')  // on saute les espaces.
					cUsefulComment ++;

				if (*cUsefulComment == '[')
				{
					cUsefulComment ++;
					cAuthorizedValuesChain = cUsefulComment;

					while (*cUsefulComment != '\0' && *cUsefulComment != ']')
						cUsefulComment ++;
					g_return_val_if_fail (*cUsefulComment != '\0', 1);
					*cUsefulComment = '\0';
					cUsefulComment ++;
					while (*cUsefulComment == ' ')  // on saute les espaces.
						cUsefulComment ++;

					pAuthorizedValuesList = g_strsplit (cAuthorizedValuesChain, ";", 0);
				}
				else
				{
					pAuthorizedValuesList = NULL;
				}
				if (cUsefulComment[strlen (cUsefulComment) - 1] == '\n')
					cUsefulComment[strlen (cUsefulComment) - 1] = '\0';
				if (cUsefulComment[strlen (cUsefulComment) - 1] == '/')
				{
					bIsAligned = FALSE;
					cUsefulComment[strlen (cUsefulComment) - 1] = '\0';
				}
				else
				{
					bIsAligned = TRUE;
				}
				//g_print ("cUsefulComment : %s\n", cUsefulComment);

				pTipString = strchr (cUsefulComment, '{');
				if (pTipString != NULL)
				{
					if (*(pTipString-1) == '\n')
						*(pTipString-1) ='\0';
					else
						*pTipString = '\0';

					pTipString ++;

					gchar *pTipEnd = strrchr (pTipString, '}');
					if (pTipEnd != NULL)
						*pTipEnd = '\0';
				}

				if (pTipString != NULL)
				{
					//g_print ("pTipString : '%s'\n", pTipString);
					write_message (pTipString);
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
					case CAIRO_DOCK_WIDGET_THEME_LIST_ENTRY :
					case CAIRO_DOCK_WIDGET_USER_THEME_SELECTOR :
					case CAIRO_DOCK_WIDGET_THEME_SELECTOR :
					case CAIRO_DOCK_WIDGET_ANIMATION_LIST :
					case CAIRO_DOCK_WIDGET_DIALOG_DECORATOR_LIST :
					case CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST :
					case CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST_WITH_DEFAULT :
					case CAIRO_DOCK_WIDGET_GAUGE_LIST :
					case CAIRO_DOCK_WIDGET_DOCK_LIST :
					case CAIRO_DOCK_WIDGET_ICON_THEME_LIST :
					case CAIRO_DOCK_WIDGET_MODULE_LIST :
					case CAIRO_DOCK_WIDGET_JUMP_TO_MODULE :
					case CAIRO_DOCK_WIDGET_JUMP_TO_MODULE_IF_EXISTS :
					
					case CAIRO_DOCK_WIDGET_EMPTY_WIDGET :
					case CAIRO_DOCK_WIDGET_TEXT_LABEL :
					case CAIRO_DOCK_WIDGET_HANDBOOK :
					case CAIRO_DOCK_WIDGET_SEPARATOR :
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

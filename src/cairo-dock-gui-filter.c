/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <glib/gi18n.h>

#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gui-filter.h"

extern gchar *g_cConfFile;

static GString *sBuffer = NULL;
static void _copy_string_to_buffer (gchar *cSentence)
{
	g_string_assign (sBuffer, cSentence);
	gchar *str;
	for (str = sBuffer->str; *str != '\0'; str ++)
	{
		if (*str >= 'A' && *str <= 'Z')
		{
			*str = *str - 'A' + 'a';
		}
	}
}
#define _search_in_buffer(cKeyWord) g_strstr_len (sBuffer->str, -1, cKeyWord)
static gchar *cairo_dock_highlight_key_word (const gchar *cSentence, const gchar *cKeyWord, gboolean bBold)
{
	//_copy_string_to_buffer (cSentence);
	gchar *cModifiedString = NULL;
	gchar *str = _search_in_buffer (cKeyWord);
	if (str != NULL)
	{
		//g_print ("* trouve %s dans '%s'\n", cKeyWord, sBuffer->str);
		gchar *cBuffer = g_strdup (cSentence);
		str = cBuffer + (str - sBuffer->str);
		*str = '\0';
		cModifiedString = g_strdup_printf ("%s<span color=\"red\">%s%s%s</span>%s", cBuffer, (bBold?"<b>":""), cKeyWord, (bBold?"</b>":""), str + strlen (cKeyWord));
		g_free (cBuffer);
	}
	
	return cModifiedString;
}

static gboolean _cairo_dock_search_words_in_frame_title (gchar **pKeyWords, GtkWidget *pCurrentFrame, gboolean bAllWords, gboolean bHighLightText, gboolean bHideOther)
{
	//\______________ On recupere son titre.
	GtkWidget *pFrameLabel = NULL;
	GtkWidget *pLabelContainer = (GTK_IS_FRAME (pCurrentFrame) ?
		gtk_frame_get_label_widget (GTK_FRAME (pCurrentFrame)) :
		gtk_expander_get_label_widget (GTK_EXPANDER (pCurrentFrame)));
	//g_print ("pLabelContainer : %x\n", pLabelContainer);
	if (GTK_IS_LABEL (pLabelContainer))
	{
		pFrameLabel = pLabelContainer;
	}
	else if (pLabelContainer != NULL)
	{
		GList *pChildList = gtk_container_get_children (GTK_CONTAINER (pLabelContainer));
		if (pChildList != NULL && pChildList->next != NULL)
			pFrameLabel = pChildList->next->data;
	}
	
	//\______________ On cherche les mots-cles dedans.
	gchar *cModifiedText = NULL, *str = NULL, *cKeyWord;
	gboolean bFoundInFrameTitle = FALSE;
	if (pFrameLabel != NULL)
	{
		const gchar *cFrameTitle = gtk_label_get_text (GTK_LABEL (pFrameLabel));
		int i;
		for (i = 0; pKeyWords[i] != NULL; i ++)
		{
			cKeyWord = pKeyWords[i];
			_copy_string_to_buffer (cFrameTitle);
			if (bHighLightText)
				cModifiedText = cairo_dock_highlight_key_word (cFrameTitle, cKeyWord, TRUE);
			else
				str = _search_in_buffer (cKeyWord);
			if (cModifiedText != NULL || str != NULL)  // on a trouve ce mot.
			{
				//g_print ("  on a trouve %s dans le titre\n", cKeyWord);
				bFoundInFrameTitle = TRUE;
				if (cModifiedText != NULL)
				{
					gtk_label_set_markup (GTK_LABEL (pFrameLabel), cModifiedText);
					cFrameTitle = gtk_label_get_label (GTK_LABEL (pFrameLabel));  // Pango inclus.
					g_free (cModifiedText);
					cModifiedText = NULL;
				}
				else
					str = NULL;
				if (! bAllWords)
					break ;
			}
			else if (bAllWords)
			{
				bFoundInFrameTitle = FALSE;
				break ;
			}
		}
		if (! bFoundInFrameTitle)  // on remet le texte par defaut.
		{
			cModifiedText = g_strdup_printf ("<b>%s</b>", cFrameTitle);
			gtk_label_set_markup (GTK_LABEL (pFrameLabel), cModifiedText);
			g_free (cModifiedText);
			cModifiedText = NULL;
		}
	}
	return bFoundInFrameTitle;
}


void cairo_dock_apply_filter_on_group_widget (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther, GSList *pWidgetList)
{
	//g_print ("%s ()\n", __func__);
	if (sBuffer == NULL)
		sBuffer = g_string_new ("");
	gpointer *pGroupKeyWidget;
	GList *pSubWidgetList;
	GtkWidget *pLabel, *pAlign, *pKeyBox, *pVBox, *pFrame, *pOneWidget, *pCurrentFrame = NULL, *pLabelContainer, *pFrameLabel, *pExpander;
	const gchar *cDescription;
	gchar *cToolTip = NULL;
	gchar *cModifiedText=NULL, *str=NULL;
	gboolean bFound, bFrameVisible;
	int i;
	gchar *cKeyWord;
	GSList *w;
	for (w = pWidgetList; w != NULL; w = w->next)
	{
		bFound = FALSE;
		pGroupKeyWidget = w->data;
		//g_print ("widget : %s - %s\n", pGroupKeyWidget[0], pGroupKeyWidget[1]);
		pSubWidgetList = pGroupKeyWidget[2];
		if (pSubWidgetList == NULL)
			continue;
		pLabel = pGroupKeyWidget[4];
		if (pLabel == NULL)
			continue;
		
		//pOneWidget = pSubWidgetList->data;
		pAlign = gtk_widget_get_parent (pLabel);
		pKeyBox = gtk_widget_get_parent (pAlign);
		pVBox = gtk_widget_get_parent (pKeyBox);
		pFrame = gtk_widget_get_parent (pVBox);
		
		//\______________ On cache une frame vide, ou au contraire on montre son contenu si elle contient les mots-cles.
		if (pFrame != pCurrentFrame)  // on a change de frame.
		{
			if (pCurrentFrame)
			{
				gboolean bFoundInFrameTitle = _cairo_dock_search_words_in_frame_title (pKeyWords, pCurrentFrame, bAllWords, bHighLightText, bHideOther);
				if (! bFrameVisible && bHideOther)
				{
					if (! bFoundInFrameTitle)
						gtk_widget_hide (pCurrentFrame);
					else
						gtk_widget_show_all (pCurrentFrame);  // montre tous les widgets du groupe.
				}
				else
					gtk_widget_show (pCurrentFrame);
			}
			
			if (GTK_IS_FRAME (pFrame))  // devient la frame courante.
			{
				pExpander = gtk_widget_get_parent (pFrame);
				if (GTK_IS_EXPANDER (pExpander))
					pFrame = pExpander;  // c'est l'expander qui a le texte, c'est donc ca qu'on veut cacher.
				pCurrentFrame = pFrame;
				bFrameVisible = FALSE;
			}
			else
			{
				pCurrentFrame = NULL;
			}
			//g_print ("pCurrentFrame <- %x\n", pCurrentFrame);
		}
		
		cDescription = gtk_label_get_text (GTK_LABEL (pLabel));  // sans les markup Pango.
		if (bSearchInToolTip)
			cToolTip = gtk_widget_get_tooltip_text (pKeyBox);
		//g_print ("cDescription : %s (%s)\n", cDescription, cToolTip);
		
		bFound = FALSE;
		for (i = 0; pKeyWords[i] != NULL; i ++)
		{
			cKeyWord = pKeyWords[i];
			_copy_string_to_buffer (cDescription);
			if (bHighLightText)
				cModifiedText = cairo_dock_highlight_key_word (cDescription, cKeyWord, FALSE);
			else
				str = _search_in_buffer (cKeyWord);
			if (cModifiedText == NULL && str == NULL)
			{
				if (cToolTip != NULL)
				{
					_copy_string_to_buffer (cToolTip);
					str = _search_in_buffer (cKeyWord);
				}
			}
			
			if (cModifiedText != NULL || str != NULL)
			{
				//g_print ("  on a trouve %s\n", cKeyWord);
				bFound = TRUE;
				if (cModifiedText != NULL)
				{
					gtk_label_set_markup (GTK_LABEL (pLabel), cModifiedText);
					cDescription = gtk_label_get_label (GTK_LABEL (pLabel));  // Pango inclus.
					g_free (cModifiedText);
					cModifiedText = NULL;
				}
				else
				{
					gtk_label_set_text (GTK_LABEL (pLabel), cDescription);
					str = NULL;
				}
				if (! bAllWords)
					break ;
			}
			else if (bAllWords)
			{
				bFound = FALSE;
				break ;
			}
		}
		if (bFound)
		{
			//g_print ("on montre ce widget\n");
			gtk_widget_show (pKeyBox);
			if (pCurrentFrame != NULL)
				bFrameVisible = TRUE;
		}
		else if (bHideOther)
		{
			//g_print ("on cache ce widget\n");
			gtk_widget_hide (pKeyBox);
		}
		else
			gtk_widget_show (pKeyBox);
		g_free (cToolTip);
	}
	if (pCurrentFrame)  // la derniere frame.
	{
		gboolean bFoundInFrameTitle = _cairo_dock_search_words_in_frame_title (pKeyWords, pCurrentFrame, bAllWords, bHighLightText, bHideOther);
		if (! bFrameVisible && bHideOther)
		{
			if (! bFoundInFrameTitle)
				gtk_widget_hide (pCurrentFrame);
			else
				gtk_widget_show_all (pCurrentFrame);  // montre tous les widgets du groupe.
		}
		else
			gtk_widget_show (pCurrentFrame);
	}
}

void cairo_dock_apply_filter_on_group_list (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther, GList *pGroupDescriptionList)
{
	//g_print ("%s ()\n", __func__);
	if (sBuffer == NULL)
		sBuffer = g_string_new ("");
	CairoDockGroupDescription *pGroupDescription, *pInternalGroupDescription;
	gchar *cKeyWord, *str = NULL, *cModifiedText = NULL, *cDescription, *cToolTip = NULL;
	gboolean bFound, bFrameVisible;
	GtkWidget *pGroupBox, *pLabel, *pCategoryFrame, *pCurrentCategoryFrame = NULL;
	GError *erreur = NULL;
	GKeyFile *pKeyFile;
	GKeyFile *pMainKeyFile = cairo_dock_open_key_file (g_cConfFile);
	
	int i;
	GList *gd;
	gchar *cGettextDomain;
	for (gd = pGroupDescriptionList; gd != NULL; gd = gd->next)
	{
		pGroupDescription = gd->data;
		pGroupDescription->bMatchFilter = FALSE;
	}
	for (gd = pGroupDescriptionList; gd != NULL; gd = gd->next)
	{
		//g_print ("pGroupDescription:%x\n", gd->data);
		//\_______________ On recupere le group description.
		pGroupDescription = gd->data;
		
		if (pGroupDescription->cInternalModule)
		{
			g_print ("%s : bouton emprunte a %s\n", pGroupDescription->cGroupName, pGroupDescription->cInternalModule);
			pInternalGroupDescription = cairo_dock_find_module_description (pGroupDescription->cInternalModule);
			if (pInternalGroupDescription != NULL)
				pGroupBox = gtk_widget_get_parent (pInternalGroupDescription->pActivateButton);
			else
				continue;
			pLabel = pInternalGroupDescription->pLabel;
			g_print ("ok, found pGroupBox\n");
		}
		else
		{
			pGroupBox = gtk_widget_get_parent (pGroupDescription->pActivateButton);
			pLabel = pGroupDescription->pLabel;
		}
		//g_print ("  %x\n", pGroupDescription->pActivateButton);
		pCategoryFrame = gtk_widget_get_parent (pGroupBox);
		cGettextDomain = pGroupDescription->cGettextDomain;
		bFound = FALSE;
		
		cDescription = dgettext (cGettextDomain, pGroupDescription->cGroupName);
		if (bSearchInToolTip)
			cToolTip = dgettext (cGettextDomain, pGroupDescription->cDescription);
		//g_print ("cDescription : %s (%s)(%x,%x)\n", cDescription, cToolTip, cModifiedText, str);
		
		//\_______________ On change de frame.
		if (pCategoryFrame != pCurrentCategoryFrame)  // on a change de frame.
		{
			if (pCurrentCategoryFrame)
			{
				if (! bFrameVisible && bHideOther)
				{
					//g_print (" on cache cette categorie\n");
					gtk_widget_hide (pCurrentCategoryFrame);
				}
				else
					gtk_widget_show (pCurrentCategoryFrame);
			}
			pCurrentCategoryFrame = pCategoryFrame;
			//g_print (" pCurrentCategoryFrame <- %x\n", pCurrentCategoryFrame);
		}
		
		//\_______________ On cherche chaque mot dans la description du module.
		for (i = 0; pKeyWords[i] != NULL; i ++)
		{
			cKeyWord = pKeyWords[i];
			_copy_string_to_buffer (cDescription);
			if (bHighLightText)
				cModifiedText = cairo_dock_highlight_key_word (cDescription, cKeyWord, TRUE);
			else
				str = _search_in_buffer (cKeyWord);
			if (cModifiedText == NULL && str == NULL)
			{
				if (cToolTip != NULL)
				{
					_copy_string_to_buffer (cToolTip);
					str = _search_in_buffer (cKeyWord);
				}
			}
			
			if (cModifiedText != NULL || str != NULL)
			{
				//g_print (">>> on a trouve direct %s\n", cKeyWord);
				bFound = TRUE;
				if (cModifiedText != NULL)
				{
					gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
					gtk_label_set_markup (GTK_LABEL (pLabel), cModifiedText);
					g_free (cModifiedText);
					cModifiedText = NULL;
				}
				else
				{
					gtk_label_set_text (GTK_LABEL (pLabel), cDescription);
					str = NULL;
				}
				if (! bAllWords)
					break ;
			}
			else if (bAllWords)
			{
				bFound = FALSE;
				break ;
			}
		}
		
		//\_______________ On cherche chaque mot a l'interieur du module.
		if (! bFound && pGroupDescription->cOriginalConfFilePath != NULL)
		{
			//\_______________ On recupere les groupes du module.
			g_print ("* on cherche dans le fichier de conf %s ...\n", pGroupDescription->cOriginalConfFilePath);
			gchar **pGroupList = NULL;
			CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
			if (pModule != NULL)
			{
				pKeyFile = cairo_dock_open_key_file (pModule->cConfFilePath);
				if (pKeyFile != NULL)
				{
					gsize length = 0;
					pGroupList = g_key_file_get_groups (pKeyFile, &length);
				}
			}
			else  // groupe interne, le fichier de conf n'est ouvert qu'une seule fois.
			{
				pKeyFile = pMainKeyFile;
				pGroupList = g_new0 (gchar *, 2);
				pGroupList[0] = g_strdup (pGroupDescription->cGroupName);
			}
			
			//\_______________ Pour chaque groupe on parcourt toutes les cles.
			if (pGroupList != NULL)
			{
				int iNbWords;
				for (iNbWords = 0; pKeyWords[iNbWords] != NULL; iNbWords ++);
				gboolean *bFoundWords = g_new0 (gboolean , iNbWords);
				
				gchar *cUsefulComment;
				gchar iElementType;
				int iNbElements;
				gchar **pAuthorizedValuesList;
				gchar *cTipString;
				gboolean bIsAligned;
				gchar **pKeyList;
				gchar *cGroupName, *cKeyName, *cKeyComment;
				int j, k;
				for (k = 0; pGroupList[k] != NULL; k ++)
				{
					cGroupName = pGroupList[k];
					pKeyList = g_key_file_get_keys (pKeyFile, cGroupName, NULL, NULL);
					for (j = 0; pKeyList[j] != NULL; j ++)
					{
						cKeyName = pKeyList[j];
						//\_______________ On recupere la description + bulle d'aide de la cle.
						cKeyComment =  g_key_file_get_comment (pKeyFile, cGroupName, cKeyName, NULL);
						cUsefulComment = cairo_dock_parse_key_comment (cKeyComment, &iElementType, &iNbElements, &pAuthorizedValuesList, &bIsAligned, &cTipString);
						if (cUsefulComment == NULL)
						{
							g_free (cKeyComment);
							continue;
						}
						
						cUsefulComment = dgettext (cGettextDomain, cUsefulComment);
						if (cTipString != NULL)
						{
							if (bSearchInToolTip)
								cTipString = dgettext (cGettextDomain, cTipString);
							else
								cTipString = NULL;
						}
						//\_______________ On y cherche les mots-cles.
						for (i = 0; pKeyWords[i] != NULL; i ++)
						{
							if (bFoundWords[i])
								continue;
							cKeyWord = pKeyWords[i];
							str = NULL;
							if (cUsefulComment)
							{
								_copy_string_to_buffer (cUsefulComment);
								str = _search_in_buffer (cKeyWord);
							}
							if (! str && cTipString)
							{
								_copy_string_to_buffer (cTipString);
								str = _search_in_buffer (cKeyWord);
							}
							if (! str && pAuthorizedValuesList)
							{
								int l;
								for (l = 0; pAuthorizedValuesList[l] != NULL; l ++)
								{
									_copy_string_to_buffer (dgettext (cGettextDomain, pAuthorizedValuesList[l]));
									str = _search_in_buffer (cKeyWord);
									if (str != NULL)
										break ;
								}
							}
							
							if (str != NULL)
							{
								//g_print (">>>on a trouve %s\n", pKeyWords[i]);
								bFound = TRUE;
								str = NULL;
								if (! bAllWords)
								{
									break ;
								}
								bFoundWords[i] = TRUE;
							}
						}
						
						g_free (cKeyComment);
						if (! bAllWords && bFound)
							break ;
					}  // fin de parcours du groupe.
					g_strfreev (pKeyList);
					if (! bAllWords && bFound)
						break ;
				}  // fin de parcours des groupes.
				g_strfreev (pGroupList);
				
				if (bAllWords && bFound)
				{
					for (i = 0; i < iNbWords; i ++)
					{
						if (! bFoundWords[i])
						{
							//g_print ("par contre il manque %s, dommage\n", pKeyWords[i]);
							bFound = FALSE;
							break;
						}
					}
				}
				g_free (bFoundWords);
			}  // fin du cas ou on avait des groupes a etudier.
			if (pKeyFile != pMainKeyFile)
				g_key_file_free (pKeyFile);
			//g_print ("bFound : %d\n", bFound);
			
			if (bHighLightText && bFound)  // on passe le label du groupe en bleu + gras.
			{
				cModifiedText = g_strdup_printf ("<b><span color=\"blue\">%s</span></b>", cDescription);
				//g_print ("cModifiedText : %s\n", cModifiedText);
				gtk_label_set_markup (GTK_LABEL (pLabel), dgettext (cGettextDomain, cModifiedText));
				g_free (cModifiedText);
				cModifiedText = NULL;
			}
		}  // fin du cas ou on devait chercher dans le groupe.

		if (pGroupDescription->cInternalModule)
		{
			pInternalGroupDescription = cairo_dock_find_module_description (pGroupDescription->cInternalModule);
			if (pInternalGroupDescription != NULL)
			{
				pInternalGroupDescription->bMatchFilter |= bFound;
				bFound = pInternalGroupDescription->bMatchFilter;
			}
		}
		else
		{
			pGroupDescription->bMatchFilter |= bFound;
			bFound = pGroupDescription->bMatchFilter;
		}
		if (bFound)
		{
			//g_print ("on montre ce groupe\n");
			gtk_widget_show (pGroupBox);
			if (pCurrentCategoryFrame != NULL)
				bFrameVisible = TRUE;
		}
		else if (bHideOther)
		{
			//g_print ("on cache ce groupe (%s)\n", pGroupDescription->cGroupName);
			gtk_widget_hide (pGroupBox);
		}
		else
			gtk_widget_show (pGroupBox);
		if (! bHighLightText || ! bFound)
		{
			gtk_label_set_markup (GTK_LABEL (pLabel), dgettext (cGettextDomain, cDescription));
		}
	}
	g_key_file_free (pMainKeyFile);
}

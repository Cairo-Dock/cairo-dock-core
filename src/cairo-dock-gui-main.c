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
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-main.h"

#define CAIRO_DOCK_GROUP_ICON_SIZE 32
#define CAIRO_DOCK_CATEGORY_ICON_SIZE 32
#define CAIRO_DOCK_NB_BUTTONS_BY_ROW 4
#define CAIRO_DOCK_NB_BUTTONS_BY_ROW_MIN 3
#define CAIRO_DOCK_GUI_MARGIN 6
#define CAIRO_DOCK_TABLE_MARGIN 12
#define CAIRO_DOCK_CONF_PANEL_WIDTH 1150
#define CAIRO_DOCK_CONF_PANEL_WIDTH_MIN 800
#define CAIRO_DOCK_CONF_PANEL_HEIGHT 700
#define CAIRO_DOCK_PREVIEW_WIDTH 200
#define CAIRO_DOCK_PREVIEW_WIDTH_MIN 100
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_ICON_MARGIN 6
#define CAIRO_DOCK_TAB_ICON_SIZE 32

struct _CairoDockCategoryWidgetTable {
	GtkWidget *pFrame;
	GtkWidget *pTable;
	gint iNbRows;
	gint iNbItemsInCurrentRow;
	GtkToolItem *pCategoryButton;
	} ;

struct _CairoDockGroupDescription {
	gchar *cGroupName;
	gchar *cTitle;
	gint iCategory;
	gchar *cDescription;
	gchar *cPreviewFilePath;
	GtkWidget *pActivateButton;
	GtkWidget *pLabel;
	gchar *cOriginalConfFilePath;
	gchar *cIcon;
	gchar *cConfFilePath;
	const gchar *cGettextDomain;
	void (* load_custom_widget) (CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
	const gchar **cDependencies;
	gboolean bIgnoreDependencies;
	GList *pExternalModules;
	const gchar *cInternalModule;
	gboolean bMatchFilter;
	} ;

typedef struct _CairoDockGroupDescription CairoDockGroupDescription;
typedef struct _CairoDockCategoryWidgetTable CairoDockCategoryWidgetTable;

static CairoDockCategoryWidgetTable s_pCategoryWidgetTables[CAIRO_DOCK_NB_CATEGORY+1];
GSList *s_pCurrentWidgetList;  // liste des widgets du module courant.
GSList *s_pExtraCurrentWidgetList;  // liste des widgets des eventuels modules lies.
static GList *s_pGroupDescriptionList = NULL;
static GtkWidget *s_pPreviewImage = NULL;
static GtkWidget *s_pOkButton = NULL;
static GtkWidget *s_pApplyButton = NULL;
static GtkWidget *s_pBackButton = NULL;
static GtkWidget *s_pMainWindow = NULL;
static GtkWidget *s_pGroupsVBox = NULL;
static CairoDockGroupDescription *s_pCurrentGroup = NULL;
static GtkWidget *s_pCurrentGroupWidget = NULL;
static GtkWidget *s_pToolBar = NULL;
static GtkWidget *s_pGroupFrame = NULL;
static GtkWidget *s_pFilterEntry = NULL;
static GtkWidget *s_pActivateButton = NULL;
static GtkWidget *s_pStatusBar = NULL;
static GSList *s_path = NULL;
static int s_iPreviewWidth, s_iNbButtonsByRow;
static CairoDialog *s_pDialog = NULL;
static int s_iSidShowGroupDialog = 0;

extern gchar *g_cConfFile;
extern CairoDock *g_pMainDock;
extern int g_iXScreenWidth[2], g_iXScreenHeight[2];
extern CairoDockDesktopEnv g_iDesktopEnv;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cCairoDockDataDir;
extern gboolean g_bEasterEggs;

static const gchar *s_cCategoriesDescription[2*(CAIRO_DOCK_NB_CATEGORY+1)] = {
	N_("Behaviour"), "icon-behavior.svg",
	N_("Appearance"), "icon-appearance.svg",
	N_("Accessories"), "icon-accessories.svg",
	N_("Desktop"), "icon-desktop.svg",
	N_("Controlers"), "icon-controler.svg",
	N_("Plug-ins"), "icon-extensions.svg",
	N_("All"), "icon-all.svg" };

static void cairo_dock_hide_all_categories (void);
static void cairo_dock_show_all_categories (void);
static void cairo_dock_show_one_category (int iCategory);
static void cairo_dock_toggle_category_button (int iCategory);
static void cairo_dock_show_group (CairoDockGroupDescription *pGroupDescription);
static CairoDockGroupDescription *cairo_dock_find_module_description (const gchar *cModuleName);
static void cairo_dock_apply_current_filter (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther);
static GtkWidget *cairo_dock_present_group_widget (const gchar *cConfFilePath, CairoDockGroupDescription *pGroupDescription, gboolean bSingleGroup, CairoDockModuleInstance *pInstance);

  ////////////
 // FILTER //
////////////

static GString *sBuffer = NULL;
static inline void _copy_string_to_buffer (const gchar *cSentence)
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
	GtkWidget *pLabel, *pKeyBox, *pVBox, *pFrame, *pCurrentFrame = NULL, *pExpander;
	const gchar *cDescription;
	gchar *cToolTip = NULL;
	gchar *cModifiedText=NULL, *str=NULL;
	gboolean bFound, bFrameVisible = !bHideOther;
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
		pKeyBox = pGroupKeyWidget[5];
		if (pKeyBox == NULL)
			continue;
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
	GKeyFile *pKeyFile;
	GKeyFile *pMainKeyFile = cairo_dock_open_key_file (g_cConfFile);
	
	int i;
	GList *gd;
	const gchar *cGettextDomain;
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
			//g_print ("* on cherche dans le fichier de conf %s ...\n", pGroupDescription->cOriginalConfFilePath);
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


  ///////////////
 // CALLBACKS //
///////////////

static void on_click_category_button (GtkButton *button, gpointer data)
{
	int iCategory = GPOINTER_TO_INT (data);
	//g_print ("%s (%d)\n", __func__, iCategory);
	cairo_dock_show_one_category (iCategory);
}

static void on_click_all_button (GtkButton *button, gpointer data)
{
	//g_print ("%s ()\n", __func__);
	cairo_dock_show_all_categories ();
}

static void on_click_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription)
{
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cGroupName);
	cairo_dock_show_group (pGroupDescription);
}

static void _show_group_or_category (gpointer pPlace)
{
	if (pPlace == NULL)
		cairo_dock_show_all_categories ();
	else if ((int)pPlace < 10)  // categorie.
	{
		if (pPlace == 0)
			cairo_dock_show_all_categories ();
		else
		{
			int iCategory = (int)pPlace - 1;
			cairo_dock_show_one_category (iCategory);
		}
	}
	else  // groupe.
	{
		cairo_dock_show_group (pPlace);
	}
}

static gpointer _get_previous_widget (void)
{
	if (s_path == NULL || s_path->next == NULL)
	{
		if (s_path != NULL)  // utile ?...
		{
			g_slist_free (s_path);
			s_path = NULL;
		}
		return 0;
	}
	
	//s_path = g_list_delete_link (s_path, s_path);
	GSList *tmp = s_path;
	s_path = s_path->next;
	tmp->next = NULL;
	g_slist_free (tmp);
	
	return s_path->data;
}
static void on_click_back_button (GtkButton *button, gpointer data)
{
	gpointer pPrevPlace = _get_previous_widget ();
	_show_group_or_category (pPrevPlace);
}

static gboolean _show_group_dialog (CairoDockGroupDescription *pGroupDescription)
{
	gchar *cDescription = NULL;
	if (pGroupDescription->cDescription != NULL)
	{
		if (*pGroupDescription->cDescription == '/')
		{
			//g_print ("on recupere la description de %s\n", pGroupDescription->cDescription);
			
			gsize length = 0;
			GError *erreur = NULL;
			g_file_get_contents (pGroupDescription->cDescription,
				&cDescription,
				&length,
				&erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
			}
		}
	}
	
	int iPreviewWidgetWidth = s_iPreviewWidth;
	GtkWidget *pPreviewImage = s_pPreviewImage;
	if (pGroupDescription->cPreviewFilePath != NULL && strcmp (pGroupDescription->cPreviewFilePath, "none") != 0)
	{
		//g_print ("on recupere la prevue de %s\n", pGroupDescription->cPreviewFilePath);
		int iPreviewWidth, iPreviewHeight;
		GdkPixbuf *pPreviewPixbuf = NULL;
		if (gdk_pixbuf_get_file_info (pGroupDescription->cPreviewFilePath, &iPreviewWidth, &iPreviewHeight) != NULL)
		{
			if (iPreviewWidth > CAIRO_DOCK_PREVIEW_WIDTH)
			{
				iPreviewHeight *= 1.*CAIRO_DOCK_PREVIEW_WIDTH/iPreviewWidth;
				iPreviewWidth = CAIRO_DOCK_PREVIEW_WIDTH;
			}
			if (iPreviewHeight > CAIRO_DOCK_PREVIEW_HEIGHT)
			{
				iPreviewWidth *= 1.*CAIRO_DOCK_PREVIEW_HEIGHT/iPreviewHeight;
				iPreviewHeight = CAIRO_DOCK_PREVIEW_HEIGHT;
			}
			if (iPreviewWidth > iPreviewWidgetWidth)
			{
				iPreviewHeight *= 1.*iPreviewWidgetWidth/iPreviewWidth;
				iPreviewWidth = iPreviewWidgetWidth;
			}
			g_print ("preview : %dx%d\n", iPreviewWidth, iPreviewHeight);
			pPreviewPixbuf = gdk_pixbuf_new_from_file_at_size (pGroupDescription->cPreviewFilePath, iPreviewWidth, iPreviewHeight, NULL);
		}
		if (pPreviewPixbuf == NULL)
		{
			cd_warning ("no preview available");
			pPreviewPixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				TRUE,
				8,
				1,
				1);
		}
		gtk_image_set_from_pixbuf (GTK_IMAGE (pPreviewImage), pPreviewPixbuf);
		gdk_pixbuf_unref (pPreviewPixbuf);
		gtk_widget_show (pPreviewImage);
	}
	
	if (s_pDialog != NULL)
		if (! cairo_dock_dialog_unreference (s_pDialog))
			cairo_dock_dialog_unreference (s_pDialog);
	Icon *pIcon = cairo_dock_get_current_active_icon ();
	if (pIcon == NULL || pIcon->cParentDockName == NULL || cairo_dock_icon_is_being_removed (pIcon))
		pIcon = cairo_dock_get_dialogless_icon ();
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon != NULL ? pIcon->cParentDockName : NULL);
	
	CairoDialogAttribute attr;
	memset (&attr, 0, sizeof (CairoDialogAttribute));
	attr.cText = dgettext (pGroupDescription->cGettextDomain, cDescription != NULL ? cDescription : pGroupDescription->cDescription);
	attr.cImageFilePath = pGroupDescription->cIcon;
	attr.bNoInput = TRUE;
	s_pDialog = cairo_dock_build_dialog (&attr, pIcon, CAIRO_CONTAINER (pDock));
	
	cairo_dock_dialog_reference (s_pDialog);
	
	gtk_window_set_transient_for (GTK_WINDOW (s_pDialog->container.pWidget), GTK_WINDOW (s_pMainWindow));
	g_free (cDescription);

	s_iSidShowGroupDialog = 0;
	return FALSE;
}
static void on_enter_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription)
{
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cDescription);
	if (g_pMainDock == NULL)  // inutile en maintenance, le dialogue risque d'apparaitre sur la souris.
		return ;
	
	if (s_iSidShowGroupDialog != 0)
		g_source_remove (s_iSidShowGroupDialog);
	
	s_iSidShowGroupDialog = g_timeout_add (500, (GSourceFunc)_show_group_dialog, (gpointer) pGroupDescription);
}
static void on_leave_group_button (GtkButton *button, gpointer *data)
{
	//g_print ("%s ()\n", __func__);
	if (s_iSidShowGroupDialog != 0)
	{
		g_source_remove (s_iSidShowGroupDialog);
		s_iSidShowGroupDialog = 0;
	}

	int iPreviewWidgetWidth = s_iPreviewWidth;
	GtkWidget *pPreviewImage = s_pPreviewImage;
	gtk_widget_hide (pPreviewImage);
	
	if (! cairo_dock_dialog_unreference (s_pDialog))
		cairo_dock_dialog_unreference (s_pDialog);
	s_pDialog = NULL;
}


static void cairo_dock_reset_current_widget_list (void)
{
	cairo_dock_free_generated_widget_list (s_pCurrentWidgetList);
	s_pCurrentWidgetList = NULL;
	g_slist_foreach (s_pExtraCurrentWidgetList, (GFunc) cairo_dock_free_generated_widget_list, NULL);
	g_slist_free (s_pExtraCurrentWidgetList);
	s_pExtraCurrentWidgetList = NULL;
}

static void cairo_dock_free_categories (void)
{
	memset (s_pCategoryWidgetTables, 0, sizeof (s_pCategoryWidgetTables));  // les widgets a l'interieur sont detruits avec la fenetre.
	
	CairoDockGroupDescription *pGroupDescription;
	GList *pElement;
	for (pElement = s_pGroupDescriptionList; pElement != NULL; pElement = pElement->next)  /// utile de supprimer cette liste ? ...
	{
		pGroupDescription = pElement->data;
		g_free (pGroupDescription->cGroupName);
		g_free (pGroupDescription->cDescription);
		g_free (pGroupDescription->cPreviewFilePath);
	}
	g_list_free (s_pGroupDescriptionList);
	s_pGroupDescriptionList = NULL;
	
	s_pPreviewImage = NULL;
	
	s_pOkButton = NULL;
	s_pApplyButton = NULL;
	
	s_pMainWindow = NULL;
	s_pToolBar = NULL;
	s_pStatusBar = NULL;
	
	if (s_pCurrentGroup != NULL)
	{
		g_free (s_pCurrentGroup->cConfFilePath);
		s_pCurrentGroup->cConfFilePath = NULL;
		s_pCurrentGroup = NULL;
	}
	cairo_dock_reset_current_widget_list ();
	s_pCurrentGroupWidget = NULL;  // detruit en meme temps que la fenetre.
	g_slist_free (s_path);
	s_path = NULL;
}

static gboolean on_delete_main_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	g_print ("%s (%x)\n", __func__, pBlockingLoop);
	if (pBlockingLoop != NULL)
	{
		cd_debug ("dialogue detruit, on sort de la boucle");
		//if (g_main_loop_is_running (pBlockingLoop))
		//	g_main_loop_quit (pBlockingLoop);
	}
	cairo_dock_free_categories ();
	if (s_iSidShowGroupDialog != 0)
	{
		g_source_remove (s_iSidShowGroupDialog);
		s_iSidShowGroupDialog = 0;
	}
	
	cairo_dock_dialog_window_destroyed ();
	
	return FALSE;
}


static void cairo_dock_write_current_group_conf_file (gchar *cConfFilePath, CairoDockModuleInstance *pInstance)
{
	g_return_if_fail (cConfFilePath != NULL && s_pCurrentWidgetList != NULL);
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	if (pKeyFile == NULL)
		return ;

	cairo_dock_update_keyfile_from_widget_list (pKeyFile, s_pCurrentWidgetList);
	if (pInstance != NULL && pInstance->pModule->pInterface->save_custom_widget != NULL)
		pInstance->pModule->pInterface->save_custom_widget (pInstance, pKeyFile);
	cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	g_key_file_free (pKeyFile);
}

static void cairo_dock_write_extra_group_conf_file (gchar *cConfFilePath, CairoDockModuleInstance *pInstance, int iNumExtraModule)
{
	g_return_if_fail (cConfFilePath != NULL && s_pExtraCurrentWidgetList != NULL);
	GSList *pWidgetList = g_slist_nth_data  (s_pExtraCurrentWidgetList, iNumExtraModule);
	g_return_if_fail (pWidgetList != NULL);

	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	if (pKeyFile == NULL)
		return ;
	
	cairo_dock_update_keyfile_from_widget_list (pKeyFile, pWidgetList);
	if (pInstance != NULL && pInstance->pModule->pInterface->save_custom_widget != NULL)
		pInstance->pModule->pInterface->save_custom_widget (pInstance, pKeyFile);
	cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	g_key_file_free (pKeyFile);
}

static void on_click_apply (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	
	CairoDockGroupDescription *pGroupDescription = s_pCurrentGroup;
	if (pGroupDescription == NULL)
		return ;
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	if (pModule != NULL)
	{
		if (pModule->pInstancesList != NULL)
		{
			CairoDockModuleInstance *pModuleInstance;
			GList *pElement;
			for (pElement = pModule->pInstancesList; pElement != NULL; pElement= pElement->next)
			{
				pModuleInstance = pElement->data;
				if (strcmp (pModuleInstance->cConfFilePath, pGroupDescription->cConfFilePath) == 0)
					break ;
			}
			g_return_if_fail (pModuleInstance != NULL);
			
			cairo_dock_write_current_group_conf_file (pModuleInstance->cConfFilePath, pModuleInstance);
			cairo_dock_reload_module_instance (pModuleInstance, TRUE);
		}
		else
		{
			cairo_dock_write_current_group_conf_file (pModule->cConfFilePath, NULL);
		}
	}
	else
	{
		cairo_dock_write_current_group_conf_file (g_cConfFile, NULL);
		CairoDockInternalModule *pInternalModule = cairo_dock_find_internal_module_from_name (pGroupDescription->cGroupName);
		g_return_if_fail (pInternalModule != NULL);
		//g_print ("found module %s\n", pInternalModule->cModuleName);
		cairo_dock_reload_internal_module (pInternalModule, g_cConfFile);
		if (pInternalModule->pExternalModules != NULL)  // comme on ne sait pas sur quel(s) module(s) on a fait des modif, on les met tous a jour.
		{
			CairoDockModuleInstance *pModuleInstance;
			GList *m;
			int i = 0;
			for (m = pInternalModule->pExternalModules; m != NULL; m = m->next)
			{
				pModule = cairo_dock_find_module_from_name (m->data);
				if (pModule == NULL || pModule->pInstancesList == NULL)
					continue;
				pModuleInstance = pModule->pInstancesList->data;
				cairo_dock_write_extra_group_conf_file (pModuleInstance->cConfFilePath, pModuleInstance, i);
				cairo_dock_reload_module_instance (pModuleInstance, TRUE);
				i ++;
			}
		}
	}
}

static void on_click_quit (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	GMainLoop *pBlockingLoop = g_object_get_data (G_OBJECT (pWindow), "loop");
	
	gboolean bReturn;
	g_signal_emit_by_name (pWindow, "delete-event", NULL, &bReturn);
	///on_delete_main_gui (pWindow, NULL, pBlockingLoop);
	
	if (pBlockingLoop == NULL)
		gtk_widget_destroy (pWindow);
}

static void on_click_ok (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	
	on_click_apply (button, pWindow);
	on_click_quit (button, pWindow);
}

static void on_click_activate_given_group (GtkToggleButton *button, CairoDockGroupDescription *pGroupDescription)
{
	g_return_if_fail (pGroupDescription != NULL);
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cGroupName);
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	g_return_if_fail (pModule != NULL);
	if (g_pMainDock == NULL)
	{
		cairo_dock_add_remove_element_to_key (g_cConfFile, "System", "modules", pGroupDescription->cGroupName, gtk_toggle_button_get_active (button));
	}
	else if (pModule->pInstancesList == NULL)
	{
		cairo_dock_activate_module_and_load (pGroupDescription->cGroupName);
	}
	else
	{
		cairo_dock_deactivate_module_and_unload (pGroupDescription->cGroupName);
	}
}

static void on_click_activate_current_group (GtkToggleButton *button, gpointer *data)
{
	CairoDockGroupDescription *pGroupDescription = s_pCurrentGroup;
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	g_return_if_fail (pModule != NULL);
	if (pModule->pInstancesList == NULL)
	{
		cairo_dock_activate_module_and_load (pGroupDescription->cGroupName);
	}
	else
	{
		cairo_dock_deactivate_module_and_unload (pGroupDescription->cGroupName);
	}
	if (pGroupDescription->pActivateButton != NULL)
	{
		g_signal_handlers_block_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pGroupDescription->pActivateButton), pModule->pInstancesList != NULL);
		g_signal_handlers_unblock_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
	}
}


  ////////////
 // FILTER //
////////////

static gboolean bAllWords = FALSE;
static gboolean bSearchInToolTip = FALSE;
static gboolean bHighLightText = FALSE;
static gboolean bHideOther = FALSE;

static inline void _reset_filter_state (void)
{
	bAllWords = FALSE;
	bSearchInToolTip = TRUE;
	bHighLightText = TRUE;
	bHideOther = TRUE;
}

static void on_activate_filter (GtkEntry *pEntry, gpointer data)
{
	const gchar *cFilterText = gtk_entry_get_text (pEntry);
	if (cFilterText == NULL || *cFilterText == '\0')
	{
		return;
	}
	
	gchar **pKeyWords = g_strsplit (cFilterText, " ", 0);
	if (pKeyWords == NULL)  // 1 seul mot.
	{
		pKeyWords = g_new0 (gchar*, 2);
		pKeyWords[0] = (gchar *) cFilterText;
	}
	gchar *str;
	int i,j;
	for (i = 0; pKeyWords[i] != NULL; i ++)
	{
		for (str = pKeyWords[i]; *str != '\0'; str ++)
		{
			if (*str >= 'A' && *str <= 'Z')
			{
				*str = *str - 'A' + 'a';
			}
		}
	}
	cairo_dock_apply_current_filter (pKeyWords, bAllWords, bSearchInToolTip, bHighLightText, bHideOther);
	
	g_strfreev (pKeyWords);
}

static void _trigger_current_filter (void)
{
	gboolean bReturn;
	g_signal_emit_by_name (s_pFilterEntry, "activate", NULL, &bReturn);
}
static void on_toggle_all_words (GtkCheckMenuItem *pMenuItem/**GtkToggleButton *pButton*/, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bAllWords = gtk_check_menu_item_get_active (pMenuItem);
	_trigger_current_filter ();
}
static void on_toggle_search_in_tooltip (GtkCheckMenuItem *pMenuItem/**GtkToggleButton *pButton*/, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bSearchInToolTip = gtk_check_menu_item_get_active (pMenuItem);
	_trigger_current_filter ();
}
static void on_toggle_highlight_words (GtkCheckMenuItem *pMenuItem/**GtkToggleButton *pButton*/, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bHighLightText = gtk_check_menu_item_get_active (pMenuItem);
	_trigger_current_filter ();
}
static void on_toggle_hide_others (GtkCheckMenuItem *pMenuItem/**GtkToggleButton *pButton*/, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bHideOther = gtk_check_menu_item_get_active (pMenuItem);
	_trigger_current_filter ();
}
static void on_clear_filter (GtkButton *pButton, GtkEntry *pEntry)
{
	gtk_entry_set_text (pEntry, "");
	//gpointer pCurrentPlace = cairo_dock_get_current_widget ();
	//g_print ("pCurrentPlace : %x\n", pCurrentPlace);
	//_show_group_or_category (pCurrentPlace);
	const gchar *keyword[2] = {"fabounetfabounetfabounet", NULL};
	cairo_dock_apply_current_filter ((gchar **)keyword, FALSE, FALSE, FALSE, FALSE);
}


  ////////////
 // WINDOW //
////////////

static inline GtkWidget *_make_image (const gchar *cImage, int iSize)
{
	GtkWidget *pImage = NULL;
	if (strncmp (cImage, "gtk-", 4) == 0)
	{
		if (iSize >= 48)
			iSize = GTK_ICON_SIZE_DIALOG;
		else if (iSize >= 32)
			iSize = GTK_ICON_SIZE_LARGE_TOOLBAR;
		else
			iSize = GTK_ICON_SIZE_BUTTON;
		pImage = gtk_image_new_from_stock (cImage, iSize);
	}
	else
	{
		gchar *cIconPath = NULL;
		if (*cImage != '/')
		{
			cIconPath = g_strconcat (g_cCairoDockDataDir, "/config-panel/", cImage, NULL);
			if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS))
			{
				g_free (cIconPath);
				cIconPath = g_strconcat (CAIRO_DOCK_SHARE_DATA_DIR"/", cImage, NULL);
			}
		}
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cIconPath ? cIconPath : cImage, iSize, iSize, NULL);
		g_free (cIconPath);
		if (pixbuf != NULL)
		{
			pImage = gtk_image_new_from_pixbuf (pixbuf);
			gdk_pixbuf_unref (pixbuf);
		}
	}
	return pImage;
}
static void _add_image_on_button (GtkWidget *pButton, const gchar *cImage, int iSize)
{
	if (cImage == NULL)
		return ;
	GtkWidget *pImage = _make_image (cImage, iSize);
	if (pImage != NULL)
		gtk_button_set_image (GTK_BUTTON (pButton), pImage);  /// unref l'image ?...
}
static GtkToolItem *_make_toolbutton (const gchar *cLabel, const gchar *cImage, int iSize)
{
	if (cImage == NULL)
	{
		GtkToolItem *pWidget = gtk_toggle_tool_button_new ();
		gtk_tool_button_set_label (GTK_TOOL_BUTTON (pWidget), cLabel);
		return pWidget;
	}
	GtkWidget *pImage = _make_image (cImage, iSize);
	GtkToolItem *pWidget = gtk_toggle_tool_button_new ();
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (pWidget), pImage);
	if (cLabel == NULL)
		return pWidget;
	
	GtkWidget *pLabel = gtk_label_new (NULL);
	gchar *cLabel2 = g_strdup_printf ("<span font_desc=\"Sans 12\"><b>%s</b></span>", cLabel);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel2);
	g_free (cLabel2);
	
	GtkWidget *pAlign = gtk_alignment_new (0., 0.5, 0., 1.);
	gtk_alignment_set_padding (GTK_ALIGNMENT (pAlign), 0, 0, CAIRO_DOCK_GUI_MARGIN, 0);
	gtk_container_add (GTK_CONTAINER (pAlign), pLabel);
	gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (pWidget), pAlign);
	
	return pWidget;
}

static inline CairoDockGroupDescription *_add_group_button (const gchar *cGroupName, const gchar *cIcon, int iCategory, const gchar *cDescription, const gchar *cPreviewFilePath, int iActivation, gboolean bConfigurable, const gchar *cOriginalConfFilePath, const gchar *cGettextDomain, const gchar **cDependencies, GList *pExternalModules, const gchar *cInternalModule, const gchar *cTitle)
{
	//\____________ On garde une trace de ses caracteristiques.
	CairoDockGroupDescription *pGroupDescription = g_new0 (CairoDockGroupDescription, 1);
	pGroupDescription->cGroupName = g_strdup (cGroupName);
	pGroupDescription->cDescription = g_strdup (cDescription);
	pGroupDescription->iCategory = iCategory;
	pGroupDescription->cPreviewFilePath = g_strdup (cPreviewFilePath);
	pGroupDescription->cOriginalConfFilePath = g_strdup (cOriginalConfFilePath);
	gchar *cIconPath = NULL;
	if (cIcon)
	{
		if (*cIcon == '/')  // on ecrase les chemins des icons d'applets.
		{
			cIconPath = g_strdup_printf ("%s/config-panel/%s.png", g_cCairoDockDataDir, cGroupName);
			if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS))
			{
				g_free (cIconPath);
				cIconPath = g_strdup (cIcon);
			}
		}
		else if (strncmp (cIcon, "gtk-", 4) == 0)
		{
			cIconPath = g_strdup (cIcon);
		}
		else  // categorie ou module interne.
		{
			cIconPath = g_strconcat (g_cCairoDockDataDir, "/config-panel/", cIcon, NULL);
			if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS))
			{
				g_free (cIconPath);
				cIconPath = g_strconcat (CAIRO_DOCK_SHARE_DATA_DIR"/", cIcon, NULL);
			}
		}
	}
	pGroupDescription->cIcon = cIconPath;
	pGroupDescription->cGettextDomain = cGettextDomain;
	pGroupDescription->cDependencies = cDependencies;
	pGroupDescription->pExternalModules = pExternalModules;
	pGroupDescription->cInternalModule = cInternalModule;
	pGroupDescription->cTitle = cTitle;
	
	s_pGroupDescriptionList = g_list_prepend (s_pGroupDescriptionList, pGroupDescription);
	if (cInternalModule != NULL)
		return pGroupDescription;
	
	//\____________ On construit le bouton du groupe.
	GtkWidget *pGroupHBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	
	pGroupDescription->pActivateButton = gtk_check_button_new ();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pGroupDescription->pActivateButton), iActivation);
	g_signal_connect (G_OBJECT (pGroupDescription->pActivateButton), "clicked", G_CALLBACK(on_click_activate_given_group), pGroupDescription);
	if (iActivation == -1)
		gtk_widget_set_sensitive (pGroupDescription->pActivateButton, FALSE);
	gtk_box_pack_start (GTK_BOX (pGroupHBox),
		pGroupDescription->pActivateButton,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pGroupButton = gtk_button_new ();
	if (bConfigurable)
		g_signal_connect (G_OBJECT (pGroupButton), "clicked", G_CALLBACK(on_click_group_button), pGroupDescription);
	else
		gtk_widget_set_sensitive (pGroupButton, FALSE);
	g_signal_connect (G_OBJECT (pGroupButton), "enter", G_CALLBACK(on_enter_group_button), pGroupDescription);
	g_signal_connect (G_OBJECT (pGroupButton), "leave", G_CALLBACK(on_leave_group_button), NULL);
	_add_image_on_button (pGroupButton,
		cIconPath,
		CAIRO_DOCK_GROUP_ICON_SIZE);
	gtk_box_pack_start (GTK_BOX (pGroupHBox),
		pGroupButton,
		FALSE,
		FALSE,
		0);

	pGroupDescription->pLabel = gtk_label_new (dgettext (pGroupDescription->cGettextDomain, cTitle));
	gtk_box_pack_start (GTK_BOX (pGroupHBox),
		pGroupDescription->pLabel,
		FALSE,
		FALSE,
		0);

	//\____________ On place le bouton dans sa table.
	CairoDockCategoryWidgetTable *pCategoryWidget = &s_pCategoryWidgetTables[iCategory];
	if (pCategoryWidget->iNbItemsInCurrentRow == s_iNbButtonsByRow)
	{
		pCategoryWidget->iNbItemsInCurrentRow = 0;
		pCategoryWidget->iNbRows ++;
	}
	gtk_table_attach_defaults (GTK_TABLE (pCategoryWidget->pTable),
		pGroupHBox,
		pCategoryWidget->iNbItemsInCurrentRow,
		pCategoryWidget->iNbItemsInCurrentRow+1,
		pCategoryWidget->iNbRows,
		pCategoryWidget->iNbRows+1);
	pCategoryWidget->iNbItemsInCurrentRow ++;
	return pGroupDescription;
}

static gboolean _cairo_dock_add_one_module_widget (CairoDockModule *pModule, const gchar *cActiveModules)
{
	const gchar *cModuleName = pModule->pVisitCard->cModuleName;
	if (pModule->cConfFilePath == NULL && ! g_bEasterEggs)  // option perso : les plug-ins non utilises sont grises et ne rajoutent pas leur .conf au theme courant.
		pModule->cConfFilePath = cairo_dock_check_module_conf_file (pModule->pVisitCard);
	int iActive;
	if (! pModule->pInterface->stopModule)
		iActive = -1;
	else if (g_pMainDock == NULL && cActiveModules != NULL)  // avant chargement du theme.
	{
		gchar *str = g_strstr_len (cActiveModules, strlen (cActiveModules), cModuleName);
		iActive = (str != NULL &&
			(str[strlen(cModuleName)] == '\0' || str[strlen(cModuleName)] == ';') &&
			(str == cActiveModules || *(str-1) == ';'));
	}
	else
		iActive = (pModule->pInstancesList != NULL);
	
	CairoDockGroupDescription *pGroupDescription = _add_group_button (cModuleName,
		pModule->pVisitCard->cIconFilePath,
		pModule->pVisitCard->iCategory,
		pModule->pVisitCard->cDescription,
		pModule->pVisitCard->cPreviewFilePath,
		iActive,
		pModule->cConfFilePath != NULL,
		NULL,
		pModule->pVisitCard->cGettextDomain,
		NULL,
		NULL,
		pModule->pVisitCard->cInternalModule,
		pModule->pVisitCard->cTitle);
	//g_print ("+ %s : %x;%x\n", cModuleName,pGroupDescription, pGroupDescription->pActivateButton);
	pGroupDescription->cOriginalConfFilePath = g_strdup_printf ("%s/%s", pModule->pVisitCard->cShareDataDir, pModule->pVisitCard->cConfFileName);  // petite optimisation, pour pas dupliquer la chaine 2 fois.
	pGroupDescription->load_custom_widget = pModule->pInterface->load_custom_widget;
	//return FALSE;
	return TRUE;  // continue.
}


static gboolean on_expose (GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	gpointer data)
{
	if (! mySystem.bConfigPanelTransparency)
		return FALSE;
	cairo_t *pCairoContext = gdk_cairo_create (pWidget->window);
	int w, h;
	gtk_window_get_size (GTK_WINDOW (pWidget), &w, &h);
	cairo_pattern_t *pPattern = cairo_pattern_create_linear (0,0,
		w,
		h);
	cairo_pattern_set_extend (pPattern, CAIRO_EXTEND_REPEAT);
	cairo_pattern_add_color_stop_rgba   (pPattern,
		1.,
		0.,0.,0.,0.);
	cairo_pattern_add_color_stop_rgba   (pPattern,
		.25,
		241/255., 234/255., 255/255., 0.8);
	cairo_pattern_add_color_stop_rgba   (pPattern,
		0.,
		255/255., 255/255., 255/255., 0.9);
	cairo_set_source (pCairoContext, pPattern);
	
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pCairoContext);
	
	cairo_pattern_destroy (pPattern);
	cairo_destroy (pCairoContext);
	return FALSE;
}
static GtkWidget *cairo_dock_build_main_ihm (const gchar *cConfFilePath, gboolean bMaintenanceMode)
{
	//\_____________ On construit la fenetre.
	if (s_pMainWindow != NULL)
	{
		gtk_window_present (GTK_WINDOW (s_pMainWindow));
		return s_pMainWindow;
	}
	s_pMainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	//gtk_container_set_border_width (s_pMainWindow, CAIRO_DOCK_GUI_MARGIN);
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);
	gtk_window_set_icon_from_file (GTK_WINDOW (s_pMainWindow), cIconPath, NULL);
	g_free (cIconPath);
	
	GtkWidget *pMainHBox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (s_pMainWindow), pMainHBox);
	
	if (g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] > CAIRO_DOCK_CONF_PANEL_WIDTH)
	{
		s_iPreviewWidth = CAIRO_DOCK_PREVIEW_WIDTH;
		s_iNbButtonsByRow = CAIRO_DOCK_NB_BUTTONS_BY_ROW;
	}
	else if (g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] > CAIRO_DOCK_CONF_PANEL_WIDTH_MIN)
	{
		double a = 1.*(CAIRO_DOCK_PREVIEW_WIDTH - CAIRO_DOCK_PREVIEW_WIDTH_MIN) / (CAIRO_DOCK_CONF_PANEL_WIDTH - CAIRO_DOCK_CONF_PANEL_WIDTH_MIN);
		double b = CAIRO_DOCK_PREVIEW_WIDTH_MIN - CAIRO_DOCK_CONF_PANEL_WIDTH_MIN * a;
		s_iPreviewWidth = a * g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] + b;
		s_iNbButtonsByRow = CAIRO_DOCK_NB_BUTTONS_BY_ROW - 1;
	}
	else
	{
		s_iPreviewWidth = CAIRO_DOCK_PREVIEW_WIDTH_MIN;
		s_iNbButtonsByRow = CAIRO_DOCK_NB_BUTTONS_BY_ROW_MIN;
	}
	GtkWidget *pCategoriesVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_widget_set_size_request (pCategoriesVBox, s_iPreviewWidth+2*CAIRO_DOCK_GUI_MARGIN, CAIRO_DOCK_PREVIEW_HEIGHT);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pCategoriesVBox,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pVBox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pVBox,
		TRUE,
		TRUE,
		0);
	s_pGroupsVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_TABLE_MARGIN);
	GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), s_pGroupsVBox);
	gtk_box_pack_start (GTK_BOX (pVBox),
		pScrolledWindow,
		TRUE,
		TRUE,
		0);
	
	if (g_iDesktopEnv != CAIRO_DOCK_KDE && g_iDesktopEnv != CAIRO_DOCK_UNKNOWN_ENV && mySystem.bConfigPanelTransparency)
	{
		cairo_dock_set_colormap_for_window (s_pMainWindow);
		gtk_widget_set_app_paintable (s_pMainWindow, TRUE);
		g_signal_connect (G_OBJECT (s_pMainWindow),
			"expose-event",
			G_CALLBACK (on_expose),
			NULL);
	}
	/*g_signal_connect (G_OBJECT (s_pGroupsVBox),
		"expose-event",
		G_CALLBACK (on_expose),
		NULL);*/
		
	//\_____________ On construit les boutons de chaque categorie.
	GtkWidget *pCategoriesFrame = gtk_frame_new (NULL);
	gtk_container_set_border_width (GTK_CONTAINER (pCategoriesFrame), CAIRO_DOCK_GUI_MARGIN);
	gtk_frame_set_shadow_type (GTK_FRAME (pCategoriesFrame), GTK_SHADOW_OUT);
	
	GtkWidget *pLabel;
	gchar *cLabel = g_strdup_printf ("<span font_desc=\"Sans 12\" color=\"#81728C\"><b><u>%s :</u></b></span>", _("Categories"));
	pLabel = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
	g_free (cLabel);
	gtk_frame_set_label_widget (GTK_FRAME (pCategoriesFrame), pLabel);
	
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pCategoriesFrame,
		FALSE,
		FALSE,
		0);
	
	s_pToolBar = gtk_toolbar_new ();
	gtk_toolbar_set_orientation (GTK_TOOLBAR (s_pToolBar), GTK_ORIENTATION_VERTICAL);
	gtk_toolbar_set_style (GTK_TOOLBAR (s_pToolBar), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (s_pToolBar), FALSE);
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (s_pToolBar), GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_container_add (GTK_CONTAINER (pCategoriesFrame), s_pToolBar);
	
	CairoDockCategoryWidgetTable *pCategoryWidget;
	GtkToolItem *pCategoryButton;
	pCategoryWidget = &s_pCategoryWidgetTables[CAIRO_DOCK_NB_CATEGORY];
	pCategoryButton = _make_toolbutton (_("All"),
		"icon-all.svg",
		CAIRO_DOCK_CATEGORY_ICON_SIZE);
	g_signal_connect (G_OBJECT (pCategoryButton), "clicked", G_CALLBACK(on_click_all_button), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (s_pToolBar) , pCategoryButton, -1);
	pCategoryWidget->pCategoryButton = pCategoryButton;
	
	guint i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryButton = _make_toolbutton (gettext (s_cCategoriesDescription[2*i]),
			s_cCategoriesDescription[2*i+1],
			CAIRO_DOCK_CATEGORY_ICON_SIZE);
		g_signal_connect (G_OBJECT (pCategoryButton), "clicked", G_CALLBACK(on_click_category_button), GINT_TO_POINTER (i));
		gtk_toolbar_insert (GTK_TOOLBAR (s_pToolBar) , pCategoryButton,-1);
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		pCategoryWidget->pCategoryButton = pCategoryButton;
	}
	
	//\_____________ On construit les widgets table de chaque categorie.
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		
		pCategoryWidget->pFrame = gtk_frame_new (NULL);
		gtk_container_set_border_width (GTK_CONTAINER (pCategoryWidget->pFrame), CAIRO_DOCK_GUI_MARGIN);
		gtk_frame_set_shadow_type (GTK_FRAME (pCategoryWidget->pFrame), GTK_SHADOW_OUT);
		
		pLabel = gtk_label_new (NULL);
		cLabel = g_strdup_printf ("<span font_desc=\"Sans 12\"><b>%s</b></span>", gettext (s_cCategoriesDescription[2*i]));
		gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
		g_free (cLabel);
		gtk_frame_set_label_widget (GTK_FRAME (pCategoryWidget->pFrame), pLabel);
		
		pCategoryWidget->pTable = gtk_table_new (1,
			s_iNbButtonsByRow,
			TRUE);
		gtk_table_set_row_spacings (GTK_TABLE (pCategoryWidget->pTable), CAIRO_DOCK_GUI_MARGIN);
		gtk_table_set_col_spacings (GTK_TABLE (pCategoryWidget->pTable), CAIRO_DOCK_GUI_MARGIN);
		gtk_container_add (GTK_CONTAINER (pCategoryWidget->pFrame),
			pCategoryWidget->pTable);
		gtk_box_pack_start (GTK_BOX (s_pGroupsVBox),
			pCategoryWidget->pFrame,
			FALSE,
			FALSE,
			0);
	}
	
	
	//\_____________ On remplit avec les groupes du fichier.
	GError *erreur = NULL;
	GKeyFile* pKeyFile = g_key_file_new();
	g_key_file_load_from_file (pKeyFile, cConfFilePath, 0, &erreur);  // inutile de garder les commentaires ici.
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cConfFilePath, erreur->message);
		g_error_free (erreur);
		g_key_file_free (pKeyFile);
		return NULL;
	}
	
	gsize length = 0;
	gchar **pGroupList = g_key_file_get_groups (pKeyFile, &length);
	gchar *cGroupName;
	gint iActive;
	CairoDockInternalModule *pInternalModule;
	gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_CONF_FILE);
	for (i = 0; i < length; i ++)
	{
		cGroupName = pGroupList[i];
		pInternalModule = cairo_dock_find_internal_module_from_name (cGroupName);
		if (pInternalModule == NULL)
			continue;
		
		if (g_key_file_has_key (pKeyFile, cGroupName, "active", NULL))
			iActive = g_key_file_get_boolean (pKeyFile, cGroupName, "active", NULL);
		else
			iActive = -1;  // <=> non desactivable
		
		_add_group_button (cGroupName,
			pInternalModule->cIcon,
			pInternalModule->iCategory,
			pInternalModule->cDescription,
			NULL,  // pas de prevue
			iActive,
			TRUE,  // <=> configurable
			cOriginalConfFilePath,
			NULL,  // domaine de traduction : celui du dock.
			pInternalModule->cDependencies,
			pInternalModule->pExternalModules,
			NULL,
			pInternalModule->cTitle);
	}
	g_free (cOriginalConfFilePath);
	g_strfreev (pGroupList);
	
	
	//\_____________ On remplit avec les modules.
	gchar *cActiveModules = (g_pMainDock == NULL ? g_key_file_get_string (pKeyFile, "System", "modules", NULL) : NULL);
	cairo_dock_foreach_module_in_alphabetical_order ((GCompareFunc) _cairo_dock_add_one_module_widget, cActiveModules);
	g_free (cActiveModules);
	g_key_file_free (pKeyFile);
	
	//\_____________ On ajoute le filtre.
	// frame
	GtkWidget *pFilterFrame = gtk_frame_new (NULL);
	cLabel = g_strdup_printf ("<span font_desc=\"Sans 12\" color=\"#81728C\"><b><u>%s :</u></b></span>", _("Filter"));
	GtkWidget *pFilterLabelContainer = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	GtkWidget *pImage = gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (pFilterLabelContainer), pImage);
	
	pLabel = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
	g_free (cLabel);
	gtk_container_add (GTK_CONTAINER (pFilterLabelContainer), pLabel);
	
	gtk_frame_set_label_widget (GTK_FRAME (pFilterFrame), pFilterLabelContainer);
	gtk_container_set_border_width (GTK_CONTAINER (pFilterFrame), CAIRO_DOCK_GUI_MARGIN);
	gtk_frame_set_shadow_type (GTK_FRAME (pFilterFrame), GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pFilterFrame,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pOptionVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_container_add (GTK_CONTAINER (pFilterFrame), pOptionVBox);
	
	// entree de texte
	GtkWidget *pFilterBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_box_pack_start (GTK_BOX (pOptionVBox),
		pFilterBox,
		FALSE,
		FALSE,
		0);
	s_pFilterEntry = gtk_entry_new ();
	g_signal_connect (s_pFilterEntry, "activate", G_CALLBACK (on_activate_filter), NULL);
	gtk_box_pack_start (GTK_BOX (pFilterBox),
		s_pFilterEntry,
		FALSE,
		FALSE,
		0);
	GtkWidget *pClearButton = gtk_button_new ();
	_add_image_on_button (pClearButton,
		GTK_STOCK_CLEAR,
		16);
	g_signal_connect (pClearButton, "clicked", G_CALLBACK (on_clear_filter), s_pFilterEntry);
	gtk_box_pack_start (GTK_BOX (pFilterBox),
		pClearButton,
		TRUE,  // sinon le bouton est repousse en-dehors de la marge.
		FALSE,
		0);
	
	// options
	_reset_filter_state ();
	
	GtkWidget *pMenuBar = gtk_menu_bar_new ();
	GtkWidget *pMenuItem = gtk_menu_item_new_with_label (_("Options"));
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenuBar), pMenuItem);
	gtk_box_pack_start (GTK_BOX (pOptionVBox),
		pMenuBar,
		FALSE,
		FALSE,
		0);
	GtkWidget *pMenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pMenu);
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("All words"));
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	g_signal_connect (pMenuItem, "toggled", G_CALLBACK (on_toggle_all_words), NULL);
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("Highlight words"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (pMenuItem), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	g_signal_connect (pMenuItem, "toggled", G_CALLBACK (on_toggle_highlight_words), NULL);
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("Hide others"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (pMenuItem), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	g_signal_connect (pMenuItem, "toggled", G_CALLBACK (on_toggle_hide_others), NULL);
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("Search in description"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (pMenuItem), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	g_signal_connect (pMenuItem, "toggled", G_CALLBACK (on_toggle_search_in_tooltip), NULL);
	
	//\_____________ On ajoute le cadre d'activation du module.
	s_pGroupFrame = gtk_frame_new ("pouet");
	gtk_container_set_border_width (GTK_CONTAINER (s_pGroupFrame), CAIRO_DOCK_GUI_MARGIN);
	gtk_frame_set_shadow_type (GTK_FRAME (s_pGroupFrame), GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		s_pGroupFrame,
		FALSE,
		FALSE,
		0);
	s_pActivateButton = gtk_check_button_new_with_label (_("Activate this module"));
	g_signal_connect (G_OBJECT (s_pActivateButton), "clicked", G_CALLBACK(on_click_activate_current_group), NULL);
	gtk_container_add (GTK_CONTAINER (s_pGroupFrame), s_pActivateButton);
	gtk_widget_show_all (s_pActivateButton);
	
	//\_____________ On ajoute la zone de prevue.
	GtkWidget *pInfoVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pInfoVBox,
		FALSE,
		FALSE,
		0);
	
	s_pPreviewImage = gtk_image_new_from_pixbuf (NULL);
	gtk_container_add (GTK_CONTAINER (pInfoVBox), s_pPreviewImage);
	
	//\_____________ On ajoute les boutons.
	GtkWidget *pButtonsHBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_box_pack_end (GTK_BOX (pVBox),
		pButtonsHBox,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pQuitButton = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect (G_OBJECT (pQuitButton), "clicked", G_CALLBACK(on_click_quit), s_pMainWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		pQuitButton,
		FALSE,
		FALSE,
		0);
	
	s_pBackButton = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	g_signal_connect (G_OBJECT (s_pBackButton), "clicked", G_CALLBACK(on_click_back_button), NULL);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		s_pBackButton,
		FALSE,
		FALSE,
		0);
	
	s_pOkButton = gtk_button_new_from_stock (GTK_STOCK_OK);
	g_signal_connect (G_OBJECT (s_pOkButton), "clicked", G_CALLBACK(on_click_ok), s_pMainWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		s_pOkButton,
		FALSE,
		FALSE,
		0);
	
	s_pApplyButton = gtk_button_new_from_stock (GTK_STOCK_APPLY);
	g_signal_connect (G_OBJECT (s_pApplyButton), "clicked", G_CALLBACK(on_click_apply), NULL);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		s_pApplyButton,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pSwitchButton = cairo_dock_make_switch_gui_button ();
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		pSwitchButton,
		FALSE,
		FALSE,
		0);
	
	//\_____________ On ajoute la barre de status a la fin.
	s_pStatusBar = gtk_statusbar_new ();
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (s_pStatusBar), FALSE);
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		s_pStatusBar,
		TRUE,
		TRUE,
		0);
	g_object_set_data (G_OBJECT (s_pMainWindow), "status-bar", s_pStatusBar);
	
	gtk_window_resize (GTK_WINDOW (s_pMainWindow),
		MIN (CAIRO_DOCK_CONF_PANEL_WIDTH, g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL]),
		MIN (CAIRO_DOCK_CONF_PANEL_HEIGHT, g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - (g_pMainDock && g_pMainDock->container.bIsHorizontal ? g_pMainDock->iMaxDockHeight : 0)));
	
	
	gtk_widget_show_all (s_pMainWindow);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pGroupFrame);
	gtk_widget_hide (s_pPreviewImage);
	
	cairo_dock_dialog_window_created ();
	
	if (bMaintenanceMode)
	{
		gtk_window_set_title (GTK_WINDOW (s_pMainWindow), _("< Maintenance mode >"));
		GMainLoop *pBlockingLoop = g_main_loop_new (NULL, FALSE);
		g_object_set_data (G_OBJECT (s_pMainWindow), "loop", pBlockingLoop);
		g_signal_connect (s_pMainWindow,
			"delete-event",
			G_CALLBACK (on_delete_main_gui),
			pBlockingLoop);
		
		g_print ("debut de boucle bloquante ...\n");
		GDK_THREADS_LEAVE ();
		g_main_loop_run (pBlockingLoop);
		GDK_THREADS_ENTER ();
		g_print ("fin de boucle bloquante\n");
		
		g_main_loop_unref (pBlockingLoop);
	}
	else
	{
		//gtk_window_set_title (GTK_WINDOW (s_pMainWindow), _("Configuration of Cairo-Dock"));
		///cairo_dock_show_one_category (0);
		g_signal_connect (G_OBJECT (s_pMainWindow),
			"delete-event",
			G_CALLBACK (on_delete_main_gui),
			NULL);
	}
	return s_pMainWindow;
}

  ////////////////
 // CATEGORIES //
////////////////

static void cairo_dock_hide_all_categories (void)
{
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		gtk_widget_hide (pCategoryWidget->pFrame);
	}
	
	gtk_widget_show (s_pOkButton);
	gtk_widget_show (s_pApplyButton);
}

static void cairo_dock_show_all_categories (void)
{
	if (s_pStatusBar)
		gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);
	
	//\_______________ On detruit le widget du groupe courant.
	if (s_pCurrentGroupWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentGroupWidget);
		s_pCurrentGroupWidget = NULL;
		cairo_dock_reset_current_widget_list ();
		
		s_pCurrentGroup = NULL;
	}

	//\_______________ On montre chaque module.
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		gtk_widget_show_all (pCategoryWidget->pFrame);
		g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), FALSE);
		g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
	}
	pCategoryWidget = &s_pCategoryWidgetTables[i];
	g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), TRUE);
	g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pGroupFrame);
	
	//\_______________ On actualise le titre de la fenetre.
	gtk_window_set_title (GTK_WINDOW (s_pMainWindow), _("Configuration of Cairo-Dock"));
	if (s_path == NULL || s_path->data != GINT_TO_POINTER (0))
		s_path = g_slist_prepend (s_path, GINT_TO_POINTER (0));

	//\_______________ On declenche le filtre.
	_trigger_current_filter ();
}

static void cairo_dock_show_one_category (int iCategory)
{
	if (s_pStatusBar)
		gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);
	
	//\_______________ On detruit le widget du groupe courant.
	if (s_pCurrentGroupWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentGroupWidget);
		s_pCurrentGroupWidget = NULL;
		cairo_dock_reset_current_widget_list ();
		s_pCurrentGroup = NULL;
	}

	//\_______________ On montre chaque module de la categorie.
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		if (i != iCategory)
			gtk_widget_hide (pCategoryWidget->pFrame);
		else
			gtk_widget_show_all (pCategoryWidget->pFrame);
		
		g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), (i == iCategory));
		g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
	}
	pCategoryWidget = &s_pCategoryWidgetTables[i];
	g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), FALSE);
	g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pGroupFrame);
	
	//\_______________ On actualise le titre de la fenetre.
	gtk_window_set_title (GTK_WINDOW (s_pMainWindow), gettext (s_cCategoriesDescription[2*iCategory]));
	if (s_path == NULL || s_path->data != GINT_TO_POINTER (iCategory+1))
		s_path = g_slist_prepend (s_path, GINT_TO_POINTER (iCategory+1));

	//\_______________ On declenche le filtre.
	_trigger_current_filter ();
}

static void cairo_dock_toggle_category_button (int iCategory)
{
	if (s_pMainWindow == NULL || s_pCurrentGroup == NULL || s_pCurrentGroup->cGroupName == NULL || s_pCurrentWidgetList == NULL)
		return ;
	
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), (i == iCategory));
		g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
	}
	pCategoryWidget = &s_pCategoryWidgetTables[i];
	g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), FALSE);
	g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
}


static void cairo_dock_insert_extern_widget_in_gui (GtkWidget *pWidget)
{
	if (s_pCurrentGroupWidget != NULL)
		gtk_widget_destroy (s_pCurrentGroupWidget);
	s_pCurrentGroupWidget = pWidget;
	
	gtk_box_pack_start (GTK_BOX (s_pGroupsVBox),
		pWidget,
		TRUE,
		TRUE,
		CAIRO_DOCK_GUI_MARGIN);
	gtk_widget_show_all (pWidget);
}

static void _reload_current_module_widget (CairoDockModuleInstance *pInstance)
{
	g_return_if_fail (s_pCurrentGroupWidget != NULL && s_pCurrentGroup != NULL && s_pCurrentWidgetList != NULL);
	
	int iNotebookPage = (GTK_IS_NOTEBOOK (s_pCurrentGroupWidget) ? 
			gtk_notebook_get_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget)) :
			-1);
	
	gtk_widget_destroy (s_pCurrentGroupWidget);
	s_pCurrentGroupWidget = NULL;
	
	cairo_dock_reset_current_widget_list ();
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (s_pCurrentGroup->cGroupName);
	GtkWidget *pWidget;
	if (pModule != NULL)
	{
		pWidget = cairo_dock_present_group_widget (pModule->cConfFilePath, s_pCurrentGroup, FALSE, pInstance);
	}
	else
	{
		pWidget = cairo_dock_present_group_widget (g_cConfFile, s_pCurrentGroup, TRUE, NULL);
	}
	if (iNotebookPage != -1)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget), iNotebookPage);
	}
}
static GtkWidget *cairo_dock_present_group_widget (const gchar *cConfFilePath, CairoDockGroupDescription *pGroupDescription, gboolean bSingleGroup, CairoDockModuleInstance *pInstance)
{
	cd_debug ("%s (%s, %s)", __func__, cConfFilePath, pGroupDescription->cGroupName);
	//cairo_dock_set_status_message (NULL, "");
	if (s_pStatusBar)
		gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);
	g_free (pGroupDescription->cConfFilePath);
	pGroupDescription->cConfFilePath = g_strdup (cConfFilePath);
	
	//\_______________ On cree le widget du groupe.
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	cairo_dock_reset_current_widget_list ();
	
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	GtkWidget *pWidget;
	if (bSingleGroup)
	{
		pWidget = cairo_dock_build_group_widget (pKeyFile,
			pGroupDescription->cGroupName,
			pGroupDescription->cGettextDomain,
			s_pMainWindow,
			&s_pCurrentWidgetList,
			pDataGarbage,
			pGroupDescription->cOriginalConfFilePath);
	}
	else
	{
		pWidget = cairo_dock_build_key_file_widget (pKeyFile,
			pGroupDescription->cGettextDomain,
			s_pMainWindow,
			&s_pCurrentWidgetList,
			pDataGarbage,
			pGroupDescription->cOriginalConfFilePath);
	}
	
	//\_______________ On complete avec les modules additionnels.
	if (pGroupDescription->pExternalModules != NULL)
	{
		// on cree les widgets de tous les modules externes dans un meme notebook.
		g_print ("on cree les widgets de tous les modules externes\n");
		GtkWidget *pNoteBook = NULL;
		GKeyFile* pExtraKeyFile;
		CairoDockModule *pModule;
		CairoDockModuleInstance *pExtraInstance;
		GSList *pExtraCurrentWidgetList = NULL;
		CairoDockGroupDescription *pExtraGroupDescription;
		GList *p;
		for (p = pGroupDescription->pExternalModules; p != NULL; p = p->next)
		{
			//g_print (" + %s\n", p->data);
			pModule = cairo_dock_find_module_from_name (p->data);
			if (pModule == NULL)
				continue;
			if (pModule->cConfFilePath == NULL)  // on n'est pas encore passe par la dans le cas ou le plug-in n'a pas ete active; mais on veut pouvoir configurer un plug-in meme lorsqu'il est inactif.
			{
				pModule->cConfFilePath = cairo_dock_check_module_conf_file (pModule->pVisitCard);
			}
			//g_print (" + %s\n", pModule->cConfFilePath);

			pExtraGroupDescription = cairo_dock_find_module_description (p->data);
			//g_print (" pExtraGroupDescription : %x\n", pExtraGroupDescription);
			if (pExtraGroupDescription == NULL)
				continue;

			pExtraKeyFile = cairo_dock_open_key_file (pModule->cConfFilePath);
			if (pExtraKeyFile == NULL)
				continue;
			
			pExtraCurrentWidgetList = NULL;
			pNoteBook = cairo_dock_build_key_file_widget (pExtraKeyFile,
				pExtraGroupDescription->cGettextDomain,
				s_pMainWindow,
				&pExtraCurrentWidgetList,
				pDataGarbage,
				pExtraGroupDescription->cOriginalConfFilePath);  /// TODO : fournir pNoteBook a la fonction.
			s_pExtraCurrentWidgetList = g_slist_prepend (s_pExtraCurrentWidgetList, pExtraCurrentWidgetList);
			
			pExtraInstance = (pModule->pInstancesList ? pModule->pInstancesList->data : NULL);  // il y'a la une limitation : les modules attaches a un module interne ne peuvent etre instancie plus d'une fois. Dans la pratique ca ne pose aucun probleme.
			if (pExtraInstance != NULL && pExtraGroupDescription->load_custom_widget != NULL)
				pExtraGroupDescription->load_custom_widget (pExtraInstance, pKeyFile);
			g_key_file_free (pExtraKeyFile);
		}
		
		// on rajoute la page du module interne en 1er dans le notebook.
		if (pNoteBook != NULL)
		{
			GtkWidget *pLabel = gtk_label_new (dgettext (pGroupDescription->cGettextDomain, pGroupDescription->cTitle));
			GtkWidget *pLabelContainer = NULL;
			GtkWidget *pAlign = NULL;
			if (pGroupDescription->cIcon != NULL && *pGroupDescription->cIcon != '\0')
			{
				pLabelContainer = gtk_hbox_new (FALSE, CAIRO_DOCK_ICON_MARGIN);
				pAlign = gtk_alignment_new (0., 0.5, 0., 0.);
				gtk_container_add (GTK_CONTAINER (pAlign), pLabelContainer);

				GtkWidget *pImage = gtk_image_new ();
				GdkPixbuf *pixbuf;
				if (*pGroupDescription->cIcon != '/')
					pixbuf = gtk_widget_render_icon (pImage,
						pGroupDescription->cIcon ,
						GTK_ICON_SIZE_BUTTON,
						NULL);
				else
					pixbuf = gdk_pixbuf_new_from_file_at_size (pGroupDescription->cIcon, CAIRO_DOCK_TAB_ICON_SIZE, CAIRO_DOCK_TAB_ICON_SIZE, NULL);
				if (pixbuf != NULL)
				{
					gtk_image_set_from_pixbuf (GTK_IMAGE (pImage), pixbuf);
					gdk_pixbuf_unref (pixbuf);
				}
				gtk_container_add (GTK_CONTAINER (pLabelContainer),
					pImage);
				gtk_container_add (GTK_CONTAINER (pLabelContainer), pLabel);
				gtk_widget_show_all (pLabelContainer);
			}
			
			GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
			gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
			gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pWidget);
			
			gtk_notebook_prepend_page (GTK_NOTEBOOK (pNoteBook), pScrolledWindow, (pAlign != NULL ? pAlign : pLabel));
			pWidget = pNoteBook;
		}
	}
	
	if (pInstance != NULL && pGroupDescription->load_custom_widget != NULL)
		pGroupDescription->load_custom_widget (pInstance, pKeyFile);
	g_key_file_free (pKeyFile);

	//\_______________ On affiche le widget du groupe dans l'interface.
	cairo_dock_hide_all_categories ();
	
	cairo_dock_insert_extern_widget_in_gui (pWidget);  // devient le widget courant.
	
	s_pCurrentGroup = pGroupDescription;
	
	gtk_window_set_title (GTK_WINDOW (s_pMainWindow), dgettext (pGroupDescription->cGettextDomain, pGroupDescription->cTitle));
	
	//\_______________ On met a jour la frame du groupe (label + check-button).
	GtkWidget *pLabel = gtk_label_new (NULL);
	gchar *cLabel = g_strdup_printf ("<span font_desc=\"Sans 12\" color=\"#81728C\"><u><b>%s</b></u></span>", dgettext (pGroupDescription->cGettextDomain, pGroupDescription->cTitle));
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
	g_free (cLabel);
	gtk_frame_set_label_widget (GTK_FRAME (s_pGroupFrame), pLabel);
	gtk_widget_show_all (s_pGroupFrame);
	
	g_signal_handlers_block_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	if (pModule != NULL && pModule->pInterface->stopModule != NULL)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (s_pActivateButton), pModule->pInstancesList != NULL);
		gtk_widget_set_sensitive (s_pActivateButton, TRUE);
	}
	else
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (s_pActivateButton), TRUE);
		gtk_widget_set_sensitive (s_pActivateButton, FALSE);
	}
	g_signal_handlers_unblock_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
	if (s_path == NULL || s_path->data != pGroupDescription)
		s_path = g_slist_prepend (s_path, pGroupDescription);
	
	//\_______________ On declenche le filtre.
	_trigger_current_filter ();
	
	//\_______________ On gere les dependances.
	if (pGroupDescription->cDependencies != NULL && ! pGroupDescription->bIgnoreDependencies && g_pMainDock != NULL)
	{
		pGroupDescription->bIgnoreDependencies = TRUE;  // cette fonction re-entrante.
		gboolean bReload = FALSE;
		const gchar *cModuleName, *cMessage;
		CairoDockModule *pDependencyModule;
		GError *erreur = NULL;
		int i;
		for (i = 0; pGroupDescription->cDependencies[i] != NULL; i += 2)
		{
			cModuleName = pGroupDescription->cDependencies[i];
			cMessage = pGroupDescription->cDependencies[i+1];
			
			pDependencyModule = cairo_dock_find_module_from_name (cModuleName);
			if (pDependencyModule == NULL)
			{
				Icon *pIcon = cairo_dock_get_current_active_icon ();
				if (pIcon == NULL)
					pIcon = cairo_dock_get_dialogless_icon ();
				CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon != NULL ? pIcon->cParentDockName : NULL);
				cairo_dock_show_temporary_dialog_with_icon_printf (_("The module '%s' is not present. You need to install it or its dependencies to make the most of this module."), pIcon, CAIRO_CONTAINER (pDock), 10, "same icon", cModuleName);
			}
			else if (pDependencyModule != pModule)
			{
				if (pDependencyModule->pInstancesList == NULL && pDependencyModule->pInterface->initModule != NULL)
				{
					gchar *cWarning = g_strdup_printf (_("The module '%s' is not activated."), cModuleName);
					gchar *cQuestion = g_strdup_printf ("%s\n%s\n%s", cWarning, gettext (cMessage), _("Do you want to activate it now ?"));
					int iAnswer = cairo_dock_ask_general_question_and_wait (cQuestion);
					g_free (cQuestion);
					g_free (cWarning);
					
					if (iAnswer == GTK_RESPONSE_YES)
					{
						cairo_dock_activate_module (pDependencyModule, &erreur);
						if (erreur != NULL)
						{
							cd_warning (erreur->message);
							g_error_free (erreur);
							erreur = NULL;
						}
						else
							bReload = TRUE;
					}
				}
			}
		}
		if (bReload)
			_reload_current_module_widget (NULL);
		pGroupDescription->bIgnoreDependencies = FALSE;
	}
	
	return pWidget;
}


static CairoDockGroupDescription *cairo_dock_find_module_description (const gchar *cModuleName)
{
	g_return_val_if_fail (cModuleName != NULL, NULL);
	CairoDockGroupDescription *pGroupDescription = NULL;
	GList *pElement = NULL;
	for (pElement = s_pGroupDescriptionList; pElement != NULL; pElement = pElement->next)
	{
		pGroupDescription = pElement->data;
		if (strcmp (pGroupDescription->cGroupName, cModuleName) == 0)
			return pGroupDescription ;
	}
	return NULL;
}

static void cairo_dock_present_module_gui (CairoDockModule *pModule)
{
	g_return_if_fail (pModule != NULL);
	
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (pModule->pVisitCard->cModuleName);
	if (pGroupDescription == NULL)
		return ;
	
	CairoDockModuleInstance *pModuleInstance = (pModule->pInstancesList != NULL ? pModule->pInstancesList->data : NULL);
	gchar *cConfFilePath = (pModuleInstance != NULL ? pModuleInstance->cConfFilePath : pModule->cConfFilePath);
	cairo_dock_present_group_widget (cConfFilePath, pGroupDescription, FALSE, pModuleInstance);
}

static void cairo_dock_present_module_instance_gui (CairoDockModuleInstance *pModuleInstance)
{
	g_return_if_fail (pModuleInstance != NULL);
	
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (pModuleInstance->pModule->pVisitCard->cModuleName);
	if (pGroupDescription == NULL)
		return ;
	
	cairo_dock_present_group_widget (pModuleInstance->cConfFilePath, pGroupDescription, FALSE, pModuleInstance);
}

static void cairo_dock_show_group (CairoDockGroupDescription *pGroupDescription)
{
	g_return_if_fail (pGroupDescription != NULL);
	gboolean bSingleGroup;
	gchar *cConfFilePath;
	CairoDockModuleInstance *pModuleInstance = NULL;
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	if (pModule == NULL)  // c'est un groupe du fichier de conf principal.
	{
		cConfFilePath = g_cConfFile;
		bSingleGroup = TRUE;
	}
	else  // c'est un module, on recupere son fichier de conf en entier.
	{
		if (pModule->cConfFilePath == NULL)  // on n'est pas encore passe par la dans le cas ou le plug-in n'a pas ete active; mais on veut pouvoir configurer un plug-in meme lorsqu'il est inactif.
		{
			pModule->cConfFilePath = cairo_dock_check_module_conf_file (pModule->pVisitCard);
		}
		if (pModule->cConfFilePath == NULL)
		{
			cd_warning ("couldn't load a conf file for this module => can't configure it.");
			return;
		}
		
		pModuleInstance = (pModule->pInstancesList != NULL ? pModule->pInstancesList->data : NULL);
		cConfFilePath = (pModuleInstance != NULL ? pModuleInstance->cConfFilePath : pModule->cConfFilePath);
		
		bSingleGroup = FALSE;
	}
	
	cairo_dock_present_group_widget (cConfFilePath, pGroupDescription, bSingleGroup, pModuleInstance);
}


static void cairo_dock_apply_current_filter (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther)
{
	cd_debug ("");
	if (s_pCurrentGroup != NULL)
	{
		cairo_dock_apply_filter_on_group_widget (pKeyWords, bAllWords, bSearchInToolTip, bHighLightText, bHideOther, s_pCurrentWidgetList);
		if (s_pExtraCurrentWidgetList != NULL)
		{
			GSList *l;
			for (l = s_pExtraCurrentWidgetList; l != NULL; l = l->next)
			{
				cairo_dock_apply_filter_on_group_widget (pKeyWords, bAllWords, bSearchInToolTip, bHighLightText, bHideOther, l->data);
			}
		}
	}
	else
	{
		cairo_dock_apply_filter_on_group_list (pKeyWords, bAllWords, bSearchInToolTip, bHighLightText, bHideOther, s_pGroupDescriptionList);
	}
}



static void _present_help_from_dialog (int iClickedButton, GtkWidget *pInteractiveWidget, gpointer data, CairoDialog *pDialog)
{
	if (iClickedButton == 0 || iClickedButton == -1)  // click OK or press Enter.
	{
		CairoDockModule *pModule = cairo_dock_find_module_from_name ("Help");
		g_return_if_fail (pModule != NULL);
		cairo_dock_build_main_ihm (g_cConfFile, FALSE);
		cairo_dock_present_module_gui (pModule);
	}
}
static GtkWidget * show_main_gui (void)
{
	GtkWidget *pWindow = cairo_dock_build_main_ihm (g_cConfFile, FALSE);
	CairoDockModule *pModule = cairo_dock_find_module_from_name ("Help");
	if (pModule != NULL)
	{
		gchar *cHelpHistory = g_strdup_printf ("%s/.help/entered-once", g_cCairoDockDataDir);
		if (! g_file_test (cHelpHistory, G_FILE_TEST_EXISTS))
		{
			Icon *pIcon = cairo_dock_get_dialogless_icon ();
			cairo_dock_show_dialog_full (_("It seems that you've never entered the help module yet.\nIf you have some difficulty to configure the dock, or if you are willing to customize it,\nthe Help module is here for you !\nDo you want to take a look at it now ?"),
				pIcon,
				CAIRO_CONTAINER (g_pMainDock),
				10e3,
				CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON,
				NULL,
				(CairoDockActionOnAnswerFunc) _present_help_from_dialog,
				NULL,
				NULL);
		}
	}
	return pWindow;
}

  /////////////
 // BACKEND //
/////////////

static void show_module_instance_gui (CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	int iNotebookPage = (GTK_IS_NOTEBOOK (s_pCurrentGroupWidget) ? 
		(iShowPage >= 0 ?
			iShowPage :
			gtk_notebook_get_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget))) :
		-1);
	
	cairo_dock_build_main_ihm (g_cConfFile, FALSE);
	cairo_dock_present_module_instance_gui (pModuleInstance);
	cairo_dock_toggle_category_button (pModuleInstance->pModule->pVisitCard->iCategory);  // on active la categorie du module.
	
	if (iNotebookPage != -1)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget), iNotebookPage);
	}
}

static void show_module_gui (const gchar *cModuleName)
{
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cModuleName);
	if (pGroupDescription == NULL)
	{
		cairo_dock_build_main_ihm (g_cConfFile, FALSE);
		pGroupDescription = cairo_dock_find_module_description (cModuleName);
		g_return_if_fail (pGroupDescription != NULL);
	}
	
	cairo_dock_show_group (pGroupDescription);
}

static gboolean module_is_opened (CairoDockModuleInstance *pModuleInstance)
{
	if (s_pMainWindow == NULL || pModuleInstance == NULL || s_pCurrentGroup == NULL || s_pCurrentGroup->cGroupName == NULL || s_pCurrentWidgetList == NULL)
		return FALSE;
	
	if (strcmp (pModuleInstance->pModule->pVisitCard->cModuleName, s_pCurrentGroup->cGroupName) != 0)  // on est en train d'editer ce module dans le panneau de conf.
		return FALSE;
	
	gchar *cConfFilePath = g_object_get_data (G_OBJECT (s_pMainWindow), "conf-file");
	if (cConfFilePath == NULL)
		return FALSE;
	CairoDockModuleInstance *pInstance;
	GList *pElement;
	for (pElement = pModuleInstance->pModule->pInstancesList; pElement != NULL; pElement= pElement->next)
	{
		pInstance = pElement->data;
		if (pInstance->cConfFilePath && strcmp (pModuleInstance->cConfFilePath, pInstance->cConfFilePath) == 0)
			return TRUE;
	}
	return FALSE;
}

static void deactivate_module_in_gui (const gchar *cModuleName)
{
	if (s_pMainWindow == NULL || cModuleName == NULL)
		return ;
	
	if (s_pCurrentGroup != NULL && s_pCurrentGroup->cGroupName != NULL && strcmp (cModuleName, s_pCurrentGroup->cGroupName) == 0)  // on est en train d'editer ce module dans le panneau de conf.
	{
		g_signal_handlers_block_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (s_pActivateButton), FALSE);
		g_signal_handlers_unblock_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
	}
	
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cModuleName);
	g_return_if_fail (pGroupDescription != NULL && pGroupDescription->pActivateButton != NULL);
	g_signal_handlers_block_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pGroupDescription->pActivateButton), FALSE);
	g_signal_handlers_unblock_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
}

static void set_status_message_on_gui (const gchar *cMessage)
{
	if (s_pStatusBar == NULL)
		return ;
	gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);  // clear any previous message, underflow is allowed.
	gtk_statusbar_push (GTK_STATUSBAR (s_pStatusBar), 0, cMessage);
}

static GSList *get_widgets_from_name (const gchar *cGroupName, const gchar *cKeyName)
{
	g_return_val_if_fail (s_pCurrentWidgetList != NULL, NULL);
	GSList *pList = cairo_dock_find_widgets_from_name (s_pCurrentWidgetList, cGroupName, cKeyName);
	if (pList == NULL && s_pExtraCurrentWidgetList != NULL)
	{
		GSList *l;
		for (l = s_pExtraCurrentWidgetList; l != NULL; l = l->next)
		{
			pList = cairo_dock_find_widgets_from_name (l->data, cGroupName, cKeyName);
			if (pList != NULL)
				break ;
		}
	}
	return pList;
}

static void close_gui (void)
{
	if (s_pMainWindow != NULL)
	{
		on_click_quit (NULL, s_pMainWindow);
	}
}

void cairo_dock_register_main_gui_backend (void)
{
	CairoDockGuiBackend *pBackend = g_new0 (CairoDockGuiBackend, 1);
	
	pBackend->show_main_gui 		= show_main_gui;
	pBackend->show_module_instance_gui 	= show_module_instance_gui;
	pBackend->show_module_gui 		= show_module_gui;
	pBackend->set_status_message_on_gui = set_status_message_on_gui;
	pBackend->module_is_opened 		= module_is_opened;
	pBackend->deactivate_module_in_gui 	= deactivate_module_in_gui;
	pBackend->get_widgets_from_name 	= get_widgets_from_name;
	pBackend->close_gui 			= close_gui;
	
	cairo_dock_register_gui_backend (pBackend);
}

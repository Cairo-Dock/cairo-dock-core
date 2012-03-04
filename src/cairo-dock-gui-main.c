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

#include "config.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-commons.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-gui-items.h"
#include "cairo-dock-gui-main.h"

#define CAIRO_DOCK_GROUP_ICON_SIZE 32
#define CAIRO_DOCK_CATEGORY_ICON_SIZE 24
#define CAIRO_DOCK_NB_BUTTONS_BY_ROW 4
#define CAIRO_DOCK_NB_BUTTONS_BY_ROW_MIN 3
#define CAIRO_DOCK_TABLE_MARGIN 6
#define CAIRO_DOCK_CONF_PANEL_WIDTH 1250
#define CAIRO_DOCK_CONF_PANEL_WIDTH_MIN 800
#define CAIRO_DOCK_CONF_PANEL_HEIGHT 700
#define CAIRO_DOCK_PREVIEW_WIDTH 200
#define CAIRO_DOCK_PREVIEW_WIDTH_MIN 100
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_ICON_MARGIN 6
#define CAIRO_DOCK_TAB_ICON_SIZE 32

extern CairoDockDesktopGeometry g_desktopGeometry;

extern gchar *g_cConfFile;
extern CairoContainer *g_pPrimaryContainer;
extern CairoDock *g_pMainDock;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cCairoDockDataDir;
extern gboolean g_bEasterEggs;

struct _CairoDockCategoryWidgetTable {
	GtkWidget *pFrame;
	GtkWidget *pTable;
	gint iNbRows;
	gint iNbItemsInCurrentRow;
	GtkToolItem *pCategoryButton;
	} ;

struct _CairoDockGroupDescription {
	gchar *cGroupName;
	const gchar *cTitle;
	gint iCategory;
	gchar *cDescription;
	gchar *cPreviewFilePath;
	GtkWidget *pActivateButton;
	GtkWidget *pLabel;
	GtkWidget *pGroupHBox;
	gchar *cOriginalConfFilePath;
	gchar *cIcon;
	gchar *cConfFilePath;
	const gchar *cGettextDomain;
	void (* load_custom_widget) (CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
	const gchar **cDependencies;
	gboolean bIgnoreDependencies;
	GList *pExtensions;
	const gchar *cInternalModule;
	GList *pManagers;
	gboolean bMatchFilter;
	} ;

typedef struct _CairoDockGroupDescription CairoDockGroupDescription;
typedef struct _CairoDockCategoryWidgetTable CairoDockCategoryWidgetTable;

static CairoDockCategoryWidgetTable s_pCategoryWidgetTables[CAIRO_DOCK_NB_CATEGORY+1];
GSList *s_pCurrentWidgetList;  // liste des widgets du module courant.
GSList *s_pExtraCurrentWidgetList;  // liste des widgets des eventuels modules lies.
static GList *s_pGroupDescriptionList = NULL;
static GtkWidget *s_pPreviewBox = NULL;
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

static const gchar *s_cCategoriesDescription[2*(CAIRO_DOCK_NB_CATEGORY+1)] = {
	N_("Behaviour"), "icon-behavior.svg",
	N_("Appearance"), "icon-appearance.svg",
	N_("Files"), "icon-files.svg",
	N_("Internet"), "icon-internet.svg",
	N_("Desktop"), "icon-desktop.svg",
	N_("Accessories"), "icon-accessories.svg",
	N_("System"), "icon-system.svg",
	N_("Fun"), "icon-fun.svg",
	N_("All"), "icon-all.svg" };

static void cairo_dock_hide_all_categories (void);
static void cairo_dock_show_all_categories (void);
static void cairo_dock_show_one_category (int iCategory);
static void cairo_dock_toggle_category_button (int iCategory);
static void cairo_dock_show_group (CairoDockGroupDescription *pGroupDescription);
static CairoDockGroupDescription *cairo_dock_find_module_description (const gchar *cModuleName);
static void cairo_dock_apply_current_filter (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther);
static GtkWidget *cairo_dock_present_group_widget (const gchar *cConfFilePath, CairoDockGroupDescription *pGroupDescription, gboolean bSingleGroup, CairoDockModuleInstance *pInstance);
static void cairo_dock_reset_current_widget_list (void);
static void _trigger_current_filter (void);

  ////////////
 // FILTER //
////////////

gchar *_get_valid_module_conf_file (CairoDockModule *pModule)
{
	if (pModule->pInstancesList != NULL)  // module is already instanciated, take the first instance's conf-file.
	{
		CairoDockModuleInstance *pModuleInstance = pModule->pInstancesList->data;
		return g_strdup (pModuleInstance->cConfFilePath);
	}
	else if (pModule->pVisitCard->cConfFileName != NULL)  // not instanciated yet, take a conf-file in the module's user dir, or the default conf-file.
	{
		// open the module's user dir.
		gchar *cUserDataDirPath = cairo_dock_check_module_conf_dir (pModule);
		cd_debug ("cUserDataDirPath: %s", cUserDataDirPath);
		GDir *dir = g_dir_open (cUserDataDirPath, 0, NULL);
		if (dir == NULL)
		{
			g_free (cUserDataDirPath);
			return NULL;
		}
		
		// look for a conf-file.
		const gchar *cFileName;
		gchar *cInstanceFilePath = NULL;
		while ((cFileName = g_dir_read_name (dir)) != NULL)
		{
			gchar *str = strstr (cFileName, ".conf");
			if (!str)
				continue;
			if (*(str+5) != '-' && *(str+5) != '\0')  // xxx.conf or xxx.conf-i
				continue;
			cInstanceFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, cFileName);
			break;
		}
		g_dir_close (dir);
		// if no conf-file, copy the default one into the folder and take this one.
		if (cInstanceFilePath == NULL)  // no conf file present yet.
		{
			gboolean r = cairo_dock_copy_file (pModule->cConfFilePath, cUserDataDirPath);
			/**gchar *cCommand = g_strdup_printf ("cp \"%s\" \"%s\"", pModule->cConfFilePath, cUserDataDirPath);
			int r = system (cCommand);
			g_free (cCommand);*/
			if (r)  // copy ok
				cInstanceFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, pModule->pVisitCard->cConfFileName);
		}
		g_free (cUserDataDirPath);
		return cInstanceFilePath;
	}
	return NULL;
}

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
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GSList *pSubWidgetList;
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
		pSubWidgetList = pGroupKeyWidget->pSubWidgetList;
		if (pSubWidgetList == NULL)
			continue;
		pLabel = pGroupKeyWidget->pLabel;
		if (pLabel == NULL)
			continue;
		pKeyBox = pGroupKeyWidget->pKeyBox;
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
	gchar *cKeyWord, *str = NULL, *cModifiedText = NULL;
	const gchar *cDescription, *cToolTip = NULL;
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
			cd_debug ("%s : bouton emprunte a %s\n", pGroupDescription->cGroupName, pGroupDescription->cInternalModule);
			pInternalGroupDescription = cairo_dock_find_module_description (pGroupDescription->cInternalModule);
			if (pInternalGroupDescription != NULL)
				pGroupBox = gtk_widget_get_parent (pInternalGroupDescription->pActivateButton);
			else
				continue;
			pLabel = pInternalGroupDescription->pLabel;
			cd_debug ("ok, found pGroupBox\n");
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
		
		cDescription = pGroupDescription->cTitle;
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
				pKeyFile = cairo_dock_open_key_file (pModule->cConfFilePath);  // search in default conf file
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
				
				const gchar *cUsefulComment;
				gchar iElementType;
				guint iNbElements;
				gchar **pAuthorizedValuesList;
				const gchar *cTipString;
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


  ///////////////////
 // MODULE DISPLAY //
/////////////////////

static void _add_module_to_grid (CairoDockCategoryWidgetTable *pCategoryWidget, GtkWidget *pWidget)
{
	if (pCategoryWidget->iNbItemsInCurrentRow == s_iNbButtonsByRow)
	{
		pCategoryWidget->iNbItemsInCurrentRow = 0;
		pCategoryWidget->iNbRows ++;
	}
	gtk_table_attach_defaults (GTK_TABLE (pCategoryWidget->pTable),
		pWidget,
		pCategoryWidget->iNbItemsInCurrentRow,
		pCategoryWidget->iNbItemsInCurrentRow+1,
		pCategoryWidget->iNbRows,
		pCategoryWidget->iNbRows+1);
	pCategoryWidget->iNbItemsInCurrentRow ++;
}

static void _remove_module_from_grid (GtkWidget *pWidget, GtkContainer *pContainer)
{
	g_object_ref (pWidget);
	gtk_container_remove (GTK_CONTAINER (pContainer), GTK_WIDGET (pWidget));
}

static void cairo_dock_prepare_module_list (const gchar* cTitle, gpointer path)
{
	// Remove status message.
	if (s_pStatusBar)
		gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);
	
	// Destroy current group widget.
	if (s_pCurrentGroupWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentGroupWidget);
		s_pCurrentGroupWidget = NULL;
		cairo_dock_reset_current_widget_list ();
		s_pCurrentGroup = NULL;
	}
	
	// Hide non related buttons and box.
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pGroupFrame);
	
	// Run filter.
	_trigger_current_filter ();
	
	// Update window title.
	gtk_window_set_title (GTK_WINDOW (s_pMainWindow), cTitle);

	// Add page to path if needed.
	if (s_path == NULL || s_path->data != path)
		s_path = g_slist_prepend (s_path, path);
}


  ///////////////
 // CALLBACKS //
///////////////

static void on_click_toggle_activated (GtkButton *button, gpointer data)
{
	int iCatMin = 1; // Don't change the first group (0).
	gboolean bEnableHideModules = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gboolean bGroupShow[CAIRO_DOCK_NB_CATEGORY+1];
	CairoDockGroupDescription *pGroupDescription;
	CairoDockCategoryWidgetTable *pCategoryWidget;

	for (int iCategory = iCatMin; iCategory < CAIRO_DOCK_NB_CATEGORY; iCategory ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[iCategory];

		// Set visible categories to FALSE
		bGroupShow[iCategory] = FALSE;

		// Remove all modules widgets from category table.
		gtk_container_foreach (GTK_CONTAINER (pCategoryWidget->pTable), (GtkCallback) _remove_module_from_grid, GTK_CONTAINER (pCategoryWidget->pTable));

		// Reset category table.
		pCategoryWidget->iNbRows = 0;
		pCategoryWidget->iNbItemsInCurrentRow = 0;
		
		gtk_widget_destroy(pCategoryWidget->pTable);
		pCategoryWidget->pTable = gtk_table_new (1, s_iNbButtonsByRow, TRUE);
		gtk_container_add (GTK_CONTAINER (pCategoryWidget->pFrame), GTK_WIDGET (pCategoryWidget->pTable));
		gtk_table_set_row_spacings (GTK_TABLE (pCategoryWidget->pTable), CAIRO_DOCK_FRAME_MARGIN);
		gtk_table_set_col_spacings (GTK_TABLE (pCategoryWidget->pTable), CAIRO_DOCK_FRAME_MARGIN);
	}

	// Put the widgets in the new table.
	for (GList *gd = g_list_last (s_pGroupDescriptionList); gd != NULL; gd = gd->prev)
	{
		pGroupDescription = gd->data;
		if (pGroupDescription->iCategory >= iCatMin
			&& pGroupDescription->pGroupHBox != NULL
			&& (bEnableHideModules == FALSE || gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pGroupDescription->pActivateButton)) == TRUE))
		{
			bGroupShow[pGroupDescription->iCategory] = TRUE;
			_add_module_to_grid (&s_pCategoryWidgetTables[pGroupDescription->iCategory], GTK_WIDGET (pGroupDescription->pGroupHBox));
			g_object_unref (pGroupDescription->pGroupHBox);
		}
	}

	// Show everything except empty groups.
	gtk_widget_show_all (GTK_WIDGET (s_pGroupsVBox));
	for (int iCategory = iCatMin; iCategory < CAIRO_DOCK_NB_CATEGORY; iCategory ++)
	{
		if (bGroupShow[iCategory] == FALSE)
		{
			pCategoryWidget = &s_pCategoryWidgetTables[iCategory];
			gtk_widget_hide (GTK_WIDGET (pCategoryWidget->pFrame));
		}
	}
	
	//\_______________ Set common settings for modules list.
	cairo_dock_prepare_module_list (_("Cairo-Dock configuration"), GINT_TO_POINTER (0));
}

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
	else if (GPOINTER_TO_INT (pPlace) < CAIRO_DOCK_NB_CATEGORY+1)  // categorie.
	{
		if (pPlace == 0)
			cairo_dock_show_all_categories ();
		else
		{
			int iCategory = GPOINTER_TO_INT (pPlace) - 1;
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
			//g_print ("preview : %dx%d\n", iPreviewWidth, iPreviewHeight);
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
		else 
			gtk_widget_show (s_pPreviewBox);

		gtk_image_set_from_pixbuf (GTK_IMAGE (pPreviewImage), pPreviewPixbuf);
		gdk_pixbuf_unref (pPreviewPixbuf);
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
	myDialogsParam.dialogTextDescription.bUseMarkup = TRUE;
	s_pDialog = cairo_dock_build_dialog (&attr, pIcon, CAIRO_CONTAINER (pDock));
	myDialogsParam.dialogTextDescription.bUseMarkup = FALSE;
	
	cairo_dock_dialog_reference (s_pDialog);
	
	gtk_window_set_transient_for (GTK_WINDOW (s_pDialog->container.pWidget), GTK_WINDOW (s_pMainWindow));
	g_free (cDescription);

	s_iSidShowGroupDialog = 0;
	return FALSE;
}
static void on_enter_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription)
{
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cDescription);
	if (g_pPrimaryContainer == NULL)  // inutile en maintenance, le dialogue risque d'apparaitre sur la souris.
		return ;
	
	if (s_iSidShowGroupDialog != 0)
		g_source_remove (s_iSidShowGroupDialog);
	
	s_iSidShowGroupDialog = g_timeout_add (330, (GSourceFunc)_show_group_dialog, (gpointer) pGroupDescription);
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
	gtk_widget_hide (s_pPreviewBox);
	
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

static void _cairo_dock_free_group_description (CairoDockGroupDescription *pGroupDescription)
{
	if (pGroupDescription == NULL)
		return;
	g_free (pGroupDescription->cGroupName);
	g_free (pGroupDescription->cDescription);
	g_free (pGroupDescription->cPreviewFilePath);
	g_free (pGroupDescription->cOriginalConfFilePath);
	g_free (pGroupDescription->cIcon);
	g_free (pGroupDescription->cConfFilePath);
	g_list_free (pGroupDescription->pExtensions);
	g_list_free (pGroupDescription->pManagers);
	g_free (pGroupDescription);
}

static void cairo_dock_free_categories (void)
{
	memset (s_pCategoryWidgetTables, 0, sizeof (s_pCategoryWidgetTables));  // les widgets a l'interieur sont detruits avec la fenetre.
	
	CairoDockGroupDescription *pGroupDescription;
	GList *pElement;
	for (pElement = s_pGroupDescriptionList; pElement != NULL; pElement = pElement->next)
	{
		pGroupDescription = pElement->data;
		_cairo_dock_free_group_description (pGroupDescription);
	}
	s_pCurrentGroup = NULL;
	g_list_free (s_pGroupDescriptionList);
	s_pGroupDescriptionList = NULL;
	
	s_pPreviewImage = NULL;
	
	s_pOkButton = NULL;
	s_pApplyButton = NULL;
	
	s_pMainWindow = NULL;
	s_pToolBar = NULL;
	s_pStatusBar = NULL;
	
	cairo_dock_reset_current_widget_list ();
	s_pCurrentGroupWidget = NULL;  // detruit en meme temps que la fenetre.
	g_slist_free (s_path);
	s_path = NULL;
}

static gboolean on_delete_main_gui (GtkWidget *pWidget, GdkEvent *event, gpointer data)
{
	cairo_dock_free_categories ();
	if (s_iSidShowGroupDialog != 0)
	{
		g_source_remove (s_iSidShowGroupDialog);
		s_iSidShowGroupDialog = 0;
	}
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
				if (strcmp (pModuleInstance->cConfFilePath, pGroupDescription->cConfFilePath) == 0)  // the one that has been opened.
					break ;
			}
			if (pElement == NULL)  // couldn't find it, the instance that was being edited has been deleted meanwhile.
				return;
			
			cairo_dock_write_current_group_conf_file (pModuleInstance->cConfFilePath, pModuleInstance);
			cairo_dock_reload_module_instance (pModuleInstance, TRUE);
		}
		else
		{
			gchar *cFile = _get_valid_module_conf_file (pModule);
			cairo_dock_write_current_group_conf_file (cFile, NULL);
			g_free (cFile);
		}
	}
	else
	{
		cairo_dock_write_current_group_conf_file (g_cConfFile, NULL);
		
		const gchar *cManagerName;
		GldiManager *pManager;
		GList *m;
		for (m = pGroupDescription->pManagers; m != NULL; m = m->next)
		{
			cManagerName = m->data;
			pManager = gldi_get_manager (cManagerName);
			g_return_if_fail (pManager != NULL);
			gldi_reload_manager (pManager, g_cConfFile);
		}
		
		if (pGroupDescription->pExtensions != NULL)  // comme on ne sait pas sur quel(s) module(s) on a fait des modif, on les met tous a jour.
		{
			CairoDockModuleInstance *pModuleInstance;
			GList *m;
			int i = 0;
			for (m = pGroupDescription->pExtensions; m != NULL; m = m->next)
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
	///gboolean bReturn;
	///g_signal_emit_by_name (pWindow, "delete-event", NULL, &bReturn);
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
	if (g_pPrimaryContainer == NULL)
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
	on_click_activate_given_group (button, pGroupDescription);
	
	if (pGroupDescription->pActivateButton != NULL)  // on repercute le changement sur le bouton d'activation du groupe.
	{
		CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
		g_return_if_fail (pModule != NULL);
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
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
static void on_clear_filter (GtkEntry *pEntry, GtkEntryIconPosition icon_pos, GdkEvent *event, gpointer data)
{
	gtk_entry_set_text (pEntry, "");
	//gpointer pCurrentPlace = cairo_dock_get_current_widget ();
	//g_print ("pCurrentPlace : %x\n", pCurrentPlace);
	//_show_group_or_category (pCurrentPlace);
	const gchar *keyword[2] = {"fabounetfabounetfabounet", NULL};
	cairo_dock_apply_current_filter ((gchar **)keyword, FALSE, FALSE, FALSE, FALSE);
}
#endif


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
				cIconPath = g_strconcat (CAIRO_DOCK_SHARE_DATA_DIR"/icons/", cImage, NULL);
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
	gchar *cLabel2 = g_strdup_printf ("<span size='large' weight='800'>%s</span>", cLabel);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel2);
	g_free (cLabel2);
	
	GtkWidget *pAlign = gtk_alignment_new (0., 0.5, 0., 1.);
	gtk_alignment_set_padding (GTK_ALIGNMENT (pAlign), 0, 0, CAIRO_DOCK_FRAME_MARGIN, 0);
	gtk_container_add (GTK_CONTAINER (pAlign), pLabel);
	gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (pWidget), pAlign);
	
	return pWidget;
}

static inline CairoDockGroupDescription *_add_group_button (const gchar *cGroupName, const gchar *cIcon, int iCategory, const gchar *cDescription, const gchar *cPreviewFilePath, int iActivation, gboolean bConfigurable, const gchar *cOriginalConfFilePath, const gchar *cGettextDomain, const gchar **cDependencies, GList *pExtensions, const gchar *cInternalModule, const gchar *cTitle)
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
				cIconPath = g_strconcat (CAIRO_DOCK_SHARE_DATA_DIR"/icons/", cIcon, NULL);
			}
		}
	}
	pGroupDescription->cIcon = cIconPath;
	pGroupDescription->cGettextDomain = cGettextDomain;
	pGroupDescription->cDependencies = cDependencies;
	pGroupDescription->pExtensions = pExtensions;
	pGroupDescription->cInternalModule = cInternalModule;
	pGroupDescription->cTitle = cTitle;
	
	s_pGroupDescriptionList = g_list_prepend (s_pGroupDescriptionList, pGroupDescription);
	if (cInternalModule != NULL)
		return pGroupDescription;
	
	//\____________ On construit le bouton du groupe.
	GtkWidget *pGroupHBox = _gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN);
	pGroupDescription->pGroupHBox = pGroupHBox;
	
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
	gtk_button_set_relief (GTK_BUTTON (pGroupButton), GTK_RELIEF_NONE);
	if (bConfigurable)
		g_signal_connect (G_OBJECT (pGroupButton), "clicked", G_CALLBACK(on_click_group_button), pGroupDescription);
	else
		gtk_widget_set_sensitive (pGroupButton, FALSE);
	g_signal_connect (G_OBJECT (pGroupButton), "enter", G_CALLBACK(on_enter_group_button), pGroupDescription);
	g_signal_connect (G_OBJECT (pGroupButton), "leave", G_CALLBACK(on_leave_group_button), NULL);

	GtkWidget *pButtonHBox = _gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN);
	GtkWidget *pImage = _make_image (cIconPath, CAIRO_DOCK_GROUP_ICON_SIZE);
	gtk_box_pack_start (GTK_BOX (pButtonHBox), pImage, FALSE, FALSE, 0);
	pGroupDescription->pLabel = gtk_label_new (pGroupDescription->cTitle);
	gtk_box_pack_start (GTK_BOX (pButtonHBox),
		pGroupDescription->pLabel,
		FALSE,
		FALSE,
		0);
	gtk_container_add (GTK_CONTAINER (pGroupButton), pButtonHBox);
	gtk_box_pack_start (GTK_BOX (pGroupHBox),
		pGroupButton,
		TRUE,
		TRUE,
		0);

	//\____________ On place le bouton dans sa table.
	_add_module_to_grid (&s_pCategoryWidgetTables[iCategory], pGroupHBox);
	return pGroupDescription;
}

static gboolean _cairo_dock_add_one_module_widget (CairoDockModule *pModule, const gchar *cActiveModules)
{
	const gchar *cModuleName = pModule->pVisitCard->cModuleName;
	///if (pModule->cConfFilePath == NULL && ! g_bEasterEggs)  // option perso : les plug-ins non utilises sont grises et ne rajoutent pas leur .conf au theme courant.
	///	pModule->cConfFilePath = cairo_dock_check_module_conf_file (pModule->pVisitCard);
	int iActive;
	if (! pModule->pInterface->stopModule)
		iActive = -1;
	else if (g_pPrimaryContainer == NULL && cActiveModules != NULL)  // avant chargement du theme.
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
		pModule->pVisitCard->cConfFileName != NULL,
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

static void _load_shortkeys_widget (CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile)
{
	//\_____________ On recupere notre emplacement perso dans la fenetre.
	CairoDockGroupKeyWidget *myWidget = cairo_dock_gui_find_group_key_widget_in_list (s_pCurrentWidgetList, "Shortkeys", "shortkeys");
	g_return_if_fail (myWidget != NULL);
	
	//\_____________ On construit le tree-view.
	GtkWidget *pOneWidget = cairo_dock_build_shortkeys_widget ();
	
	//\_____________ On l'ajoute a la fenetre.
	GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (pScrolledWindow, "height-request", MIN (2*CAIRO_DOCK_PREVIEW_HEIGHT, g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - 175), NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pOneWidget);
	myWidget->pSubWidgetList = g_slist_append (myWidget->pSubWidgetList, pOneWidget);  // on le met dans la liste, non pas pour recuperer sa valeur, mais pour pouvoir y acceder facilement plus tard.
	gtk_box_pack_start (GTK_BOX (myWidget->pKeyBox), pScrolledWindow, FALSE, FALSE, 0);
}

#define _add_one_main_group_button(cGroupName, cIcon, iCategory, cDescription, cTitle) \
_add_group_button (cGroupName,\
		cIcon,\
		iCategory,\
		cDescription,\
		NULL,  /* pas de prevue*/\
		-1,  /* <=> non desactivable*/\
		TRUE,  /* <=> configurable*/\
		cOriginalConfFilePath,\
		NULL,  /* domaine de traduction : celui du dock.*/\
		NULL,  /* no dependencies*/\
		NULL,  /* no external modules*/\
		NULL,  /* no internal module*/\
		cTitle)
static void _add_main_groups_buttons (void)
{
	CairoDockGroupDescription *pGroupDescription;
	const gchar *cOriginalConfFilePath = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_CONF_FILE;
	pGroupDescription = _add_one_main_group_button ("Position",
		"icon-position.svg",
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("Set the position of the main dock."),
		_("Position"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Docks");
	
	pGroupDescription = _add_one_main_group_button ("Accessibility",
		"icon-visibility.svg",
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("Do you like your dock to be always visible,\n or on the contrary unobtrusive?\nConfigure the way you access your docks and sub-docks!"),
		_("Visibility"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Docks");
	
	pGroupDescription = _add_one_main_group_button ("TaskBar",
		"icon-taskbar.png",
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("Display and interact with currently open windows."),
		_("Taskbar"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Taskbar");
	
	pGroupDescription = _add_one_main_group_button ("Shortkeys",
		"gtk-select-font",  /// TODO: trouver une meilleure icone, et l'utiliser aussi pour le backend "simple"...
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("Define all the keyboard shortcuts currently available."),
		_("Shortkeys"));
	pGroupDescription->load_custom_widget = _load_shortkeys_widget;
	
	pGroupDescription = _add_one_main_group_button ("System",
		"icon-system.svg",
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("All of the parameters you will never want to tweak."),
		_("System"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Docks");
	pGroupDescription->pManagers = g_list_prepend (pGroupDescription->pManagers, (gchar*)"Connection");
	pGroupDescription->pManagers = g_list_prepend (pGroupDescription->pManagers, (gchar*)"Containers");
	pGroupDescription->pManagers = g_list_prepend (pGroupDescription->pManagers, (gchar*)"Backends");
	
	pGroupDescription = _add_one_main_group_button ("Background",
		"icon-background.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Set a background for your dock."),
		_("Background"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Docks");
	
	pGroupDescription = _add_one_main_group_button ("Views",
		"icon-views.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Select a view for each of your docks."),
		_("Views"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Backends");
	pGroupDescription->pExtensions = g_list_prepend (NULL, (gchar*)"dock rendering");
	
	pGroupDescription = _add_one_main_group_button ("Dialogs",
		"icon-dialogs.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Configure text bubble appearance."),
		_("Dialog boxes"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Dialogs");
	pGroupDescription->pExtensions = g_list_prepend (NULL, (gchar*)"dialog rendering");
	
	pGroupDescription = _add_one_main_group_button ("Desklets",
		"icon-desklets.png",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Applets can be displayed on your desktop as widgets."),
		_("Desklets"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Desklets");
	pGroupDescription->pExtensions = g_list_prepend (NULL, (gchar*)"desklet rendering");
	
	pGroupDescription = _add_one_main_group_button ("Icons",
		"icon-icons.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("All about icons:\n size, reflection, icon theme,..."),
		_("Icons"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Icons");
	
	pGroupDescription = _add_one_main_group_button ("Indicators",
		"icon-indicators.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Indicators are additional markers for your icons."),
		_("Indicators"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Indicators");
	pGroupDescription->pExtensions = g_list_prepend (NULL, (gchar*)"drop indicator");
	
	pGroupDescription = _add_one_main_group_button ("Labels",
		"icon-labels.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Define icon caption and quick-info style."),
		_("Captions"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Icons");
}

static GtkWidget *cairo_dock_build_main_ihm_left_frame (const gchar *cText)
{
	// frame
	GtkWidget *pFrame = gtk_frame_new (NULL);
	//gtk_container_set_border_width (GTK_CONTAINER (pFrame), CAIRO_DOCK_FRAME_MARGIN);
	gtk_frame_set_shadow_type (GTK_FRAME (pFrame), GTK_SHADOW_NONE);
	
	// label
	gchar *cLabel = g_strdup_printf ("<span size='x-large' weight='800' color=\"#81728C\">%s</span>", cText);
	GtkWidget *pLabel = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
	g_free (cLabel);
	gtk_frame_set_label_widget (GTK_FRAME (pFrame), pLabel);
	
	return pFrame;
}

static GtkWidget *cairo_dock_build_main_ihm (const gchar *cConfFilePath)
{
	//\_____________ On construit la fenetre.
	if (s_pMainWindow != NULL)
	{
		gtk_window_present (GTK_WINDOW (s_pMainWindow));
		return s_pMainWindow;
	}
	s_pMainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	//gtk_container_set_border_width (s_pMainWindow, CAIRO_DOCK_FRAME_MARGIN);
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);
	gtk_window_set_icon_from_file (GTK_WINDOW (s_pMainWindow), cIconPath, NULL);
	g_free (cIconPath);

	GtkWidget *pMainHBox = _gtk_hbox_new (0);
	gtk_container_add (GTK_CONTAINER (s_pMainWindow), pMainHBox);
	
	if (g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] > CAIRO_DOCK_CONF_PANEL_WIDTH)
	{
		s_iPreviewWidth = CAIRO_DOCK_PREVIEW_WIDTH;
		s_iNbButtonsByRow = CAIRO_DOCK_NB_BUTTONS_BY_ROW;
	}
	else if (g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] > CAIRO_DOCK_CONF_PANEL_WIDTH_MIN)
	{
		double a = 1.*(CAIRO_DOCK_PREVIEW_WIDTH - CAIRO_DOCK_PREVIEW_WIDTH_MIN) / (CAIRO_DOCK_CONF_PANEL_WIDTH - CAIRO_DOCK_CONF_PANEL_WIDTH_MIN);
		double b = CAIRO_DOCK_PREVIEW_WIDTH_MIN - CAIRO_DOCK_CONF_PANEL_WIDTH_MIN * a;
		s_iPreviewWidth = a * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] + b;
		s_iNbButtonsByRow = CAIRO_DOCK_NB_BUTTONS_BY_ROW - 1;
	}
	else
	{
		s_iPreviewWidth = CAIRO_DOCK_PREVIEW_WIDTH_MIN;
		s_iNbButtonsByRow = CAIRO_DOCK_NB_BUTTONS_BY_ROW_MIN;
	}

	GtkWidget *pCategoriesVBox = _gtk_vbox_new (CAIRO_DOCK_FRAME_MARGIN);
	gtk_widget_set_size_request (pCategoriesVBox, s_iPreviewWidth+2*CAIRO_DOCK_FRAME_MARGIN, CAIRO_DOCK_PREVIEW_HEIGHT);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pCategoriesVBox,
		FALSE,
		FALSE,
		0);

	GtkWidget *pVBox = _gtk_vbox_new (0);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pVBox,
		TRUE,
		TRUE,
		0);
	s_pGroupsVBox = _gtk_vbox_new (CAIRO_DOCK_TABLE_MARGIN);
	GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	GtkWidget *pViewport = gtk_viewport_new( NULL, NULL );
	gtk_viewport_set_shadow_type ( GTK_VIEWPORT (pViewport), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER( pViewport ), s_pGroupsVBox );
	gtk_container_add (GTK_CONTAINER( pScrolledWindow ), pViewport );

	gtk_box_pack_start (GTK_BOX (pVBox),
		pScrolledWindow,
		TRUE,
		TRUE,
		0);
	
	// Empty box to get some space between window border and filter label
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		_gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN / 2),
		FALSE,
		FALSE,
		0);

	//\_____________ Filter.
	GtkWidget *pFilterFrame = cairo_dock_build_main_ihm_left_frame (_("Filter"));
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pFilterFrame,
		FALSE,
		FALSE,
		0);

	// text entry
	GtkWidget *pFilterBox = _gtk_hbox_new (0);
	gtk_container_add (GTK_CONTAINER (pFilterFrame), pFilterBox);

	s_pFilterEntry = gtk_entry_new ();
	g_signal_connect (s_pFilterEntry, "activate", G_CALLBACK (on_activate_filter), NULL);
	gtk_box_pack_start (GTK_BOX (pFilterBox),
		s_pFilterEntry,
		TRUE,
		FALSE,
		0);
	//~ gtk_container_set_focus_child (GTK_CONTAINER (s_pMainWindow), pFilterBox); /// set focus to filter box
	
	#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
	gtk_entry_set_icon_activatable (GTK_ENTRY (s_pFilterEntry), GTK_ENTRY_ICON_SECONDARY, TRUE);
	gtk_entry_set_icon_from_stock (GTK_ENTRY (s_pFilterEntry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CLEAR);
	g_signal_connect (s_pFilterEntry, "icon-press", G_CALLBACK (on_clear_filter), NULL);
	#endif
	
	// Filter options
	_reset_filter_state ();
	
	GtkWidget *pMenuBar = gtk_menu_bar_new ();
	GtkWidget *pMenuItem = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES, NULL);
	gtk_menu_item_set_label (GTK_MENU_ITEM (pMenuItem), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenuBar), pMenuItem);
	gtk_box_pack_start (GTK_BOX (pFilterBox),
		pMenuBar,
		FALSE,
		FALSE,
		0);
	GtkWidget *pMenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pMenu);
	
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("All words"));
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	g_signal_connect (pMenuItem, "toggled", G_CALLBACK (on_toggle_all_words), NULL);
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("Highlighted words"));
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
	
	//\_____________ Add module display choice.
	GtkWidget *pModuleSelectFrame = cairo_dock_build_main_ihm_left_frame (_("Applets"));
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pModuleSelectFrame,
		FALSE,
		FALSE,
		0);
	GtkWidget *pModuleSelect = gtk_check_button_new_with_label (_("Hide disabled"));
	gtk_container_add (GTK_CONTAINER (pModuleSelectFrame), pModuleSelect);
	g_signal_connect (G_OBJECT (pModuleSelect), "clicked", G_CALLBACK(on_click_toggle_activated), NULL);

	//\_____________ On construit les boutons de chaque categorie.
	GtkWidget *pCategoriesFrame = cairo_dock_build_main_ihm_left_frame (_("Categories"));
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pCategoriesFrame,
		TRUE,  /// FALSE
		TRUE,  /// FALSE
		0);
	
	s_pToolBar = gtk_toolbar_new ();
	#if (GTK_MAJOR_VERSION < 3 && GTK_MINOR_VERSION < 16)
	gtk_toolbar_set_orientation (GTK_TOOLBAR (s_pToolBar), GTK_ORIENTATION_VERTICAL);
	#else
	gtk_orientable_set_orientation (GTK_ORIENTABLE (s_pToolBar), GTK_ORIENTATION_VERTICAL);
	#endif
	gtk_toolbar_set_style (GTK_TOOLBAR (s_pToolBar), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (s_pToolBar), TRUE);  /// FALSE
	//gtk_widget_set (s_pToolBar, "height-request", 300, NULL);
	//g_object_set (s_pToolBar, "expand", TRUE, NULL);
	///gtk_toolbar_set_icon_size (GTK_TOOLBAR (s_pToolBar), GTK_ICON_SIZE_LARGE_TOOLBAR);  /// GTK_ICON_SIZE_LARGE_TOOLBAR
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
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryButton), TRUE);
	
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
		gtk_container_set_border_width (GTK_CONTAINER (pCategoryWidget->pFrame), CAIRO_DOCK_FRAME_MARGIN);
		gtk_frame_set_shadow_type (GTK_FRAME (pCategoryWidget->pFrame), GTK_SHADOW_OUT);
		
		GtkWidget *pLabel = gtk_label_new (NULL);
		gchar *cLabel = g_strdup_printf ("<span size='large' weight='800'>%s</span>", gettext (s_cCategoriesDescription[2*i]));
		gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
		g_free (cLabel);
		gtk_frame_set_label_widget (GTK_FRAME (pCategoryWidget->pFrame), pLabel);
		
		pCategoryWidget->pTable = gtk_table_new (1,
			s_iNbButtonsByRow,
			TRUE);
		gtk_table_set_row_spacings (GTK_TABLE (pCategoryWidget->pTable), CAIRO_DOCK_FRAME_MARGIN);
		gtk_table_set_col_spacings (GTK_TABLE (pCategoryWidget->pTable), CAIRO_DOCK_FRAME_MARGIN);
		gtk_container_add (GTK_CONTAINER (pCategoryWidget->pFrame),
			pCategoryWidget->pTable);
		gtk_box_pack_start (GTK_BOX (s_pGroupsVBox),
			pCategoryWidget->pFrame,
			FALSE,
			FALSE,
			0);
	}
	
	
	//\_____________ On remplit avec les groupes du fichier.
	_add_main_groups_buttons ();
	/**GError *erreur = NULL;
	GKeyFile* pKeyFile = g_key_file_new();
	g_key_file_load_from_file (pKeyFile, cConfFilePath, 0, &erreur);  // inutile de garder les commentaires ici.
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cConfFilePath, erreur->message);
		g_error_free (erreur);
		g_key_file_free (pKeyFile);
		return NULL;
	}
	
	_add_main_groups_buttons ();
	gsize length = 0;
	gchar **pGroupList = g_key_file_get_groups (pKeyFile, &length);
	gchar *cGroupName;
	gint iActive;
	CairoDockInternalModule *pInternalModule;
	const gchar *cOriginalConfFilePath = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_CONF_FILE;
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
			pInternalModule->pExtensions,
			NULL,
			pInternalModule->cTitle);
	}
	g_strfreev (pGroupList);*/
	
	//\_____________ On remplit avec les modules.
	gchar *cActiveModules;
	if (g_pPrimaryContainer == NULL)
	{
		GKeyFile* pKeyFile = g_key_file_new();
		g_key_file_load_from_file (pKeyFile, cConfFilePath, 0, NULL);  // inutile de garder les commentaires ici.
		cActiveModules = g_key_file_get_string (pKeyFile, "System", "modules", NULL);
		g_key_file_free (pKeyFile);
	}
	else
		cActiveModules = NULL;
	cairo_dock_foreach_module_in_alphabetical_order ((GCompareFunc) _cairo_dock_add_one_module_widget, cActiveModules);
	g_free (cActiveModules);
	
	//\_____________ On ajoute le cadre d'activation du module.
	s_pGroupFrame = gtk_frame_new ("pouet");
	gtk_frame_set_shadow_type (GTK_FRAME (s_pGroupFrame), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		s_pGroupFrame,
		FALSE,
		FALSE,
		0);
	s_pActivateButton = gtk_check_button_new_with_label (_("Enable this module"));
	g_signal_connect (G_OBJECT (s_pActivateButton), "clicked", G_CALLBACK(on_click_activate_current_group), NULL);
	gtk_container_add (GTK_CONTAINER (s_pGroupFrame), s_pActivateButton);
	gtk_widget_show_all (s_pActivateButton);
	
	//\_____________ On ajoute la zone de prevue.
	s_pPreviewBox = _gtk_vbox_new (CAIRO_DOCK_FRAME_MARGIN);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		s_pPreviewBox,
		FALSE,
		FALSE,
		0);
	
	s_pPreviewImage = gtk_image_new_from_pixbuf (NULL);
	gtk_container_add (GTK_CONTAINER (s_pPreviewBox), s_pPreviewImage);
	
	//\_____________ On ajoute les boutons.
	GtkWidget *pButtonsHBox = _gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN);
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
	
	gchar *cLink = cairo_dock_get_third_party_applets_link ();
	GtkWidget *pThirdPartyButton = gtk_link_button_new_with_label (cLink, _("More applets"));
	gtk_widget_set_tooltip_text (pThirdPartyButton, _("Get more applets online !"));
	GtkWidget *pImage = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (pThirdPartyButton), pImage);
	g_free (cLink);
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		pThirdPartyButton,
		FALSE,
		FALSE,
		0);
	
	//\_____________ On ajoute la barre de status a la fin.
	s_pStatusBar = gtk_statusbar_new ();
	#if (GTK_MAJOR_VERSION < 3)
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (s_pStatusBar), FALSE);
	#endif
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		s_pStatusBar,
		TRUE,
		TRUE,
		0);
	g_object_set_data (G_OBJECT (s_pMainWindow), "status-bar", s_pStatusBar);
	g_object_set_data (G_OBJECT (s_pMainWindow), "frame-width", GINT_TO_POINTER (200));
	
	gtk_window_resize (GTK_WINDOW (s_pMainWindow),
		MIN (CAIRO_DOCK_CONF_PANEL_WIDTH, g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]),
		MIN (CAIRO_DOCK_CONF_PANEL_HEIGHT, g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - (g_pMainDock && g_pMainDock->container.bIsHorizontal ? g_pMainDock->iMaxDockHeight : 0)));
	
	
	gtk_widget_show_all (s_pMainWindow);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pGroupFrame);
	gtk_widget_hide (s_pPreviewBox);
	
	g_signal_connect (G_OBJECT (s_pMainWindow),
		"delete-event",
		G_CALLBACK (on_delete_main_gui),
		NULL);
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
	
	//\_______________ Set common settings for modules list.
	cairo_dock_prepare_module_list (_("Cairo-Dock configuration"), GINT_TO_POINTER (0));
}

static void cairo_dock_show_one_category (int iCategory)
{
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
	
	//\_______________ Set common settings for modules list.
	cairo_dock_prepare_module_list (gettext (s_cCategoriesDescription[2*iCategory]), GINT_TO_POINTER (iCategory+1));
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
		CAIRO_DOCK_FRAME_MARGIN);
	gtk_widget_show_all (pWidget);
}

static void _reload_current_module_widget (CairoDockModuleInstance *pInstance, int iShowPage)
{
	g_return_if_fail (s_pCurrentGroupWidget != NULL && s_pCurrentGroup != NULL && s_pCurrentWidgetList != NULL);
	
	int iNotebookPage;
	if ((GTK_IS_NOTEBOOK (s_pCurrentGroupWidget)))
	{
		if (iShowPage == -1)
			iNotebookPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget));
		else
			iNotebookPage = iShowPage;
	}
	else
		iNotebookPage = -1;
	
	gtk_widget_destroy (s_pCurrentGroupWidget);
	s_pCurrentGroupWidget = NULL;
	
	cairo_dock_reset_current_widget_list ();
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (s_pCurrentGroup->cGroupName);
	GtkWidget *pWidget;
	if (pModule != NULL)
	{
		gchar *cFile = _get_valid_module_conf_file (pModule);
		pWidget = cairo_dock_present_group_widget (cFile, s_pCurrentGroup, FALSE, pInstance);
		g_free (cFile);
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

static inline gboolean _module_is_opened (CairoDockModuleInstance *pInstance)
{
	if (s_pMainWindow == NULL || s_pCurrentGroup == NULL || s_pCurrentGroup->cGroupName == NULL || s_pCurrentGroup->cConfFilePath == NULL || s_pCurrentWidgetList == NULL || pInstance == NULL || pInstance->cConfFilePath == NULL)
		return FALSE;
	
	if (strcmp (pInstance->pModule->pVisitCard->cModuleName, s_pCurrentGroup->cGroupName) != 0)  // est-on est en train d'editer ce module dans le panneau de conf.
		return FALSE;
	
	if (strcmp (pInstance->cConfFilePath, s_pCurrentGroup->cConfFilePath) != 0)
		return FALSE;  // est-ce cette instance.
	
	return TRUE;
}

static void reload_current_widget (CairoDockModuleInstance *pInstance, int iShowPage)
{
	if (! _module_is_opened (pInstance))
	{
		cairo_dock_gui_items_reload_current_widget (pInstance, iShowPage);
		return;
	}
	
	_reload_current_module_widget (pInstance, iShowPage);
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
	if (pGroupDescription->pExtensions != NULL)
	{
		// on cree les widgets de tous les modules externes dans un meme notebook.
		//g_print ("on cree les widgets de tous les modules externes\n");
		GtkWidget *pNoteBook = NULL;
		GKeyFile* pExtraKeyFile;
		CairoDockModule *pModule;
		CairoDockModuleInstance *pExtraInstance;
		GSList *pExtraCurrentWidgetList = NULL;
		CairoDockGroupDescription *pExtraGroupDescription;
		GList *p;
		for (p = pGroupDescription->pExtensions; p != NULL; p = p->next)
		{
			//g_print (" + %s\n", p->data);
			pModule = cairo_dock_find_module_from_name (p->data);
			if (pModule == NULL || pModule->pVisitCard->cConfFileName == NULL)
				continue;
			
			pExtraGroupDescription = cairo_dock_find_module_description (p->data);
			//g_print (" pExtraGroupDescription : %x\n", pExtraGroupDescription);
			if (pExtraGroupDescription == NULL)
				continue;
			
			g_free (pExtraGroupDescription->cConfFilePath);
			pExtraGroupDescription->cConfFilePath = _get_valid_module_conf_file (pModule);
			
			pExtraKeyFile = cairo_dock_open_key_file (pExtraGroupDescription->cConfFilePath);
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
			GtkWidget *pLabel = gtk_label_new (pGroupDescription->cTitle);
			GtkWidget *pLabelContainer = NULL;
			GtkWidget *pAlign = NULL;
			if (pGroupDescription->cIcon != NULL && *pGroupDescription->cIcon != '\0')
			{
				pLabelContainer = _gtk_hbox_new (CAIRO_DOCK_ICON_MARGIN);
				pAlign = gtk_alignment_new (0., 0.5, 0., 0.);
				gtk_container_add (GTK_CONTAINER (pAlign), pLabelContainer);

				GtkWidget *pImage = gtk_image_new ();
				GdkPixbuf *pixbuf;
				if (*pGroupDescription->cIcon != '/')
				{
					#if (GTK_MAJOR_VERSION < 3)
					pixbuf = gtk_widget_render_icon (pImage,
						pGroupDescription->cIcon ,
						GTK_ICON_SIZE_BUTTON,
						NULL);
					#else
					pixbuf = gtk_widget_render_icon_pixbuf (pImage,
						pGroupDescription->cIcon ,
						GTK_ICON_SIZE_BUTTON);
					#endif
				}
				else
				{
					pixbuf = gdk_pixbuf_new_from_file_at_size (pGroupDescription->cIcon, CAIRO_DOCK_TAB_ICON_SIZE, CAIRO_DOCK_TAB_ICON_SIZE, NULL);
				}
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
	
	//\_______________ On affiche le widget du groupe dans l'interface.
	cairo_dock_hide_all_categories ();
	
	s_pCurrentGroup = pGroupDescription;
	
	gtk_window_set_title (GTK_WINDOW (s_pMainWindow), pGroupDescription->cTitle);
	
	if (/**pInstance != NULL && */pGroupDescription->load_custom_widget != NULL)
		pGroupDescription->load_custom_widget (pInstance, pKeyFile);
	
	cairo_dock_insert_extern_widget_in_gui (pWidget);  // devient le widget courant.
	
	g_key_file_free (pKeyFile);
	
	//\_______________ On met a jour la frame du groupe (label + check-button).
	GtkWidget *pLabel = gtk_label_new (NULL);
	gchar *cLabel = g_strdup_printf ("<span size='x-large' weight='800' color=\"#81728C\">%s</span>", pGroupDescription->cTitle);
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
	if (pGroupDescription->cDependencies != NULL && ! pGroupDescription->bIgnoreDependencies && g_pPrimaryContainer != NULL)
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
				cairo_dock_show_temporary_dialog_with_icon_printf (_("The '%s' module is not present. You need to install it and all its dependencies in order to use this module."), pIcon, CAIRO_CONTAINER (pDock), 10, "same icon", cModuleName);
			}
			else if (pDependencyModule != pModule)
			{
				if (pDependencyModule->pInstancesList == NULL && pDependencyModule->pInterface->initModule != NULL)
				{
					gchar *cWarning = g_strdup_printf (_("The '%s' module is not enabled."), cModuleName);
					gchar *cQuestion = g_strdup_printf ("%s\n%s\n%s", cWarning, gettext (cMessage), _("Do you want to enable it now?"));
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
			_reload_current_module_widget (NULL, -1);
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
	gchar *cConfFilePath;
	if (pModuleInstance)
		cConfFilePath = g_strdup (pModuleInstance->cConfFilePath);
	else
		cConfFilePath = _get_valid_module_conf_file (pModule);
	cairo_dock_present_group_widget (cConfFilePath, pGroupDescription, FALSE, pModuleInstance);
	g_free (cConfFilePath);
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
		cConfFilePath = g_strdup (g_cConfFile);
		bSingleGroup = TRUE;
	}
	else  // c'est un module, on recupere son fichier de conf en entier.
	{
		cConfFilePath = _get_valid_module_conf_file (pModule);
		pModuleInstance = (pModule->pInstancesList != NULL ? pModule->pInstancesList->data : NULL);
		bSingleGroup = FALSE;
	}
	
	cairo_dock_present_group_widget (cConfFilePath, pGroupDescription, bSingleGroup, pModuleInstance);
	g_free (cConfFilePath);
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


static GtkWidget * show_main_gui (void)
{
	GtkWidget *pWindow = cairo_dock_build_main_ihm (g_cConfFile);
	return pWindow;
}

  /////////////
 // BACKEND //
/////////////

static void show_module_gui (const gchar *cModuleName)
{
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cModuleName);
	if (pGroupDescription == NULL)
	{
		cairo_dock_build_main_ihm (g_cConfFile);
		pGroupDescription = cairo_dock_find_module_description (cModuleName);
		g_return_if_fail (pGroupDescription != NULL);
	}
	
	cairo_dock_show_group (pGroupDescription);
}

static void show_module_instance_gui (CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	int iNotebookPage = (GTK_IS_NOTEBOOK (s_pCurrentGroupWidget) ? 
		(iShowPage >= 0 ?
			iShowPage :
			gtk_notebook_get_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget))) :
		-1);
	
	cairo_dock_build_main_ihm (g_cConfFile);
	cairo_dock_present_module_instance_gui (pModuleInstance);
	cairo_dock_toggle_category_button (pModuleInstance->pModule->pVisitCard->iCategory);  // on active la categorie du module.
	
	if (iNotebookPage != -1)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget), iNotebookPage);
	}
}

static void close_gui (void)
{
	if (s_pMainWindow != NULL)
	{
		on_click_quit (NULL, s_pMainWindow);
	}
}

static void update_module_state (const gchar *cModuleName, gboolean bActive)
//static void deactivate_module_in_gui (const gchar *cModuleName)
{
	if (s_pMainWindow == NULL || cModuleName == NULL)
		return ;
	
	if (s_pCurrentGroup != NULL && s_pCurrentGroup->cGroupName != NULL && strcmp (cModuleName, s_pCurrentGroup->cGroupName) == 0)  // on est en train d'editer ce module dans le panneau de conf.
	{
		g_signal_handlers_block_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (s_pActivateButton), bActive);
		g_signal_handlers_unblock_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
	}
	
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cModuleName);
	g_return_if_fail (pGroupDescription != NULL && pGroupDescription->pActivateButton != NULL);
	g_signal_handlers_block_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pGroupDescription->pActivateButton), bActive);
	g_signal_handlers_unblock_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
}

/**static gboolean _module_is_opened (CairoDockModuleInstance *pModuleInstance)
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
}*/
static inline gboolean _desklet_is_opened (CairoDesklet *pDesklet)
{
	if (s_pMainWindow == NULL || pDesklet == NULL)
		return FALSE;
	Icon *pIcon = pDesklet->pIcon;
	g_return_val_if_fail (pIcon != NULL, FALSE);
	
	CairoDockModuleInstance *pModuleInstance = pIcon->pModuleInstance;
	g_return_val_if_fail (pModuleInstance != NULL, FALSE);
	
	return _module_is_opened (pModuleInstance);
}
static void update_desklet_params (CairoDesklet *pDesklet)
{
	if (! _desklet_is_opened (pDesklet))
	{
		cairo_dock_gui_items_update_desklet_params (pDesklet);
		return ;
	}
	cairo_dock_update_desklet_widgets (pDesklet, s_pCurrentWidgetList);
}

static void update_desklet_visibility_params (CairoDesklet *pDesklet)
{
	if (! _desklet_is_opened (pDesklet))
	{
		cairo_dock_update_desklet_visibility_params (pDesklet);
		return ;
	}
	cairo_dock_update_desklet_visibility_widgets (pDesklet, s_pCurrentWidgetList);
}

static void update_module_instance_container (CairoDockModuleInstance *pInstance, gboolean bDetached)
{
	if (! _module_is_opened (pInstance))
	{
		cairo_dock_gui_items_update_module_instance_container (pInstance, bDetached);
		return;
	}
	cairo_dock_update_is_detached_widget (bDetached, s_pCurrentWidgetList);
}

static void update_modules_list (void)
{
	if (s_pMainWindow == NULL)
		return ;
	
	// On detruit la liste des boutons de chaque groupe.
	gchar *cCurrentGroupName = (s_pCurrentGroup ? g_strdup (s_pCurrentGroup->cGroupName) : NULL);
	GList *gd;
	CairoDockGroupDescription *pGroupDescription;
	for (gd = s_pGroupDescriptionList; gd != NULL; gd = gd->next)
	{
		pGroupDescription = gd->data;
		gtk_widget_destroy (pGroupDescription->pGroupHBox);
		pGroupDescription->pGroupHBox = NULL;
		_cairo_dock_free_group_description (pGroupDescription);
	}
	g_list_free (s_pGroupDescriptionList);
	s_pGroupDescriptionList = NULL;
	s_pCurrentGroup = NULL;
	
	g_slist_free (s_path);
	s_path = NULL;
		
	// on reset les tables de chaque categorie.
	int i;
	CairoDockCategoryWidgetTable *pCategoryWidget;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		pCategoryWidget->iNbItemsInCurrentRow = 0;
		pCategoryWidget->iNbRows = 0;
	}
	
	// on recree chaque groupe.
	_add_main_groups_buttons ();
	
	cairo_dock_foreach_module_in_alphabetical_order ((GCompareFunc) _cairo_dock_add_one_module_widget, NULL);
	
	// on retrouve le groupe courant.
	if (cCurrentGroupName != NULL)
	{
		for (gd = s_pGroupDescriptionList; gd != NULL; gd = gd->next)
		{
			pGroupDescription = gd->data;
			if (pGroupDescription->cGroupName && strcmp (cCurrentGroupName, pGroupDescription->cGroupName) == 0)
				s_pCurrentGroup = pGroupDescription;
		}
		g_free (cCurrentGroupName);
	}
	
	gtk_widget_show_all (s_pMainWindow);
}

static void update_shortkeys (void)
{
	if (s_pMainWindow == NULL
	|| s_pCurrentGroup == NULL
	|| s_pCurrentGroup->cGroupName == NULL
	|| strcmp (s_pCurrentGroup->cGroupName, "Shortkeys") != 0)  // the Shortkeys widget is not currently displayed => nothing to do.
		return ;
	
	CairoDockGroupKeyWidget *pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (s_pCurrentWidgetList, "Shortkeys", "shortkeys");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	
	GtkWidget *pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	GtkTreeModel *pModel = gtk_tree_view_get_model (GTK_TREE_VIEW (pOneWidget));
	g_return_if_fail (pModel != NULL);
	gtk_list_store_clear (GTK_LIST_STORE (pModel));
	cd_keybinder_foreach ((GFunc) cairo_dock_add_shortkey_to_model, pModel);
}

static void set_status_message_on_gui (const gchar *cMessage)
{
	if (s_pStatusBar == NULL)
	{
		cairo_dock_gui_items_set_status_message_on_gui (cMessage);
		return;
	}
	gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);  // clear any previous message, underflow is allowed.
	gtk_statusbar_push (GTK_STATUSBAR (s_pStatusBar), 0, cMessage);
}

static CairoDockGroupKeyWidget *get_widget_from_name (CairoDockModuleInstance *pInstance, const gchar *cGroupName, const gchar *cKeyName)
{
	if (!_module_is_opened (pInstance))
		return cairo_dock_gui_items_get_widget_from_name (pInstance, cGroupName, cKeyName);
	
	CairoDockGroupKeyWidget *pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (s_pCurrentWidgetList, cGroupName, cKeyName);
	if (pGroupKeyWidget == NULL && s_pExtraCurrentWidgetList != NULL)
	{
		GSList *l;
		for (l = s_pExtraCurrentWidgetList; l != NULL; l = l->next)
		{
			pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (l->data, cGroupName, cKeyName);
			if (pGroupKeyWidget != NULL)
				break ;
		}
	}
	return pGroupKeyWidget;
}

void cairo_dock_register_main_gui_backend (void)
{
	CairoDockMainGuiBackend *pBackend = g_new0 (CairoDockMainGuiBackend, 1);
	
	pBackend->show_main_gui 					= show_main_gui;
	pBackend->show_module_gui 					= show_module_gui;
	//pBackend->show_module_instance_gui 		= show_module_instance_gui;
	pBackend->close_gui 						= close_gui;
	pBackend->update_module_state 				= update_module_state;
	pBackend->update_desklet_params 			= update_desklet_params;
	pBackend->update_desklet_visibility_params 	= update_desklet_visibility_params;
	pBackend->update_module_instance_container 	= update_module_instance_container;
	pBackend->update_modules_list 				= update_modules_list;
	pBackend->update_shortkeys 					= update_shortkeys;
	pBackend->bCanManageThemes 					= FALSE;
	pBackend->cDisplayedName 					= _("Simple Mode");
	pBackend->cTooltip 							= NULL;
	
	cairo_dock_register_config_gui_backend (pBackend);
	
	CairoDockGuiBackend *pConfigBackend = g_new0 (CairoDockGuiBackend, 1);
	
	pConfigBackend->set_status_message_on_gui 	= set_status_message_on_gui;
	pConfigBackend->reload_current_widget 		= reload_current_widget;
	pConfigBackend->show_module_instance_gui 	= show_module_instance_gui;
	pConfigBackend->get_widget_from_name 		= get_widget_from_name;
	
	cairo_dock_register_gui_backend (pConfigBackend);
}

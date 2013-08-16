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

#include <stdlib.h>
#include <string.h>

#include "applet-struct.h"
#include "applet-tips-dialog.h"

static void _on_tips_category_changed (GtkComboBox *pWidget, gpointer *data);

typedef struct {
	GKeyFile *pKeyFile;
	gchar **pGroupList;
	gint iNbGroups;
	gchar **pKeyList;  // keys of the current group
	gsize iNbKeys;
	gint iNumTipGroup;  // current group being displayed.
	gint iNumTipKey;  // current key being displayed.
	GtkWidget *pCategoryCombo;
	} CDTipsData;

static void _cairo_dock_get_next_tip (CDTipsData *pTips)
{
	pTips->iNumTipKey ++;  // skip the current expander to go to the current label, which will be skipped in the first iteration.
	const gchar *cGroupName = pTips->pGroupList[pTips->iNumTipGroup];
	gboolean bOk;
	do
	{
		pTips->iNumTipKey ++;
		if (pTips->iNumTipKey >= (gint) pTips->iNbKeys)  // no more key, go to next group.
		{
			pTips->iNumTipGroup ++;
			if (pTips->iNumTipGroup >= pTips->iNbGroups)  // no more group, restart from first group.
				pTips->iNumTipGroup = 0;
			pTips->iNumTipKey = 0;
			
			// since the group has changed, get the keys again.
			g_strfreev (pTips->pKeyList);
			cGroupName = pTips->pGroupList[pTips->iNumTipGroup];
			pTips->pKeyList = g_key_file_get_keys (pTips->pKeyFile, cGroupName, &pTips->iNbKeys, NULL);
			
			// and update the category in the comb
			g_signal_handlers_block_matched (pTips->pCategoryCombo,
				G_SIGNAL_MATCH_FUNC,
				0,
				0,
				0,
				_on_tips_category_changed,
				NULL);
			gtk_combo_box_set_active (GTK_COMBO_BOX (pTips->pCategoryCombo), pTips->iNumTipGroup);
			g_signal_handlers_unblock_matched (pTips->pCategoryCombo,
				G_SIGNAL_MATCH_FUNC,
				0,
				0,
				0,
				_on_tips_category_changed,
				NULL);
		}
		
		// check if the key is an expander widget.
		const gchar *cKeyName = pTips->pKeyList[pTips->iNumTipKey];
		gchar *cKeyComment =  g_key_file_get_comment (pTips->pKeyFile, cGroupName, cKeyName, NULL);
		bOk = (cKeyComment && *cKeyComment == CAIRO_DOCK_WIDGET_EXPANDER);  // whether it's an expander.
		g_free (cKeyComment);
	} while (!bOk);
}

static void _cairo_dock_get_previous_tip (CDTipsData *pTips)
{
	pTips->iNumTipKey --;
	
	const gchar *cGroupName = pTips->pGroupList[pTips->iNumTipGroup];
	gboolean bOk;
	do
	{
		pTips->iNumTipKey --;
		if (pTips->iNumTipKey < 0)  // no more key, go to previous group.
		{
			pTips->iNumTipGroup --;
			if (pTips->iNumTipGroup < 0)  // no more group, restart from the last group.
				pTips->iNumTipGroup = pTips->iNbGroups - 1;
			
			// since the group has changed, get the keys again.
			g_strfreev (pTips->pKeyList);
			cGroupName = pTips->pGroupList[pTips->iNumTipGroup];
			pTips->pKeyList = g_key_file_get_keys (pTips->pKeyFile, cGroupName, &pTips->iNbKeys, NULL);

			pTips->iNumTipKey = pTips->iNbKeys - 2;
			
			// and update the category in the comb
			g_signal_handlers_block_matched (pTips->pCategoryCombo,
				G_SIGNAL_MATCH_FUNC,
				0,
				0,
				0,
				_on_tips_category_changed,
				NULL);
			gtk_combo_box_set_active (GTK_COMBO_BOX (pTips->pCategoryCombo), pTips->iNumTipGroup);
			g_signal_handlers_unblock_matched (pTips->pCategoryCombo,
				G_SIGNAL_MATCH_FUNC,
				0,
				0,
				0,
				_on_tips_category_changed,
				NULL);
		}
		
		// check if the key is an expander widget.
		const gchar *cKeyName = pTips->pKeyList[pTips->iNumTipKey];
		gchar *cKeyComment =  g_key_file_get_comment (pTips->pKeyFile, cGroupName, cKeyName, NULL);
		bOk = (cKeyComment && *cKeyComment == CAIRO_DOCK_WIDGET_EXPANDER);  // whether it's an expander.
	} while (!bOk);
}

static gchar *_build_tip_text (CDTipsData *pTips)
{
	const gchar *cGroupName = pTips->pGroupList[pTips->iNumTipGroup];
	const gchar *cKeyName1 = pTips->pKeyList[pTips->iNumTipKey];
	const gchar *cKeyName2 = pTips->pKeyList[pTips->iNumTipKey+1];
	
	char iElementType;
	guint iNbElements = 0;
	gboolean bAligned;
	const gchar *cHint1 = NULL;  // points on the comment.
	gchar **pAuthorizedValuesList1 = NULL;
	gchar *cKeyComment1 =  g_key_file_get_comment (pTips->pKeyFile, cGroupName, cKeyName1, NULL);
	cairo_dock_parse_key_comment (cKeyComment1, &iElementType, &iNbElements, &pAuthorizedValuesList1, &bAligned, &cHint1);  // points on the comment.
	
	const gchar *cHint2 = NULL;
	gchar **pAuthorizedValuesList2 = NULL;
	gchar *cKeyComment2 =  g_key_file_get_comment (pTips->pKeyFile, cGroupName, cKeyName2, NULL);
	const gchar *cText2 = cairo_dock_parse_key_comment (cKeyComment2, &iElementType, &iNbElements, &pAuthorizedValuesList2, &bAligned, &cHint2);
	
	gchar *cText = g_strdup_printf ("<b>%s</b>\n\n<i>%s</i>\n\n%s",
		_("Tips and Tricks"),
		(pAuthorizedValuesList1 ? gettext (pAuthorizedValuesList1[0]) : ""),
		gettext (cText2));
	
	g_strfreev (pAuthorizedValuesList1);
	g_strfreev (pAuthorizedValuesList2);
	g_free (cKeyComment1);
	g_free (cKeyComment2);
	return cText;
}
static void _update_tip_text (CDTipsData *pTips, CairoDialog *pDialog)
{
	gchar *cText = _build_tip_text (pTips);
	
	gldi_dialog_set_message (pDialog, cText);
	
	g_free (cText);
}
static void _tips_dialog_action (int iClickedButton, G_GNUC_UNUSED GtkWidget *pInteractiveWidget, CDTipsData *pTips, CairoDialog *pDialog)
{
	cd_debug ("%s (%d)", __func__, iClickedButton);
	if (iClickedButton == 2 || iClickedButton == -1)  // click on "next", or Enter.
	{
		// show the next tip
		_cairo_dock_get_next_tip (pTips);
		
		_update_tip_text (pTips, pDialog);
		
		gldi_object_ref (GLDI_OBJECT(pDialog));  // keep the dialog alive.
	}
	else if (iClickedButton == 1)  // click on "previous"
	{
		// show the previous tip
		_cairo_dock_get_previous_tip (pTips);
		
		_update_tip_text (pTips, pDialog);
		
		gldi_object_ref (GLDI_OBJECT(pDialog));  // keep the dialog alive.
	}
	else  // click on "close" or Escape
	{
		myData.iLastTipGroup = pTips->iNumTipGroup;
		myData.iLastTipKey = pTips->iNumTipKey;
		gchar *cConfFilePath = g_strdup_printf ("%s/.help", g_cCairoDockDataDir);
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_INT, "Last Tip", "group", pTips->iNumTipGroup,
			G_TYPE_INT, "Last Tip", "key", pTips->iNumTipKey,
			G_TYPE_INVALID);
		g_free (cConfFilePath);
	}
	cd_debug ("tips : %d/%d", pTips->iNumTipGroup, pTips->iNumTipKey);
}
static void _on_free_tips_dialog (CDTipsData *pTips)
{
	g_key_file_free (pTips->pKeyFile);
	g_strfreev (pTips->pGroupList);
	g_strfreev (pTips->pKeyList);
	g_free (pTips);
}

static void _on_tips_category_changed (GtkComboBox *pWidget, gpointer *data)
{
	CDTipsData *pTips = data[0];
	CairoDialog *pDialog = data[1];
	
	int iNumItem = gtk_combo_box_get_active (pWidget);
	g_return_if_fail (iNumItem < pTips->iNbGroups);
	
	pTips->iNumTipGroup = iNumItem;
	
	// since the group has changed, get the keys again.
	g_strfreev (pTips->pKeyList);
	const gchar *cGroupName = pTips->pGroupList[pTips->iNumTipGroup];
	pTips->pKeyList = g_key_file_get_keys (pTips->pKeyFile, cGroupName, &pTips->iNbKeys, NULL);
	pTips->iNumTipKey = 0;
	
	_update_tip_text (pTips, pDialog);
}

void cairo_dock_show_tips (void)
{
	if (myData.iSidGetParams != 0)
		return;
	
	// open the tips file
	const gchar *cConfFilePath = CD_APPLET_MY_CONF_FILE;
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	gsize iNbGroups = 0;
	gchar **pGroupList = g_key_file_get_groups (pKeyFile, &iNbGroups);
	iNbGroups -= 4;   // skip the last 4 groups (Troubleshooting and Contribute + Icon and Desklet).
	g_return_if_fail (pGroupList != NULL && iNbGroups > 0);
	
	// get the last displayed tip.
	guint iNumTipGroup, iNumTipKey;
	if (myData.iLastTipGroup < 0 || myData.iLastTipKey < 0)  // first time we display a tip.
	{
		iNumTipGroup = iNumTipKey = 0;
	}
	else
	{
		iNumTipGroup = myData.iLastTipGroup;
		iNumTipKey = myData.iLastTipKey;
		if (iNumTipGroup >= iNbGroups)  // be sure to stay inside the limits.
		{
			iNumTipGroup = iNbGroups - 1;
			iNumTipKey = 0;
		}
	}
	const gchar *cGroupName = pGroupList[iNumTipGroup];
	
	gsize iNbKeys = 0;
	gchar **pKeyList = g_key_file_get_keys (pKeyFile, cGroupName, &iNbKeys, NULL);
	g_return_if_fail (pKeyList != NULL && iNbKeys > 0);
	if (iNumTipKey >= iNbKeys)  // be sure to stay inside the limits.
		iNumTipKey = 0;
	
	CDTipsData *pTips = g_new0 (CDTipsData, 1);
	pTips->pKeyFile = pKeyFile;
	pTips->pGroupList = pGroupList;
	pTips->iNbGroups = iNbGroups;
	pTips->pKeyList = pKeyList;
	pTips->iNbKeys = iNbKeys;
	pTips->iNumTipGroup = iNumTipGroup;
	pTips->iNumTipKey = iNumTipKey;
	
	// update to the next tip.
	if (myData.iLastTipGroup >= 0 && myData.iLastTipKey >= 0)  // a previous tip exist => take the next one;
		_cairo_dock_get_next_tip (pTips);
	
	// build a list of the available groups.
	GtkWidget *pInteractiveWidget = _gtk_vbox_new (3);
	#if (GTK_MAJOR_VERSION < 3 && GTK_MINOR_VERSION < 24)
	GtkWidget *pComboBox = gtk_combo_box_new_text ();
	#else
	GtkWidget *pComboBox = gtk_combo_box_text_new ();
	#endif
	guint i;
	for (i = 0; i < iNbGroups; i ++)
	{
		#if (GTK_MAJOR_VERSION < 3 && GTK_MINOR_VERSION < 24)
		gtk_combo_box_append_text (GTK_COMBO_BOX (pComboBox), gettext (pGroupList[i]));
		#else
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (pComboBox), gettext (pGroupList[i]));
		#endif
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (pComboBox), pTips->iNumTipGroup);
	pTips->pCategoryCombo = pComboBox;
	static gpointer data_combo[2];
	data_combo[0] = pTips;  // the 2nd data is the dialog, we'll set it after we make it.
	g_signal_connect (G_OBJECT (pComboBox), "changed", G_CALLBACK(_on_tips_category_changed), data_combo);
	GtkWidget *pJumpBox = _gtk_hbox_new (3);
	GtkWidget *label = gtk_label_new (_("Category"));
	gldi_dialog_set_widget_text_color (label);
	gtk_box_pack_end (GTK_BOX (pJumpBox), pComboBox, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (pJumpBox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pInteractiveWidget), pJumpBox, FALSE, FALSE, 0);
	
	// build the dialog.
	gchar *cText = _build_tip_text (pTips);
	CairoDialogAttr attr;
	memset (&attr, 0, sizeof (CairoDialogAttr));
	attr.cText = cText;
	attr.cImageFilePath = NULL;
	attr.pInteractiveWidget = pInteractiveWidget;
	attr.pActionFunc = (CairoDockActionOnAnswerFunc)_tips_dialog_action;
	attr.pUserData = pTips;
	attr.pFreeDataFunc = (GFreeFunc)_on_free_tips_dialog;
	/// GTK_STOCK is now deprecated, here is a temporary fix to avoid compilation errors
	#if GTK_CHECK_VERSION(3, 9, 8)
	const gchar *cButtons[] = {"cancel", "gtk-go-forward-rtl", "gtk-go-forward-ltr", NULL};
	#else
	const gchar *cButtons[] = {"cancel", GTK_STOCK_GO_FORWARD"-rtl", GTK_STOCK_GO_FORWARD"-ltr", NULL};
	#endif
	attr.cButtonsImage = cButtons;
	attr.bUseMarkup = TRUE;
	attr.pIcon = myIcon;
	attr.pContainer = myContainer;
	CairoDialog *pTipsDialog = gldi_dialog_new (&attr);
	
	data_combo[1] = pTipsDialog;
	
	g_free (cText);
}

/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-log.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-gui-callbacks.h"

#define CAIRO_DOCK_PREVIEW_WIDTH 250
#define CAIRO_DOCK_PREVIEW_HEIGHT 250

extern gchar *g_cConfFile;
extern CairoDock *g_pMainDock;
static CairoDialog *s_pDialog = NULL;
static int s_iSidShowGroupDialog = 0;

void on_click_category_button (GtkButton *button, gpointer data)
{
	int iCategory = GPOINTER_TO_INT (data);
	//g_print ("%s (%d)\n", __func__, iCategory);
	cairo_dock_show_one_category (iCategory);
}

void on_click_all_button (GtkButton *button, gpointer data)
{
	//g_print ("%s ()\n", __func__);
	cairo_dock_show_all_categories ();
}


void on_click_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription)
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
void on_click_back_button (GtkButton *button, gpointer data)
{
	gpointer pPrevPlace = cairo_dock_get_previous_widget ();
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
	
	int iPreviewWidgetWidth;
	GtkWidget *pPreviewImage = cairo_dock_get_preview_image (&iPreviewWidgetWidth);
	if (pGroupDescription->cPreviewFilePath != NULL)
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
			pPreviewPixbuf = gdk_pixbuf_new_from_file_at_size (pGroupDescription->cPreviewFilePath, iPreviewWidth, iPreviewHeight, NULL);
		}
		if (pPreviewPixbuf == NULL)
		{
			cd_warning ("pas de prevue disponible");
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
	if (pIcon == NULL || pIcon->cParentDockName == NULL || pIcon->fPersonnalScale > 0)
		pIcon = cairo_dock_get_dialogless_icon ();
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon != NULL ? pIcon->cParentDockName : NULL);
	s_pDialog = cairo_dock_show_temporary_dialog_with_icon (dgettext (pGroupDescription->cGettextDomain, cDescription != NULL ? cDescription : pGroupDescription->cDescription), pIcon, CAIRO_CONTAINER (pDock), 0., pGroupDescription->cIcon);
	cairo_dock_dialog_reference (s_pDialog);
	
	g_free (cDescription);

	s_iSidShowGroupDialog = 0;
	return FALSE;
}
void on_enter_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription)
{
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cDescription);
	if (g_pMainDock == NULL)  // inutile en maintenance, le dialogue risque d'apparaitre sur la souris.
		return ;
	
	if (s_iSidShowGroupDialog != 0)
		g_source_remove (s_iSidShowGroupDialog);
	
	s_iSidShowGroupDialog = g_timeout_add (500, (GSourceFunc)_show_group_dialog, (gpointer) pGroupDescription);
}
void on_leave_group_button (GtkButton *button, gpointer *data)
{
	//g_print ("%s ()\n", __func__);
	if (s_iSidShowGroupDialog != 0)
	{
		g_source_remove (s_iSidShowGroupDialog);
		s_iSidShowGroupDialog = 0;
	}

	int iPreviewWidth;
	GtkWidget *pPreviewImage = cairo_dock_get_preview_image (&iPreviewWidth);
	gtk_widget_hide (pPreviewImage);
	
	if (! cairo_dock_dialog_unreference (s_pDialog))
		cairo_dock_dialog_unreference (s_pDialog);
	s_pDialog = NULL;
}


void on_click_apply (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	
	CairoDockGroupDescription *pGroupDescription = cairo_dock_get_current_group ();
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
		if (pInternalModule != NULL)
		{
			//g_print ("found module %s\n", pInternalModule->cModuleName);
			cairo_dock_reload_internal_module (pInternalModule, g_cConfFile);

		}
		else
			cairo_dock_read_conf_file (g_cConfFile, g_pMainDock);
	}
}

void on_click_ok (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	
	on_click_apply (button, pWindow);
	on_click_quit (button, pWindow);
}

void on_click_quit (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	GMainLoop *pBlockingLoop = g_object_get_data (G_OBJECT (pWindow), "loop");
	on_delete_main_gui (pWindow, NULL, pBlockingLoop);
	gtk_widget_destroy (pWindow);
}

void on_click_activate_given_group (GtkToggleButton *button, CairoDockGroupDescription *pGroupDescription)
{
	g_return_if_fail (pGroupDescription != NULL);
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cGroupName);
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	g_return_if_fail (pModule != NULL);
	if (g_pMainDock == NULL)
	{
		cairo_dock_add_remove_element_to_key (g_cConfFile,  "System", "modules", pGroupDescription->cGroupName, gtk_toggle_button_get_active (button));
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

void on_click_activate_current_group (GtkToggleButton *button, gpointer *data)
{
	//g_print ("%s ()\n", __func__);
	CairoDockGroupDescription *pGroupDescription = cairo_dock_get_current_group ();
	//g_print ("%s\n", pGroupDescription->cGroupName);
	
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



void on_click_normal_apply (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	GSList *pWidgetList = g_object_get_data (G_OBJECT (pWindow), "widget-list");
	gchar *cConfFilePath = g_object_get_data (G_OBJECT (pWindow), "conf-file");
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	cairo_dock_update_keyfile_from_widget_list (pKeyFile, pWidgetList);
	cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	g_key_file_free (pKeyFile);
	
	CairoDockApplyConfigFunc pAction = g_object_get_data (G_OBJECT (pWindow), "action");
	gpointer pUserData = g_object_get_data (G_OBJECT (pWindow), "action-data");
	
	if (pAction != NULL)
		pAction (pUserData);
	else
		g_object_set_data (G_OBJECT (pWindow), "result", GINT_TO_POINTER (1));
}

void on_click_normal_quit (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	GMainLoop *pBlockingLoop = g_object_get_data (G_OBJECT (pWindow), "loop");
	on_delete_normal_gui (pWindow, NULL, pBlockingLoop);
	if (pBlockingLoop == NULL)
		gtk_widget_destroy (pWindow);
}

void on_click_normal_ok (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	
	on_click_normal_apply (button, pWindow);
	on_click_normal_quit (button, pWindow);
}


gboolean on_delete_main_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	cd_debug ("%s (%x)", __func__, pBlockingLoop);
	if (pBlockingLoop != NULL)
	{
		cd_debug ("dialogue detruit, on sort de la boucle");
		if (g_main_loop_is_running (pBlockingLoop))
			g_main_loop_quit (pBlockingLoop);
	}
	cairo_dock_free_categories ();
	if (s_iSidShowGroupDialog != 0)
	{
		g_source_remove (s_iSidShowGroupDialog);
		s_iSidShowGroupDialog = 0;
	}
	return FALSE;
}

gboolean on_delete_normal_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	//g_print ("%s ()\n", __func__);
	
	if (pBlockingLoop != NULL && g_main_loop_is_running (pBlockingLoop))
	{
		g_main_loop_quit (pBlockingLoop);
	}
	
	gpointer pUserData = g_object_get_data (G_OBJECT (pWidget), "action-data");
	GFreeFunc pFreeUserData = g_object_get_data (G_OBJECT (pWidget), "free-data");
	if (pFreeUserData != NULL && pUserData != NULL)
		pFreeUserData (pUserData);
	
	GSList *pWidgetList = g_object_get_data (G_OBJECT (pWidget), "widget-list");
	cairo_dock_free_generated_widget_list (pWidgetList);
	cairo_dock_config_panel_destroyed ();
	
	GPtrArray *pDataGarbage = g_object_get_data (G_OBJECT (pWidget), "garbage");
	/// nettoyer.
	
	return FALSE;
}


static gboolean bAllWords = FALSE;
static gboolean bSearchInToolTip = FALSE;
static gboolean bHighLightText = FALSE;
static gboolean bHideOther = FALSE;

void cairo_dock_reset_filter_state (void)
{
	bAllWords = FALSE;
	bSearchInToolTip = TRUE;
	bHighLightText = TRUE;
	bHideOther = TRUE;
}

void cairo_dock_activate_filter (GtkEntry *pEntry, gpointer data)
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
void cairo_dock_toggle_all_words (GtkToggleButton *pButton, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bAllWords = gtk_toggle_button_get_active (pButton);
	cairo_dock_trigger_current_filter ();
}
void cairo_dock_toggle_search_in_tooltip (GtkToggleButton *pButton, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bSearchInToolTip = gtk_toggle_button_get_active (pButton);
	cairo_dock_trigger_current_filter ();
}
void cairo_dock_toggle_highlight_words (GtkToggleButton *pButton, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bHighLightText = gtk_toggle_button_get_active (pButton);
	cairo_dock_trigger_current_filter ();
}
void cairo_dock_toggle_hide_others (GtkToggleButton *pButton, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bHideOther = gtk_toggle_button_get_active (pButton);
	cairo_dock_trigger_current_filter ();
}
void cairo_dock_clear_filter (GtkButton *pButton, GtkEntry *pEntry)
{
	gtk_entry_set_text (pEntry, "");
	gpointer pCurrentPlace = cairo_dock_get_current_widget ();
	//g_print ("pCurrentPlace : %x\n", pCurrentPlace);
	//_show_group_or_category (pCurrentPlace);
	gchar *keyword[2] = {"fabounetfabounetfabounet", NULL};
	cairo_dock_apply_current_filter (keyword, FALSE, FALSE, FALSE, FALSE);
}

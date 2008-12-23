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

void on_click_category_button (GtkButton *button, gpointer *data)
{
	int iCategory = GPOINTER_TO_INT (data);
	g_print ("%s (%d)\n", __func__, iCategory);
	cairo_dock_show_one_category (iCategory);
}

void on_click_all_button (GtkButton *button, gpointer *data)
{
	g_print ("%s ()\n", __func__);
	cairo_dock_show_all_categories ();
}

static void _show_group (CairoDockGroupDescription *pGroupDescription)
{
	gboolean bSingleGroup;
	gchar *cConfFilePath;
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
		g_print ("  %s (%s)\n", __func__, pModule->cConfFilePath);
	
		cairo_dock_update_applet_conf_file_with_containers (NULL, pModule->cConfFilePath);
		
		cConfFilePath = pModule->cConfFilePath;
		bSingleGroup = FALSE;
	}
	
	cairo_dock_present_group_widget (cConfFilePath, pGroupDescription, bSingleGroup);
}
void on_click_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription)
{
	g_print ("%s (%s)\n", __func__, pGroupDescription->cGroupName);
	_show_group (pGroupDescription);
}

void on_click_back_button (GtkButton *button, gpointer *data)
{
	gpointer pPrevPlace = cairo_dock_get_previous_widget ();
	if (pPrevPlace == NULL)
		cairo_dock_show_all_categories ();
	else if ((int)pPrevPlace < 10)  // categorie.
	{
		if (pPrevPlace == 0)
			cairo_dock_show_all_categories ();
		else
		{
			int iCategory = (int)pPrevPlace - 1;
			cairo_dock_show_one_category (iCategory);
		}
	}
	else  // groupe.
	{
		_show_group (pPrevPlace);
	}
}

void on_enter_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription)
{
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cDescription);
	gchar *cDescription = NULL;
	
	//GtkWidget *pDescriptionTextView = cairo_dock_get_description_label ();
	if (pGroupDescription->cDescription != NULL)
	{
		if (*pGroupDescription->cDescription == '/')
		{
			g_print ("on recupere la description de %s\n", pGroupDescription->cDescription);
			
			gsize length = 0;
			GError *erreur = NULL;
			g_file_get_contents  (pGroupDescription->cDescription,
				&cDescription,
				&length,
				&erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
			}
			/*GtkTextBuffer *pTextBuffer = gtk_text_buffer_new (NULL);
			gtk_text_buffer_set_text (pTextBuffer, cDescription, -1);
			gtk_text_view_set_buffer (GTK_TEXT_VIEW (pDescriptionTextView), pTextBuffer);
			g_object_unref (pTextBuffer);*/
		}
		else
		{
			/*GtkTextBuffer *pTextBuffer = gtk_text_buffer_new (NULL);
			gtk_text_buffer_set_text (pTextBuffer, pGroupDescription->cDescription, -1);
			gtk_text_view_set_buffer (GTK_TEXT_VIEW (pDescriptionTextView), pTextBuffer);
			g_object_unref (pTextBuffer);*/
		}
		//gtk_widget_show (pDescriptionTextView);
	}
	
	GtkWidget *pPreviewImage = cairo_dock_get_preview_image ();
	if (pGroupDescription->cPreviewFilePath != NULL)
	{
		g_print ("on recupere la prevue de %s\n", pGroupDescription->cPreviewFilePath);
		int iPreviewWidth, iPreviewHeight;
		GdkPixbuf *pPreviewPixbuf = NULL;
		if (gdk_pixbuf_get_file_info (pGroupDescription->cPreviewFilePath, &iPreviewWidth, &iPreviewHeight) != NULL)
		{
			iPreviewWidth = MIN (iPreviewWidth, CAIRO_DOCK_PREVIEW_WIDTH);
			iPreviewHeight = MIN (iPreviewHeight, CAIRO_DOCK_PREVIEW_HEIGHT);
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
	if (pIcon == NULL)
		pIcon = cairo_dock_get_dialogless_icon ();
	//if (pIcon != NULL)
	//	cairo_dock_remove_dialog_if_any (pIcon);
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon != NULL ? pIcon->cParentDockName : NULL);
	s_pDialog = cairo_dock_show_temporary_dialog_with_icon (cDescription != NULL ? cDescription : pGroupDescription->cDescription, pIcon, CAIRO_CONTAINER (pDock), 0., pGroupDescription->cIcon);
	cairo_dock_dialog_reference (s_pDialog);
	
	g_free (cDescription);
}
void on_leave_group_button (GtkButton *button, gpointer *data)
{
	//g_print ("%s ()\n", __func__);
	//GtkWidget *pDescriptionTextView = cairo_dock_get_description_label ();
	//gtk_widget_hide (pDescriptionTextView);
	
	GtkWidget *pPreviewImage = cairo_dock_get_preview_image ();
	gtk_widget_hide (pPreviewImage);
	
	if (! cairo_dock_dialog_unreference (s_pDialog))
		cairo_dock_dialog_unreference (s_pDialog);
	s_pDialog = NULL;
}


void on_click_apply (GtkButton *button, GtkWidget *pWindow)
{
	g_print ("%s ()\n", __func__);
	
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
			
			cairo_dock_write_current_group_conf_file (pModuleInstance->cConfFilePath);
			cairo_dock_reload_module_instance (pModuleInstance, TRUE);
		}
	}
	else
	{
		cairo_dock_write_current_group_conf_file (g_cConfFile);
		CairoDockInternalModule *pInternalModule = cairo_dock_find_internal_module_from_name (pGroupDescription->cGroupName);
		if (pInternalModule != NULL)
		{
			g_print ("found module %s\n", pInternalModule->cModuleName);
			cairo_dock_reload_internal_module (pInternalModule, g_cConfFile);

		}
		else
			cairo_dock_read_conf_file (g_cConfFile, g_pMainDock);
	}
}

void on_click_ok (GtkButton *button, GtkWidget *pWindow)
{
	g_print ("%s ()\n", __func__);
	
	on_click_apply (button, pWindow);
	on_click_quit (button, pWindow);
}

void on_click_quit (GtkButton *button, GtkWidget *pWindow)
{
	g_print ("%s ()\n", __func__);
	GMainLoop *pBlockingLoop = g_object_get_data (G_OBJECT (pWindow), "loop");
	on_delete_main_gui (pWindow, NULL, pBlockingLoop);
	gtk_widget_destroy (pWindow);
}

void on_click_activate_given_group (GtkToggleButton *button, CairoDockGroupDescription *pGroupDescription)
{
	g_return_if_fail (pGroupDescription != NULL);
	g_print ("%s (%s)\n", __func__, pGroupDescription->cGroupName);
	
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
	g_print ("%s ()\n", __func__);
	CairoDockGroupDescription *pGroupDescription = cairo_dock_get_current_group ();
	g_print ("%s\n", pGroupDescription->cGroupName);
	
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
	g_print ("%s ()\n", __func__);
	GSList *pWidgetList = g_object_get_data (G_OBJECT (pWindow), "widget-list");
	gchar *cConfFilePath = g_object_get_data (G_OBJECT (pWindow), "conf-file");
	GKeyFile *pKeyFile = g_key_file_new ();

	GError *erreur = NULL;
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return ;
	}

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
	g_print ("%s ()\n", __func__);
	GMainLoop *pBlockingLoop = g_object_get_data (G_OBJECT (pWindow), "loop");
	on_delete_normal_gui (pWindow, NULL, pBlockingLoop);
	if (pBlockingLoop == NULL)
		gtk_widget_destroy (pWindow);
}

void on_click_normal_ok (GtkButton *button, GtkWidget *pWindow)
{
	g_print ("%s ()\n", __func__);
	
	on_click_normal_apply (button, pWindow);
	on_click_normal_quit (button, pWindow);
}


gboolean on_delete_main_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	g_print ("%s (%x)\n", __func__, pBlockingLoop);
	if (pBlockingLoop != NULL)
	{
		g_print ("dialogue detruit, on sort de la boucle");
		if (g_main_loop_is_running (pBlockingLoop))
			g_main_loop_quit (pBlockingLoop);
	}
	cairo_dock_free_categories ();
	return FALSE;
}

gboolean on_delete_normal_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	g_print ("%s ()\n", __func__);
	
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

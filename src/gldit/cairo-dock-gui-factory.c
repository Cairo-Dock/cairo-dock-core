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

#include "../config.h"
#include "gldi-config.h"
#include "gldi-icon-names.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_sync
#include "cairo-dock-animations.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-icon-facility.h"  // gldi_icons_get_any_without_dialog
#include "cairo-dock-module-manager.h"  // GldiModule
#include "cairo-dock-module-instance-manager.h"  // GldiModuleInstance
#include "cairo-dock-applet-facility.h"  // play_sound
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-facility.h"  // cairo_dock_get_available_docks
#include "cairo-dock-packages.h"
#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-windows-manager.h"  // gldi_window_pick
#include "cairo-dock-task.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-launcher-manager.h" // cairo_dock_launch_command_sync
#include "cairo-dock-separator-manager.h" // GLDI_OBJECT_IS_SEPARATOR_ICON
#include "cairo-dock-gui-factory.h"

#define CAIRO_DOCK_ICON_MARGIN 6
#define CAIRO_DOCK_PREVIEW_WIDTH 350
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_README_WIDTH_MIN 400
#define CAIRO_DOCK_README_WIDTH 500
#define CAIRO_DOCK_TAB_ICON_SIZE 24  // 32
#define CAIRO_DOCK_FRAME_ICON_SIZE 24
#define DEFAULT_TEXT_COLOR .6  // light grey


extern gchar *g_cExtrasDirPath;
extern gboolean g_bUseOpenGL;
extern CairoDock *g_pMainDock;

static gchar *_cairo_dock_gui_get_active_row_in_combo (GtkWidget *pOneWidget);


typedef struct {
	GtkWidget *pControlContainer;  // widget contenant le widget de controle et les widgets controles.
	int iFirstSensitiveWidget;
	int iNbControlledWidgets;
	int iNbSensitiveWidgets;
	int iNonSensitiveWidget;
	} CDControlWidget;

#define _cairo_dock_gui_allocate_new_model(...)\
	gtk_list_store_new (CAIRO_DOCK_MODEL_NB_COLUMNS,\
		G_TYPE_STRING,   /* CAIRO_DOCK_MODEL_NAME*/\
		G_TYPE_STRING,   /* CAIRO_DOCK_MODEL_RESULT*/\
		G_TYPE_STRING,   /* CAIRO_DOCK_MODEL_DESCRIPTION_FILE*/\
		G_TYPE_STRING,   /* CAIRO_DOCK_MODEL_IMAGE*/\
		G_TYPE_BOOLEAN,  /* CAIRO_DOCK_MODEL_ACTIVE*/\
		G_TYPE_INT,      /* CAIRO_DOCK_MODEL_ORDER*/\
		G_TYPE_INT,      /* CAIRO_DOCK_MODEL_ORDER2*/\
		GDK_TYPE_PIXBUF, /* CAIRO_DOCK_MODEL_ICON*/\
		G_TYPE_INT,      /* CAIRO_DOCK_MODEL_STATE*/\
		G_TYPE_DOUBLE,   /* CAIRO_DOCK_MODEL_SIZE*/\
		G_TYPE_STRING)   /* CAIRO_DOCK_MODEL_AUTHOR*/

#if ! GTK_CHECK_VERSION(3, 10, 0)
GtkWidget* gtk_button_new_from_icon_name (const gchar *icon_name, GtkIconSize  size)
{
	GtkWidget *image = gtk_image_new_from_icon_name (icon_name, size);
	return (GtkWidget*) g_object_new (GTK_TYPE_BUTTON,
		"image", image,
		NULL);
}
#endif

static void _cairo_dock_activate_one_element (G_GNUC_UNUSED GtkCellRendererToggle * cell_renderer, gchar * path, GtkTreeModel * model)
{
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter_from_string (model, &iter, path))
		return ;
	gboolean bState;
	gtk_tree_model_get (model, &iter, CAIRO_DOCK_MODEL_ACTIVE, &bState, -1);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, CAIRO_DOCK_MODEL_ACTIVE, !bState, -1);
}

static gboolean _cairo_dock_increase_order (GtkTreeModel * model, G_GNUC_UNUSED GtkTreePath * path, GtkTreeIter * iter, int *pOrder)
{
	int iMyOrder;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_ORDER, &iMyOrder, -1);

	if (iMyOrder == *pOrder)
	{
		gtk_list_store_set (GTK_LIST_STORE (model), iter, CAIRO_DOCK_MODEL_ORDER, iMyOrder + 1, -1);
		return TRUE;
	}
	return FALSE;
}

static gboolean _cairo_dock_decrease_order (GtkTreeModel * model, G_GNUC_UNUSED GtkTreePath * path, GtkTreeIter * iter, int *pOrder)
{
	int iMyOrder;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_ORDER, &iMyOrder, -1);

	if (iMyOrder == *pOrder)
	{
		gtk_list_store_set (GTK_LIST_STORE (model), iter, CAIRO_DOCK_MODEL_ORDER, iMyOrder - 1, -1);
		return TRUE;
	}
	return FALSE;
}

static gboolean _cairo_dock_decrease_order_if_greater (GtkTreeModel * model, G_GNUC_UNUSED GtkTreePath * path, GtkTreeIter * iter, int *pOrder)
{
	int iMyOrder;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_ORDER, &iMyOrder, -1);

	if (iMyOrder > *pOrder)
	{
		gtk_list_store_set (GTK_LIST_STORE (model), iter, CAIRO_DOCK_MODEL_ORDER, iMyOrder - 1, -1);
		return TRUE;
	}
	return FALSE;
}

static void _cairo_dock_go_up (G_GNUC_UNUSED GtkButton *button, GtkTreeView *pTreeView)
{
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (pTreeView);

	GtkTreeModel *pModel;
	GtkTreeIter iter;
	if (! gtk_tree_selection_get_selected (pSelection, &pModel, &iter))
		return ;

	int iOrder;
	gtk_tree_model_get (pModel, &iter, CAIRO_DOCK_MODEL_ORDER, &iOrder, -1);
	iOrder --;
	if (iOrder < 0)
		return;

	gtk_tree_model_foreach (GTK_TREE_MODEL (pModel), (GtkTreeModelForeachFunc) _cairo_dock_increase_order, &iOrder);

	gtk_list_store_set (GTK_LIST_STORE (pModel), &iter, CAIRO_DOCK_MODEL_ORDER, iOrder, -1);
}

static void _cairo_dock_go_down (G_GNUC_UNUSED GtkButton *button, GtkTreeView *pTreeView)
{
	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (pTreeView);

	GtkTreeModel *pModel;
	GtkTreeIter iter;
	if (! gtk_tree_selection_get_selected (pSelection, &pModel, &iter))
		return ;

	int iOrder;
	gtk_tree_model_get (pModel, &iter, CAIRO_DOCK_MODEL_ORDER, &iOrder, -1);
	iOrder ++;
	if (iOrder > gtk_tree_model_iter_n_children (pModel, NULL) - 1)
		return;

	gtk_tree_model_foreach (GTK_TREE_MODEL (pModel), (GtkTreeModelForeachFunc) _cairo_dock_decrease_order, &iOrder);

	gtk_list_store_set (GTK_LIST_STORE (pModel), &iter, CAIRO_DOCK_MODEL_ORDER, iOrder, -1);
}

static void _cairo_dock_add (G_GNUC_UNUSED GtkButton *button, gpointer *data)
{
	GtkTreeView *pTreeView = data[0];
	GtkWidget *pEntry = data[1];

	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));

	GtkTreeModel *pModel = gtk_tree_view_get_model (pTreeView);
	gtk_list_store_append (GTK_LIST_STORE (pModel), &iter);

	gtk_list_store_set (GTK_LIST_STORE (pModel), &iter,
		CAIRO_DOCK_MODEL_ACTIVE, TRUE,
		CAIRO_DOCK_MODEL_NAME, gtk_entry_get_text (GTK_ENTRY (pEntry)),
		CAIRO_DOCK_MODEL_ORDER, gtk_tree_model_iter_n_children (pModel, NULL) - 1, -1);

	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (pTreeView);
	gtk_tree_selection_select_iter (pSelection, &iter);
}

static void _cairo_dock_remove (G_GNUC_UNUSED GtkButton *button, gpointer *data)
{
	GtkTreeView *pTreeView = data[0];
	GtkWidget *pEntry = data[1];

	GtkTreeSelection *pSelection = gtk_tree_view_get_selection (pTreeView);
	GtkTreeModel *pModel;

	GtkTreeIter iter;
	if (! gtk_tree_selection_get_selected (pSelection, &pModel, &iter))
		return ;

	gchar *cValue = NULL;
	int iOrder;
	gtk_tree_model_get (pModel, &iter,
		CAIRO_DOCK_MODEL_NAME, &cValue,
		CAIRO_DOCK_MODEL_ORDER, &iOrder, -1);

	gtk_list_store_remove (GTK_LIST_STORE (pModel), &iter);
	gtk_tree_model_foreach (GTK_TREE_MODEL (pModel), (GtkTreeModelForeachFunc) _cairo_dock_decrease_order_if_greater, &iOrder);

	gtk_entry_set_text (GTK_ENTRY (pEntry), cValue);
	g_free (cValue);
}

static gchar* _cairo_dock_gui_get_package_title (const gchar* cTitle, const gchar* cVersion)
{
	gchar *cTitleVersion;
	if (cTitle == NULL)
		cTitleVersion = NULL;
	else if (cVersion == NULL)
		cTitleVersion = g_strconcat ("<b>", cTitle, "</b>", NULL);
	else
		cTitleVersion = g_strconcat ("<b>", cTitle, "</b> - ", cVersion, NULL);
	return cTitleVersion;
}

static gchar* _cairo_dock_gui_get_package_author (const gchar* cAuthor)
{
	if (cAuthor == NULL)
		return NULL;
	gchar *cBy = g_strdup_printf (_("by %s"), cAuthor);
	gchar *cThemed = g_strdup_printf ("<small><tt>%s</tt></small>", cBy);
	g_free (cBy);
	return cThemed;
}

static gchar* _cairo_dock_gui_get_package_size (double fSize)
{
	if (fSize < 0.001)  // < 1ko
		return NULL;
	gchar *cThemed;
	if (fSize < .1)
		cThemed = g_strdup_printf ("<small>%.0f%s</small>", fSize*1e3, _("kB"));
	else
		cThemed = g_strdup_printf ("<small>%.1f%s</small>", fSize, _("MB"));
	return cThemed;
}

static const gchar* _cairo_dock_gui_get_package_state (gint iState)
{
	const gchar *cState = NULL;
	switch (iState)
	{
		case CAIRO_DOCK_LOCAL_PACKAGE:          cState = _("Local"); break;
		case CAIRO_DOCK_USER_PACKAGE:           cState = _("User"); break;
		case CAIRO_DOCK_DISTANT_PACKAGE:        cState = _("Net"); break;
		case CAIRO_DOCK_NEW_PACKAGE:            cState = _("New"); break;
		case CAIRO_DOCK_UPDATED_PACKAGE:        cState = _("Updated"); break;
		default:                                cState = NULL; break;
	}
	return cState;
}

static GdkPixbuf* _cairo_dock_gui_get_package_state_icon (gint iState)
{
	const gchar *cType;
	switch (iState)
	{
		case CAIRO_DOCK_LOCAL_PACKAGE: 		cType = "icons/theme-local.svg"; break;
		case CAIRO_DOCK_USER_PACKAGE: 		cType = "icons/theme-user.svg"; break;
		case CAIRO_DOCK_DISTANT_PACKAGE: 	cType = "icons/theme-distant.svg"; break;
		case CAIRO_DOCK_NEW_PACKAGE: 		cType = "icons/theme-new.svg"; break;
		case CAIRO_DOCK_UPDATED_PACKAGE:	cType = "icons/theme-updated.svg"; break;
		default: 							cType = NULL; break;
	}
	gchar *cStateIcon = g_strconcat (GLDI_SHARE_DATA_DIR"/", cType, NULL);
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cStateIcon, 24, 24, NULL);
	g_free (cStateIcon);
	return pixbuf;
}

static gboolean on_delete_async_widget (GtkWidget *pWidget, G_GNUC_UNUSED GdkEvent *event, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("%s ()", __func__);
	GldiTask *pTask = g_object_get_data (G_OBJECT (pWidget), "cd-task");
	if (pTask != NULL)
	{
		gldi_task_discard (pTask);
		g_object_set_data (G_OBJECT (pWidget), "cd-task", NULL);
	}
	return FALSE;  // propagate event
}

static inline void _set_preview_image (const gchar *cPreviewFilePath, GtkImage *pPreviewImage, GtkWidget *pPreviewImageFrame)
{
	int iPreviewWidth, iPreviewHeight;
	GtkRequisition requisition;
	gtk_widget_get_preferred_size (GTK_WIDGET (pPreviewImage), &requisition, NULL);
	requisition.width = CAIRO_DOCK_PREVIEW_WIDTH;
	requisition.height = CAIRO_DOCK_PREVIEW_HEIGHT;

	GdkPixbuf *pPreviewPixbuf = NULL;
	if (gdk_pixbuf_get_file_info (cPreviewFilePath, &iPreviewWidth, &iPreviewHeight) != NULL)
	{
		iPreviewWidth = MIN (iPreviewWidth, CAIRO_DOCK_PREVIEW_WIDTH);
		if (requisition.width > 1 && iPreviewWidth > requisition.width)
			iPreviewWidth = requisition.width;
		iPreviewHeight = MIN (iPreviewHeight, CAIRO_DOCK_PREVIEW_HEIGHT);
		if (requisition.height > 1 && iPreviewHeight > requisition.height)
			iPreviewHeight = requisition.height;
		cd_debug ("preview : %dx%d => %dx%d", requisition.width, requisition.height, iPreviewWidth, iPreviewHeight);
		pPreviewPixbuf = gdk_pixbuf_new_from_file_at_size (cPreviewFilePath, iPreviewWidth, iPreviewHeight, NULL);
	}
	if (pPreviewPixbuf == NULL)
	{
		pPreviewPixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			TRUE,
			8,
			1,
			1);
	}
	else if (pPreviewImageFrame) // We have an image, display border.
		gtk_frame_set_shadow_type (GTK_FRAME (pPreviewImageFrame), GTK_SHADOW_ETCHED_IN);

	gtk_image_set_from_pixbuf (pPreviewImage, pPreviewPixbuf);
	g_object_unref (pPreviewPixbuf);
}

static void _on_got_readme (const gchar *cDescription, GtkWidget *pDescriptionLabel)
{
	if (cDescription && strncmp (cDescription, "<!DOCTYPE", 9) == 0)  // message received when file is not found: <!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN"><html><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL /dustbin/Metal//readme was not found on this server.</p><hr><address>Apache/2.2.3 (Debian) mod_ssl/2.2.3 OpenSSL/0.9.8c Server at themes.glx-dock.org Port 80</address></body></html>
		gtk_label_set_markup (GTK_LABEL (pDescriptionLabel), "");
	else
		gtk_label_set_markup (GTK_LABEL (pDescriptionLabel), cDescription);
	
	GldiTask *pTask = g_object_get_data (G_OBJECT (pDescriptionLabel), "cd-task");
	if (pTask != NULL)
	{
		//g_print ("remove the task\n");
		gldi_task_discard (pTask);  // pas de gldi_task_free dans la callback de la tache.
		g_object_set_data (G_OBJECT (pDescriptionLabel), "cd-task", NULL);
	}
}

static void _on_got_preview_file (const gchar *cPreviewFilePath, gpointer *data)
{
	GtkImage *pPreviewImage = data[1];
	GtkWidget *pImageFrame = data[7];
	
	if (cPreviewFilePath != NULL)
	{
		_set_preview_image (cPreviewFilePath, GTK_IMAGE (pPreviewImage), pImageFrame);
		g_remove (cPreviewFilePath);
	}
	GldiTask *pTask = g_object_get_data (G_OBJECT (pPreviewImage), "cd-task");
	if (pTask != NULL)
	{
		gldi_task_discard (pTask);
		g_object_set_data (G_OBJECT (pPreviewImage), "cd-task", NULL);
	}
}

static void cairo_dock_label_set_label_show (GtkLabel *pLabel, const gchar *cLabel)
{
	if (cLabel == NULL)
		gtk_widget_hide (GTK_WIDGET (pLabel));
	else
	{
		gtk_label_set_label(GTK_LABEL (pLabel), cLabel);
		gtk_widget_show (GTK_WIDGET (pLabel));
	}
}

static void _cairo_dock_selection_changed (GtkTreeModel *model, GtkTreeIter iter, gpointer *data)
{
	static gchar *cPrevPath = NULL;
	gchar *cPath = NULL;
	GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
	if (path)
	{
		cPath = gtk_tree_path_to_string (path);
		gtk_tree_path_free (path);
	}
	if (cPrevPath && cPath && strcmp (cPrevPath, cPath) == 0)
	{
		g_free (cPath);
		return;
	}
	g_free (cPrevPath);
	cPrevPath = cPath;
	
	// get the widgets of the global preview widget.
	GtkLabel *pDescriptionLabel = data[0];
	GtkImage *pPreviewImage = data[1];
	GtkLabel* pTitle = data[2];
	GtkLabel* pAuthor = data[3];
	GtkLabel* pState = data[4];
	GtkImage* pStateIcon = data[5];
	GtkLabel* pSize = data[6];
	GtkWidget *pImageFrame = data[7];
	
	gtk_label_set_justify (GTK_LABEL (pDescriptionLabel), GTK_JUSTIFY_FILL);
	gtk_label_set_line_wrap (pDescriptionLabel, TRUE);
	
	// get the info of this theme.
	gchar *cDescriptionFilePath = NULL, *cPreviewFilePath = NULL, *cName = NULL, *cAuthor = NULL;
	gint iState = 0;
	double fSize = 0.;
	GdkPixbuf *pixbuf = NULL;
	gtk_tree_model_get (model, &iter,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, &cDescriptionFilePath,
		CAIRO_DOCK_MODEL_IMAGE, &cPreviewFilePath,
		CAIRO_DOCK_MODEL_NAME, &cName,
		CAIRO_DOCK_MODEL_AUTHOR, &cAuthor,
		CAIRO_DOCK_MODEL_ICON, &pixbuf,
		CAIRO_DOCK_MODEL_SIZE, &fSize,
		CAIRO_DOCK_MODEL_STATE, &iState, -1);
	cd_debug ("line selected (%s; %s; %f)", cDescriptionFilePath, cPreviewFilePath, fSize);
	
	// fill the info bar.
	if (pTitle)
	{
		gchar *cTitle = _cairo_dock_gui_get_package_title (cName, NULL);
		cairo_dock_label_set_label_show (GTK_LABEL (pTitle), cTitle);
		g_free (cTitle);
	}
	if (pAuthor)
	{
		gchar *cBy = _cairo_dock_gui_get_package_author (cAuthor);
		cairo_dock_label_set_label_show (GTK_LABEL (pAuthor), cBy);
		g_free (cBy);
	}
	if (pState)
	{
		const gchar *cState = _cairo_dock_gui_get_package_state (iState);
		cairo_dock_label_set_label_show (GTK_LABEL (pState), cState);
	}
	if (pSize)
	{
		gchar *cSize = _cairo_dock_gui_get_package_size (fSize);
		cairo_dock_label_set_label_show (GTK_LABEL (pSize), cSize);
		g_free (cSize);
	}
	if (pStateIcon)
		gtk_image_set_from_pixbuf (GTK_IMAGE (pStateIcon), pixbuf);
	
	// get or fill the readme.
	if (cDescriptionFilePath != NULL)
	{
		GldiTask *pTask = g_object_get_data (G_OBJECT (pDescriptionLabel), "cd-task");
		//g_print ("prev task : %x\n", pTask);
		if (pTask != NULL)
		{
			gldi_task_discard (pTask);
			g_object_set_data (G_OBJECT (pDescriptionLabel), "cd-task", NULL);
		}
		
		if (strncmp (cDescriptionFilePath, "http://", 7) == 0)  // fichier distant.
		{
			cd_debug ("fichier readme distant (%s)", cDescriptionFilePath);
			
			gtk_label_set_markup (pDescriptionLabel, "loading...");
			pTask = cairo_dock_get_url_data_async (cDescriptionFilePath, (GFunc) _on_got_readme, pDescriptionLabel);
			g_object_set_data (G_OBJECT (pDescriptionLabel), "cd-task", pTask);
			//g_print ("new task : %x\n", pTask);
		}
		else if (*cDescriptionFilePath == '/')  // fichier local
		{
			gsize length = 0;
			gchar *cDescription = NULL;
			g_file_get_contents (cDescriptionFilePath,
				&cDescription,
				&length,
				NULL);
			if (length > 0 && cDescription[length-1] == '\n') // if there is a new line at the end
				cDescription[length-1]='\0';
			gtk_label_set_markup (pDescriptionLabel, dgettext ("cairo-dock-plugins", cDescription)); // TODO: use dgettext with the right domain
			g_free (cDescription);
		}
		else if (strcmp (cDescriptionFilePath, "none") != 0)  // texte de la description.
		{
			gtk_label_set_markup (pDescriptionLabel, cDescriptionFilePath);
		}
		else  // rien.
			gtk_label_set_markup (pDescriptionLabel, NULL);
	}

	// Hide image frame until we display the image (which can fail).
	if (pImageFrame)
		gtk_frame_set_shadow_type (GTK_FRAME (pImageFrame), GTK_SHADOW_NONE);

	// get or fill the preview image.
	if (cPreviewFilePath != NULL)
	{
		GldiTask *pTask = g_object_get_data (G_OBJECT (pPreviewImage), "cd-task");
		if (pTask != NULL)
		{
			gldi_task_discard (pTask);
			g_object_set_data (G_OBJECT (pPreviewImage), "cd-task", NULL);
		}
		
		if (strncmp (cPreviewFilePath, "http://", 7) == 0)  // fichier distant.
		{
			cd_debug ("fichier preview distant (%s)", cPreviewFilePath);
			gtk_image_set_from_pixbuf (pPreviewImage, NULL);  // set blank image while downloading.
			
			pTask = cairo_dock_download_file_async (cPreviewFilePath, NULL, (GFunc) _on_got_preview_file, data);  // NULL <=> as a temporary file
			g_object_set_data (G_OBJECT (pPreviewImage), "cd-task", pTask);
		}
		else  // fichier local ou rien.
			_set_preview_image (cPreviewFilePath, pPreviewImage, pImageFrame);
	}

	g_free (cDescriptionFilePath);
	g_free (cPreviewFilePath);
	g_free (cName);
	g_free (cAuthor);
	if (pixbuf)
		g_object_unref (pixbuf);
}

static void _cairo_dock_select_custom_item_in_combo (GtkComboBox *widget, gpointer *data)
{
	GtkTreeModel *model = gtk_combo_box_get_model (widget);
	g_return_if_fail (model != NULL);

	GtkTreeIter iter;
	if (!gtk_combo_box_get_active_iter (widget, &iter))
		return ;
	
	GtkWidget *parent = data[1];
	GtkWidget *pKeyBox = data[0];
	int iNbWidgets = GPOINTER_TO_INT (data[2]);
	GList *children = gtk_container_get_children (GTK_CONTAINER (parent));
	GList *c = g_list_find (children, pKeyBox);
	g_return_if_fail (c != NULL && c->next != NULL);
	
	gchar *cName = NULL;
	gtk_tree_model_get (model, &iter, CAIRO_DOCK_MODEL_RESULT, &cName, -1);
	
	gboolean bActive = (cName != NULL && strcmp (cName, "personnal") == 0);
	GtkWidget *w;
	int i;
	for (c = c->next, i = 0; c != NULL && i < iNbWidgets; c = c->next, i ++)
	{
		w = c->data;
		gtk_widget_set_sensitive (w, bActive);
	}
	
	g_list_free (children);
	g_free (cName);
}

static void _cairo_dock_select_one_item_in_combo (GtkComboBox *widget, gpointer *data)
{
	GtkTreeModel *model = gtk_combo_box_get_model (widget);
	g_return_if_fail (model != NULL);
	
	GtkTreeIter iter;
	if (!gtk_combo_box_get_active_iter (widget, &iter))
		return ;
	
	_cairo_dock_selection_changed (model, iter, data);
}

static gboolean _cairo_dock_select_one_item_in_tree (G_GNUC_UNUSED GtkTreeSelection * selection, GtkTreeModel * model, GtkTreePath * path, gboolean path_currently_selected, gpointer *data)
{
	if (path_currently_selected)
		return TRUE;
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter (model, &iter, path))
		return FALSE;

	_cairo_dock_selection_changed (model, iter, data);
	return TRUE;
}

static void _cairo_dock_select_one_item_in_control_combo (GtkComboBox *widget, gpointer *data)
{
	GtkTreeModel *model = gtk_combo_box_get_model (widget);
	g_return_if_fail (model != NULL);
	
	//g_print ("%s ()\n", __func__);
	GtkTreeIter iter;
	if (!gtk_combo_box_get_active_iter (widget, &iter))
		return ;
	
	int iNumItem = gtk_combo_box_get_active (widget);
	//gtk_tree_model_get (model, &iter, CAIRO_DOCK_MODEL_ORDER, &iNumItem, -1);
	
	GtkWidget *parent = data[1];
	GtkWidget *pKeyBox = data[0];
	int iNbWidgets = GPOINTER_TO_INT (data[2]);
	GList *children = gtk_container_get_children (GTK_CONTAINER (parent));
	GList *c = g_list_find (children, pKeyBox);
	g_return_if_fail (c != NULL);
	
	//g_print ("%d widgets controles\n", iNbWidgets);
	GtkWidget *w;
	int i=0;
	for (c = c->next; c != NULL && i < iNbWidgets; c = c->next)
	{
		w = c->data;
		//g_print (" %d/%d -> %d\n", i, iNbWidgets, i == iNumItem);
		if (GTK_IS_ALIGNMENT (w))
			continue;
		if (GTK_IS_EXPANDER (w))
		{
			gtk_expander_set_expanded (GTK_EXPANDER (w), i == iNumItem);
		}
		else
		{
			gtk_widget_set_sensitive (w, i == iNumItem);
		}
		i ++;
	}
	
	g_list_free (children);
}

static GList *_activate_sub_widgets (GList *children, int iNbControlledWidgets, gboolean bSensitive)
{
	//g_print ("%s (%d, %d)\n", __func__, iNbControlledWidgets, bSensitive);
	GList *c = children;
	GtkWidget *w;
	int i = 0, iNbControlSubWidgets;
	while (c != NULL && i < iNbControlledWidgets)
	{
		w = c->data;
		//g_print ("%d in ]%d;%d[ ; %d\n", i, iOrder1, iOrder1 + iOrder2, GTK_IS_ALIGNMENT (w));
		if (GTK_IS_ALIGNMENT (w))  // les separateurs sont dans un alignement.
			continue;
		gtk_widget_set_sensitive (w, bSensitive);
		
		iNbControlSubWidgets = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "nb-ctrl-widgets"));
		if (iNbControlSubWidgets > 0)  // ce widget en controle d'autres.
		{
			c = _activate_sub_widgets (c, iNbControlSubWidgets, bSensitive);
		}
		else  // il est tout seul, on passe juste a la suite.
		{
			c = c->next;
		}
		i ++;
	}
	return c;
}
static void _cairo_dock_select_one_item_in_control_combo_selective (GtkComboBox *widget, gpointer *data)
{
	GtkTreeModel *model = gtk_combo_box_get_model (widget);
	g_return_if_fail (model != NULL);
	
	GtkTreeIter iter;
	if (!gtk_combo_box_get_active_iter (widget, &iter))
		return ;
	
	int iOrder1, iOrder2, iExcept;
	gtk_tree_model_get (model, &iter,
		CAIRO_DOCK_MODEL_ORDER, &iOrder1,
		CAIRO_DOCK_MODEL_ORDER2, &iOrder2,
		CAIRO_DOCK_MODEL_STATE, &iExcept, -1);
	
	GtkWidget *parent = data[1];
	GtkWidget *pKeyBox = data[0];
	int iNbWidgets = GPOINTER_TO_INT (data[2]);
	//g_print ("%s (%d, %d / %d)\n", __func__, iOrder1, iOrder2, iNbWidgets);
	GList *children = gtk_container_get_children (GTK_CONTAINER (parent));
	GList *c = g_list_find (children, pKeyBox);
	g_return_if_fail (c != NULL);
	
	//g_print ("%d widgets controles (%d au total)\n", iNbWidgets, g_list_length (children));
	GtkWidget *w;
	int i = 0, iNbControlSubWidgets;
	gboolean bSensitive;
	c = c->next;
	while (c != NULL && i < iNbWidgets)
	{
		w = c->data;
		//g_print (" %d in [%d;%d] ; %d\n", i, iOrder1-1, iOrder1 + iOrder2-1, GTK_IS_ALIGNMENT (w));
		if (GTK_IS_ALIGNMENT (w))  // les separateurs sont dans un alignement.
		{
			c = c->next;
			continue;
		}
		bSensitive = (i >= iOrder1 - 1 && i < iOrder1 + iOrder2 - 1 && i != iExcept - 1);
		gtk_widget_set_sensitive (w, bSensitive);
		
		iNbControlSubWidgets = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "nb-ctrl-widgets"));
		if (iNbControlSubWidgets > 0)
		{
			//g_print ("  ce widget en controle %d autres\n", iNbControlSubWidgets);
			c = _activate_sub_widgets (c->next, iNbControlSubWidgets, bSensitive);
			if (bSensitive)
			{
				gboolean bReturn;
				GtkWidget *sw = g_object_get_data (G_OBJECT (w), "one-widget");
				if (GTK_IS_CHECK_BUTTON (sw))
					g_signal_emit_by_name (sw, "toggled", NULL, &bReturn);
				else if (GTK_IS_COMBO_BOX (sw))
					g_signal_emit_by_name (sw, "changed", NULL, &bReturn);
			}
		}
		else
		{
			c = c->next;
		}
		i ++;
	}
	
	g_list_free (children);
}

static void _cairo_dock_show_image_preview (GtkFileChooser *pFileChooser, GtkImage *pPreviewImage)
{
	gchar *cFileName = gtk_file_chooser_get_preview_filename (pFileChooser);
	if (cFileName == NULL)
		return ;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cFileName, 64, 64, NULL);
	g_free (cFileName);
	if (pixbuf != NULL)
	{
		gtk_image_set_from_pixbuf (pPreviewImage, pixbuf);
		g_object_unref (pixbuf);
		gtk_file_chooser_set_preview_widget_active (pFileChooser, TRUE);
	}
	else
		gtk_file_chooser_set_preview_widget_active (pFileChooser, FALSE);
}
static void _cairo_dock_pick_a_file (G_GNUC_UNUSED GtkButton *button, gpointer *data)
{
	GtkEntry *pEntry = data[0];
	gint iFileType = GPOINTER_TO_INT (data[1]); // 0: file ; 1: dirs ; 2: images
	GtkWindow *pParentWindow = data[2];

	GtkWidget* pFileChooserDialog = gtk_file_chooser_dialog_new (
		(iFileType == 0 ? _("Pick up a file") : iFileType == 1 ? _("Pick up a directory") : _("Pick up an image")),
		pParentWindow,
		(iFileType == 1 ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN),
		_("Ok"),
		GTK_RESPONSE_OK,
		_("Cancel"),
		GTK_RESPONSE_CANCEL,
		NULL);

	// set the current folder to the current value in conf.
	const gchar *cFilePath = gtk_entry_get_text (pEntry);
	if (cFilePath == NULL || *cFilePath != '/')
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (pFileChooserDialog),
			iFileType == 2 ?
				g_get_user_special_dir (G_USER_DIRECTORY_PICTURES) :
				g_getenv ("HOME"));
	else
	{
		gchar *cDirectoryPath = g_path_get_dirname (cFilePath);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (pFileChooserDialog),
			cDirectoryPath);
		g_free (cDirectoryPath);
	}
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (pFileChooserDialog), FALSE);
	if (iFileType == 2) // image: add shortcuts to icons of the system
	{
		gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (pFileChooserDialog),
			"/usr/share/icons", NULL);
		gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (pFileChooserDialog),
			"/usr/share/pixmaps", NULL);
	}

	// a filter
	GtkFileFilter *pFilter;
	if (iFileType == 0)
	{
		pFilter = gtk_file_filter_new ();
		gtk_file_filter_set_name (pFilter, _("All"));
		gtk_file_filter_add_pattern (pFilter, "*");
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (pFileChooserDialog), pFilter);
	}
	if (iFileType != 1) // preview and images filter: not when selecting a directory
	{
		pFilter = gtk_file_filter_new ();
		gtk_file_filter_set_name (pFilter, _("Image"));
		gtk_file_filter_add_pixbuf_formats (pFilter);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (pFileChooserDialog), pFilter);

		// a preview
		GtkWidget *pPreviewImage = gtk_image_new ();
		gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (pFileChooserDialog), pPreviewImage);
		g_signal_connect (GTK_FILE_CHOOSER (pFileChooserDialog), "update-preview",
			G_CALLBACK (_cairo_dock_show_image_preview), pPreviewImage);
	}

	gtk_widget_show (pFileChooserDialog);
	int answer = gtk_dialog_run (GTK_DIALOG (pFileChooserDialog));
	if (answer == GTK_RESPONSE_OK)
	{
		gchar *cFilePath = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pFileChooserDialog));
		gtk_entry_set_text (pEntry, cFilePath);
		g_free (cFilePath);
	}
	gtk_widget_destroy (pFileChooserDialog);
}

//Sound Callback
static void _cairo_dock_play_a_sound (G_GNUC_UNUSED GtkButton *button, gpointer *data)
{
	GtkWidget *pEntry = data[0];
	const gchar *cSoundPath = gtk_entry_get_text (GTK_ENTRY (pEntry));
	cairo_dock_play_sound (cSoundPath);
}

static void _cairo_dock_set_original_value (G_GNUC_UNUSED GtkButton *button, CairoDockGroupKeyWidget *pGroupKeyWidget)
{
	gchar *cGroupName = pGroupKeyWidget->cGroupName;
	gchar *cKeyName = pGroupKeyWidget->cKeyName;
	GSList *pSubWidgetList = pGroupKeyWidget->pSubWidgetList;
	gchar *cOriginalConfFilePath = pGroupKeyWidget->cOriginalConfFilePath;
	cd_debug ("%s (%s, %s, %s)", __func__, cGroupName, cKeyName, cOriginalConfFilePath);
	
	GSList *pList;
	GtkWidget *pOneWidget = pSubWidgetList->data;
	GError *erreur = NULL;
	gsize length = 0;
	
	GKeyFile *pKeyFile = g_key_file_new ();
	g_key_file_load_from_file (pKeyFile, cOriginalConfFilePath, 0, &erreur);  // inutile de garder les commentaires ce coup-ci.
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
		return ;
	}
	
	if (GTK_IS_SPIN_BUTTON (pOneWidget) || GTK_IS_SCALE (pOneWidget))
	{
		gsize i = 0;
		gboolean bIsSpin = GTK_IS_SPIN_BUTTON (pOneWidget);
		double *fValuesList = g_key_file_get_double_list (pKeyFile, cGroupName, cKeyName, &length, &erreur);
		
		for (pList = pSubWidgetList; pList != NULL && i < length; pList = pList->next, i++)
		{
			pOneWidget = pList->data;
			if (bIsSpin)
				gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), fValuesList[i]);
			else
				gtk_range_set_value (GTK_RANGE (pOneWidget), fValuesList[i]);
		}
		
		g_free (fValuesList);
	}
	else if (GTK_IS_COLOR_BUTTON (pOneWidget))
	{
		double *fValuesList = g_key_file_get_double_list (pKeyFile, cGroupName, cKeyName, &length, &erreur);

		if (length > 2)
		{
			GdkRGBA color;
			color.red = fValuesList[0];
			color.green = fValuesList[1];
			color.blue = fValuesList[2];
			if (length > 3)
				color.alpha = fValuesList[3];
			else
				color.alpha = 1.;
			gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (pOneWidget), &color);
		}
		g_free (fValuesList);
	}
	g_key_file_free (pKeyFile);
}

static void _cairo_dock_key_grab_cb (GtkWidget *wizard_window, GdkEventKey *event, GtkEntry *pEntry)
{
	gchar *key;
	cd_debug ("key pressed");
	if (gtk_accelerator_valid (event->keyval, event->state))
	{
		/* This lets us ignore all ignorable modifier keys, including
		* NumLock and many others. :)
		*
		* The logic is: keep only the important modifiers that were pressed
		* for this event. */
		event->state &= gtk_accelerator_get_default_mod_mask();

		/* Generate the correct name for this key */
		key = gtk_accelerator_name (event->keyval, event->state);

		cd_debug ("KEY GRABBED: '%s'", key);

		/* Re-enable widgets */
		gtk_widget_set_sensitive (GTK_WIDGET(pEntry), TRUE);

		/* Disconnect the key grabber */
		g_signal_handlers_disconnect_by_func (G_OBJECT(wizard_window), G_CALLBACK(_cairo_dock_key_grab_cb), pEntry);

		/* Copy the pressed key to the text entry */
		gtk_entry_set_text (GTK_ENTRY(pEntry), key);

		/* Free the string */
		g_free (key);
	}
}

static void _cairo_dock_key_grab_clicked (G_GNUC_UNUSED GtkButton *button, gpointer *data)
{
	GtkEntry *pEntry = data[0];
	GtkWindow *pParentWindow = data[1];

	//set widget insensitive
	gtk_widget_set_sensitive (GTK_WIDGET(pEntry), FALSE);

	g_signal_connect (G_OBJECT(pParentWindow), "key-press-event", G_CALLBACK(_cairo_dock_key_grab_cb), pEntry);
}

static void _cairo_dock_key_grab_class (G_GNUC_UNUSED GtkButton *button, gpointer *data)
{
	GtkEntry *pEntry = data[0];
	GtkWindow *pParentWindow = data[1];

	cd_debug ("clicked");
	gtk_widget_set_sensitive (GTK_WIDGET(pEntry), FALSE);  // lock the widget during the grab (it makes it more comprehensive).
	
	const gchar *cResult = NULL;
	GldiWindowActor *actor = gldi_window_pick (pParentWindow);
	
	if (actor && actor->bIsTransientFor)
		actor = gldi_window_get_transient_for (actor);
	
	if (actor)
		cResult = actor->cClass;
	else
		cd_warning ("couldn't get a window actor");
	
	gtk_widget_set_sensitive (GTK_WIDGET(pEntry), TRUE);  // unlock the widget
	gtk_entry_set_text (pEntry, cResult);  // write the result in the entry-box
}

void _cairo_dock_set_value_in_pair (GtkSpinButton *pSpinButton, gpointer *data)
{
	GtkWidget *pPairSpinButton = data[0];
	GtkWidget *pToggleButton = data[1];
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pToggleButton)))
	{
		int iValue = gtk_spin_button_get_value (pSpinButton);
		int iPairValue = gtk_spin_button_get_value (GTK_SPIN_BUTTON (pPairSpinButton));
		if (iValue != iPairValue)
		{
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (pPairSpinButton), iValue);
		}
	}
}

static void _cairo_dock_toggle_control_button (GtkCheckButton *pButton, gpointer *data)
{
	GtkWidget *parent = data[1];
	GtkWidget *pKeyBox = data[0];
	int iNbWidgets = GPOINTER_TO_INT (data[2]);
	
	GList *children = gtk_container_get_children (GTK_CONTAINER (parent));
	GList *c = g_list_find (children, pKeyBox);
	g_return_if_fail (c != NULL);
	
	gboolean bActive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pButton));
	if (iNbWidgets < 0)
	{
		bActive = !bActive;
		iNbWidgets = - iNbWidgets;
	}
	GtkWidget *w;
	int i;
	for (c = c->next, i = 0; c != NULL && i < iNbWidgets; c = c->next, i ++)
	{
		w = c->data;
		cd_debug (" %d/%d -> %d", i, iNbWidgets, bActive);
		gtk_widget_set_sensitive (w, bActive);
	}
	
	g_list_free (children);
}

static void _list_icon_theme_in_dir (const gchar *cDirPath, GHashTable *pHashTable)
{
	GError *erreur = NULL;
	GDir *dir = g_dir_open (cDirPath, 0, &erreur);
	if (erreur != NULL)
	{
		cd_message ("%s\n", erreur->message);  // ~/.icons might not exist, don't make a fuss
		g_error_free (erreur);
		return ;
	}
	
	const gchar *cFileName;
	GString *sIndexFile = g_string_new ("");
	while ((cFileName = g_dir_read_name (dir)) != NULL)
	{
		g_string_printf (sIndexFile, "%s/%s/index.theme", cDirPath, cFileName);
		if (! g_file_test (sIndexFile->str, G_FILE_TEST_EXISTS))
			continue;
			
		GKeyFile *pKeyFile = cairo_dock_open_key_file (sIndexFile->str);
		if (pKeyFile == NULL)
			continue;
		
		if (! g_key_file_get_boolean (pKeyFile, "Icon Theme", "Hidden", NULL) && g_key_file_has_key (pKeyFile, "Icon Theme", "Directories", NULL))
		{
			gchar *cName = g_key_file_get_string (pKeyFile, "Icon Theme", "Name", NULL);
			if (cName != NULL)
			{
				g_hash_table_insert (pHashTable, cName, g_strdup (cFileName));
			}
		}
		
		g_key_file_free (pKeyFile);
	}
	g_string_free (sIndexFile, TRUE);
	g_dir_close (dir);
}

static GHashTable *_cairo_dock_build_icon_themes_list (const gchar **cDirs)
{
	GHashTable *pHashTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		g_free);
	gchar *cName = g_strdup (N_("_Custom Icons_"));
	g_hash_table_insert (pHashTable, g_strdup (gettext (cName)), cName);
	
	int i;
	for (i = 0; cDirs[i] != NULL; i ++)
	{
		_list_icon_theme_in_dir (cDirs[i], pHashTable);
	}
	return pHashTable;
}

static GHashTable *_cairo_dock_build_screens_list (void)
{
	GHashTable *pHashTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		g_free);
	g_hash_table_insert (pHashTable, g_strdup (_("Use all screens")), g_strdup ("-1"));
	
	if (g_desktopGeometry.iNbScreens > 1)
	{
		int xmax=0, ymax=0;
		int i;
		for (i = 0; i < g_desktopGeometry.iNbScreens; i ++)
		{
			int x = cairo_dock_get_screen_position_x (i), y = cairo_dock_get_screen_position_y (i);
			if (x > xmax) xmax = x;
			if (y > ymax) ymax = y;
		}
		const gchar *xpos, *ypos;
		int x, y;
		for (i = 0; i < g_desktopGeometry.iNbScreens; i ++)
		{
			xpos = ypos = NULL;
			x = cairo_dock_get_screen_position_x (i), y = cairo_dock_get_screen_position_y (i);
			
			if (xmax > 0)  // at least 2 screens horizontally
			{
				if (x == 0)
					xpos = _("left");
				else if (x == xmax)
					xpos = _("right");
				else
					xpos = _("middle");
				
			}
			if (ymax > 0)  // at least 2 screens vertically
			{
				if (y == 0)
					ypos = _("top");
				else if (y == ymax)
					ypos = _("bottom");
				else
					ypos = _("middle");
				
			}
			gchar *cLabel = g_strdup_printf ("%s %d (%s%s%s)", _("Screen"), i, 	xpos?xpos:"", xpos&&ypos?" - ":"", ypos?ypos:"");
			g_hash_table_insert (pHashTable, cLabel, g_strdup_printf ("%d", i));
		}
	}
	else  // if we have only 1 screen, and the screen-number is set to 0 (default value), let's insert a row for it; it's just to not have a blank widget with no line of the combo being selected, since anyway the widget will be unsensitive.
	{
		g_hash_table_insert (pHashTable, g_strdup (_("Use all screens")), g_strdup ("0"));
	}
	return pHashTable;
}

typedef void (*CDForeachRendererFunc) (GHFunc pFunction, GtkListStore *pListStore);

static inline GtkListStore *_build_list_for_gui (CDForeachRendererFunc pFunction, GHFunc pHFunction, const gchar *cEmptyItem)
{
	GtkListStore *pListStore = _cairo_dock_gui_allocate_new_model ();
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (pListStore), CAIRO_DOCK_MODEL_NAME, GTK_SORT_ASCENDING);
	if (cEmptyItem)
		pHFunction ((gchar*)cEmptyItem, NULL, pListStore);
	if (pFunction)
		pFunction (pHFunction, pListStore);
	return pListStore;
}

static void _cairo_dock_add_one_renderer_item (const gchar *cName, CairoDockRenderer *pRenderer, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, (pRenderer && pRenderer->cDisplayedName ? pRenderer->cDisplayedName : cName),
		CAIRO_DOCK_MODEL_RESULT, cName,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, (pRenderer != NULL ? pRenderer->cReadmeFilePath : "none"),
		CAIRO_DOCK_MODEL_IMAGE, (pRenderer != NULL ? pRenderer->cPreviewFilePath : "none"), -1);
}
static GtkListStore *_cairo_dock_build_renderer_list_for_gui (void)
{
	return _build_list_for_gui ((CDForeachRendererFunc)cairo_dock_foreach_dock_renderer, (GHFunc)_cairo_dock_add_one_renderer_item, "");
}

static void _cairo_dock_add_one_decoration_item (const gchar *cName, CairoDeskletDecoration *pDecoration, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, (pDecoration && pDecoration->cDisplayedName && *pDecoration->cDisplayedName != '\0' ? pDecoration->cDisplayedName : cName),
		CAIRO_DOCK_MODEL_RESULT, cName,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none"/*(pRenderer != NULL ? pRenderer->cReadmeFilePath : "none")*/,
		CAIRO_DOCK_MODEL_IMAGE, "none"/*(pRenderer != NULL ? pRenderer->cPreviewFilePath : "none")*/, -1);
}
static GtkListStore *_cairo_dock_build_desklet_decorations_list_for_gui (void)
{
	return _build_list_for_gui ((CDForeachRendererFunc)cairo_dock_foreach_desklet_decoration, (GHFunc)_cairo_dock_add_one_decoration_item, NULL);
}
static GtkListStore *_cairo_dock_build_desklet_decorations_list_for_applet_gui (void)
{
	return _build_list_for_gui ((CDForeachRendererFunc)cairo_dock_foreach_desklet_decoration, (GHFunc)_cairo_dock_add_one_decoration_item, "default");
}

static void _cairo_dock_add_one_animation_item (const gchar *cName, CairoDockAnimationRecord *pRecord, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, (pRecord && pRecord->cDisplayedName != NULL && *pRecord->cDisplayedName != '\0' ? pRecord->cDisplayedName : cName),
		CAIRO_DOCK_MODEL_RESULT, cName,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
		CAIRO_DOCK_MODEL_IMAGE, "none", -1);
}
static GtkListStore *_cairo_dock_build_animations_list_for_gui (void)
{
	return _build_list_for_gui ((CDForeachRendererFunc)cairo_dock_foreach_animation, (GHFunc)_cairo_dock_add_one_animation_item, "");
}

static void _cairo_dock_add_one_dialog_decorator_item (const gchar *cName, CairoDialogDecorator *pDecorator, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, (pDecorator && pDecorator->cDisplayedName != NULL && *pDecorator->cDisplayedName != '\0' ? pDecorator->cDisplayedName : cName),
		CAIRO_DOCK_MODEL_RESULT, cName,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
		CAIRO_DOCK_MODEL_IMAGE, "none", -1);
}
static GtkListStore *_cairo_dock_build_dialog_decorator_list_for_gui (void)
{
	return _build_list_for_gui ((CDForeachRendererFunc)cairo_dock_foreach_dialog_decorator, (GHFunc)_cairo_dock_add_one_dialog_decorator_item, NULL);
}

static GtkListStore *_cairo_dock_build_dock_list_for_gui (GList *pDocks)
{
	GtkListStore *pListStore = _cairo_dock_gui_allocate_new_model ();
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (pListStore), CAIRO_DOCK_MODEL_NAME, GTK_SORT_ASCENDING);
	
	const gchar *cName;
	gchar *cUserName;
	GtkTreeIter iter;
	CairoDock *pDock;
	GList *d;
	for (d = pDocks; d != NULL; d = d->next)
	{
		pDock = d->data;
		cName = gldi_dock_get_name (pDock);
		cUserName = gldi_dock_get_readable_name (pDock);
		
		memset (&iter, 0, sizeof (GtkTreeIter));
		gtk_list_store_append (pListStore, &iter);
		gtk_list_store_set (pListStore, &iter,
			CAIRO_DOCK_MODEL_NAME, cUserName?cUserName:cName,
			CAIRO_DOCK_MODEL_RESULT, cName,
			CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
			CAIRO_DOCK_MODEL_IMAGE, "none", -1);
		g_free (cUserName);
	}
	
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pListStore), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pListStore), &iter,
		CAIRO_DOCK_MODEL_NAME, _("New main dock"),
		CAIRO_DOCK_MODEL_RESULT, "_New Dock_",  // this name does likely not exist, which will lead to the creation of a new dock.
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
		CAIRO_DOCK_MODEL_IMAGE, "none", -1);
	return pListStore;
}

static void _cairo_dock_add_one_icon_theme_item (const gchar *cDisplayedName, const gchar *cFolderName, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	//g_print ("+ %s (%s)\n", cName, cDisplayedName);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, cDisplayedName,
		CAIRO_DOCK_MODEL_RESULT, cFolderName,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
		CAIRO_DOCK_MODEL_IMAGE, "none", -1);
}
static GtkListStore *_cairo_dock_build_icon_theme_list_for_gui (GHashTable *pHashTable)
{
	GtkListStore *pIconThemeListStore = _build_list_for_gui (NULL, (GHFunc)_cairo_dock_add_one_icon_theme_item, "");
	g_hash_table_foreach (pHashTable, (GHFunc)_cairo_dock_add_one_icon_theme_item, pIconThemeListStore);
	return pIconThemeListStore;
}

static void _cairo_dock_add_one_screen_item (const gchar *cDisplayedName, const gchar *cId, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	//g_print ("+ %s (%s)\n", cName, cDisplayedName);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, cDisplayedName,
		CAIRO_DOCK_MODEL_RESULT, cId,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
		CAIRO_DOCK_MODEL_IMAGE, "none", -1);
}
static GtkListStore *_cairo_dock_build_screens_list_for_gui (GHashTable *pHashTable)
{
	GtkListStore *pListStore = _build_list_for_gui (NULL, NULL, NULL);
	g_hash_table_foreach (pHashTable, (GHFunc)_cairo_dock_add_one_screen_item, pListStore);
	return pListStore;
}

static gboolean _on_screen_modified (GtkWidget *pCombo)
{
	GtkListStore *pListStore = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (pCombo)));
	GHashTable *pHashTable = _cairo_dock_build_screens_list ();
	
	gtk_list_store_clear (GTK_LIST_STORE (pListStore));
	g_hash_table_foreach (pHashTable, (GHFunc)_cairo_dock_add_one_screen_item, pListStore);
	
	gtk_widget_set_sensitive (pCombo, g_desktopGeometry.iNbScreens > 1);
	
	g_hash_table_destroy (pHashTable);
	return GLDI_NOTIFICATION_LET_PASS;
}
static void _on_list_destroyed (G_GNUC_UNUSED gpointer data)
{
	gldi_object_remove_notification (&myDesktopMgr,
		NOTIFICATION_DESKTOP_GEOMETRY_CHANGED,
		(GldiNotificationFunc) _on_screen_modified,
		GLDI_RUN_AFTER);
}

static gboolean _test_one_name (GtkTreeModel *model, G_GNUC_UNUSED GtkTreePath *path, GtkTreeIter *iter, gpointer *data)
{
	gchar *cName = NULL, *cResult = NULL;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_RESULT, &cResult, -1);
	if (cResult == NULL)
		gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_NAME, &cName, -1);
	else if (data[3])
		cairo_dock_extract_package_type_from_name (cResult);
	if ((cResult && strcmp (data[0], cResult) == 0) || (cName && strcmp (data[0], cName) == 0))
	{
		GtkTreeIter *iter_to_fill = data[1];
		memcpy (iter_to_fill, iter, sizeof (GtkTreeIter));
		gboolean *bFound = data[2];
		*bFound = TRUE;
		g_free (cName);
		g_free (cResult);
		return TRUE;
	}
	g_free (cName);
	g_free (cResult);
	return FALSE;
}
static gboolean _cairo_dock_find_iter_from_name_full (GtkListStore *pModele, const gchar *cName, GtkTreeIter *iter, gboolean bIsTheme)
{
	if (cName == NULL)
		return FALSE;
	gboolean bFound = FALSE;
	gconstpointer data[4] = {cName, iter, &bFound, GINT_TO_POINTER (bIsTheme)};
	gtk_tree_model_foreach (GTK_TREE_MODEL (pModele), (GtkTreeModelForeachFunc) _test_one_name, data);
	return bFound;
}
 
#define _cairo_dock_find_iter_from_name(pModele, cName, iter) _cairo_dock_find_iter_from_name_full (pModele, cName, iter, FALSE)

static void cairo_dock_fill_combo_with_themes (GtkWidget *pCombo, GHashTable *pThemeTable, gchar *cActiveTheme, gchar *cHint)
{
	cd_debug ("%s (%s, %s)", __func__, cActiveTheme, cHint);
	// fill the combo's model
	GtkTreeModel *modele = gtk_combo_box_get_model (GTK_COMBO_BOX (pCombo));
	g_return_if_fail (modele != NULL);
	cairo_dock_fill_model_with_themes (GTK_LIST_STORE (modele), pThemeTable, cHint);
	
	// select the given one
	GtkTreeIter iter;
	cairo_dock_extract_package_type_from_name (cActiveTheme);
	if (_cairo_dock_find_iter_from_name_full (GTK_LIST_STORE (modele), cActiveTheme, &iter, TRUE))
	{
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (pCombo), &iter);
		///gboolean bReturn;
		///g_signal_emit_by_name (pCombo, "changed", NULL, &bReturn);
		//cd_debug ("%s found ", cActiveTheme);
	}
}

static void _got_themes_combo_list (GHashTable *pThemeTable, gpointer *data)
{
	if (pThemeTable == NULL)
	{
		cairo_dock_set_status_message (data[1], "Couldn't list available themes (is connection alive ?)");
		return ;
	}
	else
		cairo_dock_set_status_message (data[1], "");
	
	GtkWidget *pCombo = data[0];
	gchar *cValue = data[2];
	gchar *cHint = data[3];
	GldiTask *pTask = g_object_get_data (G_OBJECT (pCombo), "cd-task");
	
	if (pTask != NULL)
	{
		//g_print ("remove the task\n");
		gldi_task_discard (pTask);  // pas de gldi_task_free dans la callback de la tache.
		g_object_set_data (G_OBJECT (pCombo), "cd-task", NULL);
	}
	
	GtkTreeModel *pModel = gtk_combo_box_get_model (GTK_COMBO_BOX (pCombo));
	g_return_if_fail (pModel != NULL);
	GtkTreeIter iter;
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (pCombo), &iter))
	{
		g_free (cValue);
		cValue = NULL;
		gtk_tree_model_get (pModel, &iter, CAIRO_DOCK_MODEL_RESULT, &cValue, -1);
	}
	
	gtk_list_store_clear (GTK_LIST_STORE (pModel));
	cairo_dock_fill_combo_with_themes (pCombo, pThemeTable, cValue, cHint);
	
	g_free (cValue);
	data[2] = NULL;
	g_free (cHint);
	data[3] = NULL;
}


static void _cairo_dock_configure_module (G_GNUC_UNUSED GtkButton *button, const gchar *cModuleName)
{
	GldiModule *pModule = gldi_module_get (cModuleName);
	Icon *pIcon = cairo_dock_get_current_active_icon ();
	if (pIcon == NULL)
		pIcon = gldi_icons_get_any_without_dialog ();
	CairoDock *pDock = gldi_dock_get (pIcon != NULL ? pIcon->cParentDockName : NULL);
	gchar *cMessage = NULL;
	
	if (pModule == NULL)
	{
		cMessage = g_strdup_printf (_("The '%s' module was not found.\nBe sure to install it with the same version as the dock to enjoy these features."), cModuleName);
		int iDuration = 10e3;
		if (pIcon != NULL && pDock != NULL)
			gldi_dialog_show_temporary_with_icon (cMessage, pIcon, CAIRO_CONTAINER (pDock), iDuration, "same icon");
		else
			gldi_dialog_show_general_message (cMessage, iDuration);
	}
	else if (pModule != NULL && pModule->pInstancesList == NULL)
	{
		cMessage = g_strdup_printf (_("The '%s' plug-in is not active.\nActivate it now?"), cModuleName);
		int iClickedButton = gldi_dialog_show_and_wait (cMessage,
			pIcon, CAIRO_CONTAINER (pDock),
			GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, NULL);
		if (iClickedButton == 0 || iClickedButton == -1)  // ok button or Enter.
		{
			gldi_module_activate (pModule);
		}
	}
	g_free (cMessage);
}

static void _cairo_dock_widget_launch_command (G_GNUC_UNUSED GtkButton *button, const gchar *cCommandToLaunch)
{
	gchar *cResult = cairo_dock_launch_command_sync (cCommandToLaunch);
	if (cResult != NULL)
		cd_debug ("%s: %s => %s", __func__, cCommandToLaunch, cResult);
	g_free (cResult);
}

static void _on_text_changed (GtkWidget *pEntry, gchar *cDefaultValue);
static void _set_default_text (GtkWidget *pEntry, gchar *cDefaultValue)
{
	g_signal_handlers_block_by_func (G_OBJECT(pEntry), G_CALLBACK(_on_text_changed), cDefaultValue);
	gtk_entry_set_text (GTK_ENTRY (pEntry), cDefaultValue);
	g_signal_handlers_unblock_by_func (G_OBJECT(pEntry), G_CALLBACK(_on_text_changed), cDefaultValue);

	g_object_set_data (G_OBJECT (pEntry), "ignore-value", GINT_TO_POINTER (TRUE));

	GdkRGBA color;
	color.red = DEFAULT_TEXT_COLOR;
	color.green = DEFAULT_TEXT_COLOR;
	color.blue = DEFAULT_TEXT_COLOR;
	color.alpha = 1.;
	gtk_widget_override_color (pEntry, GTK_STATE_FLAG_NORMAL, &color);
}
static void _on_text_changed (GtkWidget *pEntry, G_GNUC_UNUSED gchar *cDefaultValue)
{
	// if the text has changed, it means the user has modified it (because we block this callback when we set the default value) -> mark the value as 'valid' and reset the color to the normal style.
	g_object_set_data (G_OBJECT (pEntry), "ignore-value", GINT_TO_POINTER (FALSE));

	gtk_widget_override_color (pEntry, GTK_STATE_FLAG_NORMAL, NULL);
}
static gboolean on_text_focus_in (GtkWidget *pEntry, G_GNUC_UNUSED GdkEventFocus *event, gchar *cDefaultValue)  // user takes the focus
{
	if (g_object_get_data (G_OBJECT (pEntry), "ignore-value") != NULL)  // the current value is the default text => erase it
	{
		g_signal_handlers_block_by_func (G_OBJECT(pEntry), G_CALLBACK(_on_text_changed), cDefaultValue);
		gtk_entry_set_text (GTK_ENTRY (pEntry), "");
		g_signal_handlers_unblock_by_func (G_OBJECT(pEntry), G_CALLBACK(_on_text_changed), cDefaultValue);
	}
	return FALSE;
}
static gboolean on_text_focus_out (GtkWidget *pEntry, G_GNUC_UNUSED GdkEventFocus *event, gchar *cDefaultValue)  // user leaves the entry
{
	const gchar *cText = gtk_entry_get_text (GTK_ENTRY (pEntry));
	if (! cText || *cText == '\0')
	{
		_set_default_text (pEntry, cDefaultValue);
	}
	return FALSE;
}

#define _allocate_new_buffer\
	data = g_new0 (gconstpointer, 8); \
	if (pDataGarbage) g_ptr_array_add (pDataGarbage, data);


GtkWidget *cairo_dock_widget_image_frame_new (GtkWidget *pWidget)
{
	// ImageFrame : Display the visible border around the image.
	GtkWidget *pImageFrame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (pImageFrame), GTK_SHADOW_ETCHED_IN);
	
	// ImagePadding : Get some space between the visible border and the image.
	GtkWidget *pImagePadding = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (pImagePadding), GTK_SHADOW_NONE);
	gtk_container_set_border_width (GTK_CONTAINER (pImagePadding), CAIRO_DOCK_GUI_MARGIN);
	gtk_container_add (GTK_CONTAINER (pImageFrame), pImagePadding);
	
	// Return with content widget inside.
	gtk_container_add (GTK_CONTAINER (pImagePadding), pWidget);
	return pImageFrame;
}

GtkWidget *cairo_dock_gui_make_preview_box (GtkWidget *pMainWindow, GtkWidget *pOneWidget, gboolean bHorizontalPackaging, int iAddInfoBar, const gchar *cInitialDescription, const gchar *cInitialImage, GPtrArray *pDataGarbage)
{
	gconstpointer *data;
	_allocate_new_buffer;
	
	// readme label.
	GtkWidget *pDescriptionLabel = gtk_label_new (cInitialDescription);
	gtk_label_set_use_markup  (GTK_LABEL (pDescriptionLabel), TRUE);
	gtk_label_set_justify (GTK_LABEL (pDescriptionLabel), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (pDescriptionLabel), TRUE);
	gtk_label_set_selectable (GTK_LABEL (pDescriptionLabel), TRUE);
	g_signal_connect (G_OBJECT (pDescriptionLabel), "destroy", G_CALLBACK (on_delete_async_widget), NULL);

	// min size
	int iFrameWidth = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pMainWindow), "frame-width"));
	int iMinSize = (gldi_desktop_get_width() - iFrameWidth) /2.5;

	// preview image
	GtkWidget *pPreviewImage = gtk_image_new_from_pixbuf (NULL);
	g_signal_connect (G_OBJECT (pPreviewImage), "destroy", G_CALLBACK (on_delete_async_widget), NULL);
	
	// Test if can be removed
	if (bHorizontalPackaging)
		gtk_widget_set_size_request (pPreviewImage, MIN (iMinSize, CAIRO_DOCK_PREVIEW_WIDTH), CAIRO_DOCK_PREVIEW_HEIGHT);

	// Add a frame around the image.
	GtkWidget *pImageFrame = cairo_dock_widget_image_frame_new (pPreviewImage);
	gtk_widget_set_size_request (pImageFrame, CAIRO_DOCK_README_WIDTH_MIN, -1);
	// and load it.
	if (cInitialImage)
		_set_preview_image (cInitialImage, GTK_IMAGE (pPreviewImage), pImageFrame);
	else
		gtk_frame_set_shadow_type (GTK_FRAME (pImageFrame), GTK_SHADOW_NONE);

	GtkWidget *pPreviewBox;
	GtkWidget* pDescriptionFrame = NULL;
	GtkWidget* pTextVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN);

	// info bar
	if (iAddInfoBar)
	{
		// vertical frame.
		pDescriptionFrame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(pDescriptionFrame), GTK_SHADOW_OUT);
		
		// title
		GtkWidget* pTitle = gtk_label_new (NULL);
		gtk_label_set_use_markup (GTK_LABEL (pTitle), TRUE);
		gtk_widget_set_name (pTitle, "pTitle");
		
		// author
		GtkWidget* pAuthor = gtk_label_new (NULL);
		gtk_label_set_use_markup (GTK_LABEL (pAuthor), TRUE);
		gtk_widget_set_name (pAuthor, "pAuthor");
		gtk_widget_hide (pAuthor);
		
		data[2] = pTitle;
		data[3] = pAuthor;
		
		// pack in 1 or 2 lines.
		GtkWidget* pFirstLine = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
		GtkWidget *pSecondLine = NULL;
		
		if (bHorizontalPackaging)
		{
			// Use the frame border for the title. 
			gtk_frame_set_label_widget (GTK_FRAME (pDescriptionFrame), pTitle);
		}
		else
			gtk_box_pack_start (GTK_BOX (pFirstLine), pTitle, FALSE, FALSE, CAIRO_DOCK_ICON_MARGIN);
		
		if (iAddInfoBar == 1)
		{
			gtk_box_pack_end (GTK_BOX (pFirstLine), pAuthor, FALSE, FALSE, CAIRO_DOCK_ICON_MARGIN);
		}
		else
		{
			GtkWidget* pState = gtk_label_new (NULL);
			gtk_label_set_use_markup (GTK_LABEL (pState), TRUE);
			gtk_box_pack_end (GTK_BOX (pFirstLine), pState, FALSE, FALSE, CAIRO_DOCK_ICON_MARGIN);  // state on the right.
			gtk_widget_set_name (pState, "pState");
			
			GtkWidget* pStateIcon = gtk_image_new_from_pixbuf (NULL);
			gtk_box_pack_end (GTK_BOX (pFirstLine), pStateIcon, FALSE, FALSE, CAIRO_DOCK_ICON_MARGIN);  // icon next to state.
			gtk_widget_set_name (pStateIcon, "pStateIcon");

			pSecondLine = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
			
			gtk_box_pack_start (GTK_BOX (pSecondLine), pAuthor, FALSE, FALSE, CAIRO_DOCK_ICON_MARGIN);  // author below title.
			
			GtkWidget* pSize = gtk_label_new (NULL);
			gtk_label_set_use_markup (GTK_LABEL (pSize), TRUE);
			gtk_box_pack_end (GTK_BOX (pSecondLine), pSize, FALSE, FALSE, CAIRO_DOCK_ICON_MARGIN);  // size below state.
			gtk_widget_set_name (pSize, "pSize");
			
			data[4] = pState;
			data[5] = pStateIcon;
			data[6] = pSize;
		}
		// pack everything in the frame vbox.
		gtk_box_pack_start (GTK_BOX (pTextVBox), pFirstLine, FALSE, FALSE, CAIRO_DOCK_GUI_MARGIN);
		if (pSecondLine)
			gtk_box_pack_start (GTK_BOX (pTextVBox), pSecondLine, FALSE, FALSE, CAIRO_DOCK_GUI_MARGIN);
		GtkWidget* pDescriptionBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN); // align text data on the left
		gtk_box_pack_start (GTK_BOX (pDescriptionBox), pDescriptionLabel, FALSE, FALSE, CAIRO_DOCK_GUI_MARGIN); // Maybe TRUE pour GTK3 & horiz
		gtk_box_pack_start (GTK_BOX (pTextVBox), pDescriptionBox, FALSE, FALSE, CAIRO_DOCK_GUI_MARGIN);
	}
	else
	{
		gtk_box_pack_start (GTK_BOX (pTextVBox), pDescriptionLabel, FALSE, FALSE, CAIRO_DOCK_GUI_MARGIN);
	}
	
	// connect to the widget.
	data[0] = pDescriptionLabel;
	data[1] = pPreviewImage;
	data[7] = pImageFrame;
	
	if (GTK_IS_COMBO_BOX (pOneWidget))
	{
		g_signal_connect (G_OBJECT (pOneWidget),
			"changed",
			G_CALLBACK (_cairo_dock_select_one_item_in_combo),
			data);
	}
	else if (GTK_IS_TREE_VIEW (pOneWidget))
	{
		GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pOneWidget));
		gtk_tree_selection_set_select_function (selection,
			(GtkTreeSelectionFunc) _cairo_dock_select_one_item_in_tree,
			data,
			NULL);
	}

	// Build boxes fieldset.
	if (bHorizontalPackaging)
	{
		// FrameHBox will set the frame border full size to the right.
		GtkWidget *pFrameHBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
		if (pDescriptionFrame)
		{
			gtk_container_add (GTK_CONTAINER(pDescriptionFrame), pFrameHBox);
			pPreviewBox = pDescriptionFrame;
		}
		else
		{
			pPreviewBox = pFrameHBox;
		}
		gtk_box_pack_start (GTK_BOX (pFrameHBox), pTextVBox, TRUE, TRUE, 0);

		GtkWidget *pPreviewImageBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN);

		GtkWidget *pPreviewImageSubBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
		gtk_box_pack_start (GTK_BOX (pPreviewImageSubBox), pImageFrame, FALSE, FALSE, 2 * CAIRO_DOCK_GUI_MARGIN);
		gtk_box_pack_start (GTK_BOX (pPreviewImageBox), pPreviewImageSubBox, FALSE, FALSE, 0);
		
		gtk_box_pack_end (GTK_BOX (pFrameHBox), pPreviewImageBox, FALSE, FALSE, 2 * CAIRO_DOCK_GUI_MARGIN);
	}
	else
	{
		// Add TextVBox to the main frame if created. (iAddInfoBar > 0)
		if (pDescriptionFrame)
		{
			gtk_container_add (GTK_CONTAINER(pDescriptionFrame), pTextVBox);
			pPreviewBox = pDescriptionFrame;
		}
		else
		{
			pPreviewBox = pTextVBox;
		}
		
		// pPreviewImageBox : center image on x axis.
		GtkWidget *pPreviewImageBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN);
		// pPreviewImageBox : prevent image frame from using full width.
		GtkWidget *pPreviewImageSubBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
		gtk_box_pack_start (GTK_BOX (pPreviewImageSubBox), pImageFrame, TRUE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (pPreviewImageBox), pPreviewImageSubBox, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (pTextVBox), pPreviewImageBox, FALSE, FALSE, 2 * CAIRO_DOCK_GUI_MARGIN);
	}
	
	return pPreviewBox;
}


GtkWidget *cairo_dock_widget_handbook_new (GldiModule *pModule)
{
	g_return_val_if_fail (pModule != NULL, NULL);
	
	// Frame with label
	GtkWidget *pFrame = gtk_frame_new (NULL);
	gtk_container_set_border_width (GTK_CONTAINER (pFrame), CAIRO_DOCK_GUI_MARGIN);
	gchar *cLabel = g_strdup_printf ("<big><b>%s </b></big>v%s",
		pModule->pVisitCard->cTitle,
		pModule->pVisitCard->cModuleVersion);
	GtkWidget *pLabel = gtk_label_new (cLabel);
	g_free (cLabel);
	gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
	gtk_frame_set_label_widget (GTK_FRAME (pFrame), pLabel);
	
	// TopHBox : Will align widgets on top.
	GtkWidget *pTopHBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
	gtk_container_add (GTK_CONTAINER (pFrame), pTopHBox);

	// TextBox : Align text widgets on the left from top to bottom.
	GtkWidget *pTextBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (pTopHBox), pTextBox, FALSE, FALSE, 0);

	// Author(s) text
	gchar *cDescription = g_strdup_printf ("<small><tt>by %s</tt></small>", pModule->pVisitCard->cAuthor);
	pLabel = gtk_label_new (cDescription);
	g_free (cDescription);
	gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (pLabel), TRUE);
	gtk_label_set_justify (GTK_LABEL (pLabel), GTK_JUSTIFY_LEFT);
	GtkWidget *pAlignLeft = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
	gtk_container_set_border_width (GTK_CONTAINER (pAlignLeft), CAIRO_DOCK_GUI_MARGIN);
	gtk_box_pack_start (GTK_BOX (pAlignLeft), pLabel, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pTextBox), pAlignLeft, FALSE, FALSE, 0);
	
	// Description Text
	cDescription = g_strdup_printf ("<span rise='8000'>%s</span>",
		dgettext (pModule->pVisitCard->cGettextDomain, pModule->pVisitCard->cDescription));
	pLabel = gtk_label_new (cDescription);
	g_free (cDescription);
	gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
	gtk_label_set_selectable (GTK_LABEL (pLabel), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (pLabel), TRUE);
	gtk_label_set_justify (GTK_LABEL (pLabel), GTK_JUSTIFY_LEFT);
	g_object_set (pLabel, "width-request", CAIRO_DOCK_README_WIDTH, NULL);

	gtk_box_pack_start (GTK_BOX (pTextBox), pLabel, FALSE, FALSE, 0);

	// ModuleImage
	int iPreviewWidth, iPreviewHeight;
	GdkPixbuf *pPreviewPixbuf = NULL;
	if (gdk_pixbuf_get_file_info (pModule->pVisitCard->cPreviewFilePath, &iPreviewWidth, &iPreviewHeight) != NULL)  // The return value is owned by GdkPixbuf and should not be freed.
	{
		int w = 200, h = 200;
		if (iPreviewWidth > w)
		{
			iPreviewHeight *= 1.*w/iPreviewWidth;
			iPreviewWidth = w;
		}
		if (iPreviewHeight > h)
		{
			iPreviewWidth *= 1.*h/iPreviewHeight;
			iPreviewHeight = h;
		}
		pPreviewPixbuf = gdk_pixbuf_new_from_file_at_size (pModule->pVisitCard->cPreviewFilePath, iPreviewWidth, iPreviewHeight, NULL);
		if (pPreviewPixbuf != NULL)
		{
			// ImageBox : Align the image on top.
			GtkWidget *pImageBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN);
			gtk_box_pack_end (GTK_BOX (pTopHBox), pImageBox, FALSE, FALSE, CAIRO_DOCK_GUI_MARGIN);
			
			// Image Widget.
			GtkWidget *pModuleImage = gtk_image_new_from_pixbuf (NULL);
			gtk_image_set_from_pixbuf (GTK_IMAGE (pModuleImage), pPreviewPixbuf);
			g_object_unref (pPreviewPixbuf);
			
			// Add a frame around the image.
			GtkWidget *pImageFrame = cairo_dock_widget_image_frame_new (pModuleImage);
			gtk_box_pack_start (GTK_BOX (pImageBox), pImageFrame, FALSE, FALSE, 0);
		}
	}
	
	return pFrame;
}


#define _pack_in_widget_box(pSubWidget) gtk_box_pack_start (GTK_BOX (pWidgetBox), pSubWidget, FALSE, FALSE, 0)
#define _pack_subwidget(pSubWidget) do {\
	pSubWidgetList = g_slist_append (pSubWidgetList, pSubWidget);\
	_pack_in_widget_box (pSubWidget); } while (0)
#define _pack_hscale(pSubWidget) do {\
	GtkWidget *pExtendedWidget;\
	if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[0] != NULL && pAuthorizedValuesList[1] != NULL && pAuthorizedValuesList[2] != NULL && pAuthorizedValuesList[3] != NULL) {\
		pExtendedWidget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);\
		GtkWidget *label = gtk_label_new (dgettext (cGettextDomain, pAuthorizedValuesList[2]));\
		GtkWidget *pAlign = gtk_alignment_new (1., 1., 0., 0.);\
		gtk_container_add (GTK_CONTAINER (pAlign), label);\
		gtk_box_pack_start (GTK_BOX (pExtendedWidget), pAlign, FALSE, FALSE, 0);\
		gtk_box_pack_start (GTK_BOX (pExtendedWidget), pSubWidget, FALSE, FALSE, 0);\
		label = gtk_label_new (dgettext (cGettextDomain, pAuthorizedValuesList[3]));\
		pAlign = gtk_alignment_new (1., 1., 0., 0.);\
		gtk_container_add (GTK_CONTAINER (pAlign), label);\
		gtk_box_pack_start (GTK_BOX (pExtendedWidget), pAlign, FALSE, FALSE, 0); }\
	else {\
		pExtendedWidget = pOneWidget; }\
	pSubWidgetList = g_slist_append (pSubWidgetList, pSubWidget);\
	_pack_in_widget_box (pExtendedWidget); } while (0)

static inline GtkWidget *_combo_box_entry_new_with_model (GtkTreeModel *modele, int column)
{
	GtkWidget *w = gtk_combo_box_new_with_model_and_entry (modele);
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (w), column);
	return w;
}

#define _add_combo_from_modele(modele, bAddPreviewWidgets, bWithEntry, bHorizontalPackaging) do {\
	if (modele == NULL) { \
		pOneWidget = gtk_combo_box_new_with_entry ();\
		_pack_subwidget (pOneWidget); }\
	else {\
		cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);\
		if (bWithEntry) {\
			pOneWidget = _combo_box_entry_new_with_model (GTK_TREE_MODEL (modele), CAIRO_DOCK_MODEL_NAME); }\
		else {\
			pOneWidget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (modele));\
			rend = gtk_cell_renderer_text_new ();\
			gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (pOneWidget), rend, FALSE);\
			gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (pOneWidget), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);}\
		if (bAddPreviewWidgets) {\
			pPreviewBox = cairo_dock_gui_make_preview_box (pMainWindow, pOneWidget, bHorizontalPackaging, 1, NULL, NULL, pDataGarbage);\
			gboolean bFullSize = bWithEntry || bHorizontalPackaging;\
			gtk_box_pack_start (GTK_BOX (pAdditionalItemsVBox ? pAdditionalItemsVBox : pKeyBox), pPreviewBox, bFullSize, bFullSize, 0);}\
		if (_cairo_dock_find_iter_from_name (modele, cValue, &iter))\
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (pOneWidget), &iter);\
		_pack_subwidget (pOneWidget);\
		g_free (cValue); } } while (0)

const gchar *cairo_dock_parse_key_comment (gchar *cKeyComment, char *iElementType, guint *iNbElements, gchar ***pAuthorizedValuesList, gboolean *bAligned, const gchar **cTipString)
{
	if (cKeyComment == NULL || *cKeyComment == '\0')
		return NULL;
	
	gchar *cUsefulComment = cKeyComment;
	while (*cUsefulComment == '#' || *cUsefulComment == ' ' || *cUsefulComment == '\n')  // on saute les # et les espaces.
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
	if (*cUsefulComment == CAIRO_DOCK_WIDGET_CAIRO_ONLY)
	{
		if (g_bUseOpenGL)
			return NULL;
		cUsefulComment ++;
	}	
	else if (*cUsefulComment == CAIRO_DOCK_WIDGET_OPENGL_ONLY)
	{
		if (! g_bUseOpenGL)
			return NULL;
		cUsefulComment ++;
	}
	
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

GtkWidget *cairo_dock_build_group_widget (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath)
{
	g_return_val_if_fail (pKeyFile != NULL && cGroupName != NULL, NULL);
	//GPtrArray *pDataGarbage = g_ptr_array_new ();
	//GPtrArray *pModelGarbage = g_ptr_array_new ();
	
	gconstpointer *data;
	gsize length = 0;
	gchar **pKeyList;
	
	GtkWidget *pOneWidget;
	GSList * pSubWidgetList;
	GtkWidget *pLabel=NULL, *pLabelContainer;
	GtkWidget *pGroupBox, *pKeyBox=NULL, *pSmallVBox, *pWidgetBox=NULL;
	GtkWidget *pEntry;
	GtkWidget *pTable;
	GtkWidget *pButtonAdd, *pButtonRemove;
	GtkWidget *pButtonDown, *pButtonUp;
	GtkWidget *pButtonFileChooser, *pButtonPlay;
	GtkWidget *pFrame, *pFrameVBox;
	GtkWidget *pScrolledWindow;
	GtkWidget *pToggleButton=NULL;
	GtkCellRenderer *rend;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkWidget *pBackButton;
	GList *pControlWidgets = NULL;
	int iFirstSensitiveWidget = 0, iNbControlledWidgets = 0, iNbSensitiveWidgets = 0;
	gchar *cKeyName, *cKeyComment, **pAuthorizedValuesList;
	const gchar *cUsefulComment, *cTipString;
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	int j;
	guint k, iNbElements;
	char iElementType;
	gboolean bValue, *bValueList;
	int iValue, iMinValue, iMaxValue, *iValueList;
	double fValue, fMinValue, fMaxValue, *fValueList;
	gchar *cValue, **cValueList, *cSmallIcon=NULL;
	GdkRGBA gdkColor;
	GtkListStore *modele;
	gboolean bAddBackButton;
	GtkWidget *pPreviewBox;
	GtkWidget *pAdditionalItemsVBox;
	gboolean bIsAligned;
	gboolean bInsert;
	gboolean bFullSize;
	
	pGroupBox = NULL;
	pFrame = NULL;
	pFrameVBox = NULL;
	
	pKeyList = g_key_file_get_keys (pKeyFile, cGroupName, NULL, NULL);
	g_return_val_if_fail (pKeyList != NULL, NULL);
	
	for (j = 0; pKeyList[j] != NULL; j ++)
	{
		cKeyName = pKeyList[j];
		
		//\______________ On parse le commentaire.
		pAuthorizedValuesList = NULL;
		cTipString = NULL;
		iNbElements = 0;
		cKeyComment =  g_key_file_get_comment (pKeyFile, cGroupName, cKeyName, NULL);
		cUsefulComment = cairo_dock_parse_key_comment (cKeyComment, &iElementType, &iNbElements, &pAuthorizedValuesList, &bIsAligned, &cTipString);
		if (cUsefulComment == NULL)
		{
			g_free (cKeyComment);
			continue;
		}
		if (iElementType == '[')  // on gere le bug de la Glib, qui rajoute les nouvelles cles apres le commentaire du groupe suivant !
		{
			g_free (cKeyComment);
			continue;
		}
		
		//\______________ On cree la boite du groupe si c'est la 1ere cle valide.
		if (pGroupBox == NULL)  // maintenant qu'on a au moins un element dans ce groupe, on cree sa page dans le notebook.
		{
			pGroupBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN);
			gtk_container_set_border_width (GTK_CONTAINER (pGroupBox), CAIRO_DOCK_GUI_MARGIN);
		}
		
		pKeyBox = NULL;
		pLabel = NULL;
		pWidgetBox = NULL;
		pAdditionalItemsVBox = NULL;
		bFullSize = (iElementType == CAIRO_DOCK_WIDGET_THEME_LIST
			|| iElementType == CAIRO_DOCK_WIDGET_VIEW_LIST
			|| iElementType == CAIRO_DOCK_WIDGET_EMPTY_FULL);

		if (iElementType == CAIRO_DOCK_WIDGET_HANDBOOK)
		{
			cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
			GldiModule *pModule = gldi_module_get (cValue);
			g_free (cValue);
			GtkWidget *pHandbook = cairo_dock_widget_handbook_new (pModule);
			if (pHandbook != NULL)
				gtk_box_pack_start (GTK_BOX (pGroupBox), pHandbook, TRUE, TRUE, 0);
		}
		else if (iElementType != CAIRO_DOCK_WIDGET_FRAME && iElementType != CAIRO_DOCK_WIDGET_EXPANDER && iElementType != CAIRO_DOCK_WIDGET_SEPARATOR)
		{
			//\______________ On cree la boite de la cle.
			if (iElementType == CAIRO_DOCK_WIDGET_THEME_LIST || iElementType == CAIRO_DOCK_WIDGET_VIEW_LIST)
			{
				pAdditionalItemsVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
				gtk_box_pack_start (pFrameVBox != NULL ? GTK_BOX (pFrameVBox) :  GTK_BOX (pGroupBox),
					pAdditionalItemsVBox,
					bFullSize,
					bFullSize,
					0);
				pKeyBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
				gtk_box_pack_start (GTK_BOX (pAdditionalItemsVBox),
					pKeyBox,
					FALSE,
					FALSE,
					0);
			}
			else
			{
				pKeyBox = (bIsAligned ? gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN) : gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN));
				gtk_box_pack_start (pFrameVBox != NULL ? GTK_BOX (pFrameVBox) : GTK_BOX (pGroupBox),
					pKeyBox,
					bFullSize,
					bFullSize,
					0);
				
			}
			if (cTipString != NULL)
			{
				gtk_widget_set_tooltip_text (pKeyBox, dgettext (cGettextDomain, cTipString));
			}
			if (pControlWidgets != NULL)
			{
				CDControlWidget *cw = pControlWidgets->data;
				//g_print ("ctrl (%d widgets)\n", iNbControlledWidgets);
				if (cw->pControlContainer == (pFrameVBox ? pFrameVBox : pGroupBox))
				{
					//g_print ("ctrl (iNbControlledWidgets:%d, iFirstSensitiveWidget:%d, iNbSensitiveWidgets:%d)\n", iNbControlledWidgets, iFirstSensitiveWidget, iNbSensitiveWidgets);
					cw->iNbControlledWidgets --;
					if (cw->iFirstSensitiveWidget > 0)
						cw->iFirstSensitiveWidget --;
					cw->iNonSensitiveWidget --;
					
					GtkWidget *w = (pAdditionalItemsVBox ? pAdditionalItemsVBox : pKeyBox);
					if (cw->iFirstSensitiveWidget == 0 && cw->iNbSensitiveWidgets > 0 && cw->iNonSensitiveWidget != 0)  // on est dans la zone des widgets sensitifs.
					{
						//g_print (" => sensitive\n");
						cw->iNbSensitiveWidgets --;
						if (GTK_IS_EXPANDER (w))
							gtk_expander_set_expanded (GTK_EXPANDER (w), TRUE);
					}
					else
					{
						//g_print (" => unsensitive\n");
						if (!GTK_IS_EXPANDER (w))
							gtk_widget_set_sensitive (w, FALSE);
					}
					if (cw->iFirstSensitiveWidget == 0 && cw->iNbControlledWidgets == 0)
					{
						pControlWidgets = g_list_delete_link (pControlWidgets, pControlWidgets);
						g_free (cw);
					}
				}
			}
			
			//\______________ On cree le label descriptif et la boite du widget.
			if (*cUsefulComment != '\0' && strcmp (cUsefulComment, "loading...") != 0)
			{
				pLabel = gtk_label_new (NULL);
				gtk_label_set_use_markup  (GTK_LABEL (pLabel), TRUE);
				gtk_label_set_markup (GTK_LABEL (pLabel), dgettext (cGettextDomain, cUsefulComment));
			}
			if (pLabel != NULL)
			{
				GtkWidget *pAlign = gtk_alignment_new (0., 0.5, 0., 0.);
				gtk_container_add (GTK_CONTAINER (pAlign), pLabel);
				gtk_box_pack_start (GTK_BOX (pKeyBox),
					pAlign,
					FALSE,
					FALSE,
					0);
			}
			
			if (iElementType != CAIRO_DOCK_WIDGET_EMPTY_WIDGET)  // inutile si rien dans dedans.
			{	// cette boite permet d'empiler les widgets a droite, mais en les rangeant de gauche a droite normalement.
				pWidgetBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_GUI_MARGIN);
				gtk_box_pack_end (GTK_BOX (pKeyBox),
					pWidgetBox,
					FALSE,
					FALSE,
					0);
			}
		}
		
		pSubWidgetList = NULL;
		bAddBackButton = FALSE;
		bInsert = TRUE;
		
		//\______________ On cree les widgets selon leur type.
		switch (iElementType)
		{
			case CAIRO_DOCK_WIDGET_CHECK_BUTTON :  // boolean
			case CAIRO_DOCK_WIDGET_CHECK_CONTROL_BUTTON :  // boolean qui controle le widget suivant
				length = 0;
				bValueList = g_key_file_get_boolean_list (pKeyFile, cGroupName, cKeyName, &length, NULL);
				
				for (k = 0; k < iNbElements; k ++)
				{
					bValue =  (k < length ? bValueList[k] : FALSE);
					pOneWidget = gtk_check_button_new ();
					gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), bValue);
					
					if (iElementType == CAIRO_DOCK_WIDGET_CHECK_CONTROL_BUTTON)
					{
						_allocate_new_buffer;
						data[0] = pKeyBox;
						data[1] = (pFrameVBox != NULL ? pFrameVBox : pGroupBox);
						if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[0] != NULL)
							iNbControlledWidgets = g_ascii_strtod (pAuthorizedValuesList[0], NULL);
						else
							iNbControlledWidgets = 1;
						data[2] = GINT_TO_POINTER (iNbControlledWidgets);
						if (iNbControlledWidgets < 0)  // a negative value means that the behavior is inverted.
						{
							bValue = !bValue;
							iNbControlledWidgets = -iNbControlledWidgets;
						}
						g_signal_connect (G_OBJECT (pOneWidget), "toggled", G_CALLBACK(_cairo_dock_toggle_control_button), data);
						
						g_object_set_data (G_OBJECT (pKeyBox), "nb-ctrl-widgets", GINT_TO_POINTER (iNbControlledWidgets));
						g_object_set_data (G_OBJECT (pKeyBox), "one-widget", pOneWidget);
						
						if (! bValue)  // les widgets suivants seront inactifs.
						{
							CDControlWidget *cw = g_new0 (CDControlWidget, 1);
							pControlWidgets = g_list_prepend (pControlWidgets, cw);
							cw->iNbSensitiveWidgets = 0;
							cw->iNbControlledWidgets = iNbControlledWidgets;
							cw->iFirstSensitiveWidget = 1;
							cw->pControlContainer = (pFrameVBox != NULL ? pFrameVBox : pGroupBox);
						}  // sinon le widget suivant est sensitif, donc rien a faire.
					}
					
					_pack_subwidget (pOneWidget);
				}
				g_free (bValueList);
			break;

			case CAIRO_DOCK_WIDGET_SPIN_INTEGER :  // integer
			case CAIRO_DOCK_WIDGET_HSCALE_INTEGER :  // integer dans un HScale
			case CAIRO_DOCK_WIDGET_SIZE_INTEGER :  // double integer WxH
				if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[0] != NULL)
				{
					iMinValue = g_ascii_strtod (pAuthorizedValuesList[0], NULL);
					if (pAuthorizedValuesList[1] != NULL)
						iMaxValue = g_ascii_strtod (pAuthorizedValuesList[1], NULL);
					else
						iMaxValue = 9999;
				}
				else
				{
					iMinValue = 0;
					iMaxValue = 9999;
				}
				if (iElementType == CAIRO_DOCK_WIDGET_SIZE_INTEGER)
					iNbElements *= 2;
				length = 0;
				iValueList = g_key_file_get_integer_list (pKeyFile, cGroupName, cKeyName, &length, NULL);
				GtkWidget *pPrevOneWidget=NULL;
				int iPrevValue=0;
				if (iElementType == CAIRO_DOCK_WIDGET_SIZE_INTEGER)
				{
					pToggleButton = gtk_toggle_button_new ();
					GtkWidget *pImage = gtk_image_new_from_icon_name (GLDI_ICON_NAME_MEDIA_PAUSE, GTK_ICON_SIZE_MENU);  // trouver une image...
					gtk_button_set_image (GTK_BUTTON (pToggleButton), pImage);
				}
				for (k = 0; k < iNbElements; k ++)
				{
					iValue =  (k < length ? iValueList[k] : 0);
					GtkAdjustment *pAdjustment = gtk_adjustment_new (iValue,
						0,
						1,
						1,
						MAX (1, (iMaxValue - iMinValue) / 20),
						0);
					
					if (iElementType == CAIRO_DOCK_WIDGET_HSCALE_INTEGER)
					{
						pOneWidget = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT (pAdjustment));
						gtk_scale_set_digits (GTK_SCALE (pOneWidget), 0);
						g_object_set (pOneWidget, "width-request", 150, NULL);
						
						_pack_hscale (pOneWidget);
					}
					else
					{
						pOneWidget = gtk_spin_button_new (GTK_ADJUSTMENT (pAdjustment), 1., 0);
						
						_pack_subwidget (pOneWidget);
					}
					g_object_set (pAdjustment, "lower", (double) iMinValue, "upper", (double) iMaxValue, NULL); // le 'width-request' sur un GtkHScale avec 'fMinValue' non nul plante ! Donc on les met apres...
					gtk_adjustment_set_value (GTK_ADJUSTMENT (pAdjustment), iValue);
					
					if (iElementType == CAIRO_DOCK_WIDGET_SIZE_INTEGER && k+1 < iNbElements)  // on rajoute le separateur.
					{
						GtkWidget *pLabelX = gtk_label_new ("x");
						_pack_in_widget_box (pLabelX);
					}
					if (iElementType == CAIRO_DOCK_WIDGET_SIZE_INTEGER && (k&1))  // on lie les 2 spins entre eux.
					{
						if (iPrevValue == iValue)
							gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pToggleButton), TRUE);
						_allocate_new_buffer;
						data[0] = pPrevOneWidget;
						data[1] = pToggleButton;
						g_signal_connect (G_OBJECT (pOneWidget), "value-changed", G_CALLBACK(_cairo_dock_set_value_in_pair), data);
						_allocate_new_buffer;
						data[0] = pOneWidget;
						data[1] = pToggleButton;
						g_signal_connect (G_OBJECT (pPrevOneWidget), "value-changed", G_CALLBACK(_cairo_dock_set_value_in_pair), data);
					}
					pPrevOneWidget = pOneWidget;
					iPrevValue = iValue;
				}
				if (iElementType == CAIRO_DOCK_WIDGET_SIZE_INTEGER)
				{
					_pack_in_widget_box (pToggleButton);
				}
				bAddBackButton = TRUE;
				g_free (iValueList);
			break;

			case CAIRO_DOCK_WIDGET_SPIN_DOUBLE :  // float.
			case CAIRO_DOCK_WIDGET_HSCALE_DOUBLE :  // float dans un HScale.
				if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[0] != NULL)
					fMinValue = g_ascii_strtod (pAuthorizedValuesList[0], NULL);
				else
					fMinValue = 0;
				if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[1] != NULL)
					fMaxValue = g_ascii_strtod (pAuthorizedValuesList[1], NULL);
				else
					fMaxValue = 9999;
				length = 0;
				fValueList = g_key_file_get_double_list (pKeyFile, cGroupName, cKeyName, &length, NULL);
				for (k = 0; k < iNbElements; k ++)
				{
					fValue =  (k < length ? fValueList[k] : 0);

					GtkAdjustment *pAdjustment = gtk_adjustment_new (fValue,
						0,
						1,
						(fMaxValue - fMinValue) / 20.,
						(fMaxValue - fMinValue) / 10.,
						0);
					
					if (iElementType == CAIRO_DOCK_WIDGET_HSCALE_DOUBLE)
					{
						pOneWidget = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT (pAdjustment));
						gtk_scale_set_digits (GTK_SCALE (pOneWidget), 3);
						g_object_set (pOneWidget, "width-request", 150, NULL);
						
						_pack_hscale (pOneWidget);
					}
					else
					{
						pOneWidget = gtk_spin_button_new (GTK_ADJUSTMENT (pAdjustment),
							1.,
							3);
						_pack_subwidget (pOneWidget);
					}
					g_object_set (pAdjustment, "lower", fMinValue, "upper", fMaxValue, NULL); // le 'width-request' sur un GtkHScale avec 'fMinValue' non nul plante ! Donc on les met apres...
					gtk_adjustment_set_value (GTK_ADJUSTMENT (pAdjustment), fValue);
				}
				bAddBackButton = TRUE,
				g_free (fValueList);
			break;
			
			case CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGB :  // float x3 avec un bouton de choix de couleur.
			case CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGBA :  // float x4 avec un bouton de choix de couleur.
				iNbElements = (iElementType == CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGB ? 3 : 4);
				length = 0;
				fValueList = g_key_file_get_double_list (pKeyFile, cGroupName, cKeyName, &length, NULL);
				gboolean bUseAlpha = FALSE;
				if (length > 2)
				{
					gdkColor.red = fValueList[0];
					gdkColor.green = fValueList[1];
					gdkColor.blue = fValueList[2];
					if (length > 3 && iElementType == CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGBA)
					{
						bUseAlpha = TRUE;
						gdkColor.alpha = fValueList[3];
					}
					else
						gdkColor.alpha = 1.;
				}
				pOneWidget = gtk_color_button_new_with_rgba (&gdkColor);
				gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (pOneWidget), bUseAlpha);
				_pack_subwidget (pOneWidget);
				bAddBackButton = TRUE,
				g_free (fValueList);
			break;
			
			case CAIRO_DOCK_WIDGET_VIEW_LIST :  // liste des vues.
			{
				GtkListStore *pRendererListStore = _cairo_dock_build_renderer_list_for_gui ();
				_add_combo_from_modele (pRendererListStore, TRUE, FALSE, TRUE);
				g_object_unref (pRendererListStore);
			}
			break ;
			
			case CAIRO_DOCK_WIDGET_THEME_LIST :  // liste les themes dans combo, avec prevue et readme.
				//\______________ On construit le widget de visualisation de themes.
				modele = _cairo_dock_gui_allocate_new_model ();
				gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (modele), CAIRO_DOCK_MODEL_NAME, GTK_SORT_ASCENDING);
				
				_add_combo_from_modele (modele, TRUE, FALSE, TRUE);
				
				if (iElementType == CAIRO_DOCK_WIDGET_THEME_LIST)  // add the state icon.
				{
					rend = gtk_cell_renderer_pixbuf_new ();
					gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (pOneWidget), rend, FALSE);
					gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (pOneWidget), rend, "pixbuf", CAIRO_DOCK_MODEL_ICON, NULL);
					gtk_cell_layout_reorder (GTK_CELL_LAYOUT (pOneWidget), rend, 0);
				}
				
				//\______________ On recupere les themes.
				if (pAuthorizedValuesList != NULL)
				{
					// get the local, shared and distant paths.
					gchar *cShareThemesDir = NULL, *cUserThemesDir = NULL, *cDistantThemesDir = NULL, *cHint = NULL;
					if (pAuthorizedValuesList[0] != NULL)
					{
						cShareThemesDir = (*pAuthorizedValuesList[0] != '\0' ? cairo_dock_search_image_s_path (pAuthorizedValuesList[0]) : NULL);  // on autorise les ~/blabla.
						if (pAuthorizedValuesList[1] != NULL)
						{
							cUserThemesDir = g_strdup_printf ("%s/%s", g_cExtrasDirPath, pAuthorizedValuesList[1]);
							if (pAuthorizedValuesList[2] != NULL)
							{
								cDistantThemesDir = (*pAuthorizedValuesList[2] != '\0' ? pAuthorizedValuesList[2] : NULL);
								cHint = pAuthorizedValuesList[3];  // NULL to not filter.
							}
						}
					}
					
					// list local packages first.
					_allocate_new_buffer;
					data[0] = pOneWidget;
					data[1] = pMainWindow;
					data[2] = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);  // freed in the callback '_got_themes_combo_list'.
					data[3] = g_strdup (cHint);  // idem
					
					GHashTable *pThemeTable = cairo_dock_list_packages (cShareThemesDir, cUserThemesDir, NULL, NULL);
					_got_themes_combo_list (pThemeTable, (gpointer*)data);
					
					// list distant packages asynchronously.
					if (cDistantThemesDir != NULL)
					{
						cairo_dock_set_status_message_printf (pMainWindow, _("Listing themes in '%s' ..."), cDistantThemesDir);
						data[2] = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);  // freed in the callback '_got_themes_combo_list'.
						data[3] = g_strdup (cHint);
						GldiTask *pTask = cairo_dock_list_packages_async (NULL, NULL, cDistantThemesDir, (CairoDockGetPackagesFunc) _got_themes_combo_list, data, pThemeTable);  // the table will be freed along with the task.
						g_object_set_data (G_OBJECT (pOneWidget), "cd-task", pTask);
						g_signal_connect (G_OBJECT (pOneWidget), "destroy", G_CALLBACK (on_delete_async_widget), NULL);
					}
					else
					{
						g_hash_table_destroy (pThemeTable);
					}
					g_free (cUserThemesDir);
					g_free (cShareThemesDir);
				}
			break ;
			
			case CAIRO_DOCK_WIDGET_ANIMATION_LIST :  // liste des animations.
			{
				GtkListStore *pAnimationsListStore = _cairo_dock_build_animations_list_for_gui ();
				_add_combo_from_modele (pAnimationsListStore, FALSE, FALSE, FALSE);
				g_object_unref (pAnimationsListStore);
			}
			break ;
			
			case CAIRO_DOCK_WIDGET_DIALOG_DECORATOR_LIST :  // liste des decorateurs de dialogue.
			{
				GtkListStore *pDialogDecoratorListStore = _cairo_dock_build_dialog_decorator_list_for_gui ();
				_add_combo_from_modele (pDialogDecoratorListStore, FALSE, FALSE, FALSE);
				g_object_unref (pDialogDecoratorListStore);
			}
			break ;
			
			case CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST :  // liste des decorations de desklet.
			case CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST_WITH_DEFAULT :  // idem mais avec le choix "defaut" en plus.
			{
				GtkListStore *pDecorationsListStore = ( iElementType == CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST ?
					_cairo_dock_build_desklet_decorations_list_for_gui () :
					_cairo_dock_build_desklet_decorations_list_for_applet_gui () );
				_add_combo_from_modele (pDecorationsListStore, FALSE, FALSE, FALSE);
				g_object_unref (pDecorationsListStore);
				
				_allocate_new_buffer;
				data[0] = pKeyBox;
				data[1] = (pFrameVBox != NULL ? pFrameVBox : pGroupBox);
				iNbControlledWidgets = 9;
				data[2] = GINT_TO_POINTER (iNbControlledWidgets);
				iNbControlledWidgets --;  // car dans cette fonction, on ne compte pas le separateur.
				g_signal_connect (G_OBJECT (pOneWidget), "changed", G_CALLBACK (_cairo_dock_select_custom_item_in_combo), data);
				
				GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (pOneWidget));
				GtkTreeIter iter;
				if (pOneWidget && gtk_combo_box_get_active_iter (GTK_COMBO_BOX (pOneWidget), &iter))
				{
					gchar *cName = NULL;
					gtk_tree_model_get (model, &iter,
						CAIRO_DOCK_MODEL_RESULT, &cName, -1);
					if (! cName || strcmp (cName, "personnal") != 0)  // widgets suivants inactifs.
					{
						CDControlWidget *cw = g_new0 (CDControlWidget, 1);
						pControlWidgets = g_list_prepend (pControlWidgets, cw);
						cw->iNbControlledWidgets = iNbControlledWidgets;
						cw->iNbSensitiveWidgets = 0;
						cw->iFirstSensitiveWidget = 1;
						cw->pControlContainer = (pFrameVBox != NULL ? pFrameVBox : pGroupBox);
					}  // widgets sensitifs donc rien a faire.
					g_free (cName);
				}
			}
			break ;
			
			case CAIRO_DOCK_WIDGET_DOCK_LIST :  // liste des docks existant.
			{
				// get the list of available docks, avoiding the sub-dock in the case of a Stack-Icon
				CairoDock *pSubDock = NULL;
				GError *error = NULL;
				int iIconType = g_key_file_get_integer (pKeyFile, cGroupName, "Icon Type", &error);
				if (error != NULL) // it's certainly not a container
					g_error_free (error);
				else if (iIconType == GLDI_USER_ICON_TYPE_STACK) // it's a stack-icon
				{
					gchar *cContainerName = g_key_file_get_string (pKeyFile, cGroupName, "Name", NULL);
					pSubDock = gldi_dock_get (cContainerName);
					g_free (cContainerName);
				}
				GList *pDocks = cairo_dock_get_available_docks (NULL, pSubDock);  // NULL because we want the current parent dock in the list.
				
				// make a tree-model from the list
				GtkListStore *pDocksListStore = _cairo_dock_build_dock_list_for_gui (pDocks);
				g_list_free (pDocks);
				
				// build the combo-box
				pOneWidget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (pDocksListStore));
				rend = gtk_cell_renderer_text_new ();
				gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (pOneWidget), rend, FALSE);
				gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (pOneWidget), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
				
				// select the current value
				cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);  // current value = current parent dock name
				GtkTreeIter iter;
				gboolean bIterFound = FALSE;
				if (cValue == NULL || *cValue == '\0')  // dock not specified => it's the main dock
					bIterFound = _cairo_dock_find_iter_from_name (pDocksListStore, CAIRO_DOCK_MAIN_DOCK_NAME, &iter);
				else
					bIterFound = _cairo_dock_find_iter_from_name (pDocksListStore, cValue, &iter);
				if (bIterFound)
					gtk_combo_box_set_active_iter (GTK_COMBO_BOX (pOneWidget), &iter);
				g_free (cValue);
				
				g_object_unref (pDocksListStore);
				_pack_subwidget (pOneWidget);
			}
			break ;
			
			case CAIRO_DOCK_WIDGET_ICON_THEME_LIST :
			{
				gchar *cUserPath = g_strdup_printf ("%s/.icons", g_getenv ("HOME"));
				const gchar *path[3];
				path[0] = (const gchar *)cUserPath;
				path[1] = "/usr/share/icons";
				path[2] = NULL;
				
				GHashTable *pHashTable = _cairo_dock_build_icon_themes_list (path);
				
				GtkListStore *pIconThemeListStore = _cairo_dock_build_icon_theme_list_for_gui (pHashTable);
				
				_add_combo_from_modele (pIconThemeListStore, FALSE, FALSE, FALSE);
				
				g_object_unref (pIconThemeListStore);
				g_free (cUserPath);
				g_hash_table_destroy (pHashTable);
			}
			break ;
			
			case CAIRO_DOCK_WIDGET_ICONS_LIST :
			{
				if (g_pMainDock == NULL) // maintenance mode... no dock, no icons
				{
					cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);

					pOneWidget = gtk_entry_new ();
					gtk_entry_set_text (GTK_ENTRY (pOneWidget), cValue); // if there is a problem, we can edit it.
					_pack_subwidget (pOneWidget);

					g_free (cValue);
					break;
				}

				// build the modele and combo
				modele = _cairo_dock_gui_allocate_new_model ();
				pOneWidget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (modele));
				rend = gtk_cell_renderer_pixbuf_new ();
				gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (pOneWidget), rend, FALSE);
				gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (pOneWidget), rend, "pixbuf", CAIRO_DOCK_MODEL_ICON, NULL);
				rend = gtk_cell_renderer_text_new ();
				gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (pOneWidget), rend, FALSE);
				gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (pOneWidget), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
				_pack_subwidget (pOneWidget);
				
				// get the dock
				CairoDock *pDock = NULL;
				if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[0] != NULL)
					pDock = gldi_dock_get (pAuthorizedValuesList[0]);
				if (!pDock)
					pDock = g_pMainDock;
				
				// insert each icon
				cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
				gint iDesiredIconSize = cairo_dock_search_icon_size (GTK_ICON_SIZE_LARGE_TOOLBAR); // 24 by default
				GtkTreeIter iter;
				Icon *pIcon;
				gchar *cImagePath, *cID;
				const gchar *cName;
				GdkPixbuf *pixbuf;
				GList *ic;
				for (ic = pDock->icons; ic != NULL; ic = ic->next)
				{
					pIcon = ic->data;
					if (pIcon->cDesktopFileName != NULL
					|| pIcon->pModuleInstance != NULL)
					{
						pixbuf = NULL;
						cImagePath = NULL;
						cName = NULL;
						
						// get the ID
						if (pIcon->cDesktopFileName != NULL)
							cID = pIcon->cDesktopFileName;
						else
							cID = pIcon->pModuleInstance->cConfFilePath;
						
						// get the image
						if (pIcon->cFileName != NULL)
						{
							cImagePath = cairo_dock_search_icon_s_path (pIcon->cFileName, iDesiredIconSize);
						}
						if (cImagePath == NULL || ! g_file_test (cImagePath, G_FILE_TEST_EXISTS))
						{
							g_free (cImagePath);
							if (GLDI_OBJECT_IS_SEPARATOR_ICON (pIcon))
							{
								if (myIconsParam.cSeparatorImage)
									cImagePath = cairo_dock_search_image_s_path (myIconsParam.cSeparatorImage);
							}
							else if (CAIRO_DOCK_IS_APPLET (pIcon))
							{
								cImagePath = g_strdup (pIcon->pModuleInstance->pModule->pVisitCard->cIconFilePath);
							}
							else
							{
								cImagePath = cairo_dock_search_image_s_path (CAIRO_DOCK_DEFAULT_ICON_NAME);
								if (cImagePath == NULL || ! g_file_test (cImagePath, G_FILE_TEST_EXISTS))
								{
									g_free (cImagePath);
									cImagePath = g_strdup (GLDI_SHARE_DATA_DIR"/icons/"CAIRO_DOCK_DEFAULT_ICON_NAME);
								}
							}
						}
						//g_print (" + %s\n", cImagePath);
						if (cImagePath != NULL)
						{
							pixbuf = gdk_pixbuf_new_from_file_at_size (cImagePath, iDesiredIconSize, iDesiredIconSize, NULL);
						}
						//g_print (" -> %p\n", pixbuf);
						
						// get the name
						if (CAIRO_DOCK_IS_USER_SEPARATOR (pIcon))  // separator
							cName = "---------";
						else if (CAIRO_DOCK_IS_APPLET (pIcon))  // applet
							cName = pIcon->pModuleInstance->pModule->pVisitCard->cTitle;
						else  // launcher
							cName = (pIcon->cInitialName ? pIcon->cInitialName : pIcon->cName);
						
						// store the icon
						memset (&iter, 0, sizeof (GtkTreeIter));
						gtk_list_store_append (GTK_LIST_STORE (modele), &iter);
						gtk_list_store_set (GTK_LIST_STORE (modele), &iter,
							CAIRO_DOCK_MODEL_NAME, cName,
							CAIRO_DOCK_MODEL_RESULT, cID,
							CAIRO_DOCK_MODEL_ICON, pixbuf, -1);
						g_free (cImagePath);
						if (pixbuf)
							g_object_unref (pixbuf);
						
						if (cValue && strcmp (cValue, cID) == 0)
							gtk_combo_box_set_active_iter (GTK_COMBO_BOX (pOneWidget), &iter);
					}
				}
				g_free (cValue);
			}
			break ;
			
			case CAIRO_DOCK_WIDGET_SCREENS_LIST :
			{
				GHashTable *pHashTable = _cairo_dock_build_screens_list ();
				
				GtkListStore *pScreensListStore = _cairo_dock_build_screens_list_for_gui (pHashTable);
				
				_add_combo_from_modele (pScreensListStore, FALSE, FALSE, FALSE);
				
				g_object_unref (pScreensListStore);
				g_hash_table_destroy (pHashTable);
				
				gldi_object_register_notification (&myDesktopMgr,
					NOTIFICATION_DESKTOP_GEOMETRY_CHANGED,
					(GldiNotificationFunc) _on_screen_modified,
					GLDI_RUN_AFTER, pScreensListStore);
				g_signal_connect (pOneWidget, "destroy", G_CALLBACK (_on_list_destroyed), NULL);
				
				if (g_desktopGeometry.iNbScreens <= 1)
					gtk_widget_set_sensitive (pOneWidget, FALSE);
			}
			break ;
			
			case CAIRO_DOCK_WIDGET_JUMP_TO_MODULE :  // bouton raccourci vers un autre module
			case CAIRO_DOCK_WIDGET_JUMP_TO_MODULE_IF_EXISTS :  // idem mais seulement affiche si le module existe.
				if (pAuthorizedValuesList == NULL || pAuthorizedValuesList[0] == NULL || *pAuthorizedValuesList[0] == '\0')
					break ;
				
				gchar *cModuleName = NULL;
				GldiModule *pModule = gldi_module_get (pAuthorizedValuesList[0]);
				if (pModule != NULL)
					cModuleName = (gchar*)pModule->pVisitCard->cModuleName;  // 'cModuleName' will not be freed
				else
				{
					if (iElementType == CAIRO_DOCK_WIDGET_JUMP_TO_MODULE_IF_EXISTS)
					{
						gtk_widget_set_sensitive (pLabel, FALSE);
						break ;
					}
					cd_warning ("module '%s' not found", pAuthorizedValuesList[0]);
					cModuleName = g_strdup (pAuthorizedValuesList[0]);  // petite fuite memoire dans ce cas tres rare ...
				}
				pOneWidget = gtk_button_new_from_icon_name (GLDI_ICON_NAME_JUMP_TO, GTK_ICON_SIZE_BUTTON);
				g_signal_connect (G_OBJECT (pOneWidget),
					"clicked",
					G_CALLBACK (_cairo_dock_configure_module),
					cModuleName);
				_pack_subwidget (pOneWidget);
			break ;
			
			case CAIRO_DOCK_WIDGET_LAUNCH_COMMAND :
			case CAIRO_DOCK_WIDGET_LAUNCH_COMMAND_IF_CONDITION :
				if (pAuthorizedValuesList == NULL || pAuthorizedValuesList[0] == NULL || *pAuthorizedValuesList[0] == '\0')
					break ;
				gchar *cFirstCommand = NULL;
				cFirstCommand = pAuthorizedValuesList[0];
				if (iElementType == CAIRO_DOCK_WIDGET_LAUNCH_COMMAND_IF_CONDITION)
				{
					if (pAuthorizedValuesList[1] == NULL)
					{ // condition without condition...
						gtk_widget_set_sensitive (pLabel, FALSE);
						break ;
					}
					gchar *cSecondCommand = pAuthorizedValuesList[1];
					gchar *cResult = cairo_dock_launch_command_sync (cSecondCommand);
					cd_debug ("%s: %s => %s", __func__, cSecondCommand, cResult);
					if (cResult == NULL || *cResult == '0' || *cResult == '\0')  // result is 'fail'
					{
						gtk_widget_set_sensitive (pLabel, FALSE);
						g_free (cResult);
						break ;
					}
					g_free (cResult);
				}
				pOneWidget = gtk_button_new_from_icon_name (GLDI_ICON_NAME_JUMP_TO, GTK_ICON_SIZE_BUTTON);
				g_signal_connect (G_OBJECT (pOneWidget),
					"clicked",
					G_CALLBACK (_cairo_dock_widget_launch_command),
					g_strdup (cFirstCommand));
				_pack_subwidget (pOneWidget);
			break ;
			
			case CAIRO_DOCK_WIDGET_LIST :  // a list of strings.
			case CAIRO_DOCK_WIDGET_NUMBERED_LIST :  // a list of numbered strings.
			case CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST :  // a list of numbered strings whose current choice defines the sensitivity of the widgets below.
			case CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST_SELECTIVE :  // a list of numbered strings whose current choice defines the sensitivity of the widgets below given in the list.
			case CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY :  // a list of strings with possibility to select a non-existing one.
				if ((iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST || iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST_SELECTIVE) && pAuthorizedValuesList == NULL)
				{
					break;
				}
				cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);  // nous permet de recuperer les ';' aussi.
				// on construit la combo.
				pOneWidget = cairo_dock_gui_make_combo (iElementType == CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY);
				modele = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (pOneWidget)));
				gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (modele), GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
				
				int iNonSensitiveWidget = 0;
				// on la remplit.
				if (pAuthorizedValuesList != NULL)
				{
					k = 0;
					int iSelectedItem = -1, iOrder1, iOrder2, iExcept;
					gboolean bNumberedList = (iElementType == CAIRO_DOCK_WIDGET_NUMBERED_LIST || iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST || iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST_SELECTIVE);
					if (bNumberedList)
						iSelectedItem = atoi (cValue);
					gchar *cResult = (bNumberedList ? g_new0 (gchar , 10) : NULL);
					int dk = (iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST_SELECTIVE ? 3 : 1);
					if (iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST || iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST_SELECTIVE)
						iNbControlledWidgets = 0;
					for (k = 0; pAuthorizedValuesList[k] != NULL; k += dk)  // on ajoute toutes les chaines possibles a la combo.
					{
						GtkTreeIter iter;
						gtk_list_store_append (GTK_LIST_STORE (modele), &iter);
						if (iSelectedItem == -1 && cValue && strcmp (cValue, pAuthorizedValuesList[k]) == 0)
							iSelectedItem = k/dk;
						
						if (cResult != NULL)
							snprintf (cResult, 9, "%d", k/dk);
						
						iExcept = 0;
						if (iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST_SELECTIVE)
						{
							iOrder1 = atoi (pAuthorizedValuesList[k+1]);
							gchar *str = strchr (pAuthorizedValuesList[k+2], ',');
							if (str)  // Note: this mechanism is an addition to the original {first widget, number of widgets}; it's not very generic nor beautiful, but until we need more, it's well enough (currently, only the Dock background needs it).
							{
								*str = '\0';
								iExcept = atoi (str+1);
							}
							iOrder2 = atoi (pAuthorizedValuesList[k+2]);
							iNbControlledWidgets = MAX (iNbControlledWidgets, iOrder1 + iOrder2 - 1);
							//g_print ("iSelectedItem:%d ; k/dk:%d\n", iSelectedItem , k/dk);
							if (iSelectedItem == (int)k/dk)
							{
								iFirstSensitiveWidget = iOrder1;
								iNbSensitiveWidgets = iOrder2;
								iNonSensitiveWidget = iExcept;
								if (iNonSensitiveWidget != 0)
									iNbControlledWidgets ++;
							}
						}
						else
						{
							iOrder1 = iOrder2 = k;
						}
						gtk_list_store_set (GTK_LIST_STORE (modele), &iter,
							CAIRO_DOCK_MODEL_NAME, (iElementType != CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY ? dgettext (cGettextDomain, pAuthorizedValuesList[k]) : pAuthorizedValuesList[k]),
							CAIRO_DOCK_MODEL_RESULT, (cResult != NULL ? cResult : pAuthorizedValuesList[k]),
							CAIRO_DOCK_MODEL_ORDER, iOrder1,
							CAIRO_DOCK_MODEL_ORDER2, iOrder2,
							CAIRO_DOCK_MODEL_STATE, iExcept, -1);
					}
					g_free (cResult);
					
					// on active l'element courant.
					if (iElementType != CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY)
					{
						if (iSelectedItem == -1)  // si le choix courant n'etait pas dans la liste, on decide de selectionner le 1er.
							iSelectedItem = 0;
						if (k != 0)  // rien dans le gtktree => plantage.
							gtk_combo_box_set_active (GTK_COMBO_BOX (pOneWidget), iSelectedItem);
					}
					else
					{
						if (iSelectedItem == -1)
						{
							GtkWidget *e = gtk_bin_get_child (GTK_BIN (pOneWidget));
							gtk_entry_set_text (GTK_ENTRY (e), cValue);
						}
						else
							gtk_combo_box_set_active (GTK_COMBO_BOX (pOneWidget), iSelectedItem);
					}
					if (iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST || iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST_SELECTIVE)
					{
						_allocate_new_buffer;
						data[0] = pKeyBox;
						data[1] = (pFrameVBox != NULL ? pFrameVBox : pGroupBox);
						if (iElementType == CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST)
						{
							iNbControlledWidgets = k;
							data[2] = GINT_TO_POINTER (iNbControlledWidgets);
							g_signal_connect (G_OBJECT (pOneWidget), "changed", G_CALLBACK (_cairo_dock_select_one_item_in_control_combo), data);
							iFirstSensitiveWidget = iSelectedItem+1;  // on decroit jusqu'a 0.
							iNbSensitiveWidgets = 1;
							//g_print ("CONTROL : %d,%d,%d\n", iNbControlledWidgets, iFirstSensitiveWidget, iNbSensitiveWidgets);
						}
						else
						{
							data[2] = GINT_TO_POINTER (iNbControlledWidgets);
							g_signal_connect (G_OBJECT (pOneWidget), "changed", G_CALLBACK (_cairo_dock_select_one_item_in_control_combo_selective), data);
							//g_print ("CONTROL : %d,%d,%d\n", iNbControlledWidgets, iFirstSensitiveWidget, iNbSensitiveWidgets);
						}
						g_object_set_data (G_OBJECT (pKeyBox), "nb-ctrl-widgets", GINT_TO_POINTER (iNbControlledWidgets));
						g_object_set_data (G_OBJECT (pKeyBox), "one-widget", pOneWidget);
						CDControlWidget *cw = g_new0 (CDControlWidget, 1);
						pControlWidgets = g_list_prepend (pControlWidgets, cw);
						cw->pControlContainer = (pFrameVBox != NULL ? pFrameVBox : pGroupBox);
						cw->iNbControlledWidgets = iNbControlledWidgets;
						cw->iFirstSensitiveWidget = iFirstSensitiveWidget;
						cw->iNbSensitiveWidgets = iNbSensitiveWidgets;
						cw->iNonSensitiveWidget = iNonSensitiveWidget;
						//g_print (" pControlContainer:%x\n", pControlContainer);
					}
				}
				_pack_subwidget (pOneWidget);
				g_free (cValue);
			break ;
			
			case CAIRO_DOCK_WIDGET_TREE_VIEW_SORT :  // N strings listed from top to bottom.
			case CAIRO_DOCK_WIDGET_TREE_VIEW_SORT_AND_MODIFY :  // same with possibility to add/remove some.
			case CAIRO_DOCK_WIDGET_TREE_VIEW_MULTI_CHOICE :  // N strings that can be selected or not.
				// on construit le tree view.
				cValueList = g_key_file_get_string_list (pKeyFile, cGroupName, cKeyName, &length, NULL);
				pOneWidget = gtk_tree_view_new ();
				modele = _cairo_dock_gui_allocate_new_model ();
				gtk_tree_view_set_model (GTK_TREE_VIEW (pOneWidget), GTK_TREE_MODEL (modele));
				gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (modele), CAIRO_DOCK_MODEL_ORDER, GTK_SORT_ASCENDING);
				gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (pOneWidget), FALSE);
				
				if (iElementType == CAIRO_DOCK_WIDGET_TREE_VIEW_MULTI_CHOICE)
				{
					rend = gtk_cell_renderer_toggle_new ();
					gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "active", CAIRO_DOCK_MODEL_ACTIVE, NULL);
					g_signal_connect (G_OBJECT (rend), "toggled", (GCallback) _cairo_dock_activate_one_element, modele);
				}
				
				rend = gtk_cell_renderer_text_new ();
				gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
				selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pOneWidget));
				gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
				
				pSubWidgetList = g_slist_append (pSubWidgetList, pOneWidget);
				
				pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
				//g_print ("length:%d\n", length);
				
				if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[0] != NULL)
					for (k = 0; pAuthorizedValuesList[k] != NULL; k++);
				else
					k = 1;
				g_object_set (pScrolledWindow, "height-request", (iElementType == CAIRO_DOCK_WIDGET_TREE_VIEW_SORT_AND_MODIFY ? 100 : MIN (100, k * 25)), NULL);
				gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
				#if GTK_CHECK_VERSION (3, 8, 0)
				gtk_container_add (GTK_CONTAINER (pScrolledWindow), pOneWidget);
				#else
				gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pOneWidget);
				#endif
				_pack_in_widget_box (pScrolledWindow);
				
				if (iElementType != CAIRO_DOCK_WIDGET_TREE_VIEW_MULTI_CHOICE)
				{
					pSmallVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN);
					_pack_in_widget_box (pSmallVBox);
					
					pButtonUp = gtk_button_new_from_icon_name (GLDI_ICON_NAME_GO_UP, GTK_ICON_SIZE_BUTTON);
					g_signal_connect (G_OBJECT (pButtonUp),
						"clicked",
						G_CALLBACK (_cairo_dock_go_up),
						pOneWidget);
					gtk_box_pack_start (GTK_BOX (pSmallVBox),
						pButtonUp,
						FALSE,
						FALSE,
						0);
					
					pButtonDown = gtk_button_new_from_icon_name (GLDI_ICON_NAME_GO_DOWN, GTK_ICON_SIZE_BUTTON);
					g_signal_connect (G_OBJECT (pButtonDown),
						"clicked",
						G_CALLBACK (_cairo_dock_go_down),
						pOneWidget);
					gtk_box_pack_start (GTK_BOX (pSmallVBox),
						pButtonDown,
						FALSE,
						FALSE,
						0);
				}
				
				if (iElementType == CAIRO_DOCK_WIDGET_TREE_VIEW_SORT_AND_MODIFY)
				{
					pTable = gtk_grid_new ();
					_pack_in_widget_box (pTable);
						
					_allocate_new_buffer;
					
					pButtonAdd = gtk_button_new_from_icon_name (GLDI_ICON_NAME_ADD, GTK_ICON_SIZE_BUTTON);
					g_signal_connect (G_OBJECT (pButtonAdd),
						"clicked",
						G_CALLBACK (_cairo_dock_add),
						data);
					pButtonRemove = gtk_button_new_from_icon_name (GLDI_ICON_NAME_REMOVE, GTK_ICON_SIZE_BUTTON);
					g_signal_connect (G_OBJECT (pButtonRemove),
						"clicked",
						G_CALLBACK (_cairo_dock_remove),
						data);
					pEntry = gtk_entry_new ();

					gtk_grid_attach (GTK_GRID (pTable),
						pButtonAdd,
						0, 0, 1, 1);
					gtk_grid_attach_next_to (GTK_GRID (pTable),
						pButtonRemove,
						pButtonAdd, GTK_POS_BOTTOM, 1, 1);
					gtk_grid_attach_next_to (GTK_GRID (pTable),
						pEntry,
						pButtonAdd, GTK_POS_RIGHT, 1, 2);
					data[0] = pOneWidget;
					data[1] = pEntry;
				}
				
				// on le remplit.
				if (iElementType == CAIRO_DOCK_WIDGET_TREE_VIEW_SORT_AND_MODIFY)  // on liste les choix actuels tout simplement.
				{
					for (k = 0; k < length; k ++)
					{
						cValue = cValueList[k];
						if (cValue != NULL)  // paranoia.
						{
							memset (&iter, 0, sizeof (GtkTreeIter));
							gtk_list_store_append (modele, &iter);
							gtk_list_store_set (modele, &iter,
								CAIRO_DOCK_MODEL_ACTIVE, TRUE,
								CAIRO_DOCK_MODEL_NAME, cValue,
								CAIRO_DOCK_MODEL_RESULT, cValue,
								CAIRO_DOCK_MODEL_ORDER, k, -1);
						}
					}
				}
				else if (pAuthorizedValuesList != NULL)  // on liste les choix possibles dans l'ordre choisi. Pour CAIRO_DOCK_WIDGET_TREE_VIEW_MULTI_CHOICE, on complete avec ceux n'ayant pas ete selectionnes.
				{
					gint iNbPossibleValues = 0, iOrder = 0;
					while (pAuthorizedValuesList[iNbPossibleValues] != NULL)
						iNbPossibleValues ++;
					guint l;
					for (l = 0; l < length; l ++)
					{
						cValue = cValueList[l];
						if (! g_ascii_isdigit (*cValue))  // ancien format.
						{
							cd_debug ("old format\n");
							int k;
							for (k = 0; k < iNbPossibleValues; k ++)  // on cherche la correspondance.
							{
								if (strcmp (cValue, pAuthorizedValuesList[k]) == 0)
								{
									cd_debug (" correspondance %s <-> %d", cValue, k);
									g_free (cValueList[l]);
									cValueList[l] = g_strdup_printf ("%d", k);
									cValue = cValueList[l];
									break ;
								}
							}
							if (k < iNbPossibleValues)
								iValue = k;
							else
								continue;
						}
						else
							iValue = atoi (cValue);
						
						if (iValue < iNbPossibleValues)
						{
							memset (&iter, 0, sizeof (GtkTreeIter));
							gtk_list_store_append (modele, &iter);
							gtk_list_store_set (modele, &iter,
								CAIRO_DOCK_MODEL_ACTIVE, TRUE,
								CAIRO_DOCK_MODEL_NAME, dgettext (cGettextDomain, pAuthorizedValuesList[iValue]),
								CAIRO_DOCK_MODEL_RESULT, cValue,
								CAIRO_DOCK_MODEL_ORDER, iOrder ++, -1);
						}
					}
					
					if (iOrder < iNbPossibleValues)  // il reste des valeurs a inserer (ce peut etre de nouvelles valeurs apparues lors d'une maj du fichier de conf, donc CAIRO_DOCK_WIDGET_TREE_VIEW_SORT est concerne aussi). 
					{
						gchar cResult[10];
						for (k = 0; pAuthorizedValuesList[k] != NULL; k ++)
						{
							cValue =  pAuthorizedValuesList[k];
							for (l = 0; l < length; l ++)
							{
								iValue = atoi (cValueList[l]);
								if (iValue == (int)k)  // a deja ete inseree.
									break;
							}
							
							if (l == length)  // elle n'a pas encore ete inseree.
							{
								snprintf (cResult, 9, "%d", k);
								memset (&iter, 0, sizeof (GtkTreeIter));
								gtk_list_store_append (modele, &iter);
								gtk_list_store_set (modele, &iter,
									CAIRO_DOCK_MODEL_ACTIVE, (iElementType == CAIRO_DOCK_WIDGET_TREE_VIEW_SORT),
									CAIRO_DOCK_MODEL_NAME, dgettext (cGettextDomain, cValue),
									CAIRO_DOCK_MODEL_RESULT, cResult,
									CAIRO_DOCK_MODEL_ORDER, iOrder ++, -1);
							}
						}
					}
				}
			break ;
			
			case CAIRO_DOCK_WIDGET_FONT_SELECTOR :  // string avec un selecteur de font a cote du GtkEntry.
				cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
				pOneWidget = gtk_font_button_new_with_font (cValue);
				gtk_font_button_set_show_style (GTK_FONT_BUTTON (pOneWidget), TRUE);
				gtk_font_button_set_show_size (GTK_FONT_BUTTON (pOneWidget), TRUE);
				gtk_font_button_set_use_size (GTK_FONT_BUTTON (pOneWidget), TRUE);
				gtk_font_button_set_use_font (GTK_FONT_BUTTON (pOneWidget), TRUE);
				_pack_subwidget (pOneWidget);
				g_free (cValue);
			break;
			
			case CAIRO_DOCK_WIDGET_LINK :  // string avec un lien internet a cote.
				cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
				pOneWidget = gtk_link_button_new_with_label (cValue, pAuthorizedValuesList && pAuthorizedValuesList[0] ? pAuthorizedValuesList[0] : _("link"));
				_pack_subwidget (pOneWidget);
				g_free (cValue);
			break;
			
			
			case CAIRO_DOCK_WIDGET_STRING_ENTRY :  // string (Merci Fab !) :-P
			case CAIRO_DOCK_WIDGET_PASSWORD_ENTRY :  // string de type "password", crypte dans le .conf et cache dans l'UI (Merci Tofe !) :-)
			case CAIRO_DOCK_WIDGET_FILE_SELECTOR :  // string avec un selecteur de fichier a cote du GtkEntry.
			case CAIRO_DOCK_WIDGET_FOLDER_SELECTOR :  // string avec un selecteur de repertoire a cote du GtkEntry.
			case CAIRO_DOCK_WIDGET_SOUND_SELECTOR :  // string avec un selecteur de fichier a cote du GtkEntry et un boutton play.
			case CAIRO_DOCK_WIDGET_SHORTKEY_SELECTOR :  // string avec un selecteur de touche clavier (Merci Ctaf !)
			case CAIRO_DOCK_WIDGET_CLASS_SELECTOR :  // string avec un selecteur de class (Merci Matttbe !)
			case CAIRO_DOCK_WIDGET_IMAGE_SELECTOR :  // string with a file selector (results are filtered to only display images)
				// on construit l'entree de texte.
				cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
				pOneWidget = gtk_entry_new ();
				pEntry = pOneWidget;
				if( iElementType == CAIRO_DOCK_WIDGET_PASSWORD_ENTRY )  // on cache le texte entre et on decrypte 'cValue'.
				{
					gtk_entry_set_visibility (GTK_ENTRY (pOneWidget), FALSE);
					gchar *cDecryptedString = NULL;
					cairo_dock_decrypt_string ( cValue, &cDecryptedString );
					g_free (cValue);
					cValue = cDecryptedString;
				}
				gtk_entry_set_text (GTK_ENTRY (pOneWidget), cValue);
				_pack_subwidget (pOneWidget);
				
				// on ajoute des boutons qui la rempliront.
				if (iElementType == CAIRO_DOCK_WIDGET_FILE_SELECTOR ||
					iElementType == CAIRO_DOCK_WIDGET_FOLDER_SELECTOR ||
					iElementType == CAIRO_DOCK_WIDGET_SOUND_SELECTOR ||
					iElementType == CAIRO_DOCK_WIDGET_IMAGE_SELECTOR)  // we add a file selector
				{
					_allocate_new_buffer;
					data[0] = pEntry;
					if (iElementType == CAIRO_DOCK_WIDGET_FOLDER_SELECTOR)
						data[1] = GINT_TO_POINTER (1);
					else if (iElementType == CAIRO_DOCK_WIDGET_IMAGE_SELECTOR)
						data[1] = GINT_TO_POINTER (2);
					else
						data[1] = GINT_TO_POINTER (0);
					data[2] = GTK_WINDOW (pMainWindow);
					pButtonFileChooser = gtk_button_new_from_icon_name (GLDI_ICON_NAME_OPEN, GTK_ICON_SIZE_BUTTON);
					g_signal_connect (G_OBJECT (pButtonFileChooser),
						"clicked",
						G_CALLBACK (_cairo_dock_pick_a_file),
						data);
					_pack_in_widget_box (pButtonFileChooser);
					if (iElementType == CAIRO_DOCK_WIDGET_SOUND_SELECTOR) //Sound Play Button
					{
						pButtonPlay = gtk_button_new_from_icon_name (GLDI_ICON_NAME_MEDIA_PLAY, GTK_ICON_SIZE_BUTTON); //Outch
						g_signal_connect (G_OBJECT (pButtonPlay),
							"clicked",
							G_CALLBACK (_cairo_dock_play_a_sound),
							data);
						_pack_in_widget_box (pButtonPlay);
					}
				}
				else if (iElementType == CAIRO_DOCK_WIDGET_SHORTKEY_SELECTOR || iElementType == CAIRO_DOCK_WIDGET_CLASS_SELECTOR)  // on ajoute un selecteur de touches/classe.
				{
					GtkWidget *pGrabKeyButton = gtk_button_new_with_label(_("Grab"));
					_allocate_new_buffer;
					data[0] = pOneWidget;
					data[1] = pMainWindow;
					gtk_widget_add_events(pMainWindow, GDK_KEY_PRESS_MASK);
					if (iElementType == CAIRO_DOCK_WIDGET_CLASS_SELECTOR)
					{
						g_signal_connect (G_OBJECT (pGrabKeyButton),
							"clicked",
							G_CALLBACK (_cairo_dock_key_grab_class),
							data);
					}
					else
					{
						g_signal_connect (G_OBJECT (pGrabKeyButton),
							"clicked",
							G_CALLBACK (_cairo_dock_key_grab_clicked),
							data);
					}
					_pack_in_widget_box (pGrabKeyButton);
				}
				
				if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[0] != NULL)  // default displayed value when empty
				{
					gchar *cDefaultText = g_strdup (dgettext (cGettextDomain, pAuthorizedValuesList[0]));
					if (cValue == NULL || *cValue == '\0')  // currently the entry is empty.
					{
						gtk_entry_set_text (GTK_ENTRY (pOneWidget), cDefaultText);
						g_object_set_data (G_OBJECT (pOneWidget), "ignore-value", GINT_TO_POINTER (TRUE));

						GdkRGBA color;
						color.red = DEFAULT_TEXT_COLOR;
						color.green = DEFAULT_TEXT_COLOR;
						color.blue = DEFAULT_TEXT_COLOR;
						color.alpha = 1.;
						gtk_widget_override_color (pOneWidget, GTK_STATE_FLAG_NORMAL, &color);
					}
					g_signal_connect (pOneWidget, "changed", G_CALLBACK (_on_text_changed), cDefaultText);
					g_signal_connect (pOneWidget, "focus-in-event", G_CALLBACK (on_text_focus_in), cDefaultText);
					g_signal_connect (pOneWidget, "focus-out-event", G_CALLBACK (on_text_focus_out), cDefaultText);
					g_object_set_data (G_OBJECT (pOneWidget), "default-text", cDefaultText);
				}
				g_free (cValue);
			break;

			case CAIRO_DOCK_WIDGET_EMPTY_WIDGET :  // container pour widget personnalise.
			case CAIRO_DOCK_WIDGET_EMPTY_FULL :
				
			break ;
			
			case CAIRO_DOCK_WIDGET_TEXT_LABEL :  // juste le label de texte.
			{
				int iFrameWidth = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pMainWindow), "frame-width"));
				gtk_widget_set_size_request (pLabel, MIN (800, gldi_desktop_get_width() - iFrameWidth), -1);
				gtk_label_set_justify (GTK_LABEL (pLabel), GTK_JUSTIFY_LEFT);
				gtk_label_set_line_wrap (GTK_LABEL (pLabel), TRUE);
			}
			break ;
			
			case CAIRO_DOCK_WIDGET_HANDBOOK :
				/// TODO: if possible, build the handbook widget here rather than outside of the swith-case ...
			break ;
			
			case CAIRO_DOCK_WIDGET_FRAME :  // frame.
			case CAIRO_DOCK_WIDGET_EXPANDER :  // frame dans un expander.
				if (pAuthorizedValuesList == NULL)
				{
					pFrame = NULL;
					pFrameVBox = NULL;
				}
				else
				{
					if (pAuthorizedValuesList[0] == NULL || *pAuthorizedValuesList[0] == '\0')
						cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);  // utile ?
					else
					{
						cValue = pAuthorizedValuesList[0];
						cSmallIcon = pAuthorizedValuesList[1];
					}
					
					gchar *cFrameTitle = g_strdup_printf ("<b>%s</b>", dgettext (cGettextDomain, cValue));
					pLabel= gtk_label_new (NULL);
					gtk_label_set_markup (GTK_LABEL (pLabel), cFrameTitle);
					g_free (cFrameTitle);
					
					pLabelContainer = NULL;
					if (cSmallIcon != NULL)
					{
						pLabelContainer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_ICON_MARGIN/2);
						GtkWidget *pImage = _gtk_image_new_from_file (cSmallIcon, GTK_ICON_SIZE_MENU);
						gtk_container_add (GTK_CONTAINER (pLabelContainer),
							pImage);
						
						gtk_container_add (GTK_CONTAINER (pLabelContainer),
							pLabel);
					}
					
					GtkWidget *pExternFrame;
					if (iElementType == CAIRO_DOCK_WIDGET_FRAME)
					{
						pExternFrame = gtk_frame_new (NULL);
						gtk_container_set_border_width (GTK_CONTAINER (pExternFrame), CAIRO_DOCK_GUI_MARGIN);
						gtk_frame_set_shadow_type (GTK_FRAME (pExternFrame), GTK_SHADOW_OUT);
						gtk_frame_set_label_widget (GTK_FRAME (pExternFrame), (pLabelContainer != NULL ? pLabelContainer : pLabel));
						pFrame = pExternFrame;
						//g_print ("on met pLabelContainer:%x (%x > %x)\n", pLabelContainer, gtk_frame_get_label_widget (GTK_FRAME (pFrame)), pLabel);
					}
					else
					{
						pExternFrame = gtk_expander_new (NULL);
						gtk_expander_set_expanded (GTK_EXPANDER (pExternFrame), FALSE);
						gtk_expander_set_label_widget (GTK_EXPANDER (pExternFrame), (pLabelContainer != NULL ? pLabelContainer : pLabel));
						pFrame = gtk_frame_new (NULL);
						gtk_container_set_border_width (GTK_CONTAINER (pFrame), CAIRO_DOCK_GUI_MARGIN);
						gtk_frame_set_shadow_type (GTK_FRAME (pFrame), GTK_SHADOW_OUT);
						gtk_container_add (GTK_CONTAINER (pExternFrame),
							pFrame);
					}
					//pSubWidgetList = g_slist_append (pSubWidgetList, pExternFrame);
					
					gtk_box_pack_start (GTK_BOX (pGroupBox),
						pExternFrame,
						FALSE,
						FALSE,
						0);

					pFrameVBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, CAIRO_DOCK_GUI_MARGIN);
					gtk_container_add (GTK_CONTAINER (pFrame),
						pFrameVBox);
					
					if (pAuthorizedValuesList[0] == NULL || *pAuthorizedValuesList[0] == '\0')
						g_free (cValue);
					
					
					if (pControlWidgets != NULL)
					{
						cd_debug ("ctrl\n");
						CDControlWidget *cw = pControlWidgets->data;
						if (cw->pControlContainer == pGroupBox)
						{
							cd_debug ("ctrl (iNbControlledWidgets:%d, iFirstSensitiveWidget:%d, iNbSensitiveWidgets:%d)", cw->iNbControlledWidgets, cw->iFirstSensitiveWidget, cw->iNbSensitiveWidgets);
							cw->iNbControlledWidgets --;
							if (cw->iFirstSensitiveWidget > 0)
								cw->iFirstSensitiveWidget --;
							cw->iNonSensitiveWidget --;
							
							GtkWidget *w = pExternFrame;
							if (cw->iFirstSensitiveWidget == 0 && cw->iNbSensitiveWidgets > 0 && cw->iNonSensitiveWidget != 0)
							{
								cd_debug (" => sensitive\n");
								cw->iNbSensitiveWidgets --;
								if (GTK_IS_EXPANDER (w))
									gtk_expander_set_expanded (GTK_EXPANDER (w), TRUE);
							}
							else
							{
								cd_debug (" => unsensitive\n");
								if (!GTK_IS_EXPANDER (w))
									gtk_widget_set_sensitive (w, FALSE);
							}
							if (cw->iFirstSensitiveWidget == 0 && cw->iNbControlledWidgets == 0)
							{
								pControlWidgets = g_list_delete_link (pControlWidgets, pControlWidgets);
								g_free (cw);
							}
						}
					}
				}
				break;

			case CAIRO_DOCK_WIDGET_SEPARATOR :  // separateur.
			{
				GtkWidget *pAlign = gtk_alignment_new (.5, .5, 0.8, 1.);
				g_object_set (pAlign, "height-request", 12, NULL);
				pOneWidget = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
				gtk_container_add (GTK_CONTAINER (pAlign), pOneWidget);
				gtk_box_pack_start (GTK_BOX (pFrameVBox != NULL ? pFrameVBox : pGroupBox),
					pAlign,
					FALSE,
					FALSE,
					0);
			}
			break ;

			default :
				cd_warning ("this conf file has an unknown widget type ! (%c)", iElementType);
				bInsert = FALSE;
			break ;
		}
		
		if (bInsert)  // donc pSubWidgetList peut etre NULL
		{
			pGroupKeyWidget = g_new0 (CairoDockGroupKeyWidget, 1);
			pGroupKeyWidget->cGroupName = g_strdup (cGroupName);  // car on ne pourra pas le liberer s'il est partage entre plusieurs 'data'.
			pGroupKeyWidget->cKeyName = cKeyName;
			pGroupKeyWidget->pSubWidgetList = pSubWidgetList;
			pGroupKeyWidget->cOriginalConfFilePath = (gchar *)cOriginalConfFilePath;
			pGroupKeyWidget->pLabel = pLabel;
			pGroupKeyWidget->pKeyBox = pKeyBox;  // on ne peut pas remonter a la keybox a partir du label a cause du cas particulier des widgets a prevue.
			*pWidgetList = g_slist_prepend (*pWidgetList, pGroupKeyWidget);
			if (bAddBackButton && cOriginalConfFilePath != NULL)
			{
				pBackButton = gtk_button_new ();
				GtkWidget *pImage = gtk_image_new_from_icon_name (GLDI_ICON_NAME_CLEAR, GTK_ICON_SIZE_MENU);
				gtk_button_set_image (GTK_BUTTON (pBackButton), pImage);
				g_signal_connect (G_OBJECT (pBackButton), "clicked", G_CALLBACK(_cairo_dock_set_original_value), pGroupKeyWidget);
				_pack_in_widget_box (pBackButton);
			}
		}
		else
			g_free (cKeyName);
		
		if (pAuthorizedValuesList != NULL)
			g_strfreev (pAuthorizedValuesList);
		g_free (cKeyComment);
	}
	g_free (pKeyList);  // les chaines a l'interieur sont dans les group-key widgets.
	
	if (pControlWidgets != NULL)
		cd_warning ("this conf file has an invalid combo list somewhere !");
	
	return pGroupBox;
}


GtkWidget *cairo_dock_build_key_file_widget_full (GKeyFile* pKeyFile, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath, GtkWidget *pCurrentNoteBook)
{
	gsize length = 0;
	gchar **pGroupList = g_key_file_get_groups (pKeyFile, &length);
	g_return_val_if_fail (pGroupList != NULL, NULL);
	
	GtkWidget *pNoteBook = pCurrentNoteBook;
	if (! pNoteBook)
	{
		pNoteBook = gtk_notebook_new ();
		gtk_notebook_set_scrollable (GTK_NOTEBOOK (pNoteBook), TRUE);
		gtk_notebook_popup_enable (GTK_NOTEBOOK (pNoteBook));
		g_object_set (G_OBJECT (pNoteBook), "tab-pos", GTK_POS_TOP, NULL);
	}
	
	GtkWidget *pGroupWidget, *pLabel, *pLabelContainer, *pAlign;
	gchar *cGroupName, *cGroupComment, *cIcon, *cDisplayedGroupName;
	int i;
	for (i = 0; pGroupList[i] != NULL; i++)
	{
		cGroupName = pGroupList[i];
		
		//\____________ On recupere les caracteristiques du groupe.
		cGroupComment = g_key_file_get_comment (pKeyFile, cGroupName, NULL, NULL);
		cIcon = NULL;
		cDisplayedGroupName = NULL;
		if (cGroupComment != NULL && *cGroupComment != '\0')  // extract the icon name/path, inside brackets [].
		{
			gchar *str = strrchr (cGroupComment, '[');
			if (str != NULL)
			{
				cIcon = str+1;
				str = strrchr (cIcon, ']');
				if (str != NULL)
					*str = '\0';
				str = strrchr (cIcon, ';');
				if (str != NULL)
				{
					*str = '\0';
					cDisplayedGroupName = str + 1;
				}
			}
		}
		
		//\____________ On construit son widget.
		pLabel = gtk_label_new (dgettext (cGettextDomain, cDisplayedGroupName ? cDisplayedGroupName : cGroupName));
		pLabelContainer = NULL;
		pAlign = NULL;
		if (cIcon != NULL)
		{
			pLabelContainer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, CAIRO_DOCK_ICON_MARGIN);
			pAlign = gtk_alignment_new (0., 0.5, 0., 0.);
			gtk_container_add (GTK_CONTAINER (pAlign), pLabelContainer);

			GtkWidget *pImage = _gtk_image_new_from_file (cIcon, GTK_ICON_SIZE_BUTTON);
			gtk_container_add (GTK_CONTAINER (pLabelContainer),
				pImage);
			gtk_container_add (GTK_CONTAINER (pLabelContainer), pLabel);
			gtk_widget_show_all (pLabelContainer);
		}
		g_free (cGroupComment);
		
		pGroupWidget = cairo_dock_build_group_widget (pKeyFile, cGroupName, cGettextDomain, pMainWindow, pWidgetList, pDataGarbage, cOriginalConfFilePath);
		
		GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		#if GTK_CHECK_VERSION (3, 8, 0)
		gtk_container_add (GTK_CONTAINER (pScrolledWindow), pGroupWidget);
		#else
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pGroupWidget);
		#endif
		
		gtk_notebook_append_page (GTK_NOTEBOOK (pNoteBook), pScrolledWindow, (pAlign != NULL ? pAlign : pLabel));
	}
	
	g_strfreev (pGroupList);
	return pNoteBook;
}

GtkWidget *cairo_dock_build_conf_file_widget (const gchar *cConfFilePath, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath)
{
	//\_____________ On recupere les groupes du fichier.
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	if (pKeyFile == NULL)
		return NULL;
	
	//\_____________ On construit le widget.
	GtkWidget *pNoteBook = cairo_dock_build_key_file_widget (pKeyFile, cGettextDomain, pMainWindow, pWidgetList, pDataGarbage, cOriginalConfFilePath);

	g_key_file_free (pKeyFile);
	return pNoteBook;
}


static void _cairo_dock_get_each_widget_value (CairoDockGroupKeyWidget *pGroupKeyWidget, GKeyFile *pKeyFile)
{
	gchar *cGroupName = pGroupKeyWidget->cGroupName;
	gchar *cKeyName = pGroupKeyWidget->cKeyName;
	GSList *pSubWidgetList = pGroupKeyWidget->pSubWidgetList;
	if (pSubWidgetList == NULL)
		return ;
	GSList *pList;
	gsize i = 0, iNbElements = g_slist_length (pSubWidgetList);
	GtkWidget *pOneWidget = pSubWidgetList->data;

	if (GTK_IS_CHECK_BUTTON (pOneWidget))
	{
		gboolean *tBooleanValues = g_new0 (gboolean, iNbElements);
		for (pList = pSubWidgetList; pList != NULL; pList = pList->next)
		{
			pOneWidget = pList->data;
			tBooleanValues[i] = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pOneWidget));
			i ++;
		}
		if (iNbElements > 1)
			g_key_file_set_boolean_list (pKeyFile, cGroupName, cKeyName, tBooleanValues, iNbElements);
		else
			g_key_file_set_boolean (pKeyFile, cGroupName, cKeyName, tBooleanValues[0]);
		g_free (tBooleanValues);
	}
	else if (GTK_IS_SPIN_BUTTON (pOneWidget) || GTK_IS_SCALE (pOneWidget))
	{
		gboolean bIsSpin = GTK_IS_SPIN_BUTTON (pOneWidget);
		
		if ((bIsSpin && gtk_spin_button_get_digits (GTK_SPIN_BUTTON (pOneWidget)) == 0) || (! bIsSpin && gtk_scale_get_digits (GTK_SCALE (pOneWidget)) == 0))
		{
			int *tIntegerValues = g_new0 (int, iNbElements);
			for (pList = pSubWidgetList; pList != NULL; pList = pList->next)
			{
				pOneWidget = pList->data;
				tIntegerValues[i] = (bIsSpin ? gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pOneWidget)) : gtk_range_get_value (GTK_RANGE (pOneWidget)));
				i ++;
			}
			if (iNbElements > 1)
				g_key_file_set_integer_list (pKeyFile, cGroupName, cKeyName, tIntegerValues, iNbElements);
			else
				g_key_file_set_integer (pKeyFile, cGroupName, cKeyName, tIntegerValues[0]);
			g_free (tIntegerValues);
		}
		else
		{
			double *tDoubleValues = g_new0 (double, iNbElements);
			for (pList = pSubWidgetList; pList != NULL; pList = pList->next)
			{
				pOneWidget = pList->data;
				tDoubleValues[i] = (bIsSpin ? gtk_spin_button_get_value (GTK_SPIN_BUTTON (pOneWidget)) : gtk_range_get_value (GTK_RANGE (pOneWidget)));
				i ++;
			}
			if (iNbElements > 1)
				g_key_file_set_double_list (pKeyFile, cGroupName, cKeyName, tDoubleValues, iNbElements);
			else
				g_key_file_set_double (pKeyFile, cGroupName, cKeyName, tDoubleValues[0]);
			g_free (tDoubleValues);
		}
	}
	else if (GTK_IS_COMBO_BOX (pOneWidget))
	{
		gchar **tValues = g_new0 (gchar*, iNbElements+1);
		gchar *cValue;
		for (pList = pSubWidgetList; pList != NULL; pList = pList->next)
		{
			pOneWidget = pList->data;
			cValue = _cairo_dock_gui_get_active_row_in_combo (pOneWidget);
			tValues[i] = (cValue ? cValue : g_strdup(""));
			i ++;
		}
		if (iNbElements > 1)
			g_key_file_set_string_list (pKeyFile, cGroupName, cKeyName, (const gchar * const *)tValues, iNbElements);
		else
			g_key_file_set_string (pKeyFile, cGroupName, cKeyName, tValues[0]);
		g_strfreev (tValues);
	}
	else if (GTK_IS_FONT_BUTTON (pOneWidget))
	{
		const gchar *cFontName = gtk_font_button_get_font_name (GTK_FONT_BUTTON (pOneWidget));
		g_key_file_set_string (pKeyFile, cGroupName, cKeyName, cFontName);
	}
	else if (GTK_IS_COLOR_BUTTON (pOneWidget))
	{
		double col[4];
		int iNbColors;

		GdkRGBA gdkColor;
		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (pOneWidget), &gdkColor);
		iNbColors = 4;
		col[0] = gdkColor.red;
		col[1] = gdkColor.green;
		col[2] = gdkColor.blue;
		col[3] = gdkColor.alpha;

		g_key_file_set_double_list (pKeyFile, cGroupName, cKeyName, col, iNbColors);
	}
	else if (GTK_IS_ENTRY (pOneWidget))
	{
		gchar *cValue = NULL;
		if (g_object_get_data (G_OBJECT (pOneWidget), "ignore-value") == NULL)
		{
			const gchar *cWidgetValue = gtk_entry_get_text (GTK_ENTRY (pOneWidget));
			if( !gtk_entry_get_visibility(GTK_ENTRY (pOneWidget)) )
			{
				cairo_dock_encrypt_string( cWidgetValue,  &cValue );
			}
			else
			{
				cValue = g_strdup (cWidgetValue);
			}
		}
		g_key_file_set_string (pKeyFile, cGroupName, cKeyName, cValue?cValue:"");

		g_free( cValue );
	}
	else if (GTK_IS_TREE_VIEW (pOneWidget))
	{
		gboolean bGetActiveOnly = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pOneWidget), "get-selected-line-only"));
		gchar **tStringValues = cairo_dock_gui_get_active_rows_in_tree_view (pOneWidget, bGetActiveOnly, &iNbElements);
		if (iNbElements > 1)
			g_key_file_set_string_list (pKeyFile, cGroupName, cKeyName, (const gchar * const *)tStringValues, iNbElements);
		else
			g_key_file_set_string (pKeyFile, cGroupName, cKeyName, (tStringValues[0] != NULL ? tStringValues[0] : ""));
		g_strfreev (tStringValues);
	}
}
void cairo_dock_update_keyfile_from_widget_list (GKeyFile *pKeyFile, GSList *pWidgetList)
{
	g_slist_foreach (pWidgetList, (GFunc) _cairo_dock_get_each_widget_value, pKeyFile);
}



static void _cairo_dock_free_group_key_widget (CairoDockGroupKeyWidget *pGroupKeyWidget, G_GNUC_UNUSED gpointer user_data)
{
        cd_debug ("");
        g_free (pGroupKeyWidget->cKeyName);
        g_free (pGroupKeyWidget->cGroupName);
        g_slist_free (pGroupKeyWidget->pSubWidgetList);  // les elements de pSubWidgetList sont les widgets, et se feront liberer lors de la destruction de la fenetre.
        // cOriginalConfFilePath n'est pas duplique.
        g_free (pGroupKeyWidget);
}
void cairo_dock_free_generated_widget_list (GSList *pWidgetList)
{
        g_slist_foreach (pWidgetList, (GFunc) _cairo_dock_free_group_key_widget, NULL);
        g_slist_free (pWidgetList);
}


  ///////////////
 // utilities //
///////////////

static void _fill_model_with_one_theme (const gchar *cThemeName, CairoDockPackage *pTheme, gpointer *data)
{
	GtkListStore *pModele = data[0];
	gchar *cHint = data[1];
	if ( ! cHint  // no hint is specified => take all themes
		|| ! pTheme->cHint  // the theme has no hint => it's a generic theme, take it
		|| strcmp (cHint, pTheme->cHint) == 0 )  // hints match, take it
	{
		GtkTreeIter iter;
		memset (&iter, 0, sizeof (GtkTreeIter));
		gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
		gchar *cReadmePath = g_strdup_printf ("%s/readme", pTheme->cPackagePath);
		gchar *cPreviewPath = g_strdup_printf ("%s/preview", pTheme->cPackagePath);
		gchar *cResult = g_strdup_printf ("%s[%d]", cThemeName, pTheme->iType);

		GdkPixbuf *pixbuf = _cairo_dock_gui_get_package_state_icon (pTheme->iType);
		gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
			CAIRO_DOCK_MODEL_NAME, pTheme->cDisplayedName,
			CAIRO_DOCK_MODEL_RESULT, cResult,
			CAIRO_DOCK_MODEL_DESCRIPTION_FILE, cReadmePath,
			CAIRO_DOCK_MODEL_IMAGE, cPreviewPath, 
			CAIRO_DOCK_MODEL_ORDER, pTheme->iRating,
			CAIRO_DOCK_MODEL_ORDER2, pTheme->iSobriety,
			CAIRO_DOCK_MODEL_STATE, pTheme->iType,
			CAIRO_DOCK_MODEL_SIZE, pTheme->fSize,
			CAIRO_DOCK_MODEL_ICON, pixbuf,
			CAIRO_DOCK_MODEL_AUTHOR, pTheme->cAuthor, -1);
		g_free (cReadmePath);
		g_free (cPreviewPath);
		g_free (cResult);
		g_object_unref (pixbuf);
	}
}
void cairo_dock_fill_model_with_themes (GtkListStore *pModel, GHashTable *pThemeTable, const gchar *cHint)
{
	gconstpointer data[2];
	data[0] = pModel;
	data[1] = cHint;
	g_hash_table_foreach (pThemeTable, (GHFunc)_fill_model_with_one_theme, data);
}


void cairo_dock_fill_combo_with_list (GtkWidget *pCombo, GList *pElementList, const gchar *cActiveElement)
{
	GtkTreeModel *pModele = gtk_combo_box_get_model (GTK_COMBO_BOX (pCombo));
	g_return_if_fail (pModele != NULL);
	
	GtkTreeIter iter;
	GList *l;
	for (l = pElementList; l != NULL; l = l->next)
	{
		gchar *cElement = l->data;
		memset (&iter, 0, sizeof (GtkTreeIter));
		gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
		gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
			CAIRO_DOCK_MODEL_NAME, cElement,
			CAIRO_DOCK_MODEL_RESULT, cElement,
			CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
			CAIRO_DOCK_MODEL_IMAGE, "none", -1);
	}
	
	if (cActiveElement != NULL && _cairo_dock_find_iter_from_name (GTK_LIST_STORE (pModele), cActiveElement, &iter))
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (pCombo), &iter);
}


GtkWidget *cairo_dock_gui_make_tree_view (gboolean bGetActiveOnly)
{
	GtkListStore *modele = _cairo_dock_gui_allocate_new_model ();
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (modele), CAIRO_DOCK_MODEL_NAME, GTK_SORT_ASCENDING);
	GtkWidget *pOneWidget = gtk_tree_view_new ();
	gtk_tree_view_set_model (GTK_TREE_VIEW (pOneWidget), GTK_TREE_MODEL (modele));
	
	if (bGetActiveOnly)  // le resultat sera la ligne courante selectionnee (NULL si aucune).
	{
		GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pOneWidget));
		gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
		g_object_set_data (G_OBJECT (pOneWidget), "get-selected-line-only", GINT_TO_POINTER (1));
	}  // else le resultat sera toutes les lignes cochees.
	return pOneWidget;
}

GtkWidget *cairo_dock_gui_make_combo (gboolean bWithEntry)
{
	GtkListStore *modele = _cairo_dock_gui_allocate_new_model ();
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (modele), CAIRO_DOCK_MODEL_NAME, GTK_SORT_ASCENDING);
	GtkWidget *pOneWidget;
	if (bWithEntry)
	{
		pOneWidget = _combo_box_entry_new_with_model (GTK_TREE_MODEL (modele), CAIRO_DOCK_MODEL_NAME);
	}
	else
	{
		pOneWidget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (modele));
		GtkCellRenderer *rend = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (pOneWidget), rend, FALSE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (pOneWidget), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
	}
	return pOneWidget;
}

void cairo_dock_gui_select_in_combo_full (GtkWidget *pOneWidget, const gchar *cValue, gboolean bIsTheme)
{
	GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (pOneWidget));
	g_return_if_fail (model != NULL);
	GtkTreeIter iter;
	if (_cairo_dock_find_iter_from_name_full (GTK_LIST_STORE (model), cValue, &iter, bIsTheme))
	{
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (pOneWidget), &iter);
	}
}

static gboolean _get_active_elements (GtkTreeModel * model, G_GNUC_UNUSED GtkTreePath * path, GtkTreeIter * iter, GSList **pStringList)
{
	//g_print ("%s ()\n", __func__);
	gboolean bActive;
	gchar *cValue = NULL, *cResult = NULL;
	gtk_tree_model_get (model, iter,
		CAIRO_DOCK_MODEL_ACTIVE, &bActive,
		CAIRO_DOCK_MODEL_NAME, &cValue,
		CAIRO_DOCK_MODEL_RESULT, &cResult, -1);
	if (cResult != NULL)
	{
		g_free (cValue);
		cValue = cResult;
	}
	
	if (bActive)
	{
		*pStringList = g_slist_append (*pStringList, cValue);
	}
	else
	{
		g_free (cValue);
	}
	return FALSE;
}
gchar **cairo_dock_gui_get_active_rows_in_tree_view (GtkWidget *pOneWidget, gboolean bSelectedRows, gsize *iNbElements)
{
	GtkTreeModel *pModel = gtk_tree_view_get_model (GTK_TREE_VIEW (pOneWidget));
	guint i = 0;
	gchar **tStringValues;
	
	if (bSelectedRows)
	{
		GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pOneWidget));
		GList *pRows = gtk_tree_selection_get_selected_rows (selection, NULL);
		tStringValues = g_new0 (gchar *, g_list_length (pRows) + 1);
		
		GList *r;
		GtkTreePath *cPath;
		for (r = pRows; r != NULL; r = r->next)
		{
			cPath = r->data;
			GtkTreeIter iter;
			if (! gtk_tree_model_get_iter (pModel, &iter, cPath))
				continue;
			
			gchar *cName = NULL;
			gtk_tree_model_get (pModel, &iter, CAIRO_DOCK_MODEL_RESULT, &cName, -1);
			tStringValues[i++] = cName;
		}
		*iNbElements = i;
	}
	else
	{
		GSList *pActiveElementList = NULL;
		gtk_tree_model_foreach (GTK_TREE_MODEL (pModel), (GtkTreeModelForeachFunc) _get_active_elements, &pActiveElementList);
		*iNbElements = g_slist_length (pActiveElementList);
		tStringValues = g_new0 (gchar *, *iNbElements + 1);
		
		GSList * pListElement;
		for (pListElement = pActiveElementList; pListElement != NULL; pListElement = pListElement->next)
		{
			//g_print (" %d) %s\n", i, pListElement->data);
			tStringValues[i++] = pListElement->data;
		}
		g_slist_free (pActiveElementList);  // ses donnees sont dans 'tStringValues' et seront donc liberees avec.
	}
	return tStringValues;
}

static gchar *_cairo_dock_gui_get_active_row_in_combo (GtkWidget *pOneWidget)
{
	gchar *cValue = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (pOneWidget));  // toutes nos combo sont crees avec un modele.
	if (model != NULL && gtk_combo_box_get_active_iter (GTK_COMBO_BOX (pOneWidget), &iter))
		gtk_tree_model_get (model, &iter, CAIRO_DOCK_MODEL_RESULT, &cValue, -1);
	if (cValue == NULL && GTK_IS_COMBO_BOX (pOneWidget) && gtk_combo_box_get_has_entry (GTK_COMBO_BOX (pOneWidget)))
	{
		GtkWidget *pEntry = gtk_bin_get_child (GTK_BIN (pOneWidget));
		cValue = g_strdup (gtk_entry_get_text (GTK_ENTRY (pEntry)));
	}
	return cValue;
}


static int _find_widget_from_name (gpointer *data, gpointer *pUserData)
{
	gchar *cWantedGroupName = pUserData[0];
	gchar *cWantedKeyName = pUserData[1];
	
	gchar *cGroupName = data[0];
	gchar *cKeyName = data[1];
	
	if (strcmp (cGroupName, cWantedGroupName) == 0 && strcmp (cKeyName, cWantedKeyName) == 0)
		return 0;
	else
		return 1;
}
CairoDockGroupKeyWidget *cairo_dock_gui_find_group_key_widget_in_list (GSList *pWidgetList, const gchar *cGroupName, const gchar *cKeyName)
{
	const gchar *data[2] = {cGroupName, cKeyName};
	GSList *pElement = g_slist_find_custom (pWidgetList, data, (GCompareFunc) _find_widget_from_name);
	if (pElement == NULL)
		return NULL;
	return pElement->data;
}


GtkWidget *_gtk_image_new_from_file (const gchar *cIcon, int iSize)
{
	g_return_val_if_fail (cIcon, NULL);
	GtkWidget *pImage = NULL;
	if (*cIcon != '/')  // named icon
	{
		pImage = gtk_image_new_from_icon_name (cIcon, iSize);
	}
	else  // path
	{
		iSize = cairo_dock_search_icon_size (iSize);
		pImage = gtk_image_new ();
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cIcon, iSize, iSize, NULL);
		if (pixbuf != NULL)
		{
			gtk_image_set_from_pixbuf (GTK_IMAGE (pImage), pixbuf);
			g_object_unref (pixbuf);
		}
	}
	return pImage;
}

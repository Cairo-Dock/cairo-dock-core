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

#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-applet-facility.h"  // play_sound
#include "cairo-dock-dialogs.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-gauge.h"
#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-gui-factory.h"

#define CAIRO_DOCK_GUI_MARGIN 4
#define CAIRO_DOCK_ICON_MARGIN 6
#define CAIRO_DOCK_PREVIEW_WIDTH 400
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_APPLET_ICON_SIZE 32
#define CAIRO_DOCK_TAB_ICON_SIZE 32
#define CAIRO_DOCK_FRAME_ICON_SIZE 24

extern CairoDock *g_pMainDock;
extern gchar *g_cCairoDockDataDir;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;

typedef enum {
	CAIRO_DOCK_MODEL_NAME = 0,
	CAIRO_DOCK_MODEL_RESULT,
	CAIRO_DOCK_MODEL_DESCRIPTION_FILE,
	CAIRO_DOCK_MODEL_ACTIVE,
	CAIRO_DOCK_MODEL_ORDER,
	CAIRO_DOCK_MODEL_IMAGE,
	CAIRO_DOCK_MODEL_ICON,
	CAIRO_DOCK_MODEL_ORDER2,
	CAIRO_DOCK_MODEL_NB_COLUMNS
	} _CairoDockModelColumns;

static GtkListStore *s_pRendererListStore = NULL;
static GtkListStore *s_pDecorationsListStore = NULL;
static GtkListStore *s_pDecorationsListStore2 = NULL;
static GtkListStore *s_pAnimationsListStore = NULL;
static GtkListStore *s_pDialogDecoratorListStore = NULL;
static GtkListStore *s_pGaugeListStore = NULL;
static GtkListStore *s_pDocksListStore = NULL;

#define _allocate_new_model(...)\
	gtk_list_store_new (CAIRO_DOCK_MODEL_NB_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_INT)

static void _cairo_dock_activate_one_element (GtkCellRendererToggle * cell_renderer, gchar * path, GtkTreeModel * model)
{
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter_from_string (model, &iter, path))
		return ;
	gboolean bState;
	gtk_tree_model_get (model, &iter, CAIRO_DOCK_MODEL_ACTIVE, &bState, -1);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, CAIRO_DOCK_MODEL_ACTIVE, !bState, -1);
}

static gboolean _cairo_dock_increase_order (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, int *pOrder)
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

static gboolean _cairo_dock_decrease_order (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, int *pOrder)
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

static gboolean _cairo_dock_decrease_order_if_greater (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, int *pOrder)
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

static void _cairo_dock_go_up (GtkButton *button, GtkTreeView *pTreeView)
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

static void _cairo_dock_go_down (GtkButton *button, GtkTreeView *pTreeView)
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

static void _cairo_dock_add (GtkButton *button, gpointer *data)
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

static void _cairo_dock_remove (GtkButton *button, gpointer *data)
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

static void _cairo_dock_selection_changed (GtkTreeModel *model, GtkTreeIter iter, gpointer *data)
{
	static gchar *s_cPrevPreview = NULL, *s_cPrevReadme = NULL;
	GtkLabel *pDescriptionLabel = data[0];
	GtkImage *pPreviewImage = data[1];
	GError *erreur = NULL;
	gchar *cDescriptionFilePath = NULL, *cPreviewFilePath = NULL;
	//g_print ("iter:%d\n", iter);
	gtk_tree_model_get (model, &iter,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, &cDescriptionFilePath,
		CAIRO_DOCK_MODEL_IMAGE, &cPreviewFilePath, -1);
	//g_print ("ok\n");
	
	
	if (cDescriptionFilePath != NULL && (!s_cPrevPreview || strcmp (s_cPrevPreview, cDescriptionFilePath) != 0))
	{
		g_free (s_cPrevPreview);
		s_cPrevPreview = g_strdup (cDescriptionFilePath);
		gchar *cDescription = NULL;
		if (strncmp (cDescriptionFilePath, "http://", 7) == 0)
		{
			cd_debug ("fichier readme distant (%s)", cDescriptionFilePath);

			gchar *str = strrchr (cDescriptionFilePath, '/');
			g_return_if_fail (str != NULL);
			*str = '\0';
			cDescription = cairo_dock_get_distant_file_content (cDescriptionFilePath, "", str+1, 0, NULL);
		}
		else
		{
			cd_debug ("fichier readme local (%s)", cDescriptionFilePath);
			gsize length = 0;
			g_file_get_contents  (cDescriptionFilePath,
				&cDescription,
				&length,
				NULL);
		}
		
		gtk_label_set_markup (pDescriptionLabel, cDescription ? cDescription : "");
		g_free (cDescription);
	}

	if (cPreviewFilePath != NULL && (!s_cPrevReadme || strcmp (s_cPrevReadme, cPreviewFilePath) != 0))
	{
		g_free (s_cPrevReadme);
		s_cPrevReadme = g_strdup (cPreviewFilePath);
		
		gboolean bDistant = FALSE;
		if (strncmp (cPreviewFilePath, "http://", 7) == 0)
		{
			cd_debug ("fichier preview distant (%s)", cPreviewFilePath);
			
			gchar *str = strrchr (cPreviewFilePath, '/');
			g_return_if_fail (str != NULL);
			*str = '\0';
			gchar *cTmpFilePath = cairo_dock_download_file (cPreviewFilePath, "", str+1, 0, NULL, NULL);
			
			g_free (cPreviewFilePath);
			cPreviewFilePath = cTmpFilePath;
			bDistant = TRUE;
		}
		
		int iPreviewWidth, iPreviewHeight;
		GdkPixbuf *pPreviewPixbuf = NULL;
		if (gdk_pixbuf_get_file_info (cPreviewFilePath, &iPreviewWidth, &iPreviewHeight) != NULL)
		{
			iPreviewWidth = MIN (iPreviewWidth, CAIRO_DOCK_PREVIEW_WIDTH);
			iPreviewHeight = MIN (iPreviewHeight, CAIRO_DOCK_PREVIEW_HEIGHT);
			g_print ("preview : %dx%d\n", iPreviewWidth, iPreviewHeight);
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
		gtk_image_set_from_pixbuf (pPreviewImage, pPreviewPixbuf);
		gdk_pixbuf_unref (pPreviewPixbuf);
		
		if (bDistant)
		{
			g_remove (cPreviewFilePath);
		}
	}

	g_free (cDescriptionFilePath);
	g_free (cPreviewFilePath);
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

static gboolean _cairo_dock_select_one_item_in_tree (GtkTreeSelection * selection, GtkTreeModel * model, GtkTreePath * path, gboolean path_currently_selected, gpointer *data)
{
	if (path_currently_selected)
		return TRUE;
	GtkTreeIter iter;
	if (! gtk_tree_model_get_iter (model, &iter, path))
		return FALSE;

	_cairo_dock_selection_changed (model, iter, data);
	return TRUE;
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
		gdk_pixbuf_unref (pixbuf);
		gtk_file_chooser_set_preview_widget_active (pFileChooser, TRUE);
	}
	else
		gtk_file_chooser_set_preview_widget_active (pFileChooser, FALSE);
}
static void _cairo_dock_pick_a_file (GtkButton *button, gpointer *data)
{
	GtkEntry *pEntry = data[0];
	gint iFileType = GPOINTER_TO_INT (data[1]);
	GtkWindow *pParentWindow = data[2];

	GtkWidget* pFileChooserDialog = gtk_file_chooser_dialog_new (
		(iFileType == 0 ? "Pick up a file" : "Pick up a directory"),
		pParentWindow,
		(iFileType == 0 ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER),
		GTK_STOCK_OK,
		GTK_RESPONSE_OK,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_CANCEL,
		NULL);
	const gchar *cFilePath = gtk_entry_get_text (pEntry);
	gchar *cDirectoryPath = (cFilePath == NULL || *cFilePath != '/' ? g_strdup (g_cCurrentThemePath) : g_path_get_dirname (cFilePath));
	//g_print (">>> on se place sur '%s'\n", cDirectoryPath);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (pFileChooserDialog), cDirectoryPath);
	g_free (cDirectoryPath);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (pFileChooserDialog), FALSE);

	GtkWidget *pPreviewImage = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (pFileChooserDialog), pPreviewImage);
	g_signal_connect (GTK_FILE_CHOOSER (pFileChooserDialog), "update-preview", G_CALLBACK (_cairo_dock_show_image_preview), pPreviewImage);

	gtk_widget_show (pFileChooserDialog);
	int answer = gtk_dialog_run (GTK_DIALOG (pFileChooserDialog));
	if (answer == GTK_RESPONSE_OK)
	{
		gchar *cFilePath = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pFileChooserDialog));
		gtk_entry_set_text (pEntry, cFilePath);
	}
	gtk_widget_destroy (pFileChooserDialog);
}

//Sound Callback
static void _cairo_dock_play_a_sound (GtkButton *button, gpointer *data)
{
	GtkWidget *pEntry = data[0];
	const gchar *cSoundPath = gtk_entry_get_text (GTK_ENTRY (pEntry));
	cairo_dock_play_sound (cSoundPath);
}

static void _cairo_dock_set_original_value (GtkButton *button, gpointer *data)
{
	gchar *cGroupName = data[0];
	gchar *cKeyName = data[1];
	GSList *pSubWidgetList = data[2];
	gchar *cOriginalConfFilePath = data[3];
	//g_print ("%s (%s, %s, %s)\n", __func__, cGroupName, cKeyName, cOriginalConfFilePath);
	
	GSList *pList;
	gsize i = 0;
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
	
	if (GTK_IS_SPIN_BUTTON (pOneWidget) || GTK_IS_HSCALE (pOneWidget))
	{
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
	g_key_file_free (pKeyFile);
}

static void _cairo_dock_key_grab_cb (GtkWidget *wizard_window, GdkEventKey *event, GtkEntry *pEntry)
{
	gchar *key;
	cd_message ("key press event\n");
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

		g_printerr ("KEY GRABBED: %s\n", key);

		/* Re-enable widgets */
		gtk_widget_set_sensitive (GTK_WIDGET(pEntry), TRUE);

		/* Disconnect the key grabber */
		g_signal_handlers_disconnect_by_func (GTK_OBJECT(wizard_window), GTK_SIGNAL_FUNC(_cairo_dock_key_grab_cb), pEntry);

		/* Copy the pressed key to the text entry */
		gtk_entry_set_text (GTK_ENTRY(pEntry), key);

		/* Free the string */
		g_free (key);
	}
}

static void _cairo_dock_key_grab_clicked (GtkButton *button, gpointer *data)
{
	GtkEntry *pEntry = data[0];
	GtkWindow *pParentWindow = data[1];

	cd_message ("clicked\n");
	//set widget insensitive
	gtk_widget_set_sensitive (GTK_WIDGET(pEntry), FALSE);
	//  gtk_widget_set_sensitive (wizard_notebook, FALSE);

	g_signal_connect (GTK_WIDGET(pParentWindow), "key-press-event", GTK_SIGNAL_FUNC(_cairo_dock_key_grab_cb), pEntry);
}

static void _cairo_dock_set_font (GtkFontButton *widget, GtkEntry *pEntry)
{
	const gchar *cFontName = gtk_font_button_get_font_name (GTK_FONT_BUTTON (widget));
	cd_message (" -> %s\n", cFontName);
	if (cFontName != NULL)
		gtk_entry_set_text (pEntry, cFontName);
}

static void _cairo_dock_set_color (GtkColorButton *pColorButton, GSList *pWidgetList)
{
	GdkColor gdkColor;
	gtk_color_button_get_color (pColorButton, &gdkColor);

	GtkSpinButton *pSpinButton;
	GSList *pList = pWidgetList;
	if (pList == NULL)
		return;
	pSpinButton = pList->data;
	gtk_spin_button_set_value (pSpinButton, 1. * gdkColor.red / 65535);
	pList = pList->next;

	if (pList == NULL)
		return;
	pSpinButton = pList->data;
	gtk_spin_button_set_value (pSpinButton, 1. * gdkColor.green / 65535);
	pList = pList->next;

	if (pList == NULL)
		return;
	pSpinButton = pList->data;
	gtk_spin_button_set_value (pSpinButton, 1. * gdkColor.blue / 65535);
	pList = pList->next;

	if (gtk_color_button_get_use_alpha (pColorButton))
	{
		if (pList == NULL)
		return;
		pSpinButton = pList->data;
		gtk_spin_button_set_value (pSpinButton, 1. * gtk_color_button_get_alpha (pColorButton) / 65535);
	}
}

static void _cairo_dock_get_current_color (GtkColorButton *pColorButton, GSList *pWidgetList)
{
	GtkSpinButton *pSpinButton;
	int i, color[4] = {0,0,0,0};
	GSList *c;
	for (c = pWidgetList, i = 0; c != NULL && i < 4; c = c->next, i ++)
	{
		pSpinButton = c->data;
		color[i] = gtk_spin_button_get_value (pSpinButton) * 65535;
	}
	
	GdkColor gdkColor;
	gdkColor.red = color[0];
	gdkColor.green = color[1];
	gdkColor.blue = color[2];
	gtk_color_button_set_color (pColorButton, &gdkColor);
	
	if (gtk_color_button_get_use_alpha (pColorButton))
		gtk_color_button_set_alpha (pColorButton, color[3]);
}

static void _cairo_dock_set_value_in_pair (GtkSpinButton *pSpinButton, gpointer *data)
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


#define _build_list_for_gui(pListStore, cEmptyItem, pHashTable, pHFunction) do { \
	if (pListStore != NULL)\
		g_object_unref (pListStore);\
	if (pHashTable == NULL) {\
		pListStore = NULL;\
		return ; }\
	pListStore = _allocate_new_model ();\
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (pListStore), CAIRO_DOCK_MODEL_NAME, GTK_SORT_ASCENDING);\
	if (cEmptyItem) {\
		pHFunction (cEmptyItem, NULL, pListStore); }\
	g_hash_table_foreach (pHashTable, (GHFunc) pHFunction, pListStore); } while (0)

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
void cairo_dock_build_renderer_list_for_gui (GHashTable *pHashTable)
{
	_build_list_for_gui (s_pRendererListStore, "", pHashTable, _cairo_dock_add_one_renderer_item);
}
static void _cairo_dock_add_one_decoration_item (const gchar *cName, CairoDeskletDecoration *pDecoration, GtkListStore *pModele)
{
	cd_debug ("%s (%s)", __func__, cName);
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, cName,
		CAIRO_DOCK_MODEL_RESULT, cName,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none"/*(pRenderer != NULL ? pRenderer->cReadmeFilePath : "none")*/,
		CAIRO_DOCK_MODEL_IMAGE, "none"/*(pRenderer != NULL ? pRenderer->cPreviewFilePath : "none")*/, -1);
}
void cairo_dock_build_desklet_decorations_list_for_gui (GHashTable *pHashTable)
{
	_build_list_for_gui (s_pDecorationsListStore, NULL, pHashTable, _cairo_dock_add_one_decoration_item);
}
void cairo_dock_build_desklet_decorations_list_for_applet_gui (GHashTable *pHashTable)
{
	_build_list_for_gui (s_pDecorationsListStore2, "default", pHashTable, _cairo_dock_add_one_decoration_item);
}
static void _cairo_dock_add_one_animation_item (const gchar *cName, gpointer data, GtkListStore *pModele)
{
	gint iAnimationID = GPOINTER_TO_INT (data);
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, (cName != NULL && *cName != '\0' ? gettext (cName) : cName),
		CAIRO_DOCK_MODEL_RESULT, cName,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
		CAIRO_DOCK_MODEL_IMAGE, "none", -1);
}
void cairo_dock_build_animations_list_for_gui (GHashTable *pHashTable)
{
	_build_list_for_gui (s_pAnimationsListStore, "", pHashTable, _cairo_dock_add_one_animation_item);
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
void cairo_dock_build_dialog_decorator_list_for_gui (GHashTable *pHashTable)
{
	_build_list_for_gui (s_pDialogDecoratorListStore, NULL, pHashTable, _cairo_dock_add_one_dialog_decorator_item);
}

static void _cairo_dock_add_one_gauge_item (const gchar *cName, CairoDialogDecorator *pDecorator, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, cName != NULL && *cName != '\0' ? gettext (cName) : cName,
		CAIRO_DOCK_MODEL_RESULT, cName,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
		CAIRO_DOCK_MODEL_IMAGE, "none", -1);
}
static void cairo_dock_build_gauge_list_for_gui (GHashTable *pHashTable)
{
	_build_list_for_gui (s_pGaugeListStore, NULL, pHashTable, _cairo_dock_add_one_gauge_item);
}

static void _cairo_dock_add_one_dock_item (const gchar *cName, CairoDock *pDock, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, cName,
		CAIRO_DOCK_MODEL_RESULT, cName,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, "none",
		CAIRO_DOCK_MODEL_IMAGE, "none", -1);
}
static void cairo_dock_build_dock_list_for_gui (void)
{
	if (s_pDocksListStore != NULL)
		g_object_unref (s_pDocksListStore);
	s_pDocksListStore = _allocate_new_model ();
	_cairo_dock_add_one_dock_item ("", NULL, s_pDocksListStore);
	cairo_dock_foreach_docks ((GHFunc) _cairo_dock_add_one_dock_item, s_pDocksListStore);
}

static void _cairo_dock_fill_modele_with_themes (const gchar *cThemeName, CairoDockTheme *pTheme, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gchar *cReadmePath = g_strdup_printf ("%s/readme", pTheme->cThemePath);
	gchar *cPreviewPath = g_strdup_printf ("%s/preview", pTheme->cThemePath);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, pTheme->cDisplayedName,
		CAIRO_DOCK_MODEL_RESULT, cThemeName,
		CAIRO_DOCK_MODEL_ACTIVE, FALSE,
		CAIRO_DOCK_MODEL_DESCRIPTION_FILE, cReadmePath,
		CAIRO_DOCK_MODEL_IMAGE, cPreviewPath, 
		CAIRO_DOCK_MODEL_ORDER, pTheme->iRating,
		CAIRO_DOCK_MODEL_ORDER2, pTheme->iSobriety, -1);
}
static gboolean _cairo_dock_test_one_name (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer *data)
{
	gchar *cName = NULL, *cResult = NULL;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_RESULT, &cResult, -1);
	if (cResult == NULL)
		gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_NAME, &cName, -1);
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
static gboolean _cairo_dock_find_iter_from_name (GtkListStore *pModele, const gchar *cName, GtkTreeIter *iter)
{
	//g_print ("%s (%s)\n", __func__, cName);
	if (cName == NULL)
		return FALSE;
	gboolean bFound = FALSE;
	gconstpointer data[3] = {cName, iter, &bFound};
	gtk_tree_model_foreach (GTK_TREE_MODEL (pModele), (GtkTreeModelForeachFunc) _cairo_dock_test_one_name, data);
	return bFound;
}

#define CD_MAX_RATING 5
static inline void _render_rating (GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *iter, int iColumnIndex)
{
	gint iRating = 0;
	gtk_tree_model_get (model, iter, iColumnIndex, &iRating, -1);
	if (iRating > CD_MAX_RATING)
		iRating = CD_MAX_RATING;
	if (iRating > 0)
	{
		GString *s = g_string_sized_new (CD_MAX_RATING*4+1);
		int i;
		for (i= 0; i < iRating; i ++)
			g_string_append (s, "★");
		for (;i < CD_MAX_RATING; i ++)
			g_string_append (s, "☆");
		g_object_set (cell, "text", s->str, NULL);  // markup
		g_string_free (s, TRUE);
	}
	else
	{
		g_object_set (cell, "text", "-", NULL);
	}
}
static void _cairo_dock_render_sobriety (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, gpointer data)
{
	_render_rating (cell, model, iter, CAIRO_DOCK_MODEL_ORDER2);
}
static void _cairo_dock_render_rating (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model,GtkTreeIter *iter, gpointer data)
{
	_render_rating (cell, model, iter, CAIRO_DOCK_MODEL_ORDER);
}

static void _cairo_dock_configure_module (GtkButton *button, gpointer *data)
{
	GtkTreeView *pCombo = data[0];
	GtkWindow *pDialog = data[1];
	gchar *cModuleName = data[2];
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	CairoDockInternalModule *pInternalModule = cairo_dock_find_internal_module_from_name (cModuleName);
	Icon *pIcon = cairo_dock_get_current_active_icon ();
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon != NULL ? pIcon->cParentDockName : NULL);
	gchar *cMessage = NULL;
	
	if (pModule == NULL && pInternalModule == NULL)
	{
		cMessage = g_strdup_printf (_("The '%s' module was not found.\nBe sure to install it in the same version as the dock to enjoy these features."), cModuleName);
		int iDuration = 10e3;
		if (pIcon != NULL && pDock != NULL)
			cairo_dock_show_temporary_dialog_with_icon (cMessage, pIcon, CAIRO_CONTAINER (pDock), iDuration, "same icon");
		else
			cairo_dock_show_general_message (cMessage, iDuration);
	}
	else
	{
		CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cModuleName);
		if (pGroupDescription == NULL)
		{
			cairo_dock_build_main_ihm (g_cConfFile, FALSE);
			pGroupDescription = cairo_dock_find_module_description (cModuleName);
			g_return_if_fail (pGroupDescription != NULL);
		}
		if (pModule != NULL && pModule->pInstancesList == NULL)
		{
			cMessage = g_strdup_printf (_("The '%s' plug-in is not active.\nBe sure to activate it to enjoy these features."), cModuleName);
			int iDuration = 8e3;
			if (pIcon != NULL && pDock != NULL)
				cairo_dock_show_temporary_dialog_with_icon (cMessage, pIcon, CAIRO_CONTAINER (pDock), iDuration, "same icon");
			else
				cairo_dock_show_general_message (cMessage, 8000);
		}
		cairo_dock_show_group (pGroupDescription);
	}
	g_free (cMessage);
}

#define _allocate_new_buffer\
	data = g_new (gconstpointer, 3); \
	g_ptr_array_add (pDataGarbage, data);

#define _pack_in_widget_box(pSubWidget) gtk_box_pack_start (GTK_BOX (pWidgetBox), pSubWidget, FALSE, FALSE, 0)
#define _pack_subwidget(pSubWidget) do {\
	pSubWidgetList = g_slist_append (pSubWidgetList, pSubWidget);\
	_pack_in_widget_box (pSubWidget); } while (0)
#define _add_combo_from_modele(modele, bAddPreviewWidgets, bWithEntry) do {\
	if (modele == NULL) { \
		pOneWidget = gtk_combo_box_entry_new ();\
		_pack_subwidget (pOneWidget); }\
	else {\
		cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);\
		pOneWidget = (bWithEntry ? gtk_combo_box_entry_new_with_model (GTK_TREE_MODEL (modele), CAIRO_DOCK_MODEL_NAME) : gtk_combo_box_new_with_model (GTK_TREE_MODEL (modele)));\
		rend = gtk_cell_renderer_text_new ();\
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (pOneWidget), rend, FALSE);\
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (pOneWidget), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);\
		if (bAddPreviewWidgets) {\
			pDescriptionLabel = gtk_label_new (NULL);\
			gtk_label_set_use_markup  (GTK_LABEL (pDescriptionLabel), TRUE);\
			pPreviewImage = gtk_image_new_from_pixbuf (NULL);\
			_allocate_new_buffer;\
			data[0] = pDescriptionLabel;\
			data[1] = pPreviewImage;\
			g_signal_connect (G_OBJECT (pOneWidget), "changed", G_CALLBACK (_cairo_dock_select_one_item_in_combo), data);\
			pPreviewBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);\
			gtk_box_pack_start (GTK_BOX (pAdditionalItemsVBox ? pAdditionalItemsVBox : pKeyBox), pPreviewBox, FALSE, FALSE, 0);\
			gtk_box_pack_start (GTK_BOX (pPreviewBox), pPreviewImage, FALSE, FALSE, 0);\
			gtk_box_pack_start (GTK_BOX (pPreviewBox), pDescriptionLabel, FALSE, FALSE, 0); }\
		if (_cairo_dock_find_iter_from_name (modele, cValue, &iter))\
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (pOneWidget), &iter);\
		_pack_subwidget (pOneWidget);\
		g_free (cValue); } } while (0)

gchar *cairo_dock_parse_key_comment (gchar *cKeyComment, char *iElementType, guint *iNbElements, gchar ***pAuthorizedValuesList, gboolean *bAligned, gchar **cTipString)
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
	gchar iIdentifier = 0;
	gchar *cHighLightedSentence;
	//GPtrArray *pDataGarbage = g_ptr_array_new ();
	//GPtrArray *pModelGarbage = g_ptr_array_new ();
	
	gconstpointer *data;
	int iNbBuffers = 0;
	gsize length = 0;
	gchar **pKeyList;
	
	GtkWidget *pOneWidget;
	GSList * pSubWidgetList;
	GtkWidget *pLabel=NULL, *pLabelContainer;
	GtkWidget *pGroupBox, *pKeyBox=NULL, *pSmallVBox, *pWidgetBox=NULL;
	GtkWidget *pEntry;
	GtkWidget *pTable;
	GtkWidget *pButtonAdd, *pButtonRemove;
	GtkWidget *pButtonDown, *pButtonUp, *pButtonConfig;
	GtkWidget *pButtonFileChooser, *pButtonPlay;
	GtkWidget *pFrame, *pFrameVBox;
	GtkWidget *pScrolledWindow;
	GtkWidget *pColorButton;
	GtkWidget *pFontButton;
	GtkWidget *pDescriptionLabel;
	GtkWidget *pPreviewImage;
	GtkWidget *pButtonConfigRenderer;
	GtkWidget *pToggleButton=NULL;
	GtkCellRenderer *rend;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkWidget *pBackButton;
	gchar *cGroupComment, *cKeyName, *cKeyComment, *cUsefulComment, *cAuthorizedValuesChain, *pTipString, **pAuthorizedValuesList, *cSmallGroupIcon=NULL, *cDisplayedGroupName=NULL;
	gpointer *pGroupKeyWidget;
	int i, j;
	guint k, iNbElements;
	int iNumPage=0, iPresentedNumPage=0;
	char iElementType;
	gboolean bValue, *bValueList;
	int iValue, iMinValue, iMaxValue, *iValueList;
	double fValue, fMinValue, fMaxValue, *fValueList;
	gchar *cValue, **cValueList, *cSmallIcon=NULL;
	GdkColor gdkColor;
	GtkListStore *modele;
	gboolean bAddBackButton;
	GtkWidget *pPreviewBox;
	GtkWidget *pAdditionalItemsVBox;
	gboolean bIsAligned;
	
	pGroupBox = NULL;
	pFrame = NULL;
	pFrameVBox = NULL;
	cGroupComment  = g_key_file_get_comment (pKeyFile, cGroupName, NULL, NULL);
	if (cGroupComment != NULL)
	{
		cGroupComment[strlen(cGroupComment)-1] = '\0';
		gchar *str = strrchr (cGroupComment, '[');
		if (str != NULL)
		{
			cSmallGroupIcon = str+1;
			str = strrchr (cSmallGroupIcon, ']');
			if (str != NULL)
				*str = '\0';
			str = strrchr (cSmallGroupIcon, ';');
			if (str != NULL)
			{
				*str = '\0';
				cDisplayedGroupName = str + 1;
			}	
		}
	}
	
	pKeyList = g_key_file_get_keys (pKeyFile, cGroupName, NULL, NULL);

	for (j = 0; pKeyList[j] != NULL; j ++)
	{
		cKeyName = pKeyList[j];
		
		//\______________ On parse le commentaire.
		cKeyComment =  g_key_file_get_comment (pKeyFile, cGroupName, cKeyName, NULL);
		cUsefulComment = cairo_dock_parse_key_comment (cKeyComment, &iElementType, &iNbElements, &pAuthorizedValuesList, &bIsAligned, &pTipString);
		if (cUsefulComment == NULL)
		{
			g_free (cKeyComment);
			continue;
		}
		
		//\______________ On cree la boite du groupe si c'est la 1ere cle valide.
		if (pGroupBox == NULL)  // maintenant qu'on a au moins un element dans ce groupe, on cree sa page dans le notebook.
		{
			pLabel = gtk_label_new (dgettext (cGettextDomain, cDisplayedGroupName ? cDisplayedGroupName : cGroupName));
			
			pLabelContainer = NULL;
			GtkWidget *pAlign = NULL;
			if (cSmallGroupIcon != NULL && *cSmallGroupIcon != '\0')
			{
				pLabelContainer = gtk_hbox_new (FALSE, CAIRO_DOCK_ICON_MARGIN);
				pAlign = gtk_alignment_new (0., 0.5, 0., 0.);
				gtk_container_add (GTK_CONTAINER (pAlign), pLabelContainer);
				
				GtkWidget *pImage = gtk_image_new ();
				GdkPixbuf *pixbuf;
				if (*cSmallGroupIcon != '/')
					pixbuf = gtk_widget_render_icon (pImage,
						cSmallGroupIcon ,
						GTK_ICON_SIZE_BUTTON,
						NULL);
				else
					pixbuf = gdk_pixbuf_new_from_file_at_size (cSmallGroupIcon, CAIRO_DOCK_TAB_ICON_SIZE, CAIRO_DOCK_TAB_ICON_SIZE, NULL);
				if (pixbuf != NULL)
				{
					gtk_image_set_from_pixbuf (GTK_IMAGE (pImage), pixbuf);
					gdk_pixbuf_unref (pixbuf);
					gtk_container_add (GTK_CONTAINER (pLabelContainer),
						pImage);
				}
				gtk_container_add (GTK_CONTAINER (pLabelContainer),
					pLabel);
				gtk_widget_show_all (pLabelContainer);
			}
			
			pGroupBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
			gtk_container_set_border_width (GTK_CONTAINER (pGroupBox), CAIRO_DOCK_GUI_MARGIN);
		}
		
		pAdditionalItemsVBox = NULL;
		if (iElementType != CAIRO_DOCK_WIDGET_FRAME && iElementType != CAIRO_DOCK_WIDGET_EXPANDER && iElementType != CAIRO_DOCK_WIDGET_SEPARATOR)
		{
			//\______________ On cree la boite de la cle.
			if (iElementType == CAIRO_DOCK_WIDGET_THEME_LIST || iElementType == CAIRO_DOCK_WIDGET_THEME_LIST_ENTRY || iElementType == CAIRO_DOCK_WIDGET_VIEW_LIST)
			{
				pAdditionalItemsVBox = gtk_vbox_new (FALSE, 0);
				gtk_box_pack_start (pFrameVBox != NULL ? GTK_BOX (pFrameVBox) :  GTK_BOX (pGroupBox),
					pAdditionalItemsVBox,
					FALSE,
					FALSE,
					0);
				pKeyBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
				gtk_box_pack_start (GTK_BOX (pAdditionalItemsVBox),
					pKeyBox,
					FALSE,
					FALSE,
					0);
			}
			else
			{
				pKeyBox = (bIsAligned ? gtk_hbox_new : gtk_vbox_new) (FALSE, CAIRO_DOCK_GUI_MARGIN);
				gtk_box_pack_start (pFrameVBox != NULL ? GTK_BOX (pFrameVBox) :  GTK_BOX (pGroupBox),
					pKeyBox,
					FALSE,
					FALSE,
					0);
			}
			if (pTipString != NULL)
			{
				gtk_widget_set_tooltip_text (pKeyBox, dgettext (cGettextDomain, pTipString));
			}
			
			//\______________ On cree le label descriptif et la boite du widget.
			if (iElementType != CAIRO_DOCK_WIDGET_EMPTY_WIDGET)
			{
				if (*cUsefulComment != '\0' && strcmp (cUsefulComment, "...") != 0)
				{
					pLabel = gtk_label_new (NULL);
					gtk_label_set_use_markup  (GTK_LABEL (pLabel), TRUE);
					gtk_label_set_markup (GTK_LABEL (pLabel), dgettext (cGettextDomain, cUsefulComment));
					GtkWidget *pAlign = gtk_alignment_new (0., 0.5, 0., 0.);
					gtk_container_add (GTK_CONTAINER (pAlign), pLabel);
					gtk_box_pack_start (GTK_BOX (pKeyBox),
						pAlign,
						FALSE,
						FALSE,
						0);
				}
				
				pWidgetBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
				gtk_box_pack_end (GTK_BOX (pKeyBox),
					pWidgetBox,
					FALSE,
					FALSE,
					0);
			}
		}
		
		pSubWidgetList = NULL;
		bAddBackButton = FALSE;
		
		//\______________ On cree les widgets selon leur type.
		switch (iElementType)
		{
			case CAIRO_DOCK_WIDGET_CHECK_BUTTON :  // boolean
				length = 0;
				bValueList = g_key_file_get_boolean_list (pKeyFile, cGroupName, cKeyName, &length, NULL);

				for (k = 0; k < iNbElements; k ++)
				{
					bValue =  (k < length ? bValueList[k] : FALSE);
					pOneWidget = gtk_check_button_new ();
					gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), bValue);

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
					GtkWidget *pImage = gtk_image_new_from_stock (GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_MENU);  // trouver une image...
					gtk_button_set_image (GTK_BUTTON (pToggleButton), pImage);
				}
				for (k = 0; k < iNbElements; k ++)
				{
					iValue =  (k < length ? iValueList[k] : 0);
					GtkObject *pAdjustment = gtk_adjustment_new (iValue,
						0,
						1,
						1,
						MAX (1, (iMaxValue - iMinValue) / 20),
						0);

					if (iElementType == CAIRO_DOCK_WIDGET_HSCALE_INTEGER)
					{
						pOneWidget = gtk_hscale_new (GTK_ADJUSTMENT (pAdjustment));
						gtk_scale_set_digits (GTK_SCALE (pOneWidget), 0);
						gtk_widget_set (pOneWidget, "width-request", 150, NULL);
					}
					else
					{
						pOneWidget = gtk_spin_button_new (GTK_ADJUSTMENT (pAdjustment), 1., 0);
					}
					g_object_set (pAdjustment, "lower", (double) iMinValue, "upper", (double) iMaxValue, NULL); // le 'width-request' sur un GtkHScale avec 'fMinValue' non nul plante ! Donc on les met apres...
					gtk_adjustment_set_value (GTK_ADJUSTMENT (pAdjustment), iValue);

					_pack_subwidget (pOneWidget);
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
			case CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGB :  // float x3 avec un bouton de choix de couleur.
			case CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGBA :  // float x4 avec un bouton de choix de couleur.
			case CAIRO_DOCK_WIDGET_HSCALE_DOUBLE :  // float dans un HScale.
				if (iElementType == CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGB || iElementType == CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGBA)  // couleur
				{
					fMinValue = 0;
					fMaxValue = 1;
					if (iNbElements == 1)  // nbre d'elements non precise
					{
						iNbElements = (iElementType == CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGB ? 3 : 4);
					}
				}
				else
				{
					if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[0] != NULL)
						fMinValue = g_ascii_strtod (pAuthorizedValuesList[0], NULL);
					else
						fMinValue = 0;
					if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[1] != NULL)
						fMaxValue = g_ascii_strtod (pAuthorizedValuesList[1], NULL);
					else
						fMaxValue = 9999;
				}
				length = 0;
				fValueList = g_key_file_get_double_list (pKeyFile, cGroupName, cKeyName, &length, NULL);
				for (k = 0; k < iNbElements; k ++)
				{
					fValue =  (k < length ? fValueList[k] : 0);

					GtkObject *pAdjustment = gtk_adjustment_new (fValue,
						0,
						1,
						(fMaxValue - fMinValue) / 20.,
						(fMaxValue - fMinValue) / 10.,
						0);

					if (iElementType == CAIRO_DOCK_WIDGET_HSCALE_DOUBLE)
					{
						pOneWidget = gtk_hscale_new (GTK_ADJUSTMENT (pAdjustment));
						gtk_scale_set_digits (GTK_SCALE (pOneWidget), 3);
						gtk_widget_set (pOneWidget, "width-request", 150, NULL);
					}
					else
					{
						pOneWidget = gtk_spin_button_new (GTK_ADJUSTMENT (pAdjustment),
							1.,
							3);
					}
					g_object_set (pAdjustment, "lower", fMinValue, "upper", fMaxValue, NULL); // le 'width-request' sur un GtkHScale avec 'fMinValue' non nul plante ! Donc on les met apres...
					gtk_adjustment_set_value (GTK_ADJUSTMENT (pAdjustment), fValue);

					_pack_subwidget (pOneWidget);
				}
				if (iElementType == CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGB || iElementType == CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGBA)
				{
					if (length > 2)
					{
						gdkColor.red = fValueList[0] * 65535;
						gdkColor.green = fValueList[1] * 65535;
						gdkColor.blue = fValueList[2] * 65535;
					}
					pColorButton = gtk_color_button_new_with_color (&gdkColor);
					if (iNbElements > 3)
					{
						gtk_color_button_set_use_alpha (GTK_COLOR_BUTTON (pColorButton), TRUE);
						if (length > 3)
							gtk_color_button_set_alpha (GTK_COLOR_BUTTON (pColorButton), fValueList[3] * 65535);
						else
							gtk_color_button_set_alpha (GTK_COLOR_BUTTON (pColorButton), 65535);
					}
					else
						gtk_color_button_set_use_alpha (GTK_COLOR_BUTTON (pColorButton), FALSE);

					_pack_in_widget_box (pColorButton);
					g_signal_connect (G_OBJECT (pColorButton), "color-set", G_CALLBACK(_cairo_dock_set_color), pSubWidgetList);
					g_signal_connect (G_OBJECT (pColorButton), "clicked", G_CALLBACK(_cairo_dock_get_current_color), pSubWidgetList);
				}
				bAddBackButton = TRUE,
				g_free (fValueList);
			break;

			case CAIRO_DOCK_WIDGET_VIEW_LIST :  // liste des vues.
				_add_combo_from_modele (s_pRendererListStore, TRUE, FALSE);
			break ;
			
			case CAIRO_DOCK_WIDGET_THEME_LIST :  // liste les themes dans combo, avec prevue et readme.
			case CAIRO_DOCK_WIDGET_THEME_LIST_ENTRY :  // idem mais avec une combo-entry.
				//\______________ On construit le widget de visualisation de themes.
				modele = _allocate_new_model ();
				gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (modele), CAIRO_DOCK_MODEL_NAME, GTK_SORT_ASCENDING);
				
				_add_combo_from_modele (modele, TRUE, iElementType == CAIRO_DOCK_WIDGET_THEME_LIST_ENTRY);
				
				//\______________ On recupere les themes.
				if (pAuthorizedValuesList != NULL)
				{
					gchar *cShareThemesDir = NULL, *cUserThemesDir = NULL, *cDistantThemesDir = NULL;
					if (pAuthorizedValuesList[0] != NULL)
					{
						cShareThemesDir = (*pAuthorizedValuesList[0] != '\0' ? pAuthorizedValuesList[0] : NULL);
						if (pAuthorizedValuesList[1] != NULL)
						{
							cUserThemesDir = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, pAuthorizedValuesList[1]);
							cDistantThemesDir = pAuthorizedValuesList[2];
						}
					}
					GHashTable *pThemeTable = cairo_dock_list_themes (cShareThemesDir, cUserThemesDir, cDistantThemesDir);
					g_free (cUserThemesDir);
					
					cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
					cairo_dock_fill_combo_with_themes (pOneWidget, pThemeTable, cValue);
					g_hash_table_destroy (pThemeTable);
					g_free (cValue);
				}
			break ;
			
			case CAIRO_DOCK_WIDGET_USER_THEME_SELECTOR :  // liste des themes utilisateurs, avec une encoche a cote (pour suppression).
				//\______________ On construit le treeview des themes.
				modele = _allocate_new_model ();
				gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (modele), CAIRO_DOCK_MODEL_NAME, GTK_SORT_ASCENDING);
				pOneWidget = gtk_tree_view_new ();
				gtk_tree_view_set_model (GTK_TREE_VIEW (pOneWidget), GTK_TREE_MODEL (modele));
				gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (pOneWidget), FALSE);
				
				rend = gtk_cell_renderer_toggle_new ();
				gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "active", CAIRO_DOCK_MODEL_ACTIVE, NULL);
				g_signal_connect (G_OBJECT (rend), "toggled", (GCallback) _cairo_dock_activate_one_element, modele);

				rend = gtk_cell_renderer_text_new ();
				gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, NULL, rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
				
				selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pOneWidget));
				gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
				
				pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
				gtk_widget_set (pScrolledWindow, "height-request", 100, NULL);
				gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
				gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pOneWidget);
				pSubWidgetList = g_slist_append (pSubWidgetList, pOneWidget);
				_pack_in_widget_box (pScrolledWindow);
				
				//\______________ On recupere les themes utilisateurs.
				if (pAuthorizedValuesList != NULL && pAuthorizedValuesList[0] != NULL && *pAuthorizedValuesList[0] != '\0')
				{
					gchar *cUserThemesDir = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, pAuthorizedValuesList[0]);
					GHashTable *pThemeTable = cairo_dock_list_themes (NULL, cUserThemesDir, NULL);
					g_free (cUserThemesDir);

					g_hash_table_foreach (pThemeTable, (GHFunc)_cairo_dock_fill_modele_with_themes, modele);
					g_hash_table_destroy (pThemeTable);
				}
			break ;

			case CAIRO_DOCK_WIDGET_ANIMATION_LIST :  // liste des animations.
				_add_combo_from_modele (s_pAnimationsListStore, FALSE, FALSE);
			break ;
			
			case CAIRO_DOCK_WIDGET_DIALOG_DECORATOR_LIST :  // liste des decorateurs de dialogue.
				_add_combo_from_modele (s_pDialogDecoratorListStore, FALSE, FALSE);
			break ;
			
			case CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST :  // liste des decorations de desklet.
			case CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST_WITH_DEFAULT :  // idem mais avec le choix "defaut" en plus.
				_add_combo_from_modele ((iElementType == CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST ? s_pDecorationsListStore : s_pDecorationsListStore2), FALSE, FALSE);
			break ;
			
			case CAIRO_DOCK_WIDGET_GAUGE_LIST :  // liste des themes de jauge.
				if (s_pGaugeListStore == NULL)
				{
					GHashTable *pGaugeTable = cairo_dock_list_available_gauges ();
					cairo_dock_build_gauge_list_for_gui (pGaugeTable);
					g_hash_table_destroy (pGaugeTable);
				}
				_add_combo_from_modele (s_pGaugeListStore, FALSE, FALSE);
			break ;
			
			case CAIRO_DOCK_WIDGET_DOCK_LIST :  // liste des docks existant.
				cairo_dock_build_dock_list_for_gui ();
				modele = s_pDocksListStore;
				pOneWidget = gtk_combo_box_entry_new_with_model (GTK_TREE_MODEL (modele), CAIRO_DOCK_MODEL_NAME);
				
				cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, NULL);
				GtkTreeIter iter;
				if (_cairo_dock_find_iter_from_name (modele, cValue, &iter))
					gtk_combo_box_set_active_iter (GTK_COMBO_BOX (pOneWidget), &iter);
				g_free (cValue);
				
				_pack_subwidget (pOneWidget);
			break ;
			
			case CAIRO_DOCK_WIDGET_JUMP_TO_MODULE :  // bouton raccourci vers un autre module
			case CAIRO_DOCK_WIDGET_JUMP_TO_MODULE_IF_EXISTS :  // idem mais seulement affiche si le module existe.
				if (pAuthorizedValuesList == NULL || pAuthorizedValuesList[0] == NULL || *pAuthorizedValuesList[0] == '\0')
					break ;
				
				const gchar *cModuleName = NULL;
				CairoDockInternalModule *pInternalModule = cairo_dock_find_internal_module_from_name (pAuthorizedValuesList[0]);
				if (pInternalModule != NULL)
					cModuleName = pInternalModule->cModuleName;
				else
				{
					CairoDockModule *pModule = cairo_dock_find_module_from_name (pAuthorizedValuesList[0]);
					if (pModule != NULL)
						cModuleName = pModule->pVisitCard->cModuleName;
					else
					{
						if (iElementType == CAIRO_DOCK_WIDGET_JUMP_TO_MODULE_IF_EXISTS)
							break ;
						cd_warning ("module '%s' not found", pAuthorizedValuesList[0]);
						cModuleName = g_strdup (pAuthorizedValuesList[0]);  // petite fuite memoire dans ce cas tres precis ...
					}
				}
				pOneWidget = gtk_button_new_from_stock (GTK_STOCK_JUMP_TO);
				_allocate_new_buffer;
				data[0] = pOneWidget;
				data[1] = pMainWindow;
				data[2] = cModuleName;
				g_signal_connect (G_OBJECT (pOneWidget),
					"clicked",
					G_CALLBACK (_cairo_dock_configure_module),
					data);
				_pack_subwidget (pOneWidget);
			break ;
			
			case CAIRO_DOCK_WIDGET_LIST :  // a list of strings.
			case CAIRO_DOCK_WIDGET_NUMBERED_LIST :  // a list of numbered strings.
			case CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY :  // a list of strings with possibility to select a non-existing one.
				cValue = g_key_file_get_locale_string (pKeyFile, cGroupName, cKeyName, NULL, NULL);  // nous permet de recuperer les ';' aussi.
				// on construit la combo.
				modele = _allocate_new_model ();
				if (iElementType == CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY)
				{
					pOneWidget = gtk_combo_box_entry_new_with_model (GTK_TREE_MODEL (modele), CAIRO_DOCK_MODEL_NAME);
				}
				else
				{
					pOneWidget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (modele));
					rend = gtk_cell_renderer_text_new ();
					gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (pOneWidget), rend, FALSE);
					gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (pOneWidget), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
				}
				
				// on la remplit.
				if (pAuthorizedValuesList != NULL)
				{
					k = 0;
					int iSelectedItem = -1;
					if (iElementType == CAIRO_DOCK_WIDGET_NUMBERED_LIST)
						iSelectedItem = atoi (cValue);
					gchar *cResult = (iElementType == CAIRO_DOCK_WIDGET_NUMBERED_LIST ? g_new0 (gchar , 10) : NULL);
					for (k = 0; pAuthorizedValuesList[k] != NULL; k ++)  // on ajoute toutes les chaines possibles a la combo.
					{
						GtkTreeIter iter;
						gtk_list_store_append (GTK_LIST_STORE (modele), &iter);
						if (iSelectedItem == -1 && cValue && strcmp (cValue, pAuthorizedValuesList[k]) == 0)
							iSelectedItem = k;

						if (cResult != NULL)
							snprintf (cResult, 9, "%d", k);
						gtk_list_store_set (GTK_LIST_STORE (modele), &iter,
							CAIRO_DOCK_MODEL_NAME, (iElementType == 'l' ? dgettext (cGettextDomain, pAuthorizedValuesList[k]) : pAuthorizedValuesList[k]),
							CAIRO_DOCK_MODEL_RESULT, (cResult != NULL ? cResult : pAuthorizedValuesList[k]), -1);

					}
					g_free (cResult);
					
					// on active l'element courant.
					if (iElementType != CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY && iSelectedItem == -1)  // si le choix courant n'etait pas dans la liste, on decide de selectionner le 1er.
						iSelectedItem = 0;
					if (k != 0)  // rien dans le gtktree => plantage.
						gtk_combo_box_set_active (GTK_COMBO_BOX (pOneWidget), iSelectedItem);
				}
				_pack_subwidget (pOneWidget);
				g_free (cValue);
			break ;
			
			case CAIRO_DOCK_WIDGET_TREE_VIEW_SORT :  // N strings listed from top to bottom.
			case CAIRO_DOCK_WIDGET_TREE_VIEW_SORT_AND_MODIFY :  // same with possibility to add/remove some.
			case CAIRO_DOCK_WIDGET_TREE_VIEW_MULTI_CHOICE :  // N strings that can be selected or not.
				// on construit le tree view.
				cValueList = g_key_file_get_locale_string_list (pKeyFile, cGroupName, cKeyName, NULL, &length, NULL);
				pOneWidget = gtk_tree_view_new ();
				modele = _allocate_new_model ();
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
				gtk_widget_set (pScrolledWindow, "height-request", 100, NULL);
				gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
				gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pOneWidget);
				_pack_in_widget_box (pScrolledWindow);
				
				if (iElementType != CAIRO_DOCK_WIDGET_TREE_VIEW_MULTI_CHOICE)
				{
					pSmallVBox = gtk_vbox_new (FALSE, 3);
					_pack_in_widget_box (pSmallVBox);

					pButtonUp = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
					g_signal_connect (G_OBJECT (pButtonUp),
						"clicked",
						G_CALLBACK (_cairo_dock_go_up),
						pOneWidget);
					gtk_box_pack_start (GTK_BOX (pSmallVBox),
						pButtonUp,
						FALSE,
						FALSE,
						0);

					pButtonDown = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
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
					pTable = gtk_table_new (2, 2, FALSE);
					_pack_in_widget_box (pTable);
						
					_allocate_new_buffer;
					
					pButtonAdd = gtk_button_new_from_stock (GTK_STOCK_ADD);
					g_signal_connect (G_OBJECT (pButtonAdd),
						"clicked",
						G_CALLBACK (_cairo_dock_add),
						data);
					gtk_table_attach (GTK_TABLE (pTable),
						pButtonAdd,
						0, 1,
						0, 1,
						GTK_SHRINK, GTK_SHRINK,
						0, 0);
					pButtonRemove = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
					g_signal_connect (G_OBJECT (pButtonRemove),
						"clicked",
						G_CALLBACK (_cairo_dock_remove),
						data);
					gtk_table_attach (GTK_TABLE (pTable),
						pButtonRemove,
						0, 1,
						1, 2,
						GTK_SHRINK, GTK_SHRINK,
						0, 0);
					pEntry = gtk_entry_new ();
					gtk_table_attach (GTK_TABLE (pTable),
						pEntry,
						1, 2,
						0, 2,
						GTK_SHRINK, GTK_SHRINK,
						0, 0);
					
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
						iValue = atoi (cValue);
						if (iValue < iNbPossibleValues)  // devrait toujours etre vrai.
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
								if (iValue == k)  // a deja ete inseree.
									break;
							}
							
							if (l == length)  // elle n'a pas encore ete inseree.
							{
								snprintf (cResult, 9, "%d", k);
								memset (&iter, 0, sizeof (GtkTreeIter));
								gtk_list_store_append (modele, &iter);
								gtk_list_store_set (modele, &iter,
									CAIRO_DOCK_MODEL_ACTIVE, FALSE,
									CAIRO_DOCK_MODEL_NAME, dgettext (cGettextDomain, cValue),
									CAIRO_DOCK_MODEL_RESULT, cResult,
									CAIRO_DOCK_MODEL_ORDER, iOrder ++, -1);
							}
						}
					}
				}
			break ;
			
			case CAIRO_DOCK_WIDGET_THEME_SELECTOR :  // tree view with 3 sortable columns.
				//\______________ On construit le treeview des themes.
				modele = _allocate_new_model ();
				gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (modele), CAIRO_DOCK_MODEL_NAME, GTK_SORT_ASCENDING);
				pOneWidget = gtk_tree_view_new ();
				gtk_tree_view_set_model (GTK_TREE_VIEW (pOneWidget), GTK_TREE_MODEL (modele));
				gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (pOneWidget), TRUE);
				gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (pOneWidget), TRUE);
				g_object_set_data (G_OBJECT (pOneWidget), "get-active-line-only", GINT_TO_POINTER (1));
				
				rend = gtk_cell_renderer_text_new ();
				//gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, _("theme"), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
				GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes (_("theme"), rend, "text", CAIRO_DOCK_MODEL_NAME, NULL);
				gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_NAME);
				gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
				
				rend = gtk_cell_renderer_text_new ();
				//gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (pOneWidget), -1, _("rating"), rend, "text", CAIRO_DOCK_MODEL_ORDER, NULL);
				col = gtk_tree_view_column_new_with_attributes (_("rating"), rend, "text", CAIRO_DOCK_MODEL_ORDER, NULL);
				gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_ORDER);
				gtk_tree_view_column_set_cell_data_func (col, rend, (GtkTreeCellDataFunc)_cairo_dock_render_rating, NULL, NULL);
				gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
				
				rend = gtk_cell_renderer_text_new ();
				//gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (pOneWidget), -1, _("sobriety"), rend, (GtkTreeCellDataFunc)_render_sobriety, NULL, NULL );
				col = gtk_tree_view_column_new_with_attributes (_("sobriety"), rend, "text", CAIRO_DOCK_MODEL_ORDER2, NULL);
				gtk_tree_view_column_set_sort_column_id (col, CAIRO_DOCK_MODEL_ORDER2);
				gtk_tree_view_column_set_cell_data_func (col, rend, (GtkTreeCellDataFunc)_cairo_dock_render_sobriety, NULL, NULL);
				gtk_tree_view_append_column (GTK_TREE_VIEW (pOneWidget), col);
				
				selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pOneWidget));
				gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
				
				GtkObject *adj = gtk_adjustment_new (0., 0., 100., 1, 10, 10);
				gtk_tree_view_set_vadjustment (GTK_TREE_VIEW (pOneWidget), GTK_ADJUSTMENT (adj));
				pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
				gtk_widget_set (pScrolledWindow, "height-request", CAIRO_DOCK_PREVIEW_HEIGHT+20, NULL);  // prevue + readme.
				gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
				gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pOneWidget);
				pSubWidgetList = g_slist_append (pSubWidgetList, pOneWidget);
				_pack_in_widget_box (pScrolledWindow);
				
				//\______________ On construit le widget de prevue et on le rajoute a la suite. 
				pDescriptionLabel = gtk_label_new (NULL);
				gtk_label_set_use_markup  (GTK_LABEL (pDescriptionLabel), TRUE);
				pPreviewImage = gtk_image_new_from_pixbuf (NULL);
				_allocate_new_buffer;
				data[0] = pDescriptionLabel;
				data[1] = pPreviewImage;
				gtk_tree_selection_set_select_function (selection,
					(GtkTreeSelectionFunc) _cairo_dock_select_one_item_in_tree,
					data,
					NULL);
				pPreviewBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
				gtk_box_pack_start (GTK_BOX (pPreviewBox), pPreviewImage, FALSE, FALSE, 0);
				gtk_box_pack_start (GTK_BOX (pPreviewBox), pDescriptionLabel, FALSE, FALSE, 0);
				_pack_in_widget_box (pPreviewBox);
				
				//\______________ On recupere les themes.
				if (pAuthorizedValuesList != NULL)
				{
					gchar *cShareThemesDir = NULL, *cUserThemesDir = NULL, *cDistantThemesDir = NULL;
					if (pAuthorizedValuesList[0] != NULL)
					{
						cShareThemesDir = (*pAuthorizedValuesList[0] != '\0' ? pAuthorizedValuesList[0] : NULL);
						if (pAuthorizedValuesList[1] != NULL)
						{
							cUserThemesDir = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, pAuthorizedValuesList[1]);
							cDistantThemesDir = pAuthorizedValuesList[2];
						}
					}
					GHashTable *pThemeTable = cairo_dock_list_themes (cShareThemesDir, cUserThemesDir, cDistantThemesDir);
					g_free (cUserThemesDir);

					g_hash_table_foreach (pThemeTable, (GHFunc)_cairo_dock_fill_modele_with_themes, modele);
					g_hash_table_destroy (pThemeTable);
					
					GtkTreeIter iter;
					if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (modele), &iter))
						gtk_tree_selection_select_iter (selection, &iter);
					
				}
			break ;
			
			
			case 'r' :  // deprecated.
				cd_warning ("\nTHIS CONF FILE IS OUT OF DATE\n");
			break ;
			
			case CAIRO_DOCK_WIDGET_STRING_ENTRY :  // string
			case CAIRO_DOCK_WIDGET_PASSWORD_ENTRY :  // string de type "password", crypte dans le .conf et cache dans l'UI (Merci Tofe !) :-)
			case CAIRO_DOCK_WIDGET_FILE_SELECTOR :  // string avec un selecteur de fichier a cote du GtkEntry.
			case CAIRO_DOCK_WIDGET_FOLDER_SELECTOR :  // string avec un selecteur de repertoire a cote du GtkEntry.
			case CAIRO_DOCK_WIDGET_SOUND_SELECTOR :  // string avec un selecteur de fichier a cote du GtkEntry et un boutton play.
			case CAIRO_DOCK_WIDGET_FONT_SELECTOR :  // string avec un selecteur de font a cote du GtkEntry.
			case CAIRO_DOCK_WIDGET_SHORTKEY_SELECTOR :  // string avec un selecteur de touche clavier (Merci Ctaf !)
				// on construit l'entree de texte.
				cValue = g_key_file_get_locale_string (pKeyFile, cGroupName, cKeyName, NULL, NULL);  // nous permet de recuperer les ';' aussi.
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
				if (iElementType == CAIRO_DOCK_WIDGET_FILE_SELECTOR || iElementType == CAIRO_DOCK_WIDGET_FOLDER_SELECTOR || iElementType == CAIRO_DOCK_WIDGET_SOUND_SELECTOR)  // on ajoute un selecteur de fichier.
				{
					_allocate_new_buffer;
					data[0] = pEntry;
					data[1] = GINT_TO_POINTER (iElementType != CAIRO_DOCK_WIDGET_SOUND_SELECTOR ? (iElementType == CAIRO_DOCK_WIDGET_FILE_SELECTOR ? 0 : 1) : 0);
					data[2] = pMainWindow;
					pButtonFileChooser = gtk_button_new_from_stock (GTK_STOCK_OPEN);
					g_signal_connect (G_OBJECT (pButtonFileChooser),
						"clicked",
						G_CALLBACK (_cairo_dock_pick_a_file),
						data);
					_pack_in_widget_box (pButtonFileChooser);
					if (iElementType == CAIRO_DOCK_WIDGET_SOUND_SELECTOR) //Sound Play Button
					{
						pButtonPlay = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PLAY); //Outch
						g_signal_connect (G_OBJECT (pButtonPlay),
							"clicked",
							G_CALLBACK (_cairo_dock_play_a_sound),
							data);
						_pack_in_widget_box (pButtonPlay);
					}
				}
				else if (iElementType == CAIRO_DOCK_WIDGET_FONT_SELECTOR)  // on ajoute un selecteur de font.
				{
					pFontButton = gtk_font_button_new_with_font (gtk_entry_get_text (GTK_ENTRY (pEntry)));
					gtk_font_button_set_show_style (GTK_FONT_BUTTON (pFontButton), FALSE);
					gtk_font_button_set_show_size (GTK_FONT_BUTTON (pFontButton), FALSE);
					g_signal_connect (G_OBJECT (pFontButton),
						"font-set",
						G_CALLBACK (_cairo_dock_set_font),
						pEntry);
					_pack_in_widget_box (pFontButton);
				}
				else if (iElementType == CAIRO_DOCK_WIDGET_SHORTKEY_SELECTOR)  // on ajoute un selecteur de touches.
				{
					GtkWidget *pGrabKeyButton = gtk_button_new_with_label(_("grab"));

					_allocate_new_buffer;
					data[0] = pOneWidget;
					data[1] = pMainWindow;
					gtk_widget_add_events(pMainWindow, GDK_KEY_PRESS_MASK);
					g_signal_connect (G_OBJECT (pGrabKeyButton),
						"clicked",
						G_CALLBACK (_cairo_dock_key_grab_clicked),
						data);
					
					_pack_in_widget_box (pGrabKeyButton);
				}
			break;

			case CAIRO_DOCK_WIDGET_EMPTY_WIDGET :  // container pour widget personnalise.
				pOneWidget = gtk_hbox_new (0, FALSE);
				pSubWidgetList = g_slist_append (pSubWidgetList, pOneWidget);
				gtk_box_pack_start(GTK_BOX (pFrameVBox != NULL ? pFrameVBox : pGroupBox),
					pOneWidget,
					FALSE,
					FALSE,
					0);
			break ;
			
			case CAIRO_DOCK_WIDGET_TEXT_LABEL :  // juste le label de texte.
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
						pLabelContainer = gtk_hbox_new (FALSE, CAIRO_DOCK_ICON_MARGIN/2);
						GtkWidget *pImage = gtk_image_new ();
						GdkPixbuf *pixbuf;
						if (*cSmallIcon != '/')
							pixbuf = gtk_widget_render_icon (pImage,
								cSmallIcon ,
								GTK_ICON_SIZE_MENU,
								NULL);
						else
							pixbuf = gdk_pixbuf_new_from_file_at_size (cSmallIcon, CAIRO_DOCK_FRAME_ICON_SIZE, CAIRO_DOCK_FRAME_ICON_SIZE, NULL);
						if (pixbuf != NULL)
						{
							gtk_image_set_from_pixbuf (GTK_IMAGE (pImage), pixbuf);
							gdk_pixbuf_unref (pixbuf);
						}
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

					pFrameVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
					gtk_container_add (GTK_CONTAINER (pFrame),
						pFrameVBox);
					
					if (pAuthorizedValuesList[0] == NULL || *pAuthorizedValuesList[0] == '\0')
						g_free (cValue);
				}
				break;

			case CAIRO_DOCK_WIDGET_SEPARATOR :  // separateur.
			{
				GtkWidget *pAlign = gtk_alignment_new (.5, 0., 0.5, 0.);
				pOneWidget = gtk_hseparator_new ();
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
			break ;
		}

		if (pSubWidgetList != NULL)
		{
			pGroupKeyWidget = g_new (gpointer, 6);
			pGroupKeyWidget[0] = g_strdup (cGroupName);  // car on ne pourra pas le liberer s'il est partage entre plusieurs 'data'.
			pGroupKeyWidget[1] = cKeyName;
			pGroupKeyWidget[2] = pSubWidgetList;
			pGroupKeyWidget[3] = (gchar *)cOriginalConfFilePath;
			pGroupKeyWidget[4] = pLabel;
			pGroupKeyWidget[5] = pKeyBox;  // on ne peut pas remonter a la keybox a partir du label a cause du cas particulier des widgets a prevue.
			*pWidgetList = g_slist_prepend (*pWidgetList, pGroupKeyWidget);
			if (bAddBackButton && cOriginalConfFilePath != NULL)
			{
				pBackButton = gtk_button_new ();
				GtkWidget *pImage = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);  // gtk_image_new_from_stock (GTK_STOCK_UNDO, GTK_ICON_SIZE_BUTTON);
				gtk_button_set_image (GTK_BUTTON (pBackButton), pImage);
				g_signal_connect (G_OBJECT (pBackButton), "clicked", G_CALLBACK(_cairo_dock_set_original_value), pGroupKeyWidget);
				_pack_in_widget_box (pBackButton);
			}
		}

		g_strfreev (pAuthorizedValuesList);
		g_free (cKeyComment);
	}
	g_free (cGroupComment);  // cSmallGroupIcon et cDisplayedGroupName pointaient dessus.
	g_free (pKeyList);  // on libere juste la liste de chaines, pas les chaines a l'interieur.
	
	return pGroupBox;
}


GtkWidget *cairo_dock_build_key_file_widget (GKeyFile* pKeyFile, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath)
{
	gsize length = 0;
	gchar **pGroupList = g_key_file_get_groups (pKeyFile, &length);
	g_return_val_if_fail (pGroupList != NULL, NULL);
	
	GtkWidget *pNoteBook = gtk_notebook_new ();
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (pNoteBook), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (pNoteBook));
	g_object_set (G_OBJECT (pNoteBook), "tab-pos", GTK_POS_TOP, NULL);
	
	GtkWidget *pGroupWidget, *pLabel, *pLabelContainer, *pAlign;
	gchar *cGroupName, *cGroupComment, *cIcon;
	int iCategory, i = 0;
	while (pGroupList[i] != NULL)
	{
		cGroupName = pGroupList[i];
		
		//\____________ On recupere les caracteristiques du groupe.
		cGroupComment  = g_key_file_get_comment (pKeyFile, cGroupName, NULL, NULL);
		iCategory = 0;
		cIcon = NULL;
		if (cGroupComment != NULL)
		{
			cGroupComment[strlen(cGroupComment)-1] = '\0';
			gchar *ptr = cGroupComment;
			if (*ptr == '!')
			{
				ptr = strrchr (cGroupComment, '\n');
				if (ptr != NULL)
					ptr ++;
				else
					ptr = cGroupComment;
			}
			if (*ptr == '[')
				ptr ++;
			
			gchar *str = strchr (ptr, ';');
			if (str != NULL)
			{
				*str = '\0';
				if (*(str-1) == ']')
					*(str-1) = '\0';
				
				cIcon = ptr;
				ptr = str+1;
				
				str = strchr (ptr, ';');
				if (str != NULL)
					*str = '\0';
				iCategory = atoi (ptr);
			}
			else
			{
				if (ptr[strlen(ptr)-1] ==  ']')
					ptr[strlen(ptr)-1] = '\0';
				cIcon = ptr;
			}
		}
		
		//\____________ On construit son widget.
		pLabel = gtk_label_new (dgettext (cGettextDomain, cGroupName));
		pLabelContainer = NULL;
		pAlign = NULL;
		if (cIcon != NULL && *cIcon != '\0')
		{
			pLabelContainer = gtk_hbox_new (FALSE, CAIRO_DOCK_ICON_MARGIN);
			pAlign = gtk_alignment_new (0., 0.5, 0., 0.);
			gtk_container_add (GTK_CONTAINER (pAlign), pLabelContainer);

			GtkWidget *pImage = gtk_image_new ();
			GdkPixbuf *pixbuf;
			if (*cIcon != '/')
				pixbuf = gtk_widget_render_icon (pImage,
					cIcon ,
					GTK_ICON_SIZE_BUTTON,
					NULL);
			else
				pixbuf = gdk_pixbuf_new_from_file_at_size (cIcon, CAIRO_DOCK_TAB_ICON_SIZE, CAIRO_DOCK_TAB_ICON_SIZE, NULL);
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
		g_free (cGroupComment);
		
		pGroupWidget = cairo_dock_build_group_widget (pKeyFile, cGroupName, cGettextDomain, pMainWindow, pWidgetList, pDataGarbage, cOriginalConfFilePath);
		
		GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), pGroupWidget);
		
		gtk_notebook_append_page (GTK_NOTEBOOK (pNoteBook), pScrolledWindow, (pAlign != NULL ? pAlign : pLabel));
		
		i ++;
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



static gboolean _cairo_dock_get_active_elements (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, GSList **pStringList)
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
static void _cairo_dock_get_each_widget_value (gpointer *data, GKeyFile *pKeyFile)
{
	gchar *cGroupName = data[0];
	gchar *cKeyName = data[1];
	GSList *pSubWidgetList = data[2];
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
	else if (GTK_IS_SPIN_BUTTON (pOneWidget) || GTK_IS_HSCALE (pOneWidget))
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
		GtkTreeIter iter;
		gchar *cValue =  NULL;
		if (GTK_IS_COMBO_BOX_ENTRY (pOneWidget))
		{
			cValue = gtk_combo_box_get_active_text (GTK_COMBO_BOX (pOneWidget));
		}
		else if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (pOneWidget), &iter))
		{
			GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (pOneWidget));
			if (model != NULL)
				gtk_tree_model_get (model, &iter, CAIRO_DOCK_MODEL_RESULT, &cValue, -1);
		}
		g_key_file_set_string (pKeyFile, cGroupName, cKeyName, (cValue != NULL ? cValue : ""));
		g_free (cValue);
	}
	else if (GTK_IS_ENTRY (pOneWidget))
	{
		gchar *cValue = NULL;
		const gchar *cWidgetValue = gtk_entry_get_text (GTK_ENTRY (pOneWidget));
		if( !gtk_entry_get_visibility(GTK_ENTRY (pOneWidget)) )
		{
			cairo_dock_encrypt_string( cWidgetValue,  &cValue );
		}
		else
		{
			cValue = g_strdup( cWidgetValue );
		}
		const gchar* const * cPossibleLocales = g_get_language_names ();
		gchar *cKeyNameFull, *cTranslatedValue;
		// g_key_file_set_locale_string ne marche pas avec une locale NULL comme le fait 'g_key_file_get_locale_string', il faut donc le faire a la main !
		
		for (i = 0; cPossibleLocales[i] != NULL; i++)  
		{
			cKeyNameFull = g_strdup_printf ("%s[%s]", cKeyName, cPossibleLocales[i]);
			cTranslatedValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyNameFull, NULL);
			g_free (cKeyNameFull);
			if (cTranslatedValue != NULL && strcmp (cTranslatedValue, "") != 0)
				{
				g_free (cTranslatedValue);
				break;
				}
			g_free (cTranslatedValue);
		}
		if (cPossibleLocales[i] != NULL)
			g_key_file_set_locale_string (pKeyFile, cGroupName, cKeyName, cPossibleLocales[i], cValue);
		else
			g_key_file_set_string (pKeyFile, cGroupName, cKeyName, cValue);

		g_free( cValue );
	}
	else if (GTK_IS_TREE_VIEW (pOneWidget))
	{
		gboolean bGetActiveOnly = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pOneWidget), "get-active-line-only"));
		GtkTreeModel *pModel = gtk_tree_view_get_model (GTK_TREE_VIEW (pOneWidget));
		gchar **tStringValues = NULL;
		
		if (bGetActiveOnly)
		{
			GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pOneWidget));
			GList *pRows = gtk_tree_selection_get_selected_rows (selection, NULL);
			iNbElements = g_list_length (pRows);
			tStringValues = g_new0 (gchar *, iNbElements + 1);
			
			i = 0;
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
			iNbElements = i;
		}
		else
		{
			GSList *pActiveElementList = NULL;
			gtk_tree_model_foreach (GTK_TREE_MODEL (pModel), (GtkTreeModelForeachFunc) _cairo_dock_get_active_elements, &pActiveElementList);
			iNbElements = g_slist_length (pActiveElementList);
			tStringValues = g_new0 (gchar *, iNbElements + 1);
			
			i = 0;
			GSList * pListElement;
			for (pListElement = pActiveElementList; pListElement != NULL; pListElement = pListElement->next)
			{
				tStringValues[i++] = pListElement->data;
			}
			g_slist_free (pActiveElementList);  // ses donnees sont dans 'tStringValues' et seront donc liberees avec.
		}
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



static void _cairo_dock_free_widget_list (gpointer *data, gpointer user_data)
{
        cd_debug ("");
        g_free (data[0]);
        g_free (data[1]);
        g_slist_free (data[2]);  // les elements de data[2] sont les widgets, et se feront liberer lors de la destruction de la fenetre.
}
void cairo_dock_free_generated_widget_list (GSList *pWidgetList)
{
        g_slist_foreach (pWidgetList, (GFunc) _cairo_dock_free_widget_list, NULL);
        g_slist_free (pWidgetList);
}


static int _cairo_dock_find_widget_from_name (gpointer *data, gpointer *pUserData)
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
GSList *cairo_dock_find_widgets_from_name (GSList *pWidgetList, const gchar *cGroupName, const gchar *cKeyName)
{
	const gchar *data[2] = {cGroupName, cKeyName};
	GSList *pElement = g_slist_find_custom (pWidgetList, data, (GCompareFunc) _cairo_dock_find_widget_from_name);
	if (pElement == NULL)
		return NULL;
	
	gpointer *pWidgetData = pElement->data;
	GSList *pSubWidgetList = pWidgetData[2];
	return pSubWidgetList;
}
GtkWidget *cairo_dock_find_widget_from_name (GSList *pWidgetList, const gchar *cGroupName, const gchar *cKeyName)
{
	GSList *pSubWidgetList = cairo_dock_find_widgets_from_name (pWidgetList, cGroupName, cKeyName);
	g_return_val_if_fail (pSubWidgetList != NULL, NULL);
	return pSubWidgetList->data;
}


void cairo_dock_fill_combo_with_themes (GtkWidget *pCombo, GHashTable *pThemeTable, gchar *cActiveTheme)
{
	GtkTreeModel *modele = gtk_combo_box_get_model (GTK_COMBO_BOX (pCombo));
	g_return_if_fail (modele != NULL);
	g_hash_table_foreach (pThemeTable, (GHFunc)_cairo_dock_fill_modele_with_themes, modele);
	
	GtkTreeIter iter;
	if (_cairo_dock_find_iter_from_name (GTK_LIST_STORE (modele), cActiveTheme, &iter))
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (pCombo), &iter);
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

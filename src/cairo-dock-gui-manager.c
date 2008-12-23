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
#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gui-callbacks.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-gui-manager.h"

#define CAIRO_DOCK_GROUP_ICON_SIZE 32
#define CAIRO_DOCK_CATEGORY_ICON_SIZE 32
#define CAIRO_DOCK_NB_BUTTONS_BY_ROW 4
#define CAIRO_DOCK_GUI_MARGIN 6
#define CAIRO_DOCK_TABLE_MARGIN 12
#define CAIRO_DOCK_CONF_PANEL_WIDTH 1096
#define CAIRO_DOCK_CONF_PANEL_HEIGHT 700
#define CAIRO_DOCK_PREVIEW_WIDTH 250
#define CAIRO_DOCK_PREVIEW_HEIGHT 250

static CairoDockCategoryWidgetTable s_pCategoryWidgetTables[CAIRO_DOCK_NB_CATEGORY];
static GList *s_pGroupDescriptionList = NULL;
static GtkWidget *s_pDescriptionTextView = NULL;
static GtkWidget *s_pPreviewImage = NULL;
static GtkWidget *s_pOkButton = NULL;
static GtkWidget *s_pApplyButton = NULL;
static GtkWidget *s_pBackButton = NULL;
static GtkTooltips *s_pToolTipsGroup = NULL;
static GtkWidget *s_pMainWindow = NULL;
static GtkWidget *s_pGroupsVBox = NULL;
static CairoDockGroupDescription *s_pCurrentGroup = NULL;
static GtkWidget *s_pCurrentGroupWidget = NULL;
static GSList *s_pCurrentWidgetList = NULL;
static GtkWidget *s_pToolBar = NULL;
static GtkWidget *s_pGroupFrame = NULL;
static GtkWidget *s_pActivateButton = NULL;
static GSList *s_path = NULL;

extern gchar *g_cConfFile;
extern CairoDock *g_pMainDock;
extern int g_iScreenWidth[2], g_iScreenHeight[2];

gchar *cCategoriesDescription[2*CAIRO_DOCK_NB_CATEGORY] = {
	N_("Behaviour"), "gtk-preferences",
	N_("Theme"), "gtk-color-picker",
	N_("Accessories"), "gtk-dnd-multiple",
	N_("Desktop"), "gtk-file",
	N_("Controlers"), "gtk-zoom-fit",
	N_("Plug-ins"), "gtk-disconnect" };


static int iNbConfigDialogs = 0;
int cairo_dock_get_nb_config_panels (void)
{
	return iNbConfigDialogs;
}
void cairo_dock_config_panel_destroyed (void)
{
	iNbConfigDialogs --;
	if (iNbConfigDialogs <= 0)
	{
		iNbConfigDialogs = 0;
		if (g_pMainDock != NULL)  // peut arriver au 1er lancement.
			cairo_dock_pop_down (g_pMainDock);
	}
	g_print ("iNbConfigDialogs <- %d\n", iNbConfigDialogs);
	
}
void cairo_dock_config_panel_created (void)
{
	iNbConfigDialogs ++;
	g_print ("iNbConfigDialogs <- %d\n", iNbConfigDialogs);
}


static void _cairo_dock_add_image_on_button (GtkWidget *pButton, const gchar *cImage, int iSize)
{
	if (cImage == NULL)
		return ;
	
	GtkWidget *pImage = NULL;
	if (*cImage != '/')
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
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cImage, iSize, iSize, NULL);
		if (pixbuf != NULL)
		{
			pImage = gtk_image_new_from_pixbuf (pixbuf);
			gdk_pixbuf_unref (pixbuf);
		}
	}
	if (pImage != NULL)
		gtk_button_set_image (GTK_BUTTON (pButton), pImage);  /// unref l'image ?...
}
static GtkToolItem *_cairo_dock_make_toolbutton (const gchar *cLabel, const gchar *cImage, int iSize)
{
	if (cImage == NULL)
		return gtk_tool_button_new (NULL, cLabel);
	
	GtkWidget *pImage = NULL;
	if (*cImage != '/')
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
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cImage, iSize, iSize, NULL);
		if (pixbuf != NULL)
		{
			pImage = gtk_image_new_from_pixbuf (pixbuf);
			gdk_pixbuf_unref (pixbuf);
		}
	}
	
	GtkToolItem *pWidget = gtk_tool_button_new (pImage, NULL);
	if (cLabel == NULL)
		return pWidget;
	
	GtkWidget *pLabel = gtk_label_new (NULL);
	gchar *cLabel2 = g_strdup_printf ("<span font_desc=\"Times New Roman italic 20\">%c</span>%s", *cLabel, cLabel+1);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel2);
	g_free (cLabel2);
	
	GtkWidget *pAlign = gtk_alignment_new (0., 0.5, 0., 1.);
	gtk_alignment_set_padding (GTK_ALIGNMENT (pAlign), 0, 0, CAIRO_DOCK_GUI_MARGIN*2, 0);
	gtk_container_add (GTK_CONTAINER (pAlign), pLabel);
	gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (pWidget), pAlign);
	
	return pWidget;
}

static void _cairo_dock_add_group_button (gchar *cGroupName, gchar *cIcon, int iCategory, gchar *cDescription, gchar *cPreviewFilePath, int iActivation, gboolean bConfigurable, gchar *cOriginalConfFilePath)
{
	//\____________ On garde une trace de ses caracteristiques.
	CairoDockGroupDescription *pGroupDescription = g_new0 (CairoDockGroupDescription, 1);
	pGroupDescription->cGroupName = g_strdup (cGroupName);
	pGroupDescription->cDescription = g_strdup (cDescription);
	pGroupDescription->iCategory = iCategory;
	pGroupDescription->cPreviewFilePath = g_strdup (cPreviewFilePath);
	s_pGroupDescriptionList = g_list_prepend (s_pGroupDescriptionList, pGroupDescription);
	pGroupDescription->cOriginalConfFilePath = g_strdup (cOriginalConfFilePath);
	pGroupDescription->cIcon = g_strdup (cIcon);
	
	//\____________ On construit le bouton du groupe.
	GtkWidget *pGroupButton, *pGroupLabel, *pGroupHBox;
	pGroupButton = gtk_button_new ();
	if (bConfigurable)
		g_signal_connect (G_OBJECT (pGroupButton), "clicked", G_CALLBACK(on_click_group_button), pGroupDescription);
	else
		gtk_widget_set_sensitive (pGroupButton, FALSE);
	g_signal_connect (G_OBJECT (pGroupButton), "enter", G_CALLBACK(on_enter_group_button), pGroupDescription);
	g_signal_connect (G_OBJECT (pGroupButton), "leave", G_CALLBACK(on_leave_group_button), NULL);
	pGroupLabel = gtk_label_new (gettext (cGroupName));
	_cairo_dock_add_image_on_button (pGroupButton,
		cIcon,
		CAIRO_DOCK_GROUP_ICON_SIZE);
	
	pGroupHBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	
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
	
	gtk_box_pack_start (GTK_BOX (pGroupHBox),
		pGroupButton,
		FALSE,
		FALSE,
		0);
	gtk_box_pack_start (GTK_BOX (pGroupHBox),
		pGroupLabel,
		FALSE,
		FALSE,
		0);

	//\____________ On place le bouton dans sa table.
	CairoDockCategoryWidgetTable *pCategoryWidget = &s_pCategoryWidgetTables[iCategory];
	if (pCategoryWidget->iNbItemsInCurrentRow == CAIRO_DOCK_NB_BUTTONS_BY_ROW)
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
}

static gboolean _cairo_dock_add_one_module_widget (gchar *cModuleName, CairoDockModule *pModule, gchar *cActiveModules)
{
	if (pModule->cConfFilePath == NULL)
		pModule->cConfFilePath = cairo_dock_check_module_conf_file (pModule->pVisitCard);
	int iActive;
	if (! pModule->pInterface->stopModule)
		iActive = -1;
	else if (g_pMainDock == NULL && cActiveModules != NULL)
	{
		gchar *str = g_strstr_len (cActiveModules, strlen (cActiveModules), cModuleName);
		iActive = (str != NULL &&
			(str[strlen(cModuleName)] == '\0' || str[strlen(cModuleName)] == ';') &&
			(str == cActiveModules || *(str-1) == ';'));
	}
	else
		iActive = (pModule->pInstancesList != NULL);
	gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", pModule->pVisitCard->cShareDataDir, pModule->pVisitCard->cConfFileName);
	_cairo_dock_add_group_button (cModuleName,
		pModule->pVisitCard->cIconFilePath,
		pModule->pVisitCard->iCategory,
		pModule->pVisitCard->cReadmeFilePath,
		pModule->pVisitCard->cPreviewFilePath,
		iActive,
		pModule->cConfFilePath != NULL,
		cOriginalConfFilePath);
	return FALSE;
}

static gboolean on_expose (GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	gpointer data)
{
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
GtkWidget *cairo_dock_build_main_ihm (gchar *cConfFilePath, gboolean bMaintenanceMode)
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
	
	GtkWidget *pMainHBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_container_add (GTK_CONTAINER (s_pMainWindow), pMainHBox);
	gtk_container_set_border_width (GTK_CONTAINER (pMainHBox), CAIRO_DOCK_GUI_MARGIN);
	
	GtkWidget *pCategoriesVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_widget_set_size_request (pCategoriesVBox, CAIRO_DOCK_PREVIEW_WIDTH+2*CAIRO_DOCK_GUI_MARGIN, CAIRO_DOCK_PREVIEW_HEIGHT);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pCategoriesVBox,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_TABLE_MARGIN);
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
	
	cairo_dock_set_colormap_for_window (s_pMainWindow);
	gtk_widget_set_app_paintable (s_pMainWindow, TRUE);
	g_signal_connect (G_OBJECT (s_pMainWindow),
		"expose-event",
		G_CALLBACK (on_expose),
		NULL);
	
	/*g_signal_connect (G_OBJECT (s_pGroupsVBox),
		"expose-event",
		G_CALLBACK (on_expose),
		NULL);*/
	
	s_pToolTipsGroup = gtk_tooltips_new ();
	gtk_tooltips_enable (GTK_TOOLTIPS (s_pToolTipsGroup));
	
	
	//\_____________ On construit les boutons de chaque categorie.
	GtkWidget *pCategoriesFrame = gtk_frame_new (NULL);
	gtk_container_set_border_width (GTK_CONTAINER (pCategoriesFrame), CAIRO_DOCK_GUI_MARGIN);
	gtk_frame_set_shadow_type (GTK_FRAME (pCategoriesFrame), GTK_SHADOW_OUT);
	
	GtkWidget *pLabel;
	gchar *cLabel = g_strdup_printf ("<span font_desc=\"Times New Roman italic 15\" color=\"#6B2E96\"><b><u>%s :</u></b></span>", _("Categories"));
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
	
	const gchar *cCategoryName;
	GtkToolItem *pCategoryButton;
	pCategoryButton = _cairo_dock_make_toolbutton (_("All"),
		"gtk-file",
		CAIRO_DOCK_CATEGORY_ICON_SIZE);
	g_signal_connect (G_OBJECT (pCategoryButton), "clicked", G_CALLBACK(on_click_all_button), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (s_pToolBar) , pCategoryButton,-1);
	
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryButton = _cairo_dock_make_toolbutton (gettext (cCategoriesDescription[2*i]),
			cCategoriesDescription[2*i+1],
			CAIRO_DOCK_CATEGORY_ICON_SIZE);
		g_signal_connect (G_OBJECT (pCategoryButton), "clicked", G_CALLBACK(on_click_category_button), GINT_TO_POINTER (i));
		gtk_toolbar_insert (GTK_TOOLBAR (s_pToolBar) , pCategoryButton,-1);
	}
	
	
	//\_____________ On construit les widgets table de chaque categorie.
	CairoDockCategoryWidgetTable *pCategoryWidget;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		
		pCategoryWidget->pFrame = gtk_frame_new (NULL);
		gtk_container_set_border_width (GTK_CONTAINER (pCategoryWidget->pFrame), CAIRO_DOCK_GUI_MARGIN);
		gtk_frame_set_shadow_type (GTK_FRAME (pCategoryWidget->pFrame), GTK_SHADOW_OUT);
		
		pLabel = gtk_label_new (NULL);
		cLabel = g_strdup_printf ("<span font_desc=\"Times New Roman italic 15\"><b>%s</b></span>", gettext (cCategoriesDescription[2*i]));
		gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
		g_free (cLabel);
		gtk_frame_set_label_widget (GTK_FRAME (pCategoryWidget->pFrame), pLabel);
		
		pCategoryWidget->pTable = gtk_table_new (1,
			CAIRO_DOCK_NB_BUTTONS_BY_ROW,
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
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cConfFilePath, erreur->message);
		g_error_free (erreur);
		return NULL;
	}
	
	gsize length = 0;
	gchar **pGroupList = g_key_file_get_groups (pKeyFile, &length);
	int iCategory;
	gchar *cGroupName, *cGroupComment, *cIcon, *cDescription;
	GtkWidget *pGroupButton, *pGroupLabel, *pGroupHBox;
	CairoDockGroupDescription *pGroupDescription;
	gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_CONF_FILE);
	i = 0;
	while (pGroupList[i] != NULL)
	{
		cGroupName = pGroupList[i];
		
		//\____________ On recupere les caracteristiqes du groupe.
		cGroupComment  = g_key_file_get_comment (pKeyFile, cGroupName, NULL, NULL);
		iCategory = 0;
		cIcon = NULL;
		cDescription = NULL;
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
			gchar *str = strchr (ptr, ';');
			if (str != NULL)
			{
				*str = '\0';
				cIcon = ptr;
				ptr = str+1;
				
				str = strchr (ptr, ';');
				if (str != NULL)
				{
					*str = '\0';
					iCategory = atoi (ptr);
					ptr = str+1;
					
					cDescription = ptr;
				}
				else
					iCategory = atoi (ptr);
			}
			else
				cIcon = ptr;
		}
		
		_cairo_dock_add_group_button (cGroupName, cIcon, iCategory, cDescription, NULL, -1, TRUE, cOriginalConfFilePath);
		
		g_free (cGroupComment);
		i ++;
	}
	g_free (cOriginalConfFilePath);
	g_strfreev (pGroupList);
	
	
	//\_____________ On remplit avec les modules.
	gchar *cActiveModules = (g_pMainDock == NULL ? g_key_file_get_string (pKeyFile, "System", "modules", NULL) : NULL);
	cairo_dock_foreach_module ((GHRFunc) _cairo_dock_add_one_module_widget, cActiveModules);
	g_free (cActiveModules);
	g_key_file_free (pKeyFile);
	
	
	//\_____________ On ajoute les zones d'infos.
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
	
	GtkWidget *pInfoVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pInfoVBox,
		FALSE,
		FALSE,
		0);
	
	s_pPreviewImage = gtk_image_new_from_pixbuf (NULL);
	gtk_container_add (GTK_CONTAINER (pInfoVBox), s_pPreviewImage);
	
	s_pDescriptionTextView = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (s_pDescriptionTextView), GTK_WRAP_WORD);
	gtk_container_add (GTK_CONTAINER (pInfoVBox), s_pDescriptionTextView);
	
	
	//\_____________ On ajoute les boutons.
	GtkWidget *pButtonsHBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN*3);
	gtk_box_pack_end (GTK_BOX (pVBox),
		pButtonsHBox,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pQuitButton = gtk_button_new_from_stock (GTK_STOCK_QUIT);
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
	
	
	gtk_window_resize (GTK_WINDOW (s_pMainWindow), MIN (CAIRO_DOCK_CONF_PANEL_WIDTH, g_iScreenWidth[CAIRO_DOCK_HORIZONTAL]), MIN (CAIRO_DOCK_CONF_PANEL_HEIGHT, g_iScreenHeight[CAIRO_DOCK_HORIZONTAL]-20));
	
	gtk_widget_show_all (s_pMainWindow);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pGroupFrame);
	gtk_widget_hide (s_pDescriptionTextView);
	gtk_widget_hide (s_pPreviewImage);
	
	cairo_dock_config_panel_created ();
	
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
		gtk_window_set_title (GTK_WINDOW (s_pMainWindow), _("Configuration of Cairo-Dock"));
		g_signal_connect (G_OBJECT (s_pMainWindow),
			"delete-event",
			G_CALLBACK (on_delete_main_gui),
			NULL);
	}
	return s_pMainWindow;
}


GtkWidget *cairo_dock_get_preview_image (void)
{
	return s_pPreviewImage;
}

GtkWidget *cairo_dock_get_description_label (void)
{
	return s_pDescriptionTextView;
}

GtkTooltips *cairo_dock_get_main_tooltips (void)
{
	return s_pToolTipsGroup;
}

GtkWidget *cairo_dock_get_main_window (void)
{
	return s_pMainWindow;
}

CairoDockGroupDescription *cairo_dock_get_current_group (void)
{
	return s_pCurrentGroup;
}


void cairo_dock_hide_all_categories (void)
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

void cairo_dock_show_all_categories (void)
{
	if (s_pCurrentGroupWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentGroupWidget);
		s_pCurrentGroupWidget = NULL;
		cairo_dock_free_generated_widget_list (s_pCurrentWidgetList);
		s_pCurrentWidgetList = NULL;
		s_pCurrentGroup = NULL;
	}
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		gtk_widget_show (pCategoryWidget->pFrame);
	}
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pGroupFrame);
	
	gtk_window_set_title (GTK_WINDOW (cairo_dock_get_main_window ()), _("Configuration of Cairo-Dock"));
	if (s_path != NULL && s_path->next != NULL && s_path->next->data == GINT_TO_POINTER (0))
		s_path = g_slist_delete_link (s_path, s_path);
	else
		s_path = g_slist_prepend (s_path, GINT_TO_POINTER (0));
}

void cairo_dock_show_one_category (int iCategory)
{
	if (s_pCurrentGroupWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentGroupWidget);
		s_pCurrentGroupWidget = NULL;
		cairo_dock_free_generated_widget_list (s_pCurrentWidgetList);
		s_pCurrentWidgetList = NULL;
		s_pCurrentGroup = NULL;
	}
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		if (i != iCategory)
			gtk_widget_hide (pCategoryWidget->pFrame);
		else
			gtk_widget_show (pCategoryWidget->pFrame);
	}
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pGroupFrame);
	
	gtk_window_set_title (GTK_WINDOW (cairo_dock_get_main_window ()), gettext (cCategoriesDescription[2*iCategory]));
	if (s_path != NULL && s_path->next != NULL && s_path->next->data == GINT_TO_POINTER (iCategory+1))
		s_path = g_slist_delete_link (s_path, s_path);
	else
		s_path = g_slist_prepend (s_path, GINT_TO_POINTER (iCategory+1));
}

void cairo_dock_insert_extern_widget_in_gui (GtkWidget *pWidget)
{
	if (s_pCurrentGroupWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentGroupWidget);
	}
	s_pCurrentGroupWidget = pWidget;
	
	gtk_box_pack_start (GTK_BOX (s_pGroupsVBox),
		pWidget,
		FALSE,
		FALSE,
		CAIRO_DOCK_GUI_MARGIN);
	gtk_widget_show_all (pWidget);
}

void cairo_dock_present_group_widget (gchar *cConfFilePath, CairoDockGroupDescription *pGroupDescription, gboolean bSingleGroup)
{
	g_print ("%s (%s, %s)\n", __func__, cConfFilePath, pGroupDescription->cGroupName);
	g_free (pGroupDescription->cConfFilePath);
	pGroupDescription->cConfFilePath = g_strdup (cConfFilePath);
	
	//\_______________ On cree le widget du groupe.
	GError *erreur = NULL;
	GKeyFile* pKeyFile = g_key_file_new();
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cConfFilePath, erreur->message);
		g_error_free (erreur);
		return ;
	}

	GPtrArray *pDataGarbage = g_ptr_array_new ();
	GtkWidget *pWidget;
	if (bSingleGroup)
	{
		pWidget = cairo_dock_build_group_widget (pKeyFile,
			pGroupDescription->cGroupName,
			cairo_dock_get_main_tooltips (),
			NULL,
			cairo_dock_get_main_window (),
			&s_pCurrentWidgetList,
			pDataGarbage,
			pGroupDescription->cOriginalConfFilePath);
	}
	else
	{
		pWidget = cairo_dock_build_conf_file_widget (cConfFilePath,
			cairo_dock_get_main_tooltips (),
			NULL,
			cairo_dock_get_main_window (),
			&s_pCurrentWidgetList,
			pDataGarbage,
			pGroupDescription->cOriginalConfFilePath);
	}
	
	//\_______________ On affiche le widget du groupe.
	cairo_dock_hide_all_categories ();
	
	cairo_dock_insert_extern_widget_in_gui (pWidget);
	
	s_pCurrentGroup = pGroupDescription;
	
	gtk_window_set_title (GTK_WINDOW (cairo_dock_get_main_window ()), gettext (pGroupDescription->cGroupName));
	
	//\_______________ On met a jour la frame du groupe.
	GtkWidget *pLabel = gtk_label_new (NULL);
	gchar *cLabel = g_strdup_printf ("<span font_desc=\"Times New Roman italic 12\" color=\"#6B2E96\"><u><b>%s</b></u></span>", pGroupDescription->cGroupName);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
	g_free (cLabel);
	gtk_frame_set_label_widget (GTK_FRAME (s_pGroupFrame), pLabel);
	gtk_widget_show_all (s_pGroupFrame);
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	g_signal_handlers_block_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
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
	if (s_path != NULL && s_path->next != NULL && s_path->next->data == pGroupDescription)
		s_path = g_slist_delete_link (s_path, s_path);
	else
		s_path = g_slist_prepend (s_path, pGroupDescription);
}


void cairo_dock_present_module_gui (CairoDockModule *pModule)
{
	g_return_if_fail (pModule != NULL);
	
	CairoDockGroupDescription *pGroupDescription = NULL;
	GList *pElement = NULL;
	for (pElement = s_pGroupDescriptionList; pElement != NULL; pElement = pElement->next)
	{
		pGroupDescription = pElement->data;
		if (strcmp (pGroupDescription->cGroupName, pModule->pVisitCard->cModuleName) == 0)
			break ;
	}
	if (pElement == NULL)
		return ;
	
	cairo_dock_present_group_widget (pModule->cConfFilePath, pGroupDescription, FALSE);
}

void cairo_dock_present_module_instance_gui (CairoDockModuleInstance *pModuleInstance)
{
	g_return_if_fail (pModuleInstance != NULL);
	
	CairoDockGroupDescription *pGroupDescription = NULL;
	GList *pElement = NULL;
	for (pElement = s_pGroupDescriptionList; pElement != NULL; pElement = pElement->next)
	{
		pGroupDescription = pElement->data;
		if (strcmp (pGroupDescription->cGroupName, pModuleInstance->pModule->pVisitCard->cModuleName) == 0)
			break ;
	}
	if (pElement == NULL)
		return ;
	
	cairo_dock_present_group_widget (pModuleInstance->cConfFilePath, pGroupDescription, FALSE);
}



void cairo_dock_free_categories (void)
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
	
	s_pDescriptionTextView = NULL;
	s_pPreviewImage = NULL;
	
	s_pOkButton = NULL;
	s_pApplyButton = NULL;
	
	s_pToolTipsGroup = NULL;
	s_pMainWindow = NULL;
	s_pToolBar = NULL;
	
	if (s_pCurrentGroup != NULL)
	{
		g_free (s_pCurrentGroup->cConfFilePath);
		s_pCurrentGroup->cConfFilePath = NULL;
		s_pCurrentGroup = NULL;
	}
	cairo_dock_free_generated_widget_list (s_pCurrentWidgetList);
	s_pCurrentWidgetList = NULL;
	s_pCurrentGroupWidget = NULL;  // detruit en meme temps que la fenetre.
	g_slist_free (s_path);
	s_path = NULL;
	cairo_dock_config_panel_destroyed ();
}



void cairo_dock_write_current_group_conf_file (gchar *cConfFilePath)
{
	g_return_if_fail (cConfFilePath != NULL && s_pCurrentWidgetList != NULL);
	GKeyFile *pKeyFile = g_key_file_new ();

	GError *erreur = NULL;
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return ;
	}

	cairo_dock_update_keyfile_from_widget_list (pKeyFile, s_pCurrentWidgetList);
	cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	g_key_file_free (pKeyFile);
}


void cairo_dock_reload_current_group_widget (void)
{
	g_return_if_fail (s_pCurrentGroupWidget != NULL && s_pCurrentGroup != NULL && s_pCurrentWidgetList != NULL);
	
	gtk_widget_destroy (s_pCurrentGroupWidget);
	s_pCurrentGroupWidget = NULL;
	
	cairo_dock_free_generated_widget_list (s_pCurrentWidgetList);
	s_pCurrentWidgetList = NULL;
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (s_pCurrentGroup->cGroupName);
	if (pModule != NULL)
	{
		cairo_dock_present_group_widget (pModule->cConfFilePath, s_pCurrentGroup, FALSE);
	}
	else
	{
		cairo_dock_present_group_widget (g_cConfFile, s_pCurrentGroup, TRUE);
	}
}



gboolean cairo_dock_build_normal_gui (gchar *cConfFilePath, const gchar *cGettextDomain, const gchar *cTitle, int iWidth, int iHeight, CairoDockApplyConfigFunc pAction, gpointer pUserData, GFreeFunc pFreeUserData)
{
	//\_____________ On construit la fenetre.
	GtkWidget *pMainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);
	gtk_window_set_icon_from_file (GTK_WINDOW (pMainWindow), cIconPath, NULL);
	g_free (cIconPath);
	if (cTitle != NULL)
		gtk_window_set_title (GTK_WINDOW (pMainWindow), cTitle);
	
	GtkWidget *pMainVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_container_add (GTK_CONTAINER (pMainWindow), pMainVBox);
	
	GtkTooltips *pToolTipsGroup = gtk_tooltips_new ();
	gtk_tooltips_enable (GTK_TOOLTIPS (pToolTipsGroup));
	
	//\_____________ On construit l'IHM du fichier de conf.
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	GtkWidget *pNoteBook = cairo_dock_build_conf_file_widget (cConfFilePath,
		pToolTipsGroup,
		cGettextDomain,
		pMainWindow,
		&pWidgetList,
		pDataGarbage,
		NULL);
	gtk_box_pack_start (GTK_BOX (pMainVBox),
		pNoteBook,
		FALSE,
		FALSE,
		0);
	
	//\_____________ On ajoute les boutons.
	GtkWidget *pButtonsHBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN*2);
	gtk_box_pack_end (GTK_BOX (pMainVBox),
		pButtonsHBox,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pQuitButton = gtk_button_new_from_stock (GTK_STOCK_QUIT);
	g_signal_connect (G_OBJECT (pQuitButton), "clicked", G_CALLBACK(on_click_normal_quit), pMainWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		pQuitButton,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pOkButton = gtk_button_new_from_stock (GTK_STOCK_OK);
	g_signal_connect (G_OBJECT (pOkButton), "clicked", G_CALLBACK(on_click_normal_ok), pMainWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		pOkButton,
		FALSE,
		FALSE,
		0);
	
	if (pAction != NULL)
	{
		GtkWidget *pApplyButton = gtk_button_new_from_stock (GTK_STOCK_APPLY);
		g_signal_connect (G_OBJECT (pApplyButton), "clicked", G_CALLBACK(on_click_normal_apply), pMainWindow);
		gtk_box_pack_end (GTK_BOX (pButtonsHBox),
			pApplyButton,
			FALSE,
			FALSE,
			0);
	}
	
	gtk_window_resize (GTK_WINDOW (pMainWindow), iWidth, iHeight);
	
	gtk_widget_show_all (pMainWindow);
	cairo_dock_config_panel_created ();
	
	g_object_set_data (G_OBJECT (pMainWindow), "conf-file", g_strdup (cConfFilePath));
	g_object_set_data (G_OBJECT (pMainWindow), "widget-list", pWidgetList);
	g_object_set_data (G_OBJECT (pMainWindow), "garbage", pDataGarbage);
	int iResult = 0;
	if (pAction != NULL)
	{
		g_object_set_data (G_OBJECT (pMainWindow), "action", pAction);
		g_object_set_data (G_OBJECT (pMainWindow), "action-data", pUserData);
		g_object_set_data (G_OBJECT (pMainWindow), "free-data", pFreeUserData);
		g_signal_connect (G_OBJECT (pMainWindow),
			"delete-event",
			G_CALLBACK (on_delete_normal_gui),
			NULL);
	}
	else  // on bloque.
	{
		GMainLoop *pBlockingLoop = g_main_loop_new (NULL, FALSE);
		g_object_set_data (G_OBJECT (pMainWindow), "loop", pBlockingLoop);
		g_signal_connect (G_OBJECT (pMainWindow),
			"delete-event",
			G_CALLBACK (on_delete_normal_gui),
			pBlockingLoop);

		g_print ("debut de boucle bloquante ...\n");
		GDK_THREADS_LEAVE ();
		g_main_loop_run (pBlockingLoop);
		GDK_THREADS_ENTER ();
		g_print ("fin de boucle bloquante\n");
		
		g_main_loop_unref (pBlockingLoop);
		iResult = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pMainWindow), "result"));
		g_print ("iResult : %d\n", iResult);
		gtk_widget_destroy (pMainWindow);
	}
	
	return iResult;
}


gpointer cairo_dock_get_previous_widget (void)
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
	
	return s_path->next->data;
}

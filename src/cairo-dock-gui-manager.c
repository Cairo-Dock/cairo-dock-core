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
#include "cairo-dock-dialogs.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-gui-filter.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-gui-manager.h"

#define CAIRO_DOCK_GROUP_ICON_SIZE 32
#define CAIRO_DOCK_CATEGORY_ICON_SIZE 32
#define CAIRO_DOCK_NB_BUTTONS_BY_ROW 4
#define CAIRO_DOCK_NB_BUTTONS_BY_ROW_MIN 3
#define CAIRO_DOCK_GUI_MARGIN 6
#define CAIRO_DOCK_TABLE_MARGIN 12
#define CAIRO_DOCK_CONF_PANEL_WIDTH 1150
#define CAIRO_DOCK_CONF_PANEL_WIDTH_MIN 800
#define CAIRO_DOCK_CONF_PANEL_HEIGHT 700
#define CAIRO_DOCK_PREVIEW_WIDTH 250
#define CAIRO_DOCK_PREVIEW_WIDTH_MIN 100
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_ICON_MARGIN 6
#define CAIRO_DOCK_TAB_ICON_SIZE 32

static CairoDockCategoryWidgetTable s_pCategoryWidgetTables[CAIRO_DOCK_NB_CATEGORY];
static GList *s_pGroupDescriptionList = NULL;
static GtkWidget *s_pPreviewImage = NULL;
static GtkWidget *s_pOkButton = NULL;
static GtkWidget *s_pApplyButton = NULL;
static GtkWidget *s_pBackButton = NULL;
static GtkWidget *s_pMainWindow = NULL;
static GtkWidget *s_pGroupsVBox = NULL;
static CairoDockGroupDescription *s_pCurrentGroup = NULL;
static GtkWidget *s_pCurrentGroupWidget = NULL;
static GSList *s_pCurrentWidgetList = NULL;
static GSList *s_pExtraCurrentWidgetList = NULL;
static GtkWidget *s_pToolBar = NULL;
static GtkWidget *s_pGroupFrame = NULL;
static GtkWidget *s_pFilterEntry = NULL;
static GtkWidget *s_pActivateButton = NULL;
static GtkWidget *s_pStatusBar = NULL;
static GSList *s_path = NULL;
static int s_iPreviewWidth, s_iNbButtonsByRow;

extern gchar *g_cConfFile;
extern CairoDock *g_pMainDock;
extern int g_iXScreenWidth[2], g_iXScreenHeight[2];
extern CairoDockDesktopEnv g_iDesktopEnv;

const gchar *cCategoriesDescription[2*CAIRO_DOCK_NB_CATEGORY] = {
	N_("Behaviour"), "gtk-preferences",
	N_("Appearance"), "gtk-color-picker",
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
	cd_debug ("iNbConfigDialogs <- %d", iNbConfigDialogs);
	
}
void cairo_dock_config_panel_created (void)
{
	iNbConfigDialogs ++;
	cd_debug ("iNbConfigDialogs <- %d", iNbConfigDialogs);
}


static void _cairo_dock_reset_current_widget_list (void)
{
	cairo_dock_free_generated_widget_list (s_pCurrentWidgetList);
	s_pCurrentWidgetList = NULL;
	g_slist_foreach (s_pExtraCurrentWidgetList, (GFunc) cairo_dock_free_generated_widget_list, NULL);
	g_slist_free (s_pExtraCurrentWidgetList);
	s_pExtraCurrentWidgetList = NULL;
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
	gchar *cLabel2 = g_strdup_printf ("<span font_desc=\"Century Schoolbook L 16\">%s</span>", cLabel);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel2);
	g_free (cLabel2);
	
	GtkWidget *pAlign = gtk_alignment_new (0., 0.5, 0., 1.);
	gtk_alignment_set_padding (GTK_ALIGNMENT (pAlign), 0, 0, CAIRO_DOCK_GUI_MARGIN*2, 0);
	gtk_container_add (GTK_CONTAINER (pAlign), pLabel);
	gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (pWidget), pAlign);
	
	return pWidget;
}

static inline CairoDockGroupDescription *_cairo_dock_add_group_button (const gchar *cGroupName, gchar *cIcon, int iCategory, const gchar *cDescription, const gchar *cPreviewFilePath, int iActivation, gboolean bConfigurable, const gchar *cOriginalConfFilePath, const gchar *cGettextDomain, const gchar **cDependencies, GList *pExternalModules, const gchar *cInternalModule)
{
	//\____________ On garde une trace de ses caracteristiques.
	CairoDockGroupDescription *pGroupDescription = g_new0 (CairoDockGroupDescription, 1);
	pGroupDescription->cGroupName = g_strdup (cGroupName);
	pGroupDescription->cDescription = g_strdup (cDescription);
	pGroupDescription->iCategory = iCategory;
	pGroupDescription->cPreviewFilePath = g_strdup (cPreviewFilePath);
	pGroupDescription->cOriginalConfFilePath = g_strdup (cOriginalConfFilePath);
	pGroupDescription->cIcon = g_strdup (cIcon);
	pGroupDescription->cGettextDomain = cGettextDomain;
	pGroupDescription->cDependencies = cDependencies;
	pGroupDescription->pExternalModules = pExternalModules;
	pGroupDescription->cInternalModule = cInternalModule;
	
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
	_cairo_dock_add_image_on_button (pGroupButton,
		cIcon,
		CAIRO_DOCK_GROUP_ICON_SIZE);
	gtk_box_pack_start (GTK_BOX (pGroupHBox),
		pGroupButton,
		FALSE,
		FALSE,
		0);

	pGroupDescription->pLabel = gtk_label_new (dgettext (pGroupDescription->cGettextDomain, cGroupName));
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

static gboolean _cairo_dock_add_one_module_widget (/*gchar *cModuleName, */CairoDockModule *pModule, const gchar *cActiveModules)
{
	gchar *cModuleName = pModule->pVisitCard->cModuleName;
	if (pModule->cConfFilePath == NULL)
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
	
	CairoDockGroupDescription *pGroupDescription = _cairo_dock_add_group_button (cModuleName,
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
		pModule->pVisitCard->cInternalModule);
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
GtkWidget *cairo_dock_build_main_ihm (const gchar *cConfFilePath, gboolean bMaintenanceMode)
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
	
	GtkWidget *pVBox = gtk_vbox_new (FALSE, 0*CAIRO_DOCK_TABLE_MARGIN);
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
	
	GtkToolItem *pCategoryButton;
	pCategoryButton = _cairo_dock_make_toolbutton (_("All"),
		"gtk-file",
		CAIRO_DOCK_CATEGORY_ICON_SIZE);
	g_signal_connect (G_OBJECT (pCategoryButton), "clicked", G_CALLBACK(on_click_all_button), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (s_pToolBar) , pCategoryButton,-1);
	
	guint i;
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
	CairoDockInternalModule *pInternalModule;
	gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_CONF_FILE);
	for (i = 0; i < length; i ++)
	{
		cGroupName = pGroupList[i];
		pInternalModule = cairo_dock_find_internal_module_from_name (cGroupName);
		if (pInternalModule == NULL)
			continue;
		
		_cairo_dock_add_group_button (cGroupName,
			pInternalModule->cIcon,
			pInternalModule->iCategory,
			pInternalModule->cDescription,
			NULL,  // pas de prevue
			-1,  // <=> non desactivable
			TRUE,  // <=> configurable
			cOriginalConfFilePath,
			NULL,  // domaine de traduction : celui du dock.
			pInternalModule->cDependencies,
			pInternalModule->pExternalModules,
			NULL);
	}
	g_free (cOriginalConfFilePath);
	g_strfreev (pGroupList);
	
	
	//\_____________ On remplit avec les modules.
	gchar *cActiveModules = (g_pMainDock == NULL ? g_key_file_get_string (pKeyFile, "System", "modules", NULL) : NULL);
	cairo_dock_foreach_module_in_alphabetical_order ((GCompareFunc) _cairo_dock_add_one_module_widget, cActiveModules);
	g_free (cActiveModules);
	g_key_file_free (pKeyFile);
	
	
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
	
	//\_____________ On ajoute le filtre.
	GtkWidget *pFilterFrame = gtk_frame_new (NULL);
	cLabel = g_strdup_printf ("<span font_desc=\"Times New Roman italic 15\" color=\"#6B2E96\"><b><u>%s :</u></b></span>", _("Filter"));
	pLabel = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
	g_free (cLabel);
	gtk_frame_set_label_widget (GTK_FRAME (pFilterFrame), pLabel);
	gtk_container_set_border_width (GTK_CONTAINER (s_pGroupFrame), CAIRO_DOCK_GUI_MARGIN);
	gtk_frame_set_shadow_type (GTK_FRAME (s_pGroupFrame), GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pFilterFrame,
		FALSE,
		FALSE,
		0);
	GtkWidget *pOptionVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_container_add (GTK_CONTAINER (pFilterFrame), pOptionVBox);
	
	GtkWidget *pFilterBox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pOptionVBox),
		pFilterBox,
		FALSE,
		FALSE,
		0);
	s_pFilterEntry = gtk_entry_new ();
	g_signal_connect (s_pFilterEntry, "activate", G_CALLBACK (cairo_dock_activate_filter), NULL);
	gtk_box_pack_start (GTK_BOX (pFilterBox),
		s_pFilterEntry,
		FALSE,
		FALSE,
		0);
	GtkWidget *pClearButton = gtk_button_new ();
	_cairo_dock_add_image_on_button (pClearButton,
		GTK_STOCK_CLEAR,
		16);
	g_signal_connect (pClearButton, "clicked", G_CALLBACK (cairo_dock_clear_filter), s_pFilterEntry);
	gtk_box_pack_start (GTK_BOX (pFilterBox),
		pClearButton,
		FALSE,
		FALSE,
		0);
	
	cairo_dock_reset_filter_state ();
	GtkWidget *pOptionButton = gtk_check_button_new_with_label (_("All words"));
	g_signal_connect (pOptionButton, "toggled", G_CALLBACK (cairo_dock_toggle_all_words), NULL);
	gtk_box_pack_start (GTK_BOX (pOptionVBox),
		pOptionButton,
		FALSE,
		FALSE,
		0);
	pOptionButton = gtk_check_button_new_with_label (_("Highlight words"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pOptionButton), TRUE);
	g_signal_connect (pOptionButton, "toggled", G_CALLBACK (cairo_dock_toggle_highlight_words), NULL);
	gtk_box_pack_start (GTK_BOX (pOptionVBox),
		pOptionButton,
		FALSE,
		FALSE,
		0);
	pOptionButton = gtk_check_button_new_with_label (_("Hide others"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pOptionButton), TRUE);
	g_signal_connect (pOptionButton, "toggled", G_CALLBACK (cairo_dock_toggle_hide_others), NULL);
	gtk_box_pack_start (GTK_BOX (pOptionVBox),
		pOptionButton,
		FALSE,
		FALSE,
		0);
	pOptionButton = gtk_check_button_new_with_label (_("Search in description"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pOptionButton), TRUE);
	g_signal_connect (pOptionButton, "toggled", G_CALLBACK (cairo_dock_toggle_search_in_tooltip), NULL);
	gtk_box_pack_start (GTK_BOX (pOptionVBox),
		pOptionButton,
		FALSE,
		FALSE,
		0);
	
	//\_____________ On ajoute les boutons.
	GtkWidget *pButtonsHBox = gtk_hbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
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
	
	//\_____________ On ajoute la barre de status a la fin.
	s_pStatusBar = gtk_statusbar_new ();
	gtk_box_pack_end (GTK_BOX (pVBox),  /// pButtonsHBox ?...
		s_pStatusBar,
		FALSE,
		FALSE,
		0);
	
	gtk_window_resize (GTK_WINDOW (s_pMainWindow),
		MIN (CAIRO_DOCK_CONF_PANEL_WIDTH, g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL]),
		MIN (CAIRO_DOCK_CONF_PANEL_HEIGHT, g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - (g_pMainDock && g_pMainDock->bHorizontalDock ? g_pMainDock->iMaxDockHeight : 0)));
	
	if (g_pMainDock != NULL && ! myAccessibility.bReserveSpace)  // evitons d'empieter sur le dock.
	{
		if (g_pMainDock->bHorizontalDock)
		{
			if (g_pMainDock->bDirectionUp)
				gtk_window_move (GTK_WINDOW (s_pMainWindow), 0, 0);
			else
				gtk_window_move (GTK_WINDOW (s_pMainWindow), 0, g_pMainDock->iMaxDockHeight);
		}
	}
	
	gtk_widget_show_all (s_pMainWindow);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pGroupFrame);
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


GtkWidget *cairo_dock_get_preview_image (int *iPreviewWidth)
{
	*iPreviewWidth = s_iPreviewWidth;
	return s_pPreviewImage;
}

GtkWidget *cairo_dock_get_main_window (void)
{
	return s_pMainWindow;
}

CairoDockGroupDescription *cairo_dock_get_current_group (void)
{
	return s_pCurrentGroup;
}

GSList *cairo_dock_get_current_widget_list (void)
{
	return s_pCurrentWidgetList;
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
	if (s_pStatusBar)
		gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);
	
	//\_______________ On detruit le widget du groupe courant.
	if (s_pCurrentGroupWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentGroupWidget);
		s_pCurrentGroupWidget = NULL;
		_cairo_dock_reset_current_widget_list ();
		
		s_pCurrentGroup = NULL;
	}

	//\_______________ On montre chaque module.
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		gtk_widget_show_all (pCategoryWidget->pFrame);
	}
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pGroupFrame);
	
	//\_______________ On actualise le titre de la fenetre.
	gtk_window_set_title (GTK_WINDOW (cairo_dock_get_main_window ()), _("Configuration of Cairo-Dock"));
	if (s_path == NULL || s_path->data != GINT_TO_POINTER (0))
		s_path = g_slist_prepend (s_path, GINT_TO_POINTER (0));

	//\_______________ On declenche le filtre.
	cairo_dock_trigger_current_filter ();
}

void cairo_dock_show_one_category (int iCategory)
{
	if (s_pStatusBar)
		gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);
	
	//\_______________ On detruit le widget du groupe courant.
	if (s_pCurrentGroupWidget != NULL)
	{
		gtk_widget_destroy (s_pCurrentGroupWidget);
		s_pCurrentGroupWidget = NULL;
		_cairo_dock_reset_current_widget_list ();
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
	}
	gtk_widget_hide (s_pOkButton);
	gtk_widget_hide (s_pApplyButton);
	gtk_widget_hide (s_pGroupFrame);
	
	//\_______________ On actualise le titre de la fenetre.
	gtk_window_set_title (GTK_WINDOW (cairo_dock_get_main_window ()), gettext (cCategoriesDescription[2*iCategory]));
	if (s_path == NULL || s_path->data != GINT_TO_POINTER (iCategory+1))
		s_path = g_slist_prepend (s_path, GINT_TO_POINTER (iCategory+1));

	//\_______________ On declenche le filtre.
	cairo_dock_trigger_current_filter ();
}

void cairo_dock_insert_extern_widget_in_gui (GtkWidget *pWidget)
{
	if (s_pCurrentGroupWidget != NULL)
		gtk_widget_destroy (s_pCurrentGroupWidget);
	s_pCurrentGroupWidget = pWidget;
	
	gtk_box_pack_start (GTK_BOX (s_pGroupsVBox),
		pWidget,
		FALSE,
		FALSE,
		CAIRO_DOCK_GUI_MARGIN);
	gtk_widget_show_all (pWidget);
}

GtkWidget *cairo_dock_present_group_widget (const gchar *cConfFilePath, CairoDockGroupDescription *pGroupDescription, gboolean bSingleGroup, CairoDockModuleInstance *pInstance)
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
	
	_cairo_dock_reset_current_widget_list ();
	
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	GtkWidget *pWidget;
	if (bSingleGroup)
	{
		pWidget = cairo_dock_build_group_widget (pKeyFile,
			pGroupDescription->cGroupName,
			pGroupDescription->cGettextDomain,
			cairo_dock_get_main_window (),
			&s_pCurrentWidgetList,
			pDataGarbage,
			pGroupDescription->cOriginalConfFilePath);
	}
	else
	{
		pWidget = cairo_dock_build_key_file_widget (pKeyFile,
			pGroupDescription->cGettextDomain,
			cairo_dock_get_main_window (),
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
			GPtrArray *pExtraDataGarbage = g_ptr_array_new ();
			
			pExtraCurrentWidgetList = NULL;
			pNoteBook = cairo_dock_build_key_file_widget (pExtraKeyFile,
				pExtraGroupDescription->cGettextDomain,
				cairo_dock_get_main_window (),
				&pExtraCurrentWidgetList,
				pExtraDataGarbage,
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
			GtkWidget *pLabel = gtk_label_new (dgettext (pGroupDescription->cGettextDomain, pGroupDescription->cGroupName));
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
			gtk_notebook_prepend_page (GTK_NOTEBOOK (pNoteBook), pWidget, (pAlign != NULL ? pAlign : pLabel));
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
	
	gtk_window_set_title (GTK_WINDOW (cairo_dock_get_main_window ()), gettext (pGroupDescription->cGroupName));
	
	//\_______________ On met a jour la frame du groupe (label + check-button).
	GtkWidget *pLabel = gtk_label_new (NULL);
	gchar *cLabel = g_strdup_printf ("<span font_desc=\"Times New Roman italic 15\" color=\"#6B2E96\"><u><b>%s</b></u></span>", pGroupDescription->cGroupName);
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
	cairo_dock_trigger_current_filter ();
	
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
				cairo_dock_show_temporary_dialog_with_icon (_("The module '%s' is not present. You need to install it or its dependencies to make the most of this module."), pIcon, CAIRO_CONTAINER (pDock), 10, "same icon");
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
			cairo_dock_reload_current_group_widget (NULL);
		pGroupDescription->bIgnoreDependencies = FALSE;
	}
	
	return pWidget;
}


CairoDockGroupDescription *cairo_dock_find_module_description (const gchar *cModuleName)
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

void cairo_dock_present_module_gui (CairoDockModule *pModule)
{
	g_return_if_fail (pModule != NULL);
	
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (pModule->pVisitCard->cModuleName);
	if (pGroupDescription == NULL)
		return ;
	
	CairoDockModuleInstance *pModuleInstance = (pModule->pInstancesList != NULL ? pModule->pInstancesList->data : NULL);
	gchar *cConfFilePath = (pModuleInstance != NULL ? pModuleInstance->cConfFilePath : pModule->cConfFilePath);
	cairo_dock_present_group_widget (cConfFilePath, pGroupDescription, FALSE, pModuleInstance);
}

void cairo_dock_present_module_instance_gui (CairoDockModuleInstance *pModuleInstance)
{
	g_return_if_fail (pModuleInstance != NULL);
	
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (pModuleInstance->pModule->pVisitCard->cModuleName);
	if (pGroupDescription == NULL)
		return ;
	
	cairo_dock_present_group_widget (pModuleInstance->cConfFilePath, pGroupDescription, FALSE, pModuleInstance);
}

void cairo_dock_show_group (CairoDockGroupDescription *pGroupDescription)
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
	_cairo_dock_reset_current_widget_list ();
	s_pCurrentGroupWidget = NULL;  // detruit en meme temps que la fenetre.
	g_slist_free (s_path);
	s_path = NULL;
	cairo_dock_config_panel_destroyed ();
}



void cairo_dock_write_current_group_conf_file (gchar *cConfFilePath, CairoDockModuleInstance *pInstance)
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

void cairo_dock_write_extra_group_conf_file (gchar *cConfFilePath, CairoDockModuleInstance *pInstance, int iNumExtraModule)
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



gboolean cairo_dock_build_normal_gui (gchar *cConfFilePath, const gchar *cGettextDomain, const gchar *cTitle, int iWidth, int iHeight, CairoDockApplyConfigFunc pAction, gpointer pUserData, GFreeFunc pFreeUserData, GtkWidget **pWindow)
{
	//\_____________ On construit la fenetre.
	GtkWidget *pMainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon_from_file (GTK_WINDOW (pMainWindow), CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON, NULL);
	if (cTitle != NULL)
		gtk_window_set_title (GTK_WINDOW (pMainWindow), cTitle);
	
	GtkWidget *pMainVBox = gtk_vbox_new (FALSE, CAIRO_DOCK_GUI_MARGIN);
	gtk_container_add (GTK_CONTAINER (pMainWindow), pMainVBox);
	
	//\_____________ On construit l'IHM du fichier de conf.
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	GtkWidget *pNoteBook = cairo_dock_build_conf_file_widget (cConfFilePath,
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
	
	//\_____________ On ajoute la barre d'etat a la fin.
	GtkWidget *pStatusBar = gtk_statusbar_new ();
	gtk_box_pack_end (GTK_BOX (pMainVBox),  /// pButtonsHBox ?...
		pStatusBar,
		FALSE,
		FALSE,
		0);
	g_object_set_data (G_OBJECT (pMainWindow), "status-bar", pStatusBar);
	
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
	else
	{
		GtkWidget *pOkButton = gtk_button_new_from_stock (GTK_STOCK_OK);
		g_signal_connect (G_OBJECT (pOkButton), "clicked", G_CALLBACK(on_click_normal_ok), pMainWindow);
		gtk_box_pack_end (GTK_BOX (pButtonsHBox),
			pOkButton,
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
		if (pWindow)
			*pWindow = pMainWindow;
	}
	else  // on bloque.
	{
		gtk_window_set_modal (GTK_WINDOW (pMainWindow), TRUE);
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
		cd_debug ("iResult : %d", iResult);
		gtk_widget_destroy (pMainWindow);
		if (pWindow)
			*pWindow = NULL;
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
	
	//s_path = g_list_delete_link (s_path, s_path);
	GSList *tmp = s_path;
	s_path = s_path->next;
	tmp->next = NULL;
	g_slist_free (tmp);
	
	return s_path->data;
}

gpointer cairo_dock_get_current_widget (void)
{
	if (s_path == NULL)
	{
		return 0;
	}
	
	return s_path->data;
}



void cairo_dock_reload_current_group_widget_full (CairoDockModuleInstance *pInstance, int iShowPage)
{
	g_return_if_fail (s_pCurrentGroupWidget != NULL && s_pCurrentGroup != NULL && s_pCurrentWidgetList != NULL);
	
	int iNotebookPage = (GTK_IS_NOTEBOOK (s_pCurrentGroupWidget) ? 
		(iShowPage >= 0 ?
			iShowPage :
			gtk_notebook_get_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget))) :
		-1);
	
	gtk_widget_destroy (s_pCurrentGroupWidget);
	s_pCurrentGroupWidget = NULL;
	
	_cairo_dock_reset_current_widget_list ();
	
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

GtkWidget *cairo_dock_get_widget_from_name (const gchar *cGroupName, const gchar *cKeyName)
{
	g_return_val_if_fail (s_pCurrentWidgetList != NULL, NULL);
	GtkWidget *pWidget = cairo_dock_find_widget_from_name (s_pCurrentWidgetList, cGroupName, cKeyName);
	if (pWidget == NULL && s_pExtraCurrentWidgetList != NULL)
	{
		GSList *l;
		for (l = s_pExtraCurrentWidgetList; l != NULL; l = l->next)
		{
			pWidget = cairo_dock_find_widget_from_name (l->data, cGroupName, cKeyName);
			if (pWidget != NULL)
				break ;
		}
	}
	return pWidget;
}



void cairo_dock_apply_current_filter (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther)
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

void cairo_dock_trigger_current_filter (void)
{
	gboolean bReturn;
	g_signal_emit_by_name (s_pFilterEntry, "activate", NULL, &bReturn);
}



void cairo_dock_deactivate_module_in_gui (const gchar *cModuleName)
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

void cairo_dock_update_desklet_size_in_gui (const gchar *cModuleName, int iWidth, int iHeight)
{
	if (s_pMainWindow == NULL || cModuleName == NULL || s_pCurrentGroup == NULL || s_pCurrentGroup->cGroupName == NULL || s_pCurrentWidgetList == NULL)
		return ;
	
	if (strcmp (cModuleName, s_pCurrentGroup->cGroupName) == 0)  // on est en train d'editer ce module dans le panneau de conf.
	{
		GtkWidget *pOneWidget;
		GSList *pSubWidgetList = cairo_dock_find_widgets_from_name (s_pCurrentWidgetList, "Desklet", "size");
		if (pSubWidgetList != NULL)
		{
			pOneWidget = pSubWidgetList->data;
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), iWidth);
			if (pSubWidgetList->next != NULL)
			{
				pOneWidget = pSubWidgetList->next->data;
				gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), iHeight);
			}
		}
		
	}
}

void cairo_dock_update_desklet_position_in_gui (const gchar *cModuleName, int x, int y)
{
	if (s_pMainWindow == NULL || cModuleName == NULL || s_pCurrentGroup == NULL || s_pCurrentGroup->cGroupName == NULL || s_pCurrentWidgetList == NULL)
		return ;
	
	if (strcmp (cModuleName, s_pCurrentGroup->cGroupName) == 0)  // on est en train d'editer ce module dans le panneau de conf.
	{
		GtkWidget *pOneWidget;
		pOneWidget = cairo_dock_get_widget_from_name ("Desklet", "x position");
		if (pOneWidget != NULL)
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), x);
		pOneWidget = cairo_dock_get_widget_from_name ("Desklet", "y position");
		if (pOneWidget != NULL)
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), y);
	}
}

void cairo_dock_set_status_message (GtkWidget *pWindow, const gchar *cMessage)
{
	GtkWidget *pStatusBar;
	if (pWindow == NULL)
		pStatusBar = s_pStatusBar;
	else
		pStatusBar = g_object_get_data (G_OBJECT (pWindow), "status-bar");
	if (pStatusBar == NULL)
		return ;
	g_print ("%s (%s sur %x/%x)\n", __func__, cMessage, pWindow, pStatusBar);
	gtk_statusbar_pop (GTK_STATUSBAR (pStatusBar), 0);  // clear any previous message, underflow is allowed.
	gtk_statusbar_push (GTK_STATUSBAR (pStatusBar), 0, cMessage);
	while (gtk_events_pending ())
		gtk_main_iteration ();
}
void cairo_dock_set_status_message_printf (GtkWidget *pWindow, const gchar *cFormat, ...)
{
	if (pWindow == NULL && s_pStatusBar == NULL)
		return ;
	g_return_if_fail (cFormat != NULL);
	va_list args;
	va_start (args, cFormat);
	gchar *cMessage = g_strdup_vprintf (cFormat, args);
	cairo_dock_set_status_message (pWindow, cMessage);
	g_free (cMessage);
	va_end (args);
}

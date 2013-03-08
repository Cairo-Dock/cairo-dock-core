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

#include "config.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_current_theme_need_save
#include "cairo-dock-X-manager.h"
#include "cairo-dock-widget-config.h"

#define CAIRO_DOCK_SIMPLE_CONF_FILE "cairo-dock-simple.conf"  // this file is not part of the theme, it's just a convenient way to display this big widget.
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_SHORTKEY_PAGE 2

extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;
extern gchar *g_cConfFile;
extern CairoDockDesktopGeometry g_desktopGeometry;

static void _config_widget_apply (CDWidget *pCdWidget);
static void _config_widget_reset (CDWidget *pCdWidget);
static void _config_widget_reload (CDWidget *pCdWidget);

#define cd_reload(module_name) do {\
	pManager = gldi_get_manager (module_name);\
	if (pManager != NULL)\
		gldi_reload_manager_from_keyfile (pManager, pKeyFile);\
	} while (0)

static gchar *_get_animation_name (int i)
{
	switch (i)
	{
		case 0:
			return g_strdup ("bounce");
		case 1:
			return g_strdup ("rotate");
		case 2:
			return g_strdup ("blink");
		case 3:
			return g_strdup ("pulse");
		case 4:
			return g_strdup ("wobbly");
		case 5:
			return g_strdup ("wave");
		case 6:
			return g_strdup ("spot");
		default:
		return g_strdup ("");
	}
}
static const gchar *_get_animation_number (const gchar *s)
{
	if (!s)
		return "";
	if (strcmp (s, "bounce") == 0)
		return "0";
	
	if (strcmp (s, "rotate") == 0)
		return "1";
	
	if (strcmp (s, "blink") == 0)
		return "2";
	
	if (strcmp (s, "pulse") == 0)
		return "3";
	
	if (strcmp (s, "wobbly") == 0)
		return "4";
	
	if (strcmp (s, "wave") == 0)
		return "5";
	
	if (strcmp (s, "spot") == 0)
		return "6";
	return "";
}
static gchar *_get_effect_name (int i)
{
	switch (i)
	{
		case 0:
			return g_strdup ("fire");
		case 1:
			return g_strdup ("stars");
		case 2:
			return g_strdup ("rain");
		case 3:
			return g_strdup ("snow");
		case 4:
			return g_strdup ("storm");
		case 5:
			return g_strdup ("firework");
		default:
		return g_strdup ("");
	}
}
static const gchar *_get_effect_number (const gchar *s)
{
	if (!s)
		return "";
	if (strcmp (s, "fire") == 0)
		return "0";
	
	if (strcmp (s, "stars") == 0)
		return "1";
	
	if (strcmp (s, "rain") == 0)
		return "2";
	
	if (strcmp (s, "snow") == 0)
		return "3";
	
	if (strcmp (s, "storm") == 0)
		return "4";
	
	if (strcmp (s, "firework") == 0)
		return "5";
	return "";
}

static GKeyFile *_make_simple_conf_file (ConfigWidget *pConfigWidget)
{
	//\_____________ On actualise le fichier de conf simple.
	// on cree le fichier au besoin, et on l'ouvre.
	gchar *cConfFilePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_SIMPLE_CONF_FILE);
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
	{
		cairo_dock_copy_file (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_SIMPLE_CONF_FILE, cConfFilePath);
	}
	
	GKeyFile* pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_val_if_fail (pSimpleKeyFile != NULL, NULL);
	
	if (cairo_dock_conf_file_needs_update (pSimpleKeyFile, CAIRO_DOCK_VERSION))
	{
		cairo_dock_upgrade_conf_file (cConfFilePath, pSimpleKeyFile, CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_SIMPLE_CONF_FILE);
		
		g_key_file_free (pSimpleKeyFile);
		pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
		g_return_val_if_fail (pSimpleKeyFile != NULL, NULL);
	}
	
	// comportement
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "screen border", myDocksParam.iScreenBorder);
	
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "visibility", myDocksParam.iVisibility);
	
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "show_on_click", (myDocksParam.bShowSubDockOnClick ? 1 : 0));
	
	g_key_file_set_string (pSimpleKeyFile, "Behavior", "hide effect", myDocksParam.cHideEffect);
	
	int iTaskbarType;
	if (! myTaskbarParam.bShowAppli)
		iTaskbarType = 0;
	else if (myTaskbarParam.bHideVisibleApplis)
		iTaskbarType = 1;
	else if (myTaskbarParam.bGroupAppliByClass)
		iTaskbarType = 2;
	else
		iTaskbarType = 3;
	pConfigWidget->iTaskbarType = iTaskbarType;
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "taskbar", iTaskbarType);

	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "place icons", myTaskbarParam.iIconPlacement);

	g_key_file_set_string (pSimpleKeyFile, "Behavior", "relative icon", myTaskbarParam.cRelativeIconName);
	
	// animations
	CairoDockModule *pModule;
	CairoDockModuleInstance *pModuleInstance;
	int iAnimOnMouseHover = -1;
	int iAnimOnClick = -1;
	int iEffectOnMouseHover = -1;
	int iEffectOnClick = -1;
	gsize length;
	pModule = cairo_dock_find_module_from_name ("Animated icons");
	if (pModule != NULL && pModule->pInstancesList != NULL)
	{
		pModuleInstance = pModule->pInstancesList->data;
		GKeyFile *pKeyFile = cairo_dock_open_key_file (pModuleInstance->cConfFilePath);
		if (pKeyFile)
		{
			int *pAnimations = g_key_file_get_integer_list (pKeyFile, "Global", "hover effects", &length, NULL);
			if (pAnimations != NULL)
			{
				iAnimOnMouseHover = pAnimations[0];
				g_free (pAnimations);
			}
			pAnimations = g_key_file_get_integer_list (pKeyFile, "Global", "click launchers", &length, NULL);
			if (pAnimations != NULL)
			{
				iAnimOnClick = pAnimations[0];
				g_free (pAnimations);
			}
			g_key_file_free (pKeyFile);
		}
	}
	
	pModule = cairo_dock_find_module_from_name ("icon effects");
	if (pModule != NULL && pModule->pInstancesList != NULL)
	{
		pModuleInstance = pModule->pInstancesList->data;
		GKeyFile *pKeyFile = cairo_dock_open_key_file (pModuleInstance->cConfFilePath);
		if (pKeyFile)
		{
			int *pAnimations = g_key_file_get_integer_list (pKeyFile, "Global", "effects", &length, NULL);
			if (pAnimations != NULL)
			{
				iEffectOnMouseHover = pAnimations[0];
				g_free (pAnimations);
			}
			pAnimations = g_key_file_get_integer_list (pKeyFile, "Global", "click launchers", &length, NULL);
			if (pAnimations != NULL)
			{
				iEffectOnClick = pAnimations[0];
				g_free (pAnimations);
			}
			g_key_file_free (pKeyFile);
		}
	}
	
	pConfigWidget->iEffectOnDisappearance = -1;
	pModule = cairo_dock_find_module_from_name ("illusion");
	if (pModule != NULL && pModule->pInstancesList != NULL)
	{
		pModuleInstance = pModule->pInstancesList->data;
		GKeyFile *pKeyFile = cairo_dock_open_key_file (pModuleInstance->cConfFilePath);
		if (pKeyFile)
		{
			pConfigWidget->iEffectOnDisappearance = g_key_file_get_integer (pKeyFile, "Global", "disappearance", NULL);
			g_key_file_free (pKeyFile);
		}
	}
	
	g_free (pConfigWidget->cHoverAnim);
	pConfigWidget->cHoverAnim = _get_animation_name (iAnimOnMouseHover);
	g_free (pConfigWidget->cHoverEffect);
	pConfigWidget->cHoverEffect = _get_effect_name (iEffectOnMouseHover);
	g_free (pConfigWidget->cClickAnim);
	pConfigWidget->cClickAnim = _get_animation_name (iAnimOnClick);
	g_free (pConfigWidget->cClickEffect);
	pConfigWidget->cClickEffect = _get_effect_name (iEffectOnClick);
	const gchar *cOnMouseHover[2] = {pConfigWidget->cHoverAnim, pConfigWidget->cHoverEffect};
	const gchar *cOnClick[2] = {pConfigWidget->cClickAnim, pConfigWidget->cClickEffect};
	
	if (g_bUseOpenGL)
		g_key_file_set_string_list (pSimpleKeyFile, "Behavior", "anim_hover", cOnMouseHover, 2);
	
	g_key_file_set_string_list (pSimpleKeyFile, "Behavior", "anim_click", cOnClick, 2);
	if (g_bUseOpenGL)
		g_key_file_set_integer (pSimpleKeyFile, "Behavior", "anim_disappear", pConfigWidget->iEffectOnDisappearance);
	
	// apparence
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "default icon directory", myIconsParam.cIconTheme);
	
	int iIconSize = cairo_dock_convert_icon_size_to_enum (myIconsParam.iIconWidth);
	iIconSize --;  // skip the "default" enum
	
	g_key_file_set_integer (pSimpleKeyFile, "Appearance", "icon size", iIconSize);
	pConfigWidget->iIconSize = iIconSize;
	
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "main dock view", myBackendsParam.cMainDockDefaultRendererName);
	
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "sub-dock view", myBackendsParam.cSubDockDefaultRendererName);
	
	cairo_dock_write_keys_to_file (pSimpleKeyFile, cConfFilePath);
	
	return pSimpleKeyFile;
}

static void _cairo_dock_add_one_animation_item (const gchar *cName, CairoDockAnimationRecord *pRecord, GtkListStore *pModele)
{
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gtk_list_store_append (GTK_LIST_STORE (pModele), &iter);
	gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
		CAIRO_DOCK_MODEL_NAME, (pRecord && pRecord->cDisplayedName != NULL && *pRecord->cDisplayedName != '\0' ? pRecord->cDisplayedName : cName),
		CAIRO_DOCK_MODEL_RESULT, cName, -1);
}
static void _add_one_animation_item (const gchar *cName, CairoDockAnimationRecord *pRecord, GtkListStore *pModele)
{
	if (!pRecord->bIsEffect)
		_cairo_dock_add_one_animation_item (cName, pRecord, pModele);
}
static void _add_one_effect_item (const gchar *cName, CairoDockAnimationRecord *pRecord, GtkListStore *pModele)
{
	if (pRecord->bIsEffect)
		_cairo_dock_add_one_animation_item (cName, pRecord, pModele);
}
static void _make_double_anim_widget (GSList *pWidgetList, GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName)
{
	CairoDockGroupKeyWidget *myWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, cGroupName, cKeyName);
	if (myWidget == NULL)  // peut arriver vu que en mode cairo on n'a pas "anim_hover"
		return;
	
	gsize length = 0;
	gchar **cValues = g_key_file_get_string_list (pKeyFile, cGroupName, cKeyName, &length, NULL);

	GtkWidget *box = _gtk_hbox_new (CAIRO_DOCK_GUI_MARGIN);
	gtk_box_pack_end (GTK_BOX (myWidget->pKeyBox), box, FALSE, FALSE, 0);
	
	GtkWidget *pLabel = gtk_label_new (_("Animation:"));
	gtk_box_pack_start (GTK_BOX (box), pLabel, FALSE, FALSE, 0);
	
	GtkWidget *pOneWidget = cairo_dock_gui_make_combo (FALSE);
	GtkTreeModel *modele = gtk_combo_box_get_model (GTK_COMBO_BOX (pOneWidget));
	_cairo_dock_add_one_animation_item ("", NULL, GTK_LIST_STORE (modele));
	cairo_dock_foreach_animation ((GHFunc) _add_one_animation_item, modele);
	gtk_box_pack_start (GTK_BOX (box), pOneWidget, FALSE, FALSE, 0);
	cairo_dock_gui_select_in_combo (pOneWidget, cValues[0]);
	myWidget->pSubWidgetList = g_slist_append (myWidget->pSubWidgetList, pOneWidget);
	
	if (g_bUseOpenGL)
	{
		#if (GTK_MAJOR_VERSION < 3)
		pLabel = gtk_vseparator_new ();
		#else
		pLabel = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
		#endif
		gtk_widget_set_size_request (pLabel, 20, 1);
		gtk_box_pack_start (GTK_BOX (box), pLabel, FALSE, FALSE, 0);
		
		pLabel = gtk_label_new (_("Effects:"));
		gtk_box_pack_start (GTK_BOX (box), pLabel, FALSE, FALSE, 0);
		
		pOneWidget = cairo_dock_gui_make_combo (FALSE);
		modele = gtk_combo_box_get_model (GTK_COMBO_BOX (pOneWidget));
		_cairo_dock_add_one_animation_item ("", NULL, GTK_LIST_STORE (modele));
		cairo_dock_foreach_animation ((GHFunc) _add_one_effect_item, modele);
		gtk_box_pack_start (GTK_BOX (box), pOneWidget, FALSE, FALSE, 0);
		cairo_dock_gui_select_in_combo (pOneWidget, cValues[1]);
		myWidget->pSubWidgetList = g_slist_append (myWidget->pSubWidgetList, pOneWidget);
	}
	g_strfreev (cValues);
}

static void _build_config_widget (ConfigWidget *pConfigWidget)
{
	//\_____________ open and update the simple conf file.
	GKeyFile* pKeyFile = _make_simple_conf_file (pConfigWidget);
	
	//\_____________ build the widgets from the key-file
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	if (pKeyFile != NULL)
	{
		pConfigWidget->widget.pWidget = cairo_dock_build_key_file_widget (pKeyFile,
			NULL,  // gettext domain
			GTK_WIDGET (pConfigWidget->pMainWindow),  // main window
			&pWidgetList,
			pDataGarbage,
			NULL);
	}
	pConfigWidget->widget.pWidgetList = pWidgetList;
	pConfigWidget->widget.pDataGarbage = pDataGarbage;
	
	//\_____________ complete with the animations widgets.
	_make_double_anim_widget (pWidgetList, pKeyFile, "Behavior", "anim_hover");
	_make_double_anim_widget (pWidgetList, pKeyFile, "Behavior", "anim_click");
	
	//\_____________ complete with the shortkeys widget.
	CairoDockGroupKeyWidget *pShortkeysWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Shortkeys", "shortkeys");
	if (pShortkeysWidget != NULL)
	{
		pConfigWidget->pShortKeysWidget = cairo_dock_shortkeys_widget_new ();
		
		gtk_box_pack_start (GTK_BOX (pShortkeysWidget->pKeyBox), pConfigWidget->pShortKeysWidget->widget.pWidget, FALSE, FALSE, 0);
	}
	
	g_key_file_free (pKeyFile);
}

ConfigWidget *cairo_dock_config_widget_new (GtkWindow *pMainWindow)
{
	ConfigWidget *pConfigWidget = g_new0 (ConfigWidget, 1);
	pConfigWidget->widget.iType = WIDGET_CONFIG;
	pConfigWidget->widget.apply = _config_widget_apply;
	pConfigWidget->widget.reset = _config_widget_reset;
	pConfigWidget->widget.reload = _config_widget_reload;
	pConfigWidget->pMainWindow = pMainWindow;
	
	_build_config_widget (pConfigWidget);
	
	return pConfigWidget;
}


static void _config_widget_apply (CDWidget *pCdWidget)
{
	ConfigWidget *pConfigWidget = CONFIG_WIDGET (pCdWidget);
	int iNumPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (pConfigWidget->widget.pWidget));
	if (iNumPage == CAIRO_DOCK_SHORTKEY_PAGE)
	{
		return;
	}
	
	//\_____________ open and update the simple key-file.
	gchar *cConfFilePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_SIMPLE_CONF_FILE);
	GKeyFile* pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_if_fail (pSimpleKeyFile != NULL);
	
	// update the keys with the widgets.
	cairo_dock_update_keyfile_from_widget_list (pSimpleKeyFile, pConfigWidget->widget.pWidgetList);
	
	cairo_dock_write_keys_to_file (pSimpleKeyFile, cConfFilePath);
	g_free (cConfFilePath);
	
	// open the main conf file to update it.
	GKeyFile* pKeyFile = cairo_dock_open_key_file (g_cConfFile);
	g_return_if_fail (pKeyFile != NULL);
	
	// comportement
	CairoDockPositionType iScreenBorder = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "screen border", NULL);
	g_key_file_set_integer (pKeyFile, "Position", "screen border", iScreenBorder);
	
	int iVisibility = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "visibility", NULL);
	g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
	
	gchar *cHideEffect = g_key_file_get_string (pSimpleKeyFile, "Behavior", "hide effect", NULL);
	g_key_file_set_string (pKeyFile, "Accessibility", "hide effect", cHideEffect);
	g_free (cHideEffect);
	
	int iShowOnClick = (g_key_file_get_integer (pSimpleKeyFile, "Behavior", "show_on_click", NULL) == 1);
	g_key_file_set_integer (pKeyFile, "Accessibility", "show_on_click", iShowOnClick);
	
	int iTaskbarType = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "taskbar", NULL);
	if (iTaskbarType != pConfigWidget->iTaskbarType)
	{
		gboolean bShowAppli = TRUE, bHideVisible, bCurrentDesktopOnly, bMixLauncherAppli, bGroupAppliByClass;
		switch (iTaskbarType)
		{
			case 0:
				bShowAppli = FALSE;
			break;
			case 1:
				bMixLauncherAppli = TRUE;
				bHideVisible = TRUE;
				bGroupAppliByClass = FALSE;
				bCurrentDesktopOnly = FALSE;
			break;
			case 2:
				bMixLauncherAppli = TRUE;
				bGroupAppliByClass = TRUE;
				bHideVisible = FALSE;
				bCurrentDesktopOnly = FALSE;
			break;
			case 3:
			default:
				bCurrentDesktopOnly = TRUE;
				bMixLauncherAppli = FALSE;
				bGroupAppliByClass = FALSE;
				bHideVisible = FALSE;
			break;
		}
		g_key_file_set_boolean (pKeyFile, "TaskBar", "show applications", bShowAppli);
		if (bShowAppli)  // sinon on inutile de modifier les autres parametres.
		{
			g_key_file_set_boolean (pKeyFile, "TaskBar", "hide visible", bHideVisible);
			g_key_file_set_boolean (pKeyFile, "TaskBar", "current desktop only", bCurrentDesktopOnly);
			g_key_file_set_boolean (pKeyFile, "TaskBar", "mix launcher appli", bMixLauncherAppli);
			g_key_file_set_boolean (pKeyFile, "TaskBar", "group by class", bGroupAppliByClass);
		}
		pConfigWidget->iTaskbarType = iTaskbarType;
	}

	int iPlaceIcons = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "place icons", NULL);
	g_key_file_set_integer (pKeyFile, "TaskBar", "place icons", iPlaceIcons);

	gchar *cRelativeIconName = g_key_file_get_string (pSimpleKeyFile, "Behavior", "relative icon", NULL);
	g_key_file_set_string (pKeyFile, "TaskBar", "relative icon", cRelativeIconName);
	g_free (cRelativeIconName);
	
	// animations
	gsize length;
	gchar **cOnMouseHover = g_key_file_get_string_list (pSimpleKeyFile, "Behavior", "anim_hover", &length, NULL);
	gchar **cOnClick = g_key_file_get_string_list (pSimpleKeyFile, "Behavior", "anim_click", &length, NULL);
	int iEffectOnDisappearance = -1;
	if (g_bUseOpenGL)
		iEffectOnDisappearance = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "anim_disappear", NULL);
	
	CairoDockModule *pModule;
	CairoDockModuleInstance *pModuleInstanceAnim = NULL;
	CairoDockModuleInstance *pModuleInstanceEffect = NULL;
	CairoDockModuleInstance *pModuleInstanceIllusion = NULL;
	
	if (cOnMouseHover && cOnMouseHover[0])
	{
		if (strcmp (cOnMouseHover[0], pConfigWidget->cHoverAnim) != 0)
		{
			pModule = cairo_dock_find_module_from_name ("Animated icons");
			if (pModule != NULL && pModule->pInstancesList != NULL)
			{
				pModuleInstanceAnim = pModule->pInstancesList->data;
				cairo_dock_update_conf_file (pModuleInstanceAnim->cConfFilePath,
					G_TYPE_STRING, "Global", "hover effects", _get_animation_number (cOnMouseHover[0]),
					G_TYPE_INVALID);
			}
			g_free (pConfigWidget->cHoverAnim);
			pConfigWidget->cHoverAnim = g_strdup (cOnMouseHover[0]);
		}
		if (cOnMouseHover[1] && strcmp (cOnMouseHover[1], pConfigWidget->cHoverEffect) != 0)
		{
			pModule = cairo_dock_find_module_from_name ("icon effects");
			if (pModule != NULL && pModule->pInstancesList != NULL)
			{
				pModuleInstanceEffect = pModule->pInstancesList->data;
				cairo_dock_update_conf_file (pModuleInstanceEffect->cConfFilePath,
					G_TYPE_STRING, "Global", "effects", _get_effect_number (cOnMouseHover[1]),
					G_TYPE_INVALID);
			}
			g_free (pConfigWidget->cHoverEffect);
			pConfigWidget->cHoverEffect = g_strdup (cOnMouseHover[1]);
		}
	}
	if (cOnClick && cOnClick[0])
	{
		if (strcmp (cOnClick[0], pConfigWidget->cClickAnim) != 0)
		{
			pModule = cairo_dock_find_module_from_name ("Animated icons");
			if (pModule != NULL && pModule->pInstancesList != NULL)
			{
				pModuleInstanceAnim = pModule->pInstancesList->data;
				const gchar *cAnimation = _get_animation_number (cOnClick[0]);
				cairo_dock_update_conf_file (pModuleInstanceAnim->cConfFilePath,
					G_TYPE_STRING, "Global", "click launchers", cAnimation,
					G_TYPE_STRING, "Global", "click applis", cAnimation,
					G_TYPE_STRING, "Global", "click applets", cAnimation,
					G_TYPE_INVALID);
			}
			g_free (pConfigWidget->cClickAnim);
			pConfigWidget->cClickAnim = g_strdup (cOnClick[0]);
		}
		if (cOnClick[1] && strcmp (cOnClick[1], pConfigWidget->cClickEffect) != 0)
		{
			pModule = cairo_dock_find_module_from_name ("icon effects");
			if (pModule != NULL && pModule->pInstancesList != NULL)
			{
				pModuleInstanceEffect = pModule->pInstancesList->data;
				const gchar *cAnimation = _get_effect_number (cOnClick[1]);
				cairo_dock_update_conf_file (pModuleInstanceEffect->cConfFilePath,
					G_TYPE_STRING, "Global", "click launchers", cAnimation,
					G_TYPE_STRING, "Global", "click applis", cAnimation,
					G_TYPE_STRING, "Global", "click applets", cAnimation,
					G_TYPE_INVALID);
			}
			g_free (pConfigWidget->cClickEffect);
			pConfigWidget->cClickEffect = g_strdup (cOnClick[1]);
		}
	}
	if (iEffectOnDisappearance != pConfigWidget->iEffectOnDisappearance)
	{
		pModule = cairo_dock_find_module_from_name ("illusion");
		if (pModule != NULL && pModule->pInstancesList != NULL)
		{
			pModuleInstanceIllusion = pModule->pInstancesList->data;
			cairo_dock_update_conf_file (pModuleInstanceIllusion->cConfFilePath,
				G_TYPE_INT, "Global", "disappearance", iEffectOnDisappearance,
				G_TYPE_INT, "Global", "appearance", iEffectOnDisappearance,
				G_TYPE_INVALID);
		}
		pConfigWidget->iEffectOnDisappearance = iEffectOnDisappearance;
	}
	
	g_strfreev (cOnMouseHover);
	g_strfreev (cOnClick);
	
	// apparence
	gchar *cIconTheme = g_key_file_get_string (pSimpleKeyFile, "Appearance", "default icon directory", NULL);
	g_key_file_set_string (pKeyFile, "Icons", "default icon directory", cIconTheme);
	g_free (cIconTheme);
	
	int iIconSize = g_key_file_get_integer (pSimpleKeyFile, "Appearance", "icon size", NULL);
	if (iIconSize != pConfigWidget->iIconSize)
	{
		int iLauncherSize, iIconGap;
		double fMaxScale, fReflectSize;
		iLauncherSize = cairo_dock_convert_icon_size_to_pixels (iIconSize+1, &fMaxScale, &fReflectSize, &iIconGap);  // +1 to skip the "default" enum
		
		gint tab[2] = {iLauncherSize, iLauncherSize};
		g_key_file_set_integer_list (pKeyFile, "Icons", "launcher size", tab, 2);
		tab[0] = myIconsParam.iSeparatorWidth;
		g_key_file_set_integer_list (pKeyFile, "Icons", "separator size", tab, 2);
		g_key_file_set_double (pKeyFile, "Icons", "zoom max", fMaxScale);
		g_key_file_set_double (pKeyFile, "Icons", "field depth", fReflectSize);
		g_key_file_set_integer (pKeyFile, "Icons", "icon gap", iIconGap);
		
		pConfigWidget->iIconSize = iIconSize;
	}
	
	gchar *cMainDockDefaultRendererName = g_key_file_get_string (pSimpleKeyFile, "Appearance", "main dock view", NULL);
	g_key_file_set_string (pKeyFile, "Views", "main dock view", cMainDockDefaultRendererName);
	g_free (cMainDockDefaultRendererName);
	
	gchar *cSubDockDefaultRendererName = g_key_file_get_string (pSimpleKeyFile, "Appearance", "sub-dock view", NULL);
	g_key_file_set_string (pKeyFile, "Views", "sub-dock view", cSubDockDefaultRendererName);
	g_free (cSubDockDefaultRendererName);
	
	// on ecrit tout.
	cairo_dock_write_keys_to_conf_file (pKeyFile, g_cConfFile);
	
	//\_____________ On recharge les modules concernes.
	GldiManager *pManager;
	cd_reload ("Docks");
	cd_reload ("Taskbar");
	cd_reload ("Icons");
	cd_reload ("Backends");
	
	if (pModuleInstanceAnim != NULL)
	{
		cairo_dock_reload_module_instance (pModuleInstanceAnim, TRUE);
	}
	if (pModuleInstanceEffect != NULL)
	{
		cairo_dock_reload_module_instance (pModuleInstanceEffect, TRUE);
	}
	if (pModuleInstanceIllusion != NULL)
	{
		cairo_dock_reload_module_instance (pModuleInstanceIllusion, TRUE);
	}
	
	g_key_file_free (pKeyFile);
	g_key_file_free (pSimpleKeyFile);
}


static void _config_widget_reset (CDWidget *pCdWidget)
{
	ConfigWidget *pConfigWidget = CONFIG_WIDGET (pCdWidget);
	cairo_dock_widget_free (CD_WIDGET (pConfigWidget->pShortKeysWidget));
	g_free (pConfigWidget->cHoverAnim);
	g_free (pConfigWidget->cHoverEffect);
	g_free (pConfigWidget->cClickAnim);
	g_free (pConfigWidget->cClickEffect);  // pShortKeysTreeView is destroyed with the other widgets.
	memset (pCdWidget+1, 0, sizeof (ConfigWidget) - sizeof (CDWidget));  // reset all our parameters.
}


void cairo_dock_widget_config_update_shortkeys (ConfigWidget *pConfigWidget)
{
	if (pConfigWidget->pShortKeysWidget)
		cairo_dock_widget_reload (CD_WIDGET (pConfigWidget->pShortKeysWidget));
}


static void _config_widget_reload (CDWidget *pCdWidget)
{
	ConfigWidget *pConfigWidget = CONFIG_WIDGET (pCdWidget);
	cairo_dock_widget_destroy_widget (CD_WIDGET (pConfigWidget));
	
	_build_config_widget (pConfigWidget);
}

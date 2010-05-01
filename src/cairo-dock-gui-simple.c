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
#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gui-simple.h"

#define CAIRO_DOCK_PREVIEW_WIDTH 200
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_SIMPLE_PANEL_WIDTH 1024
#define CAIRO_DOCK_SIMPLE_PANEL_HEIGHT 700
#define CAIRO_DOCK_SIMPLE_CONF_FILE "cairo-dock-simple.conf"

static GtkWidget *s_pSimpleConfigWindow = NULL;
static GtkWidget *s_pSimpleConfigModuleWindow = NULL;
static const  gchar *s_cCurrentModuleName = NULL;
// GSList *s_pCurrentWidgetList;  // liste des widgets du main panel.
// GSList *s_pCurrentModuleWidgetList;  // liste des widgets du module courant.
static int s_iIconSize;
static int s_iTaskbarType;
static gboolean s_bSeparateIcons;
static const gchar *s_cHoverAnim = "";
static const gchar *s_cHoverEffect = "";
static const gchar *s_cClickAnim = "";
static const gchar *s_cClickEffect = "";
static int s_iEffectOnDisappearance = -1;

extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern CairoDock *g_pMainDock;
extern gchar *g_cCairoDockDataDir;
extern gboolean g_bUseOpenGL;

#define cd_reload(module_name) do {\
	pInternalModule = cairo_dock_find_internal_module_from_name (module_name);\
	if (pInternalModule != NULL) \
		cairo_dock_reload_internal_module_from_keyfile (pInternalModule, pKeyFile);\
	} while (0)

static const gchar *_get_animation_name (int i)
{
	switch (i)
	{
		case 0:
			return "bounce";
		case 1:
			return "rotate";
		case 2:
			return "blink";
		case 3:
			return "pulse";
		case 4:
			return "wobbly";
		case 5:
			return "wave";
		case 6:
			return "spot";
		default:
		return "";
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
static const gchar *_get_effect_name (int i)
{
	switch (i)
	{
		case 0:
			return "fire";
		case 1:
			return "stars";
		case 2:
			return "rain";
		case 3:
			return "snow";
		case 4:
			return "storm";
		case 5:
			return "firework";
		default:
		return "";
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

static gchar * _make_simple_conf_file (void)
{
	//\_____________ On actualise le fichier de conf simple.
	// on cree le fichier au besoin, et on l'ouvre.
	gchar *cConfFilePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_SIMPLE_CONF_FILE);
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
	{
		gchar *cCommand = g_strdup_printf ("cp \"%s/%s\" \"%s\"", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_SIMPLE_CONF_FILE, cConfFilePath);
		int r = system (cCommand);
		g_free (cCommand);
	}
	
	GKeyFile* pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_val_if_fail (pSimpleKeyFile != NULL, NULL);
	
	if (cairo_dock_conf_file_needs_update (pSimpleKeyFile, CAIRO_DOCK_VERSION))
	{
		cairo_dock_flush_conf_file (pSimpleKeyFile, cConfFilePath, CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_SIMPLE_CONF_FILE);
		g_key_file_free (pSimpleKeyFile);
		pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
		g_return_val_if_fail (pSimpleKeyFile != NULL, NULL);
	}
	
	// comportement
	CairoDockPositionType iScreenBorder = (g_pMainDock ? ((! g_pMainDock->container.bIsHorizontal) << 1) | (! g_pMainDock->container.bDirectionUp) : 0);
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "screen border", iScreenBorder);
	
	gboolean iVisibility;
	if (myAccessibility.bReserveSpace)
		iVisibility = 1;
	else if (myAccessibility.bPopUp)
		iVisibility = 2;
	else if (myAccessibility.bAutoHide)
		iVisibility = 3;
	else if (myAccessibility.bAutoHideOnOverlap)
		iVisibility = 4;
	else if (myAccessibility.cRaiseDockShortcut)
		iVisibility = 5;
	else
		iVisibility = 0;
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "visibility", iVisibility);
	
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "show_on_click", (myAccessibility.bShowSubDockOnClick ? 1 : 0));
	
	int iTaskbarType;
	if (! myTaskBar.bShowAppli)
		iTaskbarType = 0;
	else if (myTaskBar.bHideVisibleApplis)
		iTaskbarType = 1;
	else if (myTaskBar.bGroupAppliByClass)
		iTaskbarType = 2;
	else
		iTaskbarType = 3;
	s_iTaskbarType = iTaskbarType;
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "taskbar", iTaskbarType);
	
	// animations
	CairoDockModule *pModule;
	CairoDockModuleInstance *pModuleInstance;
	int iAnimOnMouseHover = -1;
	int iAnimOnClick = -1;
	int iEffectOnMouseHover = -1;
	int iEffectOnClick = -1;
	int iEffectOnDisappearance = 4;
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
	
	s_iEffectOnDisappearance = -1;
	pModule = cairo_dock_find_module_from_name ("illusion");
	if (pModule != NULL && pModule->pInstancesList != NULL)
	{
		pModuleInstance = pModule->pInstancesList->data;
		GKeyFile *pKeyFile = cairo_dock_open_key_file (pModuleInstance->cConfFilePath);
		if (pKeyFile)
		{
			s_iEffectOnDisappearance = g_key_file_get_integer (pKeyFile, "Global", "disappearance", NULL);
			g_key_file_free (pKeyFile);
		}
	}
	
	s_cHoverAnim = _get_animation_name (iAnimOnMouseHover);
	s_cHoverEffect = _get_effect_name (iEffectOnMouseHover);
	s_cClickAnim = _get_animation_name (iAnimOnClick);
	s_cClickEffect = _get_effect_name (iEffectOnClick);
	const gchar *cOnMouseHover[2] = {s_cHoverAnim, s_cHoverEffect};
	const gchar *cOnClick[2] = {s_cClickAnim, s_cClickEffect};
	
	g_key_file_set_string_list (pSimpleKeyFile, "Behavior", "anim_hover", cOnMouseHover, 2);
	g_key_file_set_string_list (pSimpleKeyFile, "Behavior", "anim_click", cOnClick, 2);
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "anim_disappear", s_iEffectOnDisappearance);
	
	if (g_bUseOpenGL)
	{
		gchar *cComment = g_key_file_get_comment (pSimpleKeyFile, "Behavior", "anim_disappear", NULL);
		if (cComment != NULL)
		{
			gchar *str = cComment;
			cComment = g_strdup_printf ("l[%s;%s;%s;%s;%s;%s]%s", "Evaporate", "Fade out", "Explode", "Break", "Black Hole", "Random", cComment+1);
			g_free (str);
			g_key_file_set_comment (pSimpleKeyFile, "Behavior", "anim_disappear", cComment, NULL);
			g_free (cComment);
		}
	}
	
	// apparence
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "default icon directory", myIcons.cIconTheme);
	
	int iIconSize;
	if (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] <= 42)  // icones toutes petites.
		iIconSize = 0;
	else if (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] >= 54)  // icones tres grandes.
		iIconSize = 4;
	else if (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] <= 48)
	{
		if (myIcons.fAmplitude >= 2)  // icones petites.
			iIconSize = 1;
		else
			iIconSize = 2;  // moyennes.
	}
	else  // grandes.
		iIconSize = 3;
	
	g_key_file_set_integer (pSimpleKeyFile, "Appearance", "icon size", iIconSize);
	s_iIconSize = iIconSize;
	
	g_key_file_set_boolean (pSimpleKeyFile, "Appearance", "separate icons", myIcons.iSeparateIcons);
	s_bSeparateIcons = myIcons.iSeparateIcons;
	
	g_key_file_set_integer_list (pSimpleKeyFile, "Appearance", "icon's type order", myIcons.iIconsTypesList, 3);
	
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "main dock view", myViews.cMainDockDefaultRendererName);
	
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "sub-dock view", myViews.cSubDockDefaultRendererName);
	
	// themes
	g_key_file_set_boolean (pSimpleKeyFile, "Themes", "use theme launchers", FALSE);
	
	g_key_file_set_boolean (pSimpleKeyFile, "Themes", "use theme behaviour", FALSE);
	
	cairo_dock_write_keys_to_file (pSimpleKeyFile, cConfFilePath);
	
	g_key_file_free (pSimpleKeyFile);
	return cConfFilePath;
}

static gboolean on_apply_theme_simple (gpointer data)
{
	//\_______________ On recupere le choix de l'utilisateur.
	gchar *cConfFilePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_SIMPLE_CONF_FILE);
	GKeyFile* pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_free (cConfFilePath);
	g_return_val_if_fail (pSimpleKeyFile != NULL, TRUE);
	
	gchar *cNewThemeName = g_key_file_get_string (pSimpleKeyFile, "Themes", "chosen theme", NULL);
	gboolean bLoadBehavior = g_key_file_get_boolean (pSimpleKeyFile, "Themes", "use theme behaviour", NULL);
	gboolean bLoadLaunchers = g_key_file_get_boolean (pSimpleKeyFile, "Themes", "use theme launchers", NULL);
	
	g_key_file_free (pSimpleKeyFile);
	if (cNewThemeName == NULL || *cNewThemeName == '\0')
		return TRUE;
	
	//\_______________ On importe le theme.
	cairo_dock_set_status_message_printf (NULL, _("Importing theme %s ..."), cNewThemeName);
	gboolean bThemeImported = cairo_dock_import_theme (cNewThemeName, bLoadBehavior, bLoadLaunchers);
	cd_debug ("bThemeImported : %d\n", bThemeImported);
	
	//\_______________ On le charge.
	if (bThemeImported)
	{
		cairo_dock_load_current_theme ();
	}
	
	g_free (cNewThemeName);
	cairo_dock_set_status_message (s_pSimpleConfigWindow, "");
	
	//\_______________ On recharge la fenetre.
	cd_debug ("reload\n");
	cConfFilePath = _make_simple_conf_file ();
	g_free (cConfFilePath);
	cairo_dock_reload_generic_gui (s_pSimpleConfigWindow);
	return TRUE;
}

static gboolean on_apply_config_simple (gpointer data)
{
	GtkWidget * pMainVBox= gtk_bin_get_child (GTK_BIN (s_pSimpleConfigWindow));
	GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
	GtkWidget *pGroupWidget = children->data;
	if (gtk_notebook_get_current_page (GTK_NOTEBOOK (pGroupWidget)) == 3)
	{
		return on_apply_theme_simple (data);
	}
	
	//\_____________ On actualise le fichier de conf principal.
	// on ouvre les 2 fichiers (l'un en lecture, l'autre en ecriture).
	gchar *cConfFilePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, CAIRO_DOCK_SIMPLE_CONF_FILE);
	GKeyFile* pSimpleKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_free (cConfFilePath);
	g_return_val_if_fail (pSimpleKeyFile != NULL, TRUE);
	GKeyFile* pKeyFile = cairo_dock_open_key_file (g_cConfFile);
	g_return_val_if_fail (pKeyFile != NULL, TRUE);
	
	// comportement
	CairoDockPositionType iScreenBorder = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "screen border", NULL);
	g_key_file_set_integer (pKeyFile, "Position", "screen border", iScreenBorder);
	
	int iVisibility = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "visibility", NULL);
	g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
	
	gboolean bShowOnClick = (g_key_file_get_integer (pSimpleKeyFile, "Behavior", "show_on_click", NULL) == 1);
	g_key_file_set_boolean (pKeyFile, "Accessibility", "show on click", bShowOnClick);
	
	int iTaskbarType = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "taskbar", NULL);
	if (iTaskbarType != s_iTaskbarType)
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
	}
	
	// animations
	gsize length;
	gchar **cOnMouseHover = g_key_file_get_string_list (pSimpleKeyFile, "Behavior", "anim_hover", &length, NULL);
	gchar **cOnClick = g_key_file_get_string_list (pSimpleKeyFile, "Behavior", "anim_click", &length, NULL);
	int iEffectOnDisappearance = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "anim_disappear", NULL);
	
	CairoDockModule *pModule;
	CairoDockModuleInstance *pModuleInstanceAnim = NULL;
	CairoDockModuleInstance *pModuleInstanceEffect = NULL;
	CairoDockModuleInstance *pModuleInstanceIllusion = NULL;
	
	if (cOnMouseHover && cOnMouseHover[0])
	{
		if (strcmp (cOnMouseHover[0], s_cHoverAnim) != 0)
		{
			pModule = cairo_dock_find_module_from_name ("Animated icons");
			if (pModule != NULL && pModule->pInstancesList != NULL)
			{
				pModuleInstanceAnim = pModule->pInstancesList->data;
				cairo_dock_update_conf_file (pModuleInstanceAnim->cConfFilePath,
					G_TYPE_STRING, "Global", "hover effects", _get_animation_number (cOnMouseHover[0]),
					G_TYPE_INVALID);
			}
		}
		if (cOnMouseHover[1] && strcmp (cOnMouseHover[1], s_cHoverEffect) != 0)
		{
			pModule = cairo_dock_find_module_from_name ("icon effects");
			if (pModule != NULL && pModule->pInstancesList != NULL)
			{
				pModuleInstanceEffect = pModule->pInstancesList->data;
				cairo_dock_update_conf_file (pModuleInstanceEffect->cConfFilePath,
					G_TYPE_STRING, "Global", "effects", _get_effect_number (cOnMouseHover[1]),
					G_TYPE_INVALID);
			}
		}
	}
	if (cOnClick && cOnClick[0])
	{
		if (strcmp (cOnClick[0], s_cClickAnim) != 0)
		{
			pModule = cairo_dock_find_module_from_name ("Animated icons");
			if (pModule != NULL && pModule->pInstancesList != NULL)
			{
				pModuleInstanceAnim = pModule->pInstancesList->data;
				cairo_dock_update_conf_file (pModuleInstanceAnim->cConfFilePath,
					G_TYPE_STRING, "Global", "click launchers", _get_animation_number (cOnClick[0]),
					G_TYPE_INVALID);
			}
		}
		if (cOnClick[1] && strcmp (cOnClick[1], s_cClickEffect) != 0)
		{
			pModule = cairo_dock_find_module_from_name ("icon effects");
			if (pModule != NULL && pModule->pInstancesList != NULL)
			{
				pModuleInstanceEffect = pModule->pInstancesList->data;
				cairo_dock_update_conf_file (pModuleInstanceEffect->cConfFilePath,
					G_TYPE_STRING, "Global", "click launchers", _get_effect_number (cOnClick[1]),
					G_TYPE_INVALID);
			}
		}
	}
	if (iEffectOnDisappearance != s_iEffectOnDisappearance)
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
	}
	
	g_strfreev (cOnMouseHover);
	g_strfreev (cOnClick);
	
	// apparence
	gchar *cIconTheme = g_key_file_get_string (pSimpleKeyFile, "Appearance", "default icon directory", NULL);
	g_key_file_set_string (pKeyFile, "Icons", "default icon directory", cIconTheme);
	g_free (cIconTheme);
	
	int iIconSize = g_key_file_get_integer (pSimpleKeyFile, "Appearance", "icon size", NULL);
	if (iIconSize != s_iIconSize)
	{
		int iLauncherSize, iIconGap;
		double fMaxScale, fReflectSize;
		switch (iIconSize)
		{
			case 0:  // tres petites
				iLauncherSize = 42;
				fMaxScale = 2.2;
				iIconGap = 5;
				fReflectSize = .4;
			break;
			case 1:  // petites
				iLauncherSize = 44;
				fMaxScale = 2.;
				iIconGap = 4;
				fReflectSize = .4;
			break;
			case 2:  // moyennes
				iLauncherSize = 48;
				fMaxScale = 1.7;
				iIconGap = 2;
				fReflectSize = .5;
			break;
			case 3:  // grandes
			default:
				iLauncherSize = 52;
				fMaxScale = 1.5;
				iIconGap = 2;
				fReflectSize = .6;
			break;
			case 4:  // tres grandes
				iLauncherSize = 56;
				fMaxScale = 1.3;
				iIconGap = 2;
				fReflectSize = .6;
			break;
			
		}
		gint tab[2] = {iLauncherSize, iLauncherSize};
		g_key_file_set_integer_list (pKeyFile, "Icons", "launcher size", tab, 2);
		g_key_file_set_integer_list (pKeyFile, "Icons", "appli size", tab, 2);
		g_key_file_set_integer_list (pKeyFile, "Icons", "applet size", tab, 2);
		/// hauteur des separateurs ?...
		g_key_file_set_double (pKeyFile, "Icons", "zoom max", fMaxScale);
		g_key_file_set_double (pKeyFile, "Icons", "field depth", fReflectSize);
		g_key_file_set_integer (pKeyFile, "Icons", "icon gap", iIconGap);
		s_iIconSize = iIconSize;
	}
	
	gboolean bSeparateIcons = g_key_file_get_boolean (pSimpleKeyFile, "Appearance", "separate icons", NULL);
	if (bSeparateIcons != s_bSeparateIcons)
	{
		g_key_file_set_integer (pKeyFile, "Icons", "separate_icons", (bSeparateIcons ? 3 : 0));
	}
	
	gchar *cIconOrder = g_key_file_get_string (pSimpleKeyFile, "Appearance", "icon's type order", NULL);
	g_key_file_set_string (pKeyFile, "Icons", "icon's type order", cIconOrder);
	g_free (cIconOrder);
	
	gchar *cMainDockDefaultRendererName = g_key_file_get_string (pSimpleKeyFile, "Appearance", "main dock view", NULL);
	g_key_file_set_string (pKeyFile, "Views", "main dock view", cMainDockDefaultRendererName);
	g_free (cMainDockDefaultRendererName);
	
	gchar *cSubDockDefaultRendererName = g_key_file_get_string (pSimpleKeyFile, "Appearance", "sub-dock view", NULL);
	g_key_file_set_string (pKeyFile, "Views", "sub-dock view", cSubDockDefaultRendererName);
	g_free (cSubDockDefaultRendererName);
	
	cairo_dock_write_keys_to_file (pKeyFile, g_cConfFile);
	
	//\_____________ On recharge les modules concernes.
	CairoDockInternalModule *pInternalModule;
	cd_reload ("Position");
	cd_reload ("Accessibility");
	cd_reload ("TaskBar");
	cd_reload ("Icons");
	cd_reload ("Views");
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
	
	return TRUE;
}

static void on_destroy_config_simple (gpointer data)
{
	s_pSimpleConfigWindow = NULL;
}

static GtkWidget * show_main_gui (void)
{
	if (s_pSimpleConfigWindow != NULL)
	{
		gtk_window_present (GTK_WINDOW (s_pSimpleConfigWindow));
		return s_pSimpleConfigWindow;
	}
	
	//\_____________ On construit le fichier de conf simple a partir du .conf normal.
	gchar *cConfFilePath = _make_simple_conf_file ();
	
	//\_____________ On construit la fenetre.
	cairo_dock_build_generic_gui (cConfFilePath,
		NULL,
		_("Cairo-Dock configuration"),
		CAIRO_DOCK_SIMPLE_PANEL_WIDTH, CAIRO_DOCK_SIMPLE_PANEL_HEIGHT,
		(CairoDockApplyConfigFunc) on_apply_config_simple,
		NULL,
		(GFreeFunc) on_destroy_config_simple,
		&s_pSimpleConfigWindow);
	
	//\_____________ On ajoute un bouton "mode avance".
	GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (s_pSimpleConfigWindow));
	GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
	GList *w = g_list_last (children);
	GtkWidget *pButtonsHBox = w->data;
	GtkWidget *pSwitchButton = cairo_dock_make_switch_gui_button ();
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		pSwitchButton,
		FALSE,
		FALSE,
		0);
	gtk_box_reorder_child (GTK_BOX (pButtonsHBox), pSwitchButton, 1);
	gtk_widget_show_all (pSwitchButton);
	return s_pSimpleConfigWindow;
}


static gboolean on_apply_config_module_simple (gpointer data)
{
	cd_debug ("%s (%s)\n", __func__, s_cCurrentModuleName);
	CairoDockModule *pModule = cairo_dock_find_module_from_name (s_cCurrentModuleName);
	if (pModule != NULL)
	{
		if (pModule->pInstancesList != NULL)
		{
			gchar *cConfFilePath = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "conf-file");
			CairoDockModuleInstance *pModuleInstance = NULL;
			GList *pElement;
			for (pElement = pModule->pInstancesList; pElement != NULL; pElement= pElement->next)
			{
				pModuleInstance = pElement->data;
				if (strcmp (pModuleInstance->cConfFilePath, cConfFilePath) == 0)
					break ;
			}
			g_return_val_if_fail (pModuleInstance != NULL, TRUE);
			
			cairo_dock_reload_module_instance (pModuleInstance, TRUE);
		}
	}
	return TRUE;
}

static void on_destroy_config_module_simple (gpointer data)
{
	s_pSimpleConfigModuleWindow = NULL;
}


static inline GtkWidget * _present_module_widget (GtkWidget *pWindow, CairoDockModuleInstance *pInstance, const gchar *cConfFilePath, const gchar *cGettextDomain, const gchar *cOriginalConfFilePath)
{
	//\_____________ On construit l'IHM du fichier de conf.
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	GSList *pWidgetList = NULL;
	GPtrArray *pDataGarbage = g_ptr_array_new ();
	GtkWidget *pNoteBook = cairo_dock_build_key_file_widget (pKeyFile, cGettextDomain, pWindow, &pWidgetList, pDataGarbage, cOriginalConfFilePath);
	
	g_object_set_data (G_OBJECT (pWindow), "conf-file", g_strdup (cConfFilePath));
	g_object_set_data (G_OBJECT (pWindow), "widget-list", pWidgetList);
	g_object_set_data (G_OBJECT (pWindow), "garbage", pDataGarbage);
	if (pInstance != NULL)
		g_object_set_data (G_OBJECT (pWindow), "module", (gpointer)pInstance->pModule->pVisitCard->cModuleName);
	
	if (pInstance != NULL && pInstance->pModule->pInterface->load_custom_widget != NULL)
		pInstance->pModule->pInterface->load_custom_widget (pInstance, pKeyFile);
	
	g_key_file_free (pKeyFile);
	
	//\_____________ On l'insere dans la fenetre.
	GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (pWindow));
	gtk_box_pack_start (GTK_BOX (pMainVBox),
		pNoteBook,
		TRUE,
		TRUE,
		0);
	
	return pNoteBook;
}

static int _reset_current_module_widget (void)
{
	if (s_pSimpleConfigModuleWindow == NULL)
		return -1;
	GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (s_pSimpleConfigModuleWindow));
	GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
	GtkWidget *pGroupWidget = children->data;
	
	int iNotebookPage = 0;
	if (GTK_IS_NOTEBOOK (pGroupWidget))
		iNotebookPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (pGroupWidget));
	
	gtk_widget_destroy (pGroupWidget);
	GSList *pWidgetList = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "widget-list");
	cairo_dock_free_generated_widget_list (pWidgetList);
	g_object_set_data (G_OBJECT (s_pSimpleConfigModuleWindow), "widget-list", NULL);
	GPtrArray *pDataGarbage = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "garbage");
	if (pDataGarbage != NULL)
		g_ptr_array_free (pDataGarbage, TRUE);
	g_object_set_data (G_OBJECT (s_pSimpleConfigModuleWindow), "garbage", NULL);
	
	return iNotebookPage;
}

static void show_module_instance_gui (CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	int iNotebookPage = -1;
	if (s_pSimpleConfigModuleWindow == NULL)
	{
		s_pSimpleConfigModuleWindow = cairo_dock_build_generic_gui_window (dgettext (pModuleInstance->pModule->pVisitCard->cGettextDomain, pModuleInstance->pModule->pVisitCard->cTitle),
			CAIRO_DOCK_SIMPLE_PANEL_WIDTH, CAIRO_DOCK_SIMPLE_PANEL_HEIGHT,
			(CairoDockApplyConfigFunc) on_apply_config_module_simple,
			NULL,
			(GFreeFunc) on_destroy_config_module_simple);
		iNotebookPage = iShowPage;
	}
	else
	{
		iNotebookPage = _reset_current_module_widget ();
		if (iShowPage >= 0)
			iNotebookPage = iShowPage;
	}
	
	gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", pModuleInstance->pModule->pVisitCard->cShareDataDir, pModuleInstance->pModule->pVisitCard->cConfFileName);
	
	GtkWidget *pGroupWidget = _present_module_widget (s_pSimpleConfigModuleWindow,
		pModuleInstance,
		pModuleInstance->cConfFilePath,
		pModuleInstance->pModule->pVisitCard->cGettextDomain,
		cOriginalConfFilePath);
	
	g_free (cOriginalConfFilePath);
	s_cCurrentModuleName = pModuleInstance->pModule->pVisitCard->cModuleName;
	
	gtk_widget_show_all (s_pSimpleConfigModuleWindow);
	if (iNotebookPage != -1 && GTK_IS_NOTEBOOK (pGroupWidget))
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (pGroupWidget), iNotebookPage);
	}
}

static void show_module_gui (const gchar *cModuleName)
{
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	g_return_if_fail (pModule != NULL);
	
	if (s_pSimpleConfigModuleWindow == NULL)
	{
		s_pSimpleConfigModuleWindow = cairo_dock_build_generic_gui_window (cModuleName,
			CAIRO_DOCK_SIMPLE_PANEL_WIDTH, CAIRO_DOCK_SIMPLE_PANEL_HEIGHT,
			(CairoDockApplyConfigFunc) on_apply_config_module_simple,
			NULL,
			(GFreeFunc) on_destroy_config_module_simple);
	}
	else
	{
		_reset_current_module_widget ();
	}
	
	CairoDockModuleInstance *pModuleInstance = (pModule->pInstancesList != NULL ? pModule->pInstancesList->data : NULL);
	gchar *cConfFilePath = (pModuleInstance != NULL ? pModuleInstance->cConfFilePath : pModule->cConfFilePath);
	gchar *cOriginalConfFilePath = g_strdup_printf ("%s/%s", pModule->pVisitCard->cShareDataDir, pModule->pVisitCard->cConfFileName);
	
	//\_____________ On insere l'IHM du fichier de conf.
	_present_module_widget (s_pSimpleConfigModuleWindow,
		pModuleInstance,
		cConfFilePath,
		pModule->pVisitCard->cGettextDomain,
		cOriginalConfFilePath);
	
	gtk_widget_show_all (s_pSimpleConfigModuleWindow);
	g_free (cOriginalConfFilePath);
	s_cCurrentModuleName = pModule->pVisitCard->cModuleName;
}

static void set_status_message_on_gui (const gchar *cMessage)
{
	GtkWidget *pStatusBar = NULL;
	if (s_pSimpleConfigModuleWindow != NULL)
	{
		pStatusBar = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "status-bar");
	}
	else if (s_pSimpleConfigWindow != NULL)
	{
		pStatusBar = g_object_get_data (G_OBJECT (s_pSimpleConfigWindow), "status-bar");
	}
	if (pStatusBar == NULL)
		return ;
	gtk_statusbar_pop (GTK_STATUSBAR (pStatusBar), 0);  // clear any previous message, underflow is allowed.
	gtk_statusbar_push (GTK_STATUSBAR (pStatusBar), 0, cMessage);
}

static gboolean module_is_opened (CairoDockModuleInstance *pModuleInstance)
{
	if (s_pSimpleConfigModuleWindow == NULL || s_cCurrentModuleName == NULL || pModuleInstance == NULL)
		return FALSE;
	
	if (strcmp (s_cCurrentModuleName, pModuleInstance->pModule->pVisitCard->cModuleName) != 0)
		return FALSE;
	
	gchar *cConfFilePath = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "conf-file");
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

static gboolean _test_one_module_name (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer *data)
{
	gchar *cResult = NULL;
	gtk_tree_model_get (model, iter, CAIRO_DOCK_MODEL_RESULT, &cResult, -1);
	cd_debug ("- %s !\n", cResult
	);
	if (cResult && strcmp (data[0], cResult) == 0)
	{
		GtkTreeIter *iter_to_fill = data[1];
		memcpy (iter_to_fill, iter, sizeof (GtkTreeIter));
		gboolean *bFound = data[2];
		*bFound = TRUE;
		g_free (cResult);
		return TRUE;
	}
	g_free (cResult);
	return FALSE;
}
static void deactivate_module_in_gui (const gchar *cModuleName)
{
	if (s_pSimpleConfigWindow == NULL || cModuleName == NULL)
		return ;
	
	// desactive la ligne qui va bien dans le gtk_list_store...
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	g_return_if_fail (pModule != NULL);
	
	GSList *pCurrentWidgetList = g_object_get_data (G_OBJECT (s_pSimpleConfigWindow), "widget-list");
	g_return_if_fail (pCurrentWidgetList != NULL);
	
	GSList *pWidgetList = cairo_dock_find_widgets_from_name (pCurrentWidgetList, "Add-ons", "modules");
	g_return_if_fail (pWidgetList != NULL);
	
	GtkWidget *pTreeView = pWidgetList->data;
	
	GtkTreeModel *pModele = gtk_tree_view_get_model (GTK_TREE_VIEW (pTreeView));
	gboolean bFound = FALSE;
	GtkTreeIter iter;
	memset (&iter, 0, sizeof (GtkTreeIter));
	gconstpointer data[3] = {cModuleName, &iter, &bFound};
	gtk_tree_model_foreach (GTK_TREE_MODEL (pModele), (GtkTreeModelForeachFunc) _test_one_module_name, data);
	
	if (bFound)
	{
		gtk_list_store_set (GTK_LIST_STORE (pModele), &iter,
			CAIRO_DOCK_MODEL_ACTIVE, FALSE, -1);
	}
}

static GSList *get_widgets_from_name (const gchar *cGroupName, const gchar *cKeyName)
{
	g_return_val_if_fail (s_pSimpleConfigModuleWindow != NULL, NULL);
	GSList *pCurrentModuleWidgetList = g_object_get_data (G_OBJECT (s_pSimpleConfigModuleWindow), "widget-list");
	g_return_val_if_fail (pCurrentModuleWidgetList != NULL, NULL);
	return cairo_dock_find_widgets_from_name (pCurrentModuleWidgetList, cGroupName, cKeyName);
}

static void close_gui (void)
{
	if (s_pSimpleConfigWindow != NULL)
	{
		GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (s_pSimpleConfigWindow));
		GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
		GList *w = g_list_last (children);
		GtkWidget *pButtonsHBox = w->data;
		
		children = gtk_container_get_children (GTK_CONTAINER (pButtonsHBox));
		w = g_list_last (children);
		GtkWidget *pQuitButton = w->data;
		
		gboolean bReturn;
		g_signal_emit_by_name (pQuitButton, "clicked", NULL, &bReturn);
	}
	if (s_pSimpleConfigModuleWindow != NULL)
	{
		GtkWidget *pMainVBox = gtk_bin_get_child (GTK_BIN (s_pSimpleConfigModuleWindow));
		GList *children = gtk_container_get_children (GTK_CONTAINER (pMainVBox));
		GList *w = g_list_last (children);
		GtkWidget *pButtonsHBox = w->data;
		
		children = gtk_container_get_children (GTK_CONTAINER (pButtonsHBox));
		w = g_list_last (children);
		GtkWidget *pQuitButton = w->data;
		
		gboolean bReturn;
		g_signal_emit_by_name (pQuitButton, "clicked", NULL, &bReturn);
	}
}

void cairo_dock_register_simple_gui_backend (void)
{
	CairoDockGuiBackend *pBackend = g_new0 (CairoDockGuiBackend, 1);
	
	pBackend->show_main_gui 			= show_main_gui;
	pBackend->show_module_instance_gui 	= show_module_instance_gui;
	pBackend->show_module_gui 			= show_module_gui;
	pBackend->set_status_message_on_gui = set_status_message_on_gui;
	pBackend->module_is_opened 			= module_is_opened;
	pBackend->deactivate_module_in_gui 	= deactivate_module_in_gui;
	pBackend->get_widgets_from_name 	= get_widgets_from_name;
	pBackend->close_gui 				= close_gui;
	
	cairo_dock_register_gui_backend (pBackend);
}

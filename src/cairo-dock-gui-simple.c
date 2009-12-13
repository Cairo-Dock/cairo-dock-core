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

#include "cairo-dock-struct.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gui-callbacks.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-gui-simple.h"

#define CAIRO_DOCK_PREVIEW_WIDTH 200
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_SIMPLE_PANEL_WIDTH 1000
#define CAIRO_DOCK_SIMPLE_PANEL_HEIGHT 800

static GtkWidget *s_pSimpleConfigWindow = NULL;
static int s_iIconSize;

extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern CairoDock *g_pMainDock;
extern int g_iXScreenWidth[2], g_iXScreenHeight[2];
extern gchar *g_cCairoDockDataDir;

#define cd_reload(module_name) do {\
	pInternalModule = cairo_dock_find_internal_module_from_name (module_name);\
	if (pInternalModule != NULL) \
		cairo_dock_reload_internal_module_from_keyfile (pInternalModule, pKeyFile);\
	} while (0)

static gboolean on_apply_config_simple (gpointer data)
{
	g_print ("pouet\n");
	
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
	
	gboolean bShowOnClick = g_key_file_get_boolean (pSimpleKeyFile, "Behavior", "show on click", NULL);
	g_key_file_set_boolean (pKeyFile, "Accessibility", "show on click", bShowOnClick);
	
	int iTaskbarType = g_key_file_get_integer (pSimpleKeyFile, "Behavior", "taskbar", NULL);
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
	
	gchar *cIconOrder = g_key_file_get_string (pSimpleKeyFile, "Appearance", "icon's type order", NULL);
	g_key_file_set_string (pKeyFile, "Icons", "icon's type order", cIconOrder);
	g_free (cIconOrder);
	
	gboolean bMixAppletsAndLaunchers = g_key_file_get_boolean (pSimpleKeyFile, "Appearance", "mix applets with launchers", NULL);
	g_key_file_set_boolean (pKeyFile, "Icons", "mix applets with launchers", bMixAppletsAndLaunchers);
	
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
	
	/// recharger aussi les plug-ins d'animation...
	
	
	return TRUE;
}

static void on_destroy_config_simple (gpointer data)
{
	g_print ("pouic\n");
	s_pSimpleConfigWindow = NULL;
}


GtkWidget *cairo_dock_build_simplified_ihm ()
{
	if (s_pSimpleConfigWindow != NULL)
	{
		gtk_window_present (GTK_WINDOW (s_pSimpleConfigWindow));
		return s_pSimpleConfigWindow;
	}
	
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
	
	// comportement
	CairoDockPositionType iScreenBorder = ((! g_pMainDock->container.bIsHorizontal) << 1) | (! g_pMainDock->container.bDirectionUp);
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "screen border", iScreenBorder);
	
	gboolean iVisibility;
	if (myAccessibility.bReserveSpace)
		iVisibility = 1;
	else if (myAccessibility.bPopUp)
		iVisibility = 2;
	else if (myAccessibility.bAutoHide)
		iVisibility = 3;
	else if (myAccessibility.bAutoHideOnMaximized)
		iVisibility = 4;
	else if (myAccessibility.cRaiseDockShortcut)
		iVisibility = 5;
	else
		iVisibility = 0;
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "visibility", iVisibility);
	
	g_key_file_set_boolean (pSimpleKeyFile, "Behavior", "show on click", myAccessibility.bShowSubDockOnClick);
	
	int iTaskbarType;
	if (! myTaskBar.bShowAppli)
		iTaskbarType = 0;
	else if (myTaskBar.bHideVisibleApplis)
		iTaskbarType = 1;
	else if (myTaskBar.bGroupAppliByClass)
		iTaskbarType = 2;
	else
		iTaskbarType = 3;
	g_key_file_set_integer (pSimpleKeyFile, "Behavior", "taskbar", iTaskbarType);
	
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
			iIconSize = 3;  // moyennes.
	}
	else  // grandes.
		iIconSize = 4;
	
	g_key_file_set_integer (pSimpleKeyFile, "Appearance", "icon size", iIconSize);
	s_iIconSize = iIconSize;	
	
	g_key_file_set_integer_list (pSimpleKeyFile, "Appearance", "icon's type order", myIcons.iIconsTypesList, 3);
	
	g_key_file_set_boolean (pSimpleKeyFile, "Appearance", "mix applets with launchers", myIcons.bMixAppletsAndLaunchers);
	
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "main dock view", myViews.cMainDockDefaultRendererName);
	
	g_key_file_set_string (pSimpleKeyFile, "Appearance", "sub-dock view", myViews.cSubDockDefaultRendererName);
	
	
	
	cairo_dock_write_keys_to_file (pSimpleKeyFile, cConfFilePath);
	
	//\_____________ On construit la fenetre.
	cairo_dock_build_normal_gui (cConfFilePath,
		NULL,
		"Config Simple",
		1000, 800,
		(CairoDockApplyConfigFunc) on_apply_config_simple,
		NULL,
		(GFreeFunc) on_destroy_config_simple,
		&s_pSimpleConfigWindow);
}

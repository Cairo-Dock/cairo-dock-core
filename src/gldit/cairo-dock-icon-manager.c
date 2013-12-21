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

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "gldi-config.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-module-instance-manager.h"  // gldi_module_instance_reload
#include "cairo-dock-desklet-manager.h"  // gldi_desklets_foreach_icons
#include "cairo-dock-utils.h"  // cairo_dock_colors_differ
#include "cairo-dock-log.h"
#include "cairo-dock-config.h"
#include "cairo-dock-class-manager.h"  // cairo_dock_deinhibite_class
#include "cairo-dock-draw.h"  // cairo_dock_render_icon_notification
#include "cairo-dock-draw-opengl.h"  // cairo_dock_destroy_icon_fbo
#include "cairo-dock-container.h"
#include "cairo-dock-dock-manager.h"  // gldi_icons_foreach_in_docks
#include "cairo-dock-dialog-manager.h"  // cairo_dock_remove_dialog_if_any
#include "cairo-dock-data-renderer.h"  // cairo_dock_remove_data_renderer_on_icon
#include "cairo-dock-file-manager.h"  // cairo_dock_fm_remove_monitor_full
#include "cairo-dock-animations.h"  // cairo_dock_animation_will_be_visible
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-icon-facility.h"  // gldi_icons_foreach_of_type
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_open_key_file
#include "cairo-dock-indicator-manager.h"  // cairo_dock_unload_indicator_textures
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_get_current
#include "cairo-dock-user-icon-manager.h"  // GLDI_OBJECT_IS_USER_ICON
#include "cairo-dock-separator-manager.h"  // GLDI_OBJECT_IS_SEPARATOR_ICON
#include "cairo-dock-applications-manager.h"  // GLDI_OBJECT_IS_APPLI_ICON
#include "cairo-dock-launcher-manager.h"  // GLDI_OBJECT_IS_LAUNCHER_ICON
#include "cairo-dock-applet-manager.h"  // GLDI_OBJECT_IS_APPLET_ICON
#include "cairo-dock-backends-manager.h"  // cairo_dock_foreach_icon_container_renderer
#include "cairo-dock-style-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-icon-manager.h"

// public (manager, config, data)
CairoIconsParam myIconsParam;
GldiManager myIconsMgr;
GldiObjectManager myIconObjectMgr;
CairoDockImageBuffer g_pIconBackgroundBuffer;
GLuint g_pGradationTexture[2]={0, 0};

// dependencies
extern CairoDock *g_pMainDock;
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;
extern gchar *g_cCurrentIconsPath;

// private
static GList *s_pFloatingIconsList = NULL;
static int s_iNbNonStickyLaunchers = 0;
static GtkIconTheme *s_pIconTheme = NULL;
static gboolean s_bUseLocalIcons = FALSE;
static gboolean s_bUseDefaultTheme = TRUE;
static guint s_iSidReloadTheme = 0;

static void _cairo_dock_unload_icon_textures (void);
static void _cairo_dock_unload_icon_theme (void);
static void _on_icon_theme_changed (GtkIconTheme *pIconTheme, gpointer data);


void gldi_icons_foreach (GldiIconFunc pFunction, gpointer pUserData)
{
	gldi_icons_foreach_in_docks (pFunction, pUserData);
	gldi_desklets_foreach_icons (pFunction, pUserData);
}

  /////////////////////////
 /// ICONS PER DESKTOP ///
/////////////////////////

#define _is_invisible_on_this_desktop(icon, index) (icon->iSpecificDesktop != 0  /*specific desktop is defined*/ \
	&& icon->iSpecificDesktop != index  /*specific desktop is not the current one*/ \
	&& icon->iSpecificDesktop <= g_desktopGeometry.iNbDesktops * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY)  /*specific desktop is reachable*/

static void _cairo_dock_detach_launcher (Icon *pIcon)
{
	gchar *cParentDockName = g_strdup (pIcon->cParentDockName);
	gldi_icon_detach (pIcon);  // this will set cParentDockName to NULL
	
	pIcon->cParentDockName = cParentDockName;  // put it back !
}
static void _hide_launcher_on_other_desktops (Icon *icon, int index)
{
	cd_debug ("%s (%s, iNumViewport=%d)", __func__, icon->cName, icon->iSpecificDesktop);
	if (_is_invisible_on_this_desktop (icon, index))
	{
		if (! g_list_find (s_pFloatingIconsList, icon))  // paranoia
		{
			cd_debug ("launcher %s is not present on this desktop", icon->cName);
			_cairo_dock_detach_launcher (icon);
			s_pFloatingIconsList = g_list_prepend (s_pFloatingIconsList, icon);
		}
	}
}
static void _hide_icon_on_other_desktops (Icon *icon, gpointer data)
{
	if (GLDI_OBJECT_IS_USER_ICON (icon))
	{
		int index = GPOINTER_TO_INT (data);
		_hide_launcher_on_other_desktops (icon, index);
	}
}
static void _show_launcher_on_this_desktop (Icon *icon, int index)
{
	if (! _is_invisible_on_this_desktop (icon, index))
	{
		cd_debug (" => est visible sur ce viewport (iSpecificDesktop = %d).",icon->iSpecificDesktop);
		s_pFloatingIconsList = g_list_remove (s_pFloatingIconsList, icon);
		
		CairoDock *pParentDock = gldi_dock_get (icon->cParentDockName);
		if (pParentDock != NULL)
		{
			gldi_icon_insert_in_container (icon, CAIRO_CONTAINER(pParentDock), ! CAIRO_DOCK_ANIMATE_ICON);
		}
		else  // the dock doesn't exist any more -> free the icon
		{
			icon->iSpecificDesktop = 0;  // pour ne pas qu'elle soit enlevee de la liste en parallele.
			gldi_object_delete (GLDI_OBJECT (icon));
		}
	}
}

void cairo_dock_hide_show_launchers_on_other_desktops (void )  /// TODO: add a mechanism to hide an icon in a dock (or even in a container ?) without detaching it...
{
	if (s_iNbNonStickyLaunchers <= 0)
		return ;
	
	// calculate the index of the current desktop
	int iCurrentDesktop = 0, iCurrentViewportX = 0, iCurrentViewportY = 0;
	gldi_desktop_get_current (&iCurrentDesktop, &iCurrentViewportX, &iCurrentViewportY);
	int index = iCurrentDesktop * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY + iCurrentViewportX * g_desktopGeometry.iNbViewportY + iCurrentViewportY + 1;  // +1 car on commence a compter a partir de 1.
	
	// first detach what shouldn't be shown on this desktop
	gldi_icons_foreach_in_docks ((GldiIconFunc)_hide_icon_on_other_desktops, GINT_TO_POINTER (index));
	
	// then reattach what was eventually missing
	Icon *icon;
	GList *ic = s_pFloatingIconsList, *next_ic;
	while (ic != NULL)
	{
		next_ic = ic->next;  // get the next element now, because '_show_launcher_on_this_desktop' might remove 'ic' from the list.
		icon = ic->data;
		_show_launcher_on_this_desktop (icon, index);
		ic = next_ic;
	}
}

static gboolean _on_change_current_desktop_viewport_notification (G_GNUC_UNUSED gpointer data)
{
	cairo_dock_hide_show_launchers_on_other_desktops ();
	return GLDI_NOTIFICATION_LET_PASS;
}

static void _cairo_dock_delete_floating_icons (void)
{
	Icon *icon;
	GList *ic;
	for (ic = s_pFloatingIconsList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->iSpecificDesktop = 0;  // pour ne pas qu'elle soit enlevee de la liste en parallele.
		gldi_object_unref (GLDI_OBJECT (icon));
	}
	g_list_free (s_pFloatingIconsList);
	s_pFloatingIconsList = NULL;
	s_iNbNonStickyLaunchers = 0;
}

void cairo_dock_set_specified_desktop_for_icon (Icon *pIcon, int iSpecificDesktop)
{
	if (iSpecificDesktop != 0 && pIcon->iSpecificDesktop == 0)
	{
		s_iNbNonStickyLaunchers ++;
	}
	else if (iSpecificDesktop == 0 && pIcon->iSpecificDesktop != 0)
	{
		s_iNbNonStickyLaunchers --;
	}
	pIcon->iSpecificDesktop = iSpecificDesktop;
}


  //////////////////
 /// ICON THEME ///
//////////////////

/*
 * GTK_ICON_SIZE_MENU:          16
 * GTK_ICON_SIZE_SMALL_TOOLBAR: 18
 * GTK_ICON_SIZE_BUTTON:        20
 * GTK_ICON_SIZE_LARGE_TOOLBAR: 24
 * GTK_ICON_SIZE_DND:           32
 * GTK_ICON_SIZE_DIALOG:        48
 */
gint cairo_dock_search_icon_size (GtkIconSize iIconSize)
{
	gint iWidth, iHeight;
	if (! gtk_icon_size_lookup (iIconSize, &iWidth, &iHeight))
		return CAIRO_DOCK_DEFAULT_ICON_SIZE;

	return MAX (iWidth, iHeight);
}

gchar *cairo_dock_search_icon_s_path (const gchar *cFileName, gint iDesiredIconSize)
{
	g_return_val_if_fail (cFileName != NULL, NULL);
	
	//\_______________________ easy cases: we receive a path.
	if (*cFileName == '~')
	{
		return g_strdup_printf ("%s%s", g_getenv ("HOME"), cFileName+1);
	}
	
	if (*cFileName == '/')
	{
		return g_strdup (cFileName);
	}
	
	//\_______________________ check for the presence of suffix and version number.
	g_return_val_if_fail (s_pIconTheme != NULL, NULL);
	
	GString *sIconPath = g_string_new ("");
	const gchar *cSuffixTab[4] = {".svg", ".png", ".xpm", NULL};
	gboolean bHasSuffix=FALSE, bFileFound=FALSE, bHasVersion=FALSE;
	GtkIconInfo* pIconInfo = NULL;
	int j;
	gchar *str = strrchr (cFileName, '.');
	bHasSuffix = (str != NULL && g_ascii_isalpha (*(str+1)));  // exemple : "firefox.svg", but not "firefox-3.0"
	bHasVersion = (str != NULL && g_ascii_isdigit (*(str+1)) && g_ascii_isdigit (*(str-1)) && str-1 != cFileName);  // doit finir par x.y, x et y ayant autant de chiffres que l'on veut.
	
	//\_______________________ search in the local icons folder if enabled.
	if (s_bUseLocalIcons)
	{
		if (! bHasSuffix)  // test all the suffix one by one.
		{
			j = 0;
			while (cSuffixTab[j] != NULL)
			{
				g_string_printf (sIconPath, "%s/%s%s", g_cCurrentIconsPath, cFileName, cSuffixTab[j]);
				if ( g_file_test (sIconPath->str, G_FILE_TEST_EXISTS) )
				{
					bFileFound = TRUE;
					break;
				}
				j ++;
			}
		}
		else  // just test the file.
		{
			g_string_printf (sIconPath, "%s/%s", g_cCurrentIconsPath, cFileName);
			bFileFound = g_file_test (sIconPath->str, G_FILE_TEST_EXISTS);
		}
	}
	
	//\_______________________ search in the icon theme
	if (! bFileFound)  // didn't found/search in the local icons, so try the icon theme.
	{
		g_string_assign (sIconPath, cFileName);
		if (bHasSuffix)  // on vire le suffixe pour chercher tous les formats dans le theme d'icones.
		{
			gchar *str = strrchr (sIconPath->str, '.');
			if (str != NULL)
				*str = '\0';
		}

		pIconInfo = gtk_icon_theme_lookup_icon (s_pIconTheme,
			sIconPath->str,
			iDesiredIconSize, // GTK_ICON_LOOKUP_FORCE_SIZE if size < 30 ?? -> icons can be different // a lot of themes now use only svg files.
			GTK_ICON_LOOKUP_FORCE_SVG);
		if (pIconInfo == NULL && ! s_bUseLocalIcons && ! bHasVersion)  // if we were not using the default theme and didn't find any icon, let's try with the default theme (for instance gvfs will give us names from the default theme, and they might not exist in our current theme); if it has a version, we'll retry without it.
		{
			pIconInfo = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),  // the default theme is mapped in shared memory so it's available at any time.
				sIconPath->str,
				iDesiredIconSize,
				GTK_ICON_LOOKUP_FORCE_SVG);
		}
		if (pIconInfo != NULL)
		{
			g_string_assign (sIconPath, gtk_icon_info_get_filename (pIconInfo));
			bFileFound = TRUE;
			#if GTK_CHECK_VERSION (3, 8, 0)
			g_object_unref (G_OBJECT (pIconInfo));
			#else
			gtk_icon_info_free (pIconInfo);
			#endif
		}
	}
	
	
	//\_______________________ si rien trouve, on cherche sans le numero de version.
	if (! bFileFound && bHasVersion)
	{
		cd_debug ("on cherche sans le numero de version...");
		g_string_assign (sIconPath, cFileName);
		gchar *str = strrchr (sIconPath->str, '.');
		str --;  // on sait que c'est un digit.
		str --;
		while ((g_ascii_isdigit (*str) || *str == '.' || *str == '-') && (str != sIconPath->str))
			str --;
		if (str != sIconPath->str)
		{
			*(str+1) = '\0';
			cd_debug (" on cherche '%s'...", sIconPath->str);
			gchar *cPath = cairo_dock_search_icon_s_path (sIconPath->str, iDesiredIconSize);
			if (cPath != NULL)
			{
				bFileFound = TRUE;
				g_string_assign (sIconPath, cPath);
				g_free (cPath);
			}
		}
	}
	
	if (! bFileFound)
	{
		g_string_free (sIconPath, TRUE);
		return NULL;
	}
	
	gchar *cIconPath = sIconPath->str;
	g_string_free (sIconPath, FALSE);
	return cIconPath;
}

void cairo_dock_add_path_to_icon_theme (const gchar *cThemePath)
{
	if (s_bUseDefaultTheme)
	{
		g_signal_handlers_block_matched (s_pIconTheme,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _on_icon_theme_changed, NULL);
	}
	gtk_icon_theme_append_search_path (s_pIconTheme,
		cThemePath);  /// TODO: does it check for unicity ?...
	gtk_icon_theme_rescan_if_needed (s_pIconTheme);
	if (s_bUseDefaultTheme)
	{
		g_signal_handlers_unblock_matched (s_pIconTheme,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _on_icon_theme_changed, NULL);  // will do nothing if the callback has not been connected
	}
}

void cairo_dock_remove_path_from_icon_theme (const gchar *cThemePath)
{
	if (! GTK_IS_ICON_THEME (s_pIconTheme))
		return;
	g_signal_handlers_block_matched (s_pIconTheme,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, _on_icon_theme_changed, NULL);
	
	gchar **paths = NULL;
	gint iNbPaths = 0;
	gtk_icon_theme_get_search_path (s_pIconTheme, &paths, &iNbPaths);
	int i;
	for (i = 0; i < iNbPaths; i++)  // on cherche sa position dans le tableau.
	{
		if (strcmp (paths[i], cThemePath))
			break;
	}
	if (i < iNbPaths)  // trouve
	{
		g_free (paths[i]);
		for (i = i+1; i < iNbPaths; i++)  // on decale tous les suivants vers l'arriere.
		{
			paths[i-1] = paths[i];
		}
		paths[i-1] = NULL;
		gtk_icon_theme_set_search_path (s_pIconTheme, (const gchar **)paths, iNbPaths - 1);
	}
	g_strfreev (paths);
	
	g_signal_handlers_unblock_matched (s_pIconTheme,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, _on_icon_theme_changed, NULL);  // will do nothing if the callback has not been connected
}


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoIconsParam *pIcons)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	//\___________________ Reflets.
	pIcons->fReflectHeightRatio = cairo_dock_get_double_key_value (pKeyFile, "Icons", "field depth", &bFlushConfFileNeeded, 0.7, NULL, NULL);

	pIcons->fAlbedo = cairo_dock_get_double_key_value (pKeyFile, "Icons", "albedo", &bFlushConfFileNeeded, .6, NULL, NULL);

#ifndef AVOID_PATENT_CRAP
	double fMaxScale = cairo_dock_get_double_key_value (pKeyFile, "Icons", "zoom max", &bFlushConfFileNeeded, 0., NULL, NULL);
	if (fMaxScale == 0)
	{
		pIcons->fAmplitude = g_key_file_get_double (pKeyFile, "Icons", "amplitude", NULL);
		fMaxScale = 1 + pIcons->fAmplitude;
		g_key_file_set_double (pKeyFile, "Icons", "zoom max", fMaxScale);
	}
	else
		pIcons->fAmplitude = fMaxScale - 1;
#else
	pIcons->fAmplitude = 0.;
#endif
	
	pIcons->iSinusoidWidth = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "sinusoid width", &bFlushConfFileNeeded, 250, NULL, NULL);
	pIcons->iSinusoidWidth = MAX (1, pIcons->iSinusoidWidth);

	pIcons->iIconGap = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "icon gap", &bFlushConfFileNeeded, 0, NULL, NULL);

	//\___________________ Ficelle.
	pIcons->iStringLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "string width", &bFlushConfFileNeeded, 0, NULL, NULL);

	gdouble couleur[4];
	cairo_dock_get_double_list_key_value (pKeyFile, "Icons", "string color", &bFlushConfFileNeeded, pIcons->fStringColor, 4, couleur, NULL, NULL);

	pIcons->fAlphaAtRest = cairo_dock_get_double_key_value (pKeyFile, "Icons", "alpha at rest", &bFlushConfFileNeeded, 1., NULL, NULL);
	
	//\___________________ Theme d'icone.
	pIcons->cIconTheme = cairo_dock_get_string_key_value (pKeyFile, "Icons", "default icon directory", &bFlushConfFileNeeded, NULL, "Launchers", NULL);
	if (g_key_file_has_key (pKeyFile, "Icons", "local icons", NULL))  // anciens parametres.
	{
		bFlushConfFileNeeded = TRUE;
		gboolean bUseLocalIcons = g_key_file_get_boolean (pKeyFile, "Icons", "local icons", NULL);
		if (bUseLocalIcons)
		{
			g_free (pIcons->cIconTheme);
			pIcons->cIconTheme = g_strdup ("_Custom Icons_");
			g_key_file_set_string (pKeyFile, "Icons", "default icon directory", pIcons->cIconTheme);
		}
	}
	
	gchar *cLauncherBackgroundImageName = cairo_dock_get_string_key_value (pKeyFile, "Icons", "icons bg", &bFlushConfFileNeeded, NULL, NULL, NULL);
	if (cLauncherBackgroundImageName != NULL)
	{
		pIcons->cBackgroundImagePath = cairo_dock_search_image_s_path (cLauncherBackgroundImageName);
		g_free (cLauncherBackgroundImageName);
	}
		
	//\___________________ icons size
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "launcher ", bFlushConfFileNeeded, pIcons->iIconWidth, pIcons->iIconHeight);
	if (pIcons->iIconWidth == 0)
		pIcons->iIconWidth = 48;
	if (pIcons->iIconHeight == 0)
		pIcons->iIconHeight = 48;
	
	//\___________________ Parametres des separateurs.
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "separator ", bFlushConfFileNeeded, pIcons->iSeparatorWidth, pIcons->iSeparatorHeight);
	if (pIcons->iSeparatorWidth == 0)
		pIcons->iSeparatorWidth = pIcons->iIconWidth;
	if (pIcons->iSeparatorHeight == 0)
		pIcons->iSeparatorHeight = pIcons->iIconHeight;
	if (pIcons->iSeparatorHeight > pIcons->iIconHeight)
		pIcons->iSeparatorHeight = pIcons->iIconHeight;
	
	pIcons->iSeparatorType = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "separator type", &bFlushConfFileNeeded, -1, NULL, NULL);
	if (pIcons->iSeparatorType >= CAIRO_DOCK_NB_SEPARATOR_TYPES)  // nouveau parametre, avant il etait dans dock-rendering.
	{
		pIcons->iSeparatorType = CAIRO_DOCK_NORMAL_SEPARATOR;  // ce qui suit est tres moche, mais c'est pour eviter d'avoir a repasser derriere tous les themes.
		gchar *cMainDockDefaultRendererName = g_key_file_get_string (pKeyFile, "Views", "main dock view", NULL);
		if (cMainDockDefaultRendererName && (strcmp (cMainDockDefaultRendererName, "3D plane") == 0 || strcmp (cMainDockDefaultRendererName, "Curve") == 0))
		{
			gchar *cRenderingConfFile = g_strdup_printf ("%s/plug-ins/rendering/rendering.conf", g_cCurrentThemePath);
			GKeyFile *keyfile = cairo_dock_open_key_file (cRenderingConfFile);
			g_free (cRenderingConfFile);
			if (keyfile == NULL)
				pIcons->iSeparatorType = CAIRO_DOCK_NORMAL_SEPARATOR;
			else
			{
				gsize length=0;
				if (strcmp (cMainDockDefaultRendererName, "3D plane") == 0)
				{
					pIcons->iSeparatorType = g_key_file_get_integer (keyfile, "Inclinated Plane", "draw separator", NULL);
				}
				else
				{
					pIcons->iSeparatorType = g_key_file_get_integer (keyfile, "Curve", "draw curve separator", NULL);
				}
				double *color = g_key_file_get_double_list (keyfile, "Inclinated Plane", "separator color", &length, NULL);
				if (length > 0)
					memcpy (pIcons->fSeparatorColor, color, length*sizeof (gdouble));
				else
				{
					pIcons->fSeparatorColor[0] = pIcons->fSeparatorColor[1] = pIcons->fSeparatorColor[2] = .9;
				}
				if (length < 4)
					pIcons->fSeparatorColor[3] = 1.;
				g_key_file_free (keyfile);
			}
		}
		g_key_file_set_integer (pKeyFile, "Icons", "separator type", pIcons->iSeparatorType);
		g_key_file_set_double_list (pKeyFile, "Icons", "separator color", pIcons->fSeparatorColor, 4);
		g_free (cMainDockDefaultRendererName);
	}
	else
	{
		double couleur[4] = {0.9,0.9,1.0,1.0};
		cairo_dock_get_double_list_key_value (pKeyFile, "Icons", "separator color", &bFlushConfFileNeeded, pIcons->fSeparatorColor, 4, couleur, NULL, NULL);
	}
	if (pIcons->iSeparatorType == CAIRO_DOCK_NORMAL_SEPARATOR)
		pIcons->cSeparatorImage = cairo_dock_get_string_key_value (pKeyFile, "Icons", "separator image", &bFlushConfFileNeeded, NULL, "Separators", NULL);

	pIcons->bRevolveSeparator = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "revolve separator image", &bFlushConfFileNeeded, TRUE, "Separators", NULL);

	pIcons->bConstantSeparatorSize = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "force size", &bFlushConfFileNeeded, TRUE, "Separators", NULL);
	
	//\___________________ labels font
	CairoIconsParam *pLabels = pIcons;
	gboolean bCustomFont = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "custom", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	gchar *cFont = (bCustomFont ? cairo_dock_get_string_key_value (pKeyFile, "Labels", "police", &bFlushConfFileNeeded, NULL, "Icons", NULL) : NULL);
	gldi_text_description_set_font (&pLabels->iconTextDescription, cFont);
	
	g_print ("pLabels->iconTextDescription.cFont: %s, %d\n", pLabels->iconTextDescription.cFont, pLabels->iconTextDescription.iSize);
	
	//\___________________ labels text color
	pLabels->iconTextDescription.bOutlined = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "text oulined", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	double couleur_backlabel[4] = {0., 0., 0., 0.85};
	double couleur_label[3] = {1., 1., 1.};
	gboolean bDefaultColors = (cairo_dock_get_integer_key_value (pKeyFile, "Labels", "colors", &bFlushConfFileNeeded, 0, NULL, NULL) == 0);
	pLabels->iconTextDescription.bUseDefaultColors = bDefaultColors;
	if (bDefaultColors)
	{
		pLabels->iconTextDescription.bOutlined = FALSE;
	}
	else
	{
		cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text color", &bFlushConfFileNeeded, pLabels->iconTextDescription.fColorStart, 3, couleur_label, "Labels", "text color start");
		
		double couleur_linelabel[4] = {0., 0., 0., 1};
		cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text line color", &bFlushConfFileNeeded, pLabels->iconTextDescription.fLineColor, 4, couleur_linelabel, NULL, NULL);
		
		cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text background color", &bFlushConfFileNeeded, pLabels->iconTextDescription.fBackgroundColor, 4, couleur_backlabel, "Icons", NULL);
		if (!g_key_file_has_key (pKeyFile, "Labels", "qi same", NULL))  // old params
		{
			gboolean bUseBackgroundForLabel = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "background for label", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
			if (! bUseBackgroundForLabel)
			{
				pLabels->iconTextDescription.fBackgroundColor[3] = 0;  // ne sera pas dessine.
				g_key_file_set_double_list (pKeyFile, "Icons", "text background color", pLabels->iconTextDescription.fBackgroundColor, 4);
			}
		}
	}
	
	pLabels->iconTextDescription.iMargin = cairo_dock_get_integer_key_value (pKeyFile, "Labels", "text margin", &bFlushConfFileNeeded, 4, NULL, NULL);
	
	//\___________________ quick-info
	gldi_text_description_copy (&pLabels->quickInfoTextDescription, &pLabels->iconTextDescription);
	pLabels->quickInfoTextDescription.iMargin = 1;  // to minimize the surface of the quick-info (0 would be too much).
	pLabels->quickInfoTextDescription.iSize = 12;  // no need to update the fd, it will be done when loading the text buffer
	
	gboolean bQuickInfoSameLook = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "qi same", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	if ( !bQuickInfoSameLook)
	{
		cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "qi bg color", &bFlushConfFileNeeded, pLabels->quickInfoTextDescription.fBackgroundColor, 4, couleur_backlabel, NULL, NULL);
		cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "qi text color", &bFlushConfFileNeeded, pLabels->quickInfoTextDescription.fColorStart, 3, couleur_label, NULL, NULL);
		pLabels->quickInfoTextDescription.bUseDefaultColors = FALSE;
	}
	
	pLabels->iLabelSize = (pLabels->iconTextDescription.iSize != 0 ?
		pLabels->iconTextDescription.iSize +
		(pLabels->iconTextDescription.bOutlined ? 2 : 0) +
		2 * pLabels->iconTextDescription.iMargin +
		6  // 2px linewidth + 3px to take into account the y offset of the characters + 1 px to take into account the gap between icon and label
		: 0);
	g_print ("pLabels->iLabelSize: %d (%d)\n", pLabels->iLabelSize, pLabels->iconTextDescription.iSize);
	
	//\___________________ labels visibility
	int iShowLabel = cairo_dock_get_integer_key_value (pKeyFile, "Labels", "show_labels", &bFlushConfFileNeeded, -1, NULL, NULL);
	gboolean bShow, bLabelForPointedIconOnly;
	if (iShowLabel == -1)  // nouveau parametre
	{
		if (g_key_file_has_key (pKeyFile, "Labels", "show labels", NULL))
			bShow = g_key_file_get_boolean (pKeyFile, "Labels", "show labels", NULL);
		else
			bShow = TRUE;
		bLabelForPointedIconOnly = g_key_file_get_boolean (pKeyFile, "System", "pointed icon only", NULL);
		iShowLabel = (! bShow ? 0 : (bLabelForPointedIconOnly ? 1 : 2));
		g_key_file_set_integer (pKeyFile, "Labels", "show_labels", iShowLabel);
	}
	else
	{
		bShow = (iShowLabel != 0);
		bLabelForPointedIconOnly = (iShowLabel == 1);
	}
	if (! bShow)
		pLabels->iconTextDescription.iSize = 0;

	pLabels->bLabelForPointedIconOnly = bLabelForPointedIconOnly;
	
	pLabels->fLabelAlphaThreshold = cairo_dock_get_double_key_value (pKeyFile, "Labels", "alpha threshold", &bFlushConfFileNeeded, 10., "System", NULL);
	pLabels->fLabelAlphaThreshold = (pLabels->fLabelAlphaThreshold + 10.) / 10.;  // [0;50] -> [1;6]
	
	return bFlushConfFileNeeded;
}


  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (CairoIconsParam *pIcons)
{
	g_free (pIcons->cSeparatorImage);
	g_free (pIcons->cBackgroundImagePath);
	g_free (pIcons->cIconTheme);
	
	// labels
	CairoIconsParam *pLabels = pIcons;
	gldi_text_description_reset (&pLabels->iconTextDescription);
	gldi_text_description_reset (&pLabels->quickInfoTextDescription);
}


  ////////////
 /// LOAD ///
////////////

static void _cairo_dock_load_icons_background_surface (const gchar *cImagePath)
{
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	
	int iSizeWidth = myIconsParam.iIconWidth * (1 + myIconsParam.fAmplitude);
	int iSizeHeight = myIconsParam.iIconHeight * (1 + myIconsParam.fAmplitude);
	
	cairo_dock_load_image_buffer (&g_pIconBackgroundBuffer,
		cImagePath,
		iSizeWidth,
		iSizeHeight,
		CAIRO_DOCK_FILL_SPACE);
}

static void _load_renderer (G_GNUC_UNUSED const gchar *cRenderername, CairoIconContainerRenderer *pRenderer, G_GNUC_UNUSED gpointer data)
{
	if (pRenderer && pRenderer->load)
		pRenderer->load ();
}
static void _cairo_dock_load_icon_textures (void)
{
	_cairo_dock_load_icons_background_surface (myIconsParam.cBackgroundImagePath);
	
	cairo_dock_foreach_icon_container_renderer ((GHFunc)_load_renderer, NULL);
}
static void _reload_in_desklet (CairoDesklet *pDesklet, G_GNUC_UNUSED gpointer data)
{
	if (CAIRO_DOCK_IS_APPLET (pDesklet->pIcon))
	{
		gldi_object_reload (GLDI_OBJECT(pDesklet->pIcon->pModuleInstance), FALSE);
	}
}
static gboolean _on_icon_theme_changed_idle (G_GNUC_UNUSED gpointer data)
{
	cd_debug ("");
	gldi_desklets_foreach ((GldiDeskletForeachFunc) _reload_in_desklet, NULL);
	cairo_dock_reload_buffers_in_all_docks (FALSE);
	s_iSidReloadTheme = 0;
	return FALSE;
}
static void _on_icon_theme_changed (G_GNUC_UNUSED GtkIconTheme *pIconTheme, G_GNUC_UNUSED gpointer data)
{
	cd_message ("theme has changed");
	// Reload the icons in idle, because this signal is triggered directly by 'gtk_icon_theme_set_search_path()'; so we may end reloading an applet in the middle of its work (ex.: Status-Notifier when the watcher terminates)
	if (s_iSidReloadTheme == 0)
		s_iSidReloadTheme = g_idle_add (_on_icon_theme_changed_idle, NULL);
}
static void _cairo_dock_load_icon_theme (void)
{
	g_return_if_fail (s_pIconTheme == NULL);
	if (myIconsParam.cIconTheme == NULL  // no icon theme defined => use the default one.
	|| strcmp (myIconsParam.cIconTheme, "_Custom Icons_") == 0)  // use custom icons and default theme as fallback
	{
		s_pIconTheme = gtk_icon_theme_get_default ();
		g_signal_connect (G_OBJECT (s_pIconTheme), "changed", G_CALLBACK (_on_icon_theme_changed), NULL);
		s_bUseDefaultTheme = TRUE;
		s_bUseLocalIcons = (myIconsParam.cIconTheme != NULL);
	}
	else  // use the given icon theme
	{
		s_pIconTheme = gtk_icon_theme_new ();
		gtk_icon_theme_set_custom_theme (s_pIconTheme, myIconsParam.cIconTheme);
		s_bUseLocalIcons = FALSE;
		s_bUseDefaultTheme = FALSE;
	}
}

static void load (void)
{
	cairo_dock_create_icon_fbo ();
	
	_cairo_dock_load_icon_theme ();
	
	_cairo_dock_load_icon_textures ();
}


  //////////////
 /// RELOAD ///
//////////////

static void _reload_separators (G_GNUC_UNUSED const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	///cairo_dock_remove_automatic_separators (pDock);
	gboolean bSeparatorsNeedReload = GPOINTER_TO_INT(data);
	gboolean bHasSeparator = FALSE;
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (GLDI_OBJECT_IS_SEPARATOR_ICON (icon))
		{
			if (bSeparatorsNeedReload)
			{
				cairo_dock_icon_set_requested_size (icon, 0, 0);
				cairo_dock_set_icon_size_in_dock (pDock, icon);
			}
			cairo_dock_load_icon_image (icon, icon->pContainer);
			bHasSeparator = TRUE;
		}
	}
	if (bHasSeparator)
	{
		if (bSeparatorsNeedReload)
			cairo_dock_update_dock_size (pDock);  // either to trigger the loading of the separator rendering, or to take into account the change in the separators size
		gtk_widget_queue_draw (pDock->container.pWidget);  // in any case, refresh the drawing
	}
}

static void _calculate_icons (G_GNUC_UNUSED const gchar *cDockName, CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	cairo_dock_calculate_dock_icons (pDock);
}

static void _cairo_dock_resize_one_dock (G_GNUC_UNUSED const gchar *cDockName, CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	cairo_dock_update_dock_size (pDock);
}

static void _reload_one_label (Icon *pIcon, G_GNUC_UNUSED gpointer data)
{
	cairo_dock_load_icon_text (pIcon);
	cairo_dock_load_icon_quickinfo (pIcon);
}

static void reload (CairoIconsParam *pPrevIcons, CairoIconsParam *pIcons)
{
	// if the separator size has changed, we need to re-allocate it, reload the image, and update the dock size
	// if the separator rendering has changed (type or color), we need to load the new rendering, which is done by the View (during the compute_size)
	// otherwise, we just need to redraw 
	gboolean bSeparatorsNeedReload = (pPrevIcons->iSeparatorWidth != pIcons->iSeparatorWidth
		|| pPrevIcons->iSeparatorHeight != pIcons->iSeparatorHeight
		|| pPrevIcons->iSeparatorType != pIcons->iSeparatorType
		|| cairo_dock_colors_differ (pPrevIcons->fSeparatorColor, pIcons->fSeparatorColor));  // same if color has changed (for the flat separator rendering)
	gboolean bSeparatorNeedRedraw = (g_strcmp0 (pPrevIcons->cSeparatorImage, pIcons->cSeparatorImage) != 0
		|| pPrevIcons->bRevolveSeparator != pIcons->bRevolveSeparator);
	
	if (bSeparatorsNeedReload || bSeparatorNeedRedraw)
	{
		gldi_docks_foreach ((GHFunc)_reload_separators, GINT_TO_POINTER(bSeparatorsNeedReload));
		return;
	}
	
	gboolean bThemeChanged = (g_strcmp0 (pIcons->cIconTheme, pPrevIcons->cIconTheme) != 0);
	if (bThemeChanged)
	{
		_cairo_dock_unload_icon_theme ();
		
		_cairo_dock_load_icon_theme ();
	}
	
	gboolean bIconBackgroundImagesChanged = FALSE;
	// if background images are different, reload them and trigger the reload of all icons
	if (g_strcmp0 (pPrevIcons->cBackgroundImagePath, pIcons->cBackgroundImagePath) != 0
	|| pPrevIcons->fAmplitude != pIcons->fAmplitude)
	{
		bIconBackgroundImagesChanged = TRUE;
		_cairo_dock_load_icons_background_surface (pIcons->cBackgroundImagePath);
	}
	
	///cairo_dock_create_icon_pbuffer ();
	cairo_dock_destroy_icon_fbo ();
	cairo_dock_create_icon_fbo ();
	
	if (pPrevIcons->iIconWidth != pIcons->iIconWidth ||
		pPrevIcons->iIconHeight != pIcons->iIconHeight ||
		pPrevIcons->iSeparatorWidth != pIcons->iSeparatorWidth ||
		pPrevIcons->iSeparatorHeight != pIcons->iSeparatorHeight ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude ||
		(!g_bUseOpenGL && pPrevIcons->fReflectHeightRatio != pIcons->fReflectHeightRatio) ||
		(!g_bUseOpenGL && pPrevIcons->fAlbedo != pIcons->fAlbedo) ||
		bThemeChanged ||
		bIconBackgroundImagesChanged)  // oui on ne fait pas dans la finesse.
	{
		cairo_dock_reload_buffers_in_all_docks (TRUE);
	}
	
	if (pPrevIcons->iIconWidth != pIcons->iIconWidth ||
		pPrevIcons->iIconHeight != pIcons->iIconHeight ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude)
	{
		_cairo_dock_unload_icon_textures ();
		myIndicatorsMgr.unload ();
		_cairo_dock_load_icon_textures ();
		myIndicatorsMgr.load ();
	}
	
	cairo_dock_set_all_views_to_default (0);  // met a jour la taille (decorations incluses) de tous les docks; le chargement des separateurs plats se fait dans le calcul de max dock size.
	gldi_docks_foreach ((GHFunc)_calculate_icons, NULL);
	gldi_docks_redraw_all_root ();
	
	// labels
	CairoIconsParam *pLabels = pIcons;
	CairoIconsParam *pPrevLabels = pPrevIcons;
	gldi_icons_foreach ((GldiIconFunc) _reload_one_label, NULL);
	
	if (pPrevLabels->iLabelSize != pLabels->iLabelSize)
	{
		gldi_docks_foreach ((GHFunc) _cairo_dock_resize_one_dock, NULL);
	}
}


  //////////////
 /// UNLOAD ///
//////////////

static void _unload_renderer (G_GNUC_UNUSED const gchar *cRenderername, CairoIconContainerRenderer *pRenderer, G_GNUC_UNUSED gpointer data)
{
	if (pRenderer && pRenderer->unload)
		pRenderer->unload ();
}
static void _cairo_dock_unload_icon_textures (void)
{
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	
	cairo_dock_foreach_icon_container_renderer ((GHFunc)_unload_renderer, NULL);
}
static void _cairo_dock_unload_icon_theme (void)
{
	if (s_bUseDefaultTheme)
		g_signal_handlers_disconnect_by_func (G_OBJECT(s_pIconTheme), G_CALLBACK(_on_icon_theme_changed), NULL);
	else
		g_object_unref (s_pIconTheme);
	s_pIconTheme = NULL;
}
static void unload (void)
{
	_cairo_dock_unload_icon_textures ();
	
	cairo_dock_destroy_icon_fbo ();
	
	_cairo_dock_delete_floating_icons ();
	
	if (g_pGradationTexture[0] != 0)
	{
		_cairo_dock_delete_texture (g_pGradationTexture[0]);
		g_pGradationTexture[0] = 0;
	}
	if (g_pGradationTexture[1] != 0)
	{
		_cairo_dock_delete_texture (g_pGradationTexture[1]);
		g_pGradationTexture[1] = 0;
	}
	
	cairo_dock_reset_source_context ();
	
	_cairo_dock_unload_icon_theme ();
}


  ////////////
 /// INIT ///
////////////

static gboolean on_style_changed (G_GNUC_UNUSED gpointer data)
{
	g_print ("%s (%d)\n", __func__, myIconsParam.iconTextDescription.bUseDefaultColors);
	if (myIconsParam.iconTextDescription.bUseDefaultColors)  // reload labels and quick-info
	{
		g_print (" reload labels...\n");
		gldi_icons_foreach ((GldiIconFunc) _reload_one_label, NULL);
	}
	
	if (myIconsParam.iconTextDescription.cFont == NULL)  // if label size changed, reload docks views
	{
		gldi_text_description_set_font (&myIconsParam.iconTextDescription, NULL);
		
		int iLabelSize = (myIconsParam.iconTextDescription.iSize != 0 ?
			myIconsParam.iconTextDescription.iSize +
			(myIconsParam.iconTextDescription.bOutlined ? 2 : 0) +
			2 * myIconsParam.iconTextDescription.iMargin +
			5  // linewidth + 2px to take into account the y offset of the characters + 1 px to take into account the gap between icon and label
			: 0);
		if (iLabelSize != myIconsParam.iLabelSize)
		{
			g_print ("myIconsParam.iLabelSize: %d (%d)\n", myIconsParam.iLabelSize, myIconsParam.iconTextDescription.iSize);
			myIconsParam.iLabelSize = iLabelSize;
			gldi_docks_foreach ((GHFunc) _cairo_dock_resize_one_dock, NULL);
		}
	}
	return GLDI_NOTIFICATION_LET_PASS;
}

static void init (void)
{
	gldi_object_register_notification (&myDesktopMgr,
		NOTIFICATION_DESKTOP_CHANGED,
		(GldiNotificationFunc) _on_change_current_desktop_viewport_notification,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myIconObjectMgr,
		NOTIFICATION_RENDER_ICON,
		(GldiNotificationFunc) cairo_dock_render_icon_notification,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myStyleMgr,
		NOTIFICATION_STYLE_CHANGED,
		(GldiNotificationFunc) on_style_changed,
		GLDI_RUN_AFTER, NULL);
}

  ///////////////
 /// MANAGER ///
///////////////

static void _load_image (Icon *icon)
{
	int iWidth = cairo_dock_icon_get_allocated_width (icon);
	int iHeight = cairo_dock_icon_get_allocated_height (icon);
	cairo_surface_t *pSurface = NULL;
	
	if (icon->cFileName)
	{
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName, MAX (iWidth, iHeight));
		if (cIconPath != NULL && *cIconPath != '\0')
			pSurface = cairo_dock_create_surface_from_image_simple (cIconPath,
				iWidth,
				iHeight);
		g_free (cIconPath);
	}
	cairo_dock_load_image_buffer_from_surface (&icon->image, pSurface, iWidth, iHeight);
}
static void init_object (GldiObject *obj, G_GNUC_UNUSED gpointer attr)
{
	Icon *icon = (Icon*)obj;
	icon->iface.load_image = _load_image;
}

static void reset_object (GldiObject *obj)
{
	Icon *icon = (Icon*)obj;
	cd_debug ("%s (%s , %s, %s)", __func__, icon->cName, icon->cClass, gldi_object_get_type(icon));
	
	GldiContainer *pContainer = cairo_dock_get_icon_container (icon);
	if (pContainer != NULL)
	{
		gldi_icon_detach (icon);
	}
	
	if (icon->cClass != NULL && (GLDI_OBJECT_IS_LAUNCHER_ICON (icon) || GLDI_OBJECT_IS_APPLET_ICON (icon)))  // c'est un inhibiteur.
		cairo_dock_deinhibite_class (icon->cClass, icon);  // unset the appli if it had any
	
	gldi_object_notify (icon, NOTIFICATION_STOP_ICON, icon);
	cairo_dock_remove_transition_on_icon (icon);
	cairo_dock_remove_data_renderer_on_icon (icon);
	
	if (icon->pSubDock != NULL)
		gldi_object_unref (GLDI_OBJECT(icon->pSubDock));
	
	if (icon->iSpecificDesktop != 0)
	{
		s_iNbNonStickyLaunchers --;
		s_pFloatingIconsList = g_list_remove(s_pFloatingIconsList, icon);
	}
	
	if (icon->iSidRedrawSubdockContent != 0)
		g_source_remove (icon->iSidRedrawSubdockContent);
	if (icon->iSidLoadImage != 0)  // remove timers after any function that could trigger one (for instance, cairo_dock_deinhibite_class calls cairo_dock_trigger_load_icon_buffers)
		g_source_remove (icon->iSidLoadImage);
	if (icon->iSidDoubleClickDelay != 0)
		g_source_remove (icon->iSidDoubleClickDelay);
	
	// free data
	g_free (icon->cDesktopFileName);
	g_free (icon->cFileName);
	g_free (icon->cName);
	g_free (icon->cInitialName);
	g_free (icon->cCommand);
	g_free (icon->cWorkingDirectory);
	g_free (icon->cBaseURI);
	g_free (icon->cParentDockName);  // on ne liberera pas le sous-dock ici sous peine de se mordre la queue, donc il faut l'avoir fait avant.
	g_free (icon->cClass);
	g_free (icon->cWmClass);
	g_free (icon->cQuickInfo);
	///g_free (icon->cLastAttentionDemand);
	g_free (icon->pHiddenBgColor);
	if (icon->pMimeTypes)
		g_strfreev (icon->pMimeTypes);
	
	cairo_dock_unload_image_buffer (&icon->image);
	
	cairo_dock_unload_image_buffer (&icon->label);
	
	cairo_dock_destroy_icon_overlays (icon);
}

void gldi_register_icons_manager (void)
{
	// Manager
	memset (&myIconsMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myIconsMgr), &myManagerObjectMgr, NULL);
	myIconsMgr.cModuleName  = "Icons";
	// interface
	myIconsMgr.init         = init;
	myIconsMgr.load         = load;
	myIconsMgr.unload       = unload;
	myIconsMgr.reload       = (GldiManagerReloadFunc)reload;
	myIconsMgr.get_config   = (GldiManagerGetConfigFunc)get_config;
	myIconsMgr.reset_config = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myIconsParam, 0, sizeof (CairoIconsParam));
	myIconsMgr.pConfig = (GldiManagerConfigPtr)&myIconsParam;
	myIconsMgr.iSizeOfConfig = sizeof (CairoIconsParam);
	// data
	memset (&g_pIconBackgroundBuffer, 0, sizeof (CairoDockImageBuffer));
	myIconsMgr.pData = (GldiManagerDataPtr)NULL;
	myIconsMgr.iSizeOfData = 0;
	
	// ObjectManager
	memset (&myIconObjectMgr, 0, sizeof (GldiObjectManager));
	myIconObjectMgr.cName        = "Icon";
	myIconObjectMgr.iObjectSize  = sizeof (Icon);
	// interface
	myIconObjectMgr.init_object  = init_object;
	myIconObjectMgr.reset_object = reset_object;
	// signals
	gldi_object_install_notifications (&myIconObjectMgr, NB_NOTIFICATIONS_ICON);
}

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
#include "cairo-dock-notifications.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-module-factory.h"  // cairo_dock_deinstanciate_module
#include "cairo-dock-log.h"
#include "cairo-dock-config.h"
#include "cairo-dock-class-manager.h"  // cairo_dock_deinhibite_class
#include "cairo-dock-draw.h"  // cairo_dock_render_icon_notification
#include "cairo-dock-draw-opengl.h"  // cairo_dock_destroy_icon_fbo
#include "cairo-dock-container.h"
#include "cairo-dock-dock-manager.h"  // cairo_dock_foreach_icons_in_docks
#include "cairo-dock-dialog-manager.h"  // cairo_dock_remove_dialog_if_any
#include "cairo-dock-data-renderer.h"  // cairo_dock_remove_data_renderer_on_icon
#include "cairo-dock-file-manager.h"  // cairo_dock_fm_remove_monitor_full
#include "cairo-dock-animations.h"  // cairo_dock_animation_will_be_visible
#include "cairo-dock-application-facility.h"  // cairo_dock_reserve_one_icon_geometry_for_window_manager
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-icon-facility.h"  // cairo_dock_foreach_icons_of_type
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_open_key_file
#include "cairo-dock-indicator-manager.h"  // cairo_dock_unload_indicator_textures
#include "cairo-dock-X-manager.h"  // cairo_dock_get_current_desktop_and_viewport
#include "cairo-dock-applications-manager.h"  // cairo_dock_unregister_appli
#include "cairo-dock-backends-manager.h"  // cairo_dock_foreach_icon_container_renderer
#define _MANAGER_DEF_
#include "cairo-dock-icon-manager.h"

// public (manager, config, data)
CairoIconsParam myIconsParam;
CairoIconsManager myIconsMgr;
CairoDockImageBuffer g_pIconBackgroundBuffer;
GLuint g_pGradationTexture[2]={0, 0};

// dependancies
extern CairoDock *g_pMainDock;
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;
extern gchar *g_cCurrentIconsPath;

extern CairoDockDesktopGeometry g_desktopGeometry;
extern gchar *g_cCurrentLaunchersPath;
extern CairoDock *g_pMainDock;

// private
static GList *s_pFloatingIconsList = NULL;
static int s_iNbNonStickyLaunchers = 0;
static GtkIconTheme *s_pIconTheme = NULL;
static gboolean s_bUseLocalIcons = FALSE;
static gboolean s_bUseDefaultTheme = TRUE;

static void _cairo_dock_unload_icon_textures (void);
static void _cairo_dock_unload_icon_theme (void);


void cairo_dock_free_icon (Icon *icon)
{
	if (icon == NULL)
		return ;
	cd_debug ("%s (%s , %s)", __func__, icon->cName, icon->cClass);
	
	cairo_dock_remove_dialog_if_any (icon);
	if (icon->iSidRedrawSubdockContent != 0)
		g_source_remove (icon->iSidRedrawSubdockContent);
	if (icon->iSidLoadImage != 0)
		g_source_remove (icon->iSidLoadImage);
	if (icon->iSidDoubleClickDelay != 0)
		g_source_remove (icon->iSidDoubleClickDelay);
	if (CAIRO_DOCK_IS_NORMAL_APPLI (icon))
		cairo_dock_unregister_appli (icon);
	else if (icon->cClass != NULL)  // c'est un inhibiteur.
		cairo_dock_deinhibite_class (icon->cClass, icon);
	if (icon->pModuleInstance != NULL)
		cairo_dock_deinstanciate_module (icon->pModuleInstance);
	cairo_dock_notify_on_object (icon, NOTIFICATION_STOP_ICON, icon);
	cairo_dock_notify_on_object (icon, NOTIFICATION_DESTROY, icon);
	cairo_dock_remove_transition_on_icon (icon);
	cairo_dock_remove_data_renderer_on_icon (icon);
	
	if (icon->iSpecificDesktop != 0)
	{
		s_iNbNonStickyLaunchers --;
		s_pFloatingIconsList = g_list_remove(s_pFloatingIconsList, icon);
	}
	
	cairo_dock_clear_notifications_on_object (icon);
	
	cairo_dock_free_icon_buffers (icon);
	cd_debug ("icon freeed");
	g_free (icon);
}


void cairo_dock_foreach_icons (CairoDockForeachIconFunc pFunction, gpointer pUserData)
{
	cairo_dock_foreach_icons_in_docks (pFunction, pUserData);
	cairo_dock_foreach_icons_in_desklets (pFunction, pUserData);
}

  /////////////////////////
 /// ICONS PER DESKTOP ///
/////////////////////////

static CairoDock *_cairo_dock_insert_floating_icon_in_dock (Icon *icon, CairoDock *pMainDock)
{
	cd_message ("%s (%s)", __func__, icon->cName);
	g_return_val_if_fail (pMainDock != NULL, NULL);
	
	//\_________________ On determine dans quel dock l'inserer.
	CairoDock *pParentDock = pMainDock;
	g_return_val_if_fail (pParentDock != NULL, NULL);
	
	//\_________________ On l'insere dans son dock parent (sans animation, puisqu'on n'anime pas non plus son enlevement).
	cairo_dock_insert_icon_in_dock (icon, pParentDock, ! CAIRO_DOCK_ANIMATE_ICON);
	cd_message (" insertion de %s complete (%.2f %.2fx%.2f) dans %s", icon->cName, icon->fInsertRemoveFactor, icon->fWidth, icon->fHeight, icon->cParentDockName);
	
	return pParentDock;
}
static CairoDock * _cairo_dock_detach_launcher(Icon *pIcon)
{
	cd_debug ("%s (%s, parent dock=%s)", __func__, pIcon->cName, pIcon->cParentDockName);
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pParentDock == NULL)
		return NULL;

	gchar *cParentDockName = g_strdup(pIcon->cParentDockName);
	cairo_dock_detach_icon_from_dock (pIcon, pParentDock); // this will set cParentDockName to NULL
	
	pIcon->cParentDockName = cParentDockName; // put it back !

	///cairo_dock_update_dock_size (pParentDock);
	return pParentDock;
}
static void _cairo_dock_hide_show_launchers_on_other_desktops (Icon *icon, CairoDock *pMainDock)
{
	if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon) || CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))
	{
		cd_debug ("%s (%s, iNumViewport=%d)", __func__, icon->cName, icon->iSpecificDesktop);
		CairoDock *pParentDock = NULL;
		// a specific desktop/viewport has been selected for this icon

		int iCurrentDesktop = 0, iCurrentViewportX = 0, iCurrentViewportY = 0;
		cairo_dock_get_current_desktop_and_viewport (&iCurrentDesktop, &iCurrentViewportX, &iCurrentViewportY);
		int index = iCurrentDesktop * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY + iCurrentViewportX * g_desktopGeometry.iNbViewportY + iCurrentViewportY + 1;  // +1 car on commence a compter a partir de 1.
		
		if (icon->iSpecificDesktop == 0 || icon->iSpecificDesktop == index || icon->iSpecificDesktop > g_desktopGeometry.iNbDesktops * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY)
		{
			cd_debug (" => est visible sur ce viewport (iSpecificDesktop = %d).",icon->iSpecificDesktop);
			// check that it is in the detached list
			if( g_list_find(s_pFloatingIconsList, icon) != NULL )
			{
				pParentDock = _cairo_dock_insert_floating_icon_in_dock (icon, pMainDock);
				s_pFloatingIconsList = g_list_remove(s_pFloatingIconsList, icon);
			}
		}
		else
		{
			cd_debug (" Viewport actuel = %d => n'est pas sur le viewport actuel.", iCurrentViewportX + g_desktopGeometry.iNbViewportX*iCurrentViewportY);
			if( g_list_find(s_pFloatingIconsList, icon) == NULL ) // only if not yet detached
			{
				cd_debug( "Detach launcher %s", icon->cName);
				pParentDock = _cairo_dock_detach_launcher(icon);
				s_pFloatingIconsList = g_list_prepend(s_pFloatingIconsList, icon);
			}
		}
		if (pParentDock != NULL)
			gtk_widget_queue_draw (pParentDock->container.pWidget);
	}
}

void cairo_dock_hide_show_launchers_on_other_desktops (CairoDock *pDock)
{
	if (s_iNbNonStickyLaunchers <= 0)
		return ;
	// first we detach what shouldn't be shown on this viewport
	g_list_foreach (pDock->icons, (GFunc)_cairo_dock_hide_show_launchers_on_other_desktops, pDock);
	// then we reattach what was eventually missing
	g_list_foreach (s_pFloatingIconsList, (GFunc)_cairo_dock_hide_show_launchers_on_other_desktops, pDock);
}

static gboolean _on_change_current_desktop_viewport_notification (gpointer data)
{
        CairoDock *pDock = g_pMainDock;
        cairo_dock_hide_show_launchers_on_other_desktops(pDock);
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static void _cairo_dock_delete_floating_icons (void)
{
	Icon *icon;
	GList *ic;
	for (ic = s_pFloatingIconsList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->iSpecificDesktop = 0;  // pour ne pas qu'elle soit enlevee de la liste en parallele.
		cairo_dock_free_icon (icon);
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

gchar *cairo_dock_search_icon_s_path (const gchar *cFileName)
{
	g_return_val_if_fail (cFileName != NULL, NULL);
	
	//\_______________________ cas faciles : l'entree est deja un chemin.
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
	int i, j;
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
			128,
			GTK_ICON_LOOKUP_FORCE_SVG);
		if (pIconInfo != NULL)
		{
			g_string_assign (sIconPath, gtk_icon_info_get_filename (pIconInfo));
			bFileFound = TRUE;
			gtk_icon_info_free (pIconInfo);
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
			cd_debug (" on cherche '%s'...\n", sIconPath->str);
			gchar *cPath = cairo_dock_search_icon_s_path (sIconPath->str);
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
	
	///pIcons->fReflectSize = pIcons->iIconHeight * pIcons->fReflectHeightRatio;
	
	//\___________________ labels font
	CairoIconsParam *pLabels = pIcons;
	gboolean bCustomFont = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "custom", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	gchar *cFontDescription = (bCustomFont ? cairo_dock_get_string_key_value (pKeyFile, "Labels", "police", &bFlushConfFileNeeded, NULL, "Icons", NULL) : NULL);
	if (cFontDescription == NULL)
		cFontDescription = cairo_dock_get_default_system_font ();
	
	PangoFontDescription *fd = pango_font_description_from_string (cFontDescription);
	pLabels->iconTextDescription.cFont = g_strdup (pango_font_description_get_family (fd));
	pLabels->iconTextDescription.iSize = pango_font_description_get_size (fd);
	if (!pango_font_description_get_size_is_absolute (fd))
		pLabels->iconTextDescription.iSize /= PANGO_SCALE;
	if (!bCustomFont)
		pLabels->iconTextDescription.iSize *= 1.33;  // c'est pas beau, mais ca evite de casser tous les themes.
	if (pLabels->iconTextDescription.iSize == 0)
		pLabels->iconTextDescription.iSize = 14;
	pLabels->iconTextDescription.iWeight = pango_font_description_get_weight (fd);
	pLabels->iconTextDescription.iStyle = pango_font_description_get_style (fd);
	
	if (g_key_file_has_key (pKeyFile, "Labels", "size", NULL))  // anciens parametres.
	{
		pLabels->iconTextDescription.iSize = g_key_file_get_integer (pKeyFile, "Labels", "size", NULL);
		int iLabelWeight = g_key_file_get_integer (pKeyFile, "Labels", "weight", NULL);
		pLabels->iconTextDescription.iWeight = cairo_dock_get_pango_weight_from_1_9 (iLabelWeight);
		gboolean bLabelStyleItalic = g_key_file_get_boolean (pKeyFile, "Labels", "italic", NULL);
		if (bLabelStyleItalic)
			pLabels->iconTextDescription.iStyle = PANGO_STYLE_ITALIC;
		else
			pLabels->iconTextDescription.iStyle = PANGO_STYLE_NORMAL;
		
		pango_font_description_set_size (fd, pLabels->iconTextDescription.iSize * PANGO_SCALE);
		pango_font_description_set_weight (fd, pLabels->iconTextDescription.iWeight);
		pango_font_description_set_style (fd, pLabels->iconTextDescription.iStyle);
		
		g_free (cFontDescription);
		cFontDescription = pango_font_description_to_string (fd);
		g_key_file_set_string (pKeyFile, "Labels", "police", cFontDescription);
		bFlushConfFileNeeded = TRUE;
	}
	pango_font_description_free (fd);
	g_free (cFontDescription);
	
	//\___________________ labels text color
	pLabels->iconTextDescription.bOutlined = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "text oulined", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	double couleur_label[3] = {1., 1., 1.};
	cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text color start", &bFlushConfFileNeeded, pLabels->iconTextDescription.fColorStart, 3, couleur_label, "Icons", NULL);
	
	cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text color stop", &bFlushConfFileNeeded, pLabels->iconTextDescription.fColorStop, 3, couleur_label, "Icons", NULL);
	
	pLabels->iconTextDescription.bVerticalPattern = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "vertical label pattern", &bFlushConfFileNeeded, TRUE, "Icons", NULL);

	double couleur_backlabel[4] = {0., 0., 0., 0.5};
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
	
	pLabels->iconTextDescription.iMargin = cairo_dock_get_integer_key_value (pKeyFile, "Labels", "text margin", &bFlushConfFileNeeded, 4, NULL, NULL);
	
	//\___________________ quick-info
	memcpy (&pLabels->quickInfoTextDescription, &pLabels->iconTextDescription, sizeof (CairoDockLabelDescription));
	pLabels->quickInfoTextDescription.cFont = g_strdup (pLabels->iconTextDescription.cFont);
	pLabels->quickInfoTextDescription.iSize = 12;
	pLabels->quickInfoTextDescription.iWeight = PANGO_WEIGHT_HEAVY;
	pLabels->quickInfoTextDescription.iStyle = PANGO_STYLE_NORMAL;
	
	gboolean bQuickInfoSameLook = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "qi same", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	if (bQuickInfoSameLook)
	{
		memcpy (pLabels->quickInfoTextDescription.fBackgroundColor, pLabels->iconTextDescription.fBackgroundColor, 4 * sizeof (gdouble));
		memcpy (pLabels->quickInfoTextDescription.fColorStart, pLabels->iconTextDescription.fColorStart, 3 * sizeof (gdouble));
	}
	else
	{
		cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "qi bg color", &bFlushConfFileNeeded, pLabels->quickInfoTextDescription.fBackgroundColor, 4, couleur_backlabel, NULL, NULL);
		cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "qi text color", &bFlushConfFileNeeded, pLabels->quickInfoTextDescription.fColorStart, 3, couleur_label, NULL, NULL);
	}
	memcpy (pLabels->quickInfoTextDescription.fColorStop, pLabels->quickInfoTextDescription.fColorStart, 3 * sizeof (gdouble));
	
	pLabels->iLabelSize = (pLabels->iconTextDescription.iSize != 0 ?
		pLabels->iconTextDescription.iSize +
		(pLabels->iconTextDescription.bOutlined ? 2 : 0) +
		2 * pLabels->iconTextDescription.iMargin
		+ 6 : 0);
	
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
	{
		g_free (pLabels->iconTextDescription.cFont);
		pLabels->iconTextDescription.cFont = NULL;
		pLabels->iconTextDescription.iSize = 0;
	}
	pLabels->bLabelForPointedIconOnly = bLabelForPointedIconOnly;
	
	pLabels->fLabelAlphaThreshold = cairo_dock_get_double_key_value (pKeyFile, "Labels", "alpha threshold", &bFlushConfFileNeeded, 10., "System", NULL);
	pLabels->fLabelAlphaThreshold = (pLabels->fLabelAlphaThreshold + 10.) / 10.;  // [0;50] -> [1;6]
	
	pLabels->bTextAlwaysHorizontal = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "always horizontal", &bFlushConfFileNeeded, FALSE, "System", NULL);
	
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
	g_free (pLabels->iconTextDescription.cFont);
	g_free (pLabels->quickInfoTextDescription.cFont);
}


  ////////////
 /// LOAD ///
////////////

static void _cairo_dock_load_icons_background_surface (const gchar *cImagePath)
{
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	
	int iSize = myIconsParam.iIconWidth;
	if (iSize == 0)
		iSize = 48;
	
	double fMaxScale = cairo_dock_get_max_scale (g_pMainDock);
	iSize *= fMaxScale;
	
	cairo_dock_load_image_buffer (&g_pIconBackgroundBuffer,
		cImagePath,
		iSize,
		iSize,
		CAIRO_DOCK_FILL_SPACE);
}

static void _load_renderer (const gchar *cRenderername, CairoIconContainerRenderer *pRenderer, gpointer data)
{
	if (pRenderer && pRenderer->load)
		pRenderer->load ();
}
static void _cairo_dock_load_icon_textures (void)
{
	_cairo_dock_load_icons_background_surface (myIconsParam.cBackgroundImagePath);
	
	cairo_dock_foreach_icon_container_renderer ((GHFunc)_load_renderer, NULL);
}
static void _reload_in_desklet (CairoDesklet *pDesklet, gpointer data)
{
	if (CAIRO_DOCK_IS_APPLET (pDesklet->pIcon))
	{
		cairo_dock_reload_module_instance (pDesklet->pIcon->pModuleInstance, FALSE);
	}
}
static void _on_icon_theme_changed (GtkIconTheme *pIconTheme, gpointer data)
{
	cd_message ("theme has changed");
	cairo_dock_foreach_desklet ((CairoDockForeachDeskletFunc) _reload_in_desklet, NULL);
	cairo_dock_reload_buffers_in_all_docks ();
}
static void _cairo_dock_load_icon_theme (void)
{
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
	_cairo_dock_load_icon_textures ();
	
	cairo_dock_create_icon_fbo ();
	
	_cairo_dock_load_icon_theme ();
}


  //////////////
 /// RELOAD ///
//////////////

static void _remove_separators (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_remove_automatic_separators (pDock);
}
static void _insert_separators (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)  // les separateurs utilisateurs ne sont pas recrees, on les recharge donc.
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))  // il n'y a que des separateurs utilisateurs dans le dock en ce moment.
		{
			cairo_dock_load_icon_image (icon, CAIRO_CONTAINER (pDock));
		}
	}
	cairo_dock_insert_automatic_separators_in_dock (pDock);
}
static void _calculate_icons (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_calculate_dock_icons (pDock);
}

static void _reload_one_label (Icon *pIcon, CairoContainer *pContainer, CairoIconsParam *pLabels)
{
	cairo_dock_load_icon_text (pIcon, &pLabels->iconTextDescription);
	///double fMaxScale = cairo_dock_get_max_scale (pContainer);
	double fMaxScale = (pIcon->fHeight != 0 ? (pContainer->bIsHorizontal ? pIcon->iImageHeight : pIcon->iImageWidth) / pIcon->fHeight : 1.);
	cairo_dock_load_icon_quickinfo (pIcon, &pLabels->quickInfoTextDescription, fMaxScale);
}
static void _cairo_dock_resize_one_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_update_dock_size (pDock);
}

static void reload (CairoIconsParam *pPrevIcons, CairoIconsParam *pIcons)
{
	gboolean bInsertSeparators = FALSE;
	
	if (cairo_dock_strings_differ (pPrevIcons->cSeparatorImage, pIcons->cSeparatorImage) ||
		pPrevIcons->iSeparatorWidth != pIcons->iSeparatorWidth ||
		pPrevIcons->iSeparatorHeight != pIcons->iSeparatorHeight ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude)
	{
		bInsertSeparators = TRUE;
		cairo_dock_foreach_docks ((GHFunc)_remove_separators, NULL);
	}
	
	gboolean bThemeChanged = cairo_dock_strings_differ (pIcons->cIconTheme, pPrevIcons->cIconTheme);
	if (bThemeChanged)
	{
		_cairo_dock_unload_icon_theme ();
		
		_cairo_dock_load_icon_theme ();
	}
	
	gboolean bIconBackgroundImagesChanged = FALSE;
	// if background images are different, reload them and trigger the reload of all icons
	if (cairo_dock_strings_differ (pPrevIcons->cBackgroundImagePath, pIcons->cBackgroundImagePath) ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude)
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
		cairo_dock_reload_buffers_in_all_docks ();
	}
	
	if (bInsertSeparators)
	{
		cairo_dock_foreach_docks ((GHFunc)_insert_separators, NULL);
	}
	
	if (pPrevIcons->iIconWidth != pIcons->iIconWidth ||
		pPrevIcons->iIconHeight != pIcons->iIconHeight ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude)
	{
		_cairo_dock_unload_icon_textures ();
		myIndicatorsMgr.mgr.unload ();
		_cairo_dock_load_icon_textures ();
		myIndicatorsMgr.mgr.load ();
	}
	
	cairo_dock_set_all_views_to_default (0);  // met a jour la taille (decorations incluses) de tous les docks; le chargement des separateurs plats se fait dans le calcul de max dock size.
	cairo_dock_foreach_docks ((GHFunc)_calculate_icons, NULL);
	cairo_dock_redraw_root_docks (FALSE);  // main dock inclus.
	
	// labels
	CairoIconsParam *pLabels = pIcons;
	CairoIconsParam *pPrevLabels = pPrevIcons;
	cairo_dock_foreach_icons ((CairoDockForeachIconFunc) _reload_one_label, pLabels);
	
	if (pPrevLabels->iLabelSize != pLabels->iLabelSize)
	{
		cairo_dock_foreach_docks ((GHFunc) _cairo_dock_resize_one_dock, NULL);
	}
}


  //////////////
 /// UNLOAD ///
//////////////

static void _unload_renderer (const gchar *cRenderername, CairoIconContainerRenderer *pRenderer, gpointer data)
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

static void init (void)
{
	cairo_dock_register_notification_on_object (&myDesktopMgr,
		NOTIFICATION_DESKTOP_CHANGED,
		(CairoDockNotificationFunc) _on_change_current_desktop_viewport_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myIconsMgr,
		NOTIFICATION_RENDER_ICON,
		(CairoDockNotificationFunc) cairo_dock_render_icon_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_icons_manager (void)
{
	// Manager
	memset (&myIconsMgr, 0, sizeof (CairoIconsManager));
	myIconsMgr.mgr.cModuleName 	= "Icons";
	myIconsMgr.mgr.init 		= init;
	myIconsMgr.mgr.load 		= load;
	myIconsMgr.mgr.unload 		= unload;
	myIconsMgr.mgr.reload 		= (GldiManagerReloadFunc)reload;
	myIconsMgr.mgr.get_config 	= (GldiManagerGetConfigFunc)get_config;
	myIconsMgr.mgr.reset_config = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myIconsParam, 0, sizeof (CairoIconsParam));
	myIconsMgr.mgr.pConfig = (GldiManagerConfigPtr)&myIconsParam;
	myIconsMgr.mgr.iSizeOfConfig = sizeof (CairoIconsParam);
	// data
	memset (&g_pIconBackgroundBuffer, 0, sizeof (CairoDockImageBuffer));
	myIconsMgr.mgr.pData = (GldiManagerDataPtr)NULL;
	myIconsMgr.mgr.iSizeOfData = 0;
	// signals
	cairo_dock_install_notifications_on_object (&myIconsMgr, NB_NOTIFICATIONS_ICON);
	// register
	gldi_register_manager (GLDI_MANAGER(&myIconsMgr));
}

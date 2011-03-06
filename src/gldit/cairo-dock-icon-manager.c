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

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include <gtk/gtkgl.h>

#include "gldi-config.h"
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

static void _cairo_dock_unload_icon_textures (void);


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
	if (icon->cBaseURI != NULL)
		cairo_dock_fm_remove_monitor_full (icon->cBaseURI, (icon->pSubDock != NULL), (icon->iVolumeID != 0 ? icon->cCommand : NULL));
	if (CAIRO_DOCK_IS_NORMAL_APPLI (icon))
		cairo_dock_unregister_appli (icon);
	else if (icon->cClass != NULL)  // c'est un inhibiteur.
		cairo_dock_deinhibite_class (icon->cClass, icon);
	if (icon->pModuleInstance != NULL)
		cairo_dock_deinstanciate_module (icon->pModuleInstance);
	cairo_dock_notify_on_object (&myIconsMgr, NOTIFICATION_STOP_ICON, icon);
	cairo_dock_notify_on_object (icon, NOTIFICATION_STOP_ICON, icon);
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

static CairoDock *_cairo_dock_insert_floating_icon_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bUpdateSize, gboolean bAnimate)
{
	cd_message ("%s (%s)", __func__, icon->cName);
	g_return_val_if_fail (pMainDock != NULL, NULL);
	
	//\_________________ On determine dans quel dock l'inserer.
	CairoDock *pParentDock = pMainDock;
	g_return_val_if_fail (pParentDock != NULL, NULL);

	//\_________________ On l'insere dans son dock parent en animant ce dernier eventuellement.
	cairo_dock_insert_icon_in_dock (icon, pParentDock, bUpdateSize, bAnimate);
	cd_message (" insertion de %s complete (%.2f %.2fx%.2f) dans %s", icon->cName, icon->fInsertRemoveFactor, icon->fWidth, icon->fHeight, icon->cParentDockName);

	if (bAnimate && cairo_dock_animation_will_be_visible (pParentDock))
	{
		cairo_dock_launch_animation (CAIRO_CONTAINER (pParentDock));
	}
	else
	{
		icon->fInsertRemoveFactor = 0;
		icon->fScale = 1.;
	}
	
	cairo_dock_reserve_one_icon_geometry_for_window_manager (&icon->Xid, icon, pMainDock);

	return pParentDock;
}
static CairoDock * _cairo_dock_detach_launcher(Icon *pIcon)
{
	cd_debug ("%s (%s, parent dock=%s)", __func__, pIcon->cName, pIcon->cParentDockName);
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pParentDock == NULL)
		return NULL;

	gchar *cParentDockName = g_strdup(pIcon->cParentDockName);
	cairo_dock_detach_icon_from_dock (pIcon, pParentDock, TRUE); // this will set cParentDockName to NULL
	
	pIcon->cParentDockName = cParentDockName; // put it back !

	cairo_dock_update_dock_size (pParentDock);
	return pParentDock;
}
static void _cairo_dock_hide_show_launchers_on_other_desktops (Icon *icon, CairoContainer *pContainer, CairoDock *pMainDock)
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
				pParentDock = _cairo_dock_insert_floating_icon_in_dock (icon, pMainDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
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
	// first we detach what shouldn't be show on this viewport
	cairo_dock_foreach_icons_of_type (pDock->icons, CAIRO_DOCK_LAUNCHER, (CairoDockForeachIconFunc)_cairo_dock_hide_show_launchers_on_other_desktops, pDock);
	// then we reattach what was eventually missing
	cairo_dock_foreach_icons_of_type (s_pFloatingIconsList, CAIRO_DOCK_LAUNCHER, (CairoDockForeachIconFunc)_cairo_dock_hide_show_launchers_on_other_desktops, pDock);
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
 /// GET CONFIG ///
//////////////////

static const gchar * s_cIconTypeNames[(CAIRO_DOCK_NB_GROUPS+1)/2] = {"launchers", "applications", "applets"};

static gboolean get_config (GKeyFile *pKeyFile, CairoIconsParam *pIcons)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	//\___________________ Ordre des icones.
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_GROUPS; i ++)
		pIcons->tIconTypeOrder[i] = i;
	gsize length=0;
	
	int iSeparateIcons = 0;
	if (! g_key_file_has_key (pKeyFile, "Icons", "separate_icons", NULL))  // old parameters.
	{
		if (!g_key_file_get_boolean (pKeyFile, "Icons", "mix applets with launchers", NULL))
		{
			if (!g_key_file_get_boolean (pKeyFile, "Icons", "mix applis with launchers", NULL))
				iSeparateIcons = 3;
			else
				iSeparateIcons = 2;
		}
		else if (!g_key_file_get_boolean (pKeyFile, "Icons", "mix applis with launchers", NULL))
			iSeparateIcons = 1;
	}
	pIcons->iSeparateIcons = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "separate_icons", &bFlushConfFileNeeded, iSeparateIcons , NULL, NULL);
	
	cairo_dock_get_integer_list_key_value (pKeyFile, "Icons", "icon's type order", &bFlushConfFileNeeded, pIcons->iIconsTypesList, 3, NULL, "Cairo Dock", NULL);  // on le recupere meme si on ne separe pas les icones, pour le panneau de conf simple.
	if (pIcons->iIconsTypesList[0] == 0 && pIcons->iIconsTypesList[1] == 0)  // old format.
	{
		cd_debug ("icon's type order : old format\n");
		gchar **cIconsTypesList = cairo_dock_get_string_list_key_value (pKeyFile, "Icons", "icon's type order", &bFlushConfFileNeeded, &length, NULL, "Cairo Dock", NULL);
		
		if (cIconsTypesList != NULL && length > 0)
		{
			cd_debug (" conversion ...\n");
			unsigned int i, j;
			for (i = 0; i < length; i ++)
			{
				cd_debug (" %d) %s\n", i, cIconsTypesList[i]);
				for (j = 0; j < ((CAIRO_DOCK_NB_GROUPS + 1) / 2); j ++)
				{
					if (strcmp (cIconsTypesList[i], s_cIconTypeNames[j]) == 0)
					{
						cd_debug ("   => %d\n", j);
						pIcons->tIconTypeOrder[2*j] = 2 * i;
					}
				}
			}
		}
		g_strfreev (cIconsTypesList);
		
		pIcons->iIconsTypesList[0] = pIcons->tIconTypeOrder[2*0]/2;
		pIcons->iIconsTypesList[1] = pIcons->tIconTypeOrder[2*1]/2;
		pIcons->iIconsTypesList[2] = pIcons->tIconTypeOrder[2*2]/2;
		cd_debug ("mise a jour avec {%d;%d;%d}\n", pIcons->iIconsTypesList[0], pIcons->iIconsTypesList[1], pIcons->iIconsTypesList[2]);
		g_key_file_set_integer_list (pKeyFile, "Icons", "icon's type order", pIcons->iIconsTypesList, 3);
		bFlushConfFileNeeded = TRUE;
	}
	
	for (i = 0; i < 3; i ++)
		pIcons->tIconTypeOrder[2*pIcons->iIconsTypesList[i]] = 2*i;
	if (pIcons->iSeparateIcons == 0)
	{
		for (i = 0; i < 3; i ++)
			pIcons->tIconTypeOrder[2*i] = 0;
	}
	else if (pIcons->iSeparateIcons == 1)
	{
		pIcons->tIconTypeOrder[CAIRO_DOCK_APPLET] = pIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER];
	}
	else if (pIcons->iSeparateIcons == 2)
	{
		pIcons->tIconTypeOrder[CAIRO_DOCK_APPLI] = pIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER];
	}
	
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
	pIcons->pDefaultIconDirectory = g_new0 (gpointer, 2 * 4);  // theme d'icone + theme local + theme default + NULL final.
	int j = 0;
	gboolean bLocalIconsUsed = FALSE, bDefaultThemeUsed = FALSE;
	
	if (pIcons->cIconTheme == NULL || *pIcons->cIconTheme == '\0')  // theme systeme.
	{
		j ++;
		bDefaultThemeUsed = TRUE;
	}
	else if (pIcons->cIconTheme[0] == '~')
	{
		pIcons->pDefaultIconDirectory[2*j] = g_strdup_printf ("%s%s", getenv ("HOME"), pIcons->cIconTheme+1);
		j ++;
	}
	else if (pIcons->cIconTheme[0] == '/')
	{
		pIcons->pDefaultIconDirectory[2*j] = g_strdup (pIcons->cIconTheme);
		j ++;
	}
	else if (strncmp (pIcons->cIconTheme, "_LocalTheme_", 12) == 0 || strncmp (pIcons->cIconTheme, "_ThemeDirectory_", 16) == 0 || strncmp (pIcons->cIconTheme, "_Custom Icons_", 14) == 0)
	{
		pIcons->pDefaultIconDirectory[2*j] = g_strdup (g_cCurrentIconsPath);
		j ++;
		bLocalIconsUsed = TRUE;
	}
	else
	{
		pIcons->pDefaultIconDirectory[2*j+1] = gtk_icon_theme_new ();
		gtk_icon_theme_set_custom_theme (pIcons->pDefaultIconDirectory[2*j+1], pIcons->cIconTheme);
		j ++;
	}
	
	if (! bLocalIconsUsed)
	{
		pIcons->pDefaultIconDirectory[2*j] = g_strdup (g_cCurrentIconsPath);
		j ++;
	}
	if (! bDefaultThemeUsed)
	{
		j ++;
	}
	pIcons->iNbIconPlaces = j;
	
	gchar *cLauncherBackgroundImageName = cairo_dock_get_string_key_value (pKeyFile, "Icons", "icons bg", &bFlushConfFileNeeded, NULL, NULL, NULL);
	if (cLauncherBackgroundImageName != NULL)
	{
		pIcons->cBackgroundImagePath = cairo_dock_search_image_s_path (cLauncherBackgroundImageName);
		g_free (cLauncherBackgroundImageName);
	}
		
	//\___________________ Parametres des lanceurs.
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "launcher ", bFlushConfFileNeeded, pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER]);
	if (pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] == 0)
		pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] = 48;
	if (pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] == 0)
		pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] = 48;
	
	//\___________________ Parametres des applis.
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "appli ", bFlushConfFileNeeded, pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI], pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI]);
	if (pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] == 0)
		pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] = pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
	if (pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI] == 0)
		pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] = pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER];
	
	//\___________________ Parametres des applets.
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "applet ", bFlushConfFileNeeded, pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET], pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET]);
	if (pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] == 0)
		pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] = pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
	if (pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] == 0)
		pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] = pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER];
	
	//\___________________ Parametres des separateurs.
	cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "separator ", bFlushConfFileNeeded, pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12], pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12]);
	if (pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] == 0)
		pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] = pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
	if (pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] == 0)
		pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] = pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER];
	pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] = MIN (pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12], pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER]);
	
	pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR23] = pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12];
	pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR23] = pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12];
	
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
	
	
	pIcons->fReflectSize = 0;
	for (i = 0; i < CAIRO_DOCK_NB_GROUPS; i ++)
	{
		if (pIcons->tIconAuthorizedHeight[i] > 0)
			pIcons->fReflectSize = MAX (pIcons->fReflectSize, pIcons->tIconAuthorizedHeight[i]);
	}
	if (pIcons->fReflectSize == 0)  // on n'a pas trouve de hauteur, on va essayer avec la largeur.
	{
		for (i = 0; i < CAIRO_DOCK_NB_GROUPS; i ++)
		{
			if (pIcons->tIconAuthorizedWidth[i] > 0)
				pIcons->fReflectSize = MAX (pIcons->fReflectSize, pIcons->tIconAuthorizedWidth[i]);
		}
		if (pIcons->fReflectSize > 0)
			pIcons->fReflectSize = MIN (48, pIcons->fReflectSize);
		else
			pIcons->fReflectSize = 48;
	}
	pIcons->fReflectSize *= pIcons->fReflectHeightRatio;
	
	
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
		2 * pLabels->iconTextDescription.iMargin : 0);
	
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
	if (pIcons->pDefaultIconDirectory != NULL)
	{
		gpointer data;
		int i;
		for (i = 0; i < pIcons->iNbIconPlaces; i ++)
		{
			if (pIcons->pDefaultIconDirectory[2*i] != NULL)
				g_free (pIcons->pDefaultIconDirectory[2*i]);
			else if (pIcons->pDefaultIconDirectory[2*i+1] != NULL)
				g_object_unref (pIcons->pDefaultIconDirectory[2*i+1]);
		}
		g_free (pIcons->pDefaultIconDirectory);
	}
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

static void _cairo_dock_load_icons_background_surface (const gchar *cImagePath, double fMaxScale)
{
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	
	int iSize = MAX (myIconsParam.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], myIconsParam.tIconAuthorizedWidth[CAIRO_DOCK_APPLI]);
	if (iSize == 0)
		iSize = 48;
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
	double fMaxScale = cairo_dock_get_max_scale (g_pMainDock);
	
	_cairo_dock_load_icons_background_surface (myIconsParam.cBackgroundImagePath, fMaxScale);
	
	cairo_dock_foreach_icon_container_renderer ((GHFunc)_load_renderer, NULL);
}

static void load (void)
{
	_cairo_dock_load_icon_textures ();
	
	cairo_dock_create_icon_fbo ();
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
	cairo_dock_insert_separators_in_dock (pDock);
}
static void _calculate_icons (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_calculate_dock_icons (pDock);
}
static void _reorder_icons (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_remove_automatic_separators (pDock);

	if (GPOINTER_TO_INT (data) && pDock->bIsMainDock)
	{
		cairo_dock_reorder_classes ();  // on re-ordonne les applis a cote des lanceurs/applets.
	}
	pDock->icons = g_list_sort (pDock->icons, (GCompareFunc) cairo_dock_compare_icons_order);
}

static void _reload_one_label (Icon *pIcon, CairoContainer *pContainer, CairoIconsParam *pLabels)
{
	cairo_dock_load_icon_text (pIcon, &pLabels->iconTextDescription);
	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	cairo_dock_load_icon_quickinfo (pIcon, &pLabels->quickInfoTextDescription, fMaxScale);
}
static void _cairo_dock_resize_one_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_update_dock_size (pDock);
}

static void reload (CairoIconsParam *pPrevIcons, CairoIconsParam *pIcons)
{
	double fMaxScale = cairo_dock_get_max_scale (g_pMainDock);
	gboolean bInsertSeparators = FALSE;
	
	gboolean bGroupOrderChanged;
	if (pPrevIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER] != pIcons->tIconTypeOrder[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconTypeOrder[CAIRO_DOCK_APPLI] != pIcons->tIconTypeOrder[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconTypeOrder[CAIRO_DOCK_APPLET] != pIcons->tIconTypeOrder[CAIRO_DOCK_APPLET] ||
		pPrevIcons->iSeparateIcons != pIcons->iSeparateIcons)
		bGroupOrderChanged = TRUE;
	else
		bGroupOrderChanged = FALSE;
	
	if (bGroupOrderChanged)
	{
		bInsertSeparators = TRUE;  // on enleve les separateurs avant de re-ordonner.
		cairo_dock_foreach_docks ((GHFunc)_reorder_icons, GINT_TO_POINTER (pPrevIcons->iSeparateIcons && ! pIcons->iSeparateIcons));
	}
	
	if ((pPrevIcons->iSeparateIcons && ! pIcons->iSeparateIcons) ||
		cairo_dock_strings_differ (pPrevIcons->cSeparatorImage, pIcons->cSeparatorImage) ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude)
	{
		bInsertSeparators = TRUE;
		cairo_dock_foreach_docks ((GHFunc)_remove_separators, NULL);
	}
	
	gboolean bThemeChanged = cairo_dock_strings_differ (pIcons->cIconTheme, pPrevIcons->cIconTheme);
	
	gboolean bIconBackgroundImagesChanged = FALSE;
	// if background images are different, reload them and trigger the reload of all icons
	if (cairo_dock_strings_differ (pPrevIcons->cBackgroundImagePath, pIcons->cBackgroundImagePath) ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude)
	{
		bIconBackgroundImagesChanged = TRUE;
		_cairo_dock_load_icons_background_surface (pIcons->cBackgroundImagePath, fMaxScale);
	}
	
	///cairo_dock_create_icon_pbuffer ();
	cairo_dock_destroy_icon_fbo ();
	cairo_dock_create_icon_fbo ();
	
	if (pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLI] ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_APPLET] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_APPLET] ||
		pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] ||
		pPrevIcons->fAmplitude != pIcons->fAmplitude ||
		(!g_bUseOpenGL && pPrevIcons->fReflectHeightRatio != pIcons->fReflectHeightRatio) ||
		(!g_bUseOpenGL && pPrevIcons->fAlbedo != pIcons->fAlbedo) ||
		bThemeChanged ||
		bIconBackgroundImagesChanged)  // oui on ne fait pas dans la finesse.
	{
		cairo_dock_reload_buffers_in_all_docks (TRUE);  // TRUE <=> y compris les applets.
	}
	
	if (bInsertSeparators)
	{
		cairo_dock_foreach_docks ((GHFunc)_insert_separators, NULL);
	}
	
	if (pPrevIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] ||
		pPrevIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != pIcons->tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] ||
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
	if (pPrevLabels->bTextAlwaysHorizontal != pLabels->bTextAlwaysHorizontal)
	{
		cairo_dock_reload_buffers_in_all_docks (TRUE);  // les modules aussi.
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

static void unload (void)
{
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	
	cairo_dock_foreach_icon_container_renderer ((GHFunc)_unload_renderer, NULL);
	
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
	myIconsMgr.mgr.pConfig = (GldiManagerConfigPtr*)&myIconsParam;
	myIconsMgr.mgr.iSizeOfConfig = sizeof (CairoIconsParam);
	// data
	memset (&g_pIconBackgroundBuffer, 0, sizeof (CairoDockImageBuffer));
	myIconsMgr.mgr.pData = (GldiManagerDataPtr*)NULL;
	myIconsMgr.mgr.iSizeOfData = 0;
	// signals
	cairo_dock_install_notifications_on_object (&myIconsMgr, NB_NOTIFICATIONS_ICON);
	// register
	gldi_register_manager (GLDI_MANAGER(&myIconsMgr));
}

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
#include <cairo.h>

#include "cairo-dock-image-buffer.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-utils.h"  // cairo_dock_launch_command
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-module-instance-manager.h"  // GldiModuleInstance
#include "cairo-dock-module-manager.h"  // GldiModuleInstance
#include "cairo-dock-log.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-container.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-applet-facility.h"

extern gchar *g_cExtrasDirPath;

extern gboolean g_bUseOpenGL;


void cairo_dock_set_icon_surface_full (cairo_t *pIconContext, cairo_surface_t *pSurface, double fScale, double fAlpha, Icon *pIcon)
{
	//\________________ On efface l'ancienne image.
	if (! cairo_dock_begin_draw_icon_cairo (pIcon, 0, pIconContext))  // 0 <=> erase
		return;
	
	//\________________ On applique la nouvelle image.
	if (pSurface != NULL && fScale > 0)
	{
		cairo_save (pIconContext);
		if (fScale != 1 && pIcon != NULL)
		{
			int iWidth, iHeight;
			cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
			cairo_translate (pIconContext, .5 * iWidth * (1 - fScale) , .5 * iHeight * (1 - fScale));
			cairo_scale (pIconContext, fScale, fScale);
		}
		
		cairo_set_source_surface (
			pIconContext,
			pSurface,
			0.,
			0.);
		
		if (fAlpha != 1)
			cairo_paint_with_alpha (pIconContext, fAlpha);
		else
			cairo_paint (pIconContext);
		cairo_restore (pIconContext);
	}
	cairo_dock_end_draw_icon_cairo (pIcon);
	
	if (pIcon->pContainer) gtk_widget_queue_draw (pIcon->pContainer->pWidget);
}


/// TODO: don't use 'cairo_dock_set_icon_surface' here...
gboolean cairo_dock_set_image_on_icon (cairo_t *pIconContext, const gchar *cIconName, Icon *pIcon, G_GNUC_UNUSED GldiContainer *pContainer)
{
	//g_print ("%s (%s)\n", __func__, cIconName);
	// load the image in a surface.
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
	if (iWidth <= 0 || iHeight <= 0) return FALSE;
	
	cairo_surface_t *pImageSurface = cairo_dock_create_surface_from_icon (cIconName,
		iWidth,
		iHeight);
	
	// check that it's ok.
	if (pImageSurface == NULL)  // let the icon in its current state.
		return FALSE;
	
	// set the new image on the icon
	cairo_dock_set_icon_surface (pIconContext, pImageSurface, pIcon);
	
	cairo_surface_destroy (pImageSurface);
	
	if (cIconName != pIcon->cFileName)
	{
		g_free (pIcon->cFileName);
		pIcon->cFileName = g_strdup (cIconName);
	}
	return TRUE;
}

void cairo_dock_set_image_on_icon_with_default (cairo_t *pIconContext, const gchar *cIconName, Icon *pIcon, GldiContainer *pContainer, const gchar *cDefaultImagePath)
{
	if (! cIconName || ! cairo_dock_set_image_on_icon (pIconContext, cIconName, pIcon, pContainer))
		cairo_dock_set_image_on_icon (pIconContext, cDefaultImagePath, pIcon, pContainer);
}


void cairo_dock_set_hours_minutes_as_quick_info (Icon *pIcon, int iTimeInSeconds)
{
	int hours = iTimeInSeconds / 3600;
	int minutes = (iTimeInSeconds % 3600) / 60;
	if (hours != 0)
		gldi_icon_set_quick_info_printf (pIcon, "%dh%02d", hours, abs (minutes));
	else
		gldi_icon_set_quick_info_printf (pIcon, "%dmn", minutes);
}

void cairo_dock_set_minutes_secondes_as_quick_info (Icon *pIcon, int iTimeInSeconds)
{
	int minutes = iTimeInSeconds / 60;
	int secondes = iTimeInSeconds % 60;
	//cd_debug ("%s (%d:%d)", __func__, minutes, secondes);
	if (minutes != 0)
		gldi_icon_set_quick_info_printf (pIcon, "%d:%02d", minutes, abs (secondes));
	else
		gldi_icon_set_quick_info_printf (pIcon, "%s0:%02d", (secondes < 0 ? "-" : ""), abs (secondes));
}

gchar *cairo_dock_get_human_readable_size (long long int iSizeInBytes)
{
	double fValue;
	if (iSizeInBytes >> 10 == 0)
	{
		return g_strdup_printf ("%dB", (int) iSizeInBytes);
	}
	else if (iSizeInBytes >> 20 == 0)
	{
		fValue = (double) (iSizeInBytes) / 1024.;
		return g_strdup_printf (fValue < 10 ? "%.1fK" : "%.0fK", fValue);
	}
	else if (iSizeInBytes >> 30 == 0)
	{
		fValue = (double) (iSizeInBytes >> 10) / 1024.;
		return g_strdup_printf (fValue < 10 ? "%.1fM" : "%.0fM", fValue);
	}
	else if (iSizeInBytes >> 40 == 0)
	{
		fValue = (double) (iSizeInBytes >> 20) / 1024.;
		return g_strdup_printf (fValue < 10 ? "%.1fG" : "%.0fG", fValue);
	}
	else
	{
		fValue = (double) (iSizeInBytes >> 30) / 1024.;
		return g_strdup_printf (fValue < 10 ? "%.1fT" : "%.0fT", fValue);
	}
}

void cairo_dock_set_size_as_quick_info (Icon *pIcon, long long int iSizeInBytes)
{
	gchar *cSize = cairo_dock_get_human_readable_size (iSizeInBytes);
	gldi_icon_set_quick_info (pIcon, cSize);
	g_free (cSize);
}



gchar *cairo_dock_get_theme_path_for_module (const gchar *cAppletConfFilePath, GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultThemeName, const gchar *cShareThemesDir, const gchar *cExtraDirName)
{
	gchar *cThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	
	gchar *cUserThemesDir = (cExtraDirName != NULL ? g_strdup_printf ("%s/%s", g_cExtrasDirPath, cExtraDirName) : NULL);
	CairoDockPackageType iType = cairo_dock_extract_package_type_from_name (cThemeName);
	gchar *cThemePath = cairo_dock_get_package_path (cThemeName, cShareThemesDir, cUserThemesDir, cExtraDirName, iType);
	
	if (iType != CAIRO_DOCK_ANY_PACKAGE)
	{
		g_key_file_set_string (pKeyFile, cGroupName, cKeyName, cThemeName);
		cairo_dock_write_keys_to_file (pKeyFile, cAppletConfFilePath);
	}
	g_free (cThemeName);
	g_free (cUserThemesDir);
	return cThemePath;
}


//Utile pour jouer des fichiers son depuis le dock.
//A utiliser avec l'Objet UI 'u' dans les .conf
void cairo_dock_play_sound (const gchar *cSoundPath)
{
	cd_debug ("%s (%s)", __func__, cSoundPath);
	if (cSoundPath == NULL)
	{
		cd_warning ("No sound to play, skip.");
		return;
	}
	
	const gchar *args[] = {NULL, NULL, NULL, NULL};
	if (g_file_test ("/usr/bin/paplay", G_FILE_TEST_EXISTS))
	{
		args[0] = "/usr/bin/paplay";
		args[1] = "--client-name=cairo-dock";
		args[2] = cSoundPath;
	}
	else if (g_file_test ("/usr/bin/aplay", G_FILE_TEST_EXISTS))
	{
		args[0] = "/usr/bin/aplay";
		args[1] = cSoundPath;
	}
	else if (g_file_test ("/usr/bin/play", G_FILE_TEST_EXISTS))
	{
		args[0] = "/usr/bin/play";
		args[1] = cSoundPath;
	}
	
	if (args[0]) cairo_dock_launch_command_argv (args);
}

// should be in gnome-integration if needed...
/*void cairo_dock_get_gnome_version (int *iMajor, int *iMinor, int *iMicro) {
	gchar *cContent = NULL;
	gsize length = 0;
	GError *erreur = NULL;
	g_file_get_contents ("/usr/share/gnome-about/gnome-version.xml", &cContent, &length, &erreur);
	
	if (erreur != NULL) {
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
		*iMajor = 0;
		*iMinor = 0;
		*iMicro = 0;
		return;
	}
	
	gchar **cLineList = g_strsplit (cContent, "\n", -1);
	gchar *cOneLine = NULL, *cMajor = NULL, *cMinor = NULL, *cMicro = NULL;
	int i, iMaj = 0, iMin = 0, iMic = 0;
	for (i = 0; cLineList[i] != NULL; i ++) {
		cOneLine = cLineList[i];
		if (*cOneLine == '\0')
			continue;
		
		//Seeking for Major
		cMajor = g_strstr_len (cOneLine, -1, "<platform>");  //<platform>2</platform>
		if (cMajor != NULL) {
			cMajor += 10; //On saute <platform>
			gchar *str = strchr (cMajor, '<');
			if (str != NULL)
				*str = '\0'; //On bloque a </platform>
			iMaj = atoi (cMajor);
		}
		else { //Gutsy xml's doesn't have <platform> but <major>
			cMajor = g_strstr_len (cOneLine, -1, "<major>");  //<major>2</major>
			if (cMajor != NULL) {
				cMajor += 7; //On saute <major>
				gchar *str = strchr (cMajor, '<');
				if (str != NULL)
					*str = '\0'; //On bloque a </major>
				iMaj = atoi (cMajor);
			}
		}
		
		//Seeking for Minor
		cMinor = g_strstr_len (cOneLine, -1, "<minor>");  //<minor>22</minor>
		if (cMinor != NULL) {
			cMinor += 7; //On saute <minor>
			gchar *str = strchr (cMinor, '<');
			if (str != NULL)
				*str = '\0'; //On bloque a </minor>
			iMin = atoi (cMinor);
		}
		
		//Seeking for Micro
		cMicro = g_strstr_len (cOneLine, -1, "<micro>");  //<micro>3</micro>
		if (cMicro != NULL) {
			cMicro += 7; //On saute <micro>
			gchar *str = strchr (cMicro, '<');
			if (str != NULL)
				*str = '\0'; //On bloque a </micro>
			iMic = atoi (cMicro);
		}
		
		if (iMaj != 0 && iMin != 0 && iMic != 0)
			break; //On s'enfou du reste
	}
	
	cd_debug ("Gnome Version %d.%d.%d", iMaj, iMin, iMic);
	
	*iMajor = iMaj;
	*iMinor = iMin;
	*iMicro = iMic;
	
	g_free (cContent);
	g_strfreev (cLineList);
}*/

void cairo_dock_pop_up_about_applet (G_GNUC_UNUSED GtkMenuItem *menu_item, GldiModuleInstance *pModuleInstance)
{
	gldi_module_instance_popup_description (pModuleInstance);
}


void cairo_dock_open_module_config_on_demand (int iClickedButton, G_GNUC_UNUSED GtkWidget *pInteractiveWidget, GldiModuleInstance *pModuleInstance, G_GNUC_UNUSED CairoDialog *pDialog)
{
	if (iClickedButton == 0 || iClickedButton == -1)  // bouton OK ou touche Entree.
	{
		cairo_dock_show_module_instance_gui (pModuleInstance, -1);
	}
}


void cairo_dock_insert_icons_in_applet (GldiModuleInstance *pInstance, GList *pIconsList, const gchar *cDockRenderer, const gchar *cDeskletRenderer, gpointer pDeskletRendererData)
{
	Icon *pIcon = pInstance->pIcon;
	g_return_if_fail (pIcon != NULL);
	
	GldiContainer *pContainer = pInstance->pContainer;
	g_return_if_fail (pContainer != NULL);
	
	if (pInstance->pDock)
	{
		if (pIcon->pSubDock == NULL)
		{
			if (pIcon->cName == NULL)
				gldi_icon_set_name (pIcon, pInstance->pModule->pVisitCard->cModuleName);
			if (cairo_dock_check_unique_subdock_name (pIcon))
				gldi_icon_set_name (pIcon, pIcon->cName);
			pIcon->pSubDock = gldi_subdock_new (pIcon->cName, cDockRenderer, pInstance->pDock, pIconsList);
			if (pIcon->pSubDock)
				pIcon->pSubDock->bPreventDraggingIcons = TRUE;  // par defaut pour toutes les applets on empeche de pouvoir deplacer/supprimer les icones a la souris.
		}
		else
		{
			Icon *pOneIcon;
			GList *ic;
			for (ic = pIconsList; ic != NULL; ic = ic->next)
			{
				pOneIcon = ic->data;
				gldi_icon_insert_in_container (pOneIcon, CAIRO_CONTAINER(pIcon->pSubDock), ! CAIRO_DOCK_ANIMATE_ICON);
			}
			g_list_free (pIconsList);
			
			cairo_dock_set_renderer (pIcon->pSubDock, cDockRenderer);
			cairo_dock_update_dock_size (pIcon->pSubDock);
		}
		
		if (pIcon->iSubdockViewType != 0)  // trigger the redraw after the icons are loaded.
			cairo_dock_trigger_redraw_subdock_content_on_icon (pIcon);
	}
	else if (pInstance->pDesklet)
	{
		Icon *pOneIcon;
		GList *ic;
		for (ic = pIconsList; ic != NULL; ic = ic->next)
		{
			pOneIcon = ic->data;
			cairo_dock_set_icon_container (pOneIcon, pInstance->pDesklet);
		}
		pInstance->pDesklet->icons = g_list_concat (pInstance->pDesklet->icons, pIconsList);  /// + sort icons and insert automatic separators...
		cairo_dock_set_desklet_renderer_by_name (pInstance->pDesklet, cDeskletRenderer, (CairoDeskletRendererConfigPtr) pDeskletRendererData);
		cairo_dock_redraw_container (pInstance->pContainer);
	}
}


void cairo_dock_insert_icon_in_applet (GldiModuleInstance *pInstance, Icon *pOneIcon)
{
	// get the container (create it if necessary)
	GldiContainer *pContainer = NULL;
	if (pInstance->pDock)
	{
		Icon *pIcon = pInstance->pIcon;
		if (pIcon->pSubDock == NULL)
		{
			if (pIcon->cName == NULL)
				gldi_icon_set_name (pIcon, pInstance->pModule->pVisitCard->cModuleName);
			if (cairo_dock_check_unique_subdock_name (pIcon))
				gldi_icon_set_name (pIcon, pIcon->cName);
			pIcon->pSubDock = gldi_subdock_new (pIcon->cName, NULL, pInstance->pDock, NULL);
			if (pIcon->pSubDock)
				pIcon->pSubDock->bPreventDraggingIcons = TRUE;  // par defaut pour toutes les applets on empeche de pouvoir deplacer/supprimer les icones a la souris.
		}
		pContainer = CAIRO_CONTAINER (pIcon->pSubDock);
	}
	else if (pInstance->pDesklet)
	{
		pContainer = CAIRO_CONTAINER (pInstance->pDesklet);
	}
	g_return_if_fail (pContainer != NULL);
	
	// insert the icon inside
	gldi_icon_insert_in_container (pOneIcon, pContainer, ! CAIRO_DOCK_ANIMATE_ICON);
}


void cairo_dock_remove_all_icons_from_applet (GldiModuleInstance *pInstance)
{
	Icon *pIcon = pInstance->pIcon;
	g_return_if_fail (pIcon != NULL);
	
	GldiContainer *pContainer = pInstance->pContainer;
	g_return_if_fail (pContainer != NULL);
	
	cd_debug ("%s (%s)", __func__, pInstance->pModule->pVisitCard->cModuleName);
	if (pInstance->pDesklet && pInstance->pDesklet->icons != NULL)
	{
		cd_debug (" destroy desklet icons");
		GList *icons = pInstance->pDesklet->icons;
		pInstance->pDesklet->icons = NULL;
		GList *ic;
		Icon *icon;
		for (ic = icons; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			cairo_dock_set_icon_container (icon, NULL);
			gldi_object_unref (GLDI_OBJECT(icon));
		}
		g_list_free (icons);
		cairo_dock_redraw_container (pInstance->pContainer);
	}
	if (pIcon->pSubDock != NULL)
	{
		if (pInstance->pDock)  // optimisation : on ne detruit pas le sous-dock.
		{
			cd_debug (" destroy sub-dock icons");
			GList *icons = pIcon->pSubDock->icons;
			pIcon->pSubDock->icons = NULL;
			GList *ic;
			Icon *icon;
			for (ic = icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				cairo_dock_set_icon_container (icon, NULL);
				gldi_object_unref (GLDI_OBJECT(icon));
			}
			g_list_free (icons);
		}
		else  // precaution pas chere
		{
			cd_debug (" destroy sub-dock");
			gldi_object_unref (GLDI_OBJECT(pIcon->pSubDock));
			pIcon->pSubDock = NULL;
		}
	}
}


void cairo_dock_resize_applet (GldiModuleInstance *pInstance, int w, int h)
{
	Icon *pIcon = pInstance->pIcon;
	g_return_if_fail (pIcon != NULL);
	
	GldiContainer *pContainer = pInstance->pContainer;
	g_return_if_fail (pContainer != NULL);
	
	if (pInstance->pDock)
	{
		cairo_dock_icon_set_requested_size (pIcon, w, h);
		cairo_dock_resize_icon_in_dock (pIcon, pInstance->pDock);
	}
	else  // in desklet mode, just resize the desklet, it will trigger the reload of the applet when the 'configure' event is received.
	{
		gtk_window_resize (GTK_WINDOW (pContainer->pWidget),
			w,
			h);  /// TODO: actually, the renderer should handle this, because except with the 'Simple' view, we can't know the actual size of the icon.
	}
}


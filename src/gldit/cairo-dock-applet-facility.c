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

#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-config.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-applet-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-applet-facility.h"

extern gchar *g_cExtrasDirPath;
extern CairoDockImageBuffer g_pIconBackgroundBuffer;

extern gboolean g_bUseOpenGL;


void cairo_dock_set_icon_surface_full (cairo_t *pIconContext, cairo_surface_t *pSurface, double fScale, double fAlpha, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (cairo_status (pIconContext) == CAIRO_STATUS_SUCCESS);
	
	//\________________ On efface l'ancienne image.
	cairo_dock_erase_cairo_context (pIconContext);
	
	//\________________ On met le background de l'icone si necessaire
	if (pIcon != NULL &&
		pIcon->pIconBuffer != NULL &&
		g_pIconBackgroundBuffer.pSurface != NULL &&
		(! CAIRO_DOCK_IS_SEPARATOR (pIcon)))
	{
		//cd_message (">>> %s prendra un fond d'icone", pIcon->cName);
		cairo_save (pIconContext);
		int iWidth, iHeight;
		cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
		cairo_scale(pIconContext,
			iWidth / g_pIconBackgroundBuffer.iWidth,
			iHeight / g_pIconBackgroundBuffer.iHeight);
		cairo_set_source_surface (pIconContext,
			g_pIconBackgroundBuffer.pSurface,
			0.,
			0.);
		cairo_set_operator (pIconContext, CAIRO_OPERATOR_DEST_OVER);
		cairo_paint (pIconContext);
		cairo_restore (pIconContext);
	}
	
	//\________________ On applique la nouvelle image.
	if (pSurface != NULL && fScale > 0)
	{
		cairo_save (pIconContext);
		if (fScale != 1 && pIcon != NULL)
		{
			int iWidth, iHeight;
			cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
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
	if (g_bUseOpenGL)
		cairo_dock_update_icon_texture (pIcon);
}


void cairo_dock_set_icon_surface_with_reflect (cairo_t *pIconContext, cairo_surface_t *pSurface, Icon *pIcon, CairoContainer *pContainer)
{
	cairo_dock_set_icon_surface_full (pIconContext, pSurface, 1., 1., pIcon, pContainer);
	
	cairo_dock_add_reflection_to_icon (pIcon, pContainer);
}

void cairo_dock_set_image_on_icon (cairo_t *pIconContext, const gchar *cImagePath, Icon *pIcon, CairoContainer *pContainer)
{
	if (cImagePath != pIcon->cFileName)
	{
		g_free (pIcon->cFileName);
		pIcon->cFileName = g_strdup (cImagePath);
	}
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
	cairo_surface_t *pImageSurface = cairo_dock_create_surface_for_icon (cImagePath,
		iWidth,
		iHeight);
	
	cairo_dock_set_icon_surface_with_reflect (pIconContext, pImageSurface, pIcon, pContainer);
	
	cairo_surface_destroy (pImageSurface);
}

void cairo_dock_set_icon_surface_with_bar (cairo_t *pIconContext, cairo_surface_t *pSurface, double fValue, Icon *pIcon)
{
	g_return_if_fail (cairo_status (pIconContext) == CAIRO_STATUS_SUCCESS);
	
	//\________________ On efface l'ancienne image.
	cairo_dock_erase_cairo_context (pIconContext);
	
	//\________________ On applique la nouvelle image.
	if (pSurface != NULL)
	{
		cairo_set_source_surface (
			pIconContext,
			pSurface,
			0.,
			0.);
		cairo_paint (pIconContext);
	}
	
	//\________________ On dessine la barre.
	cairo_dock_draw_bar_on_icon (pIconContext, fValue, pIcon);
	
	if (g_bUseOpenGL)
		cairo_dock_update_icon_texture (pIcon);
}

void cairo_dock_draw_bar_on_icon (cairo_t *pIconContext, double fValue, Icon *pIcon)
{
	int iWidth = pIcon->iImageWidth;
	int iHeight = pIcon->iImageHeight;
	
	cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0.,
		0.,
		iWidth,
		0.);  // de gauche a droite.
	g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);
	
	cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		(fValue >= 0 ? 0. : 1.),
		1.,
		0.,
		0.,
		1.);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		(fValue >= 0 ? 1. : 0.),
		0.,
		1.,
		0.,
		1.);
	
	cairo_save (pIconContext);
	cairo_set_operator (pIconContext, CAIRO_OPERATOR_OVER);
	
	cairo_set_line_width (pIconContext, 6);
	cairo_set_line_cap (pIconContext, CAIRO_LINE_CAP_ROUND);
	
	cairo_move_to (pIconContext, 3, iHeight - 3);
	cairo_rel_line_to (pIconContext, (iWidth - 6) * fabs (fValue), 0);
	
	cairo_set_source (pIconContext, pGradationPattern);
	cairo_stroke (pIconContext);
	
	cairo_pattern_destroy (pGradationPattern);
	cairo_restore (pIconContext);
}

void cairo_dock_set_hours_minutes_as_quick_info (Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds)
{
	int hours = iTimeInSeconds / 3600;
	int minutes = (iTimeInSeconds % 3600) / 60;
	if (hours != 0)
		cairo_dock_set_quick_info_printf (pIcon, pContainer, "%dh%02d", hours, abs (minutes));
	else
		cairo_dock_set_quick_info_printf (pIcon, pContainer, "%dmn", minutes);
}

void cairo_dock_set_minutes_secondes_as_quick_info (Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds)
{
	int minutes = iTimeInSeconds / 60;
	int secondes = iTimeInSeconds % 60;
	cd_debug ("%s (%d:%d)\n", __func__, minutes, secondes);
	if (minutes != 0)
		cairo_dock_set_quick_info_printf (pIcon, pContainer, "%d:%02d", minutes, abs (secondes));
	else
		cairo_dock_set_quick_info_printf (pIcon, pContainer, "%s0:%02d", (secondes < 0 ? "-" : ""), abs (secondes));
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

void cairo_dock_set_size_as_quick_info (Icon *pIcon, CairoContainer *pContainer, long long int iSizeInBytes)
{
	gchar *cSize = cairo_dock_get_human_readable_size (iSizeInBytes);
	cairo_dock_set_quick_info (pIcon, pContainer, cSize);
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


GtkWidget *cairo_dock_create_sub_menu (const gchar *cLabel, GtkWidget *pMenu, const gchar *cImage)
{
	GtkWidget *pMenuItem, *image, *pSubMenu = gtk_menu_new ();
	if (cImage == NULL)
	{
		pMenuItem = gtk_menu_item_new_with_label (cLabel);
	}
	else
	{
		pMenuItem = gtk_image_menu_item_new_with_label (cLabel);
		if (*cImage == '/')
		{
			GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cImage, 24, 24, NULL);
			image = gtk_image_new_from_pixbuf (pixbuf);
			g_object_unref (pixbuf);
		}
		else
		{
			image = gtk_image_new_from_stock (cImage, GTK_ICON_SIZE_MENU);
		}
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);
	}
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem); 
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenu);
	return pSubMenu; 
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
	
	GError *erreur = NULL;
	gchar *cSoundCommand = NULL;
	if (g_file_test ("/usr/bin/paplay", G_FILE_TEST_EXISTS))
		cSoundCommand = g_strdup_printf("paplay --client-name=cairo-dock \"%s\"", cSoundPath);

	else if (g_file_test ("/usr/bin/aplay", G_FILE_TEST_EXISTS))
		cSoundCommand = g_strdup_printf("aplay \"%s\"", cSoundPath);

	else if (g_file_test ("/usr/bin/play", G_FILE_TEST_EXISTS))
		cSoundCommand = g_strdup_printf("play \"%s\"", cSoundPath);
	
	
	cairo_dock_launch_command (cSoundCommand);
	
	g_free (cSoundCommand);
}

void cairo_dock_get_gnome_version (int *iMajor, int *iMinor, int *iMicro) {
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
}

void cairo_dock_pop_up_about_applet (GtkMenuItem *menu_item, CairoDockModuleInstance *pModuleInstance)
{
	cairo_dock_popup_module_instance_description (pModuleInstance);
}


void cairo_dock_open_module_config_on_demand (int iClickedButton, GtkWidget *pInteractiveWidget, CairoDockModuleInstance *pModuleInstance, CairoDialog *pDialog)
{
	if (iClickedButton == 0 || iClickedButton == -1)  // bouton OK ou touche Entree.
	{
		cairo_dock_show_module_instance_gui (pModuleInstance, -1);
	}
}


void cairo_dock_insert_icons_in_applet (CairoDockModuleInstance *pInstance, GList *pIconsList, const gchar *cDockRenderer, const gchar *cDeskletRenderer, gpointer pDeskletRendererData)
{
	Icon *pIcon = pInstance->pIcon;
	g_return_if_fail (pIcon != NULL);
	
	CairoContainer *pContainer = pInstance->pContainer;
	g_return_if_fail (pContainer != NULL);
	
	if (pInstance->pDock)
	{
		if (pIcon->pSubDock == NULL)
		{
			if (pIcon->cName == NULL)
				cairo_dock_set_icon_name (pInstance->pModule->pVisitCard->cModuleName, pIcon, pContainer);
			if (cairo_dock_check_unique_subdock_name (pIcon))
				cairo_dock_set_icon_name (pIcon->cName, pIcon, pContainer);
			pIcon->pSubDock = cairo_dock_create_subdock_from_scratch (pIconsList, pIcon->cName, pInstance->pDock);
			if (pIcon->pSubDock)
				pIcon->pSubDock->bPreventDraggingIcons = TRUE;  // par defaut pour toutes les applets on empeche de pouvoir deplacer/supprimer les icones a la souris.
			if (pIcon->iSubdockViewType != 0)
				cairo_dock_trigger_redraw_subdock_content_on_icon (pIcon);
		}
		else
		{
			Icon *pOneIcon;
			GList *ic;
			for (ic = pIconsList; ic != NULL; ic = ic->next)
			{
				pOneIcon = ic->data;
				cairo_dock_insert_icon_in_dock (pOneIcon, pIcon->pSubDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
				pOneIcon->cParentDockName = g_strdup (pIcon->cName);
				cairo_dock_trigger_load_icon_buffers (pOneIcon, CAIRO_CONTAINER (pIcon->pSubDock));
			}
			g_list_free (pIconsList);
		}
		cairo_dock_set_renderer (pIcon->pSubDock, cDockRenderer);
		cairo_dock_update_dock_size (pIcon->pSubDock);
	}
	else if (pInstance->pDesklet)
	{
		if (pIcon->pSubDock != NULL)  // precaution.
		{
			cairo_dock_destroy_dock (pIcon->pSubDock, pIcon->cName);
			pIcon->pSubDock = NULL;
		}
		pInstance->pDesklet->icons = g_list_concat (pInstance->pDesklet->icons, pIconsList);
		cairo_dock_set_desklet_renderer_by_name (pInstance->pDesklet, cDeskletRenderer, (CairoDeskletRendererConfigPtr) pDeskletRendererData);
		cairo_dock_redraw_container (pInstance->pContainer);
	}
}


void cairo_dock_insert_icon_in_applet (CairoDockModuleInstance *pInstance, Icon *pOneIcon)
{
	Icon *pIcon = pInstance->pIcon;
	g_return_if_fail (pIcon != NULL);
	
	CairoContainer *pContainer = pInstance->pContainer;
	g_return_if_fail (pContainer != NULL);
	
	if (pOneIcon == NULL)
		return;
	
	if (pInstance->pDock)
	{
		if (pIcon->pSubDock == NULL)
		{
			if (pIcon->cName == NULL)
				cairo_dock_set_icon_name (pInstance->pModule->pVisitCard->cModuleName, pIcon, pContainer);
			if (cairo_dock_check_unique_subdock_name (pIcon))
				cairo_dock_set_icon_name (pIcon->cName, pIcon, pContainer);
			pIcon->pSubDock = cairo_dock_create_subdock_from_scratch (NULL, pIcon->cName, pInstance->pDock);
			if (pIcon->pSubDock)
				pIcon->pSubDock->bPreventDraggingIcons = TRUE;  // par defaut pour toutes les applets on empeche de pouvoir deplacer/supprimer les icones a la souris.
		}
		if (pOneIcon->fOrder == CAIRO_DOCK_LAST_ORDER)
		{
			Icon *pLastIcon = cairo_dock_get_last_icon (pIcon->pSubDock->icons);
			pOneIcon->fOrder = (pLastIcon ? pLastIcon->fOrder + 1 : 0);
		}
		cairo_dock_trigger_load_icon_buffers (pOneIcon, CAIRO_CONTAINER (pIcon->pSubDock));
		cairo_dock_insert_icon_in_dock (pOneIcon, pIcon->pSubDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
		pOneIcon->cParentDockName = g_strdup (pIcon->cName);
	}
	else if (pInstance->pDesklet)
	{
		if (pIcon->pSubDock != NULL)  // precaution.
		{
			cairo_dock_destroy_dock (pIcon->pSubDock, pIcon->cName);
			pIcon->pSubDock = NULL;
		}
		
		if (pOneIcon->fOrder == CAIRO_DOCK_LAST_ORDER)
		{
			Icon *pLastIcon = cairo_dock_get_last_icon (pInstance->pDesklet->icons);
			pOneIcon->fOrder = (pLastIcon ? pLastIcon->fOrder + 1 : 0);
		}
		cairo_dock_insert_icon_in_desklet (pOneIcon, pInstance->pDesklet);
	}
}

gboolean cairo_dock_detach_icon_from_applet (CairoDockModuleInstance *pInstance, Icon *pOneIcon)
{
	Icon *pIcon = pInstance->pIcon;
	g_return_val_if_fail (pIcon != NULL, FALSE);
	
	CairoContainer *pContainer = pInstance->pContainer;
	g_return_val_if_fail (pContainer != NULL, FALSE);
	
	if (pOneIcon == NULL)
		return FALSE;
	
	gboolean bRemoved = FALSE;
	GList *pIconsList = (pInstance->pDock ? (pIcon->pSubDock ? pIcon->pSubDock->icons : NULL) : pInstance->pDesklet->icons);
	if (pInstance->pDock)
	{
		if (pIcon->pSubDock != NULL)
		{
			bRemoved = cairo_dock_detach_icon_from_dock (pOneIcon, pIcon->pSubDock, FALSE);
			cairo_dock_update_dock_size (pIcon->pSubDock);
		}
	}
	else if (pInstance->pDesklet)
	{
		bRemoved = cairo_dock_detach_icon_from_desklet (pOneIcon, pInstance->pDesklet);
	}
	return bRemoved;
}

gboolean cairo_dock_remove_icon_from_applet (CairoDockModuleInstance *pInstance, Icon *pOneIcon)
{
	gboolean r = cairo_dock_detach_icon_from_applet (pInstance, pOneIcon);
	cairo_dock_free_icon (pOneIcon);
	return r;
}

void cairo_dock_remove_all_icons_from_applet (CairoDockModuleInstance *pInstance)
{
	Icon *pIcon = pInstance->pIcon;
	g_return_if_fail (pIcon != NULL);
	
	CairoContainer *pContainer = pInstance->pContainer;
	g_return_if_fail (pContainer != NULL);
	
	cd_debug ("%s (%s)", __func__, pInstance->pModule->pVisitCard->cModuleName);
	if (pInstance->pDesklet && pInstance->pDesklet->icons != NULL)
	{
		cd_debug (" destroy desklet icons");
		g_list_foreach (pInstance->pDesklet->icons, (GFunc) cairo_dock_free_icon, NULL);
		g_list_free (pInstance->pDesklet->icons);
		pInstance->pDesklet->icons = NULL;
		cairo_dock_redraw_container (pInstance->pContainer);
	}
	if (pIcon->pSubDock != NULL)
	{
		if (pInstance->pDock)  // optimisation : on ne detruit pas le sous-dock.
		{
			cd_debug (" destroy sub-dock icons");
			g_list_foreach (pIcon->pSubDock->icons, (GFunc) cairo_dock_free_icon, NULL);
			g_list_free (pIcon->pSubDock->icons);
			pIcon->pSubDock->icons = NULL;
			pIcon->pSubDock->pFirstDrawnElement = NULL;
		}
		else  // precaution pas chere
		{
			cd_debug (" destroy sub-dock");
			cairo_dock_destroy_dock (pIcon->pSubDock, pIcon->cName);
			pIcon->pSubDock = NULL;
		}
	}
}

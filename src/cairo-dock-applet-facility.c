/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <cairo.h>

#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-config.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-applet-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-applet-facility.h"

extern gchar *g_cCurrentThemePath;
extern gchar *g_cCairoDockDataDir;
extern gchar *g_cConfFile;
extern cairo_surface_t *g_pIconBackgroundImageSurface;
extern double g_iIconBackgroundImageWidth, g_iIconBackgroundImageHeight;

extern gboolean g_bUseOpenGL;


void cairo_dock_set_icon_surface_full (cairo_t *pIconContext, cairo_surface_t *pSurface, double fScale, double fAlpha, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (cairo_status (pIconContext) == CAIRO_STATUS_SUCCESS);
	
	//\________________ On efface l'ancienne image.
	cairo_dock_erase_cairo_context (pIconContext);
	
	//\_____________ On met le background de l'icone si necessaire
	if (pIcon != NULL &&
		pIcon->pIconBuffer != NULL &&
		g_pIconBackgroundImageSurface != NULL &&
		(CAIRO_DOCK_IS_NORMAL_LAUNCHER(pIcon) || CAIRO_DOCK_IS_APPLI(pIcon) || (myIcons.bBgForApplets && CAIRO_DOCK_IS_APPLET(pIcon))))
	{
		cd_message (">>> %s prendra un fond d'icone", pIcon->acName);

		cairo_save (pIconContext);
		cairo_scale(pIconContext,
			pIcon->fWidth / g_iIconBackgroundImageWidth,
			pIcon->fHeight / g_iIconBackgroundImageHeight);
		cairo_set_source_surface (pIconContext,
			g_pIconBackgroundImageSurface,
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
	
	cairo_dock_add_reflection_to_icon (pIconContext, pIcon, pContainer);
}

void cairo_dock_set_image_on_icon (cairo_t *pIconContext, gchar *cImagePath, Icon *pIcon, CairoContainer *pContainer)
{
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
	cairo_surface_t *pImageSurface = cairo_dock_create_surface_for_icon (cImagePath,
		pIconContext,
		iWidth,
		iHeight);
	
	cairo_dock_set_icon_surface_with_reflect (pIconContext, pImageSurface, pIcon, pContainer);
	
	cairo_surface_destroy (pImageSurface);
}

void cairo_dock_set_icon_surface_with_bar (cairo_t *pIconContext, cairo_surface_t *pSurface, double fValue, Icon *pIcon, CairoContainer *pContainer)
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
	cairo_dock_draw_bar_on_icon (pIconContext, fValue, pIcon, pContainer);
	
	if (g_bUseOpenGL)
		cairo_dock_update_icon_texture (pIcon);
}

void cairo_dock_draw_bar_on_icon (cairo_t *pIconContext, double fValue, Icon *pIcon, CairoContainer *pContainer)
{
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
	
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

void cairo_dock_set_hours_minutes_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds)
{
	int hours = iTimeInSeconds / 3600;
	int minutes = (iTimeInSeconds % 3600) / 60;
	if (hours != 0)
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%dh%02d", hours, abs (minutes));
	else
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%dmn", minutes);
}

void cairo_dock_set_minutes_secondes_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds)
{
	int minutes = iTimeInSeconds / 60;
	int secondes = iTimeInSeconds % 60;
	if (minutes != 0)
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%d:%02d", minutes, abs (secondes));
	else
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%s0:%02d", (secondes < 0 ? "-" : ""), abs (secondes));
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

void cairo_dock_set_size_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, long long int iSizeInBytes)
{
	gchar *cSize = cairo_dock_get_human_readable_size (iSizeInBytes);
	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	cairo_dock_set_quick_info (pSourceContext, cSize, pIcon, fMaxScale);
	g_free (cSize);
}



gchar *cairo_dock_get_theme_path_for_module (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName, const gchar *cShareThemesDir, const gchar *cExtraDirName)
{
	gchar *cThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	
	gchar *cUserThemesDir = (cExtraDirName != NULL ? g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, cExtraDirName) : NULL);
	gchar *cThemePath = cairo_dock_get_theme_path (cThemeName, cShareThemesDir, cUserThemesDir, cExtraDirName);
	
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
			GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cImage, 32, 32, NULL);
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
		cd_warning ("No sound to play, halt.");
		return;
	}
	
	GError *erreur = NULL;
	gchar *cSoundCommand = NULL;
	if (g_file_test ("/usr/bin/play", G_FILE_TEST_EXISTS))
		cSoundCommand = g_strdup_printf("play \"%s\"", cSoundPath);
		
	else if (g_file_test ("/usr/bin/aplay", G_FILE_TEST_EXISTS))
		cSoundCommand = g_strdup_printf("aplay \"%s\"", cSoundPath);
	
	else if (g_file_test ("/usr/bin/paplay", G_FILE_TEST_EXISTS))
		cSoundCommand = g_strdup_printf("paplay \"%s\"", cSoundPath);
	
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
		cairo_dock_build_main_ihm (g_cConfFile, FALSE);
		cairo_dock_present_module_instance_gui (pModuleInstance);
	}
}

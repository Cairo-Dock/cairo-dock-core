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

#include "../config.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-data-renderer.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-load.h"
#include "cairo-dock-icon-loader.h"

#define CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME "default-icon-appli.svg"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopGeometry g_desktopGeometry;

extern gchar *g_cCurrentThemePath;

extern CairoDockImageBuffer g_pIndicatorBuffer;
extern CairoDockImageBuffer g_pActiveIndicatorBuffer;
extern CairoDockImageBuffer g_pClassIndicatorBuffer;
extern CairoDockImageBuffer g_pIconBackgroundBuffer;
extern CairoDockImageBuffer g_pBoxAboveBuffer;
extern CairoDockImageBuffer g_pBoxBelowBuffer;

extern gboolean g_bUseOpenGL;

static void cairo_dock_load_launcher (Icon *icon, int iWidth, int iHeight, cairo_t* pSourceContext);
static void cairo_dock_load_appli (Icon *icon, int iWidth, int iHeight, cairo_t* pSourceContext);
static void cairo_dock_load_applet (Icon *icon, int iWidth, int iHeight, cairo_t* pSourceContext);
static void cairo_dock_load_separator (Icon *icon, int iWidth, int iHeight, cairo_t* pSourceContext);

static void _cairo_dock_draw_subdock_content_as_box (Icon *pIcon, int w, int h, cairo_t *pCairoContext);
static void _cairo_dock_draw_subdock_content_as_stack (Icon *pIcon, int w, int h, cairo_t *pCairoContext);
static void _cairo_dock_draw_subdock_content_as_emblem (Icon *pIcon, int w, int h, cairo_t *pCairoContext);


  //////////////
 /// LOADER ///
//////////////

void cairo_dock_fill_one_icon_buffer (Icon *icon, cairo_t* pSourceContext, gdouble fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp)
{
	//g_print ("%s (%d, %.2f, %s)\n", __func__, icon->iType, fMaxScale, icon->cFileName);
	if (cairo_dock_icon_is_being_removed (icon))  // si la fenetre est en train de se faire degager du dock, pas la peine de mettre a jour son icone. /// A voir pour les icones d'appli ...
	{
		cd_debug ("skip icon reload for %s", icon->cName);
		return;
	}
	
	//\______________ on reset les buffers (on garde la surface/texture actuelle pour les emblemes).
	cairo_surface_t *pPrevSurface = icon->pIconBuffer;
	GLuint iPrevTexture = icon->iIconTexture;
	
	if (icon->pReflectionBuffer != NULL)
	{
		cairo_surface_destroy (icon->pReflectionBuffer);
		icon->pReflectionBuffer = NULL;
	}
	
	if (icon->fWidth < 0 || icon->fHeight < 0)  // on ne veut pas de surface.
	{
		if (pPrevSurface != NULL)
			cairo_surface_destroy (pPrevSurface);
		icon->pIconBuffer = NULL;
		if (iPrevTexture != 0)
			_cairo_dock_delete_texture (iPrevTexture);
		icon->iIconTexture = 0;
		return;
	}
	
	int w, h;
	
	//\______________ on charge la surface/texture de l'icone.
	if (CAIRO_DOCK_IS_LAUNCHER (icon)/** || (CAIRO_DOCK_IS_USER_SEPARATOR (icon) && icon->cFileName != NULL)*/)  // si c'est un lanceur /*ou un separateur avec une icone.*/
	{
		/// A FAIRE : verifier qu'on peut enlever le test sur fMaxScale ...
		if (icon->fWidth == 0 || fMaxScale != 1.)
			icon->fWidth = myIcons.tIconAuthorizedWidth[icon->iType];
		if (icon->fHeight == 0 || fMaxScale != 1.)
			icon->fHeight = myIcons.tIconAuthorizedHeight[icon->iType];
		
		w = (bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale;
		h = (bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale;
		cairo_dock_load_launcher (icon, w, h, pSourceContext);
	}
	else if (CAIRO_DOCK_IS_APPLET (icon))  // c'est l'icone d'une applet.
	{
		//g_print ("  icon->cFileName : %s\n", icon->cFileName);
		if (icon->fWidth == 0)
			icon->fWidth = myIcons.tIconAuthorizedWidth[icon->iType];
		if (icon->fHeight == 0)
			icon->fHeight = myIcons.tIconAuthorizedHeight[icon->iType];
		
		w = (bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale;
		h = (bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale;
		cairo_dock_load_applet (icon, w, h, pSourceContext);
	}
	else if (CAIRO_DOCK_IS_APPLI (icon))  // c'est l'icone d'une appli valide. Dans cet ordre on n'a pas besoin de verifier que c'est NORMAL_APPLI.
	{
		icon->fWidth = myIcons.tIconAuthorizedWidth[icon->iType];
		icon->fHeight = myIcons.tIconAuthorizedHeight[icon->iType];
		
		w = (bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale;
		h = (bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale;
		cairo_dock_load_appli (icon, w, h, pSourceContext);
	}
	else  // c'est une icone de separation.
	{
		/// A FAIRE : verifier qu'on peut enlever le test sur fMaxScale ...
		if (icon->fWidth == 0 || fMaxScale != 1.)
			icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12];
		if (icon->fHeight == 0 || fMaxScale != 1.)
			icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12];
		
		w = (bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale;
		h = (bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale;
		cairo_dock_load_separator (icon, w, h, pSourceContext);
	}

	//\______________ Si rien charge, on met une image par defaut.
	if (icon->pIconBuffer == pPrevSurface)
	{
		gchar *cIconPath = cairo_dock_generate_file_path (CAIRO_DOCK_DEFAULT_ICON_NAME);
		if (cIconPath == NULL || ! g_file_test (cIconPath, G_FILE_TEST_EXISTS))
		{
			g_free (cIconPath);
			cIconPath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_ICON_NAME);
		}
		CairoDockIconType iType = cairo_dock_get_icon_type  (icon);
		/// A FAIRE : verifier qu'on peut enlever le test sur fMaxScale ...
		if (icon->fWidth == 0 || fMaxScale != 1.)
			icon->fWidth = myIcons.tIconAuthorizedWidth[iType];
		if (icon->fHeight == 0 || fMaxScale != 1.)
			icon->fHeight = myIcons.tIconAuthorizedHeight[iType];
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			pSourceContext,
			w,
			h);
		g_free (cIconPath);
	}
	cd_debug ("%s (%s) -> %.2fx%.2f", __func__, icon->cName, icon->fWidth, icon->fHeight);
	
	//\_____________ On met le background de l'icone si necessaire
	if (icon->pIconBuffer != NULL &&
		g_pIconBackgroundBuffer.pSurface != NULL &&
		(! CAIRO_DOCK_IS_SEPARATOR (icon)/* && (myIcons.bBgForApplets || ! CAIRO_DOCK_IS_APPLET(pIcon))*/))
	{
		cairo_t *pCairoIconBGContext = cairo_create (icon->pIconBuffer);
		cairo_scale(pCairoIconBGContext,
			w / g_pIconBackgroundBuffer.iWidth,
			h / g_pIconBackgroundBuffer.iHeight);
		cairo_set_source_surface (pCairoIconBGContext,
			g_pIconBackgroundBuffer.pSurface,
			0.,
			0.);
		cairo_set_operator (pCairoIconBGContext, CAIRO_OPERATOR_DEST_OVER);
		cairo_paint (pCairoIconBGContext);
		cairo_destroy (pCairoIconBGContext);
	}
	
	//\______________ le reflet en mode cairo.
	if (! g_bUseOpenGL && myIcons.fAlbedo > 0 && icon->pIconBuffer != NULL && ! (CAIRO_DOCK_IS_APPLET (icon) && icon->cFileName == NULL))
	{
		icon->pReflectionBuffer = cairo_dock_create_reflection_surface (icon->pIconBuffer,
			pSourceContext,
			w,
			h,
			myIcons.fReflectSize * fMaxScale,
			myIcons.fAlbedo,
			bIsHorizontal,
			bDirectionUp);
	}
	
	//\______________ on charge la texture si elle ne l'a pas ete.
	if (g_bUseOpenGL && icon->iIconTexture == iPrevTexture)
	{
		icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
	}
	
	if (pPrevSurface != NULL)
		cairo_surface_destroy (pPrevSurface);
	if (iPrevTexture != 0)
		_cairo_dock_delete_texture (iPrevTexture);
	//g_print ("%s (%s, %x, %.1fx%.1f) ->%d\n", __func__, icon->cName, icon->pIconBuffer, icon->fWidth, icon->fHeight, icon->iIconTexture);
}


gchar *cairo_dock_cut_string (const gchar *cString, int iNbCaracters)  // gere l'UTF-8
{
	g_return_val_if_fail (cString != NULL, NULL);
	gchar *cTruncatedName = NULL;
	gsize bytes_read, bytes_written;
	GError *erreur = NULL;
	gchar *cUtf8Name = g_locale_to_utf8 (cString,
		-1,
		&bytes_read,
		&bytes_written,
		&erreur);  // inutile sur Ubuntu, qui est nativement UTF8, mais sur les autres on ne sait pas.
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	if (cUtf8Name == NULL)  // une erreur s'est produite, on tente avec la chaine brute.
		cUtf8Name = g_strdup (cString);
	
	const gchar *cEndValidChain = NULL;
	int iStringLength;
	if (g_utf8_validate (cUtf8Name, -1, &cEndValidChain))
	{
		iStringLength = g_utf8_strlen (cUtf8Name, -1);
		int iNbChars = -1;
		if (iNbCaracters < 0)
		{
			iNbChars = MAX (0, iStringLength + iNbCaracters);
		}
		else if (iStringLength > iNbCaracters)
		{
			iNbChars = iNbCaracters;
		}
		
		if (iNbChars != -1)
		{
			cTruncatedName = g_new0 (gchar, 8 * (iNbChars + 4));  // 8 octets par caractere.
			if (iNbChars != 0)
				g_utf8_strncpy (cTruncatedName, cUtf8Name, iNbChars);
			
			gchar *cTruncature = g_utf8_offset_to_pointer (cTruncatedName, iNbChars);
			*cTruncature = '.';
			*(cTruncature+1) = '.';
			*(cTruncature+2) = '.';
		}
	}
	else
	{
		iStringLength = strlen (cString);
		int iNbChars = -1;
		if (iNbCaracters < 0)
		{
			iNbChars = MAX (0, iStringLength + iNbCaracters);
		}
		else if (iStringLength > iNbCaracters)
		{
			iNbChars = iNbCaracters;
		}
		
		if (iNbChars != -1)
		{
			cTruncatedName = g_new0 (gchar, iNbCaracters + 4);
			if (iNbChars != 0)
				strncpy (cTruncatedName, cString, iNbChars);
			
			cTruncatedName[iNbChars] = '.';
			cTruncatedName[iNbChars+1] = '.';
			cTruncatedName[iNbChars+2] = '.';
		}
	}
	if (cTruncatedName == NULL)
		cTruncatedName = cUtf8Name;
	else
		g_free (cUtf8Name);
	//g_print (" -> etiquette : %s\n", cTruncatedName);
	return cTruncatedName;
}

void cairo_dock_fill_one_text_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription)
{
	//g_print ("%s (%s, %d)\n", __func__, cLabelPolice, iLabelSize);
	cairo_surface_destroy (icon->pTextBuffer);
	icon->pTextBuffer = NULL;
	if (icon->iLabelTexture != 0)
	{
		_cairo_dock_delete_texture (icon->iLabelTexture);
		icon->iLabelTexture = 0;
	}
	if (icon->cName == NULL || (pTextDescription->iSize == 0))
		return ;

	gchar *cTruncatedName = NULL;
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.iAppliMaxNameLength > 0)
	{
		cTruncatedName = cairo_dock_cut_string (icon->cName, myTaskBar.iAppliMaxNameLength);
	}
	
	double fTextXOffset, fTextYOffset;
	cairo_surface_t* pNewSurface = cairo_dock_create_surface_from_text ((cTruncatedName != NULL ? cTruncatedName : icon->cName),
		pSourceContext,
		pTextDescription,
		&icon->iTextWidth, &icon->iTextHeight);
	g_free (cTruncatedName);
	//g_print (" -> %s : (%.2f;%.2f) %dx%d\n", icon->cName, icon->fTextXOffset, icon->fTextYOffset, icon->iTextWidth, icon->iTextHeight);

	icon->pTextBuffer = pNewSurface;
	
	if (g_bUseOpenGL && icon->pTextBuffer != NULL)
	{
		icon->iLabelTexture = cairo_dock_create_texture_from_surface (icon->pTextBuffer);
	}
}

void cairo_dock_fill_one_quick_info_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription, double fMaxScale)
{
	cairo_surface_destroy (icon->pQuickInfoBuffer);
	icon->pQuickInfoBuffer = NULL;
	if (icon->iQuickInfoTexture != 0)
	{
		_cairo_dock_delete_texture (icon->iQuickInfoTexture);
		icon->iQuickInfoTexture = 0;
	}
	if (icon->cQuickInfo == NULL)
		return ;

	double fQuickInfoXOffset, fQuickInfoYOffset;
	icon->pQuickInfoBuffer = cairo_dock_create_surface_from_text_full (icon->cQuickInfo,
		pSourceContext,
		pTextDescription,
		fMaxScale,
		icon->fWidth * fMaxScale,
		&icon->iQuickInfoWidth, &icon->iQuickInfoHeight, NULL, NULL);
	
	if (g_bUseOpenGL && icon->pQuickInfoBuffer != NULL)
	{
		icon->iQuickInfoTexture = cairo_dock_create_texture_from_surface (icon->pQuickInfoBuffer);
	}
}



void cairo_dock_fill_icon_buffers (Icon *icon, cairo_t *pSourceContext, double fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp)
{
	cairo_dock_fill_one_icon_buffer (icon, pSourceContext, fMaxScale, bIsHorizontal, bDirectionUp);

	cairo_dock_fill_one_text_buffer (icon, pSourceContext, &myLabels.iconTextDescription);

	cairo_dock_fill_one_quick_info_buffer (icon, pSourceContext, &myLabels.quickInfoTextDescription, fMaxScale);
}

void cairo_dock_load_one_icon_from_scratch (Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (pIcon != NULL);
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pContainer));
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		cairo_dock_fill_icon_buffers_for_dock (pIcon, pCairoContext, pDock);
	}
	else
	{
		cairo_dock_fill_icon_buffers_for_desklet (pIcon, pCairoContext);
	}
	cairo_destroy (pCairoContext);
}

void cairo_dock_reload_buffers_in_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	gboolean bReloadAppletsToo = GPOINTER_TO_INT (data);
	cd_message ("%s (%s, %d)", __func__, cDockName, bReloadAppletsToo);

	double fFlatDockWidth = - myIcons.iIconGap;
	pDock->iMaxIconHeight = 0;

	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDock));

	Icon* icon;
	GList* ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		
		if (CAIRO_DOCK_IS_APPLET (icon))
		{
			if (bReloadAppletsToo)  /// modif du 23/05/2009 : utiliser la taille avec ratio ici. les applets doivent faire attention a utiliser la fonction get_icon_extent().
				cairo_dock_reload_module_instance (icon->pModuleInstance, FALSE);
		}
		else
		{
			icon->fWidth /= pDock->container.fRatio;
			icon->fHeight /= pDock->container.fRatio;
			cairo_dock_fill_icon_buffers_for_dock (icon, pCairoContext, pDock);
			icon->fWidth *= pDock->container.fRatio;
			icon->fHeight *= pDock->container.fRatio;
		}
		
		//g_print (" =size <- %.2fx%.2f\n", icon->fWidth, icon->fHeight);
		fFlatDockWidth += myIcons.iIconGap + icon->fWidth;
		pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);
	}
	pDock->fFlatDockWidth = (int) fFlatDockWidth;  /// (int) n'est plus tellement necessaire ...
	cairo_destroy (pCairoContext);
}

void cairo_dock_reload_one_icon_buffer_in_dock (Icon *icon, CairoDock *pDock)
{
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDock));
	cairo_dock_reload_one_icon_buffer_in_dock_full (icon, pDock, pCairoContext);
	cairo_destroy (pCairoContext);
}


  //////////////////
 /// RESSOURCES ///
//////////////////

void cairo_dock_load_icons_background_surface (const gchar *cImagePath, double fMaxScale)
{
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	
	int iSize = MAX (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI]);
	if (iSize == 0)
		iSize = 48;
	iSize *= fMaxScale;
	cairo_dock_load_image_buffer (&g_pIconBackgroundBuffer,
		cImagePath,
		iSize,
		iSize,
		CAIRO_DOCK_FILL_SPACE);
}

void cairo_dock_load_box_surface (double fMaxScale)
{
	cairo_dock_unload_image_buffer (&g_pBoxAboveBuffer);
	cairo_dock_unload_image_buffer (&g_pBoxBelowBuffer);
	
	int iSize = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
	if (iSize == 0)
		iSize = 48;
	iSize *= fMaxScale;
	
	gchar *cUserPath = cairo_dock_generate_file_path ("box-front.png");
	if (! g_file_test (cUserPath, G_FILE_TEST_EXISTS))
	{
		g_free (cUserPath);
		cUserPath = NULL;
	}
	cairo_dock_load_image_buffer (&g_pBoxAboveBuffer,
		cUserPath ? cUserPath : CAIRO_DOCK_SHARE_DATA_DIR"/box-front.png",
		iSize,
		iSize,
		CAIRO_DOCK_FILL_SPACE);
	
	cUserPath = cairo_dock_generate_file_path ("box-back.png");
	if (! g_file_test (cUserPath, G_FILE_TEST_EXISTS))
	{
		g_free (cUserPath);
		cUserPath = NULL;
	}
	cairo_dock_load_image_buffer (&g_pBoxBelowBuffer,
		CAIRO_DOCK_SHARE_DATA_DIR"/box-back.png",
		iSize,
		iSize,
		CAIRO_DOCK_FILL_SPACE);
}

void cairo_dock_load_task_indicator (const gchar *cIndicatorImagePath, double fMaxScale, double fIndicatorRatio)
{
	cairo_dock_unload_image_buffer (&g_pIndicatorBuffer);
	
	double fLauncherWidth = (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : 48);
	double fLauncherHeight = (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : 48);
	double fScale = (myIndicators.bLinkIndicatorWithIcon ? fMaxScale : 1.) * fIndicatorRatio;
	
	cairo_dock_load_image_buffer (&g_pIndicatorBuffer,
		cIndicatorImagePath,
		fLauncherWidth * fScale,
		fLauncherHeight * fScale,
		CAIRO_DOCK_KEEP_RATIO);
}

void cairo_dock_load_active_window_indicator (const gchar *cImagePath, double fMaxScale, double fCornerRadius, double fLineWidth, double *fActiveColor)
{
	cairo_dock_unload_image_buffer (&g_pActiveIndicatorBuffer);
	
	int iWidth = MAX (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI]);
	int iHeight = MAX (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI], myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI]);
	if (iWidth == 0)
		iWidth = 48;
	if (iHeight == 0)
		iHeight = 48;
	iWidth *= fMaxScale;
	iHeight *= fMaxScale;
	
	if (cImagePath != NULL)
	{
		cairo_dock_load_image_buffer (&g_pActiveIndicatorBuffer,
			cImagePath,
			iWidth,
			iHeight,
			CAIRO_DOCK_FILL_SPACE);
	}
	else if (fActiveColor[3] > 0)
	{
		cairo_surface_t *pSurface = cairo_dock_create_blank_surface (iWidth, iHeight);
		cairo_t *pCairoContext = cairo_create (pSurface);
		
		fCornerRadius = MIN (fCornerRadius, (iWidth - fLineWidth) / 2);
		double fFrameWidth = iWidth - (2 * fCornerRadius + fLineWidth);
		double fFrameHeight = iHeight - 2 * fLineWidth;
		double fDockOffsetX = fCornerRadius + fLineWidth/2;
		double fDockOffsetY = fLineWidth/2;
		cairo_dock_draw_frame (pCairoContext, fCornerRadius, fLineWidth, fFrameWidth, fFrameHeight, fDockOffsetX, fDockOffsetY, 1, 0., CAIRO_DOCK_HORIZONTAL);
		
		cairo_set_source_rgba (pCairoContext, fActiveColor[0], fActiveColor[1], fActiveColor[2], fActiveColor[3]);
		if (fLineWidth > 0)
		{
			cairo_set_line_width (pCairoContext, fLineWidth);
			cairo_stroke (pCairoContext);
		}
		else
		{
			cairo_fill (pCairoContext);
		}
		cairo_destroy (pCairoContext);
		
		cairo_dock_load_image_buffer_from_surface (&g_pActiveIndicatorBuffer,
			pSurface,
			iWidth,
			iHeight);
	}
}

void cairo_dock_load_class_indicator (const gchar *cIndicatorImagePath, double fMaxScale)
{
	cairo_dock_unload_image_buffer (&g_pClassIndicatorBuffer);
	
	double fLauncherWidth = (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : 48);
	double fLauncherHeight = (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : 48);
	
	cairo_dock_load_image_buffer (&g_pClassIndicatorBuffer,
		cIndicatorImagePath,
		fLauncherWidth/3,
		fLauncherHeight/3,
		CAIRO_DOCK_KEEP_RATIO);
}

void cairo_dock_load_icon_textures (void)
{
	double fMaxScale = cairo_dock_get_max_scale (g_pMainDock);
	
	cairo_dock_load_icons_background_surface (myIcons.cBackgroundImagePath, fMaxScale);
	
	cairo_dock_load_task_indicator (myTaskBar.bShowAppli && (myTaskBar.bMixLauncherAppli || myTaskBar.bDrawIndicatorOnAppli) ? myIndicators.cIndicatorImagePath : NULL, fMaxScale, myIndicators.fIndicatorRatio);
	
	cairo_dock_load_active_window_indicator (myIndicators.cActiveIndicatorImagePath, fMaxScale, myIndicators.iActiveCornerRadius, myIndicators.iActiveLineWidth, myIndicators.fActiveColor);
	
	cairo_dock_load_class_indicator (myTaskBar.bShowAppli && myTaskBar.bGroupAppliByClass ? myIndicators.cClassIndicatorImagePath : NULL, fMaxScale);
	
	cairo_dock_load_box_surface (fMaxScale);  /// a deplacer...
}

void cairo_dock_unload_icon_textures (void)
{
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	cairo_dock_unload_image_buffer (&g_pIndicatorBuffer);
	cairo_dock_unload_image_buffer (&g_pActiveIndicatorBuffer);
	cairo_dock_unload_image_buffer (&g_pClassIndicatorBuffer);
	cairo_dock_unload_image_buffer (&g_pBoxAboveBuffer);
	cairo_dock_unload_image_buffer (&g_pBoxBelowBuffer);
}



void cairo_dock_add_reflection_to_icon (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer)
{
	if (g_bUseOpenGL)
		return ;
	g_return_if_fail (pIcon != NULL && pContainer!= NULL);
	if (pIcon->pReflectionBuffer != NULL)
		cairo_surface_destroy (pIcon->pReflectionBuffer);
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
	pIcon->pReflectionBuffer = cairo_dock_create_reflection_surface (pIcon->pIconBuffer,
		pSourceContext,
		iWidth,
		iHeight,
		myIcons.fReflectSize * cairo_dock_get_max_scale (pContainer),
		myIcons.fAlbedo,
		pContainer->bIsHorizontal,
		pContainer->bDirectionUp);
}

  ///////////////////////
 /// CONTAINER ICONS ///
///////////////////////

void cairo_dock_draw_subdock_content_on_icon (Icon *pIcon, CairoDock *pDock)
{
	g_return_if_fail (pIcon != NULL && pIcon->pSubDock != NULL && (pIcon->pIconBuffer != NULL || pIcon->iIconTexture != 0));
	//g_print ("%s (%s)\n", __func__, pIcon->cName);
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, CAIRO_CONTAINER (pDock), &w, &h);
	
	//\______________ On efface le dessin existant.
	cairo_t *pCairoContext = NULL;
	if (pIcon->iIconTexture != 0)  // dessin opengl
	{
		if (! cairo_dock_begin_draw_icon (pIcon, CAIRO_CONTAINER (pDock), 0))
			return ;
		
		_cairo_dock_set_blend_source ();
		if (g_pIconBackgroundBuffer.iTexture != 0)  // on ecrase le dessin existant avec l'image de fond des icones.
		{
			_cairo_dock_enable_texture ();
			_cairo_dock_set_alpha (1.);
			_cairo_dock_apply_texture_at_size (g_pIconBackgroundBuffer.iTexture, w, h);
		}
		else  // sinon on efface juste ce qu'il y'avait.
		{
			glPolygonMode (GL_FRONT, GL_FILL);
			_cairo_dock_set_alpha (0.);
			glBegin(GL_QUADS);
			glVertex3f(-.5*w,  .5*h, 0.);
			glVertex3f( .5*w,  .5*h, 0.);
			glVertex3f( .5*w, -.5*h, 0.);
			glVertex3f(-.5*w, -.5*h, 0.);
			glEnd();
			_cairo_dock_enable_texture ();
			_cairo_dock_set_alpha (1.);
		}
		_cairo_dock_set_blend_alpha ();
	}
	else if (pIcon->pIconBuffer != NULL)  // dessin cairo
	{
		pCairoContext = cairo_create (pIcon->pIconBuffer);
		g_return_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
		
		if (g_pIconBackgroundBuffer.pSurface != NULL)  // on ecrase le dessin existant avec l'image de fond des icones.
		{
			cairo_save (pCairoContext);
			cairo_scale(pCairoContext,
				(double) w / g_pIconBackgroundBuffer.iWidth,
				(double) h / g_pIconBackgroundBuffer.iHeight);
			cairo_set_source_surface (pCairoContext,
				g_pIconBackgroundBuffer.pSurface,
				0.,
				0.);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
			cairo_paint (pCairoContext);
			cairo_restore (pCairoContext);
		}
		else  // sinon on efface juste ce qu'il y'avait.
		{
			cairo_dock_erase_cairo_context (pCairoContext);
		}
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	}
	else
		return ;
	
	//\______________ On dessine les 3 ou 4 premieres icones du sous-dock.
	if (pIcon->cClass != NULL)
		_cairo_dock_draw_subdock_content_as_stack (pIcon, w, h, pCairoContext);
	else
	{
		switch (pIcon->iSubdockViewType)
		{
			case 1 :
				_cairo_dock_draw_subdock_content_as_emblem (pIcon, w, h, pCairoContext);
			break;
			case 2:
				_cairo_dock_draw_subdock_content_as_stack (pIcon, w, h, pCairoContext);
			break;
			case 3:
				_cairo_dock_draw_subdock_content_as_box (pIcon, w, h, pCairoContext);
			break;
			default:
				cd_warning ("invalid sub-dock content view for %s", pIcon->cName);
			break;
		}
	}
	
	//\______________ On finit le dessin.
	if (pIcon->iIconTexture != 0)
	{
		_cairo_dock_disable_texture ();
		cairo_dock_end_draw_icon (pIcon, CAIRO_CONTAINER (pDock));
	}
	else
	{
		if (g_bUseOpenGL)
			cairo_dock_update_icon_texture (pIcon);
		else
			cairo_dock_add_reflection_to_icon (pCairoContext, pIcon, CAIRO_CONTAINER (pDock));
		cairo_destroy (pCairoContext);
	}
}





  //////////////////////
 /// IMPLEMENTATION ///
//////////////////////

static void cairo_dock_load_launcher (Icon *icon, int iWidth, int iHeight, cairo_t* pSourceContext)
{
	gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName);
	
	if (icon->pSubDock != NULL && icon->iSubdockViewType != 0)  // icone de sous-dock avec un rendu specifique, on le redessinera lorsque les icones du sous-dock auront ete chargees.
	{
		icon->pIconBuffer = cairo_dock_create_blank_surface (iWidth, iHeight);
	}
	else if (cIconPath != NULL && *cIconPath != '\0')  // icone possedant une image, on affiche celle-ci.
	{
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			pSourceContext,
			iWidth,
			iHeight);
	}
	else if (icon->pSubDock != NULL && icon->cClass != NULL && icon->cDesktopFileName == NULL)  // icone pointant sur une classe (epouvantail).
	{
		//g_print ("c'est un epouvantail\n");
		icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass,
			pSourceContext,
			iWidth,
			iHeight);
		if (icon->pIconBuffer == NULL)  // aucun inhibiteur ou aucune image correspondant a cette classe, on cherche a copier une des icones d'appli de cette classe.
		{
			const GList *pApplis = cairo_dock_list_existing_appli_with_class (icon->cClass);
			if (pApplis != NULL)
			{
				Icon *pOneIcon = (Icon *) (g_list_last ((GList*)pApplis)->data);  // on prend le dernier car les applis sont inserees a l'envers, et on veut avoir celle qui etait deja present dans le dock (pour 2 raison : continuite, et la nouvelle (en 1ere position) n'est pas forcement deja dans un dock, ce qui fausse le ratio).
				icon->pIconBuffer = cairo_dock_duplicate_inhibator_surface_for_appli (pSourceContext,
					pOneIcon,
					iWidth,
					iHeight);
			}
		}
	}
	g_free (cIconPath);
}

static void cairo_dock_load_appli (Icon *icon, int iWidth, int iHeight, cairo_t* pSourceContext)
{
	if (myTaskBar.iMinimizedWindowRenderType == 1 && icon->bIsHidden && icon->iBackingPixmap != 0)
	{
		cairo_surface_t *pPrevSurface = icon->pIconBuffer;
		GLuint iPrevTexture = icon->iIconTexture;
		
		// on cree la miniature.
		if (g_bUseOpenGL)
		{
			icon->iIconTexture = cairo_dock_texture_from_pixmap (icon->Xid, icon->iBackingPixmap);
			//g_print ("opengl thumbnail : %d\n", icon->iIconTexture);
		}
		if (icon->iIconTexture == 0)
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_xpixmap (icon->iBackingPixmap, pSourceContext, iWidth, iHeight);
			if (g_bUseOpenGL)
				icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
		}
		// on affiche l'image precedente en embleme.
		if (icon->iIconTexture != 0 && iPrevTexture != 0)
		{
			CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			CairoEmblem *e = cairo_dock_make_emblem_from_texture (iPrevTexture,icon, CAIRO_CONTAINER (pParentDock));
			cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
			cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
			g_free (e);  // on n'utilise pas cairo_dock_free_emblem pour ne pas detruire la texture avec.
		}
		else if (icon->pIconBuffer != NULL && pPrevSurface != NULL)
		{
			CairoDock *pParentDock = NULL;  // cairo_dock_search_dock_from_name (icon->cParentDockName);
			CairoEmblem *e = cairo_dock_make_emblem_from_surface (pPrevSurface, 0, 0, icon, CAIRO_CONTAINER (pParentDock));
			cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
			cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
			g_free (e);  // meme remarque.
		}
	}
	if (icon->pIconBuffer == NULL && myTaskBar.bOverWriteXIcons && ! cairo_dock_class_is_using_xicon (icon->cClass))
		icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass, pSourceContext, iWidth, iHeight);
	if (icon->pIconBuffer == NULL)
		icon->pIconBuffer = cairo_dock_create_surface_from_xwindow (icon->Xid, pSourceContext, iWidth, iHeight);
	if (icon->pIconBuffer == NULL)  // certaines applis comme xterm ne definissent pas d'icone, on en met une par defaut.
	{
		g_print ("%s (%ld) doesn't define any icon, we set the default one.\n", icon->cName, icon->Xid);
		gchar *cIconPath = cairo_dock_generate_file_path (CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
		if (cIconPath == NULL || ! g_file_test (cIconPath, G_FILE_TEST_EXISTS))
		{
			g_free (cIconPath);
			cIconPath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
		}
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			pSourceContext,
			iWidth,
			iHeight);
		g_free (cIconPath);
	}
}

static void cairo_dock_load_applet (Icon *icon, int iWidth, int iHeight, cairo_t* pSourceContext)
{
	icon->pIconBuffer = cairo_dock_create_applet_surface (icon->cFileName,
		pSourceContext,
		iWidth,
		iHeight);
}

static void cairo_dock_load_separator (Icon *icon, int iWidth, int iHeight, cairo_t* pSourceContext)
{
	if (CAIRO_DOCK_IS_USER_SEPARATOR (icon) && icon->cFileName != NULL)
	{
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName);
		if (cIconPath != NULL && *cIconPath != '\0')
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
				pSourceContext,
				iWidth,
				iHeight);
		}
		g_free (cIconPath);
	}
	else
	{
		icon->pIconBuffer = cairo_dock_create_separator_surface (pSourceContext,
			iWidth,
			iHeight);
	}
}



static void _cairo_dock_draw_subdock_content_as_emblem (Icon *pIcon, int w, int h, cairo_t *pCairoContext)
{
	//\______________ On dessine les 4 premieres icones du sous-dock en embleme.
	CairoEmblem e;
	memset (&e, 0, sizeof (CairoEmblem));
	e.fScale = .5;
	int i;
	Icon *icon;
	GList *ic;
	for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 4; ic = ic->next, i++)
	{
		icon = ic->data;
		if (CAIRO_DOCK_IS_SEPARATOR (icon))
		{
			i --;
			continue;
		}
		e.iPosition = i;
		if (pCairoContext == NULL)
		{
			e.iTexture = icon->iIconTexture;
			_cairo_dock_apply_emblem_texture (&e, w, h);
		}
		else
		{
			e.pSurface = icon->pIconBuffer;
			cairo_dock_get_icon_extent (icon, CAIRO_CONTAINER (pIcon->pSubDock), &e.iWidth, &e.iHeight);
			
			cairo_save (pCairoContext);
			_cairo_dock_apply_emblem_surface (&e, w, h, pCairoContext);
			cairo_restore (pCairoContext);
		}
	}
}

static void _cairo_dock_draw_subdock_content_as_stack (Icon *pIcon, int w, int h, cairo_t *pCairoContext)
{
	//\______________ On dessine les 4 premieres icones du sous-dock en pile.
	CairoEmblem e;
	memset (&e, 0, sizeof (CairoEmblem));
	e.fScale = .8;
	int i;
	Icon *icon;
	GList *ic;
	for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 3; ic = ic->next, i++)
	{
		icon = ic->data;
		if (CAIRO_DOCK_IS_SEPARATOR (icon))
		{
			i --;
			continue;
		}
		switch (i)
		{
			case 0:
				e.iPosition = CAIRO_DOCK_EMBLEM_UPPER_RIGHT;
			break;
			case 1:
				if (ic->next == NULL)
					e.iPosition = CAIRO_DOCK_EMBLEM_LOWER_LEFT;
				else
					e.iPosition = CAIRO_DOCK_EMBLEM_MIDDLE;
			break;
			case 2:
				e.iPosition = CAIRO_DOCK_EMBLEM_LOWER_LEFT;
			break;
			default : break;
		}
		if (pIcon->iIconTexture != 0)
		{
			e.iTexture = icon->iIconTexture;
			_cairo_dock_apply_emblem_texture (&e, w, h);
		}
		else
		{
			e.pSurface = icon->pIconBuffer;
			cairo_dock_get_icon_extent (icon, CAIRO_CONTAINER (pIcon->pSubDock), &e.iWidth, &e.iHeight);
			
			cairo_save (pCairoContext);
			_cairo_dock_apply_emblem_surface (&e, w, h, pCairoContext);
			cairo_restore (pCairoContext);
		}
	}
}

static void _cairo_dock_draw_subdock_content_as_box (Icon *pIcon, int w, int h, cairo_t *pCairoContext)
{
	if (pCairoContext)
	{
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
		cairo_save (pCairoContext);
		cairo_scale(pCairoContext,
			(double) w / g_pBoxBelowBuffer.iWidth,
			(double) h / g_pBoxBelowBuffer.iHeight);
		cairo_set_source_surface (pCairoContext,
			g_pBoxBelowBuffer.pSurface,
			0.,
			0.);
		cairo_paint (pCairoContext);
		cairo_restore (pCairoContext);
		
		cairo_save (pCairoContext);
		cairo_scale(pCairoContext,
			.8,
			.8);
		int i;
		Icon *icon;
		GList *ic;
		for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 3; ic = ic->next, i++)
		{
			icon = ic->data;
			if (CAIRO_DOCK_IS_SEPARATOR (icon))
			{
				i --;
				continue;
			}
			
			cairo_set_source_surface (pCairoContext,
				icon->pIconBuffer,
				.1*w,
				.1*i*h);
			cairo_paint (pCairoContext);
		}
		cairo_restore (pCairoContext);
		
		cairo_scale(pCairoContext,
			(double) w / g_pBoxAboveBuffer.iWidth,
			(double) h / g_pBoxAboveBuffer.iHeight);
		cairo_set_source_surface (pCairoContext,
			g_pBoxAboveBuffer.pSurface,
			0.,
			0.);
		cairo_paint (pCairoContext);
	}
	else
	{
		_cairo_dock_set_blend_source ();
		_cairo_dock_apply_texture_at_size (g_pBoxBelowBuffer.iTexture, w, h);
		
		_cairo_dock_set_blend_alpha ();
		int i;
		Icon *icon;
		GList *ic;
		for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 3; ic = ic->next, i++)
		{
			icon = ic->data;
			if (CAIRO_DOCK_IS_SEPARATOR (icon))
			{
				i --;
				continue;
			}
			glBindTexture (GL_TEXTURE_2D, icon->iIconTexture);
			_cairo_dock_apply_current_texture_at_size_with_offset (.8*w, .8*h, 0., .1*(1-i)*h);
		}
		
		_cairo_dock_apply_texture_at_size (g_pBoxAboveBuffer.iTexture, w, h);
	}
}

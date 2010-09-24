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
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-data-renderer.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-icon-loader.h"
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-load.h"

#define CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME "default-icon-appli.svg"

GLuint g_pGradationTexture[2]={0, 0};

extern CairoDockDesktopGeometry g_desktopGeometry;
extern CairoDockDesktopBackground *g_pFakeTransparencyDesktopBg;

extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentIconsPath;
extern gchar *g_cCurrentImagesPath;

extern GLuint g_pGradationTexture[2];

extern gboolean g_bUseOpenGL;

static CairoDockDesktopBackground *s_pDesktopBg = NULL;  // une fois alloue, le pointeur restera le meme tout le temps.

void cairo_dock_free_label_description (CairoDockLabelDescription *pTextDescription)
{
	if (pTextDescription == NULL)
		return ;
	g_free (pTextDescription->cFont);
	g_free (pTextDescription);
}

void cairo_dock_copy_label_description (CairoDockLabelDescription *pDestTextDescription, CairoDockLabelDescription *pOrigTextDescription)
{
	g_return_if_fail (pOrigTextDescription != NULL && pDestTextDescription != NULL);
	memcpy (pDestTextDescription, pOrigTextDescription, sizeof (CairoDockLabelDescription));
	pDestTextDescription->cFont = g_strdup (pOrigTextDescription->cFont);
}

CairoDockLabelDescription *cairo_dock_duplicate_label_description (CairoDockLabelDescription *pOrigTextDescription)
{
	g_return_val_if_fail (pOrigTextDescription != NULL, NULL);
	CairoDockLabelDescription *pTextDescription = g_memdup (pOrigTextDescription, sizeof (CairoDockLabelDescription));
	pTextDescription->cFont = g_strdup (pOrigTextDescription->cFont);
	return pTextDescription;
}

gchar *cairo_dock_search_image_s_path (const gchar *cImageFile)
{
	g_return_val_if_fail (cImageFile != NULL, NULL);
	gchar *cImagePath;
	if (*cImageFile == '~')
	{
		cImagePath = g_strdup_printf ("%s%s", getenv("HOME"), cImageFile + 1);
		if (!g_file_test (cImagePath, G_FILE_TEST_EXISTS))
		{
			g_free (cImagePath);
			cImagePath = NULL;
		}
	}
	else if (*cImageFile == '/')
	{
		if (!g_file_test (cImageFile, G_FILE_TEST_EXISTS))
			cImagePath = NULL;
		else
			cImagePath = g_strdup (cImageFile);
	}
	else
	{
		cImagePath = g_strdup_printf ("%s/%s", g_cCurrentImagesPath, cImageFile);
		if (!g_file_test (cImagePath, G_FILE_TEST_EXISTS))
		{
			g_free (cImagePath);
			cImagePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, cImageFile);
			if (!g_file_test (cImagePath, G_FILE_TEST_EXISTS))
			{
				g_free (cImagePath);
				cImagePath = g_strdup_printf ("%s/%s", g_cCurrentIconsPath, cImageFile);
				if (!g_file_test (cImagePath, G_FILE_TEST_EXISTS))
				{
					g_free (cImagePath);
					cImagePath = NULL;
				}
			}
		}
	}
	return cImagePath;
}


void cairo_dock_load_image_buffer_full (CairoDockImageBuffer *pImage, const gchar *cImageFile, int iWidth, int iHeight, CairoDockLoadImageModifier iLoadModifier, double fAlpha)
{
	if (cImageFile == NULL)
		return;
	gchar *cImagePath = cairo_dock_search_image_s_path (cImageFile);
	double w, h;
	pImage->pSurface = cairo_dock_create_surface_from_image (
		cImagePath,
		1.,
		iWidth,
		iHeight,
		iLoadModifier,
		&w,
		&h,
		&pImage->fZoomX,
		&pImage->fZoomY);
	pImage->iWidth = w;
	pImage->iHeight = h;
	
	if (fAlpha < 1)
	{
		cairo_surface_t *pNewSurfaceAlpha = cairo_dock_create_blank_surface (
			w,
			h);
		cairo_t *pCairoContext = cairo_create (pNewSurfaceAlpha);

		cairo_set_source_surface (pCairoContext, pImage->pSurface, 0, 0);
		cairo_paint_with_alpha (pCairoContext, fAlpha);
		cairo_destroy (pCairoContext);

		cairo_surface_destroy (pImage->pSurface);
		pImage->pSurface = pNewSurfaceAlpha;
	}
	
	if (g_bUseOpenGL)
		pImage->iTexture = cairo_dock_create_texture_from_surface (pImage->pSurface);
	
	g_free (cImagePath);
}

void cairo_dock_load_image_buffer_from_surface (CairoDockImageBuffer *pImage, cairo_surface_t *pSurface, int iWidth, int iHeight)
{
	pImage->pSurface = pSurface;
	pImage->iWidth = iWidth;
	pImage->iHeight = iHeight;
	pImage->fZoomX = 1.;
	pImage->fZoomY = 1.;
	if (g_bUseOpenGL)
		pImage->iTexture = cairo_dock_create_texture_from_surface (pImage->pSurface);
}

CairoDockImageBuffer *cairo_dock_create_image_buffer (const gchar *cImageFile, int iWidth, int iHeight, CairoDockLoadImageModifier iLoadModifier)
{
	CairoDockImageBuffer *pImage = g_new0 (CairoDockImageBuffer, 1);
	
	cairo_dock_load_image_buffer (pImage, cImageFile, iWidth, iHeight, iLoadModifier);
	
	return pImage;
}

void cairo_dock_unload_image_buffer (CairoDockImageBuffer *pImage)
{
	if (pImage->pSurface != NULL)
	{
		cairo_surface_destroy (pImage->pSurface);
	}
	if (pImage->iTexture != 0)
	{
		_cairo_dock_delete_texture (pImage->iTexture);
	}
	memset (pImage, 0, sizeof (CairoDockImageBuffer));
}

void cairo_dock_free_image_buffer (CairoDockImageBuffer *pImage)
{
	if (pImage == NULL)
		return;
	cairo_dock_unload_image_buffer (pImage);
	g_free (pImage);
}


  ///////////////////////
 /// DOCK BACKGROUND ///
///////////////////////

static cairo_surface_t *_cairo_dock_make_stripes_background (int iWidth, int iHeight, double *fStripesColorBright, double *fStripesColorDark, int iNbStripes, double fStripesWidth, double fStripesAngle)
{
	cairo_pattern_t *pStripesPattern;
	double fWidth = iWidth;
	if (fabs (fStripesAngle) != 90)
		pStripesPattern = cairo_pattern_create_linear (0.0f,
			0.0f,
			iWidth,
			iWidth * tan (fStripesAngle * G_PI/180.));
	else
		pStripesPattern = cairo_pattern_create_linear (0.0f,
			0.0f,
			0.,
			(fStripesAngle == 90) ? iHeight : - iHeight);
	g_return_val_if_fail (cairo_pattern_status (pStripesPattern) == CAIRO_STATUS_SUCCESS, NULL);

	cairo_pattern_set_extend (pStripesPattern, CAIRO_EXTEND_REPEAT);

	if (iNbStripes > 0)
	{
		gdouble fStep;
		int i;
		for (i = 0; i < iNbStripes+1; i ++)
		{
			fStep = (double)i / iNbStripes;
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep - fStripesWidth / 2.,
				fStripesColorBright[0],
				fStripesColorBright[1],
				fStripesColorBright[2],
				fStripesColorBright[3]);
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep,
				fStripesColorDark[0],
				fStripesColorDark[1],
				fStripesColorDark[2],
				fStripesColorDark[3]);
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep + fStripesWidth / 2.,
				fStripesColorBright[0],
				fStripesColorBright[1],
				fStripesColorBright[2],
				fStripesColorBright[3]);
		}
	}
	else
	{
		cairo_pattern_add_color_stop_rgba (pStripesPattern,
			0.,
			fStripesColorDark[0],
			fStripesColorDark[1],
			fStripesColorDark[2],
			fStripesColorDark[3]);
		cairo_pattern_add_color_stop_rgba (pStripesPattern,
			1.,
			fStripesColorBright[0],
			fStripesColorBright[1],
			fStripesColorBright[2],
			fStripesColorBright[3]);
	}

	cairo_surface_t *pNewSurface = cairo_dock_create_blank_surface (
			iWidth,
			iHeight);
	cairo_t *pImageContext = cairo_create (pNewSurface);
	cairo_set_source (pImageContext, pStripesPattern);
	cairo_paint (pImageContext);

	cairo_pattern_destroy (pStripesPattern);
	cairo_destroy (pImageContext);
	
	return pNewSurface;
}
static void _cairo_dock_load_default_background (CairoDockImageBuffer *pImage, int iWidth, int iHeight)
{
	//g_print ("%s (%s, %d)\n", __func__, myBackground.cBackgroundImageFile, myBackground.bBackgroundImageRepeat);
	if (myBackground.cBackgroundImageFile != NULL)
	{
		if (myBackground.bBackgroundImageRepeat)
		{
			cairo_surface_t *pBgSurface = cairo_dock_create_surface_from_pattern (myBackground.cBackgroundImageFile,
				iWidth,
				iHeight,
				myBackground.fBackgroundImageAlpha);
			cairo_dock_load_image_buffer_from_surface (pImage,
				pBgSurface,
				iWidth,
				iHeight);
		}
		else
		{
			cairo_dock_load_image_buffer_full (pImage,
				myBackground.cBackgroundImageFile,
				iWidth,
				iHeight,
				CAIRO_DOCK_FILL_SPACE,
				myBackground.fBackgroundImageAlpha);
		}
	}
	if (pImage->pSurface == NULL)
	{
		cairo_surface_t *pBgSurface = _cairo_dock_make_stripes_background (
			iWidth,
			iHeight,
			myBackground.fStripesColorBright,
			myBackground.fStripesColorDark,
			myBackground.iNbStripes,
			myBackground.fStripesWidth,
			myBackground.fStripesAngle);
		cairo_dock_load_image_buffer_from_surface (pImage,
			pBgSurface,
			iWidth,
			iHeight);
	}
}

void cairo_dock_load_dock_background (CairoDock *pDock)
{
	cairo_dock_unload_image_buffer (&pDock->backgroundBuffer);
	
	int iWidth = pDock->iDecorationsWidth;
	int iHeight = pDock->iDecorationsHeight;
	
	if (pDock->bGlobalBg || pDock->iRefCount > 0)
	{
		_cairo_dock_load_default_background (&pDock->backgroundBuffer, iWidth, iHeight);
	}
	else if (pDock->cBgImagePath != NULL)
	{
		cairo_dock_load_image_buffer (&pDock->backgroundBuffer, pDock->cBgImagePath, iWidth, iHeight, CAIRO_DOCK_FILL_SPACE);
	}
	if (pDock->backgroundBuffer.pSurface == NULL)
	{
		cairo_surface_t *pSurface = _cairo_dock_make_stripes_background (iWidth, iHeight, pDock->fBgColorBright, pDock->fBgColorDark, 0, 0., 90);
		cairo_dock_load_image_buffer_from_surface (&pDock->backgroundBuffer, pSurface, iWidth, iHeight);
	}
}

static gboolean _load_background_idle (CairoDock *pDock)
{
	cairo_dock_load_dock_background (pDock);
	
	pDock->iSidLoadBg = 0;
	return FALSE;
}
void cairo_dock_trigger_load_dock_background (CairoDock *pDock)
{
	if (pDock->iDecorationsWidth == pDock->backgroundBuffer.iWidth && pDock->iDecorationsHeight == pDock->backgroundBuffer.iHeight)  // mise a jour inutile.
		return;
	if (pDock->iSidLoadBg == 0)
		pDock->iSidLoadBg = g_idle_add ((GSourceFunc)_load_background_idle, pDock);
}


  //////////////////
 /// DESKTOP BG ///
//////////////////

static cairo_surface_t *_cairo_dock_create_surface_from_desktop_bg (void)  // attention : fonction lourde.
{
	//g_print ("+++ %s ()\n", __func__);
	Pixmap iRootPixmapID = cairo_dock_get_window_background_pixmap (cairo_dock_get_root_id ());
	g_return_val_if_fail (iRootPixmapID != 0, NULL);
	
	cairo_surface_t *pDesktopBgSurface = NULL;
	GdkPixbuf *pBgPixbuf = cairo_dock_get_pixbuf_from_pixmap (iRootPixmapID, FALSE);  // FALSE <=> on n'y ajoute pas de transparence.
	if (pBgPixbuf != NULL)
	{
		if (gdk_pixbuf_get_height (pBgPixbuf) == 1 && gdk_pixbuf_get_width (pBgPixbuf) == 1)  // couleur unie.
		{
			guchar *pixels = gdk_pixbuf_get_pixels (pBgPixbuf);
			cd_debug ("c'est une couleur unie (%.2f, %.2f, %.2f)", (double) pixels[0] / 255, (double) pixels[1] / 255, (double) pixels[2] / 255);
			
			pDesktopBgSurface = cairo_dock_create_blank_surface (
				g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL],
				g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
			
			cairo_t *pCairoContext = cairo_create (pDesktopBgSurface);
			cairo_set_source_rgb (pCairoContext,
				(double) pixels[0] / 255,
				(double) pixels[1] / 255,
				(double) pixels[2] / 255);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
			cairo_paint (pCairoContext);
			cairo_destroy (pCairoContext);
		}
		else
		{
			double fWidth, fHeight;
			cairo_surface_t *pBgSurface = cairo_dock_create_surface_from_pixbuf (pBgPixbuf,
				1,
				0,
				0,
				FALSE,
				&fWidth,
				&fHeight,
				NULL, NULL);
			
			if (fWidth < g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] || fHeight < g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL])
			{
				cd_debug ("c'est un degrade ou un motif (%dx%d)", (int) fWidth, (int) fHeight);
				pDesktopBgSurface = cairo_dock_create_blank_surface (
					g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL],
					g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
				cairo_t *pCairoContext = cairo_create (pDesktopBgSurface);
				
				cairo_pattern_t *pPattern = cairo_pattern_create_for_surface (pBgSurface);
				g_return_val_if_fail (cairo_pattern_status (pPattern) == CAIRO_STATUS_SUCCESS, NULL);
				cairo_pattern_set_extend (pPattern, CAIRO_EXTEND_REPEAT);
				
				cairo_set_source (pCairoContext, pPattern);
				cairo_paint (pCairoContext);
				
				cairo_destroy (pCairoContext);
				cairo_pattern_destroy (pPattern);
				cairo_surface_destroy (pBgSurface);
			}
			else
			{
				cd_debug ("c'est un fond d'ecran de taille %dx%d", (int) fWidth, (int) fHeight);
				pDesktopBgSurface = pBgSurface;
			}
		}
		
		g_object_unref (pBgPixbuf);
	}
	return pDesktopBgSurface;
}

CairoDockDesktopBackground *cairo_dock_get_desktop_background (gboolean bWithTextureToo)
{
	//g_print ("%s (%d, %d)\n", __func__, bWithTextureToo, s_pDesktopBg?s_pDesktopBg->iRefCount:-1);
	if (s_pDesktopBg == NULL)
	{
		s_pDesktopBg = g_new0 (CairoDockDesktopBackground, 1);
	}
	if (s_pDesktopBg->pSurface == NULL)
	{
		s_pDesktopBg->pSurface = _cairo_dock_create_surface_from_desktop_bg ();
	}
	if (s_pDesktopBg->iTexture == 0 && bWithTextureToo)
	{
		s_pDesktopBg->iTexture = cairo_dock_create_texture_from_surface (s_pDesktopBg->pSurface);
	}
	
	s_pDesktopBg->iRefCount ++;
	if (s_pDesktopBg->iSidDestroyBg != 0)
	{
		//g_print ("cancel pending destroy\n");
		g_source_remove (s_pDesktopBg->iSidDestroyBg);
		s_pDesktopBg->iSidDestroyBg = 0;
	}
	return s_pDesktopBg;
}

static gboolean _destroy_bg (CairoDockDesktopBackground *pDesktopBg)
{
	//g_print ("%s ()\n", __func__);
	g_return_val_if_fail (pDesktopBg != NULL, 0);
	if (pDesktopBg->pSurface != NULL)
	{
		cairo_surface_destroy (pDesktopBg->pSurface);
		pDesktopBg->pSurface = NULL;
		//g_print ("--- surface destroyed\n");
	}
	if (pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (pDesktopBg->iTexture);
		pDesktopBg->iTexture = 0;
	}
	pDesktopBg->iSidDestroyBg = 0;
	return FALSE;
}
void cairo_dock_destroy_desktop_background (CairoDockDesktopBackground *pDesktopBg)
{
	//g_print ("%s ()\n", __func__);
	g_return_if_fail (pDesktopBg != NULL);
	if (pDesktopBg->iRefCount > 0)
		pDesktopBg->iRefCount --;
	if (pDesktopBg->iRefCount == 0 && pDesktopBg->iSidDestroyBg == 0)
	{
		//g_print ("add pending destroy\n");
		pDesktopBg->iSidDestroyBg = g_timeout_add_seconds (3, (GSourceFunc)_destroy_bg, pDesktopBg);
	}
}

cairo_surface_t *cairo_dock_get_desktop_bg_surface (CairoDockDesktopBackground *pDesktopBg)
{
	g_return_val_if_fail (pDesktopBg != NULL, NULL);
	return pDesktopBg->pSurface;
}

GLuint cairo_dock_get_desktop_bg_texture (CairoDockDesktopBackground *pDesktopBg)
{
	g_return_val_if_fail (pDesktopBg != NULL, 0);
	return pDesktopBg->iTexture;
}

void cairo_dock_reload_desktop_background (void)
{
	//g_print ("%s ()\n", __func__);
	if (s_pDesktopBg == NULL)  // rien a recharger.
		return ;
	if (s_pDesktopBg->pSurface == NULL && s_pDesktopBg->iTexture == 0)  // rien a recharger.
		return ;
	
	if (s_pDesktopBg->pSurface != NULL)
	{
		cairo_surface_destroy (s_pDesktopBg->pSurface);
		//g_print ("--- surface destroyed\n");
	}
	s_pDesktopBg->pSurface = _cairo_dock_create_surface_from_desktop_bg ();
	
	if (s_pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (s_pDesktopBg->iTexture);
		s_pDesktopBg->iTexture = cairo_dock_create_texture_from_surface (s_pDesktopBg->pSurface);
	}
}


void cairo_dock_unload_additionnal_textures (void)
{
	cd_debug ("");
	if (s_pDesktopBg)  // on decharge le desktop-bg de force.
	{
		if (s_pDesktopBg->iSidDestroyBg != 0)
		{
			g_source_remove (s_pDesktopBg->iSidDestroyBg);
			s_pDesktopBg->iSidDestroyBg = 0;
		}
		s_pDesktopBg->iRefCount = 0;
		_destroy_bg (s_pDesktopBg);  // detruit ses ressources immediatement, mais pas le pointeur.
	}
	g_pFakeTransparencyDesktopBg = NULL;
	cairo_dock_unload_desklet_buttons ();
	cairo_dock_unload_dialog_buttons ();
	cairo_dock_unload_icon_textures ();
	cairo_dock_unload_indicator_textures ();
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
	if (s_pDesktopBg != NULL && s_pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (s_pDesktopBg->iTexture);
		s_pDesktopBg->iTexture = 0;
	}
	///cairo_dock_destroy_icon_pbuffer ();
	cairo_dock_destroy_icon_fbo ();
	cairo_dock_unload_default_data_renderer_font ();
	cairo_dock_unload_flying_container_textures ();
	cairo_dock_reset_source_context ();
}

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



#include "cairo-dock-draw.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-image-buffer.h"

extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentIconsPath;
extern gchar *g_cCurrentImagesPath;

extern gboolean g_bUseOpenGL;


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
	double w=0, h=0;
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
	
	if ((iLoadModifier & CAIRO_DOCK_ANIMATED_IMAGE) && h != 0)
	{
		g_print ("%dx%d\n", (int)w, (int)h);
		if (w >= 2*h)  // we need at least 2 frames (Note: we assume that frames are wide).
		{
			if ((int)w % (int)h == 0)  // w = k*h
			{
				pImage->iNbFrames = w / h;
			}
			else if (w > 2 * h)  // if we're pretty sure this image is an animated one, try to be smart, to handle the case of non-square frames.
			{
				// assume we have wide frames => w > h
				int w_ = h+1, h_ = h;
				do
				{
					g_print (" %d/%d\n", w_, (int)w);
					if ((int)w % w_ == 0)
					{
						pImage->iNbFrames = w / w_;
						break;
					}
					w_ ++;
				} while (w_ < w / 2);
			}
		}
		g_print ("CAIRO_DOCK_ANIMATED_IMAGE -> %d frames\n", pImage->iNbFrames);
	}
	
	if (fAlpha < 1 && pImage->pSurface != NULL)
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

void cairo_dock_load_image_buffer_from_texture (CairoDockImageBuffer *pImage, GLuint iTexture)
{
	pImage->iTexture = iTexture;
	pImage->iWidth = 1.;
	pImage->iHeight = 1.;
	pImage->fZoomX = 1.;
	pImage->fZoomY = 1.;
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

void cairo_dock_apply_image_buffer_surface_with_offset (CairoDockImageBuffer *pImage, cairo_t *pCairoContext, double x, double y, double fAlpha)
{
	if (cairo_dock_image_buffer_is_animated (pImage))
	{
		cairo_save (pCairoContext);
		cairo_translate (pCairoContext, x, y);
		cairo_rectangle (pCairoContext, 0, 0, pImage->iHeight, pImage->iHeight);
		cairo_clip (pCairoContext);
		cairo_set_source_surface (pCairoContext, pImage->pSurface, - pImage->iCurrentFrame * pImage->iHeight, 0.);
		cairo_paint_with_alpha (pCairoContext, fAlpha);
		cairo_restore (pCairoContext);
	}
	else
	{
		cairo_set_source_surface (pCairoContext, pImage->pSurface, 0., 0.);
		cairo_paint_with_alpha (pCairoContext, fAlpha);
	}
}

void cairo_dock_apply_image_buffer_texture_with_offset (CairoDockImageBuffer *pImage, double x, double y)
{
	glBindTexture (GL_TEXTURE_2D, pImage->iTexture);
	if (cairo_dock_image_buffer_is_animated (pImage))
	{
		int iFrameWidth = pImage->iWidth / pImage->iNbFrames;
		_cairo_dock_apply_current_texture_portion_at_size_with_offset ((double)pImage->iCurrentFrame / pImage->iNbFrames, 0,
			1. / pImage->iNbFrames, 1.,
			iFrameWidth, pImage->iHeight,
			x, y);
	}
	else
	{
		_cairo_dock_apply_current_texture_at_size_with_offset (pImage->iWidth, pImage->iHeight, x, y);
	}
}

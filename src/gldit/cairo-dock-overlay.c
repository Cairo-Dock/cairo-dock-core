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

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-log.h"
#include "cairo-dock-overlay.h"

extern gboolean g_bUseOpenGL;

static double f = .5;

struct _CairoOverlay
{
	CairoDockImageBuffer image;
	CairoOverlayPosition iPosition;
} ;
typedef struct _CairoOverlay CairoOverlay;


static void _add_overlay_to_icon (Icon *pIcon, CairoOverlay *pOverlay, CairoOverlayPosition iPosition)
{
	pOverlay->iPosition = iPosition;
	
	cairo_dock_remove_overlay_at_position (pIcon, iPosition);
	
	pIcon->pOverlays = g_list_prepend (pIcon->pOverlays, pOverlay);
}

static void _free_overlay (CairoOverlay *pOverlay)
{
	cairo_dock_unload_image_buffer (&pOverlay->image);
	g_free (pOverlay);
}

gboolean cairo_dock_add_overlay_from_image (Icon *pIcon, const gchar *cImageFile, CairoOverlayPosition iPosition)
{
	CairoOverlay *pOverlay = g_new0 (CairoOverlay, 1);
	g_print ("%s (%s)\n", __func__, cImageFile);
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
	cairo_dock_load_image_buffer (&pOverlay->image, cImageFile, iWidth * f, iHeight * f, 0);
	if (pOverlay->image.pSurface == NULL)
	{
		g_free (pOverlay);
		return FALSE;
	}
	
	_add_overlay_to_icon (pIcon, pOverlay, iPosition);
	return TRUE;
}

void cairo_dock_add_overlay_from_surface (Icon *pIcon, cairo_surface_t *pSurface, int iWidth, int iHeight, CairoOverlayPosition iPosition)
{
	CairoOverlay *pOverlay = g_new0 (CairoOverlay, 1);
	
	cairo_dock_load_image_buffer_from_surface (&pOverlay->image, pSurface, iWidth, iHeight);
	
	_add_overlay_to_icon (pIcon, pOverlay, iPosition);
}

void cairo_dock_remove_overlay_at_position (Icon *pIcon, CairoOverlayPosition iPosition)
{
	GList* ov;
	CairoOverlay *p;
	for (ov = pIcon->pOverlays; ov != NULL; ov = ov->next)
	{
		p = ov->data;
		if (p->iPosition == iPosition)
		{
			pIcon->pOverlays = g_list_delete_link (pIcon->pOverlays, ov);  // it's ok to do that since we break from the loop after that.
			_free_overlay (p);
			break;
		}
	}
}

CairoDockImageBuffer *cairo_dock_get_overlay_buffer_at_position (Icon *pIcon, CairoOverlayPosition iPosition)
{
	GList* ov;
	CairoOverlay *p;
	for (ov = pIcon->pOverlays; ov != NULL; ov = ov->next)
	{
		p = ov->data;
		if (p->iPosition == iPosition)
		{
			return &p->image;
		}
	}
	return NULL;
}

void cairo_dock_destroy_icon_overlays (Icon *pIcon)
{
	g_list_foreach (pIcon->pOverlays, (GFunc)_free_overlay, NULL);
	g_list_free (pIcon->pOverlays);
	pIcon->pOverlays = NULL;
}


static void _get_overlay_position (CairoOverlay *pOverlay, int w, int h, int *x, int *y)  // from the center of the icon.
{
	int wo = pOverlay->image.iWidth;
	int ho = pOverlay->image.iHeight;
	switch (pOverlay->iPosition)
	{
		case CAIRO_OVERLAY_LOWER_LEFT:
		default:
			*x = (-w  + wo) / 2;
			*y = (-h + ho) / 2;
		break;
		case CAIRO_OVERLAY_LOWER_RIGHT:
			*x = (w  - wo) / 2;
			*y = (-h + ho) / 2;
		break;
		case CAIRO_OVERLAY_BOTTOM:
			*x = 0;
			*y = (-h + ho) / 2;
		break;
		case CAIRO_OVERLAY_UPPER_LEFT:
			*x = (-w  + wo) / 2;
			*y = (h - ho) / 2;
		break;
		case CAIRO_OVERLAY_UPPER_RIGHT:
			*x = (w  - wo) / 2;
			*y = (h - ho) / 2;
		break;
		case CAIRO_OVERLAY_TOP:
			*x = 0;
			*y = (h - ho) / 2;
		break;
		case CAIRO_OVERLAY_RIGHT:
			*x = (w  - wo) / 2;
			*y = 0;
		break;
		case CAIRO_OVERLAY_LEFT:
			*x = (-w  + wo) / 2;
			*y = 0;
		break;
		case CAIRO_OVERLAY_MIDDLE:
			*x = 0.;
			*y = 0.;
		break;
	}
}

void cairo_dock_draw_icon_overlays_cairo (Icon *pIcon, double fRatio, cairo_t *pCairoContext)
{
	if (pIcon->pOverlays == NULL)
		return;
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	
	GList* ov;
	CairoOverlay *p;
	int x, y;  // relatively to the icon center.
	for (ov = pIcon->pOverlays; ov != NULL; ov = ov->next)
	{
		p = ov->data;
		if (! p->image.pSurface)
			continue;
		_get_overlay_position (p, w, h, &x, &y);
		cairo_save (pCairoContext);
		
		// translate to the middle of the icon.
		cairo_translate (pCairoContext,
			pIcon->fWidth * pIcon->fWidthFactor/ 2 * pIcon->fScale,
			pIcon->fHeight / 2 * pIcon->fScale);
		
		// scale like the icon
		cairo_scale (pCairoContext,
			fRatio * pIcon->fScale / (1 + myIconsParam.fAmplitude),
			fRatio * pIcon->fScale / (1 + myIconsParam.fAmplitude));
		
		// translate to the overlay top-left corner.
		cairo_translate (pCairoContext,
			x - p->image.iWidth / 2,
			- y - p->image.iHeight / 2);
		
		// draw.
		cairo_set_source_surface (pCairoContext,
			p->image.pSurface,
			0,
			0);
		if (pIcon->fAlpha == 1)
			cairo_paint (pCairoContext);
		else
			cairo_paint_with_alpha (pCairoContext, pIcon->fAlpha);
		
		cairo_restore (pCairoContext);
	}
}

void cairo_dock_draw_icon_overlays_opengl (Icon *pIcon, double fRatio)
{
	if (pIcon->pOverlays == NULL)
		return;
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	
	GList* ov;
	CairoOverlay *p;
	int x, y;  // relatively to the icon center.
	for (ov = pIcon->pOverlays; ov != NULL; ov = ov->next)
	{
		p = ov->data;
		if (! p->image.iTexture)
			continue;
		_get_overlay_position (p, w, h, &x, &y);
		glPushMatrix ();
		
		// scale/rotate like the icon
		glRotatef (-pIcon->fOrientation/G_PI*180., 0., 0., 1.);
		
		glScalef (fRatio * pIcon->fScale / (1 + myIconsParam.fAmplitude),
			fRatio * pIcon->fScale / (1 + myIconsParam.fAmplitude),
			1.);
		
		// translate to the overlay center.
		glTranslatef (x,
			y,
			0.);
		
		// draw.
		cairo_dock_draw_texture_with_alpha (p->image.iTexture,
			p->image.iWidth,
			p->image.iHeight,
			pIcon->fAlpha);
		glPopMatrix ();
	}
}



static void _print_overlay_on_icon (Icon *pIcon, CairoContainer *pContainer, CairoOverlay *pOverlay, CairoOverlayPosition iPosition)
{
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	
	int x, y;  // relatively to the icon center.
	pOverlay->iPosition = iPosition;
	_get_overlay_position (pOverlay, w, h, &x, &y);
	
	if (pIcon->iIconTexture != 0 && pOverlay->image.iTexture != 0)  // dessin opengl : on dessine sur la texture de l'icone avec le mecanisme habituel.
	{
		if (! cairo_dock_begin_draw_icon (pIcon, pContainer, 1))  // 1 = keep current drawing
			return ;
		
		_cairo_dock_enable_texture ();
		
		_cairo_dock_set_blend_alpha ();
		
		glBindTexture (GL_TEXTURE_2D, pOverlay->image.iTexture);
		_cairo_dock_apply_current_texture_at_size_with_offset (pOverlay->image.iWidth, pOverlay->image.iHeight, x, y);
		
		_cairo_dock_disable_texture ();
		
		cairo_dock_end_draw_icon (pIcon, pContainer);
	}
	else if (pIcon->pIconBuffer != NULL && pOverlay->image.pSurface != NULL)
	{
		cairo_t *pCairoContext = cairo_create (pIcon->pIconBuffer);
		g_return_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
		
		cairo_translate (pCairoContext,
			w/2 + x - pOverlay->image.iWidth / 2,
			h/2 - y - pOverlay->image.iHeight / 2);
		
		cairo_set_source_surface (pCairoContext,
			pOverlay->image.pSurface,
			0,
			0);
		cairo_paint (pCairoContext);
		
		cairo_destroy (pCairoContext);
	}
}

void cairo_dock_print_overlay_on_icon (Icon *pIcon, CairoContainer *pContainer, const gchar *cImageFile, CairoOverlayPosition iPosition)
{
	CairoOverlay *pOverlay = g_new0 (CairoOverlay, 1);
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
	cairo_dock_load_image_buffer (&pOverlay->image, cImageFile, iWidth * f, iHeight * f, 0);
	
	_print_overlay_on_icon (pIcon, pContainer, pOverlay, iPosition);
	
	_free_overlay (pOverlay);
}

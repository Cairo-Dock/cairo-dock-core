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
#include "cairo-dock-notifications.h"
#include "cairo-dock-overlay.h"

extern gboolean g_bUseOpenGL;

#define CD_DEFAULT_SCALE 0.5


  /////////////////////
 /// CREATE / FREE ///
/////////////////////

static inline CairoOverlay *_new_overlay (void)
{
	CairoOverlay *pOverlay = g_new0 (CairoOverlay, 1);
	pOverlay->object.ref = 1;
	cairo_dock_install_notifications_on_object (pOverlay, NB_NOTIFICATIONS_OBJECT);
	return pOverlay;
}

static CairoOverlay *cairo_dock_create_overlay_from_image (Icon *pIcon, const gchar *cImageFile)
{
	CairoOverlay *pOverlay = _new_overlay ();
	pOverlay->fScale = CD_DEFAULT_SCALE;
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
	cairo_dock_load_image_buffer (&pOverlay->image, cImageFile, iWidth * pOverlay->fScale, iHeight * pOverlay->fScale, 0);
	return pOverlay;
}

static CairoOverlay *cairo_dock_create_overlay_from_surface (Icon *pIcon, cairo_surface_t *pSurface, int iWidth, int iHeight)
{
	CairoOverlay *pOverlay = _new_overlay ();
	pOverlay->fScale = CD_DEFAULT_SCALE;
	
	cairo_dock_load_image_buffer_from_surface (&pOverlay->image, pSurface,
		iWidth > 0 ? iWidth : cairo_dock_icon_get_allocated_width (pIcon),
		iHeight > 0 ? iHeight : cairo_dock_icon_get_allocated_height (pIcon));  // we don't need the icon to be actually loaded to add an overlay on it.
	
	return pOverlay;
}

static CairoOverlay *cairo_dock_create_overlay_from_texture (GLuint iTexture, int iWidth, int iHeight)
{
	CairoOverlay *pOverlay = _new_overlay ();
	pOverlay->fScale = CD_DEFAULT_SCALE;
	
	cairo_dock_load_image_buffer_from_texture (&pOverlay->image, iTexture, iWidth, iHeight);  // size will be used to draw it if the scale is set to 0.
	
	return pOverlay;
}

static void cairo_dock_free_overlay (CairoOverlay *pOverlay)
{
	if (!pOverlay)
		return;
	cairo_dock_notify_on_object (pOverlay, NOTIFICATION_DESTROY, pOverlay);
	cairo_dock_unload_image_buffer (&pOverlay->image);
	g_free (pOverlay);
}


  ////////////////////
 /// ADD / REMOVE ///
////////////////////

static inline void cairo_dock_add_overlay_to_icon (Icon *pIcon, CairoOverlay *pOverlay, CairoOverlayPosition iPosition, gpointer data)
{
	if (! pOverlay)
		return;
	
	// complete the overlay
	pOverlay->iPosition = iPosition;
	pOverlay->data = data;
	pOverlay->pIcon = pIcon;  // overlays are stucked to their icon.
	
	// remove any overlay that matches the couple (position, data).
	if (data != NULL)
	{
		GList* ov = pIcon->pOverlays, *next_ov;
		CairoOverlay *p;
		while (ov)
		{
			p = ov->data;
			next_ov = ov->next;  // get the next element now, since this one may be deleted during this iteration.
			
			if (p->data == data && p->iPosition == iPosition)  // same position and same origin => remove
			{
				pIcon->pOverlays = g_list_delete_link (pIcon->pOverlays, ov);
				cairo_dock_free_overlay (p);
			}
			ov = next_ov;
		}
	}
	
	// add the new overlay to the icon
	pIcon->pOverlays = g_list_prepend (pIcon->pOverlays, pOverlay);
}

CairoOverlay *cairo_dock_add_overlay_from_image (Icon *pIcon, const gchar *cImageFile, CairoOverlayPosition iPosition, gpointer data)
{
	CairoOverlay *pOverlay = cairo_dock_create_overlay_from_image (pIcon, cImageFile);
	
	cairo_dock_add_overlay_to_icon (pIcon, pOverlay, iPosition, data);
	
	return pOverlay;
}

CairoOverlay *cairo_dock_add_overlay_from_surface (Icon *pIcon, cairo_surface_t *pSurface, int iWidth, int iHeight, CairoOverlayPosition iPosition, gpointer data)
{
	CairoOverlay *pOverlay = cairo_dock_create_overlay_from_surface (pIcon, pSurface, iWidth, iHeight);
	
	cairo_dock_add_overlay_to_icon (pIcon, pOverlay, iPosition, data);
	
	return pOverlay;
}

CairoOverlay *cairo_dock_add_overlay_from_texture (Icon *pIcon, GLuint iTexture, CairoOverlayPosition iPosition, gpointer data)
{
	CairoOverlay *pOverlay = cairo_dock_create_overlay_from_texture (iTexture, 0, 0);
	
	cairo_dock_add_overlay_to_icon (pIcon, pOverlay, iPosition, data);
	
	return pOverlay;
}


void cairo_dock_destroy_overlay (CairoOverlay *pOverlay)
{
	if (! pOverlay)
		return;
	
	// detach the overlay from the icon
	Icon *pIcon = pOverlay->pIcon;
	if (pIcon)
	{
		pIcon->pOverlays = g_list_remove (pIcon->pOverlays, pOverlay);
	}
	
	// free the overlay
	cairo_dock_free_overlay (pOverlay);
}


void cairo_dock_remove_overlay_at_position (Icon *pIcon, CairoOverlayPosition iPosition, gpointer data)
{
	if (! pIcon)
		return;
	g_return_if_fail (data != NULL);  // a NULL data can't be used to identify an overlay.
	
	GList* ov = pIcon->pOverlays, *next_ov;
	CairoOverlay *p;
	while (ov)
	{
		p = ov->data;
		next_ov = ov->next;  // get the next element now, since this one may be deleted during this iteration.
		
		if (p->data == data && p->iPosition == iPosition)  // same position and same origin => remove
		{
			pIcon->pOverlays = g_list_delete_link (pIcon->pOverlays, ov);
			cairo_dock_free_overlay (p);
		}
		ov = next_ov;
	}
}

  /////////////////////
 /// ICON OVERLAYS ///
/////////////////////

void cairo_dock_destroy_icon_overlays (Icon *pIcon)
{
	g_list_foreach (pIcon->pOverlays, (GFunc)cairo_dock_free_overlay, NULL);
	g_list_free (pIcon->pOverlays);
	pIcon->pOverlays = NULL;
}


/**static void _get_overlay_position_and_size (CairoOverlay *pOverlay, int w, int h, int *x, int *y, int *wo, int *ho)  // from the center of the icon.
{
	if (pOverlay->fScale > 0)
	{
		*wo = w * pOverlay->fScale;
		*ho = h * pOverlay->fScale;
	}
	else
	{
		*wo = pOverlay->image.iWidth;
		*ho = pOverlay->image.iHeight;
	}
	switch (pOverlay->iPosition)
	{
		case CAIRO_OVERLAY_LOWER_LEFT:
		default:
			*x = (-w  + *wo) / 2;
			*y = (-h + *ho) / 2;
		break;
		case CAIRO_OVERLAY_LOWER_RIGHT:
			*x = (w  - *wo) / 2;
			*y = (-h + *ho) / 2;
		break;
		case CAIRO_OVERLAY_BOTTOM:
			*x = 0;
			*y = (-h + *ho) / 2;
		break;
		case CAIRO_OVERLAY_UPPER_LEFT:
			*x = (-w  + *wo) / 2;
			*y = (h - *ho) / 2;
		break;
		case CAIRO_OVERLAY_UPPER_RIGHT:
			*x = (w  - *wo) / 2;
			*y = (h - *ho) / 2;
		break;
		case CAIRO_OVERLAY_TOP:
			*x = 0;
			*y = (h - *ho) / 2;
		break;
		case CAIRO_OVERLAY_RIGHT:
			*x = (w  - *wo) / 2;
			*y = 0;
		break;
		case CAIRO_OVERLAY_LEFT:
			*x = (-w  + *wo) / 2;
			*y = 0;
		break;
		case CAIRO_OVERLAY_MIDDLE:
			*x = 0.;
			*y = 0.;
		break;
	}
}*/
static void _get_overlay_position_and_size (CairoOverlay *pOverlay, int w, int h, double z, int *x, int *y, int *wo, int *ho)  // from the center of the icon.
{
	if (pOverlay->fScale > 0)
	{
		*wo = round (w * z * pOverlay->fScale);  // = pIcon->fWidth * pIcon->fScale
		*ho = round (h * z * pOverlay->fScale);
	}
	else
	{
		*wo = pOverlay->image.iWidth * z;
		*ho = pOverlay->image.iHeight * z;
	}
	switch (pOverlay->iPosition)
	{
		case CAIRO_OVERLAY_LOWER_LEFT:
		default:
			*x = (-w*z  + *wo) / 2;
			*y = (-h*z + *ho) / 2;
		break;
		case CAIRO_OVERLAY_LOWER_RIGHT:
			*x = (w*z  - *wo) / 2;
			*y = (-h*z + *ho) / 2;
		break;
		case CAIRO_OVERLAY_BOTTOM:
			*x = 0;
			*y = (-h*z + *ho) / 2;
		break;
		case CAIRO_OVERLAY_UPPER_LEFT:
			*x = (-w*z  + *wo) / 2;
			*y = (h*z - *ho) / 2;
		break;
		case CAIRO_OVERLAY_UPPER_RIGHT:
			*x = (w*z  - *wo) / 2;
			*y = (h*z - *ho) / 2;
		break;
		case CAIRO_OVERLAY_TOP:
			*x = 0;
			*y = (h*z - *ho) / 2;
		break;
		case CAIRO_OVERLAY_RIGHT:
			*x = (w*z  - *wo) / 2;
			*y = 0;
		break;
		case CAIRO_OVERLAY_LEFT:
			*x = (-w*z  + *wo) / 2;
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
	double fMaxScale = cairo_dock_get_icon_max_scale (pIcon);
	double z = fRatio * pIcon->fScale / fMaxScale;
	
	GList* ov;
	CairoOverlay *p;
	int wo, ho;  // actual size at which the overlay will rendered.
	int x, y;  // position of the overlay relatively to the icon center.
	double dx, dy;  // offset to place the surface on the integer grid to avoid blur.
	for (ov = pIcon->pOverlays; ov != NULL; ov = ov->next)
	{
		p = ov->data;
		if (! p->image.pSurface)
			continue;
		/**_get_overlay_position_and_size (p, w, h, &x, &y, &wo, &ho);
		cairo_save (pCairoContext);
		
		// translate to the middle of the icon.
		cairo_translate (pCairoContext,
			pIcon->fWidth / 2 * pIcon->fScale,
			pIcon->fHeight / 2 * pIcon->fScale);
		
		// scale like the icon
		cairo_scale (pCairoContext,
			fRatio * pIcon->fScale / fMaxScale,
			fRatio * pIcon->fScale / fMaxScale);
		
		// translate to the overlay top-left corner.
		cairo_translate (pCairoContext,
			x - wo/2 - 1,  /// -1 to compensate the round errors; TODO: use double
			- y - ho/2. + 1);
		
		// draw.
		cairo_scale (pCairoContext,
			(double) wo / p->image.iWidth,
			(double) ho / p->image.iHeight);*/
		
		_get_overlay_position_and_size (p, w, h, z, &x, &y, &wo, &ho);
		dx = (wo & 1) * .5;
		dy = (ho & 1) * .5;
		cairo_save (pCairoContext);
		
		// translate to the top-left corner of the overlay.
		cairo_translate (pCairoContext,
			floor (pIcon->fWidth / 2 * pIcon->fScale + x - wo/2) + dx,
			floor (pIcon->fHeight / 2 * pIcon->fScale - y - ho/2) + dy);
		
		// draw.
		cairo_scale (pCairoContext,
			(double) wo / p->image.iWidth,
			(double) ho / p->image.iHeight);
		cairo_dock_apply_image_buffer_surface_with_offset (&p->image, pCairoContext, 0., 0., pIcon->fAlpha);
		
		cairo_restore (pCairoContext);
	}
}

void cairo_dock_draw_icon_overlays_opengl (Icon *pIcon, double fRatio)
{
	if (pIcon->pOverlays == NULL)
		return;
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	double fMaxScale = cairo_dock_get_icon_max_scale (pIcon);
	double z = fRatio * pIcon->fScale / fMaxScale;
	
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_over ();
	_cairo_dock_set_alpha (pIcon->fAlpha);
	
	GList* ov;
	CairoOverlay *p;
	int wo, ho;  // actual size at which the overlay will rendered.
	int x, y;  // position of the overlay relatively to the icon center.
	double dx, dy;  // offset to place the surface on the integer grid to avoid blur.
	for (ov = pIcon->pOverlays; ov != NULL; ov = ov->next)
	{
		p = ov->data;
		if (! p->image.iTexture)
			continue;
		glPushMatrix ();
		/**_get_overlay_position_and_size (p, w, h, &x, &y, &wo, &ho);
		
		// scale/rotate like the icon
		glRotatef (-pIcon->fOrientation/G_PI*180., 0., 0., 1.);
		
		glScalef (fRatio * pIcon->fScale / fMaxScale,
			fRatio * pIcon->fScale / fMaxScale,
			1.);
		
		// translate to the overlay center.
		glTranslatef (x,
			y,
			0.);
		
		// draw.
		glScalef ((double)wo / p->image.iWidth,
			(double)ho / p->image.iHeight,
			1.);
		cairo_dock_apply_image_buffer_texture (&p->image);*/
		_get_overlay_position_and_size (p, w, h, z, &x, &y, &wo, &ho);
		dx = (wo & 1) * .5;
		dy = (ho & 1) * .5;
		
		// translate to the overlay center.
		glRotatef (-pIcon->fOrientation/G_PI*180., 0., 0., 1.);
		glTranslatef (x + dx,
			y + dy,
			0.);
		
		// draw.
		_cairo_dock_apply_texture_at_size (p->image.iTexture, wo, ho);
		
		glPopMatrix ();
	}
	_cairo_dock_disable_texture ();
}


  /////////////
 /// PRINT ///
/////////////

static void cairo_dock_print_overlay_on_icon (Icon *pIcon, CairoContainer *pContainer, CairoOverlay *pOverlay, CairoOverlayPosition iPosition)
{
	if (! pOverlay)
		return;
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	//g_print ("%s (%dx%d, %d, %p)\n", __func__, w, h, iPosition, pContainer);
	
	int x, y;  // relatively to the icon center.
	int wo, ho;  // overlay size
	pOverlay->iPosition = iPosition;
	_get_overlay_position_and_size (pOverlay, w, h, 1, &x, &y, &wo, &ho);
	
	if (pIcon->image.iTexture != 0 && pOverlay->image.iTexture != 0)  // dessin opengl : on dessine sur la texture de l'icone avec le mecanisme habituel.
	{
		/// TODO: handle the case where the drawing is not yet possible (container not yet sized).
		if (! cairo_dock_begin_draw_icon (pIcon, pContainer, 1))  // 1 = keep current drawing
			return ;
		
		glPushMatrix ();
		_cairo_dock_enable_texture ();
		
		_cairo_dock_set_blend_alpha ();
		///glBindTexture (GL_TEXTURE_2D, pOverlay->image.iTexture);
		///_cairo_dock_apply_current_texture_at_size_with_offset (wo, ho, x, y);
		glTranslatef (x, y, 0);
		glScalef ((double)wo / pOverlay->image.iWidth,
			(double)ho / pOverlay->image.iHeight,
			1.);
		cairo_dock_apply_image_buffer_texture (&pOverlay->image);
		
		_cairo_dock_disable_texture ();
		glPopMatrix ();
		
		cairo_dock_end_draw_icon (pIcon, pContainer);
	}
	else if (pIcon->image.pSurface != NULL && pOverlay->image.pSurface != NULL)
	{
		cairo_t *pCairoContext = cairo_create (pIcon->image.pSurface);
		g_return_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
		
		cairo_translate (pCairoContext,
			w/2 + x - wo/2,
			h/2 - y - ho/2);
		
		cairo_scale (pCairoContext,
			(double) wo / pOverlay->image.iWidth,
			(double) ho / pOverlay->image.iHeight);
		
		cairo_dock_apply_image_buffer_surface_with_offset (&pOverlay->image, pCairoContext, 0., 0., 1);
		cairo_paint (pCairoContext);
		
		cairo_destroy (pCairoContext);
	}
}

gboolean cairo_dock_print_overlay_on_icon_from_image (Icon *pIcon, CairoContainer *pContainer, const gchar *cImageFile, CairoOverlayPosition iPosition)
{
	CairoOverlay *pOverlay = cairo_dock_create_overlay_from_image (pIcon, cImageFile);
	if (! pOverlay)
		return FALSE;
	
	cairo_dock_print_overlay_on_icon (pIcon, pContainer, pOverlay, iPosition);
	
	cairo_dock_free_overlay (pOverlay);
	return TRUE;
}

void cairo_dock_print_overlay_on_icon_from_surface (Icon *pIcon, CairoContainer *pContainer, cairo_surface_t *pSurface, int iWidth, int iHeight, CairoOverlayPosition iPosition)
{
	CairoOverlay *pOverlay = cairo_dock_create_overlay_from_surface (pIcon, pSurface, iWidth, iHeight);
	
	cairo_dock_print_overlay_on_icon (pIcon, pContainer, pOverlay, iPosition);
	
	pOverlay->image.pSurface = NULL;  // we don't want to free the surface.
	cairo_dock_free_overlay (pOverlay);
}

void cairo_dock_print_overlay_on_icon_from_texture (Icon *pIcon, CairoContainer *pContainer, GLuint iTexture, CairoOverlayPosition iPosition)
{
	CairoOverlay *pOverlay = cairo_dock_create_overlay_from_texture (iTexture, 0, 0);
	
	cairo_dock_print_overlay_on_icon (pIcon, pContainer, pOverlay, iPosition);
	
	pOverlay->image.iTexture = 0;  // we don't want to free the texture.
	cairo_dock_free_overlay (pOverlay);
}

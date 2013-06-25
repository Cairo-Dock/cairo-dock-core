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
#define _MANAGER_DEF_
#include "cairo-dock-overlay.h"

// public (manager, config, data)
GldiObjectManager myOverlayObjectMgr;

// dependancies
extern gboolean g_bUseOpenGL;

// private
#define CD_DEFAULT_SCALE 0.5


CairoOverlay *gldi_overlay_new (CairoOverlayAttr *attr)
{
	return (CairoOverlay*)gldi_object_new (&myOverlayObjectMgr, attr);
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
				gldi_object_unref (GLDI_OBJECT(p));
			}
			ov = next_ov;
		}
	}
	
	// add the new overlay to the icon
	pIcon->pOverlays = g_list_prepend (pIcon->pOverlays, pOverlay);
}

CairoOverlay *cairo_dock_add_overlay_from_image (Icon *pIcon, const gchar *cImageFile, CairoOverlayPosition iPosition, gpointer data)
{
	CairoOverlayAttr attr;
	memset (&attr, 0, sizeof (CairoOverlayAttr));
	attr.iPosition = iPosition;
	attr.pIcon = pIcon;
	attr.data = data;
	attr.cImageFile = cImageFile;
	return gldi_overlay_new (&attr);
}

CairoOverlay *cairo_dock_add_overlay_from_surface (Icon *pIcon, cairo_surface_t *pSurface, int iWidth, int iHeight, CairoOverlayPosition iPosition, gpointer data)
{
	CairoOverlayAttr attr;
	memset (&attr, 0, sizeof (CairoOverlayAttr));
	attr.iPosition = iPosition;
	attr.pIcon = pIcon;
	attr.data = data;
	attr.pSurface = pSurface;
	attr.iWidth = iWidth;
	attr.iHeight = iHeight;
	return gldi_overlay_new (&attr);
}

CairoOverlay *cairo_dock_add_overlay_from_texture (Icon *pIcon, GLuint iTexture, CairoOverlayPosition iPosition, gpointer data)
{
	CairoOverlayAttr attr;
	memset (&attr, 0, sizeof (CairoOverlayAttr));
	attr.iPosition = iPosition;
	attr.pIcon = pIcon;
	attr.data = data;
	attr.iTexture = iTexture;
	return gldi_overlay_new (&attr);
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
			gldi_object_unref (GLDI_OBJECT(p));  // will remove it from the list
		}
		ov = next_ov;
	}
}

  /////////////////////
 /// ICON OVERLAYS ///
/////////////////////

void cairo_dock_destroy_icon_overlays (Icon *pIcon)
{
	GList *pOverlays = pIcon->pOverlays;
	pIcon->pOverlays = NULL;  // nullify the list to avoid unnecessary roundtrips.
	g_list_foreach (pOverlays, (GFunc)gldi_object_unref, NULL);
	g_list_free (pOverlays);
}


static void _get_overlay_position_and_size (CairoOverlay *pOverlay, int w, int h, double z, double *x, double *y, int *wo, int *ho)  // from the center of the icon.
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
	double x, y;  // position of the overlay relatively to the icon center.
	for (ov = pIcon->pOverlays; ov != NULL; ov = ov->next)
	{
		p = ov->data;
		if (! p->image.pSurface)
			continue;
		
		_get_overlay_position_and_size (p, w, h, z, &x, &y, &wo, &ho);
		x += (pIcon->fWidth * pIcon->fScale - wo) / 2.;
		y = - y + (pIcon->fHeight * pIcon->fScale - ho) / 2.;
		if (pIcon->fScale == 1)  // place the overlay on the grid to avoid scale blur (only when the icon is at rest, otherwise it makes the movement jerky).
		{
			if (wo & 1)
				x = floor (x) + .5;
			else
				x = round (x);
			if (ho & 1)
				y = floor (y) + .5;
			else
				y = round (y);
		}
		cairo_save (pCairoContext);
		
		// translate to the top-left corner of the overlay.
		cairo_translate (pCairoContext, x, y);
		
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
	int wo, ho;  // actual size at which the overlay will be rendered.
	double x, y;  // position of the overlay relatively to the icon center.
	for (ov = pIcon->pOverlays; ov != NULL; ov = ov->next)
	{
		p = ov->data;
		if (! p->image.iTexture)
			continue;
		glPushMatrix ();
		
		_get_overlay_position_and_size (p, w, h, z, &x, &y, &wo, &ho);
		if (pIcon->fScale == 1)  // place the overlay on the grid to avoid scale blur (only when the icon is at rest, otherwise it makes the movement jerky).
		{
			if (wo & 1)
				x = floor (x) + .5;
			else
				x = round (x);
			if (ho & 1)
				y = floor (y) + .5;
			else
				y = round (y);
		}
		
		// translate to the overlay center.
		glRotatef (-pIcon->fOrientation/G_PI*180., 0., 0., 1.);
		glTranslatef (x, y, 0.);
		
		// draw.
		_cairo_dock_apply_texture_at_size (p->image.iTexture, wo, ho);
		
		glPopMatrix ();
	}
	_cairo_dock_disable_texture ();
}


  /////////////
 /// PRINT ///
/////////////

static void cairo_dock_print_overlay_on_icon (Icon *pIcon, CairoOverlay *pOverlay, CairoOverlayPosition iPosition)
{
	if (! pOverlay)
		return;
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	//g_print ("%s (%dx%d, %d, %p)\n", __func__, w, h, iPosition, pContainer);
	
	double x, y;  // relatively to the icon center.
	int wo, ho;  // overlay size
	pOverlay->iPosition = iPosition;
	_get_overlay_position_and_size (pOverlay, w, h, 1, &x, &y, &wo, &ho);
	
	if (pIcon->image.iTexture != 0 && pOverlay->image.iTexture != 0)  // dessin opengl : on dessine sur la texture de l'icone avec le mecanisme habituel.
	{
		/// TODO: handle the case where the drawing is not yet possible (container not yet sized).
		if (! cairo_dock_begin_draw_icon (pIcon, 1))  // 1 = keep current drawing
			return ;
		
		glPushMatrix ();
		_cairo_dock_enable_texture ();
		
		_cairo_dock_set_blend_pbuffer ();
		///glBindTexture (GL_TEXTURE_2D, pOverlay->image.iTexture);
		///_cairo_dock_apply_current_texture_at_size_with_offset (wo, ho, x, y);
		glTranslatef (x, y, 0);
		glScalef ((double)wo / pOverlay->image.iWidth,
			(double)ho / pOverlay->image.iHeight,
			1.);
		cairo_dock_apply_image_buffer_texture (&pOverlay->image);
		
		_cairo_dock_disable_texture ();
		glPopMatrix ();
		
		cairo_dock_end_draw_icon (pIcon);
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

gboolean cairo_dock_print_overlay_on_icon_from_image (Icon *pIcon, const gchar *cImageFile, CairoOverlayPosition iPosition)
{
	CairoOverlay *pOverlay = cairo_dock_add_overlay_from_image (pIcon, cImageFile, iPosition, NULL);
	if (! pOverlay)
		return FALSE;
	
	cairo_dock_print_overlay_on_icon (pIcon, pOverlay, iPosition);
	
	gldi_object_unref (GLDI_OBJECT(pOverlay));
	return TRUE;
}

void cairo_dock_print_overlay_on_icon_from_surface (Icon *pIcon, cairo_surface_t *pSurface, int iWidth, int iHeight, CairoOverlayPosition iPosition)
{
	CairoOverlay *pOverlay = cairo_dock_add_overlay_from_surface (pIcon, pSurface, iWidth, iHeight, iPosition, NULL);
	
	cairo_dock_print_overlay_on_icon (pIcon, pOverlay, iPosition);
	
	pOverlay->image.pSurface = NULL;  // we don't want to free the surface.
	gldi_object_unref (GLDI_OBJECT(pOverlay));
}

void cairo_dock_print_overlay_on_icon_from_texture (Icon *pIcon, GLuint iTexture, CairoOverlayPosition iPosition)
{
	CairoOverlay *pOverlay = cairo_dock_add_overlay_from_texture (pIcon, iTexture, iPosition, NULL);  // we don't need the exact size of the texture as long as the scale is not 0, since then we'll simply draw the texture at the size of the icon * scale
	
	cairo_dock_print_overlay_on_icon (pIcon, pOverlay, iPosition);
	
	pOverlay->image.iTexture = 0;  // we don't want to free the texture.
	gldi_object_unref (GLDI_OBJECT(pOverlay));
}

  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	CairoOverlay *pOverlay = (CairoOverlay*)obj;
	CairoOverlayAttr *cattr = (CairoOverlayAttr*)attr;
	g_return_if_fail (cattr->pIcon != NULL);
	
	pOverlay->fScale = CD_DEFAULT_SCALE;
	pOverlay->iPosition = cattr->iPosition;
	
	if (cattr->cImageFile != NULL)
	{
		int iWidth, iHeight;
		cairo_dock_get_icon_extent (cattr->pIcon, &iWidth, &iHeight);
		cairo_dock_load_image_buffer (&pOverlay->image, cattr->cImageFile, iWidth * pOverlay->fScale, iHeight * pOverlay->fScale, 0);
	}
	else if (cattr->pSurface != NULL)
	{
		cairo_dock_load_image_buffer_from_surface (&pOverlay->image, cattr->pSurface,
			cattr->iWidth > 0 ? cattr->iWidth : cairo_dock_icon_get_allocated_width (cattr->pIcon),
			cattr->iHeight > 0 ? cattr->iHeight : cairo_dock_icon_get_allocated_height (cattr->pIcon));  // we don't need the icon to be actually loaded to add an overlay on it.
	}
	else if (cattr->iTexture != 0)
	{
		cairo_dock_load_image_buffer_from_texture (&pOverlay->image, cattr->iTexture, 1, 1);  // size will be used to draw it if the scale is set to 0.
	}
	
	if (cattr->data != NULL)
	{
		cairo_dock_add_overlay_to_icon (cattr->pIcon, pOverlay, cattr->iPosition, cattr->data);
	}
}

static void reset_object (GldiObject *obj)
{
	CairoOverlay *pOverlay = (CairoOverlay*)obj;
	
	// detach the overlay from the icon
	Icon *pIcon = pOverlay->pIcon;
	if (pIcon)
	{
		pIcon->pOverlays = g_list_remove (pIcon->pOverlays, pOverlay);
	}
	
	// free data
	cairo_dock_unload_image_buffer (&pOverlay->image);
}

void gldi_register_overlays_manager (void)
{
	// Object Manager
	memset (&myOverlayObjectMgr, 0, sizeof (GldiObjectManager));
	myOverlayObjectMgr.cName        = "Overlay";
	myOverlayObjectMgr.iObjectSize  = sizeof (CairoOverlay);
	// interface
	myOverlayObjectMgr.init_object  = init_object;
	myOverlayObjectMgr.reset_object = reset_object;
	// signals
	gldi_object_install_notifications (&myOverlayObjectMgr, NB_NOTIFICATIONS_OVERLAYS);
}

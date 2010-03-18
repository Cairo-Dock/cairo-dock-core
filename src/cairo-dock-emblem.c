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

#include "cairo-dock-icons.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-load.h"
#include "cairo-dock-log.h"
#include "cairo-dock-emblem.h"

extern CairoDockImageBuffer g_pIconBackgroundBuffer;
extern CairoDockImageBuffer g_pBoxAboveBuffer;
extern CairoDockImageBuffer g_pBoxBelowBuffer;
extern gboolean g_bUseOpenGL;

static double a = .5;

//merci a Necropotame et ChAnGFu !

CairoEmblem *cairo_dock_make_emblem (const gchar *cImageFile, Icon *pIcon, CairoContainer *pContainer, cairo_t *pSourceContext)
{
	CairoEmblem *pEmblem = g_new0 (CairoEmblem, 1);
	pEmblem->fScale = a;
	
	//\___________ On calcule les dimensions de l'embleme.
	int w, h;
	cairo_dock_get_icon_extent (pIcon, pContainer, &w, &h);
	pEmblem->iWidth = a * w;
	pEmblem->iHeight = a * h;
	
	//\___________ On cree la surface/texture a cette taille.
	cairo_surface_t *pEmblemSurface = cairo_dock_create_surface_from_image_simple (cImageFile, pSourceContext, pEmblem->iWidth, pEmblem->iHeight);
	
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer) && pEmblemSurface)
	{
		pEmblem->iTexture = cairo_dock_create_texture_from_surface (pEmblemSurface);
		cairo_surface_destroy (pEmblemSurface);
	}
	else
		pEmblem->pSurface = pEmblemSurface;
	
	return pEmblem;
}

CairoEmblem *cairo_dock_make_emblem_from_surface (cairo_surface_t *pSurface, int iSurfaceWidth, int iSurfaceHeight, Icon *pIcon, CairoContainer *pContainer)
{
	CairoEmblem *pEmblem = g_new0 (CairoEmblem, 1);
	pEmblem->fScale = a;
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, pContainer, &w, &h);
	pEmblem->iWidth = (iSurfaceWidth > 0 ? iSurfaceWidth : w);
	pEmblem->iHeight = (iSurfaceHeight > 0 ? iSurfaceHeight : h);
	pEmblem->pSurface = pSurface;
	return pEmblem;
}

CairoEmblem *cairo_dock_make_emblem_from_texture (GLuint iTexture, Icon *pIcon, CairoContainer *pContainer)
{
	CairoEmblem *pEmblem = g_new0 (CairoEmblem, 1);
	pEmblem->fScale = a;
	
	pEmblem->iTexture = iTexture;  // inutile de connaitre la taille de la texture.
	return pEmblem;
}


void cairo_dock_free_emblem (CairoEmblem *pEmblem)
{
	if (pEmblem == NULL)
		return;
	
	if (pEmblem->pSurface != NULL)
		cairo_surface_destroy (pEmblem->pSurface);
	if (pEmblem->iTexture != 0)
		_cairo_dock_delete_texture (pEmblem->iTexture);
	g_free (pEmblem);
}


void _cairo_dock_apply_emblem_texture (CairoEmblem *pEmblem, int w, int h)
{
	double a = pEmblem->fScale;
	double x, y;
	switch (pEmblem->iPosition)
	{
		case CAIRO_DOCK_EMBLEM_UPPER_RIGHT:
			x = w/2 * (1 - a);
			y = h/2 * (1 - a);
		break;
		case CAIRO_DOCK_EMBLEM_LOWER_RIGHT:
			x = w/2 * (1 - a);
			y = -h/2 * (1 - a);
		break;
		case CAIRO_DOCK_EMBLEM_UPPER_LEFT:
			x = -(double)w/2 * (1 - a);
			y = (double)h/2 * (1 - a);
		break;
		case CAIRO_DOCK_EMBLEM_LOWER_LEFT:
		default:
			x = -w/2 * (1 - a);
			y = -h/2 * (1 - a);
		break;
		case CAIRO_DOCK_EMBLEM_MIDDLE:
			x = 0.;
			y = 0.;
		break;
	}
	glBindTexture (GL_TEXTURE_2D, pEmblem->iTexture);
	_cairo_dock_apply_current_texture_at_size_with_offset (a*w, a*h, x, y);
}

void _cairo_dock_apply_emblem_surface (CairoEmblem *pEmblem, int w, int h, cairo_t *pCairoContext)
{
	double a = pEmblem->fScale;
	double zx = (double) a*w / pEmblem->iWidth;
	double zy = (double) a*h / pEmblem->iHeight;
	cairo_scale (pCairoContext, zx, zy);
	
	double x, y;
	switch (pEmblem->iPosition)
	{
		case CAIRO_DOCK_EMBLEM_UPPER_RIGHT:
			x = w * (1 - a);
			y = 0.;
		break;
		case CAIRO_DOCK_EMBLEM_LOWER_RIGHT:
			x = w * (1 - a);
			y = h * (1 - a);
		break;
		case CAIRO_DOCK_EMBLEM_UPPER_LEFT:
			x = 0.;
			y = 0.;
		break;
		case CAIRO_DOCK_EMBLEM_LOWER_LEFT:
		default:
			x = 0.;
			y = h * (1 - a);
		break;
		case CAIRO_DOCK_EMBLEM_MIDDLE:
			x = w/2 * (1 - a);
			y = h/2 * (1 - a);
		break;
	}
	cairo_set_source_surface (pCairoContext, pEmblem->pSurface, x/zx, y/zy);
	cairo_paint (pCairoContext);
}
void cairo_dock_draw_emblem_on_icon (CairoEmblem *pEmblem, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (pEmblem != NULL);
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, pContainer, &w, &h);
	
	if (pIcon->iIconTexture != 0 && pEmblem->iTexture != 0)  // dessin opengl : on dessine sur la texture de l'icone avec le mecanisme habituel.
	{
		if (! cairo_dock_begin_draw_icon (pIcon, pContainer, 1))
			return ;
		
		_cairo_dock_enable_texture ();
		
		_cairo_dock_set_blend_source ();
		
		_cairo_dock_apply_texture_at_size (pIcon->iIconTexture, w, h);
		
		_cairo_dock_set_blend_alpha ();
		
		_cairo_dock_apply_emblem_texture (pEmblem, w, h);
		
		_cairo_dock_disable_texture ();
		
		cairo_dock_end_draw_icon (pIcon, pContainer);
	}
	else if (pIcon->pIconBuffer != NULL && pEmblem->pSurface != NULL)
	{
		cairo_t *pCairoContext = cairo_create (pIcon->pIconBuffer);
		g_return_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
		
		_cairo_dock_apply_emblem_surface (pEmblem, w, h, pCairoContext);
		
		cairo_paint (pCairoContext);
		
		cairo_destroy (pCairoContext);
	}
}

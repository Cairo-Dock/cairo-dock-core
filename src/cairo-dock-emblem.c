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


static void _cairo_dock_apply_emblem_texture (CairoEmblem *pEmblem, int w, int h)
{
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
static void _cairo_dock_apply_emblem_surface (CairoEmblem *pEmblem, int w, int h, cairo_t *pCairoContext)
{
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
		if (! cairo_dock_begin_draw_icon (pIcon, pContainer))
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


static void _cairo_dock_draw_subdock_content_as_emblem (Icon *pIcon, CairoDock *pDock, int w, int h, cairo_t *pCairoContext)
{
	//\______________ On dessine les 4 premieres icones du sous-dock en embleme.
	CairoEmblem e;
	memset (&e, 0, sizeof (CairoEmblem));
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



static void _cairo_dock_draw_subdock_content_as_stack (Icon *pIcon, CairoDock *pDock, int w, int h, cairo_t *pCairoContext)
{
	//\______________ On dessine les 4 premieres icones du sous-dock en pile.
	CairoEmblem e;
	memset (&e, 0, sizeof (CairoEmblem));
	double a_ = a;
	a = .8;
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
	a = a_;
}


static void _cairo_dock_draw_subdock_content_as_box (Icon *pIcon, CairoDock *pDock, int w, int h, cairo_t *pCairoContext)
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
		if (! cairo_dock_begin_draw_icon (pIcon, CAIRO_CONTAINER (pDock)))
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
		_cairo_dock_draw_subdock_content_as_stack (pIcon, pDock, w, h, pCairoContext);
	else
	{
		switch (pIcon->iSubdockViewType)
		{
			case 1 :
				_cairo_dock_draw_subdock_content_as_emblem (pIcon, pDock, w, h, pCairoContext);
			break;
			case 2:
				_cairo_dock_draw_subdock_content_as_stack (pIcon, pDock, w, h, pCairoContext);
			break;
			case 3:
				_cairo_dock_draw_subdock_content_as_box (pIcon, pDock, w, h, pCairoContext);
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

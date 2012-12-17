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


#include "gldi-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-icon-container.h"

CairoDockImageBuffer g_pBoxAboveBuffer;
CairoDockImageBuffer g_pBoxBelowBuffer;

extern CairoDock *g_pMainDock;

extern gboolean g_bUseOpenGL;


static void _cairo_dock_draw_subdock_content_as_emblem (Icon *pIcon, G_GNUC_UNUSED CairoContainer *pContainer, int w, int h, cairo_t *pCairoContext)
{
	//\______________ On dessine les 4 premieres icones du sous-dock en embleme.
	int wi, hi;
	int i;
	Icon *icon;
	GList *ic;
	for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 4; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) || icon->image.pSurface == NULL)
			continue;
		
		cairo_dock_get_icon_extent (icon, &wi, &hi);
		// we could use cairo_dock_print_overlay_on_icon_from_surface (pIcon, pContainer, icon->image.pSurface, wi, hi, i), but it's slightly optimized to draw it ourselves.
		
		cairo_save (pCairoContext);
		cairo_translate (pCairoContext, (i&1) * w/2, (i/2) * h/2);
		
		cairo_scale (pCairoContext, .5 * w / wi, .5 * h / hi);
		cairo_set_source_surface (pCairoContext, icon->image.pSurface, 0, 0);
		cairo_paint (pCairoContext);
		
		cairo_restore (pCairoContext);
		
		i ++;
	}
}

static void _cairo_dock_draw_subdock_content_as_emblem_opengl (Icon *pIcon, G_GNUC_UNUSED CairoContainer *pContainer, int w, int h)
{
	//\______________ On dessine les 4 premieres icones du sous-dock en embleme.
	int i;
	Icon *icon;
	GList *ic;
	for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 4; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) || icon->image.iTexture == 0)
			continue;
		
		glBindTexture (GL_TEXTURE_2D, icon->image.iTexture);
		_cairo_dock_apply_current_texture_at_size_with_offset (w/2, h/2, ((i&1)-.5)*w/2, (.5-(i/2))*h/2);
		
		i ++;
	}
}

static void _cairo_dock_draw_subdock_content_as_stack (Icon *pIcon, G_GNUC_UNUSED CairoContainer *pContainer, int w, int h, cairo_t *pCairoContext)
{
	//\______________ On dessine les 4 premieres icones du sous-dock en pile.
	int wi, hi;
	int i, k=0;
	Icon *icon;
	GList *ic;
	for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 3; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) || !icon->image.pSurface)
			continue;
		
		switch (i)
		{
			case 0:
				k = 0;
			break;
			case 1:
				if (ic->next == NULL)
					k = 2;
				else
					k = 1;
			break;
			case 2:
				k = 2;
			break;
			default : break;
		}
		
		cairo_dock_get_icon_extent (icon, &wi, &hi);
		
		cairo_save (pCairoContext);
		cairo_translate (pCairoContext, k * w / 10, k * h / 10);
		
		cairo_scale (pCairoContext, .8 * w / wi, .8 * h / hi);
		cairo_set_source_surface (pCairoContext, icon->image.pSurface, 0, 0);
		cairo_paint (pCairoContext);
		
		cairo_restore (pCairoContext);
		
		i ++;
	}
}

static void _cairo_dock_draw_subdock_content_as_stack_opengl (Icon *pIcon, G_GNUC_UNUSED CairoContainer *pContainer, int w, int h)
{
	//\______________ On dessine les 4 premieres icones du sous-dock en pile.
	int i,k=0;
	Icon *icon;
	GList *ic;
	for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 3; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) || icon->image.iTexture == 0)
			continue;
		
		switch (i)
		{
			case 0:
				k = 1;
			break;
			case 1:
				if (ic->next == NULL)
					k = -1;
				else
					k = 0;
			break;
			case 2:
				k = -1;
			break;
			default : break;
		}
		
		glBindTexture (GL_TEXTURE_2D, icon->image.iTexture);
		_cairo_dock_apply_current_texture_at_size_with_offset (w*.8, h*.8, -k*w/10, k*h/10);
		
		i ++;
	}
}


static void _cairo_dock_load_box_surface (void)
{
	cairo_dock_unload_image_buffer (&g_pBoxAboveBuffer);
	cairo_dock_unload_image_buffer (&g_pBoxBelowBuffer);
	
	// load the image at a reasonnable size, it will then be scaled up/down when drawn.
	int iSizeWidth = myIconsParam.iIconWidth * (1 + myIconsParam.fAmplitude);
	int iSizeHeight = myIconsParam.iIconHeight * (1 + myIconsParam.fAmplitude);
	
	gchar *cUserPath = cairo_dock_generate_file_path ("box-front");
	if (! g_file_test (cUserPath, G_FILE_TEST_EXISTS))
	{
		g_free (cUserPath);
		cUserPath = NULL;
	}
	cairo_dock_load_image_buffer (&g_pBoxAboveBuffer,
		cUserPath ? cUserPath : GLDI_SHARE_DATA_DIR"/icons/box-front.png",
		iSizeWidth,
		iSizeHeight,
		CAIRO_DOCK_FILL_SPACE);
	
	cUserPath = cairo_dock_generate_file_path ("box-back");
	if (! g_file_test (cUserPath, G_FILE_TEST_EXISTS))
	{
		g_free (cUserPath);
		cUserPath = NULL;
	}
	cairo_dock_load_image_buffer (&g_pBoxBelowBuffer,
		cUserPath ? cUserPath : GLDI_SHARE_DATA_DIR"/icons/box-back.png",
		iSizeWidth,
		iSizeHeight,
		CAIRO_DOCK_FILL_SPACE);
}

static void _cairo_dock_unload_box_surface (void)
{
	cairo_dock_unload_image_buffer (&g_pBoxAboveBuffer);
	cairo_dock_unload_image_buffer (&g_pBoxBelowBuffer);
}

static void _cairo_dock_draw_subdock_content_as_box (Icon *pIcon, CairoContainer *pContainer, int w, int h, cairo_t *pCairoContext)
{
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	cairo_save (pCairoContext);
	cairo_scale(pCairoContext,
		(double) w / g_pBoxBelowBuffer.iWidth,
		(double) h / g_pBoxBelowBuffer.iHeight);
	cairo_dock_draw_surface (pCairoContext,
		g_pBoxBelowBuffer.pSurface,
		g_pBoxBelowBuffer.iWidth, g_pBoxBelowBuffer.iHeight,
		pContainer->bDirectionUp,
		pContainer->bIsHorizontal,
		1.);
	cairo_restore (pCairoContext);
	
	cairo_save (pCairoContext);
	if (pContainer->bIsHorizontal)
	{
		if (!pContainer->bDirectionUp)
			cairo_translate (pCairoContext, 0., .2*h);
	}
	else
	{
		if (! pContainer->bDirectionUp)
			cairo_translate (pCairoContext, .2*h, 0.);
	}
	/**cairo_scale (pCairoContext,
		.8,
		.8);*/
	int i;
	double dx, dy;
	int wi, hi;
	Icon *icon;
	GList *ic;
	for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 3; ic = ic->next, i++)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		{
			i --;
			continue;
		}
		
		if (pContainer->bIsHorizontal)
		{
			dx = .1*w;
			if (pContainer->bDirectionUp)
				dy = .1*i*h;
			else
				dy = - .1*i*h;
		}
		else
		{
			dy = .1*w;
			if (pContainer->bDirectionUp)
				dx = .1*i*h;
			else
				dx = - .1*i*h;
		}
		
		cairo_dock_get_icon_extent (icon, &wi, &hi);
		
		cairo_save (pCairoContext);
		cairo_translate (pCairoContext, dx, dy);
		cairo_scale (pCairoContext, .8 * w / wi, .8 * h / hi);
		cairo_set_source_surface (pCairoContext,
			icon->image.pSurface,
			0,
			0);
		cairo_paint (pCairoContext);
		cairo_restore (pCairoContext);
	}
	cairo_restore (pCairoContext);
	
	cairo_scale(pCairoContext,
		(double) w / g_pBoxAboveBuffer.iWidth,
		(double) h / g_pBoxAboveBuffer.iHeight);
	cairo_dock_draw_surface (pCairoContext,
		g_pBoxAboveBuffer.pSurface,
		g_pBoxAboveBuffer.iWidth, g_pBoxAboveBuffer.iHeight,
		pContainer->bDirectionUp,
		pContainer->bIsHorizontal,
		1.);
}

static void _cairo_dock_draw_subdock_content_as_box_opengl (Icon *pIcon, CairoContainer *pContainer, int w, int h)
{
	_cairo_dock_set_blend_source ();
	glPushMatrix ();
	if (pContainer->bIsHorizontal)
	{
		if (! pContainer->bDirectionUp)
			glScalef (1., -1., 1.);
	}
	else
	{
		glRotatef (90., 0., 0., 1.);
		if (! pContainer->bDirectionUp)
			glScalef (1., -1., 1.);
	}
	_cairo_dock_apply_texture_at_size (g_pBoxBelowBuffer.iTexture, w, h);
	
	glMatrixMode(GL_TEXTURE);
	glPushMatrix ();
	if (pContainer->bIsHorizontal)
	{
		if (! pContainer->bDirectionUp)
			glScalef (1., -1., 1.);
	}
	else
	{
		glRotatef (-90., 0., 0., 1.);
		if (! pContainer->bDirectionUp)
			glScalef (1., -1., 1.);
	}
	glMatrixMode (GL_MODELVIEW);
	_cairo_dock_set_blend_alpha ();
	int i;
	Icon *icon;
	GList *ic;
	for (ic = pIcon->pSubDock->icons, i = 0; ic != NULL && i < 3; ic = ic->next, i++)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		{
			i --;
			continue;
		}
		glBindTexture (GL_TEXTURE_2D, icon->image.iTexture);
		_cairo_dock_apply_current_texture_at_size_with_offset (.8*w, .8*h, 0., .1*(1-i)*h);
	}
	glMatrixMode(GL_TEXTURE);
	glPopMatrix ();
	glMatrixMode (GL_MODELVIEW);
	
	_cairo_dock_apply_texture_at_size (g_pBoxAboveBuffer.iTexture, w, h);
	glPopMatrix ();
}


void cairo_dock_register_icon_container_renderers (void)
{
	CairoIconContainerRenderer *p;
	p = g_new0 (CairoIconContainerRenderer, 1);
	p->render = _cairo_dock_draw_subdock_content_as_emblem;
	p->render_opengl = _cairo_dock_draw_subdock_content_as_emblem_opengl;
	cairo_dock_register_icon_container_renderer ("Emblem", p);
	
	p = g_new0 (CairoIconContainerRenderer, 1);
	p->render = _cairo_dock_draw_subdock_content_as_stack;
	p->render_opengl = _cairo_dock_draw_subdock_content_as_stack_opengl;
	cairo_dock_register_icon_container_renderer ("Stack", p);
	
	p = g_new0 (CairoIconContainerRenderer, 1);
	p->load = _cairo_dock_load_box_surface;
	p->unload = _cairo_dock_unload_box_surface;
	p->render = _cairo_dock_draw_subdock_content_as_box;
	p->render_opengl = _cairo_dock_draw_subdock_content_as_box_opengl;
	cairo_dock_register_icon_container_renderer ("Box", p);
	
	memset (&g_pBoxAboveBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&g_pBoxBelowBuffer, 0, sizeof (CairoDockImageBuffer));
}

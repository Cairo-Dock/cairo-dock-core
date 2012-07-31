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

/** See the "data/gauges" folder for some exemples */

#include <math.h>

#include "gldi-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-opengl-path.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-container.h"  // cairo_dock_get_max_scale
#include "cairo-dock-icon-facility.h"  // cairo_dock_get_icon_max_scale
#include "cairo-dock-progressbar.h"

// color gradation: N colors, 1/N each -> clamped image
// image path + stretched/clamp or repeat
// vertical/horizontal
// alignment in [0,1] <=> [bottom,top] or [left,right]
// bar height

typedef struct {
	CairoDataRenderer dataRenderer;
	cairo_surface_t *pBarSurface;
	GLuint iBarTexture;
	gint iBarThickness;
	gchar *cImageGradation;
	gdouble fColorGradation[8];  // 2 rgba colors
	gboolean bCustomColors;
	gdouble fScale;
} ProgressBar;

#define CD_MIN_BAR_THICKNESS 2

extern gboolean g_bUseOpenGL;

  //////////////////////////////////////
 /////////////// LOAD /////////////////
//////////////////////////////////////

static void _make_bar_surface (ProgressBar *pProgressBar)
{
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pProgressBar);
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	
	if (pProgressBar->cImageGradation != NULL)  // an image is provided
	{
		pProgressBar->pBarSurface = cairo_dock_create_surface_from_image_simple (
			pProgressBar->cImageGradation,
			iWidth,
			pProgressBar->iBarThickness);
	}
	
	if (pProgressBar->pBarSurface == NULL)  // no image was provided, or it was not valid.
	{
		// create a surface to bufferize the pattern.
		pProgressBar->pBarSurface = cairo_dock_create_blank_surface (iWidth, pProgressBar->iBarThickness);
		
		// create the pattern.
		cairo_t *ctx = cairo_create (pProgressBar->pBarSurface);
		cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0.,
			0.,
			iWidth,
			0.);  // de gauche a droite.
		g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);

		cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);

		gdouble *fColorGradation = pProgressBar->fColorGradation;
		int iNbColors = 2;
		int i;
		for (i = 0; i < iNbColors; i ++)
		{
			cairo_pattern_add_color_stop_rgba (pGradationPattern,
				(double)i / (iNbColors-1),
				fColorGradation[4*i+0],
				fColorGradation[4*i+1],
				fColorGradation[4*i+2],
				fColorGradation[4*i+3]);
		}
		cairo_set_source (ctx, pGradationPattern);
		
		// draw the pattern on the surface.
		cairo_set_operator (ctx, CAIRO_OPERATOR_OVER);
		
		cairo_set_line_width (ctx, pProgressBar->iBarThickness);
		
		double r = .5*pProgressBar->iBarThickness;  // radius
		cairo_move_to (ctx, 0, r);
		cairo_rel_line_to (ctx, iWidth, 0);
		
		cairo_stroke (ctx);
		
		cairo_pattern_destroy (pGradationPattern);
		cairo_destroy (ctx);
	}
	pProgressBar->iBarTexture = cairo_dock_create_texture_from_surface (pProgressBar->pBarSurface);
}

static void load (ProgressBar *pProgressBar, Icon *pIcon, CairoProgressBarAttribute *pAttribute)
{
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pProgressBar);
	
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	if (iWidth == 0 || iHeight == 0)
		return ;
	
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	pRenderer->iRank = iNbValues;
	
	// define the bar thickness
	pProgressBar->fScale = cairo_dock_get_icon_max_scale (pIcon);
	double fBarThickness;
	fBarThickness = MAX (myIndicatorsParam.iBarThickness, CD_MIN_BAR_THICKNESS);
	fBarThickness *= pProgressBar->fScale;  // the given size is therefore reached when the icon is at rest.
	pProgressBar->iBarThickness = ceil (fBarThickness);
	
	// load the bar image
	pProgressBar->cImageGradation = g_strdup (pAttribute->cImageGradation);
	if (pAttribute->fColorGradation)
	{
		pProgressBar->bCustomColors = TRUE;
		memcpy (pProgressBar->fColorGradation, pAttribute->fColorGradation, 8*sizeof (gdouble));
	}
	else
	{
		memcpy (pProgressBar->fColorGradation, myIndicatorsParam.fBarColorStart, 4*sizeof (gdouble));
		memcpy (&pProgressBar->fColorGradation[4], myIndicatorsParam.fBarColorStop, 4*sizeof (gdouble));
	}
	
	_make_bar_surface (pProgressBar);
	
	// set the size for the overlay.
	pRenderer->iHeight = pRenderer->iRank * pProgressBar->iBarThickness + 1;
	pRenderer->iOverlayPosition = (pAttribute->bUseCustomPosition ? pAttribute->iCustomPosition : CAIRO_OVERLAY_BOTTOM);
}

  ///////////////////////////////////////
 ////////////// RENDER  ////////////////
///////////////////////////////////////

static void render (ProgressBar *pProgressBar, cairo_t *pCairoContext)
{
	g_return_if_fail (pProgressBar != NULL);
	g_return_if_fail (pCairoContext != NULL && cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pProgressBar);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	
	double r = .5*pProgressBar->iBarThickness;  // radius
	double x, y, v;
	int i;
	for (i = 0; i < iNbValues; i ++)
	{
		x = 0.;  // the bar is left-aligned.
		y = iHeight - (i + 1) * pProgressBar->iBarThickness;  // first value at bottom.
		v = cairo_data_renderer_get_normalized_current_value_with_latency (pRenderer, i);
		
		if (v >= 0 && v <= 1)  // any negative value is an "undef" value
		{
			cairo_save (pCairoContext);
			cairo_translate (pCairoContext, x, y);
			cairo_set_line_cap (pCairoContext, CAIRO_LINE_CAP_ROUND);
			r = .5*pProgressBar->iBarThickness;
			
			// outline
			if (myIndicatorsParam.bBarHasOutline)
			{
				cairo_set_source_rgba (pCairoContext,
					myIndicatorsParam.fBarColorOutline[0],
					myIndicatorsParam.fBarColorOutline[1],
					myIndicatorsParam.fBarColorOutline[2],
					myIndicatorsParam.fBarColorOutline[3]);
				cairo_set_line_width (pCairoContext, pProgressBar->iBarThickness);
				
				cairo_move_to (pCairoContext, r, r);
				cairo_rel_line_to (pCairoContext, (iWidth -2*r) * v, 0);
				
				cairo_stroke (pCairoContext);
			}
			
			// bar
			cairo_set_source_surface (pCairoContext, pProgressBar->pBarSurface, 0, 0);
			cairo_set_line_width (pCairoContext, pProgressBar->iBarThickness-2);
			
			cairo_move_to (pCairoContext, r+1, r);
			cairo_rel_line_to (pCairoContext, (iWidth -2*r-2) * v, 0);
			
			cairo_stroke (pCairoContext);
			
			cairo_restore (pCairoContext);
		}
	}
}

  ///////////////////////////////////////////////
 /////////////// RENDER OPENGL /////////////////
///////////////////////////////////////////////

#define _CD_PATH_DIM 2
#define _cd_gl_path_get_nth_vertex_x(pPath, i) pPath->pVertices[_CD_PATH_DIM*(i)]
#define _cd_gl_path_get_nth_vertex_y(pPath, i) pPath->pVertices[_CD_PATH_DIM*(i)+1]

static void render_opengl (ProgressBar *pProgressBar)
{
	g_return_if_fail (pProgressBar != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pProgressBar);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	
	double x, y, v, w, r = pProgressBar->iBarThickness/2.;
	double dx = .5;  // required to not have sharp edges on the left rounded corners... although maybe it should be adressed by the Overlay...
	int i;
	for (i = 0; i < iNbValues; i ++)
	{
		v = cairo_data_renderer_get_normalized_current_value_with_latency (pRenderer, i);
		w = iWidth - pProgressBar->iBarThickness;
		x = - iWidth / 2. + w * v/2 + r + dx;  // center of the bar; the bar is left-aligned.
		y = i * pProgressBar->iBarThickness;  // first value at bottom.
		
		if (v >= 0 && v <= 1)  // any negative value is an "undef" value
		{
			// make a rounded rectangle path.
			const CairoDockGLPath *pFramePath = cairo_dock_generate_rectangle_path (w * v, 2*r, r, TRUE);
			
			// bind the texture to the path
			// we don't use the automatic coods generation because we want to bind the texture to the interval [0; v].
			glColor4f (1., 1., 1., 1.);
			_cairo_dock_set_blend_source ();  // doesn't really matter here.
			_cairo_dock_enable_texture ();
			glBindTexture (GL_TEXTURE_2D, pProgressBar->iBarTexture);
			
			GLfloat *pCoords = g_new0 (GLfloat, (pFramePath->iNbPoints+1) * _CD_PATH_DIM);
			int i;
			for (i = 0; i < pFramePath->iCurrentPt; i ++)
			{
				pCoords[_CD_PATH_DIM*i] = (.5 + _cd_gl_path_get_nth_vertex_x (pFramePath, i) / (w*v+2*r)) * v;  // [0;v]
				pCoords[_CD_PATH_DIM*i+1] = .5 + _cd_gl_path_get_nth_vertex_y (pFramePath, i) / (pProgressBar->iBarThickness);
			}
			glEnableClientState (GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer (_CD_PATH_DIM, GL_FLOAT, 0, pCoords);
			
			// draw the path.
			glPushMatrix ();
			glTranslatef (x,
				y,
				0.);
			cairo_dock_fill_gl_path (pFramePath, 0);  // 0 <=> no texture, since we bound it ourselves.
			
			_cairo_dock_disable_texture ();
			glDisableClientState (GL_TEXTURE_COORD_ARRAY);
			
			// outline
			if (myIndicatorsParam.bBarHasOutline)
			{
				glColor4f (myIndicatorsParam.fBarColorOutline[0],
					myIndicatorsParam.fBarColorOutline[1],
					myIndicatorsParam.fBarColorOutline[2],
					myIndicatorsParam.fBarColorOutline[3]);
				_cairo_dock_set_blend_alpha ();
				glLineWidth (1.5);
				cairo_dock_stroke_gl_path (pFramePath, FALSE);
			}
			
			g_free (pCoords);
			glPopMatrix ();
		}
	}
}

  /////////////////////////////////////////
 /////////////// RELOAD  /////////////////
/////////////////////////////////////////

static void reload (ProgressBar *pProgressBar)
{
	g_return_if_fail (pProgressBar != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pProgressBar);
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	g_print ("%s (%dx%d)\n", __func__, iWidth, iHeight);
	
	// since we take our parameters from the config, reset them
	double fBarThickness;
	fBarThickness = MAX (myIndicatorsParam.iBarThickness, CD_MIN_BAR_THICKNESS);
	fBarThickness *= pProgressBar->fScale;  // the given size is therefore reached when the icon is at rest.
	pProgressBar->iBarThickness = ceil (fBarThickness);
	if (!pProgressBar->bCustomColors)
	{
		memcpy (pProgressBar->fColorGradation, myIndicatorsParam.fBarColorStart, 4*sizeof (gdouble));
		memcpy (&pProgressBar->fColorGradation[4], myIndicatorsParam.fBarColorStop, 4*sizeof (gdouble));
	}
	
	// reload the bar surface
	if (pProgressBar->pBarSurface)
	{
		cairo_surface_destroy (pProgressBar->pBarSurface);
		pProgressBar->pBarSurface = NULL;
	}
	if (pProgressBar->iBarTexture != 0)
	{
		_cairo_dock_delete_texture (pProgressBar->iBarTexture);
		pProgressBar->iBarTexture = 0;
	}
	_make_bar_surface (pProgressBar);
	
	// set the size for the overlay.
	pRenderer->iHeight = pRenderer->iRank * pProgressBar->iBarThickness + 1;
}

  ////////////////////////////////////////
 /////////////// UNLOAD /////////////////
////////////////////////////////////////

static void unload (ProgressBar *pProgressBar)
{
	cd_debug("");
	if (pProgressBar->pBarSurface)
		cairo_surface_destroy (pProgressBar->pBarSurface);
	if (pProgressBar->iBarTexture)
		_cairo_dock_delete_texture (pProgressBar->iBarTexture);
	g_free (pProgressBar->cImageGradation);
}


  //////////////////////////////////////////
 /////////////// RENDERER /////////////////
//////////////////////////////////////////
void cairo_dock_register_data_renderer_progressbar (void)
{
	// create a new record
	CairoDockDataRendererRecord *pRecord = g_new0 (CairoDockDataRendererRecord, 1);
	
	// fill the properties we need
	pRecord->interface.load	         = (CairoDataRendererLoadFunc) load;
	pRecord->interface.render        = (CairoDataRendererRenderFunc) render;
	pRecord->interface.render_opengl = (CairoDataRendererRenderOpenGLFunc) render_opengl;
	pRecord->interface.reload        = (CairoDataRendererReloadFunc) reload;
	pRecord->interface.unload        = (CairoDataRendererUnloadFunc) unload;
	pRecord->iStructSize             = sizeof (ProgressBar);
	pRecord->bUseOverlay             = TRUE;
	
	// register
	cairo_dock_register_data_renderer ("progressbar", pRecord);
}

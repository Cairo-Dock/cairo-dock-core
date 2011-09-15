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

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "cairo-dock-log.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-container.h"
#include "cairo-dock-graph.h"

typedef struct _Graph {
	CairoDataRenderer dataRenderer;
	CairoDockTypeGraph iType;
	gdouble *fHighColor;  // iNbValues triplets (r,v,b).
	gdouble *fLowColor;  // idem.
	cairo_pattern_t **pGradationPatterns;  // iNbValues patterns.
	gdouble fBackGroundColor[4];
	cairo_surface_t *pBackgroundSurface;
	GLuint iBackgroundTexture;
	gint iRadius;
	gint iMargin;
	gboolean bMixGraphs;
	} Graph;


extern gboolean g_bUseOpenGL;


static void render (Graph *pGraph, cairo_t *pCairoContext)
{
	g_return_if_fail (pGraph != NULL);
	g_return_if_fail (pCairoContext != NULL && cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGraph);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	
	if (pGraph->pBackgroundSurface != NULL)
	{
		cairo_set_source_surface (pCairoContext, pGraph->pBackgroundSurface, 0., 0.);
		cairo_paint (pCairoContext);
	}
	
	int iNbDrawings = iNbValues / pRenderer->iRank;
	if (iNbDrawings == 0)
		return;
	
	int iMargin = pGraph->iMargin;
	int iWidth = pRenderer->iWidth - 2*iMargin;
	int iHeight = pRenderer->iHeight - 2*iMargin;
	iHeight /= iNbDrawings;
	
	double fValue;
	cairo_pattern_t *pGradationPattern;
	int t, n = MIN (pData->iMemorySize, iWidth);  // for iteration over the memorized values.
	int i;
	for (i = 0; i < iNbValues; i ++)
	{
		cairo_save (pCairoContext);
		if (! pGraph->bMixGraphs)
			cairo_translate (pCairoContext,
				0.,
				i * iHeight);
		pGradationPattern = pGraph->pGradationPatterns[i];
		if (pGradationPattern != NULL)
			cairo_set_source (pCairoContext, pGradationPattern);
		else
			cairo_set_source_rgb (pCairoContext,
				pGraph->fLowColor[3*i+0],
				pGraph->fLowColor[3*i+1],
				pGraph->fLowColor[3*i+2]);
		
		switch (pGraph->iType)
		{
			case CAIRO_DOCK_GRAPH_LINE:
			case CAIRO_DOCK_GRAPH_PLAIN:
			default :
				cairo_set_line_width (pCairoContext, 1);
				cairo_set_line_join (pCairoContext, CAIRO_LINE_JOIN_ROUND);
				fValue = cairo_data_renderer_get_normalized_current_value (pRenderer, i);
				if (fValue <= CAIRO_DATA_RENDERER_UNDEF_VALUE+1)  // undef value -> let's draw 0
					fValue = 0;
				cairo_move_to (pCairoContext,
					iMargin + iWidth - .5,
					iMargin + (1 - fValue) * iHeight - .5);
				for (t = 1; t < n; t ++)
				{
					fValue = cairo_data_renderer_get_normalized_value (pRenderer, i, -t);
					if (fValue <= CAIRO_DATA_RENDERER_UNDEF_VALUE+1)  // undef value -> let's draw 0
						fValue = 0;
					cairo_line_to (pCairoContext,
						iMargin + (n - t) * iWidth / n - .5,
						iMargin + (1 - fValue) * iHeight - .5);
				}
				if (pGraph->iType == CAIRO_DOCK_GRAPH_PLAIN)
				{
					cairo_line_to (pCairoContext,
						iMargin - .5,
						iMargin + iHeight - .5);
					cairo_rel_line_to (pCairoContext,
						iWidth,
						0.);
					cairo_close_path (pCairoContext);
					cairo_fill_preserve (pCairoContext);
				}
				cairo_stroke (pCairoContext);
			break;
			
			case CAIRO_DOCK_GRAPH_BAR:
			{
				///double fBarWidth = iWidth / pData->iMemorySize / 4;
				double fBarWidth = 1;
				cairo_set_line_width (pCairoContext, fBarWidth);
				for (t = 1; t < n; t ++)
				{
					fValue = cairo_data_renderer_get_normalized_value (pRenderer, i, -t);
					if (fValue <= CAIRO_DATA_RENDERER_UNDEF_VALUE+1)  // undef value -> let's draw 0
						fValue = 0;
					if (fValue > 0)
					{
						cairo_move_to (pCairoContext,
							iMargin + (n - t) * iWidth / n - .5,
							iMargin + iHeight - .5);
						cairo_rel_line_to (pCairoContext,
							0.,
							- fValue * iHeight);
						cairo_stroke (pCairoContext);
					}
				}
			}
			break;
			
			case CAIRO_DOCK_GRAPH_CIRCLE:
			case CAIRO_DOCK_GRAPH_CIRCLE_PLAIN:
				cairo_set_line_width (pCairoContext, 1);
				cairo_set_line_join (pCairoContext, CAIRO_LINE_JOIN_ROUND);
				fValue = cairo_data_renderer_get_normalized_current_value (pRenderer, i);
				if (fValue <= CAIRO_DATA_RENDERER_UNDEF_VALUE+1)  // undef value -> let's draw 0
					fValue = 0;
				double angle, radius = MIN (iWidth, iHeight)/2;
				angle = -2*G_PI*(-.5/pData->iMemorySize);
				cairo_move_to (pCairoContext,
					iMargin + iWidth/2 + radius * (fValue * cos (angle)),
					iMargin + iHeight/2 + radius * (fValue * sin (angle)));
				angle = -2*G_PI*(.5/pData->iMemorySize);
				cairo_line_to (pCairoContext,
					iMargin + iWidth/2 + radius * (fValue * cos (angle)),
					iMargin + iHeight/2 + radius * (fValue * sin (angle)));
				for (t = 1; t < n; t ++)
				{
					fValue = cairo_data_renderer_get_normalized_value (pRenderer, i, -t);
					if (fValue <= CAIRO_DATA_RENDERER_UNDEF_VALUE+1)  // undef value -> let's draw 0
						fValue = 0;
					angle = -2*G_PI*((t-.5)/n);
					cairo_line_to (pCairoContext,
						iMargin + iWidth/2 + radius * (fValue * cos (angle)),
						iMargin + iHeight/2 + radius * (fValue * sin (angle)));
					angle = -2*G_PI*((t+.5)/n);
					cairo_line_to (pCairoContext,
						iMargin + iWidth/2 + radius * (fValue * cos (angle)),
						iMargin + iHeight/2 + radius * (fValue * sin (angle)));
				}
				if (pGraph->iType == CAIRO_DOCK_GRAPH_CIRCLE_PLAIN)
				{
					cairo_close_path (pCairoContext);
					cairo_fill_preserve (pCairoContext);
				}
				cairo_stroke (pCairoContext);
			break;
		}
		cairo_restore (pCairoContext);
		
		cairo_dock_render_overlays_to_context (pRenderer, i, pCairoContext);
	}
}

static void render_opengl (Graph *pGraph)
{
	g_return_if_fail (pGraph != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGraph);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	
	if (pGraph->iBackgroundTexture != 0)
	{
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_pbuffer ();  // ceci reste un mystere...
		_cairo_dock_set_alpha (1.);
		_cairo_dock_apply_texture_at_size_with_alpha (pGraph->iBackgroundTexture, pRenderer->iWidth, pRenderer->iHeight, 1.);
		_cairo_dock_disable_texture ();
	}
	
	/// to be continued ...
}


static inline cairo_surface_t *_cairo_dock_create_graph_background (double fWidth, double fHeight, int iRadius, int iMargin, gdouble *pBackGroundColor, CairoDockTypeGraph iType, int iNbDrawings)
{
	// on cree la surface.
	cairo_surface_t *pBackgroundSurface = cairo_dock_create_blank_surface (
		fWidth,
		fHeight);
	cairo_t *pCairoContext = cairo_create (pBackgroundSurface);
	
	// on trace le fond : un rectangle au coin arrondi.
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba (pCairoContext,
		pBackGroundColor[0],
		pBackGroundColor[1],
		pBackGroundColor[2],
		pBackGroundColor[3]);
	
	double fRadius = iRadius;
	cairo_set_line_width (pCairoContext, fRadius);
	cairo_set_line_join (pCairoContext, CAIRO_LINE_JOIN_ROUND);
	cairo_move_to (pCairoContext, .5*fRadius, .5*fRadius);
	cairo_rel_line_to (pCairoContext, fWidth - (fRadius), 0);
	cairo_rel_line_to (pCairoContext, 0, fHeight - (fRadius));
	cairo_rel_line_to (pCairoContext, -(fWidth - (fRadius)) ,0);
	cairo_close_path (pCairoContext);
	cairo_stroke (pCairoContext);  // on trace d'abord les contours arrondis.
	
	cairo_rectangle (pCairoContext, fRadius, fRadius, (fWidth - 2*fRadius), (fHeight - 2*fRadius));
	cairo_fill (pCairoContext);  // puis on rempli l'interieur.
	
	// on trace les axes.
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgb (pCairoContext,
		myIconsParam.quickInfoTextDescription.fBackgroundColor[0],
		myIconsParam.quickInfoTextDescription.fBackgroundColor[1],
		myIconsParam.quickInfoTextDescription.fBackgroundColor[2]);  // meme couleur que le fond des info-rapides, mais opaque.
	cairo_set_line_width (pCairoContext, 1.);
	if (iType == CAIRO_DOCK_GRAPH_CIRCLE || iType == CAIRO_DOCK_GRAPH_CIRCLE_PLAIN)
	{
		double r = .5 * MIN (fWidth - 2*iMargin, (fHeight - 2*iMargin) / iNbDrawings);
		int i;
		for (i = 0; i < iNbDrawings; i ++)
		{
			cairo_arc (pCairoContext,
				fWidth/2,
				iMargin + r * (2 * i + 1),
				r,
				0.,
				360.);
			cairo_move_to (pCairoContext, fWidth/2, iMargin + r * (2 * i + 1));
			cairo_rel_line_to (pCairoContext, r, 0.);
			cairo_stroke (pCairoContext);
		}
	}
	else
	{
		cairo_move_to (pCairoContext, iMargin-.5, iMargin-.5);
		cairo_rel_line_to (pCairoContext, 0., fHeight - 2*iMargin);
		cairo_stroke (pCairoContext);
		int i;
		for (i = 0; i < iNbDrawings; i ++)
		{
			cairo_move_to (pCairoContext,
				iMargin - .5,
				floor ((fHeight - 2 * iMargin) * (i + 1) / iNbDrawings) + iMargin - .5);
			cairo_rel_line_to (pCairoContext, fWidth - 2*iMargin, 0.);
			cairo_stroke (pCairoContext);
		}
	}
	
	cairo_destroy (pCairoContext);
	return  pBackgroundSurface;
}
static cairo_pattern_t *_cairo_dock_create_graph_pattern (Graph *pGraph, gdouble *fLowColor, gdouble *fHighColor, double fOffsetY)
{
	cairo_pattern_t *pGradationPattern = NULL;
	if (fLowColor[0] != fHighColor[0] || fLowColor[1] != fHighColor[1] || fLowColor[2] != fHighColor[2])  // un degrade existe.
	{
		int iMargin = pGraph->iMargin;
		double fWidth = pGraph->dataRenderer.iWidth - 2*iMargin;
		double fHeight = pGraph->dataRenderer.iHeight - 2*iMargin;
		fHeight /= (pGraph->dataRenderer.data.iNbValues / pGraph->dataRenderer.iRank);
		
		if (pGraph->iType == CAIRO_DOCK_GRAPH_CIRCLE || pGraph->iType == CAIRO_DOCK_GRAPH_CIRCLE_PLAIN)
		{
			double radius = MIN (fWidth, fHeight)/2.;
			pGradationPattern = cairo_pattern_create_radial (fWidth/2,
				iMargin + radius + fOffsetY,
				0.,
				fWidth/2,
				iMargin + radius + fOffsetY,
				radius);
		}
		else
			pGradationPattern = cairo_pattern_create_linear (0.,
				iMargin + fHeight + fOffsetY,
				0.,
				iMargin + fOffsetY);
		g_return_val_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS, NULL);	
		
		cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_PAD);
		cairo_pattern_add_color_stop_rgba (pGradationPattern,
			0.,
			fLowColor[0],
			fLowColor[1],
			fLowColor[2],
			1.);
		cairo_pattern_add_color_stop_rgba (pGradationPattern,
			1.,
			fHighColor[0],
			fHighColor[1],
			fHighColor[2],
			1.);
	}
	return pGradationPattern;
}
#define dc .5
#define _guess_color(i) (((pGraph->fLowColor[i] < pGraph->fHighColor[i] && pGraph->fLowColor[i] > dc) || pGraph->fLowColor[i] > 1-dc) ? pGraph->fLowColor[i] - dc : pGraph->fLowColor[i] + dc)
//#define _guess_color(i) (1. - pGraph->fLowColor[i])
static void _set_overlay_zones (Graph *pGraph)
{
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGraph);
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	int i;
	
	// on complete le data-renderer.
	int iNbDrawings = iNbValues / pRenderer->iRank;
	int iMargin = pGraph->iMargin;
	double fOneGraphHeight = iHeight - 2*iMargin;
	fOneGraphHeight /= iNbDrawings;
	double fOneGraphWidth = iWidth - 2*iMargin;
	fOneGraphWidth /= iNbDrawings;
	int iTextWidth = MIN (48, pRenderer->iWidth/2);  // on definit une taille pour les zones de texte.
	int iTextHeight = MIN (16, fOneGraphHeight/1.5);
	int iLabelWidth = MIN (48, pRenderer->iWidth/2);  // on definit une taille pour les zones de texte.
	int iLabelHeight = MIN (16, fOneGraphHeight/2.);
	int h = fOneGraphHeight/8;  // ecart du texte au-dessus de l'axe Ox.
	CairoDataRendererTextParam *pValuesText;
	CairoDataRendererEmblem *pEmblem;
	CairoDataRendererText *pLabel;
	for (i = 0; i < iNbValues; i ++)
	{
		/// rajouter l'alignement gauche/centre/droit ...
		if (pRenderer->pLabels)  // les labels en haut a gauche.
		{
			pLabel = &pRenderer->pLabels[i];
			if (iLabelHeight > 8)  // sinon trop petit, et empiete sur la valeur.
			{
				if (pGraph->bMixGraphs)
				{
					pLabel->param.fX = (double)(iMargin + i * fOneGraphWidth + iLabelWidth/2) / iWidth - .5;
					pLabel->param.fY = (double)(iHeight - iMargin - iLabelHeight/2) / iHeight - .5;
				}
				else
				{
					pLabel->param.fX = (double) (iMargin + iLabelWidth/2) / iWidth - .5;
					pLabel->param.fY = .5 - (double)(iMargin + h + i * fOneGraphHeight + iLabelHeight/2) / iHeight;
				}
				pLabel->param.fWidth = (double)iLabelWidth / iWidth;
				pLabel->param.fHeight = (double)iLabelHeight / iHeight;
				pLabel->param.pColor[0] = 1.;
                pLabel->param.pColor[1] = 1.;
                pLabel->param.pColor[2] = 1.;
                pLabel->param.pColor[3] = 1.;  // white, a little transparent
			}
			else
			{
				pLabel->param.fWidth = pLabel->param.fHeight = 0;
			}
		}
		if (pRenderer->pValuesText)  // les valeurs en bas au milieu.
		{
			pValuesText = &pRenderer->pValuesText[i];
			if (pGraph->bMixGraphs)
			{
				pValuesText->fX = (double)(iMargin + i * fOneGraphWidth + iTextWidth/2) / iWidth - .5;
				pValuesText->fY = (double)(iMargin + h + iTextHeight/2) / iHeight - .5;
			}
			else
			{
				pValuesText->fX = (double)0.;  // centered.
				pValuesText->fY = .5 - (double)(iMargin + (i+1) * fOneGraphHeight - iTextHeight/2 - h) / iHeight;
			}
			pValuesText->fWidth = (double)iTextWidth / iWidth;
			pValuesText->fHeight = (double)iTextHeight / iHeight;
			if (pGraph->fBackGroundColor[3] > .2 && pGraph->fBackGroundColor[3] < .7)
			{
				pValuesText->pColor[0] = pGraph->fBackGroundColor[0];
				pValuesText->pColor[1] = pGraph->fBackGroundColor[1];
				pValuesText->pColor[2] = pGraph->fBackGroundColor[2];
				//g_print ("bg color: %.2f;%.2f;%.2f\n", pGraph->fBackGroundColor[0], pGraph->fBackGroundColor[1], pGraph->fBackGroundColor[2]);
			}
			else
			{
				pValuesText->pColor[0] = _guess_color (0);
				pValuesText->pColor[1] = _guess_color (1);
				pValuesText->pColor[2] = _guess_color (2);
				//g_print ("line color: %.2f;%.2f;%.2f\n", pGraph->fLowColor[0], pGraph->fLowColor[1], pGraph->fLowColor[2]);
			}
			//g_print ("text color: %.2f;%.2f;%.2f\n", pValuesText->pColor[0], pValuesText->pColor[1], pValuesText->pColor[2]);
			pValuesText->pColor[3] = 1.;
		}
	}
}

static void load (Graph *pGraph, CairoContainer *pContainer, CairoGraphAttribute *pAttribute)
{
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGraph);
	
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	if (iWidth == 0 || iHeight == 0)
		return ;
	
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	pGraph->iType = pAttribute->iType;
	pGraph->bMixGraphs = pAttribute->bMixGraphs;
	pRenderer->iRank = (pAttribute->bMixGraphs ? iNbValues : 1);
	
	pGraph->fHighColor = g_new0 (double, 3 * iNbValues);
	if (pAttribute->fHighColor != NULL)
		memcpy (pGraph->fHighColor, pAttribute->fHighColor, 3 * iNbValues * sizeof (double));
	pGraph->fLowColor = g_new0 (double, 3 * iNbValues);
	if (pAttribute->fLowColor != NULL)
		memcpy (pGraph->fLowColor, pAttribute->fLowColor, 3 * iNbValues * sizeof (double));

	int i;
	pGraph->pGradationPatterns = g_new (cairo_pattern_t *, iNbValues);
	for (i = 0; i < iNbValues; i ++)
	{
		pGraph->pGradationPatterns[i] = _cairo_dock_create_graph_pattern (pGraph,
			&pGraph->fLowColor[3*i],
			&pGraph->fHighColor[3*i],
			0.);
	}
	
	pGraph->iRadius = MIN (iWidth, iHeight)/12.;
	pGraph->iMargin = pGraph->iRadius * (1. - sqrt(2)/2);
	if (pAttribute->fBackGroundColor != NULL)
		memcpy (pGraph->fBackGroundColor, pAttribute->fBackGroundColor, 4 * sizeof (double));
	pGraph->pBackgroundSurface = _cairo_dock_create_graph_background (
		iWidth,
		iHeight,
		pGraph->iRadius,
		pGraph->iMargin,
		pGraph->fBackGroundColor,
		pGraph->iType,
		iNbValues / pRenderer->iRank);
	if (g_bUseOpenGL && 0)
		pGraph->iBackgroundTexture = cairo_dock_create_texture_from_surface (pGraph->pBackgroundSurface);
	
	// on complete le data-renderer.
	_set_overlay_zones (pGraph);
}


static void reload (Graph *pGraph)
{
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGraph);
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	pGraph->iRadius = MIN (iWidth, iHeight)/12.;
	pGraph->iMargin = pGraph->iRadius * (1. - sqrt(2)/2);
	if (pGraph->pBackgroundSurface != NULL)
		cairo_surface_destroy (pGraph->pBackgroundSurface);
	pGraph->pBackgroundSurface = _cairo_dock_create_graph_background (iWidth, iHeight, pGraph->iRadius, pGraph->iMargin, pGraph->fBackGroundColor, pGraph->iType, iNbValues / pRenderer->iRank);
	if (pGraph->iBackgroundTexture != 0)
		_cairo_dock_delete_texture (pGraph->iBackgroundTexture);
	if (g_bUseOpenGL && 0)
		pGraph->iBackgroundTexture = cairo_dock_create_texture_from_surface (pGraph->pBackgroundSurface);
	else
		pGraph->iBackgroundTexture = 0;
	int i;
	for (i = 0; i < iNbValues; i ++)
	{
		if (pGraph->pGradationPatterns[i] != NULL)
			cairo_pattern_destroy (pGraph->pGradationPatterns[i]);
		pGraph->pGradationPatterns[i] = _cairo_dock_create_graph_pattern (pGraph, &pGraph->fLowColor[3*i], &pGraph->fHighColor[3*i], 0.);
	}
	
	// on re-complete le data-renderer.
	_set_overlay_zones (pGraph);
}


static void unload (Graph *pGraph)
{
	cd_debug ("");
	if (pGraph->pBackgroundSurface != NULL)
		cairo_surface_destroy (pGraph->pBackgroundSurface);
	if (pGraph->iBackgroundTexture != 0)
		_cairo_dock_delete_texture (pGraph->iBackgroundTexture);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGraph);
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	int i;
	for (i = 0; i < iNbValues; i ++)
	{
		if (pGraph->pGradationPatterns[i] != NULL)
			cairo_pattern_destroy (pGraph->pGradationPatterns[i]);
	}
	
	g_free (pGraph->pGradationPatterns);
	g_free (pGraph->fHighColor);
	g_free (pGraph->fLowColor);
}



  //////////////////////////////////////////
 /////////////// RENDERER /////////////////
//////////////////////////////////////////
void cairo_dock_register_data_renderer_graph (void)
{
	CairoDockDataRendererRecord *pRecord = g_new0 (CairoDockDataRendererRecord, 1);
	pRecord->interface.load			= (CairoDataRendererLoadFunc) load;
	pRecord->interface.render		= (CairoDataRendererRenderFunc) render;
	pRecord->interface.render_opengl	= (CairoDataRendererRenderOpenGLFunc) NULL;
	pRecord->interface.reload		= (CairoDataRendererReloadFunc) reload;
	pRecord->interface.unload		= (CairoDataRendererUnloadFunc) unload;
	pRecord->iStructSize			= sizeof (Graph);
	pRecord->cThemeDirName = NULL;
	pRecord->cDefaultTheme = NULL;
	
	cairo_dock_register_data_renderer ("graph", pRecord);
}

/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "cairo-dock-log.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-container.h"
#include "cairo-dock-graph.h"

extern gboolean g_bUseOpenGL;


void cairo_dock_render_graph (CairoDockGraph *pGraph, cairo_t *pCairoContext)
{
	g_return_if_fail (pGraph != NULL && pCairoContext != NULL);
	g_return_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGraph);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	
	if (pGraph->pBackgroundSurface != NULL)
	{
		cairo_set_source_surface (pCairoContext, pGraph->pBackgroundSurface, 0., 0.);
		cairo_paint (pCairoContext);
	}
	
	int iNbDrawings = pData->iNbValues / pRenderer->iRank;
	if (iNbDrawings == 0)
		return;
	
	double fMargin = pGraph->fMargin;
	double fWidth = pRenderer->iWidth - 2*fMargin;
	double fHeight = pRenderer->iHeight - 2*fMargin;
	fHeight /= iNbDrawings;
	
	double fValue;
	cairo_pattern_t *pGradationPattern;
	int i;
	for (i = 0; i < pData->iNbValues; i ++)
	{
		if (! pGraph->bMixGraphs)
			cairo_translate (pCairoContext,
				0.,
				i * fHeight);
		pGradationPattern = pGraph->pGradationPatterns[i];
		if (pGradationPattern != NULL)
			cairo_set_source (pCairoContext, pGradationPattern);
		else
			cairo_set_source_rgb (pCairoContext,
				pGraph->fLowColor[3*i+0],
				pGraph->fLowColor[3*i+1],
				pGraph->fLowColor[3*i+2]);
		
		if (pGraph->iType == CAIRO_DOCK_GRAPH2_LINE || pGraph->iType == CAIRO_DOCK_GRAPH2_PLAIN)
		{
			cairo_set_line_width (pCairoContext, 1);
			cairo_set_line_join (pCairoContext, CAIRO_LINE_JOIN_ROUND);
			fValue = cairo_data_renderer_get_normalized_current_value (pRenderer, i);
			cairo_move_to (pCairoContext, fMargin + fWidth, fMargin + (1 - fValue) * fHeight);
			int t, n = pData->iMemorySize - 1;
			for (t = 1; t < pData->iMemorySize; t ++)
			{
				fValue = cairo_data_renderer_get_normalized_value (pRenderer, i, -t);
				cairo_line_to (pCairoContext,
					fMargin + (n - t) * fWidth / n,
					fMargin + (1 - fValue) * fHeight);
			}
			if (pGraph->iType == CAIRO_DOCK_GRAPH2_PLAIN)
			{
				cairo_line_to (pCairoContext,
					fMargin,
					fMargin + fHeight);
				cairo_line_to (pCairoContext,
					fMargin + fWidth,
					fMargin + fHeight);
				cairo_close_path (pCairoContext);
				cairo_fill_preserve (pCairoContext);
			}
			cairo_stroke (pCairoContext);
		}
		else if (pGraph->iType == CAIRO_DOCK_GRAPH2_BAR)
		{
			double fBarWidth = fWidth / pData->iMemorySize / 4;
			cairo_set_line_width (pCairoContext, fBarWidth);
			int t, n = pData->iMemorySize - 1;
			for (t = 0; t < pData->iMemorySize; t ++)
			{
				fValue = cairo_data_renderer_get_normalized_value (pRenderer, i, -t);
				cairo_move_to (pCairoContext,
					fMargin + (n - t) * fWidth / n,
					fMargin + fHeight);
				cairo_rel_line_to (pCairoContext,
					0.,
					- fValue * fHeight);
				cairo_stroke (pCairoContext);
			}
		}
		else
		{
			cairo_set_line_width (pCairoContext, 1);
			cairo_set_line_join (pCairoContext, CAIRO_LINE_JOIN_ROUND);
			fValue = cairo_data_renderer_get_normalized_current_value (pRenderer, i);
			double angle, radius = MIN (fWidth, fHeight)/2;
			angle = -2*G_PI*(-.5/pData->iMemorySize);
			cairo_move_to (pCairoContext,
				fMargin + fWidth/2 + radius * (fValue * cos (angle)),
				fMargin + fHeight/2 + radius * (fValue * sin (angle)));
			angle = -2*G_PI*(.5/pData->iMemorySize);
			cairo_line_to (pCairoContext,
				fMargin + fWidth/2 + radius * (fValue * cos (angle)),
				fMargin + fHeight/2 + radius * (fValue * sin (angle)));
			int t;
			for (t = 1; t < pData->iMemorySize; t ++)
			{
				fValue = cairo_data_renderer_get_normalized_value (pRenderer, i, -t);
				angle = -2*G_PI*((t-.5)/pData->iMemorySize);
				cairo_line_to (pCairoContext,
					fMargin + fWidth/2 + radius * (fValue * cos (angle)),
					fMargin + fHeight/2 + radius * (fValue * sin (angle)));
				angle = -2*G_PI*((t+.5)/pData->iMemorySize);
				cairo_line_to (pCairoContext,
					fMargin + fWidth/2 + radius * (fValue * cos (angle)),
					fMargin + fHeight/2 + radius * (fValue * sin (angle)));
			}
			if (pGraph->iType == CAIRO_DOCK_GRAPH2_CIRCLE_PLAIN)
			{
				cairo_close_path (pCairoContext);
				cairo_fill_preserve (pCairoContext);
			}
			cairo_stroke (pCairoContext);
		}
	}
}


static void cairo_dock_render_graph_opengl (CairoDockGraph *pGraph)
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
	
	
}


static inline cairo_surface_t *_cairo_dock_create_graph_background (cairo_t *pSourceContext, double fWidth, double fHeight, int iRadius, double fMargin, gdouble *pBackGroundColor, CairoDockTypeGraph iType, int iNbDrawings)
{
	// on cree la surface.
	cairo_surface_t *pBackgroundSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
		CAIRO_CONTENT_COLOR_ALPHA,
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
		myLabels.quickInfoTextDescription.fBackgroundColor[0],
		myLabels.quickInfoTextDescription.fBackgroundColor[1],
		myLabels.quickInfoTextDescription.fBackgroundColor[2]);  // meme couleur que le fond des info-rapides, mais opaque.
	cairo_set_line_width (pCairoContext, 1.);
	if (iType == CAIRO_DOCK_GRAPH2_CIRCLE || iType == CAIRO_DOCK_GRAPH2_CIRCLE_PLAIN)
	{
		double r = .5 * MIN (fWidth - 2*fMargin, (fHeight - 2*fMargin) / iNbDrawings);
		int i;
		for (i = 0; i < iNbDrawings; i ++)
		{
			cairo_arc (pCairoContext,
				fWidth/2,
				fMargin + r * (2 * i + 1),
				r,
				0.,
				360.);
			cairo_move_to (pCairoContext, fWidth/2, fMargin + r * (2 * i + 1));
			cairo_rel_line_to (pCairoContext, r, 0.);
			cairo_stroke (pCairoContext);
		}
	}
	else
	{
		cairo_move_to (pCairoContext, fMargin, fMargin);
		cairo_rel_line_to (pCairoContext, 0., fHeight - 2*fMargin);
		cairo_stroke (pCairoContext);
		int i;
		for (i = 0; i < iNbDrawings; i ++)
		{
			cairo_move_to (pCairoContext, fMargin, (fHeight - 2 * fMargin) * (i + 1) / iNbDrawings + fMargin);
			cairo_rel_line_to (pCairoContext, fWidth - 2*fMargin, 0.);
			cairo_stroke (pCairoContext);
		}
	}
	
	cairo_destroy (pCairoContext);
	return  pBackgroundSurface;
}
static cairo_pattern_t *_cairo_dock_create_graph_pattern (CairoDockGraph *pGraph, gdouble *fLowColor, gdouble *fHighColor, double fOffsetY)
{
	cairo_pattern_t *pGradationPattern = NULL;
	if (fLowColor[0] != fHighColor[0] || fLowColor[1] != fHighColor[1] || fLowColor[2] != fHighColor[2])  // un degrade existe.
	{
		double fMargin = pGraph->fMargin;
		double fWidth = pGraph->dataRenderer.iWidth - 2*fMargin;
		double fHeight = pGraph->dataRenderer.iHeight - 2*fMargin;
		fHeight /= pGraph->dataRenderer.data.iNbValues / pGraph->dataRenderer.iRank;
		
		if (pGraph->iType == CAIRO_DOCK_GRAPH2_CIRCLE || pGraph->iType == CAIRO_DOCK_GRAPH2_CIRCLE_PLAIN)
		{
			double radius = MIN (fWidth, fHeight)/2;
			pGradationPattern = cairo_pattern_create_radial (fWidth/2,
				fMargin + radius + fOffsetY,
				0.,
				fWidth/2,
				fMargin + radius + fOffsetY,
				radius);
		}
		else
			pGradationPattern = cairo_pattern_create_linear (0.,
				fMargin + fHeight + fOffsetY,
				0.,
				fMargin + fOffsetY);
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
static void cairo_dock_load_graph (CairoDockGraph *pGraph, cairo_t *pSourceContext, CairoContainer *pContainer, CairoGraphAttribute *pAttribute)
{
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGraph);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	if (iWidth == 0 || iHeight == 0)
		return ;
	
	pGraph->iType = pAttribute->iType;
	pGraph->dataRenderer.iRank = (pAttribute->bMixGraphs ? pData->iNbValues : 1);
	
	pGraph->fHighColor = g_new0 (double, 3 * pData->iNbValues);
	if (pAttribute->fHighColor != NULL)
		memcpy (pGraph->fHighColor, pAttribute->fHighColor, 3 * pData->iNbValues * sizeof (double));
	pGraph->fLowColor = g_new0 (double, 3 * pData->iNbValues);
	if (pAttribute->fLowColor != NULL)
		memcpy (pGraph->fLowColor, pAttribute->fLowColor, 3 * pData->iNbValues * sizeof (double));

	int i;
	pGraph->pGradationPatterns = g_new (cairo_pattern_t *, pData->iNbValues);
	for (i = 0; i < pData->iNbValues; i ++)
	{
		pGraph->pGradationPatterns[i] = _cairo_dock_create_graph_pattern (pGraph,
			&pGraph->fLowColor[3*i],
			&pGraph->fHighColor[3*i],
			0.);
	}
	
	pGraph->iRadius = pAttribute->iRadius;
	pGraph->fMargin = pGraph->iRadius * (1. - sqrt(2)/2);
	if (pAttribute->fBackGroundColor != NULL)
		memcpy (pGraph->fBackGroundColor, pAttribute->fBackGroundColor, 4 * sizeof (double));
	pGraph->pBackgroundSurface = _cairo_dock_create_graph_background (pSourceContext,
		iWidth,
		iHeight,
		pGraph->iRadius,
		pGraph->fMargin,
		pGraph->fBackGroundColor,
		pGraph->iType,
		pData->iNbValues / pRenderer->iRank);
	if (g_bUseOpenGL)
		pGraph->iBackgroundTexture = cairo_dock_create_texture_from_surface (pGraph->pBackgroundSurface);
}


static void cairo_dock_reload_graph (CairoDockGraph *pGraph, cairo_t *pSourceContext)
{
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGraph);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int iWidth = pRenderer->iWidth, iHeight = pRenderer->iHeight;
	if (pGraph->pBackgroundSurface != NULL)
		cairo_surface_destroy (pGraph->pBackgroundSurface);
	pGraph->pBackgroundSurface = _cairo_dock_create_graph_background (pSourceContext, iWidth, iHeight, pGraph->iRadius, pGraph->fMargin, pGraph->fBackGroundColor, pGraph->iType, pData->iNbValues / pRenderer->iRank);
	if (pGraph->iBackgroundTexture != 0)
		_cairo_dock_delete_texture (pGraph->iBackgroundTexture);
	if (g_bUseOpenGL)
		pGraph->iBackgroundTexture = cairo_dock_create_texture_from_surface (pGraph->pBackgroundSurface);
	else
		pGraph->iBackgroundTexture = 0;
	int i;
	for (i = 0; i < pGraph->dataRenderer.data.iNbValues; i ++)
	{
		if (pGraph->pGradationPatterns[i] != NULL)
			cairo_pattern_destroy (pGraph->pGradationPatterns[i]);
		pGraph->pGradationPatterns[i] = _cairo_dock_create_graph_pattern (pGraph, &pGraph->fLowColor[3*i], &pGraph->fHighColor[3*i], 0.);
	}
}


static void cairo_dock_free_graph (CairoDockGraph *pGraph)
{
	cd_debug ("");
	if (pGraph == NULL)
		return ;
	if (pGraph->pBackgroundSurface != NULL)
		cairo_surface_destroy (pGraph->pBackgroundSurface);
	if (pGraph->iBackgroundTexture != 0)
		_cairo_dock_delete_texture (pGraph->iBackgroundTexture);
	int i;
	for (i = 0; i < pGraph->dataRenderer.data.iNbValues; i ++)
	{
		if (pGraph->pGradationPatterns[i] != NULL)
			cairo_pattern_destroy (pGraph->pGradationPatterns[i]);
	}
	
	g_free (pGraph->pGradationPatterns);
	g_free (pGraph->fHighColor);
	g_free (pGraph->fLowColor);
	g_free (pGraph);
}



  //////////////////////////////////////////
 /////////////// RENDERER /////////////////
//////////////////////////////////////////
CairoDockGraph *cairo_dock_new_graph (void)
{
	CairoDockGraph *pGraph = g_new0 (CairoDockGraph, 1);
	pGraph->dataRenderer.interface.new				= (CairoDataRendererNewFunc) cairo_dock_new_graph;
	pGraph->dataRenderer.interface.load				= (CairoDataRendererLoadFunc) cairo_dock_load_graph;
	pGraph->dataRenderer.interface.render			= (CairoDataRendererRenderFunc) cairo_dock_render_graph;
	pGraph->dataRenderer.interface.render_opengl	= (CairoDataRendererRenderOpenGLFunc) NULL;
	pGraph->dataRenderer.interface.reload			= (CairoDataRendererReloadFunc) cairo_dock_reload_graph;
	pGraph->dataRenderer.interface.free				= (CairoDataRendererFreeFunc) cairo_dock_free_graph;
	return pGraph;
}

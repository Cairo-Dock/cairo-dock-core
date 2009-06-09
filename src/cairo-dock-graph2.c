/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <cairo-dock-log.h>
#include <cairo-dock-surface-factory.h>
#include <cairo-dock-dock-factory.h>
#include <cairo-dock-renderer-manager.h>
#include <cairo-dock-draw-opengl.h>
#include <cairo-dock-draw.h>
#include <cairo-dock-internal-labels.h>
#include <cairo-dock-internal-icons.h>
#include <cairo-dock-internal-background.h>
#include "cairo-dock-container.h"
#include <cairo-dock-graph2.h>

extern gboolean g_bUseOpenGL;


void cairo_dock_render_graph2 (CairoDockGraph2 *pGraph, cairo_t *pCairoContext)
{
	g_return_if_fail (pGraph != NULL && pCairoContext != NULL);
	g_return_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
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
	fHeight /= pRenderer->iRank;
	
	double *pTabValues;
	cairo_pattern_t *pGradationPattern;
	int i;
	for (i = 0; i < iNbDrawings; i ++)
	{
		
	}
	int k;
	for (k = 0; k < (pGraph->pTabValues2 == NULL ? 1 : 2); k ++)
	{
		if (k == 1)
		{
			pTabValues = pGraph->pTabValues2;
			if (! pGraph->bMixDoubleGraph)
				cairo_translate (pCairoContext,
					0.,
					fHeight);
			pGradationPattern = pGraph->pGradationPattern2;
		}
		else
		{
			pTabValues = pGraph->pTabValues;
			pGradationPattern = pGraph->pGradationPattern;
			
		}
		if (pGradationPattern != NULL)
			cairo_set_source (pCairoContext, pGradationPattern);
		else
			cairo_set_source_rgb (pCairoContext,
				pGraph->fLowColor[0],
				pGraph->fLowColor[1],
				pGraph->fLowColor[2]);
		if (pGraph->iType == CAIRO_DOCK_GRAPH2_LINE || pGraph->iType == CAIRO_DOCK_GRAPH2_PLAIN)
		{
			cairo_set_line_width (pCairoContext, 1);
			cairo_set_line_join (pCairoContext, CAIRO_LINE_JOIN_ROUND);
			int j = pGraph->iCurrentIndex + 1;
			if (j >= pGraph->iNbValues)
				j -= pGraph->iNbValues;
			cairo_move_to (pCairoContext, fMargin, fMargin + (1 - pTabValues[j]) * fHeight);
			int i;
			for (i = 1; i < pGraph->iNbValues; i ++)
			{
				j = pGraph->iCurrentIndex + i + 1;
				if (j >= pGraph->iNbValues)
					j -= pGraph->iNbValues;
				cairo_line_to (pCairoContext,
					fMargin + i * fWidth / pGraph->iNbValues,
					fMargin + (1 - pTabValues[j]) * fHeight);
			}
			if (pGraph->iType == CAIRO_DOCK_GRAPH2_PLAIN)
			{
				i --;
				cairo_line_to (pCairoContext,
					fMargin + i * fWidth / pGraph->iNbValues,
					fMargin + fHeight);
				cairo_line_to (pCairoContext,
					fMargin,
					fMargin + fHeight);
				cairo_close_path (pCairoContext);
				cairo_fill_preserve (pCairoContext);
			}
			cairo_stroke (pCairoContext);
		}
		else if (pGraph->iType == CAIRO_DOCK_GRAPH2_BAR)
		{
			double fBarWidth = fWidth / pGraph->iNbValues / 4;
			cairo_set_line_width (pCairoContext, fBarWidth);
			int i, j;
			for (i = 1; i < pGraph->iNbValues; i ++)
			{
				j = pGraph->iCurrentIndex + i + 1;
				if (j >= pGraph->iNbValues)
					j -= pGraph->iNbValues;
				cairo_move_to (pCairoContext,
					fMargin + i * fWidth / pGraph->iNbValues,
					fMargin + fHeight);
				cairo_rel_line_to (pCairoContext,
					0.,
					- pTabValues[j] * fHeight);
				cairo_stroke (pCairoContext);
			}
		}
		else
		{
			cairo_set_line_width (pCairoContext, 1);
			cairo_set_line_join (pCairoContext, CAIRO_LINE_JOIN_ROUND);
			int j = pGraph->iCurrentIndex + 1;
			if (j >= pGraph->iNbValues)
				j -= pGraph->iNbValues;
			double angle, radius = MIN (fWidth, fHeight)/2;
			angle = 2*G_PI*(-.5/pGraph->iNbValues);
			cairo_move_to (pCairoContext,
				fMargin + fWidth/2 + radius * (pTabValues[j] * cos (angle)),
				fMargin + fHeight/2 + radius * (pTabValues[j] * sin (angle)));
			angle = 2*G_PI*(.5/pGraph->iNbValues);
			cairo_line_to (pCairoContext,
				fMargin + fWidth/2 + radius * (pTabValues[j] * cos (angle)),
				fMargin + fHeight/2 + radius * (pTabValues[j] * sin (angle)));
			int i;
			for (i = 1; i < pGraph->iNbValues; i ++)
			{
				j = pGraph->iCurrentIndex + i + 1;
				if (j >= pGraph->iNbValues)
					j -= pGraph->iNbValues;
				angle = 2*G_PI*((i-.5)/pGraph->iNbValues);
				cairo_line_to (pCairoContext,
					fMargin + fWidth/2 + radius * (pTabValues[j] * cos (angle)),
					fMargin + fHeight/2 + radius * (pTabValues[j] * sin (angle)));
				angle = 2*G_PI*((i+.5)/pGraph->iNbValues);
				cairo_line_to (pCairoContext,
					fMargin + fWidth/2 + radius * (pTabValues[j] * cos (angle)),
					fMargin + fHeight/2 + radius * (pTabValues[j] * sin (angle)));
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


static cairo_surface_t *_cairo_dock_create_graph2_background (cairo_t *pSourceContext, double fWidth, double fHeight, int iRadius, gdouble *pBackGroundColor, CairoDockTypeGraph2 iType)
{
	cairo_surface_t *pBackgroundSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
		CAIRO_CONTENT_COLOR_ALPHA,
		fWidth,
		fHeight);
	cairo_t *pCairoContext = cairo_create (pBackgroundSurface);
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
	cairo_stroke (pCairoContext);
	
	cairo_rectangle (pCairoContext, fRadius, fRadius, (fWidth - 2*fRadius), (fHeight - 2*fRadius));
	cairo_fill (pCairoContext);
	
	
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	double fMargin = fRadius * (1. - sqrt(2)/2);
	cairo_set_source_rgb (pCairoContext, myLabels.quickInfoTextDescription.fBackgroundColor[0], myLabels.quickInfoTextDescription.fBackgroundColor[1], myLabels.quickInfoTextDescription.fBackgroundColor[2]);  // meme couleur que le fond des info-rapides.
	cairo_set_line_width (pCairoContext, 1.);
	if (iType == CAIRO_DOCK_GRAPH2_CIRCLE || iType == CAIRO_DOCK_GRAPH2_CIRCLE_PLAIN)
	{
		cairo_arc (pCairoContext,
			fWidth/2,
			fHeight/2,
			MIN (fWidth, fHeight)/2 - fMargin,
			0.,
			360.);
	}
	else
	{
		cairo_move_to (pCairoContext, fMargin, fMargin);
		cairo_rel_line_to (pCairoContext, 0., fHeight - 2*fMargin);
		cairo_rel_line_to (pCairoContext, fWidth - 2*fMargin, 0.);
	}
	cairo_stroke (pCairoContext);
	
	
	cairo_destroy (pCairoContext);
	return  pBackgroundSurface;
}
static cairo_pattern_t *_cairo_dock_create_graph2_pattern (CairoDockGraph2 *pGraph, gdouble *fLowColor, gdouble *fHighColor, double fOffsetY)
{
	cairo_pattern_t *pGradationPattern = NULL;
	if (fLowColor[0] != fHighColor[0] || fLowColor[1] != fHighColor[1] || fLowColor[2] != fHighColor[2])
	{
		double fMargin = pGraph->fMargin;
		double fWidth = pGraph->fWidth - 2*fMargin;
		double fHeight = pGraph->fHeight - 2*fMargin;
		if (pGraph->pTabValues2 != NULL && ! pGraph->bMixDoubleGraph)
		{
			fHeight /= 2;
		}
		
		if (pGraph->iType == CAIRO_DOCK_GRAPH2_CIRCLE || pGraph->iType == CAIRO_DOCK_GRAPH2_CIRCLE_PLAIN)
		{
			double radius = MIN (fWidth, fHeight)/2;
			pGradationPattern = cairo_pattern_create_radial (fMargin + radius,
				fMargin + radius + fOffsetY,
				0.,
				fMargin + radius,
				fMargin + radius + fOffsetY,
				radius);
		}
		else
			pGradationPattern = cairo_pattern_create_linear (0.,
				fMargin + fHeight + fOffsetY,
				0.,
				fMargin + fOffsetY);
		g_return_val_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS, NULL);	
		
		cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
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
CairoDockGraph2* cairo_dock_create_graph2 (cairo_t *pSourceContext, int iNbValues, CairoDockTypeGraph2 iType, double fWidth, double fHeight, gdouble *pLowColor, gdouble *pHighColor, gdouble *pBackGroundColor, gdouble *pLowColor2, gdouble *pHighColor2)
{
	g_return_val_if_fail (iNbValues > 0, NULL);
	CairoDockGraph2 *pGraph = g_new0 (CairoDockGraph2, 1);
	pGraph->iNbValues = iNbValues;
	pGraph->pTabValues = g_new0 (double, iNbValues);
	if (iType & CAIRO_DOCK_DOUBLE_GRAPH2)
		pGraph->pTabValues2 = g_new0 (double, iNbValues);
	pGraph->iType = iType & CAIRO_DOCK_TYPE_GRAPH2_MASK;
	
	if (pLowColor != NULL)
		memcpy (pGraph->fLowColor, pLowColor, sizeof (pGraph->fLowColor));
	if (pHighColor != NULL)
		memcpy (pGraph->fHighColor, pHighColor, sizeof (pGraph->fHighColor));
	if (pBackGroundColor != NULL)
		memcpy (pGraph->fBackGroundColor, pBackGroundColor, sizeof (pGraph->fBackGroundColor));
	if (pLowColor2 != NULL)
		memcpy (pGraph->fLowColor2, pLowColor2, sizeof (pGraph->fLowColor2));
	if (pHighColor2 != NULL)
		memcpy (pGraph->fHighColor2, pHighColor2, sizeof (pGraph->fHighColor2));
	
	pGraph->fWidth = fWidth;
	pGraph->fHeight = fHeight;
	pGraph->iRadius = myBackground.iDockRadius;  // memes arrondis que le dock et les desklets.
	pGraph->fMargin = pGraph->iRadius * (1. - sqrt(2)/2);
	pGraph->bMixDoubleGraph = (iType & CAIRO_DOCK_MIX_DOUBLE_GRAPH2);
	
	pGraph->pBackgroundSurface = _cairo_dock_create_graph2_background (pSourceContext, fWidth, fHeight, myBackground.iDockRadius, pGraph->fBackGroundColor, iType);
	
	pGraph->pGradationPattern = _cairo_dock_create_graph2_pattern (pGraph, pGraph->fLowColor, pGraph->fHighColor, 0.);
	if (pGraph->pTabValues2 != NULL)
		pGraph->pGradationPattern2 = _cairo_dock_create_graph2_pattern (pGraph, pGraph->fLowColor2, pGraph->fHighColor2, 0.);
	
	return pGraph;
}

void cairo_dock_reload_graph2 (CairoDockGraph2 *pGraph, cairo_t *pSourceContext)
{
	if (pGraph->pBackgroundSurface != NULL)
		cairo_surface_destroy (pGraph->pBackgroundSurface);
	pGraph->pBackgroundSurface = _cairo_dock_create_graph2_background (pSourceContext, pGraph->fWidth, pGraph->fHeight, pGraph->iRadius, pGraph->fBackGroundColor, pGraph->iType);
	
	for (i = 0; i < pGraph->pDataRenderer.iNbValues; i ++)
	{
		if (pGraph->pGradationPattern[i] != NULL)
			cairo_pattern_destroy (pGraph->pGradationPattern[i]);
		pGraph->pGradationPattern[i] = _cairo_dock_create_graph2_pattern (pGraph, &pGraph->fLowColor[3*i], &pGraph->fHighColor[3*i], 0.);
	}
}


void cairo_dock_free_graph2 (CairoDockGraph2 *pGraph)
{
	cd_debug ("");
	if (pGraph == NULL)
		return ;
	if (pGraph->pBackgroundSurface != NULL)
		cairo_surface_destroy (pGraph->pBackgroundSurface);
	int i;
	for (i = 0; i < pGraph->pDataRenderer.iNbValues; i ++)
	{
		if (pGraph->pGradationPattern[i] != NULL)
			cairo_pattern_destroy (pGraph->pGradationPattern[i]);
	}
	g_free (pGraph);
}



void cairo_dock_add_watermark_on_graph2 (cairo_t *pSourceContext, CairoDockGraph2 *pGraph, gchar *cImagePath, double fAlpha)
{
	/*g_return_if_fail (pGraph != NULL && pGraph->pBackgroundSurface != NULL && cImagePath != NULL);
	
	cairo_surface_t *pWatermarkSurface = cairo_dock_create_surface_for_icon (cImagePath, pSourceContext, pGraph->fWidth/2, pGraph->fHeight/2);
	
	cairo_t *pCairoContext = cairo_create (pGraph->pBackgroundSurface);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	
	cairo_set_source_surface (pCairoContext, pWatermarkSurface, pGraph->fWidth/4, pGraph->fHeight/4);
	cairo_paint_with_alpha (pCairoContext, fAlpha);
	
	cairo_destroy (pCairoContext);
	
	cairo_surface_destroy (pWatermarkSurface);*/
}


  //////////////////////////////////////////
 /////////////// RENDERER /////////////////
//////////////////////////////////////////
CairoDockGraph2 *cairo_dock_new_graph2 (void)
{
	CairoDockGraph2 *pGraph = g_new0 (CairoDockGraph2, 1);
	pGauge->dataRenderer.interface.new				= cairo_dock_new_graph2;
	pGauge->dataRenderer.interface.load				= cairo_dock_load_graph2;
	pGauge->dataRenderer.interface.render			= cairo_dock_render_graph2;
	pGauge->dataRenderer.interface.render_opengl	= NULL;
	pGauge->dataRenderer.interface.free				= cairo_dock_free_graph2;
	pGauge->dataRenderer.interface.reload			= cairo_dock_reload_graph2;
	return pGraph;
}
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

#include "cairo-dock-icon-facility.h"  // cairo_dock_get_next_element
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"  // cairo_dock_get_first_drawn_element_linear
#include "cairo-dock-animations.h"  // cairo_dock_calculate_magnitude
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"  // myDocksParam
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-desktop-manager.h"  // g_pFakeTransparencyDesktopBg
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-style-manager.h"
#include "cairo-dock-draw-opengl.h"  // pour cairo_dock_render_one_icon
#include "cairo-dock-overlay.h"  // cairo_dock_draw_icon_overlays_cairo
#include "cairo-dock-draw.h"

extern CairoDockImageBuffer g_pVisibleZoneBuffer;

extern GldiDesktopBackground *g_pFakeTransparencyDesktopBg;
extern gboolean g_bUseOpenGL;


cairo_t * cairo_dock_create_drawing_context_generic (GldiContainer *pContainer)
{
	return gdk_cairo_create (gldi_container_get_gdk_window (pContainer));
}

cairo_t *cairo_dock_create_drawing_context_on_container (GldiContainer *pContainer)
{
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (pContainer);
	g_return_val_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS, FALSE);
	
	///if (myContainersParam.bUseFakeTransparency)
	///{
		if (g_pFakeTransparencyDesktopBg && g_pFakeTransparencyDesktopBg->pSurface)
		{
			if (pContainer->bIsHorizontal)
				cairo_set_source_surface (pCairoContext, g_pFakeTransparencyDesktopBg->pSurface, - pContainer->iWindowPositionX, - pContainer->iWindowPositionY);
			else
				cairo_set_source_surface (pCairoContext, g_pFakeTransparencyDesktopBg->pSurface, - pContainer->iWindowPositionY, - pContainer->iWindowPositionX);
		}
		/**else
			cairo_set_source_rgba (pCairoContext, 0.8, 0.8, 0.8, 0.0);
	}*/
	else
		cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pCairoContext);
	
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	return pCairoContext;
}

cairo_t *cairo_dock_create_drawing_context_on_area (GldiContainer *pContainer, GdkRectangle *pArea, double *fBgColor)
{
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (pContainer);
	g_return_val_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS, pCairoContext);
	
	if (pArea != NULL && (pArea->x > 0 || pArea->y > 0))
	{
		cairo_rectangle (pCairoContext,
			pArea->x,
			pArea->y,
			pArea->width,
			pArea->height);
		cairo_clip (pCairoContext);
	}
	
	///if (myContainersParam.bUseFakeTransparency)
	///{
		if (g_pFakeTransparencyDesktopBg && g_pFakeTransparencyDesktopBg->pSurface)
		{
			if (pContainer->bIsHorizontal)
				cairo_set_source_surface (pCairoContext, g_pFakeTransparencyDesktopBg->pSurface, - pContainer->iWindowPositionX, - pContainer->iWindowPositionY);
			else
				cairo_set_source_surface (pCairoContext, g_pFakeTransparencyDesktopBg->pSurface, - pContainer->iWindowPositionY, - pContainer->iWindowPositionX);
		}
		/**else
			cairo_set_source_rgba (pCairoContext, 0.8, 0.8, 0.8, 0.0);
	}*/
	else if (fBgColor != NULL)
		cairo_set_source_rgba (pCairoContext, fBgColor[0], fBgColor[1], fBgColor[2], fBgColor[3]);
	else
		cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pCairoContext);
	
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	return pCairoContext;
}




double cairo_dock_calculate_extra_width_for_trapeze (double fFrameHeight, double fInclination, double fRadius, double fLineWidth)
{
	if (2 * fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = fInclination * cosa;
	
	double fExtraWidth = fInclination * (fFrameHeight - (FALSE ? 2 : 1-cosa) * fRadius) + fRadius * (FALSE ? 1 : sina);
	return (2 * fExtraWidth + fLineWidth);
	/**double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (myDocksParam.bRoundedBottomCorner ? 2 : 1) * fRadius);
	double fDeltaCornerForLoop = fRadius * cosa + (myDocksParam.bRoundedBottomCorner ? fRadius * (1 + sina) * fInclination : 0);
	
	return (2 * (fLineWidth/2 + fDeltaXForLoop + fDeltaCornerForLoop + myDocksParam.iFrameMargin));*/
}

/**void cairo_dock_draw_rounded_rectangle (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight)
{
	if (2*fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
	double fDockOffsetX = fRadius + fLineWidth/2;
	double fDockOffsetY = 0.;
	cairo_move_to (pCairoContext, fDockOffsetX, fDockOffsetY);
	cairo_rel_line_to (pCairoContext, fFrameWidth, 0);
	//\_________________ Coin haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		fRadius, 0,
		fRadius, fRadius);
	cairo_rel_line_to (pCairoContext, 0, (fFrameHeight + fLineWidth - fRadius * 2));
	//\_________________ Coin bas droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0, fRadius,
		-fRadius, fRadius);

	cairo_rel_line_to (pCairoContext, - fFrameWidth, 0);
	//\_________________ Coin bas gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		-fRadius, 0,
		-fRadius, - fRadius);
	cairo_rel_line_to (pCairoContext, 0, (- fFrameHeight - fLineWidth + fRadius * 2));
	//\_________________ Coin haut gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0, -fRadius,
		fRadius, -fRadius);
	if (fRadius < 1)
		cairo_close_path (pCairoContext);
}*/
void cairo_dock_draw_rounded_rectangle (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight)
{
	if (2*fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
	double fDockOffsetX = fRadius + fLineWidth/2;
	double fDockOffsetY = fLineWidth/2;
	cairo_move_to (pCairoContext, fDockOffsetX, fDockOffsetY);
	cairo_rel_line_to (pCairoContext, fFrameWidth, 0);
	//\_________________ Coin haut droit.
	cairo_arc (pCairoContext,
		fDockOffsetX + fFrameWidth, fDockOffsetY + fRadius,
		fRadius,
		-G_PI/2, 0.);
	cairo_rel_line_to (pCairoContext, 0, (fFrameHeight + fLineWidth - fRadius * 2));
	//\_________________ Coin bas droit.
	cairo_arc (pCairoContext,
		fDockOffsetX + fFrameWidth, fDockOffsetY + fFrameHeight - fLineWidth/2 - fRadius,
		fRadius,
		0., G_PI/2);
	
	cairo_rel_line_to (pCairoContext, - fFrameWidth, 0);
	//\_________________ Coin bas gauche.
	cairo_arc (pCairoContext,
		fDockOffsetX, fDockOffsetY + fFrameHeight - fLineWidth/2 - fRadius,
		fRadius,
		G_PI/2, G_PI);
	
	cairo_rel_line_to (pCairoContext, 0, (- fFrameHeight - fLineWidth + fRadius * 2));
	//\_________________ Coin haut gauche.
	cairo_arc (pCairoContext,
		fDockOffsetX, fDockOffsetY + fRadius,
		fRadius,
		G_PI, -G_PI/2);
	
	if (fRadius < 1)
		cairo_close_path (pCairoContext);
}

static double cairo_dock_draw_frame_horizontal (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, int sens, double fInclination, gboolean bRoundedBottomCorner)  // la largeur est donnee par rapport "au fond".
{
	if (2*fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;
	double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (bRoundedBottomCorner ? 2 : 1-sina) * fRadius);

	cairo_move_to (pCairoContext, fDockOffsetX, fDockOffsetY);

	cairo_rel_line_to (pCairoContext, fFrameWidth, 0);
	//\_________________ Coin haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		fRadius * (1. / cosa - fInclination), 0,
		fRadius * cosa, sens * fRadius * (1 - sina));
	cairo_rel_line_to (pCairoContext, fDeltaXForLoop, sens * (fFrameHeight + fLineWidth - fRadius * (bRoundedBottomCorner ? 2 : 1 - sina)));
	//\_________________ Coin bas droit.
	if (bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			fRadius * (1 + sina) * fInclination, sens * fRadius * (1 + sina),
			-fRadius * cosa, sens * fRadius * (1 + sina));

	cairo_rel_line_to (pCairoContext, - fFrameWidth -  2 * fDeltaXForLoop - (bRoundedBottomCorner ? 0 : 2 * fRadius * cosa), 0);
	//\_________________ Coin bas gauche.
	if (bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			-fRadius * (fInclination + 1. / cosa), 0,
			-fRadius * cosa, -sens * fRadius * (1 + sina));
	cairo_rel_line_to (pCairoContext, fDeltaXForLoop, sens * (- fFrameHeight - fLineWidth + fRadius * (bRoundedBottomCorner ? 2 : 1 - sina)));
	//\_________________ Coin haut gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		fRadius * (1 - sina) * fInclination, -sens * fRadius * (1 - sina),
		fRadius * cosa, -sens * fRadius * (1 - sina));
	if (fRadius < 1)
		cairo_close_path (pCairoContext);
	//return fDeltaXForLoop + fRadius * cosa;
	return fInclination * (fFrameHeight - (FALSE ? 2 : 1-sina) * fRadius) + fRadius * (FALSE ? 1 : cosa);
}
static double cairo_dock_draw_frame_vertical (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, int sens, double fInclination, gboolean bRoundedBottomCorner)
{
	if (2*fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
	double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (bRoundedBottomCorner ? 2 : 1) * fRadius);
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;

	cairo_move_to (pCairoContext, fDockOffsetY, fDockOffsetX);

	cairo_rel_line_to (pCairoContext, 0, fFrameWidth);
	//\_________________ Coin haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0, fRadius * (1. / cosa - fInclination),
		sens * fRadius * (1 - sina), fRadius * cosa);
	cairo_rel_line_to (pCairoContext, sens * (fFrameHeight + fLineWidth - fRadius * (bRoundedBottomCorner ? 2 : 1 - sina)), fDeltaXForLoop);
	//\_________________ Coin bas droit.
	if (bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			sens * fRadius * (1 + sina), fRadius * (1 + sina) * fInclination,
			sens * fRadius * (1 + sina), -fRadius * cosa);

	cairo_rel_line_to (pCairoContext, 0, - fFrameWidth -  2 * fDeltaXForLoop - (bRoundedBottomCorner ? 0 : 2 * fRadius * cosa));
	//\_________________ Coin bas gauche.
	if (bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			0, -fRadius * (fInclination + 1. / cosa),
			-sens * fRadius * (1 + sina), -fRadius * cosa);
	cairo_rel_line_to (pCairoContext, sens * (- fFrameHeight - fLineWidth + fRadius * (bRoundedBottomCorner ? 2 : 1)), fDeltaXForLoop);
	//\_________________ Coin haut gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		-sens * fRadius * (1 - sina), fRadius * (1 - sina) * fInclination,
		-sens * fRadius * (1 - sina), fRadius * cosa);
	if (fRadius < 1)
		cairo_close_path (pCairoContext);
	//return fDeltaXForLoop + fRadius * cosa;
	return fInclination * (fFrameHeight - (FALSE ? 2 : 1-sina) * fRadius) + fRadius * (FALSE ? 1 : cosa);
}
double cairo_dock_draw_frame (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, int sens, double fInclination, gboolean bHorizontal, gboolean bRoundedBottomCorner)
{
	if (bHorizontal)
		return cairo_dock_draw_frame_horizontal (pCairoContext, fRadius, fLineWidth, fFrameWidth, fFrameHeight, fDockOffsetX, fDockOffsetY, sens, fInclination, bRoundedBottomCorner);
	else
		return cairo_dock_draw_frame_vertical (pCairoContext, fRadius, fLineWidth, fFrameWidth, fFrameHeight, fDockOffsetX, fDockOffsetY, sens, fInclination, bRoundedBottomCorner);
}

void cairo_dock_render_decorations_in_frame (cairo_t *pCairoContext, CairoDock *pDock, double fOffsetY, double fOffsetX, double fWidth)
{
	//g_print ("%.2f\n", pDock->fDecorationsOffsetX);
	if (pDock->backgroundBuffer.pSurface == NULL)
		return ;
	cairo_save (pCairoContext);
	
	if (pDock->container.bIsHorizontal)
	{
		cairo_translate (pCairoContext, fOffsetX, fOffsetY);
		cairo_scale (pCairoContext, (double)fWidth / pDock->backgroundBuffer.iWidth, (double)pDock->iDecorationsHeight / pDock->backgroundBuffer.iHeight);  // pDock->container.iWidth
	}
	else
	{
		cairo_translate (pCairoContext, fOffsetY, fOffsetX);
		cairo_scale (pCairoContext, (double)pDock->iDecorationsHeight / pDock->backgroundBuffer.iHeight, (double)fWidth / pDock->backgroundBuffer.iWidth);
	}
	
	cairo_dock_draw_surface (pCairoContext, pDock->backgroundBuffer.pSurface, pDock->backgroundBuffer.iWidth, pDock->backgroundBuffer.iHeight, pDock->container.bDirectionUp, pDock->container.bIsHorizontal, -1.);  // -1 <=> fill_preserve
	
	cairo_restore (pCairoContext);
}


void cairo_dock_set_icon_scale_on_context (cairo_t *pCairoContext, Icon *icon, gboolean bIsHorizontal, G_GNUC_UNUSED double fRatio, gboolean bDirectionUp)
{
	if (bIsHorizontal)
	{
		if (myIconsParam.bConstantSeparatorSize && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		{
			cairo_translate (pCairoContext,
				icon->fWidthFactor * icon->fWidth * (icon->fScale - 1) / 2,
				(bDirectionUp ? 1 * icon->fHeightFactor * icon->fHeight * (icon->fScale - 1) : 0));
			cairo_scale (pCairoContext,
				icon->fWidth / icon->image.iWidth * icon->fWidthFactor/**fRatio * icon->fWidthFactor / (1 + myIconsParam.fAmplitude)*/,
				icon->fHeight / icon->image.iHeight * icon->fHeightFactor/**fRatio * icon->fHeightFactor / (1 + myIconsParam.fAmplitude)*/);
		}
		else
			cairo_scale (pCairoContext,
				icon->fWidth / icon->image.iWidth * icon->fWidthFactor * icon->fScale/**fRatio * icon->fWidthFactor * icon->fScale / (1 + myIconsParam.fAmplitude)*/ * icon->fGlideScale,
				icon->fHeight / icon->image.iHeight * icon->fHeightFactor * icon->fScale/**fRatio * icon->fHeightFactor * icon->fScale / (1 + myIconsParam.fAmplitude)*/ * icon->fGlideScale);
	}
	else
	{
		if (myIconsParam.bConstantSeparatorSize && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		{
			cairo_translate (pCairoContext,
				icon->fHeightFactor * icon->fHeight * (icon->fScale - 1) / 2,
				(bDirectionUp ? 1 * icon->fWidthFactor * icon->fWidth * (icon->fScale - 1) : 0));
			cairo_scale (pCairoContext,
				icon->fHeight / icon->image.iWidth * icon->fHeightFactor/**fRatio * icon->fHeightFactor / (1 + myIconsParam.fAmplitude)*/,
				icon->fWidth / icon->image.iHeight * icon->fWidthFactor/**fRatio * icon->fWidthFactor / (1 + myIconsParam.fAmplitude)*/);
		}
		else
			cairo_scale (pCairoContext,
				icon->fHeight / icon->image.iWidth * icon->fHeightFactor * icon->fScale/**fRatio * icon->fHeightFactor * icon->fScale / (1 + myIconsParam.fAmplitude)*/ * icon->fGlideScale,
				icon->fWidth / icon->image.iHeight * icon->fWidthFactor * icon->fScale/**fRatio * icon->fWidthFactor * icon->fScale / (1 + myIconsParam.fAmplitude)*/ * icon->fGlideScale);
		
	}
}


void cairo_dock_draw_icon_reflect_cairo (Icon *icon, GldiContainer *pContainer, cairo_t *pCairoContext)
{
	if (pContainer->bUseReflect && icon->image.pSurface != NULL)
	{
		cairo_save (pCairoContext);
		double fScale = (myIconsParam.bConstantSeparatorSize && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) ? 1. : icon->fScale);
		
		if (pContainer->bIsHorizontal)
		{
			cairo_translate (pCairoContext,
				0,
				(pContainer->bDirectionUp ?
					icon->fDeltaYReflection + icon->fHeight * fScale :  // go to bottom of icon
					-icon->fDeltaYReflection - icon->fHeight * myIconsParam.fReflectHeightRatio));  // go to top of reflect
			cairo_rectangle (pCairoContext, 0, 0, icon->fWidth * icon->fScale, icon->fHeight * myIconsParam.fReflectHeightRatio);
			if (pContainer->bDirectionUp)
				cairo_translate (pCairoContext, 0, icon->fHeight * icon->fHeightFactor * fScale);
			else
				cairo_translate (pCairoContext, 0, icon->fHeight * icon->fHeightFactor * myIconsParam.fReflectHeightRatio);  // go back to top of icon, since we will flip y axis
		}
		else
		{
			cairo_translate (pCairoContext,
				(pContainer->bDirectionUp ?
					icon->fDeltaYReflection + icon->fHeight * fScale :  // go to bottom of icon
					-icon->fDeltaYReflection - icon->fHeight * myIconsParam.fReflectHeightRatio),  // go to top of reflect
				0);
			cairo_rectangle (pCairoContext, 0, 0, icon->fHeight * myIconsParam.fReflectHeightRatio, icon->fWidth * icon->fScale);
			if (pContainer->bDirectionUp)
				cairo_translate (pCairoContext, icon->fHeight * icon->fHeightFactor * fScale, 0);
			else
				cairo_translate (pCairoContext, icon->fHeight * icon->fHeightFactor * myIconsParam.fReflectHeightRatio, 0);  // go back to top of icon, since we will flip y axis
		}
		cairo_clip (pCairoContext);
		
		cairo_dock_set_icon_scale_on_context (pCairoContext, icon, pContainer->bIsHorizontal, 1., pContainer->bDirectionUp);
		
		if (pContainer->bIsHorizontal)
			cairo_scale (pCairoContext, 1, -1);
		else
			cairo_scale (pCairoContext, -1, 1);
		
		cairo_set_source_surface (pCairoContext, icon->image.pSurface, 0.0, 0.0);
		
		if (myBackendsParam.bDynamicReflection)  // on applique la surface avec un degrade en transparence, ou avec une transparence simple.
		{
			cairo_pattern_t *pGradationPattern;
			if (pContainer->bIsHorizontal)
			{
				if (pContainer->bDirectionUp)
					pGradationPattern = cairo_pattern_create_linear (
						0,
						icon->image.iHeight,
						0,
						icon->image.iHeight * (1 - myIconsParam.fReflectHeightRatio));
				else
					pGradationPattern = cairo_pattern_create_linear (
						0,
						0.,
						0,
						icon->image.iHeight * myIconsParam.fReflectHeightRatio);
				g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);
				
				cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					0.,
					0.,
					0.,
					0.,
					icon->fAlpha * myIconsParam.fAlbedo);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					1.,
					0.,
					0.,
					0.,
					0.);
			}
			else
			{
				if (pContainer->bDirectionUp)
					pGradationPattern = cairo_pattern_create_linear (
						icon->image.iWidth,
						0.,
						icon->image.iWidth * (1-myIconsParam.fReflectHeightRatio),
						0);
				else
					pGradationPattern = cairo_pattern_create_linear (
						0,
						0.,
						icon->image.iWidth * myIconsParam.fReflectHeightRatio,
						0);
				g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);
				
				cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					0.,
					0.,
					0.,
					0.,
					icon->fAlpha * myIconsParam.fAlbedo);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					1.,
					0.,
					0.,
					0.,
					0.);
			}
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
			cairo_mask (pCairoContext, pGradationPattern);

			cairo_pattern_destroy (pGradationPattern);
		}
		else
		{
			cairo_paint_with_alpha (pCairoContext, icon->fAlpha * myIconsParam.fAlbedo);
		}
		cairo_restore (pCairoContext);
	}
}

void cairo_dock_draw_icon_cairo (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext)
{
	//\_____________________ On dessine l'icone.
	if (icon->image.pSurface != NULL)
	{
		cairo_save (pCairoContext);
		
		cairo_dock_set_icon_scale_on_context (pCairoContext, icon, pDock->container.bIsHorizontal, 1., pDock->container.bDirectionUp);
		cairo_set_source_surface (pCairoContext, icon->image.pSurface, 0.0, 0.0);
		if (icon->fAlpha == 1)
			cairo_paint (pCairoContext);
		else
			cairo_paint_with_alpha (pCairoContext, icon->fAlpha);

		cairo_restore (pCairoContext);
	}
	//\_____________________ On dessine son reflet.
	cairo_dock_draw_icon_reflect_cairo (icon, CAIRO_CONTAINER (pDock), pCairoContext);
}

gboolean cairo_dock_render_icon_notification (G_GNUC_UNUSED gpointer pUserData, Icon *icon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext)
{
	if (*bHasBeenRendered)
		return GLDI_NOTIFICATION_LET_PASS;
	if (pCairoContext != NULL)
	{
		if (icon->image.pSurface != NULL)
		{
			cairo_dock_draw_icon_cairo (icon, pDock, pCairoContext);
		}
	}
	else
	{
		if (icon->image.iTexture != 0)
		{
			cairo_dock_draw_icon_opengl (icon, pDock);
		}
	}
	
	*bHasBeenRendered = TRUE;
	return GLDI_NOTIFICATION_LET_PASS;
}

void cairo_dock_render_one_icon (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext, double fDockMagnitude, gboolean bUseText)
{
	int iWidth = pDock->container.iWidth;
	double fRatio = pDock->container.fRatio;
	gboolean bDirectionUp = pDock->container.bDirectionUp;
	gboolean bIsHorizontal = pDock->container.bIsHorizontal;
	
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskbarParam.fVisibleAppliAlpha != 0 && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && !(myTaskbarParam.iMinimizedWindowRenderType == 1 && icon->pAppli->bIsHidden))
	{
		double fAlpha = (icon->pAppli->bIsHidden ? MIN (1 - myTaskbarParam.fVisibleAppliAlpha, 1) : MIN (myTaskbarParam.fVisibleAppliAlpha + 1, 1));
		if (fAlpha != 1)
			icon->fAlpha = fAlpha;  // astuce bidon pour pas multiplier 2 fois.
		/**if (icon->bIsHidden)
			icon->fAlpha *= MIN (1 - myTaskbarParam.fVisibleAppliAlpha, 1);
		else
			icon->fAlpha *= MIN (myTaskbarParam.fVisibleAppliAlpha + 1, 1);*/
		//g_print ("fVisibleAppliAlpha : %.2f & %d => %.2f\n", myTaskbarParam.fVisibleAppliAlpha, icon->bIsHidden, icon->fAlpha);
	}
	
	//\_____________________ On se place sur l'icone.
	double fGlideScale;
	if (icon->fGlideOffset != 0 && (! myIconsParam.bConstantSeparatorSize || ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon)))
	{
		double fPhase =  icon->fPhase + icon->fGlideOffset * icon->fWidth / fRatio / myIconsParam.iSinusoidWidth * G_PI;
		if (fPhase < 0)
		{
			fPhase = 0;
		}
		else if (fPhase > G_PI)
		{
			fPhase = G_PI;
		}
		fGlideScale = (1 + fDockMagnitude * pDock->fMagnitudeMax * myIconsParam.fAmplitude * sin (fPhase)) / icon->fScale;  // c'est un peu hacky ... il faudrait passer l'icone precedente en parametre ...
		if (bDirectionUp)
		{
			if (bIsHorizontal)
				cairo_translate (pCairoContext, 0., (1-fGlideScale)*icon->fHeight*icon->fScale);
			else
				cairo_translate (pCairoContext, (1-fGlideScale)*icon->fHeight*icon->fScale, 0.);
		}
	}
	else
		fGlideScale = 1;
	icon->fGlideScale = fGlideScale;
	
	if (bIsHorizontal)
		cairo_translate (pCairoContext, icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1), icon->fDrawY);
	else
		cairo_translate (pCairoContext, icon->fDrawY, icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1));
	
	cairo_save (pCairoContext);
	
	//\_____________________ On positionne l'icone.
	if (icon->fOrientation != 0)
		cairo_rotate (pCairoContext, icon->fOrientation);
	
	//\_____________________ On dessine l'icone.
	gboolean bIconHasBeenDrawn = FALSE;
	gldi_object_notify (&myIconObjectMgr, NOTIFICATION_PRE_RENDER_ICON, icon, pDock, pCairoContext);
	gldi_object_notify (&myIconObjectMgr, NOTIFICATION_RENDER_ICON, icon, pDock, &bIconHasBeenDrawn, pCairoContext);
	
	cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	if (bUseText && icon->label.pSurface != NULL && icon->iHideLabel == 0
	&& (icon->bPointed || (icon->fScale > 1.01 && ! myIconsParam.bLabelForPointedIconOnly)))  // 1.01 car sin(pi) = 1+epsilon :-/  //  && icon->iAnimationState < CAIRO_DOCK_STATE_CLICKED
	{
		cairo_save (pCairoContext);
		
		double fMagnitude;
		if (myIconsParam.bLabelForPointedIconOnly || pDock->fMagnitudeMax == 0. || myIconsParam.fAmplitude == 0.)
		{
			fMagnitude = fDockMagnitude;  // (icon->fScale - 1) / myIconsParam.fAmplitude / sin (icon->fPhase);  // sin (phi ) != 0 puisque fScale > 1.
		}
		else
		{
			fMagnitude = (icon->fScale - 1) / myIconsParam.fAmplitude;  /// il faudrait diviser par pDock->fMagnitudeMax ...
			fMagnitude = pow (fMagnitude, myIconsParam.fLabelAlphaThreshold);
			///fMagnitude *= (fMagnitude * myIconsParam.fLabelAlphaThreshold + 1) / (myIconsParam.fLabelAlphaThreshold + 1);
		}
		
		int iLabelSize = icon->label.iHeight;
		///int iLabelSize = myIconsParam.iLabelSize;
		int gap = (myDocksParam.iDockLineWidth + myDocksParam.iFrameMargin) * (1 - pDock->fMagnitudeMax) + 1;  // gap between icon and label: let 1px between the icon or the dock's outline
		//g_print ("%d / %d\n", icon->label.iHeight, myIconsParam.iLabelSize),
		cairo_identity_matrix (pCairoContext);  // on positionne les etiquettes sur un pixels entier, sinon ca floute.
		if (bIsHorizontal)
			cairo_translate (pCairoContext, floor (icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1)), floor (icon->fDrawY));
		else
			cairo_translate (pCairoContext, floor (icon->fDrawY), floor (icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1)));
		
		double fOffsetX = (icon->fWidth * icon->fScale - icon->label.iWidth) / 2;
		if (fOffsetX < - icon->fDrawX)  // l'etiquette deborde a gauche.
			fOffsetX = - icon->fDrawX;
		else if (icon->fDrawX + fOffsetX + icon->label.iWidth > iWidth)  // l'etiquette deborde a droite.
			fOffsetX = iWidth - icon->label.iWidth - icon->fDrawX;
		
		if (bIsHorizontal)
		{
			cairo_dock_apply_image_buffer_surface_with_offset (&icon->label, pCairoContext,
				floor (fOffsetX), floor (bDirectionUp ? - iLabelSize - gap : icon->fHeight * icon->fScale + gap), fMagnitude);
		}
		else  // horizontal label on a vertical dock -> draw them next to the icon, vertically centered (like the Parabolic view)
		{
			if (icon->pSubDock && gldi_container_is_visible (CAIRO_CONTAINER (icon->pSubDock)))  // in vertical mode 
			{
				fMagnitude /= 3;
			}
			const int pad = 0;
			double fY = icon->fDrawY;
			int  iMaxWidth = (pDock->container.bDirectionUp ?
				fY - gap - pad :
				pDock->container.iHeight - (fY + icon->fHeight * icon->fScale + gap + pad));
			int iOffsetX = floor (bDirectionUp ? 
				MAX (- iMaxWidth, - gap - pad - icon->label.iWidth):
				icon->fHeight * icon->fScale + gap + pad);
			int iOffsetY = floor (icon->fWidth * icon->fScale/2 - icon->label.iHeight/2);
			if (icon->label.iWidth < iMaxWidth)
			{
				cairo_dock_apply_image_buffer_surface_with_offset (&icon->label, pCairoContext,
					iOffsetX,
					iOffsetY,
					fMagnitude);
			}
			else
			{
				cairo_dock_apply_image_buffer_surface_with_offset_and_limit (&icon->label, pCairoContext, iOffsetX, iOffsetY, fMagnitude, iMaxWidth);
			}
		}
		
		cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
	}
	
	//\_____________________ Draw the overlays on top of that.
	cairo_dock_draw_icon_overlays_cairo (icon, fRatio, pCairoContext);
}


void cairo_dock_render_one_icon_in_desklet (Icon *icon, GldiContainer *pContainer, cairo_t *pCairoContext, gboolean bUseText)
{
	//\_____________________ On dessine l'icone en fonction de son placement, son angle, et sa transparence.
	//g_print ("%s (%.2f;%.2f x %.2f)\n", __func__, icon->fDrawX, icon->fDrawY, icon->fScale);
	if (icon->image.pSurface != NULL)
	{
		cairo_save (pCairoContext);
		
		cairo_translate (pCairoContext, icon->fDrawX, icon->fDrawY);
		cairo_scale (pCairoContext, icon->fWidthFactor * icon->fScale, icon->fHeightFactor * icon->fScale);
		if (icon->fOrientation != 0)
			cairo_rotate (pCairoContext, icon->fOrientation);
		
		cairo_dock_apply_image_buffer_surface_with_offset (&icon->image, pCairoContext, 0, 0, icon->fAlpha);
		
		cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
		
		if (pContainer->bUseReflect)
		{
			cairo_dock_draw_icon_reflect_cairo (icon, pContainer, pCairoContext);
		}
	}
	
	//\_____________________ On dessine les etiquettes.
	if (bUseText && icon->label.pSurface != NULL)
	{
		cairo_save (pCairoContext);
		double fOffsetX = (icon->fWidthFactor * icon->fWidth * icon->fScale - icon->label.iWidth) / 2;
		if (fOffsetX < - icon->fDrawX)
			fOffsetX = - icon->fDrawX;
		else if (icon->fDrawX + fOffsetX + icon->label.iWidth > pContainer->iWidth)
			fOffsetX = pContainer->iWidth - icon->label.iWidth - icon->fDrawX;
		if (icon->fOrientation != 0)
		{
			cairo_rotate (pCairoContext, icon->fOrientation);
		}
		cairo_dock_apply_image_buffer_surface_with_offset (&icon->label, pCairoContext,
			fOffsetX, -icon->label.iHeight, 1.);
		cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
	}
	
	//\_____________________ Draw the overlays on top of that.
	cairo_dock_draw_icon_overlays_cairo (icon, pContainer->fRatio, pCairoContext);
}



void cairo_dock_draw_string (cairo_t *pCairoContext, CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator)
{
	bForceConstantSeparator = bForceConstantSeparator || myIconsParam.bConstantSeparatorSize;
	GList *ic, *pFirstDrawnElement = pDock->icons;
	if (pFirstDrawnElement == NULL || fStringLineWidth <= 0)
		return ;

	cairo_save (pCairoContext);
	cairo_set_tolerance (pCairoContext, 0.5);
	Icon *prev_icon = NULL, *next_icon, *icon;
	double x, y, fCurvature = 0.3;
	if (bIsLoop)
	{
		ic = cairo_dock_get_previous_element (pFirstDrawnElement, pDock->icons);
		prev_icon = ic->data;
	}
	ic = pFirstDrawnElement;
	icon = ic->data;
	GList *next_ic;
	double x1, x2, x3;
	double y1, y2, y3;
	double dx, dy;
	x = icon->fDrawX + icon->fWidth * icon->fScale * icon->fWidthFactor / 2;
	y = icon->fDrawY + icon->fHeight * icon->fScale / 2 + (bForceConstantSeparator && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) ? icon->fHeight * (icon->fScale - 1) / 2 : 0);
	if (pDock->container.bIsHorizontal)
		cairo_move_to (pCairoContext, x, y);
	else
		cairo_move_to (pCairoContext, y, x);
	do
	{
		if (prev_icon != NULL)
		{
			x1 = prev_icon->fDrawX + prev_icon->fWidth * prev_icon->fScale * prev_icon->fWidthFactor / 2;
			y1 = prev_icon->fDrawY + prev_icon->fHeight * prev_icon->fScale / 2 + (bForceConstantSeparator && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (prev_icon) ? prev_icon->fHeight * (prev_icon->fScale - 1) / 2 : 0);
		}
		else
		{
			x1 = x;
			y1 = y;
		}
		prev_icon = icon;

		ic = cairo_dock_get_next_element (ic, pDock->icons);
		if (ic == pFirstDrawnElement && ! bIsLoop)
			break;
		icon = ic->data;
		x2 = icon->fDrawX + icon->fWidth * icon->fScale * icon->fWidthFactor / 2;
		y2 = icon->fDrawY + icon->fHeight * icon->fScale / 2 + (bForceConstantSeparator && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) ? icon->fHeight * (icon->fScale - 1) / 2 : 0);

		dx = x2 - x;
		dy = y2 - y;

		next_ic = cairo_dock_get_next_element (ic, pDock->icons);
		next_icon = (next_ic == pFirstDrawnElement && ! bIsLoop ? NULL : next_ic->data);
		if (next_icon != NULL)
		{
			x3 = next_icon->fDrawX + next_icon->fWidth * next_icon->fScale * next_icon->fWidthFactor / 2;
			y3 = next_icon->fDrawY + next_icon->fHeight * next_icon->fScale / 2 + (bForceConstantSeparator && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (next_icon) ? next_icon->fHeight * (next_icon->fScale - 1) / 2 : 0);
		}
		else
		{
			x3 = x2;
			y3 = y2;
		}

		if (pDock->container.bIsHorizontal)
			cairo_rel_curve_to (pCairoContext,
				(fabs ((x - x1) / (y - y1)) > .35 ? dx * fCurvature : 0),
				(fabs ((x - x1) / (y - y1)) > .35 ? dx * fCurvature * (y - y1) / (x - x1) : 0),
				(fabs ((x3 - x2) / (y3 - y2)) > .35 ? dx * (1 - fCurvature) : dx),
				(fabs ((x3 - x2) / (y3 - y2)) > .35 ? MAX (0, MIN (dy, dy - dx * fCurvature * (y3 - y2) / (x3 - x2))) : dy),
				dx,
				dy);
		else
			cairo_rel_curve_to (pCairoContext,
				(fabs ((x - x1) / (y - y1)) > .35 ? dx * fCurvature * (y - y1) / (x - x1) : 0),
				(fabs ((x - x1) / (y - y1)) > .35 ? dx * fCurvature : 0),
				(fabs ((x3 - x2) / (y3 - y2)) > .35 ? MAX (0, MIN (dy, dy - dx * fCurvature * (y3 - y2) / (x3 - x2))) : dy),
				(fabs ((x3 - x2) / (y3 - y2)) > .35 ? dx * (1 - fCurvature) : dx),
				dy,
				dx);
		x = x2;
		y = y2;
	}
	while (ic != pFirstDrawnElement);

	cairo_set_line_width (pCairoContext, myIconsParam.iStringLineWidth);
	cairo_set_source_rgba (pCairoContext, myIconsParam.fStringColor[0], myIconsParam.fStringColor[1], myIconsParam.fStringColor[2], myIconsParam.fStringColor[3]);
	cairo_stroke (pCairoContext);
	cairo_restore (pCairoContext);
}

void cairo_dock_render_icons_linear (cairo_t *pCairoContext, CairoDock *pDock)
{
	GList *pFirstDrawnElement = cairo_dock_get_first_drawn_element_linear (pDock->icons);
	if (pFirstDrawnElement == NULL)
		return;
	
	double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);  // * pDock->fMagnitudeMax
	Icon *icon;
	GList *ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;

		cairo_save (pCairoContext);
		cairo_dock_render_one_icon (icon, pDock, pCairoContext, fDockMagnitude, TRUE);
		cairo_restore (pCairoContext);

		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);
}



void cairo_dock_draw_surface (cairo_t *pCairoContext, cairo_surface_t *pSurface, int iWidth, int iHeight, gboolean bDirectionUp, gboolean bHorizontal, gdouble fAlpha)
{
	if (bDirectionUp)
	{
		if (bHorizontal)
		{
			cairo_set_source_surface (pCairoContext, pSurface, 0., 0.);
		}
		else
		{
			cairo_rotate (pCairoContext, - G_PI/2);
			cairo_set_source_surface (pCairoContext, pSurface, - iWidth, 0.);
		}
	}
	else
	{
		if (bHorizontal)
		{
			cairo_scale (pCairoContext, 1., -1.);
			cairo_set_source_surface (pCairoContext, pSurface, 0., - iHeight);
		}
		else
		{
			cairo_rotate (pCairoContext, G_PI/2);
			cairo_set_source_surface (pCairoContext, pSurface, 0., - iHeight);
		}
	}
	if (fAlpha == -1)
		cairo_fill_preserve (pCairoContext);
	else if (fAlpha != 1)
		cairo_paint_with_alpha (pCairoContext, fAlpha);
	else
		cairo_paint (pCairoContext);
}



void cairo_dock_render_hidden_dock (cairo_t *pCairoContext, CairoDock *pDock)
{
	//\_____________________ on dessine la zone de rappel.
	if (g_pVisibleZoneBuffer.pSurface != NULL)
	{
		cairo_save (pCairoContext);
		int w = MIN (myDocksParam.iZoneWidth, pDock->container.iWidth);
		int h = MIN (myDocksParam.iZoneHeight, pDock->container.iHeight);
		
		if (pDock->container.bIsHorizontal)
		{
			if (pDock->container.bDirectionUp)
				cairo_translate (pCairoContext, (pDock->container.iWidth - w)/2, pDock->container.iHeight - h);
			else
				cairo_translate (pCairoContext, (pDock->container.iWidth - w)/2, 0.);
		}
		else
		{
			if (pDock->container.bDirectionUp)
				cairo_translate (pCairoContext, pDock->container.iHeight - h, (pDock->container.iWidth - w)/2);
			else
				cairo_translate (pCairoContext, 0., (pDock->container.iWidth - w)/2);
		}
		cairo_dock_draw_surface (pCairoContext, g_pVisibleZoneBuffer.pSurface,
			w,
			h,
			pDock->container.bDirectionUp,  // reverse image with dock.
			pDock->container.bIsHorizontal,
			1.);
		cairo_restore (pCairoContext);
	}
	
	//\_____________________ on dessine les icones demandant l'attention.
	GList *pFirstDrawnElement = cairo_dock_get_first_drawn_element_linear (pDock->icons);
	if (pFirstDrawnElement == NULL)
		return;
	double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
	
	double y;
	Icon *icon;
	GList *ic = pFirstDrawnElement;
	double *pHiddenBgColor;
	const double r = (myDocksParam.bUseDefaultColors ? myStyleParam.iCornerRadius/2 : 4);  // corner radius of the background
	const double gap = 2;  // gap to the screen
	double alpha;
	double dw = (myIconsParam.iIconGap > 2 ? 2 : 0);  // 1px margin around the icons for a better readability (only if icons won't be stuck togather then).
	double w, h;
	do
	{
		icon = ic->data;
		if (icon->bIsDemandingAttention || icon->bAlwaysVisible)
		{
			y = icon->fDrawY;
			icon->fDrawY = (pDock->container.bDirectionUp ? pDock->container.iHeight - icon->fHeight * icon->fScale  - gap: gap);
			
			if (icon->bHasHiddenBg)
			{
				pHiddenBgColor = NULL;
				if (icon->pHiddenBgColor)  // custom bg color
					pHiddenBgColor = icon->pHiddenBgColor;
				else if (! myDocksParam.bUseDefaultColors)  // default bg color
					pHiddenBgColor = myDocksParam.fHiddenBg;
				//if (pHiddenBgColor && pHiddenBgColor[3] != 0)
				{
					cairo_save (pCairoContext);
					if (pHiddenBgColor)
					{
						cairo_set_source_rgba (pCairoContext, pHiddenBgColor[0], pHiddenBgColor[1], pHiddenBgColor[2], pHiddenBgColor[3]);
						alpha = 1.;
					}
					else
					{
						gldi_style_colors_set_bg_color (pCairoContext);
						alpha = .7;
					}
					w = icon->fWidth * icon->fScale;
					h = icon->fHeight * icon->fScale;
					if (pDock->container.bIsHorizontal)
					{
						cairo_translate (pCairoContext, icon->fDrawX - dw / 2, icon->fDrawY);
						cairo_dock_draw_rounded_rectangle (pCairoContext, r, 0, w - 2*r + dw, h);
					}
					else
					{
						cairo_translate (pCairoContext, icon->fDrawY - dw / 2, icon->fDrawX);
						cairo_dock_draw_rounded_rectangle (pCairoContext, r, 0, h - 2*r + dw, w);
					}
					///cairo_fill (pCairoContext);
					cairo_clip (pCairoContext);
					cairo_paint_with_alpha (pCairoContext, alpha * pDock->fPostHideOffset);
					cairo_restore (pCairoContext);
				}
			}
			
			cairo_save (pCairoContext);
			icon->fAlpha = pDock->fPostHideOffset;
			cairo_dock_render_one_icon (icon, pDock, pCairoContext, fDockMagnitude, TRUE);
			cairo_restore (pCairoContext);
			icon->fDrawY = y;
		}
		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);
}

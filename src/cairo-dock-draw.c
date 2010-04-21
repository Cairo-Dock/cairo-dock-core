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
#include <pango/pango.h>
#include <gtk/gtk.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "cairo-dock-icons.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw-opengl.h"  // pour cairo_dock_render_one_icon
#include "cairo-dock-draw.h"


extern CairoDockImageBuffer g_pDockBackgroundBuffer;
/**extern CairoDockImageBuffer g_pIndicatorBuffer;
extern CairoDockImageBuffer g_pActiveIndicatorBuffer;
extern CairoDockImageBuffer g_pClassIndicatorBuffer;*/
extern CairoDockImageBuffer g_pIconBackgroundImageBuffer;
extern CairoDockImageBuffer g_pVisibleZoneBuffer;

extern CairoDockDesktopBackground *g_pFakeTransparencyDesktopBg;
extern gboolean g_bUseGlitz;
extern gboolean g_bUseOpenGL;


cairo_t * cairo_dock_create_drawing_context_generic (CairoContainer *pContainer)
{
#ifdef HAVE_GLITZ
	if (pContainer->pGlitzDrawable)
	{
		//g_print ("creation d'un contexte lie a une surface glitz\n");
		glitz_surface_t* pGlitzSurface;
		cairo_surface_t* pCairoSurface;
		cairo_t* pCairoContext;

		pGlitzSurface = glitz_surface_create (pContainer->pGlitzDrawable,
			pContainer->pGlitzFormat,
			pContainer->iWidth,
			pContainer->iHeight,
			0,
			NULL);

		if (pContainer->pDrawFormat->doublebuffer)
			glitz_surface_attach (pGlitzSurface,
				pContainer->pGlitzDrawable,
				GLITZ_DRAWABLE_BUFFER_BACK_COLOR);
		else
			glitz_surface_attach (pGlitzSurface,
				pContainer->pGlitzDrawable,
				GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

		pCairoSurface = cairo_glitz_surface_create (pGlitzSurface);
		pCairoContext = cairo_create (pCairoSurface);

		cairo_surface_destroy (pCairoSurface);
		glitz_surface_destroy (pGlitzSurface);

		return pCairoContext;
	}
#endif // HAVE_GLITZ
	return gdk_cairo_create (pContainer->pWidget->window);
}

cairo_t *cairo_dock_create_drawing_context_on_container (CairoContainer *pContainer)
{
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (pContainer);
	g_return_val_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS, FALSE);
	
	if (mySystem.bUseFakeTransparency)
	{
		if (g_pFakeTransparencyDesktopBg && g_pFakeTransparencyDesktopBg->pSurface)
			cairo_set_source_surface (pCairoContext, g_pFakeTransparencyDesktopBg->pSurface, - pContainer->iWindowPositionX, - pContainer->iWindowPositionY);
		else
			cairo_set_source_rgba (pCairoContext, 0.8, 0.8, 0.8, 0.0);
	}
	else
		cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pCairoContext);
	
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	return pCairoContext;
}

cairo_t *cairo_dock_create_drawing_context_on_area (CairoContainer *pContainer, GdkRectangle *pArea, double *fBgColor)
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
	
	if (mySystem.bUseFakeTransparency)
	{
		if (g_pFakeTransparencyDesktopBg && g_pFakeTransparencyDesktopBg->pSurface)
			cairo_set_source_surface (pCairoContext, g_pFakeTransparencyDesktopBg->pSurface, - pContainer->iWindowPositionX, - pContainer->iWindowPositionY);
		else
			cairo_set_source_rgba (pCairoContext, 0.8, 0.8, 0.8, 0.0);
	}
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
	/**double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (myBackground.bRoundedBottomCorner ? 2 : 1) * fRadius);
	double fDeltaCornerForLoop = fRadius * cosa + (myBackground.bRoundedBottomCorner ? fRadius * (1 + sina) * fInclination : 0);
	
	return (2 * (fLineWidth/2 + fDeltaXForLoop + fDeltaCornerForLoop + myBackground.iFrameMargin));*/
}

void cairo_dock_draw_rounded_rectangle (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight)
{
	double fDockOffsetX = fRadius + fLineWidth/2;
	double fDockOffsetY = 0.;
	if (2*fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
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
}

static double cairo_dock_draw_frame_horizontal (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, int sens, double fInclination, gboolean bRoundedBottomCorner)  // la largeur est donnee par rapport "au fond".
{
	if (2*fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
	double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (bRoundedBottomCorner ? 2 : 1) * fRadius);
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;

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
	if (g_pDockBackgroundBuffer.pSurface == NULL)
		return ;
	cairo_save (pCairoContext);
	
	if (myBackground.cBackgroundImageFile && !myBackground.bBackgroundImageRepeat)  /// g_pBackgroundSurfaceFull != NULL
	{
		double f = (myBackground.fDecorationSpeed || myBackground.bDecorationsFollowMouse ? .5 : 0.);
		if (pDock->container.bIsHorizontal)
			cairo_translate (pCairoContext, pDock->fDecorationsOffsetX * myBackground.fDecorationSpeed - pDock->container.iWidth * f, fOffsetY);
		else
			cairo_translate (pCairoContext, fOffsetY, pDock->fDecorationsOffsetX * myBackground.fDecorationSpeed - pDock->container.iWidth * f);
		
		cairo_dock_draw_surface (pCairoContext, g_pDockBackgroundBuffer.pSurface, g_pDockBackgroundBuffer.iWidth, g_pDockBackgroundBuffer.iHeight, pDock->container.bDirectionUp, pDock->container.bIsHorizontal, -1.);  // -1 <=> fill_preserve
	}
	else
	{
		if (pDock->container.bIsHorizontal)
		{
			cairo_translate (pCairoContext, pDock->fDecorationsOffsetX * myBackground.fDecorationSpeed + fOffsetX, fOffsetY);
			cairo_scale (pCairoContext, fWidth / g_pDockBackgroundBuffer.iWidth, 1. * pDock->iDecorationsHeight / g_pDockBackgroundBuffer.iHeight);  // pDock->container.iWidth
		}
		else
		{
			cairo_translate (pCairoContext, fOffsetY, pDock->fDecorationsOffsetX * myBackground.fDecorationSpeed + fOffsetX);
			cairo_scale (pCairoContext, 1. * pDock->iDecorationsHeight / g_pDockBackgroundBuffer.iHeight, 1. * fWidth / g_pDockBackgroundBuffer.iWidth);
		}
		
		cairo_dock_draw_surface (pCairoContext, g_pDockBackgroundBuffer.pSurface, g_pDockBackgroundBuffer.iWidth, g_pDockBackgroundBuffer.iHeight, pDock->container.bDirectionUp, pDock->container.bIsHorizontal, -1.);  // -1 <=> fill_preserve
	}
	cairo_restore (pCairoContext);
}


/**static void _cairo_dock_draw_appli_indicator (Icon *icon, cairo_t *pCairoContext, gboolean bIsHorizontal, double fRatio, gboolean bDirectionUp)
{
	cairo_save (pCairoContext);
	///if (icon->fOrientation != 0)
	///	cairo_rotate (pCairoContext, icon->fOrientation);
	double z = 1 + myIcons.fAmplitude;
	double w = g_pIndicatorBuffer.iWidth;
	double h = g_pIndicatorBuffer.iHeight;
	double dy = myIndicators.iIndicatorDeltaY / z;
	if (myIndicators.bLinkIndicatorWithIcon)
	{
		w /= z;
		h /= z;
		if (bIsHorizontal)
		{
			cairo_translate (pCairoContext,
				(icon->fWidth - w * fRatio) * icon->fWidthFactor * icon->fScale / 2,
				(bDirectionUp ?
					(icon->fHeight - (h - dy) * fRatio) * icon->fScale :
					- dy * icon->fScale * fRatio));
			cairo_scale (pCairoContext,
				fRatio * icon->fWidthFactor * icon->fScale / z,
				fRatio * icon->fHeightFactor * icon->fScale / z * (bDirectionUp ? 1 : -1));
		}
		else
		{
			cairo_translate (pCairoContext,
				(bDirectionUp ?
					(icon->fHeight - (h - dy) * fRatio) * icon->fScale :
					- dy * icon->fScale * fRatio),
					(icon->fWidth - (bDirectionUp ? 0 : w * fRatio)) * icon->fWidthFactor * icon->fScale / 2);
			cairo_scale (pCairoContext,
				fRatio * icon->fHeightFactor * icon->fScale / z * (bDirectionUp ? 1 : -1),
				fRatio * icon->fWidthFactor * icon->fScale / z);
		}
	}
	else
	{
		if (bIsHorizontal)
		{
			cairo_translate (pCairoContext,
				(icon->fWidth * icon->fScale - w * fRatio) / 2,
				(bDirectionUp ? 
					(icon->fHeight * icon->fScale - (h - dy) * fRatio) :
					- dy * fRatio));
			cairo_scale (pCairoContext,
				fRatio,
				fRatio);
		}
		else
		{
			cairo_translate (pCairoContext,
				(bDirectionUp ? 
					icon->fHeight * icon->fScale - (h - dy) * fRatio : 
					- dy * 1 * fRatio),
				(icon->fWidth * icon->fScale - w * fRatio) / 2);
			cairo_scale (pCairoContext,
				fRatio,
				fRatio);
		}
	}
	
	cairo_dock_draw_surface (pCairoContext, g_pIndicatorBuffer.pSurface, w, h, bDirectionUp, bIsHorizontal, 1.);
	cairo_restore (pCairoContext);
}

static void _cairo_dock_draw_active_window_indicator (cairo_t *pCairoContext, Icon *icon)
{
	cairo_save (pCairoContext);
	///if (icon->fOrientation != 0)
	///	cairo_rotate (pCairoContext, icon->fOrientation);
	cairo_scale (pCairoContext,
		icon->fWidth * icon->fWidthFactor / g_pActiveIndicatorBuffer.iWidth * icon->fScale,
		icon->fHeight * icon->fHeightFactor / g_pActiveIndicatorBuffer.iHeight * icon->fScale);
	cairo_set_source_surface (pCairoContext, g_pActiveIndicatorBuffer.pSurface, 0., 0.);
	cairo_paint (pCairoContext);
	cairo_restore (pCairoContext);
}

static void _cairo_dock_draw_class_indicator (cairo_t *pCairoContext, Icon *icon, gboolean bIsHorizontal, double fRatio, gboolean bDirectionUp)
{
	double w = g_pClassIndicatorBuffer.iWidth;
	double h = g_pClassIndicatorBuffer.iHeight;
	cairo_save (pCairoContext);
	if (bIsHorizontal)
	{
		if (bDirectionUp)
			cairo_translate (pCairoContext,
				icon->fWidth * icon->fScale - w * fRatio,
				0.);
		else
			cairo_translate (pCairoContext,
				icon->fWidth * icon->fScale - w * fRatio,
				icon->fHeight * icon->fScale - h * fRatio);
	}
	else
	{
		if (bDirectionUp)
			cairo_translate (pCairoContext,
				0.,
				icon->fWidth * icon->fScale - w * fRatio);
		else
			cairo_translate (pCairoContext,
				icon->fHeight * icon->fScale - h * fRatio,
				icon->fWidth * icon->fScale - w * fRatio);
	}
	cairo_scale (pCairoContext, fRatio, fRatio);
	cairo_dock_draw_surface (pCairoContext, g_pClassIndicatorBuffer.pSurface, w, h, bDirectionUp, bIsHorizontal, 1.);
	cairo_restore (pCairoContext);
}*/

void cairo_dock_set_icon_scale_on_context (cairo_t *pCairoContext, Icon *icon, gboolean bIsHorizontal, double fRatio, gboolean bDirectionUp)
{
	if (bIsHorizontal)
	{
		if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
		{
			cairo_translate (pCairoContext,
				1 * icon->fWidthFactor * icon->fWidth * (icon->fScale - 1) / 2,
				(bDirectionUp ? 1 * icon->fHeightFactor * icon->fHeight * (icon->fScale - 1) : 0));
			cairo_scale (pCairoContext,
				fRatio * icon->fWidthFactor / (1 + myIcons.fAmplitude),
				fRatio * icon->fHeightFactor / (1 + myIcons.fAmplitude));
		}
		else
			cairo_scale (pCairoContext,
				fRatio * icon->fWidthFactor * icon->fScale / (1 + myIcons.fAmplitude) * icon->fGlideScale,
				fRatio * icon->fHeightFactor * icon->fScale / (1 + myIcons.fAmplitude) * icon->fGlideScale);
	}
	else
	{
		if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
		{
			cairo_translate (pCairoContext,
				1 * icon->fHeightFactor * icon->fHeight * (icon->fScale - 1) / 2,
				(bDirectionUp ? 1 * icon->fWidthFactor * icon->fWidth * (icon->fScale - 1) : 0));
			cairo_scale (pCairoContext,
				fRatio * icon->fHeightFactor / (1 + myIcons.fAmplitude),
				fRatio * icon->fWidthFactor / (1 + myIcons.fAmplitude));
		}
		else
			cairo_scale (pCairoContext,
				fRatio * icon->fHeightFactor * icon->fScale / (1 + myIcons.fAmplitude) * icon->fGlideScale,
				fRatio * icon->fWidthFactor * icon->fScale / (1 + myIcons.fAmplitude) * icon->fGlideScale);
		
	}
}


void cairo_dock_draw_icon_cairo (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext)
{
	double fRatio = pDock->container.fRatio;
	//\_____________________ On dessine l'icone.
	if (icon->pIconBuffer != NULL)
	{
		cairo_save (pCairoContext);
		
		cairo_dock_set_icon_scale_on_context (pCairoContext, icon, pDock->container.bIsHorizontal, fRatio, pDock->container.bDirectionUp);
		cairo_set_source_surface (pCairoContext, icon->pIconBuffer, 0.0, 0.0);
		if (icon->fAlpha == 1)
			cairo_paint (pCairoContext);
		else
			cairo_paint_with_alpha (pCairoContext, icon->fAlpha);

		cairo_restore (pCairoContext);
	}
	//\_____________________ On dessine son reflet.
	if (pDock->container.bUseReflect && icon->pReflectionBuffer != NULL)  // on dessine les reflets.
	{
		cairo_save (pCairoContext);
		
		if (pDock->container.bIsHorizontal)
		{
			if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
				cairo_translate (pCairoContext, 0, (pDock->container.bDirectionUp ? icon->fDeltaYReflection + icon->fHeight : -icon->fDeltaYReflection - myIcons.fReflectSize * fRatio));
			else
				cairo_translate (pCairoContext, 0, (pDock->container.bDirectionUp ? icon->fDeltaYReflection + icon->fHeight * icon->fScale : -icon->fDeltaYReflection - myIcons.fReflectSize * icon->fScale * fRatio));
		}
		else
		{
			if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
				cairo_translate (pCairoContext, (pDock->container.bDirectionUp ? icon->fDeltaYReflection + icon->fHeight : -icon->fDeltaYReflection - myIcons.fReflectSize * fRatio), 0);
			else
				cairo_translate (pCairoContext, (pDock->container.bDirectionUp ? icon->fDeltaYReflection + icon->fHeight * icon->fScale : -icon->fDeltaYReflection - myIcons.fReflectSize * icon->fScale * fRatio), 0);
		}
		cairo_dock_set_icon_scale_on_context (pCairoContext, icon, pDock->container.bIsHorizontal, fRatio, pDock->container.bDirectionUp);
		
		cairo_set_source_surface (pCairoContext, icon->pReflectionBuffer, 0.0, 0.0);
		
		if (mySystem.bDynamicReflection && icon->fScale > 1)  // on applique la surface avec un degrade en transparence, ou avec une transparence simple.
		{
			cairo_pattern_t *pGradationPattern;
			if (pDock->container.bIsHorizontal)
			{
				pGradationPattern = cairo_pattern_create_linear (0.,
					(pDock->container.bDirectionUp ? 0. : myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude)),
					0.,
					(pDock->container.bDirectionUp ? myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude) / icon->fScale : myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude) * (1. - 1./ icon->fScale)));  // de haut en bas.
				g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);
				
				cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					0.,
					0.,
					0.,
					0.,
					1.);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					1.,
					0.,
					0.,
					0.,
					1 - (icon->fScale - 1) / myIcons.fAmplitude);  // astuce pour ne pas avoir a re-creer la surface de la reflection.
			}
			else
			{
				pGradationPattern = cairo_pattern_create_linear ((pDock->container.bDirectionUp ? 0. : myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude)),
					0.,
					(pDock->container.bDirectionUp ? myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude) / icon->fScale : myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude) * (1. - 1./ icon->fScale)),
					0.);
				g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);
				
				cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					0.,
					0.,
					0.,
					0.,
					1.);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					1.,
					0.,
					0.,
					0.,
					1. - (icon->fScale - 1) / myIcons.fAmplitude);  // astuce pour ne pas avoir a re-creer la surface de la reflection.
			}
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
			cairo_translate (pCairoContext, 0, 0);
			cairo_mask (pCairoContext, pGradationPattern);

			cairo_pattern_destroy (pGradationPattern);
		}
		else
		{
			if (icon->fAlpha == 1)
				cairo_paint (pCairoContext);
			else
				cairo_paint_with_alpha (pCairoContext, icon->fAlpha);
		}
		cairo_restore (pCairoContext);
	}
}

gboolean cairo_dock_render_icon_notification (gpointer pUserData, Icon *icon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext)
{
	if (*bHasBeenRendered)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	if (pCairoContext != NULL)
	{
		if (icon->pIconBuffer != NULL)
		{
			cairo_dock_draw_icon_cairo (icon, pDock, pCairoContext);
		}
	}
	else
	{
		if (icon->iIconTexture != 0)
		{
			cairo_dock_draw_icon_opengl (icon, pDock);
		}
	}
	
	*bHasBeenRendered = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

void cairo_dock_render_one_icon (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext, double fDockMagnitude, gboolean bUseText)
{
	int iWidth = pDock->container.iWidth;
	double fRatio = pDock->container.fRatio;
	gboolean bDirectionUp = pDock->container.bDirectionUp;
	gboolean bIsHorizontal = pDock->container.bIsHorizontal;
	
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.fVisibleAppliAlpha != 0 && ! CAIRO_DOCK_IS_APPLET (icon) && !(icon->iBackingPixmap != 0 && icon->bIsHidden))
	{
		double fAlpha = (icon->bIsHidden ? MIN (1 - myTaskBar.fVisibleAppliAlpha, 1) : MIN (myTaskBar.fVisibleAppliAlpha + 1, 1));
		if (fAlpha != 1)
			icon->fAlpha = fAlpha;  // astuce bidon pour pas multiplier 2 fois.
		/**if (icon->bIsHidden)
			icon->fAlpha *= MIN (1 - myTaskBar.fVisibleAppliAlpha, 1);
		else
			icon->fAlpha *= MIN (myTaskBar.fVisibleAppliAlpha + 1, 1);*/
		//g_print ("fVisibleAppliAlpha : %.2f & %d => %.2f\n", myTaskBar.fVisibleAppliAlpha, icon->bIsHidden, icon->fAlpha);
	}
	
	//\_____________________ On se place sur l'icone.
	double fGlideScale;
	if (icon->fGlideOffset != 0 && (! myIcons.bConstantSeparatorSize || ! CAIRO_DOCK_IS_SEPARATOR (icon)))
	{
		double fPhase =  icon->fPhase + icon->fGlideOffset * icon->fWidth / fRatio / myIcons.iSinusoidWidth * G_PI;
		if (fPhase < 0)
		{
			fPhase = 0;
		}
		else if (fPhase > G_PI)
		{
			fPhase = G_PI;
		}
		fGlideScale = (1 + fDockMagnitude * myIcons.fAmplitude * sin (fPhase)) / icon->fScale;  // c'est un peu hacky ... il faudrait passer l'icone precedente en parametre ...
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
	
	/** //\_____________________ On dessine l'indicateur derriere.
	if (icon->bHasIndicator && ! myIndicators.bIndicatorAbove && g_pIndicatorBuffer.pSurface != NULL)
	{
		_cairo_dock_draw_appli_indicator (icon, pCairoContext, bIsHorizontal, fRatio, bDirectionUp);
	}
	gboolean bIsActive = FALSE;
	Window xActiveId = cairo_dock_get_current_active_window ();
	if (xActiveId != 0 && g_pActiveIndicatorBuffer.pSurface != NULL)
	{
		bIsActive = (icon->Xid == xActiveId);
		if (!bIsActive && icon->pSubDock != NULL)
		{
			Icon *subicon;
			GList *ic;
			for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
			{
				subicon = ic->data;
				if (subicon->Xid == xActiveId)
				{
					bIsActive = TRUE;
					break;
				}
			}
		}
	}
	///if (icon->Xid != 0 && icon->Xid == cairo_dock_get_current_active_window () && ! myIndicators.bActiveIndicatorAbove && g_pActiveIndicatorSurface != NULL)
	if (bIsActive && ! myIndicators.bActiveIndicatorAbove)
	{
		_cairo_dock_draw_active_window_indicator (pCairoContext, icon);
	}*/
	
	//\_____________________ On dessine l'icone.
	gboolean bIconHasBeenDrawn = FALSE;
	cairo_dock_notify (CAIRO_DOCK_PRE_RENDER_ICON, icon, pDock, pCairoContext);
	cairo_dock_notify (CAIRO_DOCK_RENDER_ICON, icon, pDock, &bIconHasBeenDrawn, pCairoContext);
	
	/** //\_____________________ On dessine l'indicateur devant.
	if (bIsActive && myIndicators.bActiveIndicatorAbove)
	{
		_cairo_dock_draw_active_window_indicator (pCairoContext, icon);
	}
	if (icon->bHasIndicator && myIndicators.bIndicatorAbove && g_pIndicatorBuffer.pSurface != NULL)
	{
		_cairo_dock_draw_appli_indicator (icon, pCairoContext, bIsHorizontal, fRatio, bDirectionUp);
	}
	if (icon->pSubDock != NULL && icon->cClass != NULL && g_pClassIndicatorBuffer.pSurface != NULL && icon->Xid == 0)  // le dernier test est de la paranoia.
	{
		_cairo_dock_draw_class_indicator (pCairoContext, icon, bIsHorizontal, fRatio, bDirectionUp);
	}*/
	
	cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	if (bUseText && icon->pTextBuffer != NULL && (icon->fScale > 1.01 || myIcons.fAmplitude == 0) && (! myLabels.bLabelForPointedIconOnly || icon->bPointed))  // 1.01 car sin(pi) = 1+epsilon :-/  //  && icon->iAnimationState < CAIRO_DOCK_STATE_CLICKED
	{
		cairo_save (pCairoContext);
		
		cairo_identity_matrix (pCairoContext);  // on positionne les etiquettes sur un pixels entier, sinon ca floute.
		if (bIsHorizontal)
			cairo_translate (pCairoContext, floor (icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1)), floor (icon->fDrawY));
		else
			cairo_translate (pCairoContext, floor (icon->fDrawY), floor (icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1)));
		
		double fOffsetX = (icon->fWidthFactor * icon->fWidth * icon->fScale - icon->iTextWidth) / 2;
		if (fOffsetX < - icon->fDrawX)
			fOffsetX = - icon->fDrawX;
		else if (icon->fDrawX + fOffsetX + icon->iTextWidth > iWidth)
			fOffsetX = iWidth - icon->iTextWidth - icon->fDrawX;
		
		if (icon->fOrientation != 0 && ! mySystem.bTextAlwaysHorizontal)
			cairo_rotate (pCairoContext, icon->fOrientation);
		
		if (! bIsHorizontal && mySystem.bTextAlwaysHorizontal)
		{
			cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				floor ((pDock->container.bDirectionUp ? -myLabels.iLabelSize : - (pDock->container.bUseReflect ? myIcons.fReflectSize : 0.)) - myLabels.iconTextDescription.iMargin + 1),
				0.);
		}
		else if (bIsHorizontal)
			cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				floor (fOffsetX),
				floor (bDirectionUp ? -myLabels.iLabelSize : icon->fHeight * icon->fScale/* - icon->fTextYOffset*/));
		else
		{
			cairo_translate (pCairoContext, icon->iTextWidth/2, icon->iTextHeight/2);
			cairo_rotate (pCairoContext, bDirectionUp ? - G_PI/2 : G_PI/2);
			cairo_translate (pCairoContext, -icon->iTextWidth/2, -icon->iTextHeight/2);
			cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				floor (bDirectionUp ? -myLabels.iLabelSize : icon->fHeight * icon->fScale/* - icon->fTextYOffset*/),
				floor (fOffsetX));
		}
		double fMagnitude;
		if (myLabels.bLabelForPointedIconOnly)
		{
			fMagnitude = fDockMagnitude;  // (icon->fScale - 1) / myIcons.fAmplitude / sin (icon->fPhase);  // sin (phi ) != 0 puisque fScale > 1.
		}
		else
		{
			fMagnitude = (icon->fScale - 1) / myIcons.fAmplitude;  /// il faudrait diviser par pDock->fMagnitudeMax ...
			fMagnitude = pow (fMagnitude, mySystem.fLabelAlphaThreshold);
			///fMagnitude *= (fMagnitude * mySystem.fLabelAlphaThreshold + 1) / (mySystem.fLabelAlphaThreshold + 1);
		}
		if (fMagnitude > .1)
			cairo_paint_with_alpha (pCairoContext, fMagnitude);
		cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
	}
	
	//\_____________________ On dessine les infos additionnelles.
	if (icon->pQuickInfoBuffer != NULL)
	{
		cairo_translate (pCairoContext,
			(- icon->iQuickInfoWidth * fRatio + icon->fWidthFactor * icon->fWidth) / 2 * icon->fScale,
			(icon->fHeight - icon->iQuickInfoHeight * fRatio) * icon->fScale);
		
		cairo_scale (pCairoContext,
			fRatio * icon->fScale / (1 + myIcons.fAmplitude) * 1,
			fRatio * icon->fScale / (1 + myIcons.fAmplitude) * 1);
		
		cairo_set_source_surface (pCairoContext,
			icon->pQuickInfoBuffer,
			0,
			0);
		if (icon->fAlpha == 1)
			cairo_paint (pCairoContext);
		else
			cairo_paint_with_alpha (pCairoContext, icon->fAlpha);
	}
}


void cairo_dock_render_one_icon_in_desklet (Icon *icon, cairo_t *pCairoContext, gboolean bUseReflect, gboolean bUseText, int iWidth)
{
	//\_____________________ On dessine l'icone en fonction de son placement, son angle, et sa transparence.
	//g_print ("%s (%.2f;%.2f x %.2f)\n", __func__, icon->fDrawX, icon->fDrawY, icon->fScale);
	cairo_translate (pCairoContext, icon->fDrawX, icon->fDrawY);
	cairo_save (pCairoContext);
	cairo_scale (pCairoContext, icon->fWidthFactor * icon->fScale, icon->fHeightFactor * icon->fScale);
	if (icon->fOrientation != 0)
		cairo_rotate (pCairoContext, icon->fOrientation);
	
	double fAlpha = icon->fAlpha;
	
	if (bUseReflect && icon->pReflectionBuffer != NULL)  // on dessine les reflets.
	{
		if (icon->pIconBuffer != NULL)
			cairo_set_source_surface (pCairoContext, icon->pIconBuffer, 0.0, 0.0);
		if (fAlpha == 1)
			cairo_paint (pCairoContext);
		else
			cairo_paint_with_alpha (pCairoContext, fAlpha);

		cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).

		cairo_save (pCairoContext);
		cairo_translate (pCairoContext, 0, - icon->fDeltaYReflection + icon->fHeight * icon->fScale);
		cairo_scale (pCairoContext, icon->fWidthFactor * icon->fScale, icon->fHeightFactor * icon->fScale);
		
		cairo_set_source_surface (pCairoContext, icon->pReflectionBuffer, 0.0, 0.0);
		
		if (mySystem.bDynamicReflection && icon->fScale != 1)
		{
			cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0.,
				0.,
				0.,
				myIcons.fReflectSize / icon->fScale);  // de haut en bas.
			g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);
			
			cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
			cairo_pattern_add_color_stop_rgba (pGradationPattern,
				0.,
				0.,
				0.,
				0.,
				1.);  // astuce pour ne pas avoir a re-creer la surface de la reflection.
			cairo_pattern_add_color_stop_rgba (pGradationPattern,
				1.,
				0.,
				0.,
				0.,
				0.);
			
			cairo_save (pCairoContext);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
			cairo_translate (pCairoContext, 0, 0);
			cairo_mask (pCairoContext, pGradationPattern);
			cairo_restore (pCairoContext);
			
			cairo_pattern_destroy (pGradationPattern);
		}
		else
		{
			if (fAlpha == 1)
				cairo_paint (pCairoContext);
			else
				cairo_paint_with_alpha (pCairoContext, fAlpha);
		}
	}
	else  // on dessine l'icone tout simplement.
	{
		if (icon->pIconBuffer != NULL)
			cairo_set_source_surface (pCairoContext, icon->pIconBuffer, 0.0, 0.0);
		if (fAlpha == 1)
			cairo_paint (pCairoContext);
		else
			cairo_paint_with_alpha (pCairoContext, fAlpha);
	}
	
	cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	if (bUseText && icon->pTextBuffer != NULL)
	{
		cairo_save (pCairoContext);
		double fOffsetX = (icon->fWidthFactor * icon->fWidth * icon->fScale - icon->iTextWidth) / 2;
		if (fOffsetX < - icon->fDrawX)
			fOffsetX = - icon->fDrawX;
		else if (icon->fDrawX + fOffsetX + icon->iTextWidth > iWidth)
			fOffsetX = iWidth - icon->iTextWidth - icon->fDrawX;
		if (icon->fOrientation != 0)
		{
			cairo_rotate (pCairoContext, icon->fOrientation);
		}
		cairo_set_source_surface (pCairoContext,
			icon->pTextBuffer,
			fOffsetX,
			-myLabels.iLabelSize);
		cairo_paint (pCairoContext);
		cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
	}
	
	///if (icon->bHasIndicator)
	///	_cairo_dock_draw_appli_indicator (icon, pCairoContext, TRUE, 1., TRUE);
	
	//\_____________________ On dessine les infos additionnelles.
	if (icon->pQuickInfoBuffer != NULL)
	{
		cairo_translate (pCairoContext,
			//-icon->fQuickInfoXOffset + icon->fWidth / 2,
			//icon->fHeight - icon->fQuickInfoYOffset);
			(- icon->iQuickInfoWidth + icon->fWidth) / 2 * icon->fScale,
			(icon->fHeight - icon->iQuickInfoHeight) * icon->fScale);
		
		cairo_scale (pCairoContext,
			icon->fScale,
			icon->fScale);
		
		cairo_set_source_surface (pCairoContext,
			icon->pQuickInfoBuffer,
			0,
			0);
		cairo_paint (pCairoContext);
	}
}



void cairo_dock_draw_string (cairo_t *pCairoContext, CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator)
{
	bForceConstantSeparator = bForceConstantSeparator || myIcons.bConstantSeparatorSize;
	GList *ic, *pFirstDrawnElement = (pDock->pFirstDrawnElement != NULL ? pDock->pFirstDrawnElement : pDock->icons);
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
	y = icon->fDrawY + icon->fHeight * icon->fScale / 2 + (bForceConstantSeparator && CAIRO_DOCK_IS_SEPARATOR (icon) ? icon->fHeight * (icon->fScale - 1) / 2 : 0);
	if (pDock->container.bIsHorizontal)
		cairo_move_to (pCairoContext, x, y);
	else
		cairo_move_to (pCairoContext, y, x);
	do
	{
		if (prev_icon != NULL)
		{
			x1 = prev_icon->fDrawX + prev_icon->fWidth * prev_icon->fScale * prev_icon->fWidthFactor / 2;
			y1 = prev_icon->fDrawY + prev_icon->fHeight * prev_icon->fScale / 2 + (bForceConstantSeparator && CAIRO_DOCK_IS_SEPARATOR (prev_icon) ? prev_icon->fHeight * (prev_icon->fScale - 1) / 2 : 0);
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
		y2 = icon->fDrawY + icon->fHeight * icon->fScale / 2 + (bForceConstantSeparator && CAIRO_DOCK_IS_SEPARATOR (icon) ? icon->fHeight * (icon->fScale - 1) / 2 : 0);

		dx = x2 - x;
		dy = y2 - y;

		next_ic = cairo_dock_get_next_element (ic, pDock->icons);
		next_icon = (next_ic == pFirstDrawnElement && ! bIsLoop ? NULL : next_ic->data);
		if (next_icon != NULL)
		{
			x3 = next_icon->fDrawX + next_icon->fWidth * next_icon->fScale * next_icon->fWidthFactor / 2;
			y3 = next_icon->fDrawY + next_icon->fHeight * next_icon->fScale / 2 + (bForceConstantSeparator && CAIRO_DOCK_IS_SEPARATOR (next_icon) ? next_icon->fHeight * (next_icon->fScale - 1) / 2 : 0);
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

	cairo_set_line_width (pCairoContext, myIcons.iStringLineWidth);
	cairo_set_source_rgba (pCairoContext, myIcons.fStringColor[0], myIcons.fStringColor[1], myIcons.fStringColor[2], myIcons.fStringColor[3]);
	cairo_stroke (pCairoContext);
	cairo_restore (pCairoContext);
}

void cairo_dock_render_icons_linear (cairo_t *pCairoContext, CairoDock *pDock)
{
	//GList *pFirstDrawnElement = (pDock->pFirstDrawnElement != NULL ? pDock->pFirstDrawnElement : pDock->icons);
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
		int w = MIN (myAccessibility.iVisibleZoneWidth, pDock->container.iWidth);
		int h = MIN (myAccessibility.iVisibleZoneHeight, pDock->container.iHeight);
		
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
			(myBackground.bReverseVisibleImage ? pDock->container.bDirectionUp : TRUE),
			pDock->container.bIsHorizontal,
			myBackground.fVisibleZoneAlpha);
		cairo_restore (pCairoContext);
	}
	
	//\_____________________ on dessine les icones demandant l'attention.
	//if (myTaskBar.cAnimationOnDemandsAttention)
	{
		GList *pFirstDrawnElement = cairo_dock_get_first_drawn_element_linear (pDock->icons);
		if (pFirstDrawnElement == NULL)
			return;
		double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
		
		double y;
		Icon *icon;
		GList *ic = pFirstDrawnElement;
		do
		{
			icon = ic->data;
			if (icon->bIsDemandingAttention || icon->bAlwaysVisible)
			{
				y = icon->fDrawY;
				icon->fDrawY = (pDock->container.bDirectionUp ? pDock->container.iHeight - icon->fHeight * icon->fScale : 0.);
				cairo_save (pCairoContext);
				cairo_dock_render_one_icon (icon, pDock, pCairoContext, fDockMagnitude, TRUE);
				cairo_restore (pCairoContext);
				icon->fDrawY = y;
			}
			ic = cairo_dock_get_next_element (ic, pDock->icons);
		} while (ic != pFirstDrawnElement);
	}
}

/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
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
#include "cairo-dock-callbacks.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-hidden-dock.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-draw.h"

extern gint g_iScreenWidth[2];
extern gint g_iScreenHeight[2];
extern int g_iScreenOffsetX, g_iScreenOffsetY;

extern gboolean bDirectionUp;
extern double g_fBackgroundImageWidth, g_fBackgroundImageHeight;
extern cairo_surface_t *g_pBackgroundSurface;
extern cairo_surface_t *g_pBackgroundSurfaceFull;

extern cairo_surface_t *g_pVisibleZoneSurface;

extern cairo_surface_t *g_pIndicatorSurface;
extern double g_fIndicatorWidth, g_fIndicatorHeight;

extern cairo_surface_t *g_pActiveIndicatorSurface;
extern double g_fActiveIndicatorWidth, g_fActiveIndicatorHeight;

extern cairo_surface_t *g_pClassIndicatorSurface;
extern double g_fClassIndicatorWidth, g_fClassIndicatorHeight;

extern cairo_surface_t *g_pDesktopBgSurface;
extern gboolean g_bUseGlitz;
extern gboolean g_bUseOpenGL;
extern gboolean g_bEasterEggs;


void cairo_dock_set_colormap_for_window (GtkWidget *pWidget)
{
	GdkScreen* pScreen = gtk_widget_get_screen (pWidget);
	GdkColormap* pColormap = gdk_screen_get_rgba_colormap (pScreen);
	if (!pColormap)
		pColormap = gdk_screen_get_rgb_colormap (pScreen);
	
	/// est-ce que ca vaut le coup de plutot faire ca avec le visual obtenu pour l'openGL ?...
	//GdkVisual *visual = gdkx_visual_get (pVisInfo->visualid);
	//pColormap = gdk_colormap_new (visual, TRUE);

	gtk_widget_set_colormap (pWidget, pColormap);
}

void cairo_dock_set_colormap (CairoContainer *pContainer)
{
	GdkColormap* pColormap;
#ifdef HAVE_GLITZ
	if (g_bUseGlitz)
	{
		glitz_drawable_format_t templ, *format;
		unsigned long	    mask = GLITZ_FORMAT_DOUBLEBUFFER_MASK;
		XVisualInfo		    *vinfo = NULL;
		int			    screen = 0;
		GdkVisual		    *visual;
		GdkDisplay		    *gdkdisplay;
		Display		    *xdisplay;

		templ.doublebuffer = 1;
		gdkdisplay = gtk_widget_get_display (pContainer->pWidget);
		xdisplay   = gdk_x11_display_get_xdisplay (gdkdisplay);

		int i = 0;
		do
		{
			format = glitz_glx_find_window_format (xdisplay,
				screen,
				mask,
				&templ,
				i++);
			if (format)
			{
				vinfo = glitz_glx_get_visual_info_from_format (xdisplay,
					screen,
					format);
				if (vinfo->depth == 32)
				{
					pContainer->pDrawFormat = format;
					break;
				}
				else if (!pContainer->pDrawFormat)
				{
					pContainer->pDrawFormat = format;
				}
			}
		} while (format);

		if (! pContainer->pDrawFormat)
		{
			cd_warning ("no double buffered GLX visual");
		}
		else
		{
			vinfo = glitz_glx_get_visual_info_from_format (xdisplay,
				screen,
				pContainer->pDrawFormat);

			visual = gdkx_visual_get (vinfo->visualid);
			pColormap = gdk_colormap_new (visual, TRUE);

			gtk_widget_set_colormap (pContainer->pWidget, pColormap);
			gtk_widget_set_double_buffered (pContainer->pWidget, FALSE);
			return ;
		}
	}
#endif
	
	cairo_dock_set_colormap_for_window (pContainer->pWidget);
}


double cairo_dock_get_current_dock_width_linear (CairoDock *pDock)
{
	if (pDock->icons == NULL)
		//return 2 * myBackground.iDockRadius + myBackground.iDockLineWidth + 2 * myBackground.iFrameMargin;
		return 1 + 2 * myBackground.iFrameMargin;

	Icon *pLastIcon = cairo_dock_get_last_drawn_icon (pDock);
	Icon *pFirstIcon = cairo_dock_get_first_drawn_icon (pDock);
	double fWidth = pLastIcon->fX - pFirstIcon->fX + pLastIcon->fWidth * pLastIcon->fScale + 2 * myBackground.iFrameMargin;  //  + 2 * myBackground.iDockRadius + myBackground.iDockLineWidth + 2 * myBackground.iFrameMargin

	return fWidth;
}
/*void cairo_dock_get_current_dock_width_height (CairoDock *pDock, double *fWidth, double *fHeight)
{
	if (pDock->icons == NULL)
	{
		*fWidth = 2 * myBackground.iDockRadius + myBackground.iDockLineWidth + 2 * myBackground.iFrameMargin;
		*fHeight = 2 * myBackground.iDockRadius + myBackground.iDockLineWidth + 2 * myBackground.iFrameMargin;
	}

	Icon *pLastIcon = cairo_dock_get_last_drawn_icon (pDock);
	Icon *pFirstIcon = cairo_dock_get_first_drawn_icon (pDock);
	*fWidth = pLastIcon->fX - pFirstIcon->fX + 2 * myBackground.iDockRadius + myBackground.iDockLineWidth + 2 * myBackground.iFrameMargin + pLastIcon->fWidth * pLastIcon->fScale;
	if (pLastIcon->fY > pFirstIcon->fY)
		*fHeight = MAX (pLastIcon->fY + pLastIcon->fHeight * pLastIcon->fScale - pFirstIcon->fY, pFirstIcon->fHeight * pFirstIcon->fScale);
	else
		*fHeight = MAX (pFirstIcon->fY + pFirstIcon->fHeight * pFirstIcon->fScale - pLastIcon->fY, pLastIcon->fHeight * pLastIcon->fScale);
}*/

cairo_t * cairo_dock_create_context_from_window (CairoContainer *pContainer)
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

static double cairo_dock_draw_frame_horizontal (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, int sens, double fInclination)  // la largeur est donnee par rapport "au fond".
{
	if (2*fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
	double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (myBackground.bRoundedBottomCorner ? 2 : 1) * fRadius);
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;

	cairo_move_to (pCairoContext, fDockOffsetX, fDockOffsetY);

	cairo_rel_line_to (pCairoContext, fFrameWidth, 0);
	//\_________________ Coin haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		fRadius * (1. / cosa - fInclination), 0,
		fRadius * cosa, sens * fRadius * (1 - sina));
	cairo_rel_line_to (pCairoContext, fDeltaXForLoop, sens * (fFrameHeight + fLineWidth - fRadius * (myBackground.bRoundedBottomCorner ? 2 : 1 - sina)));
	//\_________________ Coin bas droit.
	if (myBackground.bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			fRadius * (1 + sina) * fInclination, sens * fRadius * (1 + sina),
			-fRadius * cosa, sens * fRadius * (1 + sina));

	cairo_rel_line_to (pCairoContext, - fFrameWidth -  2 * fDeltaXForLoop - (myBackground.bRoundedBottomCorner ? 0 : 2 * fRadius * cosa), 0);
	//\_________________ Coin bas gauche.
	if (myBackground.bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			-fRadius * (fInclination + 1. / cosa), 0,
			-fRadius * cosa, -sens * fRadius * (1 + sina));
	cairo_rel_line_to (pCairoContext, fDeltaXForLoop, sens * (- fFrameHeight - fLineWidth + fRadius * (myBackground.bRoundedBottomCorner ? 2 : 1 - sina)));
	//\_________________ Coin haut gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		fRadius * (1 - sina) * fInclination, -sens * fRadius * (1 - sina),
		fRadius * cosa, -sens * fRadius * (1 - sina));
	if (fRadius < 1)
		cairo_close_path (pCairoContext);
	return fDeltaXForLoop + fRadius * cosa;
}
static double cairo_dock_draw_frame_vertical (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, int sens, double fInclination)
{
	if (2*fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
	double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (myBackground.bRoundedBottomCorner ? 2 : 1) * fRadius);
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;

	cairo_move_to (pCairoContext, fDockOffsetY, fDockOffsetX);

	cairo_rel_line_to (pCairoContext, 0, fFrameWidth);
	//\_________________ Coin haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0, fRadius * (1. / cosa - fInclination),
		sens * fRadius * (1 - sina), fRadius * cosa);
	cairo_rel_line_to (pCairoContext, sens * (fFrameHeight + fLineWidth - fRadius * (myBackground.bRoundedBottomCorner ? 2 : 1 - sina)), fDeltaXForLoop);
	//\_________________ Coin bas droit.
	if (myBackground.bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			sens * fRadius * (1 + sina), fRadius * (1 + sina) * fInclination,
			sens * fRadius * (1 + sina), -fRadius * cosa);

	cairo_rel_line_to (pCairoContext, 0, - fFrameWidth -  2 * fDeltaXForLoop - (myBackground.bRoundedBottomCorner ? 0 : 2 * fRadius * cosa));
	//\_________________ Coin bas gauche.
	if (myBackground.bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			0, -fRadius * (fInclination + 1. / cosa),
			-sens * fRadius * (1 + sina), -fRadius * cosa);
	cairo_rel_line_to (pCairoContext, sens * (- fFrameHeight - fLineWidth + fRadius * (myBackground.bRoundedBottomCorner ? 2 : 1)), fDeltaXForLoop);
	//\_________________ Coin haut gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		-sens * fRadius * (1 - sina), fRadius * (1 - sina) * fInclination,
		-sens * fRadius * (1 - sina), fRadius * cosa);
	if (fRadius < 1)
		cairo_close_path (pCairoContext);
	return fDeltaXForLoop + fRadius * cosa;
}
double cairo_dock_draw_frame (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, int sens, double fInclination, gboolean bHorizontal)
{
	if (bHorizontal)
		return cairo_dock_draw_frame_horizontal (pCairoContext, fRadius, fLineWidth, fFrameWidth, fFrameHeight, fDockOffsetX, fDockOffsetY, sens, fInclination);
	else
		return cairo_dock_draw_frame_vertical (pCairoContext, fRadius, fLineWidth, fFrameWidth, fFrameHeight, fDockOffsetX, fDockOffsetY, sens, fInclination);
}

void cairo_dock_render_decorations_in_frame (cairo_t *pCairoContext, CairoDock *pDock, double fOffsetY, double fOffsetX, double fWidth)
{
	//g_print ("%.2f\n", pDock->fDecorationsOffsetX);
	if (g_pBackgroundSurfaceFull != NULL)
	{
		cairo_save (pCairoContext);
		
		if (pDock->bHorizontalDock)
			cairo_translate (pCairoContext, pDock->fDecorationsOffsetX * mySystem.fStripesSpeedFactor - pDock->iCurrentWidth * 0.5, fOffsetY);
		else
			cairo_translate (pCairoContext, fOffsetY, pDock->fDecorationsOffsetX * mySystem.fStripesSpeedFactor - pDock->iCurrentWidth * 0.5);
		
		
		cairo_surface_t *pSurface = g_pBackgroundSurfaceFull;
		cairo_dock_draw_surface (pCairoContext, pSurface, g_fBackgroundImageWidth, g_fBackgroundImageHeight, pDock->bDirectionUp, pDock->bHorizontalDock, -1.);  // -1 <=> fill_preserve
		
		cairo_restore (pCairoContext);
	}
	else if (g_pBackgroundSurface != NULL)
	{
		cairo_save (pCairoContext);
		
		if (pDock->bHorizontalDock)
		{
			cairo_translate (pCairoContext, pDock->fDecorationsOffsetX * mySystem.fStripesSpeedFactor + fOffsetX, fOffsetY);
			cairo_scale (pCairoContext, 1. * fWidth / g_fBackgroundImageWidth, 1. * pDock->iDecorationsHeight / g_fBackgroundImageHeight);  // pDock->iCurrentWidth
		}
		else
		{
			cairo_translate (pCairoContext, fOffsetY, pDock->fDecorationsOffsetX * mySystem.fStripesSpeedFactor + fOffsetX);
			cairo_scale (pCairoContext, 1. * pDock->iDecorationsHeight / g_fBackgroundImageHeight, 1. * fWidth / g_fBackgroundImageWidth);
		}
		
		cairo_surface_t *pSurface = g_pBackgroundSurface;
		cairo_dock_draw_surface (pCairoContext, pSurface, g_fBackgroundImageWidth, g_fBackgroundImageHeight, pDock->bDirectionUp, pDock->bHorizontalDock, -1.);  // -1 <=> fill_preserve
		
		cairo_restore (pCairoContext);
	}
}


static void _cairo_dock_draw_appli_indicator (Icon *icon, cairo_t *pCairoContext, gboolean bHorizontalDock, double fRatio, gboolean bDirectionUp)
{
	cairo_save (pCairoContext);
	if (icon->fOrientation != 0)
		cairo_rotate (pCairoContext, icon->fOrientation);
	if (myIndicators.bLinkIndicatorWithIcon)
	{
		if (bHorizontalDock)
		{
			cairo_translate (pCairoContext,
				(icon->fWidth - g_fIndicatorWidth * fRatio) * icon->fWidthFactor * icon->fScale / 2,
				(bDirectionUp ? 
					(icon->fHeight - (g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + myIcons.fAmplitude)) * fRatio) * icon->fScale :
					(g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + myIcons.fAmplitude)) * icon->fScale * fRatio));
			cairo_scale (pCairoContext,
				fRatio * icon->fWidthFactor * icon->fScale / (1 + myIcons.fAmplitude),
				fRatio * icon->fHeightFactor * icon->fScale / (1 + myIcons.fAmplitude) * (bDirectionUp ? 1 : -1));
		}
		else
		{
			cairo_translate (pCairoContext,
				(bDirectionUp ? 
					(icon->fHeight - (g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + myIcons.fAmplitude)) * fRatio) * icon->fScale : 
					(g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + myIcons.fAmplitude)) * icon->fScale * fRatio),
					(icon->fWidth - g_fIndicatorWidth * fRatio) * icon->fWidthFactor * icon->fScale / 2);
			cairo_scale (pCairoContext,
				fRatio * icon->fHeightFactor * icon->fScale / (1 + myIcons.fAmplitude) * (bDirectionUp ? 1 : -1),
				fRatio * icon->fWidthFactor * icon->fScale / (1 + myIcons.fAmplitude));
		}
	}
	else
	{
		if (bHorizontalDock)
		{
			cairo_translate (pCairoContext,
				/*icon->fDrawXAtRest - icon->fDrawX +*/ (icon->fWidth * icon->fScale - g_fIndicatorWidth * fRatio) / 2,
				/*icon->fDrawYAtRest - icon->fDrawY +*/ (bDirectionUp ? 
					(icon->fHeight * icon->fScale - (g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + myIcons.fAmplitude)) * fRatio) :
					(g_fIndicatorHeight * icon->fScale - myIndicators.iIndicatorDeltaY) * fRatio));
		}
		else
		{
			cairo_translate (pCairoContext,
				/*icon->fDrawYAtRest - icon->fDrawY +*/ (bDirectionUp ? 
					(icon->fHeight - (g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + myIcons.fAmplitude)) * fRatio) * icon->fScale : 
					(g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + myIcons.fAmplitude)) * icon->fScale * fRatio),
				/*icon->fDrawXAtRest - icon->fDrawX +*/ (icon->fWidth * icon->fScale - g_fIndicatorWidth * fRatio) / 2);
		}
	}
	
	cairo_dock_draw_surface (pCairoContext, g_pIndicatorSurface, g_fIndicatorWidth, g_fIndicatorHeight, bDirectionUp, bHorizontalDock, 1.);
	cairo_restore (pCairoContext);
}

static void _cairo_dock_draw_active_window_indicator (cairo_t *pCairoContext, Icon *icon, double fRatio)
{
	cairo_save (pCairoContext);
	if (icon->fOrientation != 0)
		cairo_rotate (pCairoContext, icon->fOrientation);
	cairo_scale (pCairoContext,
		icon->fWidth * icon->fWidthFactor / fRatio / g_fActiveIndicatorWidth * icon->fScale / (1 + myIcons.fAmplitude),
		icon->fHeight * icon->fHeightFactor / fRatio / g_fActiveIndicatorHeight * icon->fScale / (1 + myIcons.fAmplitude));
	cairo_set_source_surface (pCairoContext, g_pActiveIndicatorSurface, 0., 0.);
	cairo_paint (pCairoContext);
	cairo_restore (pCairoContext);
}

static void _cairo_dock_draw_class_indicator (cairo_t *pCairoContext, Icon *icon, gboolean bHorizontalDock, double fRatio, gboolean bDirectionUp)
{
	cairo_save (pCairoContext);
	if (bHorizontalDock)
	{
		if (bDirectionUp)
			cairo_translate (pCairoContext,
				icon->fWidth * icon->fScale - g_fClassIndicatorWidth * fRatio,
				0.);
		else
			cairo_translate (pCairoContext,
				icon->fWidth * icon->fScale - g_fClassIndicatorWidth * fRatio,
				icon->fHeight * icon->fScale - g_fClassIndicatorHeight * fRatio);
	}
	else
	{
		if (bDirectionUp)
			cairo_translate (pCairoContext,
				0.,
				icon->fWidth * icon->fScale - g_fClassIndicatorWidth * fRatio);
		else
			cairo_translate (pCairoContext,
				icon->fHeight * icon->fScale - g_fClassIndicatorHeight * fRatio,
				icon->fWidth * icon->fScale - g_fClassIndicatorWidth * fRatio);
	}
	cairo_scale (pCairoContext, fRatio, fRatio);
	cairo_dock_draw_surface (pCairoContext, g_pClassIndicatorSurface, g_fClassIndicatorWidth, g_fClassIndicatorHeight, bDirectionUp, bHorizontalDock, 1.);
	cairo_restore (pCairoContext);
}

void cairo_dock_set_icon_scale_on_context (cairo_t *pCairoContext, Icon *icon, gboolean bHorizontalDock, double fRatio, gboolean bDirectionUp)
{
	if (bHorizontalDock)
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
	double fRatio = pDock->fRatio;
	//\_____________________ On dessine l'icone.
	if (icon->pIconBuffer != NULL)
	{
		cairo_save (pCairoContext);
		
		cairo_dock_set_icon_scale_on_context (pCairoContext, icon, pDock->bHorizontalDock, fRatio, pDock->bDirectionUp);
		cairo_set_source_surface (pCairoContext, icon->pIconBuffer, 0.0, 0.0);
		if (icon->fAlpha == 1)
			cairo_paint (pCairoContext);
		else
			cairo_paint_with_alpha (pCairoContext, icon->fAlpha);

		cairo_restore (pCairoContext);
	}
	//\_____________________ On dessine son reflet.
	if (pDock->bUseReflect && icon->pReflectionBuffer != NULL)  // on dessine les reflets.
	{
		cairo_save (pCairoContext);
		
		if (pDock->bHorizontalDock)
		{
			if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
				cairo_translate (pCairoContext, 0, (pDock->bDirectionUp ? icon->fDeltaYReflection + icon->fHeight : -icon->fDeltaYReflection - myIcons.fReflectSize * fRatio));
			else
				cairo_translate (pCairoContext, 0, (pDock->bDirectionUp ? icon->fDeltaYReflection + icon->fHeight * icon->fScale : -icon->fDeltaYReflection - myIcons.fReflectSize * icon->fScale * fRatio));
		}
		else
		{
			if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
				cairo_translate (pCairoContext, (pDock->bDirectionUp ? icon->fDeltaYReflection + icon->fHeight : -icon->fDeltaYReflection - myIcons.fReflectSize * fRatio), 0);
			else
				cairo_translate (pCairoContext, (pDock->bDirectionUp ? icon->fDeltaYReflection + icon->fHeight * icon->fScale : -icon->fDeltaYReflection - myIcons.fReflectSize * icon->fScale * fRatio), 0);
		}
		cairo_dock_set_icon_scale_on_context (pCairoContext, icon, pDock->bHorizontalDock, fRatio, pDock->bDirectionUp);
		
		cairo_set_source_surface (pCairoContext, icon->pReflectionBuffer, 0.0, 0.0);
		
		if (mySystem.bDynamicReflection && icon->fScale > 1)  // on applique la surface avec un degrade en transparence, ou avec une transparence simple.
		{
			cairo_pattern_t *pGradationPattern;
			if (pDock->bHorizontalDock)
			{
				pGradationPattern = cairo_pattern_create_linear (0.,
					(pDock->bDirectionUp ? 0. : myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude)),
					0.,
					(pDock->bDirectionUp ? myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude) / icon->fScale : myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude) * (1. - 1./ icon->fScale)));  // de haut en bas.
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
				pGradationPattern = cairo_pattern_create_linear ((pDock->bDirectionUp ? 0. : myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude)),
					0.,
					(pDock->bDirectionUp ? myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude) / icon->fScale : myIcons.fReflectSize / fRatio * (1 + myIcons.fAmplitude) * (1. - 1./ icon->fScale)),
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

gboolean cairo_dock_render_icon_notification_cairo (gpointer pUserData, Icon *icon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext)
{
	if (pCairoContext == NULL)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	if (*bHasBeenRendered)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	if (icon->pIconBuffer == NULL)
	{
		*bHasBeenRendered = TRUE;
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	}
	
	cairo_dock_draw_icon_cairo (icon, pDock, pCairoContext);
	
	*bHasBeenRendered = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

void cairo_dock_render_one_icon (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext, double fDockMagnitude, gboolean bUseText)
{
	int iWidth = pDock->iCurrentWidth;
	double fRatio = pDock->fRatio;
	gboolean bDirectionUp = pDock->bDirectionUp;
	gboolean bHorizontalDock = pDock->bHorizontalDock;
	
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.fVisibleAppliAlpha != 0 && ! CAIRO_DOCK_IS_APPLET (icon))
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
			if (bHorizontalDock)
				cairo_translate (pCairoContext, 0., (1-fGlideScale)*icon->fHeight*icon->fScale);
			else
				cairo_translate (pCairoContext, (1-fGlideScale)*icon->fHeight*icon->fScale, 0.);
	}
	else
		fGlideScale = 1;
	icon->fGlideScale = fGlideScale;
	
	if (bHorizontalDock)
		cairo_translate (pCairoContext, icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1), icon->fDrawY);
	else
		cairo_translate (pCairoContext, icon->fDrawY, icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1));
	
	cairo_save (pCairoContext);
	
	//\_____________________ On dessine l'indicateur derriere.
	if (icon->bHasIndicator && ! myIndicators.bIndicatorAbove && g_pIndicatorSurface != NULL)
	{
		_cairo_dock_draw_appli_indicator (icon, pCairoContext, bHorizontalDock, fRatio, bDirectionUp);
	}
	if (icon->Xid != 0 && icon->Xid == cairo_dock_get_current_active_window () && ! myIndicators.bActiveIndicatorAbove && g_pActiveIndicatorSurface != NULL)
	{
		_cairo_dock_draw_active_window_indicator (pCairoContext, icon, fRatio);
	}
	
	//\_____________________ On positionne l'icone.
	if (icon->fOrientation != 0)
		cairo_rotate (pCairoContext, icon->fOrientation);
	
	//\_____________________ On dessine l'icone.
	gboolean bIconHasBeenDrawn = FALSE;
	cairo_dock_notify (CAIRO_DOCK_RENDER_ICON, icon, pDock, &bIconHasBeenDrawn, pCairoContext);
	
	cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
	
	//\_____________________ On dessine l'indicateur devant.
	if (icon->Xid != 0 && icon->Xid == cairo_dock_get_current_active_window () && myIndicators.bActiveIndicatorAbove && g_pActiveIndicatorSurface != NULL)
	{
		_cairo_dock_draw_active_window_indicator (pCairoContext, icon, fRatio);
	}
	if (icon->bHasIndicator && myIndicators.bIndicatorAbove && g_pIndicatorSurface != NULL)
	{
		_cairo_dock_draw_appli_indicator (icon, pCairoContext, bHorizontalDock, fRatio, bDirectionUp);
	}
	if (icon->pSubDock != NULL && icon->cClass != NULL && g_pClassIndicatorSurface != NULL && icon->Xid == 0)  // le dernier test est de la paranoia.
	{
		_cairo_dock_draw_class_indicator (pCairoContext, icon, bHorizontalDock, fRatio, bDirectionUp);
	}
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	if (bUseText && icon->pTextBuffer != NULL && icon->fScale > 1.01 && (! mySystem.bLabelForPointedIconOnly || icon->bPointed))  // 1.01 car sin(pi) = 1+epsilon :-/  //  && icon->iAnimationState < CAIRO_DOCK_STATE_CLICKED
	{
		cairo_save (pCairoContext);
		double fOffsetX = -icon->fTextXOffset + icon->fWidthFactor * icon->fWidth * icon->fScale / 2;
		if (fOffsetX < - icon->fDrawX)
			fOffsetX = - icon->fDrawX;
		else if (icon->fDrawX + fOffsetX + icon->iTextWidth > iWidth)
			fOffsetX = iWidth - icon->iTextWidth - icon->fDrawX;
		
		if (icon->fOrientation != 0 && ! mySystem.bTextAlwaysHorizontal)
			cairo_rotate (pCairoContext, icon->fOrientation);
		
		if (! bHorizontalDock && mySystem.bTextAlwaysHorizontal)
		{
			cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				(pDock->bDirectionUp ? 0. : - myIcons.fReflectSize),
				0);
		}
		else if (bHorizontalDock)
			cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				fOffsetX,
				bDirectionUp ? -myLabels.iconTextDescription.iSize : icon->fHeight * icon->fScale - icon->fTextYOffset);
		else
			cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				bDirectionUp ? -myLabels.iconTextDescription.iSize : icon->fHeight * icon->fScale - icon->fTextYOffset,
				fOffsetX);
		
		double fMagnitude;
		if (mySystem.bLabelForPointedIconOnly)
		{
			fMagnitude = fDockMagnitude;  // (icon->fScale - 1) / myIcons.fAmplitude / sin (icon->fPhase);  // sin (phi ) != 0 puisque fScale > 1.
		}
		else
		{
			fMagnitude = (icon->fScale - 1) / myIcons.fAmplitude;  /// il faudrait diviser par pDock->fMagnitudeMax ...
			fMagnitude *= (fMagnitude * mySystem.fLabelAlphaThreshold + 1) / (mySystem.fLabelAlphaThreshold + 1);
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
	
	cairo_save (pCairoContext);
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	if (bUseText && icon->pTextBuffer != NULL)
	{
		double fOffsetX = -icon->fTextXOffset + icon->fWidthFactor * icon->fWidth * icon->fScale * 0.5;
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
			-myLabels.iconTextDescription.iSize);
		cairo_paint (pCairoContext);
	}
	
	//\_____________________ On dessine les infos additionnelles.
	cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).
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
	if (pDock->bHorizontalDock)
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

		if (pDock->bHorizontalDock)
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
	GList *pFirstDrawnElement = (pDock->pFirstDrawnElement != NULL ? pDock->pFirstDrawnElement : pDock->icons);
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

void cairo_dock_render_background (cairo_t *pCairoContext, CairoDock *pDock)
{
	if (g_pVisibleZoneSurface != NULL)
	{
		g_print ("%d;%d\n", myHiddenDock.bReverseVisibleImage, pDock->bDirectionUp);
		cairo_dock_draw_surface (pCairoContext, g_pVisibleZoneSurface,
			pDock->iCurrentWidth, pDock->iCurrentHeight,
			(myHiddenDock.bReverseVisibleImage ? pDock->bDirectionUp : TRUE),
			pDock->bHorizontalDock,
			1.);
	}
}

void cairo_dock_render_blank (cairo_t *pCairoContext, CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
}



void cairo_dock_compute_icon_area (Icon *icon, CairoContainer *pContainer, GdkRectangle *pArea)
{
	double fReflectSize = 0;
	if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->bUseReflect)
	{
		fReflectSize = myIcons.fReflectSize * icon->fScale * fabs (icon->fHeightFactor) + icon->fDeltaYReflection;
	}
	
	double fX = icon->fDrawX;
	fX += icon->fWidth * icon->fScale * (1 - fabs (icon->fWidthFactor))/2;
	
	double fY = icon->fDrawY;
	fY += (pContainer->bDirectionUp ? icon->fHeight * icon->fScale * (1 - icon->fHeightFactor)/2 : - fReflectSize);
	
	if (pContainer->bIsHorizontal)
	{
		pArea->x = (int) floor (fX);
		pArea->y = (int) floor (fY);
		pArea->width = (int) ceil (icon->fWidth * icon->fScale * fabs (icon->fWidthFactor)) + 1;
		pArea->height = (int) ceil (icon->fHeight * icon->fScale * fabs (icon->fHeightFactor) + fReflectSize);
	}
	else
	{
		pArea->x = (int) floor (fY);
		pArea->y = (int) floor (fX);
		pArea->width = ((int) ceil (icon->fHeight * icon->fScale * fabs (icon->fHeightFactor) + fReflectSize)) + 1;
		pArea->height = (int) ceil (icon->fWidth * icon->fScale * fabs (icon->fWidthFactor));
	}
}

void cairo_dock_redraw_icon (Icon *icon, CairoContainer *pContainer)
{
	g_return_if_fail (icon != NULL && pContainer != NULL);
	GdkRectangle rect;
	cairo_dock_compute_icon_area (icon, pContainer, &rect);
	cairo_dock_redraw_container_area (pContainer, &rect);
}

void cairo_dock_redraw_container (CairoContainer *pContainer)
{
	g_return_if_fail (pContainer != NULL);
	GdkRectangle rect = {0, 0, pContainer->iWidth, pContainer->iHeight};
	if (! pContainer->bIsHorizontal)
	{
		rect.width = pContainer->iHeight;
		rect.height = pContainer->iWidth;
	}
	cairo_dock_redraw_container_area (pContainer, &rect);
}

void cairo_dock_redraw_container_area (CairoContainer *pContainer, GdkRectangle *pArea)
{
	g_return_if_fail (pContainer != NULL);
	if (! GTK_WIDGET_VISIBLE (pContainer->pWidget))
		return ;
	if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->bAtBottom && (CAIRO_DOCK (pContainer)->iRefCount > 0 || CAIRO_DOCK (pContainer)->bAutoHide))  // inutile de redessiner.
		return ;
	//g_print ("rect (%d;%d) (%dx%d)\n", pArea->x, pArea->y, pArea->width, pArea->height);
	if (pArea->width > 0 && pArea->height > 0)
		gdk_window_invalidate_rect (pContainer->pWidget->window, pArea, FALSE);
}




void cairo_dock_set_window_position_at_balance (CairoDock *pDock, int iNewWidth, int iNewHeight)
{
	if (! g_bEasterEggs && pDock->iRefCount == 0 && pDock->fAlign != .5)
	{
		iNewWidth = pDock->iMaxDockWidth;
	}
	pDock->iWindowPositionX = (g_iScreenWidth[pDock->bHorizontalDock] - iNewWidth) * pDock->fAlign + pDock->iGapX;
	pDock->iWindowPositionY = (pDock->bDirectionUp ? g_iScreenHeight[pDock->bHorizontalDock] - iNewHeight - pDock->iGapY : pDock->iGapY);
	//g_print ("pDock->iGapX : %d => iWindowPositionX <- %d\n", pDock->iGapX, pDock->iWindowPositionX);
	//g_print ("iNewHeight : %d -> pDock->iWindowPositionY <- %d\n", iNewHeight, pDock->iWindowPositionY);

	if (pDock->iWindowPositionX < 0)
		pDock->iWindowPositionX = 0;
	else if (pDock->iWindowPositionX > g_iScreenWidth[pDock->bHorizontalDock] - iNewWidth)
		pDock->iWindowPositionX = g_iScreenWidth[pDock->bHorizontalDock] - iNewWidth;

	if (pDock->iWindowPositionY < 0)
		pDock->iWindowPositionY = 0;
	else if (pDock->iWindowPositionY > g_iScreenHeight[pDock->bHorizontalDock] - iNewHeight)
		pDock->iWindowPositionY = g_iScreenHeight[pDock->bHorizontalDock] - iNewHeight;
	
	pDock->iWindowPositionX += g_iScreenOffsetX;
	pDock->iWindowPositionY += g_iScreenOffsetY;
}

void cairo_dock_get_window_position_and_geometry_at_balance (CairoDock *pDock, CairoDockSizeType iSizeType, int *iNewWidth, int *iNewHeight)
{
	//g_print ("%s (%d)\n", __func__, iSizeType);
	if (iSizeType == CAIRO_DOCK_MAX_SIZE)
	{
		*iNewWidth = pDock->iMaxDockWidth;
		*iNewHeight = pDock->iMaxDockHeight;
		pDock->iLeftMargin = pDock->iMaxLeftMargin;
		pDock->iRightMargin = pDock->iMaxRightMargin;
	}
	else if (iSizeType == CAIRO_DOCK_NORMAL_SIZE)
	{
		*iNewWidth = pDock->iMinDockWidth;
		*iNewHeight = pDock->iMinDockHeight;
		pDock->iLeftMargin = pDock->iMinLeftMargin;
		pDock->iRightMargin = pDock->iMinRightMargin;
	}
	else
	{
		*iNewWidth = myHiddenDock.iVisibleZoneWidth;
		*iNewHeight = myHiddenDock.iVisibleZoneHeight;
		pDock->iLeftMargin = 0;
		pDock->iRightMargin = 0;
	}
	
	if (g_bEasterEggs)
	{
		if (pDock->fAlign < .5)
		{
			*iNewWidth -= pDock->iLeftMargin * (.5 - pDock->fAlign) * 2;
		}
		else if (pDock->fAlign > .5)
		{
			*iNewWidth -= pDock->iRightMargin * (pDock->fAlign - .5) * 2;
		}
	}
	
	cairo_dock_set_window_position_at_balance (pDock, *iNewWidth, *iNewHeight);
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


cairo_t *cairo_dock_create_drawing_context (CairoContainer *pContainer)
{
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (pContainer);
	g_return_val_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS, FALSE);
	
	if (mySystem.bUseFakeTransparency)
		if (g_pDesktopBgSurface != NULL)
			cairo_set_source_surface (pCairoContext, g_pDesktopBgSurface, - pContainer->iWindowPositionX, - pContainer->iWindowPositionY);
		else
			cairo_set_source_rgba (pCairoContext, 0.8, 0.8, 0.8, 0.0);
	else
		cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pCairoContext);
	
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	return pCairoContext;
}

cairo_t *cairo_dock_create_drawing_context_on_area (CairoContainer *pContainer, GdkRectangle *pArea, double *fBgColor)
{
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (pContainer);
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
		if (g_pDesktopBgSurface != NULL)
			cairo_set_source_surface (pCairoContext, g_pDesktopBgSurface, - pContainer->iWindowPositionX, - pContainer->iWindowPositionY);
		else
			cairo_set_source_rgba (pCairoContext, 0.8, 0.8, 0.8, 0.0);
	else if (fBgColor != NULL)
		cairo_set_source_rgba (pCairoContext, fBgColor[0], fBgColor[1], fBgColor[2], fBgColor[3]);
	else
		cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pCairoContext);
	
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	return pCairoContext;
}

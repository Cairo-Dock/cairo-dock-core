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
#include "cairo-dock-draw.h"


extern gint g_iScreenWidth[2];
extern gint g_iScreenHeight[2];

extern gint g_iDockLineWidth;
extern gint g_iDockRadius;
extern double g_fLineColor[4];
extern gint g_iFrameMargin;
extern gint g_iStringLineWidth;
extern double g_fStringColor[4];
extern double g_fReflectSize;

extern double g_fAmplitude;
extern int g_iSinusoidWidth;

extern gboolean g_bRoundedBottomCorner;
extern gboolean bDirectionUp;
extern double g_fBackgroundImageWidth, g_fBackgroundImageHeight;
extern cairo_surface_t *g_pBackgroundSurface[2];
extern cairo_surface_t *g_pBackgroundSurfaceFull[2];

extern cairo_surface_t *g_pVisibleZoneSurface;
extern double g_fAmplitude;

extern CairoDockLabelDescription g_iconTextDescription;
extern CairoDockLabelDescription g_quickInfoTextDescription;

extern double g_fAlphaAtRest;

extern int g_tNbIterInOneRound[CAIRO_DOCK_NB_ANIMATIONS];
extern double g_fAlbedo;
extern gboolean g_bConstantSeparatorSize;
extern cairo_surface_t *g_pIndicatorSurface[2];
extern double g_fIndicatorWidth, g_fIndicatorHeight;

extern cairo_surface_t *g_pDropIndicatorSurface;
extern double g_fDropIndicatorWidth, g_fDropIndicatorHeight;

extern cairo_surface_t *g_pActiveIndicatorSurface;
extern double g_fActiveIndicatorWidth, g_fActiveIndicatorHeight;

extern cairo_surface_t *g_pDesktopBgSurface;
extern gboolean g_bUseGlitz;

void cairo_dock_set_colormap_for_window (GtkWidget *pWidget)
{
	GdkScreen* pScreen = gtk_widget_get_screen (pWidget);
	GdkColormap* pColormap = gdk_screen_get_rgba_colormap (pScreen);
	if (!pColormap)
		pColormap = gdk_screen_get_rgb_colormap (pScreen);

	gtk_widget_set_colormap (pWidget, pColormap);
}

void cairo_dock_set_colormap (CairoContainer *pContainer)
{
	//g_print ("%s f%d)\n", __func__, g_bUseGlitz);
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
			cd_warning ("Attention : no double buffered GLX visual");
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
		//return 2 * g_iDockRadius + g_iDockLineWidth + 2 * g_iFrameMargin;
		return 1 + 2 * g_iFrameMargin;

	Icon *pLastIcon = cairo_dock_get_last_drawn_icon (pDock);
	Icon *pFirstIcon = cairo_dock_get_first_drawn_icon (pDock);
	double fWidth = pLastIcon->fX - pFirstIcon->fX + pLastIcon->fWidth * pLastIcon->fScale + 2 * g_iFrameMargin;  //  + 2 * g_iDockRadius + g_iDockLineWidth + 2 * g_iFrameMargin

	return fWidth;
}
/*void cairo_dock_get_current_dock_width_height (CairoDock *pDock, double *fWidth, double *fHeight)
{
	if (pDock->icons == NULL)
	{
		*fWidth = 2 * g_iDockRadius + g_iDockLineWidth + 2 * g_iFrameMargin;
		*fHeight = 2 * g_iDockRadius + g_iDockLineWidth + 2 * g_iFrameMargin;
	}

	Icon *pLastIcon = cairo_dock_get_last_drawn_icon (pDock);
	Icon *pFirstIcon = cairo_dock_get_first_drawn_icon (pDock);
	*fWidth = pLastIcon->fX - pFirstIcon->fX + 2 * g_iDockRadius + g_iDockLineWidth + 2 * g_iFrameMargin + pLastIcon->fWidth * pLastIcon->fScale;
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
	double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (g_bRoundedBottomCorner ? 2 : 1) * fRadius);
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;

	cairo_move_to (pCairoContext, fDockOffsetX, fDockOffsetY);

	cairo_rel_line_to (pCairoContext, fFrameWidth, 0);
	//\_________________ Coin haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		fRadius * (1. / cosa - fInclination), 0,
		fRadius * cosa, sens * fRadius * (1 - sina));
	cairo_rel_line_to (pCairoContext, fDeltaXForLoop, sens * (fFrameHeight + fLineWidth - fRadius * (g_bRoundedBottomCorner ? 2 : 1 - sina)));
	//\_________________ Coin bas droit.
	if (g_bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			fRadius * (1 + sina) * fInclination, sens * fRadius * (1 + sina),
			-fRadius * cosa, sens * fRadius * (1 + sina));

	cairo_rel_line_to (pCairoContext, - fFrameWidth -  2 * fDeltaXForLoop - (g_bRoundedBottomCorner ? 0 : 2 * fRadius * cosa), 0);
	//\_________________ Coin bas gauche.
	if (g_bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			-fRadius * (fInclination + 1. / cosa), 0,
			-fRadius * cosa, -sens * fRadius * (1 + sina));
	cairo_rel_line_to (pCairoContext, fDeltaXForLoop, sens * (- fFrameHeight - fLineWidth + fRadius * (g_bRoundedBottomCorner ? 2 : 1 - sina)));
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
	double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (g_bRoundedBottomCorner ? 2 : 1) * fRadius);
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;

	cairo_move_to (pCairoContext, fDockOffsetY, fDockOffsetX);

	cairo_rel_line_to (pCairoContext, 0, fFrameWidth);
	//\_________________ Coin haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0, fRadius * (1. / cosa - fInclination),
		sens * fRadius * (1 - sina), fRadius * cosa);
	cairo_rel_line_to (pCairoContext, sens * (fFrameHeight + fLineWidth - fRadius * (g_bRoundedBottomCorner ? 2 : 1 - sina)), fDeltaXForLoop);
	//\_________________ Coin bas droit.
	if (g_bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			sens * fRadius * (1 + sina), fRadius * (1 + sina) * fInclination,
			sens * fRadius * (1 + sina), -fRadius * cosa);

	cairo_rel_line_to (pCairoContext, 0, - fFrameWidth -  2 * fDeltaXForLoop - (g_bRoundedBottomCorner ? 0 : 2 * fRadius * cosa));
	//\_________________ Coin bas gauche.
	if (g_bRoundedBottomCorner)
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			0, -fRadius * (fInclination + 1. / cosa),
			-sens * fRadius * (1 + sina), -fRadius * cosa);
	cairo_rel_line_to (pCairoContext, sens * (- fFrameHeight - fLineWidth + fRadius * (g_bRoundedBottomCorner ? 2 : 1)), fDeltaXForLoop);
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
	if (g_pBackgroundSurfaceFull[pDock->bHorizontalDock] != NULL)
	{
		cairo_save (pCairoContext);

		if (pDock->bHorizontalDock)
			cairo_translate (pCairoContext, pDock->fDecorationsOffsetX * mySystem.fStripesSpeedFactor - pDock->iCurrentWidth * 0.5, fOffsetY);
		else
			cairo_translate (pCairoContext, fOffsetY, pDock->fDecorationsOffsetX * mySystem.fStripesSpeedFactor - pDock->iCurrentWidth * 0.5);
		
		cairo_set_source_surface (pCairoContext, g_pBackgroundSurfaceFull[pDock->bHorizontalDock], 0., 0.);
		cairo_fill_preserve (pCairoContext);
		cairo_restore (pCairoContext);
	}
	else if (g_pBackgroundSurface[pDock->bHorizontalDock] != NULL)
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
		
		//g_print ("(%dx%d) / (%dx%d)\n", pDock->iCurrentWidth, (int) pDock->iMaxIconHeight, (int) g_fBackgroundImageWidth, (int) g_fBackgroundImageHeight);
		cairo_set_source_surface (pCairoContext, g_pBackgroundSurface[pDock->bHorizontalDock], 0., 0.);
		cairo_fill_preserve (pCairoContext);
		cairo_restore (pCairoContext);
	}
}


void cairo_dock_manage_animations (Icon *icon, CairoDock *pDock)
{
	icon->fDrawXAtRest = icon->fDrawX;
	icon->fDrawYAtRest = icon->fDrawY;
	//\_____________________ On gere l'animation de rebond.
	if (icon->iAnimationType == CAIRO_DOCK_BOUNCE && icon->iCount > 0)
	{
		int n = g_tNbIterInOneRound[CAIRO_DOCK_BOUNCE];  // nbre d'iteration pour 1 aplatissement+montree+descente.
		int k = n - (icon->iCount % n) - 3;  // 3 iterations pour s'aplatir.
		n -= 3;   // nbre d'iteration pour 1 montree+descente.

		if (k > 0)
		{
			double fPossibleDeltaY = MIN (100, (pDock->bDirectionUp ? icon->fDrawY : pDock->iCurrentHeight - (icon->fDrawY + icon->fHeight * icon->fScale)));  // on borne a 100 pixels pour les rendus qui ont des fenetres grandes..

			icon->fDeltaYReflection = (pDock->bDirectionUp ? -1. : 1.) * k / (n/2) * fPossibleDeltaY * (2 - 1.*k/(n/2));
			icon->fDrawY += icon->fDeltaYReflection;
			icon->fDeltaYReflection *= 1.25;  // le reflet "rebondira" de 25% de la hauteur au sol.
			//g_print ("%d) + %.2f (%d)\n", icon->iCount, (pDock->bDirectionUp ? -1. : 1.) * k / (n/2) * fPossibleDeltaY * (2 - 1.*k/(n/2)), k);
		}
		else  // on commence par s'aplatir.
		{
			icon->fHeightFactor *= 1.*(2 - 1.5*k) / 10;
			icon->fDeltaYReflection = (pDock->bDirectionUp ? 1 : -1) * (1 - icon->fHeightFactor) / 2 * icon->fHeight * icon->fScale;
			icon->fDrawY += icon->fDeltaYReflection;
			//g_print ("%d) * %.2f (%d)\n", icon->iCount, icon->fHeightFactor, k);
		}
		icon->iCount --;  // c'est une loi de type acceleration dans le champ de pesanteur. 'g' et 'v0' n'interviennent pas directement, car s'expriment en fonction de 'fPossibleDeltaY' et 'n'.
	}

	//\_____________________ On gere l'animation de rotation sur elle-meme.
	if (icon->iAnimationType == CAIRO_DOCK_ROTATE && icon->iCount > 0)
	{
		int c = icon->iCount;
		int n = g_tNbIterInOneRound[CAIRO_DOCK_ROTATE] / 4;  // nbre d'iteration pour 1/2 tour.
		if ((c/n) & 1)
		{
			icon->fWidthFactor *= ((c/(2*n)) & 1 ? 1. : -1.) * (c%n) / n;
		}
		else
		{
			icon->fWidthFactor *= ((c/(2*n)) & 1 ? 1. : -1.) * ((c%n) - n) / n;
		}
		icon->iRotationY = (c/n) * 180;
		icon->iCount --;
	}

	//\_____________________ On gere l'animation de rotation horizontale.
	if (icon->iAnimationType == CAIRO_DOCK_UPSIDE_DOWN && icon->iCount > 0)
	{
		int c = icon->iCount;
		int n = g_tNbIterInOneRound[CAIRO_DOCK_UPSIDE_DOWN] / 4;  // nbre d'iteration pour 1/2 tour.
		if ((c/n) & 1)
		{
			icon->fHeightFactor *= ((c/(2*n)) & 1 ? 1. : -1.) * (c%n) / n;
		}
		else
		{
			icon->fHeightFactor *= ((c/(2*n)) & 1 ? 1. : -1.) * ((c%n) - n) / n;
		}
		icon->iRotationX = (c/n) * 180;
		icon->iCount --;
	}

	//\_____________________ On gere l'animation wobbly.
	if (icon->iAnimationType == CAIRO_DOCK_WOBBLY && icon->iCount > 0)  // merci a Tshirtman cette animation !
	{
		int c = icon->iCount;
		int n = g_tNbIterInOneRound[CAIRO_DOCK_WOBBLY] / 4;  // nbre d'iteration pour 1 etirement/retrecissement.
		int k = c%n;

		double fMinSize = .3, fMaxSize = MIN (1.75, pDock->iCurrentHeight / icon->fWidth);  // au plus 1.75, soit 3/8 de l'icone qui deborde de part et d'autre de son emplacement. c'est suffisamment faible pour ne pas trop empieter sur ses voisines.

		double fSizeFactor = ((c/n) & 1 ? 1. / (n - k) : 1. / (1 + k));
		//double fSizeFactor = ((c/n) & 1 ? 1.*(k+1)/n : 1.*(n-k)/n);
		fSizeFactor = (fMinSize - fMaxSize) * fSizeFactor + fMaxSize;
		if ((c/(2*n)) & 1)
		{
			icon->fWidthFactor *= fSizeFactor;
			icon->fHeightFactor *= fMinSize;
			//g_print ("%d) width <- %.2f ; height <- %.2f (%d)\n", c, icon->fWidthFactor, icon->fHeightFactor, k);
		}
		else
		{
			icon->fHeightFactor *= fSizeFactor;
			icon->fWidthFactor *= fMinSize;
			//g_print ("%d) height <- %.2f ; width <- %.2f (%d)\n", c, icon->fHeightFactor, icon->fWidthFactor, k);
		}
		icon->iCount --;
	}

	//\_____________________ On gere l'animation de l'icone qui suit ou evite le curseur.
	if (icon->iAnimationType == CAIRO_DOCK_FOLLOW_MOUSE)
	{
		icon->fScale = 1 + g_fAmplitude;
		icon->fDrawX = pDock->iMouseX  - icon->fWidth * icon->fScale / 2;
		icon->fDrawY = pDock->iMouseY - icon->fHeight * icon->fScale / 2 ;
		icon->fAlpha = 0.4;
	}
	else if (icon->iAnimationType == CAIRO_DOCK_AVOID_MOUSE)
	{
		icon->fAlpha = 0.4;
		icon->fDrawX += icon->fWidth / 2 * (icon->fScale - 1) / g_fAmplitude * (icon->fPhase < G_PI/2 ? -1 : 1);
	}

	//\_____________________ On gere l'animation d'ondelette.
	if (icon->iCount > 0 && icon->iAnimationType == CAIRO_DOCK_PULSE)
	{
		icon->fAlpha = 1. * (icon->iCount % g_tNbIterInOneRound[CAIRO_DOCK_PULSE]) / g_tNbIterInOneRound[CAIRO_DOCK_PULSE];
		icon->iCount --;
	}

	//\_____________________ On gere l'animation de clignotement.
	if (icon->iCount > 0 && icon->iAnimationType == CAIRO_DOCK_BLINK)
	{
		int c = icon->iCount;
		int n = g_tNbIterInOneRound[CAIRO_DOCK_BLINK] / 2;  // nbre d'iteration pour une inversion d'alpha.
		if ( (c/n) & 1)
			icon->fAlpha *= 1. * (c%n) / n;
		else
			icon->fAlpha *= 1. * (n - 1 - (c%n)) / n;
		icon->fAlpha *= icon->fAlpha;  // pour accentuer.
		icon->iCount --;
	}


	if (icon->fWidthFactor >= 0 && icon->fWidthFactor < 0.05)
		icon->fWidthFactor = 0.05;
	else if (icon->fWidthFactor < 0 && icon->fWidthFactor > -0.05)
		icon->fWidthFactor = -0.05;

	icon->fDrawX += (1 - icon->fWidthFactor) / 2 * icon->fWidth * icon->fScale;

	if (icon->fHeightFactor >= 0 && icon->fHeightFactor < 0.05)
		icon->fHeightFactor = 0.05;
	else if (icon->fHeightFactor < 0 && icon->fHeightFactor > -0.05)
		icon->fHeightFactor = -0.05;

	icon->fDrawY += (1 - icon->fHeightFactor) / 2 * icon->fHeight * icon->fScale;
	
	cairo_dock_update_removing_inserting_icon (icon);
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
					(icon->fHeight - (g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + g_fAmplitude)) * fRatio) * icon->fScale :
					(g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + g_fAmplitude)) * icon->fScale * fRatio));
			cairo_scale (pCairoContext,
				fRatio * icon->fWidthFactor * icon->fScale / (1 + g_fAmplitude),
				fRatio * icon->fHeightFactor * icon->fScale / (1 + g_fAmplitude) * (bDirectionUp ? 1 : -1));
		}
		else
		{
			cairo_translate (pCairoContext,
				(bDirectionUp ? 
					(icon->fHeight - (g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + g_fAmplitude)) * fRatio) * icon->fScale : 
					(g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + g_fAmplitude)) * icon->fScale * fRatio),
					(icon->fWidth - g_fIndicatorWidth * fRatio) * icon->fWidthFactor * icon->fScale / 2);
			cairo_scale (pCairoContext,
				fRatio * icon->fHeightFactor * icon->fScale / (1 + g_fAmplitude) * (bDirectionUp ? 1 : -1),
				fRatio * icon->fWidthFactor * icon->fScale / (1 + g_fAmplitude));
		}
	}
	else
	{
		if (bHorizontalDock)
		{
			cairo_translate (pCairoContext,
				icon->fDrawXAtRest - icon->fDrawX + (icon->fWidth * icon->fScale - g_fIndicatorWidth * fRatio) / 2,
				icon->fDrawYAtRest - icon->fDrawY + (bDirectionUp ? 
					(icon->fHeight * icon->fScale - (g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + g_fAmplitude)) * fRatio) :
					(g_fIndicatorHeight * icon->fScale - myIndicators.iIndicatorDeltaY) * fRatio));
		}
		else
		{
			cairo_translate (pCairoContext,
				icon->fDrawYAtRest - icon->fDrawY + (bDirectionUp ? 
					(icon->fHeight - (g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + g_fAmplitude)) * fRatio) * icon->fScale : 
					(g_fIndicatorHeight - myIndicators.iIndicatorDeltaY / (1 + g_fAmplitude)) * icon->fScale * fRatio),
				icon->fDrawXAtRest - icon->fDrawX + (icon->fWidth * icon->fScale - g_fIndicatorWidth * fRatio) / 2);
		}
	}
	
	cairo_set_source_surface (pCairoContext, g_pIndicatorSurface[bHorizontalDock], 0.0, 0.0);
	cairo_paint (pCairoContext);
	cairo_restore (pCairoContext);
}

void cairo_dock_render_one_icon (Icon *icon, cairo_t *pCairoContext, gboolean bHorizontalDock, double fRatio, double fDockMagnitude, gboolean bUseReflect, gboolean bUseText, int iWidth, gboolean bDirectionUp)
{
	//\_____________________ On separe 2 cas : dessin avec le tampon complet, et dessin avec le ou les petits tampons.
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
	
	double fGlideScale;
	if (icon->fGlideOffset != 0)
	{
		double fPhase =  icon->fPhase + icon->fGlideOffset * icon->fWidth / fRatio / g_iSinusoidWidth * G_PI;
		if (fPhase < 0)
		{
			fPhase = 0;
		}
		else if (fPhase > G_PI)
		{
			fPhase = G_PI;
		}
		fGlideScale = (1 + fDockMagnitude * g_fAmplitude * sin (fPhase)) / icon->fScale;  // c'est un peu hacky ... il faudrait passer l'icone precedente en parametre ...
		if (bDirectionUp)
			if (bHorizontalDock)
				cairo_translate (pCairoContext, 0., (1-fGlideScale)*icon->fHeight*icon->fScale);
			else
				cairo_translate (pCairoContext, (1-fGlideScale)*icon->fHeight*icon->fScale, 0.);
	}
	else
		fGlideScale = 1;
	
	if (bHorizontalDock)
		cairo_translate (pCairoContext, icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1), icon->fDrawY);
	else
		cairo_translate (pCairoContext, icon->fDrawY, icon->fDrawX + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1));
	
	if (icon->bHasIndicator && ! myIndicators.bIndicatorAbove && g_pIndicatorSurface[0] != NULL)
	{
		_cairo_dock_draw_appli_indicator (icon, pCairoContext, bHorizontalDock, fRatio, bDirectionUp);
	}
	
	gboolean bDrawFullBuffer  = (bUseReflect && icon->pFullIconBuffer != NULL && (! mySystem.bDynamicReflection || icon->fScale == 1) && (icon->iCount == 0 || icon->iAnimationType == CAIRO_DOCK_ROTATE || icon->iAnimationType == CAIRO_DOCK_BLINK));
	int iCurrentWidth= 1;
	//\_____________________ On dessine l'icone en fonction de son placement, son angle, et sa transparence.
	cairo_save (pCairoContext);
	if (bHorizontalDock)
	{
		if (bDrawFullBuffer && ! bDirectionUp)
			cairo_translate (pCairoContext, 0, - g_fReflectSize * icon->fScale);
		if (g_bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
		{
			cairo_translate (pCairoContext, 1 * icon->fWidthFactor * icon->fWidth * (icon->fScale - 1) / 2, (bDirectionUp ? 1 * icon->fHeightFactor * icon->fHeight * (icon->fScale - 1) : 0));
			cairo_scale (pCairoContext, fRatio * icon->fWidthFactor / (1 + g_fAmplitude), fRatio * icon->fHeightFactor / (1 + g_fAmplitude));
		}
		else
			cairo_scale (pCairoContext, fRatio * icon->fWidthFactor * icon->fScale / (1 + g_fAmplitude)*fGlideScale, fRatio * icon->fHeightFactor * icon->fScale / (1 + g_fAmplitude)*fGlideScale);
		if (icon->fOrientation != 0)
		{
			cairo_rotate (pCairoContext, icon->fOrientation);
		}
	}
	else
	{
		if (bDrawFullBuffer && ! bDirectionUp)
			cairo_translate (pCairoContext, - g_fReflectSize * icon->fScale, 0);
		if (g_bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
		{
			cairo_translate (pCairoContext, 1 * icon->fHeightFactor * icon->fHeight * (icon->fScale - 1) / 2, (bDirectionUp ? 1 * icon->fWidthFactor * icon->fWidth * (icon->fScale - 1) : 0));
			cairo_scale (pCairoContext, fRatio * icon->fHeightFactor / (1 + g_fAmplitude), fRatio * icon->fWidthFactor / (1 + g_fAmplitude));
		}
		else
			cairo_scale (pCairoContext, fRatio * icon->fHeightFactor * icon->fScale / (1 + g_fAmplitude)*fGlideScale, fRatio * icon->fWidthFactor * icon->fScale / (1 + g_fAmplitude)*fGlideScale);
		if (icon->fOrientation != 0)
			cairo_rotate (pCairoContext, icon->fOrientation);
	}
	
	double fPreviousAlpha = icon->fAlpha;
	if (icon->iCount > 0 && icon->iAnimationType == CAIRO_DOCK_PULSE)
	{
		if (icon->fAlpha > 0)
		{
			cairo_save (pCairoContext);
			double fScaleFactor = 1 + (1 - icon->fAlpha);
			if (bHorizontalDock)
				cairo_translate (pCairoContext, icon->fWidth / fRatio * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2, icon->fHeight / fRatio * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2);
			else
				cairo_translate (pCairoContext, icon->fHeight / fRatio * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2, icon->fWidth / fRatio * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2);
			cairo_scale (pCairoContext, fScaleFactor, fScaleFactor);
			if (icon->pIconBuffer != NULL)
				cairo_set_source_surface (pCairoContext, icon->pIconBuffer, 0.0, 0.0);
			cairo_paint_with_alpha (pCairoContext, icon->fAlpha);
			cairo_restore (pCairoContext);
		}
		icon->fAlpha = .8;
	}
	
	double fAlpha = icon->fAlpha * (fDockMagnitude + g_fAlphaAtRest * (1 - fDockMagnitude));
	
	if (icon->Xid != 0 && icon->Xid == cairo_dock_get_current_active_window () && ! myIndicators.bActiveIndicatorAbove && g_pActiveIndicatorSurface != NULL)
	{
		cairo_save (pCairoContext);
		if (icon->fWidth / fRatio != g_fActiveIndicatorWidth || icon->fHeight / fRatio != g_fActiveIndicatorHeight)
		{
			cairo_scale (pCairoContext, icon->fWidth * icon->fWidthFactor * 1 / fRatio / g_fActiveIndicatorWidth, icon->fHeight * icon->fHeightFactor * 1 / fRatio / g_fActiveIndicatorHeight);
		}
		cairo_set_source_surface (pCairoContext, g_pActiveIndicatorSurface, 0., 0.);
		cairo_paint (pCairoContext);
		cairo_restore (pCairoContext);
	}
	
	if (bUseReflect && icon->pReflectionBuffer != NULL)  // on dessine les reflets.
	{
		if (bDrawFullBuffer)  // on les dessine d'un bloc.
		{
			cairo_set_source_surface (pCairoContext, icon->pFullIconBuffer, 0.0, 0.0);
			if (fAlpha == 1)
				cairo_paint (pCairoContext);
			else
				cairo_paint_with_alpha (pCairoContext, fAlpha);
		}
		else  // on les dessine separement, en gerant eventuellement dynamiquement la transparence du reflet.
		{
			if (icon->pIconBuffer != NULL)
				cairo_set_source_surface (pCairoContext, icon->pIconBuffer, 0.0, 0.0);
			if (fAlpha == 1)
				cairo_paint (pCairoContext);
			else
				cairo_paint_with_alpha (pCairoContext, fAlpha);

			cairo_restore (pCairoContext);  // retour juste apres la translation (fDrawX, fDrawY).

			cairo_save (pCairoContext);
			if (bHorizontalDock)
			{
				cairo_translate (pCairoContext, 0, - icon->fDeltaYReflection + (bDirectionUp ? icon->fHeight * icon->fScale : - g_fReflectSize * icon->fScale));
				cairo_scale (pCairoContext, fRatio * icon->fWidthFactor * icon->fScale / (1 + g_fAmplitude), fRatio * icon->fHeightFactor * icon->fScale / (1 + g_fAmplitude));
			}
			else
			{
				cairo_translate (pCairoContext, - icon->fDeltaYReflection + (bDirectionUp ? icon->fHeight * icon->fScale : - g_fReflectSize * icon->fScale), 0);
				cairo_scale (pCairoContext, fRatio * icon->fHeightFactor * icon->fScale / (1 + g_fAmplitude), fRatio * icon->fWidthFactor * icon->fScale / (1 + g_fAmplitude));
			}
			
			if (icon->iCount > 0 && icon->iAnimationType == CAIRO_DOCK_PULSE)
			{
				if (fPreviousAlpha > 0)
				{
					cairo_save (pCairoContext);
					double fScaleFactor = 1 + (1 - fPreviousAlpha);
					if (bHorizontalDock)
						cairo_translate (pCairoContext, icon->fWidth * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2, icon->fHeight * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2);
					else
						cairo_translate (pCairoContext, icon->fHeight * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2, icon->fWidth * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2);
					cairo_scale (pCairoContext, fScaleFactor, fScaleFactor);
					if (icon->pIconBuffer != NULL)
						cairo_set_source_surface (pCairoContext, icon->pReflectionBuffer, 0.0, 0.0);
					cairo_paint_with_alpha (pCairoContext, fPreviousAlpha);
					cairo_restore (pCairoContext);
				}
			}
			
			cairo_set_source_surface (pCairoContext, icon->pReflectionBuffer, 0.0, 0.0);
			
			if (mySystem.bDynamicReflection && icon->fScale > 1)
			{
				cairo_pattern_t *pGradationPattern;
				if (bHorizontalDock)
				{
					pGradationPattern = cairo_pattern_create_linear (0.,
						(bDirectionUp ? 0. : g_fReflectSize / fRatio * (1 + g_fAmplitude)),
						0.,
						(bDirectionUp ? g_fReflectSize / fRatio * (1 + g_fAmplitude) / icon->fScale : g_fReflectSize / fRatio * (1 + g_fAmplitude) * (1. - 1./ icon->fScale)));  // de haut en bas.
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
						1 - (icon->fScale - 1) / g_fAmplitude);  // astuce pour ne pas avoir a re-creer la surface de la reflection.
				}
				else
				{
					pGradationPattern = cairo_pattern_create_linear ((bDirectionUp ? 0. : g_fReflectSize / fRatio * (1 + g_fAmplitude)),
						0.,
						(bDirectionUp ? g_fReflectSize / fRatio * (1 + g_fAmplitude) / icon->fScale : g_fReflectSize / fRatio * (1 + g_fAmplitude) * (1. - 1./ icon->fScale)),
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
						1. - (icon->fScale - 1) / g_fAmplitude);  // astuce pour ne pas avoir a re-creer la surface de la reflection.
				}
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

	if (icon->Xid != 0 && icon->Xid == cairo_dock_get_current_active_window () && myIndicators.bActiveIndicatorAbove && g_pActiveIndicatorSurface != NULL)
	{
		cairo_save (pCairoContext);
		if (icon->fWidth / 1. / (1 + g_fAmplitude) != g_fActiveIndicatorWidth || icon->fHeight / 1. / (1 + g_fAmplitude) != g_fActiveIndicatorHeight)
		{
			cairo_scale (pCairoContext, icon->fWidth * icon->fWidthFactor / 1. / g_fActiveIndicatorWidth * icon->fScale / (1 + g_fAmplitude), icon->fHeight * icon->fHeightFactor / 1. / g_fActiveIndicatorHeight * icon->fScale / (1 + g_fAmplitude));
		}
		cairo_set_source_surface (pCairoContext, g_pActiveIndicatorSurface, 0., 0.);
		cairo_paint (pCairoContext);
		cairo_restore (pCairoContext);
	}
	
	if (icon->bHasIndicator && myIndicators.bIndicatorAbove && g_pIndicatorSurface[0] != NULL)
	{
		_cairo_dock_draw_appli_indicator (icon, pCairoContext, bHorizontalDock, fRatio, bDirectionUp);
	}
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	if (bUseText && icon->pTextBuffer != NULL && icon->fScale > 1.01 && (! mySystem.bLabelForPointedIconOnly || icon->bPointed) && icon->iCount == 0)  // 1.01 car sin(pi) = 1+epsilon :-/
	{
		cairo_save (pCairoContext);
		double fOffsetX = -icon->fTextXOffset + icon->fWidthFactor * icon->fWidth * icon->fScale / 2;
		if (fOffsetX < - icon->fDrawX)
			fOffsetX = - icon->fDrawX;
		else if (icon->fDrawX + fOffsetX + icon->iTextWidth > iWidth)
			fOffsetX = iWidth - icon->iTextWidth - icon->fDrawX;
		if (icon->fOrientation != 0 && ! mySystem.bTextAlwaysHorizontal)
		{
			cairo_rotate (pCairoContext, icon->fOrientation);
		}
		/*if (fRatio < 1)  // bof, finalement spa top de reduire le texte.
			cairo_scale (pCairoContext,
				fRatio,
				fRatio);*/
		if (! bHorizontalDock && mySystem.bTextAlwaysHorizontal)
		{
			cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				0,
				0);
		}
		else if (bHorizontalDock)
			cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				fOffsetX,
				bDirectionUp ? -g_iconTextDescription.iSize : icon->fHeight * icon->fScale - icon->fTextYOffset);
		else
			cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				bDirectionUp ? -g_iconTextDescription.iSize : icon->fHeight * icon->fScale - icon->fTextYOffset,
				fOffsetX);
		
		double fMagnitude;
		if (mySystem.bLabelForPointedIconOnly)
		{
			fMagnitude = fDockMagnitude;  // (icon->fScale - 1) / g_fAmplitude / sin (icon->fPhase);  // sin (phi ) != 0 puisque fScale > 1.
		}
		else
		{
			fMagnitude = (icon->fScale - 1) / g_fAmplitude;  /// il faudrait diviser par pDock->fMagnitudeMax ...
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
			//-icon->fQuickInfoXOffset + icon->fWidth / 2,
			//icon->fHeight - icon->fQuickInfoYOffset);
			(- icon->iQuickInfoWidth * fRatio + icon->fWidthFactor * icon->fWidth) / 2 * icon->fScale,
			(icon->fHeight - icon->iQuickInfoHeight * fRatio) * icon->fScale);
		
		cairo_scale (pCairoContext,
			fRatio * icon->fScale / (1 + g_fAmplitude) * 1,
			fRatio * icon->fScale / (1 + g_fAmplitude) * 1);
		
		cairo_set_source_surface (pCairoContext,
			icon->pQuickInfoBuffer,
			0,
			0);
		if (fAlpha == 1)
			cairo_paint (pCairoContext);
		else
			cairo_paint_with_alpha (pCairoContext, fAlpha);
	}
}


void cairo_dock_render_one_icon_in_desklet (Icon *icon, cairo_t *pCairoContext, gboolean bUseReflect, gboolean bUseText, int iWidth)
{
	//\_____________________ On separe 2 cas : dessin avec le tampon complet, et dessin avec le ou les petits tampons.
	gboolean bDrawFullBuffer  = (bUseReflect && icon->pFullIconBuffer != NULL && (! mySystem.bDynamicReflection || icon->fScale == 1) && (icon->iCount == 0 || icon->iAnimationType == CAIRO_DOCK_ROTATE || icon->iAnimationType == CAIRO_DOCK_BLINK));
	int iCurrentWidth= 1;
	//\_____________________ On dessine l'icone en fonction de son placement, son angle, et sa transparence.
	//cairo_push_group (pCairoContext);
	//g_print ("%s (%.2f;%.2f x %.2f)\n", __func__, icon->fDrawX, icon->fDrawY, icon->fScale);
	cairo_translate (pCairoContext, icon->fDrawX, icon->fDrawY);
	cairo_save (pCairoContext);
	/**if (bDrawFullBuffer&& ! pDock->bDirectionUp)
		cairo_translate (pCairoContext, 0, - g_fReflectSize * icon->fScale);*/
	if (g_bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
	{
		cairo_translate (pCairoContext, icon->fWidthFactor * icon->fWidth * (icon->fScale - 1) / 2, icon->fHeightFactor * icon->fHeight * (icon->fScale - 1));
		cairo_scale (pCairoContext, icon->fWidthFactor, icon->fHeightFactor);
	}
	else
		cairo_scale (pCairoContext, icon->fWidthFactor * icon->fScale, icon->fHeightFactor * icon->fScale);
	if (icon->fOrientation != 0)
	{
		cairo_rotate (pCairoContext, icon->fOrientation);
	}
	
	double fPreviousAlpha = icon->fAlpha;
	if (icon->iCount > 0 && icon->iAnimationType == CAIRO_DOCK_PULSE)
	{
		if (icon->fAlpha > 0)
		{
			cairo_save (pCairoContext);
			double fScaleFactor = 1 + (1 - icon->fAlpha);
			cairo_translate (pCairoContext, icon->fWidth * (1 - fScaleFactor) / 2, icon->fHeight * (1 - fScaleFactor) / 2);
			cairo_scale (pCairoContext, fScaleFactor, fScaleFactor);
			if (icon->pIconBuffer != NULL)
				cairo_set_source_surface (pCairoContext, icon->pIconBuffer, 0.0, 0.0);
			cairo_paint_with_alpha (pCairoContext, icon->fAlpha);
			cairo_restore (pCairoContext);
		}
		icon->fAlpha = .8;
	}
	
	double fAlpha = icon->fAlpha;
	
	if (bUseReflect && icon->pReflectionBuffer != NULL)  // on dessine les reflets.
	{
		if (bDrawFullBuffer)  // on les dessine d'un bloc.
		{
			cairo_set_source_surface (pCairoContext, icon->pFullIconBuffer, 0.0, 0.0);
			if (fAlpha == 1)
				cairo_paint (pCairoContext);
			else
				cairo_paint_with_alpha (pCairoContext, fAlpha);
		}
		else  // on les dessine separement, en gerant eventuellement dynamiquement la transparence du reflet.
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
			
			if (icon->iCount > 0 && icon->iAnimationType == CAIRO_DOCK_PULSE)
			{
				if (fPreviousAlpha > 0)
				{
					cairo_save (pCairoContext);
					double fScaleFactor = 1 + (1 - fPreviousAlpha);
					cairo_translate (pCairoContext, icon->fWidth * (1 - fScaleFactor) / 2, icon->fHeight * (1 - fScaleFactor) / 2);
					cairo_scale (pCairoContext, fScaleFactor, fScaleFactor);
					if (icon->pIconBuffer != NULL)
						cairo_set_source_surface (pCairoContext, icon->pReflectionBuffer, 0.0, 0.0);
					cairo_paint_with_alpha (pCairoContext, fPreviousAlpha);
					cairo_restore (pCairoContext);
				}
			}
			
			cairo_set_source_surface (pCairoContext, icon->pReflectionBuffer, 0.0, 0.0);
			
			if (mySystem.bDynamicReflection && icon->fScale != 1)
			{
				cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0.,
					0.,
					0.,
					g_fReflectSize / icon->fScale);  // de haut en bas.
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
	//cairo_pop_group (pCairoContext);
	
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
			-g_iconTextDescription.iSize);
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
	bForceConstantSeparator = bForceConstantSeparator || g_bConstantSeparatorSize;
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

	cairo_set_line_width (pCairoContext, g_iStringLineWidth);
	cairo_set_source_rgba (pCairoContext, g_fStringColor[0], g_fStringColor[1], g_fStringColor[2], g_fStringColor[3]);
	cairo_stroke (pCairoContext);
	cairo_restore (pCairoContext);
}

void cairo_dock_render_icons_linear (cairo_t *pCairoContext, CairoDock *pDock, double fRatio)
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
		cairo_dock_render_one_icon (icon, pCairoContext, pDock->bHorizontalDock, fRatio, fDockMagnitude, pDock->bUseReflect, TRUE, pDock->iCurrentWidth, pDock->bDirectionUp);
		cairo_restore (pCairoContext);

		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);
}



void cairo_dock_render_background (cairo_t *pCairoContext, CairoDock *pDock)
{
	//g_print ("%s (%d, %x)\n", __func__, pDock->bIsMainDock, g_pVisibleZoneSurface);
	if (g_pVisibleZoneSurface != NULL)
	{
		cairo_set_source_surface (pCairoContext, g_pVisibleZoneSurface, 0.0, 0.0);
		//cairo_paint_with_alpha (pCairoContext, myHiddenDock.fVisibleZoneAlpha);
		cairo_paint (pCairoContext);
	}
}

void cairo_dock_render_blank (cairo_t *pCairoContext, CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
}


void cairo_dock_redraw_my_icon (Icon *icon, CairoContainer *pContainer)
{
	g_return_if_fail (icon != NULL && pContainer != NULL);
	if (! GTK_WIDGET_VISIBLE (pContainer->pWidget))
		return ;
	
	double fReflectSize = 0;
	gboolean bHorizontal = pContainer->bIsHorizontal, bDirectionUp = pContainer->bDirectionUp;
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		if (pDock->bUseReflect)
			fReflectSize = g_fReflectSize * icon->fScale * fabs (icon->fHeightFactor);
		if (pDock->bAtBottom && (pDock->iRefCount > 0 || pDock->bAutoHide))  // inutile de redessiner.
			return ;
	}
	
	GdkRectangle rect = {(int) floor (icon->fDrawX + MIN (0, icon->fWidth * icon->fScale * icon->fWidthFactor)),
		(int) floor (icon->fDrawY - (! bDirectionUp ? fReflectSize : 0)),
		(int) ceil (icon->fWidth * icon->fScale * fabs (icon->fWidthFactor)) + 1,
		(int) ceil (icon->fHeight * icon->fScale + fReflectSize)};
	if (! bHorizontal)
	{
		rect.x = (int) floor (icon->fDrawY - (! bDirectionUp ? fReflectSize : 0));
		rect.y = (int) floor (icon->fDrawX + MIN (0, icon->fWidth * icon->fScale * icon->fWidthFactor));
		rect.width = (int) ceil (icon->fHeight * icon->fScale + fReflectSize) + 1;
		rect.height = (int) ceil (icon->fWidth * icon->fScale * fabs (icon->fWidthFactor));
	}
	//g_print ("rect (%d;%d) (%dx%d)\n", rect.x, rect.y, rect.width, rect.height);
	if (rect.width > 0 && rect.height > 0)
	{
#ifdef HAVE_GLITZ
		if (pContainer->pDrawFormat && pContainer->pDrawFormat->doublebuffer)
			gtk_widget_queue_draw (pContainer->pWidget);
		else
#endif
		gdk_window_invalidate_rect (pContainer->pWidget->window, &rect, FALSE);
	}
}



void cairo_dock_set_window_position_at_balance (CairoDock *pDock, int iNewWidth, int iNewHeight)
{
	pDock->iWindowPositionX = (g_iScreenWidth[pDock->bHorizontalDock] - iNewWidth) * pDock->fAlign + pDock->iGapX;
	/*if (pDock->fAlign < 0.5)
	{
		pDock->iWindowPositionX += pDock->iLeftMargin * (0.5 - pDock->fAlign)*2;
	}
	else
	{
		pDock->iWindowPositionX += pDock->iRightMargin * (pDock->fAlign - 0.5)*2;
	}*/
	pDock->iWindowPositionY = (pDock->bDirectionUp ? g_iScreenHeight[pDock->bHorizontalDock] - iNewHeight - pDock->iGapY : pDock->iGapY);
	//g_print ("pDock->iGapX : %d => iWindowPositionX <- %d\n", pDock->iGapX, pDock->iWindowPositionX);
	//g_print ("iNewHeight : %d -> pDock->iWindowPositionY <- %d\n", iNewHeight, pDock->iWindowPositionY);

	if (pDock->iWindowPositionX < 0)
		pDock->iWindowPositionX = 0;
	else if (pDock->iWindowPositionX > g_iScreenWidth[pDock->bHorizontalDock])
		pDock->iWindowPositionX = g_iScreenWidth[pDock->bHorizontalDock];

	if (pDock->iWindowPositionY < 0)
		pDock->iWindowPositionY = 0;
	else if (pDock->iWindowPositionY > g_iScreenHeight[pDock->bHorizontalDock])
		pDock->iWindowPositionY = g_iScreenHeight[pDock->bHorizontalDock];
}

void cairo_dock_get_window_position_and_geometry_at_balance (CairoDock *pDock, CairoDockSizeType iSizeType, int *iNewWidth, int *iNewHeight)
{
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

	cairo_dock_set_window_position_at_balance (pDock, *iNewWidth, *iNewHeight);
}

double cairo_dock_calculate_extra_width_for_trapeze (double fFrameHeight, double fInclination, double fRadius, double fLineWidth)
{
	if (2 * fRadius > fFrameHeight + fLineWidth)
		fRadius = (fFrameHeight + fLineWidth) / 2 - 1;
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = fInclination * cosa;
	
	double fDeltaXForLoop = fInclination * (fFrameHeight + fLineWidth - (g_bRoundedBottomCorner ? 2 : 1) * fRadius);
	double fDeltaCornerForLoop = fRadius * cosa + (g_bRoundedBottomCorner ? fRadius * (1 + sina) * fInclination : 0);
	
	return (2 * (fLineWidth/2 + fDeltaXForLoop + fDeltaCornerForLoop + g_iFrameMargin));
}



void cairo_dock_draw_drop_indicator (CairoDock *pDock, cairo_t *pCairoContext)
{
	double fX = pDock->iMouseX - g_fDropIndicatorWidth / 2;
	
	if (pDock->bHorizontalDock)
		cairo_rectangle (pCairoContext,
			(int) pDock->iMouseX - g_fDropIndicatorWidth/2,
			(int) (pDock->bDirectionUp ? 0 : pDock->iCurrentHeight - 2*g_fDropIndicatorHeight),
			(int) g_fDropIndicatorWidth,
			(int) (pDock->bDirectionUp ? 2*g_fDropIndicatorHeight : pDock->iCurrentHeight));
	else
		cairo_rectangle (pCairoContext,
			(int) (pDock->bDirectionUp ? 0 : pDock->iCurrentHeight - 2*g_fDropIndicatorHeight),
			(int) pDock->iMouseX - g_fDropIndicatorWidth/2,
			(int) (pDock->bDirectionUp ? 2*g_fDropIndicatorHeight : pDock->iCurrentHeight),
			(int) g_fDropIndicatorWidth);
	cairo_clip (pCairoContext);
	
	//cairo_move_to (pCairoContext, fX, 0);
	if (pDock->bHorizontalDock)
		cairo_translate (pCairoContext, fX, (pDock->bDirectionUp ? 0 : pDock->iCurrentHeight));
	else
		cairo_translate (pCairoContext, (pDock->bDirectionUp ? 0 : pDock->iCurrentHeight), fX);
	double fRotationAngle = (pDock->bHorizontalDock ? (pDock->bDirectionUp ? 0 : G_PI) : (pDock->bDirectionUp ? -G_PI/2 : G_PI/2));
	cairo_rotate (pCairoContext, fRotationAngle);
	
	//cairo_move_to (pCairoContext, fX, pDock->iDropIndicatorOffset);
	cairo_translate (pCairoContext, 0, pDock->iDropIndicatorOffset);
	cairo_pattern_t* pPattern = cairo_pattern_create_for_surface (g_pDropIndicatorSurface);
	g_return_if_fail (cairo_pattern_status (pPattern) == CAIRO_STATUS_SUCCESS);
	cairo_pattern_set_extend (pPattern, CAIRO_EXTEND_REPEAT);
	cairo_set_source (pCairoContext, pPattern);
	
	cairo_translate (pCairoContext, 0, -pDock->iDropIndicatorOffset);
	cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0.,
		0.,
		0.,
		2*g_fDropIndicatorHeight);  // de haut en bas.
	g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);

	cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		0.,
		0.,
		0.,
		0.,
		0.);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		0.4,
		0.,
		0.,
		0.,
		1.);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		0.5,
		0.,
		0.,
		0.,
		1.);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		1.,
		0.,
		0.,
		0.,
		0.);

	//cairo_translate (pCairoContext, 0, 0);
	cairo_mask (pCairoContext, pGradationPattern);
	//cairo_paint (pCairoContext);
	
	cairo_pattern_destroy (pPattern);
	cairo_pattern_destroy (pGradationPattern);
}

gboolean cairo_dock_display_drop_indicator (CairoDock *pDock)
{
	GdkRectangle rect = {(int) pDock->iMouseX - g_fDropIndicatorWidth/2,
		(int) (pDock->bDirectionUp ? 0 : pDock->iCurrentHeight - 2*g_fDropIndicatorHeight),
		(int) g_fDropIndicatorWidth,
		(int) 2*g_fDropIndicatorHeight};  /// A peaufiner...
	if (! pDock->bHorizontalDock)
	{
		rect.x = (int) (pDock->bDirectionUp ? 0 : pDock->iCurrentHeight - 2*g_fDropIndicatorHeight);
		rect.y = (int) pDock->iMouseX - g_fDropIndicatorWidth/2;
		rect.width = (int) 2*g_fDropIndicatorHeight;
		rect.height =(int) g_fDropIndicatorWidth;
	}
	//g_print ("rect (%d;%d) (%dx%d)\n", rect.x, rect.y, rect.width, rect.height);
	if (rect.width > 0 && rect.height > 0)
	{
		pDock->iDropIndicatorOffset += 3;
		if (pDock->iDropIndicatorOffset > 2*g_fDropIndicatorHeight)
			pDock->iDropIndicatorOffset = 0;
#ifdef HAVE_GLITZ
		if (pDock->pDrawFormat && pDock->pDrawFormat->doublebuffer)
			gtk_widget_queue_draw (pDock->pWidget);
		else
#endif
		gdk_window_invalidate_rect (pDock->pWidget->window, &rect, FALSE);
	}
	return TRUE;
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

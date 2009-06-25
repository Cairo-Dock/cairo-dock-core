 /*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cairo.h>
#include <pango/pango.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#ifdef HAVE_GLITZ
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include <gtk/gtkgl.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <GL/gl.h> 
#include <GL/glu.h> 
#include <GL/glx.h> 
#include <gdk/x11/gdkglx.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-load.h"
#include "cairo-dock-config.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-hidden-dock.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dock-facility.h"

extern int g_iScreenWidth[2], g_iScreenHeight[2];
extern int g_iScreenOffsetX, g_iScreenOffsetY;
extern int g_iWmHint;
extern gboolean g_bEasterEggs;


void cairo_dock_reload_reflects_in_dock (CairoDock *pDock)
{
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pReflectionBuffer != NULL)
		{
			cairo_dock_add_reflection_to_icon (pCairoContext, icon, CAIRO_CONTAINER (pDock));
		}
	}
	cairo_destroy (pCairoContext);
}


void cairo_dock_update_dock_size (CairoDock *pDock)  // iMaxIconHeight et fFlatDockWidth doivent avoir ete mis a jour au prealable.
{
	g_return_if_fail (pDock != NULL);
	if (pDock->fRatio != 0 && pDock->fRatio != 1)  // on remet leur taille reelle aux icones, sinon le calcul de max_dock_size sera biaise.
	{
		GList *ic;
		Icon *icon;
		pDock->fFlatDockWidth = -myIcons.iIconGap;
		pDock->iMaxIconHeight = 0;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			icon->fWidth /= pDock->fRatio;
			icon->fHeight /= pDock->fRatio;
			pDock->fFlatDockWidth += icon->fWidth + myIcons.iIconGap;
			pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);
		}
		pDock->fRatio = 1.;
	}
	pDock->calculate_max_dock_size (pDock);
	
	int iMaxAuthorizedWidth = cairo_dock_get_max_authorized_dock_width (pDock);
	int n = 0;
	do
	{
		double fPrevRatio = pDock->fRatio;
		//g_print ("  %s (%d / %d)\n", __func__, (int)pDock->iMaxDockWidth, iMaxAuthorizedWidth);
		if (pDock->iMaxDockWidth > iMaxAuthorizedWidth)
		{
			pDock->fRatio *= 1. * iMaxAuthorizedWidth / pDock->iMaxDockWidth;
		}
		else
		{
			double fMaxRatio = (pDock->iRefCount == 0 ? 1 : myViews.fSubDockSizeRatio);
			if (pDock->fRatio < fMaxRatio)
			{
				pDock->fRatio *= 1. * iMaxAuthorizedWidth / pDock->iMaxDockWidth;
				pDock->fRatio = MIN (pDock->fRatio, fMaxRatio);
			}
			else
				pDock->fRatio = fMaxRatio;
		}
		
		if (pDock->iMaxDockHeight > g_iScreenHeight[pDock->bHorizontalDock])
		{
			pDock->fRatio = MIN (pDock->fRatio, fPrevRatio * g_iScreenHeight[pDock->bHorizontalDock] / pDock->iMaxDockHeight);
		}
		
		if (fPrevRatio != pDock->fRatio)
		{
			//g_print ("  -> changement du ratio : %.3f -> %.3f (%d, %d try)\n", fPrevRatio, pDock->fRatio, pDock->iRefCount, n);
			Icon *icon;
			GList *ic;
			pDock->fFlatDockWidth = -myIcons.iIconGap;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				icon->fWidth *= pDock->fRatio / fPrevRatio;
				icon->fHeight *= pDock->fRatio / fPrevRatio;
				pDock->fFlatDockWidth += icon->fWidth + myIcons.iIconGap;
			}
			pDock->iMaxIconHeight *= pDock->fRatio / fPrevRatio;
			
			pDock->calculate_max_dock_size (pDock);
		}
		
		n ++;
	} while ((pDock->iMaxDockWidth > iMaxAuthorizedWidth || pDock->iMaxDockHeight > g_iScreenHeight[pDock->bHorizontalDock]) && n < 4);
	
	if (! pDock->bInside && (pDock->bAutoHide && pDock->iRefCount == 0))
		return;
	else if (GTK_WIDGET_VISIBLE (pDock->pWidget))
	{
		//g_print ("%s (%d;%d => %s size)\n", __func__, pDock->bInside, pDock->bIsShrinkingDown, pDock->bInside || pDock->bIsShrinkingDown ? "max" : "normal");
		int iNewWidth, iNewHeight;
		cairo_dock_get_window_position_and_geometry_at_balance (pDock, (pDock->bInside || pDock->bIsShrinkingDown ? CAIRO_DOCK_MAX_SIZE : CAIRO_DOCK_NORMAL_SIZE), &iNewWidth, &iNewHeight);  // inutile de recalculer Y mais bon...

		if (pDock->bHorizontalDock)
		{
			if (pDock->iCurrentWidth != iNewWidth || pDock->iCurrentHeight != iNewHeight)
				gdk_window_move_resize (pDock->pWidget->window,
					pDock->iWindowPositionX,
					pDock->iWindowPositionY,
					iNewWidth,
					iNewHeight);
		}
		else
		{
			if (pDock->iCurrentWidth != iNewHeight || pDock->iCurrentHeight != iNewWidth)
				gdk_window_move_resize (pDock->pWidget->window,
					pDock->iWindowPositionY,
					pDock->iWindowPositionX,
					iNewHeight,
					iNewWidth);
		}
	}
	
	cairo_dock_set_icons_geometry_for_window_manager (pDock);

	cairo_dock_update_background_decorations_if_necessary (pDock, pDock->iDecorationsWidth, pDock->iDecorationsHeight);
	
	if (g_bEasterEggs)
	{
		cairo_dock_set_input_shape (pDock);
	}
}

Icon *cairo_dock_calculate_dock_icons (CairoDock *pDock)
{
	Icon *pPointedIcon = pDock->calculate_icons (pDock);
	cairo_dock_manage_mouse_position (pDock);
	return (pDock->iMousePositionType == CAIRO_DOCK_MOUSE_INSIDE ? pPointedIcon : NULL);
}


void cairo_dock_reserve_space_for_dock (CairoDock *pDock, gboolean bReserve)
{
	Window Xid = GDK_WINDOW_XID (pDock->pWidget->window);
	int left=0, right=0, top=0, bottom=0;
	int left_start_y=0, left_end_y=0, right_start_y=0, right_end_y=0, top_start_x=0, top_end_x=0, bottom_start_x=0, bottom_end_x=0;
	int iHeight, iWidth;

	if (bReserve)
	{
		int iWindowPositionX = pDock->iWindowPositionX, iWindowPositionY = pDock->iWindowPositionY;
		cairo_dock_get_window_position_and_geometry_at_balance (pDock, (pDock->bAutoHide ? CAIRO_DOCK_MIN_SIZE : CAIRO_DOCK_NORMAL_SIZE), &iWidth, &iHeight);
		if (pDock->bDirectionUp)
		{
			if (pDock->bHorizontalDock)
			{
				bottom = iHeight + pDock->iGapY;
				bottom_start_x = pDock->iWindowPositionX;
				bottom_end_x = pDock->iWindowPositionX + iWidth;

			}
			else
			{
				right = iHeight + pDock->iGapY;
				right_start_y = pDock->iWindowPositionX;
				right_end_y = pDock->iWindowPositionX + iWidth;
			}
		}
		else
		{

			if (pDock->bHorizontalDock)
			{
				top = iHeight + pDock->iGapY;
				top_start_x = pDock->iWindowPositionX;
				top_end_x = pDock->iWindowPositionX + iWidth;
			}
			else
			{
				left = iHeight + pDock->iGapY;
				left_start_y = pDock->iWindowPositionX;
				left_end_y = pDock->iWindowPositionX + iWidth;
			}
		}
		pDock->iWindowPositionX = iWindowPositionX;
		pDock->iWindowPositionY = iWindowPositionY;
	}

	cairo_dock_set_strut_partial (Xid, left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);

	/**if ((bReserve && ! pDock->bDirectionUp) || (g_iWmHint == GDK_WINDOW_TYPE_HINT_DOCK))  // merci a Robrob pour le patch !
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_DOCK");  // gtk_window_set_type_hint ne marche que sur une fenetre avant de la rendre visible !
	else if (g_iWmHint == GDK_WINDOW_TYPE_HINT_NORMAL)
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_NORMAL");  // idem.
	else if (g_iWmHint == GDK_WINDOW_TYPE_HINT_TOOLBAR)
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_TOOLBAR");  // idem.*/
}


void cairo_dock_place_root_dock (CairoDock *pDock)
{
	cd_debug ("%s (%d, %d, %d)", __func__, pDock->bAutoHide, pDock->iRefCount, pDock->bIsMainDock);
	int iNewWidth, iNewHeight;
	if (pDock->bAutoHide && pDock->iRefCount == 0)
	{
		cairo_dock_get_window_position_and_geometry_at_balance (pDock, CAIRO_DOCK_MIN_SIZE, &iNewWidth, &iNewHeight);
		pDock->fFoldingFactor = (mySystem.bAnimateOnAutoHide ? mySystem.fUnfoldAcceleration : 0);
	}
	else
	{
		pDock->fFoldingFactor = 0;
		cairo_dock_get_window_position_and_geometry_at_balance (pDock, CAIRO_DOCK_NORMAL_SIZE, &iNewWidth, &iNewHeight);
	}
	
	cd_debug (" move to (%d;%d)", pDock->iWindowPositionX, pDock->iWindowPositionY);
	if (pDock->bHorizontalDock)
		gdk_window_move_resize (pDock->pWidget->window,
			pDock->iWindowPositionX,
			pDock->iWindowPositionY,
			iNewWidth,
			iNewHeight);
	else
		gdk_window_move_resize (pDock->pWidget->window,
			pDock->iWindowPositionY,
			pDock->iWindowPositionX,
			iNewHeight,
			iNewWidth);
}

void cairo_dock_prevent_dock_from_out_of_screen (CairoDock *pDock)
{
	int x, y;  // position du point invariant du dock.
	x = pDock->iWindowPositionX +  pDock->iCurrentWidth * pDock->fAlign;
	y = (pDock->bDirectionUp ? pDock->iWindowPositionY + pDock->iCurrentHeight : pDock->iWindowPositionY);
	cd_debug ("%s (%d;%d)", __func__, x, y);
	
	pDock->iGapX = x - g_iScreenWidth[pDock->bHorizontalDock] * pDock->fAlign;
	pDock->iGapY = (pDock->bDirectionUp ? g_iScreenHeight[pDock->bHorizontalDock] - y : y);
	cd_debug (" -> (%d;%d)", pDock->iGapX, pDock->iGapY);
	
	if (pDock->iGapX < - g_iScreenWidth[pDock->bHorizontalDock]/2)
		pDock->iGapX = - g_iScreenWidth[pDock->bHorizontalDock]/2;
	if (pDock->iGapX > g_iScreenWidth[pDock->bHorizontalDock]/2)
		pDock->iGapX = g_iScreenWidth[pDock->bHorizontalDock]/2;
	if (pDock->iGapY < 0)
		pDock->iGapY = 0;
	if (pDock->iGapY > g_iScreenHeight[pDock->bHorizontalDock])
		pDock->iGapY = g_iScreenHeight[pDock->bHorizontalDock];
}

#define CD_VISIBILITY_MARGIN 20
void cairo_dock_set_window_position_at_balance (CairoDock *pDock, int iNewWidth, int iNewHeight)
{
	pDock->iWindowPositionX = (g_iScreenWidth[pDock->bHorizontalDock] - iNewWidth) * pDock->fAlign + pDock->iGapX;
	if (pDock->iRefCount == 0 && pDock->fAlign != .5)
		pDock->iWindowPositionX += (.5 - pDock->fAlign) * (pDock->iMaxDockWidth - iNewWidth);
	pDock->iWindowPositionY = (pDock->bDirectionUp ? g_iScreenHeight[pDock->bHorizontalDock] - iNewHeight - pDock->iGapY : pDock->iGapY);
	//g_print ("pDock->iGapX : %d => iWindowPositionX <- %d\n", pDock->iGapX, pDock->iWindowPositionX);
	//g_print ("iNewHeight : %d -> pDock->iWindowPositionY <- %d\n", iNewHeight, pDock->iWindowPositionY);
	
	if (pDock->iRefCount == 0)
	{
		if (pDock->iWindowPositionX + iNewWidth < CD_VISIBILITY_MARGIN)
			pDock->iWindowPositionX = CD_VISIBILITY_MARGIN - iNewWidth;
		else if (pDock->iWindowPositionX > g_iScreenWidth[pDock->bHorizontalDock] - CD_VISIBILITY_MARGIN)
			pDock->iWindowPositionX = g_iScreenWidth[pDock->bHorizontalDock] - CD_VISIBILITY_MARGIN;
	}
	else
	{
		if (pDock->iWindowPositionX < - pDock->iLeftMargin)
			pDock->iWindowPositionX = - pDock->iLeftMargin;
		else if (pDock->iWindowPositionX > g_iScreenWidth[pDock->bHorizontalDock] - iNewWidth + pDock->iMinRightMargin)
			pDock->iWindowPositionX = g_iScreenWidth[pDock->bHorizontalDock] - iNewWidth + pDock->iMinRightMargin;
	}
	if (pDock->iWindowPositionY < - pDock->iMaxIconHeight)
		pDock->iWindowPositionY = - pDock->iMaxIconHeight;
	else if (pDock->iWindowPositionY > g_iScreenHeight[pDock->bHorizontalDock] - iNewHeight + pDock->iMaxIconHeight)
		pDock->iWindowPositionY = g_iScreenHeight[pDock->bHorizontalDock] - iNewHeight + pDock->iMaxIconHeight;
	
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
	
	cairo_dock_set_window_position_at_balance (pDock, *iNewWidth, *iNewHeight);
}


void cairo_dock_set_subdock_position_linear (Icon *pPointedIcon, CairoDock *pDock)
{
	CairoDock *pSubDock = pPointedIcon->pSubDock;
	int iX = pPointedIcon->fXAtRest - (pDock->fFlatDockWidth - pDock->iMaxDockWidth) / 2 + pPointedIcon->fWidth / 2;
	if (pSubDock->bHorizontalDock == pDock->bHorizontalDock)
	{
		pSubDock->fAlign = 0.5;
		pSubDock->iGapX = iX + pDock->iWindowPositionX - (pDock->bHorizontalDock ? g_iScreenOffsetX : g_iScreenOffsetY) - g_iScreenWidth[pDock->bHorizontalDock] / 2;  // les sous-dock ont un alignement egal a 0.5
		pSubDock->iGapY = pDock->iGapY + pDock->iMaxDockHeight;
	}
	else
	{
		pSubDock->fAlign = (pDock->bDirectionUp ? 1 : 0);
		pSubDock->iGapX = (pDock->iGapY + pDock->iMaxDockHeight) * (pDock->bDirectionUp ? -1 : 1);
		if (pDock->bDirectionUp)
			pSubDock->iGapY = g_iScreenWidth[pDock->bHorizontalDock] - (iX + pDock->iWindowPositionX - (pDock->bHorizontalDock ? g_iScreenOffsetX : g_iScreenOffsetY)) - pSubDock->iMaxDockHeight / 2;  // les sous-dock ont un alignement egal a 1.
		else
			pSubDock->iGapY = iX + pDock->iWindowPositionX - pSubDock->iMaxDockHeight / 2;  // les sous-dock ont un alignement egal a 0.
	}
}



void cairo_dock_set_input_shape (CairoDock *pDock)
{
	/*gint   iIgnore;
	gint   iMajor;
	gint   iMinor;

	if (!XShapeQueryExtension (GDK_WINDOW_XDISPLAY (pDock->pWidget->window),
				   &iIgnore,
				   &iIgnore))
		g_print ("No ShapeQueryExtension\n");

	if (!XShapeQueryVersion (GDK_WINDOW_XDISPLAY (pDock->pWidget->window),
				 &iMajor,
				 &iMinor))
		g_print ("No ShapeQueryExtension\n");

	// for shaped input we need at least XShape 1.1
	if (iMajor != 1 && iMinor < 1)
		g_print ("ShapeQueryExtension too old\n");*/
	if (pDock->pShapeBitmap != NULL)
		g_object_unref ((gpointer) pDock->pShapeBitmap);
	
	if (pDock->inputArea.width == 0 || pDock->inputArea.height == 0 || pDock->iRefCount > 0 || pDock->bAutoHide)
	{
		pDock->pShapeBitmap = NULL;
		return ;
	}
	pDock->pShapeBitmap = (GdkBitmap*) gdk_pixmap_new (NULL,
		pDock->bHorizontalDock ? pDock->iMinDockWidth : pDock->iMinDockHeight,
		pDock->bHorizontalDock ? pDock->iMinDockHeight : pDock->iMinDockWidth,
		1);
	//g_print ("%s (%d;%d ; %dx%d\n", __func__, pDock->inputArea.x, pDock->inputArea.y, pDock->inputArea.width, pDock->inputArea.height);
	
	cairo_t *pCairoContext = gdk_cairo_create (pDock->pShapeBitmap);
	cairo_set_source_rgba (pCairoContext, 1.0f, 1.0f, 1.0f, 0.0f);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	cairo_paint (pCairoContext);
	cairo_set_source_rgba (pCairoContext, 1, 1, 1, 1);
	if (pDock->bHorizontalDock)
	{
		cairo_rectangle (pCairoContext,
			pDock->inputArea.x,
			pDock->bDirectionUp ? pDock->inputArea.y : pDock->iCurrentHeight - pDock->inputArea.y - pDock->inputArea.height,
			pDock->inputArea.width,
			pDock->inputArea.height);
	}
	else
	{
		cairo_rectangle (pCairoContext,
			pDock->bDirectionUp ? pDock->inputArea.y : pDock->iCurrentHeight - pDock->inputArea.y - pDock->inputArea.height,
			pDock->iCurrentWidth - pDock->inputArea.x - pDock->inputArea.width,
			pDock->inputArea.height,
			pDock->inputArea.width);
	}
	cairo_fill (pCairoContext);
	cairo_destroy (pCairoContext);
}


  ///////////////////
 /// LINEAR DOCK ///
///////////////////
GList *cairo_dock_calculate_icons_positions_at_rest_linear (GList *pIconList, double fFlatDockWidth, int iXOffset)
{
	//g_print ("%s (%d, +%d)\n", __func__, fFlatDockWidth, iXOffset);
	double x_cumulated = iXOffset;
	double fXMin = 99999;
	GList* ic, *pFirstDrawnElement = NULL;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		if (x_cumulated + icon->fWidth / 2 < 0)
			icon->fXAtRest = x_cumulated + fFlatDockWidth;
		else if (x_cumulated + icon->fWidth / 2 > fFlatDockWidth)
			icon->fXAtRest = x_cumulated - fFlatDockWidth;
		else
			icon->fXAtRest = x_cumulated;

		if (icon->fXAtRest < fXMin)
		{
			fXMin = icon->fXAtRest;
			pFirstDrawnElement = ic;
		}
		//g_print ("%s : fXAtRest = %.2f\n", icon->acName, icon->fXAtRest);

		x_cumulated += icon->fWidth + myIcons.iIconGap;
	}

	return pFirstDrawnElement;
}

double cairo_dock_calculate_max_dock_width (CairoDock *pDock, GList *pFirstDrawnElementGiven, double fFlatDockWidth, double fWidthConstraintFactor, double fExtraWidth)
{
	double fMaxDockWidth = 0.;
	//g_print ("%s (%d)\n", __func__, fFlatDockWidth);
	GList *pIconList = pDock->icons;
	if (pIconList == NULL)
		return 2 * myBackground.iDockRadius + myBackground.iDockLineWidth + 2 * myBackground.iFrameMargin;

	//\_______________ On remet a zero les positions extremales des icones.
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fXMax = -1e4;
		icon->fXMin = 1e4;
	}

	//\_______________ On simule le passage du curseur sur toute la largeur du dock, et on chope la largeur maximale qui s'en degage, ainsi que les positions d'equilibre de chaque icone.
	GList *pFirstDrawnElement = (pFirstDrawnElementGiven != NULL ? pFirstDrawnElementGiven : pIconList);
	//for (int iVirtualMouseX = 0; iVirtualMouseX < fFlatDockWidth; iVirtualMouseX ++)
	GList *ic2;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		cairo_dock_calculate_wave_with_position_linear (pIconList, pFirstDrawnElement, icon->fXAtRest, pDock->fMagnitudeMax, fFlatDockWidth, 0, 0, 0.5, 0, pDock->bDirectionUp);
		ic2 = pFirstDrawnElement;
		do
		{
			icon = ic2->data;

			if (icon->fX + icon->fWidth * icon->fScale > icon->fXMax)
				icon->fXMax = icon->fX + icon->fWidth * icon->fScale;
			if (icon->fX < icon->fXMin)
				icon->fXMin = icon->fX;

			ic2 = cairo_dock_get_next_element (ic2, pDock->icons);
		} while (ic2 != pFirstDrawnElement);
	}
	cairo_dock_calculate_wave_with_position_linear (pIconList, pFirstDrawnElement, fFlatDockWidth - 1, pDock->fMagnitudeMax, fFlatDockWidth, 0, 0, pDock->fAlign, 0, pDock->bDirectionUp);  // pDock->fFoldingFactor
	ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;

		if (icon->fX + icon->fWidth * icon->fScale > icon->fXMax)
			icon->fXMax = icon->fX + icon->fWidth * icon->fScale;
		if (icon->fX < icon->fXMin)
			icon->fXMin = icon->fX;

		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);

	fMaxDockWidth = (icon->fXMax - ((Icon *) pFirstDrawnElement->data)->fXMin) * fWidthConstraintFactor + fExtraWidth;
	fMaxDockWidth = ceil (fMaxDockWidth) + 1;

	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fXMin += fMaxDockWidth / 2;
		icon->fXMax += fMaxDockWidth / 2;
		//g_print ("%s : [%d;%d]\n", icon->acName, (int) icon->fXMin, (int) icon->fXMax);
		icon->fX = icon->fXAtRest;
		icon->fScale = 1;
	}

	return fMaxDockWidth;
}

Icon * cairo_dock_calculate_wave_with_position_linear (GList *pIconList, GList *pFirstDrawnElementGiven, int x_abs, gdouble fMagnitude, double fFlatDockWidth, int iWidth, int iHeight, double fAlign, double fFoldingFactor, gboolean bDirectionUp)
{
	//g_print (">>>>>%s (%d/%.2f, %dx%d, %.2f, %.2f)\n", __func__, x_abs, fFlatDockWidth, iWidth, iHeight, fAlign, fFoldingFactor);
	if (pIconList == NULL)
		return NULL;
	if (x_abs < 0 && iWidth > 0)  // ces cas limite sont la pour empecher les icones de retrecir trop rapidement quand on sort par les cotes.
		///x_abs = -1;
		x_abs = 0;
	else if (x_abs > fFlatDockWidth && iWidth > 0)
		///x_abs = fFlatDockWidth+1;
		x_abs = (int) fFlatDockWidth;
	
	float x_cumulated = 0, fXMiddle, fDeltaExtremum;
	GList* ic, *pointed_ic;
	Icon *icon, *prev_icon;

	GList *pFirstDrawnElement = (pFirstDrawnElementGiven != NULL ? pFirstDrawnElementGiven : pIconList);
	ic = pFirstDrawnElement;
	pointed_ic = (x_abs < 0 ? ic : NULL);
	do
	{
		icon = ic->data;
		x_cumulated = icon->fXAtRest;
		fXMiddle = icon->fXAtRest + icon->fWidth / 2;

		//\_______________ On calcule sa phase (pi/2 au niveau du curseur).
		icon->fPhase = (fXMiddle - x_abs) / myIcons.iSinusoidWidth * G_PI + G_PI / 2;
		if (icon->fPhase < 0)
		{
			icon->fPhase = 0;
		}
		else if (icon->fPhase > G_PI)
		{
			icon->fPhase = G_PI;
		}
		
		//\_______________ On en deduit l'amplitude de la sinusoide au niveau de cette icone, et donc son echelle.
		icon->fScale = 1 + fMagnitude * myIcons.fAmplitude * sin (icon->fPhase);
		if (iWidth > 0 && icon->fPersonnalScale != 0)
		{
			if (icon->fPersonnalScale > 0)
				icon->fScale *= icon->fPersonnalScale;
			else
				icon->fScale *= (1 + icon->fPersonnalScale);
		}
		icon->fY = (bDirectionUp ? iHeight - myBackground.iDockLineWidth - myBackground.iFrameMargin - icon->fScale * icon->fHeight : 0*myBackground.iDockLineWidth + 0*myBackground.iFrameMargin);
		
		//\_______________ Si on avait deja defini l'icone pointee, on peut placer l'icone courante par rapport a la precedente.
		if (pointed_ic != NULL)
		{
			if (ic == pFirstDrawnElement)  // peut arriver si on est en dehors a gauche du dock.
			{
				icon->fX = x_cumulated - 1. * (fFlatDockWidth - iWidth) / 2;
				//g_print ("  en dehors a gauche : icon->fX = %.2f (%.2f)\n", icon->fX, x_cumulated);
			}
			else
			{
				prev_icon = (ic->prev != NULL ? ic->prev->data : cairo_dock_get_last_icon (pIconList));
				icon->fX = prev_icon->fX + (prev_icon->fWidth + myIcons.iIconGap) * prev_icon->fScale;

				if (icon->fX + icon->fWidth * icon->fScale > icon->fXMax - myIcons.fAmplitude * fMagnitude * (icon->fWidth + 1.5*myIcons.iIconGap) / 8 && iWidth != 0)
				{
					//g_print ("  on contraint %s (fXMax=%.2f , fX=%.2f\n", prev_icon->acName, prev_icon->fXMax, prev_icon->fX);
					fDeltaExtremum = icon->fX + icon->fWidth * icon->fScale - (icon->fXMax - myIcons.fAmplitude * fMagnitude * (icon->fWidth + 1.5*myIcons.iIconGap) / 16);
					if (myIcons.fAmplitude != 0)
						icon->fX -= fDeltaExtremum * (1 - (icon->fScale - 1) / myIcons.fAmplitude) * fMagnitude;
				}
			}
			icon->fX = fAlign * iWidth + (icon->fX - fAlign * iWidth) * (1. - fFoldingFactor);
			//g_print ("  a droite : icon->fX = %.2f (%.2f)\n", icon->fX, x_cumulated);
		}
		
		//\_______________ On regarde si on pointe sur cette icone.
		if (x_cumulated + icon->fWidth + .5*myIcons.iIconGap >= x_abs && x_cumulated - .5*myIcons.iIconGap <= x_abs && pointed_ic == NULL)  // on a trouve l'icone sur laquelle on pointe.
		{
			pointed_ic = ic;
			///icon->bPointed = TRUE;
			icon->bPointed = (x_abs != (int) fFlatDockWidth && x_abs != 0);
			icon->fX = x_cumulated - (fFlatDockWidth - iWidth) / 2 + (1 - icon->fScale) * (x_abs - x_cumulated + .5*myIcons.iIconGap);
			icon->fX = fAlign * iWidth + (icon->fX - fAlign * iWidth) * (1. - fFoldingFactor);
			//g_print ("  icone pointee : fX = %.2f (%.2f, %d)\n", icon->fX, x_cumulated, icon->bPointed);
		}
		else
			icon->bPointed = FALSE;
			
		ic = cairo_dock_get_next_element (ic, pIconList);
	} while (ic != pFirstDrawnElement);
	
	//\_______________ On place les icones precedant l'icone pointee par rapport a celle-ci.
	if (pointed_ic == NULL)  // on est a droite des icones.
	{
		pointed_ic = (pFirstDrawnElement->prev == NULL ? g_list_last (pIconList) : pFirstDrawnElement->prev);
		icon = pointed_ic->data;
		icon->fX = x_cumulated - (fFlatDockWidth - iWidth) / 2 + (1 - icon->fScale) * (icon->fWidth + .5*myIcons.iIconGap);
		icon->fX = fAlign * iWidth + (icon->fX - fAlign * iWidth) * (1 - fFoldingFactor);
		//g_print ("  en dehors a droite : icon->fX = %.2f (%.2f)\n", icon->fX, x_cumulated);
	}
	
	ic = pointed_ic;
	while (ic != pFirstDrawnElement)
	{
		icon = ic->data;
		
		ic = ic->prev;
		if (ic == NULL)
			ic = g_list_last (pIconList);
			
		prev_icon = ic->data;
		
		prev_icon->fX = icon->fX - (prev_icon->fWidth + myIcons.iIconGap) * prev_icon->fScale;
		//g_print ("fX <- %.2f; fXMin : %.2f\n", prev_icon->fX, prev_icon->fXMin);
		if (prev_icon->fX < prev_icon->fXMin + myIcons.fAmplitude * fMagnitude * (prev_icon->fWidth + 1.5*myIcons.iIconGap) / 8 && iWidth != 0 && x_abs < iWidth && fMagnitude > 0)  /// && prev_icon->fPhase == 0   // on rajoute 'fMagnitude > 0' sinon il y'a un leger "saut" du aux contraintes a gauche de l'icone pointee.
		{
			//g_print ("  on contraint %s (fXMin=%.2f , fX=%.2f\n", prev_icon->acName, prev_icon->fXMin, prev_icon->fX);
			fDeltaExtremum = prev_icon->fX - (prev_icon->fXMin + myIcons.fAmplitude * fMagnitude * (prev_icon->fWidth + 1.5*myIcons.iIconGap) / 16);
			if (myIcons.fAmplitude != 0)
				prev_icon->fX -= fDeltaExtremum * (1 - (prev_icon->fScale - 1) / myIcons.fAmplitude) * fMagnitude;
		}
		prev_icon->fX = fAlign * iWidth + (prev_icon->fX - fAlign * iWidth) * (1. - fFoldingFactor);
		//g_print ("  prev_icon->fX : %.2f\n", prev_icon->fX);
	}

	return pointed_ic->data;
}

Icon *cairo_dock_apply_wave_effect_linear (CairoDock *pDock)
{
	//\_______________ On calcule la position du curseur dans le referentiel du dock a plat.
	int dx = pDock->iMouseX - pDock->iCurrentWidth / 2;  // ecart par rapport au milieu du dock a plat.
	int x_abs = dx + pDock->fFlatDockWidth / 2;  // ecart par rapport a la gauche du dock minimal  plat.

	//\_______________ On calcule l'ensemble des parametres des icones.
	double fMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex) * pDock->fMagnitudeMax;
	Icon *pPointedIcon = cairo_dock_calculate_wave_with_position_linear (pDock->icons, pDock->pFirstDrawnElement, x_abs, fMagnitude, pDock->fFlatDockWidth, pDock->iCurrentWidth, pDock->iCurrentHeight, pDock->fAlign, pDock->fFoldingFactor, pDock->bDirectionUp);  // iMaxDockWidth
	return pPointedIcon;
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


void cairo_dock_check_if_mouse_inside_linear (CairoDock *pDock)
{
	CairoDockMousePositionType iMousePositionType;
	int iWidth = pDock->iCurrentWidth;
	int iHeight = pDock->iCurrentHeight;
	///int iExtraHeight = (pDock->bAtBottom ? 0 : myLabels.iLabelSize);
	int iExtraHeight = 0;  /// il faudrait voir si on a un sous-dock ou un dialogue au dessus :-/
	int iMouseX = pDock->iMouseX;
	int iMouseY = pDock->iMouseY;
	//g_print ("%s (%dx%d, %dx%d, %f)\n", __func__, iMouseX, iMouseY, iWidth, iHeight, pDock->fFoldingFactor);

	//\_______________ On regarde si le curseur est dans le dock ou pas, et on joue sur la taille des icones en consequence.
	int x_abs = pDock->iMouseX + (pDock->fFlatDockWidth - iWidth) / 2;  // abscisse par rapport a la gauche du dock minimal plat.
	gboolean bMouseInsideDock = (x_abs >= 0 && x_abs <= pDock->fFlatDockWidth && iMouseX > 0 && iMouseX < iWidth);
	//g_print ("bMouseInsideDock : %d (%d;%d/%.2f)\n", bMouseInsideDock, pDock->bInside, x_abs, pDock->fFlatDockWidth);

	if (! bMouseInsideDock)
	{
		double fSideMargin = fabs (pDock->fAlign - .5) * (iWidth - pDock->fFlatDockWidth);
		if (x_abs < - fSideMargin || x_abs > pDock->fFlatDockWidth + fSideMargin)
			iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;
		else
			iMousePositionType = CAIRO_DOCK_MOUSE_ON_THE_EDGE;
	}
	else if ((pDock->bDirectionUp && iMouseY >= iExtraHeight && iMouseY <= iHeight) || (!pDock->bDirectionUp && iMouseY >= 0 && iMouseY <= iHeight - iExtraHeight))  // et en plus on est dedans en y.  //  && pPointedIcon != NULL
	{
		//g_print ("on est dedans en x et en y (iMouseX=%d => x_abs=%d)\n", iMouseX, x_abs);
		//pDock->bInside = TRUE;
		iMousePositionType = CAIRO_DOCK_MOUSE_INSIDE;
	}
	else
		iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;

	pDock->iMousePositionType = iMousePositionType;
}

void cairo_dock_manage_mouse_position (CairoDock *pDock)
{
	static gboolean bReturn = FALSE;
	switch (pDock->iMousePositionType)
	{
		case CAIRO_DOCK_MOUSE_INSIDE :
			//g_print ("INSIDE\n");
			if (cairo_dock_entrance_is_allowed (pDock) && pDock->iMagnitudeIndex < CAIRO_DOCK_NB_MAX_ITERATIONS && ! pDock->bIsGrowingUp/** && cairo_dock_get_removing_or_inserting_icon (pDock->icons) == NULL*/)  // on est dedans et la taille des icones est non maximale bien que le dock ne soit pas en train de grossir.  ///  && pDock->iSidMoveDown == 0
			{
				//g_print ("on est dedans en x et en y et la taille des icones est non maximale bien qu'aucune icone  ne soit animee (%d;%d)\n", pDock->bAtBottom, pDock->bInside);
				//pDock->bInside = TRUE;
				if ((pDock->bAtBottom && pDock->iRefCount == 0 && ! pDock->bAutoHide) || (pDock->iCurrentWidth != pDock->iMaxDockWidth || pDock->iCurrentHeight != pDock->iMaxDockHeight) || (!pDock->bInside))  // on le fait pas avec l'auto-hide, car un signal d'entree est deja emis a cause des mouvements/redimensionnements de la fenetre, et en rajouter un ici fout le boxon.  // !pDock->bInside ajoute pour le bug du chgt de bureau.
				{
					//g_print ("  on emule une re-rentree (pDock->iMagnitudeIndex:%d)\n", pDock->iMagnitudeIndex);
					g_signal_emit_by_name (pDock->pWidget, "enter-notify-event", NULL, &bReturn);
				}
				else // on se contente de faire grossir les icones.
				{
					//g_print ("  on se contente de faire grossir les icones\n");
					pDock->bAtBottom = FALSE;
					cairo_dock_start_growing (pDock);
					
					if (pDock->iSidMoveDown != 0)
					{
						g_source_remove (pDock->iSidMoveDown);
						pDock->iSidMoveDown = 0;
					}
					if (pDock->bAutoHide && pDock->iRefCount == 0 && pDock->iSidMoveUp == 0)
						pDock->iSidMoveUp = g_timeout_add (40, (GSourceFunc) cairo_dock_move_up, pDock);
				}
			}
		break ;

		case CAIRO_DOCK_MOUSE_ON_THE_EDGE :
			///pDock->fDecorationsOffsetX = - pDock->iCurrentWidth / 2;  // on fixe les decorations.
			if (pDock->iMagnitudeIndex > 0 && ! pDock->bIsGrowingUp)
				cairo_dock_start_shrinking (pDock);
		break ;

		case CAIRO_DOCK_MOUSE_OUTSIDE :
			//g_print ("en dehors du dock (bIsShrinkingDown:%d;bIsGrowingUp:%d;iMagnitudeIndex:%d;bAtBottom:%d)\n", pDock->bIsShrinkingDown, pDock->bIsGrowingUp, pDock->iMagnitudeIndex, pDock->bAtBottom);
			///pDock->fDecorationsOffsetX = - pDock->iCurrentWidth / 2;  // on fixe les decorations.
			if (! pDock->bIsGrowingUp && ! pDock->bIsShrinkingDown && pDock->iSidLeaveDemand == 0 && ! pDock->bAtBottom)  // bAtBottom ajoute pour la 1.5.4
			{
				//g_print ("on force a quitter (iRefCount:%d)\n", pDock->iRefCount);
				if (pDock->iRefCount > 0 && myAccessibility.iLeaveSubDockDelay > 0)
					pDock->iSidLeaveDemand = g_timeout_add (myAccessibility.iLeaveSubDockDelay, (GSourceFunc) cairo_dock_emit_leave_signal, (gpointer) pDock);
				else
					cairo_dock_emit_leave_signal (pDock);
			}
		break ;
	}
}

#define make_icon_avoid_mouse(icon) \
	cairo_dock_mark_icon_as_avoiding_mouse (icon);\
	icon->fAlpha = 0.75;\
	if (myIcons.fAmplitude != 0)\
		icon->fDrawX += icon->fWidth / 2 * (icon->fScale - 1) / myIcons.fAmplitude * (icon->fPhase < G_PI/2 ? -1 : 1);

static gboolean _cairo_dock_check_can_drop_linear (CairoDock *pDock, CairoDockIconType iType, double fMargin)
{
	gboolean bCanDrop = FALSE;
	Icon *icon;
	GList *pFirstDrawnElement = (pDock->pFirstDrawnElement != NULL ? pDock->pFirstDrawnElement : pDock->icons);
	GList *ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;
		if (icon->bPointed)  // && icon->iAnimationState != CAIRO_DOCK_FOLLOW_MOUSE
		{
			if (pDock->iMouseX < icon->fDrawX + icon->fWidth * icon->fScale * fMargin)  // on est a gauche.  // fDrawXAtRest
			{
				Icon *prev_icon = cairo_dock_get_previous_element (ic, pDock->icons) -> data;
				if ((cairo_dock_get_icon_order (icon) == cairo_dock_get_group_order (iType) || cairo_dock_get_icon_order (prev_icon) == cairo_dock_get_group_order (iType)))  // && prev_icon->iAnimationType != CAIRO_DOCK_FOLLOW_MOUSE
				{
					make_icon_avoid_mouse (icon);
					make_icon_avoid_mouse (prev_icon);
					//g_print ("%s> <%s\n", prev_icon->acName, icon->acName);
					bCanDrop = TRUE;
				}
			}
			else if (pDock->iMouseX > icon->fDrawX + icon->fWidth * icon->fScale * (1 - fMargin))  // on est a droite.  // fDrawXAtRest
			{
				Icon *next_icon = cairo_dock_get_next_element (ic, pDock->icons) -> data;
				if ((icon->iType == iType || next_icon->iType == iType))  // && next_icon->iAnimationType != CAIRO_DOCK_FOLLOW_MOUSE
				{
					make_icon_avoid_mouse (icon);
					make_icon_avoid_mouse (next_icon);
					//g_print ("%s> <%s\n", icon->acName, next_icon->acName);
					bCanDrop = TRUE;
				}
				ic = cairo_dock_get_next_element (ic, pDock->icons);  // on la saute.
				if (ic == pFirstDrawnElement)
					break ;
			}  // else on est dessus.
		}
		else
			cairo_dock_stop_marking_icon_as_avoiding_mouse (icon);
		
		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);
	
	return bCanDrop;
}


void cairo_dock_check_can_drop_linear (CairoDock *pDock)
{
	if (pDock->icons == NULL)
		return;
	
	if (pDock->bIsDragging)
		pDock->bCanDrop = _cairo_dock_check_can_drop_linear (pDock, pDock->iAvoidingMouseIconType, pDock->fAvoidingMouseMargin);
	else
		pDock->bCanDrop = FALSE;
}

void cairo_dock_stop_marking_icons (CairoDock *pDock)
{
	if (pDock->icons == NULL)
		return;
	//g_print ("%s (%d)\n", __func__, iType);

	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		cairo_dock_stop_marking_icon_as_avoiding_mouse (icon);
	}
}


void cairo_dock_scroll_dock_icons (CairoDock *pDock, int iScrollAmount)
{
	if (iScrollAmount == 0)  // fin de scroll
	{
		cairo_dock_set_icons_geometry_for_window_manager (pDock);
		return ;
	}
	
	//\_______________ On fait tourner les icones.
	pDock->iScrollOffset += iScrollAmount;
	if (pDock->iScrollOffset >= pDock->fFlatDockWidth)
		pDock->iScrollOffset -= pDock->fFlatDockWidth;
	if (pDock->iScrollOffset < 0)
		pDock->iScrollOffset += pDock->fFlatDockWidth;
	
	pDock->calculate_max_dock_size (pDock);  // recalcule le pFirstDrawnElement.
	
	//\_______________ On recalcule toutes les icones.
	Icon *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	Icon *pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
	gtk_widget_queue_draw (pDock->pWidget);
	
	//\_______________ On gere le changement d'icone.
	if (pPointedIcon != pLastPointedIcon)
	{
		cairo_dock_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);
	}
}


GList *cairo_dock_get_first_drawn_element_linear (GList *icons)
{
	Icon *icon;
	GList *ic;
	GList *pFirstDrawnElement = NULL;
	for (ic = icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->bPointed)
			break ;
	}
	
	if (ic == NULL || ic->next == NULL)  // derniere icone ou aucune pointee.
		pFirstDrawnElement = icons;
	else
		pFirstDrawnElement = ic->next;
	return pFirstDrawnElement;
}

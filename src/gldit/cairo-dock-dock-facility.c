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
#include <gtk/gtk.h>

#include "cairo-dock-applications-manager.h"  // cairo_dock_set_icons_geometry_for_window_manager
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-separator-manager.h"  // GLDI_OBJECT_IS_SEPARATOR_ICON
#include "cairo-dock-stack-icon-manager.h"  // GLDI_OBJECT_IS_DRAWER_ICON
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-backends-manager.h"  // myBackendsParam.fSubDockSizeRatio
#include "cairo-dock-log.h"
#include "cairo-dock-style-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dialog-manager.h"  // gldi_dialogs_replace_all
#include "cairo-dock-indicator-manager.h"  // myIndicators.bUseClassIndic
#include "cairo-dock-animations.h"
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_get*
#include "cairo-dock-data-renderer.h"  // cairo_dock_reload_data_renderer_on_icon
#include "cairo-dock-opengl.h"  // gldi_gl_container_begin_draw

extern CairoDockGLConfig g_openglConfig;
#include "cairo-dock-dock-facility.h"

extern gboolean g_bUseOpenGL;  // for cairo_dock_make_preview()


/**
 * @pre iMaxIconHeight and fFlatDockWidth have to have been updated
 */
void cairo_dock_update_dock_size (CairoDock *pDock)
{
	g_return_if_fail (pDock != NULL);
	//g_print ("%s (%p, %d)\n", __func__, pDock, pDock->iRefCount);
	if (pDock->iSidUpdateDockSize != 0)
	{
		//g_print (" -> delayed\n");
		return;
	}
	int iPrevMaxDockHeight = pDock->iMaxDockHeight;
	int iPrevMaxDockWidth = pDock->iMaxDockWidth;
	
	//\__________________________ First compute the dock's size.
	
	// set the icons' size back to their default, otherwise max_dock_size could be wrong
	if (pDock->container.fRatio != 0)
	{
		GList *ic;
		Icon *icon;
		pDock->fFlatDockWidth = -myIconsParam.iIconGap;
		pDock->iMaxIconHeight = 0;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			icon->fWidth /= pDock->container.fRatio;
			icon->fHeight /= pDock->container.fRatio;
			pDock->fFlatDockWidth += icon->fWidth + myIconsParam.iIconGap;
			if (! GLDI_OBJECT_IS_SEPARATOR_ICON (icon))
				pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);
		}
		if (pDock->iMaxIconHeight == 0)
			pDock->iMaxIconHeight = 10;
		pDock->container.fRatio = 1.;
	}
	
	// compute the size of the dock.
	pDock->iActiveWidth = pDock->iActiveHeight = 0;
	pDock->pRenderer->compute_size (pDock);
	if (pDock->iActiveWidth == 0)
		pDock->iActiveWidth = pDock->iMaxDockWidth;
	if (pDock->iActiveHeight == 0)
		pDock->iActiveHeight = pDock->iMaxDockHeight;
	
	// in case it's larger than the screen, iterate on the ratio until it fits the screen's width
	int iScreenHeight = gldi_dock_get_screen_height (pDock);
	double hmax = pDock->iMaxIconHeight;
	int iMaxAuthorizedWidth = cairo_dock_get_max_authorized_dock_width (pDock);
	int n = 0;  // counter to ensure we'll not loop forever.
	do
	{
		double fPrevRatio = pDock->container.fRatio;
		//g_print ("  %s (%d / %d)\n", __func__, (int)pDock->iMaxDockWidth, iMaxAuthorizedWidth);
		if (pDock->iMaxDockWidth > iMaxAuthorizedWidth)
		{
			pDock->container.fRatio *= (double)iMaxAuthorizedWidth / pDock->iMaxDockWidth;
		}
		else
		{
			double fMaxRatio = (pDock->iRefCount == 0 ? 1 : myBackendsParam.fSubDockSizeRatio);
			if (pDock->container.fRatio < fMaxRatio)
			{
				pDock->container.fRatio *= (double)iMaxAuthorizedWidth / pDock->iMaxDockWidth;
				pDock->container.fRatio = MIN (pDock->container.fRatio, fMaxRatio);
			}
			else
				pDock->container.fRatio = fMaxRatio;
		}
		
		if (pDock->iMaxDockHeight > iScreenHeight)
		{
			pDock->container.fRatio = MIN (pDock->container.fRatio, fPrevRatio * iScreenHeight / pDock->iMaxDockHeight);
		}
		
		if (fPrevRatio != pDock->container.fRatio)
		{
			//g_print ("  -> change of the ratio : %.3f -> %.3f (%d, %d try)\n", fPrevRatio, pDock->container.fRatio, pDock->iRefCount, n);
			Icon *icon;
			GList *ic;
			pDock->fFlatDockWidth = -myIconsParam.iIconGap;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				icon->fWidth *= pDock->container.fRatio / fPrevRatio;
				icon->fHeight *= pDock->container.fRatio / fPrevRatio;
				pDock->fFlatDockWidth += icon->fWidth + myIconsParam.iIconGap;
			}
			hmax *= pDock->container.fRatio / fPrevRatio;
			
			pDock->iActiveWidth = pDock->iActiveHeight = 0;
			pDock->pRenderer->compute_size (pDock);
			if (pDock->iActiveWidth == 0)
				pDock->iActiveWidth = pDock->iMaxDockWidth;
			if (pDock->iActiveHeight == 0)
				pDock->iActiveHeight = pDock->iMaxDockHeight;
		}
		
		//g_print ("*** ratio : %.3f -> %.3f\n", fPrevRatio, pDock->container.fRatio);
		n ++;
	} while ((pDock->iMaxDockWidth > iMaxAuthorizedWidth || pDock->iMaxDockHeight > iScreenHeight || (pDock->container.fRatio < 1 && pDock->iMaxDockWidth < iMaxAuthorizedWidth-5)) && n < 8);
	pDock->iMaxIconHeight = hmax;
	//g_print (">>> iMaxIconHeight : %d, ratio : %.2f, fFlatDockWidth : %.2f\n", (int) pDock->iMaxIconHeight, pDock->container.fRatio, pDock->fFlatDockWidth);
	
	//\__________________________ Then take the necessary actions due to the new size.
	// calculate the position of icons in the new frame.
	cairo_dock_calculate_dock_icons (pDock);
	
	// update the dock's shape.
	if (iPrevMaxDockHeight == pDock->iMaxDockHeight && iPrevMaxDockWidth == pDock->iMaxDockWidth)  // if the size has changed, shapes will be updated by the "configure" callback, so we don't need to do it here; if not, we do it in case the icons define a new shape (ex.: separators in Panel view) or in case the screen edge has changed.
	{
		cairo_dock_update_input_shape (pDock);  // done after the icons' position is known.
		switch (pDock->iInputState)  // update the input zone
		{
			case CAIRO_DOCK_INPUT_ACTIVE: cairo_dock_set_input_shape_active (pDock); break;
			case CAIRO_DOCK_INPUT_AT_REST: cairo_dock_set_input_shape_at_rest (pDock); break;
			default: break;  // if hidden, nothing to do.
		}
	}
	
	if (iPrevMaxDockHeight == pDock->iMaxDockHeight && iPrevMaxDockWidth == pDock->iMaxDockWidth)  // same remark as for the input shape.
	{
		/// TODO: check that...
		///pDock->bWMIconsNeedUpdate = TRUE;
		cairo_dock_trigger_set_WM_icons_geometry (pDock);
	}
	
	// if the size has changed, move the dock to keep it centered.
	if (gldi_container_is_visible (CAIRO_CONTAINER (pDock)) && (iPrevMaxDockHeight != pDock->iMaxDockHeight || iPrevMaxDockWidth != pDock->iMaxDockWidth))
	{
		//g_print ("*******%s (%dx%d -> %dx%d)\n", __func__, iPrevMaxDockWidth, iPrevMaxDockHeight, pDock->iMaxDockWidth, pDock->iMaxDockHeight);
		cairo_dock_move_resize_dock (pDock);
	}
	
	// reload its background.
	cairo_dock_trigger_load_dock_background (pDock);
	
	// update the space reserved on the screen.
	if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_RESERVE)
		cairo_dock_reserve_space_for_dock (pDock, TRUE);
}

static gboolean _update_dock_size_idle (CairoDock *pDock)
{
	pDock->iSidUpdateDockSize = 0;
	cairo_dock_update_dock_size (pDock);
	gtk_widget_queue_draw (pDock->container.pWidget);
	return FALSE;
}
void cairo_dock_trigger_update_dock_size (CairoDock *pDock)
{
	if (pDock->iSidUpdateDockSize == 0)
	{
		pDock->iSidUpdateDockSize = g_idle_add ((GSourceFunc) _update_dock_size_idle, pDock);
	}
}

static gboolean _emit_leave_signal_delayed (CairoDock *pDock)
{
	cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
	pDock->iSidLeaveDemand = 0;
	return FALSE;
}
static void cairo_dock_manage_mouse_position (CairoDock *pDock)
{
	switch (pDock->iMousePositionType)
	{
		case CAIRO_DOCK_MOUSE_INSIDE :
			//g_print ("INSIDE (%d;%d;%d;%d;%d)\n", cairo_dock_entrance_is_allowed (pDock), pDock->iMagnitudeIndex, pDock->bIsGrowingUp, pDock->bIsShrinkingDown, pDock->iInputState);
			if (cairo_dock_entrance_is_allowed (pDock)
			    && ((pDock->iMagnitudeIndex < CAIRO_DOCK_NB_MAX_ITERATIONS
			         && ! pDock->bIsGrowingUp)
			       || pDock->bIsShrinkingDown)
			    && pDock->iInputState != CAIRO_DOCK_INPUT_HIDDEN
			    && (pDock->iInputState != CAIRO_DOCK_INPUT_AT_REST
			        || pDock->bIsDragging))
			        /* We are inside and icons' size is not the maximum even if
			         * the dock is not growing but we respect the 'cache' and
			         * 'idle' states
			         */
			{
				if (pDock->iRefCount != 0 && !pDock->container.bInside)
				{
					
					break;
				}
				//pDock->container.bInside = TRUE;
				/* we do it not with 'auto-hide' mode because a entry signal is
				 * already sent due to the movements and resize of the window
				 * and we re-add it one here and it's a problem
				 * '! pDock->container.bInside' has been added to fix the bug
				 * when switching between desktops
				if ((pDock->bAtBottom && pDock->iRefCount == 0
				     && ! pDock->bAutoHide)
				   || (pDock->container.iWidth != pDock->iMaxDockWidth
				       || pDock->container.iHeight != pDock->iMaxDockHeight)
				   || ! pDock->container.bInside)
				 */
				if ((pDock->iMagnitudeIndex == 0 && pDock->iRefCount == 0
				     && ! pDock->bAutoHide && ! pDock->bIsGrowingUp)
				   || !pDock->container.bInside)
				   /* we are probably a little bit paranoia here, especially
				    * with the first case ... anyway, if we missed the
				    * 'enter' event for some reason, force it here.
				    */
				{
					//g_print ("  we emulate a re-entry (pDock->iMagnitudeIndex:%d)\n", pDock->iMagnitudeIndex);
					cairo_dock_emit_enter_signal (CAIRO_CONTAINER (pDock));
				}
				else // we settle for growing icons
				{
					//g_print ("  we settle for growing icons\n");
					cairo_dock_start_growing (pDock);
					if (pDock->bAutoHide && pDock->iRefCount == 0)
						cairo_dock_start_showing (pDock);
				}
			}
		break ;

		case CAIRO_DOCK_MOUSE_ON_THE_EDGE :
			if (pDock->iMagnitudeIndex > 0 && ! pDock->bIsGrowingUp)
				cairo_dock_start_shrinking (pDock);
		break ;

		case CAIRO_DOCK_MOUSE_OUTSIDE :
			//g_print ("en dehors du dock (bIsShrinkingDown:%d;bIsGrowingUp:%d;iMagnitudeIndex:%d)\n", pDock->bIsShrinkingDown, pDock->bIsGrowingUp, pDock->iMagnitudeIndex);
			if (! pDock->bIsGrowingUp && ! pDock->bIsShrinkingDown
			    && pDock->iSidLeaveDemand == 0 && pDock->iMagnitudeIndex > 0
			    && ! pDock->bIconIsFlyingAway)
			{
				if (pDock->iRefCount > 0)
				{
					Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
					if (pPointingIcon && pPointingIcon->bPointed)  // sous-dock pointe, on le laisse en position haute.
						return;
				}
				//g_print ("on force a quitter (iRefCount:%d; bIsGrowingUp:%d; iMagnitudeIndex:%d)\n", pDock->iRefCount, pDock->bIsGrowingUp, pDock->iMagnitudeIndex);
				pDock->iSidLeaveDemand = g_timeout_add (MAX (myDocksParam.iLeaveSubDockDelay, 300), (GSourceFunc) _emit_leave_signal_delayed, (gpointer) pDock);
			}
		break ;
	}
}
Icon *cairo_dock_calculate_dock_icons (CairoDock *pDock)
{
	Icon *pPointedIcon = pDock->pRenderer->calculate_icons (pDock);
	cairo_dock_manage_mouse_position (pDock);
	return pPointedIcon;
	/**if (pDock->iMousePositionType == CAIRO_DOCK_MOUSE_INSIDE)
	{
		return pPointedIcon;
	}
	else
	{
		if (pPointedIcon)
			pPointedIcon->bPointed = FALSE;
		return NULL;
	}*/
}



  ////////////////////////////////
 /// WINDOW SIZE AND POSITION ///
////////////////////////////////

#define CANT_RESERVE_SPACE_WARNING "It's only possible to reserve space from the edge of the screen and not on the middle of two screens."

#define _has_multiple_screens_and_on_one_screen(iNumScreen) (g_desktopGeometry.iNbScreens > 1 && iNumScreen > -1)

void cairo_dock_reserve_space_for_dock (CairoDock *pDock, gboolean bReserve)
{
	int left=0, right=0, top=0, bottom=0;
	int left_start_y=0, left_end_y=0, right_start_y=0, right_end_y=0, top_start_x=0, top_end_x=0, bottom_start_x=0, bottom_end_x=0;
	
	if (bReserve)
	{
		int w = pDock->iMinDockWidth;
		int h = pDock->iMinDockHeight;
		int x, y;  // position that should have dock's window if it has a minimum size.
		cairo_dock_get_window_position_at_balance (pDock, w, h, &x, &y);

		if (pDock->container.bDirectionUp)
		{
			if (pDock->container.bIsHorizontal)
			{
				if (_has_multiple_screens_and_on_one_screen (pDock->iNumScreen)
					&& cairo_dock_get_screen_position_y (pDock->iNumScreen) // y offset
						+ cairo_dock_get_screen_height (pDock->iNumScreen)  // height of the current screen
						< gldi_desktop_get_height ()) // total height
					cd_warning (CANT_RESERVE_SPACE_WARNING);
				else
				{
					bottom = h + pDock->iGapY;
					bottom_start_x = x;
					bottom_end_x = x + w;
				}
			}
			else
			{
				if (_has_multiple_screens_and_on_one_screen (pDock->iNumScreen)
					&& cairo_dock_get_screen_position_x (pDock->iNumScreen) // x offset
						+ cairo_dock_get_screen_width (pDock->iNumScreen)  // width of the current screen
						< gldi_desktop_get_width ()) // total width
					cd_warning (CANT_RESERVE_SPACE_WARNING);
				else
				{
					right = h + pDock->iGapY;
					right_start_y = x;
					right_end_y = x + w;
				}
			}
		}
		else
		{
			if (pDock->container.bIsHorizontal)
			{
				if (_has_multiple_screens_and_on_one_screen (pDock->iNumScreen)
					&& cairo_dock_get_screen_position_y (pDock->iNumScreen) > 0)
					cd_warning (CANT_RESERVE_SPACE_WARNING);
				else
				{
					top = h + pDock->iGapY;
					top_start_x = x;
					top_end_x = x + w;
				}
			}
			else
			{
				if (_has_multiple_screens_and_on_one_screen (pDock->iNumScreen)
					&& cairo_dock_get_screen_position_x (pDock->iNumScreen) > 0)
					cd_warning (CANT_RESERVE_SPACE_WARNING);
				else
				{
					left = h + pDock->iGapY;
					left_start_y = x;
					left_end_y = x + w;
				}
			}
		}
	}
	gldi_container_reserve_space (CAIRO_CONTAINER(pDock), left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);
}

void cairo_dock_prevent_dock_from_out_of_screen (CairoDock *pDock)
{
	int x, y;  // position of the invariant point of the dock.
	x = pDock->container.iWindowPositionX +  pDock->container.iWidth * pDock->fAlign;
	y = (pDock->container.bDirectionUp ? pDock->container.iWindowPositionY + pDock->container.iHeight : pDock->container.iWindowPositionY);
	//cd_debug ("%s (%d;%d)", __func__, x, y);
	
	int W = gldi_dock_get_screen_width (pDock), H = gldi_dock_get_screen_height (pDock);
	pDock->iGapX = x - W * pDock->fAlign;
	pDock->iGapY = (pDock->container.bDirectionUp ? H - y : y);
	//cd_debug (" -> (%d;%d)", pDock->iGapX, pDock->iGapY);
	
	if (pDock->iGapX < - W/2)
		pDock->iGapX = - W/2;
	if (pDock->iGapX > W/2)
		pDock->iGapX = W/2;
	if (pDock->iGapY < 0)
		pDock->iGapY = 0;
	if (pDock->iGapY > H)
		pDock->iGapY = H;
}

#define CD_VISIBILITY_MARGIN 20
void cairo_dock_get_window_position_at_balance (CairoDock *pDock, int iNewWidth, int iNewHeight, int *iNewPositionX, int *iNewPositionY)
{
	int W = gldi_dock_get_screen_width (pDock), H = gldi_dock_get_screen_height (pDock);
	int iWindowPositionX = (W - iNewWidth) * pDock->fAlign + pDock->iGapX;
	if (pDock->iRefCount == 0 && pDock->fAlign != .5)
		iWindowPositionX += (.5 - pDock->fAlign) * (pDock->iMaxDockWidth - iNewWidth);
	int iWindowPositionY = (pDock->container.bDirectionUp ? H - iNewHeight - pDock->iGapY : pDock->iGapY);
	//g_print ("pDock->iGapX : %d => iWindowPositionX <- %d\n", pDock->iGapX, iWindowPositionX);
	//g_print ("iNewHeight : %d -> pDock->container.iWindowPositionY <- %d\n", iNewHeight, iWindowPositionY);
	
	if (pDock->iRefCount == 0)
	{
		if (iWindowPositionX + iNewWidth < CD_VISIBILITY_MARGIN)
			iWindowPositionX = CD_VISIBILITY_MARGIN - iNewWidth;
		else if (iWindowPositionX > W - CD_VISIBILITY_MARGIN)
			iWindowPositionX = W - CD_VISIBILITY_MARGIN;
	}
	else
	{
		if (iWindowPositionX < - pDock->iLeftMargin)
			iWindowPositionX = - pDock->iLeftMargin;
		else if (iWindowPositionX > W - iNewWidth + pDock->iMinRightMargin)
			iWindowPositionX = W - iNewWidth + pDock->iMinRightMargin;
	}
	if (iWindowPositionY < - pDock->iMaxIconHeight)
		iWindowPositionY = - pDock->iMaxIconHeight;
	else if (iWindowPositionY > H - iNewHeight + pDock->iMaxIconHeight)
		iWindowPositionY = H - iNewHeight + pDock->iMaxIconHeight;

	*iNewPositionX = iWindowPositionX + gldi_dock_get_screen_offset_x (pDock);
	*iNewPositionY = iWindowPositionY + gldi_dock_get_screen_offset_y (pDock);
	//g_print ("POSITION : %d+%d ; %d+%d\n", iWindowPositionX, pDock->iScreenOffsetX, iWindowPositionY, pDock->iScreenOffsetY);
}

static gboolean _move_resize_dock (CairoDock *pDock)
{
	int iNewWidth = pDock->iMaxDockWidth;
	int iNewHeight = pDock->iMaxDockHeight;
	int iNewPositionX, iNewPositionY;
	cairo_dock_get_window_position_at_balance (pDock, iNewWidth, iNewHeight, &iNewPositionX, &iNewPositionY);
	/* We can't intercept the case where the new dimensions == current ones
	 * because we can have 2 resizes at the "same" time and they will cancel
	 * themselves (remove + insert of one icon). We need 2 configure otherwise
	 * the size will be blocked to the value of the first 'configure'
	 */
	//g_print (" -> %dx%d, %d;%d\n", iNewWidth, iNewHeight, iNewPositionX, iNewPositionY);

	if (pDock->container.bIsHorizontal)
	{
		if (gldi_container_is_wayland_backend ())
			gdk_window_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pDock)), iNewWidth, iNewHeight);
		else gdk_window_move_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pDock)),
				iNewPositionX,
				iNewPositionY,
				iNewWidth,
				iNewHeight);
		/* When we have two gdk_window_move_resize in a row, Compiz will
		 * disturbed and it will block the draw of the dock. It seems Compiz
		 * sends too much 'configure' compare to Metacity. 
		 */
	}
	else
	{
		if (gldi_container_is_wayland_backend ())
			gdk_window_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pDock)), iNewHeight, iNewWidth);
		else gdk_window_move_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pDock)),
				iNewPositionY,
				iNewPositionX,
				iNewHeight,
				iNewWidth);		
	}
	pDock->iSidMoveResize = 0;
	return FALSE;
}

void cairo_dock_move_resize_dock (CairoDock *pDock)
{
	//g_print ("*********%s (current : %dx%d, %d;%d)\n", __func__, pDock->container.iWidth, pDock->container.iHeight, pDock->container.iWindowPositionX, pDock->container.iWindowPositionY);
	if (pDock->iSidMoveResize == 0)
	{
		pDock->iSidMoveResize = g_idle_add ((GSourceFunc)_move_resize_dock, pDock);
	}
	return ;
}


  ///////////////////
 /// INPUT SHAPE ///
///////////////////
static cairo_region_t *_cairo_dock_create_input_shape (CairoDock *pDock, int w, int h)
{
 	int W = pDock->iMaxDockWidth;
	int H = pDock->iMaxDockHeight;
	if (W == 0 || H == 0)  // very unlikely to happen, but anyway avoid this case.
	{
		return NULL;
	}
	
	double offset = (W - pDock->iActiveWidth) * pDock->fAlign + (pDock->iActiveWidth - w) / 2;
	
	cairo_region_t *pShapeBitmap;
	if (pDock->container.bIsHorizontal)
	{
		pShapeBitmap = gldi_container_create_input_shape (CAIRO_CONTAINER (pDock),
			///(W - w) * pDock->fAlign,
			offset,
			pDock->container.bDirectionUp ? H - h : 0,
			w,
			h);
	}
	else
	{
		pShapeBitmap = gldi_container_create_input_shape (CAIRO_CONTAINER (pDock),
			pDock->container.bDirectionUp ? H - h : 0,
			///(W - w) * pDock->fAlign,
			offset,
			h,
			w);
	}
	return pShapeBitmap;
}

void cairo_dock_update_input_shape (CairoDock *pDock)
{
	//\_______________ destroy the current input zones.
	if (pDock->pShapeBitmap != NULL)
	{
		cairo_region_destroy (pDock->pShapeBitmap);
		pDock->pShapeBitmap = NULL;
	}
	if (pDock->pHiddenShapeBitmap != NULL)
	{
		cairo_region_destroy (pDock->pHiddenShapeBitmap);
		pDock->pHiddenShapeBitmap = NULL;
	}
	if (pDock->pActiveShapeBitmap != NULL)
	{
		cairo_region_destroy (pDock->pActiveShapeBitmap);
		pDock->pActiveShapeBitmap = NULL;
	}
	
	//\_______________ define the input zones' geometry
	int W = pDock->iMaxDockWidth;
	int H = pDock->iMaxDockHeight;
	int w = pDock->iMinDockWidth;
	int h = pDock->iMinDockHeight;
	//g_print ("%s (%dx%d; %dx%d)\n", __func__, w, h, W, H);
	int w_ = 0;  // Note: in older versions of X, a fully empty input shape was not working and we had to set 1 pixel ON.
	int h_ = 0;
	
	//\_______________ check that the dock can have input zones.
	if (w == 0 || h == 0 || pDock->iRefCount > 0 || W == 0 || H == 0)
	{
		if (pDock->iActiveWidth != pDock->iMaxDockWidth || pDock->iActiveHeight != pDock->iMaxDockHeight)
			// else all the dock is active when the mouse is inside, so we can just set a NULL shape.
			pDock->pActiveShapeBitmap = _cairo_dock_create_input_shape (pDock, pDock->iActiveWidth, pDock->iActiveHeight);
		if (pDock->iInputState != CAIRO_DOCK_INPUT_ACTIVE)
		{
			//g_print ("+++ input shape active on update input shape\n");
			cairo_dock_set_input_shape_active (pDock);
			pDock->iInputState = CAIRO_DOCK_INPUT_ACTIVE;
		}
		return ;
	}
	
	//\_______________ create the input zones based on the previous geometries.
	pDock->pShapeBitmap = _cairo_dock_create_input_shape (pDock, w, h);
	
	pDock->pHiddenShapeBitmap = _cairo_dock_create_input_shape (pDock, w_, h_);
	
	if (pDock->iActiveWidth != pDock->iMaxDockWidth || pDock->iActiveHeight != pDock->iMaxDockHeight)
		// else all the dock is active when the mouse is inside, so we can just set a NULL shape.
		pDock->pActiveShapeBitmap = _cairo_dock_create_input_shape (pDock, pDock->iActiveWidth, pDock->iActiveHeight);
	
	//\_______________ if the renderer can define the input shape, let it finish the job.
	if (pDock->pRenderer->update_input_shape != NULL)
		pDock->pRenderer->update_input_shape (pDock);
}



  ///////////////////
 /// LINEAR DOCK ///
///////////////////

void cairo_dock_calculate_icons_positions_at_rest_linear (GList *pIconList, double fFlatDockWidth)
{
	//g_print ("%s (%d, +%d)\n", __func__, fFlatDockWidth);
	double x_cumulated = 0;
	GList* ic;
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
		//g_print ("%s : fXAtRest = %.2f\n", icon->cName, icon->fXAtRest);

		x_cumulated += icon->fWidth + myIconsParam.iIconGap;
	}
}

double cairo_dock_calculate_max_dock_width (CairoDock *pDock, double fFlatDockWidth, double fWidthConstraintFactor, double fExtraWidth)
{
	double fMaxDockWidth = 0.;
	//g_print ("%s (%d)\n", __func__, (int)fFlatDockWidth);
	GList *pIconList = pDock->icons;
	if (pIconList == NULL)
		return 2 * myDocksParam.iDockRadius + myDocksParam.iDockLineWidth + 2 * myDocksParam.iFrameMargin;

	// We reset extreme positions of the icons.
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fXMax = -1e4;
		icon->fXMin = 1e4;
	}

	/* We simulate the move of the cursor in all the width of the dock and we
	 * get the maximum width and the balance position for each icon.
	 */
	GList *ic2;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		cairo_dock_calculate_wave_with_position_linear (pIconList, icon->fXAtRest, pDock->fMagnitudeMax, fFlatDockWidth, 0, 0, 0.5, 0, pDock->container.bDirectionUp);
		
		for (ic2 = pIconList; ic2 != NULL; ic2 = ic2->next)
		{
			icon = ic2->data;

			if (icon->fX + icon->fWidth * icon->fScale > icon->fXMax)
				icon->fXMax = icon->fX + icon->fWidth * icon->fScale;
			if (icon->fX < icon->fXMin)
				icon->fXMin = icon->fX;
		}
	}
	cairo_dock_calculate_wave_with_position_linear (pIconList, fFlatDockWidth - 1, pDock->fMagnitudeMax, fFlatDockWidth, 0, 0, pDock->fAlign, 0, pDock->container.bDirectionUp);  // last calculation at the extreme right of the dock.
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		if (icon->fX + icon->fWidth * icon->fScale > icon->fXMax)
			icon->fXMax = icon->fX + icon->fWidth * icon->fScale;
		if (icon->fX < icon->fXMin)
			icon->fXMin = icon->fX;
	}

	fMaxDockWidth = (icon->fXMax - ((Icon *) pIconList->data)->fXMin) * fWidthConstraintFactor + fExtraWidth;
	fMaxDockWidth = ceil (fMaxDockWidth) + 1;

	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fXMin += fMaxDockWidth / 2;
		icon->fXMax += fMaxDockWidth / 2;
		//g_print ("%s : [%d;%d]\n", icon->cName, (int) icon->fXMin, (int) icon->fXMax);
		icon->fX = icon->fXAtRest;
		icon->fScale = 1;
	}

	return fMaxDockWidth;
}

Icon * cairo_dock_calculate_wave_with_position_linear (GList *pIconList, int x_abs, gdouble fMagnitude, double fFlatDockWidth, int iWidth, int iHeight, double fAlign, double fFoldingFactor, gboolean bDirectionUp)
{
	//g_print (">>>>>%s (%d/%.2f, %dx%d, %.2f, %.2f)\n", __func__, x_abs, fFlatDockWidth, iWidth, iHeight, fAlign, fFoldingFactor);
	if (pIconList == NULL)
		return NULL;
	if (x_abs < 0 && iWidth > 0)
		// to avoid too quick resize when leaving from the edges.
		///x_abs = -1;
		x_abs = 0;
	else if (x_abs > fFlatDockWidth && iWidth > 0)
		///x_abs = fFlatDockWidth+1;
		x_abs = (int) fFlatDockWidth;
	
	
	float x_cumulated = 0, fXMiddle, fDeltaExtremum;
	GList* ic, *pointed_ic;
	Icon *icon, *prev_icon;
	double fScale = 0.;
	double offset = 0.;
	pointed_ic = (x_abs < 0 ? pIconList : NULL);
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		x_cumulated = icon->fXAtRest;
		fXMiddle = icon->fXAtRest + icon->fWidth / 2;

		//\_______________ We compute its phase (pi/2 next to the cursor).
		icon->fPhase = (fXMiddle - x_abs) / myIconsParam.iSinusoidWidth * G_PI + G_PI / 2;
		if (icon->fPhase < 0)
		{
			icon->fPhase = 0;
		}
		else if (icon->fPhase > G_PI)
		{
			icon->fPhase = G_PI;
		}
		
		//\_______________ We deduct the sinusoidal amplitude next to the icon (its scale)
		icon->fScale = 1 + fMagnitude * myIconsParam.fAmplitude * sin (icon->fPhase);
		if (iWidth > 0 && icon->fInsertRemoveFactor != 0)
		{
			fScale = icon->fScale;
			///offset += (icon->fWidth * icon->fScale) * (pointed_ic == NULL ? 1 : -1);
			if (icon->fInsertRemoveFactor > 0)
				icon->fScale *= icon->fInsertRemoveFactor;
			else
				icon->fScale *= (1 + icon->fInsertRemoveFactor);
			///offset -= (icon->fWidth * icon->fScale) * (pointed_ic == NULL ? 1 : -1);
		}
		
		icon->fY = (bDirectionUp ? iHeight - myDocksParam.iDockLineWidth - myDocksParam.iFrameMargin - icon->fScale * icon->fHeight : myDocksParam.iDockLineWidth + myDocksParam.iFrameMargin);
		//g_print ("%s fY : %d; %.2f\n", icon->cName, iHeight, icon->fHeight);
		
		/* If we already have defined a pointed icon, we can move the current
		 * icon compared to the previous one
		 */
		if (pointed_ic != NULL)
		{
			if (ic == pIconList)  // can happen if we are outside from the left of the dock.
			{
				icon->fX = x_cumulated - 1. * (fFlatDockWidth - iWidth) / 2;
				//g_print ("  outside from the left : icon->fX = %.2f (%.2f)\n", icon->fX, x_cumulated);
			}
			else
			{
				prev_icon = (ic->prev != NULL ? ic->prev->data : cairo_dock_get_last_icon (pIconList));
				icon->fX = prev_icon->fX + (prev_icon->fWidth + myIconsParam.iIconGap) * prev_icon->fScale;

				if (icon->fX + icon->fWidth * icon->fScale > icon->fXMax - myIconsParam.fAmplitude * fMagnitude * (icon->fWidth + 1.5*myIconsParam.iIconGap) / 8 && iWidth != 0)
				{
					//g_print ("  we constraint %s (fXMax=%.2f , fX=%.2f\n", prev_icon->cName, prev_icon->fXMax, prev_icon->fX);
					fDeltaExtremum = icon->fX + icon->fWidth * icon->fScale - (icon->fXMax - myIconsParam.fAmplitude * fMagnitude * (icon->fWidth + 1.5*myIconsParam.iIconGap) / 16);
					if (myIconsParam.fAmplitude != 0)
						icon->fX -= fDeltaExtremum * (1 - (icon->fScale - 1) / myIconsParam.fAmplitude) * fMagnitude;
				}
			}
			icon->fX = fAlign * iWidth + (icon->fX - fAlign * iWidth) * (1. - fFoldingFactor);
			//g_print ("  on the right : icon->fX = %.2f (%.2f)\n", icon->fX, x_cumulated);
		}
		
		//\_______________ We check if we have a pointer on this icon.
		if (pointed_ic == NULL
		    && x_cumulated + icon->fWidth + .5*myIconsParam.iIconGap >= x_abs
		    && x_cumulated - .5*myIconsParam.iIconGap <= x_abs) // we found the pointed icon.
		{
			pointed_ic = ic;
			///icon->bPointed = TRUE;
			icon->bPointed = (x_abs != (int) fFlatDockWidth && x_abs != 0);
			icon->fX = x_cumulated - (fFlatDockWidth - iWidth) / 2 + (1 - icon->fScale) * (x_abs - x_cumulated + .5*myIconsParam.iIconGap);
			icon->fX = fAlign * iWidth + (icon->fX - fAlign * iWidth) * (1. - fFoldingFactor);
			//g_print ("  pointed icon: fX = %.2f (%.2f, %d)\n", icon->fX, x_cumulated, icon->bPointed);
		}
		else
			icon->bPointed = FALSE;
		
		if (iWidth > 0 && icon->fInsertRemoveFactor != 0)
		{
			if (pointed_ic != ic)  // bPointed can be false for the last icon on the right.
				offset += (icon->fWidth * (fScale - icon->fScale)) * (pointed_ic == NULL ? 1 : -1);
			else
				offset += (2*(fXMiddle - x_abs) * (fScale - icon->fScale)) * (pointed_ic == NULL ? 1 : -1);
		}
	}
	
	//\_______________ We place icons before pointed icon beside this one
	if (pointed_ic == NULL)  // We are at the right of icons.
	{
		pointed_ic = g_list_last (pIconList);
		icon = pointed_ic->data;
		icon->fX = x_cumulated - (fFlatDockWidth - iWidth) / 2 + (1 - icon->fScale) * (icon->fWidth + .5*myIconsParam.iIconGap);
		icon->fX = fAlign * iWidth + (icon->fX - fAlign * iWidth) * (1 - fFoldingFactor);
		//g_print ("  outside on the right: icon->fX = %.2f (%.2f)\n", icon->fX, x_cumulated);
	}
	
	ic = pointed_ic;
	while (ic != pIconList)
	{
		icon = ic->data;
		
		ic = ic->prev;  // since ic != pIconList, ic->prev is not NULL
		prev_icon = ic->data;
		
		prev_icon->fX = icon->fX - (prev_icon->fWidth + myIconsParam.iIconGap) * prev_icon->fScale;
		//g_print ("fX <- %.2f; fXMin : %.2f\n", prev_icon->fX, prev_icon->fXMin);
		if (prev_icon->fX < prev_icon->fXMin + myIconsParam.fAmplitude * fMagnitude * (prev_icon->fWidth + 1.5*myIconsParam.iIconGap) / 8
		    && iWidth != 0 && x_abs < iWidth && fMagnitude > 0)  /// && prev_icon->fPhase == 0
		    // We re-add 'fMagnitude > 0' otherwise we have a small jump due to constraints on the left of the pointed icon.
		{
			//g_print ("  we constraint %s (fXMin=%.2f , fX=%.2f\n", prev_icon->cName, prev_icon->fXMin, prev_icon->fX);
			fDeltaExtremum = prev_icon->fX - (prev_icon->fXMin + myIconsParam.fAmplitude * fMagnitude * (prev_icon->fWidth + 1.5*myIconsParam.iIconGap) / 16);
			if (myIconsParam.fAmplitude != 0)
				prev_icon->fX -= fDeltaExtremum * (1 - (prev_icon->fScale - 1) / myIconsParam.fAmplitude) * fMagnitude;
		}
		prev_icon->fX = fAlign * iWidth + (prev_icon->fX - fAlign * iWidth) * (1. - fFoldingFactor);
		//g_print ("  prev_icon->fX : %.2f\n", prev_icon->fX);
	}
	
	if (offset != 0)
	{
		offset /= 2;
		//g_print ("offset : %.2f (pointed:%s)\n", offset, pointed_ic?((Icon*)pointed_ic->data)->cName:"none");
		for (ic = pIconList; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			//if (ic == pIconList)
			//	cd_debug ("fX : %.2f - %.2f", icon->fX, offset);
			icon->fX -= offset;
		}
	}
	
	icon = pointed_ic->data;
	return (icon->bPointed ? icon : NULL);
}

Icon *cairo_dock_apply_wave_effect_linear (CairoDock *pDock)
{
	//\_______________ We compute the cursor's position in the container of the flat dock
	//int dx = pDock->container.iMouseX - (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2) - pDock->container.iWidth / 2;  // gap compare to the middle of the flat dock.
	//int x_abs = dx + pDock->fFlatDockWidth / 2;  // gap compare to the left of the minimal flat dock.
	//g_print ("%s (flat:%d, w:%d, x:%d)\n", __func__, (int)pDock->fFlatDockWidth, pDock->container.iWidth, pDock->container.iMouseX);
	double offset = (pDock->container.iWidth - pDock->iActiveWidth) * pDock->fAlign + (pDock->iActiveWidth - pDock->fFlatDockWidth) / 2;
	int x_abs = pDock->container.iMouseX - offset;

	//\_______________ We compute all parameters for the icons.
	double fMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);  // * pDock->fMagnitudeMax
	Icon *pPointedIcon = cairo_dock_calculate_wave_with_position_linear (pDock->icons, x_abs, fMagnitude, pDock->fFlatDockWidth, pDock->container.iWidth, pDock->container.iHeight, pDock->fAlign, pDock->fFoldingFactor, pDock->container.bDirectionUp);  // iMaxDockWidth
	return pPointedIcon;
}

double cairo_dock_get_current_dock_width_linear (CairoDock *pDock)
{
	if (pDock->icons == NULL)
		//return 2 * myDocksParam.iDockRadius + myDocksParam.iDockLineWidth + 2 * myDocksParam.iFrameMargin;
		return 1 + 2 * myDocksParam.iFrameMargin;

	Icon *pLastIcon = cairo_dock_get_last_icon (pDock->icons);
	Icon *pFirstIcon = cairo_dock_get_first_icon (pDock->icons);
	double fWidth = pLastIcon->fX - pFirstIcon->fX + pLastIcon->fWidth * pLastIcon->fScale + 2 * myDocksParam.iFrameMargin;  //  + 2 * myDocksParam.iDockRadius + myDocksParam.iDockLineWidth + 2 * myDocksParam.iFrameMargin

	return fWidth;
}

void cairo_dock_check_if_mouse_inside_linear (CairoDock *pDock)
{
	/// no global mouse position on Wayland, this is managed by the
	/// leave / enter notification
	if (gldi_container_is_wayland_backend ()) return;

	CairoDockMousePositionType iMousePositionType;
	int iWidth = pDock->container.iWidth;
	///int iHeight = (pDock->fMagnitudeMax != 0 ? pDock->container.iHeight : pDock->iMinDockHeight);
	int iHeight = pDock->iActiveHeight;
	///int iExtraHeight = (pDock->bAtBottom ? 0 : myIconsParam.iLabelSize);
	// int iExtraHeight = 0;  /// we should check if we have a sub-dock or a dialogue on top of it :-/
	int iMouseX = pDock->container.iMouseX;
	int iMouseY = (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->container.iMouseY : pDock->container.iMouseY);
	//g_print ("%s (%dx%d, %dx%d, %f)\n", __func__, iMouseX, iMouseY, iWidth, iHeight, pDock->fFoldingFactor);

	//\_______________ We check if the cursor is in the dock and we change icons size according to that.
	double offset = (iWidth - pDock->iActiveWidth) * pDock->fAlign + (pDock->iActiveWidth - pDock->fFlatDockWidth) / 2;
	int x_abs = pDock->container.iMouseX - offset;
	///int x_abs = pDock->container.iMouseX + (pDock->fFlatDockWidth - iWidth) * pDock->fAlign;  // abscisse par rapport a la gauche du dock minimal plat.
	gboolean bMouseInsideDock = (x_abs >= 0 && x_abs <= pDock->fFlatDockWidth && iMouseX > 0 && iMouseX < iWidth);
	//g_print ("bMouseInsideDock : %d (%d;%d/%.2f)\n", bMouseInsideDock, pDock->container.bInside, x_abs, pDock->fFlatDockWidth);

	if (iMouseY >= 0 && iMouseY < iHeight) { // inside in the Y axis
		if (! bMouseInsideDock)  // outside of the dock but on the edge.
			iMousePositionType = CAIRO_DOCK_MOUSE_ON_THE_EDGE;
		else
			iMousePositionType = CAIRO_DOCK_MOUSE_INSIDE;
	}
	else
		iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;

	pDock->iMousePositionType = iMousePositionType;
}


#define make_icon_avoid_mouse(icon, sens) do { \
	cairo_dock_mark_icon_as_avoiding_mouse (icon);\
	icon->fAlpha = 0.75;\
	if (myIconsParam.fAmplitude != 0)\
		icon->fDrawX += icon->fWidth * icon->fScale / 4 * sens; } while (0)
static inline gboolean _cairo_dock_check_can_drop_linear (CairoDock *pDock, CairoDockIconGroup iGroup, double fMargin)
{
	gboolean bCanDrop = FALSE;
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->bPointed)
		{
			cd_debug ("icon->fWidth: %d, %.2f", (int)icon->fWidth, icon->fScale);
			cd_debug ("x: %d / %d", pDock->container.iMouseX, (int)icon->fDrawX);
			if (pDock->container.iMouseX < icon->fDrawX + icon->fWidth * icon->fScale * fMargin)  // we are on the left.  // fDrawXAtRest
			{
				Icon *prev_icon = (ic->prev ? ic->prev->data : NULL);
				if (icon->iGroup == iGroup || (prev_icon && prev_icon->iGroup == iGroup))
				{
					make_icon_avoid_mouse (icon, 1);
					if (prev_icon)
						make_icon_avoid_mouse (prev_icon, -1);
					//g_print ("%s> <%s\n", prev_icon->cName, icon->cName);
					bCanDrop = TRUE;
				}
			}
			else if (pDock->container.iMouseX > icon->fDrawX + icon->fWidth * icon->fScale * (1 - fMargin))  // on est a droite.  // fDrawXAtRest
			{
				Icon *next_icon = (ic->next ? ic->next->data : NULL);
				if (icon->iGroup == iGroup || (next_icon && next_icon->iGroup == iGroup))
				{
					make_icon_avoid_mouse (icon, -1);
					if (next_icon)
						make_icon_avoid_mouse (next_icon, 1);
					//g_print ("%s> <%s\n", icon->cName, next_icon->cName);
					bCanDrop = TRUE;
				}
				ic = ic->next;  // we skip it.
				if (ic == NULL)
					break;
			}  // else: we are on top of it.
		}
		else
			cairo_dock_stop_marking_icon_as_avoiding_mouse (icon);
	}
	
	return bCanDrop;
}


void cairo_dock_check_can_drop_linear (CairoDock *pDock)
{
	if (! pDock->bIsDragging)  // not dragging, so no drop possible.
	{
		pDock->bCanDrop = FALSE;
	}
	else if (pDock->icons == NULL)  // dragging but no icons, so drop always possible.
	{
		pDock->bCanDrop = TRUE;
	}
	else  // dragging and some icons.
	{
		pDock->bCanDrop = _cairo_dock_check_can_drop_linear (pDock, pDock->iAvoidingMouseIconType, pDock->fAvoidingMouseMargin);
	}
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


void cairo_dock_set_subdock_position_linear (Icon *pPointedIcon, CairoDock *pDock)
{
	CairoDock *pSubDock = pPointedIcon->pSubDock;
	///int iX = pPointedIcon->fXAtRest - (pDock->fFlatDockWidth - pDock->iMaxDockWidth) / 2 + pPointedIcon->fWidth / 2 + (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2);
	//int iX = pPointedIcon->fDrawX + pPointedIcon->fWidth * pPointedIcon->fScale / 2 + (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2);
	int iX = pPointedIcon->fDrawX + pPointedIcon->fWidth * pPointedIcon->fScale / 2;
	if (pSubDock->container.bIsHorizontal == pDock->container.bIsHorizontal)
	{
		pSubDock->fAlign = 0.5;
		pSubDock->iGapX = iX + pDock->container.iWindowPositionX - gldi_dock_get_screen_offset_x (pDock) - gldi_dock_get_screen_width (pDock) / 2;  // here, sub-docks have an alignment of 0.5
		pSubDock->iGapY = pDock->iGapY + pDock->iActiveHeight;
	}
	else
	{
		pSubDock->fAlign = (pDock->container.bDirectionUp ? 1 : 0);
		pSubDock->iGapX = (pDock->iGapY + pDock->iActiveHeight) * (pDock->container.bDirectionUp ? -1 : 1);
		if (pDock->container.bDirectionUp)
			pSubDock->iGapY = gldi_dock_get_screen_width (pDock) - (iX + pDock->container.iWindowPositionX - gldi_dock_get_screen_offset_x (pDock)) - pSubDock->iMaxDockHeight / 2;  // sub-docks have an alignment of 1
		else
			pSubDock->iGapY = iX + pDock->container.iWindowPositionX - pSubDock->iMaxDockHeight / 2;  // sub-docks have an alignment of 0
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
	
	if (ic == NULL || ic->next == NULL)  // last icon or no pointed icon.
		pFirstDrawnElement = icons;
	else
		pFirstDrawnElement = ic->next;
	return pFirstDrawnElement;
}


void cairo_dock_show_subdock (Icon *pPointedIcon, CairoDock *pParentDock)
{
	cd_debug ("we show the child dock");
	CairoDock *pSubDock = pPointedIcon->pSubDock;
	g_return_if_fail (pSubDock != NULL);
	if (gldi_container_is_wayland_backend ())
	{
		// on Wayland, we cannot get the pointer position until as long
		// as it is outside; need to set these to sane defaults so that
		// leave / enter events will work properly
		pSubDock->iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;
		pSubDock->container.iMouseX = -1;
		pSubDock->container.iMouseY = -1;
	}
	
	if (gldi_container_is_visible (CAIRO_CONTAINER (pSubDock)))  // already visible.
	{
		if (pSubDock->bIsShrinkingDown)  // It's decreasing, we reverse the process.
		{
			cairo_dock_start_growing (pSubDock);
		}
		return ;
	}
	
	// place the sub-dock
	pSubDock->pRenderer->set_subdock_position (pPointedIcon, pParentDock);
	
	int iNewWidth = pSubDock->iMaxDockWidth;
	int iNewHeight = pSubDock->iMaxDockHeight;
	
	// new positioning code should work on both X11 and Wayland, but, by default, it is used
	// only on Wayland, unless it is specifically requested in the cmake configuration
	gboolean use_new_positioning = FALSE;
	#if (CAIRO_DOCK_USE_NEW_POSITIONING_ON_X11 == 1)
		use_new_positioning = TRUE;
	#else
		use_new_positioning = gldi_container_is_wayland_backend ();
	#endif
	
	if (use_new_positioning)
	{
		if (pSubDock->container.bIsHorizontal)
			gdk_window_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pSubDock)),
				iNewWidth, iNewHeight);
        else gdk_window_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pSubDock)),
				iNewHeight, iNewWidth);


		GdkRectangle rect = {0, 0, 1, 1};
		GdkGravity rect_anchor = GDK_GRAVITY_NORTH;
		GdkGravity subdock_anchor = GDK_GRAVITY_SOUTH;
		gldi_container_calculate_rect (CAIRO_CONTAINER (pParentDock), pPointedIcon,
			&rect, &rect_anchor, &subdock_anchor);
		gldi_container_move_to_rect (CAIRO_CONTAINER (pSubDock),
			&rect, rect_anchor, subdock_anchor, GDK_ANCHOR_SLIDE, 0, 0);
	}
	else
	{
		int iNewPositionX, iNewPositionY;
		cairo_dock_get_window_position_at_balance (
			pSubDock, iNewWidth, iNewHeight, &iNewPositionX, &iNewPositionY);
		if (pSubDock->container.bIsHorizontal)
		{
			gdk_window_move_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pSubDock)),
				iNewPositionX,
				iNewPositionY,
				iNewWidth,
				iNewHeight);
		}
		else
		{
			gdk_window_move_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pSubDock)),
				iNewPositionY,
				iNewPositionX,
				iNewHeight,
				iNewWidth);
			/* in this case, the sub-dock is over the label, so this one is drawn
			 * with a low transparency, so we trigger the redraw.
			 */
			gtk_widget_queue_draw (pParentDock->container.pWidget);
		}
	}
	
	// note: when using gtk-layer-shell (the parent dock is layer surface),
	// showing the window has to happen after (relative) positioning
	gtk_window_present (GTK_WINDOW (pSubDock->container.pWidget));
		
	// animate it
	if (myDocksParam.bAnimateSubDock && pSubDock->icons != NULL)
	{
		pSubDock->fFoldingFactor = .99;
		cairo_dock_start_growing (pSubDock);  // We start growing icons
		/* We re-compute icons' size because the first draw was done with
		 * parameters of an hidden dock ; or the showed animation can take
		 * more time than the hidden one.
		 */
		pSubDock->pRenderer->calculate_icons (pSubDock);
	}
	else
	{
		pSubDock->fFoldingFactor = 0;
		///gtk_widget_queue_draw (pSubDock->container.pWidget);
	}
	gldi_object_notify (pPointedIcon, NOTIFICATION_UNFOLD_SUBDOCK, pPointedIcon);
	
	gldi_dialogs_replace_all ();
}


static gboolean _cairo_dock_dock_is_child (CairoDock *pCurrentDock, CairoDock *pSubDock)
{
	GList *pIconsList;
	Icon *pIcon;
	// check all icons of this dock (recursively)
	for (pIconsList = pCurrentDock->icons; pIconsList != NULL; pIconsList = pIconsList->next)
	{
		pIcon = pIconsList->data;
		if (pIcon->pSubDock != NULL
		&& (pIcon->pSubDock == pSubDock // this subdock is inside the current dock!
			|| _cairo_dock_dock_is_child (pIcon->pSubDock, pSubDock))) // check recursively
			return TRUE;
	}
	return FALSE;
}
static void _add_one_dock_to_list (G_GNUC_UNUSED const gchar *cName, CairoDock *pDock, gpointer *data)
{
	CairoDock *pParentDock = data[0];
	CairoDock *pSubDock = data[1];
	
	// get user docks only
	Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
	if (pPointingIcon && ! GLDI_OBJECT_IS_STACK_ICON (pPointingIcon))
		// avoid sub-docks that are not from the theme (applet sub-docks, class sub-docks, etc).
		return;
	
	// ignore the parent dock.
	if (pDock == pParentDock)
		return;
	
	// ignore any child sub-dock (if it's a subdock).
	if (pSubDock != NULL
	&& (pSubDock == pDock || _cairo_dock_dock_is_child (pSubDock, pDock)))
		return;
	
	data[2] = g_list_prepend (data[2], pDock);
}
GList *cairo_dock_get_available_docks (CairoDock *pParentDock, CairoDock *pSubDock)  // avoid 'pParentDock', and 'pSubDock' and any of its children
{
	gpointer data[3] = {pParentDock, pSubDock, NULL};
	gldi_docks_foreach ((GHFunc)_add_one_dock_to_list, data);
	return data[2];
}


static gboolean _redraw_subdock_content_idle (Icon *pIcon)
{
	CairoDock *pDock = CAIRO_DOCK(cairo_dock_get_icon_container (pIcon));
	if (pDock != NULL)
	{
		if (pIcon->pSubDock != NULL)
		{
			cairo_dock_draw_subdock_content_on_icon (pIcon, pDock);
		}
		else
		{
			/* the icon could lose its sub-dock in the meantime
			(e.g. a class having 2 icons and we remove one of these icons)
			 */
			cairo_dock_reload_icon_image (pIcon, CAIRO_CONTAINER (pDock));
		}
		cairo_dock_redraw_icon (pIcon);
		if (pDock->iRefCount != 0 && ! pIcon->bDamaged)  // now that the icon image is correct, redraw the pointing icon if needed
			cairo_dock_trigger_redraw_subdock_content (pDock);
	}
	pIcon->iSidRedrawSubdockContent = 0;
	return FALSE;
}
void cairo_dock_trigger_redraw_subdock_content (CairoDock *pDock)
{
	Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
	//g_print ("%s (%s, %d)\n", __func__, pPointingIcon?pPointingIcon->cName:NULL, pPointingIcon?pPointingIcon->iSubdockViewType:0);
	if (pPointingIcon != NULL && (pPointingIcon->iSubdockViewType != 0 || (pPointingIcon->cClass != NULL && ! myIndicatorsParam.bUseClassIndic && (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pPointingIcon) || GLDI_OBJECT_IS_LAUNCHER_ICON (pPointingIcon)))))
	{
		/* if we already have an expected re-draw, we go to the end in order to
		 * not do it before the redraw of icon linked to this trigger
		 */
		if (pPointingIcon->iSidRedrawSubdockContent != 0)
			g_source_remove (pPointingIcon->iSidRedrawSubdockContent);
		pPointingIcon->iSidRedrawSubdockContent = g_idle_add ((GSourceFunc) _redraw_subdock_content_idle, pPointingIcon);
	}
}

void cairo_dock_trigger_redraw_subdock_content_on_icon (Icon *icon)
{
	if (icon->iSidRedrawSubdockContent != 0)
		g_source_remove (icon->iSidRedrawSubdockContent);
	icon->iSidRedrawSubdockContent = g_idle_add ((GSourceFunc) _redraw_subdock_content_idle, icon);
}

void cairo_dock_redraw_subdock_content (CairoDock *pDock)
{
	CairoDock *pParentDock = NULL;
	Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
	if (pPointingIcon != NULL && pPointingIcon->iSubdockViewType != 0 && pPointingIcon->iSidRedrawSubdockContent == 0 && pParentDock != NULL)
	{
		cairo_dock_draw_subdock_content_on_icon (pPointingIcon, pParentDock);
		cairo_dock_redraw_icon (pPointingIcon);
	}
}

static gboolean _update_WM_icons (CairoDock *pDock)
{
	cairo_dock_set_icons_geometry_for_window_manager (pDock);
	pDock->iSidUpdateWMIcons = 0;
	return FALSE;
}
void cairo_dock_trigger_set_WM_icons_geometry (CairoDock *pDock)
{
	if (pDock->iSidUpdateWMIcons == 0)
	{
		pDock->iSidUpdateWMIcons = g_idle_add ((GSourceFunc) _update_WM_icons, pDock);
	}
}

void cairo_dock_resize_icon_in_dock (Icon *pIcon, CairoDock *pDock)  // resize the icon according to the requested size previously set on the icon.
{
	cairo_dock_set_icon_size_in_dock (pDock, pIcon);
	
	cairo_dock_load_icon_image (pIcon, CAIRO_CONTAINER (pDock));  // handles the applet's context
	
	if (cairo_dock_get_icon_data_renderer (pIcon) != NULL)
		cairo_dock_reload_data_renderer_on_icon (pIcon, CAIRO_CONTAINER (pDock));
	
	cairo_dock_trigger_update_dock_size (pDock);
	gtk_widget_queue_draw (pDock->container.pWidget);
}


  ///////////////////////
 /// DOCK BACKGROUND ///
///////////////////////

static cairo_surface_t *_cairo_dock_make_stripes_background (int iWidth, int iHeight, GldiColor *fStripesColorBright, GldiColor *fStripesColorDark, int iNbStripes, double fStripesWidth, double fStripesAngle)
{
	cairo_pattern_t *pStripesPattern;
	if (fabs (fStripesAngle) != 90)
		pStripesPattern = cairo_pattern_create_linear (0.0f,
			0.0f,
			iWidth,
			iWidth * tan (fStripesAngle * G_PI/180.));
	else
		pStripesPattern = cairo_pattern_create_linear (0.0f,
			0.0f,
			0.,
			(fStripesAngle == 90) ? iHeight : - iHeight);
	g_return_val_if_fail (cairo_pattern_status (pStripesPattern) == CAIRO_STATUS_SUCCESS, NULL);

	cairo_pattern_set_extend (pStripesPattern, CAIRO_EXTEND_REPEAT);

	if (iNbStripes > 0)
	{
		gdouble fStep;
		int i;
		for (i = 0; i < iNbStripes+1; i ++)
		{
			fStep = (double)i / iNbStripes;
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep - fStripesWidth / 2.,
				fStripesColorBright->rgba.red,
				fStripesColorBright->rgba.green,
				fStripesColorBright->rgba.blue,
				fStripesColorBright->rgba.alpha);
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep,
				fStripesColorDark->rgba.red,
				fStripesColorDark->rgba.green,
				fStripesColorDark->rgba.blue,
				fStripesColorDark->rgba.alpha);
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep + fStripesWidth / 2.,
				fStripesColorBright->rgba.red,
				fStripesColorBright->rgba.green,
				fStripesColorBright->rgba.blue,
				fStripesColorBright->rgba.alpha);
		}
	}
	else
	{
		cairo_pattern_add_color_stop_rgba (pStripesPattern,
			0.,
			fStripesColorDark->rgba.red,
			fStripesColorDark->rgba.green,
			fStripesColorDark->rgba.blue,
			fStripesColorDark->rgba.alpha);
		cairo_pattern_add_color_stop_rgba (pStripesPattern,
			1.,
			fStripesColorBright->rgba.red,
			fStripesColorBright->rgba.green,
			fStripesColorBright->rgba.blue,
			fStripesColorBright->rgba.alpha);
	}

	cairo_surface_t *pNewSurface = cairo_dock_create_blank_surface (
			iWidth,
			iHeight);
	cairo_t *pImageContext = cairo_create (pNewSurface);
	cairo_set_source (pImageContext, pStripesPattern);
	cairo_paint (pImageContext);

	cairo_pattern_destroy (pStripesPattern);
	cairo_destroy (pImageContext);
	
	return pNewSurface;
}
static void _cairo_dock_load_default_background (CairoDockImageBuffer *pImage, int iWidth, int iHeight)
{
	cd_debug ("%s (%s, %d, %dx%d)", __func__, myDocksParam.cBackgroundImageFile, myDocksParam.bBackgroundImageRepeat, iWidth, iHeight);
	if (myDocksParam.bUseDefaultColors)
	{
		cairo_surface_t *pBgSurface = cairo_dock_create_blank_surface (
			iWidth,
			iHeight);
		cairo_t *pImageContext = cairo_create (pBgSurface);
		
		/* Add a small vertical gradation to the bg color, it looks better than
		 * a completely monochrome background. At the top is the original color
		 * which connects nicely with other items (labels, menus, dialogs)
		 */
		GldiColor bg_color, bg_color2;
		gldi_style_color_get (GLDI_COLOR_BG, &bg_color);
		gldi_style_color_shade (&bg_color, GLDI_COLOR_SHADE_LIGHT, &bg_color2);
		
		cairo_pattern_t *pattern = cairo_pattern_create_linear (0, 0, 0, iHeight);
		cairo_pattern_set_extend (pattern, CAIRO_EXTEND_NONE);
		cairo_pattern_add_color_stop_rgba (pattern,
			1.,
			bg_color.rgba.red, bg_color.rgba.green, bg_color.rgba.blue, bg_color.rgba.alpha);  // this will be at the bottom of the dock
		cairo_pattern_add_color_stop_rgba (pattern,
			0.5,
			bg_color2.rgba.red, bg_color2.rgba.green, bg_color2.rgba.blue, bg_color2.rgba.alpha);  // middle
		cairo_pattern_add_color_stop_rgba (pattern,
			0.,
			bg_color.rgba.red, bg_color.rgba.green, bg_color.rgba.blue, bg_color.rgba.alpha);  // and this is at the top
		cairo_set_source (pImageContext, pattern);
		///gldi_style_colors_set_bg_color (pImageContext);
		
		cairo_pattern_destroy (pattern);
		cairo_paint (pImageContext);
		cairo_destroy (pImageContext);
		cairo_dock_load_image_buffer_from_surface (pImage,
			pBgSurface,
			iWidth,
			iHeight);
	}
	else if (myDocksParam.cBackgroundImageFile != NULL)
	{
		if (myDocksParam.bBackgroundImageRepeat)
		{
			cairo_surface_t *pBgSurface = cairo_dock_create_surface_from_pattern (myDocksParam.cBackgroundImageFile,
				iWidth,
				iHeight,
				myDocksParam.fBackgroundImageAlpha);
			cairo_dock_load_image_buffer_from_surface (pImage,
				pBgSurface,
				iWidth,
				iHeight);
		}
		else
		{
			cairo_dock_load_image_buffer_full (pImage,
				myDocksParam.cBackgroundImageFile,
				iWidth,
				iHeight,
				CAIRO_DOCK_FILL_SPACE,
				myDocksParam.fBackgroundImageAlpha);
		}
	}
	if (pImage->pSurface == NULL)
	{
		cairo_surface_t *pBgSurface = _cairo_dock_make_stripes_background (
			iWidth,
			iHeight,
			&myDocksParam.fStripesColorBright,
			&myDocksParam.fStripesColorDark,
			myDocksParam.iNbStripes,
			myDocksParam.fStripesWidth,
			myDocksParam.fStripesAngle);
		cairo_dock_load_image_buffer_from_surface (pImage,
			pBgSurface,
			iWidth,
			iHeight);
	}
}

void cairo_dock_load_dock_background (CairoDock *pDock)
{
	cairo_dock_unload_image_buffer (&pDock->backgroundBuffer);
	
	int iWidth = pDock->iDecorationsWidth;
	int iHeight = pDock->iDecorationsHeight;
	
	if (pDock->bGlobalBg || pDock->iRefCount > 0)
	{
		_cairo_dock_load_default_background (&pDock->backgroundBuffer, iWidth, iHeight);
	}
	else if (pDock->cBgImagePath != NULL)
	{
		cairo_dock_load_image_buffer (&pDock->backgroundBuffer, pDock->cBgImagePath, iWidth, iHeight, CAIRO_DOCK_FILL_SPACE);
	}
	if (pDock->backgroundBuffer.pSurface == NULL)
	{
		cairo_surface_t *pSurface = _cairo_dock_make_stripes_background (iWidth, iHeight, &pDock->fBgColorBright, &pDock->fBgColorDark, 0, 0., 90);
		cairo_dock_load_image_buffer_from_surface (&pDock->backgroundBuffer, pSurface, iWidth, iHeight);
	}
	gtk_widget_queue_draw (pDock->container.pWidget);
}

static gboolean _load_background_idle (CairoDock *pDock)
{
	cairo_dock_load_dock_background (pDock);
	
	pDock->iSidLoadBg = 0;
	return FALSE;
}
void cairo_dock_trigger_load_dock_background (CairoDock *pDock)
{
	if (pDock->iDecorationsWidth == pDock->backgroundBuffer.iWidth && pDock->iDecorationsHeight == pDock->backgroundBuffer.iHeight)  // mise a jour inutile.
		return;
	if (pDock->iSidLoadBg == 0)
		pDock->iSidLoadBg = g_idle_add ((GSourceFunc)_load_background_idle, pDock);
}


void cairo_dock_make_preview (CairoDock *pDock, const gchar *cPreviewPath)
{
	if (pDock && pDock->pRenderer)
	{
		// place the mouse in the middle of the dock and update the icons position
		pDock->container.iMouseX = pDock->container.iWidth/2;
		pDock->container.iMouseY = 1;
		cairo_dock_calculate_dock_icons (pDock);
		
		// dump the context into a cairo-surface
		cairo_surface_t *pSurface;
		int w = (pDock->container.bIsHorizontal ? pDock->container.iWidth : pDock->container.iHeight);  // iActiveWidth
		int h = (pDock->container.bIsHorizontal ? pDock->container.iHeight : pDock->container.iWidth);  // iActiveHeight
		GLubyte *glbuffer = NULL;
		if (g_bUseOpenGL)
		{
			if (gldi_gl_container_begin_draw (CAIRO_CONTAINER (pDock)))
			{
				pDock->pRenderer->render_opengl (pDock);
			}
			int s = 4;  // 4 channels of 1 byte each (rgba).
			GLubyte *buffer = (GLubyte *) g_malloc (w * h * s);
			glbuffer = (GLubyte *) g_malloc (w * h * s);

			glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, (GLvoid *)buffer);

			// make upside down
			int x, y;
			for (y=0; y<h; y++) {
				for (x=0; x<w*s; x++) {
					glbuffer[y * s * w + x] = buffer[(h - y - 1) * w * s + x];
				}
			}
			
			int iStride = w * s;  // number of bytes between the beginning of the 2 lines
			pSurface = cairo_image_surface_create_for_data ((guchar *)glbuffer,
				CAIRO_FORMAT_ARGB32,
				w,
				h,
				iStride);
			
			g_free (buffer);
		}
		else
		{
			pSurface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
				w,
				h);
			cairo_t *pCairoContext = cairo_create (pSurface);
			pDock->pRenderer->render (pCairoContext, pDock);
			cairo_destroy (pCairoContext);
		}
		// dump the surface into a PNG
		if (!pDock->container.bIsHorizontal)
		{
			cairo_t *pCairoContext = cairo_create (pSurface);
			cairo_translate (pCairoContext, w/2, h/2);
			cairo_rotate (pCairoContext, -G_PI/2);
			cairo_translate (pCairoContext, -h/2, -w/2);
			cairo_destroy (pCairoContext);
		}
		
		cairo_surface_write_to_png (pSurface, cPreviewPath);
		cairo_surface_destroy (pSurface);
		g_free (glbuffer);
	}
}

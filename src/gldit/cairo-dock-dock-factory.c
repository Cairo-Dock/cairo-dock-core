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
#include <GL/gl.h>
#include <GL/glu.h>

#include "cairo-dock-separator-manager.h"  // gldi_auto_separator_icon_new
#include "cairo-dock-log.h"
#include "cairo-dock-draw-opengl.h"  // for the redirected texture
#include "cairo-dock-data-renderer.h"  // cairo_dock_reload_data_renderer_on_icon/cairo_dock_refresh_data_renderer
#include "cairo-dock-windows-manager.h"  // gldi_windows_get_active
#include "cairo-dock-indicator-manager.h"  // myIndicators.bUseClassIndic
#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-module-manager.h"  // CAIRO_DOCK_MODULE_CAN_DESKLET
#include "cairo-dock-module-instance-manager.h"  // pModuleInstance->
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.bHideVisibleApplis
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-config.h"  // cairo_dock_is_loading
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-log.h"
#include "cairo-dock-menu.h"  // gldi_menu_popup
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-visibility.h"  // gldi_dock_search_overlapping_window
#include "cairo-dock-flying-container.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-class-manager.h"  // cairo_dock_check_class_subdock_is_empty
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"  // gldi_windows_get_active
#include "cairo-dock-dock-factory.h"

// dependencies
extern CairoDockHidingEffect *g_pHidingBackend;
extern CairoDockHidingEffect *g_pKeepingBelowBackend;
extern gboolean g_bUseOpenGL;
extern CairoDockGLConfig g_openglConfig;

// private
static Icon *s_pIconClicked = NULL;  // pour savoir quand on deplace une icone a la souris. Dangereux si l'icone se fait effacer en cours ...
static int s_iClickX, s_iClickY;  // coordonnees du clic dans le dock, pour pouvoir initialiser le deplacement apres un seuil.
static int s_iSidShowSubDockDemand = 0;
static int s_iSidActionOnDragHover = 0;
static CairoDock *s_pDockShowingSubDock = NULL;  // on n'accede pas a son contenu, seulement l'adresse.
static CairoDock *s_pSubDockShowing = NULL;  // on n'accede pas a son contenu, seulement l'adresse.
static CairoFlyingContainer *s_pFlyingContainer = NULL;
static int s_iFirstClickX=0, s_iFirstClickY=0;  // for double-click.
static gboolean s_bFrozenDock = FALSE;
static gboolean s_bIconDragged = FALSE;
static gboolean _check_mouse_outside (CairoDock *pDock);
static void cairo_dock_stop_icon_glide (CairoDock *pDock);
#define CD_CLICK_ZONE 5

  /////////////////
 /// CALLBACKS ///
/////////////////

static gboolean _mouse_is_really_outside (CairoDock *pDock)
{
	if (gldi_container_is_wayland_backend ())
		return pDock->iMousePositionType == CAIRO_DOCK_MOUSE_OUTSIDE;
	int x1, x2, y1, y2;
	if (pDock->iInputState == CAIRO_DOCK_INPUT_ACTIVE)
	{
		x1 = (pDock->container.iWidth - pDock->iActiveWidth) * pDock->fAlign;
		x2 = x1 + pDock->iActiveWidth;
		if (pDock->container.bDirectionUp)
		{
			y1 = pDock->container.iHeight - pDock->iActiveHeight + 1;
			y2 = pDock->container.iHeight;
		}
		else
		{
			y1 = 0;
			y2 = pDock->iActiveHeight - 1;
		}
	}
	else if (pDock->iInputState == CAIRO_DOCK_INPUT_AT_REST)
	{
		x1 = (pDock->container.iWidth - pDock->iMinDockWidth) * pDock->fAlign;
		x2 = x1 + pDock->iMinDockWidth;
		if (pDock->container.bDirectionUp)
		{
			y1 = pDock->container.iHeight - pDock->iMinDockHeight + 1;
			y2 = pDock->container.iHeight;
		}
		else
		{
			y1 = 0;
			y2 = pDock->iMinDockHeight - 1;
		}		
	}
	else  // hidden
		return TRUE;
	if (pDock->container.iMouseX <= x1
	|| pDock->container.iMouseX >= x2)
		return TRUE;
	if (pDock->container.iMouseY < y1
	|| pDock->container.iMouseY > y2)  // Note: Compiz has a bug: when using the "cube rotation" plug-in, it will reserve 2 pixels for itself on the left and right edges of the screen. So the mouse is not inside the dock when it's at x=0 or x=Ws-1 (no 'enter' event is sent; it's as if the x=0 or x=Ws-1 vertical line of pixels is out of the screen).
		return TRUE;

	return FALSE;
}

void cairo_dock_freeze_docks (gboolean bFreeze)
{
	s_bFrozenDock = bFreeze;  /// instead, try to connect to the motion-event and intercept it ...
}

static gboolean _on_expose (G_GNUC_UNUSED GtkWidget *pWidget, cairo_t *pCairoContext, CairoDock *pDock)
{
	if (g_bUseOpenGL && pDock->pRenderer->render_opengl != NULL)  // OpenGL rendering
	{
		GdkRectangle area;
		double x1, x2, y1, y2;
		cairo_clip_extents (pCairoContext, &x1, &y1, &x2, &y2);
		area.x = x1;
		area.y = y1;
		area.width = x2 - x1;
		area.height = y2 - y1;
		
		if (! gldi_gl_container_begin_draw_full (CAIRO_CONTAINER (pDock), area.x + area.y != 0 ? &area : NULL, TRUE))
			return FALSE;
		
		if (cairo_dock_is_loading ())
		{
			// don't draw anything, just let it transparent
		}
		else if (cairo_dock_is_hidden (pDock) && (g_pHidingBackend == NULL || !g_pHidingBackend->bCanDisplayHiddenDock))
		{
			cairo_dock_render_hidden_dock_opengl (pDock);
		}
		else
		{
			gldi_object_notify (pDock, NOTIFICATION_RENDER, pDock, NULL);
		}
		
		gldi_gl_container_end_draw (CAIRO_CONTAINER (pDock));
	}
	else if (! g_bUseOpenGL && pDock->pRenderer->render != NULL)  // cairo rendering
	{
		cairo_dock_init_drawing_context_on_container (CAIRO_CONTAINER (pDock), pCairoContext);
		
		if (cairo_dock_is_loading ())
		{
			// don't draw anything, just let it transparent
		}
		else if (cairo_dock_is_hidden (pDock) && (g_pHidingBackend == NULL || !g_pHidingBackend->bCanDisplayHiddenDock))
		{
			cairo_dock_render_hidden_dock (pCairoContext, pDock);
		}
		else
		{
			gldi_object_notify (pDock, NOTIFICATION_RENDER, pDock, pCairoContext);
		}
	}
	return FALSE;
}


static gboolean _emit_leave_signal_delayed (CairoDock *pDock)
{
	//g_print ("%s(%d)\n", __func__, pDock->iRefCount);
	cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
	pDock->iSidLeaveDemand = 0;
	return FALSE;
}
static gboolean _cairo_dock_show_sub_dock_delayed (CairoDock *pDock)
{
	s_iSidShowSubDockDemand = 0;
	s_pDockShowingSubDock = NULL;
	s_pSubDockShowing = NULL;
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	//g_print ("%s (%x, %x)", __func__, icon, icon ? icon->pSubDock:0);
	if (icon != NULL && icon->pSubDock != NULL)
		cairo_dock_show_subdock (icon, pDock);

	return FALSE;
}
static void _search_icon (Icon *icon, gpointer *data)
{
	if (icon == data[0])
		data[1] = icon;
}
static gboolean _cairo_dock_action_on_drag_hover (Icon *pIcon)
{
	gpointer data[2] = {pIcon, NULL};
	gldi_icons_foreach_in_docks ((GldiIconFunc)_search_icon, data);  // on verifie que l'icone ne s'est pas faite effacee entre-temps.
	pIcon = data[1];
	if (pIcon && pIcon->iface.action_on_drag_hover)
		pIcon->iface.action_on_drag_hover (pIcon);
	s_iSidActionOnDragHover = 0;
	return FALSE;
}
static void _on_change_icon (Icon *pLastPointedIcon, Icon *pPointedIcon, CairoDock *pDock)
{
	//g_print ("%s (%s -> %s)\n", __func__, pLastPointedIcon?pLastPointedIcon->cName:"none", pPointedIcon?pPointedIcon->cName:"none");
	//cd_debug ("on change d'icone dans %x (-> %s)", pDock, (pPointedIcon != NULL ? pPointedIcon->cName : "rien"));
	if (s_iSidShowSubDockDemand != 0 && pDock == s_pDockShowingSubDock)
	{
		//cd_debug ("on annule la demande de montrage de sous-dock");
		g_source_remove (s_iSidShowSubDockDemand);
		s_iSidShowSubDockDemand = 0;
		s_pDockShowingSubDock = NULL;
		s_pSubDockShowing = NULL;
	}
	
	// take action when dragging something onto an icon
	if (s_iSidActionOnDragHover != 0)
	{
		//cd_debug ("on annule la demande de montrage d'appli");
		g_source_remove (s_iSidActionOnDragHover);
		s_iSidActionOnDragHover = 0;
	}
	
	if (pDock->bIsDragging && pPointedIcon && pPointedIcon->iface.action_on_drag_hover)
	{
		s_iSidActionOnDragHover = g_timeout_add (600, (GSourceFunc) _cairo_dock_action_on_drag_hover, pPointedIcon);
	}
	
	// replace dialogs
	gldi_dialogs_refresh_all ();
	
	// hide the sub-dock of the previous pointed icon
	if (pLastPointedIcon != NULL && pLastPointedIcon->pSubDock != NULL)  // on a quitte une icone ayant un sous-dock.
	{
		CairoDock *pSubDock = pLastPointedIcon->pSubDock;
		if (gldi_container_is_visible (CAIRO_CONTAINER (pSubDock)))  // le sous-dock est visible, on retarde son cachage.
		{
			//g_print ("on cache %s en changeant d'icone\n", pLastPointedIcon->cName);
			if (pSubDock->iSidLeaveDemand == 0)
			{
				//g_print (" on retarde le cachage du dock de %dms\n", MAX (myDocksParam.iLeaveSubDockDelay, 300));
				pSubDock->iSidLeaveDemand = g_timeout_add (MAX (myDocksParam.iLeaveSubDockDelay, 300), (GSourceFunc) _emit_leave_signal_delayed, (gpointer) pSubDock);  // on force le retard meme si iLeaveSubDockDelay est a 0, car lorsqu'on entre dans un sous-dock, il arrive frequemment qu'on glisse hors de l'icone qui pointe dessus, et c'est tres desagreable d'avoir le dock qui se ferme avant d'avoir pu entre dedans.
			}
		}
	}
	
	// show the sub-dock of the current pointed icon
	if (pPointedIcon != NULL && pPointedIcon->pSubDock != NULL && (! myDocksParam.bShowSubDockOnClick || CAIRO_DOCK_IS_APPLI (pPointedIcon) || pDock->bIsDragging))  // on entre sur une icone ayant un sous-dock.
	{
		// if we were leaving the sub-dock, cancel that.
		if (pPointedIcon->pSubDock->iSidLeaveDemand != 0)
		{
			g_source_remove (pPointedIcon->pSubDock->iSidLeaveDemand);
			pPointedIcon->pSubDock->iSidLeaveDemand = 0;
		}
		// and show the sub-dock, possibly with a delay.
		if (myDocksParam.iShowSubDockDelay > 0)
		{
			if (s_iSidShowSubDockDemand != 0)
				g_source_remove (s_iSidShowSubDockDemand);
			s_iSidShowSubDockDemand = g_timeout_add (myDocksParam.iShowSubDockDelay, (GSourceFunc) _cairo_dock_show_sub_dock_delayed, pDock);  // we can't be showing more than 1 sub-dock, so this timeout can be global to all docks.
			s_pDockShowingSubDock = pDock;
			s_pSubDockShowing = pPointedIcon->pSubDock;
		}
		else
			cairo_dock_show_subdock (pPointedIcon, pDock);
	}
	
	// notify everybody
	if (pPointedIcon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pPointedIcon))
	{
		gboolean bStartAnimation = FALSE;
		gldi_object_notify (pDock, NOTIFICATION_ENTER_ICON, pPointedIcon, pDock, &bStartAnimation);
		
		if (bStartAnimation)
		{
			cairo_dock_mark_icon_as_hovered_by_mouse (pPointedIcon);  // mark the animation as 'hover' if it's not already in another state (clicked, etc).
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
		}
	}
}


static void cairo_dock_stop_icon_glide (CairoDock *pDock)
{
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fGlideOffset = 0;
		icon->iGlideDirection = 0;
	}
}
static void _cairo_dock_make_icon_glide (Icon *pPointedIcon, Icon *pMovingicon, CairoDock *pDock)
{
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon == pMovingicon)
			continue;
		//if (pDock->container.iMouseX > s_pMovingicon->fDrawXAtRest + s_pMovingicon->fWidth * s_pMovingicon->fScale /2)  // on a deplace l'icone a droite.  // fDrawXAtRest
		if (pMovingicon->fXAtRest < pPointedIcon->fXAtRest)  // on a deplace l'icone a droite.
		{
			//g_print ("%s : %.2f / %.2f ; %.2f / %d (%.2f)\n", icon->cName, icon->fXAtRest, pMovingicon->fXAtRest, icon->fDrawX, pDock->container.iMouseX, icon->fGlideOffset);
			if (icon->fXAtRest > pMovingicon->fXAtRest && icon->fDrawX < pDock->container.iMouseX + 5 && icon->fGlideOffset == 0)  // icone entre l'icone deplacee et le curseur.
			{
				//g_print ("  %s glisse vers la gauche\n", icon->cName);
				icon->iGlideDirection = -1;
			}
			else if (icon->fXAtRest > pMovingicon->fXAtRest && icon->fDrawX > pDock->container.iMouseX && icon->fGlideOffset != 0)
			{
				//g_print ("  %s glisse vers la droite\n", icon->cName);
				icon->iGlideDirection = 1;
			}
			else if (icon->fXAtRest < pMovingicon->fXAtRest && icon->fGlideOffset > 0)
			{
				//g_print ("  %s glisse en sens inverse vers la gauche\n", icon->cName);
				icon->iGlideDirection = -1;
			}
		}
		else
		{
			//g_print ("deplacement de %s vers la gauche (%.2f / %d)\n", icon->cName, icon->fDrawX + icon->fWidth * fMaxScale + myIconsParam.iIconGap, pDock->container.iMouseX);
			if (icon->fXAtRest < pMovingicon->fXAtRest && icon->fDrawX + icon->image.iWidth + myIconsParam.iIconGap >= pDock->container.iMouseX && icon->fGlideOffset == 0)  // icone entre l'icone deplacee et le curseur.
			{
				//g_print ("  %s glisse vers la droite\n", icon->cName);
				icon->iGlideDirection = 1;
			}
			else if (icon->fXAtRest < pMovingicon->fXAtRest && icon->fDrawX + icon->image.iWidth + myIconsParam.iIconGap <= pDock->container.iMouseX && icon->fGlideOffset != 0)
			{
				//g_print ("  %s glisse vers la gauche\n", icon->cName);
				icon->iGlideDirection = -1;
			}
			else if (icon->fXAtRest > pMovingicon->fXAtRest && icon->fGlideOffset < 0)
			{
				//g_print ("  %s glisse en sens inverse vers la droite\n", icon->cName);
				icon->iGlideDirection = 1;
			}
		}
	}
}
static gboolean _on_motion_notify (GtkWidget* pWidget,
	GdkEventMotion* pMotion,
	CairoDock *pDock)
{
	static double fLastTime = 0;
	if (s_bFrozenDock && pMotion != NULL && pMotion->time != 0)
		return FALSE;
	Icon *pPointedIcon=NULL, *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	//g_print ("%s (%.2f;%.2f, %d)\n", __func__, pMotion->x, pMotion->y, pDock->iInputState);
	
	if (pMotion != NULL)
	{
		//g_print ("%s (%d,%d) (%d, %.2fms, bAtBottom:%d; bIsShrinkingDown:%d)\n", __func__, (int) pMotion->x, (int) pMotion->y, pMotion->is_hint, pMotion->time - fLastTime, pDock->bAtBottom, pDock->bIsShrinkingDown);
		//\_______________ On deplace le dock si ALT est enfoncee.
		if ((pMotion->state & GDK_MOD1_MASK) && (pMotion->state & GDK_BUTTON1_MASK))
		{
			if (pDock->container.bIsHorizontal)
			{
				pDock->container.iWindowPositionX = pMotion->x_root - pDock->container.iMouseX;
				pDock->container.iWindowPositionY = pMotion->y_root - pDock->container.iMouseY;
				gtk_window_move (GTK_WINDOW (pWidget),
					pDock->container.iWindowPositionX,
					pDock->container.iWindowPositionY);
			}
			else
			{
				pDock->container.iWindowPositionX = pMotion->y_root - pDock->container.iMouseX;
				pDock->container.iWindowPositionY = pMotion->x_root - pDock->container.iMouseY;
				gtk_window_move (GTK_WINDOW (pWidget),
					pDock->container.iWindowPositionY,
					pDock->container.iWindowPositionX);
			}
			gdk_device_get_state (pMotion->device, pMotion->window, NULL, NULL);
			return FALSE;
		}
		
		//\_______________ On recupere la position de la souris.
		if (pDock->container.bIsHorizontal)
		{
			pDock->container.iMouseX = (int) pMotion->x;
			pDock->container.iMouseY = (int) pMotion->y;
		}
		else
		{
			pDock->container.iMouseX = (int) pMotion->y;
			pDock->container.iMouseY = (int) pMotion->x;
		}
		
		//\_______________ On tire l'icone volante.
		if (s_pFlyingContainer != NULL && ! pDock->container.bInside)
		{
			gldi_flying_container_drag (s_pFlyingContainer, pDock);
		}
		
		//\_______________ On elague le flux des MotionNotify, sinon X en envoie autant que le permet le CPU !
		if (pMotion->time != 0 && pMotion->time - fLastTime < myBackendsParam.fRefreshInterval && s_pIconClicked == NULL)
		{
			gdk_device_get_state (pMotion->device, pMotion->window, NULL, NULL);
			return FALSE;
		}
		
		//\_______________ On recalcule toutes les icones et on redessine.
		pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
		//g_print ("pPointedIcon: %s\n", pPointedIcon?pPointedIcon->cName:"none");
		gtk_widget_queue_draw (pWidget);
		fLastTime = pMotion->time;
		
		//\_______________ On tire l'icone cliquee.
		if (s_pIconClicked != NULL && s_pIconClicked->iAnimationState != CAIRO_DOCK_STATE_REMOVE_INSERT && ! myDocksParam.bLockIcons && ! myDocksParam.bLockAll && (fabs (pMotion->x - s_iClickX) > CD_CLICK_ZONE || fabs (pMotion->y - s_iClickY) > CD_CLICK_ZONE) && ! pDock->bPreventDraggingIcons)
		{
			s_bIconDragged = TRUE;
			cairo_dock_mark_icon_as_following_mouse (s_pIconClicked);
			//pDock->fAvoidingMouseMargin = .5;
			pDock->iAvoidingMouseIconType = s_pIconClicked->iGroup;  // on pourrait le faire lors du clic aussi.
			s_pIconClicked->fScale = cairo_dock_get_icon_max_scale (s_pIconClicked);
			s_pIconClicked->fDrawX = pDock->container.iMouseX  - s_pIconClicked->fWidth * s_pIconClicked->fScale / 2;
			s_pIconClicked->fDrawY = pDock->container.iMouseY - s_pIconClicked->fHeight * s_pIconClicked->fScale / 2 ;
			s_pIconClicked->fAlpha = 0.75;
		}

		//gdk_event_request_motions (pMotion);  // ce sera pour GDK 2.12.
		gdk_device_get_state (pMotion->device, pMotion->window, NULL, NULL);  // pour recevoir d'autres MotionNotify.
	}
	else  // cas d'un drag and drop.
	{
		//g_print ("motion on drag\n");
		//\_______________ On recupere la position de la souris.
		gldi_container_update_mouse_position (CAIRO_CONTAINER (pDock));
		
		//\_______________ On recalcule toutes les icones et on redessine.
		pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
		gtk_widget_queue_draw (pWidget);
		
		pDock->fAvoidingMouseMargin = .25;  // on peut dropper entre 2 icones ...
		pDock->iAvoidingMouseIconType = CAIRO_DOCK_LAUNCHER;  // ... seulement entre 2 icones du groupe "lanceurs".
	}
	
	//\_______________ On gere le changement d'icone.
	gboolean bStartAnimation = FALSE;
	if (pPointedIcon != pLastPointedIcon)
	{
		_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);
		
		if (pPointedIcon != NULL && s_pIconClicked != NULL && s_pIconClicked->iGroup == pPointedIcon->iGroup && ! myDocksParam.bLockIcons && ! myDocksParam.bLockAll && ! pDock->bPreventDraggingIcons)
		{
			_cairo_dock_make_icon_glide (pPointedIcon, s_pIconClicked, pDock);
			bStartAnimation = TRUE;
		}
	}
	
	//\_______________ On notifie tout le monde.
	gldi_object_notify (pDock, NOTIFICATION_MOUSE_MOVED, pDock, &bStartAnimation);
	if (bStartAnimation)
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	
	return FALSE;
}

static gboolean _hide_child_docks (CairoDock *pDock)
{
	GList* ic;
	Icon *icon;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pSubDock == NULL)
			continue;
		if (gldi_container_is_visible (CAIRO_CONTAINER (icon->pSubDock)))
		{
			if (icon->pSubDock->container.bInside)
			{
				//cd_debug ("on est dans le sous-dock, donc on ne le cache pas");
				return FALSE;
			}
			else if (icon->pSubDock->iSidLeaveDemand == 0)  // si on sort du dock sans passer par le sous-dock, par exemple en sortant par le bas.
			{
				//cd_debug ("on cache %s par filiation", icon->cName);
				icon->pSubDock->fFoldingFactor = (myDocksParam.bAnimateSubDock ? 1 : 0);  /// 0
				gtk_widget_hide (icon->pSubDock->container.pWidget);
			}
		}
	}
	return TRUE;
}

static gboolean _on_leave_notify (G_GNUC_UNUSED GtkWidget* pWidget, GdkEventCrossing* pEvent, CairoDock *pDock)
{
	// g_print ("%s (bIsMainDock : %d; bInside:%d; iState:%d; iRefCount:%d, pEvent: %p)\n", __func__, pDock->bIsMainDock, pDock->container.bInside, pDock->iInputState, pDock->iRefCount, pEvent);
	//\_______________ On tire le dock => on ignore le signal.
	if (pEvent != NULL && (pEvent->state & GDK_MOD1_MASK) && (pEvent->state & GDK_BUTTON1_MASK))
	{
		return FALSE;
	}
	
	//\_______________ On ignore les signaux errones venant d'un WM buggue (Kwin) ou meme de X (changement de bureau).
	//if (pEvent)
		//g_print ("leave event: %d;%d; %d;%d; %d; %d\n", (int)pEvent->x, (int)pEvent->y, (int)pEvent->x_root, (int)pEvent->y_root, pEvent->mode, pEvent->detail);
	if (pEvent && (pEvent->x != 0 ||  pEvent->y != 0 || pEvent->x_root != 0 || pEvent->y_root != 0))  // strange leave events occur (detail = GDK_NOTIFY_NONLINEAR, nil coordinates); let's ignore them!
	{
		if (pDock->container.bIsHorizontal)
		{
			pDock->container.iMouseX = pEvent->x;
			pDock->container.iMouseY = pEvent->y;
		}
		else
		{
			pDock->container.iMouseX = pEvent->y;
			pDock->container.iMouseY = pEvent->x;
		}
	}
	else
	{
		//g_print (" forced leave event: %d;%d\n", pDock->container.iMouseX, pDock->container.iMouseY);
	}

	/// no global mouse position on Wayland, the below check and later checks for mouse position will not work
	if (gldi_container_is_wayland_backend () && pEvent)
		pDock->iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;

	if (/**pEvent && */!_mouse_is_really_outside(pDock))  // check that the mouse is really outside (the request might not come from the Window Manager, for instance if we deactivate the menu; this also works around buggy WM like KWin).
	{
		//g_print (" not really outside (%d;%d ; %d/%d)\n", pDock->container.iMouseX, pDock->container.iMouseY, pDock->iMaxDockHeight, pDock->iMinDockHeight);
		if (pDock->iSidTestMouseOutside == 0 && pEvent && ! pDock->bHasModalWindow)  // si l'action induit un changement de bureau, ou une appli qui bloque le focus (gksu), X envoit un signal de sortie alors qu'on est encore dans le dock, et donc n'en n'envoit plus lorsqu'on en sort reellement. On teste donc pendant qques secondes apres l'evenement. C'est ausi vrai pour l'affichage d'un menu/dialogue interactif, mais comme on envoie nous-meme un signal de sortie lorsque le menu disparait, il est inutile de le faire ici.
		{
			//g_print ("start checking mouse\n");
			pDock->iSidTestMouseOutside = g_timeout_add (500, (GSourceFunc)_check_mouse_outside, pDock);
		}
		return FALSE;
	}
	
	//\_______________ On retarde la sortie.
	if (pEvent != NULL)  // sortie naturelle.
	{
		if (pDock->iSidLeaveDemand == 0)  // pas encore de demande de sortie.
		{
			if (pDock->iRefCount == 0)  // cas du main dock : on retarde si on pointe sur un sous-dock (pour laisser le temps au signal d'entree dans le sous-dock d'etre traite) ou si l'on a l'auto-hide.
			{
				//g_print (" leave event : %.1f;%.1f (%dx%d)\n", pEvent->x, pEvent->y, pDock->container.iWidth, pDock->container.iHeight);
				Icon *pPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
				if (pPointedIcon != NULL && pPointedIcon->pSubDock != NULL && gldi_container_is_visible (CAIRO_CONTAINER (pPointedIcon->pSubDock)))
				{
					//g_print (" on retarde la sortie du dock de %dms\n", MAX (myDocksParam.iLeaveSubDockDelay, 330));
					pDock->iSidLeaveDemand = g_timeout_add (MAX (myDocksParam.iLeaveSubDockDelay, 250), (GSourceFunc) _emit_leave_signal_delayed, (gpointer) pDock);
					return TRUE;
				}
				else if (pDock->bAutoHide)
				{
					const int delay = 0;  // 250
					if (delay != 0)  /// maybe try to see if we left the dock frankly, or just by a few pixels...
					{
						//g_print (" delay the leave event by %dms\n", delay);
						pDock->iSidLeaveDemand = g_timeout_add (250, (GSourceFunc) _emit_leave_signal_delayed, (gpointer) pDock);
						return TRUE;
					}
				}
			}
			else/** if (myDocksParam.iLeaveSubDockDelay != 0)*/  // cas d'un sous-dock : on retarde le cachage.
			{
				//g_print (" on retarde la sortie du sous-dock de %dms\n", myDocksParam.iLeaveSubDockDelay);
				pDock->iSidLeaveDemand = g_timeout_add (MAX (myDocksParam.iLeaveSubDockDelay, 50), (GSourceFunc) _emit_leave_signal_delayed, (gpointer) pDock);
				//g_print (" -> pDock->iSidLeaveDemand = %d\n", pDock->iSidLeaveDemand);
				return TRUE;
			}
		}
		else  // deja une sortie en attente.
		{
			//g_print (" une sortie est deja programmee (%d)\n", pDock->iSidLeaveDemand);
			return TRUE;
		}
	}  // sinon c'est nous qui avons explicitement demande cette sortie, donc on continue.
	
	if (pDock->iSidTestMouseOutside != 0)
	{
		//g_print ("stop checking mouse (leave)\n");
		g_source_remove (pDock->iSidTestMouseOutside);
		pDock->iSidTestMouseOutside = 0;
	}
	
	//\_______________ Arrive ici, on est sorti du dock.
	pDock->container.bInside = FALSE;
	pDock->iAvoidingMouseIconType = -1;
	pDock->fAvoidingMouseMargin = 0;
	
	//\_______________ On cache ses sous-docks.
	if (! _hide_child_docks (pDock))  // on quitte si l'un des sous-docks reste visible (on est entre dedans), pour rester en position "haute".
	{
		//g_print (" un des sous-docks reste visible");
		return TRUE;
	}
	
	if (pDock->iRefCount != 0)  // sub-dock -> if the main icon is currently pointed, and doesn't have a dialog that would be in the way, stay visible
	{
		CairoDock *pParentDock = NULL;
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
		if (pPointingIcon && pParentDock)
		{
			if (pPointingIcon->bPointed && pParentDock->container.bInside && ! gldi_icon_has_dialog (pPointingIcon))
			{
				//g_print (" the main icon is currently pointed, stay visible\n");
				return TRUE;
			}
		}
	}
	
	if (s_iSidShowSubDockDemand != 0 && (pDock->iRefCount == 0 || s_pSubDockShowing == pDock))  // si ce dock ou l'un des sous-docks etait programme pour se montrer, on annule.
	{
		g_source_remove (s_iSidShowSubDockDemand);
		s_iSidShowSubDockDemand = 0;
		s_pDockShowingSubDock = NULL;
		s_pSubDockShowing = NULL;
	}
	
	//\_______________ If a modal window is raised, we discard the 'leave-event' to stay in the up position.
	if (pDock->bHasModalWindow)
		return TRUE;
	
	//\_______________ On gere le drag d'une icone hors du dock.
	if (s_pIconClicked != NULL
	&& (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (s_pIconClicked)
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (s_pIconClicked)
		|| (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (s_pIconClicked) && s_pIconClicked->cDesktopFileName && pDock->iMaxDockHeight > 30)  // if the dock is narrow (like a panel), prevent from dragging separators outside of the dock. TODO: maybe we need a parameter in the view...
		|| CAIRO_DOCK_IS_DETACHABLE_APPLET (s_pIconClicked))
	&& s_pFlyingContainer == NULL
	&& ! myDocksParam.bLockIcons
	&& ! myDocksParam.bLockAll
	&& ! pDock->bPreventDraggingIcons)
	{
		cd_debug ("on a sorti %s du dock (%d;%d) / %dx%d", s_pIconClicked->cName, pDock->container.iMouseX, pDock->container.iMouseY, pDock->container.iWidth, pDock->container.iHeight);
		
		//if (! _hide_child_docks (pDock))  // on quitte si on entre dans un sous-dock, pour rester en position "haute".
		//	return ;
		
		CairoDock *pOriginDock = CAIRO_DOCK(cairo_dock_get_icon_container (s_pIconClicked));
		if (pOriginDock == pDock && _mouse_is_really_outside (pDock))  // ce test est la pour parer aux WM deficients mentaux comme KWin qui nous font sortir/rentrer lors d'un clic.
		{
			cd_debug (" on detache l'icone");
			pOriginDock->bIconIsFlyingAway = TRUE;
			gldi_icon_detach (s_pIconClicked);
			cairo_dock_stop_icon_glide (pOriginDock);
			
			s_pFlyingContainer = gldi_flying_container_new (s_pIconClicked, pOriginDock);
			//g_print ("- s_pIconClicked <- NULL\n");
			s_pIconClicked = NULL;
			if (pDock->iRefCount > 0 || pDock->bAutoHide)  // pour garder le dock visible.
			{
				return TRUE;
			}
		}
	}
	/**else if (s_pFlyingContainer != NULL && s_pFlyingContainer->pIcon != NULL && pDock->iRefCount > 0)  // on evite les bouclages.
	{
		CairoDock *pOriginDock = gldi_dock_get (s_pFlyingContainer->pIcon->cParentDockName);
		if (pOriginDock == pDock)
			return GLDI_NOTIFICATION_INTERCEPT;
	}*/
	
	gboolean bStartAnimation = FALSE;
	gldi_object_notify (pDock, NOTIFICATION_LEAVE_DOCK, pDock, &bStartAnimation);
	if (bStartAnimation)
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	
	return TRUE;
}

static gboolean _on_dock_unmap (GtkWidget* pWidget, G_GNUC_UNUSED GdkEvent* pEvent, CairoDock *pDock)
{
	// this event is only necessary on Wayland
	// g_print ("_on_dock_unmap() for dock: %p (bIsMainDock : %d; bInside:%d)\n", pDock, pDock->bIsMainDock, pDock->container.bInside);
	pDock->iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;
	_on_leave_notify (pWidget, NULL, pDock);
	return FALSE;
}

static gboolean _on_enter_notify (G_GNUC_UNUSED GtkWidget* pWidget, GdkEventCrossing* pEvent, CairoDock *pDock)
{
	// g_print ("%s (bIsMainDock : %d; bInside:%d; state:%d; iMagnitudeIndex:%d; input shape:%p; event:%p)\n", __func__, pDock->bIsMainDock, pDock->container.bInside, pDock->iInputState, pDock->iMagnitudeIndex, pDock->pShapeBitmap, pEvent);
	if (! cairo_dock_entrance_is_allowed (pDock))
	{
		cd_message ("* entree non autorisee");
		return FALSE;
	}
	
	if (gldi_container_is_wayland_backend ())
		pDock->iMousePositionType = CAIRO_DOCK_MOUSE_INSIDE;

	// stop les timers.
	if (pDock->iSidLeaveDemand != 0)
	{
		g_source_remove (pDock->iSidLeaveDemand);
		pDock->iSidLeaveDemand = 0;
	}
	if (s_iSidShowSubDockDemand != 0)  // gere un cas tordu mais bien reel.
	{
		g_source_remove (s_iSidShowSubDockDemand);
		s_iSidShowSubDockDemand = 0;
	}
	if (pDock->iSidHideBack != 0)
	{
		//g_print ("remove hide back timeout\n");
		g_source_remove (pDock->iSidHideBack);
		pDock->iSidHideBack = 0;
	}
	if (pDock->iSidTestMouseOutside != 0)
	{
		//g_print ("stop checking mouse (enter)\n");
		g_source_remove (pDock->iSidTestMouseOutside);
		pDock->iSidTestMouseOutside = 0;
	}
	
	// input shape desactivee, le dock devient actif.
	if ((pDock->pShapeBitmap || pDock->pHiddenShapeBitmap) && pDock->iInputState != CAIRO_DOCK_INPUT_ACTIVE)
	{
		//g_print ("+++ input shape active on enter\n");
		cairo_dock_set_input_shape_active (pDock);
	}
	pDock->iInputState = CAIRO_DOCK_INPUT_ACTIVE;
	
	// si on etait deja dedans, ou qu'on etait cense l'etre, on relance juste le grossissement.
	/**if (pDock->container.bInside || pDock->bIsHiding)
	{
		pDock->container.bInside = TRUE;
		cairo_dock_start_growing (pDock);
		if (pDock->bIsHiding || cairo_dock_is_hidden (pDock))  // on (re)monte.
		{
			cd_debug ("  on etait deja dedans\n");
			cairo_dock_start_showing (pDock);
		}
		return FALSE;
	}*/
	
	gboolean bWasInside = pDock->container.bInside;
	pDock->container.bInside = TRUE;
	
	// animation d'entree.
	gboolean bStartAnimation = FALSE;
	gldi_object_notify (pDock, NOTIFICATION_ENTER_DOCK, pDock, &bStartAnimation);
	if (bStartAnimation)
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	
	pDock->fDecorationsOffsetX = 0;
	cairo_dock_stop_quick_hide ();
	
	if (s_pIconClicked != NULL)  // on pourrait le faire a chaque motion aussi.
	{
		pDock->iAvoidingMouseIconType = s_pIconClicked->iGroup;
		pDock->fAvoidingMouseMargin = .5;  /// inutile il me semble ...
	}
	
	// si on rentre avec une icone volante, on la met dedans.
	if (s_pFlyingContainer != NULL)
	{
		Icon *pFlyingIcon = s_pFlyingContainer->pIcon;
		if (pDock != pFlyingIcon->pSubDock)  // on evite les boucles.
		{
			struct timeval tv;
			int r = gettimeofday (&tv, NULL);
			double t = 0.;
			if (r == 0)
				t = tv.tv_sec + tv.tv_usec * 1e-6;
			if (t - s_pFlyingContainer->fCreationTime > 1)  // on empeche le cas ou enlever l'icone fait augmenter le ratio du dock, et donc sa hauteur, et nous fait rentrer dedans des qu'on sort l'icone.
			{
				//g_print ("on remet l'icone volante dans un dock (dock d'origine : %s)\n", pFlyingIcon->cParentDockName);
				gldi_object_unref (GLDI_OBJECT(s_pFlyingContainer));  // will detach the icon
				gldi_icon_stop_animation (pFlyingIcon);
				// reinsert the icon where it was dropped, not at its original position.
				Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);  // get the pointed icon before we insert the icon, since the inserted icon will be the pointed one!
				//g_print (" pointed icon: %s\n", icon?icon->cName:"none");
				gldi_icon_insert_in_container (pFlyingIcon, CAIRO_CONTAINER(pDock), CAIRO_DOCK_ANIMATE_ICON);
				if (icon != NULL && cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pFlyingIcon))
				{
					cairo_dock_move_icon_after_icon (pDock, pFlyingIcon, icon);
				}
				s_pFlyingContainer = NULL;
				pDock->bIconIsFlyingAway = FALSE;
			}
		}
	}
	
	// si on etait derriere, on repasse au premier plan.
	if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && pDock->bIsBelow && pDock->iRefCount == 0)
	{
		cairo_dock_pop_up (pDock);
	}
	
	// si on etait cache (entierement ou partiellement), on montre.
	if ((pDock->bIsHiding || cairo_dock_is_hidden (pDock)) && pDock->iRefCount == 0)
	{
		//g_print ("  on commence a monter\n");
		cairo_dock_start_showing (pDock);  // on a mis a jour la zone d'input avant, sinon la fonction le ferait, ce qui serait inutile.
	}
	
	// start growing up (do it before calculating icons, so that we don't seem to be in an anormal state, where we're inside a dock that doesn't grow).
	cairo_dock_start_growing (pDock);
	
	// since we've just entered the dock, the pointed icon has changed from none to the current one.
	if (pEvent != NULL && ! bWasInside)
	{
		// update the mouse coordinates
		if (pDock->container.bIsHorizontal)
		{
			pDock->container.iMouseX = (int) pEvent->x;
			pDock->container.iMouseY = (int) pEvent->y;
		}
		else
		{
			pDock->container.iMouseX = (int) pEvent->y;
			pDock->container.iMouseY = (int) pEvent->x;
		}
		// then compute the icons (especially the pointed one).
		Icon *icon = cairo_dock_calculate_dock_icons (pDock);  // returns the pointed icon
		// trigger the change to trigger the animation and sub-dock popup
		if (icon != NULL)
		{
			_on_change_icon (NULL, icon, pDock);  // we were out of the dock, so there is no previous pointed icon.
		}
	}
	
	return TRUE;
}


static gboolean _on_key_release (G_GNUC_UNUSED GtkWidget *pWidget,
	GdkEventKey *pKey,
	CairoDock *pDock)
{
	cd_debug ("on a appuye sur une touche (%d/%d)", pKey->keyval, pKey->hardware_keycode);
	if (pKey->type == GDK_KEY_PRESS)
	{
		gldi_object_notify (pDock, NOTIFICATION_KEY_PRESSED, pDock, pKey->keyval, pKey->state, pKey->string, pKey->hardware_keycode);
	}
	else if (pKey->type == GDK_KEY_RELEASE)
	{
		//g_print ("release : pKey->keyval = %d\n", pKey->keyval);
		if ((pKey->state & GDK_MOD1_MASK) && pKey->keyval == 0)  // On relache la touche ALT, typiquement apres avoir fait un ALT + clique gauche + deplacement.
		{
			if (pDock->iRefCount == 0 && pDock->iVisibility != CAIRO_DOCK_VISI_SHORTKEY)
				gldi_rootdock_write_gaps (pDock);
		}
	}
	return TRUE;
}


static gboolean _double_click_delay_over (Icon *icon)
{
	CairoDock *pDock = CAIRO_DOCK (cairo_dock_get_icon_container(icon));
	if (pDock)
	{
		gldi_icon_stop_attention (icon);  // we consider that clicking on the icon is an acknowledge of the demand of attention.
		pDock->container.iMouseX = s_iFirstClickX;
		pDock->container.iMouseY = s_iFirstClickY;
		gldi_object_notify (pDock, NOTIFICATION_CLICK_ICON, icon, pDock, GDK_BUTTON1_MASK);
		
		gldi_icon_start_animation (icon);
	}
	icon->iSidDoubleClickDelay = 0;
	return FALSE;
}
static gboolean _check_mouse_outside (CairoDock *pDock)  // ce test est principalement fait pour detecter les cas ou X nous envoit un signal leave errone alors qu'on est dedans (=> sortie refusee, bInside reste a TRUE), puis du coup ne nous en envoit pas de leave lorsqu'on quitte reellement le dock.
{
	// g_print (" %s (%d, %d, %d)\n", __func__, pDock->bIsShrinkingDown, pDock->iMagnitudeIndex, pDock->container.bInside);
	if (pDock->bIsShrinkingDown || pDock->iMagnitudeIndex == 0 || ! pDock->container.bInside)  // trivial cases : if the dock has already shrunk, or we're not inside any more, we can quit the loop.
	{
		pDock->iSidTestMouseOutside = 0;
		return FALSE;
	}
	
	gldi_container_update_mouse_position (CAIRO_CONTAINER (pDock));
	// g_print (" -> (%d, %d)\n", pDock->container.iMouseX, pDock->container.iMouseY);
	
	cairo_dock_calculate_dock_icons (pDock);  // pour faire retrecir le dock si on n'est pas dedans, merci X de nous faire sortir du dock alors que la souris est toujours dedans :-/
	return TRUE;
}
static gboolean _on_button_press (G_GNUC_UNUSED GtkWidget* pWidget, GdkEventButton* pButton, CairoDock *pDock)
{
	//g_print ("+ %s (%d/%d, %x, %d)\n", __func__, pButton->type, pButton->button, pWidget, pButton->time);
	static guint32 s_iLastTime = 0;  // time of the last event, in ms
	if (s_iLastTime != 0 && pButton->time == s_iLastTime)  // for some reason, with latest GTK3, we get all events twice; filter them
		return TRUE;  // discard and don't propagate
	s_iLastTime = pButton->time;
	if (pDock->container.bIsHorizontal)  // utile ?
	{
		pDock->container.iMouseX = (int) pButton->x;
		pDock->container.iMouseY = (int) pButton->y;
	}
	else
	{
		pDock->container.iMouseX = (int) pButton->y;
		pDock->container.iMouseY = (int) pButton->x;
	}

	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	if (pButton->button == 1)  // clic gauche.
	{
		//g_print ("+ left click\n");
		switch (pButton->type)
		{
			case GDK_BUTTON_RELEASE :
				//g_print ("+ GDK_BUTTON_RELEASE (%d/%d sur %s/%s)\n", pButton->state, GDK_CONTROL_MASK | GDK_MOD1_MASK, icon ? icon->cName : "personne", icon ? icon->cCommand : "");  // 272 = 100010000
				if (pDock->container.bIgnoreNextReleaseEvent)
				{
					pDock->container.bIgnoreNextReleaseEvent = FALSE;
					s_pIconClicked = NULL;
					s_bIconDragged = FALSE;
					return TRUE;
				}
				
				if ( ! (pButton->state & GDK_MOD1_MASK))
				{
					if (s_pIconClicked != NULL)
					{
						cd_debug ("activate %s (%s)", s_pIconClicked->cName, icon ? icon->cName : "none");
						s_pIconClicked->iAnimationState = CAIRO_DOCK_STATE_REST;  // stoppe les animations de suivi du curseur.
						pDock->iAvoidingMouseIconType = -1;
						cairo_dock_stop_icon_glide (pDock);
					}
					if (icon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) && icon == s_pIconClicked)  // released the button on the clicked icon => trigger the CLICK signal.
					{
						s_pIconClicked = NULL;  // il faut le faire ici au cas ou le clic induirait un dialogue bloquant qui nous ferait sortir du dock par exemple.
						//g_print ("+ click on '%s' (%s)\n", icon->cName, icon->cCommand);
						if (! s_bIconDragged)  // on ignore le drag'n'drop sur elle-meme.
						{
							if (icon->iNbDoubleClickListeners > 0)
							{
								if (icon->iSidDoubleClickDelay == 0)  // 1er release.
								{
									icon->iSidDoubleClickDelay = g_timeout_add (CD_DOUBLE_CLICK_DELAY, (GSourceFunc)_double_click_delay_over, icon);
									s_iFirstClickX = pDock->container.iMouseX;  // the mouse can move between the first and the second clicks; since the event is triggered when the second click occurs, the coordinates may be wrong -> we have to remember the position of the first click.
									s_iFirstClickY = pDock->container.iMouseY;
								}
							}
							else
							{
								gldi_icon_stop_attention (icon);  // we consider that clicking on the icon is an acknowledge of the demand of attention.
								
								gldi_object_notify (pDock, NOTIFICATION_CLICK_ICON, icon, pDock, pButton->state);
								
								gldi_icon_start_animation (icon);
							}
						}
					}
					else if (s_pIconClicked != NULL && icon != NULL && icon != s_pIconClicked && ! myDocksParam.bLockIcons && ! myDocksParam.bLockAll && ! pDock->bPreventDraggingIcons)  // released the icon on another one.
					{
						//g_print ("deplacement de %s\n", s_pIconClicked->cName);
						CairoDock *pOriginDock = CAIRO_DOCK (cairo_dock_get_icon_container (s_pIconClicked));
						if (pOriginDock != NULL && pDock != pOriginDock)
						{
							gldi_icon_detach (s_pIconClicked);
							
							gldi_theme_icon_write_container_name_in_conf_file (s_pIconClicked, gldi_dock_get_name (pDock));
							
							gldi_icon_insert_in_container (s_pIconClicked, CAIRO_CONTAINER(pDock), CAIRO_DOCK_ANIMATE_ICON);
						}
						
						Icon *prev_icon, *next_icon;
						if (icon->fXAtRest > s_pIconClicked->fXAtRest)
						{
							prev_icon = icon;
							next_icon = cairo_dock_get_next_icon (pDock->icons, icon);
						}
						else
						{
							prev_icon = cairo_dock_get_previous_icon (pDock->icons, icon);
							next_icon = icon;
						}
						if (icon->iGroup != s_pIconClicked->iGroup
						&& (prev_icon == NULL || prev_icon->iGroup != s_pIconClicked->iGroup)
						&& (next_icon == NULL || next_icon->iGroup != s_pIconClicked->iGroup))
						{
							s_pIconClicked = NULL;
							return FALSE;
						}
						//g_print ("deplacement de %s\n", s_pIconClicked->cName);
						///if (prev_icon != NULL && prev_icon->iGroup != s_pIconClicked->iGroup)  // the previous icon is in a different group -> we'll be at the beginning of our group.
						///	prev_icon = NULL;  // => move to the beginning of the group/dock
						cairo_dock_move_icon_after_icon (pDock, s_pIconClicked, prev_icon);
						
						cairo_dock_calculate_dock_icons (pDock);
						
						if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (s_pIconClicked))
						{
							gldi_icon_request_animation (s_pIconClicked, "bounce", 2);
						}
						gtk_widget_queue_draw (pDock->container.pWidget);
					}
					
					if (s_pFlyingContainer != NULL)  // the user released the flying icon -> detach/destroy it, or insert it
					{
						cd_debug ("on relache l'icone volante");
						if (pDock->container.bInside)
						{
							//g_print ("  on la remet dans son dock d'origine\n");
							Icon *pFlyingIcon = s_pFlyingContainer->pIcon;
							gldi_object_unref (GLDI_OBJECT(s_pFlyingContainer));
							cairo_dock_stop_marking_icon_as_following_mouse (pFlyingIcon);
							// reinsert the icon where it was dropped, not at its original position.
							Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);  // get the pointed icon before we insert the icon, since the inserted icon will be the pointed one!
							gldi_icon_insert_in_container (pFlyingIcon, CAIRO_CONTAINER(pDock), CAIRO_DOCK_ANIMATE_ICON);
							if (icon != NULL && cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pFlyingIcon))
							{
								cairo_dock_move_icon_after_icon (pDock, pFlyingIcon, icon);
							}
						}
						else
						{
							gldi_flying_container_terminate (s_pFlyingContainer);  // supprime ou detache l'icone, l'animation se terminera toute seule.
						}
						s_pFlyingContainer = NULL;
						pDock->bIconIsFlyingAway = FALSE;
						cairo_dock_stop_icon_glide (pDock);
					}
					/// a implementer ...
					///gldi_object_notify (CAIRO_CONTAINER (pDock), CAIRO_DOCK_RELEASE_ICON, icon, pDock);
				}
				else
				{
					if (pDock->iRefCount == 0 && pDock->iVisibility != CAIRO_DOCK_VISI_SHORTKEY)
						gldi_rootdock_write_gaps (pDock);
				}
				//g_print ("- apres clic : s_pIconClicked <- NULL\n");
				s_pIconClicked = NULL;
				s_bIconDragged = FALSE;
			break ;
			
			case GDK_BUTTON_PRESS :
				if ( ! (pButton->state & GDK_MOD1_MASK))
				{
					//g_print ("+ clic sur %s (%.2f)!\n", icon ? icon->cName : "rien", icon ? icon->fInsertRemoveFactor : 0.);
					s_iClickX = pButton->x;
					s_iClickY = pButton->y;
					if (icon && ! cairo_dock_icon_is_being_removed (icon) && ! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
					{
						s_pIconClicked = icon;  // on ne definit pas l'animation FOLLOW_MOUSE ici , on le fera apres le 1er mouvement, pour eviter que l'icone soit dessinee comme tel quand on clique dessus alors que le dock est en train de jouer une animation (ca provoque un flash desagreable).
						cd_debug ("clicked on %s", icon->cName);
					}
					else
						s_pIconClicked = NULL;
				}
			break ;

			case GDK_2BUTTON_PRESS :
				if (icon && ! cairo_dock_icon_is_being_removed (icon))
				{
					if (icon->iSidDoubleClickDelay != 0)
					{
						g_source_remove (icon->iSidDoubleClickDelay);
						icon->iSidDoubleClickDelay = 0;
					}
					gldi_object_notify (pDock, NOTIFICATION_DOUBLE_CLICK_ICON, icon, pDock);
					if (icon->iNbDoubleClickListeners > 0)
						pDock->container.bIgnoreNextReleaseEvent = TRUE;
				}
			break ;

			default :
			break ;
		}
	}
	else if (pButton->button == 3 && pButton->type == GDK_BUTTON_PRESS)  // clique droit.
	{
		GtkWidget *menu = gldi_container_build_menu (CAIRO_CONTAINER (pDock), icon);  // genere un CAIRO_DOCK_BUILD_CONTAINER_MENU et CAIRO_DOCK_BUILD_ICON_MENU.
		
		gldi_menu_popup (menu);
	}
	else if (pButton->button == 2 && pButton->type == GDK_BUTTON_PRESS)  // clique milieu.
	{
		if (icon && ! cairo_dock_icon_is_being_removed (icon))
		{
			gldi_object_notify (pDock, NOTIFICATION_MIDDLE_CLICK_ICON, icon, pDock);
		}
	}

	return FALSE;
}


static gboolean _on_scroll (G_GNUC_UNUSED GtkWidget* pWidget, GdkEventScroll* pScroll, CairoDock *pDock)
{
	if (pScroll->direction != GDK_SCROLL_UP && pScroll->direction != GDK_SCROLL_DOWN)  // on degage les scrolls horizontaux.
	{
		return FALSE;
	}
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);  // can be NULL
	gldi_object_notify (pDock, NOTIFICATION_SCROLL_ICON, icon, pDock, pScroll->direction);
	
	return FALSE;
}


static gboolean _on_configure (GtkWidget* pWidget, GdkEventConfigure* pEvent, CairoDock *pDock)
{
	//g_print ("%s (%p, main dock : %d) : (%d;%d) (%dx%d)\n", __func__, pDock, pDock->bIsMainDock, pEvent->x, pEvent->y, pEvent->width, pEvent->height);
	// set the new actual size of the container
	gint iNewWidth, iNewHeight, iNewX, iNewY;
	if (pDock->container.bIsHorizontal)
	{
		iNewWidth = pEvent->width;
		iNewHeight = pEvent->height;
		
		iNewX = pEvent->x;
		iNewY = pEvent->y;
	}
	else
	{
		iNewWidth = pEvent->height;
		iNewHeight = pEvent->width;
		
		iNewX = pEvent->y;
		iNewY = pEvent->x;
	}
	
	gboolean bSizeUpdated = (iNewWidth != pDock->container.iWidth || iNewHeight != pDock->container.iHeight);
	///gboolean bIsNowSized = (pDock->container.iWidth == 1 && pDock->container.iHeight == 1 && bSizeUpdated);
	gboolean bPositionUpdated = (pDock->container.iWindowPositionX != iNewX || pDock->container.iWindowPositionY != iNewY);
	pDock->container.iWidth = iNewWidth;
	pDock->container.iHeight = iNewHeight;
	pDock->container.iWindowPositionX = iNewX;
	pDock->container.iWindowPositionY = iNewY;
	
	if (pDock->container.iWidth == 1 && pDock->container.iHeight == 1)  // the X window has not yet reached its size.
	{
		return FALSE;
	}
	
	// if the size has changed, also update everything that depends on it.
	if (bSizeUpdated)  // changement de taille
	{
		// update mouse relative position inside the window
		gldi_container_update_mouse_position (CAIRO_CONTAINER (pDock));
		if (pDock->container.iMouseX < 0 || pDock->container.iMouseX > pDock->container.iWidth)  // utile ?
			pDock->container.iMouseX = 0;
		
		// update the input shape (it has been calculated in the function that made the resize)
		cairo_dock_update_input_shape (pDock);
		switch (pDock->iInputState)  // update the input zone
		{
			case CAIRO_DOCK_INPUT_ACTIVE:
				cairo_dock_set_input_shape_active (pDock);
			break;
			case CAIRO_DOCK_INPUT_AT_REST:
				cairo_dock_set_input_shape_at_rest (pDock);
			break;
			case CAIRO_DOCK_INPUT_HIDDEN:
				cairo_dock_set_input_shape_hidden (pDock);
			break;
			default:
			break;
		}
		
		// update the GL context
		if (g_bUseOpenGL)
		{
			if (! gldi_gl_container_make_current (CAIRO_CONTAINER (pDock)))
				return FALSE;
			
			gldi_gl_container_set_ortho_view (CAIRO_CONTAINER (pDock));
			
			glClearAccum (0., 0., 0., 0.);
			glClear (GL_ACCUM_BUFFER_BIT);
			
			if (pDock->iRedirectedTexture != 0)
			{
				_cairo_dock_delete_texture (pDock->iRedirectedTexture);
				pDock->iRedirectedTexture = cairo_dock_create_texture_from_raw_data (NULL, pEvent->width, pEvent->height);
			}
		}
		
		cairo_dock_calculate_dock_icons (pDock);
		//g_print ("configure size %s\n", pDock->cDockName);
		cairo_dock_trigger_set_WM_icons_geometry (pDock);  // changement de position ou de taille du dock => on replace les icones.
		
		gldi_dialogs_replace_all ();
		
		if (/**bIsNowSized*/bSizeUpdated && g_bUseOpenGL)  // in OpenGL, the context is linked to the window; now that the window has a correct size, the context is ready -> draw things that couldn't be drawn until now.
		{
			Icon *icon;
			GList *ic;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				gboolean bDamaged = icon->bDamaged;  // if bNeedApplyBackground is also TRUE, applying the background will remove the 'damage' flag.
				if (icon->bNeedApplyBackground)  // if both are TRUE, we need to do both (for instance, the data-renderer might not redraw the icon (progressbar)). draw the bg first so that we don't draw it twice.
				{
					icon->bNeedApplyBackground = FALSE;  // set to FALSE, if it doesn't work here, it will probably never do.
					cairo_dock_apply_icon_background_opengl (icon);
				}
				if (bDamaged)
				{
					//g_print ("This icon %s is damaged (%d)\n", icon->cName, icon->iSubdockViewType);
					icon->bDamaged = FALSE;
					if (cairo_dock_get_icon_data_renderer (icon) != NULL)
					{
						cairo_dock_refresh_data_renderer (icon, CAIRO_CONTAINER (pDock));
					}
					else if (icon->iSubdockViewType != 0
					|| (icon->cClass != NULL && ! myIndicatorsParam.bUseClassIndic && (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon) || GLDI_OBJECT_IS_LAUNCHER_ICON (icon))))
					{
						cairo_dock_draw_subdock_content_on_icon (icon, pDock);
					}
					else if (CAIRO_DOCK_IS_APPLET (icon))
					{
						gldi_object_reload (GLDI_OBJECT(icon->pModuleInstance), FALSE);  // easy but safe way to redraw the icon properly.
					}
					else  // if we don't know how the icon should be drawn, just reload it.
					{
						cairo_dock_load_icon_image (icon, CAIRO_CONTAINER (pDock));
					}
					if (pDock->iRefCount != 0)  // now that the icon image is correct, redraw the pointing icon if needed
						cairo_dock_trigger_redraw_subdock_content (pDock);
				}
			}
		}
	}
	else if (bPositionUpdated)  // changement de position.
	{
		//g_print ("configure x,y\n");
		cairo_dock_trigger_set_WM_icons_geometry (pDock);  // changement de position de la fenetre du dock => on replace les icones.
		
		gldi_dialogs_replace_all ();
	}
	
	if (pDock->iRefCount == 0 && (bSizeUpdated || bPositionUpdated))
	{
		if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP)
		{
			GldiWindowActor *pActiveAppli = gldi_windows_get_active ();
			if (_gldi_window_is_on_our_way (pActiveAppli, pDock))  // la fenetre active nous gene.
			{
				if (!cairo_dock_is_temporary_hidden (pDock))
					cairo_dock_activate_temporary_auto_hide (pDock);
			}
			else
			{
				if (cairo_dock_is_temporary_hidden (pDock))
					cairo_dock_deactivate_temporary_auto_hide (pDock);
			}
		}
		else if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		{
			if (gldi_dock_search_overlapping_window (pDock) != NULL)
			{
				if (!cairo_dock_is_temporary_hidden (pDock))
					cairo_dock_activate_temporary_auto_hide (pDock);
			}
			else
			{
				if (cairo_dock_is_temporary_hidden (pDock))
					cairo_dock_deactivate_temporary_auto_hide (pDock);
			}
		}
	}
	
	gtk_widget_queue_draw (pWidget);
	
	return FALSE;
}



static gboolean s_bWaitForData = FALSE;
static gboolean s_bCouldDrop = FALSE;

void _on_drag_data_received (G_GNUC_UNUSED GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, G_GNUC_UNUSED guint info, guint time, CairoDock *pDock)
{
	cd_debug ("%s (%dx%d, %d, %d)", __func__, x, y, time, pDock->container.bInside);
	if (cairo_dock_is_hidden (pDock))  // X ne semble pas tenir compte de la zone d'input pour dropper les trucs...
		return ;
	//\_________________ On recupere l'URI.
	gchar *cReceivedData = (gchar *)gtk_selection_data_get_data (selection_data);  // the data are actually 'const guchar*', but since we only allowed text and urls, it will be a string
	g_return_if_fail (cReceivedData != NULL);
	int length = strlen (cReceivedData);
	if (cReceivedData[length-1] == '\n')
		cReceivedData[--length] = '\0';  // on vire le retour chariot final.
	if (cReceivedData[length-1] == '\r')
		cReceivedData[--length] = '\0';
	
	if (s_bWaitForData)
	{
		s_bWaitForData = FALSE;
		gdk_drag_status (dc, GDK_ACTION_COPY, time);
		cd_debug ("drag info : <%s>", cReceivedData);
		pDock->iAvoidingMouseIconType = CAIRO_DOCK_LAUNCHER;
		if (g_str_has_suffix (cReceivedData, ".desktop")/** || g_str_has_suffix (cReceivedData, ".sh")*/)
			pDock->fAvoidingMouseMargin = .5;  // on ne sera jamais dessus.
		else
			pDock->fAvoidingMouseMargin = .25;
		return ;
	}
	
	//\_________________ On arrete l'animation.
	//cairo_dock_stop_marking_icons (pDock);
	pDock->iAvoidingMouseIconType = -1;
	pDock->fAvoidingMouseMargin = 0;
	
	//\_________________ On arrete le timer.
	if (s_iSidActionOnDragHover != 0)
	{
		//cd_debug ("on annule la demande de montrage d'appli");
		g_source_remove (s_iSidActionOnDragHover);
		s_iSidActionOnDragHover = 0;
	}
	
	//\_________________ On calcule la position a laquelle on l'a lache.
	cd_debug (">>> cReceivedData : '%s' (%d/%d)", cReceivedData, s_bCouldDrop, pDock->bCanDrop);
	/* icon => drop on icon
	no icon => if order undefined: drop on dock; else: drop between 2 icons.*/
	Icon *pPointedIcon = NULL;
	double fOrder;
	if (s_bCouldDrop)  // can drop on the dock
	{
		cd_debug ("drop between icons");
		
		pPointedIcon = NULL;
		fOrder = 0;
		
		// try to guess where we dropped.
		int iDropX = (pDock->container.bIsHorizontal ? x : y);
		Icon *pNeighboorIcon;
		Icon *icon = NULL;
		GList *ic;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			if (icon->bPointed)
			{
				if (iDropX < icon->fDrawX + icon->fWidth * icon->fScale/2)  // on the left side of the icon
				{
					pNeighboorIcon = (ic->prev != NULL ? ic->prev->data : NULL);
					fOrder = (pNeighboorIcon != NULL ? (icon->fOrder + pNeighboorIcon->fOrder) / 2 : icon->fOrder - 1);
				}
				else  // on the right side of the icon
				{
					pNeighboorIcon = (ic->next != NULL ? ic->next->data : NULL);
					fOrder = (pNeighboorIcon != NULL ? (icon->fOrder + pNeighboorIcon->fOrder) / 2 : icon->fOrder + 1);
				}
				break;
			}
		}
		if (myDocksParam.bLockAll)  // locked, can't add anything.
		{
			gldi_dialog_show_temporary_with_default_icon (_("Sorry but the dock is locked"), icon, CAIRO_CONTAINER (pDock), 5000);
			gtk_drag_finish (dc, FALSE, FALSE, time);
			return ;
		}
	}
	else  // drop on an icon or nowhere.
	{
		pPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
		fOrder = CAIRO_DOCK_LAST_ORDER;
		if (pPointedIcon == NULL && ! g_str_has_suffix (cReceivedData, ".desktop"))  // no icon => abort, but .desktop are always added
		{
			cd_debug ("drop nowhere");
			gtk_drag_finish (dc, FALSE, FALSE, time);
			return;
		}
	}
	cd_debug ("drop on %s (%.2f)", pPointedIcon?pPointedIcon->cName:"dock", fOrder);
	
	gldi_container_notify_drop_data (CAIRO_CONTAINER (pDock), cReceivedData, pPointedIcon, fOrder);
	
	gtk_drag_finish (dc, TRUE, FALSE, time);
}

/*static gboolean _on_drag_drop (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, guint time, G_GNUC_UNUSED CairoDock *pDock)
{
	cd_message ("%s (%dx%d, %d)", __func__, x, y, time);
	GdkAtom target = gtk_drag_dest_find_target (pWidget, dc, NULL);
	gtk_drag_get_data (pWidget, dc, target, time);
	return TRUE;  // in a drop zone.
}*/


static gboolean _on_drag_motion (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, guint time, CairoDock *pDock)
{
	cd_debug ("%s (%d;%d, %d)", __func__, x, y, time);
	
	//\_________________ On simule les evenements souris habituels.
	if (! pDock->bIsDragging)
	{
		cd_debug ("start dragging");
		pDock->bIsDragging = TRUE;
		
		/*GdkAtom gdkAtom = gdk_drag_get_selection (dc);
		Atom xAtom = gdk_x11_atom_to_xatom (gdkAtom);
		Window Xid = GDK_WINDOW_XID (dc->source_window);
		cd_debug (" <%s>", cairo_dock_get_property_name_on_xwindow (Xid, xAtom));*/
		
		gboolean bStartAnimation = FALSE;
		gldi_object_notify (pDock, NOTIFICATION_START_DRAG_DATA, pDock, &bStartAnimation);
		if (bStartAnimation)
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
		
		/*pDock->iAvoidingMouseIconType = -1;
		
		GdkAtom target = gtk_drag_dest_find_target (pWidget, dc, NULL);
		if (target == GDK_NONE)
			gdk_drag_status (dc, 0, time);
		else
		{
			gtk_drag_get_data (pWidget, dc, target, time);
			s_bWaitForData = TRUE;
			cd_debug ("get-data envoye\n");
		}*/
		
		_on_enter_notify (pWidget, NULL, pDock);  // ne sera effectif que la 1ere fois a chaque entree dans un dock.
	}
	else
	{
		//g_print ("move dragging\n");
		_on_motion_notify (pWidget, NULL, pDock);
	}
	
	int X, Y;
	if (pDock->container.bIsHorizontal)
	{
		X = x - pDock->container.iWidth/2;
		Y = y;
	}
	else
	{
		Y = x;
		X = y - pDock->container.iWidth/2;
	}
	if (pDock->iInputState == CAIRO_DOCK_INPUT_AT_REST)
	{
		int w = pDock->iMinDockWidth;
		int h = pDock->iMinDockHeight;
		
		if (X <= -w/2 || X >= w/2)
			return FALSE;  // on n'accepte pas le drop.
		if (pDock->container.bDirectionUp)
		{
			if (Y < pDock->container.iHeight - h || Y >= pDock->container.iHeight)
				return FALSE;  // on n'accepte pas le drop.
		}
		else
		{
			if (Y < 0 || Y > h)
				return FALSE;  // on n'accepte pas le drop.
		}
	}
	else if (pDock->iInputState == CAIRO_DOCK_INPUT_HIDDEN)
	{
		return FALSE;  // on n'accepte pas le drop.
	}
	
	//g_print ("take the drop\n");
	gdk_drag_status (dc, GDK_ACTION_COPY, time);
	return TRUE;  // on accepte le drop.
}

void _on_drag_leave (GtkWidget *pWidget, G_GNUC_UNUSED GdkDragContext *dc, G_GNUC_UNUSED guint time, CairoDock *pDock)
{
	//g_print ("stop dragging 1\n");
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	if ((icon && icon->pSubDock) || pDock->iRefCount > 0)  // on retarde l'evenement, car il arrive avant le leave-event, et donc le sous-dock se cache avant qu'on puisse y entrer.
	{
		cd_debug (">>> on attend...");
		while (gtk_events_pending ())  // on laisse le temps au signal d'entree dans le sous-dock d'etre traite, de facon a avoir un start-dragging avant de quitter cette fonction.
			gtk_main_iteration ();
		cd_debug (">>> pDock->container.bInside : %d", pDock->container.bInside);
	}
	//g_print ("stop dragging 2\n");
	s_bWaitForData = FALSE;
	pDock->bIsDragging = FALSE;
	s_bCouldDrop = pDock->bCanDrop;
	pDock->bCanDrop = FALSE;
	//cairo_dock_stop_marking_icons (pDock);
	pDock->iAvoidingMouseIconType = -1;
	
	// emit a leave-event signal, since we don't get one if we leave the window too quickly (!)
	if (pDock->iSidLeaveDemand == 0)
	{
		pDock->iSidLeaveDemand = g_timeout_add (MAX (myDocksParam.iLeaveSubDockDelay, 330), (GSourceFunc) _emit_leave_signal_delayed, (gpointer) pDock);  // emit with a delay, so that we can leave and enter the dock for a few ms without making it hide.
	}
	// emulate a motion event so that the mouse position is up-to-date (which is not the case if we leave the window too quickly).
	_on_motion_notify (pWidget, NULL, pDock);
}


  ///////////////////////
 /// CONTAINER IFACE ///
///////////////////////

static gboolean _cairo_dock_grow_up (CairoDock *pDock)
{
	//g_print ("%s (%d ; %2f ; bInside:%d)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->container.bInside);
	
	pDock->iMagnitudeIndex += myBackendsParam.iGrowUpInterval;
	if (pDock->iMagnitudeIndex > CAIRO_DOCK_NB_MAX_ITERATIONS)
		pDock->iMagnitudeIndex = CAIRO_DOCK_NB_MAX_ITERATIONS;

	if (pDock->fFoldingFactor != 0)
	{
		int iAnimationDeltaT = cairo_dock_get_animation_delta_t (pDock);
		pDock->fFoldingFactor -= (double) iAnimationDeltaT / myBackendsParam.iUnfoldingDuration;
		if (pDock->fFoldingFactor < 0)
			pDock->fFoldingFactor = 0;
	}
	
	gldi_container_update_mouse_position (CAIRO_CONTAINER (pDock));
	
	Icon *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	Icon *pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
	if (! pDock->bIsGrowingUp)
		return FALSE;
	
	if (pLastPointedIcon != pPointedIcon && pDock->container.bInside)
		_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);  /// probablement inutile...

	if (pDock->iMagnitudeIndex == CAIRO_DOCK_NB_MAX_ITERATIONS && pDock->fFoldingFactor == 0)  // fin de grossissement et de depliage.
	{
		gldi_dialogs_replace_all ();
		return FALSE;
	}
	else
		return TRUE;
}

static void _hide_parent_dock (CairoDock *pDock)
{
	CairoDock *pParentDock = NULL;
	Icon *pIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
	if (pIcon && pParentDock)
	{
		if (pParentDock->iRefCount == 0)
		{
			cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pParentDock));
		}
		else
		{
			//cd_message ("on cache %s par parente", cDockName);
			gtk_widget_hide (pParentDock->container.pWidget);
			_hide_parent_dock (pParentDock);
		}
	}
}

static gboolean _cairo_dock_shrink_down (CairoDock *pDock)
{
	//g_print ("%s (%d, %f, %f)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->fDecorationsOffsetX);
	//\_________________ On fait decroitre la magnitude du dock.
	pDock->iMagnitudeIndex -= myBackendsParam.iShrinkDownInterval;
	if (pDock->iMagnitudeIndex < 0)
		pDock->iMagnitudeIndex = 0;
	
	//\_________________ On replie le dock.
	if (pDock->fFoldingFactor != 0 && pDock->fFoldingFactor != 1)
	{
		int iAnimationDeltaT = cairo_dock_get_animation_delta_t (pDock);
		pDock->fFoldingFactor += (double) iAnimationDeltaT / myBackendsParam.iUnfoldingDuration;
		if (pDock->fFoldingFactor > 1)
			pDock->fFoldingFactor = 1;
	}
	
	//\_________________ On remet les decorations a l'equilibre.
	pDock->fDecorationsOffsetX *= .8;
	if (fabs (pDock->fDecorationsOffsetX) < 3)
		pDock->fDecorationsOffsetX = 0.;
	
	//\_________________ On recupere la position de la souris manuellement (car a priori on est hors du dock).
	gldi_container_update_mouse_position (CAIRO_CONTAINER (pDock));  // ce n'est pas le motion_notify qui va nous donner des coordonnees en dehors du dock, et donc le fait d'etre dedans va nous faire interrompre le shrink_down et re-grossir, du coup il faut le faire ici. L'inconvenient, c'est que quand on sort par les cotes, il n'y a soudain plus d'icone pointee, et donc le dock devient tout plat subitement au lieu de le faire doucement. Heureusement j'ai trouve une astuce. ^_^
	
	//\_________________ On recalcule les icones.
	///if (iPrevMagnitudeIndex != 0)
	{
		cairo_dock_calculate_dock_icons (pDock);
		if (! pDock->bIsShrinkingDown)
			return FALSE;
	}

	if (pDock->iMagnitudeIndex == 0 && (pDock->fFoldingFactor == 0 || pDock->fFoldingFactor == 1))  // on est arrive en bas.
	{
		//g_print ("equilibre atteint (%d)\n", pDock->container.bInside);
		if (! pDock->container.bInside)  // on peut etre hors des icones sans etre hors de la fenetre.
		{
			//g_print ("rideau !\n");
			
			//\__________________ On repasse derriere si on etait devant.
			if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && ! pDock->bIsBelow)
				cairo_dock_pop_down (pDock);
			
			//\__________________ On se redimensionne en taille normale.
			if (! pDock->bAutoHide && pDock->iRefCount == 0/** && ! pDock->bMenuVisible*/)  // fin de shrink sans auto-hide => taille normale.
			{
				//g_print ("taille normale (%x; %d)\n", pDock->pShapeBitmap , pDock->iInputState);
				if (pDock->pShapeBitmap && pDock->iInputState != CAIRO_DOCK_INPUT_AT_REST)
				{
					//g_print ("+++ input shape at rest on end shrinking\n");
					cairo_dock_set_input_shape_at_rest (pDock);
					pDock->iInputState = CAIRO_DOCK_INPUT_AT_REST;
				}
			}
			
			//\__________________ On se cache si sous-dock.
			if (pDock->iRefCount > 0)
			{
				//g_print ("on cache ce sous-dock en sortant par lui\n");
				gtk_widget_hide (pDock->container.pWidget);
				_hide_parent_dock (pDock);
			}
			///cairo_dock_hide_after_shortcut ();
			if (pDock->iVisibility == CAIRO_DOCK_VISI_SHORTKEY)  // hide at the end of the shrink animation
			{
				gtk_widget_hide (pDock->container.pWidget);
			}
		}
		else
		{
			cairo_dock_calculate_dock_icons (pDock);  // relance le grossissement si on est dedans.
		}
		if (!pDock->bIsGrowingUp)
			gldi_dialogs_replace_all ();
		return (!pDock->bIsGrowingUp && (pDock->fDecorationsOffsetX != 0 || (pDock->fFoldingFactor != 0 && pDock->fFoldingFactor != 1)));
	}
	else
	{
		return (!pDock->bIsGrowingUp);
	}
}

static gboolean _cairo_dock_hide (CairoDock *pDock)
{
	//g_print ("%s (%d, %.2f, %.2f)\n", __func__, pDock->iMagnitudeIndex, pDock->fHideOffset, pDock->fPostHideOffset);
	
	if (pDock->fHideOffset < 1)  // the hiding animation is running.
	{
		pDock->fHideOffset += 1./myBackendsParam.iHideNbSteps;
		if (pDock->fHideOffset > .99)  // fin d'anim.
		{
			pDock->fHideOffset = 1;
			
			//g_print ("on arrete le cachage\n");
			gboolean bVisibleIconsPresent = FALSE;
			Icon *pIcon;
			GList *ic;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				pIcon = ic->data;
				if (pIcon->fInsertRemoveFactor != 0)  // on accelere l'animation d'apparition/disparition.
				{
					if (pIcon->fInsertRemoveFactor > 0)
						pIcon->fInsertRemoveFactor = 0.05;
					else
						pIcon->fInsertRemoveFactor = - 0.05;
				}
				
				if (! pIcon->bIsDemandingAttention && ! pIcon->bAlwaysVisible && ! pIcon->bIsLaunching)
					gldi_icon_stop_animation (pIcon);  // s'il y'a une autre animation en cours, on l'arrete.
				else
					bVisibleIconsPresent = TRUE;
			}
			
			pDock->pRenderer->calculate_icons (pDock);
			///pDock->fFoldingFactor = (myBackendsParam.bAnimateOnAutoHide ? .99 : 0.);  // on arme le depliage.
			cairo_dock_allow_entrance (pDock);
			
			gldi_dialogs_replace_all ();
			
			if (bVisibleIconsPresent)  // il y'a des icones a montrer progressivement, on reste dans la boucle.
			{
				pDock->fPostHideOffset = 0.05;
				return TRUE;
			}
			else
			{
				pDock->fPostHideOffset = 1;  // pour que les icones demandant l'attention plus tard soient visibles.
				return FALSE;
			}
		}
	}
	else if (pDock->fPostHideOffset > 0 && pDock->fPostHideOffset < 1)  // the post-hiding animation is running.
	{
		pDock->fPostHideOffset += 1./myBackendsParam.iHideNbSteps;
		if (pDock->fPostHideOffset > .99)
		{
			pDock->fPostHideOffset = 1.;
			return FALSE;
		}
	}
	else  // else no hiding animation is running.
		return FALSE;
	return TRUE;
}

static gboolean _cairo_dock_show (CairoDock *pDock)
{
	pDock->fHideOffset -= 1./myBackendsParam.iUnhideNbSteps;
	if (pDock->fHideOffset < 0.01)
	{
		pDock->fHideOffset = 0;
		cairo_dock_allow_entrance (pDock);
		gldi_dialogs_replace_all ();  // we need it here so that a modal dialog is replaced when the dock unhides (else it would stay behind).
		return FALSE;
	}
	return TRUE;
}

static gboolean _cairo_dock_handle_inserting_removing_icons (CairoDock *pDock)
{
	gboolean bRecalculateIcons = FALSE;
	GList* ic = pDock->icons, *next_ic;
	Icon *pIcon;
	while (ic != NULL)
	{
		pIcon = ic->data;
		if (pIcon->fInsertRemoveFactor == (gdouble)0.05)  // end of removal animation -> the icon will be detached (at least)
		{
			GList *prev_ic = ic->prev;
			Icon *pPrevIcon = (prev_ic ? prev_ic->data : NULL);
			if (GLDI_OBJECT_IS_AUTO_SEPARATOR_ICON (pPrevIcon))  // this icon will maybe disappear with pIcon, so take the previous one
				prev_ic = prev_ic->prev;
			
			gboolean bIsAppli = CAIRO_DOCK_IS_NORMAL_APPLI (pIcon);
			if (bIsAppli)  // it's a valid appli icon that hides itself (for instance, because the window was unminimized with bHideVisibleApplis=true) => just detach it
			{
				cd_message ("cette appli (%s) est toujours valide, on la detache juste", pIcon->cName);
				pIcon->fInsertRemoveFactor = 0.;  // on le fait avant le reload, sinon l'icone n'est pas rechargee.
				if (!pIcon->pAppli->bIsHidden && myTaskbarParam.bHideVisibleApplis)  // on lui remet l'image normale qui servira d'embleme lorsque l'icone sera inseree a nouveau dans le dock.
					cairo_dock_reload_icon_image (pIcon, CAIRO_CONTAINER (pDock));
				pDock = gldi_appli_icon_detach (pIcon);
				if (pDock == NULL)  // the dock has been destroyed (empty class sub-dock).
				{
					return FALSE;
				}
			}
			else
			{
				cd_message (" - %s va etre supprimee", pIcon->cName);
				
				gldi_icon_detach (pIcon);
				if (pIcon->cClass != NULL && pDock == cairo_dock_get_class_subdock (pIcon->cClass))  // appli icon in its class sub-dock => destroy the class sub-dock if it becomes empty (we don't want an empty sub-dock).
				{
					gboolean bEmptyClassSubDock = cairo_dock_check_class_subdock_is_empty (pDock, pIcon->cClass);
					if (bEmptyClassSubDock)
					{
						gldi_object_unref (GLDI_OBJECT (pIcon));
						return FALSE;
					}
				}
				
				gldi_object_delete (GLDI_OBJECT(pIcon));
			}
			next_ic = (prev_ic ? prev_ic->next : pDock->icons);
		}
		else
		{
			if (pIcon->fInsertRemoveFactor == (gdouble)-0.05)  // end of appearance animation
			{
				pIcon->fInsertRemoveFactor = 0;  // cela n'arrete pas l'animation, qui peut se poursuivre meme apres que l'icone ait atteint sa taille maximale.
				bRecalculateIcons = TRUE;
			}
			else if (pIcon->fInsertRemoveFactor != 0)  // currently (dis)appearing
			{
				bRecalculateIcons = TRUE;
			}
			next_ic = ic->next;
		}
		ic = next_ic;
	}
	
	if (bRecalculateIcons)
		cairo_dock_calculate_dock_icons (pDock);
	return TRUE;
}

static gboolean _cairo_dock_dock_animation_loop (GldiContainer *pContainer)
{
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	gboolean bContinue = FALSE;
	gboolean bUpdateSlowAnimation = FALSE;
	pContainer->iAnimationStep ++;
	if (pContainer->iAnimationStep * pContainer->iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pContainer->iAnimationStep = 0;
		pContainer->bKeepSlowAnimation = FALSE;
	}
	
	if (pDock->bIsShrinkingDown)
	{
		pDock->bIsShrinkingDown = _cairo_dock_shrink_down (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		bContinue |= pDock->bIsShrinkingDown;
	}
	if (pDock->bIsGrowingUp)
	{
		pDock->bIsGrowingUp = _cairo_dock_grow_up (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		bContinue |= pDock->bIsGrowingUp;
	}
	if (pDock->bIsHiding)
	{
		//g_print ("le dock se cache\n");
		pDock->bIsHiding = _cairo_dock_hide (pDock);
		gtk_widget_queue_draw (pContainer->pWidget);  // on n'utilise pas cairo_dock_redraw_container, sinon a la derniere iteration, le dock etant cache, la fonction ne le redessine pas.
		bContinue |= pDock->bIsHiding;
	}
	if (pDock->bIsShowing)
	{
		pDock->bIsShowing = _cairo_dock_show (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		bContinue |= pDock->bIsShowing;
	}
	//g_print (" => %d, %d\n", pDock->bIsShrinkingDown, pDock->bIsGrowingUp);
	
	double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
	gboolean bIconIsAnimating;
	gboolean bNoMoreDemandingAttention = FALSE;
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		
		icon->fDeltaYReflection = 0;
		if (myIconsParam.fAlphaAtRest != 1)
			icon->fAlpha = fDockMagnitude + myIconsParam.fAlphaAtRest * (1 - fDockMagnitude);
		
		bIconIsAnimating = FALSE;
		if (bUpdateSlowAnimation)
		{
			gldi_object_notify (icon, NOTIFICATION_UPDATE_ICON_SLOW, icon, pDock, &bIconIsAnimating);
			pContainer->bKeepSlowAnimation |= bIconIsAnimating;
		}
		gldi_object_notify (icon, NOTIFICATION_UPDATE_ICON, icon, pDock, &bIconIsAnimating);
		
		if ((icon->bIsDemandingAttention || icon->bAlwaysVisible) && cairo_dock_is_hidden (pDock))  // animation d'une icone demandant l'attention dans un dock cache => on force le dessin qui normalement ne se fait pas.
		{
			gtk_widget_queue_draw (pContainer->pWidget);
		}
		
		bContinue |= bIconIsAnimating;
		if (! bIconIsAnimating)
		{
			icon->iAnimationState = CAIRO_DOCK_STATE_REST;
			if (icon->bIsDemandingAttention)
			{
				icon->bIsDemandingAttention = FALSE;  // the attention animation has finished by itself after the time it was planned for.
				bNoMoreDemandingAttention = TRUE;
			}
		}
	}
	bContinue |= pContainer->bKeepSlowAnimation;
	
	if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && bNoMoreDemandingAttention && ! pDock->bIsBelow && ! pContainer->bInside)
	{
		//g_print ("plus de raison d'etre devant\n");
		cairo_dock_pop_down (pDock);
	}
	
	if (! _cairo_dock_handle_inserting_removing_icons (pDock))
	{
		cd_debug ("ce dock n'a plus de raison d'etre");
		return FALSE;
	}
	
	if (bUpdateSlowAnimation)
	{
		gldi_object_notify (pDock, NOTIFICATION_UPDATE_SLOW, pDock, &pContainer->bKeepSlowAnimation);
	}
	gldi_object_notify (pDock, NOTIFICATION_UPDATE, pDock, &bContinue);
	
	if (! bContinue && ! pContainer->bKeepSlowAnimation)
	{
		pContainer->iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}

static gboolean _on_dock_destroyed (GtkWidget *menu, GldiContainer *pContainer);
static void _on_menu_deactivated (G_GNUC_UNUSED GtkMenuShell *menu, CairoDock *pDock)
{
	//g_print ("\n+++ %s ()\n\n", __func__);
	g_return_if_fail (CAIRO_DOCK_IS_DOCK (pDock));
	if (pDock->bHasModalWindow)  // don't send the signal if the menu was already deactivated.
	{
		pDock->bHasModalWindow = FALSE;
		cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
	}
}
static void _on_menu_destroyed (GtkWidget *menu, CairoDock *pDock)
{
	//g_print ("\n+++ %s ()\n\n", __func__);
	gldi_object_remove_notification (pDock,
		NOTIFICATION_DESTROY,
		(GldiNotificationFunc) _on_dock_destroyed,
		menu);
}
static gboolean _on_dock_destroyed (GtkWidget *menu, GldiContainer *pContainer)
{
	//g_print ("\n+++ %s ()\n\n", __func__);
	g_signal_handlers_disconnect_matched
		(menu,
		G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
		0,
		0,
		NULL,
		_on_menu_deactivated,
		pContainer);
	g_signal_handlers_disconnect_matched
		(menu,
		G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
		0,
		0,
		NULL,
		_on_menu_destroyed,
		pContainer);
	return GLDI_NOTIFICATION_LET_PASS;
}
static void _setup_menu (GldiContainer *pContainer, G_GNUC_UNUSED Icon *pIcon, GtkWidget *pMenu)
{
	// keep the dock visible
	CAIRO_DOCK (pContainer)->bHasModalWindow = TRUE;
	
	// connect signals
	if (g_signal_handler_find (pMenu,
		G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
		0,
		0,
		NULL,
		_on_menu_deactivated,
		pContainer) == 0)  // on evite de connecter 2 fois ce signal, donc la fonction est appelable plusieurs fois sur un meme menu.
	{
		// when the menu is deactivated, hide the dock back if necessary.
		g_signal_connect (G_OBJECT (pMenu),
			"deactivate",
			G_CALLBACK (_on_menu_deactivated),
			pContainer);
		// when the menu is destroyed, remove the 'destroyed' notification on the dock.
		g_signal_connect (G_OBJECT (pMenu),
			"destroy",
			G_CALLBACK (_on_menu_destroyed),
			pContainer);
		// when the dock is destroyed, remove the 2 signals on the menu.
		gldi_object_register_notification (pContainer,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_dock_destroyed,
			GLDI_RUN_AFTER, pMenu);  // the menu can stay alive even if the container disappear, so we need to ensure we won't call the callbacks then.
	}
}

static gboolean _destroy_empty_dock (CairoDock *pDock)
{
	if (pDock->bIconIsFlyingAway)  // keep the dock alive for now, in case the user re-inserts the flying icon in it.
		return TRUE;
	pDock->iSidDestroyEmptyDock = 0;
	if (pDock->icons == NULL && pDock->iRefCount == 0 && ! pDock->bIsMainDock)  // le dock est toujours a detruire.
	{
		cd_debug ("The dock '%s' is empty. No icon, no dock.", pDock->cDockName);
		gldi_object_unref (GLDI_OBJECT(pDock));
	}
	return FALSE;
}
static void _detach_icon (GldiContainer *pContainer, Icon *icon)
{
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	cd_debug ("%s (%s)", __func__, icon->cName);
	
	//\___________________ On trouve l'icone et ses 2 voisins.
	GList *prev_ic = NULL, *ic, *next_ic;
	Icon *pPrevIcon = NULL, *pNextIcon = NULL;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		if (ic->data == icon)
		{
			prev_ic = ic->prev;
			next_ic = ic->next;
			if (prev_ic)
				pPrevIcon = prev_ic->data;
			if (next_ic)
				pNextIcon = next_ic->data;
			break;
		}
	}
	g_return_if_fail (ic != NULL);  // not found (shouldn't happen)
	
	//\___________________ On stoppe ses animations.
	gldi_icon_stop_animation (icon);
	
	//\___________________ On desactive sa miniature.
	if (icon->pAppli != NULL)
	{
		//cd_debug ("on desactive la miniature de %s (Xid : %lx)", icon->cName, icon->Xid);
		gldi_window_set_thumbnail_area (icon->pAppli, 0, 0, 0, 0);
	}
	
	//\___________________ On l'enleve de la liste.
	pDock->icons = g_list_delete_link (pDock->icons, ic);
	ic = NULL;
	pDock->fFlatDockWidth -= icon->fWidth + myIconsParam.iIconGap;
	
	//\___________________ On enleve le separateur si c'est la derniere icone de son type.
	if (! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
	{
		if ((pPrevIcon == NULL || CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pPrevIcon)) && CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pNextIcon))
		{
			pDock->icons = g_list_delete_link (pDock->icons, next_ic);  // optimisation
			next_ic = NULL;
			pDock->fFlatDockWidth -= pNextIcon->fWidth + myIconsParam.iIconGap;
			cairo_dock_set_icon_container (pNextIcon, NULL);
			gldi_object_unref (GLDI_OBJECT (pNextIcon));
			pNextIcon = NULL;
		}
		if ((pNextIcon == NULL || CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pNextIcon)) && CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pPrevIcon))
		{
			pDock->icons = g_list_delete_link (pDock->icons, prev_ic);  // optimisation
			prev_ic = NULL;
			pDock->fFlatDockWidth -= pPrevIcon->fWidth + myIconsParam.iIconGap;
			cairo_dock_set_icon_container (pPrevIcon, NULL);
			gldi_object_unref (GLDI_OBJECT (pPrevIcon));
			pPrevIcon = NULL;
		}
	}
	
	//\___________________ Cette icone realisait peut-etre le max des hauteurs, comme on l'enleve on recalcule ce max.
	Icon *pOtherIcon;
	if (icon->fHeight >= pDock->iMaxIconHeight)
	{
		pDock->iMaxIconHeight = 0;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			pOtherIcon = ic->data;
			if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pOtherIcon))
			{
				pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, pOtherIcon->fHeight);
				if (pOtherIcon->fHeight == icon->fHeight)  // on sait qu'on n'ira pas plus haut.
					break;
			}
		}
	}

	//\___________________ On la remet a la taille normale en vue d'une reinsertion quelque part.
	icon->fWidth /= pDock->container.fRatio;
	icon->fHeight /= pDock->container.fRatio;
	
	//\___________________ On prevoit le redessin de l'icone pointant sur le sous-dock.
	if (pDock->iRefCount != 0 && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		cairo_dock_trigger_redraw_subdock_content (pDock);
	}
	
	//\___________________ On prevoit la destruction du dock si c'est un dock principal qui devient vide.
	if (pDock->iRefCount == 0 && pDock->icons == NULL && ! pDock->bIsMainDock)  // on supprime les docks principaux vides.
	{
		if (pDock->iSidDestroyEmptyDock == 0)
			pDock->iSidDestroyEmptyDock = g_idle_add ((GSourceFunc) _destroy_empty_dock, pDock);
	}
	else
	{
		cairo_dock_trigger_update_dock_size (pDock);
	}
	
	//\___________________ Notify everybody.
	icon->fInsertRemoveFactor = 0.;
	gldi_object_notify (pDock, NOTIFICATION_REMOVE_ICON, icon, pDock);
	
	//\___________________ unset the container, now that it's completely detached from it.
	g_free (icon->cParentDockName);
	icon->cParentDockName = NULL;
}

static void _insert_icon (GldiContainer *pContainer, Icon *icon, gboolean bAnimateIcon)
{
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	
	cd_debug ("insert %s in %s", icon->cName, gldi_dock_get_name (pDock));
	
	if (icon->cParentDockName == NULL)
		icon->cParentDockName = g_strdup (gldi_dock_get_name (pDock));

	//\______________ check if a separator is needed (ie, if the group of the new icon (not its order) is new).
	gboolean bSeparatorNeeded = FALSE;
	if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		Icon *pSameTypeIcon = cairo_dock_get_first_icon_of_group (pDock->icons, icon->iGroup);
		if (pSameTypeIcon == NULL && pDock->icons != NULL)
		{
			bSeparatorNeeded = TRUE;
		}
	}

	//\______________ insert the icon in the list.
	if (icon->fOrder == CAIRO_DOCK_LAST_ORDER)
	{
		Icon *pLastIcon = cairo_dock_get_last_icon_of_order (pDock->icons, icon->iGroup);
		if (pLastIcon != NULL)
			icon->fOrder = pLastIcon->fOrder + 1;
		else
			icon->fOrder = 1;
	}
	
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon,
		(GCompareFunc)cairo_dock_compare_icons_order);
	
	//\______________ set the icon size, now that it's inside a container.
	int wi = icon->image.iWidth, hi = icon->image.iHeight;
	cairo_dock_set_icon_size_in_dock (pDock, icon);
	
	if (wi != cairo_dock_icon_get_allocated_width (icon) || hi != cairo_dock_icon_get_allocated_height (icon)  // if size has changed, reload the buffers
	|| (! icon->image.pSurface && ! icon->image.iTexture))  // might happen, for instance if the icon is a launcher pinned on a desktop and was detached before being loaded.
		cairo_dock_trigger_load_icon_buffers (icon);
	
	pDock->fFlatDockWidth += myIconsParam.iIconGap + icon->fWidth;
	if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);
	
	//\______________ insert a separator if needed.
	if (bSeparatorNeeded)
	{
		// insert a separator after if needed
		Icon *pNextIcon = cairo_dock_get_next_icon (pDock->icons, icon);
		if (pNextIcon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pNextIcon))
		{
			Icon *pSeparatorIcon = gldi_auto_separator_icon_new (icon, pNextIcon);
			gldi_icon_insert_in_container (pSeparatorIcon, CAIRO_CONTAINER(pDock), ! CAIRO_DOCK_ANIMATE_ICON);
		}
		
		// insert a separator before if needed
		Icon *pPrevIcon = cairo_dock_get_previous_icon (pDock->icons, icon);
		if (pPrevIcon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pPrevIcon))
		{
			Icon *pSeparatorIcon = gldi_auto_separator_icon_new (pPrevIcon, icon);
			gldi_icon_insert_in_container (pSeparatorIcon, CAIRO_CONTAINER(pDock), ! CAIRO_DOCK_ANIMATE_ICON);
		}
	}
	
	//\______________ On effectue les actions demandees.
	if (bAnimateIcon)
	{
		if (cairo_dock_animation_will_be_visible (pDock))
			icon->fInsertRemoveFactor = - 0.95;
		else
			icon->fInsertRemoveFactor = - 0.05;
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
	else
		icon->fInsertRemoveFactor = 0.;
	
	cairo_dock_trigger_update_dock_size (pDock);
	
	if (pDock->iRefCount != 0 && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))  // on prevoit le redessin de l'icone pointant sur le sous-dock.
	{
		cairo_dock_trigger_redraw_subdock_content (pDock);
	}
	
	if (icon->pSubDock != NULL)
		gldi_subdock_synchronize_orientation (icon->pSubDock, pDock, FALSE);
	
	//\______________ Notify everybody.
	gldi_object_notify (pDock, NOTIFICATION_INSERT_ICON, icon, pDock);  /// TODO: make it a Container notification...
}

void gldi_dock_init_internals (CairoDock *pDock)
{
	pDock->container.iface.animation_loop = _cairo_dock_dock_animation_loop;
	pDock->container.iface.setup_menu = _setup_menu;
	pDock->container.iface.detach_icon = _detach_icon;
	pDock->container.iface.insert_icon = _insert_icon;
	
	//\__________________ set up its window
	GtkWidget *pWindow = pDock->container.pWidget;
	gtk_container_set_border_width (GTK_CONTAINER (pWindow), 0);
	gtk_window_set_gravity (GTK_WINDOW (pWindow), GDK_GRAVITY_STATIC);
	gtk_window_set_type_hint (GTK_WINDOW (pWindow), GDK_WINDOW_TYPE_HINT_DOCK);  // window must not be mapped
	
	//\__________________ connect to events.
	gtk_widget_add_events (pWindow,
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK |
		GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
		GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	
	g_signal_connect (G_OBJECT (pWindow),
		"draw",
		G_CALLBACK (_on_expose),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (_on_configure),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"key-release-event",
		G_CALLBACK (_on_key_release),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"key-press-event",
		G_CALLBACK (_on_key_release),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-press-event",
		G_CALLBACK (_on_button_press),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-release-event",
		G_CALLBACK (_on_button_press),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"scroll-event",
		G_CALLBACK (_on_scroll),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"motion-notify-event",
		G_CALLBACK (_on_motion_notify),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"enter-notify-event",
		G_CALLBACK (_on_enter_notify),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"leave-notify-event",
		G_CALLBACK (_on_leave_notify),
		pDock);
	gldi_container_enable_drop (CAIRO_CONTAINER (pDock),
		G_CALLBACK (_on_drag_data_received),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag-motion",
		G_CALLBACK (_on_drag_motion),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag-leave",
		G_CALLBACK (_on_drag_leave),
		pDock);
	/*g_signal_connect (G_OBJECT (pWindow),
		"drag-drop",
		G_CALLBACK (_on_drag_drop),
		pDock);*/
	// connect unmap signal -- on Wayland, the compositor might close the dock at any time
	if (gldi_container_is_wayland_backend ())
		g_signal_connect (G_OBJECT (pWindow),
			"unmap-event",
			G_CALLBACK (_on_dock_unmap),
			pDock);
	
	gtk_widget_show_all (pDock->container.pWidget);
}


  ///////////////
 /// FACTORY ///
///////////////

CairoDock *gldi_dock_new (const gchar *cDockName)
{
	CairoDockAttr attr;
	memset (&attr, 0, sizeof (CairoDockAttr));
	attr.cDockName = cDockName;
	return (CairoDock*)gldi_object_new (&myDockObjectMgr, &attr);
}

CairoDock *gldi_subdock_new (const gchar *cDockName, const gchar *cRendererName, CairoDock *pParentDock, GList *pIconList)
{
	CairoDockAttr attr;
	memset (&attr, 0, sizeof (CairoDockAttr));
	attr.bSubDock = TRUE;
	attr.cDockName = cDockName;
	attr.cRendererName = cRendererName;
	attr.pParentDock = pParentDock;
	attr.pIconList = pIconList;
	attr.cattr.bIsPopup = TRUE;
	return (CairoDock*)gldi_object_new (&myDockObjectMgr, &attr);
}


void cairo_dock_remove_icons_from_dock (CairoDock *pDock, CairoDock *pReceivingDock)
{
	g_return_if_fail (pReceivingDock != NULL);
	GList *pIconsList = pDock->icons;
	pDock->icons = NULL;
	Icon *icon;
	GList *ic;
	for (ic = pIconsList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		
		cairo_dock_set_icon_container (icon, NULL);  // manually detach the icon
		
		gldi_theme_icon_write_container_name_in_conf_file (icon, pReceivingDock->cDockName);
		
		cd_debug (" on re-attribue %s au dock %s", icon->cName, pReceivingDock->cDockName);
		gldi_icon_insert_in_container (icon, CAIRO_CONTAINER(pReceivingDock), CAIRO_DOCK_ANIMATE_ICON);
		
		if (CAIRO_DOCK_IS_APPLET (icon))
		{
			icon->pModuleInstance->pContainer = CAIRO_CONTAINER (pReceivingDock);  // astuce pour ne pas avoir a recharger le fichier de conf ^_^
			icon->pModuleInstance->pDock = pReceivingDock;
			gldi_object_reload (GLDI_OBJECT(icon->pModuleInstance), FALSE);
		}
		else if (cairo_dock_get_icon_data_renderer (icon) != NULL)
			cairo_dock_reload_data_renderer_on_icon (icon, CAIRO_CONTAINER (pReceivingDock));
	}

	g_list_free (pIconsList);
}


void cairo_dock_reload_buffers_in_dock (CairoDock *pDock, gboolean bRecursive, gboolean bUpdateIconSize)
{
	//g_print ("************%s (%d, %d)\n", __func__, pDock->bIsMainDock, bRecursive);
	if (bUpdateIconSize && pDock->bGlobalIconSize)
		pDock->iIconSize = myIconsParam.iIconWidth;
	
	// for each icon, reload its buffer (size may change).
	Icon* icon;
	GList* ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		
		if (CAIRO_DOCK_IS_APPLET (icon))  // for an applet, we need to let the module know that the size or the theme has changed, so that it can reload its private buffers.
		{
			gldi_object_reload (GLDI_OBJECT(icon->pModuleInstance), FALSE);
		}
		else
		{
			if (bUpdateIconSize)
			{
				cairo_dock_icon_set_requested_size (icon, 0, 0);
				cairo_dock_set_icon_size_in_dock (pDock, icon);
			}
			
			if (bUpdateIconSize && cairo_dock_get_icon_data_renderer (icon) != NULL)  // we need to reload the DataRenderer to use the new size
			{
				cairo_dock_load_icon_buffers (icon, CAIRO_CONTAINER (pDock));  // the DataRenderer uses the ImageBuffer's size on loading, so we need to load it now
				cairo_dock_reload_data_renderer_on_icon (icon, CAIRO_CONTAINER (pDock));
			}
			else
			{
				cairo_dock_trigger_load_icon_buffers (icon);
			}
		}
		
		if (bRecursive && icon->pSubDock != NULL)  // we handle the sub-dock for applets too, so that they don't need to care.
		{
			if (bUpdateIconSize)
				icon->pSubDock->iIconSize = pDock->iIconSize;
			cairo_dock_reload_buffers_in_dock (icon->pSubDock, bRecursive, bUpdateIconSize);
		}
	}
	
	if (bUpdateIconSize)
	{
		cairo_dock_update_dock_size (pDock);
	}
	gtk_widget_queue_draw (pDock->container.pWidget);
}


void cairo_dock_set_icon_size_in_dock (CairoDock *pDock, Icon *icon)
{
	if (pDock->pRenderer && pDock->pRenderer->set_icon_size)  // the view wants to decide the icons size.
	{
		pDock->pRenderer->set_icon_size (icon, pDock);
	}
	else  // generic method: icon extent = base size * max zoom
	{
		int wi, hi;  // icon size (icon size displayed at rest, as defined in the config)
		int wa, ha;  // allocated size (surface/texture).
		gboolean bIsHorizontal = (pDock->container.bIsHorizontal || (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) && myIconsParam.bRevolveSeparator));
		
		// get the displayed icon size as defined in the config
		if (! pDock->bGlobalIconSize && pDock->iIconSize != 0)
		{
			wi = hi = pDock->iIconSize;
		}
		else  // same size as main dock.
		{
			wi = myIconsParam.iIconWidth;
			hi = myIconsParam.iIconHeight;
		}
		
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))  // separators have their own size.
		{
			wi = myIconsParam.iSeparatorWidth;
			hi = MIN (myIconsParam.iSeparatorHeight, hi);
		}
		
		// take into account the requested displayed size if any
		int wir = cairo_dock_icon_get_requested_display_width (icon);
		if (wir != 0)
			wi = wir;
		int hir = cairo_dock_icon_get_requested_display_height (icon);
		if (hir != 0)
			hi = MIN (hir, hi);  // limit the icon height to the default height.
		
		// get the requested size if any
		wa = cairo_dock_icon_get_requested_width (icon);
		ha = cairo_dock_icon_get_requested_height (icon);
		
		// compute the missing size (allocated or displayed).
		double fMaxScale = 1 + myIconsParam.fAmplitude;
		if (wa == 0)
		{
			wa = (bIsHorizontal ? wi : hi) * fMaxScale;
		}
		else
		{
			if (bIsHorizontal)
				wi = wa / fMaxScale;
			else
				hi = wa / fMaxScale;
		}
		if (ha == 0)
		{
			ha = (bIsHorizontal ? hi : wi) * fMaxScale;
		}
		else
		{
			if (bIsHorizontal)
				hi = ha / fMaxScale;
			else
				wi = ha / fMaxScale;
		}
		
		// set both allocated and displayed size 
		cairo_dock_icon_set_allocated_size (icon, wa, ha);
		icon->fWidth = wi;
		icon->fHeight = hi;
	}
	// take into account the current ratio
	icon->fWidth *= pDock->container.fRatio;
	icon->fHeight *= pDock->container.fRatio;
}


void cairo_dock_create_redirect_texture_for_dock (CairoDock *pDock)
{
	if (! g_openglConfig.bFboAvailable)
		return ;
	if (pDock->iRedirectedTexture == 0)
	{
		pDock->iRedirectedTexture = cairo_dock_create_texture_from_raw_data (NULL,
			(pDock->container.bIsHorizontal ? pDock->container.iWidth : pDock->container.iHeight),
			(pDock->container.bIsHorizontal ? pDock->container.iHeight : pDock->container.iWidth));
	}
	if (pDock->iFboId == 0)
		glGenFramebuffersEXT(1, &pDock->iFboId);
}

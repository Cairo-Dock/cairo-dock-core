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
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <cairo.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif
#include <gtk/gtkgl.h>
#include <GL/glu.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-load.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-config.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-callbacks.h"

extern CairoDockDesktopGeometry g_desktopGeometry;
extern CairoDockHidingEffect *g_pHidingBackend;

static Icon *s_pIconClicked = NULL;  // pour savoir quand on deplace une icone a la souris. Dangereux si l'icone se fait effacer en cours ...
static int s_iClickX, s_iClickY;  // coordonnees du clic dans le dock, pour pouvoir initialiser le deplacement apres un seuil.
static CairoDock *s_pLastPointedDock = NULL;  // pour savoir quand on passe d'un dock a un autre.
static int s_iSidShowSubDockDemand = 0;
static int s_iSidActionOnDragHover = 0;
static CairoDock *s_pDockShowingSubDock = NULL;  // on n'accede pas a son contenu, seulement l'adresse.
static CairoFlyingContainer *s_pFlyingContainer = NULL;

extern CairoDock *g_pMainDock;

extern gboolean g_bUseOpenGL;
extern gboolean g_bLocked;

static gboolean s_bHideAfterShortcut = FALSE;
static gboolean s_bFrozenDock = FALSE;
static gboolean s_bIconDragged = FALSE;

#define _xy_is_really_outside(x, y, pDock) (pDock->container.bIsHorizontal ? (x < 0 || x >= pDock->container.iWidth || y < 0 || y >= pDock->container.iHeight) : (y < 0 || y >= pDock->container.iWidth || x < 0 || x >= pDock->container.iHeight))
#define _mouse_is_really_outside(pDock) (pDock->container.iMouseX <= 0 || pDock->container.iMouseX >= pDock->container.iWidth || pDock->container.iMouseY <= 0 || pDock->container.iMouseY >= pDock->container.iHeight)
#define CD_CLICK_ZONE 5

void cairo_dock_freeze_docks (gboolean bFreeze)
{
	s_bFrozenDock = bFreeze;
}

gboolean cairo_dock_render_dock_notification (gpointer pUserData, CairoDock *pDock, cairo_t *pCairoContext)
{
	if (! pCairoContext)  // on n'a pas mis le rendu cairo ici a cause du rendu optimise.
	{
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (pDock->pRenderer->bUseStencil ? GL_STENCIL_BUFFER_BIT : 0));
		
		cairo_dock_apply_desktop_background_opengl (CAIRO_CONTAINER (pDock));
		
		if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->pre_render_opengl)
			g_pHidingBackend->pre_render_opengl (pDock);
		
		pDock->pRenderer->render_opengl (pDock);
		
		if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->post_render_opengl)
			g_pHidingBackend->post_render_opengl (pDock);
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_on_expose (GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	CairoDock *pDock)
{
	//g_print ("%s ((%d;%d) %dx%d)\n", __func__, pExpose->area.x, pExpose->area.y, pExpose->area.width, pExpose->area.height);
	if (g_bUseOpenGL && pDock->pRenderer->render_opengl != NULL)
	{
		GdkGLContext *pGlContext = gtk_widget_get_gl_context (pDock->container.pWidget);
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pDock->container.pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return FALSE;
		
		glLoadIdentity ();
		
		if (pExpose->area.x + pExpose->area.y != 0)
		{
			glEnable (GL_SCISSOR_TEST);  // ou comment diviser par 4 l'occupation CPU !
			glScissor ((int) pExpose->area.x,
				(int) (pDock->container.bIsHorizontal ? pDock->container.iHeight : pDock->container.iWidth) -
					pExpose->area.y - pExpose->area.height,  // lower left corner of the scissor box.
				(int) pExpose->area.width,
				(int) pExpose->area.height);
		}
		
		if (cairo_dock_is_loading ())
		{
			// on laisse transparent
		}
		else if (cairo_dock_is_hidden (pDock) && (g_pHidingBackend == NULL || !g_pHidingBackend->bCanDisplayHiddenDock))
		{
			cairo_dock_render_hidden_dock_opengl (pDock);
		}
		else
		{
			cairo_dock_notify_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_RENDER_DOCK, pDock, NULL);
		}
		glDisable (GL_SCISSOR_TEST);
		
		if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
			gdk_gl_drawable_swap_buffers (pGlDrawable);
		else
			glFlush ();
		gdk_gl_drawable_gl_end (pGlDrawable);
		
		return FALSE ;
	}
	
	if (pExpose->area.x + pExpose->area.y != 0)  // x et/ou y sont > 0.
	{
		//if (! cairo_dock_is_hidden (pDock) || (g_pHidingBackend != NULL && g_pHidingBackend->bCanDisplayHiddenDock))
		{
			cairo_t *pCairoContext = cairo_dock_create_drawing_context_on_area (CAIRO_CONTAINER (pDock), &pExpose->area, NULL);
			
			if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->pre_render)
				g_pHidingBackend->pre_render (pDock, pCairoContext);
			
			if (pDock->pRenderer->render_optimized != NULL)
				pDock->pRenderer->render_optimized (pCairoContext, pDock, &pExpose->area);
			else
				pDock->pRenderer->render (pCairoContext, pDock);
			
			if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->post_render)
				g_pHidingBackend->post_render (pDock, pCairoContext);
			
			cairo_dock_notify_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_RENDER_DOCK, pDock, pCairoContext);
			
			cairo_destroy (pCairoContext);
		}
		return FALSE;
	}
	
	
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_on_container (CAIRO_CONTAINER (pDock));
	
	if (cairo_dock_is_loading ())  // transparent pendant le chargement.
	{
		
	}
	else if (cairo_dock_is_hidden (pDock) && (g_pHidingBackend == NULL || !g_pHidingBackend->bCanDisplayHiddenDock))
	{
		cairo_dock_render_hidden_dock (pCairoContext, pDock);
	}
	else
	{
		if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->pre_render)
			g_pHidingBackend->pre_render (pDock, pCairoContext);
		
		pDock->pRenderer->render (pCairoContext, pDock);
		
		if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->post_render)
			g_pHidingBackend->post_render (pDock, pCairoContext);
		
		cairo_dock_notify_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_RENDER_DOCK, pDock, pCairoContext);
	}
	
	cairo_destroy (pCairoContext);
	
#ifdef HAVE_GLITZ
	if (pDock->container.pDrawFormat && pDock->container.pDrawFormat->doublebuffer)
		glitz_drawable_swap_buffers (pDock->container.pGlitzDrawable);
#endif
	return FALSE;
}


static gboolean _cairo_dock_show_sub_dock_delayed (CairoDock *pDock)
{
	s_iSidShowSubDockDemand = 0;
	s_pDockShowingSubDock = NULL;
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	//g_print ("%s (%x, %x)", __func__, icon, icon ? icon->pSubDock:0);
	if (icon != NULL && icon->pSubDock != NULL)
		cairo_dock_show_subdock (icon, pDock);

	return FALSE;
}
static void _search_icon (Icon *icon, CairoContainer *pContainer, gpointer *data)
{
	if (icon == data[0])
		data[1] = icon;
}
static gboolean _cairo_dock_action_on_drag_hover (Icon *pIcon)
{
	gpointer data[2] = {pIcon, NULL};
	cairo_dock_foreach_icons_in_docks ((CairoDockForeachIconFunc)_search_icon, data);  // on verifie que l'icone ne s'est pas faite effacee entre-temps?
	pIcon = data[1];
	if (pIcon && pIcon->action_on_drag_hover)
		pIcon->action_on_drag_hover (pIcon);
	s_iSidActionOnDragHover = 0;
	return FALSE;
}
void cairo_dock_on_change_icon (Icon *pLastPointedIcon, Icon *pPointedIcon, CairoDock *pDock)
{
	//g_print ("%s (%x;%x)\n", __func__, pLastPointedIcon, pPointedIcon);
	//cd_debug ("on change d'icone dans %x (-> %s)", pDock, (pPointedIcon != NULL ? pPointedIcon->cName : "rien"));
	if (s_iSidShowSubDockDemand != 0 && pDock == s_pDockShowingSubDock)
	{
		//cd_debug ("on annule la demande de montrage de sous-dock");
		g_source_remove (s_iSidShowSubDockDemand);
		s_iSidShowSubDockDemand = 0;
		s_pDockShowingSubDock = NULL;
	}
	
	if (s_iSidActionOnDragHover != 0)
	{
		//cd_debug ("on annule la demande de montrage d'appli");
		g_source_remove (s_iSidActionOnDragHover);
		s_iSidActionOnDragHover = 0;
	}
	cairo_dock_replace_all_dialogs ();
	if (pDock->bIsDragging && pPointedIcon && pPointedIcon->action_on_drag_hover)
	{
		s_iSidActionOnDragHover = g_timeout_add (600, (GSourceFunc) _cairo_dock_action_on_drag_hover, pPointedIcon);
	}
	
	//g_print ("%x/%x , %x, %x\n", pDock, s_pLastPointedDock, pLastPointedIcon, pLastPointedIcon?pLastPointedIcon->pSubDock:NULL);
	if ((pDock == s_pLastPointedDock || s_pLastPointedDock == NULL) && pLastPointedIcon != NULL && pLastPointedIcon->pSubDock != NULL)  // on a quitte une icone ayant un sous-dock.
	{
		CairoDock *pSubDock = pLastPointedIcon->pSubDock;
		if (GTK_WIDGET_VISIBLE (pSubDock->container.pWidget))
		{
			//g_print ("on cache %s en changeant d'icone\n", pLastPointedIcon->cName);
			if (pSubDock->iSidLeaveDemand == 0)
			{
				//g_print ("  on retarde le cachage du dock de %dms\n", MAX (myAccessibility.iLeaveSubDockDelay, 330));
				pSubDock->iSidLeaveDemand = g_timeout_add (MAX (myAccessibility.iLeaveSubDockDelay, 330), (GSourceFunc) cairo_dock_emit_leave_signal, (gpointer) pSubDock);
			}
		}
		//else
		//	cd_debug ("pas encore visible !\n");
	}
	if (pPointedIcon != NULL && pPointedIcon->pSubDock != NULL && pPointedIcon->pSubDock != s_pLastPointedDock && (! myAccessibility.bShowSubDockOnClick || CAIRO_DOCK_IS_APPLI (pPointedIcon) || pDock->bIsDragging))  // on entre sur une icone ayant un sous-dock.
	{
		//cd_debug ("il faut montrer un sous-dock");
		if (pPointedIcon->pSubDock->iSidLeaveDemand != 0)
		{
			g_source_remove (pPointedIcon->pSubDock->iSidLeaveDemand);
			pPointedIcon->pSubDock->iSidLeaveDemand = 0;
		}
		if (myAccessibility.iShowSubDockDelay > 0)
		{
			//pDock->container.iMouseX = iX;
			if (s_iSidShowSubDockDemand != 0)
				g_source_remove (s_iSidShowSubDockDemand);
			s_iSidShowSubDockDemand = g_timeout_add (myAccessibility.iShowSubDockDelay, (GSourceFunc) _cairo_dock_show_sub_dock_delayed, pDock);
			s_pDockShowingSubDock = pDock;
			//g_print ("s_iSidShowSubDockDemand <- %d\n", s_iSidShowSubDockDemand);
		}
		else
			cairo_dock_show_subdock (pPointedIcon, pDock);
		s_pLastPointedDock = pDock;
	}

	if (s_pLastPointedDock == NULL)
	{
		//g_print ("pLastPointedDock n'est plus null\n");
		s_pLastPointedDock = pDock;
	}
	if (pPointedIcon != NULL && pDock->pRenderer->render_opengl != NULL && ! CAIRO_DOCK_IS_SEPARATOR (pPointedIcon) && pPointedIcon->iAnimationState <= CAIRO_DOCK_STATE_MOUSE_HOVERED)
	{
		gboolean bStartAnimation = FALSE;
		cairo_dock_notify_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_ENTER_ICON, pPointedIcon, pDock, &bStartAnimation);
		
		if (bStartAnimation)
		{
			pPointedIcon->iAnimationState = CAIRO_DOCK_STATE_MOUSE_HOVERED;
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
		}
	}
}


void cairo_dock_stop_icon_glide (CairoDock *pDock)
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
			//g_print ("deplacement de %s vers la gauche (%.2f / %d)\n", icon->cName, icon->fDrawX + icon->fWidth * (1+myIcons.fAmplitude) + myIcons.iIconGap, pDock->container.iMouseX);
			if (icon->fXAtRest < pMovingicon->fXAtRest && icon->fDrawX + icon->fWidth * (1+myIcons.fAmplitude) + myIcons.iIconGap >= pDock->container.iMouseX && icon->fGlideOffset == 0)  // icone entre l'icone deplacee et le curseur.
			{
				//g_print ("  %s glisse vers la droite\n", icon->cName);
				icon->iGlideDirection = 1;
			}
			else if (icon->fXAtRest < pMovingicon->fXAtRest && icon->fDrawX + icon->fWidth * (1+myIcons.fAmplitude) + myIcons.iIconGap <= pDock->container.iMouseX && icon->fGlideOffset != 0)
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
gboolean cairo_dock_on_motion_notify (GtkWidget* pWidget,
	GdkEventMotion* pMotion,
	CairoDock *pDock)
{
	static double fLastTime = 0;
	if (s_bFrozenDock && pMotion != NULL && pMotion->time != 0)
		return FALSE;
	Icon *pPointedIcon=NULL, *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	int iLastMouseX = pDock->container.iMouseX;
	//g_print ("%s (%.2f;%.2f, %d)\n", __func__, pMotion->x, pMotion->y, pDock->iInputState);
	
	if (pMotion != NULL)
	{
		//g_print ("%s (%d,%d) (%d, %.2fms, bAtBottom:%d; bIsShrinkingDown:%d)\n", __func__, (int) pMotion->x, (int) pMotion->y, pMotion->is_hint, pMotion->time - fLastTime, pDock->bAtBottom, pDock->bIsShrinkingDown);
		//\_______________ On deplace le dock si ALT est enfoncee.
		if ((pMotion->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) && (pMotion->state & GDK_BUTTON1_MASK))
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
			cairo_dock_drag_flying_container (s_pFlyingContainer, pDock);
		}
		
		//\_______________ On elague le flux des MotionNotify, sinon X en envoie autant que le permet le CPU !
		if (pMotion->time != 0 && pMotion->time - fLastTime < mySystem.fRefreshInterval && s_pIconClicked == NULL)
		{
			gdk_device_get_state (pMotion->device, pMotion->window, NULL, NULL);
			return FALSE;
		}
		
		//\_______________ On recalcule toutes les icones et on redessine.
		pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
		gtk_widget_queue_draw (pWidget);
		fLastTime = pMotion->time;
		
		//\_______________ On tire l'icone cliquee.
		if (s_pIconClicked != NULL && s_pIconClicked->iAnimationState != CAIRO_DOCK_STATE_REMOVE_INSERT && ! g_bLocked && ! myAccessibility.bLockIcons && ! myAccessibility.bLockAll && (fabs (pMotion->x - s_iClickX) > CD_CLICK_ZONE || fabs (pMotion->y - s_iClickY) > CD_CLICK_ZONE))
		{
			s_bIconDragged = TRUE;
			s_pIconClicked->iAnimationState = CAIRO_DOCK_STATE_FOLLOW_MOUSE;
			//pDock->fAvoidingMouseMargin = .5;
			pDock->iAvoidingMouseIconType = s_pIconClicked->iType;  // on pourrait le faire lors du clic aussi.
			s_pIconClicked->fScale = 1 + myIcons.fAmplitude;
			s_pIconClicked->fDrawX = pDock->container.iMouseX  - s_pIconClicked->fWidth * s_pIconClicked->fScale / 2;
			s_pIconClicked->fDrawY = pDock->container.iMouseY - s_pIconClicked->fHeight * s_pIconClicked->fScale / 2 ;
			s_pIconClicked->fAlpha = 0.75;
			if (myIcons.fAmplitude == 0)
				gtk_widget_queue_draw (pWidget);
		}

		//gdk_event_request_motions (pMotion);  // ce sera pour GDK 2.12.
		gdk_device_get_state (pMotion->device, pMotion->window, NULL, NULL);  // pour recevoir d'autres MotionNotify.
	}
	else  // cas d'un drag and drop.
	{
		//g_print ("motion on drag\n");
		//\_______________ On recupere la position de la souris.
		if (pDock->container.bIsHorizontal)
 			gdk_window_get_pointer (pWidget->window, &pDock->container.iMouseX, &pDock->container.iMouseY, NULL);
		else
			gdk_window_get_pointer (pWidget->window, &pDock->container.iMouseY, &pDock->container.iMouseX, NULL);
		
		//\_______________ On recalcule toutes les icones et on redessine.
		pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
		gtk_widget_queue_draw (pWidget);
		
		pDock->fAvoidingMouseMargin = .25;  // on peut dropper entre 2 icones ...
		pDock->iAvoidingMouseIconType = CAIRO_DOCK_LAUNCHER;  // ... seulement entre 2 lanceurs.
	}
	
	//\_______________ On asservit les decorations au curseur.
	if (pDock->container.bInside)
	{
		if (myBackground.bDecorationsFollowMouse)
		{
			pDock->fDecorationsOffsetX = pDock->container.iMouseX - pDock->container.iWidth / 2;
			//g_print ("fDecorationsOffsetX <- %.2f\n", pDock->fDecorationsOffsetX);
		}
		else
		{
			if (pDock->container.iMouseX > iLastMouseX)
			{
				pDock->fDecorationsOffsetX += 10;
				if (pDock->fDecorationsOffsetX > pDock->container.iWidth / 2)
				{
					if (myBackground.cBackgroundImageFile && !myBackground.bBackgroundImageRepeat)
						pDock->fDecorationsOffsetX -= pDock->container.iWidth;
					else
						pDock->fDecorationsOffsetX = pDock->container.iWidth / 2;
				}
			}
			else
			{
				pDock->fDecorationsOffsetX -= 10;
				if (pDock->fDecorationsOffsetX < - pDock->container.iWidth / 2)
				{
					if (myBackground.cBackgroundImageFile && !myBackground.bBackgroundImageRepeat)
						pDock->fDecorationsOffsetX += pDock->container.iWidth;
					else
						pDock->fDecorationsOffsetX = - pDock->container.iWidth / 2;
				}
			}
		}
	}
	
	//\_______________ On gere le changement d'icone.
	gboolean bStartAnimation = FALSE;
	if (pPointedIcon != pLastPointedIcon || s_pLastPointedDock == NULL)
	{
		cairo_dock_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);
		
		if (pPointedIcon != NULL && s_pIconClicked != NULL && cairo_dock_get_icon_order (s_pIconClicked) == cairo_dock_get_icon_order (pPointedIcon) && ! g_bLocked && ! myAccessibility.bLockIcons && ! myAccessibility.bLockAll)
		{
			_cairo_dock_make_icon_glide (pPointedIcon, s_pIconClicked, pDock);
			bStartAnimation = TRUE;
		}
	}
	
	//\_______________ On notifie tout le monde.
	cairo_dock_notify_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_MOUSE_MOVED, pDock, &bStartAnimation);
	if (bStartAnimation)
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	
	return FALSE;
}

gboolean cairo_dock_emit_signal_on_dock (CairoDock *pDock, const gchar *cSignal)
{
	static gboolean bReturn;
	g_signal_emit_by_name (pDock->container.pWidget, cSignal, NULL, &bReturn);
	return FALSE;
}
gboolean cairo_dock_emit_leave_signal (CairoDock *pDock)
{
	return cairo_dock_emit_signal_on_dock (pDock, "leave-notify-event");
}
gboolean cairo_dock_emit_enter_signal (CairoDock *pDock)
{
	return cairo_dock_emit_signal_on_dock (pDock, "enter-notify-event");
}


gboolean cairo_dock_on_leave_notify (GtkWidget* pWidget, GdkEventCrossing* pEvent, CairoDock *pDock)
{
	//g_print ("%s (bInside:%d; iState:%d; iRefCount:%d)\n", __func__, pDock->container.bInside, pDock->iInputState, pDock->iRefCount);
	//\_______________ On tire le dock => on ignore le signal.
	if (pEvent != NULL && (pEvent->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) && (pEvent->state & GDK_BUTTON1_MASK))
	{
		return FALSE;
	}
	
	//\_______________ On ignore les signaux errones venant d'un WM buggue (Kwin).
	if (pEvent && !_xy_is_really_outside (pEvent->x, pEvent->y, pDock))  // ce test est la pour parer aux WM deficients mentaux comme KWin qui nous font sortir/rentrer lors d'un clic.
	{
		cd_debug ("not really outside (%d;%d)\n", (int)pEvent->x, (int)pEvent->y);
		return FALSE;
	}
	
	//\_______________ On retarde la sortie.
	if (pDock->iSidLeaveDemand == 0 && pEvent != NULL)  // pas encore de demande de sortie et sortie naturelle.
	{
		if (pDock->iRefCount == 0)  // cas du main dock : on retarde si on pointe sur un sous-dock (pour laisser le temps au signal d'entree dans le sous-dock d'etre traite) ou si l'on a l'auto-hide.
		{
			//g_print (" leave event : %.1f;%.1f (%dx%d)\n", pEvent->x, pEvent->y, pDock->container.iWidth, pDock->container.iHeight);
			Icon *pPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
			if ((pPointedIcon != NULL && pPointedIcon->pSubDock != NULL && GTK_WIDGET_VISIBLE (pPointedIcon->pSubDock->container.pWidget)) || (pDock->bAutoHide))
			{
				//g_print ("  on retarde la sortie du dock de %dms\n", MAX (myAccessibility.iLeaveSubDockDelay, 330));
				pDock->iSidLeaveDemand = g_timeout_add (MAX (myAccessibility.iLeaveSubDockDelay, 330), (GSourceFunc) cairo_dock_emit_leave_signal, (gpointer) pDock);
				return TRUE;
			}
		}
		else if (myAccessibility.iLeaveSubDockDelay != 0)  // cas d'un sous-dock : on retarde le cachage.
		{
			//g_print ("  on retarde la sortie du sous-dock de %dms\n", myAccessibility.iLeaveSubDockDelay);
			pDock->iSidLeaveDemand = g_timeout_add (myAccessibility.iLeaveSubDockDelay, (GSourceFunc) cairo_dock_emit_leave_signal, (gpointer) pDock);
			return TRUE;
		}
	}
	else if (pDock->iSidLeaveDemand != 0 && pEvent != NULL)  // sortie naturelle et deja une sortie en attente.
	{
		cd_debug ("une sortie est deja programmee\n");
		return TRUE;
	}
	pDock->iSidLeaveDemand = 0;
	
	//\_______________ Arrive ici, on est sorti du dock.
	pDock->container.bInside = FALSE;
	pDock->iAvoidingMouseIconType = -1;
	pDock->fAvoidingMouseMargin = 0;
	
	//\_______________ On cache ses sous-docks.
	if (! cairo_dock_hide_child_docks (pDock))  // on quitte si l'un des sous-docks reste visible (on est entre dedans), pour rester en position "haute".
		return TRUE;
	
	if (s_iSidShowSubDockDemand != 0 && pDock->iRefCount == 0)  // si l'un des sous-docks etait programme pour se montrer, on annule.
	{
		g_source_remove (s_iSidShowSubDockDemand);
		s_iSidShowSubDockDemand = 0;
		s_pDockShowingSubDock = NULL;
	}
	
	//\_______________ On enregistre la position de la souris.
	if (pEvent != NULL)
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
	//g_print ("%s (%d, %d)\n", __func__, pDock->iRefCount, pDock->bMenuVisible);
	
	//\_______________ On quitte si le menu est leve, pour rester en position haute.
	if (pDock->bMenuVisible)
		return TRUE;
	
	//\_______________ On gere le drag d'une icone hors du dock.
	if (s_pIconClicked != NULL && (CAIRO_DOCK_IS_LAUNCHER (s_pIconClicked) || CAIRO_DOCK_IS_DETACHABLE_APPLET (s_pIconClicked) || CAIRO_DOCK_IS_USER_SEPARATOR(s_pIconClicked)) && s_pFlyingContainer == NULL && ! g_bLocked && ! myAccessibility.bLockIcons && ! myAccessibility.bLockAll)
	{
		cd_debug ("on a sorti %s du dock (%d;%d) / %dx%d\n", s_pIconClicked->cName, pDock->container.iMouseX, pDock->container.iMouseY, pDock->container.iWidth, pDock->container.iHeight);
		
		//if (! cairo_dock_hide_child_docks (pDock))  // on quitte si on entre dans un sous-dock, pour rester en position "haute".
		//	return ;
		
		CairoDock *pOriginDock = cairo_dock_search_dock_from_name (s_pIconClicked->cParentDockName);
		g_return_val_if_fail (pOriginDock != NULL, TRUE);
		if (pOriginDock == pDock && _mouse_is_really_outside (pDock))  // ce test est la pour parer aux WM deficients mentaux comme KWin qui nous font sortir/rentrer lors d'un clic.
		{
			cd_debug (" on detache l'icone\n");
			pOriginDock->bIconIsFlyingAway = TRUE;
			gchar *cParentDockName = s_pIconClicked->cParentDockName;
			s_pIconClicked->cParentDockName = NULL;
			cairo_dock_detach_icon_from_dock (s_pIconClicked, pOriginDock, TRUE);
			s_pIconClicked->cParentDockName = cParentDockName;
			cairo_dock_update_dock_size (pOriginDock);
			cairo_dock_stop_icon_glide (pOriginDock);
			
			s_pFlyingContainer = cairo_dock_create_flying_container (s_pIconClicked, pOriginDock, TRUE);
			//g_print ("- s_pIconClicked <- NULL\n");
			s_pIconClicked = NULL;
			if (pDock->iRefCount > 0 || pDock->bAutoHide)  // pour garder le dock visible.
			{
				return TRUE;
			}
		}
	}
	else if (s_pFlyingContainer != NULL && s_pFlyingContainer->pIcon != NULL && pDock->iRefCount > 0)  // on evite les bouclages.
	{
		CairoDock *pOriginDock = cairo_dock_search_dock_from_name (s_pFlyingContainer->pIcon->cParentDockName);
		if (pOriginDock == pDock)
			return TRUE;
	}
	
	//\_______________ On lance l'animation du dock.
	if (pDock->iRefCount == 0)
	{
		//g_print ("%s (auto-hide:%d)\n", __func__, pDock->bAutoHide);
		if (pDock->bAutoHide)
		{
			///pDock->fFoldingFactor = (mySystem.bAnimateOnAutoHide ? 0.001 : 0.);
			cairo_dock_start_hiding (pDock);
		}
	}
	else if (pDock->icons != NULL)
	{
		pDock->fFoldingFactor = (mySystem.bAnimateSubDock ? 0.001 : 0.);
		Icon *pIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
		//g_print ("'%s' se replie\n", pIcon?pIcon->cName:"none");
		cairo_dock_notify_on_icon (pIcon, CAIRO_DOCK_UNFOLD_SUBDOCK, pIcon);
	}
	cairo_dock_start_shrinking (pDock);  // on commence a faire diminuer la taille des icones.
	
	return TRUE;
}

gboolean cairo_dock_on_enter_notify (GtkWidget* pWidget, GdkEventCrossing* pEvent, CairoDock *pDock)
{
	//g_print ("%s (bIsMainDock : %d; bInside:%d; state:%d; iMagnitudeIndex:%d; input shape:%x; event:%ld)\n", __func__, pDock->bIsMainDock, pDock->container.bInside, pDock->iInputState, pDock->iMagnitudeIndex, pDock->pShapeBitmap, pEvent);
	s_pLastPointedDock = NULL;  // ajoute le 04/10/07 pour permettre aux sous-docks d'apparaitre si on entre en pointant tout de suite sur l'icone.
	if (! cairo_dock_entrance_is_allowed (pDock))
	{
		cd_message ("* entree non autorisee");
		return FALSE;
	}
	
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
	
	// input shape desactivee, le dock devient actif.
	if ((pDock->pShapeBitmap || pDock->pHiddenShapeBitmap) && pDock->iInputState != CAIRO_DOCK_INPUT_ACTIVE)
	{
		//g_print ("+++ input shape active on enter\n");
		gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
			NULL,
			0,
			0);
	}
	pDock->iInputState = CAIRO_DOCK_INPUT_ACTIVE;
	
	// si on etait deja dedans, ou qu'on etait cense l'etre, on relance juste le grossissement.
	if (pDock->container.bInside || pDock->bIsHiding)
	{
		pDock->container.bInside = TRUE;
		cairo_dock_start_growing (pDock);
		if (pDock->bIsHiding || cairo_dock_is_hidden (pDock))  // on (re)monte.
		{
			cd_debug ("  on etait deja dedans\n");
			cairo_dock_start_showing (pDock);
		}
		return FALSE;
	}
	
	pDock->container.bInside = TRUE;
	// animation d'entree.
	gboolean bStartAnimation = FALSE;
	cairo_dock_notify (CAIRO_DOCK_ENTER_DOCK, pDock, &bStartAnimation);
	if (bStartAnimation)
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	
	pDock->fDecorationsOffsetX = 0;
	cairo_dock_stop_quick_hide ();
	
	if (s_pIconClicked != NULL)  // on pourrait le faire a chaque motion aussi.
	{
		pDock->iAvoidingMouseIconType = s_pIconClicked->iType;
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
			double t = tv.tv_sec + tv.tv_usec * 1e-6;
			if (t - s_pFlyingContainer->fCreationTime > 1)  // on empeche le cas ou enlever l'icone fait augmenter le ratio du dock, et donc sa hauteur, et nous fait rentrer dedans des qu'on sort l'icone.
			{
				cd_debug ("on remet l'icone volante dans un dock (dock d'origine : %s)\n", pFlyingIcon->cParentDockName);
				cairo_dock_free_flying_container (s_pFlyingContainer);
				cairo_dock_stop_icon_animation (pFlyingIcon);
				cairo_dock_insert_icon_in_dock (pFlyingIcon, pDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
				cairo_dock_start_icon_animation (pFlyingIcon, pDock);
				s_pFlyingContainer = NULL;
				pDock->bIconIsFlyingAway = FALSE;
			}
		}
	}
	
	/**
	// on repasse au premier plan.
	if (myAccessibility.bPopUp && pDock->iRefCount == 0)
	{
		cairo_dock_pop_up (pDock);
		//If the dock window is entered, and there is a pending drop below event then it should be cancelled
		if (pDock->iSidPopDown != 0)
		{
			g_source_remove(pDock->iSidPopDown);
			pDock->iSidPopDown = 0;
		}
	}*/
	if (pDock->iSidHideBack != 0)
	{
		//g_print ("remove hide back timeout\n");
		g_source_remove(pDock->iSidHideBack);
		pDock->iSidHideBack = 0;
	}
	
	// si on etait en auto-hide, on commence a monter.
	if (pDock->fHideOffset != 0 && pDock->iRefCount == 0)
	{
		//g_print ("  on commence a monter\n");
		cairo_dock_start_showing (pDock);  // on a mis a jour la zone d'input avant, sinon la fonction le ferait, ce qui serait inutile.
	}
	
	// cas special.
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	if (icon != NULL)
	{
		//g_print (">>> we've just entered the dock, pointed icon becomes NULL\n");
		//if (s_pIconClicked != NULL)
		//	cd_debug (">>> on est rentre par un clic ! (KDE:%d)\n", g_iDesktopEnv == CAIRO_DOCK_KDE);
		if (_mouse_is_really_outside (pDock))  // ce test est la pour parer aux WM deficients mentaux comme KWin qui nous font sortir/rentrer lors d'un clic.
			icon->bPointed = FALSE;  // sinon on ne detecte pas l'arrive sur l'icone, c'est genant si elle a un sous-dock.
		//else
		//	cd_debug (">>> we already are inside the dock, why does this stupid WM make us enter one more time ???\n");
	}
	
	// on lance le grossissement.
	cairo_dock_start_growing (pDock);
	
	return TRUE;
}


gboolean cairo_dock_on_key_release (GtkWidget *pWidget,
	GdkEventKey *pKey,
	CairoDock *pDock)
{
	cd_debug ("on a appuye sur une touche (%d)", pKey->keyval);
	if (pKey->type == GDK_KEY_PRESS)
	{
		cairo_dock_notify (CAIRO_DOCK_KEY_PRESSED, pDock, pKey->keyval, pKey->state, pKey->string);
	}
	else if (pKey->type == GDK_KEY_RELEASE)
	{
		//g_print ("release : pKey->keyval = %d\n", pKey->keyval);
		if ((pKey->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) && pKey->keyval == 0)  // On relache la touche ALT, typiquement apres avoir fait un ALT + clique gauche + deplacement.
		{
			if (pDock->iRefCount == 0)
				cairo_dock_write_root_dock_gaps (pDock);
		}
	}
	return TRUE;
}


gboolean cairo_dock_on_button_press (GtkWidget* pWidget, GdkEventButton* pButton, CairoDock *pDock)
{
	//g_print ("+ %s (%d/%d, %x)\n", __func__, pButton->type, pButton->button, pWidget);
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
				if ( ! (pButton->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
				{
					if (s_pIconClicked != NULL)
					{
						cd_debug ("activate %s (%s)", s_pIconClicked->cName, icon ? icon->cName : "none");
						s_pIconClicked->iAnimationState = CAIRO_DOCK_STATE_REST;  // stoppe les animations de suivi du curseur.
						pDock->iAvoidingMouseIconType = -1;
						cairo_dock_stop_icon_glide (pDock);
					}
					if (icon != NULL && ! CAIRO_DOCK_IS_SEPARATOR (icon) && icon == s_pIconClicked)
					{
						s_pIconClicked = NULL;  // il faut le faire ici au cas ou le clic induirait un dialogue bloquant qui nous ferait sortir du dock par exemple.
						//g_print ("+ click on '%s' (%s)\n", icon->cName, icon->cCommand);
						if (! s_bIconDragged)  // on ignore le drag'n'drop sur elle-meme.
						{
							cairo_dock_notify (CAIRO_DOCK_CLICK_ICON, icon, pDock, pButton->state);
							if (myAccessibility.cRaiseDockShortcut != NULL)
								s_bHideAfterShortcut = TRUE;
							
							cairo_dock_start_icon_animation (icon, pDock);
							icon->bIsDemandingAttention = FALSE;  // on considere que si l'utilisateur clique sur l'icone, c'est qu'il a pris acte de la notification.
						}
					}
					else if (s_pIconClicked != NULL && icon != NULL && icon != s_pIconClicked && ! g_bLocked && ! myAccessibility.bLockIcons && ! myAccessibility.bLockAll)  //  && icon->iType == s_pIconClicked->iType
					{
						//g_print ("deplacement de %s\n", s_pIconClicked->cName);
						CairoDock *pOriginDock = CAIRO_DOCK (cairo_dock_search_container_from_icon (s_pIconClicked));
						if (pOriginDock != NULL && pDock != pOriginDock)
						{
							cairo_dock_detach_icon_from_dock (s_pIconClicked, pOriginDock, TRUE);
							cairo_dock_update_dock_size (pOriginDock);
							
							cairo_dock_update_icon_s_container_name (s_pIconClicked, icon->cParentDockName);
							
							cairo_dock_insert_icon_in_dock (s_pIconClicked, pDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
							cairo_dock_start_icon_animation (s_pIconClicked, pDock);
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
						if ((prev_icon == NULL || cairo_dock_get_icon_order (prev_icon) != cairo_dock_get_icon_order (s_pIconClicked)) && (next_icon == NULL || cairo_dock_get_icon_order (next_icon) != cairo_dock_get_icon_order (s_pIconClicked)))
						{
							s_pIconClicked = NULL;
							return FALSE;
						}
						//g_print ("deplacement de %s\n", s_pIconClicked->cName);
						if (prev_icon != NULL && cairo_dock_get_icon_order (prev_icon) != cairo_dock_get_icon_order (s_pIconClicked))
							prev_icon = NULL;
						cairo_dock_move_icon_after_icon (pDock, s_pIconClicked, prev_icon);

						pDock->pRenderer->calculate_icons (pDock);

						if (! CAIRO_DOCK_IS_SEPARATOR (s_pIconClicked))
						{
							cairo_dock_request_icon_animation (s_pIconClicked, pDock, "bounce", 2);
						}
						if (pDock->container.iSidGLAnimation == 0 || ! CAIRO_CONTAINER_IS_OPENGL (CAIRO_CONTAINER (pDock)))
							gtk_widget_queue_draw (pDock->container.pWidget);
					}
					
					if (s_pFlyingContainer != NULL)
					{
						cd_debug ("on relache l'icone volante");
						if (pDock->container.bInside)
						{
							//g_print ("  on la remet dans son dock d'origine\n");
							Icon *pFlyingIcon = s_pFlyingContainer->pIcon;
							cairo_dock_free_flying_container (s_pFlyingContainer);
							cairo_dock_stop_marking_icon_as_following_mouse (pFlyingIcon);
							cairo_dock_stop_icon_animation (pFlyingIcon);
							cairo_dock_insert_icon_in_dock (pFlyingIcon, pDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
							cairo_dock_start_icon_animation (pFlyingIcon, pDock);
						}
						else
						{
							cairo_dock_terminate_flying_container (s_pFlyingContainer);  // supprime ou detache l'icone, l'animation se terminera toute seule.
						}
						s_pFlyingContainer = NULL;
						pDock->bIconIsFlyingAway = FALSE;
						cairo_dock_stop_icon_glide (pDock);
					}
				}
				else
				{
					if (pDock->iRefCount == 0)
						cairo_dock_write_root_dock_gaps (pDock);
				}
				//g_print ("- apres clic : s_pIconClicked <- NULL\n");
				s_pIconClicked = NULL;
				s_bIconDragged = FALSE;
			break ;
			
			case GDK_BUTTON_PRESS :
				if ( ! (pButton->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
				{
					//g_print ("+ clic sur %s (%.2f)!\n", icon ? icon->cName : "rien", icon ? icon->fInsertRemoveFactor : 0.);
					s_iClickX = pButton->x;
					s_iClickY = pButton->y;
					if (icon && ! cairo_dock_icon_is_being_removed (icon))
					{
						s_pIconClicked = icon;  // on ne definit pas l'animation FOLLOW_MOUSE ici , on le fera apres le 1er mouvement, pour eviter que l'icone soit dessinee comme tel quand on clique dessus alors que le dock est en train de jouer une animation (ca provoque un flash desagreable).
						cd_debug ("clicked on %s", icon->cName);
					}
					else
						s_pIconClicked = NULL;
				}
			break ;

			case GDK_2BUTTON_PRESS :
				{
					if (icon && ! cairo_dock_icon_is_being_removed (icon))
						cairo_dock_notify (CAIRO_DOCK_DOUBLE_CLICK_ICON, icon, pDock);
				}
			break ;

			default :
			break ;
		}
	}
	else if (pButton->button == 3 && pButton->type == GDK_BUTTON_PRESS)  // clique droit.
	{
		GtkWidget *menu = cairo_dock_build_menu (icon, CAIRO_CONTAINER (pDock));  // genere un CAIRO_DOCK_BUILD_CONTAINER_MENU et CAIRO_DOCK_BUILD_ICON_MENU.
		
		cairo_dock_popup_menu_on_container (menu, CAIRO_CONTAINER (pDock));
	}
	else if (pButton->button == 2 && pButton->type == GDK_BUTTON_PRESS)  // clique milieu.
	{
		if (icon && ! cairo_dock_icon_is_being_removed (icon))
			cairo_dock_notify (CAIRO_DOCK_MIDDLE_CLICK_ICON, icon, pDock);
	}

	return FALSE;
}


gboolean cairo_dock_on_scroll (GtkWidget* pWidget, GdkEventScroll* pScroll, CairoDock *pDock)
{
	if (pScroll->direction != GDK_SCROLL_UP && pScroll->direction != GDK_SCROLL_DOWN)  // on degage les scrolls horizontaux.
	{
		return FALSE;
	}
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	if (icon != NULL)
	{
		cairo_dock_notify (CAIRO_DOCK_SCROLL_ICON, icon, pDock, pScroll->direction);
	}

	return FALSE;
}


gboolean cairo_dock_on_configure (GtkWidget* pWidget, GdkEventConfigure* pEvent, CairoDock *pDock)
{
	//g_print ("%s (main dock : %d) : (%d;%d) (%dx%d)\n", __func__, pDock->bIsMainDock, pEvent->x, pEvent->y, pEvent->width, pEvent->height);
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
	gboolean bPositionUpdated = (pDock->container.iWindowPositionX != iNewX || pDock->container.iWindowPositionY != iNewY);
	pDock->container.iWidth = iNewWidth;
	pDock->container.iHeight = iNewHeight;
	pDock->container.iWindowPositionX = iNewX;
	pDock->container.iWindowPositionY = iNewY;
	
	if (bSizeUpdated && iNewWidth > 1)  // changement de taille
	{
		//g_print ("--------------------------------> %dx%d\n", iNewWidth, iNewHeight);
		
		if (pDock->container.bIsHorizontal)
			gdk_window_get_pointer (pWidget->window, &pDock->container.iMouseX, &pDock->container.iMouseY, NULL);
		else
			gdk_window_get_pointer (pWidget->window, &pDock->container.iMouseY, &pDock->container.iMouseX, NULL);
		if (pDock->container.iMouseX < 0 || pDock->container.iMouseX > pDock->container.iWidth)  // utile ?
			pDock->container.iMouseX = 0;
		//g_print ("x,y : %d;%d\n", pDock->container.iMouseX, pDock->container.iMouseY);
		
		// les dimensions ont change, il faut remettre l'input shape a la bonne place.
		if (pDock->pHiddenShapeBitmap != NULL && pDock->iInputState == CAIRO_DOCK_INPUT_HIDDEN)
		{
			//g_print ("+++ input shape hidden on configure\n");
			gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
				NULL,
				0,
				0);
			gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
				pDock->pHiddenShapeBitmap,
				0,
				0);
		}
		else if (pDock->pShapeBitmap != NULL && pDock->iInputState == CAIRO_DOCK_INPUT_AT_REST)
		{
			//g_print ("+++ input shape at rest on configure\n");
			gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
				NULL,
				0,
				0);
			gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
				pDock->pShapeBitmap,
				0,
				0);
		}
		
		if (g_bUseOpenGL)
		{
			GdkGLContext* pGlContext = gtk_widget_get_gl_context (pWidget);
			GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (pWidget);
			GLsizei w = pEvent->width;
			GLsizei h = pEvent->height;
			if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
				return FALSE;
			
			glViewport(0, 0, w, h);
			
			/*glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, w, 0, h, 0.0, 500.0);
			
			glMatrixMode (GL_MODELVIEW);
			glLoadIdentity ();
			gluLookAt (w/2, h/2, 3.,
				w/2, h/2, 0.,
				0.0f, 1.0f, 0.0f);
			glTranslatef (0.0f, 0.0f, -3.);*/
			///cairo_dock_set_ortho_view (w, h);
			cairo_dock_set_ortho_view (CAIRO_CONTAINER (pDock));
			
			glClearAccum (0., 0., 0., 0.);
			glClear (GL_ACCUM_BUFFER_BIT);
			
			if (pDock->iRedirectedTexture != 0)
			{
				_cairo_dock_delete_texture (pDock->iRedirectedTexture);
				pDock->iRedirectedTexture = cairo_dock_load_texture_from_raw_data (NULL, pEvent->width, pEvent->height);
			}
			
			gdk_gl_drawable_gl_end (pGlDrawable);
		}
		
		#ifdef HAVE_GLITZ
		if (pDock->container.pGlitzDrawable)
		{
			glitz_drawable_update_size (pDock->container.pGlitzDrawable,
				pEvent->width,
				pEvent->height);
		}
		#endif
		
		cairo_dock_calculate_dock_icons (pDock);
		//g_print ("configure size\n");
		cairo_dock_trigger_set_WM_icons_geometry (pDock);  // changement de position ou de taille du dock => on replace les icones.
		
		cairo_dock_replace_all_dialogs ();
	}
	else if (bPositionUpdated)  // changement de position.
	{
		//g_print ("configure x,y\n");
		cairo_dock_trigger_set_WM_icons_geometry (pDock);  // changement de position de la fenetre du dock => on replace les icones.
		
		cairo_dock_replace_all_dialogs ();
	}
	
	if (pDock->iRefCount > 0 && (bSizeUpdated || bPositionUpdated))
	{
		if (myAccessibility.bAutoHideOnOverlap || myAccessibility.bAutoHideOnAnyOverlap)
		{
			if (myAccessibility.bAutoHideOnOverlap)
			{
				Icon *pActiveAppli = cairo_dock_get_current_active_icon ();
				if (_cairo_dock_appli_is_on_our_way (pActiveAppli, pDock))  // la fenetre active nous gene.
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
			else if (myAccessibility.bAutoHideOnAnyOverlap)
			{
				if (cairo_dock_search_window_overlapping_dock (pDock) != NULL)
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
	}
	
	gtk_widget_queue_draw (pWidget);
	
	return FALSE;
}



static gboolean s_bWaitForData = FALSE;

void cairo_dock_on_drag_data_received (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, guint info, guint time, CairoDock *pDock)
{
	cd_debug ("%s (%dx%d, %d, %d)", __func__, x, y, time, pDock->container.bInside);
	if (cairo_dock_is_hidden (pDock))  // X ne semble pas tenir compte de la zone d'input pour dropper les trucs...
		return ;
	//\_________________ On recupere l'URI.
	gchar *cReceivedData = (gchar *) selection_data->data;  // gtk_selection_data_get_text
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
		cd_debug ("drag info : <%s>\n", cReceivedData);
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
	cd_message (">>> cReceivedData : '%s'", cReceivedData);
	int iDropX = (pDock->container.bIsHorizontal ? x : y);
	double fOrder = CAIRO_DOCK_LAST_ORDER;
	Icon *pPointedIcon = NULL, *pNeighboorIcon = NULL;
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->bPointed)
		{
			//g_print ("On pointe sur %s\n", icon->cName);
			pPointedIcon = icon;
			double fMargin;  /// deviendra obsolete si le drag-received fonctionne.
			if (g_str_has_suffix (cReceivedData, ".desktop")/** || g_str_has_suffix (cReceivedData, ".sh")*/)  // si c'est un .desktop, on l'ajoute.
			{
				if (g_bLocked || myAccessibility.bLockIcons || myAccessibility.bLockAll)
				{
					gtk_drag_finish (dc, FALSE, FALSE, time);
					return ;
				}
				fMargin = 0.5;  // on ne sera jamais dessus.
			}
			else  // sinon on le lance si on est sur l'icone, et on l'ajoute autrement.
				fMargin = 0.25;

			if (iDropX > icon->fX + icon->fWidth * icon->fScale * (1 - fMargin))  // on est apres.
			{
				if (g_bLocked || myAccessibility.bLockIcons || myAccessibility.bLockAll)
				{
					gtk_drag_finish (dc, FALSE, FALSE, time);
					return ;
				}
				pNeighboorIcon = (ic->next != NULL ? ic->next->data : NULL);
				fOrder = (pNeighboorIcon != NULL ? (icon->fOrder + pNeighboorIcon->fOrder) / 2 : icon->fOrder + 1);
			}
			else if (iDropX < icon->fX + icon->fWidth * icon->fScale * fMargin)  // on est avant.
			{
				if (g_bLocked || myAccessibility.bLockIcons || myAccessibility.bLockAll)
				{
					gtk_drag_finish (dc, FALSE, FALSE, time);
					return ;
				}
				pNeighboorIcon = (ic->prev != NULL ? ic->prev->data : NULL);
				fOrder = (pNeighboorIcon != NULL ? (icon->fOrder + pNeighboorIcon->fOrder) / 2 : icon->fOrder - 1);
			}
			else  // on est dessus.
			{
				fOrder = CAIRO_DOCK_LAST_ORDER;
			}
		}
	}
	
	cairo_dock_notify_drop_data (cReceivedData, pPointedIcon, fOrder, CAIRO_CONTAINER (pDock));
	
	gtk_drag_finish (dc, TRUE, FALSE, time);
}

gboolean cairo_dock_on_drag_drop (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, guint time, CairoDock *pDock)
{
	cd_message ("%s (%dx%d, %d)", __func__, x, y, time);
	GdkAtom target = gtk_drag_dest_find_target (pWidget, dc, NULL);
	gtk_drag_get_data (pWidget, dc, target, time);
	return TRUE;  // in a drop zone.
}


gboolean cairo_dock_on_drag_motion (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, guint time, CairoDock *pDock)
{
	g_print ("%s (%d;%d, %d)\n", __func__, x, y, time);
	
	//\_________________ On simule les evenements souris habituels.
	if (! pDock->bIsDragging)
	{
		g_print ("start dragging\n");
		pDock->bIsDragging = TRUE;
		
		/*GdkAtom gdkAtom = gdk_drag_get_selection (dc);
		Atom xAtom = gdk_x11_atom_to_xatom (gdkAtom);
		Window Xid = GDK_WINDOW_XID (dc->source_window);
		cd_debug (" <%s>\n", cairo_dock_get_property_name_on_xwindow (Xid, xAtom));*/
		
		gboolean bStartAnimation = FALSE;
		cairo_dock_notify (CAIRO_DOCK_START_DRAG_DATA, pDock, &bStartAnimation);
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
		
		cairo_dock_on_enter_notify (pWidget, NULL, pDock);  // ne sera effectif que la 1ere fois a chaque entree dans un dock.
	}
	else
	{
		//g_print ("move dragging\n");
		cairo_dock_on_motion_notify (pWidget, NULL, pDock);
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
	int w, h;
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	if (pDock->iInputState == CAIRO_DOCK_INPUT_AT_REST)
	{
		w = pDock->iMinDockWidth;
		h = pDock->iMinDockHeight;
		
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
	
	g_print ("take the drop\n");
	gdk_drag_status (dc, GDK_ACTION_COPY, time);
	return TRUE;  // on accepte le drop.
}

void cairo_dock_on_drag_leave (GtkWidget *pWidget, GdkDragContext *dc, guint time, CairoDock *pDock)
{
	cd_debug ("stop dragging1\n");
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	if ((icon && icon->pSubDock) || pDock->iRefCount > 0)  // on retarde l'evenement, car il arrive avant le leave-event, et donc le sous-dock se cache avant qu'on puisse y entrer.
	{
		cd_debug (">>> on attend...");
		while (gtk_events_pending ())  // on laisse le temps au signal d'entree dans le sous-dock d'etre traite, de facon a avoir un start-dragging avant de quitter cette fonction.
			gtk_main_iteration ();
		cd_debug (">>> pDock->container.bInside : %d", pDock->container.bInside);
	}
	cd_debug ("stop dragging2\n");
	s_bWaitForData = FALSE;
	pDock->bIsDragging = FALSE;
	pDock->bCanDrop = FALSE;
	//cairo_dock_stop_marking_icons (pDock);
	pDock->iAvoidingMouseIconType = -1;
	cairo_dock_emit_leave_signal (pDock);
}


static void _cairo_dock_show_dock_at_mouse (CairoDock *pDock)
{
	g_return_if_fail (pDock != NULL);
	int iMouseX, iMouseY;
	if (pDock->container.bIsHorizontal)
		gdk_window_get_pointer (pDock->container.pWidget->window, &iMouseX, &iMouseY, NULL);
	else
		gdk_window_get_pointer (pDock->container.pWidget->window, &iMouseY, &iMouseX, NULL);
	//g_print (" %d;%d\n", iMouseX, iMouseY);
	
	///pDock->iGapX = pDock->container.iWindowPositionX + iMouseX - g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] * pDock->fAlign;
	///pDock->iGapY = (pDock->container.bDirectionUp ? g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] - (pDock->container.iWindowPositionY + iMouseY) : pDock->container.iWindowPositionY + iMouseY);
	pDock->iGapX = iMouseX - (g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] - pDock->container.iWidth) * pDock->fAlign - pDock->iScreenOffsetX;
	pDock->iGapY = iMouseY - (pDock->container.bDirectionUp ? g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] - pDock->container.iHeight : 0) - pDock->iScreenOffsetY;
	//g_print (" => %d;%d\n", g_pMainDock->iGapX, g_pMainDock->iGapY);
	
	int iNewPositionX, iNewPositionY;
	cairo_dock_get_window_position_at_balance (pDock,
		pDock->container.iWidth, pDock->container.iHeight,
		&iNewPositionX, &iNewPositionY);
	
	gtk_window_move (GTK_WINDOW (pDock->container.pWidget),
		(pDock->container.bIsHorizontal ? iNewPositionX : iNewPositionY),
		(pDock->container.bIsHorizontal ? iNewPositionY : iNewPositionX));
	gtk_widget_show (pDock->container.pWidget);
}
void cairo_dock_raise_from_shortcut (const char *cKeyShortcut, gpointer data)
{
	if (GTK_WIDGET_VISIBLE (g_pMainDock->container.pWidget))
	{
		gtk_widget_hide (g_pMainDock->container.pWidget);
	}
	else
	{
		_cairo_dock_show_dock_at_mouse (g_pMainDock);
	}
	s_bHideAfterShortcut = FALSE;
}

void cairo_dock_hide_after_shortcut (void)
{
	if (s_bHideAfterShortcut && GTK_WIDGET_VISIBLE (g_pMainDock->container.pWidget))
	{
		gtk_widget_hide (g_pMainDock->container.pWidget);
		s_bHideAfterShortcut = FALSE;
	}
}


/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <math.h>
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

#include "cairo-dock-menu.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-load.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-config.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-hidden-dock.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-callbacks.h"

static Icon *s_pIconClicked = NULL;  // pour savoir quand on deplace une icone a la souris. Dangereux si l'icone se fait effacer en cours ...
static CairoDock *s_pLastPointedDock = NULL;  // pour savoir quand on passe d'un dock a un autre.
static int s_iSidShowSubDockDemand = 0;
static int s_iSidShowAppliForDrop = 0;
static CairoDock *s_pDockShowingSubDock = NULL;  // on n'accede pas a son contenu, seulement l'adresse.
static CairoFlyingContainer *s_pFlyingContainer = NULL;

extern CairoDock *g_pMainDock;
extern gboolean g_bKeepAbove;

extern gint g_iXScreenWidth[2];
extern gint g_iXScreenHeight[2];
extern cairo_surface_t *g_pBackgroundSurfaceFull[2];

extern gboolean g_bEasterEggs;
extern gboolean g_bUseOpenGL;
extern gboolean g_bLocked;
extern CairoDockDesktopEnv g_iDesktopEnv;

static gboolean s_bHideAfterShortcut = FALSE;
static gboolean s_bFrozenDock = FALSE;


#define _mouse_is_really_outside(pDock) (pDock->iMouseX <= 0 || pDock->iMouseX >= pDock->iCurrentWidth || pDock->iMouseY <= 0 || pDock->iMouseY >= pDock->iCurrentHeight)

void cairo_dock_freeze_docks (gboolean bFreeze)
{
	s_bFrozenDock = bFreeze;
}

gboolean cairo_dock_render_dock_notification (gpointer pUserData, CairoDock *pDock, cairo_t *pCairoContext)
{
	if (! pCairoContext)  // on n'a pas mis le rendu cairo ici a cause du rendu optimise.
	{
		glLoadIdentity ();
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (pDock->bUseStencil ? GL_STENCIL_BUFFER_BIT : 0));
		cairo_dock_apply_desktop_background (CAIRO_CONTAINER (pDock));
		
		pDock->render_opengl (pDock);
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_on_expose (GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	CairoDock *pDock)
{
	//g_print ("%s ((%d;%d) %dx%d) (%d)\n", __func__, pExpose->area.x, pExpose->area.y, pExpose->area.width, pExpose->area.height, pDock->bAtBottom);
	if (g_bUseOpenGL && pDock->render_opengl != NULL)
	{
		GdkGLContext *pGlContext = gtk_widget_get_gl_context (pDock->pWidget);
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pDock->pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return FALSE;
		
		if (pExpose->area.x + pExpose->area.y != 0)
		{
			glEnable (GL_SCISSOR_TEST);  // ou comment diviser par 4 l'occupation CPU !
			glScissor ((int) pExpose->area.x,
				(int) (pDock->bHorizontalDock ? pDock->iCurrentHeight : pDock->iCurrentWidth) -
					pExpose->area.y - pExpose->area.height,  // lower left corner of the scissor box.
				(int) pExpose->area.width,
				(int) pExpose->area.height);
		}
		
		if (cairo_dock_is_loading ())
		{
			// on laisse transparent
		}
		else if (pDock->bAtBottom && pDock->bAutoHide && pDock->iRefCount == 0 && ! pDock->bInside && pDock->iSidMoveDown == 0)
		{
			cairo_dock_render_hidden_dock_opengl (pDock);
		}
		else
		{
			cairo_dock_notify (CAIRO_DOCK_RENDER_DOCK, pDock, NULL);
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
		if (! (pDock->bAutoHide && pDock->iRefCount == 0) || ! pDock->bAtBottom)
		{
			cairo_t *pCairoContext = cairo_dock_create_drawing_context_on_area (CAIRO_CONTAINER (pDock), &pExpose->area, NULL);
			
			if (pDock->render_optimized != NULL)
				pDock->render_optimized (pCairoContext, pDock, &pExpose->area);
			else
				pDock->render (pCairoContext, pDock);
			cairo_dock_notify (CAIRO_DOCK_RENDER_DOCK, pDock, pCairoContext);
			
			cairo_destroy (pCairoContext);
		}
		return FALSE;
	}
	
	
	cairo_t *pCairoContext = cairo_dock_create_drawing_context (CAIRO_CONTAINER (pDock));
	
	if (cairo_dock_is_loading ())  // transparent pendant le chargement.
	{
		
	}
	else if (pDock->bAtBottom && pDock->bAutoHide && pDock->iRefCount == 0 && ! pDock->bInside && pDock->iSidMoveDown == 0)
	{
		cairo_dock_render_hidden_dock (pCairoContext, pDock);
	}
	else
	{
		pDock->render (pCairoContext, pDock);
		cairo_dock_notify (CAIRO_DOCK_RENDER_DOCK, pDock, pCairoContext);
	}
	
	cairo_destroy (pCairoContext);
	
#ifdef HAVE_GLITZ
	if (pDock->pDrawFormat && pDock->pDrawFormat->doublebuffer)
		glitz_drawable_swap_buffers (pDock->pGlitzDrawable);
#endif
	return FALSE;
}


void cairo_dock_show_subdock (Icon *pPointedIcon, gboolean bUpdate, CairoDock *pDock)
{
	cd_debug ("on montre le dock fils");
	CairoDock *pSubDock = pPointedIcon->pSubDock;
	g_return_if_fail (pSubDock != NULL);
	
	if (GTK_WIDGET_VISIBLE (pSubDock->pWidget))  // il est deja visible.
	{
		if (pSubDock->bIsShrinkingDown)  // il est en cours de diminution, on renverse le processus.
		{
			cairo_dock_start_growing (pSubDock);
		}
		return ;
	}

	if (bUpdate)
	{
		pDock->calculate_icons (pDock);  // c'est un peu un hack pourri, l'idee c'est de recalculer la position exacte de l'icone pointee pour pouvoir placer le sous-dock precisement, car sa derniere position connue est decalee d'un coup de molette par rapport a la nouvelle, ce qui fait beaucoup. L'ideal etant de le faire que pour l'icone concernee ...
	}

	pSubDock->set_subdock_position (pPointedIcon, pDock);

	pSubDock->fFoldingFactor = (mySystem.bAnimateSubDock ? mySystem.fUnfoldAcceleration : 0);
	pSubDock->bAtBottom = FALSE;
	int iNewWidth, iNewHeight;
	if (pSubDock->fFoldingFactor == 0)
	{
		cd_debug ("  on montre le sous-dock sans animation");
		cairo_dock_get_window_position_and_geometry_at_balance (pSubDock, CAIRO_DOCK_MAX_SIZE, &iNewWidth, &iNewHeight);  // CAIRO_DOCK_NORMAL_SIZE -> CAIRO_DOCK_MAX_SIZE pour la 1.5.4
		pSubDock->bAtBottom = TRUE;  // bAtBottom ajoute pour la 1.5.4

		gtk_window_present (GTK_WINDOW (pSubDock->pWidget));

		if (pSubDock->bHorizontalDock)
			gdk_window_move_resize (pSubDock->pWidget->window,
				pSubDock->iWindowPositionX,
				pSubDock->iWindowPositionY,
				iNewWidth,
				iNewHeight);
		else
			gdk_window_move_resize (pSubDock->pWidget->window,
				pSubDock->iWindowPositionY,
				pSubDock->iWindowPositionX,
				iNewHeight,
				iNewWidth);

		/*if (pSubDock->bHorizontalDock)
			gtk_window_move (GTK_WINDOW (pSubDock->pWidget), pSubDock->iWindowPositionX, pSubDock->iWindowPositionY);
		else
			gtk_window_move (GTK_WINDOW (pSubDock->pWidget), pSubDock->iWindowPositionY, pSubDock->iWindowPositionX);

		gtk_window_present (GTK_WINDOW (pSubDock->pWidget));*/
		///gtk_widget_show (GTK_WIDGET (pSubDock->pWidget));
	}
	else
	{
		cd_debug ("  on montre le sous-dock avec animation");
		cairo_dock_get_window_position_and_geometry_at_balance (pSubDock, CAIRO_DOCK_MAX_SIZE, &iNewWidth, &iNewHeight);

		gtk_window_present (GTK_WINDOW (pSubDock->pWidget));
		///gtk_widget_show (GTK_WIDGET (pSubDock->pWidget));
		if (pSubDock->bHorizontalDock)
			gdk_window_move_resize (pSubDock->pWidget->window,
				pSubDock->iWindowPositionX,
				pSubDock->iWindowPositionY,
				iNewWidth,
				iNewHeight);
		else
			gdk_window_move_resize (pSubDock->pWidget->window,
				pSubDock->iWindowPositionY,
				pSubDock->iWindowPositionX,
				iNewHeight,
				iNewWidth);

		cairo_dock_start_growing (pSubDock);  // on commence a faire grossir les icones.
	}
	//g_print ("  -> Gap %d;%d -> W(%d;%d) (%d)\n", pSubDock->iGapX, pSubDock->iGapY, pSubDock->iWindowPositionX, pSubDock->iWindowPositionY, pSubDock->bHorizontalDock);
	
	gtk_window_set_keep_above (GTK_WINDOW (pSubDock->pWidget), myAccessibility.bPopUp);
	
	cairo_dock_replace_all_dialogs ();
}
static gboolean _cairo_dock_show_sub_dock_delayed (CairoDock *pDock)
{
	cd_debug ("");
	s_iSidShowSubDockDemand = 0;
	s_pDockShowingSubDock = NULL;
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	if (icon != NULL && icon->pSubDock != NULL)
		cairo_dock_show_subdock (icon, FALSE, pDock);

	return FALSE;
}


static gboolean _cairo_dock_show_xwindow_for_drop (gpointer data)
{
	Window Xid = GPOINTER_TO_INT (data);
	cairo_dock_show_xwindow (Xid);
	s_iSidShowAppliForDrop = 0;
	return FALSE;
}
void cairo_dock_on_change_icon (Icon *pLastPointedIcon, Icon *pPointedIcon, CairoDock *pDock)
{
	//g_print ("%s (%x;%x)\n", __func__, pLastPointedIcon, pPointedIcon);
	//cd_debug ("on change d'icone dans %x (-> %s)", pDock, (pPointedIcon != NULL ? pPointedIcon->acName : "rien"));
	if (s_iSidShowSubDockDemand != 0 && pDock == s_pDockShowingSubDock)
	{
		//cd_debug ("on annule la demande de montrage de sous-dock");
		g_source_remove (s_iSidShowSubDockDemand);
		s_iSidShowSubDockDemand = 0;
		s_pDockShowingSubDock = NULL;
	}
	
	if (pDock->bIsDragging && s_iSidShowAppliForDrop != 0)
	{
		//cd_debug ("on annule la demande de montrage d'appli");
		g_source_remove (s_iSidShowAppliForDrop);
		s_iSidShowAppliForDrop = 0;
	}
	cairo_dock_replace_all_dialogs ();
	if (pDock->bIsDragging && CAIRO_DOCK_IS_APPLI (pPointedIcon))
	{
		s_iSidShowAppliForDrop = g_timeout_add (500, (GSourceFunc) _cairo_dock_show_xwindow_for_drop, GINT_TO_POINTER (pPointedIcon->Xid));
	}
	
	//g_print ("%x/%x , %x, %x\n", pDock, s_pLastPointedDock, pLastPointedIcon, pLastPointedIcon?pLastPointedIcon->pSubDock:NULL);
	if ((pDock == s_pLastPointedDock || s_pLastPointedDock == NULL) && pLastPointedIcon != NULL && pLastPointedIcon->pSubDock != NULL)
	{
		CairoDock *pSubDock = pLastPointedIcon->pSubDock;
		if (GTK_WIDGET_VISIBLE (pSubDock->pWidget))
		{
			//g_print ("on cache %s en changeant d'icône\n", pLastPointedIcon->acName);
			if (pSubDock->iSidLeaveDemand == 0)
			{
				//g_print ("  on retarde le cachage du dock de %dms\n", MAX (myAccessibility.iLeaveSubDockDelay, 330));
				pSubDock->iSidLeaveDemand = g_timeout_add (MAX (myAccessibility.iLeaveSubDockDelay, 330), (GSourceFunc) cairo_dock_emit_leave_signal, (gpointer) pSubDock);
			}
		}
		//else
		//	cd_debug ("pas encore visible !\n");
	}
	if (pPointedIcon != NULL && pPointedIcon->pSubDock != NULL && pPointedIcon->pSubDock != s_pLastPointedDock && (! myAccessibility.bShowSubDockOnClick || CAIRO_DOCK_IS_APPLI (pPointedIcon) || pDock->bIsDragging))
	{
		//cd_debug ("il faut montrer un sous-dock");
		if (pPointedIcon->pSubDock->iSidLeaveDemand != 0)
		{
			g_source_remove (pPointedIcon->pSubDock->iSidLeaveDemand);
			pPointedIcon->pSubDock->iSidLeaveDemand = 0;
		}
		if (myAccessibility.iShowSubDockDelay > 0)
		{
			//pDock->iMouseX = iX;
			if (s_iSidShowSubDockDemand != 0)
				g_source_remove (s_iSidShowSubDockDemand);
			s_iSidShowSubDockDemand = g_timeout_add (myAccessibility.iShowSubDockDelay, (GSourceFunc) _cairo_dock_show_sub_dock_delayed, pDock);
			s_pDockShowingSubDock = pDock;
			//cd_debug ("s_iSidShowSubDockDemand <- %d\n", s_iSidShowSubDockDemand);
		}
		else
			cairo_dock_show_subdock (pPointedIcon, FALSE, pDock);
		s_pLastPointedDock = pDock;
	}

	if (s_pLastPointedDock == NULL)
	{
		//g_print ("pLastPointedDock n'est plus null\n");
		s_pLastPointedDock = pDock;
	}
	if (pPointedIcon != NULL && pDock->render_opengl != NULL && ! CAIRO_DOCK_IS_SEPARATOR (pPointedIcon) && pPointedIcon->iAnimationState <= CAIRO_DOCK_STATE_MOUSE_HOVERED)
	{
		gboolean bStartAnimation = FALSE;
		cairo_dock_notify (CAIRO_DOCK_ENTER_ICON, pPointedIcon, pDock, &bStartAnimation);
		
		if (bStartAnimation)
		{
			pPointedIcon->iAnimationState = CAIRO_DOCK_STATE_MOUSE_HOVERED;
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
		}
	}
}
static gboolean _cairo_dock_make_icon_glide (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon;
	GList *ic;
	gboolean bGliding = FALSE;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->iGlideDirection != 0)
		{
			icon->fGlideOffset += icon->iGlideDirection * .1;
			if (fabs (icon->fGlideOffset) > .99)
			{
				icon->fGlideOffset = icon->iGlideDirection;
				icon->iGlideDirection = 0;
			}
			else if (fabs (icon->fGlideOffset) < .01)
			{
				icon->fGlideOffset = 0;
				icon->iGlideDirection = 0;
			}
			//g_print ("  %s <- %.2f\n", icon->acName, icon->fGlideOffset);
			bGliding = TRUE;
		}
	}
	
	if (! bGliding)
	{
		g_print ("plus de glissement\n");
		pDock->iSidIconGlide = 0;
		return FALSE;
	}
	
	gtk_widget_queue_draw (pDock->pWidget);
	return TRUE;
}
void cairo_dock_stop_icon_glide (CairoDock *pDock)
{
	cd_message ("");
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fGlideOffset = 0;
		icon->iGlideDirection = 0;
	}
	if (pDock->iSidIconGlide != 0)
	{
		g_source_remove (pDock->iSidIconGlide);
		pDock->iSidIconGlide = 0;
	}
	///gtk_widget_queue_draw (pDock->pWidget);
}
gboolean cairo_dock_on_motion_notify (GtkWidget* pWidget,
	GdkEventMotion* pMotion,
	CairoDock *pDock)
{
	static double fLastTime = 0;
	if (s_bFrozenDock && pMotion != NULL && pMotion->time != 0)
		return FALSE;
	Icon *pPointedIcon=NULL, *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	int iLastMouseX = pDock->iMouseX;
	//g_print ("%s (%.2f;%.2f)\n", __func__, pMotion->x, pMotion->y);
	
	//\_______________ On elague le flux des MotionNotify, sinon X en envoie autant que le permet le CPU !
	if (pMotion != NULL)
	{
		//g_print ("%s (%d,%d) (%d, %.2fms, bAtBottom:%d; bIsShrinkingDown:%d)\n", __func__, (int) pMotion->x, (int) pMotion->y, pMotion->is_hint, pMotion->time - fLastTime, pDock->bAtBottom, pDock->bIsShrinkingDown);
		if ((pMotion->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) && (pMotion->state & GDK_BUTTON1_MASK))
		{
			//g_print ("mouse : (%d;%d); pointeur : (%d;%d)\n", pDock->iMouseX, pDock->iMouseY, (int) pMotion->x_root, (int) pMotion->y_root);
			if (pDock->bHorizontalDock)
			{
				//gtk_window_get_position (GTK_WINDOW (pDock->pWidget), &pDock->iWindowPositionX, &pDock->iWindowPositionY);
				pDock->iWindowPositionX = pMotion->x_root - pDock->iMouseX;
				pDock->iWindowPositionY = pMotion->y_root - pDock->iMouseY;
				gtk_window_move (GTK_WINDOW (pWidget),
					pDock->iWindowPositionX,
					pDock->iWindowPositionY);
			}
			else
			{
				pDock->iWindowPositionX = pMotion->y_root - pDock->iMouseX;
				pDock->iWindowPositionY = pMotion->x_root - pDock->iMouseY;
				gtk_window_move (GTK_WINDOW (pWidget),
					pDock->iWindowPositionY,
					pDock->iWindowPositionX);
			}
			gdk_device_get_state (pMotion->device, pMotion->window, NULL, NULL);
			return FALSE;
		}

		if (pDock->bHorizontalDock)
		{
			pDock->iMouseX = (int) pMotion->x;
			pDock->iMouseY = (int) pMotion->y;
		}
		else
		{
			pDock->iMouseX = (int) pMotion->y;
			pDock->iMouseY = (int) pMotion->x;
		}
		
		if (s_pFlyingContainer != NULL && ! pDock->bInside)
		{
			cairo_dock_drag_flying_container (s_pFlyingContainer, pDock);
		}
		
		if (pMotion->time != 0 && pMotion->time - fLastTime < mySystem.fRefreshInterval && s_pIconClicked == NULL)  // pDock->bIsShrinkingDown ||
		{
			gdk_device_get_state (pMotion->device, pMotion->window, NULL, NULL);
			return FALSE;
		}
		
		//\_______________ On recalcule toutes les icones.
		pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
		if (myIcons.fAmplitude != 0)
		{
			gtk_widget_queue_draw (pWidget);
		}
		fLastTime = pMotion->time;

		if (s_pIconClicked != NULL && s_pIconClicked->iAnimationState != CAIRO_DOCK_STATE_REMOVE_INSERT && ! g_bLocked)
		{
			s_pIconClicked->iAnimationState = CAIRO_DOCK_STATE_FOLLOW_MOUSE;
			//pDock->fAvoidingMouseMargin = .5;
			pDock->iAvoidingMouseIconType = s_pIconClicked->iType;  // on pourrait le faire lors du clic aussi.
			s_pIconClicked->fScale = 1 + myIcons.fAmplitude;
			s_pIconClicked->fDrawX = pDock->iMouseX  - s_pIconClicked->fWidth * s_pIconClicked->fScale / 2;
			s_pIconClicked->fDrawY = pDock->iMouseY - s_pIconClicked->fHeight * s_pIconClicked->fScale / 2 ;
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
		if (pDock->bHorizontalDock)
 			gdk_window_get_pointer (pWidget->window, &pDock->iMouseX, &pDock->iMouseY, NULL);
		else
			gdk_window_get_pointer (pWidget->window, &pDock->iMouseY, &pDock->iMouseX, NULL);

		pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
		gtk_widget_queue_draw (pWidget);
		
		pDock->iAvoidingMouseIconType = CAIRO_DOCK_LAUNCHER;
		pDock->fAvoidingMouseMargin = .25;
	}

	if (pDock->bInside)
	{
		if (mySystem.bDecorationsFollowMouse)
		{
			pDock->fDecorationsOffsetX = pDock->iMouseX - pDock->iCurrentWidth / 2;
			//g_print ("fDecorationsOffsetX <- %.2f\n", pDock->fDecorationsOffsetX);
		}
		else
		{
			if (pDock->iMouseX > iLastMouseX)
			{
				pDock->fDecorationsOffsetX += 10;
				if (pDock->fDecorationsOffsetX > pDock->iCurrentWidth / 2)
				{
					if (g_pBackgroundSurfaceFull[0] != NULL)
						pDock->fDecorationsOffsetX -= pDock->iCurrentWidth;
					else
						pDock->fDecorationsOffsetX = pDock->iCurrentWidth / 2;
				}
			}
			else
			{
				pDock->fDecorationsOffsetX -= 10;
				if (pDock->fDecorationsOffsetX < - pDock->iCurrentWidth / 2)
				{
					if (g_pBackgroundSurfaceFull[0] != NULL)
						pDock->fDecorationsOffsetX += pDock->iCurrentWidth;
					else
						pDock->fDecorationsOffsetX = - pDock->iCurrentWidth / 2;
				}
			}
		}
	}
	
	//g_print ("%x -> %x\n", pLastPointedIcon, pPointedIcon);
	if (pPointedIcon != pLastPointedIcon || s_pLastPointedDock == NULL)
	{
		cairo_dock_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);
		
		if (pPointedIcon != NULL && s_pIconClicked != NULL && cairo_dock_get_icon_order (s_pIconClicked) == cairo_dock_get_icon_order (pPointedIcon) && ! g_bLocked && ! myAccessibility.bLockIcons)
		{
			Icon *icon;
			GList *ic;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if (icon == s_pIconClicked)
					continue;
				//if (pDock->iMouseX > s_pIconClicked->fDrawXAtRest + s_pIconClicked->fWidth * s_pIconClicked->fScale /2)  // on a deplace l'icone a droite.  // fDrawXAtRest
				if (s_pIconClicked->fXAtRest < pPointedIcon->fXAtRest)  // on a deplace l'icone a droite.
				{
					//g_print ("%s : %.2f / %.2f ; %.2f / %d (%.2f)\n", icon->acName, icon->fXAtRest, s_pIconClicked->fXAtRest, icon->fDrawX, pDock->iMouseX, icon->fGlideOffset);
					if (icon->fXAtRest > s_pIconClicked->fXAtRest && icon->fDrawX < pDock->iMouseX + 1 && icon->fGlideOffset == 0)  // icone entre l'icone deplacee et le curseur.
					{
						//g_print ("  %s glisse vers la gauche\n", icon->acName);
						icon->iGlideDirection = -1;
					}
					else if (icon->fXAtRest > s_pIconClicked->fXAtRest && icon->fDrawX > pDock->iMouseX && icon->fGlideOffset != 0)
					{
						//g_print ("  %s glisse vers la droite\n", icon->acName);
						icon->iGlideDirection = 1;
					}
					else if (icon->fXAtRest < s_pIconClicked->fXAtRest && icon->fGlideOffset > 0)
					{
						//g_print ("  %s glisse en sens inverse vers la gauche\n", icon->acName);
						icon->iGlideDirection = -1;
					}
				}
				else
				{
					//g_print ("deplacement de %s vers la gauche (%.2f / %d)\n", icon->acName, icon->fDrawX + icon->fWidth * (1+myIcons.fAmplitude) + myIcons.iIconGap, pDock->iMouseX);
					if (icon->fXAtRest < s_pIconClicked->fXAtRest && icon->fDrawX + icon->fWidth * (1+myIcons.fAmplitude) + myIcons.iIconGap >= pDock->iMouseX && icon->fGlideOffset == 0)  // icone entre l'icone deplacee et le curseur.
					{
						//g_print ("  %s glisse vers la droite\n", icon->acName);
						icon->iGlideDirection = 1;
					}
					else if (icon->fXAtRest < s_pIconClicked->fXAtRest && icon->fDrawX + icon->fWidth * (1+myIcons.fAmplitude) + myIcons.iIconGap <= pDock->iMouseX && icon->fGlideOffset != 0)
					{
						//g_print ("  %s glisse vers la gauche\n", icon->acName);
						icon->iGlideDirection = -1;
					}
					else if (icon->fXAtRest > s_pIconClicked->fXAtRest && icon->fGlideOffset < 0)
					{
						//g_print ("  %s glisse en sens inverse vers la droite\n", icon->acName);
						icon->iGlideDirection = 1;
					}
				}
			}
			if (pDock->iSidIconGlide == 0)
			{
				pDock->iSidIconGlide = g_timeout_add (50, (GSourceFunc) _cairo_dock_make_icon_glide, pDock);
			}
		}
	}
	
	gboolean bStartAnimation = FALSE;
	cairo_dock_notify (CAIRO_DOCK_MOUSE_MOVED, pDock, &bStartAnimation);
	if (bStartAnimation)
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	
	return FALSE;
}

gboolean cairo_dock_emit_signal_on_dock (CairoDock *pDock, const gchar *cSignal)
{
	static gboolean bReturn;
	g_signal_emit_by_name (pDock->pWidget, cSignal, NULL, &bReturn);
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


void cairo_dock_leave_from_main_dock (CairoDock *pDock)
{
	//g_print ("%s (bMenuVisible:%d)\n", __func__, pDock->bMenuVisible);
	pDock->iAvoidingMouseIconType = -1;
	pDock->fAvoidingMouseMargin = 0;
	pDock->bInside = FALSE;
	pDock->bAtTop = FALSE;
	
	if (pDock->bMenuVisible)
	{
		return ;
	}
	
	if (pDock->iSidMoveUp != 0)  // si on est en train de monter, on arrete.
	{
		//g_print ("on arrete de monter\n");
		g_source_remove (pDock->iSidMoveUp);
		pDock->iSidMoveUp = 0;
	}
	if (pDock->bIsGrowingUp)  // si on est en train de faire grossir les icones, on arrete.
	{
		pDock->fFoldingFactor = 0;  /// utile ?...
		pDock->bIsGrowingUp = FALSE;
	}

	//s_pLastPointedDock = NULL;
	//g_print ("s_pLastPointedDock <- NULL\n");
	
	if (s_pIconClicked != NULL && (CAIRO_DOCK_IS_LAUNCHER (s_pIconClicked) || CAIRO_DOCK_IS_DETACHABLE_APPLET (s_pIconClicked) || CAIRO_DOCK_IS_USER_SEPARATOR(s_pIconClicked)) && s_pFlyingContainer == NULL && ! g_bLocked && ! myAccessibility.bLockIcons)
	{
		//g_print ("on a sorti %s du dock (%d;%d) / %dx%d\n", s_pIconClicked->acName, pDock->iMouseX, pDock->iMouseY, pDock->iCurrentWidth, pDock->iCurrentHeight);
		
		//if (! cairo_dock_hide_child_docks (pDock))  // on quitte si on entre dans un sous-dock, pour rester en position "haute".
		//	return ;
		
		CairoDock *pOriginDock = cairo_dock_search_dock_from_name (s_pIconClicked->cParentDockName);
		g_return_if_fail (pOriginDock != NULL);
		if (pOriginDock == pDock && _mouse_is_really_outside (pDock))  // ce test est la pour parer aux WM deficients mentaux comme KWin qui nous font sortir/rentrer lors d'un clic.
		{
			gchar *cParentDockName = s_pIconClicked->cParentDockName;
			s_pIconClicked->cParentDockName = NULL;
			cairo_dock_detach_icon_from_dock (s_pIconClicked, pOriginDock, TRUE);
			s_pIconClicked->cParentDockName = cParentDockName;
			cairo_dock_update_dock_size (pOriginDock);
			cairo_dock_stop_icon_glide (pOriginDock);
			
			s_pFlyingContainer = cairo_dock_create_flying_container (s_pIconClicked, pOriginDock, TRUE);
			g_print ("- s_pIconClicked <- NULL\n");
			s_pIconClicked = NULL;
			
			if (pDock->iRefCount > 0 || pDock->bAutoHide)  // pour garder le dock visible.
			{
				return;
			}
		}
	}
	else if (s_pFlyingContainer != NULL && s_pFlyingContainer->pIcon != NULL && pDock->iRefCount > 0)
	{
		CairoDock *pOriginDock = cairo_dock_search_dock_from_name (s_pFlyingContainer->pIcon->cParentDockName);
		if (pOriginDock == pDock)
			return;
	}
	
	if (pDock->iRefCount == 0)
	{
		if (pDock->bAutoHide)
		{
			pDock->fFoldingFactor = (mySystem.bAnimateOnAutoHide && mySystem.fUnfoldAcceleration != 0. ? 0.03 : 0.);
			if (pDock->iSidMoveDown == 0)  // on commence a descendre.
				pDock->iSidMoveDown = g_timeout_add (40, (GSourceFunc) cairo_dock_move_down, (gpointer) pDock);
		}
		///else  // mis en commentaire le 28/12/2008 pour les anims telles que 'rain'.
		///	pDock->bAtBottom = TRUE;
	}
	else
	{
		pDock->fFoldingFactor = (mySystem.bAnimateSubDock ? 0.03 : 0.);
		pDock->bAtBottom = TRUE;  // mis en commentaire le 12/11/07 pour permettre le quick-hide.
		//cd_debug ("on force bAtBottom");
	}
	
	///pDock->fDecorationsOffsetX = 0;
	cairo_dock_start_shrinking (pDock);  // on commence a faire diminuer la taille des icones.
}
gboolean cairo_dock_on_leave_notify (GtkWidget* pWidget, GdkEventCrossing* pEvent, CairoDock *pDock)
{
	if (g_bEasterEggs && pDock->bAtBottom)
		return FALSE;
	//g_print ("%s (bInside:%d; bAtBottom:%d; iRefCount:%d)\n", __func__, pDock->bInside, pDock->bAtBottom, pDock->iRefCount);
	/**if (pDock->bAtBottom)  // || ! pDock->bInside  // mis en commentaire pour la 1.5.4
	{
		pDock->iSidLeaveDemand = 0;
		return FALSE;
	}*/
	if (pEvent != NULL && (pEvent->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) && (pEvent->state & GDK_BUTTON1_MASK))
	{
		return FALSE;
	}
	//g_print ("%s (main dock : %d)\n", __func__, pDock->bIsMainDock);

	if (pDock->iRefCount == 0)
	{
		Icon *pPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
		if (pPointedIcon != NULL && pPointedIcon->pSubDock != NULL)
		{
			if (pDock->iSidLeaveDemand == 0)
			{
				//g_print ("  on retarde la sortie du dock de %dms\n", MAX (myAccessibility.iLeaveSubDockDelay, 330));
				pDock->iSidLeaveDemand = g_timeout_add (MAX (myAccessibility.iLeaveSubDockDelay, 330), (GSourceFunc) cairo_dock_emit_leave_signal, (gpointer) pDock);
				return TRUE;
			}
		}
	}
	else  // pEvent != NULL
	{
		if (pDock->iSidLeaveDemand == 0 && myAccessibility.iLeaveSubDockDelay != 0)
		{
			//g_print ("  on retarde la sortie du sous-dock de %dms\n", myAccessibility.iLeaveSubDockDelay);
			pDock->iSidLeaveDemand = g_timeout_add (myAccessibility.iLeaveSubDockDelay, (GSourceFunc) cairo_dock_emit_leave_signal, (gpointer) pDock);
			return TRUE;
		}
	}
	pDock->iSidLeaveDemand = 0;
	
	/*if (! pDock->bInside)
	{
		g_print ("on n'etait deja pas dedans\n");
		gtk_widget_hide (pDock->pWidget);
		return TRUE;
	}*/
	pDock->bInside = FALSE;
	//cd_debug (" on attend...");
	while (gtk_events_pending ())  // on laisse le temps au signal d'entree dans le sous-dock d'etre traite.
		gtk_main_iteration ();
	//cd_debug (" ==> pDock->bInside : %d", pDock->bInside);
	
	if (pDock->bInside)  // on est re-rentre dedans entre-temps.
		return TRUE;

	if (! cairo_dock_hide_child_docks (pDock))  // on quitte si on entre dans un sous-dock, pour rester en position "haute".
		return TRUE;
	
	if (pEvent != NULL)
	{
		pDock->iMouseX = pEvent->x;
		pDock->iMouseY = pEvent->y;
	}
	
	cairo_dock_leave_from_main_dock (pDock);

	return TRUE;
}

/// This function checks for the mouse cursor's position. If the mouse
/// cursor touches the edge of the screen upon which the dock is resting,
/// then the dock will pop up over other windows...
gboolean cairo_dock_poll_screen_edge (CairoDock *pDock)  // thanks to Smidgey for the pop-up patch !
{
	static int iPrevPointerX = -1, iPrevPointerY = -1;
	gint iMousePosX, iMousePosY;
	///static gint iSidPopUp = 0;
	
	if (pDock->iSidPopUp == 0 && !pDock->bPopped)
	{
		gdk_display_get_pointer(gdk_display_get_default(), NULL, &iMousePosX, &iMousePosY, NULL);
		if (iPrevPointerX == iMousePosX && iPrevPointerY == iMousePosY)
			return myAccessibility.bPopUp;
		
		iPrevPointerX = iMousePosX;
		iPrevPointerY = iMousePosY;
		
		CairoDockPositionType iScreenBorder1 = CAIRO_DOCK_INSIDE_SCREEN, iScreenBorder2 = CAIRO_DOCK_INSIDE_SCREEN;
		if (iMousePosY == 0)
		{
			iScreenBorder1 = CAIRO_DOCK_TOP;
		}
		else if (iMousePosY + 1 == g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL])
		{
			iScreenBorder1 = CAIRO_DOCK_BOTTOM;
		}
		if (iMousePosX == 0)
		{
			iScreenBorder2 = CAIRO_DOCK_LEFT;
		}
		else if (iMousePosX + 1 == g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL])
		{
			iScreenBorder2 = CAIRO_DOCK_RIGHT;
		}
		if (iScreenBorder1 == CAIRO_DOCK_INSIDE_SCREEN && iScreenBorder2 == CAIRO_DOCK_INSIDE_SCREEN)
			return myAccessibility.bPopUp;
		if ((iScreenBorder1 != CAIRO_DOCK_INSIDE_SCREEN && iScreenBorder2 != CAIRO_DOCK_INSIDE_SCREEN) || myAccessibility.bPopUpOnScreenBorder)
		{
			if (iScreenBorder1 != CAIRO_DOCK_INSIDE_SCREEN)
				cairo_dock_pop_up_root_docks_on_screen_edge (iScreenBorder1);
			if (iScreenBorder2 != CAIRO_DOCK_INSIDE_SCREEN)
				cairo_dock_pop_up_root_docks_on_screen_edge (iScreenBorder2);
		}
	}
	
	return myAccessibility.bPopUp;
}

gboolean cairo_dock_on_enter_notify (GtkWidget* pWidget, GdkEventCrossing* pEvent, CairoDock *pDock)
{
	if (pEvent && g_bEasterEggs && pDock->pShapeBitmap)
	{
		int x = (pDock->bHorizontalDock ? pEvent->x : pDock->iCurrentWidth - pEvent->y);
		if (x < pDock->inputArea.x * pDock->fRatio || x > (pDock->inputArea.x + pDock->inputArea.width) * pDock->fRatio)
			return FALSE;
	}
	//g_print ("%s (bIsMainDock : %d; bAtTop:%d; bInside:%d; iSidMoveDown:%d; iMagnitudeIndex:%d)\n", __func__, pDock->bIsMainDock, pDock->bAtTop, pDock->bInside, pDock->iSidMoveDown, pDock->iMagnitudeIndex);
	s_pLastPointedDock = NULL;  // ajoute le 04/10/07 pour permettre aux sous-docks d'apparaitre si on entre en pointant tout de suite sur l'icone.
	if (! cairo_dock_entrance_is_allowed (pDock))
	{
		cd_message ("* entree non autorisee");
		return FALSE;
	}

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
	
	if (pDock->iSidMoveDown == 0)
	{
		gboolean bStartAnimation = FALSE;
		cairo_dock_notify (CAIRO_DOCK_ENTER_DOCK, pDock, &bStartAnimation);
		if (bStartAnimation)
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
	
	if (pDock->bAtTop || pDock->bInside || (pDock->iSidMoveDown != 0))  // le 'iSidMoveDown != 0' est la pour empecher le dock de "vibrer" si l'utilisateur sort par en bas avec l'auto-hide active.
	{
		//g_print ("  %d;%d;%d\n", pDock->bAtTop,  pDock->bInside, pDock->iSidMoveDown);
		pDock->bInside = TRUE;  /// ajoute pour les plug-ins opengl.
		cairo_dock_start_growing (pDock);
		if (pDock->bAutoHide && pDock->iRefCount == 0 && pDock->iSidMoveUp == 0)
		{
			g_print ("  on commence a monter en force\n");
			pDock->iSidMoveUp = g_timeout_add (40, (GSourceFunc) cairo_dock_move_up, (gpointer) pDock);
		}
		return FALSE;
	}
	//g_print ("%s (main dock : %d ; %d)\n", __func__, pDock->bIsMainDock, pDock->bHorizontalDock);

	/**if (g_bUseOpenGL && pDock->render_opengl != NULL)
	{
		gboolean bStartAnimation = FALSE;
		cairo_dock_notify (CAIRO_DOCK_ENTER_DOCK, pDock, &bStartAnimation);
		if (bStartAnimation)
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}*/
	
	pDock->fDecorationsOffsetX = 0;
	if (pDock->iRefCount != 0)
	{
		gtk_window_present (GTK_WINDOW (pWidget));  /// utile ???
	}
	pDock->bInside = TRUE;
	
	cairo_dock_stop_quick_hide ();

	if (s_pIconClicked != NULL)  // on pourrait le faire a chaque motion aussi.
	{
		pDock->iAvoidingMouseIconType = s_pIconClicked->iType;
		pDock->fAvoidingMouseMargin = .5;
	}
	
	if (s_pFlyingContainer != NULL)
	{
		Icon *pFlyingIcon = s_pFlyingContainer->pIcon;
		g_print ("on remet l'icone volante dans son dock d'origine (%s)\n", pFlyingIcon->cParentDockName);
		cairo_dock_free_flying_container (s_pFlyingContainer);
		cairo_dock_insert_icon_in_dock (pFlyingIcon, pDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
		cairo_dock_start_icon_animation (pFlyingIcon, pDock);
		s_pFlyingContainer = NULL;
	}
	
	int iNewWidth, iNewHeight;
	cairo_dock_get_window_position_and_geometry_at_balance (pDock, CAIRO_DOCK_MAX_SIZE, &iNewWidth, &iNewHeight);
	if ((pDock->bAutoHide && pDock->iRefCount == 0) && pDock->bAtBottom)
		pDock->iWindowPositionY = (pDock->bDirectionUp ? g_iXScreenHeight[pDock->bHorizontalDock] - myHiddenDock.iVisibleZoneHeight - pDock->iGapY : myHiddenDock.iVisibleZoneHeight + pDock->iGapY - pDock->iMaxDockHeight);
	
	if (pDock->iCurrentWidth != iNewWidth || pDock->iCurrentHeight != iNewHeight)
	{
		if (pDock->bHorizontalDock)
			gdk_window_move_resize (pWidget->window,
				pDock->iWindowPositionX,
				pDock->iWindowPositionY,
				iNewWidth,
				iNewHeight);
		else
			gdk_window_move_resize (pWidget->window,
				pDock->iWindowPositionY,
				pDock->iWindowPositionX,
				iNewHeight,
				iNewWidth);
	}
	
	if (pDock->iSidMoveDown > 0)  // si on est en train de descendre, on arrete.
	{
		//g_print ("  on est en train de descendre, on arrete\n");
		g_source_remove (pDock->iSidMoveDown);
		pDock->iSidMoveDown = 0;
	}
	
	if (myAccessibility.bPopUp && pDock->iRefCount == 0)
	{
		//This code will trigger a pop up
		cairo_dock_pop_up (pDock);
		//If the dock window is entered, and there is a pending drop below event then it should be cancelled
		if (pDock->iSidPopDown != 0)
		{
			g_source_remove(pDock->iSidPopDown);
			pDock->iSidPopDown = 0;
		}
	}
	
	if (pDock->bAutoHide && pDock->iRefCount == 0)
	{
		//g_print ("  on commence a monter\n");
		if (pDock->iSidMoveUp == 0)  // on commence a monter.
			pDock->iSidMoveUp = g_timeout_add (40, (GSourceFunc) cairo_dock_move_up, (gpointer) pDock);
	}
	else
	{
		if (pDock->iRefCount > 0)
			pDock->bAtTop = TRUE;
		pDock->bAtBottom = FALSE;
	}
	
	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	if (icon != NULL)
	{
		//g_print (">>> we've just entered the dock, pointed icon becomes NULL\n");
		//if (s_pIconClicked != NULL)
		//	g_print (">>> on est rentre par un clic ! (KDE:%d)\n", g_iDesktopEnv == CAIRO_DOCK_KDE);
		if (_mouse_is_really_outside (pDock))  // ce test est la pour parer aux WM deficients mentaux comme KWin qui nous font sortir/rentrer lors d'un clic.
			icon->bPointed = FALSE;  // sinon on ne detecte pas l'arrive sur l'icone, c'est genant si elle a un sous-dock.
		else
			g_print (">>> we already are inside the dock, why does this stupid WM make us enter one more time ???\n");
	}
	
	cairo_dock_start_growing (pDock);

	return FALSE;
}


gboolean cairo_dock_on_key_release (GtkWidget *pWidget,
	GdkEventKey *pKey,
	CairoDock *pDock)
{
	g_print ("on a appuye sur une touche\n");
	if (pKey->type == GDK_KEY_PRESS)
	{
		cairo_dock_notify (CAIRO_DOCK_KEY_PRESSED, pDock, pKey->keyval, pKey->state, pKey->string);
	}
	else if (pKey->type == GDK_KEY_RELEASE)
	{
		g_print ("release : pKey->keyval = %d\n", pKey->keyval);
		if ((pKey->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) && pKey->keyval == 0)  // On relache la touche ALT, typiquement apres avoir fait un ALT + clique gauche + deplacement.
		{
			if (pDock->iRefCount == 0)
				cairo_dock_write_root_dock_gaps (pDock);
		}
	}
	return FALSE;
}

gchar *cairo_dock_launch_command_sync (const gchar *cCommand)
{
	gchar *standard_output=NULL, *standard_error=NULL;
	gint exit_status=0;
	GError *erreur = NULL;
	gboolean r = g_spawn_command_line_sync (cCommand,
		&standard_output,
		&standard_error,
		&exit_status,
		&erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		g_free (standard_error);
		return NULL;
	}
	if (standard_error != NULL && *standard_error != '\0')
	{
		cd_warning (standard_error);
	}
	g_free (standard_error);
	if (standard_output != NULL && *standard_output == '\0')
	{
		g_free (standard_output);
		return NULL;
	}
	return standard_output;
}

static gpointer _cairo_dock_launch_threaded (gchar *cCommand)
{
	int r;
	r = system (cCommand);
	g_free (cCommand);
	return NULL;
}
gboolean cairo_dock_launch_command_full (const gchar *cCommandFormat, gchar *cWorkingDirectory, ...)
{
	g_return_val_if_fail (cCommandFormat != NULL, FALSE);
	
	va_list args;
	va_start (args, cWorkingDirectory);
	gchar *cCommand = g_strdup_vprintf (cCommandFormat, args);
	va_end (args);
	cd_debug ("%s (%s , %s)", __func__, cCommand, cWorkingDirectory);
	
	gchar *cBGCommand;
	if (cCommand[strlen (cCommand)-1] != '&')
	{
		cBGCommand = g_strconcat (cCommand, " &", NULL);
		g_free (cCommand);
	}
	else
		cBGCommand = cCommand;
	if (cWorkingDirectory != NULL)
	{
		cCommand = g_strdup_printf ("cd '%s' && %s", cWorkingDirectory, cBGCommand);
		g_free (cBGCommand);
		cBGCommand = cCommand;
	}
	GError *erreur = NULL;
	GThread* pThread = g_thread_create ((GThreadFunc) _cairo_dock_launch_threaded, cBGCommand, FALSE, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("couldn't launch this command (%s : %s)", cBGCommand, erreur->message);
		g_error_free (erreur);
		g_free (cBGCommand);
		return FALSE;
	}
	return TRUE;
}

static int _compare_zorder (Icon *icon1, Icon *icon2)  // classe par z-order decroissant.
{
	if (icon1->iStackOrder < icon2->iStackOrder)
		return -1;
	else if (icon1->iStackOrder > icon2->iStackOrder)
		return 1;
	else
		return 0;
}
static void _cairo_dock_hide_show_in_class_subdock (Icon *icon)
{
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->Xid != 0 && ! pIcon->bIsHidden)  // par defaut on cache tout.
		{
			break;
		}
	}
	
	if (ic != NULL)  // au moins une fenetre est visible, on cache tout.
	{
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->Xid != 0 && ! pIcon->bIsHidden)
			{
				cairo_dock_minimize_xwindow (pIcon->Xid);
			}
		}
	}
	else  // on montre tout, dans l'ordre du z-order.
	{
		GList *pZOrderList = NULL;
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->Xid != 0)
				pZOrderList = g_list_insert_sorted (pZOrderList, pIcon, (GCompareFunc) _compare_zorder);
		}
		for (ic = pZOrderList; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			cairo_dock_show_xwindow (pIcon->Xid);
		}
		g_list_free (pZOrderList);
	}
}

static void _cairo_dock_show_prev_next_in_class_subdock (Icon *icon, gboolean bNext)
{
	Icon *pActiveIcon = cairo_dock_get_current_active_icon ();
	Icon *pNextIcon;
	if (pActiveIcon != NULL)
	{
		if (bNext)
		{
			pNextIcon = cairo_dock_get_next_icon (icon->pSubDock->icons, pActiveIcon);
			if (pNextIcon == NULL)  // pas trouvee ou derniere de la liste.
				pNextIcon = cairo_dock_get_first_icon (icon->pSubDock->icons);
		}
		else
		{
			pNextIcon = cairo_dock_get_previous_icon (icon->pSubDock->icons, pActiveIcon);
			if (pNextIcon == NULL)  // pas trouvee ou premiere de la liste.
				pNextIcon = cairo_dock_get_last_icon (icon->pSubDock->icons);
		}
	}
	else
	{
		pNextIcon = cairo_dock_get_first_icon (icon->pSubDock->icons);
	}
	if (pNextIcon != NULL)
		cairo_dock_show_xwindow (pNextIcon->Xid);
}

static void _cairo_dock_close_all_in_class_subdock (Icon *icon)
{
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->Xid != 0)
		{
			cairo_dock_close_xwindow (pIcon->Xid);
		}
	}
}


gboolean cairo_dock_notification_click_icon (gpointer pUserData, Icon *icon, CairoDock *pDock, guint iButtonState)
{
	g_print ("+ %s (%s)\n", __func__, icon ? icon->acName : "no icon");
	if (icon == NULL)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	if (icon->pSubDock != NULL && myAccessibility.bShowSubDockOnClick)  // icone de sous-dock a montrer au clic.
	{
		cairo_dock_show_subdock (icon, FALSE, pDock);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_IS_URI_LAUNCHER (icon))  // URI : on lance ou on monte.
	{
		cd_debug (" uri launcher");
		gboolean bIsMounted = FALSE;
		if (icon->iVolumeID > 0)
		{
			gchar *cActivationURI = cairo_dock_fm_is_mounted (icon->cBaseURI, &bIsMounted);
			g_free (cActivationURI);
		}
		if (icon->iVolumeID > 0 && ! bIsMounted)
		{
			int answer = cairo_dock_ask_question_and_wait (_("Do you want to mount this point ?"), icon, CAIRO_CONTAINER (pDock));
			if (answer != GTK_RESPONSE_YES)
			{
				return CAIRO_DOCK_LET_PASS_NOTIFICATION;
			}
			cairo_dock_fm_mount (icon, CAIRO_CONTAINER (pDock));
		}
		else
			cairo_dock_fm_launch_uri (icon->acCommand);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_IS_APPLI (icon) && ! ((iButtonState & GDK_SHIFT_MASK) && CAIRO_DOCK_IS_LAUNCHER (icon)) && ! CAIRO_DOCK_IS_APPLET (icon))  // une icone d'appli ou d'inhibiteur (hors applet) mais sans le shift+clic : on cache ou on montre.
	{
		cd_debug (" appli");
		if (cairo_dock_get_current_active_window () == icon->Xid && myTaskBar.bMinimizeOnClick)  // ne marche que si le dock est une fenêtre de type 'dock', sinon il prend le focus.
			cairo_dock_minimize_xwindow (icon->Xid);
		else
			cairo_dock_show_xwindow (icon->Xid);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	else if (CAIRO_DOCK_IS_LAUNCHER (icon))
	{
		g_print ("+ launcher\n");
		if (CAIRO_DOCK_IS_MULTI_APPLI (icon) && ! (iButtonState & GDK_SHIFT_MASK))  // un lanceur ayant un sous-dock de classe ou une icone de paille : on cache ou on montre.
		{
			if (! myAccessibility.bShowSubDockOnClick)
			{
				_cairo_dock_hide_show_in_class_subdock (icon);
			}
			return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
		}
		else if (icon->acCommand != NULL && strcmp (icon->acCommand, "none") != 0)  // finalement, on lance la commande.
		{
			gboolean bSuccess = FALSE;
			if (*icon->acCommand == '<')
			{
				bSuccess = cairo_dock_simulate_key_sequence (icon->acCommand);
				if (!bSuccess)
					bSuccess = cairo_dock_launch_command_full (icon->acCommand, icon->cWorkingDirectory);
			}
			else
			{
				bSuccess = cairo_dock_launch_command_full (icon->acCommand, icon->cWorkingDirectory);
				if (! bSuccess)
					bSuccess = cairo_dock_simulate_key_sequence (icon->acCommand);
			}
			if (! bSuccess)
			{
				cairo_dock_request_icon_animation (icon, pDock, "blink", 1);  // 1 clignotement si echec
			}
			return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
		}
	}
	else
		g_print ("no action here\n");
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


gboolean cairo_dock_notification_middle_click_icon (gpointer pUserData, Icon *icon, CairoDock *pDock)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.bCloseAppliOnMiddleClick && ! CAIRO_DOCK_IS_APPLET (icon))
	{
		cairo_dock_close_xwindow (icon->Xid);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	if (CAIRO_DOCK_IS_URI_LAUNCHER (icon) && icon->pSubDock != NULL)  // icone de repertoire.
	{
		cairo_dock_fm_launch_uri (icon->acCommand);
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	if (CAIRO_DOCK_IS_MULTI_APPLI (icon))
	{
		// On ferme tout.
		_cairo_dock_close_all_in_class_subdock (icon);
		
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_on_button_press (GtkWidget* pWidget, GdkEventButton* pButton, CairoDock *pDock)
{
	g_print ("+ %s (%d/%d)\n", __func__, pButton->type, pButton->button);
	if (pDock->bHorizontalDock)  // utile ?
	{
		pDock->iMouseX = (int) pButton->x;
		pDock->iMouseY = (int) pButton->y;
	}
	else
	{
		pDock->iMouseX = (int) pButton->y;
		pDock->iMouseY = (int) pButton->x;
	}

	Icon *icon = cairo_dock_get_pointed_icon (pDock->icons);
	if (pButton->button == 1)  // clic gauche.
	{
		g_print ("+ left click\n");
		switch (pButton->type)
		{
			case GDK_BUTTON_RELEASE :
				g_print ("+ GDK_BUTTON_RELEASE (%d/%d sur %s/%s)\n", pButton->state, GDK_CONTROL_MASK | GDK_MOD1_MASK, icon ? icon->acName : "personne", icon ? icon->acCommand : "");  // 272 = 100010000
				if ( ! (pButton->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
				{
					if (s_pIconClicked != NULL)
					{
						g_print ("release de %s (inside:%d)\n", s_pIconClicked->acName, pDock->bInside);
						s_pIconClicked->iAnimationState = CAIRO_DOCK_STATE_REST;  // stoppe les animations de suivi du curseur.
						//cairo_dock_stop_marking_icons (pDock);
						pDock->iAvoidingMouseIconType = -1;
						cairo_dock_stop_icon_glide (pDock);
					}
					if (icon != NULL && ! CAIRO_DOCK_IS_SEPARATOR (icon) && icon == s_pIconClicked)
					{
						s_pIconClicked = NULL;  // il faut le faire ici au cas ou le clic induirait un dialogue bloquant qui nous ferait sortir du dock par exemple.
						g_print ("+ click on '%s' (%s)\n", icon->acName, icon->acCommand);
						cairo_dock_notify (CAIRO_DOCK_CLICK_ICON, icon, pDock, pButton->state);
						if (myAccessibility.cRaiseDockShortcut != NULL)
							s_bHideAfterShortcut = TRUE;
						
						cairo_dock_start_icon_animation (icon, pDock);
					}
					else if (s_pIconClicked != NULL && icon != NULL && icon != s_pIconClicked && ! g_bLocked && ! myAccessibility.bLockIcons)  //  && icon->iType == s_pIconClicked->iType
					{
						g_print ("deplacement de %s\n", s_pIconClicked->acName);
						CairoDock *pOriginDock = CAIRO_DOCK (cairo_dock_search_container_from_icon (s_pIconClicked));
						if (pOriginDock != NULL && pDock != pOriginDock)
						{
							cairo_dock_detach_icon_from_dock (s_pIconClicked, pOriginDock, TRUE);  // plutot que 'cairo_dock_remove_icon_from_dock', afin de ne pas la fermer.
							///cairo_dock_remove_icon_from_dock (pOriginDock, s_pIconClicked);
							cairo_dock_update_dock_size (pOriginDock);

							cairo_dock_update_icon_s_container_name (s_pIconClicked, icon->cParentDockName);
							if (pOriginDock->iRefCount > 0 && ! myViews.bSameHorizontality)
							{
								cairo_t* pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
								cairo_dock_fill_one_text_buffer (s_pIconClicked, pSourceContext, &myLabels.iconTextDescription);
								cairo_destroy (pSourceContext);
							}

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
						//g_print ("deplacement de %s\n", s_pIconClicked->acName);
						if (prev_icon != NULL && cairo_dock_get_icon_order (prev_icon) != cairo_dock_get_icon_order (s_pIconClicked))
							prev_icon = NULL;
						cairo_dock_move_icon_after_icon (pDock, s_pIconClicked, prev_icon);

						pDock->calculate_icons (pDock);

						if (! CAIRO_DOCK_IS_SEPARATOR (s_pIconClicked))
						{
							cairo_dock_request_icon_animation (s_pIconClicked, pDock, "bounce", 2);
						}
						if (pDock->iSidGLAnimation == 0 || ! CAIRO_DOCK_CONTAINER_IS_OPENGL (CAIRO_CONTAINER (pDock)))
							gtk_widget_queue_draw (pDock->pWidget);
					}
					
					if (s_pFlyingContainer != NULL)
					{
						g_print ("on relache l'icone volante\n");
						if (pDock->bInside)
						{
							g_print ("  on la remet dans son dock d'origine\n");
							Icon *pFlyingIcon = s_pFlyingContainer->pIcon;
							cairo_dock_free_flying_container (s_pFlyingContainer);
							cairo_dock_stop_marking_icon_as_following_mouse (pFlyingIcon);
							cairo_dock_insert_icon_in_dock (pFlyingIcon, pDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
							cairo_dock_start_icon_animation (pFlyingIcon, pDock);
						}
						else
						{
							cairo_dock_terminate_flying_container (s_pFlyingContainer);  // supprime ou detache l'icone, l'animation se terminera toute seule.
						}
						s_pFlyingContainer = NULL;
						//cairo_dock_stop_marking_icons (pDock);
						cairo_dock_stop_icon_glide (pDock);
					}
				}
				else
				{
					if (pDock->iRefCount == 0)
						cairo_dock_write_root_dock_gaps (pDock);
				}
				g_print ("- apres clic : s_pIconClicked <- NULL\n");
				s_pIconClicked = NULL;
			break ;

			case GDK_BUTTON_PRESS :
				if ( ! (pButton->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
				{
					g_print ("+ clic sur %s (%.2f)!\n", icon ? icon->acName : "rien", icon ? icon->fPersonnalScale : 0.);
					if (icon && icon->fPersonnalScale <= 0)
						s_pIconClicked = icon;  // on ne definit pas l'animation FOLLOW_MOUSE ici , on le fera apres le 1er mouvement, pour eviter que l'icone soit dessinee comme tel quand on clique dessus alors que le dock est en train de jouer une animation (ca provoque un flash desagreable).
					else
						s_pIconClicked = NULL;
				}
			break ;

			case GDK_2BUTTON_PRESS :
				{
					cairo_dock_notify (CAIRO_DOCK_DOUBLE_CLICK_ICON, icon, pDock);
				}
			break ;

			default :
			break ;
		}
	}
	else if (pButton->button == 3 && pButton->type == GDK_BUTTON_PRESS)  // clique droit.
	{
		pDock->bMenuVisible = TRUE;
		GtkWidget *menu = cairo_dock_build_menu (icon, CAIRO_CONTAINER (pDock));  // genere un CAIRO_DOCK_BUILD_MENU.

		gtk_widget_show_all (menu);

		gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			NULL,
			NULL,
			1,
			gtk_get_current_event_time ());
	}
	else if (pButton->button == 2 && pButton->type == GDK_BUTTON_PRESS)  // clique milieu.
	{
		cairo_dock_notify (CAIRO_DOCK_MIDDLE_CLICK_ICON, icon, pDock);
	}

	return FALSE;
}


gboolean cairo_dock_notification_scroll_icon (gpointer pUserData, Icon *icon, CairoDock *pDock, int iDirection)
{
	if (CAIRO_DOCK_IS_MULTI_APPLI (icon))  // on emule un alt+tab sur la liste des applis du sous-dock.
	{
		_cairo_dock_show_prev_next_in_class_subdock (icon, iDirection == GDK_SCROLL_DOWN);
	}
	else if (CAIRO_DOCK_IS_APPLI (icon) && icon->cClass != NULL)
	{
		Icon *pNextIcon = cairo_dock_get_prev_next_classmate_icon (icon, iDirection == GDK_SCROLL_DOWN);
		if (pNextIcon != NULL)
			cairo_dock_show_xwindow (pNextIcon->Xid);
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}
gboolean cairo_dock_on_scroll (GtkWidget* pWidget, GdkEventScroll* pScroll, CairoDock *pDock)
{
	if (pScroll->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
	{
		if (myAccessibility.bLockIcons)
			return FALSE;
		
		int iScrollAmount = 0;
		Icon *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
		Icon *pNeighborIcon;
		if (pScroll->direction == GDK_SCROLL_UP)
		{
			pNeighborIcon = cairo_dock_get_previous_icon (pDock->icons, pLastPointedIcon);
			if (pNeighborIcon == NULL)
				pNeighborIcon = cairo_dock_get_last_icon (pDock->icons);
			iScrollAmount = (pNeighborIcon->fWidth + (pLastPointedIcon != NULL ? pLastPointedIcon->fWidth : 0)) / 2;
		}
		else if (pScroll->direction == GDK_SCROLL_DOWN)
		{
			pNeighborIcon = cairo_dock_get_next_icon (pDock->icons, pLastPointedIcon);
			if (pNeighborIcon == NULL)
				pNeighborIcon = cairo_dock_get_first_icon (pDock->icons);
			iScrollAmount = - (pNeighborIcon->fWidth + (pLastPointedIcon != NULL ? pLastPointedIcon->fWidth : 0)) / 2;
		}
		
		cairo_dock_scroll_dock_icons (pDock, iScrollAmount);
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
	gint iNewWidth, iNewHeight;
	if (pDock->bHorizontalDock)
	{
		iNewWidth = pEvent->width;
		iNewHeight = pEvent->height;
	}
	else
	{
		iNewWidth = pEvent->height;
		iNewHeight = pEvent->width;
	}

	if ((iNewWidth != pDock->iCurrentWidth || iNewHeight != pDock->iCurrentHeight) && iNewWidth > 1)
	{
		//g_print ("-> %dx%d\n", iNewWidth, iNewHeight);
		pDock->iCurrentWidth = iNewWidth;
		pDock->iCurrentHeight = iNewHeight;

		if (pDock->bHorizontalDock)
			gdk_window_get_pointer (pWidget->window, &pDock->iMouseX, &pDock->iMouseY, NULL);
		else
			gdk_window_get_pointer (pWidget->window, &pDock->iMouseY, &pDock->iMouseX, NULL);
		if (pDock->iMouseX < 0 || pDock->iMouseX > pDock->iCurrentWidth)  // utile ?
			pDock->iMouseX = 0;
		
		if (pDock->pShapeBitmap != NULL)
		{
			gtk_widget_input_shape_combine_mask (pDock->pWidget,
				(pDock->bAtBottom && pDock->iRefCount == 0 ? pDock->pShapeBitmap : NULL),
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
			
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, w, 0, h, 0.0, 500.0);
			//gluPerspective(30.0, 1.0*(GLfloat)w/(GLfloat)h, 1.0, 500.0);
			
			glMatrixMode (GL_MODELVIEW);
			glLoadIdentity ();
			gluLookAt (w/2, h/2, 3.,
				w/2, h/2, 0.,
				0.0f, 1.0f, 0.0f);
			glTranslatef (0.0f, 0.0f, -3.);
			
			glClearAccum (0., 0., 0., 0.);
			glClear (GL_ACCUM_BUFFER_BIT);
			
			gdk_gl_drawable_gl_end (pGlDrawable);
		}
		
		#ifdef HAVE_GLITZ
		if (pDock->pGlitzDrawable)
		{
			glitz_drawable_update_size (pDock->pGlitzDrawable,
				pEvent->width,
				pEvent->height);
		}
		#endif
		
		cairo_dock_calculate_dock_icons (pDock);
		gtk_widget_queue_draw (pWidget);  // il semble qu'il soit necessaire d'en rajouter un la pour eviter un "clignotement" a l'entree dans le dock.
		
		//g_print ("debut du redessin\n");
		//if (pDock->iRefCount > 0 || pDock->bAutoHide)
			while (gtk_events_pending ())  // on force un redessin immediat sinon on a quand meme un "flash".
				gtk_main_iteration ();
		//g_print ("fin du redessin\n");
	}
	
	if (pDock->iSidMoveDown == 0 && pDock->iSidMoveUp == 0)  // ce n'est pas du a une animation. Donc en cas d'apparition due a l'auto-hide, ceci ne sera pas fait ici, mais a la fin de l'animation.
	{
		cairo_dock_set_icons_geometry_for_window_manager (pDock);

		cairo_dock_replace_all_dialogs ();
	}
	
	return FALSE;
}


void cairo_dock_on_drag_data_received (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t, CairoDock *pDock)
{
	//g_print ("%s (%dx%d)\n", __func__, x, y);
	//\_________________ On recupere l'URI.
	gchar *cReceivedData = (gchar *) selection_data->data;
	g_return_if_fail (cReceivedData != NULL);
	int length = strlen (cReceivedData);
	if (cReceivedData[length-1] == '\n')
		cReceivedData[--length] = '\0';  // on vire le retour chariot final.
	if (cReceivedData[length-1] == '\r')
		cReceivedData[--length] = '\0';
	
	/*if (pDock->iAvoidingMouseIconType == -1)
	{
		g_print ("drag info : <%s>\n", cReceivedData);
		pDock->iAvoidingMouseIconType = CAIRO_DOCK_LAUNCHER;
		pDock->fAvoidingMouseMargin = .25;
		return ;
	}*/
	
	//\_________________ On arrete l'animation.
	//cairo_dock_stop_marking_icons (pDock);
	pDock->iAvoidingMouseIconType = -1;
	pDock->fAvoidingMouseMargin = 0;
	
	//\_________________ On calcule la position a laquelle on l'a lache.
	cd_message (">>> cReceivedData : %s", cReceivedData);
	double fOrder = CAIRO_DOCK_LAST_ORDER;
	Icon *pPointedIcon = NULL, *pNeighboorIcon = NULL;
	GList *ic;
	Icon *icon;
	int iDropX = (pDock->bHorizontalDock ? x : y);
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->bPointed)
		{
			//g_print ("On pointe sur %s\n", icon->acName);
			pPointedIcon = icon;
			double fMargin;
			if (g_str_has_suffix (cReceivedData, ".desktop"))  // si c'est un .desktop, on l'ajoute.
				fMargin = 0.5;  // on ne sera jamais dessus.
			else  // sinon on le lance si on est sur l'icone, et on l'ajoute autrement.
				fMargin = 0.25;

			if (iDropX > icon->fX + icon->fWidth * icon->fScale * (1 - fMargin))  // on est apres.
			{
				pNeighboorIcon = (ic->next != NULL ? ic->next->data : NULL);
				fOrder = (pNeighboorIcon != NULL ? (icon->fOrder + pNeighboorIcon->fOrder) / 2 : icon->fOrder + 1);
			}
			else if (iDropX < icon->fX + icon->fWidth * icon->fScale * fMargin)  // on est avant.
			{
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
}


gboolean cairo_dock_notification_drop_data (gpointer pUserData, const gchar *cReceivedData, Icon *icon, double fOrder, CairoContainer *pContainer)
{
	if (! CAIRO_DOCK_IS_DOCK (pContainer))
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	if (icon == NULL || CAIRO_DOCK_IS_LAUNCHER (icon) || CAIRO_DOCK_IS_SEPARATOR (icon))
	{
		CairoDock *pReceivingDock = pDock;
		if (g_str_has_suffix (cReceivedData, ".desktop"))  // c'est un fichier .desktop, on choisit de l'ajouter quoiqu'il arrive.
		{
			if (fOrder == CAIRO_DOCK_LAST_ORDER)  // on a lache dessus.
			{
				if (icon && icon->pSubDock != NULL)  // on l'ajoutera au sous-dock.
				{
					pReceivingDock = icon->pSubDock;
				}
			}
		}
		else  // c'est un fichier.
		{
			if (fOrder == CAIRO_DOCK_LAST_ORDER)  // on a lache dessus.
			{
				if (CAIRO_DOCK_IS_LAUNCHER (icon))
				{
					if (CAIRO_DOCK_IS_URI_LAUNCHER (icon))
					{
						if (icon->pSubDock != NULL || icon->iVolumeID != 0)  // on le lache sur un repertoire ou un point de montage.
						{
							cairo_dock_fm_move_into_directory (cReceivedData, icon, pContainer);
							return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
						}
						else  // on le lache sur un fichier.
						{
							return CAIRO_DOCK_LET_PASS_NOTIFICATION;
						}
					}
					else if (icon->pSubDock != NULL)  // on le lache sur un sous-dock de lanceurs.
					{
						pReceivingDock = icon->pSubDock;
					}
					else  // on le lache sur un lanceur.
					{
						gchar *cCommand = g_strdup_printf ("%s '%s'", icon->acCommand, cReceivedData);
						g_spawn_command_line_async (cCommand, NULL);
						g_free (cCommand);
						cairo_dock_request_icon_animation (icon, pDock, "blink", 2);
						return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
					}
				}
				else  // on le lache sur autre chose qu'un lanceur.
				{
					return CAIRO_DOCK_LET_PASS_NOTIFICATION;
				}
			}
			else  // on a lache a cote.
			{
				Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
				if (CAIRO_DOCK_IS_URI_LAUNCHER (pPointingIcon))  // on a lache dans un dock qui est un repertoire, on copie donc le fichier dedans.
				{
					cairo_dock_fm_move_into_directory (cReceivedData, icon, pContainer);
					return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
				}
			}
		}

		if (g_bLocked)
			return CAIRO_DOCK_LET_PASS_NOTIFICATION;
		
		cairo_dock_add_new_launcher_by_uri (cReceivedData, pReceivingDock, fOrder);
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


void cairo_dock_on_drag_motion (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, guint time, CairoDock *pDock)
{
	//g_print ("%s (%dx%d, %d)\n", __func__, x, y, time);
	//\_________________ On simule les evenements souris habituels.
	if (! pDock->bIsDragging)
	{
		cd_message ("start dragging");
		
		/*GdkAtom gdkAtom = gdk_drag_get_selection (dc);
		Atom xAtom = gdk_x11_atom_to_xatom (gdkAtom);
		
		Window Xid = GDK_WINDOW_XID (dc->source_window);
		g_print (" <%s>\n", cairo_dock_get_property_name_on_xwindow (Xid, xAtom));*/
		
		pDock->bIsDragging = TRUE;
		
		gboolean bStartAnimation = FALSE;
		cairo_dock_notify (CAIRO_DOCK_START_DRAG_DATA, pDock, &bStartAnimation);
		if (bStartAnimation)
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
		
		/*pDock->iAvoidingMouseIconType = -1;
		
		GdkAtom target = gtk_drag_dest_find_target (pWidget, dc, NULL);
		if (target == GDK_NONE)
			gdk_drag_status (dc, 0, time);
		else
			gtk_drag_get_data (pWidget, dc, target, time);
		gtk_drag_get_data (pWidget, dc, target, time);
		g_print ("get-data envoye\n");*/
		cairo_dock_on_enter_notify (pWidget, NULL, pDock);  // ne sera effectif que la 1ere fois a chaque entree dans un dock.
	}
	else
		cairo_dock_on_motion_notify (pWidget, NULL, pDock);
}

void cairo_dock_on_drag_leave (GtkWidget *pWidget, GdkDragContext *dc, guint time, CairoDock *pDock)
{
	cd_message ("stop dragging");
	pDock->bIsDragging = FALSE;
	pDock->bCanDrop = FALSE;
	//cairo_dock_stop_marking_icons (pDock);
	pDock->iAvoidingMouseIconType = -1;
	cairo_dock_emit_leave_signal (pDock);
}


/*gboolean cairo_dock_on_delete (GtkWidget *pWidget, GdkEvent *event, CairoDock *pDock)
{
	Icon *pIcon = NULL;
	if (CAIRO_DOCK_IS_DOCK (pDock))
	{
		pIcon = cairo_dock_get_pointed_icon (pDock->icons);
		if (pIcon == NULL)
			pIcon = cairo_dock_get_dialogless_icon ();
	}
	else
	{
		pIcon = CAIRO_DESKLET (pDock)->pIcon;
	}
	int answer = cairo_dock_ask_question_and_wait (_("Quit Cairo-Dock ?"), pIcon, CAIRO_CONTAINER (pDock));
	if (answer == GTK_RESPONSE_YES)
		gtk_main_quit ();
	return FALSE;
}*/


void cairo_dock_show_dock_at_mouse (CairoDock *pDock)
{
	g_return_if_fail (pDock != NULL);
	int iMouseX, iMouseY;
	if (pDock->bHorizontalDock)
		gdk_window_get_pointer (pDock->pWidget->window, &iMouseX, &iMouseY, NULL);
	else
		gdk_window_get_pointer (pDock->pWidget->window, &iMouseY, &iMouseX, NULL);
	//g_print (" %d;%d\n", iMouseX, iMouseY);
	
	pDock->iGapX = pDock->iWindowPositionX + iMouseX - g_iXScreenWidth[pDock->bHorizontalDock] * pDock->fAlign;
	pDock->iGapY = (pDock->bDirectionUp ? g_iXScreenHeight[pDock->bHorizontalDock] - (pDock->iWindowPositionY + iMouseY) : pDock->iWindowPositionY + iMouseY);
	//g_print (" => %d;%d\n", g_pMainDock->iGapX, g_pMainDock->iGapY);
	
	cairo_dock_set_window_position_at_balance (pDock, pDock->iCurrentWidth, pDock->iCurrentHeight);
	//g_print ("   => (%d;%d)\n", g_pMainDock->iWindowPositionX, g_pMainDock->iWindowPositionY);
	
	gtk_window_move (GTK_WINDOW (pDock->pWidget),
		(pDock->bHorizontalDock ? pDock->iWindowPositionX : pDock->iWindowPositionY),
		(pDock->bHorizontalDock ? pDock->iWindowPositionY : pDock->iWindowPositionX));
	gtk_widget_show (pDock->pWidget);
}

void cairo_dock_raise_from_keyboard (const char *cKeyShortcut, gpointer data)
{
	if (GTK_WIDGET_VISIBLE (g_pMainDock->pWidget))
	{
		gtk_widget_hide (g_pMainDock->pWidget);
	}
	else
	{
		cairo_dock_show_dock_at_mouse (g_pMainDock);
	}
	s_bHideAfterShortcut = FALSE;
}

void cairo_dock_hide_dock_like_a_menu (void)
{
	if (s_bHideAfterShortcut && GTK_WIDGET_VISIBLE (g_pMainDock->pWidget))
	{
		gtk_widget_hide (g_pMainDock->pWidget);
		s_bHideAfterShortcut = FALSE;
	}
}

void cairo_dock_unregister_current_flying_container (void)
{
	s_pFlyingContainer = NULL;
}

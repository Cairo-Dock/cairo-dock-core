/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/*
** Login : <ctaf42@gmail.com>
** Started on  Sun Jan 27 18:35:38 2008 Cedric GESTES
** $Id$
**
** Author(s)
**  - Cedric GESTES <ctaf42@gmail.com>
**  - Fabrice REY
**
** Copyright (C) 2008 Cedric GESTES
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gtk/gtkgl.h>
#include <GL/gl.h> 
#include <GL/glu.h> 
#include <GL/glx.h> 
#include <gdk/x11/gdkglx.h>

#include <gdk/gdkx.h>
#include "cairo-dock-draw.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-module-manager.h"  // cairo_dock_detach_module_instance
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-config.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-log.h"
#include "cairo-dock-container.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-desklet-factory.h"

extern gboolean g_bUseOpenGL;
extern CairoDockDesktopGeometry g_desktopGeometry;

static void _cairo_dock_reserve_space_for_desklet (CairoDesklet *pDesklet, gboolean bReserve);
static void on_drag_data_received_desklet (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t, CairoDesklet *pDesklet);

#define CD_WRITE_DELAY 600  // ms

  ///////////////
 /// SIGNALS ///
///////////////

static gboolean on_expose_desklet(GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	CairoDesklet *pDesklet)
{
	if (pDesklet->iDesiredWidth != 0 && pDesklet->iDesiredHeight != 0 && (pDesklet->iKnownWidth != pDesklet->iDesiredWidth || pDesklet->iKnownHeight != pDesklet->iDesiredHeight))  // skip the drawing until the desklet has reached its size.
	{
		//g_print ("on saute le dessin\n");
		if (g_bUseOpenGL)
		{
			GdkGLContext *pGlContext = gtk_widget_get_gl_context (pDesklet->container.pWidget);
			GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pDesklet->container.pWidget);
			if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
				return FALSE;
			
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glLoadIdentity ();
			
			cairo_dock_apply_desktop_background_opengl (CAIRO_CONTAINER (pDesklet));
			
			if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
				gdk_gl_drawable_swap_buffers (pGlDrawable);
			else
				glFlush ();
			gdk_gl_drawable_gl_end (pGlDrawable);
		}
		else
		{
			cairo_t *pCairoContext = cairo_dock_create_drawing_context_on_container (CAIRO_CONTAINER (pDesklet));
			cairo_destroy (pCairoContext);
		}
		return FALSE;
	}
	
	if (g_bUseOpenGL && pDesklet->pRenderer && pDesklet->pRenderer->render_opengl)
	{
		GdkGLContext *pGlContext = gtk_widget_get_gl_context (pDesklet->container.pWidget);
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pDesklet->container.pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return FALSE;
		
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity ();
		
		cairo_dock_apply_desktop_background_opengl (CAIRO_CONTAINER (pDesklet));
		
		cairo_dock_notify_on_object (&myDeskletsMgr, NOTIFICATION_RENDER_DESKLET, pDesklet, NULL);
		cairo_dock_notify_on_object (pDesklet, NOTIFICATION_RENDER_DESKLET, pDesklet, NULL);
		
		if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
			gdk_gl_drawable_swap_buffers (pGlDrawable);
		else
			glFlush ();
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
	else
	{
		cairo_t *pCairoContext = cairo_dock_create_drawing_context_on_container (CAIRO_CONTAINER (pDesklet));
		
		cairo_dock_notify_on_object (&myDeskletsMgr, NOTIFICATION_RENDER_DESKLET, pDesklet, pCairoContext);
		cairo_dock_notify_on_object (pDesklet, NOTIFICATION_RENDER_DESKLET, pDesklet, pCairoContext);
		
		cairo_destroy (pCairoContext);
	}
	
	return FALSE;
}

static void _cairo_dock_set_desklet_input_shape (CairoDesklet *pDesklet)
{
	gtk_widget_input_shape_combine_mask (pDesklet->container.pWidget,
		NULL,
		0,
		0);
	if (pDesklet->bNoInput)
	{
		GdkBitmap *pShapeBitmap = (GdkBitmap*) gdk_pixmap_new (NULL,
			pDesklet->container.iWidth,
			pDesklet->container.iHeight,
			1);
		
		cairo_t *pCairoContext = gdk_cairo_create (pShapeBitmap);
		cairo_set_source_rgba (pCairoContext, 0.0f, 0.0f, 0.0f, 0.0f);
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
		cairo_paint (pCairoContext);
		cairo_set_source_rgba (pCairoContext, 1., 1., 1., 1.);
		cairo_rectangle (pCairoContext,
			pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize,
			pDesklet->container.iHeight - myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize);
		cairo_fill (pCairoContext);
		cairo_destroy (pCairoContext);
		gtk_widget_input_shape_combine_mask (pDesklet->container.pWidget,
			pShapeBitmap,
			0,
			0);
		g_object_unref (pShapeBitmap);
		//g_print ("input shape : %dx%d\n", pDesklet->container.iWidth, pDesklet->container.iHeight);
	}
}

static gboolean _cairo_dock_write_desklet_size (CairoDesklet *pDesklet)
{
	if ((pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0) && pDesklet->pIcon != NULL && pDesklet->pIcon->pModuleInstance != NULL && cairo_dock_desklet_manager_is_ready ())
	{
		gchar *cSize = g_strdup_printf ("%d;%d", pDesklet->container.iWidth, pDesklet->container.iHeight);
		cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
			G_TYPE_STRING, "Desklet", "size", cSize,
			G_TYPE_INVALID);
		g_free (cSize);
		cairo_dock_notify_on_object (&myDeskletsMgr, NOTIFICATION_CONFIGURE_DESKLET, pDesklet);
	}
	pDesklet->iSidWriteSize = 0;
	pDesklet->iKnownWidth = pDesklet->container.iWidth;
	pDesklet->iKnownHeight = pDesklet->container.iHeight;
	if (((pDesklet->iDesiredWidth != 0 || pDesklet->iDesiredHeight != 0) && pDesklet->iDesiredWidth == pDesklet->container.iWidth && pDesklet->iDesiredHeight == pDesklet->container.iHeight) || (pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0))
	{
		pDesklet->iDesiredWidth = 0;
		pDesklet->iDesiredHeight = 0;
		
		cairo_dock_load_desklet_decorations (pDesklet);
		
		if (pDesklet->pIcon != NULL && pDesklet->pIcon->pModuleInstance != NULL)
		{
			// on recalcule la vue du desklet a la nouvelle dimension.
			CairoDeskletRenderer *pRenderer = pDesklet->pRenderer;
			if (pRenderer)
			{
				// on calcule les icones et les parametres internes de la vue.
				if (pRenderer->calculate_icons != NULL)
					pRenderer->calculate_icons (pDesklet);
				
				// on recharge l'icone principale.
				Icon* pIcon = pDesklet->pIcon;
				if (pIcon)
				{
					pIcon->iImageWidth = pIcon->fWidth;
					pIcon->iImageHeight = pIcon->fHeight;
					if (pIcon->iImageWidth > 0)
						cairo_dock_load_icon_buffers (pIcon, CAIRO_CONTAINER (pDesklet));  // pas de trigger, car on veut pouvoir associer un contexte a l'icone principale tout de suite.
				}
				
				// on recharge chaque icone.
				GList* ic;
				for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
				{
					pIcon = ic->data;
					if ((int)pIcon->fWidth != pIcon->iImageWidth || (int)pIcon->fHeight != pIcon->iImageHeight)
					{
						pIcon->iImageWidth = pIcon->fWidth;
						pIcon->iImageHeight = pIcon->fHeight;
						cairo_dock_trigger_load_icon_buffers (pIcon, CAIRO_CONTAINER (pDesklet));
					}
				}
				
				// on recharge les buffers de la vue.
				if (pRenderer->load_data != NULL)
					pRenderer->load_data (pDesklet);
			}
			
			// on recharge le module associe.
			cairo_dock_reload_module_instance (pDesklet->pIcon->pModuleInstance, FALSE);
			
			gtk_widget_queue_draw (pDesklet->container.pWidget);  // sinon on ne redessine que l'interieur.
		}
		
		Window Xid = GDK_WINDOW_XID (pDesklet->container.pWidget->window);
		if (pDesklet->bSpaceReserved)  // l'espace est reserve, on reserve la nouvelle taille.
		{
			_cairo_dock_reserve_space_for_desklet (pDesklet, TRUE);
		}
	}
	
	//g_print ("iWidth <- %d;iHeight <- %d ; (%dx%d) (%x)\n", pDesklet->container.iWidth, pDesklet->container.iHeight, pDesklet->iKnownWidth, pDesklet->iKnownHeight, pDesklet->pIcon);
	return FALSE;
}
static gboolean _cairo_dock_write_desklet_position (CairoDesklet *pDesklet)
{
	if (pDesklet->pIcon != NULL && pDesklet->pIcon->pModuleInstance != NULL)
	{
		int iRelativePositionX = (pDesklet->container.iWindowPositionX + pDesklet->container.iWidth/2 <= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]/2 ? pDesklet->container.iWindowPositionX : pDesklet->container.iWindowPositionX - g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]);
		int iRelativePositionY = (pDesklet->container.iWindowPositionY + pDesklet->container.iHeight/2 <= g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]/2 ? pDesklet->container.iWindowPositionY : pDesklet->container.iWindowPositionY - g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
		
		int iNumDesktop = -1;
		if (! cairo_dock_desklet_is_sticky (pDesklet))
		{
			GdkWindow *window = pDesklet->container.pWidget->window;
			Window Xid = GDK_WINDOW_XID (window);
			cd_debug ("This window (%d) is not sticky", (int) Xid);
			int iDesktop = cairo_dock_get_xwindow_desktop (Xid);
			int iGlobalPositionX, iGlobalPositionY, iWidthExtent, iHeightExtent;
			cairo_dock_get_xwindow_geometry (Xid, &iGlobalPositionX, &iGlobalPositionY, &iWidthExtent, &iHeightExtent);
			if (iGlobalPositionX < 0)
				iGlobalPositionX += g_desktopGeometry.iNbViewportX * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
			if (iGlobalPositionY < 0)
				iGlobalPositionY += g_desktopGeometry.iNbViewportY * g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
			
			int iViewportX = iGlobalPositionX / g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
			int iViewportY = iGlobalPositionY / g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
			
			int iCurrentDesktop, iCurrentViewportX, iCurrentViewportY;
			cairo_dock_get_current_desktop_and_viewport (&iCurrentDesktop, &iCurrentViewportX, &iCurrentViewportY);
			
			iViewportX += iCurrentViewportX;
			if (iViewportX >= g_desktopGeometry.iNbViewportX)
				iViewportX -= g_desktopGeometry.iNbViewportX;
			iViewportY += iCurrentViewportY;
			if (iViewportY >= g_desktopGeometry.iNbViewportY)
				iViewportY -= g_desktopGeometry.iNbViewportY;
			//g_print ("position : %d,%d,%d / %d,%d,%d\n", iDesktop, iViewportX, iViewportY, iCurrentDesktop, iCurrentViewportX, iCurrentViewportY);
			
			iNumDesktop = iDesktop * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY + iViewportX * g_desktopGeometry.iNbViewportY + iViewportY;
			//g_print ("desormais on place le desklet sur le bureau (%d,%d,%d)\n", iDesktop, iViewportX, iViewportY);
		}
		
		cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
			G_TYPE_INT, "Desklet", "x position", iRelativePositionX,
			G_TYPE_INT, "Desklet", "y position", iRelativePositionY,
			G_TYPE_INT, "Desklet", "num desktop", iNumDesktop,
			G_TYPE_INVALID);
		cairo_dock_notify_on_object (&myDeskletsMgr, NOTIFICATION_CONFIGURE_DESKLET, pDesklet);
	}
	
	if (pDesklet->bSpaceReserved)  // l'espace est reserve, on reserve a la nouvelle position.
	{
		_cairo_dock_reserve_space_for_desklet (pDesklet, TRUE);
	}
	if (pDesklet->pIcon && cairo_dock_icon_has_dialog (pDesklet->pIcon))
	{
		cairo_dock_replace_all_dialogs ();
	}
	pDesklet->iSidWritePosition = 0;
	return FALSE;
}
static gboolean on_configure_desklet (GtkWidget* pWidget,
	GdkEventConfigure* pEvent,
	CairoDesklet *pDesklet)
{
	//g_print (" >>>>>>>>> %s (%dx%d, %d;%d)", __func__, pEvent->width, pEvent->height, pEvent->x, pEvent->y);
	if (pDesklet->container.iWidth != pEvent->width || pDesklet->container.iHeight != pEvent->height)
	{
		if ((pEvent->width < pDesklet->container.iWidth || pEvent->height < pDesklet->container.iHeight) && (pDesklet->iDesiredWidth != 0 && pDesklet->iDesiredHeight != 0))
		{
			gdk_window_resize (pDesklet->container.pWidget->window,
				pDesklet->iDesiredWidth,
				pDesklet->iDesiredHeight);
		}
		
		pDesklet->container.iWidth = pEvent->width;
		pDesklet->container.iHeight = pEvent->height;
		
		if (g_bUseOpenGL)
		{
			GdkGLContext* pGlContext = gtk_widget_get_gl_context (pWidget);
			GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (pWidget);
			GLsizei w = pEvent->width;
			GLsizei h = pEvent->height;
			if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
				return FALSE;
			
			glViewport(0, 0, w, h);
			
			cairo_dock_set_perspective_view (CAIRO_CONTAINER (pDesklet));
			
			gdk_gl_drawable_gl_end (pGlDrawable);
		}
		
		if (pDesklet->bNoInput)
			_cairo_dock_set_desklet_input_shape (pDesklet);
		
		if (pDesklet->iSidWriteSize != 0)
		{
			g_source_remove (pDesklet->iSidWriteSize);
		}
		pDesklet->iSidWriteSize = g_timeout_add (CD_WRITE_DELAY, (GSourceFunc) _cairo_dock_write_desklet_size, (gpointer) pDesklet);
	}
	
	int x = pEvent->x, y = pEvent->y;
	//g_print ("new desklet position : (%d;%d)", x, y);
	while (x < 0)  // on passe au referentiel du viewport de la fenetre; inutile de connaitre sa position, puisqu'ils ont tous la meme taille.
		x += g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
	while (x >= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL])
		x -= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
	while (y < 0)
		y += g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
	while (y >= g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL])
		y -= g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
	//g_print (" => (%d;%d)\n", x, y);
	if (pDesklet->container.iWindowPositionX != x || pDesklet->container.iWindowPositionY != y)
	{
		pDesklet->container.iWindowPositionX = x;
		pDesklet->container.iWindowPositionY = y;

		if (cairo_dock_desklet_manager_is_ready ())
		{
			if (pDesklet->iSidWritePosition != 0)
			{
				g_source_remove (pDesklet->iSidWritePosition);
			}
			pDesklet->iSidWritePosition = g_timeout_add (CD_WRITE_DELAY, (GSourceFunc) _cairo_dock_write_desklet_position, (gpointer) pDesklet);
		}
	}
	pDesklet->moving = FALSE;

	return FALSE;
}

static gboolean on_scroll_desklet (GtkWidget* pWidget,
	GdkEventScroll* pScroll,
	CairoDesklet *pDesklet)
{
	//g_print ("scroll\n");
	if (! (pScroll->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)))
	{
		Icon *icon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);  // can be NULL
		cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_SCROLL_ICON, icon, pDesklet, pScroll->direction);
		cairo_dock_notify_on_object (CAIRO_CONTAINER (pDesklet), NOTIFICATION_SCROLL_ICON, icon, pDesklet, pScroll->direction);
	}
	return FALSE;
}

static gboolean on_unmap_desklet (GtkWidget* pWidget,
	GdkEvent *pEvent,
	CairoDesklet *pDesklet)
{
	cd_debug ("unmap desklet (bAllowMinimize:%d)\n", pDesklet->bAllowMinimize);
	Window Xid = GDK_WINDOW_XID (pWidget->window);
	if (pDesklet->iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER)  // on the widget layer, let pass the unmap event..
	//if (cairo_dock_window_is_utility (Xid))  // sur la couche des widgets, on ne fait rien.
		return FALSE;
	if (! pDesklet->bAllowMinimize)
	{
		if (pDesklet->pUnmapTimer)
		{
			double fElapsedTime = g_timer_elapsed (pDesklet->pUnmapTimer, NULL);
			cd_debug ("fElapsedTime : %fms\n", fElapsedTime);
			g_timer_destroy (pDesklet->pUnmapTimer);
			pDesklet->pUnmapTimer = NULL;
			if (fElapsedTime < .2)
				return TRUE;
		}
		gtk_window_present (GTK_WINDOW (pWidget));
	}
	else
	{
		pDesklet->bAllowMinimize = FALSE;
		if (pDesklet->pUnmapTimer == NULL)
			pDesklet->pUnmapTimer = g_timer_new ();  // starts the timer.
	}
	return TRUE;  // stops other handlers from being invoked for the event.
}

static gboolean on_button_press_desklet(GtkWidget *pWidget,
	GdkEventButton *pButton,
	CairoDesklet *pDesklet)
{
	if (pButton->button == 1)  // clic gauche.
	{
		pDesklet->container.iMouseX = pButton->x;
		pDesklet->container.iMouseY = pButton->y;
		if (pButton->type == GDK_BUTTON_PRESS)
		{
			pDesklet->bClicked = TRUE;
			if (cairo_dock_desklet_is_free (pDesklet))
			{
				///pDesklet->container.iMouseX = pButton->x;  // pour le deplacement manuel.
				///pDesklet->container.iMouseY = pButton->y;
				if (pButton->x < myDeskletsParam.iDeskletButtonSize && pButton->y < myDeskletsParam.iDeskletButtonSize)
					pDesklet->rotating = TRUE;
				else if (pButton->x > pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize && pButton->y < myDeskletsParam.iDeskletButtonSize)
					pDesklet->retaching = TRUE;
				else if (pButton->x > (pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize)/2 && pButton->x < (pDesklet->container.iWidth + myDeskletsParam.iDeskletButtonSize)/2 && pButton->y < myDeskletsParam.iDeskletButtonSize)
					pDesklet->rotatingY = TRUE;
				else if (pButton->y > (pDesklet->container.iHeight - myDeskletsParam.iDeskletButtonSize)/2 && pButton->y < (pDesklet->container.iHeight + myDeskletsParam.iDeskletButtonSize)/2 && pButton->x < myDeskletsParam.iDeskletButtonSize)
					pDesklet->rotatingX = TRUE;
				else
					pDesklet->time = pButton->time;
			}
			if (pDesklet->bAllowNoClickable && pButton->x > pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize && pButton->y > pDesklet->container.iHeight - myDeskletsParam.iDeskletButtonSize)
				pDesklet->making_transparent = TRUE;
		}
		else if (pButton->type == GDK_BUTTON_RELEASE)
		{
			if (!pDesklet->bClicked)  // on n'accepte le release que si on avait clique sur le desklet avant (on peut recevoir le release si on avait clique sur un dialogue qui chevauchait notre desklet et qui a disparu au clic).
			{
				return FALSE;
			}
			pDesklet->bClicked = FALSE;
			//g_print ("GDK_BUTTON_RELEASE\n");
			int x = pDesklet->container.iMouseX;
			int y = pDesklet->container.iMouseY;
			if (pDesklet->moving)
			{
				pDesklet->moving = FALSE;
			}
			else if (pDesklet->rotating)
			{
				pDesklet->rotating = FALSE;
				cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
					G_TYPE_INT, "Desklet", "rotation", (int) (pDesklet->fRotation / G_PI * 180.),
					G_TYPE_INVALID);
				gtk_widget_queue_draw (pDesklet->container.pWidget);
				cairo_dock_notify_on_object (&myDeskletsMgr, NOTIFICATION_CONFIGURE_DESKLET, pDesklet);
			}
			else if (pDesklet->retaching)
			{
				pDesklet->retaching = FALSE;
				if (x > pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize && y < myDeskletsParam.iDeskletButtonSize)  // on verifie qu'on est encore dedans.
				{
					Icon *icon = pDesklet->pIcon;
					g_return_val_if_fail (CAIRO_DOCK_IS_APPLET (icon), FALSE);
					cairo_dock_detach_module_instance (icon->pModuleInstance);
					return CAIRO_DOCK_INTERCEPT_NOTIFICATION;  // interception du signal.
				}
			}
			else if (pDesklet->making_transparent)
			{
				cd_debug ("pDesklet->making_transparent\n");
				pDesklet->making_transparent = FALSE;
				if (x > pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize && y > pDesklet->container.iHeight - myDeskletsParam.iDeskletButtonSize)  // on verifie qu'on est encore dedans.
				{
					Icon *icon = pDesklet->pIcon;
					g_return_val_if_fail (CAIRO_DOCK_IS_APPLET (icon), FALSE);
					pDesklet->bNoInput = ! pDesklet->bNoInput;
					cd_debug ("no input : %d (%s)\n", pDesklet->bNoInput, icon->pModuleInstance->cConfFilePath);
					cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
						G_TYPE_BOOLEAN, "Desklet", "no input", pDesklet->bNoInput,
						G_TYPE_INVALID);
					_cairo_dock_set_desklet_input_shape (pDesklet);
					gtk_widget_queue_draw (pDesklet->container.pWidget);
					if (pDesklet->bNoInput)
						cairo_dock_allow_widget_to_receive_data (pDesklet->container.pWidget, G_CALLBACK (on_drag_data_received_desklet), pDesklet);
					else
						cairo_dock_disallow_widget_to_receive_data (pDesklet->container.pWidget);
				}
			}
			else if (pDesklet->rotatingY)
			{
				pDesklet->rotatingY = FALSE;
				cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
					G_TYPE_INT, "Desklet", "depth rotation y", (int) (pDesklet->fDepthRotationY / G_PI * 180.),
					G_TYPE_INVALID);
				gtk_widget_queue_draw (pDesklet->container.pWidget);
			}
			else if (pDesklet->rotatingX)
			{
				pDesklet->rotatingX = FALSE;
				cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
					G_TYPE_INT, "Desklet", "depth rotation x", (int) (pDesklet->fDepthRotationX / G_PI * 180.),
					G_TYPE_INVALID);
				gtk_widget_queue_draw (pDesklet->container.pWidget);
			}
			else
			{
				Icon *pClickedIcon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);
				cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_CLICK_ICON, pClickedIcon, pDesklet, pButton->state);
				cairo_dock_notify_on_object (CAIRO_CONTAINER (pDesklet), NOTIFICATION_CLICK_ICON, pClickedIcon, pDesklet, pButton->state);
			}
			// prudence.
			pDesklet->rotating = FALSE;
			pDesklet->retaching = FALSE;
			pDesklet->making_transparent = FALSE;
			pDesklet->rotatingX = FALSE;
			pDesklet->rotatingY = FALSE;
		}
		else if (pButton->type == GDK_2BUTTON_PRESS)
		{
			if (! (pButton->x < myDeskletsParam.iDeskletButtonSize && pButton->y < myDeskletsParam.iDeskletButtonSize) && ! (pButton->x > (pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize)/2 && pButton->x < (pDesklet->container.iWidth + myDeskletsParam.iDeskletButtonSize)/2 && pButton->y < myDeskletsParam.iDeskletButtonSize))
			{
				Icon *pClickedIcon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);  // can be NULL
				cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_DOUBLE_CLICK_ICON, pClickedIcon, pDesklet);
				cairo_dock_notify_on_object (CAIRO_CONTAINER (pDesklet), NOTIFICATION_DOUBLE_CLICK_ICON, pClickedIcon, pDesklet);
			}
		}
	}
	else if (pButton->button == 3 && pButton->type == GDK_BUTTON_PRESS)  // clique droit.
	{
		Icon *pClickedIcon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);
		GtkWidget *menu = cairo_dock_build_menu (pClickedIcon, CAIRO_CONTAINER (pDesklet));  // genere un CAIRO_DOCK_BUILD_ICON_MENU.
		cairo_dock_popup_menu_on_container (menu, CAIRO_CONTAINER (pDesklet));
		pDesklet->container.bInside = FALSE;
		gtk_widget_queue_draw (pDesklet->container.pWidget);
	}
	else if (pButton->button == 2 && pButton->type == GDK_BUTTON_PRESS)  // clique milieu.
	{
		if (pButton->x < myDeskletsParam.iDeskletButtonSize && pButton->y < myDeskletsParam.iDeskletButtonSize)
		{
			pDesklet->fRotation = 0.;
			gtk_widget_queue_draw (pDesklet->container.pWidget);
			cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
				G_TYPE_INT, "Desklet", "rotation", 0,
				G_TYPE_INVALID);
			cairo_dock_notify_on_object (&myDeskletsMgr, NOTIFICATION_CONFIGURE_DESKLET, pDesklet);
		}
		else if (pButton->x > (pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize)/2 && pButton->x < (pDesklet->container.iWidth + myDeskletsParam.iDeskletButtonSize)/2 && pButton->y < myDeskletsParam.iDeskletButtonSize)
		{
			pDesklet->fDepthRotationY = 0.;
			gtk_widget_queue_draw (pDesklet->container.pWidget);
			cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
				G_TYPE_INT, "Desklet", "depth rotation y", 0,
				G_TYPE_INVALID);
		}
		else if (pButton->y > (pDesklet->container.iHeight - myDeskletsParam.iDeskletButtonSize)/2 && pButton->y < (pDesklet->container.iHeight + myDeskletsParam.iDeskletButtonSize)/2 && pButton->x < myDeskletsParam.iDeskletButtonSize)
		{
			pDesklet->fDepthRotationX = 0.;
			gtk_widget_queue_draw (pDesklet->container.pWidget);
			cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
				G_TYPE_INT, "Desklet", "depth rotation x", 0,
				G_TYPE_INVALID);
		}
		else
		{
			cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_MIDDLE_CLICK_ICON, pDesklet->pIcon, pDesklet);
			cairo_dock_notify_on_object (CAIRO_CONTAINER (pDesklet), NOTIFICATION_MIDDLE_CLICK_ICON, pDesklet->pIcon, pDesklet);
		}
	}
	return FALSE;
}

static void on_drag_data_received_desklet (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t, CairoDesklet *pDesklet)
{
	//g_print ("%s (%dx%d)\n", __func__, x, y);
	
	//\_________________ On recupere l'URI.
	gchar *cReceivedData = (gchar *) selection_data->data;  // gtk_selection_data_get_text
	g_return_if_fail (cReceivedData != NULL);
	int length = strlen (cReceivedData);
	if (cReceivedData[length-1] == '\n')
		cReceivedData[--length] = '\0';  // on vire le retour chariot final.
	if (cReceivedData[length-1] == '\r')
		cReceivedData[--length] = '\0';
	
	pDesklet->container.iMouseX = x;
	pDesklet->container.iMouseY = y;
	Icon *pClickedIcon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);
	cairo_dock_notify_drop_data (cReceivedData, pClickedIcon, 0, CAIRO_CONTAINER (pDesklet));
}

static gboolean on_motion_notify_desklet (GtkWidget *pWidget,
	GdkEventMotion* pMotion,
	CairoDesklet *pDesklet)
{
	/*if (pMotion->state & GDK_BUTTON1_MASK && cairo_dock_desklet_is_free (pDesklet))
	{
		cd_debug ("root : %d;%d", (int) pMotion->x_root, (int) pMotion->y_root);
	}
	else*/  // le 'press-button' est local au sous-widget clique, alors que le 'motion-notify' est global a la fenetre; c'est donc par lui qu'on peut avoir a coup sur les coordonnees du curseur (juste avant le clic).
	{
		pDesklet->container.iMouseX = pMotion->x;
		pDesklet->container.iMouseY = pMotion->y;
		gboolean bStartAnimation = FALSE;
		cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_MOUSE_MOVED, pDesklet, &bStartAnimation);
		cairo_dock_notify_on_object (CAIRO_CONTAINER (pDesklet), NOTIFICATION_MOUSE_MOVED, pDesklet, &bStartAnimation);
		if (bStartAnimation)
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDesklet));
	}
	
	if (pDesklet->rotating && cairo_dock_desklet_is_free (pDesklet))
	{
		double alpha = atan2 (pDesklet->container.iHeight, - pDesklet->container.iWidth);
		pDesklet->fRotation = alpha - atan2 (.5*pDesklet->container.iHeight - pMotion->y, pMotion->x - .5*pDesklet->container.iWidth);
		while (pDesklet->fRotation > G_PI)
			pDesklet->fRotation -= 2 * G_PI;
		while (pDesklet->fRotation <= - G_PI)
			pDesklet->fRotation += 2 * G_PI;
		gtk_widget_queue_draw(pDesklet->container.pWidget);
	}
	else if (pDesklet->rotatingY && cairo_dock_desklet_is_free (pDesklet))
	{
		pDesklet->fDepthRotationY = G_PI * (pMotion->x - .5*pDesklet->container.iWidth) / pDesklet->container.iWidth;
		gtk_widget_queue_draw(pDesklet->container.pWidget);
	}
	else if (pDesklet->rotatingX && cairo_dock_desklet_is_free (pDesklet))
	{
		pDesklet->fDepthRotationX = G_PI * (pMotion->y - .5*pDesklet->container.iHeight) / pDesklet->container.iHeight;
		gtk_widget_queue_draw(pDesklet->container.pWidget);
	}
	else if (pMotion->state & GDK_BUTTON1_MASK && cairo_dock_desklet_is_free (pDesklet) && ! pDesklet->moving)
	{
		gtk_window_begin_move_drag (GTK_WINDOW (gtk_widget_get_toplevel (pWidget)),
			1/*pButton->button*/,
			pMotion->x_root/*pButton->x_root*/,
			pMotion->y_root/*pButton->y_root*/,
			pDesklet->time/*pButton->time*/);
		pDesklet->moving = TRUE;
	}
	else
	{
		gboolean bStartAnimation = FALSE;
		Icon *pIcon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);
		if (pIcon != NULL)
		{
			if (! pIcon->bPointed)
			{
				Icon *pPointedIcon = cairo_dock_get_pointed_icon (pDesklet->icons);
				if (pPointedIcon != NULL)
					pPointedIcon->bPointed = FALSE;
				pIcon->bPointed = TRUE;
				
				//g_print ("on survole %s\n", pIcon->cName);
				cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_ENTER_ICON, pIcon, pDesklet, &bStartAnimation);
				cairo_dock_notify_on_object (CAIRO_CONTAINER (pDesklet), NOTIFICATION_ENTER_ICON, pIcon, pDesklet, &bStartAnimation);
			}
		}
		else
		{
			Icon *pPointedIcon = cairo_dock_get_pointed_icon (pDesklet->icons);
			if (pPointedIcon != NULL)
			{
				pPointedIcon->bPointed = FALSE;
				
				//g_print ("kedal\n");
				cairo_dock_notify_on_object (&myContainersMgr, NOTIFICATION_ENTER_ICON, pPointedIcon, pDesklet, &bStartAnimation);
				cairo_dock_notify_on_object (CAIRO_CONTAINER (pDesklet), NOTIFICATION_ENTER_ICON, pPointedIcon, pDesklet, &bStartAnimation);
			}
		}
		if (bStartAnimation)
		{
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDesklet));
		}
	}
	
	gdk_device_get_state (pMotion->device, pMotion->window, NULL, NULL);  // pour recevoir d'autres MotionNotify.
	return FALSE;
}


static gboolean on_focus_in_out_desklet(GtkWidget *widget,
	GdkEventFocus *event,
	CairoDesklet *pDesklet)
{
	gtk_widget_queue_draw(pDesklet->container.pWidget);
	return FALSE;
}

static gboolean on_enter_desklet (GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDesklet *pDesklet)
{
	//g_print ("%s (%d)\n", __func__, pDesklet->container.bInside);
	if (! pDesklet->container.bInside)  // avant on etait dehors, on redessine donc.
	{
		pDesklet->container.bInside = TRUE;
		gtk_widget_queue_draw (pWidget);  // redessin des boutons.
		
		///if (g_bUseOpenGL)
		{
			gboolean bStartAnimation = FALSE;
			cairo_dock_notify_on_object (&myDeskletsMgr, NOTIFICATION_ENTER_DESKLET, pDesklet, &bStartAnimation);
			cairo_dock_notify_on_object (pDesklet, NOTIFICATION_ENTER_DESKLET, pDesklet, &bStartAnimation);
			if (bStartAnimation)
				cairo_dock_launch_animation (CAIRO_CONTAINER (pDesklet));
		}
	}
	return FALSE;
}
static gboolean on_leave_desklet (GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDesklet *pDesklet)
{
	//g_print ("%s (%d)\n", __func__, pDesklet->container.bInside);
	int iMouseX, iMouseY;
	gdk_window_get_pointer (pWidget->window, &iMouseX, &iMouseY, NULL);
	if (gtk_bin_get_child (GTK_BIN (pDesklet->container.pWidget)) != NULL && iMouseX > 0 && iMouseX < pDesklet->container.iWidth && iMouseY > 0 && iMouseY < pDesklet->container.iHeight)  // en fait on est dans un widget fils, donc on ne fait rien.
	{
		return FALSE;
	}

	pDesklet->container.bInside = FALSE;
	gtk_widget_queue_draw (pWidget);  // redessin des boutons.
	
	gboolean bStartAnimation = FALSE;
	cairo_dock_notify_on_object (&myDeskletsMgr, NOTIFICATION_LEAVE_DESKLET, pDesklet, &bStartAnimation);
	cairo_dock_notify_on_object (pDesklet, NOTIFICATION_LEAVE_DESKLET, pDesklet, &bStartAnimation);
	if (bStartAnimation)
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDesklet));
	
	return FALSE;
}


  ///////////////
 /// FACTORY ///
///////////////

static void _cairo_dock_set_icon_size (CairoContainer *pContainer, Icon *icon)
{
	CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
	/**if (pDesklet->pRenderer && pDesklet->pRenderer->set_icon_size)
		pDesklet->pRenderer->set_icon_size (pDesklet, icon);*/
}

CairoDesklet *cairo_dock_new_desklet (void)
{
	cd_message ("%s ()", __func__);
	CairoDesklet *pDesklet = g_new0(CairoDesklet, 1);
	pDesklet->container.iType = CAIRO_DOCK_TYPE_DESKLET;
	pDesklet->container.bIsHorizontal = TRUE;
	pDesklet->container.bDirectionUp = TRUE;
	pDesklet->container.fRatio = 1;
	pDesklet->container.fRatio = 1;
	
	pDesklet->container.iface.set_icon_size = _cairo_dock_set_icon_size;
	
	GtkWidget* pWindow = cairo_dock_init_container (CAIRO_CONTAINER (pDesklet));
	cairo_dock_install_notifications_on_object (pDesklet, NB_NOTIFICATIONS_DESKLET);
	
	gtk_window_set_title (GTK_WINDOW(pWindow), "cairo-dock-desklet");
	gtk_widget_add_events( pWindow, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_FOCUS_CHANGE_MASK);
	gtk_container_set_border_width(GTK_CONTAINER(pWindow), 2);  // comme ca.
	gtk_window_set_default_size(GTK_WINDOW(pWindow), 10, 10);  // idem.

	g_signal_connect (G_OBJECT (pWindow),
		"expose-event",
		G_CALLBACK (on_expose_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (on_configure_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"motion-notify-event",
		G_CALLBACK (on_motion_notify_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"button-press-event",
		G_CALLBACK (on_button_press_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"button-release-event",
		G_CALLBACK (on_button_press_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"focus-in-event",
		G_CALLBACK (on_focus_in_out_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"focus-out-event",
		G_CALLBACK (on_focus_in_out_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"enter-notify-event",
		G_CALLBACK (on_enter_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"leave-notify-event",
		G_CALLBACK (on_leave_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"unmap-event",
		G_CALLBACK (on_unmap_desklet),
		pDesklet);
	g_signal_connect (G_OBJECT (pWindow),
		"scroll-event",
		G_CALLBACK (on_scroll_desklet),
		pDesklet);
	cairo_dock_allow_widget_to_receive_data (pWindow, G_CALLBACK (on_drag_data_received_desklet), pDesklet);
	
	gtk_widget_show_all (pWindow);
	
	return pDesklet;
}

void cairo_dock_free_desklet (CairoDesklet *pDesklet)
{
	if (pDesklet == NULL)
		return;

	if (pDesklet->iSidWriteSize != 0)
		g_source_remove (pDesklet->iSidWriteSize);
	if (pDesklet->iSidWritePosition != 0)
		g_source_remove (pDesklet->iSidWritePosition);
	cairo_dock_notify_on_object (&myDeskletsMgr, NOTIFICATION_STOP_DESKLET, pDesklet);
	cairo_dock_notify_on_object (pDesklet, NOTIFICATION_STOP_DESKLET, pDesklet);
	
	cairo_dock_steal_interactive_widget_from_desklet (pDesklet);

	cairo_dock_finish_container (CAIRO_CONTAINER (pDesklet));
	
	if (pDesklet->pRenderer != NULL)
	{
		if (pDesklet->pRenderer->free_data != NULL)
		{
			pDesklet->pRenderer->free_data (pDesklet);
			pDesklet->pRendererData = NULL;
		}
	}
	
	if (pDesklet->icons != NULL)
	{
		g_list_foreach (pDesklet->icons, (GFunc) cairo_dock_free_icon, NULL);
		g_list_free (pDesklet->icons);
	}
	
	g_free (pDesklet->cDecorationTheme);
	cairo_dock_free_desklet_decoration (pDesklet->pUserDecoration);
	
	cairo_dock_unload_image_buffer (&pDesklet->backGroundImageBuffer);
	cairo_dock_unload_image_buffer (&pDesklet->foreGroundImageBuffer);
	
	g_free(pDesklet);
}


void cairo_dock_configure_desklet (CairoDesklet *pDesklet, CairoDeskletAttribute *pAttribute)
{
	//g_print ("%s (%dx%d ; (%d,%d) ; %d)\n", __func__, pAttribute->iDeskletWidth, pAttribute->iDeskletHeight, pAttribute->iDeskletPositionX, pAttribute->iDeskletPositionY, pAttribute->iVisibility);
	if (pAttribute->bDeskletUseSize && (pAttribute->iDeskletWidth != pDesklet->container.iWidth || pAttribute->iDeskletHeight != pDesklet->container.iHeight))
	{
		pDesklet->iDesiredWidth = pAttribute->iDeskletWidth;
		pDesklet->iDesiredHeight = pAttribute->iDeskletHeight;
		gdk_window_resize (pDesklet->container.pWidget->window,
			pAttribute->iDeskletWidth,
			pAttribute->iDeskletHeight);
	}
	if (! pAttribute->bDeskletUseSize)
	{
		gtk_container_set_border_width (GTK_CONTAINER (pDesklet->container.pWidget), 0);
		gtk_window_set_resizable (GTK_WINDOW(pDesklet->container.pWidget), FALSE);
	}
	
	int iAbsolutePositionX = (pAttribute->iDeskletPositionX < 0 ? g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] + pAttribute->iDeskletPositionX : pAttribute->iDeskletPositionX);
	iAbsolutePositionX = MAX (0, MIN (g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] - pAttribute->iDeskletWidth, iAbsolutePositionX));
	int iAbsolutePositionY = (pAttribute->iDeskletPositionY < 0 ? g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] + pAttribute->iDeskletPositionY : pAttribute->iDeskletPositionY);
	iAbsolutePositionY = MAX (0, MIN (g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - pAttribute->iDeskletHeight, iAbsolutePositionY));
	
	if (pAttribute->bOnAllDesktops)
		gdk_window_move(pDesklet->container.pWidget->window,
			iAbsolutePositionX,
			iAbsolutePositionY);
	//g_print (" let's place the deklet at (%d;%d)", iAbsolutePositionX, iAbsolutePositionY);

	cairo_dock_set_desklet_accessibility (pDesklet, pAttribute->iVisibility, FALSE);
	
	if (pAttribute->bOnAllDesktops)
	{
		gtk_window_stick (GTK_WINDOW (pDesklet->container.pWidget));
	}
	else
	{
		gtk_window_unstick (GTK_WINDOW (pDesklet->container.pWidget));
		Window Xid = GDK_WINDOW_XID (pDesklet->container.pWidget->window);
		if (g_desktopGeometry.iNbViewportX > 0 && g_desktopGeometry.iNbViewportY > 0)
		{
			int iNumDesktop, iNumViewportX, iNumViewportY;
			iNumDesktop = pAttribute->iNumDesktop / (g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY);
			int index2 = pAttribute->iNumDesktop % (g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY);
			iNumViewportX = index2 / g_desktopGeometry.iNbViewportY;
			iNumViewportY = index2 % g_desktopGeometry.iNbViewportY;
			
			int iCurrentDesktop, iCurrentViewportX, iCurrentViewportY;
			cairo_dock_get_current_desktop_and_viewport (&iCurrentDesktop, &iCurrentViewportX, &iCurrentViewportY);
			cd_debug (">>> on fixe le desklet sur le bureau (%d,%d,%d) (cur : %d,%d,%d)\n", iNumDesktop, iNumViewportX, iNumViewportY, iCurrentDesktop, iCurrentViewportX, iCurrentViewportY);
			
			iNumViewportX -= iCurrentViewportX;
			iNumViewportY -= iCurrentViewportY;
			
			cd_debug ("on le place en %d + %d\n", iNumViewportX * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL], iAbsolutePositionX);
			cairo_dock_move_xwindow_to_absolute_position (Xid, iNumDesktop, iNumViewportX * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] + iAbsolutePositionX, iNumViewportY * g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] + iAbsolutePositionY);
		}
	}
	pDesklet->bPositionLocked = pAttribute->bPositionLocked;
	pDesklet->bNoInput = pAttribute->bNoInput;
	if (pDesklet->bNoInput)
		cairo_dock_disallow_widget_to_receive_data (pDesklet->container.pWidget);
	pDesklet->fRotation = pAttribute->iRotation / 180. * G_PI ;
	pDesklet->fDepthRotationY = pAttribute->iDepthRotationY / 180. * G_PI ;
	pDesklet->fDepthRotationX = pAttribute->iDepthRotationX / 180. * G_PI ;
	
	g_free (pDesklet->cDecorationTheme);
	pDesklet->cDecorationTheme = pAttribute->cDecorationTheme;
	pAttribute->cDecorationTheme = NULL;
	cairo_dock_free_desklet_decoration (pDesklet->pUserDecoration);
	pDesklet->pUserDecoration = pAttribute->pUserDecoration;
	pAttribute->pUserDecoration = NULL;
	
	//cd_debug ("%s (%dx%d ; %d)", __func__, pDesklet->iDesiredWidth, pDesklet->iDesiredHeight, pDesklet->iSidWriteSize);
	if (pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0 && pDesklet->iSidWriteSize == 0)
	{
		cairo_dock_load_desklet_decorations (pDesklet);
	}
}

void cairo_dock_load_desklet_decorations (CairoDesklet *pDesklet)
{
	cairo_dock_unload_image_buffer (&pDesklet->backGroundImageBuffer);
	cairo_dock_unload_image_buffer (&pDesklet->foreGroundImageBuffer);
	
	CairoDeskletDecoration *pDeskletDecorations;
	//cd_debug ("%s (%s)", __func__, pDesklet->cDecorationTheme);
	if (pDesklet->cDecorationTheme == NULL || strcmp (pDesklet->cDecorationTheme, "personnal") == 0)
		pDeskletDecorations = pDesklet->pUserDecoration;
	else if (strcmp (pDesklet->cDecorationTheme, "default") == 0)
		pDeskletDecorations = cairo_dock_get_desklet_decoration (myDeskletsParam.cDeskletDecorationsName);
	else
		pDeskletDecorations = cairo_dock_get_desklet_decoration (pDesklet->cDecorationTheme);
	if (pDeskletDecorations == NULL)  // peut arriver si rendering n'a pas encore charge ses decorations.
		return ;
	//cd_debug ("pDeskletDecorations : %s (%x)", pDesklet->cDecorationTheme, pDeskletDecorations);
	
	double fZoomX = 0., fZoomY = 0.;
	if  (pDeskletDecorations->cBackGroundImagePath != NULL && pDeskletDecorations->fBackGroundAlpha > 0)
	{
		//cd_debug ("bg : %s", pDeskletDecorations->cBackGroundImagePath);
		cairo_dock_load_image_buffer_full (&pDesklet->backGroundImageBuffer,
			pDeskletDecorations->cBackGroundImagePath,
			pDesklet->container.iWidth,
			pDesklet->container.iHeight,
			pDeskletDecorations->iLoadingModifier,
			pDeskletDecorations->fBackGroundAlpha);
		fZoomX = pDesklet->backGroundImageBuffer.fZoomX;
		fZoomY = pDesklet->backGroundImageBuffer.fZoomY;
	}
	if (pDeskletDecorations->cForeGroundImagePath != NULL && pDeskletDecorations->fForeGroundAlpha > 0)
	{
		//cd_debug ("fg : %s", pDeskletDecorations->cForeGroundImagePath);
		cairo_dock_load_image_buffer_full (&pDesklet->foreGroundImageBuffer,
			pDeskletDecorations->cForeGroundImagePath,
			pDesklet->container.iWidth,
			pDesklet->container.iHeight,
			pDeskletDecorations->iLoadingModifier,
			pDeskletDecorations->fForeGroundAlpha);
		fZoomX = pDesklet->foreGroundImageBuffer.fZoomX;
		fZoomY = pDesklet->foreGroundImageBuffer.fZoomY;
	}
	pDesklet->iLeftSurfaceOffset = pDeskletDecorations->iLeftMargin * fZoomX;
	pDesklet->iTopSurfaceOffset = pDeskletDecorations->iTopMargin * fZoomY;
	pDesklet->iRightSurfaceOffset = pDeskletDecorations->iRightMargin * fZoomX;
	pDesklet->iBottomSurfaceOffset = pDeskletDecorations->iBottomMargin * fZoomY;
	//g_print ("%d;%d;%d;%d ; %.2f;%.2f\n", pDesklet->iLeftSurfaceOffset, pDesklet->iTopSurfaceOffset, pDesklet->iRightSurfaceOffset, pDesklet->iBottomSurfaceOffset, fZoomX, fZoomY);
}

void cairo_dock_free_desklet_decoration (CairoDeskletDecoration *pDecoration)
{
	if (pDecoration == NULL)
		return ;
	g_free (pDecoration->cBackGroundImagePath);
	g_free (pDecoration->cForeGroundImagePath);
	g_free (pDecoration);
}

  ////////////////
 /// FACILITY ///
////////////////

void cairo_dock_add_interactive_widget_to_desklet_full (GtkWidget *pInteractiveWidget, CairoDesklet *pDesklet, int iRightMargin)
{
	g_return_if_fail (pDesklet != NULL && pInteractiveWidget != NULL);
	if (pDesklet->pInteractiveWidget != NULL || gtk_bin_get_child (GTK_BIN (pDesklet->container.pWidget)) != NULL)
	{
		cd_warning ("This desklet already has an interactive widget !");
		return;
	}
	
	//gtk_container_add (GTK_CONTAINER (pDesklet->container.pWidget), pInteractiveWidget);
	GtkWidget *pHBox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (pDesklet->container.pWidget), pHBox);
	
	gtk_box_pack_start (GTK_BOX (pHBox), pInteractiveWidget, TRUE, TRUE, 0);
	pDesklet->pInteractiveWidget = pInteractiveWidget;
	
	if (iRightMargin != 0)
	{
		GtkWidget *pMarginBox = gtk_vbox_new (FALSE, 0);
		gtk_widget_set (pMarginBox, "width-request", iRightMargin, NULL);
		gtk_box_pack_start (GTK_BOX (pHBox), pMarginBox, FALSE, FALSE, 0);  // a tester ...
	}
	
	gtk_widget_show_all (pHBox);
}

void cairo_dock_set_desklet_margin (CairoDesklet *pDesklet, int iRightMargin)
{
	g_return_if_fail (pDesklet != NULL && pDesklet->pInteractiveWidget != NULL);
	
	GtkWidget *pHBox = gtk_bin_get_child (GTK_BIN (pDesklet->container.pWidget));
	if (pHBox && pHBox != pDesklet->pInteractiveWidget)  // precaution.
	{
		GList *pChildList = gtk_container_get_children (GTK_CONTAINER (pHBox));
		if (pChildList != NULL)
		{
			if (pChildList->next != NULL)
			{
				GtkWidget *pMarginBox = GTK_WIDGET (pChildList->next->data);
				gtk_widget_set (pMarginBox, "width-request", iRightMargin, NULL);
			}
			else  // on rajoute le widget de la marge.
			{
				GtkWidget *pMarginBox = gtk_vbox_new (FALSE, 0);
				gtk_widget_set (pMarginBox, "width-request", iRightMargin, NULL);
				gtk_box_pack_start (GTK_BOX (pHBox), pMarginBox, FALSE, FALSE, 0);
			}
			g_list_free (pChildList);
		}
	}
}

GtkWidget *cairo_dock_steal_interactive_widget_from_desklet (CairoDesklet *pDesklet)
{
	if (pDesklet == NULL)
		return NULL;

	GtkWidget *pInteractiveWidget = pDesklet->pInteractiveWidget;
	if (pInteractiveWidget != NULL)
	{
		pInteractiveWidget = cairo_dock_steal_widget_from_its_container (pInteractiveWidget);
		pDesklet->pInteractiveWidget = NULL;
		GtkWidget *pBox = gtk_bin_get_child (GTK_BIN (pDesklet->container.pWidget));
		if (pBox != NULL)
			gtk_widget_destroy (pBox);
	}
	return pInteractiveWidget;
}



void cairo_dock_hide_desklet (CairoDesklet *pDesklet)
{
	if (pDesklet)
	{
		pDesklet->bAllowMinimize = TRUE;
		gtk_widget_hide (pDesklet->container.pWidget);
	}
}

void cairo_dock_show_desklet (CairoDesklet *pDesklet)
{
	if (pDesklet)
	{
		gtk_window_present(GTK_WINDOW(pDesklet->container.pWidget));
		gtk_window_move (GTK_WINDOW(pDesklet->container.pWidget), pDesklet->container.iWindowPositionX, pDesklet->container.iWindowPositionY);  // sinon le WM le place n'importe ou.
	}
}

void cairo_dock_zoom_out_desklet (CairoDesklet *pDesklet)
{
	g_return_if_fail (pDesklet != NULL);
	pDesklet->container.fRatio = 0.1;
	pDesklet->bGrowingUp = TRUE;
	cairo_dock_launch_animation (CAIRO_CONTAINER (pDesklet));
}

static void _cairo_dock_reserve_space_for_desklet (CairoDesklet *pDesklet, gboolean bReserve)
{
	cd_debug ("%s (%d)\n", __func__, bReserve);
	Window Xid = GDK_WINDOW_XID (pDesklet->container.pWidget->window);
	int left=0, right=0, top=0, bottom=0;
	int left_start_y=0, left_end_y=0, right_start_y=0, right_end_y=0, top_start_x=0, top_end_x=0, bottom_start_x=0, bottom_end_x=0;
	int iHeight = pDesklet->container.iHeight, iWidth = pDesklet->container.iWidth;
	int iX = pDesklet->container.iWindowPositionX, iY = pDesklet->container.iWindowPositionY;
	if (bReserve)
	{
		int iTopOffset, iBottomOffset, iRightOffset, iLeftOffset;
		iTopOffset = iY;
		iBottomOffset = g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - 1 - (iY + iHeight);
		iLeftOffset = iX;
		iRightOffset = g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] - 1 - (iX + iWidth);
		
		if (iBottomOffset < MIN (iLeftOffset, iRightOffset))  // en bas.
		{
			bottom = iHeight + iBottomOffset;
			bottom_start_x = iX;
			bottom_end_x = iX + iWidth;
		}
		else if (iTopOffset < MIN (iLeftOffset, iRightOffset))  // en haut.
		{
			top = iHeight + iTopOffset;
			top_start_x = iX;
			top_end_x = iX + iWidth;
		}
		else if (iLeftOffset < iRightOffset)  // a gauche.
		{
			left = iWidth + iLeftOffset;
			left_start_y = iY;
			left_end_y = iY + iHeight;
		}
		else  // a droite.
		{
			right = iWidth + iRightOffset;
			right_start_y = iY;
			right_end_y = iY + iHeight;
		}
	}
	//cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_DOCK");
	cairo_dock_set_strut_partial (Xid, left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);
	pDesklet->bSpaceReserved = bReserve;
}

void cairo_dock_set_desklet_accessibility (CairoDesklet *pDesklet, CairoDeskletVisibility iVisibility, gboolean bSaveState)
{
	cd_debug ("%s (%d)", __func__, iVisibility);
	
	//\_________________ On applique la nouvelle accessibilite.
	gtk_window_set_keep_below (GTK_WINDOW (pDesklet->container.pWidget), iVisibility == CAIRO_DESKLET_KEEP_BELOW);
	
	gtk_window_set_keep_above (GTK_WINDOW (pDesklet->container.pWidget), iVisibility == CAIRO_DESKLET_KEEP_ABOVE);

	Window Xid = GDK_WINDOW_XID (pDesklet->container.pWidget->window);
	cairo_dock_wm_set_on_widget_layer (Xid, iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER);
	
	if (iVisibility == CAIRO_DESKLET_RESERVE_SPACE)
	{
		if (! pDesklet->bSpaceReserved)
			_cairo_dock_reserve_space_for_desklet (pDesklet, TRUE);  // sinon inutile de le refaire, s'il y'a un changement de taille ce sera fait lors du configure-event.
	}
	else if (pDesklet->bSpaceReserved)
	{
		_cairo_dock_reserve_space_for_desklet (pDesklet, FALSE);
	}
	
	//\_________________ On enregistre le nouvel etat.
	pDesklet->iVisibility = iVisibility;
	
	Icon *icon = pDesklet->pIcon;
	if (bSaveState && CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_INT, "Desklet", "accessibility", iVisibility,
			G_TYPE_INVALID);
}

void cairo_dock_set_desklet_sticky (CairoDesklet *pDesklet, gboolean bSticky)
{
	//g_print ("%s (%d)\n", __func__, bSticky);
	int iNumDesktop;
	if (bSticky)
	{
		gtk_window_stick (GTK_WINDOW (pDesklet->container.pWidget));
		iNumDesktop = -1;
	}
	else
	{
		gtk_window_unstick (GTK_WINDOW (pDesklet->container.pWidget));
		int iCurrentDesktop, iCurrentViewportX, iCurrentViewportY;
		cairo_dock_get_current_desktop_and_viewport (&iCurrentDesktop, &iCurrentViewportX, &iCurrentViewportY);
		iNumDesktop = iCurrentDesktop * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY + iCurrentViewportX * g_desktopGeometry.iNbViewportY + iCurrentViewportY;
		cd_debug (">>> on colle ce desklet sur le bureau %d", iNumDesktop);
	}
	
	//\_________________ On enregistre le nouvel etat.
	Icon *icon = pDesklet->pIcon;
	if (CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_BOOLEAN, "Desklet", "sticky", bSticky,
			G_TYPE_INT, "Desklet", "num desktop", iNumDesktop,
			G_TYPE_INVALID);
}

gboolean cairo_dock_desklet_is_sticky (CairoDesklet *pDesklet)
{
	GdkWindow *window = pDesklet->container.pWidget->window;
	#if (GDK_MAJOR_VERSION >= 3)
	return (window->state & GDK_WINDOW_STATE_STICKY);  // API change: http://mail.gnome.org/archives/commits-list/2010-July/msg00002.html
	#else
	return (((GdkWindowObject*) window)->state & GDK_WINDOW_STATE_STICKY);
	#endif
}

void cairo_dock_lock_desklet_position (CairoDesklet *pDesklet, gboolean bPositionLocked)
{
	//g_print ("%s (%d)\n", __func__, bPositionLocked);
	pDesklet->bPositionLocked = bPositionLocked;
	
	//\_________________ On enregistre le nouvel etat.
	Icon *icon = pDesklet->pIcon;
	if (CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_BOOLEAN, "Desklet", "locked", pDesklet->bPositionLocked,
			G_TYPE_INVALID);
}


void cairo_dock_update_desklet_icons (CairoDesklet *pDesklet)
{
	// compute icons size
	if (pDesklet->pRenderer && pDesklet->pRenderer->calculate_icons != NULL)
		pDesklet->pRenderer->calculate_icons (pDesklet);
	
	// trigger load if changed
	Icon* pIcon = pDesklet->pIcon;
	if (pIcon)
		cairo_dock_load_icon_buffers (pIcon, CAIRO_CONTAINER (pDesklet));
	
	GList* ic;
	for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if ((int)pIcon->fWidth != pIcon->iImageWidth || (int)pIcon->fHeight != pIcon->iImageHeight)
		{
			pIcon->iImageWidth = pIcon->fWidth;
			pIcon->iImageHeight = pIcon->fHeight;
			cairo_dock_trigger_load_icon_buffers (pIcon, CAIRO_CONTAINER (pDesklet));
		}
	}
	
	// redraw
	cairo_dock_redraw_container (CAIRO_CONTAINER (pDesklet));
}

void cairo_dock_insert_icon_in_desklet (Icon *icon, CairoDesklet *pDesklet)
{
	g_return_if_fail (icon != NULL);
	if (g_list_find (pDesklet->icons, icon) != NULL)  // elle est deja dans ce desklet.
		return ;
	
	// insert icon
	pDesklet->icons = g_list_insert_sorted (pDesklet->icons,
		icon,
		(GCompareFunc)cairo_dock_compare_icons_order);
	icon->pContainerForLoad = CAIRO_CONTAINER (pDesklet);
	
	// calculate icons
	cairo_dock_update_desklet_icons (pDesklet);
}

gboolean cairo_dock_detach_icon_from_desklet (Icon *icon, CairoDesklet *pDesklet)
{
	if (pDesklet == NULL)
		return FALSE;
	
	GList *ic = g_list_find (pDesklet->icons, icon);
	if (! ic)
		return FALSE;
	
	// remove icon
	pDesklet->icons = g_list_delete_link (pDesklet->icons, ic);
	ic =  NULL;
	icon->pContainerForLoad = NULL;
	
	// calculate icons
	cairo_dock_update_desklet_icons (pDesklet);
}

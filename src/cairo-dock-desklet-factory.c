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
#include "cairo-dock-modules.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-config.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-log.h"
#include "cairo-dock-menu.h"
#include "cairo-dock-container.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-load.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-internal-desklets.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-desklet-factory.h"

extern gboolean g_bUseOpenGL;
extern CairoDockDesktopGeometry g_desktopGeometry;

static void _cairo_dock_reserve_space_for_desklet (CairoDesklet *pDesklet, gboolean bReserve);
static void on_drag_data_received_desklet (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t, CairoDesklet *pDesklet);

  ///////////////
 /// SIGNALS ///
///////////////

static gboolean on_expose_desklet(GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	CairoDesklet *pDesklet)
{
	if (pDesklet->iDesiredWidth != 0 && pDesklet->iDesiredHeight != 0 && (pDesklet->iKnownWidth != pDesklet->iDesiredWidth || pDesklet->iKnownHeight != pDesklet->iDesiredHeight))
	{
		//g_print ("on saute le dessin\n");
		if (g_bUseOpenGL)
		{
			// dessiner la fausse transparence.
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
		
		cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_RENDER_DESKLET, pDesklet, NULL);
		
		if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
			gdk_gl_drawable_swap_buffers (pGlDrawable);
		else
			glFlush ();
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
	else
	{
		cairo_t *pCairoContext = cairo_dock_create_drawing_context_on_container (CAIRO_CONTAINER (pDesklet));
		
		cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_RENDER_DESKLET, pDesklet, pCairoContext);
		
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
			pDesklet->container.iWidth - myDesklets.iDeskletButtonSize,
			pDesklet->container.iHeight - myDesklets.iDeskletButtonSize,
			myDesklets.iDeskletButtonSize,
			myDesklets.iDeskletButtonSize);
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
	if ((pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0) && pDesklet->pIcon != NULL && pDesklet->pIcon->pModuleInstance != NULL)
	{
		gchar *cSize = g_strdup_printf ("%d;%d", pDesklet->container.iWidth, pDesklet->container.iHeight);
		cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
			G_TYPE_STRING, "Desklet", "size", cSize,
			G_TYPE_INVALID);
		g_free (cSize);
		cairo_dock_update_desklet_size_in_gui (pDesklet->pIcon->pModuleInstance,
			pDesklet->container.iWidth,
			pDesklet->container.iHeight);
	}
	pDesklet->iSidWriteSize = 0;
	pDesklet->iKnownWidth = pDesklet->container.iWidth;
	pDesklet->iKnownHeight = pDesklet->container.iHeight;
	if (((pDesklet->iDesiredWidth != 0 || pDesklet->iDesiredHeight != 0) && pDesklet->iDesiredWidth == pDesklet->container.iWidth && pDesklet->iDesiredHeight == pDesklet->container.iHeight) || (pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0))
	{
		pDesklet->iDesiredWidth = 0;
		pDesklet->iDesiredHeight = 0;
		
		cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDesklet));
		cairo_dock_load_desklet_decorations (pDesklet);
		cairo_destroy (pCairoContext);
		
		if (pDesklet->pIcon != NULL && pDesklet->pIcon->pModuleInstance != NULL)
		{
			//g_print ("RELOAD\n");
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
		
		Window Xid = GDK_WINDOW_XID (pDesklet->container.pWidget->window);
		int iNumDesktop = -1;
		if (! cairo_dock_xwindow_is_sticky (Xid))
		{
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
		cairo_dock_update_desklet_position_in_gui (pDesklet->pIcon->pModuleInstance,
			iRelativePositionX,
			iRelativePositionY);
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
		pDesklet->iSidWriteSize = g_timeout_add (500, (GSourceFunc) _cairo_dock_write_desklet_size, (gpointer) pDesklet);
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

		if (pDesklet->iSidWritePosition != 0)
		{
			g_source_remove (pDesklet->iSidWritePosition);
		}
		pDesklet->iSidWritePosition = g_timeout_add (500, (GSourceFunc) _cairo_dock_write_desklet_position, (gpointer) pDesklet);
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
		Icon *icon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);
		if (icon != NULL)
		{
			cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_SCROLL_ICON, icon, pDesklet, pScroll->direction);
		}
	}
	return FALSE;
}

static gboolean on_unmap_desklet (GtkWidget* pWidget,
	GdkEvent *pEvent,
	CairoDesklet *pDesklet)
{
	g_print ("unmap desklet (bAllowMinimize:%d)\n", pDesklet->bAllowMinimize);
	Window Xid = GDK_WINDOW_XID (pWidget->window);
	if (cairo_dock_window_is_utility (Xid))  // sur la couche des widgets, on ne fait rien.
		return FALSE;
	if (! pDesklet->bAllowMinimize)
	{
		if (pDesklet->pUnmapTimer)
		{
			double fElapsedTime = g_timer_elapsed (pDesklet->pUnmapTimer, NULL);
			g_print ("fElapsedTime : %fms\n", fElapsedTime);
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
		if (pButton->type == GDK_BUTTON_PRESS)
		{
			pDesklet->bClicked = TRUE;
			if (cairo_dock_desklet_is_free (pDesklet))
			{
				pDesklet->container.iMouseX = pButton->x;  // pour le deplacement manuel.
				pDesklet->container.iMouseY = pButton->y;
				
				if (pButton->x < myDesklets.iDeskletButtonSize && pButton->y < myDesklets.iDeskletButtonSize)
					pDesklet->rotating = TRUE;
				else if (pButton->x > pDesklet->container.iWidth - myDesklets.iDeskletButtonSize && pButton->y < myDesklets.iDeskletButtonSize)
					pDesklet->retaching = TRUE;
				else if (pButton->x > (pDesklet->container.iWidth - myDesklets.iDeskletButtonSize)/2 && pButton->x < (pDesklet->container.iWidth + myDesklets.iDeskletButtonSize)/2 && pButton->y < myDesklets.iDeskletButtonSize)
					pDesklet->rotatingY = TRUE;
				else if (pButton->y > (pDesklet->container.iHeight - myDesklets.iDeskletButtonSize)/2 && pButton->y < (pDesklet->container.iHeight + myDesklets.iDeskletButtonSize)/2 && pButton->x < myDesklets.iDeskletButtonSize)
					pDesklet->rotatingX = TRUE;
				else
					pDesklet->time = pButton->time;
			}
			if (pDesklet->bAllowNoClickable && pButton->x > pDesklet->container.iWidth - myDesklets.iDeskletButtonSize && pButton->y > pDesklet->container.iHeight - myDesklets.iDeskletButtonSize)
				pDesklet->making_transparent = TRUE;
		}
		else if (pButton->type == GDK_BUTTON_RELEASE)
		{
			if (!pDesklet->bClicked)  // on n'accepte le release que si on avait clique sur le desklet avant (on peut recevoir le release si on avat clique sur un dialogue qui chevauchait notre desklet et qui a disparu au clic).
			{
				return FALSE;
			}
			pDesklet->bClicked = FALSE;
			cd_debug ("GDK_BUTTON_RELEASE");
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
			}
			else if (pDesklet->retaching)
			{
				pDesklet->retaching = FALSE;
				if (pButton->x > pDesklet->container.iWidth - myDesklets.iDeskletButtonSize && pButton->y < myDesklets.iDeskletButtonSize)  // on verifie qu'on est encore dedans.
				{
					Icon *icon = pDesklet->pIcon;
					g_return_val_if_fail (CAIRO_DOCK_IS_APPLET (icon), FALSE);
					cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
						G_TYPE_BOOLEAN, "Desklet", "initially detached", FALSE,
						G_TYPE_INVALID);
					cairo_dock_update_desklet_detached_state_in_gui (icon->pModuleInstance, FALSE);
					cairo_dock_reload_module_instance (icon->pModuleInstance, TRUE);
					return TRUE;  // interception du signal.
				}
			}
			else if (pDesklet->making_transparent)
			{
				g_print ("pDesklet->making_transparent\n");
				pDesklet->making_transparent = FALSE;
				if (pButton->x > pDesklet->container.iWidth - myDesklets.iDeskletButtonSize && pButton->y > pDesklet->container.iHeight - myDesklets.iDeskletButtonSize)  // on verifie qu'on est encore dedans.
				{
					Icon *icon = pDesklet->pIcon;
					g_return_val_if_fail (CAIRO_DOCK_IS_APPLET (icon), FALSE);
					pDesklet->bNoInput = ! pDesklet->bNoInput;
					g_print ("no input : %d (%s)\n", pDesklet->bNoInput, icon->pModuleInstance->cConfFilePath);
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
				cairo_dock_notify (CAIRO_DOCK_CLICK_ICON, pClickedIcon, pDesklet, pButton->state);
			}
		}
		else if (pButton->type == GDK_2BUTTON_PRESS)
		{
			if (! (pButton->x < myDesklets.iDeskletButtonSize && pButton->y < myDesklets.iDeskletButtonSize) && ! (pButton->x > (pDesklet->container.iWidth - myDesklets.iDeskletButtonSize)/2 && pButton->x < (pDesklet->container.iWidth + myDesklets.iDeskletButtonSize)/2 && pButton->y < myDesklets.iDeskletButtonSize))
			{
				Icon *pClickedIcon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);
				cairo_dock_notify (CAIRO_DOCK_DOUBLE_CLICK_ICON, pClickedIcon, pDesklet);
			}
		}
	}
	else if (pButton->button == 3 && pButton->type == GDK_BUTTON_PRESS)  // clique droit.
	{
		Icon *pClickedIcon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);
		GtkWidget *menu = cairo_dock_build_menu (pClickedIcon, CAIRO_CONTAINER (pDesklet));  // genere un CAIRO_DOCK_BUILD_ICON_MENU.
		gtk_widget_show_all (menu);
		gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			NULL,
			NULL,
			1,
			gtk_get_current_event_time ());
		pDesklet->container.bInside = FALSE;
		///pDesklet->iGradationCount = 0;  // on force le fond a redevenir transparent.
		gtk_widget_queue_draw (pDesklet->container.pWidget);
	}
	else if (pButton->button == 2 && pButton->type == GDK_BUTTON_PRESS)  // clique milieu.
	{
		if (pButton->x < myDesklets.iDeskletButtonSize && pButton->y < myDesklets.iDeskletButtonSize)
		{
			pDesklet->fRotation = 0.;
			gtk_widget_queue_draw (pDesklet->container.pWidget);
			cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
				G_TYPE_INT, "Desklet", "rotation", 0,
				G_TYPE_INVALID);
		}
		else if (pButton->x > (pDesklet->container.iWidth - myDesklets.iDeskletButtonSize)/2 && pButton->x < (pDesklet->container.iWidth + myDesklets.iDeskletButtonSize)/2 && pButton->y < myDesklets.iDeskletButtonSize)
		{
			pDesklet->fDepthRotationY = 0.;
			gtk_widget_queue_draw (pDesklet->container.pWidget);
			cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
				G_TYPE_INT, "Desklet", "depth rotation y", 0,
				G_TYPE_INVALID);
		}
		else if (pButton->y > (pDesklet->container.iHeight - myDesklets.iDeskletButtonSize)/2 && pButton->y < (pDesklet->container.iHeight + myDesklets.iDeskletButtonSize)/2 && pButton->x < myDesklets.iDeskletButtonSize)
		{
			pDesklet->fDepthRotationX = 0.;
			gtk_widget_queue_draw (pDesklet->container.pWidget);
			cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
				G_TYPE_INT, "Desklet", "depth rotation x", 0,
				G_TYPE_INVALID);
		}
		else
		{
			cairo_dock_notify (CAIRO_DOCK_MIDDLE_CLICK_ICON, pDesklet->pIcon, pDesklet);
		}
	}
	return FALSE;
}

static void on_drag_data_received_desklet (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t, CairoDesklet *pDesklet)
{
	//g_print ("%s (%dx%d)\n", __func__, x, y);
	
	//\_________________ On recupere l'URI.
	gchar *cReceivedData = (gchar *) selection_data->data;
	g_return_if_fail (cReceivedData != NULL);
	
	pDesklet->container.iMouseX = x;
	pDesklet->container.iMouseY = y;
	Icon *pClickedIcon = cairo_dock_find_clicked_icon_in_desklet (pDesklet);
	cairo_dock_notify_drop_data (cReceivedData, pClickedIcon, 0, CAIRO_CONTAINER (pDesklet));
}

static gboolean on_motion_notify_desklet(GtkWidget *pWidget,
	GdkEventMotion* pMotion,
	CairoDesklet *pDesklet)
{
	if (pMotion->state & GDK_BUTTON1_MASK && cairo_dock_desklet_is_free (pDesklet))
	{
		cd_debug ("root : %d;%d", (int) pMotion->x_root, (int) pMotion->y_root);
		/*pDesklet->moving = TRUE;
		gtk_window_move (GTK_WINDOW (pWidget),
			pMotion->x_root + pDesklet->diff_x,
			pMotion->y_root + pDesklet->diff_y);*/
	}
	else  // le 'press-button' est local au sous-widget clique, alors que le 'motion-notify' est global a la fenetre; c'est donc par lui qu'on peut avoir a coup sur les coordonnees du curseur (juste avant le clic).
	{
		pDesklet->container.iMouseX = pMotion->x;
		pDesklet->container.iMouseY = pMotion->y;
		gboolean bStartAnimation = FALSE;
		cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_MOUSE_MOVED, pDesklet, &bStartAnimation);
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
				cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_ENTER_ICON, pIcon, pDesklet, &bStartAnimation);
			}
		}
		else
		{
			Icon *pPointedIcon = cairo_dock_get_pointed_icon (pDesklet->icons);
			if (pPointedIcon != NULL)
			{
				pPointedIcon->bPointed = FALSE;
				
				//g_print ("kedal\n");
				cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_ENTER_ICON, pPointedIcon, pDesklet, &bStartAnimation);

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
			cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_ENTER_DESKLET, pDesklet, &bStartAnimation);
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
	cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_LEAVE_DESKLET, pDesklet, &bStartAnimation);
	if (bStartAnimation)
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDesklet));
	
	return FALSE;
}


  ///////////////
 /// FACTORY ///
///////////////

CairoDesklet *cairo_dock_new_desklet (void)
{
	cd_message ("%s ()", __func__);
	CairoDesklet *pDesklet = g_new0(CairoDesklet, 1);
	pDesklet->container.iType = CAIRO_DOCK_TYPE_DESKLET;
	pDesklet->container.bIsHorizontal = TRUE;
	pDesklet->container.bDirectionUp = TRUE;
	pDesklet->container.fRatio = 1;
	
	GtkWidget* pWindow = cairo_dock_create_container_window ();
	pDesklet->container.pWidget = pWindow;
	
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
	
	gtk_widget_show_all(pWindow);
	
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
	if (pDesklet->container.iSidGLAnimation != 0)
		g_source_remove (pDesklet->container.iSidGLAnimation);
	cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_STOP_DESKLET, pDesklet);
	
	cairo_dock_steal_interactive_widget_from_desklet (pDesklet);

	gtk_widget_destroy (pDesklet->container.pWidget);
	pDesklet->container.pWidget = NULL;
	
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
		pDesklet->icons = NULL;
	}
	
	g_free (pDesklet->cDecorationTheme);
	cairo_dock_free_desklet_decoration (pDesklet->pUserDecoration);
	
	cairo_dock_unload_image_buffer (&pDesklet->backGroundImageBuffer);
	cairo_dock_unload_image_buffer (&pDesklet->foreGroundImageBuffer);
	
	g_free(pDesklet);
}


void cairo_dock_configure_desklet (CairoDesklet *pDesklet, CairoDeskletAttribute *pAttribute)
{
	cd_debug ("%s (%dx%d ; (%d,%d) ; %d)", __func__, pAttribute->iDeskletWidth, pAttribute->iDeskletHeight, pAttribute->iDeskletPositionX, pAttribute->iDeskletPositionY, pAttribute->iAccessibility);
	if (pAttribute->bDeskletUseSize && (pAttribute->iDeskletWidth != pDesklet->container.iWidth || pAttribute->iDeskletHeight != pDesklet->container.iHeight))
	{
		pDesklet->iDesiredWidth = pAttribute->iDeskletWidth;
		pDesklet->iDesiredHeight = pAttribute->iDeskletHeight;
		gdk_window_resize (pDesklet->container.pWidget->window,
			pAttribute->iDeskletWidth,
			pAttribute->iDeskletHeight);
	}
	if (! pAttribute->bDeskletUseSize)
		gtk_container_set_border_width (GTK_CONTAINER (pDesklet->container.pWidget), 0);
	
	int iAbsolutePositionX = (pAttribute->iDeskletPositionX < 0 ? g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] + pAttribute->iDeskletPositionX : pAttribute->iDeskletPositionX);
	iAbsolutePositionX = MAX (0, MIN (g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] - pAttribute->iDeskletWidth, iAbsolutePositionX));
	int iAbsolutePositionY = (pAttribute->iDeskletPositionY < 0 ? g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] + pAttribute->iDeskletPositionY : pAttribute->iDeskletPositionY);
	iAbsolutePositionY = MAX (0, MIN (g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - pAttribute->iDeskletHeight, iAbsolutePositionY));
	
	if (pAttribute->bOnAllDesktops)
		gdk_window_move(pDesklet->container.pWidget->window,
			iAbsolutePositionX,
			iAbsolutePositionY);
	//g_print (" let's place the deklet at (%d;%d)", iAbsolutePositionX, iAbsolutePositionY);

	cairo_dock_set_desklet_accessibility (pDesklet, pAttribute->iAccessibility, FALSE);
	
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
			g_print (">>> on fixe le desklet sur le bureau (%d,%d,%d) (cur : %d,%d,%d)\n", iNumDesktop, iNumViewportX, iNumViewportY, iCurrentDesktop, iCurrentViewportX, iCurrentViewportY);
			
			iNumViewportX -= iCurrentViewportX;
			iNumViewportY -= iCurrentViewportY;
			
			g_print ("on le place en %d + %d\n", iNumViewportX * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL], iAbsolutePositionX);
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
		cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDesklet));
		cairo_dock_load_desklet_decorations (pDesklet);
		cairo_destroy (pCairoContext);
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
		pDeskletDecorations = cairo_dock_get_desklet_decoration (myDesklets.cDeskletDecorationsName);
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
		fZoomX = pDesklet->backGroundImageBuffer.fZoomX;
		fZoomY = pDesklet->backGroundImageBuffer.fZoomY;
	}
	pDesklet->iLeftSurfaceOffset = pDeskletDecorations->iLeftMargin * fZoomX;
	pDesklet->iTopSurfaceOffset = pDeskletDecorations->iTopMargin * fZoomY;
	pDesklet->iRightSurfaceOffset = pDeskletDecorations->iRightMargin * fZoomX;
	pDesklet->iBottomSurfaceOffset = pDeskletDecorations->iBottomMargin * fZoomY;
	//cd_debug ("%d;%d;%d;%d ; %.2f;%.2f", pDesklet->iLeftSurfaceOffset, pDesklet->iTopSurfaceOffset, pDesklet->iRightSurfaceOffset, pDesklet->iBottomSurfaceOffset, pDesklet->fBackGroundAlpha, pDesklet->fForeGroundAlpha);
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
		gtk_window_present(GTK_WINDOW(pDesklet->container.pWidget));
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
	g_print ("%s (%d)\n", __func__, bReserve);
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

//for compiz fusion "widget layer"
//set behaviour in compiz to: (class=Cairo-dock & type=utility)
void cairo_dock_set_desklet_accessibility (CairoDesklet *pDesklet, CairoDeskletAccessibility iAccessibility, gboolean bSaveState)
{
	g_print ("%s (%d)\n", __func__, iAccessibility);
	
	//\_________________ On applique la nouvelle accessibilite.
	gtk_window_set_keep_below (GTK_WINDOW (pDesklet->container.pWidget), iAccessibility == CAIRO_DESKLET_KEEP_BELOW);
	gtk_window_set_keep_above (GTK_WINDOW (pDesklet->container.pWidget), iAccessibility == CAIRO_DESKLET_KEEP_ABOVE);

	Window Xid = GDK_WINDOW_XID (pDesklet->container.pWidget->window);
	if (iAccessibility == CAIRO_DESKLET_ON_WIDGET_LAYER)
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_UTILITY");  // le hide-show le fait deconner completement, il perd son skip_task_bar ! au moins sous KDE3.
	else
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_NORMAL");
	
	if (iAccessibility == CAIRO_DESKLET_RESERVE_SPACE)
	{
		if (! pDesklet->bSpaceReserved)
			_cairo_dock_reserve_space_for_desklet (pDesklet, TRUE);  // sinon inutile de le refaire, s'il y'a un changement de taille ce sera fait lors du configure-event.
	}
	else if (pDesklet->bSpaceReserved)
	{
		_cairo_dock_reserve_space_for_desklet (pDesklet, FALSE);
	}
	
	//\_________________ On enregistre le nouvel etat.
	Icon *icon = pDesklet->pIcon;
	if (bSaveState && CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_INT, "Desklet", "accessibility", iAccessibility,
			G_TYPE_INVALID);
	
	//\_________________ On verifie que la regle de Compiz est correcte.
	if (iAccessibility == CAIRO_DESKLET_ON_WIDGET_LAYER && bSaveState)
	{
		// pour activer le plug-in, recuperer la liste, y rajouter widget-layer, puis envoyer :
		//dbus-send --print-reply --type=method_call  --dest=org.freedesktop.compiz  /org/freedesktop/compiz/core/allscreens/active_plugins  org.freedesktop.compiz.get
		// dbus-send --print-reply --type=method_call  --dest=org.freedesktop.compiz  /org/freedesktop/compiz/core/allscreens/active_plugins  org.freedesktop.compiz.set array:string:"foo",string:"fum"
		
		// on recupere la regle
		gchar *cDbusAnswer = cairo_dock_launch_command_sync ("dbus-send --print-reply --type=method_call --dest=org.freedesktop.compiz /org/freedesktop/compiz/widget/screen0/match org.freedesktop.compiz.get");
		g_print ("cDbusAnswer : '%s'\n", cDbusAnswer);
		gchar *cRule = NULL;
		gchar *str = (cDbusAnswer ? strchr (cDbusAnswer, '\n') : NULL);
		if (str)
		{
			str ++;
			while (*str == ' ')
				str ++;
			if (strncmp (str, "string", 6) == 0)
			{
				str += 6;
				while (*str == ' ')
					str ++;
				if (*str == '"')
				{
					str ++;
					gchar *ptr = strrchr (str, '"');
					if (ptr)
					{
						*ptr = '\0';
						cRule = g_strdup (str);
					}
				}
			}
		}
		g_free (cDbusAnswer);
		g_print ("got rule : '%s'\n", cRule);
		
		if (cRule == NULL)  /// on ne sait pas le distinguer d'une regle vide...
		{
			cd_warning ("couldn't get Widget Layer rule from Compiz");
		}
		
		// on complete la regle si necessaire.
		if (cRule == NULL || *cRule == '\0' || (g_strstr_len (cRule, -1, "class=Cairo-dock & type=utility") == NULL && g_strstr_len (cRule, -1, "(class=Cairo-dock) & (type=utility)") == NULL && g_strstr_len (cRule, -1, "name=cairo-dock & type=utility") == NULL))
		{
			gchar *cNewRule = (cRule == NULL || *cRule == '\0' ?
				g_strdup ("(class=Cairo-dock & type=utility)") :
				g_strdup_printf ("(%s) | (class=Cairo-dock & type=utility)", cRule));
			g_print ("set rule : %s\n", cNewRule);
			gchar *cCommand = g_strdup_printf ("dbus-send --print-reply --type=method_call --dest=org.freedesktop.compiz /org/freedesktop/compiz/widget/screen0/match org.freedesktop.compiz.set string:\"%s\"", cNewRule);
			cairo_dock_launch_command_sync (cCommand);
			g_free (cCommand);
			g_free (cNewRule);
		}
		g_free (cRule);
	}
}

void cairo_dock_set_desklet_sticky (CairoDesklet *pDesklet, gboolean bSticky)
{
	g_print ("%s (%d)\n", __func__, bSticky);
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
		g_print (">>> on colle ce desklet sur le bureau %d\n", iNumDesktop);
	}
	
	//\_________________ On enregistre le nouvel etat.
	Icon *icon = pDesklet->pIcon;
	if (CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_BOOLEAN, "Desklet", "sticky", bSticky,
			G_TYPE_INT, "Desklet", "num desktop", iNumDesktop,
			G_TYPE_INVALID);
}

void cairo_dock_lock_desklet_position (CairoDesklet *pDesklet, gboolean bPositionLocked)
{
	g_print ("%s (%d)\n", __func__, bPositionLocked);
	pDesklet->bPositionLocked = bPositionLocked;
	
	//\_________________ On enregistre le nouvel etat.
	Icon *icon = pDesklet->pIcon;
	if (CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_BOOLEAN, "Desklet", "locked", pDesklet->bPositionLocked,
			G_TYPE_INVALID);
}

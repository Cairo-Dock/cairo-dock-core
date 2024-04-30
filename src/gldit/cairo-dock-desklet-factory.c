/**
* This file is a part of the Cairo-Dock project
* Login : <ctaf42@gmail.com>
* Started on  Sun Jan 27 18:35:38 2008 Cedric GESTES
* $Id$
*
* Author(s)
*  - Cedric GESTES <ctaf42@gmail.com>
*  - Fabrice REY
*
* Copyright : (C) 2008 Cedric GESTES
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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_set_icon_container
#include "cairo-dock-module-instance-manager.h"  // gldi_module_instance_detach
#include "cairo-dock-dialog-manager.h"  // gldi_dialogs_replace_all
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_update_conf_file
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-container.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-menu.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-desklet-factory.h"

extern gboolean g_bUseOpenGL;

static void _reserve_space_for_desklet (CairoDesklet *pDesklet, gboolean bReserve);

#define CD_WRITE_DELAY 600  // ms

  ///////////////
 /// SIGNALS ///
///////////////

static gboolean on_expose_desklet(G_GNUC_UNUSED GtkWidget *pWidget, G_GNUC_UNUSED cairo_t *pCairoContext, CairoDesklet *pDesklet)
{
	if (pDesklet->iDesiredWidth != 0 && pDesklet->iDesiredHeight != 0 && (pDesklet->iKnownWidth != pDesklet->iDesiredWidth || pDesklet->iKnownHeight != pDesklet->iDesiredHeight))  // skip the drawing until the desklet has reached its size, only make it transparent.
	{
		//g_print ("on saute le dessin\n");
		if (g_bUseOpenGL)
		{
			if (! gldi_gl_container_begin_draw (CAIRO_CONTAINER (pDesklet)))
				return FALSE;
			
			gldi_gl_container_end_draw (CAIRO_CONTAINER (pDesklet));
		}
		else
		{
			cairo_dock_init_drawing_context_on_container (CAIRO_CONTAINER (pDesklet), pCairoContext);
		}
		return FALSE;
	}
	
	if (g_bUseOpenGL && pDesklet->pRenderer && pDesklet->pRenderer->render_opengl)
	{
		if (! gldi_gl_container_begin_draw (CAIRO_CONTAINER (pDesklet)))
			return FALSE;
		
		gldi_object_notify (pDesklet, NOTIFICATION_RENDER, pDesklet, NULL);
		
		gldi_gl_container_end_draw (CAIRO_CONTAINER (pDesklet));
	}
	else
	{
		cairo_dock_init_drawing_context_on_container (CAIRO_CONTAINER (pDesklet), pCairoContext);
		
		gldi_object_notify (pDesklet, NOTIFICATION_RENDER, pDesklet, pCairoContext);
	}
	
	return FALSE;
}

static void _cairo_dock_set_desklet_input_shape (CairoDesklet *pDesklet)
{
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDesklet), NULL);
	
	if (pDesklet->bNoInput)
	{
		cairo_region_t *pShapeBitmap = gldi_container_create_input_shape (CAIRO_CONTAINER (pDesklet),
			pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize,
			pDesklet->container.iHeight - myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize);
		
		gldi_container_set_input_shape (CAIRO_CONTAINER (pDesklet), pShapeBitmap);
		
		cairo_region_destroy (pShapeBitmap);
	}
}

static gboolean _cairo_dock_write_desklet_size (CairoDesklet *pDesklet)
{
	if ((pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0) && pDesklet->pIcon != NULL && pDesklet->pIcon->pModuleInstance != NULL && gldi_desklet_manager_is_ready ())
	{
		gchar *cSize = g_strdup_printf ("%d;%d", pDesklet->container.iWidth, pDesklet->container.iHeight);
		cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
			G_TYPE_STRING, "Desklet", "size", cSize,
			G_TYPE_INVALID);
		g_free (cSize);
		gldi_object_notify (pDesklet, NOTIFICATION_CONFIGURE_DESKLET, pDesklet);
	}
	pDesklet->iSidWriteSize = 0;
	pDesklet->iKnownWidth = pDesklet->container.iWidth;
	pDesklet->iKnownHeight = pDesklet->container.iHeight;
	if (((pDesklet->iDesiredWidth != 0 || pDesklet->iDesiredHeight != 0) && pDesklet->iDesiredWidth == pDesklet->container.iWidth && pDesklet->iDesiredHeight == pDesklet->container.iHeight) || (pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0))
	{
		pDesklet->iDesiredWidth = 0;
		pDesklet->iDesiredHeight = 0;
		
		gldi_desklet_load_desklet_decorations (pDesklet);  // reload the decorations at the new desklet size.
		
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
				if (pIcon)  // if the view doesn't display the main icon, it will set the allocated size to 0 so that the icon won't be loaded.
				{
					cairo_dock_load_icon_buffers (pIcon, CAIRO_CONTAINER (pDesklet));  // pas de trigger, car on veut pouvoir associer un contexte a l'icone principale tout de suite.
				}
				
				// on recharge chaque icone.
				GList* ic;
				for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
				{
					pIcon = ic->data;
					if (cairo_dock_icon_get_allocated_width (pIcon) != pIcon->image.iWidth || cairo_dock_icon_get_allocated_height (pIcon) != pIcon->image.iHeight)
					{
						cairo_dock_trigger_load_icon_buffers (pIcon);
					}
				}
				
				// on recharge les buffers de la vue.
				if (pRenderer->load_data != NULL)
					pRenderer->load_data (pDesklet);
			}
			
			// on recharge le module associe.
			gldi_object_reload (GLDI_OBJECT(pDesklet->pIcon->pModuleInstance), FALSE);
			
			gtk_widget_queue_draw (pDesklet->container.pWidget);  // sinon on ne redessine que l'interieur.
		}
		
		if (pDesklet->bSpaceReserved)  // l'espace est reserve, on reserve la nouvelle taille.
		{
			_reserve_space_for_desklet (pDesklet, TRUE);
		}
	}
	
	//g_print ("iWidth <- %d;iHeight <- %d ; (%dx%d) (%x)\n", pDesklet->container.iWidth, pDesklet->container.iHeight, pDesklet->iKnownWidth, pDesklet->iKnownHeight, pDesklet->pIcon);
	return FALSE;
}
static gboolean _cairo_dock_write_desklet_position (CairoDesklet *pDesklet)
{
	if (pDesklet->pIcon != NULL && pDesklet->pIcon->pModuleInstance != NULL)
	{
		int iRelativePositionX = (pDesklet->container.iWindowPositionX + pDesklet->container.iWidth/2 <= gldi_desktop_get_width()/2 ? pDesklet->container.iWindowPositionX : pDesklet->container.iWindowPositionX - gldi_desktop_get_width());
		int iRelativePositionY = (pDesklet->container.iWindowPositionY + pDesklet->container.iHeight/2 <= gldi_desktop_get_height()/2 ? pDesklet->container.iWindowPositionY : pDesklet->container.iWindowPositionY - gldi_desktop_get_height());
		
		int iNumDesktop = -1;
		if (! gldi_desklet_is_sticky (pDesklet))
		{
			iNumDesktop = gldi_container_get_current_desktop_index (CAIRO_CONTAINER (pDesklet));
			//g_print ("desormais on place le desklet sur le bureau (%d,%d,%d)\n", iDesktop, iViewportX, iViewportY);
		}
		cd_debug ("%d; %d; %d", iNumDesktop, iRelativePositionX, iRelativePositionY);
		cairo_dock_update_conf_file (pDesklet->pIcon->pModuleInstance->cConfFilePath,
			G_TYPE_INT, "Desklet", "x position", iRelativePositionX,
			G_TYPE_INT, "Desklet", "y position", iRelativePositionY,
			G_TYPE_INT, "Desklet", "num desktop", iNumDesktop,
			G_TYPE_INVALID);
		gldi_object_notify (pDesklet, NOTIFICATION_CONFIGURE_DESKLET, pDesklet);
	}
	
	if (pDesklet->bSpaceReserved)  // l'espace est reserve, on reserve a la nouvelle position.
	{
		_reserve_space_for_desklet (pDesklet, TRUE);
	}
	if (pDesklet->pIcon && gldi_icon_has_dialog (pDesklet->pIcon))
	{
		gldi_dialogs_replace_all ();
	}
	pDesklet->iSidWritePosition = 0;
	return FALSE;
}
static gboolean on_configure_desklet (G_GNUC_UNUSED GtkWidget* pWidget,
	GdkEventConfigure* pEvent,
	CairoDesklet *pDesklet)
{
	//g_print (" >>>>>>>>> %s (%dx%d, %d;%d)", __func__, pEvent->width, pEvent->height, pEvent->x, pEvent->y);
	if (pDesklet->container.iWidth != pEvent->width || pDesklet->container.iHeight != pEvent->height)
	{
		if ((pEvent->width < pDesklet->container.iWidth || pEvent->height < pDesklet->container.iHeight) && (pDesklet->iDesiredWidth != 0 && pDesklet->iDesiredHeight != 0))
		{
			gdk_window_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pDesklet)),
				pDesklet->iDesiredWidth,
				pDesklet->iDesiredHeight);
		}
		
		pDesklet->container.iWidth = pEvent->width;
		pDesklet->container.iHeight = pEvent->height;
		
		if (g_bUseOpenGL)
		{
			gldi_gl_container_resized (CAIRO_CONTAINER (pDesklet), pEvent->width, pEvent->height);
			
			if (! gldi_gl_container_make_current (CAIRO_CONTAINER (pDesklet)))
				return FALSE;
			
			gldi_gl_container_set_perspective_view (CAIRO_CONTAINER (pDesklet));
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
		x += gldi_desktop_get_width();
	while (x >= gldi_desktop_get_width())
		x -= gldi_desktop_get_width();
	while (y < 0)
		y += gldi_desktop_get_height();
	while (y >= gldi_desktop_get_height())
		y -= gldi_desktop_get_height();
	//g_print (" => (%d;%d)\n", x, y);
	if (pDesklet->container.iWindowPositionX != x || pDesklet->container.iWindowPositionY != y)
	{
		pDesklet->container.iWindowPositionX = x;
		pDesklet->container.iWindowPositionY = y;

		if (gldi_desklet_manager_is_ready ())
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

static gboolean on_scroll_desklet (G_GNUC_UNUSED GtkWidget* pWidget,
	GdkEventScroll* pScroll,
	CairoDesklet *pDesklet)
{
	//g_print ("scroll %d\n", pScroll->direction);
	if (pScroll->direction != GDK_SCROLL_UP && pScroll->direction != GDK_SCROLL_DOWN)  // on degage les scrolls horizontaux.
	{
		return FALSE;
	}
	Icon *icon = gldi_desklet_find_clicked_icon (pDesklet);  // can be NULL
	gldi_object_notify (pDesklet, NOTIFICATION_SCROLL_ICON, icon, pDesklet, pScroll->direction);
	return FALSE;
}

static gboolean on_unmap_desklet (GtkWidget* pWidget,
	G_GNUC_UNUSED GdkEvent *pEvent,
	CairoDesklet *pDesklet)
{
	cd_debug ("unmap desklet (bAllowMinimize:%d)", pDesklet->bAllowMinimize);
	if (pDesklet->iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER)  // on the widget layer, let pass the unmap event..
		return FALSE;
	if (! pDesklet->bAllowMinimize)
	{
		if (pDesklet->pUnmapTimer)
		{
			double fElapsedTime = g_timer_elapsed (pDesklet->pUnmapTimer, NULL);
			cd_debug ("fElapsedTime : %fms", fElapsedTime);
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

static gboolean on_button_press_desklet(G_GNUC_UNUSED GtkWidget *pWidget,
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
				gldi_object_notify (pDesklet, NOTIFICATION_CONFIGURE_DESKLET, pDesklet);
			}
			else if (pDesklet->retaching)
			{
				pDesklet->retaching = FALSE;
				if (x > pDesklet->container.iWidth - myDeskletsParam.iDeskletButtonSize && y < myDeskletsParam.iDeskletButtonSize)  // on verifie qu'on est encore dedans.
				{
					Icon *icon = pDesklet->pIcon;
					g_return_val_if_fail (CAIRO_DOCK_IS_APPLET (icon), FALSE);
					gldi_module_instance_detach (icon->pModuleInstance);
					return GLDI_NOTIFICATION_INTERCEPT;  // interception du signal.
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
					cd_debug ("no input : %d (%s)", pDesklet->bNoInput, icon->pModuleInstance->cConfFilePath);
					cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
						G_TYPE_BOOLEAN, "Desklet", "no input", pDesklet->bNoInput,
						G_TYPE_INVALID);
					_cairo_dock_set_desklet_input_shape (pDesklet);
					gtk_widget_queue_draw (pDesklet->container.pWidget);
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
				Icon *pClickedIcon = gldi_desklet_find_clicked_icon (pDesklet);
				gldi_object_notify (pDesklet, NOTIFICATION_CLICK_ICON, pClickedIcon, pDesklet, pButton->state);
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
				Icon *pClickedIcon = gldi_desklet_find_clicked_icon (pDesklet);  // can be NULL
				gldi_object_notify (pDesklet, NOTIFICATION_DOUBLE_CLICK_ICON, pClickedIcon, pDesklet);
			}
		}
	}
	else if (pButton->button == 3 && pButton->type == GDK_BUTTON_PRESS)  // clique droit.
	{
		Icon *pClickedIcon = gldi_desklet_find_clicked_icon (pDesklet);
		GtkWidget *menu = gldi_container_build_menu (CAIRO_CONTAINER (pDesklet), pClickedIcon);  // genere un CAIRO_DOCK_BUILD_ICON_MENU.
		gldi_menu_popup (menu);
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
			gldi_object_notify (pDesklet, NOTIFICATION_CONFIGURE_DESKLET, pDesklet);
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
			gldi_object_notify (pDesklet, NOTIFICATION_MIDDLE_CLICK_ICON, pDesklet->pIcon, pDesklet);
		}
	}
	return FALSE;
}

static void _on_drag_data_received (G_GNUC_UNUSED GtkWidget *pWidget, G_GNUC_UNUSED GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, G_GNUC_UNUSED guint info, guint time, CairoDesklet *pDesklet)
{
	//\_________________ On recupere l'URI.
	gchar *cReceivedData = (gchar *) gtk_selection_data_get_data (selection_data);  // the data are actually 'const guchar*', but since we only allowed text and urls, it will be a string
	g_return_if_fail (cReceivedData != NULL);
	int length = strlen (cReceivedData);
	if (cReceivedData[length-1] == '\n')
		cReceivedData[--length] = '\0';  // on vire le retour chariot final.
	if (cReceivedData[length-1] == '\r')
		cReceivedData[--length] = '\0';
	
	// g_print ("%s (%dx%d, %s)\n", __func__, x, y, cReceivedData);
	
	pDesklet->container.iMouseX = x;
	pDesklet->container.iMouseY = y;
	Icon *pClickedIcon = gldi_desklet_find_clicked_icon (pDesklet);
	gldi_container_notify_drop_data (CAIRO_CONTAINER (pDesklet), cReceivedData, pClickedIcon, 0);
	
	gtk_drag_finish (dc, TRUE, FALSE, time);
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
		gldi_object_notify (pDesklet, NOTIFICATION_MOUSE_MOVED, pDesklet, &bStartAnimation);
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
		Icon *pIcon = gldi_desklet_find_clicked_icon (pDesklet);
		if (pIcon != NULL)
		{
			if (! pIcon->bPointed)
			{
				Icon *pPointedIcon = cairo_dock_get_pointed_icon (pDesklet->icons);
				if (pPointedIcon != NULL)
					pPointedIcon->bPointed = FALSE;
				pIcon->bPointed = TRUE;
				
				//g_print ("on survole %s\n", pIcon->cName);
				gldi_object_notify (pDesklet, NOTIFICATION_ENTER_ICON, pIcon, pDesklet, &bStartAnimation);
			}
		}
		else
		{
			Icon *pPointedIcon = cairo_dock_get_pointed_icon (pDesklet->icons);
			if (pPointedIcon != NULL)
			{
				pPointedIcon->bPointed = FALSE;
				
				//g_print ("kedal\n");
				//gldi_object_notify (pDesklet, NOTIFICATION_ENTER_ICON, pPointedIcon, pDesklet, &bStartAnimation);
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


static gboolean on_focus_in_out_desklet(G_GNUC_UNUSED GtkWidget *widget,
	G_GNUC_UNUSED GdkEventFocus *event,
	CairoDesklet *pDesklet)
{
	gtk_widget_queue_draw(pDesklet->container.pWidget);
	return FALSE;
}

static gboolean on_enter_desklet (GtkWidget* pWidget,
	G_GNUC_UNUSED GdkEventCrossing* pEvent,
	CairoDesklet *pDesklet)
{
	//g_print ("%s (%d)\n", __func__, pDesklet->container.bInside);
	if (! pDesklet->container.bInside)  // avant on etait dehors, on redessine donc.
	{
		pDesklet->container.bInside = TRUE;
		gtk_widget_queue_draw (pWidget);  // redessin des boutons.
		
		gboolean bStartAnimation = FALSE;
		gldi_object_notify (pDesklet, NOTIFICATION_ENTER_DESKLET, pDesklet, &bStartAnimation);
		if (bStartAnimation)
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDesklet));
	}
	return FALSE;
}
static gboolean on_leave_desklet (GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDesklet *pDesklet)
{
	//g_print ("%s (%d, %p, %d;%d)\n", __func__, pDesklet->container.bInside, pEvent, iMouseX, iMouseY);
	int iMouseX, iMouseY;
	if (pEvent != NULL)
	{
		iMouseX = pEvent->x;
		iMouseY = pEvent->y;
	}
	else
	{
		gldi_container_update_mouse_position (CAIRO_CONTAINER (pDesklet));
		iMouseX = pDesklet->container.iMouseX;
		iMouseY = pDesklet->container.iMouseY;
	}
	if (gtk_bin_get_child (GTK_BIN (pDesklet->container.pWidget)) != NULL && iMouseX > 0 && iMouseX < pDesklet->container.iWidth && iMouseY > 0 && iMouseY < pDesklet->container.iHeight)  // en fait on est dans un widget fils, donc on ne fait rien.
	{
		return FALSE;
	}
	
	pDesklet->container.bInside = FALSE;
	Icon *pPointedIcon = cairo_dock_get_pointed_icon (pDesklet->icons);
	if (pPointedIcon != NULL)
		pPointedIcon->bPointed = FALSE;
	gtk_widget_queue_draw (pWidget);  // redessin des boutons.
	
	gboolean bStartAnimation = FALSE;
	gldi_object_notify (pDesklet, NOTIFICATION_LEAVE_DESKLET, pDesklet, &bStartAnimation);
	if (bStartAnimation)
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDesklet));
	
	return FALSE;
}


  ///////////////
 /// FACTORY ///
///////////////


static gboolean _cairo_desklet_animation_loop (GldiContainer *pContainer)
{
	CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
	gboolean bContinue = FALSE;
	gboolean bUpdateSlowAnimation = FALSE;
	pContainer->iAnimationStep ++;
	if (pContainer->iAnimationStep * pContainer->iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pContainer->iAnimationStep = 0;
		pContainer->bKeepSlowAnimation = FALSE;
	}
	
	if (pDesklet->pIcon != NULL)
	{
		gboolean bIconIsAnimating = FALSE;
		
		if (bUpdateSlowAnimation)
		{
			gldi_object_notify (pDesklet->pIcon, NOTIFICATION_UPDATE_ICON_SLOW, pDesklet->pIcon, pDesklet, &bIconIsAnimating);
			pDesklet->container.bKeepSlowAnimation |= bIconIsAnimating;
		}
		
		gldi_object_notify (pDesklet->pIcon, NOTIFICATION_UPDATE_ICON, pDesklet->pIcon, pDesklet, &bIconIsAnimating);
		if (! bIconIsAnimating)
			pDesklet->pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
		else
			bContinue = TRUE;
	}
	
	if (bUpdateSlowAnimation)
	{
		gldi_object_notify (pDesklet, NOTIFICATION_UPDATE_SLOW, pDesklet, &pContainer->bKeepSlowAnimation);
	}
	
	gldi_object_notify (pDesklet, NOTIFICATION_UPDATE, pDesklet, &bContinue);
	
	if (! bContinue && ! pContainer->bKeepSlowAnimation)
	{
		pContainer->iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}

static void _update_desklet_icons (CairoDesklet *pDesklet)
{
	// compute icons size
	if (pDesklet->pRenderer && pDesklet->pRenderer->calculate_icons != NULL)
		pDesklet->pRenderer->calculate_icons (pDesklet);
	
	// trigger load if changed
	Icon* pIcon = pDesklet->pIcon;
	if (pIcon)
	{
		if (cairo_dock_icon_get_allocated_width (pIcon) != pIcon->image.iWidth || cairo_dock_icon_get_allocated_height (pIcon) != pIcon->image.iHeight)
		{
			cairo_dock_trigger_load_icon_buffers (pIcon);
		}
	}
	
	GList* ic;
	for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (cairo_dock_icon_get_allocated_width (pIcon) != pIcon->image.iWidth || cairo_dock_icon_get_allocated_height (pIcon) != pIcon->image.iHeight)
		{
			cairo_dock_trigger_load_icon_buffers (pIcon);
		}
	}
	
	// redraw
	cairo_dock_redraw_container (CAIRO_CONTAINER (pDesklet));
}

static void _detach_icon (GldiContainer *pContainer, Icon *pIcon)
{
	CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
	// remove icon
	pDesklet->icons = g_list_remove (pDesklet->icons, pIcon);
	
	// calculate icons
	_update_desklet_icons (pDesklet);
}

static void _insert_icon (GldiContainer *pContainer, Icon *pIcon, G_GNUC_UNUSED gboolean bAnimateIcon)
{
	CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
	// insert icon
	pDesklet->icons = g_list_insert_sorted (pDesklet->icons,
		pIcon,
		(GCompareFunc)cairo_dock_compare_icons_order);
	cairo_dock_set_icon_container (pIcon, pDesklet);
	
	// calculate icons
	_update_desklet_icons (pDesklet);
}

CairoDesklet *gldi_desklet_new (CairoDeskletAttr *attr)
{
	return (CairoDesklet*)gldi_object_new (&myDeskletObjectMgr, attr);
}

void gldi_desklet_init_internals (CairoDesklet *pDesklet)
{
	pDesklet->container.iface.animation_loop = _cairo_desklet_animation_loop;
	pDesklet->container.iface.detach_icon = _detach_icon;
	pDesklet->container.iface.insert_icon = _insert_icon;
	
	// set up its window
	GtkWidget *pWindow = pDesklet->container.pWidget;
	gtk_window_set_title (GTK_WINDOW(pWindow), "cairo-dock-desklet");
	gtk_widget_add_events( pWindow,
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK |
		GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_FOCUS_CHANGE_MASK |
		GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	gtk_container_set_border_width(GTK_CONTAINER(pWindow), 1);
	
	GtkWidget *box = NULL;
	if (gldi_container_is_wayland_backend ())
	{
		// This is a hack to hide the titlebars, since on Wayland, gtk_window_set_decorated () 
		// does the opposite of what it says, see e.g.: https://gitlab.gnome.org/GNOME/gtk/-/issues/5479
		// This is worked around by adding a dummy GtkWidget as the "titlebar" and
		// then hiding it manually later (at the end of this function)
		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_window_set_titlebar (GTK_WINDOW(pWindow), box);
	}
	
	// connect the signals to the window
	g_signal_connect (G_OBJECT (pWindow),
		"draw",
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
	gldi_container_enable_drop (CAIRO_CONTAINER (pDesklet), G_CALLBACK (_on_drag_data_received), pDesklet);
	
	gtk_widget_show_all (pWindow);
	if (box) gtk_widget_hide (box);
}


void gldi_desklet_configure (CairoDesklet *pDesklet, CairoDeskletAttr *pAttribute)
{
	//g_print ("%s (%dx%d ; (%d,%d) ; %d)\n", __func__, pAttribute->iDeskletWidth, pAttribute->iDeskletHeight, pAttribute->iDeskletPositionX, pAttribute->iDeskletPositionY, pAttribute->iVisibility);
	if (pAttribute->bDeskletUseSize && (pAttribute->iDeskletWidth != pDesklet->container.iWidth || pAttribute->iDeskletHeight != pDesklet->container.iHeight))
	{
		pDesklet->iDesiredWidth = pAttribute->iDeskletWidth;
		pDesklet->iDesiredHeight = pAttribute->iDeskletHeight;
		gtk_window_resize (GTK_WINDOW (pDesklet->container.pWidget),
			pAttribute->iDeskletWidth,
			pAttribute->iDeskletHeight);
	}
	if (! pAttribute->bDeskletUseSize)
	{
		gtk_container_set_border_width (GTK_CONTAINER (pDesklet->container.pWidget), 0);
		gtk_window_set_resizable (GTK_WINDOW(pDesklet->container.pWidget), FALSE);
	}
	
	int iAbsolutePositionX = (pAttribute->iDeskletPositionX < 0 ? gldi_desktop_get_width() + pAttribute->iDeskletPositionX : pAttribute->iDeskletPositionX);
	iAbsolutePositionX = MAX (0, MIN (gldi_desktop_get_width() - pAttribute->iDeskletWidth, iAbsolutePositionX));
	int iAbsolutePositionY = (pAttribute->iDeskletPositionY < 0 ? gldi_desktop_get_height() + pAttribute->iDeskletPositionY : pAttribute->iDeskletPositionY);
	iAbsolutePositionY = MAX (0, MIN (gldi_desktop_get_height() - pAttribute->iDeskletHeight, iAbsolutePositionY));
	//g_print (" let's place the deklet at (%d;%d)", iAbsolutePositionX, iAbsolutePositionY);
	
	if (pAttribute->bOnAllDesktops)
	{
		gtk_window_stick (GTK_WINDOW (pDesklet->container.pWidget));
		gdk_window_move (gldi_container_get_gdk_window (CAIRO_CONTAINER (pDesklet)),
			iAbsolutePositionX,
			iAbsolutePositionY);
	}
	else
	{
		gtk_window_unstick (GTK_WINDOW (pDesklet->container.pWidget));
		if (g_desktopGeometry.iNbViewportX > 0 && g_desktopGeometry.iNbViewportY > 0)
		{
			int iNumDesktop, iNumViewportX, iNumViewportY;
			iNumDesktop = pAttribute->iNumDesktop / (g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY);
			int index2 = pAttribute->iNumDesktop % (g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY);
			iNumViewportX = index2 / g_desktopGeometry.iNbViewportY;
			iNumViewportY = index2 % g_desktopGeometry.iNbViewportY;
			
			int iCurrentDesktop, iCurrentViewportX, iCurrentViewportY;
			gldi_desktop_get_current (&iCurrentDesktop, &iCurrentViewportX, &iCurrentViewportY);
			cd_debug (">>> on fixe le desklet sur le bureau (%d,%d,%d) (cur : %d,%d,%d)", iNumDesktop, iNumViewportX, iNumViewportY, iCurrentDesktop, iCurrentViewportX, iCurrentViewportY);
			
			iNumViewportX -= iCurrentViewportX;
			iNumViewportY -= iCurrentViewportY;
			cd_debug ("on le place en %d + %d", iNumViewportX * gldi_desktop_get_width(), iAbsolutePositionX);
			
			gldi_container_move (CAIRO_CONTAINER (pDesklet), iNumDesktop, iNumViewportX * gldi_desktop_get_width() + iAbsolutePositionX, iNumViewportY * gldi_desktop_get_height() + iAbsolutePositionY);
		}
	}
	pDesklet->bPositionLocked = pAttribute->bPositionLocked;
	pDesklet->bNoInput = pAttribute->bNoInput;
	pDesklet->fRotation = pAttribute->iRotation / 180. * G_PI ;
	pDesklet->fDepthRotationY = pAttribute->iDepthRotationY / 180. * G_PI ;
	pDesklet->fDepthRotationX = pAttribute->iDepthRotationX / 180. * G_PI ;
	
	g_free (pDesklet->cDecorationTheme);
	pDesklet->cDecorationTheme = pAttribute->cDecorationTheme;
	pAttribute->cDecorationTheme = NULL;
	gldi_desklet_decoration_free (pDesklet->pUserDecoration);
	pDesklet->pUserDecoration = pAttribute->pUserDecoration;
	pAttribute->pUserDecoration = NULL;
	
	gldi_desklet_set_accessibility (pDesklet, pAttribute->iVisibility, FALSE);
	
	//cd_debug ("%s (%dx%d ; %d)", __func__, pDesklet->iDesiredWidth, pDesklet->iDesiredHeight, pDesklet->iSidWriteSize);
	if (pDesklet->iDesiredWidth == 0 && pDesklet->iDesiredHeight == 0 && pDesklet->iSidWriteSize == 0)
	{
		gldi_desklet_load_desklet_decorations (pDesklet);
	}
}

void gldi_desklet_load_desklet_decorations (CairoDesklet *pDesklet)
{
	cairo_dock_unload_image_buffer (&pDesklet->backGroundImageBuffer);
	cairo_dock_unload_image_buffer (&pDesklet->foreGroundImageBuffer);
	
	CairoDeskletDecoration *pDeskletDecorations;
	//cd_debug ("%s (%s)", __func__, pDesklet->cDecorationTheme);
	if (pDesklet->cDecorationTheme == NULL || strcmp (pDesklet->cDecorationTheme, "default") == 0)
		pDeskletDecorations = cairo_dock_get_desklet_decoration (myDeskletsParam.cDeskletDecorationsName);
	else if (strcmp (pDesklet->cDecorationTheme, "personnal") == 0)
		pDeskletDecorations = pDesklet->pUserDecoration;
	else
		pDeskletDecorations = cairo_dock_get_desklet_decoration (pDesklet->cDecorationTheme);
	if (pDeskletDecorations == NULL)  // peut arriver si rendering n'a pas encore charge ses decorations.
		return ;
	//cd_debug ("pDeskletDecorations : %s (%x)", pDesklet->cDecorationTheme, pDeskletDecorations);
	
	double fZoomX = 1., fZoomY = 1.;
	pDesklet->bUseDefaultColors = FALSE;
	if (pDeskletDecorations->cBackGroundImagePath && strcmp (pDeskletDecorations->cBackGroundImagePath, "automatic") == 0)
	{
		pDesklet->bUseDefaultColors = TRUE;
	}
	else if  (pDeskletDecorations->cBackGroundImagePath != NULL && pDeskletDecorations->fBackGroundAlpha > 0)
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
}

void gldi_desklet_decoration_free (CairoDeskletDecoration *pDecoration)
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

void gldi_desklet_add_interactive_widget_with_margin (CairoDesklet *pDesklet, GtkWidget *pInteractiveWidget, int iRightMargin)
{
	g_return_if_fail (pDesklet != NULL && pInteractiveWidget != NULL);
	if (pDesklet->pInteractiveWidget != NULL || gtk_bin_get_child (GTK_BIN (pDesklet->container.pWidget)) != NULL)
	{
		cd_warning ("This desklet already has an interactive widget !");
		return;
	}
	
	//gtk_container_add (GTK_CONTAINER (pDesklet->container.pWidget), pInteractiveWidget);
	GtkWidget *pHBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (pDesklet->container.pWidget), pHBox);
	
	gtk_box_pack_start (GTK_BOX (pHBox), pInteractiveWidget, TRUE, TRUE, 0);
	pDesklet->pInteractiveWidget = pInteractiveWidget;
	
	if (iRightMargin != 0)
	{
		GtkWidget *pMarginBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		g_object_set (pMarginBox, "width-request", iRightMargin, NULL);
		gtk_box_pack_start (GTK_BOX (pHBox), pMarginBox, FALSE, FALSE, 0);  // a tester ...
	}
	
	gtk_widget_show_all (pHBox);
}

void gldi_desklet_set_margin (CairoDesklet *pDesklet, int iRightMargin)
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
				g_object_set (pMarginBox, "width-request", iRightMargin, NULL);
			}
			else  // on rajoute le widget de la marge.
			{
				GtkWidget *pMarginBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
				g_object_set (pMarginBox, "width-request", iRightMargin, NULL);
				gtk_box_pack_start (GTK_BOX (pHBox), pMarginBox, FALSE, FALSE, 0);
			}
			g_list_free (pChildList);
		}
	}
}

GtkWidget *gldi_desklet_steal_interactive_widget (CairoDesklet *pDesklet)
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



void gldi_desklet_hide (CairoDesklet *pDesklet)
{
	if (pDesklet)
	{
		pDesklet->bAllowMinimize = TRUE;
		gtk_widget_hide (pDesklet->container.pWidget);
	}
}

void gldi_desklet_show (CairoDesklet *pDesklet)
{
	if (pDesklet)
	{
		gtk_window_present(GTK_WINDOW(pDesklet->container.pWidget));
		gtk_window_move (GTK_WINDOW(pDesklet->container.pWidget), pDesklet->container.iWindowPositionX, pDesklet->container.iWindowPositionY);  // sinon le WM le place n'importe ou.
	}
}

static void _reserve_space_for_desklet (CairoDesklet *pDesklet, gboolean bReserve)
{
	cd_debug ("%s (%d)", __func__, bReserve);
	int left=0, right=0, top=0, bottom=0;
	int left_start_y=0, left_end_y=0, right_start_y=0, right_end_y=0, top_start_x=0, top_end_x=0, bottom_start_x=0, bottom_end_x=0;
	int iHeight = pDesklet->container.iHeight, iWidth = pDesklet->container.iWidth;
	int iX = pDesklet->container.iWindowPositionX, iY = pDesklet->container.iWindowPositionY;
	if (bReserve)
	{
		int iTopOffset, iBottomOffset, iRightOffset, iLeftOffset;
		iTopOffset = iY;
		iBottomOffset = gldi_desktop_get_height() - 1 - (iY + iHeight);
		iLeftOffset = iX;
		iRightOffset = gldi_desktop_get_width() - 1 - (iX + iWidth);
		
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
	gldi_container_reserve_space (CAIRO_CONTAINER(pDesklet), left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);
	pDesklet->bSpaceReserved = bReserve;
}

void gldi_desklet_set_accessibility (CairoDesklet *pDesklet, CairoDeskletVisibility iVisibility, gboolean bSaveState)
{
	cd_debug ("%s (%d)", __func__, iVisibility);
	
	//\_________________ On applique la nouvelle accessibilite.
	
	gtk_window_set_keep_below (GTK_WINDOW (pDesklet->container.pWidget), iVisibility == CAIRO_DESKLET_KEEP_BELOW);
	
	gtk_window_set_keep_above (GTK_WINDOW (pDesklet->container.pWidget), iVisibility == CAIRO_DESKLET_KEEP_ABOVE);
	
	if (iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER)
	{
		if (pDesklet->iVisibility != CAIRO_DESKLET_ON_WIDGET_LAYER)
			gldi_desktop_set_on_widget_layer (CAIRO_CONTAINER (pDesklet), TRUE);
	}
	else if (pDesklet->iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER)
	{
		gldi_desktop_set_on_widget_layer (CAIRO_CONTAINER (pDesklet), FALSE);
	}
	
	if (iVisibility == CAIRO_DESKLET_RESERVE_SPACE)
	{
		if (! pDesklet->bSpaceReserved)
			_reserve_space_for_desklet (pDesklet, TRUE);  // sinon inutile de le refaire, s'il y'a un changement de taille ce sera fait lors du configure-event.
	}
	else if (pDesklet->bSpaceReserved)
	{
		_reserve_space_for_desklet (pDesklet, FALSE);
	}
	
	//\_________________ On enregistre le nouvel etat.
	pDesklet->iVisibility = iVisibility;
	
	Icon *icon = pDesklet->pIcon;
	if (bSaveState && CAIRO_DOCK_IS_APPLET (icon))
		cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
			G_TYPE_INT, "Desklet", "accessibility", iVisibility,
			G_TYPE_INVALID);
}

void gldi_desklet_set_sticky (CairoDesklet *pDesklet, gboolean bSticky)
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
		gldi_desktop_get_current (&iCurrentDesktop, &iCurrentViewportX, &iCurrentViewportY);
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

gboolean gldi_desklet_is_sticky (CairoDesklet *pDesklet)
{
	GdkWindow *window = gldi_container_get_gdk_window (CAIRO_CONTAINER (pDesklet));
	return ((gdk_window_get_state (window)) & GDK_WINDOW_STATE_STICKY);
}

void gldi_desklet_lock_position (CairoDesklet *pDesklet, gboolean bPositionLocked)
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

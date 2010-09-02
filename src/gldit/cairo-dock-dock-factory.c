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
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-container.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-dock-factory.h"

extern gchar *g_cCurrentLaunchersPath;

extern gboolean g_bKeepAbove;

extern CairoDockGLConfig g_openglConfig;
extern gboolean g_bUseOpenGL;
#ifdef HAVE_GLITZ
extern gboolean g_bUseGlitz;
#endif


static void _cairo_dock_set_icon_size (CairoDock *pDock, Icon *icon)
{
	CairoDockIconType iType = cairo_dock_get_icon_type (icon);
	if (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))  // une applet peut definir la taille de son icone elle-meme.
	{
		if (icon->fWidth == 0)
			icon->fWidth = myIcons.tIconAuthorizedWidth[iType];
		if (icon->fHeight == 0)
			icon->fHeight = myIcons.tIconAuthorizedHeight[iType];
	}
	else
	{
		icon->fWidth = myIcons.tIconAuthorizedWidth[iType];
		icon->fHeight = myIcons.tIconAuthorizedHeight[iType];
	}
}

CairoDock *cairo_dock_new_dock (const gchar *cRendererName)
{
	//\__________________ On cree un dock.
	CairoDock *pDock = g_new0 (CairoDock, 1);
	pDock->container.iType = CAIRO_DOCK_TYPE_DOCK;
	
	pDock->iRefCount = 0;  // c'est un dock racine par defaut.
	pDock->container.fRatio = 1.;
	pDock->iAvoidingMouseIconType = -1;
	pDock->fFlatDockWidth = - myIcons.iIconGap;
	pDock->container.iMouseX = -1; // utile ?
	pDock->container.iMouseY = -1;
	pDock->fMagnitudeMax = 1.;
	pDock->fPostHideOffset = 1.;
	pDock->iInputState = CAIRO_DOCK_INPUT_AT_REST;  // le dock est cree au repos. La zone d'input sera mis en place lors du configure.
	
	pDock->container.iface.set_icon_size = _cairo_dock_set_icon_size;
	
	//\__________________ On cree la fenetre GTK.
	GtkWidget *pWindow = cairo_dock_init_container (CAIRO_CONTAINER (pDock));
	gtk_container_set_border_width (GTK_CONTAINER(pWindow), 0);
	gtk_window_set_gravity (GTK_WINDOW (pWindow), GDK_GRAVITY_STATIC);
	gtk_window_set_type_hint (GTK_WINDOW (pWindow), GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_title (GTK_WINDOW (pWindow), "cairo-dock");
	
	//\__________________ On associe un renderer.
	cairo_dock_set_renderer (pDock, cRendererName);
	
	//\__________________ On connecte les evenements a la fenetre.
	gtk_widget_add_events (pWindow,
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
		GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	
	g_signal_connect (G_OBJECT (pWindow),
		"expose-event",
		G_CALLBACK (cairo_dock_on_expose),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (cairo_dock_on_configure),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"key-release-event",
		G_CALLBACK (cairo_dock_on_key_release),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"key-press-event",
		G_CALLBACK (cairo_dock_on_key_release),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-press-event",
		G_CALLBACK (cairo_dock_on_button_press),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-release-event",
		G_CALLBACK (cairo_dock_on_button_press),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"scroll-event",
		G_CALLBACK (cairo_dock_on_scroll),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"motion-notify-event",
		G_CALLBACK (cairo_dock_on_motion_notify),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"enter-notify-event",
		G_CALLBACK (cairo_dock_on_enter_notify),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"leave-notify-event",
		G_CALLBACK (cairo_dock_on_leave_notify),
		pDock);
	cairo_dock_allow_widget_to_receive_data (pWindow,
		G_CALLBACK (cairo_dock_on_drag_data_received),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag-motion",
		G_CALLBACK (cairo_dock_on_drag_motion),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag-leave",
		G_CALLBACK (cairo_dock_on_drag_leave),
		pDock);
	/*g_signal_connect (G_OBJECT (pWindow),
		"drag-drop",
		G_CALLBACK (cairo_dock_on_drag_drop),
		pDock);*/

	gtk_window_get_size (GTK_WINDOW (pWindow), &pDock->container.iWidth, &pDock->container.iHeight);  // ca n'est que la taille initiale allouee par GTK.
	gtk_widget_show_all (pWindow);
	gdk_window_set_back_pixmap (pWindow->window, NULL, FALSE);  // vraiment plus rapide ?
	
#ifdef HAVE_GLITZ
	if (g_bUseGlitz && pDock->container.pDrawFormat != NULL)
	{
		glitz_format_t templ;
		GdkDisplay	   *gdkdisplay;
		Display	   *XDisplay;
		Window	   xid;

		gdkdisplay = gdk_display_get_default ();
		XDisplay   = gdk_x11_display_get_xdisplay (gdkdisplay);
		xid = gdk_x11_drawable_get_xid (GDK_DRAWABLE (pWindow->window));
		pDock->container.pGlitzDrawable = glitz_glx_create_drawable_for_window (XDisplay,
			0,
			pDock->container.pDrawFormat,
			xid,
			pDock->container.iWidth,
			pDock->container.iHeight);
		if (! pDock->container.pGlitzDrawable)
		{
			cd_warning ("failed to create glitz drawable");
		}
		else
		{
			templ.color        = pDock->container.pDrawFormat->color;
			templ.color.fourcc = GLITZ_FOURCC_RGB;
			pDock->container.pGlitzFormat = glitz_find_format (pDock->container.pGlitzDrawable,
				GLITZ_FORMAT_RED_SIZE_MASK   |
				GLITZ_FORMAT_GREEN_SIZE_MASK |
				GLITZ_FORMAT_BLUE_SIZE_MASK  |
				GLITZ_FORMAT_ALPHA_SIZE_MASK |
				GLITZ_FORMAT_FOURCC_MASK,
				&templ,
				0);
			if (! pDock->container.pGlitzFormat)
			{
				cd_warning ("couldn't find glitz surface format");
			}
		}
	}
#endif
	
	return pDock;
}

void cairo_dock_free_dock (CairoDock *pDock)
{
	if (pDock->iSidUnhideDelayed != 0)
		g_source_remove (pDock->iSidUnhideDelayed);
	if (pDock->iSidHideBack != 0)
		g_source_remove (pDock->iSidHideBack);
	if (pDock->iSidMoveResize != 0)
		g_source_remove (pDock->iSidMoveResize);
	if (pDock->iSidLeaveDemand != 0)
		g_source_remove (pDock->iSidLeaveDemand);
	if (pDock->iSidUpdateWMIcons != 0)
		g_source_remove (pDock->iSidUpdateWMIcons);
	if (pDock->iSidLoadBg != 0)
		g_source_remove (pDock->iSidLoadBg);
	if (pDock->iSidDestroyEmptyDock != 0)
		g_source_remove (pDock->iSidDestroyEmptyDock);
	if (pDock->iSidTestMouseOutside != 0)
		g_source_remove (pDock->iSidTestMouseOutside);
	cairo_dock_notify (CAIRO_DOCK_STOP_DOCK, pDock);
	
	g_list_foreach (pDock->icons, (GFunc) cairo_dock_free_icon, NULL);
	g_list_free (pDock->icons);
	pDock->icons = NULL;
	
	if (pDock->pShapeBitmap != NULL)
		g_object_unref ((gpointer) pDock->pShapeBitmap);
	
	if (pDock->pRenderer != NULL && pDock->pRenderer->free_data != NULL)
	{
		pDock->pRenderer->free_data (pDock);
	}
	
	g_free (pDock->cRendererName);
	
	g_free (pDock->cBgImagePath);
	cairo_dock_unload_image_buffer (&pDock->backgroundBuffer);
	
	if (pDock->iFboId != 0)
		glDeleteFramebuffersEXT (1, &pDock->iFboId);
	if (pDock->iRedirectedTexture != 0)
		_cairo_dock_delete_texture (pDock->iRedirectedTexture);
	
	cairo_dock_finish_container (CAIRO_CONTAINER (pDock));
	
	g_free (pDock);
}

void cairo_dock_make_sub_dock (CairoDock *pDock, CairoDock *pParentDock)
{
	CairoDockPositionType iScreenBorder = ((! pDock->container.bIsHorizontal) << 1) | (! pDock->container.bDirectionUp);
	cd_debug ("sub-dock's position : %d/%d", pDock->container.bIsHorizontal, pDock->container.bDirectionUp);
	pDock->container.bIsHorizontal = pParentDock->container.bIsHorizontal;
	pDock->container.bDirectionUp = pParentDock->container.bDirectionUp;
	if (iScreenBorder != (((! pDock->container.bIsHorizontal) << 1) | (! pDock->container.bDirectionUp)))
	{
		cd_debug ("changement de position -> %d/%d", pDock->container.bIsHorizontal, pDock->container.bDirectionUp);
		cairo_dock_reload_reflects_in_dock (pDock);
	}
	pDock->iScreenOffsetX = pParentDock->iScreenOffsetX;
	pDock->iScreenOffsetY = pParentDock->iScreenOffsetY;
	if (g_bKeepAbove)
		gtk_window_set_keep_above (GTK_WINDOW (pDock->container.pWidget), FALSE);
	gtk_window_set_title (GTK_WINDOW (pDock->container.pWidget), "cairo-dock-sub");
	
	pDock->bAutoHide = FALSE;
	double fPrevRatio = pDock->container.fRatio;
	pDock->container.fRatio = MIN (pDock->container.fRatio, myViews.fSubDockSizeRatio);
	
	Icon *icon;
	GList *ic;
	pDock->fFlatDockWidth = - myIcons.iIconGap;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fWidth *= pDock->container.fRatio / fPrevRatio;
		icon->fHeight *= pDock->container.fRatio / fPrevRatio;
		pDock->fFlatDockWidth += icon->fWidth + myIcons.iIconGap;
	}
	pDock->iMaxIconHeight *= pDock->container.fRatio / fPrevRatio;

	cairo_dock_set_default_renderer (pDock);
	
	if (pDock->pShapeBitmap != NULL)
	{
		g_object_unref ((gpointer) pDock->pShapeBitmap);
		pDock->pShapeBitmap = NULL;
		if (pDock->iInputState != CAIRO_DOCK_INPUT_ACTIVE)
		{
			cairo_dock_set_input_shape_active (pDock);
			pDock->iInputState = CAIRO_DOCK_INPUT_ACTIVE;
		}
	}
	gtk_widget_hide (pDock->container.pWidget);
	cairo_dock_update_dock_size (pDock);
}


void cairo_dock_insert_icon_in_dock_full (Icon *icon, CairoDock *pDock, gboolean bUpdateSize, gboolean bAnimated, gboolean bInsertSeparator, GCompareFunc pCompareFunc)
{
	g_return_if_fail (icon != NULL);
	if (g_list_find (pDock->icons, icon) != NULL)  // elle est deja dans ce dock.
		return ;

	int iPreviousMinWidth = pDock->fFlatDockWidth;
	int iPreviousMaxIconHeight = pDock->iMaxIconHeight;

	//\______________ On regarde si on doit inserer un separateur.
	gboolean bSeparatorNeeded = FALSE;
	if (bInsertSeparator && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		Icon *pSameTypeIcon = cairo_dock_get_first_icon_of_order (pDock->icons, icon->iType);
		if (pSameTypeIcon == NULL && pDock->icons != NULL)
		{
			bSeparatorNeeded = TRUE;
			//g_print ("separateur necessaire\n");
		}
	}

	//\______________ On insere l'icone a sa place dans la liste.
	if (icon->fOrder == CAIRO_DOCK_LAST_ORDER)
	{
		Icon *pLastIcon = cairo_dock_get_last_icon_of_order (pDock->icons, icon->iType);
		if (pLastIcon != NULL)
			icon->fOrder = pLastIcon->fOrder + 1;
		else
			icon->fOrder = 1;
	}
	
	if (pCompareFunc == NULL)
		pCompareFunc = (GCompareFunc) cairo_dock_compare_icons_order;
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon,
		pCompareFunc);
	icon->pContainerForLoad = CAIRO_CONTAINER (pDock);
	
	if (icon->fWidth == 0)
	{
		cairo_dock_set_icon_size (CAIRO_CONTAINER (pDock), icon);
	}
	
	icon->fWidth *= pDock->container.fRatio;
	icon->fHeight *= pDock->container.fRatio;

	pDock->fFlatDockWidth += myIcons.iIconGap + icon->fWidth;
	if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);

	//\______________ On insere un separateur si necessaire.
	if (bSeparatorNeeded)
	{
		int iOrder = cairo_dock_get_icon_order (icon);
		if (iOrder + 1 < CAIRO_DOCK_NB_TYPES)
		{
			Icon *pNextIcon = cairo_dock_get_next_icon (pDock->icons, icon);
			if (pNextIcon != NULL && ((cairo_dock_get_icon_order (pNextIcon) - cairo_dock_get_icon_order (icon)) % 2 == 0) && (cairo_dock_get_icon_order (pNextIcon) != cairo_dock_get_icon_order (icon)))
			{
				int iSeparatorType = iOrder + 1;
				cd_debug ("+ insertion de %s avant %s -> iSeparatorType : %d\n", icon->cName, pNextIcon->cName, iSeparatorType);
				cairo_dock_insert_automatic_separator_in_dock (iSeparatorType, pNextIcon->cParentDockName, pDock);
			}
		}
		if (iOrder > 1)
		{
			Icon *pPrevIcon = cairo_dock_get_previous_icon (pDock->icons, icon);
			if (pPrevIcon != NULL && ((cairo_dock_get_icon_order (pPrevIcon) - cairo_dock_get_icon_order (icon)) % 2 == 0) && (cairo_dock_get_icon_order (pPrevIcon) != cairo_dock_get_icon_order (icon)))
			{
				int iSeparatorType = iOrder - 1;
				cd_debug ("+ insertion de %s (%d) apres %s -> iSeparatorType : %d\n", icon->cName, icon->pModuleInstance != NULL, pPrevIcon->cName, iSeparatorType);
				cairo_dock_insert_automatic_separator_in_dock (iSeparatorType, pPrevIcon->cParentDockName, pDock);
			}
		}
	}
	
	//\______________ On effectue les actions demandees.
	if (bAnimated)
	{
		if (cairo_dock_animation_will_be_visible (pDock))
			icon->fInsertRemoveFactor = - 0.95;
		else
			icon->fInsertRemoveFactor = - 0.05;
		cairo_dock_notify (CAIRO_DOCK_INSERT_ICON, icon, pDock);
	}
	if (bUpdateSize)
		cairo_dock_update_dock_size (pDock);
	
	if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_RESERVE && bUpdateSize && ! pDock->bAutoHide && (pDock->fFlatDockWidth != iPreviousMinWidth || pDock->iMaxIconHeight != iPreviousMaxIconHeight))
		cairo_dock_reserve_space_for_dock (pDock, TRUE);
	
	if (pDock->iRefCount != 0 && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))  // on prevoit le redessin de l'icone pointant sur le sous-dock.
	{
		cairo_dock_trigger_redraw_subdock_content (pDock);
	}
	
	if (CAIRO_DOCK_IS_STORED_LAUNCHER (icon) || CAIRO_DOCK_IS_USER_SEPARATOR (icon) || CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))
		cairo_dock_trigger_refresh_launcher_gui ();
}


static gboolean _destroy_empty_dock (CairoDock *pDock)
{
	const gchar *cDockName = cairo_dock_search_dock_name (pDock);  // safe meme si le dock n'existe plus.
	g_return_val_if_fail (cDockName != NULL, FALSE);  // si NULL, cela signifie que le dock n'existe plus, donc on n'y touche pas. Ce cas ne devrait jamais arriver, c'est de la pure parano.
	
	if (pDock->bIconIsFlyingAway)
		return TRUE;
	pDock->iSidDestroyEmptyDock = 0;
	if (pDock->icons == NULL && pDock->iRefCount == 0 && ! pDock->bIsMainDock)  // le dock est toujours a detruire.
	{
		g_print ("The dock '%s' is empty. No icon, no dock.\n", cDockName);
		cairo_dock_destroy_dock (pDock, cDockName);
	}
	return FALSE;
}
gboolean cairo_dock_detach_icon_from_dock (Icon *icon, CairoDock *pDock, gboolean bCheckUnusedSeparator)
{
	if (pDock == NULL)
		return FALSE;
	
	//\___________________ On trouve l'icone et ses 2 voisins.
	GList *prev_ic = NULL, *ic, *next_ic;
	Icon *pPrevIcon = NULL, *pNextIcon = NULL;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		if (ic -> data == icon)
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
	if (ic == NULL)  // elle est deja detachee.
		return FALSE;
	
	cd_message ("%s (%s)", __func__, icon->cName);
	g_free (icon->cParentDockName);
	icon->cParentDockName = NULL;
	icon->pContainerForLoad = NULL;
	
	//\___________________ On stoppe ses animations.
	cairo_dock_stop_icon_animation (icon);
	
	//\___________________ On desactive sa miniature.
	if (icon->Xid != 0)
	{
		//cd_debug ("on desactive la miniature de %s (Xid : %lx)", icon->cName, icon->Xid);
		cairo_dock_set_xicon_geometry (icon->Xid, 0, 0, 0, 0);
	}
	
	//\___________________ On l'enleve de la liste.
	if (pDock->pFirstDrawnElement != NULL && pDock->pFirstDrawnElement->data == icon)
	{
		if (pDock->pFirstDrawnElement->next != NULL)
			pDock->pFirstDrawnElement = pDock->pFirstDrawnElement->next;
		else
		{
			if (pDock->icons != NULL && pDock->icons->next != NULL)  // la liste n'a pas qu'un seul element.
				pDock->pFirstDrawnElement = pDock->icons;
			else
				pDock->pFirstDrawnElement = NULL;
		}
	}
	pDock->icons = g_list_delete_link (pDock->icons, ic);
	ic = NULL;
	pDock->fFlatDockWidth -= icon->fWidth + myIcons.iIconGap;
	
	//\___________________ On enleve le separateur si c'est la derniere icone de son type.
	if (bCheckUnusedSeparator && ! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
	{
		if ((pPrevIcon == NULL || CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pPrevIcon)) && CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pNextIcon))
		{
			pDock->icons = g_list_delete_link (pDock->icons, next_ic);
			next_ic = NULL;
			pDock->fFlatDockWidth -= pNextIcon->fWidth + myIcons.iIconGap;
			cairo_dock_free_icon (pNextIcon);
			pNextIcon = NULL;
		}
		else if (pNextIcon == NULL && CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pPrevIcon))
		{
			pDock->icons = g_list_delete_link (pDock->icons, prev_ic);
			prev_ic = NULL;
			pDock->fFlatDockWidth -= pPrevIcon->fWidth + myIcons.iIconGap;
			cairo_dock_free_icon (pPrevIcon);
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
			pDock->iSidDestroyEmptyDock = g_idle_add ((GSourceFunc) _destroy_empty_dock, pDock);  // on ne passe pas le nom du dock en parametre, car le dock peut se faire renommer (improbable, mais bon).
	}
	
	//\___________________ On prevoit le rafraichissement de la fenetre de config des lanceurs.
	if (CAIRO_DOCK_IS_STORED_LAUNCHER (icon) || CAIRO_DOCK_IS_USER_SEPARATOR (icon) || CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon))
		cairo_dock_trigger_refresh_launcher_gui ();
	return TRUE;
}
void cairo_dock_remove_icon_from_dock_full (CairoDock *pDock, Icon *icon, gboolean bCheckUnusedSeparator)
{
	g_return_if_fail (icon != NULL);
	
	//\___________________ On detache l'icone du dock.
	if (pDock != NULL)
		cairo_dock_detach_icon_from_dock (icon, pDock, bCheckUnusedSeparator);  // on le fait maintenant, pour que l'icone ait son type correct, et ne soit pas confondue avec un separateur
	
	//\___________________ On supprime l'icone du theme courant.
	if (icon->iface.on_delete)
	{
		gboolean r = icon->iface.on_delete (icon);
		if (r)
			cairo_dock_mark_current_theme_as_modified (TRUE);
	}
}


void cairo_dock_remove_automatic_separators (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon;
	GList *ic = pDock->icons, *next_ic;
	while (ic != NULL)
	{
		icon = ic->data;
		next_ic = ic->next;  // si l'icone se fait enlever, on perdrait le fil.
		if (CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
		{
			//g_print ("un separateur en moins (apres %s)\n", ((Icon*)ic->data)->cName);
			cairo_dock_remove_one_icon_from_dock (pDock, icon);
			cairo_dock_free_icon (icon);
		}
		ic = next_ic;
	}
}

void cairo_dock_insert_separators_in_dock (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon, *next_icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
		{
			if (ic->next != NULL)
			{
				next_icon = ic->next->data;
				if (! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (next_icon) && abs (cairo_dock_get_icon_order (icon) - cairo_dock_get_icon_order (next_icon)) > 1)  // icon->iType != next_icon->iType
				{
					int iSeparatorType = myIcons.tIconTypeOrder[next_icon->iType] - 1;
					cd_debug ("+ un separateur entre %s et %s, dans le groupe %d (=%d)\n", icon->cName, next_icon->cName, iSeparatorType, myIcons.tIconTypeOrder[iSeparatorType]);
					cairo_dock_insert_automatic_separator_in_dock (iSeparatorType, next_icon->cParentDockName, pDock);
				}
			}
		}
	}
}


Icon *cairo_dock_add_new_launcher_by_uri_or_type (const gchar *cExternDesktopFileURI, CairoDockDesktopFileType iType, CairoDock *pReceivingDock, double fOrder, CairoDockIconType iGroup)
{
	//\_________________ On ajoute un fichier desktop dans le repertoire des lanceurs du theme courant.
	gchar *cPath = NULL;
	if (cExternDesktopFileURI && strncmp (cExternDesktopFileURI, "file://", 7) == 0)
	{
		cPath = g_filename_from_uri (cExternDesktopFileURI, NULL, NULL);
	}
	GError *erreur = NULL;
	const gchar *cDockName = cairo_dock_search_dock_name (pReceivingDock);
	if (fOrder == CAIRO_DOCK_LAST_ORDER && pReceivingDock != NULL)
	{
		Icon *pLastIcon = cairo_dock_get_last_launcher (pReceivingDock->icons);
		if (pLastIcon != NULL)
			fOrder = pLastIcon->fOrder + 1;
		else
			fOrder = 1;
	}
	gchar *cNewDesktopFileName;
	if (cExternDesktopFileURI != NULL)
		cNewDesktopFileName = cairo_dock_add_desktop_file_from_uri (cPath ? cPath : cExternDesktopFileURI, cDockName, fOrder, iGroup, &erreur);
	else
		cNewDesktopFileName = cairo_dock_add_desktop_file_from_type (iType, cDockName, fOrder, iGroup, &erreur);
	g_free (cPath);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return NULL;
	}
	
	//\_________________ On verifie ici l'unicite du sous-dock.
	if (iType == CAIRO_DOCK_DESKTOP_FILE_FOR_CONTAINER && cExternDesktopFileURI == NULL)
	{
		
	}
	
	//\_________________ On charge ce nouveau lanceur.
	Icon *pNewIcon = NULL;
	if (cNewDesktopFileName != NULL)
	{
		cairo_dock_mark_current_theme_as_modified (TRUE);

		pNewIcon = cairo_dock_create_icon_from_desktop_file (cNewDesktopFileName);
		g_free (cNewDesktopFileName);

		if (pNewIcon != NULL)
		{
			cairo_dock_insert_icon_in_dock (pNewIcon, pReceivingDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);

			/**if (CAIRO_DOCK_IS_URI_LAUNCHER (pNewIcon))
			{
				cairo_dock_fm_add_monitor (pNewIcon);
			}*/
			
			if (pNewIcon->pSubDock != NULL)
				cairo_dock_trigger_redraw_subdock_content (pNewIcon->pSubDock);
			
			cairo_dock_launch_animation (CAIRO_CONTAINER (pReceivingDock));
		}
	}
	return pNewIcon;
}


void cairo_dock_remove_icons_from_dock (CairoDock *pDock, CairoDock *pReceivingDock, const gchar *cReceivingDockName)
{
	GList *pIconsList = pDock->icons;
	pDock->icons = NULL;
	Icon *icon;
	GList *ic;
	gchar *cDesktopFilePath;
	for (ic = pIconsList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		if (icon->pSubDock != NULL)
		{
			cairo_dock_remove_icons_from_dock (icon->pSubDock, pReceivingDock, cReceivingDockName);
		}

		if (pReceivingDock == NULL || cReceivingDockName == NULL)  // alors on les jete.
		{
			if (icon->iface.on_delete)
				icon->iface.on_delete (icon);  // efface le .desktop / ecrit les modules actifs.
			
			if (CAIRO_DOCK_IS_APPLET (icon))
			{
				cairo_dock_update_icon_s_container_name (icon, CAIRO_DOCK_MAIN_DOCK_NAME);  // on decide de les remettre dans le dock principal la prochaine fois qu'ils seront actives.
			}
			cairo_dock_free_icon (icon);  // de-instancie l'applet.
		}
		else  // on les re-attribue au dock receveur.
		{
			cairo_dock_update_icon_s_container_name (icon, cReceivingDockName);

			icon->fWidth /= pDock->container.fRatio;
			icon->fHeight /= pDock->container.fRatio;
			
			cd_debug (" on re-attribue %s au dock %s", icon->cName, icon->cParentDockName);
			cairo_dock_insert_icon_in_dock (icon, pReceivingDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
			
			if (CAIRO_DOCK_IS_APPLET (icon))
			{
				icon->pModuleInstance->pContainer = CAIRO_CONTAINER (pReceivingDock);  // astuce pour ne pas avoir a recharger le fichier de conf ^_^
				icon->pModuleInstance->pDock = pReceivingDock;
				cairo_dock_reload_module_instance (icon->pModuleInstance, FALSE);
			}
			cairo_dock_launch_animation (CAIRO_CONTAINER (pReceivingDock));
		}
	}
	if (pReceivingDock != NULL)
		cairo_dock_update_dock_size (pReceivingDock);

	g_list_free (pIconsList);
}


void cairo_dock_create_redirect_texture_for_dock (CairoDock *pDock)
{
	if (! g_openglConfig.bFboAvailable)
		return ;
	if (pDock->iRedirectedTexture != 0)
		return ;
	
	pDock->iRedirectedTexture = cairo_dock_load_texture_from_raw_data (NULL,
		(pDock->container.bIsHorizontal ? pDock->container.iWidth : pDock->container.iHeight),
		(pDock->container.bIsHorizontal ? pDock->container.iHeight : pDock->container.iWidth));
	
	glGenFramebuffersEXT(1, &pDock->iFboId);
}

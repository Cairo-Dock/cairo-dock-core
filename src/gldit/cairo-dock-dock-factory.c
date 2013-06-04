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

#include "cairo-dock-applications-manager.h"  // myTaskbarParam.bHideVisibleApplis
#include "cairo-dock-module-instance-manager.h"  // gldi_module_instance_reload
#include "cairo-dock-callbacks.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-separator-manager.h"  // gldi_auto_separator_icon_new
#include "cairo-dock-user-icon-manager.h"  // gldi_user_icon_new
#include "cairo-dock-backends-manager.h"  // myBackendsParam.fSubDockSizeRatio
#include "cairo-dock-windows-manager.h"  // gldi_window_set_thumbnail_area
#include "cairo-dock-log.h"
#include "cairo-dock-application-facility.h"  // cairo_dock_detach_appli
#include "cairo-dock-dialog-manager.h"  // gldi_dialogs_replace_all
#include "cairo-dock-launcher-manager.h"  // gldi_launcher_add_conf_file
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"   // cairo_dock_get_class_subdock
#include "cairo-dock-animations.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-draw-opengl.h"  // for the redirected texture
#include "cairo-dock-data-renderer.h"  // cairo_dock_reload_data_renderer_on_icon
#include "cairo-dock-dock-factory.h"

extern gchar *g_cCurrentLaunchersPath;

extern CairoDockGLConfig g_openglConfig;
extern CairoDockHidingEffect *g_pHidingBackend;
extern gboolean g_bUseOpenGL;


CairoDock *gldi_dock_new (const gchar *cDockName)
{
	CairoDockAttr attr;
	memset (&attr, 0, sizeof (CairoDockAttr));
	attr.cDockName = cDockName;
	return (CairoDock*)gldi_object_new (GLDI_MANAGER(&myDocksMgr), &attr);
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
	return (CairoDock*)gldi_object_new (GLDI_MANAGER(&myDocksMgr), &attr);
}

void cairo_dock_insert_icon_in_dock_full (Icon *icon, CairoDock *pDock, gboolean bAnimated, gboolean bInsertSeparator, GCompareFunc pCompareFunc)
{
	//g_print ("%s (%s)\n", __func__, icon->cName);
	g_return_if_fail (icon != NULL);
	if (g_list_find (pDock->icons, icon) != NULL)  // elle est deja dans ce dock.
		return ;
	if (icon->pContainer != NULL)
	{
		cd_warning ("This icon (%s) is already inside a container !", icon->cName);
	}

	// maybe the icon was detached before and then cParentDockName is NULL
	if (icon->cParentDockName == NULL)
		icon->cParentDockName = g_strdup (gldi_dock_get_name (pDock));

	//\______________ check if a separator is needed (ie, if the group of the new icon (not its order) is new).
	gboolean bSeparatorNeeded = FALSE;
	if (bInsertSeparator && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		Icon *pSameTypeIcon = cairo_dock_get_first_icon_of_group (pDock->icons, icon->iGroup);
		if (pSameTypeIcon == NULL && pDock->icons != NULL)
		{
			bSeparatorNeeded = TRUE;
			//g_print ("separateur necessaire\n");
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
	
	if (pCompareFunc == NULL)
		pCompareFunc = (GCompareFunc) cairo_dock_compare_icons_order;
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon,
		pCompareFunc);
	cairo_dock_set_icon_container (icon, pDock);
	
	//\______________ set the icon size, now that it's inside a container.
	int wi = icon->image.iWidth, hi = icon->image.iHeight;
	cairo_dock_set_icon_size_in_dock (pDock, icon);
	
	if (wi != cairo_dock_icon_get_allocated_width (icon) || hi != cairo_dock_icon_get_allocated_height (icon)  // if size has changed, reload the buffers
	|| (! icon->image.pSurface && ! icon->image.iTexture))  // might happen, for instance if the icon is a launcher pinned on a desktop and was detached before being loaded.
		cairo_dock_trigger_load_icon_buffers (icon);
	
	pDock->fFlatDockWidth += myIconsParam.iIconGap + icon->fWidth;
	if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);

	//\______________ On insere un separateur si necessaire.
	if (bSeparatorNeeded)
	{
		// insert a separator after if needed
		Icon *pNextIcon = cairo_dock_get_next_icon (pDock->icons, icon);
		if (pNextIcon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pNextIcon))
		{
			Icon *pSeparatorIcon = gldi_auto_separator_icon_new (icon, pNextIcon);
			cairo_dock_insert_icon_in_dock (pSeparatorIcon, pDock, ! CAIRO_DOCK_ANIMATE_ICON);
		}
		
		// insert a separator before if needed
		Icon *pPrevIcon = cairo_dock_get_previous_icon (pDock->icons, icon);
		if (pPrevIcon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pPrevIcon))
		{
			Icon *pSeparatorIcon = gldi_auto_separator_icon_new (pPrevIcon, icon);
			cairo_dock_insert_icon_in_dock (pSeparatorIcon, pDock, ! CAIRO_DOCK_ANIMATE_ICON);
		}
	}
	
	//\______________ On effectue les actions demandees.
	if (bAnimated)
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
		cairo_dock_synchronize_one_sub_dock_orientation (icon->pSubDock, pDock, FALSE);
	
	//\______________ Notify everybody.
	gldi_object_notify (pDock, NOTIFICATION_INSERT_ICON, icon, pDock);
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
gboolean cairo_dock_detach_icon_from_dock_full (Icon *icon, CairoDock *pDock, gboolean bCheckUnusedSeparator)
{
	if (pDock == NULL)
		return FALSE;
	if (icon->pContainer == NULL)
	{
		cd_warning ("This icon (%s) is already not inside a container !", icon->cName);  // not a big deal, just print that for debug.
	}
	
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
	if (bCheckUnusedSeparator && ! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
	{
		if ((pPrevIcon == NULL || CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pPrevIcon)) && CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pNextIcon))
		{
			pDock->icons = g_list_delete_link (pDock->icons, next_ic);
			next_ic = NULL;
			pDock->fFlatDockWidth -= pNextIcon->fWidth + myIconsParam.iIconGap;
			gldi_object_unref (GLDI_OBJECT (pNextIcon));
			pNextIcon = NULL;
		}
		if ((pNextIcon == NULL || CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pNextIcon)) && CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pPrevIcon))
		{
			pDock->icons = g_list_delete_link (pDock->icons, prev_ic);
			prev_ic = NULL;
			pDock->fFlatDockWidth -= pPrevIcon->fWidth + myIconsParam.iIconGap;
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
			pDock->iSidDestroyEmptyDock = g_idle_add ((GSourceFunc) _destroy_empty_dock, pDock);  // on ne passe pas le nom du dock en parametre, car le dock peut se faire renommer (improbable, mais bon).
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
	cairo_dock_set_icon_container (icon, NULL);
	
	return TRUE;
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
			cairo_dock_detach_icon_from_dock (icon, pDock);
			gldi_object_delete (GLDI_OBJECT(icon));
		}
		ic = next_ic;
	}
}

void cairo_dock_insert_automatic_separators_in_dock (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon, *pNextIcon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (! GLDI_OBJECT_IS_SEPARATOR_ICON (icon))
		{
			if (ic->next != NULL)
			{
				pNextIcon = ic->next->data;
				if (! GLDI_OBJECT_IS_SEPARATOR_ICON (pNextIcon) && icon->iGroup != pNextIcon->iGroup)
				{
					Icon *pSeparatorIcon = gldi_auto_separator_icon_new (icon, pNextIcon);
					cairo_dock_insert_icon_in_dock (pSeparatorIcon, pDock, ! CAIRO_DOCK_ANIMATE_ICON);
				}
			}
		}
	}
}


void cairo_dock_remove_icons_from_dock (CairoDock *pDock, CairoDock *pReceivingDock)
{
	GList *pIconsList = pDock->icons;
	pDock->icons = NULL;
	Icon *icon;
	GList *ic;
	for (ic = pIconsList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		if (icon->pSubDock != NULL)
		{
			cairo_dock_remove_icons_from_dock (icon->pSubDock, pReceivingDock);
		}

		if (pReceivingDock == NULL)  // alors on les jete.
		{
			if (CAIRO_DOCK_IS_APPLET (icon))
			{
				gldi_theme_icon_write_container_name_in_conf_file (icon, CAIRO_DOCK_MAIN_DOCK_NAME);  // on decide de les remettre dans le dock principal la prochaine fois qu'ils seront actives.
			}
			gldi_object_delete (GLDI_OBJECT(icon));
		}
		else  // on les re-attribue au dock receveur.
		{
			gldi_theme_icon_write_container_name_in_conf_file (icon, pReceivingDock->cDockName);
			
			/**icon->fWidth /= pDock->container.fRatio;  // optimization: no need to detach the icon, we just steal all of them.
			icon->fHeight /= pDock->container.fRatio;*/
			cd_debug (" on re-attribue %s au dock %s", icon->cName, icon->cParentDockName);
			cairo_dock_insert_icon_in_dock (icon, pReceivingDock, CAIRO_DOCK_ANIMATE_ICON);
			
			if (CAIRO_DOCK_IS_APPLET (icon))
			{
				icon->pModuleInstance->pContainer = CAIRO_CONTAINER (pReceivingDock);  // astuce pour ne pas avoir a recharger le fichier de conf ^_^
				icon->pModuleInstance->pDock = pReceivingDock;
				gldi_module_instance_reload (icon->pModuleInstance, FALSE);
			}
			else if (cairo_dock_get_icon_data_renderer (icon) != NULL)
				cairo_dock_reload_data_renderer_on_icon (icon, CAIRO_CONTAINER (pReceivingDock));
		}
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
			gldi_module_instance_reload (icon->pModuleInstance, FALSE);
		}
		else
		{
			if (bUpdateIconSize)
			{
				cairo_dock_icon_set_requested_size (icon, 0, 0);
				cairo_dock_set_icon_size_in_dock (pDock, icon);
			}
			cairo_dock_trigger_load_icon_buffers (icon);
			
			if (bUpdateIconSize && cairo_dock_get_icon_data_renderer (icon) != NULL)
				cairo_dock_reload_data_renderer_on_icon (icon, CAIRO_CONTAINER (pDock));
		}
		
		if (bRecursive && icon->pSubDock != NULL)  // we handle the sub-dock for applets too, so that they don't need to care.
		{
			///cairo_dock_synchronize_one_sub_dock_orientation (icon->pSubDock, pDock, FALSE);  /// should probably not be here.
			if (bUpdateIconSize)
				icon->pSubDock->iIconSize = pDock->iIconSize;
			cairo_dock_reload_buffers_in_dock (icon->pSubDock, bRecursive, bUpdateIconSize);
		}
	}
	
	if (bUpdateIconSize)
	{
		cairo_dock_update_dock_size (pDock);
		cairo_dock_calculate_dock_icons (pDock);
		
		cairo_dock_move_resize_dock (pDock);
		if (pDock->iVisibility == CAIRO_DOCK_VISI_RESERVE)  // la position/taille a change, il faut refaire la reservation.
			cairo_dock_reserve_space_for_dock (pDock, TRUE);
		gtk_widget_queue_draw (pDock->container.pWidget);
	}
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

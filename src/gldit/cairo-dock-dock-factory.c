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

#include "cairo-dock-icon-facility.h"
#include "cairo-dock-separator-manager.h"  // gldi_auto_separator_icon_new
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-module-instance-manager.h"
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
			gldi_icon_detach (icon);
			gldi_object_unref (GLDI_OBJECT(icon));
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
					gldi_icon_insert_in_container (pSeparatorIcon, CAIRO_CONTAINER(pDock), ! CAIRO_DOCK_ANIMATE_ICON);
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

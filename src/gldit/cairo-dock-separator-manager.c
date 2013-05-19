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
#include <cairo.h>
#include <stdlib.h>


#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-separator-manager.h"


cairo_surface_t *cairo_dock_create_separator_surface (int iWidth, int iHeight)
{
	cairo_surface_t *pNewSurface = NULL;
	if (myIconsParam.cSeparatorImage == NULL)
	{
		pNewSurface = cairo_dock_create_blank_surface (
			iWidth,
			iHeight);
	}
	else
	{
		gchar *cImagePath = cairo_dock_search_image_s_path (myIconsParam.cSeparatorImage);
		pNewSurface = cairo_dock_create_surface_from_image_simple (cImagePath,
			iWidth,
			iHeight);
		
		g_free (cImagePath);
	}
	
	return pNewSurface;
}


static void _load_separator (Icon *icon)
{
	int iWidth = cairo_dock_icon_get_allocated_width (icon);
	int iHeight = cairo_dock_icon_get_allocated_height (icon);
	
	cairo_surface_t *pSurface = cairo_dock_create_separator_surface (
		iWidth,
		iHeight);
	cairo_dock_load_image_buffer_from_surface (&icon->image, pSurface, iWidth, iHeight);
}

Icon *cairo_dock_create_separator_icon (int iSeparatorType)
{
	//g_print ("%s ()\n", __func__);
	//\____________ On cree l'icone.
	Icon *icon = cairo_dock_new_separator_icon (iSeparatorType);
	icon->iface.load_image = _load_separator;
	
	return icon;
}


void cairo_dock_insert_automatic_separator_in_dock (int iSeparatorType, double fOrder, const gchar *cParentDockName, CairoDock *pDock)
{
	Icon *pSeparatorIcon = cairo_dock_create_separator_icon (iSeparatorType);
	if (pSeparatorIcon != NULL)
	{
		pSeparatorIcon->fOrder = fOrder;
		pSeparatorIcon->cParentDockName = g_strdup (cParentDockName);
		
		cairo_dock_insert_icon_in_dock_full (pSeparatorIcon, pDock, ! CAIRO_DOCK_ANIMATE_ICON, ! CAIRO_DOCK_INSERT_SEPARATOR, NULL);
	}
}


/// IN PROGRESS...
void cairo_dock_insert_automatic_separators_amongst_icons (GList *pIconsList)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon, *pNextIcon;
	GList *ic, *next_ic;
	for (ic = pIconsList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		next_ic = ic->next;
		if (! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
		{
			if (next_ic != NULL)
			{
				pNextIcon = next_ic->data;
				if (! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pNextIcon) && icon->iGroup != pNextIcon->iGroup)
				{
					int iSeparatorGroup = cairo_dock_get_icon_order (icon) +
						(cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pNextIcon) ? 0 : 1);  // for separators, group = order.
					double fOrder = (cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pNextIcon) ? (icon->fOrder + pNextIcon->fOrder) / 2 : 0);
					/**Icon *pSeparatorIcon = cairo_dock_create_separator_icon (iSeparatorType);
					g_list_insert_before (pIconsList, next_ic, 
                                                         GList *sibling,
                                                         gpointer data);
					pIconsList = g_list_insert_sorted (pDock->icons,
						icon,
						cairo_dock_compare_icons_order);*/
				}
			}
		}
	}
}

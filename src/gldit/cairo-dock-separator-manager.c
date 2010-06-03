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

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "cairo-dock-icons.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-load.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-icon-loader.h"
#include "cairo-dock-separator-manager.h"


cairo_surface_t *cairo_dock_create_separator_surface (int iWidth, int iHeight)
{
	cairo_surface_t *pNewSurface = NULL;
	if (myIcons.cSeparatorImage == NULL)
	{
		pNewSurface = cairo_dock_create_blank_surface (
			iWidth,
			iHeight);
	}
	else
	{
		gchar *cImagePath = cairo_dock_generate_file_path (myIcons.cSeparatorImage);
		
		pNewSurface = cairo_dock_create_surface_from_image_simple (cImagePath,
			iWidth,
			iHeight);
		
		g_free (cImagePath);
	}
	
	return pNewSurface;
}


static void _load_separator (Icon *icon)
{
	int iWidth = icon->iImageWidth;
	int iHeight = icon->iImageHeight;
	
	icon->pIconBuffer = cairo_dock_create_separator_surface (
		iWidth,
		iHeight);
}


Icon *cairo_dock_create_separator_icon (int iSeparatorType, CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	if ((iSeparatorType & 1) && ! myIcons.iSeparateIcons)
		return NULL;
	
	//\____________ On cree l'icone.
	Icon *icon = cairo_dock_new_separator_icon (iSeparatorType);
	icon->load_image = _load_separator;
	
	//\____________ On remplit ses buffers.
	if (pDock)
		cairo_dock_load_icon_buffers (icon, CAIRO_CONTAINER (pDock));

	return icon;
}


void cairo_dock_insert_automatic_separator_in_dock (int iSeparatorType, const gchar *cParentDockName, CairoDock *pDock)
{
	Icon *pSeparatorIcon = cairo_dock_create_separator_icon (iSeparatorType, pDock);
	if (pSeparatorIcon != NULL)
	{
		pSeparatorIcon->cParentDockName = g_strdup (cParentDockName);
		pDock->icons = g_list_insert_sorted (pDock->icons,
			pSeparatorIcon,
			(GCompareFunc) cairo_dock_compare_icons_order);
		pSeparatorIcon->fWidth *= pDock->container.fRatio;
		pSeparatorIcon->fHeight *= pDock->container.fRatio;
		pDock->fFlatDockWidth += myIcons.iIconGap + pSeparatorIcon->fWidth;
		///pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, pSeparatorIcon->fHeight);
	}
}

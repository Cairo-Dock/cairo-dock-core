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
#include <stdlib.h>
#include <cairo.h>

#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-applet-factory.h"
#include "cairo-dock-applet-manager.h"


cairo_surface_t *cairo_dock_create_applet_surface (const gchar *cIconFileName, int iWidth, int iHeight)
{
	cairo_surface_t *pNewSurface;
	if (cIconFileName == NULL)
	{
		pNewSurface = cairo_dock_create_blank_surface (
			iWidth,
			iHeight);
	}
	else
	{
		gchar *cIconPath = cairo_dock_search_icon_s_path (cIconFileName);
		pNewSurface = cairo_dock_create_surface_from_image_simple (cIconPath,
			iWidth,
			iHeight);
		g_free (cIconPath);
	}
	return pNewSurface;
}


Icon *cairo_dock_create_icon_for_applet (CairoDockMinimalAppletConfig *pMinimalConfig, CairoDockModuleInstance *pModuleInstance, CairoContainer *pContainer)
{
	//\____________ On cree l'icone.
	Icon *icon = cairo_dock_new_applet_icon (pMinimalConfig, pModuleInstance);
	
	//\____________ On remplit ses buffers.
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		cairo_dock_fill_icon_buffers_for_dock (icon, pDock);
	}
	else
	{
		cairo_dock_fill_icon_buffers_for_desklet (icon);  // ne cree rien si w ou h < 0.
	}
	return icon;
}

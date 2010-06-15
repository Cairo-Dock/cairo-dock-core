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
#include "cairo-dock-icon-loader.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-applet-factory.h"
#include "cairo-dock-applet-manager.h"


static cairo_surface_t *cairo_dock_create_applet_surface (const gchar *cIconFileName, int iWidth, int iHeight)
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


static void _load_applet (Icon *icon)
{
	int iWidth = icon->iImageWidth;
	int iHeight = icon->iImageHeight;
	//g_print ("%s : %dx%d\n", icon->cName, iWidth, iHeight);
	icon->pIconBuffer = cairo_dock_create_applet_surface (icon->cFileName,
		iWidth,
		iHeight);
	if (icon->pIconBuffer == NULL && icon->pModuleInstance != NULL)  // une image inexistante a ete definie en conf => on met l'icone par defaut.
	{
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (icon->pModuleInstance->pModule->pVisitCard->cIconFilePath,
			iWidth,
			iHeight);
	}
}

static gboolean _delete_applet (Icon *icon)
{
	if (icon->pModuleInstance != NULL)
	{
		cairo_dock_deinstanciate_module (icon->pModuleInstance);  // desactive l'instance du module -> n'est plus une applet.
		cairo_dock_update_conf_file_with_active_modules ();
		icon->cDesktopFileName = g_strdup("");  // cas d'une applet controlant une appli, elle devient du coup une appli normale, ce qu'on ne veut pas.
		return TRUE;
	}
	return FALSE;
}

Icon *cairo_dock_create_icon_for_applet (CairoDockMinimalAppletConfig *pMinimalConfig, CairoDockModuleInstance *pModuleInstance, CairoContainer *pContainer)

{
	//\____________ On cree l'icone.
	Icon *icon = cairo_dock_new_applet_icon (pMinimalConfig, pModuleInstance);
	icon->iface.load_image = _load_applet;
	icon->iface.on_delete = _delete_applet;
	
	//\____________ On remplit ses buffers.
	if (pContainer != NULL)
		cairo_dock_load_icon_buffers (icon, pContainer);  // ne cree rien si w ou h < 0.
	
	return icon;
}

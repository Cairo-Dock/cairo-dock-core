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

#include "cairo-dock-image-buffer.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_icon_get_allocated_width
#include "cairo-dock-module-manager.h"  // gldi_modules_write_active
#include "cairo-dock-module-instance-manager.h"  // GldiModuleInstance
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-icon-manager.h"  // cairo_dock_search_icon_s_path
#include "cairo-dock-applet-manager.h"

// public (manager, config, data)
GldiObjectManager myAppletIconObjectMgr;

// dependancies

// private


static cairo_surface_t *_create_applet_surface (const gchar *cIconFileName, int iWidth, int iHeight)
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
		gchar *cIconPath = cairo_dock_search_icon_s_path (cIconFileName, MAX (iWidth, iHeight));
		pNewSurface = cairo_dock_create_surface_from_image_simple (cIconPath,
			iWidth,
			iHeight);
		g_free (cIconPath);
	}
	return pNewSurface;
}


static void _load_image (Icon *icon)
{
	int iWidth = cairo_dock_icon_get_allocated_width (icon);
	int iHeight = cairo_dock_icon_get_allocated_height (icon);
	cairo_surface_t *pSurface = _create_applet_surface (icon->cFileName,
		iWidth,
		iHeight);
	if (pSurface == NULL && icon->pModuleInstance != NULL)  // une image inexistante a ete definie en conf => on met l'icone par defaut. Si aucune image n'est definie, alors c'est a l'applet de faire qqch (dessiner qqch, mettre une image par defaut, etc).
	{
		cd_debug ("SET default image: %s", icon->pModuleInstance->pModule->pVisitCard->cIconFilePath);
		pSurface = cairo_dock_create_surface_from_image_simple (icon->pModuleInstance->pModule->pVisitCard->cIconFilePath,
			iWidth,
			iHeight);
	}  // on ne recharge pas myDrawContext ici, mais plutot dans cairo_dock_load_icon_image(), puisqu'elle gere aussi la destruction de la surface.
	cairo_dock_load_image_buffer_from_surface (&icon->image, pSurface, iWidth, iHeight);
}


Icon *gldi_applet_icon_new (CairoDockMinimalAppletConfig *pMinimalConfig, GldiModuleInstance *pModuleInstance)
{
	GldiAppletIconAttr attr = {pMinimalConfig, pModuleInstance};
	return (Icon*)gldi_object_new (&myAppletIconObjectMgr, &attr);
}


  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	GldiAppletIcon *icon = (GldiAppletIcon*)obj;
	GldiAppletIconAttr *pAttributes = (GldiAppletIconAttr*)attr;
	g_return_if_fail (pAttributes->pModuleInstance != NULL);
	
	CairoDockMinimalAppletConfig *pMinimalConfig = pAttributes->pMinimalConfig;
	
	icon->iface.load_image = _load_image;
	icon->iGroup = CAIRO_DOCK_LAUNCHER;
	icon->pModuleInstance = pAttributes->pModuleInstance;
	
	//\____________ On recupere les infos de sa config.
	icon->cName = pMinimalConfig->cLabel;
	pMinimalConfig->cLabel = NULL;
	icon->cFileName = pMinimalConfig->cIconFileName;
	pMinimalConfig->cIconFileName = NULL;
	
	icon->fOrder = pMinimalConfig->fOrder;
	icon->bAlwaysVisible = pMinimalConfig->bAlwaysVisible;
	icon->bHasHiddenBg = pMinimalConfig->bAlwaysVisible;  // if we're going to see the applet all the time, let's add a background. if the user doesn't want it, he can always set a transparent bg color.
	icon->pHiddenBgColor = pMinimalConfig->pHiddenBgColor;
	pMinimalConfig->pHiddenBgColor = NULL;
	
	if (! pMinimalConfig->bIsDetached)
	{
		cairo_dock_icon_set_requested_display_size (icon, pMinimalConfig->iDesiredIconWidth, pMinimalConfig->iDesiredIconHeight);
	}
	else  // l'applet creera la surface elle-meme, car on ne sait ni la taille qu'elle voudra lui donner, ni meme si elle l'utilisera !
	{
		icon->fWidth = -1;
		icon->fHeight = -1;
	}
	// probably not useful...
	icon->fScale = 1;
	icon->fGlideScale = 1;
	icon->fWidthFactor = 1.;
	icon->fHeightFactor = 1.;
}

static void reset_object (GldiObject *obj)
{
	GldiAppletIcon *icon = (GldiAppletIcon*)obj;
	if (icon->pModuleInstance != NULL)  // delete the module-instance the icon belongs to
	{
		cd_debug ("Reset: %s", icon->cName);
		gldi_object_unref (GLDI_OBJECT(icon->pModuleInstance));  // if called from the 'reset' of the instance, this will do nothing since the ref is already 0; else, when the instance unref the icon, it will do nothing since its ref is already 0
	}
}

static gboolean delete_object (GldiObject *obj)
{
	GldiAppletIcon *icon = (GldiAppletIcon*)obj;
	if (icon->pModuleInstance != NULL)  // remove the instance from the current theme
	{
		cd_debug ("Delete: %s", icon->cName);
		gldi_object_delete (GLDI_OBJECT(icon->pModuleInstance));
		return FALSE;  // don't go further, since the ModuleInstance has already unref'd ourself
	}
	return TRUE;
}

void gldi_register_applet_icons_manager (void)
{
	// Object Manager
	memset (&myAppletIconObjectMgr, 0, sizeof (GldiObjectManager));
	myAppletIconObjectMgr.cName          = "AppletIcon";
	myAppletIconObjectMgr.iObjectSize    = sizeof (GldiAppletIcon);
	// interface
	myAppletIconObjectMgr.init_object    = init_object;
	myAppletIconObjectMgr.reset_object   = reset_object;
	myAppletIconObjectMgr.delete_object  = delete_object;
	// signals
	gldi_object_install_notifications (GLDI_OBJECT (&myAppletIconObjectMgr), NB_NOTIFICATIONS_APPLET_ICON);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myAppletIconObjectMgr), &myIconObjectMgr);
}

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

#include "gldi-config.h"  // GLDI_VERSION
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-manager.h"  // gldi_dock_get_name
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_generate_unique_filename
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_write_keys_to_conf_file
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_conf_file_needs_update
#include "cairo-dock-separator-manager.h"

// public (manager, config, data)
GldiSeparatorIconManager mySeparatorIconsMgr;

// dependancies
extern gchar *g_cCurrentLaunchersPath;

// private
#define CAIRO_DOCK_SEPARATOR_CONF_FILE "separator.desktop"


static cairo_surface_t *_create_separator_surface (int iWidth, int iHeight)
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

static void _load_image (Icon *icon)
{
	int iWidth = cairo_dock_icon_get_allocated_width (icon);
	int iHeight = cairo_dock_icon_get_allocated_height (icon);
	
	cairo_surface_t *pSurface = _create_separator_surface (
		iWidth,
		iHeight);
	cairo_dock_load_image_buffer_from_surface (&icon->image, pSurface, iWidth, iHeight);
}


Icon *gldi_separator_icon_new (const gchar *cConfFile, GKeyFile *pKeyFile)
{
	GldiSeparatorIconAttr attr = {(gchar*)cConfFile, pKeyFile};
	return (Icon*)gldi_object_new (GLDI_MANAGER(&mySeparatorIconsMgr), &attr);
}

Icon *gldi_auto_separator_icon_new (Icon *pPrevIcon, Icon *pNextIcon)
{
	GldiSeparatorIconAttr attr = {NULL, NULL};
	Icon *icon = (Icon*)gldi_object_new (GLDI_MANAGER(&mySeparatorIconsMgr), &attr);
	
	icon->iGroup = cairo_dock_get_icon_order (pPrevIcon) +
		(cairo_dock_get_icon_order (pPrevIcon) == cairo_dock_get_icon_order (pNextIcon) ? 0 : 1);  // for separators, group = order.
	icon->fOrder = (cairo_dock_get_icon_order (pPrevIcon) == cairo_dock_get_icon_order (pNextIcon) ? (pPrevIcon->fOrder + pNextIcon->fOrder) / 2 : 0);
	
	return icon;
}


gchar *gldi_separator_icon_add_conf_file (const gchar *cDockName, double fOrder)
{
	//\__________________ open the template.
	const gchar *cTemplateFile = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_SEPARATOR_CONF_FILE;	
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cTemplateFile);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	//\__________________ fill the parameters
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);
	
	gchar *cNewDesktopFileName = cairo_dock_generate_unique_filename ("container.desktop", g_cCurrentLaunchersPath);
	
	//\__________________ write the keys.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_write_keys_to_conf_file (pKeyFile, cNewDesktopFilePath);
	g_free (cNewDesktopFilePath);
	g_key_file_free (pKeyFile);
	return cNewDesktopFileName;
}



Icon *gldi_separator_icon_add_new (CairoDock *pDock, double fOrder)
{
	//\_________________ add a launcher in the current theme
	const gchar *cDockName = gldi_dock_get_name (pDock);
	if (fOrder == CAIRO_DOCK_LAST_ORDER)  // the order is not defined -> place at the end
	{
		Icon *pLastIcon = cairo_dock_get_last_launcher (pDock->icons);
		fOrder = (pLastIcon ? pLastIcon->fOrder + 1 : 1);
	}
	gchar *cNewDesktopFileName = gldi_separator_icon_add_conf_file (cDockName, fOrder);
	g_return_val_if_fail (cNewDesktopFileName != NULL, NULL);
	
	//\_________________ load the new icon
	Icon *pNewIcon = gldi_user_icon_new (cNewDesktopFileName);
	g_free (cNewDesktopFileName);
	g_return_val_if_fail (pNewIcon, NULL);
	
	cairo_dock_insert_icon_in_dock (pNewIcon, pDock, CAIRO_DOCK_ANIMATE_ICON);
	
	return pNewIcon;
}


  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	Icon *icon = (Icon*)obj;
	icon->iface.load_image = _load_image;
	
	GldiUserIconAttr *pAttributes = (GldiUserIconAttr*)attr;
	if (pAttributes->pKeyFile)
	{
		GKeyFile *pKeyFile = pAttributes->pKeyFile;
		gboolean bNeedUpdate = cairo_dock_conf_file_needs_update (pKeyFile, GLDI_VERSION);
		if (bNeedUpdate)
		{
			gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pAttributes->cConfFileName);
			const gchar *cTemplateFile = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_SEPARATOR_CONF_FILE;
			cairo_dock_upgrade_conf_file (cDesktopFilePath, pKeyFile, cTemplateFile);  // update keys
			g_free (cDesktopFilePath);
		}
	}
}

static GKeyFile* reload_object (GldiObject *obj, G_GNUC_UNUSED gboolean bReloadConf, GKeyFile *pKeyFile)
{
	Icon *icon = (Icon*)obj;
	cairo_dock_load_icon_image (icon, icon->pContainer);  // n oother parameters in config -> just reload the image
	return pKeyFile;
}

void gldi_register_separator_icons_manager (void)
{
	// Manager
	memset (&mySeparatorIconsMgr, 0, sizeof (GldiSeparatorIconManager));
	mySeparatorIconsMgr.mgr.cModuleName   = "SeparatorIcon";
	mySeparatorIconsMgr.mgr.init_object   = init_object;
	mySeparatorIconsMgr.mgr.reload_object = reload_object;
	mySeparatorIconsMgr.mgr.iObjectSize   = sizeof (GldiSeparatorIcon);
	// signals
	gldi_object_install_notifications (GLDI_OBJECT (&mySeparatorIconsMgr), NB_NOTIFICATIONS_SEPARATOR_ICON);
	gldi_object_set_manager (GLDI_OBJECT (&mySeparatorIconsMgr), GLDI_MANAGER (&myUserIconsMgr));
	// register
	gldi_register_manager (GLDI_MANAGER(&mySeparatorIconsMgr));
}


void gldi_automatic_separators_add_in_list (GList *pIconsList)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon, *pNextIcon, *pSeparatorIcon;
	GList *ic, *next_ic;
	for (ic = pIconsList; ic != NULL; ic = next_ic)
	{
		icon = ic->data;
		next_ic = ic->next;
		if (! GLDI_OBJECT_IS_SEPARATOR_ICON (icon) && next_ic != NULL)
		{
			pNextIcon = next_ic->data;
			if (! GLDI_OBJECT_IS_SEPARATOR_ICON (pNextIcon) && icon->iGroup != pNextIcon->iGroup)
			{
				pSeparatorIcon = gldi_auto_separator_icon_new (icon, pNextIcon);
				pIconsList = g_list_insert_before (pIconsList, next_ic, pSeparatorIcon);  // does not modify pIconsList
			}
		}
	}
}

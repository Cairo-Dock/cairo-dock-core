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

#include <glib/gstdio.h>

#include "cairo-dock-icon-facility.h"  // cairo_dock_compare_icons_order
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_delete_conf_file
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-separator-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-user-icon-manager.h"

// public (manager, config, data)
GldiObjectManager myUserIconObjectMgr;

// dependancies
extern gchar *g_cCurrentLaunchersPath;

// private


Icon *gldi_user_icon_new (const gchar *cConfFile)
{
	gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cConfFile);
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	Icon *pIcon = NULL;
	
	//\__________________ get the type of the icon
	int iType;
	if (g_key_file_has_key (pKeyFile, "Desktop Entry", "Icon Type", NULL))
	{
		iType = g_key_file_get_integer (pKeyFile, "Desktop Entry", "Icon Type", NULL);
	}
	else  // old desktop file
	{
		gchar *cCommand = g_key_file_get_string (pKeyFile, "Desktop Entry", "Exec", NULL);
		gboolean bIsContainer;
		if (g_key_file_has_key (pKeyFile, "Desktop Entry", "Is container", NULL))
			bIsContainer = g_key_file_get_boolean (pKeyFile, "Desktop Entry", "Is container", NULL);
		else if (g_key_file_has_key (pKeyFile, "Desktop Entry", "Nb subicons", NULL))
			bIsContainer = (g_key_file_get_integer (pKeyFile, "Desktop Entry", "Nb subicons", NULL) != 0);
		else
			bIsContainer = (g_key_file_get_integer (pKeyFile, "Desktop Entry", "Type", NULL) == 1);
		
		if (bIsContainer)
		{
			iType = CAIRO_DOCK_ICON_TYPE_CONTAINER;
		}
		else if (cCommand == NULL || *cCommand == '\0')
		{
			iType = CAIRO_DOCK_ICON_TYPE_SEPARATOR;
		}
		else
		{
			iType = CAIRO_DOCK_ICON_TYPE_LAUNCHER;
		}
		g_key_file_set_integer (pKeyFile, "Desktop Entry", "Icon Type", iType);  // the specialized manager will update the conf-file because the version has changed.
		g_free (cCommand);
	}
	
	//\__________________ make an icon for the given type
	GldiObjectManager *pMgr = NULL;
	switch (iType)
	{
		case CAIRO_DOCK_ICON_TYPE_LAUNCHER:
			pMgr = &myLauncherObjectMgr;
		break;
		case CAIRO_DOCK_ICON_TYPE_CONTAINER:
			pMgr = &myStackIconObjectMgr;
		break;
		case CAIRO_DOCK_ICON_TYPE_SEPARATOR:
			pMgr = &mySeparatorIconObjectMgr;
		break;
		default:
			cd_warning ("unknown user icon type for file %s", cDesktopFilePath);
		return NULL;
	}
	
	GldiUserIconAttr attr;
	memset (&attr, 0, sizeof (attr));
	attr.cConfFileName = (gchar*)cConfFile;
	attr.pKeyFile = pKeyFile;
	pIcon = (Icon*)gldi_object_new (pMgr, &attr);
	
	g_free (cDesktopFilePath);
	g_key_file_free (pKeyFile);
	return pIcon;
}


void gldi_user_icons_new_from_directory (const gchar *cDirectory)
{
	cd_message ("%s (%s)", __func__, cDirectory);
	GDir *dir = g_dir_open (cDirectory, 0, NULL);
	g_return_if_fail (dir != NULL);
	
	Icon* icon;
	const gchar *cFileName;
	CairoDock *pParentDock;

	while ((cFileName = g_dir_read_name (dir)) != NULL)
	{
		if (g_str_has_suffix (cFileName, ".desktop"))
		{
			icon = gldi_user_icon_new (cFileName);
			if (icon == NULL || icon->cDesktopFileName == NULL)  // if the icon couldn't be loaded, remove it from the theme (it's useless to try and fail to load it each time).
			{
				if (icon)
					gldi_object_unref (GLDI_OBJECT(icon));
				cd_warning ("Unable to load a valid icon from '%s/%s'; the file is either unreadable, unvalid or does not correspond to any installed program, and will be deleted", g_cCurrentLaunchersPath, cFileName);
				gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cFileName);
				cairo_dock_delete_conf_file (cDesktopFilePath);
				g_free (cDesktopFilePath);
				continue;
			}
			
			pParentDock = gldi_dock_get (icon->cParentDockName);
			if (pParentDock != NULL)  // a priori toujours vrai.
			{
				gldi_icon_insert_in_container (icon, CAIRO_CONTAINER(pParentDock), ! CAIRO_DOCK_ANIMATE_ICON);
			}
		}
	}
	g_dir_close (dir);
}


static void init_object (GldiObject *obj, gpointer attr)
{
	Icon *icon = (Icon*)obj;
	GldiUserIconAttr *pAttributes = (GldiUserIconAttr*)attr;
	
	if (! pAttributes->pKeyFile && pAttributes->cConfFileName)
	{
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pAttributes->cConfFileName);
		pAttributes->pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
		g_free (cDesktopFilePath);
	}
	if (!pAttributes->pKeyFile)
		return;
	
	// get generic parameters
	GKeyFile *pKeyFile = pAttributes->pKeyFile;
	
	icon->fOrder = g_key_file_get_double (pKeyFile, "Desktop Entry", "Order", NULL);
	
	icon->cParentDockName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Container", NULL);
	if (icon->cParentDockName == NULL || *icon->cParentDockName == '\0')
	{
		g_free (icon->cParentDockName);
		icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);
	}
	
	int iSpecificDesktop = g_key_file_get_integer (pKeyFile, "Desktop Entry", "ShowOnViewport", NULL);
	cairo_dock_set_specified_desktop_for_icon (icon, iSpecificDesktop);
	
	icon->cDesktopFileName = g_strdup (pAttributes->cConfFileName);
	
	// create its parent dock
	CairoDock *pParentDock = gldi_dock_get (icon->cParentDockName);
	if (pParentDock == NULL)
	{
		cd_message ("The parent dock (%s) doesn't exist: we create it", icon->cParentDockName);
		pParentDock = gldi_dock_new (icon->cParentDockName);
	}
}

static void reset_object (GldiObject *obj)
{
	Icon *icon = (Icon*)obj;
	g_free (icon->cDesktopFileName);
	icon->cDesktopFileName = NULL;
}

static gboolean delete_object (GldiObject *obj)
{
	Icon *icon = (Icon*)obj;
	
	if (icon->cDesktopFileName != NULL && icon->cDesktopFileName[0] != '/')  /// TODO: check that the 2nd condition is not needed any more...
	{
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
		cairo_dock_delete_conf_file (cDesktopFilePath);
		g_free (cDesktopFilePath);
	}
	
	return TRUE;
}

static GKeyFile* reload_object (GldiObject *obj, gboolean bReloadConf, GKeyFile *pKeyFile)
{
	Icon *icon = (Icon*)obj;
	
	if (!bReloadConf)  // just reload the icon buffers.
	{
		if (GLDI_OBJECT_IS_DOCK (icon->pContainer))
			cairo_dock_set_icon_size_in_dock (CAIRO_DOCK(icon->pContainer), icon);
		cairo_dock_load_icon_buffers (icon, icon->pContainer);
		return NULL;
	}
	
	gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
	pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	//\_____________ remember current state.
	CairoDock *pDock = CAIRO_DOCK (cairo_dock_get_icon_container (icon));
	double fOrder = icon->fOrder;
	
	//\_____________ get its new params.
	icon->fOrder = g_key_file_get_double (pKeyFile, "Desktop Entry", "Order", NULL);
	
	g_free (icon->cParentDockName);
	icon->cParentDockName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Container", NULL);
	if (icon->cParentDockName == NULL || *icon->cParentDockName == '\0')
	{
		g_free (icon->cParentDockName);
		icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);
	}
	
	int iSpecificDesktop = g_key_file_get_integer (pKeyFile, "Desktop Entry", "ShowOnViewport", NULL);
	cairo_dock_set_specified_desktop_for_icon (icon, iSpecificDesktop);
	
	// get its (possibly new) container.
	CairoDock *pNewDock = gldi_dock_get (icon->cParentDockName);
	if (pNewDock == NULL)
	{
		cd_message ("The parent dock (%s) doesn't exist, we create it", icon->cParentDockName);
		pNewDock = gldi_dock_new (icon->cParentDockName);
	}
	g_return_val_if_fail (pNewDock != NULL, pKeyFile);
	
	//\_____________ manage the change of container or order.
	if (pDock != pNewDock || icon->fOrder != fOrder)
	{
		gldi_icon_detach (icon);
		gldi_icon_insert_in_container (icon, CAIRO_CONTAINER(pNewDock), CAIRO_DOCK_ANIMATE_ICON);  // le remove et le insert vont declencher le redessin de l'icone pointant sur l'ancien et le nouveau sous-dock le cas echeant.
	}
	else if (pNewDock->iRefCount != 0)  // on redessine l'icone pointant sur le sous-dock, pour le cas ou l'image ou l'ordre de l'icone aurait change.
	{
		cairo_dock_trigger_redraw_subdock_content (pNewDock);
	}
	
	g_free (cDesktopFilePath);
	return pKeyFile;
}

void gldi_register_user_icons_manager (void)
{
	// Manager
	memset (&myUserIconObjectMgr, 0, sizeof (GldiObjectManager));
	myUserIconObjectMgr.cName         = "User-Icons";
	myUserIconObjectMgr.iObjectSize   = sizeof (GldiUserIcon);
	// interface
	myUserIconObjectMgr.init_object   = init_object;
	myUserIconObjectMgr.reset_object  = reset_object;
	myUserIconObjectMgr.delete_object = delete_object;
	myUserIconObjectMgr.reload_object = reload_object;
	// signals
	gldi_object_install_notifications (&myUserIconObjectMgr, NB_NOTIFICATIONS_USER_ICON);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myUserIconObjectMgr), &myIconObjectMgr);
}

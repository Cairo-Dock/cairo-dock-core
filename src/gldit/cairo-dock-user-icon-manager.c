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
GldiUserIconManager myUserIconsMgr;

// dependancies
extern gchar *g_cCurrentLaunchersPath;

// private


/*
user-icon = {icon, desktop-file}
object-new(desktop-file, keyfile(may be null)): open file, get generic data (dock-name, order, workspace), iGroup = CAIRO_DOCK_LAUNCHER, set desktopfile, create container
delete_user_icon

specialized mgr: get data, update the key file if necessary, etc


icon_create_from_conf_file -> open keyfile -> get type -> object_new(mgr), free keyfile

- LauncherMgr (desktop file) -> get data, register class, set data from class, load_launcher, check validity, inhibite class
- SeparatorMgr (desktop file or NULL) -> load_separator
- ContainerIconMgr (desktop file) -> get data, create sub-dock, load_launcher_with_sub_dock, delete_sub_icons, create sub-dock, check name

is_user_icon: is_child(user-icon-mgr)
is_separator: is_child(separator-mgr)
is_auto_separator: is_separator && conf-file == NULL

create auto separator: object_new(SeparatorMgr, NULL)

OR

- AutoSeparatorMgr() -> load_separator
is_separator: is_child(separator-mgr) || is_child(auto-separator-mgr)




icon_create_from_conf_file -> object_new(myLauncherMgr, desktop file)
-> user-icon = {icon, desktop-file}

icon_create_dummy -> icon_new, set data (mgr = myIconsMgr)

icon->write_container_name -> user-icon, applet-icon
icon->write_order -> user-icon, applet-icon


ThemeIcon 
write-container-name
write-order

*/

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
	GldiManager *pMgr = NULL;
	switch (iType)
	{
		case CAIRO_DOCK_ICON_TYPE_LAUNCHER:
			pMgr = GLDI_MANAGER (&myLaunchersMgr);
		break;
		case CAIRO_DOCK_ICON_TYPE_CONTAINER:
			pMgr = GLDI_MANAGER (&myStackIconsMgr);
		break;
		case CAIRO_DOCK_ICON_TYPE_SEPARATOR:
			pMgr = GLDI_MANAGER (&mySeparatorIconsMgr);
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
			if (icon == NULL)  // if the icon couldn't be loaded, remove it from the theme (it's useless to try and fail to load it each time).
			{
				cd_warning ("Unable to load a valid icon from '%s/%s'; the file is either unreadable, unvalid or does not correspond to any installed program, and will be deleted", g_cCurrentLaunchersPath, cFileName);
				gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cFileName);
				cairo_dock_delete_conf_file (cDesktopFilePath);
				g_free (cDesktopFilePath);
				continue;
			}
			
			pParentDock = gldi_dock_get (icon->cParentDockName);
			if (pParentDock != NULL)  // a priori toujours vrai.
			{
				cairo_dock_insert_icon_in_dock_full (icon, pParentDock, ! CAIRO_DOCK_ANIMATE_ICON, ! CAIRO_DOCK_INSERT_SEPARATOR, NULL);
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
	/**if (pDock != pNewDock)  // container has changed.
	{
		// on la detache de son container actuel et on l'insere dans le nouveau.
		cairo_dock_detach_icon_from_dock (icon, pDock);
		cairo_dock_insert_icon_in_dock (icon, pNewDock, CAIRO_DOCK_ANIMATE_ICON);  // le remove et le insert vont declencher le redessin de l'icone pointant sur l'ancien et le nouveau sous-dock le cas echeant.
	}
	else  // same container, but different order.
	{
		if (icon->fOrder != fOrder)  // On gere le changement d'ordre.
		{
			pNewDock->icons = g_list_remove (pNewDock->icons, icon);
			pNewDock->icons = g_list_insert_sorted (pNewDock->icons,
				icon,
				(GCompareFunc) cairo_dock_compare_icons_order);
			cairo_dock_update_dock_size (pDock);  // -> recalculate icons and update input shape
		}
		// on redessine l'icone pointant sur le sous-dock, pour le cas ou l'ordre et/ou l'image du lanceur aurait change.
		if (pNewDock->iRefCount != 0)
		{
			cairo_dock_redraw_subdock_content (pNewDock);
		}
	}*/
	if (pDock != pNewDock || icon->fOrder != fOrder)
	{
		cairo_dock_detach_icon_from_dock (icon, pDock);
		cairo_dock_insert_icon_in_dock (icon, pNewDock, CAIRO_DOCK_ANIMATE_ICON);  // le remove et le insert vont declencher le redessin de l'icone pointant sur l'ancien et le nouveau sous-dock le cas echeant.
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
	memset (&myUserIconsMgr, 0, sizeof (GldiUserIconManager));
	myUserIconsMgr.mgr.cModuleName   = "User-Icons";
	myUserIconsMgr.mgr.init_object   = init_object;
	myUserIconsMgr.mgr.reset_object  = reset_object;
	myUserIconsMgr.mgr.delete_object = delete_object;
	myUserIconsMgr.mgr.reload_object = reload_object;
	myUserIconsMgr.mgr.iObjectSize   = sizeof (GldiUserIcon);
	// signals
	gldi_object_install_notifications (&myUserIconsMgr, NB_NOTIFICATIONS_ICON);
	gldi_object_set_manager (GLDI_OBJECT (&myUserIconsMgr), GLDI_MANAGER (&myIconsMgr));
	// register
	gldi_register_manager (GLDI_MANAGER(&myUserIconsMgr));
}

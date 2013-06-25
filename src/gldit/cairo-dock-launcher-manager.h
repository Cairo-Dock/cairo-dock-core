/*
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

#ifndef __CAIRO_DOCK_LAUNCHER_ICON_MANAGER__
#define  __CAIRO_DOCK_LAUNCHER_ICON_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-user-icon-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-launcher-manager.h This class handles the Launcher Icons, which are user icons used to launch a program.
*/

// manager
typedef GldiUserIconAttr GldiLauncherIconAttr;
typedef GldiUserIcon GldiLauncherIcon;

#ifndef _MANAGER_DEF_
extern GldiObjectManager myLauncherObjectMgr;
#endif

// signals
typedef enum {
	NB_NOTIFICATIONS_LAUNCHER = NB_NOTIFICATIONS_USER_ICON,
	} GldiLauncherNotifications;


/** Say if an object is a LauncherIcon.
*@param obj the object.
*@return TRUE if the object is a LauncherIcon.
*/
#define GLDI_OBJECT_IS_LAUNCHER_ICON(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myLauncherObjectMgr)


Icon *gldi_launcher_new (const gchar *cConfFile, GKeyFile *pKeyFile);


gchar *gldi_launcher_add_conf_file (const gchar *cURI, const gchar *cDockName, double fOrder);


Icon *gldi_launcher_add_new (const gchar *cURI, CairoDock *pDock, double fOrder);


void gldi_register_launchers_manager (void);


G_END_DECLS
#endif

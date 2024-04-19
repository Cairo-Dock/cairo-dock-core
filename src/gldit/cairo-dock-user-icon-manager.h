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

#ifndef __CAIRO_DOCK_USER_ICON_MANAGER__
#define  __CAIRO_DOCK_USER_ICON_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-icon-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-user-icon-manager.h This class handles the User Icons.
* These are Icons belonging to the user (like launchers, stack-icons, separators), and that have a config file.
* The config file contains at least the dock the icon belongs to and the position inside the dock.
*/

// manager
typedef struct _GldiUserIconAttr GldiUserIconAttr;
typedef struct _Icon GldiUserIcon;

#ifndef _MANAGER_DEF_
extern GldiObjectManager myUserIconObjectMgr;
#endif

struct _GldiUserIconAttr {
	const gchar *cConfFileName;
	GKeyFile *pKeyFile;
	int iType; // GldiUserIconType
};

// signals
typedef enum {
	NB_NOTIFICATIONS_USER_ICON = NB_NOTIFICATIONS_ICON,
	} GldiUserIconNotifications;


// ID of the different types of UserIcon, used in the .desktop (exported for the GUI factory)
typedef enum {
	GLDI_USER_ICON_TYPE_LAUNCHER = 0,
	GLDI_USER_ICON_TYPE_STACK,
	GLDI_USER_ICON_TYPE_SEPARATOR,
	/*CAIRO_DOCK_ICON_TYPE_CLASS_CONTAINER,
	CAIRO_DOCK_ICON_TYPE_APPLI,
	CAIRO_DOCK_ICON_TYPE_APPLET,
	CAIRO_DOCK_ICON_TYPE_OTHER,*/
	GLDI_USER_ICON_NB_ICON_TYPES
	} GldiUserIconType;

/** Say if an object is a UserIcon.
*@param obj the object.
*@return TRUE if the object is a UserIcon.
*/
#define GLDI_OBJECT_IS_USER_ICON(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myUserIconObjectMgr)


Icon *gldi_user_icon_new (const gchar *cConfFile);


void gldi_user_icons_new_from_directory (const gchar *cDirectory);


void gldi_register_user_icons_manager (void);


G_END_DECLS
#endif

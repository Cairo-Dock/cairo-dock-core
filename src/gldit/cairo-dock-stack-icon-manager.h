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

#ifndef __CAIRO_DOCK_STACK_ICON_ICON_MANAGER__
#define  __CAIRO_DOCK_STACK_ICON_ICON_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-user-icon-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-stack-icon-manager.h This class handles the Stack Icons, which are user icons pointing to a sub-dock.
*/

// manager
typedef struct GldiUserIconAttr GldiStackIconAttr;
typedef GldiUserIcon GldiStackIcon;

#ifndef _MANAGER_DEF_
extern GldiObjectManager myStackIconObjectMgr;
#endif

// signals
typedef enum {
	NB_NOTIFICATIONS_STACK_ICON = NB_NOTIFICATIONS_USER_ICON,
	} GldiStackIconNotifications;


/** Say if an object is a StackIcon.
*@param obj the object.
*@return TRUE if the object is a StackIcon.
*/
#define GLDI_OBJECT_IS_STACK_ICON(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myStackIconObjectMgr)


gchar *gldi_stack_icon_add_conf_file (const gchar *cDockName, double fOrder);


Icon *gldi_stack_icon_add_new (CairoDock *pDock, double fOrder);


void gldi_register_stack_icons_manager (void);


G_END_DECLS
#endif

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

#ifndef __CAIRO_DOCK_CLASS_ICON_MANAGER__
#define  __CAIRO_DOCK_CLASS_ICON_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-icon-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-class-manager.h This class handles the Class Icons, which are icons pointing to the sub-dock of a class.
*/

// manager
typedef struct _GldiClassIconAttr GldiClassIconAttr;
typedef Icon GldiClassIcon;

#ifndef _MANAGER_DEF_
extern GldiObjectManager myClassIconObjectMgr;
#endif

struct _GldiClassIconAttr {
	Icon *pAppliIcon;
	CairoDock *pClassSubDock;
};

// signals
typedef enum {
	NB_NOTIFICATIONS_CLASS_ICON = NB_NOTIFICATIONS_ICON,
	} GldiClassIconNotifications;


/** Say if an object is a ClassIcon.
*@param obj the object.
*@return TRUE if the object is a ClassIcon.
*/
#define GLDI_OBJECT_IS_CLASS_ICON(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myClassIconObjectMgr)


Icon *gldi_class_icon_new (Icon *pAppliIcon, CairoDock *pClassSubDock);


void gldi_register_class_icons_manager (void);

G_END_DECLS
#endif

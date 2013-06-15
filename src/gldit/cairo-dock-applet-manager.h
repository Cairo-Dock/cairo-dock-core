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

#ifndef __CAIRO_DOCK_APPLET_ICON_MANAGER__
#define  __CAIRO_DOCK_APPLET_ICON_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-icon-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-applet-manager.h This class handles the Applet Icons, which are icons used by module instances.
* Note: they are not UserIcon, because they are created by and belongs to a ModuleInstance, which is the actual object belonging to the user.
*/

typedef struct _GldiAppletIconManager GldiAppletIconManager;
typedef struct _GldiAppletIconAttr GldiAppletIconAttr;
typedef Icon GldiAppletIcon;  // icon + module-instance

#ifndef _MANAGER_DEF_
extern GldiAppletIconManager myAppletIconsMgr;
#endif

// manager
struct _GldiAppletIconManager {
	GldiManager mgr;
} ;

struct _GldiAppletIconAttr {
	CairoDockMinimalAppletConfig *pMinimalConfig;
	GldiModuleInstance *pModuleInstance;
};

// signals
typedef enum {
	NB_NOTIFICATIONS_APPLET_ICON = NB_NOTIFICATIONS_ICON,
	} GldiAppletIconNotifications;


/** Say if an object is a AppletIcon.
*@param obj the object.
*@return TRUE if the object is a AppletIcon.
*/
#define GLDI_OBJECT_IS_APPLET_ICON(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), GLDI_MANAGER(&myAppletIconsMgr))


Icon *gldi_applet_icon_new (CairoDockMinimalAppletConfig *pMinimalConfig, GldiModuleInstance *pModuleInstance);


void gldi_register_applet_icons_manager (void);


G_END_DECLS
#endif

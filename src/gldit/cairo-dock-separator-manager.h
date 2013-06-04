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

#ifndef __CAIRO_DOCK_SEPARATOR_MANAGER__
#define  __CAIRO_DOCK_SEPARATOR_MANAGER__
#include "cairo-dock-struct.h"
#include "cairo-dock-user-icon-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-separator-manager.h This class handles the Separator Icons, which are user icons doing nothing.
*/

typedef struct _GldiSeparatorIconManager GldiSeparatorIconManager;
typedef GldiUserIconAttr GldiSeparatorIconAttr;
typedef GldiUserIcon GldiSeparatorIcon;

#ifndef _MANAGER_DEF_
extern GldiSeparatorIconManager mySeparatorIconsMgr;
#endif

// manager
struct _GldiSeparatorIconManager {
	GldiManager mgr;
} ;

// signals
typedef enum {
	NB_NOTIFICATIONS_SEPARATOR_ICON = NB_NOTIFICATIONS_USER_ICON,
	} GldiSeparatorIconNotifications;


/** Say if an object is a SeparatorIcon.
*@param obj the object.
*@return TRUE if the object is a SeparatorIcon.
*/
#define GLDI_OBJECT_IS_SEPARATOR_ICON(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), GLDI_MANAGER(&mySeparatorIconsMgr))

#define GLDI_OBJECT_IS_USER_SEPARATOR_ICON(obj) (GLDI_OBJECT_IS_SEPARATOR_ICON(obj) && ((GldiSeparatorIcon*)obj)->cDesktopFileName != NULL)

#define GLDI_OBJECT_IS_AUTO_SEPARATOR_ICON(obj) (GLDI_OBJECT_IS_SEPARATOR_ICON(obj) && ((GldiSeparatorIcon*)obj)->cDesktopFileName == NULL)


Icon *gldi_separator_icon_new (const gchar *cConfFile, GKeyFile *pKeyFile);

Icon *gldi_auto_separator_icon_new (Icon *icon, Icon *pNextIcon);


void gldi_automatic_separators_add_in_list (GList *pIconsList);


gchar *gldi_separator_icon_add_conf_file (const gchar *cDockName, double fOrder);


Icon *gldi_separator_icon_add_new (CairoDock *pDock, double fOrder);


void gldi_register_separator_icons_manager (void);

G_END_DECLS
#endif


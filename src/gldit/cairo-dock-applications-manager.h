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

#ifndef __CAIRO_DOCK_APPLICATION_MANAGER__
#define  __CAIRO_DOCK_APPLICATION_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-icon-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-applications-manager.h This class manages the list of icons representing a window, ie the Taskbar.
*/

#ifndef _MANAGER_DEF_
extern GldiObjectManager myAppliIconObjectMgr;
#endif

/** Say if an object is an AppliIcon.
*@param obj the object.
*@return TRUE if the object is a AppliIcon.
*/
#define GLDI_OBJECT_IS_APPLI_ICON(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myAppliIconObjectMgr)


/** Get the list of appli-icons, including the icons not currently displayed in the dock. You can then order the list by z-order, name, etc.
*@return a newly allocated list of appli-icons. You must free the list when you're done with it, but not the icons.
*/
GList *cairo_dock_get_current_applis_list (void);

/** Get the icon of a given window, if any.
*@param actor the window actor
*@return the icon (maybe not inside a dock, maybe NULL).
*/
Icon *cairo_dock_get_appli_icon (GldiWindowActor *actor);

/** Run a function on all Appli icons.
*@param pFunction function to be called
*@param pUserData data passed to the function.
*/
void cairo_dock_foreach_appli_icon (GldiIconFunc pFunction, gpointer pUserData);

// defined in cairo-dock-application-facility.c
const CairoDockImageBuffer *gldi_appli_icon_get_image_buffer (Icon *pIcon);

G_END_DECLS
#endif

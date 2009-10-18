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

#include <X11/Xlib.h>

#include "cairo-dock-icons.h"
#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-applications-manager.h This class manages the taskbar and the interactions with X.
* Most of the internal functions make request to X, so you shouldn't use them directly, for performance considerations. Instead, use the data exposed by Cairo-Dock or available in the icon structure.
* Only the functions that don't make request to X are documented above.
*/


void cairo_dock_initialize_application_manager (Display *pDisplay);

void cairo_dock_register_appli (Icon *icon);
void cairo_dock_blacklist_appli (Window Xid);
void cairo_dock_unregister_appli (Icon *icon);


gboolean cairo_dock_appli_is_on_desktop (Icon *pIcon, int iNumDesktop, int iNumViewportX, int iNumViewportY);
gboolean cairo_dock_appli_is_on_current_desktop (Icon *pIcon);
Icon * cairo_dock_search_window_on_our_way (CairoDock *pDock, gboolean bMaximizedWindow, gboolean bFullScreenWindow);
gboolean cairo_dock_unstack_Xevents (CairoDock *pDock);
void cairo_dock_update_applis_list (CairoDock *pDock, gint iTime);
void cairo_dock_start_application_manager (CairoDock *pDock);
void cairo_dock_pause_application_manager (void);
void cairo_dock_stop_application_manager (void);

/** Get the state of the applications manager.
*@return TRUE if it is running (the X events are taken into account), FALSE otherwise.
*/
gboolean cairo_dock_application_manager_is_running (void);

/** Get the current workspace (desktop and viewport).
*@param iCurrentDesktop will be filled with the current desktop number
*@param iCurrentViewportX will be filled with the current horizontal viewport number
*@param iCurrentViewportY will be filled with the current vertical viewport number
*/
void cairo_dock_get_current_desktop_and_viewport (int *iCurrentDesktop, int *iCurrentViewportX, int *iCurrentViewportY);

/** Get the list of appli's icons currently known by Cairo-Dock, including the icons not displayed in the dock. You can then order the list by z-order, name, etc.
*@return a newly allocated list of applis's icons. You must free the list when you're finished with it, but not the icons.
*/
GList *cairo_dock_get_current_applis_list (void);

/** Get the currently active window ID.
*@return the X id.
*/
Window cairo_dock_get_current_active_window (void);
/** Get the icon of the currently active window.
*@return the icon (maybe not inside a dock).
*/
Icon *cairo_dock_get_current_active_icon (void);
/** Get the icon of a given window. The search is fast.
*@param Xid the id of the X window.
*@return the icon (maybe not inside a dock).
*/
Icon *cairo_dock_get_icon_with_Xid (Window Xid);

/** Run a function on all appli's icons.
*@param pFunction a #CairoDockForeachIconFunc function to be called
*@param bOutsideDockOnly TRUE if you only want to go through icons that are not inside a dock, FALSE to go through all icons.
*@param pUserData a data passed to the function.
*/
void cairo_dock_foreach_applis (CairoDockForeachIconFunc pFunction, gboolean bOutsideDockOnly, gpointer pUserData);

/** Run a function on all appli's icons present on a given workspace.
*@param pFunction a #CairoDockForeachIconFunc function to be called
*@param iNumDesktop number of the desktop
*@param iNumViewportX number of the horizontal viewport
*@param iNumViewportY number of the vertical viewport
*@param pUserData a data passed to the function.
*/
void cairo_dock_foreach_applis_on_viewport (CairoDockForeachIconFunc pFunction, int iNumDesktop, int iNumViewportX, int iNumViewportY, gpointer pUserData);


CairoDock *cairo_dock_insert_appli_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bUpdateSize, gboolean bAnimate);

CairoDock *cairo_dock_detach_appli (Icon *pIcon);

void cairo_dock_animate_icon_on_active (Icon *icon, CairoDock *pParentDock);

void cairo_dock_set_one_icon_geometry_for_window_manager (Icon *icon, CairoDock *pDock);
void cairo_dock_set_icons_geometry_for_window_manager (CairoDock *pDock);


G_END_DECLS
#endif

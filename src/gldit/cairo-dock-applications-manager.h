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

#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"
#include "cairo-dock-icon-manager.h"  // CairoDockForeachIconFunc
G_BEGIN_DECLS

/**
*@file cairo-dock-applications-manager.h This class manages the list of known windows, ie the Taskbar.
* It also provides convenient functions to act on all the applis icons at once.
*/

typedef struct _CairoTaskbarParam CairoTaskbarParam;
typedef struct _CairoTaskbarManager CairoTaskbarManager;

#ifndef _MANAGER_DEF_
extern CairoTaskbarParam myTaskbarParam;
extern CairoTaskbarManager myTaskbarMgr;
#endif

// params
struct _CairoTaskbarParam {
	gboolean bShowAppli;
	gboolean bGroupAppliByClass;
	gint iAppliMaxNameLength;
	gboolean bMinimizeOnClick;
	gboolean bCloseAppliOnMiddleClick;
	gboolean bHideVisibleApplis;
	gdouble fVisibleAppliAlpha;
	gboolean bAppliOnCurrentDesktopOnly;
	gboolean bDemandsAttentionWithDialog;
	gint iDialogDuration;
	gchar *cAnimationOnDemandsAttention;
	gchar *cAnimationOnActiveWindow;
	gboolean bOverWriteXIcons;
	gint iMinimizedWindowRenderType;
	gboolean bMixLauncherAppli;
	gchar *cOverwriteException;
	gchar *cGroupException;
	gchar *cForceDemandsAttention;
	} ;

// manager
struct _CairoTaskbarManager {
	GldiManager mgr;
	void (*foreach_applis) (CairoDockForeachIconFunc pFunction, gboolean bOutsideDockOnly, gpointer pUserData);
	void (*start_application_manager) (CairoDock *pDock);
	void (*stop_application_manager) (void);
	
	void (*unregister_appli) (Icon *icon);
	Icon* (*create_icon_from_xwindow) (Window Xid, CairoDock *pDock);
	
	void (*hide_show_launchers_on_other_desktops) (CairoDock *pDock);
	void (*set_specified_desktop_for_icon) (Icon *pIcon, int iSpecificDesktop);
	
	GList* (*get_current_applis_list) (void);
	Window (*get_current_active_window) (void);
	Icon* (*get_current_active_icon) (void);
	Icon* (*get_icon_with_Xid) (Window Xid);
} ;

// signals
typedef enum {
	NB_NOTIFICATIONS_TASKBAR
	} CairoTaskbarNotifications;


// Applis manager : core
void cairo_dock_unregister_appli (Icon *icon);

/** Start the applications manager. It will load all the applis, and keep monitoring them. If necessary, it will insert them into the dock.
*@param pDock the main dock
*/
void cairo_dock_start_applications_manager (CairoDock *pDock);

void cairo_dock_reset_applications_manager (void);


Icon * cairo_dock_create_icon_from_xwindow (Window Xid, CairoDock *pDock);



// Applis manager : access
#define _cairo_dock_appli_is_on_our_way(icon, pDock) (icon != NULL && cairo_dock_appli_is_on_current_desktop (icon) &&  ((myDocksParam.bAutoHideOnFullScreen && icon->bIsFullScreen) || (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP && cairo_dock_appli_overlaps_dock (icon, pDock))))

void cairo_dock_hide_show_if_current_window_is_on_our_way (CairoDock *pDock);
void cairo_dock_hide_if_any_window_overlap_or_show (CairoDock *pDock);


/** Get the icon of an application whose window covers entirely a dock, or NULL if none. If both parameters are FALSE, check for all windows.
*@param pDock the dock to test.
*@param bMaximizedWindow check for maximized windows only.
*@param bFullScreenWindow check for full screen windows only.
*@return the icon representing the window, or NULL if none has been found.
*/
Icon * cairo_dock_search_window_covering_dock (CairoDock *pDock, gboolean bMaximizedWindow, gboolean bFullScreenWindow);

/** Get the icon of an application whose window overlaps a dock, or NULL if none.
*@param pDock the dock to test.
*@return the icon of the window, or NULL if none has been found.
*/
Icon *cairo_dock_search_window_overlapping_dock (CairoDock *pDock);


/** Get the list of appli's icons currently known by Cairo-Dock, including the icons not displayed in the dock. You can then order the list by z-order, name, etc.
*@return a newly allocated list of applis's icons. You must free the list when you're done with it, but not the icons.
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
*@param pFunction a /ref CairoDockForeachIconFunc function to be called
*@param bOutsideDockOnly TRUE if you only want to go through icons that are not inside a dock, FALSE to go through all icons.
*@param pUserData a data passed to the function.
*/
void cairo_dock_foreach_applis (CairoDockForeachIconFunc pFunction, gboolean bOutsideDockOnly, gpointer pUserData);

/** Run a function on all appli's icons present on a given workspace.
*@param pFunction a /ref CairoDockForeachIconFunc function to be called
*@param iNumDesktop number of the desktop
*@param iNumViewportX number of the horizontal viewport
*@param iNumViewportY number of the vertical viewport
*@param pUserData a data passed to the function.
*/
void cairo_dock_foreach_applis_on_viewport (CairoDockForeachIconFunc pFunction, int iNumDesktop, int iNumViewportX, int iNumViewportY, gpointer pUserData);

void cairo_dock_set_icons_geometry_for_window_manager (CairoDock *pDock);


void gldi_register_applications_manager (void);

G_END_DECLS
#endif

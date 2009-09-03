
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


Icon * cairo_dock_search_window_on_our_way (gboolean bMaximizedWindow, gboolean bFullScreenWindow);
gboolean cairo_dock_unstack_Xevents (CairoDock *pDock);
void cairo_dock_update_applis_list (CairoDock *pDock, gint iTime);
void cairo_dock_start_application_manager (CairoDock *pDock);
void cairo_dock_pause_application_manager (void);
void cairo_dock_stop_application_manager (void);

/** Get the state of the applications manager.
*@return TRUE if it is running (the X events are taken into account), FALSE otherwise.
*/
gboolean cairo_dock_application_manager_is_running (void);

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



CairoDock *cairo_dock_insert_appli_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bUpdateSize, gboolean bAnimate);

CairoDock *cairo_dock_detach_appli (Icon *pIcon);

void cairo_dock_animate_icon_on_active (Icon *icon, CairoDock *pParentDock);

void cairo_dock_set_one_icon_geometry_for_window_manager (Icon *icon, CairoDock *pDock);
void cairo_dock_set_icons_geometry_for_window_manager (CairoDock *pDock);


G_END_DECLS
#endif

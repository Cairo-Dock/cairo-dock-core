
#ifndef __CAIRO_DOCK_APPLICATION_MANAGER__
#define  __CAIRO_DOCK_APPLICATION_MANAGER__

#include <X11/Xlib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

void cairo_dock_initialize_application_manager (Display *pDisplay);

void cairo_dock_register_appli (Icon *icon);
void cairo_dock_blacklist_appli (Window Xid);
void cairo_dock_unregister_appli (Icon *icon);

void  cairo_dock_set_one_icon_geometry_for_window_manager (Icon *icon, CairoDock *pDock);
void cairo_dock_set_icons_geometry_for_window_manager (CairoDock *pDock);;

void cairo_dock_close_xwindow (Window Xid);
void cairo_dock_kill_xwindow (Window Xid);
void cairo_dock_show_xwindow (Window Xid);
void cairo_dock_minimize_xwindow (Window Xid);
void cairo_dock_maximize_xwindow (Window Xid, gboolean bMaximize);
void cairo_dock_set_xwindow_fullscreen (Window Xid, gboolean bFullScreen);
void cairo_dock_set_xwindow_above (Window Xid, gboolean bAbove);
void cairo_dock_move_xwindow_to_nth_desktop (Window Xid, int iDesktopNumber, int iDesktopViewportX, int iDesktopViewportY);


gboolean cairo_dock_window_is_maximized (Window Xid);
gboolean cairo_dock_window_is_fullscreen (Window Xid);
void cairo_dock_window_is_above_or_below (Window Xid, gboolean *bIsAbove, gboolean *bIsBelow);
void cairo_dock_window_is_fullscreen_or_hidden_or_maximized (Window Xid, gboolean *bIsFullScreen, gboolean *bIsHidden, gboolean *bIsMaximized, gboolean *bDemandsAttention);
Window cairo_dock_get_active_xwindow (void);

int cairo_dock_get_window_desktop (int Xid);
void cairo_dock_get_window_geometry (int Xid, int *iGlobalPositionX, int *iGlobalPositionY, int *iWidthExtent, int *iHeightExtent);
void cairo_dock_get_window_position_on_its_viewport (int Xid, int *iRelativePositionX, int *iRelativePositionY);


gboolean cairo_dock_window_is_on_this_desktop (int Xid, int iDesktopNumber);
gboolean cairo_dock_window_is_on_current_desktop (int Xid);


void cairo_dock_animate_icon_on_active (Icon *icon, CairoDock *pParentDock);
Icon * cairo_dock_search_window_on_our_way (gboolean bMaximizedWindow, gboolean bFullScreenWindow);
gboolean cairo_dock_unstack_Xevents (CairoDock *pDock);
void cairo_dock_set_window_mask (Window Xid, long iMask);
Window *cairo_dock_get_windows_list (gulong *iNbWindows);
CairoDock *cairo_dock_insert_appli_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bUpdateSize, gboolean bAnimate);
void cairo_dock_update_applis_list (CairoDock *pDock, gint iTime);
void cairo_dock_start_application_manager (CairoDock *pDock);

void cairo_dock_pause_application_manager (void);

void cairo_dock_stop_application_manager (void);

gboolean cairo_dock_application_manager_is_running (void);

GList *cairo_dock_get_current_applis_list (void);

Window cairo_dock_get_current_active_window (void);
Icon *cairo_dock_get_current_active_icon (void);
Icon *cairo_dock_get_icon_with_Xid (Window Xid);

G_END_DECLS
#endif

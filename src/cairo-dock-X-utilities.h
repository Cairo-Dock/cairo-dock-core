
#ifndef __CAIRO_DOCK_X_UTILITIES__
#define  __CAIRO_DOCK_X_UTILITIES__

#include <X11/Xlib.h>
#include <glib.h>
G_BEGIN_DECLS

/**
*@file cairo-dock-X-utilities.h This class provides many utilities functions to interact very specifically on X.
*/

Display *cairo_dock_initialize_X_desktop_support (void);
void cairo_dock_initialize_X_support (void);

const Display *cairo_dock_get_Xdisplay (void);

  /////////////
 // DESKTOP //
/////////////
guint cairo_dock_get_root_id (void);


gboolean cairo_dock_update_screen_geometry (void);

gboolean cairo_dock_property_is_present_on_root (const gchar *cPropertyName);


int cairo_dock_get_current_desktop (void);
void cairo_dock_get_current_viewport (int *iCurrentViewPortX, int *iCurrentViewPortY);
int cairo_dock_get_nb_desktops (void);
void cairo_dock_get_nb_viewports (int *iNbViewportX, int *iNbViewportY);

gboolean cairo_dock_desktop_is_visible (void);
void cairo_dock_show_hide_desktop (gboolean bShow);
void cairo_dock_set_current_viewport (int iViewportNumberX, int iViewportNumberY);
void cairo_dock_set_current_desktop (int iDesktopNumber);

Pixmap cairo_dock_get_window_background_pixmap (Window Xid);

GdkPixbuf *cairo_dock_get_pixbuf_from_pixmap (int XPixmapID, gboolean bAddAlpha);

void cairo_dock_set_nb_viewports (int iNbViewportX, int iNbViewportY);
void cairo_dock_set_nb_desktops (gulong iNbDesktops);

gboolean cairo_dock_support_X_extension (void);
gboolean cairo_dock_xcomposite_is_available (void);
gboolean cairo_dock_xtest_is_available (void);
gboolean cairo_dock_xinerama_is_available (void);


void cairo_dock_get_screen_offsets (int iNumScreen);


  ////////////
 // WINDOW //
////////////

// SET //
void cairo_dock_set_xwindow_timestamp (Window Xid, gulong iTimeStamp);

void cairo_dock_set_strut_partial (int Xid, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x);

void cairo_dock_set_xwindow_mask (Window Xid, long iMask);

void cairo_dock_set_xwindow_type_hint (int Xid, const gchar *cWindowTypeName);
void cairo_dock_set_xicon_geometry (int Xid, int iX, int iY, int iWidth, int iHeight);

void cairo_dock_close_xwindow (Window Xid);
void cairo_dock_kill_xwindow (Window Xid);
void cairo_dock_show_xwindow (Window Xid);
void cairo_dock_minimize_xwindow (Window Xid);
void cairo_dock_maximize_xwindow (Window Xid, gboolean bMaximize);
void cairo_dock_set_xwindow_fullscreen (Window Xid, gboolean bFullScreen);
void cairo_dock_set_xwindow_above (Window Xid, gboolean bAbove);
void cairo_dock_move_xwindow_to_nth_desktop (Window Xid, int iDesktopNumber, int iDesktopViewportX, int iDesktopViewportY);

// GET //
gulong cairo_dock_get_xwindow_timestamp (Window Xid);
gboolean cairo_dock_xwindow_is_maximized (Window Xid);
gboolean cairo_dock_xwindow_is_fullscreen (Window Xid);
void cairo_dock_xwindow_is_above_or_below (Window Xid, gboolean *bIsAbove, gboolean *bIsBelow);
void cairo_dock_xwindow_is_fullscreen_or_hidden_or_maximized (Window Xid, gboolean *bIsFullScreen, gboolean *bIsHidden, gboolean *bIsMaximized, gboolean *bDemandsAttention);
gboolean cairo_dock_xwindow_is_sticky (Window Xid);
gboolean cairo_dock_window_is_utility (int Xid);
gboolean cairo_dock_window_is_dock (int Xid);

int cairo_dock_get_xwindow_desktop (Window Xid);
void cairo_dock_get_xwindow_geometry (Window Xid, int *iGlobalPositionX, int *iGlobalPositionY, int *iWidthExtent, int *iHeightExtent);
void cairo_dock_get_xwindow_position_on_its_viewport (Window Xid, int *iRelativePositionX, int *iRelativePositionY);

gboolean cairo_dock_xwindow_is_on_this_desktop (Window Xid, int iDesktopNumber);
gboolean cairo_dock_xwindow_is_on_current_desktop (Window Xid);

Window *cairo_dock_get_windows_list (gulong *iNbWindows);
Window cairo_dock_get_active_xwindow (void);


G_END_DECLS
#endif

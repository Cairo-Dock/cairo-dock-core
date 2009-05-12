
#ifndef __CAIRO_DOCK_X_UTILITIES__
#define  __CAIRO_DOCK_X_UTILITIES__

#include <X11/Xlib.h>
#include <glib.h>
G_BEGIN_DECLS


void cairo_dock_initialize_X_support (void);

const Display *cairo_dock_get_Xdisplay (void);

guint cairo_dock_get_root_id (void);

gulong cairo_dock_get_xwindow_timestamp (Window Xid);
void cairo_dock_set_xwindow_timestamp (Window Xid, gulong iTimeStamp);


void cairo_dock_set_strut_partial (int Xid, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x);

void cairo_dock_set_xwindow_type_hint (int Xid, gchar *cWindowTypeName);
gboolean cairo_dock_window_is_utility (int Xid);
gboolean cairo_dock_window_is_dock (int Xid);

void cairo_dock_set_xicon_geometry (int Xid, int iX, int iY, int iWidth, int iHeight);


gboolean cairo_dock_update_screen_geometry (void);

gboolean cairo_dock_property_is_present_on_root (gchar *cPropertyName);


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


G_END_DECLS
#endif

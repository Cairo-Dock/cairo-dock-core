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


#ifndef __CAIRO_DOCK_X_UTILITIES__
#define  __CAIRO_DOCK_X_UTILITIES__

#include "gldi-config.h"
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <glib.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/*
*@file cairo-dock-X-utilities.h Some utilities functions to interact very specifically on X.
*/

Display *cairo_dock_initialize_X_desktop_support (void);

/* Get the X Display used by the X manager. This is useful to ignore any X error silently (without having to call gdk_error_trap_push/pop each time).
 */
Display *cairo_dock_get_X_display (void);

void cairo_dock_reset_X_error_code (void);
unsigned char cairo_dock_get_X_error_code (void);

  /////////////
 // DESKTOP //
/////////////

gboolean cairo_dock_update_screen_geometry (void);

gchar **cairo_dock_get_desktops_names (void);

void cairo_dock_set_desktops_names (gchar **cNames);


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

typedef void (*GldiChangeViewportFunc) (int iNbViewportX, int iNbViewportY);

/** Change the number of viewports while keeping the total workarea as a rectangle.
 *  The new dimensions are calculated to keep it close to a square. The actual change
 *  is carried out by the callback given as the second argument.
 * @param iDeltaNbDesktops should be +1 or -1 to indicate whether to add or remove viewports
 * @param cb a function carrying out the actual change, taking two integer parameters
 *  which are the new X and Y dimensions
 */
void cairo_dock_change_nb_viewports (int iDeltaNbDesktops, GldiChangeViewportFunc cb);


  ////////////
 // WINDOW //
////////////

// SET //
void cairo_dock_set_xwindow_timestamp (Window Xid, gulong iTimeStamp);

void cairo_dock_set_xwindow_mask (Window Xid, long iMask);

//void cairo_dock_set_xwindow_type_hint (int Xid, const gchar *cWindowTypeName);
void cairo_dock_set_xicon_geometry (int Xid, int iX, int iY, int iWidth, int iHeight);

void cairo_dock_close_xwindow (Window Xid);
void cairo_dock_kill_xwindow (Window Xid);
void cairo_dock_show_xwindow (Window Xid);
void cairo_dock_minimize_xwindow (Window Xid);
void cairo_dock_lower_xwindow (Window Xid);
void cairo_dock_maximize_xwindow (Window Xid, gboolean bMaximize);
void cairo_dock_set_xwindow_fullscreen (Window Xid, gboolean bFullScreen);
void cairo_dock_set_xwindow_above (Window Xid, gboolean bAbove);
void cairo_dock_set_xwindow_sticky (Window Xid, gboolean bSticky);
void cairo_dock_move_xwindow_to_nth_desktop (Window Xid, int iDesktopNumber, int iDesktopViewportX, int iDesktopViewportY);
void cairo_dock_set_xwindow_border (Window Xid, gboolean bWithBorder);

// GET //
gulong cairo_dock_get_xwindow_timestamp (Window Xid);
gchar *cairo_dock_get_xwindow_name (Window Xid, gboolean bSearchWmName);
gchar *cairo_dock_get_xwindow_class (Window Xid, gchar **cWMClass, gchar **cWMName);

gboolean cairo_dock_xwindow_is_maximized (Window Xid);
gboolean cairo_dock_xwindow_is_fullscreen (Window Xid);
gboolean cairo_dock_xwindow_skip_taskbar (Window Xid);
gboolean cairo_dock_xwindow_is_sticky (Window Xid);
void cairo_dock_xwindow_is_above_or_below (Window Xid, gboolean *bIsAbove, gboolean *bIsBelow);
gboolean cairo_dock_xwindow_is_fullscreen_or_hidden_or_maximized (Window Xid, gboolean *bIsFullScreen, gboolean *bIsHidden, gboolean *bIsMaximized, gboolean *bDemandsAttention, gboolean *bIsSticky);
void cairo_dock_xwindow_can_minimize_maximized_close (Window Xid, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose);
gboolean cairo_dock_window_is_utility (int Xid);
gboolean cairo_dock_window_is_dock (int Xid);

Window *cairo_dock_get_windows_list (gulong *iNbWindows, gboolean bStackOrder);

Window cairo_dock_get_active_xwindow (void);


cairo_surface_t *cairo_dock_create_surface_from_xwindow (Window Xid, int iWidth, int iHeight);

cairo_surface_t *cairo_dock_create_surface_from_xpixmap (Pixmap Xid, int iWidth, int iHeight);

GLuint cairo_dock_texture_from_pixmap (Window Xid, Pixmap iBackingPixmap);


gboolean cairo_dock_get_xwindow_type (Window Xid, Window *pTransientFor);

gboolean cairo_dock_xcomposite_is_available (void);


void cairo_dock_set_strut_partial (Window Xid, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x);  // dock/desklet

int cairo_dock_get_xwindow_desktop (Window Xid);  // desklet
void cairo_dock_get_xwindow_geometry (Window Xid, int *iLocalPositionX, int *iLocalPositionY, int *iWidthExtent, int *iHeightExtent);  // desklet
void cairo_dock_move_xwindow_to_absolute_position (Window Xid, int iDesktopNumber, int iPositionX, int iPositionY);  // desklet

#endif

G_END_DECLS
#endif


#ifndef __CAIRO_DOCK_LAUNCHER_FACTORY__
#define  __CAIRO_DOCK_LAUNCHER_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


gchar *cairo_dock_search_icon_s_path (const gchar *cFileName);


void cairo_dock_load_icon_info_from_desktop_file (const gchar *cDesktopFileName, Icon *icon);


Icon * cairo_dock_create_icon_from_desktop_file (const gchar *cDesktopFileName, cairo_t *pSourceContext);

void cairo_dock_reload_icon_from_desktop_file (const gchar *cDesktopFileName, cairo_t *pSourceContext, Icon *icon);


G_END_DECLS
#endif

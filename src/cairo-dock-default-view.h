
#ifndef __CAIRO_DOCK_DEFAULT_VIEW__
#define  __CAIRO_DOCK_DEFAULT_VIEW__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cd_calculate_max_dock_size_default (CairoDock *pDock);


void cd_render_default (cairo_t *pCairoContext, CairoDock *pDock);


void cd_render_optimized_default (cairo_t *pCairoContext, CairoDock *pDock, GdkRectangle *pArea);


void cd_render_opengl_default (CairoDock *pDock);


Icon *cd_calculate_icons_default (CairoDock *pDock);


void cairo_dock_register_default_renderer (void);


G_END_DECLS
#endif


#ifndef __CAIRO_DOCK_DEFAULT_VIEW__
#define  __CAIRO_DOCK_DEFAULT_VIEW__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cairo_dock_set_subdock_position_linear (Icon *pPointedIcon, CairoDock *pParentDock);


void cairo_dock_calculate_max_dock_size_linear (CairoDock *pDock);


void cairo_dock_calculate_construction_parameters_generic (Icon *icon, int iCurrentWidth, int iCurrentHeight, int iMaxDockWidth);

void cairo_dock_render_linear (cairo_t *pCairoContext, CairoDock *pDock);


void cairo_dock_render_optimized_linear (cairo_t *pCairoContext, CairoDock *pDock, GdkRectangle *pArea);


void cairo_dock_render_opengl_linear (CairoDock *pDock);


Icon *cairo_dock_calculate_icons_linear (CairoDock *pDock);


void cairo_dock_register_default_renderer (void);


G_END_DECLS
#endif


#ifndef __CAIRO_DOCK_SEPARATOR_FACTORY__
#define  __CAIRO_DOCK_SEPARATOR_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


cairo_surface_t *cairo_dock_create_separator_surface (cairo_t *pSourceContext, double fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp, double *fWidth, double *fHeight);


Icon *cairo_dock_create_separator_icon (cairo_t *pSourceContext, int iSeparatorType, CairoDock *pDock, gboolean bApplyRatio);


G_END_DECLS
#endif


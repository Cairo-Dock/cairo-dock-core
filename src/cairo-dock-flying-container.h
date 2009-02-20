
#ifndef __CAIRO_FLYING_CONTAINER__
#define  __CAIRO_FLYING_CONTAINER__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


CairoFlyingContainer *cairo_dock_create_flying_container (Icon *pFlyingIcon, CairoDock *pOriginDock);

void cairo_dock_drag_flying_container (CairoFlyingContainer *pFlyingContainer, CairoDock *pOriginDock);

void cairo_dock_free_flying_container (CairoFlyingContainer *pFlyingContainer);

void cairo_dock_terminate_flying_container (CairoFlyingContainer *pFlyingContainer);


G_END_DECLS
#endif

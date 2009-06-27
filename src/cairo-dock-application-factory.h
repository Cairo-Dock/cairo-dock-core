
#ifndef __CAIRO_DOCK_APPLICATION_FACTORY__
#define  __CAIRO_DOCK_APPLICATION_FACTORY__

#include <glib.h>
#include <X11/Xlib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cairo_dock_initialize_application_factory (Display *pXDisplay);

//void cairo_dock_unregister_pid (Icon *icon);

cairo_surface_t *cairo_dock_create_surface_from_xpixmap (Pixmap Xid, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight);
cairo_surface_t *cairo_dock_create_surface_from_xwindow (Window Xid, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight);

CairoDock *cairo_dock_manage_appli_class (Icon *icon, CairoDock *pMainDock);

Icon * cairo_dock_create_icon_from_xwindow (cairo_t *pSourceContext, Window Xid, CairoDock *pDock);


void cairo_dock_Xproperty_changed (Icon *icon, Atom aProperty, int iState, CairoDock *pDock);

void cairo_dock_appli_demands_attention (Icon *icon);
void cairo_dock_appli_stops_demanding_attention (Icon *icon);


G_END_DECLS
#endif

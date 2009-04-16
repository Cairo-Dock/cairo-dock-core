
#ifndef __CAIRO_DOCK_SEPARATOR_FACTORY__
#define  __CAIRO_DOCK_SEPARATOR_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


cairo_surface_t *cairo_dock_create_separator_surface (cairo_t *pSourceContext, double fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp, double *fWidth, double *fHeight);

/** Create an icon that will act as a separator.
*@param pSourceContext a cairo context
*@param iSeparatorType the type of separator (CAIRO_DOCK_SEPARATOR12, CAIRO_DOCK_SEPARATOR23 or any other odd number)
*@param pDock the dock it will belong to
*@return the newly allocated icon, with all buffers filled.
*/
Icon *cairo_dock_create_separator_icon (cairo_t *pSourceContext, int iSeparatorType, CairoDock *pDock);


G_END_DECLS
#endif



#ifndef __CAIRO_DOCK_POSITION__
#define  __CAIRO_DOCK_POSITION__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"

#ifndef _INTERNAL_MODULE_
extern CairoConfigPosition myPosition;
#endif
G_BEGIN_DECLS

typedef struct _CairoConfigPosition {
	gint iGapX, iGapY;
	CairoDockPositionType iScreenBorder;
	gdouble fAlign;
	} CairoConfigPosition;

DEFINE_PRE_INIT (Position);

G_END_DECLS
#endif

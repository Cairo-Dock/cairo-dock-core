
#ifndef __CAIRO_DOCK_VIEWS__
#define  __CAIRO_DOCK_VIEWS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"


typedef struct _CairoConfigViews CairoConfigViews;
#ifndef _INTERNAL_MODULE_
extern CairoConfigViews myViews;
#endif
G_BEGIN_DECLS

struct _CairoConfigViews {
	gchar *cMainDockDefaultRendererName;
	gchar *cSubDockDefaultRendererName;
	gboolean bSameHorizontality;
	gdouble fSubDockSizeRatio;
	} ;

DEFINE_PRE_INIT (Views);

G_END_DECLS
#endif

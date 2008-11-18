
#ifndef __CAIRO_DOCK_HIDDEN_DOCK__
#define  __CAIRO_DOCK_HIDDEN_DOCK__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"


typedef struct _CairoConfigHiddenDock CairoConfigHiddenDock;
#ifndef _INTERNAL_MODULE_
extern CairoConfigHiddenDock myHiddenDock;
#endif
G_BEGIN_DECLS

struct _CairoConfigHiddenDock {
	gchar *cVisibleZoneImageFile;
	gint iVisibleZoneWidth, iVisibleZoneHeight;
	gdouble fVisibleZoneAlpha;
	gboolean bReverseVisibleImage;
	} ;


DEFINE_PRE_INIT (HiddenDock);

G_END_DECLS
#endif


#ifndef __CAIRO_DOCK_INTERNAL_LABELS__
#define  __CAIRO_DOCK_INTERNAL_LABELS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"


typedef struct _CairoConfigLabels CairoConfigLabels;
#ifndef _INTERNAL_MODULE_
extern CairoConfigLabels myLabels;
#endif
G_BEGIN_DECLS

struct _CairoConfigLabels {
	CairoDockLabelDescription iconTextDescription;
	CairoDockLabelDescription quickInfoTextDescription;
	gint iLabelSize;  // taille des etiquettes des icones, en prenant en compte le contour et la marge.
	};


DEFINE_PRE_INIT (Labels);

G_END_DECLS
#endif

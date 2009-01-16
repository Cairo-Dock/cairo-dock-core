
#ifndef __CAIRO_DOCK_INTERNAL_DESKLETS__
#define  __CAIRO_DOCK_INTERNAL_DESKLETS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"


typedef struct _CairoConfigDesklets CairoConfigDesklets;
#ifndef _INTERNAL_MODULE_
extern CairoConfigDesklets myDesklets;
#endif
G_BEGIN_DECLS

struct _CairoConfigDesklets {
	gchar *cDeskletDecorationsName;
	gint iDeskletButtonSize;
	gchar *cRotateButtonImage;
	gchar *cRetachButtonImage;
	};


DEFINE_PRE_INIT (Desklets);

G_END_DECLS
#endif

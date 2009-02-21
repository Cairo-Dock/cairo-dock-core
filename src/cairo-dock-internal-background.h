
#ifndef __CAIRO_DOCK_INTERNAL_BACKGROUND__
#define  __CAIRO_DOCK_INTERNAL_BACKGROUND__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"


typedef struct _CairoConfigBackground CairoConfigBackground;
#ifndef _INTERNAL_MODULE_
extern CairoConfigBackground myBackground;
#endif
G_BEGIN_DECLS

struct _CairoConfigBackground {
	gint iDockRadius;
	gint iDockLineWidth;
	gint iFrameMargin;
	gdouble fLineColor[4];
	gboolean bRoundedBottomCorner;
	gdouble fStripesColorBright[4];
	gchar *cBackgroundImageFile;
	gdouble fBackgroundImageAlpha;
	gboolean bBackgroundImageRepeat;
	gint iNbStripes;
	gdouble fStripesWidth;
	gdouble fStripesColorDark[4];
	gdouble fStripesAngle;
	};

#define g_iDockLineWidth myBackground.iDockLineWidth
#define g_iDockRadius myBackground.iDockRadius

DEFINE_PRE_INIT (Background);

G_END_DECLS
#endif

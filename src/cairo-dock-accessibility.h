
#ifndef __CAIRO_DOCK_ACCESSIBILITY__
#define  __CAIRO_DOCK_ACCESSIBILITY__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"

#ifndef _INTERNAL_MODULE_
extern CairoConfigAccessibility myAccessibility;
#endif
G_BEGIN_DECLS

typedef struct _CairoConfigAccessibility {
	gboolean bReserveSpace;
	gboolean bAutoHide;
	gboolean bPopUp;
	gboolean bPopUpOnScreenBorder;
	gint iMaxAuthorizedWidth;
	gchar *cRaiseDockShortcut;
	gint iLeaveSubDockDelay;
	gint iShowSubDockDelay;
	gboolean bShowSubDockOnClick;
	} CairoConfigAccessibility;

DEFINE_PRE_INIT (Accessibility);

G_END_DECLS
#endif

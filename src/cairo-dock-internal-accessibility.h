
#ifndef __CAIRO_DOCK_ACCESSIBILITY__
#define  __CAIRO_DOCK_ACCESSIBILITY__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"

typedef struct _CairoConfigAccessibility CairoConfigAccessibility;

#ifndef _INTERNAL_MODULE_
extern CairoConfigAccessibility myAccessibility;
#endif
G_BEGIN_DECLS

struct _CairoConfigAccessibility {
	gboolean bReserveSpace;
	gboolean bAutoHide;
	gboolean bAutoHideOnFullScreen;
	gboolean bAutoHideOnMaximized;
	gboolean bPopUp;
	gboolean bPopUpOnScreenBorder;
	gint iMaxAuthorizedWidth;
	gchar *cRaiseDockShortcut;
	gint iLeaveSubDockDelay;
	gint iShowSubDockDelay;
	gboolean bShowSubDockOnClick;
	gboolean bLockIcons;
	gboolean bExtendedMode;
	gint iVisibleZoneWidth, iVisibleZoneHeight;
	} ;

DEFINE_PRE_INIT (Accessibility);

G_END_DECLS
#endif

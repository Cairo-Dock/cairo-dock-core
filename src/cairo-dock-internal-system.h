
#ifndef __CAIRO_DOCK_SYSTEM__
#define  __CAIRO_DOCK_SYSTEM__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"

typedef struct _CairoConfigSystem CairoConfigSystem;

#ifndef _INTERNAL_MODULE_
extern CairoConfigSystem mySystem;
#endif
G_BEGIN_DECLS

struct _CairoConfigSystem {
	gboolean bUseFakeTransparency;
	gboolean bLabelForPointedIconOnly;
	gdouble fLabelAlphaThreshold;
	gboolean bTextAlwaysHorizontal;
	gdouble fUnfoldAcceleration;
	gboolean bAnimateOnAutoHide;
	gint iGrowUpInterval, iShrinkDownInterval;
	gdouble fMoveUpSpeed, fMoveDownSpeed;
	gdouble fRefreshInterval;
	gboolean bDynamicReflection;
	gboolean bAnimateSubDock;
	gdouble fStripesSpeedFactor;
	gboolean bDecorationsFollowMouse;
	gint iScrollAmount;
	gboolean bResetScrollOnLeave;
	gdouble fScrollAcceleration;
	gchar **cActiveModuleList;
	gint iFileSortType;
	gboolean bShowHiddenFiles;
	} ;

DEFINE_PRE_INIT (System);

G_END_DECLS
#endif

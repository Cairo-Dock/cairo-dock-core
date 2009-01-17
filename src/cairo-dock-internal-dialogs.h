
#ifndef __CAIRO_DOCK_INTERNAL_DIALOGS__
#define  __CAIRO_DOCK_INTERNAL_DIALOGS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-load.h"
#include "cairo-dock-config.h"


typedef struct _CairoConfigDialogs CairoConfigDialogs;
#ifndef _INTERNAL_MODULE_
extern CairoConfigDialogs myDialogs;
#endif
G_BEGIN_DECLS

struct _CairoConfigDialogs {
	gchar *cButtonOkImage;
	gchar *cButtonCancelImage;
	gint iDialogButtonWidth;
	gint iDialogButtonHeight;
	
	gint iDialogIconSize;

	CairoDockLabelDescription dialogTextDescription;
	gboolean bHomogeneous;
	
	gchar *cDecoratorName;
	gint iLineWidth;
	gint iCornerRadius;
	gdouble fLineColor[4];
	gdouble fDialogColor[4];
	} ;


DEFINE_PRE_INIT (Dialogs);

G_END_DECLS
#endif

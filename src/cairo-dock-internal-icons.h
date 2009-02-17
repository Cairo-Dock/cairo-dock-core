
#ifndef __CAIRO_DOCK_INTERNAL_ICONS__
#define  __CAIRO_DOCK_INTERNAL_ICONS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"


typedef struct _CairoConfigIcons CairoConfigIcons;
#ifndef _INTERNAL_MODULE_
extern CairoConfigIcons myIcons;
#endif
G_BEGIN_DECLS

struct _CairoConfigIcons {
	gdouble fFieldDepth;
	gdouble fAlbedo;
	gdouble fAmplitude;
	gint iSinusoidWidth;
	gint iIconGap;
	gint iStringLineWidth;
	gdouble fStringColor[4];
	gdouble fAlphaAtRest;
	gdouble fReflectSize;
	gchar **pDirectoryList;
	gpointer *pDefaultIconDirectory;
	gint tIconAuthorizedWidth[CAIRO_DOCK_NB_TYPES];
	gint tIconAuthorizedHeight[CAIRO_DOCK_NB_TYPES];
	gint tIconTypeOrder[CAIRO_DOCK_NB_TYPES];
	gchar *cBackgroundImagePath;
	gboolean bMixAppletsAndLaunchers;
	gboolean bUseSeparator;
	gchar *cSeparatorImage;
	gboolean bRevolveSeparator;
	gboolean bConstantSeparatorSize;
	gboolean bBgForApplets;
	};

#define g_fAmplitude myIcons.fAmplitude
#define g_fReflectSize myIcons.fReflectSize
#define g_bUseSeparator myIcons.bUseSeparator

DEFINE_PRE_INIT (Icons);

G_END_DECLS
#endif

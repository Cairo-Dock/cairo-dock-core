
#ifndef __CAIRO_DOCK_GRAPH2__
#define  __CAIRO_DOCK_GRAPH2__

#include <gtk/gtk.h>
G_BEGIN_DECLS

#include <cairo-dock-struct.h>
#include <cairo-dock-data-renderer.h>

typedef enum {
	CAIRO_DOCK_GRAPH2_LINE=0,
	CAIRO_DOCK_GRAPH2_PLAIN,
	CAIRO_DOCK_GRAPH2_BAR,
	CAIRO_DOCK_GRAPH2_CIRCLE,
	CAIRO_DOCK_GRAPH2_CIRCLE_PLAIN
	} CairoDockTypeGraph2;

typedef struct _CairoGraph2Attribute CairoGraph2Attribute;
struct _CairoGraph2Attribute {
	CairoDataRendererAttribute rendererAttribute;
	CairoDockTypeGraph iType;
	gdouble *fHighColor;  // iNbValues triplets (r,v,b).
	gdouble *fLowColor;  // idem.
	gdouble fBackGroundColor[4];
	gint iRadius;
	gboolean bMixGraphs;
};

typedef struct _CairoDockGraph2 {
	CairoDataRenderer dataRenderer;
	CairoDockTypeGraph2 iType;
	gdouble *fHighColor;  // iNbValues triplets (r,v,b).
	gdouble *fLowColor;  // idem.
	cairo_pattern_t **pGradationPatterns;  // iNbValues patterns.
	gdouble fBackGroundColor[4];
	cairo_surface_t *pBackgroundSurface;
	gint iRadius;
	gdouble fMargin;
	gboolean bMixGraphs;
	} CairoDockGraph2;

void cairo_dock_render_graph2 (CairoDockGraph2 *pGraph, cairo_t *pCairoContext);

void cairo_dock_reload_graph2 (CairoDockGraph2 *pGraph, cairo_t *pSourceContext);

void cairo_dock_load_graph2 (CairoDockGraph2 *pGraph, cairo_t *pSourceContext, CairoContainer *pContainer, CairoGraph2Attribute *pAttribute);

void cairo_dock_free_graph2 (CairoDockGraph2 *pGraph);

CairoDockGraph2* cairo_dock_new_graph2 (void);


void cairo_dock_add_watermark_on_graph2 (cairo_t *pSourceContext, CairoDockGraph2 *pGraph, gchar *cImagePath, double fAlpha);


G_END_DECLS
#endif

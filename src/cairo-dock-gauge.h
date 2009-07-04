
#ifndef __CAIRO_DOCK_GAUGE2__
#define  __CAIRO_DOCK_GAUGE2__

#include "cairo-dock-struct.h"
#include <cairo-dock-data-renderer.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
G_BEGIN_DECLS

typedef struct {
	RsvgHandle *pSvgHandle;
	cairo_surface_t *pSurface;
	gint sizeX;
	gint sizeY;
	GLuint iTexture;
} GaugeImage;

typedef struct {
	// needle
	gdouble posX, posY;
	gdouble posStart, posStop;
	gdouble direction;
	gint iNeedleRealWidth, iNeedleRealHeight;
	gdouble iNeedleOffsetX, iNeedleOffsetY;
	gdouble fNeedleScale;
	gint iNeedleWidth, iNeedleHeight;
	GaugeImage *pImageNeedle;
	// image list
	gint iNbImages;
	gint iNbImageLoaded;
	GaugeImage *pImageList;
	// text zone
	gdouble textX, textY;
	gdouble textWidth, textHeight;
	gdouble textColor[3];
} GaugeIndicator2;

typedef struct {
	CairoDataRenderer dataRenderer;
	gchar *cThemeName;
	GaugeImage *pImageBackground;
	GaugeImage *pImageForeground;
	GList *pIndicatorList;
} Gauge;

/// Attributes of a Gauge.
typedef struct _CairoGaugeAttribute CairoGaugeAttribute;
struct _CairoGaugeAttribute {
	/// General attributes of any DataRenderer.
	CairoDataRendererAttribute rendererAttribute;
	/// path to a gauge theme.
	gchar *cThemePath;
};


void cairo_dock_xml_open_file2 (const gchar *filePath, const gchar *mainNodeName,xmlDocPtr *xmlDoc,xmlNodePtr *node);

void cairo_dock_render_gauge (Gauge *pGauge, cairo_t *pCairoContext);

void cairo_dock_render_gauge_opengl2 (Gauge *pGauge);

void cairo_dock_reload_gauge (Gauge *pGauge, cairo_t *pSourceContext);

void cairo_dock_free_gauge (Gauge *pGauge);

void cairo_dock_add_watermark_on_gauge (cairo_t *pSourceContext, Gauge *pGauge, gchar *cImagePath, double fAlpha);

Gauge *cairo_dock_new_gauge (void);


GHashTable *cairo_dock_list_available_gauges (void);

const gchar *cairo_dock_get_gauge_key_value (gchar *cAppletConfFilePath, GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName);


G_END_DECLS
#endif

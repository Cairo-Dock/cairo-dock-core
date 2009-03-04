
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
} GaugeImage2;

typedef struct {
	// needle
	gdouble posX, posY;
	gdouble posStart, posStop;
	gdouble direction;
	gint iNeedleRealWidth, iNeedleRealHeight;
	gdouble iNeedleOffsetX, iNeedleOffsetY;
	gdouble fNeedleScale;
	gint iNeedleWidth, iNeedleHeight;
	GaugeImage2 *pImageNeedle;
	// image list
	gint iNbImages;
	gint iNbImageLoaded;
	GaugeImage2 *pImageList;
	// text zone
	gdouble textX, textY;
	gdouble textWidth, textHeight;
	gdouble textColor[3];
} GaugeIndicator2;

typedef struct {
	CairoDataRenderer dataRenderer;
	gchar *cThemeName;
	GaugeImage2 *pImageBackground;
	GaugeImage2 *pImageForeground;
	GList *pIndicatorList;
} Gauge2;

typedef struct _CairoGaugeAttribute CairoGaugeAttribute;
struct _CairoGaugeAttribute {
	CairoDataRendererAttribute rendererAttribute;
	gchar *cThemePath;
};


void cairo_dock_xml_open_file2 (const gchar *filePath, const gchar *mainNodeName,xmlDocPtr *xmlDoc,xmlNodePtr *node);

void cairo_dock_render_gauge2 (Gauge2 *pGauge, cairo_t *pCairoContext);

void cairo_dock_render_gauge_opengl2 (Gauge2 *pGauge);

void cairo_dock_reload_gauge2 (cairo_t *pSourceContext, Gauge2 *pGauge, int iWidth, int iHeight);

void cairo_dock_free_gauge2 (Gauge2 *pGauge);

void cairo_dock_add_watermark_on_gauge2 (cairo_t *pSourceContext, Gauge2 *pGauge, gchar *cImagePath, double fAlpha);

Gauge2 *cairo_dock_new_gauge (void);


GHashTable *cairo_dock_list_available_gauges2 (void);

const gchar *cairo_dock_get_gauge_key_value2 (gchar *cAppletConfFilePath, GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName);


G_END_DECLS
#endif


#ifndef __CAIRO_DOCK_DATA_RENDERER__
#define  __CAIRO_DOCK_DATA_RENDERER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-data-renderer.h"
G_BEGIN_DECLS


typedef struct _CairoDataRenderer CairoDataRenderer;
typedef struct _CairoDataRendererAttribute CairoDataRendererAttribute;
typedef struct _CairoDataRendererInterface CairoDataRendererInterface;
typedef struct _CairoDataToRenderer CairoDataToRenderer;

struct _CairoDataToRenderer {
	gint iNbValues;
	gint iMemorySize;
	gdouble **pTabValues;
	gint iCurrentIndex;
	gdouble *pMinMaxValues;
};

#define CAIRO_DOCK_DATA_FORMAT_MAX_LEN 12
typedef void (*CairoDockGetValueFormatFunc) (double fValue, gchar *cFormatBuffer, int iBufferLength);
struct _CairoDataRendererAttribute {
	gchar *cModelName;
	gint iNbValues;
	gchar **cTitles;
	gdouble fTextColor[3];
	gboolean bUpdateMinMax;
	gdouble *fMinMaxValues;
	gboolean bWriteValues;
	gboolean bSmoothMovement;
	CairoDockGetValueFormatFunc get_format_from_value;
};

typedef CairoDataRenderer * (*CairoDataRendererInitFunc) (const gchar *cRendererName);
typedef void (*CairoDataRendererLoadFunc) (CairoDataRenderer *pDataRenderer, cairo_t *pSourceContext, CairoDataRendererAttribute *pAttribute);
typedef void (*CairoDataRendererRenderFunc) (CairoDataRenderer *pDataRenderer, cairo_t *pCairoContext);
typedef void (*CairoDataRendererRenderOpenGLFunc) (CairoDataRenderer *pDataRenderer);
typedef void (*CairoDataRendererResizeFunc) (CairoDataRenderer *pDataRenderer, int iWidth, int iHeight);
typedef void (*CairoDataRendererFreeFunc) (CairoDataRenderer *pDataRenderer);

typedef CairoDataRenderer* (*CairoDataRendererInitFunc) (cairo_t *pSourceContext, CairoDataRendererAttribute *pAttribute);
struct _CairoDataRendererInterface {
	CairoDataRendererInitFunc init;
	CairoDataRendererLoadFunc load;
	CairoDataRendererRenderFunc render;
	CairoDataRendererRenderOpenGLFunc render_opengl;
	CairoDataRendererResizeFunc resize;
	CairoDataRendererFreeFunc free;
};

struct _CairoDataRenderer {
	CairoDataRendererInterface interface;
	gint iRank;  // nbre de valeurs que peut afficher 1 unite (en general : gauge:1/2, graph:1/2, bar:1)
	gint iWidth, iHeight;  // taille du contexte de dessin.
	gboolean bCanRenderValue;
	CairoDataToRenderer data;
	CairoDockGetValueFormatFunc get_format_from_value;
	gchar cValueBuffer[CAIRO_DOCK_DATA_FORMAT_MAX_LEN+1];
	gint dt;  // intervalle de temps entre 2 mesures, dont dt/2/n entre 2 etapes d'un changement de valeur.
};


void cairo_dock_add_new_data_renderer_on_icon (Icon *pIcon, cairo_t *pSourceContext, CairoDataRendererAttribute *pAttribute);

void cairo_dock_render_data_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pCairoContext, double *pNewValues);

void cairo_dock_remove_data_renderer_on_icon (Icon *pIcon);

void cairo_dock_reload_data_renderer_on_icon (Icon *pIcon, cairo_t *pSourceContext, CairoDataRendererAttribute *pAttribute);


G_END_DECLS
#endif

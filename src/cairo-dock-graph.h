
#ifndef __CAIRO_DOCK_GRAPH__
#define  __CAIRO_DOCK_GRAPH__

#include <gtk/gtk.h>
G_BEGIN_DECLS

#include <cairo-dock-struct.h>

typedef enum {
	CAIRO_DOCK_GRAPH_LINE=0,
	CAIRO_DOCK_GRAPH_PLAIN,
	CAIRO_DOCK_GRAPH_BAR,
	CAIRO_DOCK_GRAPH_CIRCLE,
	CAIRO_DOCK_GRAPH_CIRCLE_PLAIN
	} CairoDockTypeGraph;

#define CAIRO_DOCK_TYPE_GRAPH_MASK 7
#define CAIRO_DOCK_DOUBLE_GRAPH (1<<3)
#define CAIRO_DOCK_MIX_DOUBLE_GRAPH  (1<<4)
#define CCAIRO_DOCK_COMPUTE_MAX_VALUE (1<<5)

typedef struct _CairoDockGraph {
	gint iNbValues;
	gdouble *pTabValues;
	gdouble *pTabValues2;
	gint iCurrentIndex;
	gdouble fHighColor[3];
	gdouble fLowColor[3];
	gdouble fHighColor2[3];
	gdouble fLowColor2[3];
	gdouble fBackGroundColor[4];
	CairoDockTypeGraph iType;
	cairo_surface_t *pBackgroundSurface;
	cairo_pattern_t *pGradationPattern;
	cairo_pattern_t *pGradationPattern2;
	gdouble fWidth;
	gdouble fHeight;
	gint iRadius;
	gdouble fMargin;
	gboolean bMixDoubleGraph;
	gboolean bComputeMaxValue;
	gdouble fMaxValue;
	gdouble fMaxValue2;
	} CairoDockGraph;

/** Trace un graphe sur un contexte.
* @param pSourceContext le contexte de dessin sur lequel dessiner le graphe.
* @param pContainer le container de l'icone.
* @param pIcon l'icone.
* @param pGraph le graphe a dessiner.
*/
void cairo_dock_draw_graph (cairo_t *pCairoContext, CairoDockGraph *pGraph);

/** Ajoute une nouvelle valeur au graphe. La valeur la plus vieille est perdue.
* @param pGraph le graphe a mettre a jour.
* @param fNewValue la nouvelle valeur (entre 0 et 1).
*/
void cairo_dock_update_graph (CairoDockGraph *pGraph, double fNewValue);
/** Ajoute une nouvelle paires de valeurs au graphe. La valeur la plus vieille est perdue.
* @param pGraph le graphe a mettre a jour.
* @param fNewValue la nouvelle valeur (entre 0 et 1).
* @param fNewValue2 la 2eme nouvelle valeur (entre 0 et 1).
*/
void cairo_dock_update_double_graph (CairoDockGraph *pGraph, double fNewValue, double fNewValue2);

/** Dessine le graphe sur la surface de l'icone, et met a jour celle-ci.
* @param pSourceContext le contexte de dessin sur lequel dessiner le graphe. N'est pas altere.
* @param pContainer le container de l'icone.
* @param pIcon l'icone.
* @param pGraph le graphe a dessiner.
*/
void cairo_dock_render_graph (cairo_t *pSourceContext, CairoContainer *pContainer, Icon *pIcon, CairoDockGraph *pGraph);


/**
* @param pSourceContext un contexte de dessin.
* @param iNbValues le nombre de valeurs a retenir.
* @param iType le type du graphe.
* @param fWidth la  largeur du graphe.
* @param fHeight la hauteur du graphe.
* @param pLowColor couleur RVB des valeurs faibles.
* @param pHighColor couleur RVB des valeurs hautes.
* @param pBackGroundColor couleur RVBA du fond.
* @param pLowColor2 couleur RVB des valeurs faibles de la 2eme mesure.
* @param pHighColor2 couleur RVB des valeurs hautes de la 2eme mesure.
*/
CairoDockGraph* cairo_dock_create_graph (cairo_t *pSourceContext, int iNbValues, CairoDockTypeGraph iType, double fWidth, double fHeight, gdouble *pLowColor, gdouble *pHighColor, gdouble *pBackGroundColor, gdouble *pLowColor2, gdouble *pHighColor2);

/** Recharge un graphe a une taille donnee.
* @param pSourceContext un contexte de dessin.
* @param pGraph le graphe.
* @param iWidth la nouvelle largeur du graphe.
* @param iHeight la nouvelle hauteur du graphe.
*/
void cairo_dock_reload_graph (cairo_t *pSourceContext, CairoDockGraph *pGraph, int iWidth, int iHeight);

/** Detruit un graphe et libere les ressources allouees.
* @param pGraph le graphe.
*/
void cairo_dock_free_graph (CairoDockGraph *pGraph);


/** Ajoute une image en filigrane sur le fond du graphe.
* @param pSourceContext un contexte de dessin.
* @param pGraph le graphe.
* @param cImagePath chemin de l'image.
* @param fAlpha transparence du filigrane.
*/
void cairo_dock_add_watermark_on_graph (cairo_t *pSourceContext, CairoDockGraph *pGraph, gchar *cImagePath, double fAlpha);


G_END_DECLS
#endif

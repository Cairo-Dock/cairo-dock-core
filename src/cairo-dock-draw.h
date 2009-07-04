
#ifndef __CAIRO_DOCK_DRAW__
#define  __CAIRO_DOCK_DRAW__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


/**
*@file cairo-dock-draw.h This class provides some useful functions to draw with libcairo.
*/

  ///////////////
 /// CONTEXT ///
///////////////
/** Create a drawing context. It links it to a Glitz surface if this one is activated, or to an X surface representing the window of the container.
*@param pContainer a container.
*@return the context on which to draw. Is never NULL, test it with cairo_status() before use it, and destroy it with cairo_destroy() when you're done with it.
*/
cairo_t * cairo_dock_create_context_from_container (CairoContainer *pContainer);
#define cairo_dock_create_context_from_window cairo_dock_create_context_from_container
#define cairo_dock_create_drawing_context_generic cairo_dock_create_context_from_container

/** Create a drawing context to draw on a container. It handles fake transparency.
*@param pContainer the container on which you want to draw.
*@return the newly allocated context, to be destroyed with 'cairo_destroy'.
*/
cairo_t *cairo_dock_create_drawing_context (CairoContainer *pContainer);
#define cairo_dock_create_drawing_context_on_container cairo_dock_create_drawing_context

/** Create a drawing context to draw on a part of a container. It handles fake transparency.
*@param pContainer the container on which you want to draw
*@param pArea part of the container to draw.
*@param fBgColor background color (rgba) to fill the area with, or NULL to let it transparent.
*@return the newly allocated context, with a clip corresponding to the area, to be destroyed with 'cairo_destroy'.
*/
cairo_t *cairo_dock_create_drawing_context_on_area (CairoContainer *pContainer, GdkRectangle *pArea, double *fBgColor);



double cairo_dock_calculate_extra_width_for_trapeze (double fFrameHeight, double fInclination, double fRadius, double fLineWidth);

/** Compute the path of a rectangle with rounded corners. It doesn't stroke it, use cairo_stroke or cairo_fill to draw the line or the inside.
*@param pCairoContext a drawing context; the current matrix is not altered, but the current path is.
*@param fRadius radius if the corners.
*@param fLineWidth width of the line.
*@param fFrameWidth width of the rectangle, without the corners.
*@param fFrameHeight height of the rectangle, including the corners.
*/
void cairo_dock_draw_rounded_rectangle (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight);

/* Trace sur the context un contour trapezoidale aux coins arrondis. Le contour n'est pas dessine, mais peut l'etre a posteriori, et peut servir de cadre pour y dessiner des choses dedans.
*@param pCairoContext the context du dessin, contenant le cadre a la fin de la fonction.
*@param fRadius le rayon en pixels des coins.
*@param fLineWidth l'epaisseur en pixels du contour.
*@param fFrameWidth la largeur de la plus petite base du trapeze.
*@param fFrameHeight la hauteur du trapeze.
*@param fDockOffsetX un decalage, dans le sens de la largeur du dock, a partir duquel commencer a tracer la plus petite base du trapeze.
*@param fDockOffsetY un decalage, dans le sens de la hauteur du dock, a partir duquel commencer a tracer la plus petite base du trapeze.
*@param sens 1 pour un tracer dans le sens des aiguilles d'une montre (indirect), -1 sinon.
*@param fInclination tangente de l'angle d'inclinaison des cotes du trapeze par rapport a la vertical. 0 pour tracer un rectangle.
*@param bHorizontal CAIRO_DOCK_HORIZONTAL ou CAIRO_DOCK_VERTICAL suivant l'horizontalité du dock.
*/
double cairo_dock_draw_frame (cairo_t *pCairoContext, double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, int sens, double fInclination, gboolean bHorizontal);

/* Dessine les decorations d'un dock a l'interieur d'un cadre prealablement trace sur the context.
*@param pCairoContext the context du dessin, est laisse intact par la fonction.
*@param pDock le dock sur lequel appliquer les decorations.
*@param fOffsetY position du coin haut gauche du cadre, dans le sens de la hauteur du dock.
*@param fOffsetX position du coin haut gauche du cadre, dans le sens de la largeur du dock.
*@param fWidth largeur du cadre (et donc des decorations)
*/
void cairo_dock_render_decorations_in_frame (cairo_t *pCairoContext, CairoDock *pDock, double fOffsetY, double fOffsetX, double fWidth);


void cairo_dock_set_icon_scale_on_context (cairo_t *pCairoContext, Icon *icon, gboolean bHorizontalDock, double fRatio, gboolean bDirectionUp);

/** Draw an icon and its reflect on a dock. Only draw the icon's image and reflect, and nothing else.
*@param icon the icon to draw.
*@param pDock the dock containing the icon.
*@param pCairoContext a context on the dock, not altered by the function.
*/
void cairo_dock_draw_icon_cairo (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext);

gboolean cairo_dock_render_icon_notification_cairo (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext);

/** Draw an icon, according to its current parameters : position, transparency, reflect, rotation, stretching. Also draws its indicators, label, and quick-info. It generates a CAIRO_DOCK_RENDER_ICON notification.
*@param icon the icon to draw.
*@param pDock the dock containing the icon.
*@param pCairoContext a context on the dock, it is altered by the function.
*@param fDockMagnitude current magnitude of the dock.
*@param bUseText TRUE to draw the labels.
*/
void cairo_dock_render_one_icon (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext, double fDockMagnitude, gboolean bUseText);
void cairo_dock_render_icons_linear (cairo_t *pCairoContext, CairoDock *pDock);

void cairo_dock_render_one_icon_in_desklet (Icon *icon, cairo_t *pCairoContext, gboolean bUseReflect, gboolean bUseText, int iWidth);


/** Draw a string linking the center of all the icons of a dock.
*@param pCairoContext a context on the dock, not altered by the function.
*@param pDock the dock.
*@param fStringLineWidth width of the line.
*@param bIsLoop TRUE to loop (link the last icon to the first one).
*@param bForceConstantSeparator TRUE to consider separators having a constant size.
*/
void cairo_dock_draw_string (cairo_t *pCairoContext, CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator);


void cairo_dock_draw_surface (cairo_t *pCairoContext, cairo_surface_t *pSurface, int iWidth, int iHeight, gboolean bDirectionUp, gboolean bHorizontal, gdouble fAlpha);

/** Erase a drawing context, making it fully transparent. You don't need to erase a newly created context.
*@param pCairoContext a drawing context.
*/
#define cairo_dock_erase_cairo_context(pCairoContext) do {\
	cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 0.0);\
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);\
	cairo_paint (pCairoContext);\
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER); } while (0)


void cairo_dock_render_hidden_dock (cairo_t *pCairoContext, CairoDock *pDock);


/*#define _cairo_dock_extend_area(area, x, y, w, h) do {\
	int xmin = MIN (area.x, x);
	int ymin = MIN (area.y, y);
	area.width = MAX (area.x + area.width, x + w) - xmin;\
	area.height = MAX (area.y + area.height, y + h) - ymin;\
	area.x = MIN (area.x, x);\
	area.y = MIN (area.y, y); } while (0)
#define _cairo_dock_compute_areas_bounded_box(area, area_) _cairo_dock_extend_area (area, area_.x, area_.y, area_.width, area_.height)

#define cairo_dock_damage_container_area(pContainer, area) _cairo_dock_compute_areas_bounded_box (pContainer->damageArea, area)

#define cairo_dock_damage_icon(pIcon, pContainer) do {\
	GdkRectangle area;\
	cairo_dock_compute_icon_area (icon, pContainer, &area);\
	cairo_dock_damage_container_area(pContainer, area); } while (0)
	
#define cairo_dock_damage_container(pContainer) do {\
	pContainer->damageArea.x = 0;\
	pContainer->damageArea.y = 0;\
	if (pContainer->bHorizontal) {\
		pContainer->damageArea.width = pContainer->iWidth;\
		pContainer->damageArea.height = pContainer->iHeight; }\
	else {\
		pContainer->damageArea.width = pContainer->iHeight;\
		pContainer->damageArea.height = pContainer->iWidth; }\
	} while (0)*/


G_END_DECLS
#endif

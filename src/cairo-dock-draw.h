
#ifndef __CAIRO_DOCK_DRAW__
#define  __CAIRO_DOCK_DRAW__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

  ///////////////
 /// CONTEXT ///
///////////////
/** Cree un contexte de dessin pour la libcairo. Si glitz est active, le contexte sera lie a une surface glitz (et donc on dessinera directement sur la carte graphique), sinon a une surface X representant la fenetre du container.
*@param pContainer un container.
*@return le contexte sur lequel dessiner. N'est jamais nul; tester sa coherence avec cairo_status() avant de l'utiliser, et le detruire avec cairo_destroy() apres en avoir fini avec lui.
*/
cairo_t * cairo_dock_create_context_from_container (CairoContainer *pContainer);
#define cairo_dock_create_context_from_window cairo_dock_create_context_from_container

/** Cree un contexte de dessin cairo pour dessiner sur un container. Gere la fausse transparence.
*@param pContainer le container sur lequel on veut dessiner
*@return le contexte cairo nouvellement alloue.
*/
cairo_t *cairo_dock_create_drawing_context (CairoContainer *pContainer);

/** Cree un contexte de dessin cairo pour dessiner sur une partie d'un container seulement. Gere la fausse transparence.
*@param pContainer le container sur lequel on veut dessiner
*@param pArea la partie du container a redessiner
*@param fBgColor la couleur de fond (rvba) avec laquelle remplir l'aire, ou NULL pour la laisser transparente.
*@return le contexte cairo nouvellement alloue, avec un clip correspondant a l'aire.
*/
cairo_t *cairo_dock_create_drawing_context_on_area (CairoContainer *pContainer, GdkRectangle *pArea, double *fBgColor);



double cairo_dock_calculate_extra_width_for_trapeze (double fFrameHeight, double fInclination, double fRadius, double fLineWidth);

/**
*Trace sur le contexte un contour trapezoidale aux coins arrondis. Le contour n'est pas dessine, mais peut l'etre a posteriori, et peut servir de cadre pour y dessiner des choses dedans.
*@param pCairoContext le contexte du dessin, contenant le cadre a la fin de la fonction.
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

/**
*Dessine les decorations d'un dock a l'interieur d'un cadre prealablement trace sur le contexte.
*@param pCairoContext le contexte du dessin, est laisse intact par la fonction.
*@param pDock le dock sur lequel appliquer les decorations.
*@param fOffsetY un decalage, dans le sens de la hauteur du dock, a partir duquel appliquer les decorations.
*/
void cairo_dock_render_decorations_in_frame (cairo_t *pCairoContext, CairoDock *pDock, double fOffsetY, double fOffsetX, double fWidth);


void cairo_dock_set_icon_scale_on_context (cairo_t *pCairoContext, Icon *icon, gboolean bHorizontalDock, double fRatio, gboolean bDirectionUp);

void cairo_dock_draw_icon_cairo (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext);

gboolean cairo_dock_render_icon_notification_cairo (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext);

/**
*Dessine entierement une icone, dont toutes les caracteristiques ont ete prealablement calculees. Gere sa position, sa transparence (modulee par la transparence du dock au repos), son reflet, son placement de profil, son etiquette, et son info-rapide.
*@param icon l'icone a dessiner.
*@param pCairoContext le contexte du dessin, est altere pendant le dessin.
*@param bHorizontalDock l'horizontalite du dock contenant l'icone.
*@param fRatio le ratio de taille des icones dans ce dock.
*@param fDockMagnitude la magnitude actuelle du dock.
*@param bUseReflect TRUE pour dessiner les reflets.
*@param bUseText TRUE pour dessiner les etiquettes.
*@param iWidth largeur du container, utilisee pour que les etiquettes n'en debordent pas.
*@param bDirectionUp TRUE si le dock est oriente vers le haut.
*/
void cairo_dock_render_one_icon (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext, double fDockMagnitude, gboolean bUseText);
void cairo_dock_render_icons_linear (cairo_t *pCairoContext, CairoDock *pDock);

void cairo_dock_render_one_icon_in_desklet (Icon *icon, cairo_t *pCairoContext, gboolean bUseReflect, gboolean bUseText, int iWidth);


/**
*Dessine une ficelle reliant le centre de toutes les icones, en commencant par la 1ere dessinee.
*@param pCairoContext le contexte du dessin, n'est pas altere par la fonction.
*@param pDock le dock contenant les icônes a relier.
*@param fStringLineWidth epaisseur de la ligne.
*@param bIsLoop TRUE si on veut boucler (relier la derniere icone a la 1ere).
*@param bForceConstantSeparator TRUE pour forcer les separateurs a etre consideres comme de taille constante.
*/
void cairo_dock_draw_string (cairo_t *pCairoContext, CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator);


void cairo_dock_draw_surface (cairo_t *pCairoContext, cairo_surface_t *pSurface, int iWidth, int iHeight, gboolean bDirectionUp, gboolean bHorizontal, gdouble fAlpha);

#define cairo_dock_erase_cairo_context(pCairoContext) do {\
	cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 0.0);\
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);\
	cairo_paint (pCairoContext);\
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER); } while (0)


void cairo_dock_render_hidden_dock (cairo_t *pCairoContext, CairoDock *pDock);


/**#define _cairo_dock_extend_area(area, x, y, w, h) do {\
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

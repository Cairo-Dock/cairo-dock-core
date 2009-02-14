
#ifndef __CAIRO_DOCK_SURFACE_FACTORY__
#define  __CAIRO_DOCK_SURFACE_FACTORY__

#include <glib.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS


/**
* Calcule la taille d'une image selon une contrainte en largeur et hauteur de manière à remplir l'espace donné.
*@param fImageWidth la largeur de l'image. Contient initialement la largeur de l'image, et sera écrasée avec la largeur obtenue.
*@param fImageHeight la hauteur de l'image. Contient initialement la hauteur de l'image, et sera écrasée avec la hauteur obtenue.
*@param iWidthConstraint contrainte en largeur (0 <=> pas de contrainte).
*@param iHeightConstraint contrainte en hauteur (0 <=> pas de contrainte).
*@param bNoZoomUp TRUE ssi on ne doit pas agrandir l'image (seulement la rétrécir).
*@param fZoomWidth sera renseigné avec le facteur de zoom en largeur qui a été appliqué.
*@param fZoomHeight sera renseigné avec le facteur de zoom en hauteur qui a été appliqué.
*/
void cairo_dock_calculate_size_fill (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, gboolean bNoZoomUp, double *fZoomWidth, double *fZoomHeight);
/**
* Calcule la taille d'une image selon une contrainte en largeur et hauteur en gardant le ratio hauteur/largeur constant.
*@param fImageWidth la largeur de l'image. Contient initialement la largeur de l'image, et sera écrasée avec la largeur obtenue.
*@param fImageHeight la hauteur de l'image. Contient initialement la hauteur de l'image, et sera écrasée avec la hauteur obtenue.
*@param iWidthConstraint contrainte en largeur (0 <=> pas de contrainte).
*@param iHeightConstraint contrainte en hauteur (0 <=> pas de contrainte).
*@param bNoZoomUp TRUE ssi on ne doit pas agrandir l'image (seulement la rétrécir).
*@param fZoom sera renseigné avec le facteur de zoom qui a été appliqué.
*/
void cairo_dock_calculate_size_constant_ratio (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, gboolean bNoZoomUp, double *fZoom);
/**
* Calcule la taille d'une image selon une contrainte en largeur et hauteur.
*@param fImageWidth la largeur de l'image. Contient initialement la largeur de l'image, et sera écrasée avec la largeur obtenue.
*@param fImageHeight la hauteur de l'image. Contient initialement la hauteur de l'image, et sera écrasée avec la hauteur obtenue.
*@param iWidthConstraint contrainte en largeur (0 <=> pas de contrainte).
*@param iHeightConstraint contrainte en hauteur (0 <=> pas de contrainte).
*@param iLoadingModifier composition de modificateurs de chargement.
*@param fZoomWidth sera renseigné avec le facteur de zoom en largeur qui a été appliqué.
*@param fZoomHeight sera renseigné avec le facteur de zoom en hauteur qui a été appliqué.
*/
void cairo_dock_calculate_constrainted_size (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fZoomWidth, double *fZoomHeight);

/**
* Cree une surface à partir des données brutes d'une icone X. L'icone de plus grande taille contenue dans le buffer est prise.
*@param pXIconBuffer le tableau de données.
*@param iBufferNbElements le nombre d'éléments du tableau.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param fMaxScale le zoom maximal de l'icone.
*@param fWidth sera renseigné avec la largeur de l'icone.
*@param fHeight sera renseigné avec la hauteur de l'icone.
*@returns la surface nouvellement créée.
*/
cairo_surface_t *cairo_dock_create_surface_from_xicon_buffer (gulong *pXIconBuffer, int iBufferNbElements, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight);

/**
* Cree une surface à partir d'un GdkPixbuf.
*@param pixbuf le pixbuf.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param fMaxScale le zoom maximal de l'icone.
*@param iWidthConstraint contrainte sur la largeur, ou 0 pour ne pas la contraindre.
*@param iHeightConstraint contrainte sur la hauteur, ou 0 pour ne pas la contraindre.
*@param iLoadingModifier composition de modificateurs de chargement.
*@param fImageWidth sera renseigné avec la largeur de l'icone (hors zoom).
*@param fImageHeight sera renseigné avec la hauteur de l'icone (hors zoom).
*@param fZoomX si non NULL, sera renseigné avec le zoom horizontal qui a ete appliqué à l'image originale.
*@param fZoomY si non NULL, sera renseigné avec le zoom vertical qui a ete appliqué à l'image originale.
*@returns la surface nouvellement créée.
*/
cairo_surface_t *cairo_dock_create_surface_from_pixbuf (GdkPixbuf *pixbuf, cairo_t *pSourceContext, double fMaxScale, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fImageWidth, double *fImageHeight, double *fZoomX, double *fZoomY);

/**
* Cree une surface à partir d'une image au format quelconque.
*@param cImagePath le chemin complet de l'image.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param fMaxScale le zoom maximal de l'icone.
*@param iWidthConstraint contrainte sur la largeur, ou 0 pour ne pas la contraindre.
*@param iHeightConstraint contrainte sur la hauteur, ou 0 pour ne pas la contraindre.
*@param iLoadingModifier composition de modificateurs de chargement.
*@param fImageWidth sera renseigné avec la largeur de l'icone (hors zoom).
*@param fImageHeight sera renseigné avec la hauteur de l'icone (hors zoom).
*@param fZoomX si non NULL, sera renseigné avec le zoom horizontal qui a ete appliqué à l'image originale.
*@param fZoomY si non NULL, sera renseigné avec le zoom vertical qui a ete appliqué à l'image originale.
*@returns la surface nouvellement créée.
*/

cairo_surface_t *_cairo_dock_create_blank_surface (cairo_t *pSourceContext, int iWidth, int iHeight);

cairo_surface_t *cairo_dock_create_surface_from_image (const gchar *cImagePath, cairo_t* pSourceContext, double fMaxScale, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fImageWidth, double *fImageHeight, double *fZoomX, double *fZoomY);
/**
* Cree une surface à partir d'une image au format quelconque, a la taille donnée, sans zoom.
*@param cImagePath le chemin complet de l'image.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param fImageWidth la largeur de l'icone.
*@param fImageHeight la hauteur de l'icone.
*@returns la surface nouvellement créée.
*/
cairo_surface_t *cairo_dock_create_surface_for_icon (const gchar *cImagePath, cairo_t* pSourceContext, double fImageWidth, double fImageHeight);
/**
* Cree une surface à partir d'une image au format quelconque, a la taille donnée carrée, sans zoom.
*@param cImagePath le chemin complet de l'image.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param fImageSize la taille de l'icone.
*@returns la surface nouvellement créée.
*/
#define cairo_dock_create_surface_for_square_icon(cImagePath, pSourceContext, fImageSize) cairo_dock_create_surface_for_icon (cImagePath, pSourceContext, fImageSize, fImageSize)


/**
* Cree une surface par rotation d'une autre. N'est couramment utilisée que pour des rotations de quarts de tour.
*@param pSurface surface à faire tourner.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param fImageWidth la largeur de la surface.
*@param fImageHeight la hauteur de la surface.
*@param fRotationAngle l'angle de rotation à appliquer.
*@returns la surface nouvellement créée.
*/
cairo_surface_t * cairo_dock_rotate_surface (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fImageWidth, double fImageHeight, double fRotationAngle);

/**
* Cree une surface par réflection d'une autre. Applique un dégradé de transparence. La taille du reflet est déterminé par la config du dock, tandis que sa position est fonction de l'orientation de l'icône. Si l'icône change de container, il faut donc recreer le reflet.
*@param pSurface surface à réfléchir.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param fImageWidth la largeur de la surface.
*@param fImageHeight la hauteur de la surface.
*@param bHorizontalDock TRUE ssi la surface est à l'horizontal.
*@param fMaxScale facteur de zoom max qui sera appliqué à la surface.
*@param bDirectionUp TRUE ssi la surface a la tête en haut.
*@returns la surface nouvellement créée.
*/
cairo_surface_t * cairo_dock_create_reflection_surface (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fImageWidth, double fImageHeight, gboolean bHorizontalDock, double fMaxScale, gboolean bDirectionUp);

/**
* Cree une surface contenant une surface accolée à sa réflection. 
*@param pIconSurface la surface de l'icône.
*@param pReflectionSurface la surface du reflet.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param fImageWidth la largeur de la surface de l'icône.
*@param fImageHeight la hauteur de la surface de l'icône.
*@param bHorizontalDock TRUE ssi l'icône est à l'horizontal.
*@param fMaxScale facteur de zoom max qui sera appliqué à l'icône.
*@param bDirectionUp TRUE ssi l'icône a la tête en haut.
*@returns la surface nouvellement créée.
*/
cairo_surface_t * cairo_dock_create_icon_surface_with_reflection (cairo_surface_t *pIconSurface, cairo_surface_t *pReflectionSurface, cairo_t *pSourceContext, double fImageWidth, double fImageHeight, gboolean bHorizontalDock,double fMaxScale, gboolean bDirectionUp);

/**
* Cree une surface contenant un texte, avec une couleur de fond optionnel.
*@param cText le texte.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param pLabelDescription la description du texte.
*@param fMaxScale facteur de zoom max qui sera appliqué au texte.
*@param iTextWidth sera renseigne avec la largeur de la surface obtenue.
*@param iTextHeight sera renseigne avec la hauteur de la surface obtenue.
*@param fTextXOffset sera renseigne avec le décalage horizontal à appliquer pour centrer le texte horizontalement.
*@param fTextYOffset sera renseigne avec le décalage vertical à appliquer pour placer le texte verticalament.
*@returns la surface nouvellement créée.
*/
cairo_surface_t *cairo_dock_create_surface_from_text_full (gchar *cText, cairo_t *pSourceContext, CairoDockLabelDescription *pLabelDescription, double fMaxScale, int iMaxWidth, int *iTextWidth, int *iTextHeight, double *fTextXOffset, double *fTextYOffset);
#define cairo_dock_create_surface_from_text(cText, pSourceContext, pLabelDescription, fMaxScale, iTextWidth, iTextHeight, fTextXOffset, fTextYOffset) cairo_dock_create_surface_from_text_full (cText, pSourceContext, pLabelDescription, fMaxScale, 0, iTextWidth, iTextHeight, fTextXOffset, fTextYOffset) 

/**
* Cree une surface à l'identique d'une autre, en la redimensionnant eventuellement.
*@param pSurface surface à dupliquer.
*@param pSourceContext un contexte (non modifié par la fonction).
*@param fWidth la largeur de la surface.
*@param fHeight la hauteur de la surface.
*@param fDesiredWidth largeur désirée de la copie (0 pour garder la même taille).
*@param fDesiredHeight hauteur désirée de la copie (0 pour garder la même taille).
*@returns la surface nouvellement créée.
*/
cairo_surface_t * cairo_dock_duplicate_surface (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fWidth, double fHeight, double fDesiredWidth, double fDesiredHeight);


G_END_DECLS
#endif


#ifndef __CAIRO_DOCK_SURFACE_FACTORY__
#define  __CAIRO_DOCK_SURFACE_FACTORY__

#include <glib.h>
#include <gdk/gdk.h>
#include <cairo.h>

#include <cairo-dock-struct.h>
G_BEGIN_DECLS

/**
*@file cairo-dock-surface-factory.h This class contains functions to load any image/X buffer/GdkPixbuf/text into a cairo-surface.
* The loading of an image can be modified by a mask, to take into account the ratio, zoom, orientation, etc.
*
* The general way to load an image is by using \ref cairo_dock_create_surface_from_image.
* 
* If you just want to load an image at a given size, use \ref cairo_dock_create_surface_from_image_simple, or \ref cairo_dock_create_surface_from_icon.
* 
* To load a text, use \ref cairo_dock_create_surface_from_text.
*
*/

/// Types of image loading modifiers.
typedef enum {
	/// fill the space, with transparency if necessary.
	CAIRO_DOCK_FILL_SPACE 			= 1<<0,
	/// keep the ratio of the original image.
	CAIRO_DOCK_KEEP_RATIO 			= 1<<1,
	/// don't zoom in the image if the final surface is larger than the original image.
	CAIRO_DOCK_DONT_ZOOM_IN 		= 1<<2,
	/// orientation horizontal flip
	CAIRO_DOCK_ORIENTATION_HFLIP 		= 1<<3,
	/// orientation 180° rotation
	CAIRO_DOCK_ORIENTATION_ROT_180 	= 2<<3,
	/// orientation vertical flip
	CAIRO_DOCK_ORIENTATION_VFLIP 		= 3<<3,
	/// orientation 90° rotation + horizontal flip
	CAIRO_DOCK_ORIENTATION_ROT_90_HFLIP = 4<<3,
	/// orientation 90° rotation
	CAIRO_DOCK_ORIENTATION_ROT_90 	= 5<<3,
	/// orientation 90° rotation + vertical flip
	CAIRO_DOCK_ORIENTATION_ROT_90_VFLIP = 6<<3,
	/// orientation 270° rotation
	CAIRO_DOCK_ORIENTATION_ROT_270 	= 7<<3
	} CairoDockLoadImageModifier;
/// mask to get the orientation from a CairoDockLoadImageModifier.
#define CAIRO_DOCK_ORIENTATION_MASK (7<<3)

/// Description of the rendering of a text.
struct _CairoDockLabelDescription {
	/// font size (also approximately the resulting size in pixels)
	gint iSize;
	/// font.
	gchar *cFont;
	/// text weight. The higher, the thicker the strokes are.
	PangoWeight iWeight;
	/// text style (italic or normal).
	PangoStyle iStyle;
	/// first color of the characters.
	gdouble fColorStart[3];
	/// second color of the characters. If different from the first one, it will make a gradation.
	gdouble fColorStop[3];
	/// TRUE if the gradation is vertical (from top to bottom).
	gboolean bVerticalPattern;
	/// frame background color. Set the alpha channel to 0 to not draw a frame in the background.
	gdouble fBackgroundColor[4];
	/// TRUE to stroke the outline of the characters (in black).
	gboolean bOutlined;
	/// margin around the text, it is also the dimension of the frame if available.
	gint iMargin;
};


/* Calcule la taille d'une image selon une contrainte en largeur et hauteur de manière à remplir l'espace donné.
*@param fImageWidth the width of the image. Contient initialement the width of the image, et sera écrasée avec la largeur obtenue.
*@param fImageHeight the height of the image. Contient initialement the height of the image, et sera écrasée avec la hauteur obtenue.
*@param iWidthConstraint contrainte en largeur (0 <=> pas de contrainte).
*@param iHeightConstraint contrainte en hauteur (0 <=> pas de contrainte).
*@param bNoZoomUp TRUE ssi on ne doit pas agrandir the image (seulement la rétrécir).
*@param fZoomWidth sera renseigné avec le facteur de zoom en largeur qui a été appliqué.
*@param fZoomHeight sera renseigné avec le facteur de zoom en hauteur qui a été appliqué.
*/
void cairo_dock_calculate_size_fill (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, gboolean bNoZoomUp, double *fZoomWidth, double *fZoomHeight);

/* Calcule la taille d'une image selon une contrainte en largeur et hauteur en gardant le ratio hauteur/largeur constant.
*@param fImageWidth the width of the image. Contient initialement the width of the image, et sera écrasée avec la largeur obtenue.
*@param fImageHeight the height of the image. Contient initialement the height of the image, et sera écrasée avec la hauteur obtenue.
*@param iWidthConstraint contrainte en largeur (0 <=> pas de contrainte).
*@param iHeightConstraint contrainte en hauteur (0 <=> pas de contrainte).
*@param bNoZoomUp TRUE ssi on ne doit pas agrandir the image (seulement la rétrécir).
*@param fZoom sera renseigné avec le facteur de zoom qui a été appliqué.
*/
void cairo_dock_calculate_size_constant_ratio (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, gboolean bNoZoomUp, double *fZoom);

/** Calculate the size of an image according to a constraint on width and height, and a loading modifier.
*@param fImageWidth pointer to the width of the image. Initially contains the width of the original image, and is updated with the resulting width.
*@param fImageHeight pointer to the height of the image. Initially contains the height of the original image, and is updated with the resulting height.
*@param iWidthConstraint constraint on width (0 <=> no constraint).
*@param iHeightConstraint constraint on height (0 <=> no constraint).
*@param iLoadingModifier a mask of different loading modifiers.
*@param fZoomWidth will be filled with the zoom that has been applied on width.
*@param fZoomHeight will be filled with the zoom that has been applied on height.
*/
void cairo_dock_calculate_constrainted_size (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fZoomWidth, double *fZoomHeight);

/** Create a surface from raw data of an X icon. The biggest icon possible is taken. The ratio is kept, and the surface will fill the space with transparency if necessary.
*@param pXIconBuffer raw data of the icon.
*@param iBufferNbElements number of elements in the buffer.
*@param pSourceContext a drawing context (not altered by the function).
*@param fMaxScale maximum zoom of the icon.
*@param fWidth will be filled with the resulting width of the surface.
*@param fHeight will be filled with the resulting height of the surface.
*@return the newly allocated surface.
*/
cairo_surface_t *cairo_dock_create_surface_from_xicon_buffer (gulong *pXIconBuffer, int iBufferNbElements, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight);

/** Create a surface from a GdkPixbuf.
*@param pixbuf the pixbuf.
*@param pSourceContext a drawing context (not altered by the function).
*@param fMaxScale maximum zoom of the icon.
*@param iWidthConstraint constraint on the width, or 0 to not constraint it.
*@param iHeightConstraint constraint on the height, or 0 to not constraint it.
*@param iLoadingModifier a mask of different loading modifiers.
*@param fImageWidth will be filled with the resulting width of the surface (hors zoom).
*@param fImageHeight will be filled with the resulting height of the surface (hors zoom).
*@param fZoomX if non NULL, will be filled with the zoom that has been applied on width.
*@param fZoomY if non NULL, will be filled with the zoom that has been applied on width.
*@return the newly allocated surface.
*/
cairo_surface_t *cairo_dock_create_surface_from_pixbuf (GdkPixbuf *pixbuf, cairo_t *pSourceContext, double fMaxScale, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fImageWidth, double *fImageHeight, double *fZoomX, double *fZoomY);


/** Create an empty surface (transparent) of a given size. In OpenGL mode, this surface can act as a buffer to generate a texture.
*@param pSourceContext un contexte cairo, ou NULL pour creer une surface tampon.
*@param iWidth width of the surface.
*@param iHeight height of the surface.
*@return the newly allocated surface.
*/
cairo_surface_t *_cairo_dock_create_blank_surface (cairo_t *pSourceContext, int iWidth, int iHeight);

/** Create a surface from any image.
*@param cImagePath complete path to the image.
*@param pSourceContext a drawing context (not altered by the function).
*@param fMaxScale maximum zoom of the icon.
*@param iWidthConstraint constraint on the width, or 0 to not constraint it.
*@param iHeightConstraint constraint on the height, or 0 to not constraint it.
*@param iLoadingModifier a mask of different loading modifiers.
*@param fImageWidth will be filled with the resulting width of the surface (hors zoom).
*@param fImageHeight will be filled with the resulting height of the surface (hors zoom).
*@param fZoomX if non NULL, will be filled with the zoom that has been applied on width.
*@param fZoomY if non NULL, will be filled with the zoom that has been applied on width.
*@return the newly allocated surface.
*/
cairo_surface_t *cairo_dock_create_surface_from_image (const gchar *cImagePath, cairo_t* pSourceContext, double fMaxScale, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fImageWidth, double *fImageHeight, double *fZoomX, double *fZoomY);

/** Create a surface from any image, at a given size. If the image is given by its sole name, it is searched inside the current theme root folder.
*@param cImageFile path or name of an image.
*@param pSourceContext a drawing context (not altered by the function).
*@param fImageWidth the desired surface width.
*@param fImageHeight the desired surface height.
*@return the newly allocated surface.
*/
cairo_surface_t *cairo_dock_create_surface_from_image_simple (const gchar *cImageFile, cairo_t* pSourceContext, double fImageWidth, double fImageHeight);

/** Create a surface from any image, at a given size. If the image is given by its sole name, it is searched inside the icons themes known by Cairo-Dock. 
*@param cImagePath path or name of an image.
*@param pSourceContext a drawing context (not altered by the function).
*@param fImageWidth the desired surface width.
*@param fImageHeight the desired surface height.
*@return the newly allocated surface.
*/
cairo_surface_t *cairo_dock_create_surface_from_icon (const gchar *cImagePath, cairo_t* pSourceContext, double fImageWidth, double fImageHeight);
#define cairo_dock_create_surface_for_icon cairo_dock_create_surface_from_icon

/** Create a square surface from any image, at a given size. If the image is given by its sole name, it is searched inside the icons themes known by Cairo-Dock.
*@param cImagePath path or name of an image.
*@param pSourceContext a drawing context (not altered by the function).
*@param fImageSize the desired surface size.
*@return the newly allocated surface.
*/
#define cairo_dock_create_surface_for_square_icon(cImagePath, pSourceContext, fImageSize) cairo_dock_create_surface_for_icon (cImagePath, pSourceContext, fImageSize, fImageSize)


/** Create a surface by rotating another. Only works for 1/4 of rounds.
*@param pSurface surface to rotate.
*@param pSourceContext a drawing context (not altered by the function).
*@param fImageWidth the width of the surface.
*@param fImageHeight the height of the surface.
*@param fRotationAngle rotation angle to apply, in radians.
*@return the newly allocated surface.
*/
cairo_surface_t * cairo_dock_rotate_surface (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fImageWidth, double fImageHeight, double fRotationAngle);

/** Create a surface by reflection of another. Apply a transparency gradation. The size of the reflect is given by the global config,and its position if given by the orientation of the icon; if this changes, the reflect needs to be re-created.
*@param pSurface surface to reflect.
*@param pSourceContext a drawing context (not altered by the function).
*@param fImageWidth the width of the surface.
*@param fImageHeight the height of the surface.
*@param bHorizontalDock TRUE if the surface is in an horizontal container.
*@param fMaxScale maximum zoom of the surface.
*@param bDirectionUp TRUE if the surface is in a container whose direction is towards.
*@return the newly allocated surface.
*/
cairo_surface_t * cairo_dock_create_reflection_surface (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fImageWidth, double fImageHeight, gboolean bHorizontalDock, double fMaxScale, gboolean bDirectionUp);

/** Create a surface representing a text, according to a given text description.
*@param cText the text.
*@param pSourceContext a drawing context (not altered by the function).
*@param pLabelDescription description of the text rendering.
*@param fMaxScale maximum zoom of the text.
*@param iMaxWidth maximum authorized width for the surface; it will be zoomed in to fits this limit. 0 for no limit.
*@param iTextWidth will be filled the width of the resulting surface.
*@param iTextHeight will be filled the height of the resulting surface.
*@param fTextXOffset if non NULL, will be filled the horizontal offset to apply to center the text horizontally.
*@param fTextYOffset if non NULL, will be filled the vertical offset to apply to center the text vertically.
*@return the newly allocated surface.
*/
cairo_surface_t *cairo_dock_create_surface_from_text_full (const gchar *cText, cairo_t *pSourceContext, CairoDockLabelDescription *pLabelDescription, double fMaxScale, int iMaxWidth, int *iTextWidth, int *iTextHeight, double *fTextXOffset, double *fTextYOffset);

/** Create a surface representing a text, according to a given text description.
*@param cText the text.
*@param pSourceContext a drawing context (not altered by the function).
*@param pLabelDescription description of the text rendering.
*@param iTextWidthPtr will be filled the width of the resulting surface.
*@param iTextHeightPtr will be filled the height of the resulting surface.
*@return the newly allocated surface.
*/
#define cairo_dock_create_surface_from_text(cText, pSourceContext, pLabelDescription, iTextWidthPtr, iTextHeightPtr) cairo_dock_create_surface_from_text_full (cText, pSourceContext, pLabelDescription, 1., 0, iTextWidthPtr, iTextHeightPtr, NULL, NULL) 

/** Create a surface identical to another, possibly resizing it.
*@param pSurface surface to duplicate.
*@param pSourceContext a drawing context (not altered by the function).
*@param fWidth the width of the surface.
*@param fHeight the height of the surface.
*@param fDesiredWidth desired width of the copy (0 to keep the same size).
*@param fDesiredHeight desired height of the copy (0 to keep the same size).
*@return the newly allocated surface.
*/
cairo_surface_t * cairo_dock_duplicate_surface (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fWidth, double fHeight, double fDesiredWidth, double fDesiredHeight);


G_END_DECLS
#endif

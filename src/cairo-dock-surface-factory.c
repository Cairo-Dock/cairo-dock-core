/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>
#include <math.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>
#include <pango/pango.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-log.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-labels.h"

extern gboolean g_bUseOpenGL;


void cairo_dock_calculate_size_fill (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, gboolean bNoZoomUp, double *fZoomWidth, double *fZoomHeight)
{
	if (iWidthConstraint != 0)
	{
		*fZoomWidth = 1. * iWidthConstraint / (*fImageWidth);
		if (bNoZoomUp && *fZoomWidth > 1)
			*fZoomWidth = 1;
		else
			*fImageWidth = (double) iWidthConstraint;
	}
	else
		*fZoomWidth = 1.;
	if (iHeightConstraint != 0)
	{
		*fZoomHeight = 1. * iHeightConstraint / (*fImageHeight);
		if (bNoZoomUp && *fZoomHeight > 1)
			*fZoomHeight = 1;
		else
			*fImageHeight = (double) iHeightConstraint;
	}
	else
		*fZoomHeight = 1.;
}

void cairo_dock_calculate_size_constant_ratio (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, gboolean bNoZoomUp, double *fZoom)
{
	if (iWidthConstraint != 0 && iHeightConstraint != 0)
		*fZoom = MIN (iWidthConstraint / (*fImageWidth), iHeightConstraint / (*fImageHeight));
	else if (iWidthConstraint != 0)
		*fZoom = iWidthConstraint / (*fImageWidth);
	else if (iHeightConstraint != 0)
		*fZoom = iHeightConstraint / (*fImageHeight);
	else
		*fZoom = 1.;
	if (bNoZoomUp && *fZoom > 1)
		*fZoom = 1.;
	*fImageWidth = (*fImageWidth) * (*fZoom);
	*fImageHeight = (*fImageHeight) * (*fZoom);
}



void cairo_dock_calculate_constrainted_size (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fZoomWidth, double *fZoomHeight)
{
	gboolean bFillSpace = iLoadingModifier & CAIRO_DOCK_FILL_SPACE;
	gboolean bKeepRatio = iLoadingModifier & CAIRO_DOCK_KEEP_RATIO;
	gboolean bNoZoomUp = iLoadingModifier & CAIRO_DOCK_DONT_ZOOM_IN;
	gint iOrientation = iLoadingModifier & CAIRO_DOCK_ORIENTATION_MASK;
	if (iOrientation > CAIRO_DOCK_ORIENTATION_VFLIP)  // inversion x/y
	{
		double tmp = *fImageWidth;
		*fImageWidth = *fImageHeight;
		*fImageHeight = tmp;
	}
	if (bKeepRatio)
	{
		cairo_dock_calculate_size_constant_ratio (fImageWidth,
			fImageHeight,
			iWidthConstraint,
			iHeightConstraint,
			bNoZoomUp,
			fZoomWidth);
		*fZoomHeight = *fZoomWidth;
		if (bFillSpace)
		{
			//double fUsefulWidth = *fImageWidth;
			//double fUsefulHeight = *fImageHeight;
			if (iWidthConstraint != 0)
				*fImageWidth = iWidthConstraint;
			if (iHeightConstraint != 0)
				*fImageHeight = iHeightConstraint;
		}
	}
	else
	{
		cairo_dock_calculate_size_fill (fImageWidth,
			fImageHeight,
			iWidthConstraint,
			iHeightConstraint,
			bNoZoomUp,
			fZoomWidth,
			fZoomHeight);
	}
}

cairo_surface_t *_cairo_dock_create_blank_surface (cairo_t *pSourceContext, int iWidth, int iHeight)
{
	if (pSourceContext != NULL && ! g_bUseOpenGL)
		return cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			iWidth,
			iHeight);
	else
		return cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
			iWidth,
			iHeight);
}

cairo_surface_t *cairo_dock_create_surface_from_xicon_buffer (gulong *pXIconBuffer, int iBufferNbElements, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight)
{
	g_return_val_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);

	//\____________________ On recupere la plus grosse des icones presentes dans le tampon (meilleur rendu).
	int iIndex = 0, iBestIndex = 0;
	while (iIndex + 2 < iBufferNbElements)
	{
		if (pXIconBuffer[iIndex] > pXIconBuffer[iBestIndex])
			iBestIndex = iIndex;
		iIndex += 2 + pXIconBuffer[iIndex] * pXIconBuffer[iIndex+1];
	}

	//\____________________ On pre-multiplie chaque composante par le alpha (necessaire pour libcairo).
	*fWidth = (double) pXIconBuffer[iBestIndex];
	*fHeight = (double) pXIconBuffer[iBestIndex+1];

	int i;
	gint pixel, alpha, red, green, blue;
	float fAlphaFactor;
	gint *pPixelBuffer = (gint *) &pXIconBuffer[iBestIndex+2];  // on va ecrire le resultat du filtre directement dans le tableau fourni en entree. C'est ok car sizeof(gulong) >= sizeof(gint), donc le tableau de pixels est plus petit que le buffer fourni en entree. merci a Hannemann pour ses tests et ses screenshots ! :-)
	for (i = 0; i < (int) (*fHeight) * (*fWidth); i ++)
	{
		pixel = (gint) pXIconBuffer[iBestIndex+2+i];
		alpha = (pixel & 0xFF000000) >> 24;
		red   = (pixel & 0x00FF0000) >> 16;
		green = (pixel & 0x0000FF00) >> 8;
		blue  = (pixel & 0x000000FF);
		fAlphaFactor = (float) alpha / 255;
		red *= fAlphaFactor;
		green *= fAlphaFactor;
		blue *= fAlphaFactor;
		pPixelBuffer[i] = (pixel & 0xFF000000) + (red << 16) + (green << 8) + blue;
	}

	//\____________________ On cree la surface a partir du tampon.
	int iStride = (int) (*fWidth) * sizeof (gint);  // nbre d'octets entre le debut de 2 lignes.
	cairo_surface_t *surface_ini = cairo_image_surface_create_for_data ((guchar *)pPixelBuffer,
		CAIRO_FORMAT_ARGB32,
		(int) pXIconBuffer[iBestIndex],
		(int) pXIconBuffer[iBestIndex+1],
		(int) iStride);

	double fIconWidthSaturationFactor, fIconHeightSaturationFactor;
	cairo_dock_calculate_size_fill (fWidth,
		fHeight,
		myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI],
		myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI],
		FALSE,
		&fIconWidthSaturationFactor,
		&fIconHeightSaturationFactor);

	cairo_surface_t *pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
		ceil (*fWidth * fMaxScale),
		ceil (*fHeight * fMaxScale));
	cairo_t *pCairoContext = cairo_create (pNewSurface);

	cairo_scale (pCairoContext, fMaxScale * fIconWidthSaturationFactor, fMaxScale * fIconHeightSaturationFactor);
	cairo_set_source_surface (pCairoContext, surface_ini, 0, 0);
	cairo_paint (pCairoContext);

	cairo_surface_destroy (surface_ini);
	cairo_destroy (pCairoContext);

	return pNewSurface;
}

static inline void _apply_orientation_and_scale (cairo_t *pCairoContext, CairoDockLoadImageModifier iLoadingModifier, double fImageWidth, double fImageHeight, double fZoomX, double fZoomY, double fUsefulWidth, double fUsefulheight)
{
	int iOrientation = iLoadingModifier & CAIRO_DOCK_ORIENTATION_MASK;
	
	//if (iOrientation != 0)
	{
		cairo_translate (pCairoContext,
			fImageWidth/2,
			fImageHeight/2);
		cairo_scale (pCairoContext,
			fZoomX,
			fZoomY);
		switch (iOrientation)
		{
			case CAIRO_DOCK_ORIENTATION_HFLIP :
				cairo_scale (pCairoContext, -1., 1.);
			break ;
			case CAIRO_DOCK_ORIENTATION_ROT_180 :
				cairo_rotate (pCairoContext, G_PI);
			break ;
			case CAIRO_DOCK_ORIENTATION_VFLIP :
				cairo_scale (pCairoContext, 1., -1.);
			break ;
			case CAIRO_DOCK_ORIENTATION_ROT_90_HFLIP :
				cairo_scale (pCairoContext, -1., 1.);
				cairo_rotate (pCairoContext, - G_PI/2);
			break ;
			case CAIRO_DOCK_ORIENTATION_ROT_90 :
				cairo_rotate (pCairoContext, - G_PI/2);
			break ;
			case CAIRO_DOCK_ORIENTATION_ROT_90_VFLIP :
				cairo_scale (pCairoContext, 1., -1.);
				cairo_rotate (pCairoContext, - G_PI/2);
			break ;
			case CAIRO_DOCK_ORIENTATION_ROT_270 :
				cairo_rotate (pCairoContext, G_PI/2);
			break ;
			default :
			break ;
		}
		if (iOrientation < CAIRO_DOCK_ORIENTATION_ROT_90_HFLIP)
			cairo_translate (pCairoContext,
				- fUsefulWidth/2/fZoomX,
				- fUsefulheight/2/fZoomY);
		else
			cairo_translate (pCairoContext,
				- fUsefulheight/2/fZoomY,
				- fUsefulWidth/2/fZoomX);
	}
	/*else
	{
		cairo_scale (pCairoContext,
			fZoomX,
			fZoomY);
	}*/
}

cairo_surface_t *cairo_dock_create_surface_from_pixbuf (GdkPixbuf *pixbuf, cairo_t *pSourceContext, double fMaxScale, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fImageWidth, double *fImageHeight, double *fZoomX, double *fZoomY)
{
	*fImageWidth = gdk_pixbuf_get_width (pixbuf);
	*fImageHeight = gdk_pixbuf_get_height (pixbuf);
	double fIconWidthSaturationFactor = 1., fIconHeightSaturationFactor = 1.;
	cairo_dock_calculate_constrainted_size (fImageWidth,
			fImageHeight,
			iWidthConstraint,
			iHeightConstraint,
			iLoadingModifier,
			&fIconWidthSaturationFactor,
			&fIconHeightSaturationFactor);

	GdkPixbuf *pPixbufWithAlpha = pixbuf;
	if (! gdk_pixbuf_get_has_alpha (pixbuf))  // on lui rajoute un canal alpha s'il n'en a pas.
	{
		//g_print ("  ajout d'un canal alpha\n");
		pPixbufWithAlpha = gdk_pixbuf_add_alpha (pixbuf, FALSE, 255, 255, 255);  // TRUE <=> les pixels blancs deviennent transparents.
	}

	//\____________________ On pre-multiplie chaque composante par le alpha (necessaire pour libcairo).
	int iNbChannels = gdk_pixbuf_get_n_channels (pPixbufWithAlpha);
	int iRowstride = gdk_pixbuf_get_rowstride (pPixbufWithAlpha);
	int w = gdk_pixbuf_get_width (pPixbufWithAlpha);
	guchar *p, *pixels = gdk_pixbuf_get_pixels (pPixbufWithAlpha);
	int h = gdk_pixbuf_get_height (pPixbufWithAlpha);
	int x, y;
	int red, green, blue;
	float fAlphaFactor;
	for (y = 0; y < h; y ++)
	{
		for (x = 0; x < w; x ++)
		{
			p = pixels + y * iRowstride + x * iNbChannels;
			fAlphaFactor = (float) p[3] / 255;
			red = p[0] * fAlphaFactor;
			green = p[1] * fAlphaFactor;
			blue = p[2] * fAlphaFactor;
			p[0] = blue;
			p[1] = green;
			p[2] = red;
		}
	}
	
	cairo_surface_t *surface_ini = cairo_image_surface_create_for_data (pixels,
		CAIRO_FORMAT_ARGB32,
		w,
		h,
		iRowstride);

	cairo_surface_t *pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
		ceil ((*fImageWidth) * fMaxScale),
		ceil ((*fImageHeight) * fMaxScale));
	cairo_t *pCairoContext = cairo_create (pNewSurface);
	
	double fUsefulWidth = w * fIconWidthSaturationFactor;  // a part dans le cas fill && keep ratio, c'est la meme chose que fImageWidth et fImageHeight.
	double fUsefulHeight = h * fIconHeightSaturationFactor;
	_apply_orientation_and_scale (pCairoContext,
		iLoadingModifier,
		ceil ((*fImageWidth) * fMaxScale), ceil ((*fImageHeight) * fMaxScale),
		fMaxScale * fIconWidthSaturationFactor, fMaxScale * fIconHeightSaturationFactor,
		fUsefulWidth * fMaxScale, fUsefulHeight * fMaxScale);
	
	cairo_set_source_surface (pCairoContext, surface_ini, 0, 0);
	cairo_paint (pCairoContext);
	
	cairo_destroy (pCairoContext);
	cairo_surface_destroy (surface_ini);
	if (pPixbufWithAlpha != pixbuf)
		g_object_unref (pPixbufWithAlpha);
	
	if (fZoomX != NULL)
		*fZoomX = fIconWidthSaturationFactor;
	if (fZoomY != NULL)
		*fZoomY = fIconHeightSaturationFactor;
	
	return pNewSurface;
}


cairo_surface_t *cairo_dock_create_surface_from_image (const gchar *cImagePath, cairo_t* pSourceContext, double fMaxScale, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fImageWidth, double *fImageHeight, double *fZoomX, double *fZoomY)
{
	//g_print ("%s (%s, %dx%dx%.2f, %d)\n", __func__, cImagePath, iWidthConstraint, iHeightConstraint, fMaxScale, iLoadingModifier);
	g_return_val_if_fail (cImagePath != NULL && pSourceContext != NULL && cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	GError *erreur = NULL;
	RsvgDimensionData rsvg_dimension_data;
	RsvgHandle *rsvg_handle = NULL;
	cairo_surface_t* surface_ini;
	cairo_surface_t* pNewSurface = NULL;
	cairo_t* pCairoContext = NULL;
	double fIconWidthSaturationFactor = 1.;
	double fIconHeightSaturationFactor = 1.;
	
	//\_______________ On cherche a determiner le type de l'image. En effet, les SVG et les PNG sont charges differemment des autres.
	gboolean bIsSVG = FALSE, bIsPNG = FALSE, bIsXPM = FALSE;
	FILE *fd = fopen (cImagePath, "r");
	if (fd != NULL)
	{
		char buffer[8];
		if (fgets (buffer, 7, fd) != NULL)
		{
			if (strncmp (buffer+2, "xml", 3) == 0)
				bIsSVG = TRUE;
			else if (strncmp (buffer+1, "PNG", 3) == 0)
				bIsPNG = TRUE;
			else if (strncmp (buffer+3, "XPM", 3) == 0)
				bIsXPM = TRUE;
			cd_debug ("  format : %d;%d;%d", bIsSVG, bIsPNG, bIsXPM);
		}
		fclose (fd);
	}
	if (! bIsSVG && ! bIsPNG && ! bIsXPM)  // sinon en desespoir de cause on se base sur l'extension.
	{
		cd_debug ("  on se base sur l'extension en desespoir de cause.");
		if (g_str_has_suffix (cImagePath, ".svg"))
			bIsSVG = TRUE;
		else if (g_str_has_suffix (cImagePath, ".png"))
			bIsPNG = TRUE;
	}
	
	bIsPNG = FALSE;  /// libcairo 1.6 - 1.8 est bugguee !!!...
	if (bIsSVG)
	{
		rsvg_handle = rsvg_handle_new_from_file (cImagePath, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			return NULL;
		}
		else
		{
			g_return_val_if_fail (rsvg_handle != NULL, NULL);
			rsvg_handle_get_dimensions (rsvg_handle, &rsvg_dimension_data);
			int w = rsvg_dimension_data.width;
			int h = rsvg_dimension_data.height;
			*fImageWidth = (gdouble) w;
			*fImageHeight = (gdouble) h;
			//g_print ("%.2fx%.2f\n", *fImageWidth, *fImageHeight);
			cairo_dock_calculate_constrainted_size (fImageWidth,
				fImageHeight,
				iWidthConstraint,
				iHeightConstraint,
				iLoadingModifier,
				&fIconWidthSaturationFactor,
				&fIconHeightSaturationFactor);
			
			pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
				ceil ((*fImageWidth) * fMaxScale),
				ceil ((*fImageHeight) * fMaxScale));

			pCairoContext = cairo_create (pNewSurface);
			double fUsefulWidth = w * fIconWidthSaturationFactor;  // a part dans le cas fill && keep ratio, c'est la meme chose que fImageWidth et fImageHeight.
			double fUsefulHeight = h * fIconHeightSaturationFactor;
			_apply_orientation_and_scale (pCairoContext,
				iLoadingModifier,
				ceil ((*fImageWidth) * fMaxScale), ceil ((*fImageHeight) * fMaxScale),
				fMaxScale * fIconWidthSaturationFactor, fMaxScale * fIconHeightSaturationFactor,
				fUsefulWidth * fMaxScale, fUsefulHeight * fMaxScale);

			rsvg_handle_render_cairo (rsvg_handle, pCairoContext);
			cairo_destroy (pCairoContext);
			g_object_unref (rsvg_handle);
		}
	}
	else if (bIsPNG)
	{
		surface_ini = cairo_image_surface_create_from_png (cImagePath);  // cree un fond noir :-(
		if (cairo_surface_status (surface_ini) == CAIRO_STATUS_SUCCESS)
		{
			int w = cairo_image_surface_get_width (surface_ini);
			int h = cairo_image_surface_get_height (surface_ini);
			*fImageWidth = (double) w;
			*fImageHeight = (double) h;
			cairo_dock_calculate_constrainted_size (fImageWidth,
				fImageHeight,
				iWidthConstraint,
				iHeightConstraint,
				iLoadingModifier,
				&fIconWidthSaturationFactor,
				&fIconHeightSaturationFactor);
			
			pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
				ceil ((*fImageWidth) * fMaxScale),
				ceil ((*fImageHeight) * fMaxScale));
			pCairoContext = cairo_create (pNewSurface);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
			cairo_set_source_rgba (pCairoContext, 0., 0., 0., 0.);
			cairo_paint (pCairoContext);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
			
			double fUsefulWidth = w * fIconWidthSaturationFactor;  // a part dans le cas fill && keep ratio, c'est la meme chose que fImageWidth et fImageHeight.
			double fUsefulHeight = h * fIconHeightSaturationFactor;
			_apply_orientation_and_scale (pCairoContext,
				iLoadingModifier,
				ceil ((*fImageWidth) * fMaxScale), ceil ((*fImageHeight) * fMaxScale),
				fMaxScale * fIconWidthSaturationFactor, fMaxScale * fIconHeightSaturationFactor,
				fUsefulWidth * fMaxScale, fUsefulHeight * fMaxScale);
			
			cairo_set_source_surface (pCairoContext, surface_ini, 0, 0);
			cairo_paint (pCairoContext);
			cairo_destroy (pCairoContext);
		}
		cairo_surface_destroy (surface_ini);
	}
	else  // le code suivant permet de charger tout type d'image, mais en fait c'est un peu idiot d'utiliser des icones n'ayant pas de transparence.
	{
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (cImagePath, &erreur);  // semble se baser sur l'extension pour definir le type !
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
			return NULL;
		}
		pNewSurface = cairo_dock_create_surface_from_pixbuf (pixbuf,
			pSourceContext,
			fMaxScale,
			iWidthConstraint,
			iHeightConstraint,
			iLoadingModifier,
			fImageWidth,
			fImageHeight,
			&fIconWidthSaturationFactor,
			&fIconHeightSaturationFactor);
		g_object_unref (pixbuf);
		
	}
	
	if (fZoomX != NULL)
		*fZoomX = fIconWidthSaturationFactor;
	if (fZoomY != NULL)
		*fZoomY = fIconHeightSaturationFactor;
	
	return pNewSurface;
}

cairo_surface_t *cairo_dock_create_surface_for_icon (const gchar *cImagePath, cairo_t* pSourceContext, double fImageWidth, double fImageHeight)
{
	double fImageWidth_ = fImageWidth, fImageHeight_ = fImageHeight;
	return cairo_dock_create_surface_from_image (cImagePath,
		pSourceContext,
		1.,
		fImageWidth,
		fImageHeight,
		CAIRO_DOCK_FILL_SPACE,
		&fImageWidth_,
		&fImageHeight_,
		NULL,
		NULL);
}



cairo_surface_t * cairo_dock_rotate_surface (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fImageWidth, double fImageHeight, double fRotationAngle)
{
	g_return_val_if_fail (pSourceContext != NULL && cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS && pSurface != NULL, NULL);
	if (fRotationAngle != 0)
	{
		cairo_surface_t *pNewSurfaceRotated;
		cairo_t *pCairoContext;
		if (fabs (fRotationAngle) > G_PI / 2)
		{
			pNewSurfaceRotated = _cairo_dock_create_blank_surface (pSourceContext,
				fImageWidth,
				fImageHeight);
			pCairoContext = cairo_create (pNewSurfaceRotated);

			cairo_translate (pCairoContext, 0, fImageHeight);
			cairo_scale (pCairoContext, 1, -1);
		}
		else
		{
			pNewSurfaceRotated = _cairo_dock_create_blank_surface (pSourceContext,
				fImageHeight,
				fImageWidth);
			pCairoContext = cairo_create (pNewSurfaceRotated);

			if (fRotationAngle < 0)
			{
				cairo_move_to (pCairoContext, fImageHeight, 0);
				cairo_rotate (pCairoContext, fRotationAngle);
				cairo_translate (pCairoContext, - fImageWidth, 0);
			}
			else
			{
				cairo_move_to (pCairoContext, 0, 0);
				cairo_rotate (pCairoContext, fRotationAngle);
				cairo_translate (pCairoContext, 0, - fImageHeight);
			}
		}
		cairo_set_source_surface (pCairoContext, pSurface, 0, 0);
		cairo_paint (pCairoContext);

		cairo_destroy (pCairoContext);
		return pNewSurfaceRotated;
	}
	else
	{
		return NULL;
	}
}


static cairo_surface_t * cairo_dock_create_reflection_surface_horizontal (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fImageWidth, double fImageHeight, double fMaxScale, gboolean bDirectionUp)
{
	g_return_val_if_fail (pSourceContext != NULL && cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	//g_print ("%s (%d)\n", __func__, bDirectionUp);

	//\_______________ On cree la surface d'une fraction hauteur de l'image originale.
	double fReflectHeight = myIcons.fReflectSize * fMaxScale;
	if (pSurface == NULL || fReflectHeight == 0 || myIcons.fAlbedo == 0)
		return NULL;
	cairo_surface_t *pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
		fImageWidth,
		fReflectHeight);
	cairo_t *pCairoContext = cairo_create (pNewSurface);
	
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba (pCairoContext, 0., 0., 0., 0.);
	
	//\_______________ On dessine l'image originale inversee.
	cairo_translate (pCairoContext, 0, fImageHeight);
	cairo_scale (pCairoContext, 1., -1.);
	cairo_set_source_surface (pCairoContext, pSurface, 0, (bDirectionUp ? 0 : fImageHeight - fReflectHeight));
	
	//\_______________ On applique un degrade en transparence.
	/**cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0.,
		2*fReflectHeight,
		0.,
		fReflectHeight);  // de haut en bas.*/
	cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0.,
		fImageHeight,
		0.,
		fImageHeight-fReflectHeight);  // de haut en bas.
	g_return_val_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS, NULL);

	cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		0.,
		0.,
		0.,
		0.,
		(bDirectionUp ? myIcons.fAlbedo : 0.));
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		1.,
		0.,
		0.,
		0.,
		(bDirectionUp ? 0 : myIcons.fAlbedo));

	cairo_mask (pCairoContext, pGradationPattern);

	cairo_pattern_destroy (pGradationPattern);
	cairo_destroy (pCairoContext);
	return pNewSurface;
}

static cairo_surface_t * cairo_dock_create_reflection_surface_vertical (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fImageWidth, double fImageHeight, double fMaxScale, gboolean bDirectionUp)
{
	g_return_val_if_fail (pSurface != NULL && pSourceContext != NULL && cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);

	//\_______________ On cree la surface d'une fraction hauteur de l'image originale.
	double fReflectWidth = myIcons.fReflectSize * fMaxScale;
	if (fReflectWidth == 0 || myIcons.fAlbedo == 0)
		return NULL;
	cairo_surface_t *pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
		fReflectWidth,
		fImageHeight);
	cairo_t *pCairoContext = cairo_create (pNewSurface);

	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba (pCairoContext, 0., 0., 0., 0.);
	
	//\_______________ On dessine l'image originale inversee.
	cairo_translate (pCairoContext, fImageWidth, 0);
	cairo_scale (pCairoContext, -1., 1.);
	cairo_set_source_surface (pCairoContext, pSurface, (bDirectionUp ? 0. : fImageHeight - fReflectWidth), 0.);
	
	//\_______________ On applique un degrade en transparence.
	cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0,
		0.,
		fImageHeight-fReflectWidth,
		0.);  // de gauche a droite.
	g_return_val_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS, NULL);

	cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_REPEAT);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		0.,
		0.,
		0.,
		0.,
		(bDirectionUp ? myIcons.fAlbedo : 0.));
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		1.,
		0.,
		0.,
		0.,
		(bDirectionUp ? 0. : myIcons.fAlbedo));

	cairo_mask (pCairoContext, pGradationPattern);

	cairo_pattern_destroy (pGradationPattern);
	cairo_destroy (pCairoContext);
	return pNewSurface;
}

cairo_surface_t * cairo_dock_create_reflection_surface (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fImageWidth, double fImageHeight, gboolean bHorizontalDock, double fMaxScale, gboolean bDirectionUp)
{
	if (bHorizontalDock)
		return cairo_dock_create_reflection_surface_horizontal (pSurface, pSourceContext, fImageWidth, fImageHeight, fMaxScale, bDirectionUp);
	else
		return cairo_dock_create_reflection_surface_vertical (pSurface, pSourceContext, fImageWidth, fImageHeight, fMaxScale, bDirectionUp);
}


cairo_surface_t *cairo_dock_create_surface_from_text_full (gchar *cText, cairo_t* pSourceContext, CairoDockLabelDescription *pLabelDescription, double fMaxScale, int iMaxWidth, int *iTextWidth, int *iTextHeight, double *fTextXOffset, double *fTextYOffset)
{
	g_return_val_if_fail (cText != NULL && pLabelDescription != NULL && pSourceContext != NULL && cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	
	//\_________________ On ecrit le texte dans un calque Pango.
	PangoLayout *pLayout = pango_cairo_create_layout (pSourceContext);
	
	PangoFontDescription *pDesc = pango_font_description_new ();
	pango_font_description_set_absolute_size (pDesc, fMaxScale * pLabelDescription->iSize * PANGO_SCALE);
	pango_font_description_set_family_static (pDesc, pLabelDescription->cFont);
	pango_font_description_set_weight (pDesc, pLabelDescription->iWeight);
	pango_font_description_set_style (pDesc, pLabelDescription->iStyle);
	pango_layout_set_font_description (pLayout, pDesc);
	pango_font_description_free (pDesc);
	
	pango_layout_set_text (pLayout, cText, -1);
	
	//\_________________ On cree une surface aux dimensions du texte.
	PangoRectangle ink, log;
	pango_layout_get_pixel_extents (pLayout, &ink, &log);
	
	int iOutlineMargin = 2*pLabelDescription->iMargin + (pLabelDescription->bOutlined ? 2 : 0);  // outlined => +1 tout autour des lettres.
	double fZoomX = ((iMaxWidth != 0 && ink.width + iOutlineMargin > iMaxWidth) ? 1.*iMaxWidth / (ink.width + iOutlineMargin) : 1.);
	
	*iTextWidth = (ink.width + iOutlineMargin) * fZoomX;
	*iTextHeight = ink.height + iOutlineMargin + 0; // +1 car certaines polices "debordent".
	//Test du zoom en W ET H *iTextHeight = (ink.height + 2 + 1) * fZoom; 
	
	cairo_surface_t* pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
		*iTextWidth,
		*iTextHeight);
	cairo_t* pCairoContext = cairo_create (pNewSurface);
	
	//\_________________ On dessine le fond.
	if (pLabelDescription->fBackgroundColor != NULL && pLabelDescription->fBackgroundColor[3] > 0)  // non transparent.
	{
		cairo_save (pCairoContext);
		//double fRadius = fMaxScale * MIN (.5 * myBackground.iDockRadius, 5.);  // bon compromis.
		double fRadius = fMaxScale * MAX (pLabelDescription->iMargin, MIN (6, pLabelDescription->iSize/3));  // permet d'avoir un rayon meme si on n'a pas de marge.
		double fLineWidth = 0.;
		double fFrameWidth = *iTextWidth - 2 * fRadius - fLineWidth;
		double fFrameHeight = *iTextHeight - fLineWidth;
		double fDockOffsetX = fRadius + fLineWidth/2;
		double fDockOffsetY = 0.;
		cairo_dock_draw_rounded_rectangle (pCairoContext, fRadius, fLineWidth, fFrameWidth, fFrameHeight);
		//cairo_dock_draw_frame (pCairoContext, fRadius, fLineWidth, fFrameWidth, fFrameHeight, fDockOffsetX, fDockOffsetY, 1, 0., CAIRO_DOCK_HORIZONTAL);
		cairo_set_source_rgba (pCairoContext, pLabelDescription->fBackgroundColor[0], pLabelDescription->fBackgroundColor[1], pLabelDescription->fBackgroundColor[2], pLabelDescription->fBackgroundColor[3]);
		cairo_fill (pCairoContext);
		cairo_restore(pCairoContext);
	}
	
	//g_print ("ink : %d;%d\n", (int) ink.x, (int) ink.y);
	cairo_translate (pCairoContext, -ink.x*fZoomX + iOutlineMargin/2, -ink.y + iOutlineMargin/2);  // meme remarque pour le +1.
	
	//\_________________ On dessine les contours du texte.
	if (pLabelDescription->bOutlined)
	{
		cairo_save (pCairoContext);
		if (fZoomX != 1)
			cairo_scale (pCairoContext, fZoomX, 1.);
		cairo_push_group (pCairoContext);
		cairo_set_source_rgb (pCairoContext, 0.2, 0.2, 0.2);
		int i;
		for (i = 0; i < 4; i++)
		{
			cairo_move_to (pCairoContext, i&2-1, 2*(i&1)-1);
			pango_cairo_show_layout (pCairoContext, pLayout);
		}
		cairo_pop_group_to_source (pCairoContext);
		//cairo_paint_with_alpha (pCairoContext, .75);
		cairo_paint (pCairoContext);
		cairo_restore(pCairoContext);
	}

	//\_________________ On remplit l'interieur du texte.
	cairo_pattern_t *pGradationPattern = NULL;
	if (pLabelDescription->fColorStart != pLabelDescription->fColorStop)
	{
		/// faut-il vraiment ajouter les ink dans le pattern ???
		if (pLabelDescription->bVerticalPattern)
			pGradationPattern = cairo_pattern_create_linear (0.,
				ink.y + 0 + 0*iOutlineMargin/2,  // meme remarque pour le +1.
				0.,
				ink.y + 0 + 0*iOutlineMargin/2 + ink.height);
		else
			pGradationPattern = cairo_pattern_create_linear (ink.x + 0*iOutlineMargin/2,
				0.,
				ink.x + 0*iOutlineMargin/2 + ink.width,
				0.);
		g_return_val_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS, NULL);
		cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
		cairo_pattern_add_color_stop_rgba (pGradationPattern,
			0.,
			pLabelDescription->fColorStart[0],
			pLabelDescription->fColorStart[1],
			pLabelDescription->fColorStart[2],
			1.);
		cairo_pattern_add_color_stop_rgba (pGradationPattern,
			1.,
			pLabelDescription->fColorStop[0],
			pLabelDescription->fColorStop[1],
			pLabelDescription->fColorStop[2],
			1.);
		cairo_set_source (pCairoContext, pGradationPattern);
	}
	else
		cairo_set_source_rgb (pCairoContext, pLabelDescription->fColorStart[0], pLabelDescription->fColorStart[1], pLabelDescription->fColorStart[2]);
	cairo_move_to (pCairoContext, 0, 0);
	if (fZoomX != 1)
		cairo_scale (pCairoContext, fZoomX, 1.);
	pango_cairo_show_layout (pCairoContext, pLayout);
	cairo_pattern_destroy (pGradationPattern);
	
	cairo_destroy (pCairoContext);
	
	/* set_device_offset is buggy, doesn't work for positive offsets. so we use explicit offsets... so unfortunate.
	cairo_surface_set_device_offset (pNewSurface,
					 log.width / 2. - ink.x,
					 log.height     - ink.y);*/
	*fTextXOffset = (log.width * fZoomX / 2. - ink.x) / fMaxScale;
	*fTextYOffset = - (pLabelDescription->iSize - (log.height - ink.y)) / fMaxScale ;  // en tenant compte de l'ecart du bas du texte.
	// *fTextYOffset = - (ink.y) / fMaxScale;  // pour tenir compte de l'ecart du bas du texte.
	
	*iTextWidth = *iTextWidth / fMaxScale;
	*iTextHeight = *iTextHeight / fMaxScale;
	
	g_object_unref (pLayout);
	return pNewSurface;
}


cairo_surface_t * cairo_dock_duplicate_surface (cairo_surface_t *pSurface, cairo_t *pSourceContext, double fWidth, double fHeight, double fDesiredWidth, double fDesiredHeight)
{
	g_return_val_if_fail (pSurface != NULL && pSourceContext != NULL && cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);

	//\_______________ On cree la surface de la taille desiree.
	if (fDesiredWidth == 0)
		fDesiredWidth = fWidth;
	if (fDesiredHeight == 0)
		fDesiredHeight = fHeight;
	
	g_print ("%s (%.2fx%.2f -> %.2fx%.2f)\n", __func__, fWidth, fHeight, fDesiredWidth, fDesiredHeight);
	cairo_surface_t *pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
		fDesiredWidth,
		fDesiredHeight);
	cairo_t *pCairoContext = cairo_create (pNewSurface);

	//\_______________ On plaque la surface originale dessus.
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	cairo_scale (pCairoContext,
		fDesiredWidth / fWidth,
		fDesiredHeight / fHeight);
	
	cairo_set_source_surface (pCairoContext, pSurface, 0., 0.);
	cairo_paint (pCairoContext);
	cairo_destroy (pCairoContext);
	
	return pNewSurface;
}

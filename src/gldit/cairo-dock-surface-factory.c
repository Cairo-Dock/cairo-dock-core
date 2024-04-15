/**
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <math.h>
#include <pango/pango.h>

#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-icon-manager.h"  // cairo_dock_search_icon_s_path
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-style-manager.h"
#include "cairo-dock-surface-factory.h"

extern GldiContainer *g_pPrimaryContainer;
extern gboolean g_bUseOpenGL;


/* Calcule la taille d'une image selon une contrainte en largeur et hauteur de manière à remplir l'espace donné.
*@param fImageWidth the width of the image. Contient initialement the width of the image, et sera écrasée avec la largeur obtenue.
*@param fImageHeight the height of the image. Contient initialement the height of the image, et sera écrasée avec la hauteur obtenue.
*@param iWidthConstraint contrainte en largeur (0 <=> pas de contrainte).
*@param iHeightConstraint contrainte en hauteur (0 <=> pas de contrainte).
*@param bNoZoomUp TRUE ssi on ne doit pas agrandir the image (seulement la rétrécir).
*@param fZoomWidth sera renseigné avec le facteur de zoom en largeur qui a été appliqué.
*@param fZoomHeight sera renseigné avec le facteur de zoom en hauteur qui a été appliqué.
*/
static void _cairo_dock_calculate_size_fill (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, gboolean bNoZoomUp, double *fZoomWidth, double *fZoomHeight)
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

/* Calcule la taille d'une image selon une contrainte en largeur et hauteur en gardant le ratio hauteur/largeur constant.
*@param fImageWidth the width of the image. Contient initialement the width of the image, et sera écrasée avec la largeur obtenue.
*@param fImageHeight the height of the image. Contient initialement the height of the image, et sera écrasée avec la hauteur obtenue.
*@param iWidthConstraint contrainte en largeur (0 <=> pas de contrainte).
*@param iHeightConstraint contrainte en hauteur (0 <=> pas de contrainte).
*@param bNoZoomUp TRUE ssi on ne doit pas agrandir the image (seulement la rétrécir).
*@param fZoom sera renseigné avec le facteur de zoom qui a été appliqué.
*/
static void _cairo_dock_calculate_size_constant_ratio (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, gboolean bNoZoomUp, double *fZoom)
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


/* Calculate the size of an image according to a constraint on width and height, and a loading modifier.
*@param fImageWidth pointer to the width of the image. Initially contains the width of the original image, and is updated with the resulting width.
*@param fImageHeight pointer to the height of the image. Initially contains the height of the original image, and is updated with the resulting height.
*@param iWidthConstraint constraint on width (0 <=> no constraint).
*@param iHeightConstraint constraint on height (0 <=> no constraint).
*@param iLoadingModifier a mask of different loading modifiers.
*@param fZoomWidth will be filled with the zoom that has been applied on width.
*@param fZoomHeight will be filled with the zoom that has been applied on height.
*/
static void _cairo_dock_calculate_constrainted_size (double *fImageWidth, double *fImageHeight, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fZoomWidth, double *fZoomHeight)
{
	gboolean bFillSpace = iLoadingModifier & CAIRO_DOCK_FILL_SPACE;
	gboolean bKeepRatio = iLoadingModifier & CAIRO_DOCK_KEEP_RATIO;
	gboolean bNoZoomUp = iLoadingModifier & CAIRO_DOCK_DONT_ZOOM_IN;
	gboolean bAnimated = iLoadingModifier & CAIRO_DOCK_ANIMATED_IMAGE;
	gint iOrientation = iLoadingModifier & CAIRO_DOCK_ORIENTATION_MASK;
	if (iOrientation > CAIRO_DOCK_ORIENTATION_VFLIP)  // inversion x/y
	{
		double tmp = *fImageWidth;
		*fImageWidth = *fImageHeight;
		*fImageHeight = tmp;
	}
	
	if (bAnimated)
	{
		if (*fImageWidth > *fImageHeight)
		{
			if (((int)*fImageWidth) % ((int)*fImageHeight) == 0)  // w = k*h
			{
				iWidthConstraint = *fImageWidth / *fImageHeight * iHeightConstraint;
			}
			else if (*fImageWidth > 2 * *fImageHeight)  // if we're confident this image is an animated one, try to be smart, to handle the case of non-square frames.
			{
				// assume we have wide frames => w > h
				int w = *fImageHeight + 1;
				do
				{
					if ((int)*fImageWidth % w == 0)
					{
						iWidthConstraint = *fImageWidth / w * iHeightConstraint;
						//g_print ("frame: %d, %d\n", w, iWidthConstraint);
						break;
					}
					w ++;
				} while (w < (*fImageWidth) / 2);
			}
		}
	}
	
	if (bKeepRatio)
	{
		_cairo_dock_calculate_size_constant_ratio (fImageWidth,
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
		_cairo_dock_calculate_size_fill (fImageWidth,
			fImageHeight,
			iWidthConstraint,
			iHeightConstraint,
			bNoZoomUp,
			fZoomWidth,
			fZoomHeight);
	}
}


static inline cairo_t *_get_source_context (void)
{
	cairo_t *pSourceContext = NULL;
	if (g_pPrimaryContainer != NULL)
	{
		gtk_widget_realize (g_pPrimaryContainer->pWidget);  // ensure the widget is realized
		// this is deprecated, but it is a convenient way to get a cairo context whose properties we can use when creating our surfaces
		pSourceContext = gdk_cairo_create (gldi_container_get_gdk_window(g_pPrimaryContainer));
	}
	return pSourceContext;  // Note: we can't keep the context alive and reuse it later, because under Wayland it will make the container invisible
}

cairo_surface_t *cairo_dock_create_blank_surface_full (int iWidth, int iHeight, cairo_t *pSourceContext)
{
	cairo_t *tmpContext = NULL;
	if (!pSourceContext && !g_bUseOpenGL)
		pSourceContext = tmpContext = _get_source_context ();
	cairo_surface_t *pSurface;
	if (pSourceContext != NULL && cairo_status (pSourceContext) != CAIRO_STATUS_SUCCESS)
		pSourceContext = NULL;
	if (g_bUseOpenGL || pSourceContext == NULL)
	{
		// opengl or invalid context -> create an image that is a mere ARGB buffer that can be mapped into a texture
		double xs = 1.0, ys = 1.0; // take into account the source context scale
		if (pSourceContext != NULL)
			cairo_surface_get_device_scale (cairo_get_target (pSourceContext), &xs, &ys);
		else if (g_pPrimaryContainer != NULL)
		{
			GdkWindow* gdkwindow = gldi_container_get_gdk_window (g_pPrimaryContainer);
			xs = ys = gdk_window_get_scale_factor (gdkwindow);
		}
		pSurface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, (int)ceil(iWidth * xs), (int)ceil(iHeight * ys));
		cairo_surface_set_device_scale (pSurface, xs, ys);
	}
	else
		pSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			iWidth,
			iHeight);
		
	if(tmpContext) cairo_destroy (tmpContext);
	return pSurface;
}

cairo_surface_t *cairo_dock_create_blank_surface(int iWidth, int iHeight)
{
	return cairo_dock_create_blank_surface_full (iWidth, iHeight, NULL);
}

static inline void _apply_orientation_and_scale (cairo_t *pCairoContext, CairoDockLoadImageModifier iLoadingModifier, double fImageWidth, double fImageHeight, double fZoomX, double fZoomY, double fUsefulWidth, double fUsefulheight)
{
	int iOrientation = iLoadingModifier & CAIRO_DOCK_ORIENTATION_MASK;
	
	cairo_translate (pCairoContext,
		fImageWidth/2,
		fImageHeight/2);
	cairo_scale (pCairoContext,
		fZoomX,
		fZoomY);
	switch (iOrientation)
	{
		case CAIRO_DOCK_ORIENTATION_HFLIP :
			cd_debug ("orientation : HFLIP");
			cairo_scale (pCairoContext, -1., 1.);
		break ;
		case CAIRO_DOCK_ORIENTATION_ROT_180 :
			cd_debug ("orientation : ROT_180");
			cairo_rotate (pCairoContext, G_PI);
		break ;
		case CAIRO_DOCK_ORIENTATION_VFLIP :
			cd_debug ("orientation : VFLIP");
			cairo_scale (pCairoContext, 1., -1.);
		break ;
		case CAIRO_DOCK_ORIENTATION_ROT_90_HFLIP :
			cd_debug ("orientation : ROT_90_HFLIP");
			cairo_scale (pCairoContext, -1., 1.);
			cairo_rotate (pCairoContext, + G_PI/2);
		break ;
		case CAIRO_DOCK_ORIENTATION_ROT_90 :
			cd_debug ("orientation : ROT_90");
			cairo_rotate (pCairoContext, + G_PI/2);
		break ;
		case CAIRO_DOCK_ORIENTATION_ROT_90_VFLIP :
			cd_debug ("orientation : ROT_90_VFLIP");
			cairo_scale (pCairoContext, 1., -1.);
			cairo_rotate (pCairoContext, + G_PI/2);
		break ;
		case CAIRO_DOCK_ORIENTATION_ROT_270 :
			cd_debug ("orientation : ROT_270");
			cairo_rotate (pCairoContext, - G_PI/2);
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


cairo_surface_t *cairo_dock_create_surface_from_xicon_buffer (gulong *pXIconBuffer, int iBufferNbElements, int iWidth, int iHeight)
{
	//\____________________ On recupere la plus grosse des icones presentes dans le tampon (meilleur rendu).
	int iIndex = 0, iBestIndex = 0;
	while (iIndex + 2 < iBufferNbElements)
	{
		if (pXIconBuffer[iIndex] == 0 || pXIconBuffer[iIndex+1] == 0)  // precaution au cas ou un buffer foirreux nous serait retourne, on risque de boucler sans fin.
		{
			cd_warning ("This icon is broken !\nThis means that one of the current applications has sent a buggy icon to X.");
			if (iIndex == 0)  // tout le buffer est a jeter.
				return NULL;
			break;
		}
		if (pXIconBuffer[iIndex] > pXIconBuffer[iBestIndex])
			iBestIndex = iIndex;
		iIndex += 2 + pXIconBuffer[iIndex] * pXIconBuffer[iIndex+1];
	}

	//\____________________ On pre-multiplie chaque composante par le alpha (necessaire pour libcairo).
	int w = pXIconBuffer[iBestIndex];
	int h = pXIconBuffer[iBestIndex+1];
	iBestIndex += 2;
	//g_print ("%s (%dx%d)\n", __func__, w, h);
	
	int i, n = w * h;
	if (iBestIndex + n > iBufferNbElements)  // precaution au cas ou le nombre d'elements dans le buffer serait incorrect.
	{
		cd_warning ("This icon is broken !\nThis means that one of the current applications has sent a buggy icon to X.");
		return NULL;
	}
	gint pixel, alpha, red, green, blue;
	float fAlphaFactor;
	gint *pPixelBuffer = (gint *) &pXIconBuffer[iBestIndex];  // on va ecrire le resultat du filtre directement dans le tableau fourni en entree. C'est ok car sizeof(gulong) >= sizeof(gint), donc le tableau de pixels est plus petit que le buffer fourni en entree. merci a Hannemann pour ses tests et ses screenshots ! :-)
	for (i = 0; i < n; i ++)
	{
		pixel = (gint) pXIconBuffer[iBestIndex+i];
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
	int iStride = w * sizeof (gint);  // nbre d'octets entre le debut de 2 lignes.
	cairo_surface_t *surface_ini = cairo_image_surface_create_for_data ((guchar *)pPixelBuffer,
		CAIRO_FORMAT_ARGB32,
		w,
		h,
		iStride);
	
	double fWidth = w, fHeight = h;
	double fIconWidthSaturationFactor = 1., fIconHeightSaturationFactor = 1.;
	_cairo_dock_calculate_constrainted_size (&fWidth,
		&fHeight,
		iWidth,
		iHeight,
		CAIRO_DOCK_KEEP_RATIO | CAIRO_DOCK_FILL_SPACE,
		&fIconWidthSaturationFactor,
		&fIconHeightSaturationFactor);
	
	cairo_surface_t *pNewSurface = cairo_dock_create_blank_surface (
		iWidth,
		iHeight);
	cairo_t *pCairoContext = cairo_create (pNewSurface);
	
	double fUsefulWidth = w * fIconWidthSaturationFactor;  // a part dans le cas fill && keep ratio, c'est la meme chose que fImageWidth et fImageHeight.
	double fUsefulHeight = h * fIconHeightSaturationFactor;
	_apply_orientation_and_scale (pCairoContext,
		CAIRO_DOCK_KEEP_RATIO | CAIRO_DOCK_FILL_SPACE,
		iWidth, iHeight,
		fIconWidthSaturationFactor, fIconHeightSaturationFactor,
		fUsefulWidth, fUsefulHeight);
	cairo_set_source_surface (pCairoContext, surface_ini, 0, 0);
	cairo_paint (pCairoContext);

	cairo_surface_destroy (surface_ini);
	cairo_destroy (pCairoContext);

	return pNewSurface;
}


cairo_surface_t *cairo_dock_create_surface_from_pixbuf (GdkPixbuf *pixbuf, double fMaxScale, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fImageWidth, double *fImageHeight, double *fZoomX, double *fZoomY)
{
	*fImageWidth = gdk_pixbuf_get_width (pixbuf);
	*fImageHeight = gdk_pixbuf_get_height (pixbuf);
	double fIconWidthSaturationFactor = 1., fIconHeightSaturationFactor = 1.;
	_cairo_dock_calculate_constrainted_size (fImageWidth,
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

	cairo_surface_t *pNewSurface = cairo_dock_create_blank_surface (
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


cairo_surface_t *cairo_dock_create_surface_from_image (const gchar *cImagePath, double fMaxScale, int iWidthConstraint, int iHeightConstraint, CairoDockLoadImageModifier iLoadingModifier, double *fImageWidth, double *fImageHeight, double *fZoomX, double *fZoomY)
{
	//g_print ("%s (%s, %dx%dx%.2f, %d)\n", __func__, cImagePath, iWidthConstraint, iHeightConstraint, fMaxScale, iLoadingModifier);
	g_return_val_if_fail (cImagePath != NULL, NULL);
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
			//cd_debug ("  format : %d;%d;%d", bIsSVG, bIsPNG, bIsXPM);
		}
		fclose (fd);
	}
	else
	{
		cd_warning ("This file (%s) doesn't exist or is not readable.", cImagePath);
		return NULL;
	}
	if (! bIsSVG && ! bIsPNG && ! bIsXPM)  // sinon en desespoir de cause on se base sur l'extension.
	{
		//cd_debug ("  on se base sur l'extension en desespoir de cause.");
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
			_cairo_dock_calculate_constrainted_size (fImageWidth,
				fImageHeight,
				iWidthConstraint,
				iHeightConstraint,
				iLoadingModifier,
				&fIconWidthSaturationFactor,
				&fIconHeightSaturationFactor);
			
			pNewSurface = cairo_dock_create_blank_surface (
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
			_cairo_dock_calculate_constrainted_size (fImageWidth,
				fImageHeight,
				iWidthConstraint,
				iHeightConstraint,
				iLoadingModifier,
				&fIconWidthSaturationFactor,
				&fIconHeightSaturationFactor);
			
			pNewSurface = cairo_dock_create_blank_surface (
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

cairo_surface_t *cairo_dock_create_surface_from_image_simple (const gchar *cImageFile, double fImageWidth, double fImageHeight)
{
	g_return_val_if_fail (cImageFile != NULL, NULL);
	double fImageWidth_ = fImageWidth, fImageHeight_ = fImageHeight;
	gchar *cImagePath;
	if (*cImageFile == '/')
		cImagePath = (gchar *)cImageFile;
	else
		cImagePath = cairo_dock_search_image_s_path (cImageFile);
		
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_image (cImagePath,
		1.,
		fImageWidth,
		fImageHeight,
		CAIRO_DOCK_FILL_SPACE,
		&fImageWidth_,
		&fImageHeight_,
		NULL,
		NULL);
	if (cImagePath != cImageFile)
		g_free (cImagePath);
	return pSurface;
}

cairo_surface_t *cairo_dock_create_surface_from_icon (const gchar *cImageFile, double fImageWidth, double fImageHeight)
{
	g_return_val_if_fail (cImageFile != NULL, NULL);
	double fImageWidth_ = fImageWidth, fImageHeight_ = fImageHeight;
	gchar *cIconPath;
	if (*cImageFile == '/')
		cIconPath = (gchar *)cImageFile;
	else
		cIconPath = cairo_dock_search_icon_s_path (cImageFile, (gint) MAX (fImageWidth, fImageHeight));
		
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_image (cIconPath,
		1.,
		fImageWidth,
		fImageHeight,
		CAIRO_DOCK_FILL_SPACE,
		&fImageWidth_,
		&fImageHeight_,
		NULL,
		NULL);
	if (cIconPath != cImageFile)
		g_free (cIconPath);
	return pSurface;
}

cairo_surface_t *cairo_dock_create_surface_from_pattern (const gchar *cImageFile, double fImageWidth, double fImageHeight, double fAlpha)
{
	cairo_surface_t *pNewSurface = NULL;

	if (cImageFile != NULL)
	{
		gchar *cImagePath = cairo_dock_search_image_s_path (cImageFile);
		double w, h;
		cairo_surface_t *pPatternSurface = cairo_dock_create_surface_from_image (cImagePath,
			1.,
			0,  // no constraint on the width of the pattern => the pattern will repeat on the width
			fImageHeight,  // however, we want to see all the height of the pattern with no repetition => constraint on the height
			CAIRO_DOCK_FILL_SPACE | CAIRO_DOCK_KEEP_RATIO,
			&w,
			&h,
			NULL, NULL);
		g_free (cImagePath);
		if (pPatternSurface == NULL)
			return NULL;
		
		pNewSurface = cairo_dock_create_blank_surface (
			fImageWidth,
			fImageHeight);
		cairo_t *pCairoContext = cairo_create (pNewSurface);

		cairo_pattern_t* pPattern = cairo_pattern_create_for_surface (pPatternSurface);
		g_return_val_if_fail (cairo_pattern_status (pPattern) == CAIRO_STATUS_SUCCESS, NULL);
		cairo_pattern_set_extend (pPattern, CAIRO_EXTEND_REPEAT);

		cairo_set_source (pCairoContext, pPattern);
		cairo_paint_with_alpha (pCairoContext, fAlpha);
		cairo_destroy (pCairoContext);
		cairo_pattern_destroy (pPattern);
		
		cairo_surface_destroy (pPatternSurface);
	}
	
	return pNewSurface;
}



cairo_surface_t * cairo_dock_rotate_surface (cairo_surface_t *pSurface, double fImageWidth, double fImageHeight, double fRotationAngle)
{
	g_return_val_if_fail (pSurface != NULL, NULL);
	if (fRotationAngle != 0)
	{
		cairo_surface_t *pNewSurfaceRotated;
		cairo_t *pCairoContext;
		if (fabs (fRotationAngle) > G_PI / 2)
		{
			pNewSurfaceRotated = cairo_dock_create_blank_surface (
				fImageWidth,
				fImageHeight);
			pCairoContext = cairo_create (pNewSurfaceRotated);

			cairo_translate (pCairoContext, 0, fImageHeight);
			cairo_scale (pCairoContext, 1, -1);
		}
		else
		{
			pNewSurfaceRotated = cairo_dock_create_blank_surface (
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


cairo_surface_t *cairo_dock_create_surface_from_text_full (const gchar *cText, GldiTextDescription *pTextDescription, double fMaxScale, int iMaxWidth, int *iTextWidth, int *iTextHeight)
{
	g_return_val_if_fail (cText != NULL && pTextDescription != NULL, NULL);
	cairo_t *pSourceContext = _get_source_context ();
	g_return_val_if_fail (pSourceContext != NULL && cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	
	//\_________________ get the font description
	PangoFontDescription *pDesc = gldi_text_description_get_description (pTextDescription);
	if (!pDesc)
		cd_debug ("no text desc for '%s'", cText);
	int iSize = gldi_text_description_get_size (pTextDescription);
	pango_font_description_set_absolute_size (pDesc, fMaxScale * iSize * PANGO_SCALE);
	
	//\_________________ create a layout
	PangoLayout *pLayout = pango_cairo_create_layout (pSourceContext);
	pango_layout_set_font_description (pLayout, pDesc);
	
	if (pTextDescription->bUseMarkup)
		pango_layout_set_markup (pLayout, cText, -1);
	else
		pango_layout_set_text (pLayout, cText, -1);
	
	//\_________________ handle max width
	if (pTextDescription->fMaxRelativeWidth != 0)
	{
		int iMaxLineWidth = pTextDescription->fMaxRelativeWidth * gldi_desktop_get_width() / g_desktopGeometry.iNbScreens;  // use the mean screen width since the text might be placed anywhere on the X screen.
		pango_layout_set_width (pLayout, iMaxLineWidth * PANGO_SCALE);  // PANGO_WRAP_WORD by default
	}
	PangoRectangle log;
	pango_layout_get_pixel_extents (pLayout, NULL, &log);
	
	//\_________________ load the layout into a surface
	gboolean bDrawBackground = ! pTextDescription->bNoDecorations;
	double fRadius = (pTextDescription->bUseDefaultColors ? MIN (myStyleParam.iCornerRadius * .75, iSize/2) : fMaxScale * MAX (pTextDescription->iMargin, MIN (6, iSize/2)));  // permet d'avoir un rayon meme si on n'a pas de marge.
	int iOutlineMargin = 2*pTextDescription->iMargin * fMaxScale + (pTextDescription->bOutlined ? 2 : 0);  // outlined => +1 tout autour des lettres.
	double fZoomX = ((iMaxWidth != 0 && log.width + iOutlineMargin > iMaxWidth) ? (double)iMaxWidth / (log.width + iOutlineMargin) : 1.);
	double fLineWidth = 1;
	
	*iTextWidth = (log.width + iOutlineMargin) * fZoomX + 2*fLineWidth;  // le texte + la marge de chaque cote.
	if (bDrawBackground)  // quand on trace le cadre, on evite qu'avec des petits textes genre "1" on obtienne un fond tout rond.
	{
		*iTextWidth = MAX (*iTextWidth, 2 * fRadius + 10);
		if (iMaxWidth != 0 && *iTextWidth > iMaxWidth)
			*iTextWidth = iMaxWidth;
	}
	*iTextHeight = log.height + iOutlineMargin + 2*fLineWidth;
	
	cairo_surface_t* pNewSurface = cairo_dock_create_blank_surface_full (
		*iTextWidth,
		*iTextHeight,
		pSourceContext);
	cairo_t* pCairoContext = cairo_create (pNewSurface);
	
	//\_________________ draw the background
	if (bDrawBackground)  // non transparent.
	{
		cairo_save (pCairoContext);
		double fFrameWidth = *iTextWidth - 2 * fRadius - fLineWidth;
		double fFrameHeight = *iTextHeight - fLineWidth;
		cairo_dock_draw_rounded_rectangle (pCairoContext, fRadius, fLineWidth, fFrameWidth, fFrameHeight);
		
		if (pTextDescription->bUseDefaultColors)
			gldi_style_colors_set_bg_color (pCairoContext);
		else
			gldi_color_set_cairo (pCairoContext, &pTextDescription->fBackgroundColor);
		cairo_fill_preserve (pCairoContext);
		
		if (pTextDescription->bUseDefaultColors)
			gldi_style_colors_set_line_color (pCairoContext);
		else
			gldi_color_set_cairo (pCairoContext, &pTextDescription->fLineColor);
		cairo_set_line_width (pCairoContext, fLineWidth);
		cairo_stroke (pCairoContext);
		
		cairo_restore(pCairoContext);
	}
	
	//g_print ("%s : log = %d;%d\n", cText, (int) log.x, (int) log.y);
	int dx = (*iTextWidth - log.width * fZoomX)/2;  // pour se centrer.
	int dy = (*iTextHeight - log.height)/2;  // pour se centrer.
	cairo_translate (pCairoContext,
		-log.x*fZoomX + dx,
		-log.y + dy);
	
	//\_________________ On dessine les contours du texte.
	if (pTextDescription->bOutlined)
	{
		cairo_save (pCairoContext);
		if (fZoomX != 1)
			cairo_scale (pCairoContext, fZoomX, 1.);
		cairo_push_group (pCairoContext);
		cairo_set_source_rgb (pCairoContext, 0.2, 0.2, 0.2);
		int i;
		for (i = 0; i < 2; i++)
		{
			cairo_move_to (pCairoContext, 0, 2*i-1);
			pango_cairo_show_layout (pCairoContext, pLayout);
		}
		for (i = 0; i < 2; i++)
		{
			cairo_move_to (pCairoContext, 2*i-1, 0);
			pango_cairo_show_layout (pCairoContext, pLayout);
		}
		cairo_pop_group_to_source (pCairoContext);
		cairo_paint (pCairoContext);
		cairo_restore(pCairoContext);
	}

	//\_________________ On remplit l'interieur du texte.
	if (pTextDescription->bUseDefaultColors)
		gldi_style_colors_set_text_color (pCairoContext);
	else
		gldi_color_set_cairo_rgb (pCairoContext, &pTextDescription->fColorStart);
	cairo_move_to (pCairoContext, 0, 0);
	if (fZoomX != 1)
		cairo_scale (pCairoContext, fZoomX, 1.);
	//if (pTextDescription->bOutlined)
	//	cairo_move_to (pCairoContext, 1,1);
	pango_cairo_show_layout (pCairoContext, pLayout);
	
	cairo_destroy (pCairoContext);
	
	*iTextWidth = *iTextWidth/** / fMaxScale*/;
	*iTextHeight = *iTextHeight/** / fMaxScale*/;
	
	g_object_unref (pLayout);
	pango_font_description_set_absolute_size (pDesc, iSize * PANGO_SCALE);
	cairo_destroy (pSourceContext);
	return pNewSurface;
}


cairo_surface_t * cairo_dock_duplicate_surface (cairo_surface_t *pSurface, double fWidth, double fHeight, double fDesiredWidth, double fDesiredHeight)
{
	g_return_val_if_fail (pSurface != NULL, NULL);

	//\_______________ On cree la surface de la taille desiree.
	if (fDesiredWidth == 0)
		fDesiredWidth = fWidth;
	if (fDesiredHeight == 0)
		fDesiredHeight = fHeight;
	
	//g_print ("%s (%.2fx%.2f -> %.2fx%.2f)\n", __func__, fWidth, fHeight, fDesiredWidth, fDesiredHeight);
	cairo_surface_t *pNewSurface = cairo_dock_create_blank_surface (
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

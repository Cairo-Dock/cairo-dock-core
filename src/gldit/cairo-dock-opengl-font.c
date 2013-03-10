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

#include <math.h>
#include <gdk/gdkx.h>  // struct Display *
#include <pango/pangox.h>
#include <pango/pango.h>
#include <cairo.h>
#include <GL/gl.h>

#include "cairo-dock-surface-factory.h"  // cairo_dock_create_blank_surface
#include "cairo-dock-draw.h"  // cairo_dock_create_drawing_context_generic
#include "cairo-dock-log.h"
#include "cairo-dock-draw-opengl.h"

#include "cairo-dock-opengl-font.h"

#include "texture-gradation.h"
#define RADIAN (G_PI / 180.0)  // Conversion Radian/Degres
#define DELTA_ROUND_DEGREE 3

extern CairoContainer *g_pPrimaryContainer;

extern CairoDockGLConfig g_openglConfig;


GLuint cairo_dock_create_texture_from_text_simple (const gchar *cText, const gchar *cFontDescription, cairo_t* pSourceContext, int *iWidth, int *iHeight)
{
	g_return_val_if_fail (cText != NULL && cFontDescription != NULL, 0);
	
	//\_________________ On ecrit le texte dans un calque Pango.
	PangoLayout *pLayout = pango_cairo_create_layout (pSourceContext);
	
	PangoFontDescription *fd = pango_font_description_from_string (cFontDescription);
	pango_layout_set_font_description (pLayout, fd);
	pango_font_description_free (fd);
	
	pango_layout_set_text (pLayout, cText, -1);
	
	//\_________________ On cree une surface aux dimensions du texte.
	PangoRectangle log;
	pango_layout_get_pixel_extents (pLayout, NULL, &log);
	cairo_surface_t* pNewSurface = cairo_dock_create_blank_surface (
		log.width,
		log.height);
	*iWidth = log.width;
	*iHeight = log.height;
	
	//\_________________ On dessine le texte.
	cairo_t* pCairoContext = cairo_create (pNewSurface);
	cairo_translate (pCairoContext, -log.x, -log.y);
	cairo_set_source_rgb (pCairoContext, 1., 1., 1.);
	cairo_move_to (pCairoContext, 0, 0);
	pango_cairo_show_layout (pCairoContext, pLayout);
	cairo_destroy (pCairoContext);
	
	g_object_unref (pLayout);
	
	//\_________________ On cree la texture.
	GLuint iTexture = cairo_dock_create_texture_from_surface (pNewSurface);
	cairo_surface_destroy (pNewSurface);
	return iTexture;
}


// taken from gdkgl
// pango_x_ functions are deprecated, but as long as they work, we shouldn't care too much.
// use XLoadQueryFont() if needed...
/*static PangoFont *
gldi_font_use_pango_font_common (PangoFontMap               *font_map,
                                   const PangoFontDescription *font_desc,
                                   int                         first,
                                   int                         count,
                                   int                         list_base)
{
	PangoFont *font = NULL;
	const gchar *charset = NULL;
	PangoXSubfont subfont_id;
	gchar *xlfd = NULL;  // X Logical Font Description
	PangoXFontCache *font_cache;
	XFontStruct *fs;

	font = pango_font_map_load_font (font_map, NULL, font_desc);
	if (font == NULL)
	{
		g_warning ("cannot load PangoFont");
		goto FAIL;
	}

	charset = "iso8859-1";
	if (!pango_x_find_first_subfont (font, (gchar **)&charset, 1, &subfont_id))
	{
		g_warning ("cannot find PangoXSubfont");
		font = NULL;
		goto FAIL;
	}

	xlfd = pango_x_font_subfont_xlfd (font, subfont_id);
	if (xlfd == NULL)
	{
		g_warning ("cannot get XLFD");
		font = NULL;
		goto FAIL;
	}

	font_cache = pango_x_font_map_get_font_cache (font_map);

	fs = pango_x_font_cache_load (font_cache, xlfd);

	glXUseXFont (fs->fid, first, count, list_base);

	pango_x_font_cache_unload (font_cache, fs);

	FAIL:

	if (xlfd != NULL)
		g_free (xlfd);

	return font;
}
static PangoFont *
gldi_font_use_pango_font (const PangoFontDescription *font_desc,
                            int                         first,
                            int                         count,
                            int                         list_base)
{
	PangoFontMap *font_map;

	g_return_val_if_fail (font_desc != NULL, NULL);
	
	font_map = pango_x_font_map_for_display (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

	return gldi_font_use_pango_font_common (font_map, font_desc,
											first, count, list_base);
}


CairoDockGLFont *cairo_dock_load_bitmap_font (const gchar *cFontDescription, int first, int count)
{
	g_return_val_if_fail (cFontDescription != NULL && count > 0, NULL);
	
	GLuint iListBase = glGenLists (count);
	g_return_val_if_fail (iListBase != 0, NULL);
	
	CairoDockGLFont *pFont = g_new0 (CairoDockGLFont, 1);
	pFont->iListBase = iListBase;
	pFont->iNbChars = count;
	pFont->iCharBase = first;
	
	PangoFontDescription *fd = pango_font_description_from_string (cFontDescription);
	PangoFont *font = gldi_font_use_pango_font (fd,
		first,
		count,
		iListBase);
	g_return_val_if_fail (font != NULL, NULL);
	
	PangoFontMetrics* metrics = pango_font_get_metrics (font, NULL);
	pFont->iCharWidth = pango_font_metrics_get_approximate_char_width (metrics) / PANGO_SCALE;
	pFont->iCharHeight = (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics)) / PANGO_SCALE;
	pango_font_metrics_unref(metrics);
	
	pango_font_description_free (fd);
	return pFont;
}*/

CairoDockGLFont *cairo_dock_load_textured_font (const gchar *cFontDescription, int first, int count)
{
	g_return_val_if_fail (g_pPrimaryContainer != NULL && count > 0, NULL);
	if (first < 32)  // 32 = ' '
	{
		count -= (32 - first);
		first = 32;
	}
	gchar *cPool = g_new0 (gchar, 4*count + 1);
	int i, j=0;
	guchar c;
	for (i = 0; i < count; i ++)
	{
		c = first + i;
		if (c > 254)
		{
			count=i;
			break;
		}
		if ((c > 126 && c < 126 + 37) || (c == 173))  // le 173 est un caractere bizarre (sa taille est nulle).
		{
			cPool[j++] = ' ';
		}
		else
		{
			j += MAX (0, sprintf (cPool+j, "%lc", c));  // les caracteres ASCII >128 doivent etre convertis en multi-octets.
		}
	}
	cd_debug ("%s (%d + %d -> '%s')", __func__, first, count, cPool);
	/*iconv_t cd = iconv_open("UTF-8", "ISO-8859-1");
	gchar *outbuf = g_new0 (gchar, count*4+1);
	gchar *outbuf0 = outbuf, *inbuf0 = cPool;
	size_t inbytesleft = count;
	size_t outbytesleft = count*4;
	size_t size = iconv (cd,
		&cPool, &inbytesleft,
		&outbuf, &outbytesleft);
	cd_debug ("%d bytes left, %d bytes written => '%s'", inbytesleft, outbytesleft, outbuf0);
	g_free (inbuf0);
	cPool = outbuf0;
	iconv_close (cd);*/
	
	int iWidth, iHeight;
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (g_pPrimaryContainer);
	GLuint iTexture = cairo_dock_create_texture_from_text_simple (cPool, cFontDescription, pCairoContext, &iWidth, &iHeight);
	cairo_destroy (pCairoContext);
	g_free (cPool);
	
	CairoDockGLFont *pFont = g_new0 (CairoDockGLFont, 1);
	pFont->iTexture = iTexture;
	pFont->iNbChars = count;
	pFont->iCharBase = first;
	pFont->iNbRows = 1;
	pFont->iNbColumns = count;
	pFont->iCharWidth = (double)iWidth / count;
	pFont->iCharHeight = iHeight;
	
	cd_debug ("%d char / %d pixels => %.3f", count, iWidth, (double)iWidth / count);
	return pFont;
}

CairoDockGLFont *cairo_dock_load_textured_font_from_image (const gchar *cImagePath)
{
	double fImageWidth, fImageHeight;
	GLuint iTexture = cairo_dock_create_texture_from_image_full (cImagePath, &fImageWidth, &fImageHeight);
	g_return_val_if_fail (iTexture != 0, NULL);
	
	CairoDockGLFont *pFont = g_new0 (CairoDockGLFont, 1);
	pFont->iTexture = iTexture;
	pFont->iNbChars = 256;
	pFont->iCharBase = 0;
	pFont->iNbRows = 16;
	pFont->iNbColumns = 16;
	pFont->iCharWidth = fImageWidth / pFont->iNbColumns;
	pFont->iCharHeight = fImageHeight / pFont->iNbRows;
	return pFont;
}

void cairo_dock_free_gl_font (CairoDockGLFont *pFont)
{
	if (pFont == NULL)
		return ;
	if (pFont->iListBase != 0)
		glDeleteLists (pFont->iListBase, pFont->iNbChars);
	if (pFont->iTexture != 0)
		_cairo_dock_delete_texture (pFont->iTexture);
	g_free (pFont);
}


void cairo_dock_get_gl_text_extent (const gchar *cText, CairoDockGLFont *pFont, int *iWidth, int *iHeight)
{
	if (pFont == NULL || cText == NULL)
	{
		*iWidth = 0;
		*iHeight = 0;
		return ;
	}
	int i, w=0, wmax=0, h=pFont->iCharHeight;
	for (i = 0; cText[i] != '\0'; i ++)
	{
		if (cText[i] == '\n')
		{
			h += pFont->iCharHeight + 1;
			wmax = MAX (wmax, w);
			w = 0;
		}
		else
			w += pFont->iCharWidth;
	}
	
	*iWidth = MAX (wmax, w);
	*iHeight = h;
}


void cairo_dock_draw_gl_text (const guchar *cText, CairoDockGLFont *pFont)
{
	int n = strlen ((char *) cText);
	if (pFont->iListBase != 0)
	{
		if (pFont->iCharBase == 0 && strchr ((char *) cText, '\n') == NULL)  // version optimisee ou on a charge tous les caracteres.
		{
			glDisable (GL_TEXTURE_2D);
			glListBase (pFont->iListBase);
			glCallLists (n, GL_UNSIGNED_BYTE, (unsigned char *)cText);
			glListBase (0);
		}
		else
		{
			int i, j;
			for (i = 0; i < n; i ++)
			{
				if (cText[i] == '\n')
				{
					GLfloat rpos[4];
					glGetFloatv (GL_CURRENT_RASTER_POSITION, rpos);
					glRasterPos2f (rpos[0], rpos[1] + pFont->iCharHeight + 1);
					continue;
				}
				if (cText[i] < pFont->iCharBase || cText[i] >= pFont->iCharBase + pFont->iNbChars)
					continue;
				j = cText[i] - pFont->iCharBase;
				glCallList (pFont->iListBase + j);
			}
		}
	}
	else if (pFont->iTexture != 0)
	{
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_pbuffer ();  // rend mieux pour les textes
		glBindTexture (GL_TEXTURE_2D, pFont->iTexture);
		double u, v, du=1./pFont->iNbColumns, dv=1./pFont->iNbRows, w=pFont->iCharWidth, h=pFont->iCharHeight, x=.5*w, y=.5*h;
		int i, j;
		for (i = 0; i < n; i ++)
		{
			if (cText[i] == '\n')
			{
				x = .5*pFont->iCharWidth;
				y += pFont->iCharHeight + 1;
				continue;
			}
			if (cText[i] < pFont->iCharBase || cText[i] >= pFont->iCharBase + pFont->iNbChars)
				continue;
			
			j = cText[i] - pFont->iCharBase;
			u = (double) (j%pFont->iNbColumns) / pFont->iNbColumns;
			v = (double) (j/pFont->iNbColumns) / pFont->iNbRows;
			_cairo_dock_apply_current_texture_portion_at_size_with_offset (u, v, du, dv, w, h, x, y);
			x += pFont->iCharWidth;
		}
		_cairo_dock_disable_texture ();
	}
}

void cairo_dock_draw_gl_text_in_area (const guchar *cText, CairoDockGLFont *pFont, int iWidth, int iHeight, gboolean bCentered)
{
	g_return_if_fail (pFont != NULL && cText != NULL);
	if (pFont->iListBase != 0)  // marche po sur du raster.
	{
		cd_warning ("can't resize raster ! use a textured font inside.");
	}
	else
	{
		int w, h;
		cairo_dock_get_gl_text_extent ((char *) cText, pFont, &w, &h);
		
		double zx, zy;
		if (fabs ((double)iWidth/w) < fabs ((double)iHeight/h))  // on autorise les dimensions negatives pour pouvoir retourner le texte.
		{
			zx = (double)iWidth/w;
			zy = (iWidth*iHeight > 0 ? zx : -zx);
		}
		else
		{
			zy = (double)iHeight/h;
			zx = (iWidth*iHeight > 0 ? zy : -zy);
		}
		
		glScalef (zx, zy, 1.);
		if (bCentered)
			glTranslatef (-w/2, -h/2, 0.);
		cairo_dock_draw_gl_text (cText, pFont);
	}
}

void cairo_dock_draw_gl_text_at_position (const guchar *cText, CairoDockGLFont *pFont, int x, int y)
{
	g_return_if_fail (pFont != NULL && cText != NULL);
	if (pFont->iListBase != 0)
	{
		glRasterPos2f (x, y);
	}
	else
	{
		glTranslatef (x, y, 0);
	}
	cairo_dock_draw_gl_text (cText, pFont);
}

void cairo_dock_draw_gl_text_at_position_in_area (const guchar *cText, CairoDockGLFont *pFont, int x, int y, int iWidth, int iHeight, gboolean bCentered)
{
	g_return_if_fail (pFont != NULL && cText != NULL);
	if (pFont->iListBase != 0)  // marche po sur du raster.
	{
		cd_warning ("can't resize raster ! use a textured font inside.");
	}
	else
	{
		glTranslatef (x, y, 0);
		cairo_dock_draw_gl_text_in_area (cText, pFont, iWidth, iHeight, bCentered);
	}
}

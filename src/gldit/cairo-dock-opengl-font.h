/*
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


#ifndef __CAIRO_DOCK_OPENGL_FONT__
#define  __CAIRO_DOCK_OPENGL_FONT__

#include <glib.h>

#include <GL/glu.h>

#include "cairo-dock-struct.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-opengl-font.h This class provides different ways to draw text directly in OpenGL.
* \ref cairo_dock_create_texture_from_text_simple lets you draw any text in any font, by creating a texture from a Pango font description. This is a convenient function but not very fast.
* For a more efficient way, you load a font into a CairoDockGLFont with either :
* \ref cairo_dock_load_bitmap_font to load a subset of any font into bitmaps (bitmaps are not influenced by the transformation matrix)
* \ref cairo_dock_load_textured_font to load a subset of a Mono font into textures.
* You then use \ref cairo_dock_draw_gl_text_at_position to draw the text.
*/

/** Create a texture from a text. The text is drawn in white, so that you can later colorize it with a mere glColor.
*@param cText the text
*@param cFontDescription a description of the font, for instance "Monospace Bold 12"
*@param pSourceContext a cairo context, not altered by the function.
*@param iWidth a pointer that will be filled with the width of the texture.
*@param iHeight a pointer that will be filled with the height of the texture.
*@return a newly allocated texture.
*/
GLuint cairo_dock_create_texture_from_text_simple (const gchar *cText, const gchar *cFontDescription, cairo_t* pSourceContext, int *iWidth, int *iHeight);

/// Structure used to load a font for OpenGL text rendering.
struct _CairoDockGLFont {
	GLuint iListBase;
	GLuint iTexture;
	gint iNbRows;
	gint iNbColumns;
	gint iCharBase;
	gint iNbChars;
	gdouble iCharWidth;
	gdouble iCharHeight;
};

/* Load a font into bitmaps. You can load any characters of font with this function. The drawback is that each character is a bitmap, that is to say you can't zoom them.
*@param cFontDescription a description of the font, for instance "Monospace Bold 12"
*@param first first character to load.
*@param count number of characters to load.
*@return a newly allocated opengl font.
*/
//CairoDockGLFont *cairo_dock_load_bitmap_font (const gchar *cFontDescription, int first, int count);

/** Load a font into textures. You can then render your text like a normal texture (zoom, etc). The drawback is that only a mono font can be used with this function.
*@param cFontDescription a description of the font, for instance "Monospace Bold 12"
*@param first first character to load.
*@param count number of characters to load.
*@return a newly allocated opengl font.
*/
CairoDockGLFont *cairo_dock_load_textured_font (const gchar *cFontDescription, int first, int count);

/** Like the previous function, but loads the characters from an image. The image must be squared and contain the 256 extended ASCII characters in the alphabetic order.
*@param cImagePath path to the image.
*@return a newly allocated opengl font.
*/
CairoDockGLFont *cairo_dock_load_textured_font_from_image (const gchar *cImagePath);

/** Free an opengl font.
*@param pFont the font.
*/
void cairo_dock_free_gl_font (CairoDockGLFont *pFont);

/** Compute the size a text will take for a given font.
*@param cText the text
*@param pFont the font.
*@param iWidth a pointer that will be filled with the width of the text.
*@param iHeight a pointer that will be filled with the height of the text.
*/
void cairo_dock_get_gl_text_extent (const gchar *cText, CairoDockGLFont *pFont, int *iWidth, int *iHeight);

/** Render a text for a given font. In the case of a bitmap font, the current raster position is used. In the case of a texture font, the current model view is used.
*@param cText the text
*@param pFont the font.
*/
void cairo_dock_draw_gl_text (const guchar *cText, CairoDockGLFont *pFont);

/** Like /ref cairo_dock_draw_gl_text but at a given position.
*@param cText the text
*@param pFont the font.
*@param x x position of the left bottom corner of the text.
*@param y y position of the left bottom corner of the text.
*/
void cairo_dock_draw_gl_text_at_position (const guchar *cText, CairoDockGLFont *pFont, int x, int y);

/** Like /ref cairo_dock_draw_gl_text but resize the text so that it fits into a given area. Only works for a texture font.
*@param cText the text
*@param pFont the font.
*@param iWidth iWidth of the area.
*@param iHeight iHeight of the area
*@param bCentered whether the text is centered on the current position or not.
*/
void cairo_dock_draw_gl_text_in_area (const guchar *cText, CairoDockGLFont *pFont, int iWidth, int iHeight, gboolean bCentered);

/** Like /ref cairo_dock_draw_gl_text_in_area and /ref cairo_dock_draw_gl_text_at_position.
*@param cText the text
*@param pFont the font.
*@param x x position of the left bottom corner of the text.
*@param y y position of the left bottom corner of the text.
*@param iWidth iWidth of the area.
*@param iHeight iHeight of the area
*@param bCentered whether the text is centered on the given position or not.
*/
void cairo_dock_draw_gl_text_at_position_in_area (const guchar *cText, CairoDockGLFont *pFont, int x, int y, int iWidth, int iHeight, gboolean bCentered);


G_END_DECLS
#endif

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


#ifndef __CAIRO_DOCK_DRAW_OPENGL__
#define  __CAIRO_DOCK_DRAW_OPENGL__

#include <glib.h>

#include <gdk/x11/gdkglx.h>
#include <gtk/gtkgl.h>
#include <GL/glu.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-container.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-draw-opengl.h This class provides some useful functions to draw with OpenGL.
*/

void cairo_dock_set_icon_scale (Icon *pIcon, CairoContainer *pContainer, double fZoomFactor);

void cairo_dock_draw_icon_opengl (Icon *pIcon, CairoDock *pDock);

void cairo_dock_translate_on_icon_opengl (Icon *icon, CairoContainer *pContainer, double fDockMagnitude);

/** Draw an icon, according to its current parameters : position, transparency, reflect, rotation, stretching. Also draws its indicators, label, and quick-info. It generates a CAIRO_DOCK_RENDER_ICON notification.
*@param icon the icon to draw.
*@param pDock the dock containing the icon.
*@param fDockMagnitude current magnitude of the dock.
*@param bUseText TRUE to draw the labels.
*/
void cairo_dock_render_one_icon_opengl (Icon *icon, CairoDock *pDock, double fDockMagnitude, gboolean bUseText);

void cairo_dock_render_hidden_dock_opengl (CairoDock *pDock);

  //////////////////
 // LOAD TEXTURE //
//////////////////
/** Load a cairo surface into an OpenGL texture. The surface can be destroyed after that if you don't need it. The texture will have the same size as the surface.
*@param pImageSurface the surface, created with one of the 'cairo_dock_create_surface_xxx' functions.
*@return the newly allocated texture, to be destroyed with _cairo_dock_delete_texture.
*/
GLuint cairo_dock_create_texture_from_surface (cairo_surface_t *pImageSurface);

/** Load a pixels buffer representing an image into an OpenGL texture.
*@param pTextureRaw a buffer of pixels.
*@param iWidth width of the image.
*@param iHeight height of the image.
*@return the newly allocated texture, to be destroyed with _cairo_dock_delete_texture.
*/
GLuint cairo_dock_load_texture_from_raw_data (const guchar *pTextureRaw, int iWidth, int iHeight);

/** Load an image on the dock into an OpenGL texture. The texture will have the same size as the image. The size is given as an output, if you need it for some reason.
*@param cImagePath path to an image.
*@param fImageWidth pointer that will be filled with the width of the image.
*@param fImageHeight pointer that will be filled with the height of the image.
*@return the newly allocated texture, to be destroyed with _cairo_dock_delete_texture.
*/
GLuint cairo_dock_create_texture_from_image_full (const gchar *cImagePath, double *fImageWidth, double *fImageHeight);

/** Load an image on the dock into an OpenGL texture. The texture will have the same size as the image.
*@param cImagePath path to an image.
*@return the newly allocated texture, to be destroyed with _cairo_dock_delete_texture.
*/
#define cairo_dock_create_texture_from_image(cImagePath) cairo_dock_create_texture_from_image_full (cImagePath, NULL, NULL)

/** Delete an OpenGL texture from the Graphic Card.
*@param iTexture variable containing the ID of a texture.
*/
#define _cairo_dock_delete_texture(iTexture) glDeleteTextures (1, &iTexture)

/** Update the icon's texture with its current cairo surface. This allows you to draw an icon with libcairo, and just copy the result to the OpenGL texture to be able to draw the icon in OpenGL too.
*@param pIcon the icon.
*/
void cairo_dock_update_icon_texture (Icon *pIcon);
/** Update the icon's label texture with its current label surface.
*@param pIcon the icon.
*/
void cairo_dock_update_label_texture (Icon *pIcon);
/** Update the icon's quick-info texture with its current quick-info surface.
*@param pIcon the icon.
*/
void cairo_dock_update_quick_info_texture (Icon *pIcon);

void cairo_dock_draw_hidden_appli_icon (Icon *pIcon, CairoContainer *pContainer, gboolean bStateChanged);

  //////////////////
 // DRAW TEXTURE //
//////////////////
/** Enable texture drawing.
*/
#define _cairo_dock_enable_texture(...) do { \
	glEnable (GL_BLEND);\
	glEnable (GL_TEXTURE_2D);\
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);\
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);\
	glEnable (GL_LINE_SMOOTH);\
	glPolygonMode (GL_FRONT, GL_FILL); } while (0)

/** Disable texture drawing.
*/
#define _cairo_dock_disable_texture(...) do { \
	glDisable (GL_TEXTURE_2D);\
	glDisable (GL_LINE_SMOOTH);\
	glDisable (GL_BLEND); } while (0)

/** Set the alpha channel to a current value, other channels are set to 1.
*@param fAlpha 
*/
#define _cairo_dock_set_alpha(fAlpha) glColor4f (1., 1., 1., fAlpha)

/** Set the color blending to overwrite.
*/
#define _cairo_dock_set_blend_source(...) glBlendFunc (GL_ONE, GL_ZERO)

/** Set the color blending to mix, for premultiplied texture.
*/
#define _cairo_dock_set_blend_alpha(...) glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)

/** Set the color blending to mix.
*/
#define _cairo_dock_set_blend_over(...) glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

/** Set the color blending to mix on a pbuffer.
*/
#define _cairo_dock_set_blend_pbuffer(...) glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA)

#define _cairo_dock_apply_current_texture_at_size(w, h) do { \
	glBegin(GL_QUADS);\
	glTexCoord2f(0., 0.); glVertex3f(-.5*w,  .5*h, 0.);\
	glTexCoord2f(1., 0.); glVertex3f( .5*w,  .5*h, 0.);\
	glTexCoord2f(1., 1.); glVertex3f( .5*w, -.5*h, 0.);\
	glTexCoord2f(0., 1.); glVertex3f(-.5*w, -.5*h, 0.);\
	glEnd(); } while (0)
#define _cairo_dock_apply_current_texture_at_size_with_offset(w, h, x, y) do { \
	glBegin(GL_QUADS);\
	glTexCoord2f(0., 0.); glVertex3f(x-.5*w, y+.5*h, 0.);\
	glTexCoord2f(1., 0.); glVertex3f(x+.5*w, y+.5*h, 0.);\
	glTexCoord2f(1., 1.); glVertex3f(x+.5*w, y-.5*h, 0.);\
	glTexCoord2f(0., 1.); glVertex3f(x-.5*w, y-.5*h, 0.);\
	glEnd(); } while (0)
#define _cairo_dock_apply_current_texture_portion_at_size_with_offset(u, v, du, dv, w, h, x, y) do { \
	glBegin(GL_QUADS);\
	glTexCoord2f(u, v); glVertex3f(x-.5*w, y+.5*h, 0.);\
	glTexCoord2f(u+du, v); glVertex3f(x+.5*w, y+.5*h, 0.);\
	glTexCoord2f(u+du, v+dv); glVertex3f(x+.5*w, y-.5*h, 0.);\
	glTexCoord2f(u, v+dv); glVertex3f(x-.5*w, y-.5*h, 0.);\
	glEnd(); } while (0)

/** Draw a texture centered on the current point, at a given size.
*@param iTexture the texture
*@param w width
*@param h height
*/
#define _cairo_dock_apply_texture_at_size(iTexture, w, h) do { \
	glBindTexture (GL_TEXTURE_2D, iTexture);\
	_cairo_dock_apply_current_texture_at_size (w, h); } while (0)

/** Apply a texture centered on the current point and at the given scale.
*@param iTexture the texture
*/
#define _cairo_dock_apply_texture(iTexture) _cairo_dock_apply_texture_at_size (iTexture, 1, 1)

/** Draw a texture centered on the current point, at a given size, and with a given transparency.
*@param iTexture the texture
*@param w width
*@param h height
*@param fAlpha the transparency, between 0 and 1.
*/
#define _cairo_dock_apply_texture_at_size_with_alpha(iTexture, w, h, fAlpha) do { \
	_cairo_dock_set_alpha (fAlpha);\
	_cairo_dock_apply_texture_at_size (iTexture, w, h); } while (0)

#define cairo_dock_apply_texture _cairo_dock_apply_texture
#define cairo_dock_apply_texture_at_size _cairo_dock_apply_texture_at_size
void cairo_dock_draw_texture_with_alpha (GLuint iTexture, int iWidth, int iHeight, double fAlpha);
void cairo_dock_draw_texture (GLuint iTexture, int iWidth, int iHeight);

void cairo_dock_apply_icon_texture (Icon *pIcon);
void cairo_dock_apply_icon_texture_at_current_size (Icon *pIcon, CairoContainer *pContainer);
void cairo_dock_draw_icon_texture (Icon *pIcon, CairoContainer *pContainer);


  /////////////
 // GL PATH //
/////////////

struct _CairoDockGLPath {
	int iNbPoints;
	GLfloat *pVertices;
	int iCurrentPt;
	};

CairoDockGLPath *cairo_dock_new_gl_path (int iNbVertices, double x0, double y0);

void cairo_dock_free_gl_path (CairoDockGLPath *pPath);

void cairo_dock_gl_path_line_to (CairoDockGLPath *pPath, GLfloat x, GLfloat y);

void cairo_dock_gl_path_rel_line_to (CairoDockGLPath *pPath, GLfloat dx, GLfloat dy);

void cairo_dock_gl_path_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2, GLfloat x3, GLfloat y3);

void cairo_dock_gl_path_rel_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2, GLfloat dx3, GLfloat dy3);

void cairo_dock_gl_path_simple_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);

void cairo_dock_gl_path_rel_simple_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2);

void cairo_dock_gl_path_arc_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat xc, GLfloat yc, double cone);

void cairo_dock_close_gl_path (CairoDockGLPath *pPath);

void cairo_dock_gl_path_move_to (CairoDockGLPath *pPath, double x0, double y0);

void cairo_dock_stroke_gl_path (CairoDockGLPath *pPath, gboolean bFill);


#define _CAIRO_DOCK_PATH_DIM 2
#define _cairo_dock_define_static_vertex_tab(iNbVertices) static GLfloat pVertexTab[(iNbVertices) * _CAIRO_DOCK_PATH_DIM]
#define _cairo_dock_return_vertex_tab(...) return pVertexTab
#define _cairo_dock_set_vertex_x(_i, _x) pVertexTab[_CAIRO_DOCK_PATH_DIM*_i] = _x
#define _cairo_dock_set_vertex_y(_i, _y) pVertexTab[_CAIRO_DOCK_PATH_DIM*_i+1] = _y
#define _cairo_dock_set_vertex_xy(_i, _x, _y) do {\
	_cairo_dock_set_vertex_x(_i, _x);\
	_cairo_dock_set_vertex_y(_i, _y); } while (0)
#define _cairo_dock_close_path(i) _cairo_dock_set_vertex_xy (i, pVertexTab[0], pVertexTab[1])
#define _cairo_dock_set_vertex_pointer(pVertexTab) glVertexPointer (_CAIRO_DOCK_PATH_DIM, GL_FLOAT, 0, pVertexTab)

const GLfloat *cairo_dock_generate_rectangle_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, int *iNbPoints);
GLfloat *cairo_dock_generate_trapeze_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, double fInclination, double *fExtraWidth, int *iNbPoints);

void cairo_dock_draw_frame_background_opengl (GLuint iBackgroundTexture, double fDockWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, const GLfloat *pVertexTab, int iNbVertex, CairoDockTypeHorizontality bHorizontal, gboolean bDirectionUp, double fDecorationsOffsetX);
void cairo_dock_draw_current_path_opengl (double fLineWidth, double *fLineColor, int iNbVertex);

/** Draw a rectangle with rounded corners. The rectangle will be centered at the current point. The current matrix is altered.
*@param fRadius radius if the corners.
*@param fLineWidth width of the line. If set to 0, the background will be filled with the provided color, otherwise the path will be stroke with this color.
*@param fFrameWidth width of the rectangle, without the corners.
*@param fFrameHeight height of the rectangle, including the corners.
*@param fDockOffsetX translation on X before drawing the rectangle.
*@param fDockOffsetY translation on Y before drawing the rectangle.
*@param fLineColor color of the line if fLineWidth is non nul, or color of the background otherwise.
*/
void cairo_dock_draw_rounded_rectangle_opengl (double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, double *fLineColor);

GLfloat *cairo_dock_generate_string_path_opengl (CairoDock *pDock, gboolean bIsLoop, gboolean bForceConstantSeparator, int *iNbPoints);
void cairo_dock_draw_string_opengl (CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator);


  /////////////
 // GL FONT //
/////////////

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

/** Load a font into bitmaps. You can load any characters of font with this function. The drawback is that each character is a bitmap, that is to say you can't zoom them.
*@param cFontDescription a description of the font, for instance "Monospace Bold 12"
*@param first first character to load.
*@param count number of characters to load.
*@return a newly allocated opengl font.
*/
CairoDockGLFont *cairo_dock_load_bitmap_font (const gchar *cFontDescription, int first, int count);

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


GLuint cairo_dock_texture_from_pixmap (Window Xid, Pixmap iBackingPixmap);


G_END_DECLS
#endif

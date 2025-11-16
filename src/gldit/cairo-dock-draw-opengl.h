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

#include <GL/glu.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-container.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-draw-opengl.h This class provides some useful functions to draw with OpenGL.
*/

void cairo_dock_set_icon_scale (Icon *pIcon, GldiContainer *pContainer, double fZoomFactor);

void cairo_dock_set_container_orientation_opengl (GldiContainer *pContainer);

void cairo_dock_draw_icon_reflect_opengl (Icon *pIcon, CairoDock *pDock);

void cairo_dock_draw_icon_opengl (Icon *pIcon, CairoDock *pDock);

void cairo_dock_translate_on_icon_opengl (Icon *icon, GldiContainer *pContainer, double fDockMagnitude);

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
/** Load a cairo surface into an OpenGL texture. The surface can be destroyed after that if you don't need it.
 *  The texture will have the same (physical) size as the surface, but potentially rounded up to the nearest
 *  power of 2 if needed.
*@param pImageSurface the surface, created with one of the 'cairo_dock_create_surface_xxx' functions.
*@param pWidth if not NULL, return the actual width of the newly allocated texture here
*@param pHeight if not NULL, return the actual width of the newly allocated texture here
*@return the newly allocated texture, to be destroyed with _cairo_dock_delete_texture.
*/
GLuint cairo_dock_create_texture_from_surface_full (cairo_surface_t *pImageSurface, int *pWidth, int *pHeight);

/** Load a cairo surface into an OpenGL texture. The surface can be destroyed after that if you don't need it.
 *  The texture will have the same (physical) size as the surface, but potentially rounded up to the nearest
 *  power of 2 if needed. This function is the same as cairo_dock_create_texture_from_surface_full () but does
 *  not return the size of the new texture.
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
GLuint cairo_dock_create_texture_from_raw_data (const guchar *pTextureRaw, int iWidth, int iHeight);

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

void cairo_dock_draw_hidden_appli_icon (Icon *pIcon, GldiContainer *pContainer, gboolean bStateChanged);

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
*@param fAlpha alpha
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
#define _cairo_dock_apply_current_texture_at_size_crop(iTexture, w, h, ratio) do { \
	glBegin(GL_QUADS);\
	glTexCoord2f(0., 1-ratio); glVertex3f(-.5*w,  (ratio -.5)*h, 0.);\
	glTexCoord2f(1., 1-ratio); glVertex3f( .5*w,  (ratio -.5)*h, 0.);\
	glTexCoord2f(1., 1); glVertex3f( .5*w, -.5*h, 0.);\
	glTexCoord2f(0., 1); glVertex3f(-.5*w, -.5*h, 0.);\
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
void cairo_dock_apply_icon_texture_at_current_size (Icon *pIcon, GldiContainer *pContainer);
void cairo_dock_draw_icon_texture (Icon *pIcon, GldiContainer *pContainer);


G_END_DECLS
#endif

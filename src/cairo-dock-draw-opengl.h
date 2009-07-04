
#ifndef __CAIRO_DOCK_DRAW_OPENGL__
#define  __CAIRO_DOCK_DRAW_OPENGL__

#include <glib.h>

#include <gdk/x11/gdkglx.h>
#include <gtk/gtkgl.h>
#include <GL/glu.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-draw-opengl.h This class provides some useful functions to draw with OpenGL.
*/

void cairo_dock_set_icon_scale (Icon *pIcon, CairoContainer *pContainer, double fZoomFactor);

gboolean cairo_dock_render_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext);

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

/** Load an image on the dick into an OpenGL texture. The texture will have the same size as the image. The size is given as an output, if you need it for some reason.
*@param cImagePath path to an image.
*@param fImageWidth pointer that will be filled with the width of the image.
*@param fImageHeight pointer that will be filled with the height of the image.
*@return the newly allocated texture, to be destroyed with _cairo_dock_delete_texture.
*/
GLuint cairo_dock_create_texture_from_image_full (const gchar *cImagePath, double *fImageWidth, double *fImageHeight);

/** Load an image on the dick into an OpenGL texture. The texture will have the same size as the image.
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

  //////////////////
 // DRAW TEXTURE //
//////////////////
/** Enable texture drawing.
*/
#define _cairo_dock_enable_texture(...) do { \
	glEnable (GL_BLEND);\
	glEnable (GL_TEXTURE_2D);\
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);\
	glHint (GL_POLYGON_SMOOTH_HINT, GL_NICEST);\
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

  ///////////////
 // DRAW PATH //
///////////////
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

/** Draw a rectangle with rounded corners. The rectangle will be centered at the current point.
*@param fRadius radius if the corners.
*@param fLineWidth width of the line. If set to 0, the background will be filled with the provided color, otherwise the path will be stroke.
*@param fFrameWidth width of the rectangle, without the corners.
*@param fFrameHeight height of the rectangle, including the corners.
*@param fDockOffsetX translation on X before drawing the rectangle.
*@param fDockOffsetY translation on Y before drawing the rectangle.
*@param fLineColor color of the line if fLineWidth is non nul, or color of the background otherwise.
*/
void cairo_dock_draw_rounded_rectangle_opengl (double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, double *fLineColor);

GLfloat *cairo_dock_generate_string_path_opengl (CairoDock *pDock, gboolean bIsLoop, gboolean bForceConstantSeparator, int *iNbPoints);
void cairo_dock_draw_string_opengl (CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator);


  ///////////////////////
 // RENDER TO TEXTURE //
///////////////////////
GLXPbuffer cairo_dock_create_pbuffer (int iWidth, int iHeight, GLXContext *pContext);
void cairo_dock_create_icon_pbuffer (void);
void cairo_dock_destroy_icon_pbuffer (void);

/** Initiate an OpenGL drawing session on an icon's texture.
*@param pIcon the icon on which to draw.
*@param pContainer its container.
*@return TRUE if you can proceed to the drawing, FALSE if an error occured.
*/
gboolean cairo_dock_begin_draw_icon (Icon *pIcon, CairoContainer *pContainer);
/** Finish an OpenGL drawing session on an icon.
*@param pIcon the icon on which to draw.
*@param pContainer its container.
*@return TRUE if you can proceed to the drawing, FALSE if an error occured.
*/
void cairo_dock_end_draw_icon (Icon *pIcon, CairoContainer *pContainer);


  /////////////
 // CONTEXT //
/////////////
/** Set a perspective view to the current GL context. Perspective view accentuates the depth effect of the scene, but can distort it on the edges, and is difficult to manipulate because the size of objects depends on their position.
*@param iWidth width of the container
*@param iHeight height of the container
*/
void cairo_dock_set_perspective_view (int iWidth, int iHeight);
/** Set an orthogonal view to the current GL context. Orthogonal view is convenient to draw classic 2D, because the objects are not zoomed according to their position. The drawback is a poor depth effect.
*@param iWidth width of the container
*@param iHeight height of the container
*/
void cairo_dock_set_ortho_view (int iWidth, int iHeight);

GdkGLConfig *cairo_dock_get_opengl_config (gboolean bForceOpenGL, gboolean *bHasBeenForced);

void cairo_dock_apply_desktop_background (CairoContainer *pContainer);


G_END_DECLS
#endif

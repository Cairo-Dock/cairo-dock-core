
#ifndef __CAIRO_DOCK_DRAW_OPENGL__
#define  __CAIRO_DOCK_DRAW_OPENGL__

#include <glib.h>

#include <gdk/x11/gdkglx.h>
#include <gtk/gtkgl.h>
#include <GL/glu.h>

#include "cairo-dock-struct.h"

G_BEGIN_DECLS


void cairo_dock_set_icon_scale (Icon *pIcon, CairoContainer *pContainer, double fZoomFactor);

gboolean cairo_dock_render_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext);

void cairo_dock_render_one_icon_opengl (Icon *icon, CairoDock *pDock, double fDockMagnitude, gboolean bUseText);

void cairo_dock_render_hidden_dock_opengl (CairoDock *pDock);

////////////////////
/// LOAD TEXTURE ///
////////////////////
GLuint cairo_dock_create_texture_from_surface (cairo_surface_t *pImageSurface);

GLuint cairo_dock_load_texture_from_raw_data (const guchar *pTextureRaw, int iWidth, int iHeight);

GLuint cairo_dock_create_texture_from_image_full (const gchar *cImagePath, double *fImageWidth, double *fImageHeight);
#define cairo_dock_create_texture_from_image(cImagePath) cairo_dock_create_texture_from_image_full (cImagePath, NULL, NULL)

GLuint cairo_dock_load_local_texture (const gchar *cImageName, const gchar *cDirPath);

#define _cairo_dock_delete_texture(iTexture) glDeleteTextures (1, &iTexture)


void cairo_dock_update_icon_texture (Icon *pIcon);
void cairo_dock_update_label_texture (Icon *pIcon);
void cairo_dock_update_quick_info_texture (Icon *pIcon);

////////////////////
/// DRAW TEXTURE ///
////////////////////
#define _cairo_dock_enable_texture(...) do { \
	glEnable (GL_BLEND);\
	glEnable (GL_TEXTURE_2D);\
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);\
	glHint (GL_POLYGON_SMOOTH_HINT, GL_NICEST);\
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);\
	glEnable (GL_LINE_SMOOTH);\
	glPolygonMode (GL_FRONT, GL_FILL); } while (0)

#define _cairo_dock_disable_texture(...) do { \
	glDisable (GL_TEXTURE_2D);\
	glDisable (GL_LINE_SMOOTH);\
	glDisable (GL_BLEND); } while (0)

#define _cairo_dock_set_alpha(fAlpha) glColor4f (1., 1., 1., fAlpha)

#define _cairo_dock_set_blend_source(...) glBlendFunc (GL_ONE, GL_ZERO)

#define _cairo_dock_set_blend_alpha(...) glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)

#define _cairo_dock_set_blend_over(...) glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

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


#define _cairo_dock_apply_texture_at_size(iTexture, w, h) do { \
	glBindTexture (GL_TEXTURE_2D, iTexture);\
	_cairo_dock_apply_current_texture_at_size (w, h); } while (0)

#define _cairo_dock_apply_texture(iTexture) _cairo_dock_apply_texture_at_size (iTexture, 1, 1)

#define _cairo_dock_apply_texture_at_size_with_alpha(iTexture, w, h, fAlpha) do { \
	_cairo_dock_set_alpha (fAlpha);\
	_cairo_dock_apply_texture_at_size (iTexture, w, h); } while (0)

void cairo_dock_apply_texture (GLuint iTexture);
void cairo_dock_apply_texture_at_size (GLuint iTexture, int iWidth, int iHeight);
void cairo_dock_draw_texture_with_alpha (GLuint iTexture, int iWidth, int iHeight, double fAlpha);
void cairo_dock_draw_texture (GLuint iTexture, int iWidth, int iHeight);

void cairo_dock_apply_icon_texture (Icon *pIcon);
void cairo_dock_apply_icon_texture_at_current_size (Icon *pIcon, CairoContainer *pContainer);
void cairo_dock_draw_icon_texture (Icon *pIcon, CairoContainer *pContainer);

/////////////////
/// DRAW PATH ///
/////////////////
const GLfloat *cairo_dock_generate_rectangle_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, int *iNbPoints);
GLfloat *cairo_dock_generate_trapeze_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, double fInclination, double *fExtraWidth, int *iNbPoints);

void cairo_dock_draw_frame_background_opengl (GLuint iBackgroundTexture, double fDockWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, const GLfloat *pVertexTab, int iNbVertex, CairoDockTypeHorizontality bHorizontal, gboolean bDirectionUp, double fDecorationsOffsetX);
void cairo_dock_draw_current_path_opengl (double fLineWidth, double *fLineColor, int iNbVertex);

GLfloat *cairo_dock_generate_string_path_opengl (CairoDock *pDock, gboolean bIsLoop, gboolean bForceConstantSeparator, int *iNbPoints);
void cairo_dock_draw_string_opengl (CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator);


  /////////////////////////
 /// RENDER TO TEXTURE ///
/////////////////////////
GLXPbuffer cairo_dock_create_pbuffer (int iWidth, int iHeight, GLXContext *pContext);
void cairo_dock_create_icon_pbuffer (void);
gboolean cairo_dock_begin_draw_icon (Icon *pIcon, CairoContainer *pContainer);
void cairo_dock_end_draw_icon (Icon *pIcon, CairoContainer *pContainer);


  ///////////////
 /// CONTEXT ///
///////////////
void cairo_dock_set_perspective_view (int iWidth, int iHeight);
void cairo_dock_set_ortho_view (int iWidth, int iHeight);

GdkGLConfig *cairo_dock_get_opengl_config (gboolean bForceOpenGL, gboolean *bHasBeenForced);

void cairo_dock_apply_desktop_background (CairoContainer *pContainer);


G_END_DECLS
#endif

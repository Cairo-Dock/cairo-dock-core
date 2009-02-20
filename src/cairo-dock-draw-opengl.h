
#ifndef __CAIRO_DOCK_DRAW_OPENGL__
#define  __CAIRO_DOCK_DRAW_OPENGL__

#include <glib.h>
#include <gtk/gtkgl.h>

#include "cairo-dock-struct.h"

G_BEGIN_DECLS


void cairo_dock_set_icon_scale (Icon *pIcon, CairoContainer *pContainer, double fZoomFactor);

gboolean cairo_dock_render_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext);

void cairo_dock_render_one_icon_opengl (Icon *icon, CairoDock *pDock, double fDockMagnitude, gboolean bUseText);

GLuint cairo_dock_create_texture_from_surface (cairo_surface_t *pImageSurface);


void cairo_dock_draw_drop_indicator_opengl (CairoDock *pDock);


GLuint cairo_dock_load_texture_from_raw_data (const guchar *pTextureRaw, int iWidth, int iHeight);


void cairo_dock_update_icon_texture (Icon *pIcon);
void cairo_dock_update_label_texture (Icon *pIcon);
void cairo_dock_update_quick_info_texture (Icon *pIcon);

GLuint cairo_dock_create_texture_from_image_full (const gchar *cImagePath, double *fImageWidth, double *fImageHeight);
#define cairo_dock_create_texture_from_image(cImagePath) cairo_dock_create_texture_from_image_full (cImagePath, NULL, NULL)

GLuint cairo_dock_load_local_texture (const gchar *cImageName, const gchar *cDirPath);

void cairo_dock_render_background_opengl (CairoDock *pDock);


void cairo_dock_apply_texture (GLuint iTexture);
void cairo_dock_draw_texture (GLuint iTexture, int iWidth, int iHeight);
void cairo_dock_apply_icon_texture (Icon *pIcon);
void cairo_dock_draw_icon_texture (Icon *pIcon, CairoContainer *pContainer);


const GLfloat *cairo_dock_generate_rectangle_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, int *iNbPoints);
GLfloat *cairo_dock_generate_trapeze_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, double fInclination, double *fExtraWidth, int *iNbPoints);

void cairo_dock_draw_frame_background_opengl (GLuint iBackgroundTexture, double fDockWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, const GLfloat *pVertexTab, int iNbVertex, CairoDockTypeHorizontality bHorizontal, gboolean bDirectionUp, double fDecorationsOffsetX);
void cairo_dock_draw_current_path_opengl (double fLineWidth, double *fLineColor, const GLfloat *pVertexTab, int iNbVertex);

GdkGLConfig *cairo_dock_get_opengl_config (gboolean bForceOpenGL, gboolean *bHasBeenForced);

void cairo_dock_apply_desktop_background (CairoContainer *pContainer);


GLXPbuffer cairo_dock_create_pbuffer (int iWidth, int iHeight, GLXContext *pContext);
void cairo_dock_create_icon_pbuffer (void);
gboolean cairo_dock_begin_draw_icon (Icon *pIcon, CairoContainer *pContainer);
void cairo_dock_end_draw_icon (Icon *pIcon, CairoContainer *pContainer);


#define cairo_dock_set_inverted_texture(icon) (icon)->bInvertedTexture = TRUE
#define cairo_dock_texture_is_inverted(icon) ((icon)->bInvertedTexture)


G_END_DECLS
#endif

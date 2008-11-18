

#ifndef __CAIRO_DOCK_DARW_OPENGL__
#define  __CAIRO_DOCK_DARW_OPENGL__

#include <glib.h>

#include "cairo-dock-struct.h"


void cairo_dock_set_icon_scale (Icon *pIcon, CairoDock *pDock, double fZoomFactor);

gboolean cairo_dock_render_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered);

void cairo_dock_render_one_icon_opengl (Icon *icon, CairoDock *pDock, double fRatio, double fDockMagnitude, gboolean bUseText);

GLuint cairo_dock_create_texture_from_surface (cairo_surface_t *pImageSurface);


void cairo_dock_draw_drop_indicator_opengl (CairoDock *pDock);


GLuint cairo_dock_load_texture_from_raw_data (const guchar *pTextureRaw);


void cairo_dock_update_icon_texture (Icon *pIcon);
void cairo_dock_update_label_texture (Icon *pIcon);
void cairo_dock_update_quick_info_texture (Icon *pIcon);

GLuint cairo_dock_create_texture_from_image_full (const gchar *cImagePath, double *fImageWidth, double *fImageHeight);
#define cairo_dock_create_texture_from_image(cImagePath) cairo_dock_create_texture_from_image_full (cImagePath, NULL, NULL)

GLuint cairo_dock_load_local_texture (const gchar *cImageName, const gchar *cDirPath);

void cairo_dock_render_background_opengl (CairoDock *pDock);

void cairo_dock_draw_texture (GLuint iTexture, int iWidth, int iHeight);


const GLfloat *cairo_dock_draw_rectangle (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, int *iNbPoints);
GLfloat *cairo_dock_draw_trapeze (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, double fInclination, double *fExtraWidth, int *iNbPoints);


#endif

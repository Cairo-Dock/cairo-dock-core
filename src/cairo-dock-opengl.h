
#ifndef __CAIRO_DOCK_OPENGL__
#define  __CAIRO_DOCK_OPENGL__

#include <glib.h>

#include "cairo-dock-opengl.h"


#define cairo_dock_begin_opengl_draw(pContainer) \
	GdkGLContext *_pGlContext = gtk_widget_get_gl_context (pContainer->pWidget);\
	GdkGLDrawable *_pGlDrawable = gtk_widget_get_gl_drawable (pContainer->pWidget);\
	if (!gdk_gl_drawable_gl_begin (_pGlDrawable, _pGlContext))\
		return;

#define cairo_dock_end_opengl_draw(...) gdk_gl_drawable_gl_end (_pGlDrawable)


void cairo_dock_initialize_gl_context (CairoContainer *pContainer);


void cairo-dock_draw_texture (GLuint iTexture, double fAlpha);
void cairo-dock_draw_texture_with_size (GLuint iTexture, double fAlpha, double fWidth, double fHeight);


#endif

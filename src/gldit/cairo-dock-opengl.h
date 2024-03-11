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

#ifndef __CAIRO_DOCK_OPENGL__
#define  __CAIRO_DOCK_OPENGL__

#include <glib.h>

#include "gldi-config.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-opengl.h This class manages the OpenGL backend and context.
*/

/// This strucure summarizes the available OpenGL configuration on the system.
struct _CairoDockGLConfig {
	gboolean bIndirectRendering;
	gboolean bAlphaAvailable;
	gboolean bStencilBufferAvailable;
	gboolean bAccumBufferAvailable;
	gboolean bFboAvailable;
	gboolean bNonPowerOfTwoAvailable;
	gboolean bTextureFromPixmapAvailable;
	#ifdef HAVE_GLX
	void (*bindTexImage) (Display *display, GLXDrawable drawable, int buffer, int *attribList);  // texture from pixmap
	void (*releaseTexImage) (Display *display, GLXDrawable drawable, int buffer);  // texture from pixmap
	#elif defined(HAVE_EGL)
	void (*bindTexImage) (EGLDisplay *display, EGLSurface drawable, int buffer);  // texture from pixmap
	void (*releaseTexImage) (EGLDisplay *display, EGLSurface drawable, int buffer);  // texture from pixmap
	#endif
};

struct _GldiGLManagerBackend {
	gboolean (*init) (gboolean bForceOpenGL);
	void (*stop) (void);
	gboolean (*container_make_current) (GldiContainer *pContainer);
	void (*container_end_draw) (GldiContainer *pContainer);
	void (*container_init) (GldiContainer *pContainer);
	void (*container_finish) (GldiContainer *pContainer);
	void (*container_resized) (GldiContainer *pContainer, int iWidth, int iHeight);
};
	


  /////////////
 // BACKEND //
/////////////
/** Initialize the OpenGL backend, by trying to get a suitable GLX configuration.
*@param bForceOpenGL whether to force the use of OpenGL, or let the function decide.
*@return TRUE if OpenGL is usable.
*/
gboolean gldi_gl_backend_init (gboolean bForceOpenGL);

void gldi_gl_backend_deactivate (void);

#define gldi_gl_backend_is_safe(...) (g_bUseOpenGL && ! g_openglConfig.bIndirectRendering && g_openglConfig.bAlphaAvailable && g_openglConfig.bStencilBufferAvailable)  // bNonPowerOfTwoAvailable et FBO sont detectes une fois qu'on a un contexte OpenGL, on ne peut donc pas les inclure ici.

/* Toggle on/off the indirect rendering mode (for cards like Radeon 35xx).
*/
void gldi_gl_backend_force_indirect_rendering (void);


  ///////////////
 // CONTAINER //
///////////////

/** Make a Container's OpenGL context the current one.
*@param pContainer the container
*@return TRUE if the Container's context is now the current one.
*/
gboolean gldi_gl_container_make_current (GldiContainer *pContainer);

/** Start drawing on a Container's OpenGL context.
*@param pContainer the container
*@param pArea optional area to clip the drawing (NULL to draw on the whole Container)
*@param bClear whether to clear the color buffer or not
*/
gboolean gldi_gl_container_begin_draw_full (GldiContainer *pContainer, GdkRectangle *pArea, gboolean bClear);

/** Start drawing on a Container's OpenGL context (draw on the whole Container and clear buffers).
*@param pContainer the container
*/
#define gldi_gl_container_begin_draw(pContainer) gldi_gl_container_begin_draw_full (pContainer, NULL, TRUE)

/** Ends the drawing on a Container's OpenGL context.
*@param pContainer the container
*/
void gldi_gl_container_end_draw (GldiContainer *pContainer);


/** Set a perspective view to the current GL context to fit a given Container. You may want to ensure the Container's context is really the current one.
*@param pContainer the container
*/
void gldi_gl_container_set_perspective_view (GldiContainer *pContainer);

/** Set a perspective view to the current GL context to fit a given Icon (which must be inside a Container). You may want to ensure the Icon's Container's context is really the current one.
*@param pIcon the icon
*/
void gldi_gl_container_set_perspective_view_for_icon (Icon *pIcon);

/** Set a orthogonal view to the current GL context to fit a given Container. You may want to ensure the Container's context is really the current one.
*@param pContainer the container
*/
void gldi_gl_container_set_ortho_view (GldiContainer *pContainer);

/** Set a orthogonal view to the current GL context to fit a given Icon (which must be inside a Container). You may want to ensure the Icon's Container's context is really the current one.
*@param pIcon the icon
*/
void gldi_gl_container_set_ortho_view_for_icon (Icon *pIcon);

/** Set a shared default-initialized GL context on a window.
*@param pContainer the container, not yet realized.
*/
void gldi_gl_container_init (GldiContainer *pContainer);

/** Should be called when the window is resized so that the
 *  associated EGL surface can be resized -- needed on Wayland,
 * 	pContainer->iWidth and iHeight should be set to the desired
 *  new size */
void gldi_gl_container_resized (GldiContainer *pContainer, int iWidth, int iHeight);

void gldi_gl_container_finish (GldiContainer *pContainer);


void gldi_gl_manager_register_backend (GldiGLManagerBackend *pBackend);

G_END_DECLS
#endif

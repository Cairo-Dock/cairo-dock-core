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

#include <GL/glx.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-opengl.h This class manages the OpenGL backend and context.
*/

/// This strucure summarizes the available OpenGL configuration on the system.
struct _CairoDockGLConfig {
	///GdkGLConfig *pGlConfig;
	GLXContext context;
	XVisualInfo *pVisInfo;
	#if (GTK_MAJOR_VERSION < 3)
	Colormap xcolormap;
	GdkColormap *pColormap;
	#else  // GTK3
	GdkVisual *pGdkVisual;
	#endif
	gboolean bIndirectRendering;
	gboolean bAlphaAvailable;
	gboolean bStencilBufferAvailable;
	gboolean bAccumBufferAvailable;
	gboolean bFboAvailable;
	gboolean bNonPowerOfTwoAvailable;
	gboolean bTextureFromPixmapAvailable;
	GLuint iFboId;
	GLuint iRedirectedTexture;
	gint iRedirectWidth, iRedirectHeight;
	gboolean bRedirected;
	gboolean bSetPerspective;
	void (*bindTexImage) (Display *display, GLXDrawable drawable, int buffer, int *attribList);
	void (*releaseTexImage) (Display *display, GLXDrawable drawable, int buffer);
	} ;


  ///////////////////
 // CONFIGURATION //
///////////////////
/** Initialize the OpenGL backend, by trying to get a suitable GLX configuration.
*@param bForceOpenGL whether to force the use of OpenGL, or let the function decide.
*@return TRUE if OpenGL is usable.
*/
gboolean cairo_dock_initialize_opengl_backend (gboolean bForceOpenGL);

#define cairo_dock_opengl_is_safe(...) (g_openglConfig.context != 0 && ! g_openglConfig.bIndirectRendering && g_openglConfig.bAlphaAvailable && g_openglConfig.bStencilBufferAvailable)  // bNonPowerOfTwoAvailable et FBO sont detectes une fois qu'on a un contexte OpenGL, on ne peut donc pas les inclure ici.

/* Toggle on/off the indirect rendering mode (for cards like Radeon 35xx).
*@param bToggleIndirectRendering whether to toggle on/off the indirect rendering mode that have been detected by the function (for cards like Radeon 35xx).
*/
void cairo_dock_force_indirect_rendering (void);

#define cairo_dock_deactivate_opengl(...) do {\
	g_bUseOpenGL = FALSE;\
	g_openglConfig.context = 0; } while (0)


  ///////////////////////
 // RENDER TO TEXTURE //
///////////////////////
/** Create an FBO to render the icons inside a dock.
*/
void cairo_dock_create_icon_fbo (void);
/** Destroy the icons FBO.
*/
void cairo_dock_destroy_icon_fbo (void);

/** Initiate an OpenGL drawing session on an icon's texture.
*@param pIcon the icon on which to draw.
*@param pContainer its container, or NULL if the icon is not yet inside a container.
*@param iRenderingMode rendering mode. 0:normal, 1:don't clear the current texture, so that the drawing will be superimposed on it, 2:keep the current icon texture unchanged for all the drawing (the drawing is made on another texture).
*@return TRUE if you can proceed to the drawing, FALSE if an error occured.
*/
gboolean cairo_dock_begin_draw_icon (Icon *pIcon, CairoContainer *pContainer, gint iRenderingMode);
/** Finish an OpenGL drawing session on an icon.
*@param pIcon the icon on which to draw.
*@param pContainer its container, or NULL if the icon is not yet inside a container.
*@return TRUE if you can proceed to the drawing, FALSE if an error occured.
*/
void cairo_dock_end_draw_icon (Icon *pIcon, CairoContainer *pContainer);


  /////////////
 // CONTEXT //
/////////////

gboolean gldi_glx_make_current (CairoContainer *pContainer);

gboolean gldi_glx_begin_draw_container_full (CairoContainer *pContainer, gboolean bClear);

#define gldi_glx_begin_draw_container(pContainer) gldi_glx_begin_draw_container_full (pContainer, TRUE)

void gldi_glx_end_draw_container (CairoContainer *pContainer);


/** Set a perspective view to the current GL context to fit a given ontainer. Perspective view accentuates the depth effect of the scene, but can distort it on the edges, and is difficult to manipulate because the size of objects depends on their position.
*@param pContainer the container
*/
void cairo_dock_set_perspective_view (CairoContainer *pContainer);

void cairo_dock_set_perspective_view_for_icon (Icon *pIcon, CairoContainer *pContainer);

/** Set an orthogonal view to the current GL context to fit a given ontainer. Orthogonal view is convenient to draw classic 2D, because the objects are not zoomed according to their position. The drawback is a poor depth effect.
*@param pContainer the container
*/
void cairo_dock_set_ortho_view (CairoContainer *pContainer);

void cairo_dock_set_ortho_view_for_icon (Icon *pIcon, CairoContainer *pContainer);


/** Apply the desktop background onto a container, to emulate fake transparency.
*@param pContainer the container
*/
void gldi_glx_apply_desktop_background (CairoContainer *pContainer);

/** Set a shared default-initialized GL context on a window.
*@param pContainer the container, not yet realized.
*/
void gldi_glx_init_container (CairoContainer *pContainer);

void gldi_glx_finish_container (CairoContainer *pContainer);


G_END_DECLS
#endif

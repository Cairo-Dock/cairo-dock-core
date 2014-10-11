/**
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

#include "gldi-config.h"
#ifdef HAVE_EGL

#include <EGL/egl.h>

#ifdef HAVE_X11
#include <gdk/gdkx.h>  // GDK_WINDOW_XID
#define _gldi_container_get_Xid(pContainer) GDK_WINDOW_XID (gldi_container_get_gdk_window(pContainer))
#endif

#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_string_contains
#include "cairo-dock-opengl.h"

// dependencies
extern CairoDockGLConfig g_openglConfig;
extern GldiContainer *g_pPrimaryContainer;

// private
static EGLDisplay *s_eglDisplay = NULL;
static EGLContext s_eglContext = 0;
static EGLConfig s_eglConfig = 0;

static gboolean _check_client_egl_extension (const char *extName)
{
	EGLDisplay *display = s_eglDisplay;
	const char *eglExtensions = eglQueryString (display, EGL_EXTENSIONS);
	return cairo_dock_string_contains (eglExtensions, extName, " ");
}

static gboolean _initialize_opengl_backend (gboolean bForceOpenGL)
{
	gboolean bStencilBufferAvailable = TRUE, bAlphaAvailable = TRUE;
	
	// open a connection (= Display) to the graphic server
	EGLNativeDisplayType display_id = EGL_DEFAULT_DISPLAY;  // XDisplay*, wl_display*, etc; Note: we could pass our X Display instead of making a new connection...
	EGLDisplay *dpy = s_eglDisplay = eglGetDisplay (display_id);
	
	int major, minor;
	if (! eglInitialize (dpy, &major, &minor))
	{
		cd_warning ("Can't initialise EGL display, OpenGL will not be available");
		return FALSE;
	}
	g_print ("EGL version: %d;%d\n", major, minor);
	
	// find a Frame Buffer Configuration (= Visual) that supports the features we need
	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,  // EGL_OPENGL_ES2_BIT
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 1,
		EGL_ALPHA_SIZE, 8,
		EGL_STENCIL_SIZE , 1,
		EGL_SAMPLES, 4,
		EGL_SAMPLE_BUFFERS, 1,
		EGL_NONE
	};
	const EGLint ctx_attribs[] = {
		EGL_NONE
	};
	
	EGLConfig config;
	EGLint numConfigs=0;
	eglChooseConfig (dpy, config_attribs, &config, 1, &numConfigs);
	if (numConfigs == 0)
	{
		cd_warning ("couldn't find an appropriate visual, trying to get one without Stencil buffer\n(it may cause some little deterioration in the rendering) ...");
		config_attribs[14] = EGL_NONE;
		bStencilBufferAvailable = FALSE;
		eglChooseConfig (dpy, config_attribs, &config, 1, &numConfigs);
		
		if (numConfigs == 0 && bForceOpenGL)
		{
			// if still no luck, and user insisted on using OpenGL, give up on transparency.
			if (bForceOpenGL)
			{
				cd_warning ("we could not get an ARGB-visual, trying to get an RGB one (fake transparency will be used in return) ...");
				config_attribs[12] = None;
				bAlphaAvailable = FALSE;
				eglChooseConfig (dpy, config_attribs, &config, 1, &numConfigs);
			}
		}
		
		if (numConfigs == 0)
		{
			cd_warning ("No EGL config matching an RGBA buffer, OpenGL will not be available");
			return FALSE;
		}
	}
	g_openglConfig.bStencilBufferAvailable = bStencilBufferAvailable;
	g_openglConfig.bAlphaAvailable = bAlphaAvailable;
	s_eglConfig = config;
	
	// create a rendering context (All other context will share ressources with it, and it will be the default context in case no other context exist)
	eglBindAPI (EGL_OPENGL_API);  // specify the type of client API context before we create one.
	
	s_eglContext = eglCreateContext (dpy, config, EGL_NO_CONTEXT, ctx_attribs);
	if (s_eglContext == EGL_NO_CONTEXT)
	{
		cd_warning ("Couldn't create an EGL context, OpenGL will not be available");
		return FALSE;
	}
	
	// check some texture abilities
	g_openglConfig.bTextureFromPixmapAvailable = _check_client_egl_extension ("EGL_EXT_texture_from_pixmap");
	cd_debug ("bTextureFromPixmapAvailable: %d", g_openglConfig.bTextureFromPixmapAvailable);
	if (g_openglConfig.bTextureFromPixmapAvailable)
	{
		g_openglConfig.bindTexImage = (gpointer)eglGetProcAddress ("eglBindTexImageEXT");
		g_openglConfig.releaseTexImage = (gpointer)eglGetProcAddress ("eglReleaseTexImageEXT");
		g_openglConfig.bTextureFromPixmapAvailable = (g_openglConfig.bindTexImage && g_openglConfig.releaseTexImage);
	}
	
	return TRUE;
}

static void _stop (void)
{
	if (s_eglContext != 0)
	{
		EGLDisplay *dpy = s_eglDisplay;
		eglDestroyContext (dpy, s_eglContext);
		s_eglContext = 0;
	}
}

static gboolean _container_make_current (GldiContainer *pContainer)
{
	EGLSurface surface = pContainer->eglSurface;
	EGLDisplay *dpy = s_eglDisplay;
	return eglMakeCurrent (dpy, surface, surface, pContainer->glContext);
}

static void _container_end_draw (GldiContainer *pContainer)
{
	EGLSurface surface = pContainer->eglSurface;
	EGLDisplay *dpy = s_eglDisplay;
	eglSwapBuffers (dpy, surface);
}

static void _init_surface (G_GNUC_UNUSED GtkWidget *pWidget, GldiContainer *pContainer)
{
	// create an EGL surface for this window
	EGLDisplay *dpy = s_eglDisplay;
	EGLNativeWindowType native_window=0; // Window, wl_egl_window*, etc
	#ifdef HAVE_X11
	native_window = _gldi_container_get_Xid (pContainer);
	#endif
	pContainer->eglSurface = eglCreateWindowSurface (dpy, s_eglConfig, native_window, NULL);
	
}
static void _container_init (GldiContainer *pContainer)
{
	cairo_dock_set_default_rgba_visual (pContainer->pWidget);
	
	// create a GL context for this container (this way, we can set the viewport once and for all).
	EGLDisplay *dpy = s_eglDisplay;
	EGLContext context = s_eglContext;
	pContainer->glContext = eglCreateContext (dpy, s_eglConfig, context, NULL);
	
	// handle the double buffer manually.
	gtk_widget_set_double_buffered (pContainer->pWidget, FALSE);
	
	g_signal_connect (G_OBJECT (pContainer->pWidget),
		"realize",
		G_CALLBACK (_init_surface),
		pContainer);
}

static void _container_finish (GldiContainer *pContainer)
{
	EGLDisplay *dpy = s_eglDisplay;
	if (pContainer->glContext != 0)
	{
		if (eglGetCurrentContext() == pContainer->glContext)
		{
			if (g_pPrimaryContainer != NULL && pContainer != g_pPrimaryContainer)
				_container_make_current (g_pPrimaryContainer);
			else
				eglMakeCurrent (dpy, 0, 0, s_eglContext);
		}
		
		eglDestroyContext (dpy, pContainer->glContext);
		pContainer->glContext = 0;
	}
	if (pContainer->eglSurface != 0)
	{
		eglDestroySurface (dpy, pContainer->eglSurface);
		pContainer->eglSurface = 0;
	}
}

void gldi_register_egl_backend (void)
{
	GldiGLManagerBackend gmb;
	memset (&gmb, 0, sizeof (GldiGLManagerBackend));
	gmb.init = _initialize_opengl_backend;
	gmb.stop = _stop;
	gmb.container_make_current = _container_make_current;
	gmb.container_end_draw = _container_end_draw;
	gmb.container_init = _container_init;
	gmb.container_finish = _container_finish;
	gldi_gl_manager_register_backend (&gmb);
}

#else
#include "cairo-dock-log.h"
void gldi_register_egl_backend (void)
{
	cd_warning ("Cairo-Dock was not built with EGL support");
}
#endif

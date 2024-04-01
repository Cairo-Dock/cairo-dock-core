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
#include <EGL/eglext.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef HAVE_X11
#include <gdk/gdkx.h>  // GDK_WINDOW_XID
#endif
#ifdef HAVE_WAYLAND
#include <gdk/gdkwayland.h>
#include <wayland-egl.h>
#include <wayland-egl-backend.h>
#endif

#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_string_contains
#include "cairo-dock-opengl.h"
#include "cairo-dock-egl.h"

// dependencies
extern CairoDockGLConfig g_openglConfig;
extern GldiContainer *g_pPrimaryContainer;

// private
static GdkDisplay *s_gdkDisplay = NULL;
static EGLDisplay *s_eglDisplay = NULL;
static EGLContext s_eglContext = 0;
static EGLConfig s_eglConfig = 0;
static gboolean s_eglX11 = FALSE;
static gboolean s_eglWayland = FALSE;

// platform functions -- use these if supported
// note: eglCreatePlatformWindowSurface and eglCreatePlatformWindowSurfaceEXT differ
// in the last argument, but we only pass NULL, so we can use the same prototype
#ifdef EGL_VERSION_1_5
static PFNEGLGETPLATFORMDISPLAYPROC s_eglGetPlatformDisplay = NULL;
static PFNEGLCREATEPLATFORMWINDOWSURFACEPROC s_eglCreatePlatformWindowSurface = NULL;
#else
#ifdef EGL_EXT_platform_base
static PFNEGLGETPLATFORMDISPLAYEXTPROC s_eglGetPlatformDisplay = NULL;
static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC s_eglCreatePlatformWindowSurface = NULL;
#else
typedef EGLDisplay (EGLAPIENTRYP CD_PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform, void *native_display, const EGLint *attrib_list);
typedef EGLSurface (EGLAPIENTRYP CD_PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC) (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLint *attrib_list);
static CD_PFNEGLGETPLATFORMDISPLAYEXTPROC s_eglGetPlatformDisplay = NULL;
static CD_PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC s_eglCreatePlatformWindowSurface = NULL;
#endif
#endif


static gboolean _check_client_egl_extension (const char *extName)
{
	EGLDisplay *display = s_eglDisplay;
	const char *eglExtensions = eglQueryString (display, EGL_EXTENSIONS);
	return cairo_dock_string_contains (eglExtensions, extName, " ");
}

static void _check_backend ()
{
	s_gdkDisplay = gdk_display_get_default ();  // let's GDK do the guess
#ifdef HAVE_WAYLAND
#ifdef GDK_WINDOWING_WAYLAND
	if (GDK_IS_WAYLAND_DISPLAY(s_gdkDisplay))
		s_eglWayland = TRUE;
#endif
#endif
#ifdef HAVE_X11
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY(s_gdkDisplay))
		s_eglX11 = TRUE;
#endif
#endif
}

static gboolean _initialize_opengl_backend (gboolean bForceOpenGL)
{
	if (s_eglDisplay) return TRUE; // already initialized
	
	if (! s_gdkDisplay)
	{
		cd_warning ("Can't initialise EGL display, OpenGL will not be available");
		return FALSE;
	}
	
	// check for platform-specific support and find related EGL functions used later
	// note: this may fail on EGL < 1.5, but most systems should have a supported version
	const char *eglExtensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	if (eglExtensions == NULL) {
		cd_warning ("Cannot query EGL extensions, OpenGL will not be available");
		return FALSE;
	}
	
	if (cairo_dock_string_contains (eglExtensions, "EGL_EXT_platform_base", " "))
	{
		s_eglGetPlatformDisplay = (gpointer)eglGetProcAddress ("eglGetPlatformDisplayEXT");
		s_eglCreatePlatformWindowSurface = (gpointer)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
	}
	else
	{
		s_eglGetPlatformDisplay = (gpointer)eglGetProcAddress ("eglGetPlatformDisplay");
		s_eglCreatePlatformWindowSurface = (gpointer)eglGetProcAddress("eglCreatePlatformWindowSurface");
	}
	if (! (s_eglGetPlatformDisplay && s_eglCreatePlatformWindowSurface))
	{
		cd_warning("Cannot find EGL platform functions, OpenGL will not be available");
		return FALSE;
	}
	
	// open a connection (= Display) to the graphic server
	EGLDisplay *dpy = NULL;
	
	if (s_eglWayland)
	{
		if ( ! (cairo_dock_string_contains (eglExtensions, "EGL_EXT_platform_wayland", " ") ||
			cairo_dock_string_contains (eglExtensions, "EGL_KHR_platform_wayland", " ")))
		{
			cd_warning("Cannot find EGL platform functions, OpenGL will not be available");
			return FALSE;
		}
		struct wl_display* wdpy = NULL;
#ifdef GDK_WINDOWING_WAYLAND
		wdpy = gdk_wayland_display_get_wl_display (s_gdkDisplay);
#endif
		// EGL_PLATFORM_WAYLAND_EXT == EGL_PLATFORM_WAYLAND_KHR == 0x31D8
		if (wdpy) dpy = s_eglDisplay = s_eglGetPlatformDisplay (0x31D8, wdpy, NULL);
	}
	if (s_eglX11)
	{
		if ( ! (cairo_dock_string_contains (eglExtensions, "EGL_EXT_platform_x11", " ") ||
			cairo_dock_string_contains (eglExtensions, "EGL_KHR_platform_x11", " ")))
		{
			cd_warning("Cannot find EGL platform functions, OpenGL will not be available");
			return FALSE;
		}
		Display* xdpy = NULL;
#ifdef GDK_WINDOWING_X11
		xdpy = gdk_x11_display_get_xdisplay (s_gdkDisplay);
#endif
		// EGL_PLATFORM_X11_EXT == EGL_PLATFORM_X11_KHR == 0x31D5
		if (xdpy) dpy = s_eglDisplay = s_eglGetPlatformDisplay (0x31D5, xdpy, NULL);
	}
	
	int major, minor;
	if (! (dpy && eglInitialize (dpy, &major, &minor)))
	{
		cd_warning ("Can't initialise EGL display, OpenGL will not be available");
		s_eglDisplay = NULL;
		return FALSE;
	}
	g_print ("EGL version: %d;%d\n", major, minor);
	
	if (! _check_client_egl_extension ("EGL_KHR_surfaceless_context"))
	{
		cd_warning ("EGL does not support surfaceless contexts, OpenGL will not be available!");
		eglTerminate (dpy);
		s_eglDisplay = NULL;
		return FALSE;
	}
	
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
	
	gboolean bStencilBufferAvailable = TRUE, bAlphaAvailable = TRUE;
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
			eglTerminate (dpy);
			s_eglDisplay = NULL;
			return FALSE;
		}
	}
	g_openglConfig.bStencilBufferAvailable = bStencilBufferAvailable;
	g_openglConfig.bAlphaAvailable = bAlphaAvailable;
	s_eglConfig = config;
	
	// create a rendering context (All other context will share ressources with it, and it will be the default context in case no other context exist)
	if (!eglBindAPI (EGL_OPENGL_API))  // specify the type of client API context before we create one.
	{
		cd_warning ("Could not bind an EGL API, OpenGL will not be available");
		eglTerminate (dpy);
		s_eglDisplay = NULL;
		return FALSE;
	}
	
	s_eglContext = eglCreateContext (dpy, config, EGL_NO_CONTEXT, ctx_attribs);
	if (s_eglContext == EGL_NO_CONTEXT)
	{
		cd_warning ("Couldn't create an EGL context, OpenGL will not be available");
		eglTerminate (dpy);
		s_eglDisplay = NULL;
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
		eglTerminate (dpy);
		s_eglDisplay = NULL;
	}
}

static gboolean _container_make_current (GldiContainer *pContainer)
{
	EGLSurface surface = pContainer->eglSurface;
	if (!surface) return FALSE; // should not happen
	EGLBoolean ret = eglMakeCurrent (s_eglDisplay, surface, surface, pContainer->glContext);
	if (ret) glDrawBuffer(GL_BACK); // see e.g. https://github.com/NVIDIA/egl-wayland/issues/48
	return ret;
}

static void _container_end_draw (GldiContainer *pContainer)
{
	EGLSurface surface = pContainer->eglSurface;
	if (!surface) return;
	eglSwapBuffers (s_eglDisplay, surface);
}

static void _init_surface (GtkWidget *pWidget, GldiContainer *pContainer)
{
	cd_debug ("pWidget: %p, pContainer: %p (%dx%d)", pWidget, pContainer, pContainer->iWidth, pContainer->iHeight);
	EGLDisplay *dpy = s_eglDisplay;
	if (!dpy) return; // should not happen
	// create an EGL surface for this window
#ifdef HAVE_X11
	if (s_eglX11) 
	{
		Window win = GDK_WINDOW_XID (gldi_container_get_gdk_window (pContainer));
		pContainer->eglSurface = s_eglCreatePlatformWindowSurface (dpy, s_eglConfig, &win, NULL);
	}
#endif
#ifdef HAVE_WAYLAND
	if (s_eglWayland)
	{
		GdkWindow* gdkwindow = gldi_container_get_gdk_window (pContainer);
		gint scale = gdk_window_get_scale_factor (gdkwindow);
		gint w, h;
		if (pContainer->bIsHorizontal) { w = pContainer->iWidth; h = pContainer->iHeight; }
		else { h = pContainer->iWidth; w = pContainer->iHeight; }
		struct wl_surface* wls = gdk_wayland_window_get_wl_surface (gdkwindow);
		struct wl_egl_window* wlw = wl_egl_window_create (wls, w * scale, h * scale);
		pContainer->eglwindow = wlw;
		pContainer->eglSurface = s_eglCreatePlatformWindowSurface (dpy, s_eglConfig, (void*)wlw, NULL);
		
		// Note: for subdocks, GDK "forgets" to set the proper buffer scale, resulting in
		// surfaces scaled by the compositor, so we need to do this here manually. This is
		// likely related to gtk_widget_set_double_buffered() (and custom OpenGL rendering)
		// not being supported on by GDK on Wayland
		wl_surface_set_buffer_scale(wls, scale);
		
		// possible debug output
		// g_print ("surface: %p, window: %p, window->priv: %p, window->destroy_callback: %p\n", pContainer->eglSurface, wlw, wlw->driver_private, wlw->destroy_window_callback);
	}
#endif
	
	// make the container current to perform additional initialization
	if (!_container_make_current (pContainer))
	{
		cd_warning ("Cannot make container current!");
		return;
	}
	// set a zero swapinterval to avoid deadlock if the surface is closed at the wrong time (only needed on Wayland)
	if (s_eglWayland) eglSwapInterval (dpy, 0);
	gldi_gl_init_opengl_context ();
}

static void _destroy_surface (GtkWidget* pWidget, GldiContainer *pContainer) {
	EGLDisplay *dpy = s_eglDisplay;
	if (!dpy) return; // should not happen
	
	gboolean bCurrent = (eglGetCurrentContext() == pContainer->glContext);
	cd_debug ("pWidget: %p, pContainer: %p, surface: %p, is current: %s", pWidget, pContainer, pContainer->eglSurface,
		bCurrent ? "TRUE" : "FALSE");
	
	if (bCurrent) {
		if (! eglMakeCurrent (dpy, 0, 0, s_eglContext))
		{
			// note: binding a context without surfaces _should_ be supported
			EGLint err = eglGetError ();
			cd_warning ("eglMakeCurrent failed: %d!", err);
			
			gboolean res = FALSE;
			if (g_pPrimaryContainer != NULL && pContainer != g_pPrimaryContainer)
			{
				res = _container_make_current (g_pPrimaryContainer);
				if (!res)
				{
					EGLint err = eglGetError ();
					cd_warning ("eglMakeCurrent failed: %d!", err);
				}
			}
			if (!res)
			{
				// note: cairo_dock_create_texture_from_surface () always assumes that we have a
				// valid context, so it will not work if we bind EGL_NO_CONTEXT
				cd_warning ("calling eglMakeCurrent () with EGL_NO_CONTEXT, this can cause issues with rendering!\n");
				eglMakeCurrent (dpy, 0, 0, EGL_NO_CONTEXT);
			}
		}
	}
	
	if (pContainer->eglSurface != 0)
	{
		eglDestroySurface (dpy, pContainer->eglSurface);
		pContainer->eglSurface = 0;
	}
	#ifdef HAVE_WAYLAND
	if (pContainer->eglwindow)
	{
		// possible debug output
		// struct wl_egl_window *window = (struct wl_egl_window*)pContainer->eglwindow;
		// g_print ("window: %p, window->priv: %p, window->destroy_callback: %p\n", window, window->driver_private, window->destroy_window_callback);
		wl_egl_window_destroy (pContainer->eglwindow);
		pContainer->eglwindow = NULL;
	}
	#endif
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
	
	if (s_eglX11)
		g_signal_connect (G_OBJECT (pContainer->pWidget),
			"realize",
			G_CALLBACK (_init_surface),
			pContainer);
	/// Note: on Wayland, we have to re-init the EGL surface each time the container's window is mapped
	if (s_eglWayland)
	{
		g_signal_connect (G_OBJECT (pContainer->pWidget),
			"map",
			G_CALLBACK (_init_surface),
			pContainer);
		// our own signal, emitted before the associated wl_surface is destroyed
		g_signal_connect (G_OBJECT (pContainer->pWidget),
			"pending-unmap",
			G_CALLBACK (_destroy_surface),
			pContainer);
	}
}

static void _container_finish (GldiContainer *pContainer)
{
	EGLDisplay *dpy = s_eglDisplay;
	if (!dpy) return; // should not happen
	_destroy_surface (NULL, pContainer);
	if (pContainer->glContext != 0)
	{
		eglDestroyContext (dpy, pContainer->glContext);
		pContainer->glContext = 0;
	}
}

#ifdef HAVE_WAYLAND
static void _egl_window_resize_wayland (GldiContainer* pContainer, int iWidth, int iHeight)
{
	if (pContainer->eglwindow)
	{
		GdkWindow* gdkwindow = gldi_container_get_gdk_window (pContainer);
		gint scale = gdk_window_get_scale_factor (gdkwindow);
		wl_egl_window_resize (pContainer->eglwindow, iWidth * scale, iHeight * scale, 0, 0);
		struct wl_surface* wls = gdk_wayland_window_get_wl_surface (gdkwindow);
		wl_surface_set_buffer_scale(wls, scale);
	}
}
#endif

void gldi_register_egl_backend (void)
{
	// this fills out s_eglX11 and e_eglWayland values used in
	// _initialize_opengl_backend() and _container_init
	_check_backend ();
	GldiGLManagerBackend gmb;
	memset (&gmb, 0, sizeof (GldiGLManagerBackend));
	gmb.init = _initialize_opengl_backend;
	gmb.stop = _stop;
	gmb.container_make_current = _container_make_current;
	gmb.container_end_draw = _container_end_draw;
	gmb.container_init = _container_init;
	gmb.container_finish = _container_finish;
#ifdef HAVE_WAYLAND
	if (s_eglWayland) gmb.container_resized = _egl_window_resize_wayland;
#endif
	gldi_gl_manager_register_backend (&gmb);
}

#else
#include "cairo-dock-log.h"
void gldi_register_egl_backend (void)
{
	cd_warning ("Cairo-Dock was not built with EGL support");
}
#endif

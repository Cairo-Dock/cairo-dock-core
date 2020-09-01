#include "gldi-config.h"
#ifdef HAVE_EGL

#ifdef HAVE_WAYLAND
#define WL_EGL_PLATFORM 1
#include <gdk/gdkwayland.h>
#include <wayland-egl-core.h>
#endif

#include <EGL/egl.h>
#include "cairo-dock-egl.h"
#include "cairo-dock-container.h"
#include "cairo-dock-log.h"

#ifdef HAVE_WAYLAND
EGLDisplay* egl_get_display_wayland(GdkDisplay* dsp)
{
	struct wl_display* wdpy = gdk_wayland_display_get_wl_display(dsp);
	if (!wdpy) return NULL;
	return eglGetDisplay (wdpy);
}

void egl_init_surface_wayland (GldiContainer *pContainer, EGLDisplay* dpy, EGLConfig conf)
{
	// create an EGL surface for this window
	struct wl_surface* wls = gdk_wayland_window_get_wl_surface (
		gldi_container_get_gdk_window (pContainer));
	struct wl_egl_window* wlw = wl_egl_window_create (wls,
		pContainer->iWidth, pContainer->iHeight);
	pContainer->eglwindow = wlw;
	pContainer->eglSurface = eglCreateWindowSurface (dpy, conf, wlw, NULL);
}

void egl_window_resize_wayland (GldiContainer* pContainer, int iWidth, int iHeight)
{
	if (pContainer->eglwindow) wl_egl_window_resize (
		pContainer->eglwindow, iWidth, iHeight, 0, 0);
}

#else
EGLDisplay* egl_get_display_wayland(G_GNUC_UNUSED GdkDisplay* dsp) { return NULL; }

void egl_init_surface_wayland (G_GNUC_UNUSED GldiContainer *pContainer, G_GNUC_UNUSED EGLDisplay* dpy, G_GNUC_UNUSED EGLConfig conf)
{
	cd_error (_("Not using Wayland EGL backend!\n"));
}

void egl_window_resize_wayland (G_GNUC_UNUSED GldiContainer* pContainer, G_GNUC_UNUSED int iWidth, G_GNUC_UNUSED int iHeight)
{
	cd_error (_("Not using Wayland EGL backend!\n"));
}
#endif

#endif


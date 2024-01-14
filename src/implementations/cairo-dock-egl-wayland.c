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
	GdkWindow* gdkwindow = gldi_container_get_gdk_window (pContainer);
	gint scale = gdk_window_get_scale_factor (gdkwindow);
	gint w, h;
	if (pContainer->bIsHorizontal) { w = pContainer->iWidth; h = pContainer->iHeight; }
	else { h = pContainer->iWidth; w = pContainer->iHeight; }
	struct wl_surface* wls = gdk_wayland_window_get_wl_surface (gdkwindow);
	struct wl_egl_window* wlw = wl_egl_window_create (wls, w * scale, h * scale);
	pContainer->eglwindow = wlw;
	pContainer->eglSurface = eglCreateWindowSurface (dpy, conf, wlw, NULL);
	// Note: for subdocks, GDK "forgets" to set the proper buffer scale, resulting in
	// surfaces scaled by the compositor, so we need to do this here manually. This is
	// likely related to gtk_widget_set_double_buffered() (and custom OpenGL rendering)
	// not being supported on by GDK on Wayland
	wl_surface_set_buffer_scale(wls, scale);
}

void egl_window_resize_wayland (GldiContainer* pContainer, int iWidth, int iHeight)
{
	if (pContainer->eglwindow) {
		GdkWindow* gdkwindow = gldi_container_get_gdk_window (pContainer);
		gint scale = gdk_window_get_scale_factor (gdkwindow);
		wl_egl_window_resize (pContainer->eglwindow, iWidth * scale, iHeight * scale, 0, 0);
		struct wl_surface* wls = gdk_wayland_window_get_wl_surface (gdkwindow);
		wl_surface_set_buffer_scale(wls, scale);
	}
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


#include "gldi-config.h"
#ifdef HAVE_EGL

#include <EGL/egl.h>

#include "cairo-dock-egl.h"
#include "cairo-dock-container.h"
#include "cairo-dock-log.h"


#ifdef HAVE_X11
#include <gdk/gdkx.h>
EGLDisplay* egl_get_display_x11(GdkDisplay* dsp)
{
	Display* dpy = gdk_x11_display_get_xdisplay(dsp);
	if (!dpy) return NULL;
	return eglGetDisplay (dpy);
}

void egl_init_surface_X11 (GldiContainer *pContainer, EGLDisplay* dpy, EGLConfig conf)
{
	// create an EGL surface for this window
	EGLNativeWindowType native_window = 
		GDK_WINDOW_XID (gldi_container_get_gdk_window (pContainer));
	pContainer->eglSurface = eglCreateWindowSurface (dpy, conf, native_window, NULL);
}

#else
EGLDisplay* egl_get_display_x11(GdkDisplay* dsp) { return NULL; }
void egl_init_surface_X11 (G_GNUC_UNUSED GldiContainer *pContainer, G_GNUC_UNUSED EGLDisplay* dpy, G_GNUC_UNUSED EGLConfig conf)
{
	cd_error (_("Not using X11 EGL backend!\n"));
}
#endif

#endif

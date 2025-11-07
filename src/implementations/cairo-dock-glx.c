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
#ifdef HAVE_GLX

#include <GL/gl.h>
#include <GL/glx.h>
#include <string.h>  // strstr
#include <gdk/gdkx.h>  // GDK_WINDOW_XID
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>  // XRenderFindVisualFormat

#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_string_contains
#include "cairo-dock-X-utilities.h"  // cairo_dock_get_X_display
#include "cairo-dock-opengl.h"

// dependencies
extern CairoDockGLConfig g_openglConfig;
extern GldiContainer *g_pPrimaryContainer;

// private
static Display *s_XDisplay = NULL;
static GLXContext s_XContext = 0;
static XVisualInfo *s_XVisInfo = NULL;
GdkVisual *s_pGdkVisual = NULL;
#define _gldi_container_get_Xid(pContainer) GDK_WINDOW_XID (gldi_container_get_gdk_window(pContainer))

static gboolean _check_client_glx_extension (const char *extName)
{
	Display *display = s_XDisplay;
	const gchar *glxExtensions = glXGetClientString (display, GLX_EXTENSIONS);
	return cairo_dock_string_contains (glxExtensions, extName, " ");
}

static XVisualInfo *_get_visual_from_fbconfigs (GLXFBConfig *pFBConfigs, int iNumOfFBConfigs, Display *XDisplay)
{
	XRenderPictFormat *pPictFormat;
	XVisualInfo *pVisInfo = NULL;
	int i;
	for (i = 0; i < iNumOfFBConfigs; i++)
	{
		pVisInfo = glXGetVisualFromFBConfig (XDisplay, pFBConfigs[i]);
		if (!pVisInfo)
		{
			cd_warning ("this FBConfig has no visual.");
			continue;
		}
		
		pPictFormat = XRenderFindVisualFormat (XDisplay, pVisInfo->visual);
		if (!pPictFormat)
		{
			cd_warning ("this visual has an unknown format.");
			XFree (pVisInfo);
			pVisInfo = NULL;
			continue;
		}
		
		if (pPictFormat->direct.alphaMask > 0)
		{
			cd_message ("Strike, found a GLX visual with alpha-support !");
			break;
		}

		XFree (pVisInfo);
		pVisInfo = NULL;
	}
	return pVisInfo;
}

static gboolean _initialize_opengl_backend (gboolean bForceOpenGL)
{
	gboolean bStencilBufferAvailable, bAlphaAvailable;
	Display *XDisplay = s_XDisplay;
	
	//\_________________ On cherche un visual qui reponde a tous les criteres.
	GLXFBConfig *pFBConfigs;
	int doubleBufferAttributes[] = {
		GLX_DRAWABLE_TYPE, 		GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, 		GLX_RGBA_BIT,
		GLX_DOUBLEBUFFER, 		True,
		GLX_RED_SIZE, 			1,
		GLX_GREEN_SIZE, 		1,
		GLX_BLUE_SIZE, 			1,
		GLX_DEPTH_SIZE, 		1,
		GLX_ALPHA_SIZE, 		1,
		GLX_STENCIL_SIZE, 		1,
		/// a tester ...
		GLX_SAMPLE_BUFFERS_ARB, 1,
		GLX_SAMPLES_ARB, 		4,
		/*GL_MULTISAMPLEBUFFERS, 	1,
		GL_MULTISAMPLESAMPLES, 	2,*/
		None};
	
	XVisualInfo *pVisInfo = NULL;
	int iNumOfFBConfigs = 0;
	pFBConfigs = glXChooseFBConfig (XDisplay,
		DefaultScreen (XDisplay),
		doubleBufferAttributes,
		&iNumOfFBConfigs);
	cd_debug ("got %d FBConfig(s)", iNumOfFBConfigs);
	bStencilBufferAvailable = TRUE;
	bAlphaAvailable = TRUE;
	
	pVisInfo = _get_visual_from_fbconfigs (pFBConfigs, iNumOfFBConfigs, XDisplay);
	if (pFBConfigs)
		XFree (pFBConfigs);
	
	//\_________________ Si pas trouve, on en cherche un moins contraignant.
	if (pVisInfo == NULL)
	{
		cd_warning ("couldn't find an appropriate visual, trying to get one without Stencil buffer\n(it may cause some little deterioration in the rendering) ...");
		doubleBufferAttributes[16] = None;
		pFBConfigs = glXChooseFBConfig (XDisplay,
			DefaultScreen (XDisplay),
			doubleBufferAttributes,
			&iNumOfFBConfigs);
		cd_debug ("this time got %d FBConfig(s)", iNumOfFBConfigs);
		bStencilBufferAvailable = FALSE;
		bAlphaAvailable = TRUE;
		
		pVisInfo = _get_visual_from_fbconfigs (pFBConfigs, iNumOfFBConfigs, XDisplay);
		if (pFBConfigs)
			XFree (pFBConfigs);
		
		// if still no luck, and user insisted on using OpenGL, give up transparency.
		if (pVisInfo == NULL && bForceOpenGL)
		{
			cd_warning ("we could not get an ARGB-visual, trying to get an RGB one (fake transparency will be used in return) ...");
			doubleBufferAttributes[14] = None;
			int i, iNumOfFBConfigs;
			pFBConfigs = glXChooseFBConfig (XDisplay,
				DefaultScreen (XDisplay),
				doubleBufferAttributes,
				&iNumOfFBConfigs);
			bStencilBufferAvailable = FALSE;
			bAlphaAvailable = FALSE;
			cd_debug ("got %d FBConfig(s) without alpha channel", iNumOfFBConfigs);
			for (i = 0; i < iNumOfFBConfigs; i++)
			{
				pVisInfo = glXGetVisualFromFBConfig (XDisplay, pFBConfigs[i]);
				if (!pVisInfo)
				{
					cd_warning ("this FBConfig has no visual.");
					XFree (pVisInfo);
					pVisInfo = NULL;
				}
				else
					break;
			}
			if (pFBConfigs)
				XFree (pFBConfigs);
			
			if (pVisInfo == NULL)
			{
				cd_warning ("still no visual, this is the last chance");
				pVisInfo = glXChooseVisual (XDisplay,
					DefaultScreen (XDisplay),
					doubleBufferAttributes);
			}
		}
		
	}
	
	g_openglConfig.bStencilBufferAvailable = bStencilBufferAvailable;
	g_openglConfig.bAlphaAvailable = bAlphaAvailable;
	
	//\_________________ Si rien de chez rien, on quitte.
	if (pVisInfo == NULL)
	{
		cd_warning ("couldn't find a suitable GLX Visual, OpenGL can't be used.\n (sorry to say that, but your graphic card and/or its driver is crappy)");
		return FALSE;
	}
	
	//\_________________ create a context for this visual. All other context will share resources with it, and it will be the default context in case no other context exist.
	Display *dpy = s_XDisplay;
	s_XContext = glXCreateContext (dpy, pVisInfo, NULL, TRUE);
	g_return_val_if_fail (s_XContext != 0, FALSE);
	
	//\_________________ create a colormap based on this visual.
	GdkScreen *screen = gdk_screen_get_default ();
	GdkVisual *pGdkVisual = gdk_x11_screen_lookup_visual (screen, pVisInfo->visualid);
	s_pGdkVisual = pGdkVisual;
	g_return_val_if_fail (s_pGdkVisual != NULL, FALSE);
	s_XVisInfo = pVisInfo;
	/** Note: this is where we can't use gtkglext: gdk_x11_gl_config_new_from_visualid() sets a new colormap on the root window, and Qt doesn't like that
	cf https://bugzilla.redhat.com/show_bug.cgi?id=440340
	which would force us to do a XDeleteProperty ( dpy, DefaultRootWindow (dpy), XInternAtom (dpy, "RGB_DEFAULT_MAP", 0) ) */
	
	//\_________________ on verifier les capacites des textures.
	g_openglConfig.bTextureFromPixmapAvailable = _check_client_glx_extension ("GLX_EXT_texture_from_pixmap");
	if (g_openglConfig.bTextureFromPixmapAvailable)
	{
		g_openglConfig.bindTexImage = (gpointer)glXGetProcAddress ((GLubyte *) "glXBindTexImageEXT");
		g_openglConfig.releaseTexImage = (gpointer)glXGetProcAddress ((GLubyte *) "glXReleaseTexImageEXT");
		g_openglConfig.bTextureFromPixmapAvailable = (g_openglConfig.bindTexImage && g_openglConfig.releaseTexImage);
	}
	
	return TRUE;
}

static void _stop (void)
{
	if (s_XContext != 0)
	{
		Display *dpy = s_XDisplay;
		glXMakeCurrent (dpy, 0, NULL); // s_XContext might still be current
		glXDestroyContext (dpy, s_XContext);
		s_XContext = 0;
	}
}

static gboolean _container_make_current (GldiContainer *pContainer)
{
	Window Xid = _gldi_container_get_Xid (pContainer);
	Display *dpy = s_XDisplay;
	return glXMakeCurrent (dpy, Xid, pContainer->glContext);
}

static gboolean _offscreen_make_current (void)
{
	if (! (s_XContext && g_pPrimaryContainer)) return FALSE;
	Window Xid = _gldi_container_get_Xid (g_pPrimaryContainer);
	Display *dpy = s_XDisplay;
	return glXMakeCurrent (dpy, Xid, s_XContext);
}

static void _container_end_draw (GldiContainer *pContainer)
{
	Window Xid = _gldi_container_get_Xid (pContainer);
	Display *dpy = s_XDisplay;
	glXSwapBuffers (dpy, Xid);
}

static void _init_opengl_context (G_GNUC_UNUSED GtkWidget* pWidget, GldiContainer *pContainer)
{
	if (!_container_make_current (pContainer))
	{
		cd_warning ("Cannot make container current!");
		return;
	}
	gldi_gl_init_opengl_context ();
}

static void _container_init (GldiContainer *pContainer)
{
	// Set the visual we found during the init
	gtk_widget_set_visual (pContainer->pWidget, s_pGdkVisual);
	
	// create a GL context for this container (this way, we can set the viewport once and for all).
	Display *dpy = s_XDisplay;
	GLXContext shared_context = s_XContext;
	pContainer->glContext = glXCreateContext (dpy, s_XVisInfo, shared_context, TRUE);
	
	// handle the double buffer manually.
	gtk_widget_set_double_buffered (pContainer->pWidget, FALSE);
	
	g_signal_connect (G_OBJECT (pContainer->pWidget),
		"realize",
		G_CALLBACK (_init_opengl_context),
		pContainer);
}

static void _container_finish (GldiContainer *pContainer)
{
	if (pContainer->glContext != NULL)
	{
		Display *dpy = s_XDisplay;
		
		if (glXGetCurrentContext() == pContainer->glContext)
		{
			if (g_pPrimaryContainer != NULL && pContainer != g_pPrimaryContainer)
				_container_make_current (g_pPrimaryContainer);
			else
				glXMakeCurrent (dpy, 0, s_XContext);
		}
		
		glXDestroyContext (dpy, pContainer->glContext);
	}
}

void gldi_register_glx_backend (void)
{
	GldiGLManagerBackend gmb;
	memset (&gmb, 0, sizeof (GldiGLManagerBackend));
	gmb.init = _initialize_opengl_backend;
	gmb.stop = _stop;
	gmb.container_make_current = _container_make_current;
	gmb.offscreen_make_current = _offscreen_make_current;
	gmb.container_end_draw = _container_end_draw;
	gmb.container_init = _container_init;
	gmb.container_finish = _container_finish;
	gmb.name = "GLX";
	gldi_gl_manager_register_backend (&gmb);

	s_XDisplay = cairo_dock_get_X_display ();  // initialize it once and for all at the beginning; we use this display rather than the GDK one to avoid the GDK X errors check.
}

#else
#include "cairo-dock-log.h"
void gldi_register_glx_backend (void)
{
	cd_warning ("Cairo-Dock was not built with GLX support");
}
#endif

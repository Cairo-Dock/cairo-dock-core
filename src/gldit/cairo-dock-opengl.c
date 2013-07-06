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

#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>  // gluLookAt
#include <GL/glx.h>
#include <gdk/gdkx.h>
#include <X11/extensions/Xrender.h>  // XRenderFindVisualFormat

#include "cairo-dock-log.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_get_icon_extent
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-desktop-manager.h"  // desktop dimensions

#include "cairo-dock-opengl.h"

// public (manager, config, data)
CairoDockGLConfig g_openglConfig;
gboolean g_bUseOpenGL = FALSE;

// dependancies
extern GldiContainer *g_pPrimaryContainer;
extern GldiDesktopBackground *g_pFakeTransparencyDesktopBg;
extern gboolean g_bEasterEggs;

// private
static gboolean s_bInitialized = FALSE;

static inline gboolean _check_extension (const char *extName, const gchar *cExtensions)
{
	g_return_val_if_fail (cExtensions != NULL, FALSE);
	/*
	** Search for extName in the extensions string.  Use of strstr()
	** is not sufficient because extension names can be prefixes of
	** other extension names.  Could use strtok() but the constant
	** string returned by glGetString can be in read-only memory.
	*/
	char *p = (char *) cExtensions;

	char *end;
	int extNameLen;

	extNameLen = strlen(extName);
	end = p + strlen(p);

	while (p < end)
	{
		int n = strcspn(p, " ");
		if ((extNameLen == n) && (strncmp(extName, p, n) == 0))
		{
			return TRUE;
		}
		p += (n + 1);
	}
	return FALSE;
}
static gboolean _check_gl_extension (const char *extName)
{
	const gchar *glExtensions = (gchar *) glGetString (GL_EXTENSIONS);
	return _check_extension (extName, glExtensions);
}
static gboolean _check_client_glx_extension (const char *extName)
{
	Display *display = gdk_x11_get_default_xdisplay ();
	//int screen = 0;
	//const gchar *glxExtensions = glXQueryExtensionsString (display, screen);
	const gchar *glxExtensions = glXGetClientString (display, GLX_EXTENSIONS);
	return _check_extension (extName, glxExtensions);
}


static void _post_initialize_opengl_backend (G_GNUC_UNUSED GtkWidget *pWidget, GldiContainer *pContainer)  // initialisation necessitant un contexte opengl.
{
	g_return_if_fail (!s_bInitialized);
	
	if (! gldi_glx_make_current (pContainer))
		return ;
	
	s_bInitialized = TRUE;
	g_openglConfig.bNonPowerOfTwoAvailable = _check_gl_extension ("GL_ARB_texture_non_power_of_two");
	g_openglConfig.bFboAvailable = _check_gl_extension ("GL_EXT_framebuffer_object");
	if (!g_openglConfig.bFboAvailable)
		cd_warning ("No FBO support, some applets will be invisible if placed inside the dock.");
	
	g_openglConfig.bNonPowerOfTwoAvailable = _check_gl_extension ("GL_ARB_texture_non_power_of_two");
	g_openglConfig.bAccumBufferAvailable = _check_gl_extension ("GL_SUN_slice_accum");
	
	GLfloat fMaximumAnistropy = 0.;
	if (_check_gl_extension ("GL_EXT_texture_filter_anisotropic"))
	{
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fMaximumAnistropy);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fMaximumAnistropy);
	}
	
	cd_message ("OpenGL config summary :\n - bNonPowerOfTwoAvailable : %d\n - bFboAvailable : %d\n - direct rendering : %d\n - bTextureFromPixmapAvailable : %d\n - bAccumBufferAvailable : %d\n - Anisotroy filtering level max : %.1f\n - OpenGL version: %s\n - OpenGL vendor: %s\n - OpenGL renderer: %s\n\n",
		g_openglConfig.bNonPowerOfTwoAvailable,
		g_openglConfig.bFboAvailable,
		!g_openglConfig.bIndirectRendering,
		g_openglConfig.bTextureFromPixmapAvailable,
		g_openglConfig.bAccumBufferAvailable,
		fMaximumAnistropy,
		glGetString (GL_VERSION),
		glGetString (GL_VENDOR),
		glGetString (GL_RENDERER));
}

static inline XVisualInfo *_get_visual_from_fbconfigs (GLXFBConfig *pFBConfigs, int iNumOfFBConfigs, Display *XDisplay)
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

gboolean cairo_dock_initialize_opengl_backend (gboolean bForceOpenGL)  // taken from a MacSlow's exemple.
{
	memset (&g_openglConfig, 0, sizeof (CairoDockGLConfig));
	g_bUseOpenGL = FALSE;
	gboolean bStencilBufferAvailable, bAlphaAvailable;
	Display *XDisplay = gdk_x11_get_default_xdisplay ();
	
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
			g_openglConfig.bAlphaAvailable = FALSE;
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
	
	//\_________________ create a context for this visual. All other context will share ressources with it, and it will be the default context in case no other context exist.
	Display *dpy = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
	g_openglConfig.context = glXCreateContext (dpy, pVisInfo, NULL, TRUE);
	g_return_val_if_fail (g_openglConfig.context != 0, FALSE);
	
	//\_________________ create a colormap based on this visual.
	GdkScreen *screen = gdk_screen_get_default ();
	GdkVisual *pGdkVisual = gdk_x11_screen_lookup_visual (screen, pVisInfo->visualid);
	#if (GTK_MAJOR_VERSION < 3)
	g_openglConfig.xcolormap = XCreateColormap (dpy, DefaultRootWindow (dpy), pVisInfo->visual, AllocNone);
	g_openglConfig.pColormap = gdk_x11_colormap_foreign_new (pGdkVisual, g_openglConfig.xcolormap);
	#else
	g_openglConfig.pGdkVisual = pGdkVisual;
	g_return_val_if_fail (g_openglConfig.pGdkVisual != NULL, FALSE);
	#endif
	g_openglConfig.pVisInfo = pVisInfo;
	/** Note: this is where we can't use gtkglext: gdk_x11_gl_config_new_from_visualid() sets a new colormap on the root window, and Qt doesn't like that
	cf https://bugzilla.redhat.com/show_bug.cgi?id=440340
	which would force us to do a XDeleteProperty ( dpy, DefaultRootWindow (dpy), XInternAtom (dpy, "RGB_DEFAULT_MAP", 0) ) */
	
	g_bUseOpenGL = TRUE;
	
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


void cairo_dock_force_indirect_rendering (void)
{
	if (g_bUseOpenGL)
		g_openglConfig.bIndirectRendering = TRUE;
}



static inline void _cairo_dock_set_perspective_view (int iWidth, int iHeight)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, 1.0*(GLfloat)iWidth/(GLfloat)iHeight, 1., 4*iHeight);
	glViewport(0, 0, iWidth, iHeight);
	glMatrixMode (GL_MODELVIEW);
	
	glLoadIdentity ();
	gluLookAt (0, 0, 3., 0, 0, 0., 0.0f, 1.0f, 0.0f);
	//gluLookAt (0/2, 0/2, 3., 0/2, 0/2, 0., 0.0f, 1.0f, 0.0f);
	glTranslatef (0., 0., -iHeight*(sqrt(3)/2) - 1);
	//glTranslatef (iWidth/2, iHeight/2, -iHeight*(sqrt(3)/2) - 1);
}
void cairo_dock_set_perspective_view (GldiContainer *pContainer)
{
	int w, h;
	if (pContainer->bIsHorizontal)
	{
		w = pContainer->iWidth;
		h = pContainer->iHeight;
	}
	else
	{
		w = pContainer->iHeight;
		h = pContainer->iWidth;
	}
	_cairo_dock_set_perspective_view (w, h);
	pContainer->bPerspectiveView = TRUE;
}

void cairo_dock_set_perspective_view_for_icon (Icon *pIcon, G_GNUC_UNUSED GldiContainer *pContainer)
{
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	_cairo_dock_set_perspective_view (w, h);
}

static inline void _cairo_dock_set_ortho_view (int iWidth, int iHeight)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, iWidth, 0, iHeight, 0.0, 500.0);
	glViewport(0, 0, iWidth, iHeight);
	glMatrixMode (GL_MODELVIEW);
	
	glLoadIdentity ();
	gluLookAt (0/2, 0/2, 3.,
		0/2, 0/2, 0.,
		0.0f, 1.0f, 0.0f);
	glTranslatef (iWidth/2, iHeight/2, - iHeight/2);
}
void cairo_dock_set_ortho_view (GldiContainer *pContainer)
{
	int w, h;
	if (pContainer->bIsHorizontal)
	{
		w = pContainer->iWidth;
		h = pContainer->iHeight;
	}
	else
	{
		w = pContainer->iHeight;
		h = pContainer->iWidth;
	}
	_cairo_dock_set_ortho_view (w, h);
	pContainer->bPerspectiveView = FALSE;
}

void cairo_dock_set_ortho_view_for_icon (Icon *pIcon, G_GNUC_UNUSED GldiContainer *pContainer)
{
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	_cairo_dock_set_ortho_view (w, h);
}


void gldi_glx_apply_desktop_background (GldiContainer *pContainer)  /// TODO: make it static...
{
	if (/**! myContainersParam.bUseFakeTransparency || */! g_pFakeTransparencyDesktopBg || g_pFakeTransparencyDesktopBg->iTexture == 0)
		return ;
	
	glPushMatrix ();
	gboolean bSetPerspective = pContainer->bPerspectiveView;
	if (bSetPerspective)
		cairo_dock_set_ortho_view (pContainer);
	glLoadIdentity ();
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_source ();
	_cairo_dock_set_alpha (1.);
	glBindTexture (GL_TEXTURE_2D, g_pFakeTransparencyDesktopBg->iTexture);
	
	double x, y, w, h, W, H;
	W = gldi_desktop_get_width();
	H = gldi_desktop_get_height();
	if (pContainer->bIsHorizontal)
	{
		w = pContainer->iWidth;
		h = pContainer->iHeight;
		x = pContainer->iWindowPositionX;
		y = pContainer->iWindowPositionY;
	}
	else
	{
		h = pContainer->iWidth;
		w = pContainer->iHeight;
		y = pContainer->iWindowPositionX;
		x = pContainer->iWindowPositionY;
	}
	
	glBegin(GL_QUADS);
	glTexCoord2f ((x + 0) / W, (y + 0) / H);
	glVertex3f (0., h, 0.);  // Top Left.
	
	glTexCoord2f ((x + w) / W, (y + 0) / H);
	glVertex3f (w, h, 0.);  // Top Right
	
	glTexCoord2f ((x + w) / W, (y + h) / H);
	glVertex3f (w, 0., 0.);  // Bottom Right
	
	glTexCoord2f ((x + 0.) / W, (y + h) / H);
	glVertex3f (0., 0., 0.);  // Bottom Left
	glEnd();
	
	_cairo_dock_disable_texture ();
	if (bSetPerspective)
		cairo_dock_set_perspective_view (pContainer);
	glPopMatrix ();
}


gboolean gldi_glx_make_current (GldiContainer *pContainer)
{
	Display *dpy = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
	Window Xid = gldi_container_get_Xid (pContainer);

	return glXMakeCurrent (dpy, Xid, pContainer->glContext);
}

gboolean gldi_glx_begin_draw_container_full (GldiContainer *pContainer, gboolean bClear)
{
	if (! gldi_glx_make_current (pContainer))
		return FALSE;
	
	if (bClear)
	{
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gldi_glx_apply_desktop_background (pContainer);
	}
	glLoadIdentity ();
	
	return TRUE;
}

void gldi_glx_end_draw_container (GldiContainer *pContainer)
{
	Display *dpy = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
	Window Xid = gldi_container_get_Xid (pContainer);

	glXSwapBuffers (dpy, Xid);
}

static void _init_opengl_context (G_GNUC_UNUSED GtkWidget* pWidget, GldiContainer *pContainer)
{
	if (! g_bUseOpenGL)
		return ;
	
	if (! gldi_glx_make_current (pContainer))
		return;
	
	//g_print ("INIT OPENGL ctx\n");
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth (1.0f);
	glClearStencil (0);
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
	
	/// a tester ...
	///if (g_bEasterEggs)
	///	glEnable (GL_MULTISAMPLE_ARB);
	
	// set once and for all
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);  // GL_MODULATE / GL_DECAL /  GL_BLEND
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MIN_FILTER,
		g_bEasterEggs ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	if (g_bEasterEggs)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MAG_FILTER,
		GL_LINEAR);
}
void gldi_glx_init_container (GldiContainer *pContainer)
{
	// Set the visual we found during the init
	#if (GTK_MAJOR_VERSION < 3)
	gtk_widget_set_colormap (pContainer->pWidget, g_openglConfig.pColormap);
	#else
	gtk_widget_set_visual (pContainer->pWidget, g_openglConfig.pGdkVisual);
	#endif
	
	// create a GL context for this container (this way, we can set the viewport once and for all).
	Display *dpy = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
	GLXContext context = g_openglConfig.context;
	pContainer->glContext = glXCreateContext (dpy, g_openglConfig.pVisInfo, context, TRUE);
	
	// handle the double buffer manually.
	gtk_widget_set_double_buffered (pContainer->pWidget, FALSE);
	
	// finish the initialisation of the opengl backend, now that we have a context (and a window to bind it to)
	if (! s_bInitialized)
		g_signal_connect (G_OBJECT (pContainer->pWidget),
			"realize",
			G_CALLBACK (_post_initialize_opengl_backend),
			pContainer);
	
	// when the window will be realised, initialise its GL context.
	g_signal_connect_after (G_OBJECT (pContainer->pWidget),
		"realize",
		G_CALLBACK (_init_opengl_context),
		pContainer);
}

void gldi_glx_finish_container (GldiContainer *pContainer)
{
	if (pContainer->glContext != 0)
	{
		Display *dpy = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
		
		if (glXGetCurrentContext() == pContainer->glContext)
		{
			if (g_pPrimaryContainer != NULL && pContainer != g_pPrimaryContainer)
				gldi_glx_make_current (g_pPrimaryContainer);
			else
				glXMakeCurrent (dpy, 0, g_openglConfig.context);
		}
		
		glXDestroyContext (dpy, pContainer->glContext);
	}
}

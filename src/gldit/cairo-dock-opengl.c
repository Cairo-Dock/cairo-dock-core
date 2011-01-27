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

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtkgl.h>

#include <X11/extensions/Xrender.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <GL/glu.h>

#include "cairo-dock-log.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-desklet-factory.h"  // cairo_dock_begin_draw_icon
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-opengl.h"

CairoDockGLConfig g_openglConfig;
gboolean g_bUseOpenGL = FALSE;

extern CairoContainer *g_pPrimaryContainer;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern CairoDockDesktopBackground *g_pFakeTransparencyDesktopBg;
extern gboolean g_bEasterEggs;

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
	const gchar *glExtensions = glGetString (GL_EXTENSIONS);
	return _check_extension (extName, glExtensions);
}
static gboolean _check_client_glx_extension (const char *extName)
{
	Display *display = gdk_x11_get_default_xdisplay ();
	int screen = 0;
	//const gchar *glxExtensions = glXQueryExtensionsString (display, screen);
	const gchar *glxExtensions = glXGetClientString (display,GLX_EXTENSIONS);
	return _check_extension (extName, glxExtensions);
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
	XRenderPictFormat *pPictFormat = NULL;
	int doubleBufferAttributes[] = {
		GLX_DRAWABLE_TYPE, 	GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, 		GLX_RGBA_BIT,
		GLX_DOUBLEBUFFER, 	True,
		GLX_RED_SIZE, 		1,
		GLX_GREEN_SIZE, 		1,
		GLX_BLUE_SIZE, 		1,
		GLX_DEPTH_SIZE, 		1,
		GLX_ALPHA_SIZE, 		1,
		GLX_STENCIL_SIZE, 	1,
		/// a tester ...
		GLX_SAMPLE_BUFFERS_ARB, 1,
		GLX_SAMPLES_ARB, 4,
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
	}
	
	//\_________________ Si toujours rien, on essaye avec la methode de gdkgl.
	if (pVisInfo == NULL)
	{
		cd_warning ("still couldn't find an appropriate visual ourself, trying something else, this may not work with some drivers ...");
		g_openglConfig.pGlConfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGB |
			GDK_GL_MODE_ALPHA |
			GDK_GL_MODE_DEPTH |
			GDK_GL_MODE_DOUBLE |
			GDK_GL_MODE_STENCIL);
		bStencilBufferAvailable = TRUE;
		bAlphaAvailable = TRUE;
		
		if (g_openglConfig.pGlConfig == NULL)
		{
			cd_warning ("no luck, trying without double-buffer and stencil ...");
			g_openglConfig.pGlConfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGB |
				GDK_GL_MODE_ALPHA |
				GDK_GL_MODE_DEPTH);
			bStencilBufferAvailable = FALSE;
			bAlphaAvailable = TRUE;
		}
	}
	
	//\_________________ Si toujours rien et si l'utilisateur n'en demord pas, on en cherche un sans canal alpha.
	if (pVisInfo == NULL && g_openglConfig.pGlConfig == NULL && bForceOpenGL)
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
	g_openglConfig.bStencilBufferAvailable = bStencilBufferAvailable;
	g_openglConfig.bAlphaAvailable = bAlphaAvailable;
	
	//\_________________ Si rien de chez rien, on quitte.
	if (pVisInfo == NULL && g_openglConfig.pGlConfig == NULL)
	{
		cd_warning ("couldn't find a suitable GLX Visual, OpenGL can't be used.\n (sorry to say that, but your graphic card and/or its driver is crappy)");
		return FALSE;
	}
	
	//\_________________ On en deduit une config opengl valide pour les widgets gtk.
	if (pVisInfo != NULL)
	{
		cd_message ("ok, got a visual");
		/// The function below sets a new colormap on the root window, and Qt doesn't like that !...
		/// link with https://bugzilla.redhat.com/show_bug.cgi?id=440340 ?
		g_openglConfig.pGlConfig = gdk_x11_gl_config_new_from_visualid (pVisInfo->visualid);
		/// as a workaround, we remove this colormap.
		Display *dpy = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
		XDeleteProperty ( dpy, DefaultRootWindow (dpy), XInternAtom (dpy, "RGB_DEFAULT_MAP", 0) ); // (XA_RGB_DEFAULT_MAP)
		XFree (pVisInfo);
	}
	g_return_val_if_fail (g_openglConfig.pGlConfig != NULL, FALSE);
	g_bUseOpenGL = TRUE;
	
	//\_________________ On regarde si le rendu doit etre indirect ou pas.
	/**if (glXQueryVersion(XDisplay, &g_openglConfig.iGlxMajor, &g_openglConfig.iGlxMinor) == False)
	{
		cd_warning ("GLX not available !\nFear the worst !");
	}
	else
	{
		if (g_openglConfig.iGlxMajor <= 1 && g_openglConfig.iGlxMinor < 3)  // on a besoin de GLX >= 1.3 pour les pbuffers; mais si on a l'extension GLX_SGIX_pbuffer et un version inferieure, ca marche quand meme.
		{
			if (! _check_client_glx_extension ("GLX_SGIX_pbuffer"))
			{
				cd_warning ("No pbuffer extension in GLX.\n this might affect the drawing of some applets which are inside a dock");
			}
			else
			{
				cd_warning ("GLX version too old (%d.%d).\nCairo-Dock needs at least GLX 1.3. Indirect rendering will be toggled on/off as a workaround.", g_openglConfig.iGlxMajor, g_openglConfig.iGlxMinor);
				g_openglConfig.bPBufferAvailable = TRUE;
				g_openglConfig.bIndirectRendering = ! bToggleIndirectRendering;
			}
		}
		else
		{
			g_openglConfig.bPBufferAvailable = TRUE;
		}
	}*/
	
	//\_________________ on verifier les capacites des textures.
	g_openglConfig.bTextureFromPixmapAvailable = _check_client_glx_extension ("GLX_EXT_texture_from_pixmap");
	if (g_openglConfig.bTextureFromPixmapAvailable)
	{
		g_openglConfig.bindTexImage = (gpointer)glXGetProcAddress ("glXBindTexImageEXT");
		g_openglConfig.releaseTexImage = (gpointer)glXGetProcAddress ("glXReleaseTexImageEXT");
		g_openglConfig.bTextureFromPixmapAvailable = (g_openglConfig.bindTexImage && g_openglConfig.releaseTexImage);
	}
	
	return TRUE;
}


void cairo_dock_force_indirect_rendering (void)
{
	if (g_bUseOpenGL)
		g_openglConfig.bIndirectRendering = TRUE;
}


void cairo_dock_create_icon_fbo (void)  // it has been found that you get a speed boost if your textures is the same size and you use 1 FBO for them. => c'est le cas general dans le dock. Du coup on est gagnant a ne faire qu'un seul FBO pour toutes les icones.
{
	if (! g_openglConfig.bFboAvailable)
		return ;
	g_return_if_fail (g_openglConfig.iFboId == 0);
	
	glGenFramebuffersEXT(1, &g_openglConfig.iFboId);
	
	int iWidth = 0, iHeight = 0;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_GROUPS; i += 2)
	{
		iWidth = MAX (iWidth, myIconsParam.tIconAuthorizedWidth[i]);
		iHeight = MAX (iHeight, myIconsParam.tIconAuthorizedHeight[i]);
	}
	if (iWidth == 0)
		iWidth = 48;
	if (iHeight == 0)
		iHeight = 48;
	iWidth *= (1 + myIconsParam.fAmplitude);
	iHeight *= (1 + myIconsParam.fAmplitude);
	g_openglConfig.iRedirectedTexture = cairo_dock_load_texture_from_raw_data (NULL, iWidth, iHeight);
}

void cairo_dock_destroy_icon_fbo (void)
{
	if (g_openglConfig.iFboId == 0)
		return;
	glDeleteFramebuffersEXT (1, &g_openglConfig.iFboId);
	g_openglConfig.iFboId = 0;
	
	_cairo_dock_delete_texture (g_openglConfig.iRedirectedTexture);
	g_openglConfig.iRedirectedTexture = 0;
}

/**static GLXPbuffer _cairo_dock_create_pbuffer (int iWidth, int iHeight, GLXContext *pContext)
{
	Display *XDisplay = gdk_x11_get_default_xdisplay ();
	
	GLXFBConfig *pFBConfigs;
	XRenderPictFormat *pPictFormat = NULL;
	int visAttribs[] = {
		GLX_DRAWABLE_TYPE, 	GLX_PBUFFER_BIT | GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, 		GLX_RGBA_BIT,
		GLX_RED_SIZE, 		1,
		GLX_GREEN_SIZE, 		1,
		GLX_BLUE_SIZE, 		1,
		GLX_ALPHA_SIZE, 		1,
		GLX_DEPTH_SIZE, 		1,
		None};
	
	XVisualInfo *pVisInfo = NULL;
	int i, iNumOfFBConfigs = 0;
	pFBConfigs = glXChooseFBConfig (XDisplay,
		DefaultScreen (XDisplay),
		visAttribs,
		&iNumOfFBConfigs);
	if (iNumOfFBConfigs == 0)
	{
		cd_warning ("No suitable visual could be found for pbuffer\n this might affect the drawing of some applets which are inside a dock");
		*pContext = 0;
		return 0;
	}
	cd_debug (" -> %d FBConfig(s) pour le pbuffer", iNumOfFBConfigs);
	
	int pbufAttribs [] = {
		GLX_PBUFFER_WIDTH, iWidth,
		GLX_PBUFFER_HEIGHT, iHeight,
		GLX_LARGEST_PBUFFER, True,
		None};
	GLXPbuffer pbuffer = glXCreatePbuffer (XDisplay, pFBConfigs[0], pbufAttribs);
	
	pVisInfo = glXGetVisualFromFBConfig (XDisplay, pFBConfigs[0]);
	
	GdkGLContext *pGlContext = gtk_widget_get_gl_context (g_pMainDock->container.pWidget);
	GLXContext mainContext = GDK_GL_CONTEXT_GLXCONTEXT (pGlContext);
	*pContext = glXCreateContext (XDisplay, pVisInfo, mainContext, ! g_openglConfig.bIndirectRendering);
	
	XFree (pVisInfo);
	XFree (pFBConfigs);
	
	return pbuffer;
}

void cairo_dock_create_icon_pbuffer (void)
{
	if (! g_openglConfig.pGlConfig || ! g_openglConfig.bPBufferAvailable)
		return ;
	int iWidth = 0, iHeight = 0;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_GROUPS; i += 2)
	{
		iWidth = MAX (iWidth, myIconsParam.tIconAuthorizedWidth[i]);
		iHeight = MAX (iHeight, myIconsParam.tIconAuthorizedHeight[i]);
	}
	if (iWidth == 0)
		iWidth = 48;
	if (iHeight == 0)
		iHeight = 48;
	iWidth *= (1 + myIconsParam.fAmplitude);
	iHeight *= (1 + myIconsParam.fAmplitude);
	
	cd_debug ("%s (%dx%d)", __func__, iWidth, iHeight);
	if (g_openglConfig.iIconPbufferWidth != iWidth || g_openglConfig.iIconPbufferHeight != iHeight)
	{
		Display *XDisplay = gdk_x11_get_default_xdisplay ();
		if (g_openglConfig.iconPbuffer != 0)
		{
			glXDestroyPbuffer (XDisplay, g_openglConfig.iconPbuffer);
			glXDestroyContext (XDisplay, g_openglConfig.iconContext);
			g_openglConfig.iconContext = 0;
		}
		g_openglConfig.iconPbuffer = _cairo_dock_create_pbuffer (iWidth, iHeight, &g_openglConfig.iconContext);
		g_openglConfig.iIconPbufferWidth = iWidth;
		g_openglConfig.iIconPbufferHeight = iHeight;
		
		cd_debug ("activating pbuffer, usually buggy drivers will crash here ...");
		if (g_openglConfig.iconPbuffer != 0 && g_openglConfig.iconContext != 0 && glXMakeCurrent (XDisplay, g_openglConfig.iconPbuffer, g_openglConfig.iconContext))
		{
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, iWidth, 0, iHeight, 0.0, 500.0);
			glMatrixMode (GL_MODELVIEW);
			
			glLoadIdentity();
			gluLookAt (0., 0., 3.,
				0., 0., 0.,
				0.0f, 1.0f, 0.0f);
			
			glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
			glClearDepth (1.0f);
		}
		cd_debug (" ok, they are fine enough.\n");
	}
}

void cairo_dock_destroy_icon_pbuffer (void)
{
	Display *XDisplay = gdk_x11_get_default_xdisplay ();
	if (g_openglConfig.iconPbuffer != 0)
	{
		glXDestroyPbuffer (XDisplay, g_openglConfig.iconPbuffer);
		g_openglConfig.iconPbuffer = 0;
	}
	if (g_openglConfig.iconContext != 0)
	{
		glXDestroyContext (XDisplay, g_openglConfig.iconContext);
		g_openglConfig.iconContext = 0;
	}
	g_openglConfig.iIconPbufferWidth = 0;
	g_openglConfig.iIconPbufferHeight = 0;
}*/

gboolean cairo_dock_begin_draw_icon (Icon *pIcon, CairoContainer *pContainer, gint iRenderingMode)
{
	int iWidth, iHeight;
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		GdkGLContext *pGlContext = gtk_widget_get_gl_context (pContainer->pWidget);
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pContainer->pWidget);
		if (! gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return FALSE;
		
		///cairo_dock_set_ortho_view (pContainer);
		iWidth = pContainer->iWidth;
		iHeight = pContainer->iHeight;
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else if (g_openglConfig.iFboId != 0)
	{
		// on attache la texture au FBO.
		cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
		if (pContainer == NULL)
			pContainer = g_pPrimaryContainer;
		GdkGLContext *pGlContext = gtk_widget_get_gl_context (pContainer->pWidget);
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pContainer->pWidget);
		if (! gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return FALSE;
		glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, g_openglConfig.iFboId);  // on redirige sur notre FBO.
		g_openglConfig.bRedirected = (iRenderingMode == 2);
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
			GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_2D,
			g_openglConfig.bRedirected ? g_openglConfig.iRedirectedTexture : pIcon->iIconTexture,
			0);  // attach the texture to FBO color attachment point.
		
		GLenum status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
		if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
		{
			cd_warning ("FBO not ready for %s (tex:%d)", pIcon->cName, pIcon->iIconTexture);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);  // switch back to window-system-provided framebuffer
			glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
				GL_COLOR_ATTACHMENT0_EXT,
				GL_TEXTURE_2D,
				0,
				0);
			return FALSE;
		}
		
		// on se positionne au milieu.
		/**cairo_dock_set_ortho_view (pContainer);
		glLoadIdentity ();
		glTranslatef (iWidth/2, iHeight/2, - iHeight/2);*/
		
		if (iRenderingMode != 1)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	/**else if (g_openglConfig.iconContext != 0)
	{
		Display *XDisplay = gdk_x11_get_default_xdisplay ();
		if (! glXMakeCurrent (XDisplay, g_openglConfig.iconPbuffer, g_openglConfig.iconContext))
			return FALSE;
		glLoadIdentity ();
		glTranslatef (g_openglConfig.iIconPbufferWidth/2, g_openglConfig.iIconPbufferHeight/2, - g_openglConfig.iIconPbufferHeight/2);
	}*/
	else
		return FALSE;
	
	if (pContainer->bPerspectiveView)
	{
		cairo_dock_set_ortho_view (pContainer);
		g_openglConfig.bSetPerspective = TRUE;
	}
	else
	{
		cairo_dock_set_ortho_view (pContainer);  // au demarrage, le contexte n'a pas encore de vue.
		glLoadIdentity ();
		glTranslatef (iWidth/2, iHeight/2, - iHeight/2);
	}
	
	glColor4f(1., 1., 1., 1.);
	
	glScalef (1., -1., 1.);
	
	return TRUE;
}

void cairo_dock_end_draw_icon (Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (pIcon->iIconTexture != 0);
	
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		// copie dans notre texture
		glEnable (GL_TEXTURE_2D);
		glBindTexture (GL_TEXTURE_2D, pIcon->iIconTexture);
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glEnable(GL_BLEND);
		glBlendFunc (GL_ZERO, GL_ONE);
		glColor4f(1., 1., 1., 1.);
		
		int iWidth, iHeight;  // taille de la texture
		cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
		int x = (pContainer->iWidth - iWidth)/2;
		int y = (pContainer->iHeight - iHeight)/2;
		glCopyTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, x, y, iWidth, iHeight, 0);  // target, num mipmap, format, x,y, w,h, border.
		
		glDisable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);
	}
	else if (g_openglConfig.iFboId != 0)
	{
		if (g_openglConfig.bRedirected)  // copie dans notre texture
		{
			glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
				GL_COLOR_ATTACHMENT0_EXT,
				GL_TEXTURE_2D,
				pIcon->iIconTexture,
				0);  // maintenant on dessine dans la texture de l'icone.
			_cairo_dock_enable_texture ();
			_cairo_dock_set_blend_source ();
			
			int iWidth, iHeight;  // taille de la texture
			cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
			
			glLoadIdentity ();
			glTranslatef (iWidth/2, iHeight/2, - iHeight/2);
			_cairo_dock_apply_texture_at_size_with_alpha (g_openglConfig.iRedirectedTexture, iWidth, iHeight, 1.);
			
			_cairo_dock_disable_texture ();
			g_openglConfig.bRedirected = FALSE;
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);  // switch back to window-system-provided framebuffer
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
			GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_2D,
			0,
			0);  // on detache la texture (precaution).
		//glGenerateMipmapEXT(GL_TEXTURE_2D);  // si on utilise les mipmaps, il faut les generer explicitement avec les FBO.
	}
	/**else
	{
		x = (g_openglConfig.iIconPbufferWidth - iWidth)/2;
		y = (g_openglConfig.iIconPbufferHeight - iHeight)/2;
	}*/
		
	///if (CAIRO_DOCK_IS_DESKLET (pContainer))
	if (pContainer && g_openglConfig.bSetPerspective)
	{
		cairo_dock_set_perspective_view (pContainer);
		g_openglConfig.bSetPerspective = FALSE;
	}
	
	if (pContainer)
	{
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pContainer->pWidget);
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
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
void cairo_dock_set_perspective_view (CairoContainer *pContainer)
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

void cairo_dock_set_perspective_view_for_icon (Icon *pIcon, CairoContainer *pContainer)
{
	int w, h;
	cairo_dock_get_icon_extent (pIcon, pContainer, &w, &h);
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
void cairo_dock_set_ortho_view (CairoContainer *pContainer)
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

void cairo_dock_set_ortho_view_for_icon (Icon *pIcon, CairoContainer *pContainer)
{
	int w, h;
	cairo_dock_get_icon_extent (pIcon, pContainer, &w, &h);
	_cairo_dock_set_ortho_view (w, h);
}


void cairo_dock_apply_desktop_background_opengl (CairoContainer *pContainer)
{
	if (! myContainersParam.bUseFakeTransparency || ! g_pFakeTransparencyDesktopBg || g_pFakeTransparencyDesktopBg->iTexture == 0)
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
	W = g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
	H = g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
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




static void _cairo_dock_post_initialize_opengl_backend (GtkWidget* pWidget, gpointer data)  // initialisation necessitant un contexte opengl.
{
	static gboolean bChecked=FALSE;
	if (bChecked)
		return;
	g_print ("*** Check for cairo-desklet\n");
	GdkGLContext* pGlContext = gtk_widget_get_gl_context (pWidget);
	GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (pWidget);
	if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
		return ;
	
	bChecked = TRUE;
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
	
	cd_debug ("OpenGL config summary :\n - bNonPowerOfTwoAvailable : %d\n - bFboAvailable : %d\n - direct rendering : %d\n - bTextureFromPixmapAvailable : %d\n - bAccumBufferAvailable : %d\n - Anisotroy filtering level max : %.1f\n - OpenGL version: %s\n - OpenGL vendor: %s\n - OpenGL renderer: %s\n\n",
		g_openglConfig.bNonPowerOfTwoAvailable,
		g_openglConfig.bFboAvailable,
		!g_openglConfig.bIndirectRendering,
		g_openglConfig.bTextureFromPixmapAvailable,
		g_openglConfig.bAccumBufferAvailable,
		fMaximumAnistropy,
		glGetString (GL_VERSION),
		glGetString (GL_VENDOR),
		glGetString (GL_RENDERER));
	
	gdk_gl_drawable_gl_end (pGlDrawable);
}


static void _reset_opengl_context (GtkWidget* pWidget, gpointer data)
{
	if (! g_bUseOpenGL)
		return ;
	
	GdkGLContext* pGlContext = gtk_widget_get_gl_context (pWidget);
	GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (pWidget);
	if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
		return ;
	
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth (1.0f);
	glClearStencil (0);
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
	
	/// a tester ...
	if (g_bEasterEggs)
		glEnable (GL_MULTISAMPLE_ARB);
	
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);  // GL_MODULATE / GL_DECAL /  GL_BLEND
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MIN_FILTER,
		GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MAG_FILTER,
		GL_LINEAR);
	
	gdk_gl_drawable_gl_end (pGlDrawable);
}
void cairo_dock_set_gl_capabilities (GtkWidget *pWindow)
{
	gboolean bFirstContainer = (! g_pPrimaryContainer || ! g_pPrimaryContainer->pWidget);
	GdkGLContext *pMainGlContext = (bFirstContainer ? NULL : gtk_widget_get_gl_context (g_pPrimaryContainer->pWidget));  // NULL si on est en train de creer la fenetre du main dock, ce qui nous convient.
	gtk_widget_set_gl_capability (pWindow,
		g_openglConfig.pGlConfig,
		pMainGlContext,  // on partage les ressources entre les contextes.
		! g_openglConfig.bIndirectRendering,  // TRUE <=> direct connection to the graphics system.
		GDK_GL_RGBA_TYPE);
	if (bFirstContainer)  // c'est donc le 1er container cree, on finit l'initialisation du backend opengl.
		g_signal_connect (G_OBJECT (pWindow),
			"realize",
			G_CALLBACK (_cairo_dock_post_initialize_opengl_backend),
			NULL);
	g_signal_connect_after (G_OBJECT (pWindow),
		"realize",
		G_CALLBACK (_reset_opengl_context),
		NULL);
}

void cairo_dock_set_default_gl_context (void)
{
	GdkGLContext *pGlContext = gtk_widget_get_gl_context (g_pPrimaryContainer->pWidget);
	GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (g_pPrimaryContainer->pWidget);
	if (gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
		gdk_gl_drawable_gl_end (pGlDrawable);
}

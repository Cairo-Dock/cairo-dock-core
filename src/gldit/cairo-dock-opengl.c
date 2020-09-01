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

#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_string_contains
#include "cairo-dock-icon-facility.h"  // cairo_dock_get_icon_extent
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-desktop-manager.h"  // desktop dimensions

#include "cairo-dock-opengl.h"

// public (manager, config, data)
CairoDockGLConfig g_openglConfig;
gboolean g_bUseOpenGL = FALSE;

// dependencies
extern GldiDesktopBackground *g_pFakeTransparencyDesktopBg;
extern gboolean g_bEasterEggs;

// private
static GldiGLManagerBackend s_backend;
static gboolean s_bInitialized = FALSE;
static gboolean s_bForceOpenGL = FALSE;


gboolean gldi_gl_backend_init (gboolean bForceOpenGL)
{
	memset (&g_openglConfig, 0, sizeof (CairoDockGLConfig));
	g_bUseOpenGL = FALSE;
	s_bForceOpenGL = bForceOpenGL;  // remember it, in case later we can't post-initialize the opengl context
	
	if (s_backend.init)
		g_bUseOpenGL = s_backend.init (bForceOpenGL);
	
	return g_bUseOpenGL;
}

void gldi_gl_backend_deactivate (void)
{
	if (g_bUseOpenGL && s_backend.stop)
		s_backend.stop ();
	g_bUseOpenGL = FALSE;
}

void gldi_gl_backend_force_indirect_rendering (void)
{
	if (g_bUseOpenGL)
		g_openglConfig.bIndirectRendering = TRUE;
}


static inline void _set_perspective_view (int iWidth, int iHeight)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, 1.0*(GLfloat)iWidth/(GLfloat)iHeight, 1., 4*iHeight);
	glViewport(0, 0, iWidth, iHeight);
	glMatrixMode (GL_MODELVIEW);
	
	glLoadIdentity ();
	///gluLookAt (0, 0, 3., 0, 0, 0., 0.0f, 1.0f, 0.0f);
	///glTranslatef (0., 0., -iHeight*(sqrt(3)/2) - 1);
	gluLookAt (iWidth/2, iHeight/2, 3.,  // eye position
		iWidth/2, iHeight/2, 0.,  // center position
		0.0f, 1.0f, 0.0f);  // up direction
	glTranslatef (iWidth/2, iHeight/2, -iHeight*(sqrt(3)/2) - 1);
}
void gldi_gl_container_set_perspective_view (GldiContainer *pContainer)
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
	_set_perspective_view (w, h);
	pContainer->bPerspectiveView = TRUE;
}

void gldi_gl_container_set_perspective_view_for_icon (Icon *pIcon)
{
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	_set_perspective_view (w, h);
}

static inline void _set_ortho_view (int iWidth, int iHeight)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, iWidth, 0, iHeight, 0.0, 500.0);
	glViewport(0, 0, iWidth, iHeight);
	glMatrixMode (GL_MODELVIEW);
	
	glLoadIdentity ();
	gluLookAt (iWidth/2, iHeight/2, 3.,  // eye position
		iWidth/2, iHeight/2, 0.,  // center position
		0.0f, 1.0f, 0.0f);  // up direction
	glTranslatef (iWidth/2, iHeight/2, - iHeight/2);
}
void gldi_gl_container_set_ortho_view (GldiContainer *pContainer)
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
	_set_ortho_view (w, h);
	pContainer->bPerspectiveView = FALSE;
}

void gldi_gl_container_set_ortho_view_for_icon (Icon *pIcon)
{
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	_set_ortho_view (w, h);
}



gboolean gldi_gl_container_make_current (GldiContainer *pContainer)
{
	if (s_backend.container_make_current)
		return s_backend.container_make_current (pContainer);
	return FALSE;
}

static void _apply_desktop_background (GldiContainer *pContainer)
{
	if (! g_pFakeTransparencyDesktopBg || g_pFakeTransparencyDesktopBg->iTexture == 0)
		return ;
	
	glPushMatrix ();
	gboolean bSetPerspective = pContainer->bPerspectiveView;
	if (bSetPerspective)
		gldi_gl_container_set_ortho_view (pContainer);
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
		gldi_gl_container_set_perspective_view (pContainer);
	glPopMatrix ();
}

gboolean gldi_gl_container_begin_draw_full (GldiContainer *pContainer, GdkRectangle *pArea, gboolean bClear)
{
	if (! gldi_gl_container_make_current (pContainer))
		return FALSE;
	
	glLoadIdentity ();
	
	if (pArea != NULL)
	{
		glEnable (GL_SCISSOR_TEST);  // ou comment diviser par 4 l'occupation CPU !
		glScissor ((int) pArea->x,
			(int) (pContainer->bIsHorizontal ? pContainer->iHeight : pContainer->iWidth) -
				pArea->y - pArea->height,  // lower left corner of the scissor box.
			(int) pArea->width,
			(int) pArea->height);
	}
	
	if (bClear)
	{
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		_apply_desktop_background (pContainer);
	}
	
	return TRUE;
}

void gldi_gl_container_end_draw (GldiContainer *pContainer)
{
	glDisable (GL_SCISSOR_TEST);
	if (s_backend.container_end_draw)
		s_backend.container_end_draw (pContainer);
}


static void _init_opengl_context (G_GNUC_UNUSED GtkWidget* pWidget, GldiContainer *pContainer)
{
	if (! gldi_gl_container_make_current (pContainer))
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

// TODO: remove that when Mesa 10.1 will be used by most people
static gboolean _is_blacklisted (const gchar *cVersion, const gchar *cVendor, const gchar *cRenderer)
{
	g_return_val_if_fail (cVersion && cVendor && cRenderer, FALSE);
	if (strstr (cRenderer, "Mesa DRI Intel(R) Ivybridge Mobile") != NULL
	    && (strstr (cVersion, "Mesa 9") != NULL // affect all versions <= 10.0
	        || strstr (cVersion, "Mesa 10.0") != NULL)
	    && strstr (cVendor, "Intel Open Source Technology Center") != NULL)
	{
		cd_warning ("This card is blacklisted due to a bug with your video drivers: Intel 4000 HD Ivybridge Mobile.\n Please install Mesa >= 10.1");
		return TRUE;
	}
	return FALSE;
}

static gboolean _check_gl_extension (const char *extName)
{
	const char *glExtensions = (const char *) glGetString (GL_EXTENSIONS);
	return cairo_dock_string_contains (glExtensions, extName, " ");
}

static void _post_initialize_opengl_backend (G_GNUC_UNUSED GtkWidget *pWidget, GldiContainer *pContainer)  // initialisation necessitant un contexte opengl.
{
	g_return_if_fail (!s_bInitialized);
	
	if (! gldi_gl_container_make_current (pContainer))
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

	const gchar *cVersion  = (const gchar *) glGetString (GL_VERSION);
	const gchar *cVendor   = (const gchar *) glGetString (GL_VENDOR);
	const gchar *cRenderer = (const gchar *) glGetString (GL_RENDERER);

	cd_message ("OpenGL config summary :\n - bNonPowerOfTwoAvailable : %d\n - bFboAvailable : %d\n - direct rendering : %d\n - bTextureFromPixmapAvailable : %d\n - bAccumBufferAvailable : %d\n - Anisotroy filtering level max : %.1f\n - OpenGL version: %s\n - OpenGL vendor: %s\n - OpenGL renderer: %s\n\n",
		g_openglConfig.bNonPowerOfTwoAvailable,
		g_openglConfig.bFboAvailable,
		!g_openglConfig.bIndirectRendering,
		g_openglConfig.bTextureFromPixmapAvailable,
		g_openglConfig.bAccumBufferAvailable,
		fMaximumAnistropy,
		cVersion,
		cVendor,
		cRenderer);

	// we need a context to use glGetString, this is why we did it now
	if (! s_bForceOpenGL && _is_blacklisted (cVersion, cVendor, cRenderer))
	{
		cd_warning ("%s 'cairo-dock -o'\n"
			" OpenGL Version: %s\n OpenGL Vendor: %s\n OpenGL Renderer: %s",
			"The OpenGL backend will be deactivated. Note that you can force "
			"this OpenGL backend by launching the dock with this command:",
			cVersion, cVendor, cRenderer);
		gldi_gl_backend_deactivate ();
	}
}

void gldi_gl_container_init (GldiContainer *pContainer)
{
	if (g_bUseOpenGL && s_backend.container_init)
		s_backend.container_init (pContainer);
	
	// finish the initialisation of the opengl backend, now that we have a window we can bind context to.
	if (! s_bInitialized)
		g_signal_connect (G_OBJECT (pContainer->pWidget),
			"realize",
			G_CALLBACK (_post_initialize_opengl_backend),
			pContainer);
	
	// when the window will be realised, initialise its GL context.
	g_signal_connect (G_OBJECT (pContainer->pWidget),
		"realize",
		G_CALLBACK (_init_opengl_context),
		pContainer);
}

void gldi_gl_container_resized (GldiContainer *pContainer, int iWidth, int iHeight)
{
	if (g_bUseOpenGL && s_backend.container_resized)
		s_backend.container_resized (pContainer, iWidth, iHeight);
}

void gldi_gl_container_finish (GldiContainer *pContainer)
{
	if (g_bUseOpenGL && s_backend.container_finish)
		s_backend.container_finish (pContainer);
}



void gldi_gl_manager_register_backend (GldiGLManagerBackend *pBackend)
{
	gpointer *ptr = (gpointer*)&s_backend;
	gpointer *src = (gpointer*)pBackend;
	gpointer *src_end = (gpointer*)(pBackend + 1);
	while (src != src_end)
	{
		if (*src != NULL)
			*ptr = *src;
		src ++;
		ptr ++;
	}
}

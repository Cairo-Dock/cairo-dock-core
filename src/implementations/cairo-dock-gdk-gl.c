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

#include "cairo-dock-log.h"
#include "cairo-dock-opengl.h"


// dependencies
extern CairoDockGLConfig g_openglConfig;

// private
/*static gboolean _check_client_glx_extension (const char *extName)
{
	Display *display = cairo_dock_get_X_display ();
	const gchar *glxExtensions = glXGetClientString (display, GLX_EXTENSIONS);
	return cairo_dock_string_contains (glxExtensions, extName, " ");
}*/

static gboolean _initialize_opengl_backend (gboolean bForceOpenGL)
{
	g_openglConfig.bStencilBufferAvailable = TRUE;
	g_openglConfig.bAlphaAvailable = TRUE;
	
	//\_________________ on verifier les capacites des textures.
	/*g_openglConfig.bTextureFromPixmapAvailable = _check_client_glx_extension ("GLX_EXT_texture_from_pixmap");
	if (g_openglConfig.bTextureFromPixmapAvailable)
	{
		g_openglConfig.bindTexImage = (gpointer)glXGetProcAddress ((GLubyte *) "glXBindTexImageEXT");
		g_openglConfig.releaseTexImage = (gpointer)glXGetProcAddress ((GLubyte *) "glXReleaseTexImageEXT");
		g_openglConfig.bTextureFromPixmapAvailable = (g_openglConfig.bindTexImage && g_openglConfig.releaseTexImage);
	}*/
	
	return TRUE;
}

static gboolean _container_make_current (GldiContainer *pContainer)
{
	if (!pContainer->glContext)
		pContainer->glContext = gdk_window_create_gl_context (gldi_container_get_gdk_window(pContainer), NULL);
	gdk_gl_context_make_current (pContainer->glContext);
	return TRUE;
}

static void _container_init (GldiContainer *pContainer)
{
	// create a GL context for this container (this way, we can set the viewport once and for all).
	pContainer->glContext = gdk_window_create_gl_context (gldi_container_get_gdk_window(pContainer), NULL);
}


void gldi_register_gdk_gl_backend (void)
{
	GldiGLManagerBackend gmb;
	memset (&gmb, 0, sizeof (GldiGLManagerBackend));
	gmb.init = _initialize_opengl_backend;
	gmb.stop = NULL;
	gmb.container_make_current = _container_make_current;
	gmb.container_end_draw = NULL;
	gmb.container_init = _container_init;
	gmb.container_finish = NULL;
	gldi_gl_manager_register_backend (&gmb);
}


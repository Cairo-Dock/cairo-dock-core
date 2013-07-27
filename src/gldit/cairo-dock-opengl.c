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
#include "cairo-dock-icon-facility.h"  // cairo_dock_get_icon_extent
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-desktop-manager.h"  // desktop dimensions

#include "cairo-dock-opengl.h"

// public (manager, config, data)
CairoDockGLConfig g_openglConfig;
gboolean g_bUseOpenGL = FALSE;

// dependencies
extern GldiDesktopBackground *g_pFakeTransparencyDesktopBg;

// private
static GldiGLManagerBackend s_backend;


gboolean gldi_gl_backend_init (gboolean bForceOpenGL)
{
	memset (&g_openglConfig, 0, sizeof (CairoDockGLConfig));
	g_bUseOpenGL = FALSE;
	
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



gboolean gldi_gl_container_make_current (GldiContainer *pContainer)
{
	if (s_backend.container_make_current)
		return s_backend.container_make_current (pContainer);
	return FALSE;
}

static void _apply_desktop_background (GldiContainer *pContainer)
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


void gldi_gl_container_init (GldiContainer *pContainer)
{
	if (g_bUseOpenGL && s_backend.container_init)
		s_backend.container_init (pContainer);
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

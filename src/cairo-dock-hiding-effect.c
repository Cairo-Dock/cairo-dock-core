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
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <cairo.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif
#include <gtk/gtkgl.h>
#include <GL/glu.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-load.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-config.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-callbacks.h"

extern gboolean g_bUseOpenGL;
extern CairoDockGLConfig g_openglConfig;

static void _pre_render_opengl (CairoDock *pDock, cairo_t *pCairoContext);

static void _pre_render_move_down (CairoDock *pDock, cairo_t *pCairoContext)
{
	int N = (pDock->bIsHiding ? mySystem.iHideNbSteps : mySystem.iUnhideNbSteps);
	int k = (1 - pDock->fHideOffset) * N;
	double a = pow (1./pDock->iMaxDockHeight, 1./N);  // le dernier step est un ecart d'environ 1 pixel.
	double dy = pDock->iMaxDockHeight * pow (a, k) * (pDock->container.bDirectionUp ? 1 : -1);

	if (pCairoContext != NULL)
	{
		if (pDock->container.bIsHorizontal)
			cairo_translate (pCairoContext, 0., dy);
		else
			cairo_translate (pCairoContext, dy, 0.);
	}
	else
	{
		if (pDock->container.bIsHorizontal)
			glTranslatef (0., -dy, 0.);
		else
			glTranslatef (dy, 0., 0.);
	}
}

static void _init_fade_out (CairoDock *pDock)
{
	if (g_bUseOpenGL && ! g_openglConfig.bStencilBufferAvailable)
		cairo_dock_create_redirect_texture_for_dock (pDock);
}

static void _pre_render_fade_out (CairoDock *pDock, cairo_t *pCairoContext)
{
	if (pCairoContext == NULL && ! g_openglConfig.bStencilBufferAvailable && pDock->iFboId != 0)  // pas de glAccum.
	{
		_pre_render_opengl (pDock, pCairoContext);
	}
}

static void _post_render_fade_out (CairoDock *pDock, cairo_t *pCairoContext)
{
	double fAlpha = 1 - pDock->fHideOffset;
	if (pCairoContext != NULL)
	{
		cairo_rectangle (pCairoContext,
			0,
			0,
			pDock->container.bIsHorizontal ? pDock->container.iWidth : pDock->container.iHeight, pDock->container.bIsHorizontal ? pDock->container.iHeight : pDock->container.iWidth);
		cairo_set_line_width (pCairoContext, 0);
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_DEST_OUT);
		cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 1. - fAlpha);
		cairo_fill (pCairoContext);
	}
	else if (g_openglConfig.bStencilBufferAvailable)
	{
		glAccum (GL_LOAD, fAlpha*fAlpha);
		g_print ("%.2f\n", fAlpha*fAlpha);
		glAccum (GL_RETURN, 1.0);
	}
	else if (pDock->iFboId != 0)
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);  // switch back to window-system-provided framebuffer
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
			GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_2D,
			0,
			0);  // on detache la texture (precaution).
		// dessin dans notre fenetre.
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_source ();
		_cairo_dock_set_blend_alpha ();
		
		int iWidth, iHeight;  // taille de la texture
		if (pDock->container.bIsHorizontal)
		{
			iWidth = pDock->container.iWidth;
			iHeight = pDock->container.iHeight;
		}
		else
		{
			iWidth = pDock->container.iHeight;
			iHeight = pDock->container.iWidth;
		}
		
		glPushMatrix ();
		glLoadIdentity ();
		glTranslatef (iWidth/2, iHeight/2, - iHeight/2);
		glScalef (1., -1., 1.);
		_cairo_dock_apply_texture_at_size_with_alpha (pDock->iRedirectedTexture, iWidth, iHeight, fAlpha);
		glPopMatrix ();
		
		_cairo_dock_disable_texture ();
	}
}

static void _init_opengl (CairoDock *pDock)
{
	if (g_bUseOpenGL)
		cairo_dock_create_redirect_texture_for_dock (pDock);
}

static void _pre_render_opengl (CairoDock *pDock, cairo_t *pCairoContext)
{
	if (pCairoContext == NULL)
	{
		if (pDock->iFboId == 0)
			return ;
		
		// on attache la texture au FBO.
		glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, pDock->iFboId);  // on redirige sur notre FBO.
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
			GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_2D,
			pDock->iRedirectedTexture,
			0);  // attach the texture to FBO color attachment point.
		
		GLenum status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
		if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
		{
			cd_warning ("FBO not ready");
			return;
		}
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}

static void _post_render_zoom (CairoDock *pDock, cairo_t *pCairoContext)
{
	if (pCairoContext == NULL)
	{
		if (pDock->iFboId == 0)
			return ;
		
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);  // switch back to window-system-provided framebuffer
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
			GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_2D,
			0,
			0);  // on detache la texture (precaution).
		// dessin dans notre fenetre.
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_source ();
		
		int iWidth, iHeight;  // taille de la texture
		if (pDock->container.bIsHorizontal)
		{
			iWidth = pDock->container.iWidth;
			iHeight = pDock->container.iHeight;
		}
		else
		{
			iWidth = pDock->container.iHeight;
			iHeight = pDock->container.iWidth;
		}
		
		glPushMatrix ();
		glLoadIdentity ();
		glTranslatef (iWidth/2, 0., - iHeight/2);
		
		glScalef (1 - pDock->fHideOffset, 1 - pDock->fHideOffset, 1.);
		glTranslatef (0., iHeight/2, - iHeight/2);
		
		glScalef (1., -1., 1.);
		_cairo_dock_apply_texture_at_size_with_alpha (pDock->iRedirectedTexture, iWidth, iHeight, 1.);
		
		glPopMatrix ();
		
		_cairo_dock_disable_texture ();
	}
}


void cairo_dock_register_hiding_effects (void)
{
	CairoDockHidingEffect *p;
	p = g_new0 (CairoDockHidingEffect, 1);
	p->pre_render = _pre_render_move_down;
	cairo_dock_register_hiding_effect ("Move down", p);
	
	p = g_new0 (CairoDockHidingEffect, 1);
	p->pre_render = _pre_render_fade_out;
	p->post_render = _post_render_fade_out;
	p->init = _init_fade_out;
	cairo_dock_register_hiding_effect ("Fade out", p);
	
	p = g_new0 (CairoDockHidingEffect, 1);
	p->pre_render = _pre_render_opengl;
	p->post_render = _post_render_zoom;
	p->init = _init_opengl;
	cairo_dock_register_hiding_effect ("Zoom out", p);
}


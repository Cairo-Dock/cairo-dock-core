/*
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


#ifndef __CAIRO_DOCK_OPENGL__
#define  __CAIRO_DOCK_OPENGL__

#include <glib.h>

#include <gdk/x11/gdkglx.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-opengl.h This class manages the OpenGL backend and context.
*/


struct _CairoDockGLConfig {
	GdkGLConfig *pGlConfig;
	gboolean bHasBeenForced;
	gboolean bIndirectRendering;
	gboolean bAlphaAvailable;
	gboolean bStencilBufferAvailable;
	gboolean bPBufferAvailable;
	gboolean bNonPowerOfTwoAvailable;
	gboolean bTextureFromPixmapAvailable;
	gint iGlxMajor, iGlxMinor;
	GLXPbuffer iconPbuffer;
	GLXContext iconContext;
	gint iIconPbufferWidth, iIconPbufferHeight;
	void (*bindTexImage) (Display *display, GLXDrawable drawable, int buffer, int *attribList);
	void (*releaseTexImage) (Display *display, GLXDrawable drawable, int buffer);
	} ;


  /////////////
 // CONTEXT //
/////////////
gboolean cairo_dock_initialize_opengl_backend (gboolean bForceIndirectRendering, gboolean bForceOpenGL);

void cairo_dock_check_extensions (void);


  ///////////////////////
 // RENDER TO TEXTURE //
///////////////////////
void cairo_dock_create_icon_pbuffer (void);
void cairo_dock_destroy_icon_pbuffer (void);

/** Initiate an OpenGL drawing session on an icon's texture.
*@param pIcon the icon on which to draw.
*@param pContainer its container.
*@return TRUE if you can proceed to the drawing, FALSE if an error occured.
*/
gboolean cairo_dock_begin_draw_icon (Icon *pIcon, CairoContainer *pContainer);
/** Finish an OpenGL drawing session on an icon.
*@param pIcon the icon on which to draw.
*@param pContainer its container.
*@return TRUE if you can proceed to the drawing, FALSE if an error occured.
*/
void cairo_dock_end_draw_icon (Icon *pIcon, CairoContainer *pContainer);


/** Set a perspective view to the current GL context. Perspective view accentuates the depth effect of the scene, but can distort it on the edges, and is difficult to manipulate because the size of objects depends on their position.
*@param iWidth width of the container
*@param iHeight height of the container
*/
void cairo_dock_set_perspective_view (int iWidth, int iHeight);
/** Set an orthogonal view to the current GL context. Orthogonal view is convenient to draw classic 2D, because the objects are not zoomed according to their position. The drawback is a poor depth effect.
*@param iWidth width of the container
*@param iHeight height of the container
*/
void cairo_dock_set_ortho_view (int iWidth, int iHeight);


void cairo_dock_apply_desktop_background_opengl (CairoContainer *pContainer);


#endif

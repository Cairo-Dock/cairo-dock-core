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

#include "gldi-config.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-opengl.h This class provides some public OpenGL functions for plugins.
*/

  ///////////////
 // CONTAINER //
///////////////

/** Set a perspective view to the current GL context to fit a given Container. You may want to ensure the Container's context is really the current one.
*@param pContainer the container
*/
void gldi_gl_container_set_perspective_view (GldiContainer *pContainer);

/** Set a perspective view to the current GL context to fit a given Icon (which must be inside a Container). You may want to ensure the Icon's Container's context is really the current one.
*@param pIcon the icon
*/
void gldi_gl_container_set_perspective_view_for_icon (Icon *pIcon);

/** Set a orthogonal view to the current GL context to fit a given Container. You may want to ensure the Container's context is really the current one.
*@param pContainer the container
*/
void gldi_gl_container_set_ortho_view (GldiContainer *pContainer);

/** Set a orthogonal view to the current GL context to fit a given Icon (which must be inside a Container). You may want to ensure the Icon's Container's context is really the current one.
*@param pIcon the icon
*/
void gldi_gl_container_set_ortho_view_for_icon (Icon *pIcon);

G_END_DECLS
#endif

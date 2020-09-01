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

#ifndef __CAIRO_DOCK_EGL__
#define  __CAIRO_DOCK_EGL__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/*
*@file cairo-dock-egl.h This class provides EGL support.
*/

void gldi_register_egl_backend (void);

#ifdef HAVE_EGL
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <EGL/egl.h>

/* backend-specific code to get the proper EGLDisplay */
EGLDisplay* egl_get_display_x11(GdkDisplay* dsp);
EGLDisplay* egl_get_display_wayland(GdkDisplay* dsp);

void egl_init_surface_X11 (GldiContainer *pContainer, EGLDisplay* dpy, EGLConfig conf);
void egl_init_surface_wayland (GldiContainer *pContainer, EGLDisplay* dpy, EGLConfig conf);

void egl_window_resize_wayland (GldiContainer* pContainer, int iWidth, int iHeight);

G_END_DECLS
#endif
#endif


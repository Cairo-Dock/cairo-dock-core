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

#ifndef __GLDI_CORE__
#define  __GLDI_CORE__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-core.h This class instanciates the different core managers.
*/

typedef enum {
	GLDI_DEFAULT,
	GLDI_OPENGL,
	GLDI_CAIRO,
	} GldiRenderingMethod;

void gldi_init (GldiRenderingMethod iRendering);


void gldi_free_all (void);

/// Get some basic info about the features supported by Cairo-Dock and
/// detected at runtime. Returns a dynamically allocated string that
/// should be freed by the caller after using it.
gchar *gldi_get_diag_msg (void);

G_END_DECLS
#endif

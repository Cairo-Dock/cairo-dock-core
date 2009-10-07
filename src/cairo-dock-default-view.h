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


#ifndef __CAIRO_DOCK_DEFAULT_VIEW__
#define  __CAIRO_DOCK_DEFAULT_VIEW__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cd_calculate_max_dock_size_default (CairoDock *pDock);


void cd_render_default (cairo_t *pCairoContext, CairoDock *pDock);


void cd_render_optimized_default (cairo_t *pCairoContext, CairoDock *pDock, GdkRectangle *pArea);


void cd_render_opengl_default (CairoDock *pDock);


Icon *cd_calculate_icons_default (CairoDock *pDock);


void cairo_dock_register_default_renderer (void);


G_END_DECLS
#endif

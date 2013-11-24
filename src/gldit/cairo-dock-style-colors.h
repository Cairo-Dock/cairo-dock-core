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

#ifndef __GLDI_STYLE_COLORS__
#define  __GLDI_STYLE_COLORS__

#include "cairo-dock-struct.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-style-colors.h This class holds the style of Dialogs, Menus, Labels, etc.
* It defines background, selected background, outline and text colors.
*/

void gldi_style_color_shade (double *icolor, double shade, double *ocolor);


void gldi_style_colors_freeze (void);

int gldi_style_colors_get_index (void);

void gldi_style_colors_init (void);

void gldi_style_colors_reload (void);


void gldi_style_colors_set_bg_color (cairo_t *pCairoContext);

void gldi_style_colors_set_selected_bg_color (cairo_t *pCairoContext);

void gldi_style_colors_set_line_color (cairo_t *pCairoContext);

void gldi_style_colors_get_text_color (double *pColor);

void gldi_style_colors_paint_bg_color (cairo_t *pCairoContext, int iWidth);



G_END_DECLS
#endif

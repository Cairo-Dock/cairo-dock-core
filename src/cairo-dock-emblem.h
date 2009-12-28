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

#ifndef __CAIRO_DOCK_EMBLEM__
#define  __CAIRO_DOCK_EMBLEM__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

typedef enum {
	CAIRO_DOCK_EMBLEM_UPPER_RIGHT = 0,
	CAIRO_DOCK_EMBLEM_LOWER_RIGHT,
	CAIRO_DOCK_EMBLEM_UPPER_LEFT,
	CAIRO_DOCK_EMBLEM_LOWER_LEFT,
	CAIRO_DOCK_EMBLEM_MIDDLE,
	CAIRO_DOCK_EMBLEM_NB_POSITIONS,
} CairoEmblemPosition;

struct _CairoEmblem {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	int iWidth;
	int iHeight;
	CairoEmblemPosition iPosition;
};

CairoEmblem *cairo_dock_make_emblem (const gchar *cImageFile, Icon *pIcon, CairoContainer *pContainer, cairo_t *pSourceContext);

CairoEmblem *cairo_dock_make_emblem_from_surface (cairo_surface_t *pSurface, int iSurfaceWidth, int iSurfaceHeight, Icon *pIcon, CairoContainer *pContainer);

CairoEmblem *cairo_dock_make_emblem_from_texture (GLuint iTexture, Icon *pIcon, CairoContainer *pContainer);

#define cairo_dock_set_emblem_position(pEmblem, p) pEmblem->iPosition = p


void cairo_dock_free_emblem (CairoEmblem *pEmblem);


void cairo_dock_draw_emblem_on_icon (CairoEmblem *pEmblem, Icon *pIcon, CairoContainer *pContainer);


G_END_DECLS
#endif

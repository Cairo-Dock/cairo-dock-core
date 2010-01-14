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

/**
*@file cairo-dock-emblem.h This class defines Emblems, that are small images superimposed on the icon at a given place.
*
* The emblem is drawn directly on the icon, so it modifies its surface/texture itself. Thus, to remove an emblem, you have to set the original image on the icon.
*
* Emblems can be placed in the corners of the icon, or in the middle of it.
*
* Usage :
* create an Emblem with \ref cairo_dock_make_emblem
* set its position with \ref cairo_dock_set_emblem_position
* you can then render the emblem on the icon with \ref cairo_dock_draw_emblem_on_icon.
* free the emblem when you're done with \ref cairo_dock_free_emblem
*
* An emblem can be used as many times as you want on any icon. The only limitation is that an emblem uses either Cairo or OpenGL, this is decided at the creation time; so the icons you draw the emblem on must be drawn with the same rendering.
*/

/// Available position of the emblem on the icon.
typedef enum {
	CAIRO_DOCK_EMBLEM_UPPER_RIGHT = 0,
	CAIRO_DOCK_EMBLEM_LOWER_RIGHT,
	CAIRO_DOCK_EMBLEM_UPPER_LEFT,
	CAIRO_DOCK_EMBLEM_LOWER_LEFT,
	CAIRO_DOCK_EMBLEM_MIDDLE,
	CAIRO_DOCK_EMBLEM_NB_POSITIONS,
} CairoEmblemPosition;

/// Definition of an Emblem. You shouldn't access any of its fields directly.
struct _CairoEmblem {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	int iWidth;
	int iHeight;
	CairoEmblemPosition iPosition;
};

/** Create an emblem from an image, that suits the given icon and container. If the image is given by its sole name, it is searched inside the current theme root folder.
*@param cImageFile an image.
*@param pIcon an icon.
*@param pContainer its container.
*@param pSourceContext a cairo context, not altered by the function.
*@return the newly allocated emblem.
*/
CairoEmblem *cairo_dock_make_emblem (const gchar *cImageFile, Icon *pIcon, CairoContainer *pContainer, cairo_t *pSourceContext);

/** Create an emblem from an existing surface. The surface is appropriated by the emblem, so if you free it with \ref cairo_dock_free_emblem, it will also free the surface. Use g_free to destroy the emblem if you don't want the surface to be destroyed with.
*@param pSurface a surface.
*@param iSurfaceWidth width of the surface, 0 means it has the same width as the icon.
*@param iSurfaceHeight height of the surface, 0 means it has the same height as the icon.
*@param pIcon an icon.
*@param pContainer its container.
*@return the newly allocated emblem.
*/
CairoEmblem *cairo_dock_make_emblem_from_surface (cairo_surface_t *pSurface, int iSurfaceWidth, int iSurfaceHeight, Icon *pIcon, CairoContainer *pContainer);

/** Create an emblem from an existing texture. The texture is appropriated by the emblem, so if you free it with \ref cairo_dock_free_emblem, it will also free the texture. Use g_free to destroy the emblem if you don't want the texture to be destroyed with.
*@param iTexture a texture.
*@param pIcon an icon.
*@param pContainer its container.
*@return the newly allocated emblem.
*/
CairoEmblem *cairo_dock_make_emblem_from_texture (GLuint iTexture, Icon *pIcon, CairoContainer *pContainer);

/** Set the position of an emblem.
*@param pEmblem the emblem
*@param pos the position (a \ref CairoEmblemPosition)
*/
#define cairo_dock_set_emblem_position(pEmblem, pos) pEmblem->iPosition = pos

/** Destroy an emblem and all its allocated ressources.
*@param pEmblem the emblem
*/
void cairo_dock_free_emblem (CairoEmblem *pEmblem);


/** Permanently draw an emblem on an icon.
*@param pEmblem the emblem
*@param pIcon an icon
*@param pContainer its container
*/
void cairo_dock_draw_emblem_on_icon (CairoEmblem *pEmblem, Icon *pIcon, CairoContainer *pContainer);


G_END_DECLS
#endif

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


#ifndef __CAIRO_DOCK_SEPARATOR_FACTORY__
#define  __CAIRO_DOCK_SEPARATOR_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


cairo_surface_t *cairo_dock_create_separator_surface (cairo_t *pSourceContext, double fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp, double *fWidth, double *fHeight);

/** Create an icon that will act as a separator.
*@param pSourceContext a cairo context
*@param iSeparatorType the type of separator (CAIRO_DOCK_SEPARATOR12, CAIRO_DOCK_SEPARATOR23 or any other odd number)
*@param pDock the dock it will belong to
*@return the newly allocated icon, with all buffers filled.
*/
Icon *cairo_dock_create_separator_icon (cairo_t *pSourceContext, int iSeparatorType, CairoDock *pDock);


G_END_DECLS
#endif


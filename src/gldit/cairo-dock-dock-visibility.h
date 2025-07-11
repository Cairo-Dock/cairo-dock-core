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

#ifndef __CAIRO_DOCK_VISIBILITY__
#define  __CAIRO_DOCK_VISIBILITY__

#include "cairo-dock-struct.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-dock-visibility.h This class manages the visibility of Docks.
*/

/** Re-check if the given dock should be shown given its visibility settings */
void gldi_dock_visibility_refresh (CairoDock *pDock);

/** Get the whether any application window overlaps the given dock.
*@param pDock the dock to test.
*@return whether an overlapping window has been found.
*/
gboolean gldi_dock_has_overlapping_window (CairoDock *pDock);


void gldi_docks_visibility_start (void);

void gldi_docks_visibility_stop (void);  // not used yet

G_END_DECLS
#endif

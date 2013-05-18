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

#define _cairo_dock_appli_is_on_our_way(pAppli, pDock) (pAppli != NULL && gldi_window_is_on_current_desktop (pAppli) &&  pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP && gldi_window_overlaps_dock (pAppli, pDock))

void gldi_dock_hide_show_if_current_window_is_on_our_way (CairoDock *pDock);

void gldi_dock_hide_if_any_window_overlap_or_show (CairoDock *pDock);

gboolean gldi_dock_overlaps_window (CairoDock *pDock, GldiWindowActor *actor);
#define gldi_window_overlaps_dock(actor, pDock) gldi_dock_overlaps_window (pDock, actor)


/** Get the application whose window overlaps a dock, or NULL if none.
*@param pDock the dock to test.
*@return the window actor, or NULL if none has been found.
*/
GldiWindowActor *gldi_dock_search_overlapping_window (CairoDock *pDock);


void gldi_docks_visibility_start (void);

void gldi_docks_visibility_stop (void);  // not used yet

G_END_DECLS
#endif

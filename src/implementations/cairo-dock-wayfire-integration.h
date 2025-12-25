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


#ifndef __CAIRO_DOCK_WAYFIRE_INTEGRATION__
#define  __CAIRO_DOCK_WAYFIRE_INTEGRATION__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-wayfire-integration.h This class implements the integration of Wayfire inside Cairo-Dock.
*/

/** Init the Wayfire integration backend. Will try to connect Wayfire's IPC
 * socket and query the available functions. */
void cd_init_wayfire_backend (void);
/** Init setting and tracking the sticky and always on top states if available.
 * Can be called either before or after cd_init_wayfire_backend (). */
void gldi_wf_init_sticky_above (void);
/** Return if setting the sticky or always on top states on windows is possible.
 * Should call gldi_wf_init_sticky_above () before this function. */
void gldi_wf_can_sticky_above (gboolean *bCanSticky, gboolean *bCanAbove);
/** Set or unset the sticky state for the given window actor. */
void gldi_wf_set_sticky (GldiWindowActor *actor, gboolean bSticky);
/** Set or unset the always on top state for the given window actor. */
void gldi_wf_set_above (GldiWindowActor *actor, gboolean bAbove);
/** Check whether a window is kept above or below of other windows.
 * Note:
 *  - bIsBelow will always be set to FALSE (not supported)
 *  - bIsAbove only reflects the state set by us, not by other methods (e.g. keybindings)
 */
void gldi_wf_is_above_or_below (GldiWindowActor *actor, gboolean *bIsAbove, gboolean *bIsBelow);

G_END_DECLS
#endif


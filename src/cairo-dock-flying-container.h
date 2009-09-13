/**
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


#ifndef __CAIRO_FLYING_CONTAINER__
#define  __CAIRO_FLYING_CONTAINER__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

struct _CairoFlyingContainer {
	/// container
	CairoContainer container;
	/// the flying icon
	Icon *pIcon;
	/// Whether to draw the hand or not.
	gboolean bDrawHand;
};

gboolean cairo_dock_update_flying_container_notification (gpointer pUserData, CairoFlyingContainer *pFlyingContainer, gboolean *bContinueAnimation);

gboolean cairo_dock_render_flying_container_notification (gpointer pUserData, CairoFlyingContainer *pFlyingContainer, cairo_t *pCairoContext);


CairoFlyingContainer *cairo_dock_create_flying_container (Icon *pFlyingIcon, CairoDock *pOriginDock, gboolean bDrawHand);

void cairo_dock_drag_flying_container (CairoFlyingContainer *pFlyingContainer, CairoDock *pOriginDock);

void cairo_dock_free_flying_container (CairoFlyingContainer *pFlyingContainer);

void cairo_dock_terminate_flying_container (CairoFlyingContainer *pFlyingContainer);


G_END_DECLS
#endif

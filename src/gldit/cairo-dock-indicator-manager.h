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


#ifndef __CAIRO_DOCK_INDICATOR_MANAGER__
#define  __CAIRO_DOCK_INDICATOR_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-internal-indicators.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-indicator-manager.h This class loads the indicators and manages the associated ressources.
*/

void cairo_dock_init_indicator_manager (void);

void cairo_dock_load_indicator_textures (void);

void cairo_dock_unload_indicator_textures (void);

void cairo_dock_reload_indicators (CairoConfigIndicators *pPrevIndicators, CairoConfigIndicators *pIndicators);


G_END_DECLS
#endif

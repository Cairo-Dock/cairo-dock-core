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

#include <stdlib.h>

#include "../config.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-data-renderer-manager.h"
#include "cairo-dock-gauge.h"
#include "cairo-dock-graph.h"
#include "cairo-dock-default-view.h"
#include "cairo-dock-hiding-effect.h"
#include "cairo-dock-icon-container.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-core.h"

extern CairoContainer *g_pPrimaryContainer;

static void _gldi_register_core_managers (void)
{
	gldi_register_containers_manager ();
	gldi_register_icons_manager ();
	gldi_register_docks_manager ();
	gldi_register_desklets_manager ();
	gldi_register_dialogs_manager ();
	gldi_register_flying_manager ();
	gldi_register_applications_manager ();
	gldi_register_modules_manager ();
	gldi_register_backends_manager ();
	gldi_register_desktop_manager ();
	gldi_register_connection_manager ();
	gldi_register_indicators_manager ();
	gldi_register_gui_manager ();
	gldi_register_shortcuts_manager ();
	gldi_register_data_renderers_manager ();
	gldi_register_desktop_environment_manager ();
}

void gldi_init (GldiRenderingMethod iRendering)
{
	// allow messages.
	cd_log_init (FALSE);  // warnings by default.
	
	// register all managers
	_gldi_register_core_managers ();
	
	// init lib
	if (!g_thread_supported ())
		g_thread_init (NULL);
	
	gldi_init_managers ();
	
	// register internal backends.
	cairo_dock_register_built_in_data_renderers ();
	
	cairo_dock_register_hiding_effects ();
	
	cairo_dock_register_default_renderer ();
	
	cairo_dock_register_icon_container_renderers ();
	
	// set up rendering method.
	if (iRendering != GLDI_CAIRO)  // if cairo, nothing to do.
	{
		cairo_dock_initialize_opengl_backend (iRendering == GLDI_OPENGL);  // TRUE <=> force.
	}
}

void gldi_free_all (void)
{
	if (g_pPrimaryContainer == NULL)
		return ;
	
	// unload all data.
	gldi_unload_managers ();
	
	// reset specific managers.
	cairo_dock_reset_applications_manager ();  // y compris les applis detachees ou blacklistees.
	
	cairo_dock_deactivate_all_modules ();  // y compris les modules qui n'ont pas d'icone.
	
	cairo_dock_reset_docks_table ();  // detruit tous les docks, vide la table, et met le main-dock a NULL.
}

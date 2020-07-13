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
#include <locale.h>

#include "gldi-config.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-user-icon-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-wayland-manager.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-module-instance-manager.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-style-manager.h"
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-data-renderer-manager.h"
#include "cairo-dock-gauge.h"
#include "cairo-dock-graph.h"
#include "cairo-dock-default-view.h"
#include "cairo-dock-hiding-effect.h"
#include "cairo-dock-icon-container.h"
#include "cairo-dock-utils.h"  // cairo_dock_get_version_from_string
#include "cairo-dock-file-manager.h"
#include "cairo-dock-overlay.h"
#include "cairo-dock-log.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-core.h"

extern GldiContainer *g_pPrimaryContainer;
int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;  // version de la lib.

gboolean g_bForceWayland = FALSE;
gboolean g_bForceX11 = FALSE;

static void _gldi_register_core_managers (void)
{
	gldi_register_managers_manager ();  // must be first, since all managers derive from it
	gldi_register_containers_manager ();
	gldi_register_docks_manager ();
	gldi_register_desklets_manager ();
	gldi_register_dialogs_manager ();
	gldi_register_flying_manager ();
	gldi_register_icons_manager ();
	gldi_register_user_icons_manager ();
	gldi_register_launchers_manager ();
	gldi_register_stack_icons_manager ();
	gldi_register_class_icons_manager ();
	gldi_register_separator_icons_manager ();
	gldi_register_applications_manager ();
	gldi_register_applet_icons_manager ();
	gldi_register_modules_manager ();
	gldi_register_module_instances_manager ();
	gldi_register_windows_manager ();
	gldi_register_desktop_manager ();
	gldi_register_indicators_manager ();
	gldi_register_overlays_manager ();
	gldi_register_backends_manager ();
	gldi_register_connection_manager ();
	gldi_register_shortkeys_manager ();
	gldi_register_data_renderers_manager ();
	gldi_register_desktop_environment_manager ();
	gldi_register_style_manager ();  // get config before other manager that could use this manager
	if (!g_bForceWayland) gldi_register_X_manager ();
	if (!g_bForceX11) gldi_register_wayland_manager ();
}

void gldi_init (GldiRenderingMethod iRendering)
{
	// allow messages.
	cd_log_init (FALSE);  // warnings by default.
	setlocale (LC_NUMERIC, "C");  // avoid stupid conversion of decimal
	
	cairo_dock_get_version_from_string (GLDI_VERSION, &g_iMajorVersion, &g_iMinorVersion, &g_iMicroVersion);
	
	// register all managers
	_gldi_register_core_managers ();
	
	gldi_managers_init ();
	
	// register internal backends.
	cairo_dock_register_built_in_data_renderers ();
	
	cairo_dock_register_hiding_effects ();
	
	cairo_dock_register_default_renderer ();
	
	cairo_dock_register_icon_container_renderers ();
	
	// set up rendering method.
	if (iRendering != GLDI_CAIRO)  // if cairo, nothing to do.
	{
		gldi_gl_backend_init (iRendering == GLDI_OPENGL);  // TRUE <=> force.
	}
}

void gldi_free_all (void)
{
	if (g_pPrimaryContainer == NULL)
		return ;
	
	// unload all data.
	gldi_managers_unload ();
	
	// reset specific managers.
	gldi_modules_deactivate_all ();  /// TODO: try to do that in the unload of the manager...
	
	cairo_dock_reset_docks_table ();  // detruit tous les docks, vide la table, et met le main-dock a NULL.
}

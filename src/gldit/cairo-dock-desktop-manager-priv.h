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

#ifndef __GLDI_DESKTOP_MANAGER_PRIV__
#define  __GLDI_DESKTOP_MANAGER_PRIV__

#include "cairo-dock-struct.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-desktop-manager-priv.h This class manages the desktop: screen geometry, current desktop/viewport, etc, and notifies for any change on it.
*  Private interface, only used in core.
*/


typedef void (*CairoDockGrabKeyResult) (GldiShortkey *pBinding);

/// Definition of the Desktop Manager backend.
struct _GldiDesktopManagerBackend {
	void (*present_class) (const gchar *cClass, CairoDockDesktopManagerActionResult cb, gpointer user_data);
	void (*present_windows) (void);
	void (*present_desktops) (void);
	void (*show_widget_layer) (void);
	void (*set_on_widget_layer) (GldiContainer *pContainer, gboolean bOnWidgetLayer);
	void (*show_hide_desktop) (gboolean bShow);
	gboolean (*desktop_is_visible) (void);
	gchar** (*get_desktops_names) (void);
	gboolean (*set_desktops_names) (gchar **cNames);
	cairo_surface_t* (*get_desktop_bg_surface) (void);
	void (*set_current_desktop) (int iDesktopNumber, int iViewportNumberX, int iViewportNumberY);
	void (*refresh) (void);
	void (*grab_shortkey) (GldiShortkey *pBinding, gboolean grab, CairoDockGrabKeyResult cb); // note: cb will only be called if grab == TRUE
	void (*add_workspace) (void); // gldi_desktop_add_workspace ()
	void (*remove_last_workspace) (void); // gldi_desktop_remove_last_workspace ()
	gchar* (*get_monitor_description) (GdkMonitor *pMonitor);
	const gchar* (*get_monitor_name) (GdkMonitor *pMonitor);
	};
typedef struct _GldiDesktopManagerBackend GldiDesktopManagerBackend;


  /////////////////////
 // Desktop Backend //
/////////////////////

/** Register a Desktop Manager backend. NULL functions do not overwrite existing ones.
*@param pBackend a Desktop Manager backend; can be freed after.
*/
void gldi_desktop_manager_register_backend (GldiDesktopManagerBackend *pBackend, const gchar *name);

const gchar *gldi_desktop_manager_get_backend_names (void);

/** Register or unregister a keybinding that will then trigger the
* NOTIFICATION_SHORTKEY_PRESSED signal.
*@param pBinding the keybinding to register or unregister
*@param grab whether to register a keybinding or unregister an existing one
*@param cb a callback function to notify the caller of the result; it will
  only be called if grab == TRUE (it is assumed that unbinding always works).
*
* Note: pBinding->bSuccess will be updated based on the result.
*/
void gldi_desktop_grab_shortkey (GldiShortkey *pBinding, gboolean grab, CairoDockGrabKeyResult cb);


void gldi_register_desktop_manager (void);

G_END_DECLS
#endif

/*
 * cairo-dock-cosmic-toplevel.c
 * 
 * Interact with Wayland clients via the cosmic_toplevel_info_unstable_v1
 * and cosmic_toplevel_management_unstable_v1 protocol.
 * See for more info: https://github.com/pop-os/cosmic-protocols
 * 
 * Copyright 2020-2024 Daniel Kondor <kondor.dani@gmail.com>
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
 * 
 */

#include "gldi-config.h"
#ifdef HAVE_WAYLAND

#include <gdk/gdkwayland.h>
#include "wayland-cosmic-toplevel-management-client-protocol.h"
#include "wayland-cosmic-toplevel-info-client-protocol.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-cosmic-toplevel.h"
#include "cairo-dock-wayland-wm.h"
#include "cairo-dock-cosmic-workspaces.h"

#include <stdio.h>


typedef struct zcosmic_toplevel_handle_v1 wfthandle;


/**********************************************************************
 * static variables used                                              */

static struct zcosmic_toplevel_info_v1* s_ptoplevel_info = NULL;
static struct zcosmic_toplevel_manager_v1* s_ptoplevel_manager = NULL;

static gboolean can_close = FALSE;
static gboolean can_activate = FALSE;
static gboolean can_maximize = FALSE;
static gboolean can_minimize = FALSE;
static gboolean can_fullscreen = FALSE;
static gboolean can_move_workspace = FALSE;

static gboolean manager_found = FALSE;
static gboolean info_found = FALSE;
static uint32_t manager_id, info_id, manager_version, info_version;

/**********************************************************************
 * window manager interface -- toplevel manager                       */

static void _move_to_nth_desktop (GldiWindowActor *actor, G_GNUC_UNUSED int iNumDesktop,
	int x, int y)
{
	if (!s_ws_output) return;
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	struct zcosmic_workspace_handle_v1 *ws = gldi_cosmic_workspaces_get_handle (x, y);
	//!! TODO: we need a valid wl_output here !!
	if (ws) zcosmic_toplevel_manager_v1_move_to_workspace (s_ptoplevel_manager, wactor->handle, ws, s_ws_output);
}

static void _show (GldiWindowActor *actor)
{
	if (!can_activate) return;
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	GdkDisplay *dsp = gdk_display_get_default ();
	GdkSeat *seat = gdk_display_get_default_seat (dsp);
	struct wl_seat* wl_seat = gdk_wayland_seat_get_wl_seat (seat);
	zcosmic_toplevel_manager_v1_activate (s_ptoplevel_manager, wactor->handle, wl_seat);
}
static void _close (GldiWindowActor *actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	zcosmic_toplevel_manager_v1_close (s_ptoplevel_manager, wactor->handle);
}
static void _minimize (GldiWindowActor *actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	zcosmic_toplevel_manager_v1_set_minimized (s_ptoplevel_manager, wactor->handle);
}
static void _maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	if (bMaximize) zcosmic_toplevel_manager_v1_set_maximized (s_ptoplevel_manager, wactor->handle);
	else zcosmic_toplevel_manager_v1_unset_maximized (s_ptoplevel_manager, wactor->handle);
}

static GldiWindowActor* _get_transient_for(GldiWindowActor* actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	wfthandle* parent = wactor->parent;
	if (!parent) return NULL;
	GldiWaylandWindowActor *pactor = zcosmic_toplevel_handle_v1_get_user_data (parent);
	return (GldiWindowActor*)pactor;
}

static void _can_minimize_maximize_close ( G_GNUC_UNUSED GldiWindowActor *actor, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose)
{
	/// TODO: maybe dialogs cannot be minimized / maximized? But they are not shown in the taskbar anyway
	*bCanMinimize = can_minimize;
	*bCanMaximize = can_maximize;
	*bCanClose = can_close;
}

/// TODO: which one of these two are really used? In cairo-dock-X-manager.c,
/// they seem to do the same thing
static void _set_thumbnail_area (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y, int w, int h)
{
	if ( ! (actor && pContainerWidget) ) return;
	if (w < 0 || h < 0) return;
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	GdkWindow* window = gtk_widget_get_window (pContainerWidget);
	if (!window) return;
	struct wl_surface* surface = gdk_wayland_window_get_wl_surface (window);
	if (!surface) return;
	
	zcosmic_toplevel_manager_v1_set_rectangle(s_ptoplevel_manager, wactor->handle, surface, x, y, w, h);
}

static void _set_minimize_position (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y)
{
	_set_thumbnail_area (actor, pContainerWidget, x, y, 1, 1);
}

static void _set_fullscreen (GldiWindowActor *actor, gboolean bFullScreen)
{
	if (!actor) return;
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	if (bFullScreen) zcosmic_toplevel_manager_v1_set_fullscreen (s_ptoplevel_manager, wactor->handle, NULL);
	else zcosmic_toplevel_manager_v1_unset_fullscreen (s_ptoplevel_manager, wactor->handle);
}

static void _capabilities_cb (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zcosmic_toplevel_manager_v1 *manager,
	struct wl_array *c)
{
	can_close = FALSE;
	can_activate = FALSE;
	can_maximize = FALSE;
	can_minimize = FALSE;
	can_fullscreen = FALSE;
	can_move_workspace = FALSE;

	int i;
	uint32_t* cdata = (uint32_t*)c->data;
	for (i = 0; i * sizeof(uint32_t) < c->size; i++) switch (cdata[i])
	{
		case ZCOSMIC_TOPLEVEL_MANAGER_V1_ZCOSMIC_TOPLELEVEL_MANAGEMENT_CAPABILITIES_V1_CLOSE:
			can_close = TRUE; break;        
		case ZCOSMIC_TOPLEVEL_MANAGER_V1_ZCOSMIC_TOPLELEVEL_MANAGEMENT_CAPABILITIES_V1_ACTIVATE:
			can_activate = TRUE; break;     
		case ZCOSMIC_TOPLEVEL_MANAGER_V1_ZCOSMIC_TOPLELEVEL_MANAGEMENT_CAPABILITIES_V1_MAXIMIZE:
			can_maximize = TRUE; break;     
		case ZCOSMIC_TOPLEVEL_MANAGER_V1_ZCOSMIC_TOPLELEVEL_MANAGEMENT_CAPABILITIES_V1_MINIMIZE:
			can_minimize = TRUE; break;     
		case ZCOSMIC_TOPLEVEL_MANAGER_V1_ZCOSMIC_TOPLELEVEL_MANAGEMENT_CAPABILITIES_V1_FULLSCREEN:
			can_fullscreen = TRUE; break;   
		case ZCOSMIC_TOPLEVEL_MANAGER_V1_ZCOSMIC_TOPLELEVEL_MANAGEMENT_CAPABILITIES_V1_MOVE_TO_WORKSPACE:
			can_move_workspace = TRUE; break;
	}
}



/**********************************************************************
 * callbacks -- toplevel info                                         */

static void _gldi_toplevel_title_cb (void *data, G_GNUC_UNUSED wfthandle *handle, const char *title)
{
	gldi_wayland_wm_title_changed (data, title, FALSE);
}

static void _gldi_toplevel_appid_cb (void *data, G_GNUC_UNUSED wfthandle *handle, const char *app_id)
{
	gldi_wayland_wm_appid_changed (data, app_id, FALSE);
}

static void _gldi_toplevel_output_enter_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED wfthandle *handle, G_GNUC_UNUSED struct wl_output *output)
{
	/* TODO -- or maybe we don't care about this? */
}
static void _gldi_toplevel_output_leave_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED wfthandle *handle, G_GNUC_UNUSED struct wl_output *output)
{
	
}

static void _gldi_toplevel_state_cb (void *data, G_GNUC_UNUSED wfthandle *handle, struct wl_array *state)
{
	if (!data) return;
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	gboolean maximized_pending = FALSE;
	gboolean minimized_pending = FALSE;
	gboolean fullscreen_pending = FALSE;
	int i;
	uint32_t* stdata = (uint32_t*)state->data;
	for (i = 0; i*sizeof(uint32_t) < state->size; i++)
	{
		if (stdata[i] == ZCOSMIC_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
			gldi_wayland_wm_activated (wactor, FALSE);
		else if (stdata[i] == ZCOSMIC_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
			maximized_pending = TRUE;
		else if (stdata[i] == ZCOSMIC_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
			minimized_pending = TRUE;
		else if (stdata[i] == ZCOSMIC_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
			fullscreen_pending = TRUE;
	}
	
	gldi_wayland_wm_maximized_changed (wactor, maximized_pending, FALSE);
	gldi_wayland_wm_minimized_changed (wactor, minimized_pending, FALSE);
	gldi_wayland_wm_fullscreen_changed (wactor, fullscreen_pending, FALSE);
}

static void _gldi_toplevel_done_cb (void *data, G_GNUC_UNUSED wfthandle *handle)
{
	gldi_wayland_wm_done (data);
}

static void _gldi_toplevel_closed_cb (void *data, G_GNUC_UNUSED wfthandle *handle)
{
	gldi_wayland_wm_closed (data, TRUE);
}

/*
static void _gldi_toplevel_parent_cb (void* data, G_GNUC_UNUSED wfthandle *handle, wfthandle *parent)
{
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	wactor->parent = parent;
}
*/


static void _workspace_entered (void *data, G_GNUC_UNUSED wfthandle *handle, struct zcosmic_workspace_handle_v1 *wshandle)
{
	cd_warning ("%p -- workspace: %p", handle, wshandle);
	gldi_cosmic_workspaces_update_window ((GldiWindowActor*)data, wshandle);
	gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, data);
}

static void _dummy (G_GNUC_UNUSED void *data, G_GNUC_UNUSED wfthandle *handle, G_GNUC_UNUSED void *workspace)
{
	
}



/**********************************************************************
 * interface and object manager definitions                           */

static struct zcosmic_toplevel_manager_v1_listener cosmic_manager_interface = {
	.capabilities = _capabilities_cb
};

static struct zcosmic_toplevel_handle_v1_listener gldi_toplevel_handle_interface = {
    .title        = _gldi_toplevel_title_cb,
    .app_id       = _gldi_toplevel_appid_cb,
    .output_enter = _gldi_toplevel_output_enter_cb,
    .output_leave = _gldi_toplevel_output_leave_cb,
    .state        = _gldi_toplevel_state_cb,
    .done         = _gldi_toplevel_done_cb,
    .closed       = _gldi_toplevel_closed_cb,
//    .parent       = _gldi_toplevel_parent_cb,
    .workspace_enter = _workspace_entered,
    .workspace_leave = (void (*)(void*, struct zcosmic_toplevel_handle_v1*, struct zcosmic_workspace_handle_v1*))_dummy
};

/* register new toplevel */
static void _new_toplevel ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zcosmic_toplevel_info_v1 *manager,
	wfthandle *handle)
{
	GldiWaylandWindowActor* wactor = gldi_wayland_wm_new_toplevel (handle);
	
	zcosmic_toplevel_handle_v1_set_user_data (handle, wactor);
	GldiWindowActor *actor = (GldiWindowActor*)wactor;
	// hack required for minimize on click to work -- "pretend" that the window is in the middle of the screen
	actor->windowGeometry.x = cairo_dock_get_screen_width (0) / 2;
	actor->windowGeometry.y = cairo_dock_get_screen_height (0) / 2;
	actor->windowGeometry.width = 0;
	actor->windowGeometry.height = 0;
	
	/* note: we cannot do anything as long as we get app_id */
	zcosmic_toplevel_handle_v1_add_listener (handle, &gldi_toplevel_handle_interface, wactor);
}

/* sent when toplevel management is no longer available -- maybe unload this module? */
static void _toplevel_manager_finished ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zcosmic_toplevel_info_v1 *manager)
{
    cd_message ("Toplevel manager exited, taskbar will not be available!");
}

static struct zcosmic_toplevel_info_v1_listener cosmic_info_interface = {
    .toplevel = _new_toplevel,
    .finished = _toplevel_manager_finished,
};

static void _destroy (gpointer handle)
{
	zcosmic_toplevel_handle_v1_destroy ((wfthandle*)handle);
}

gboolean gldi_cosmic_toplevel_try_init (struct wl_registry *registry)
{
	if (!(manager_found && info_found)) return FALSE;
	
	if (info_version > (uint32_t)zcosmic_toplevel_info_v1_interface.version)
		info_version = zcosmic_toplevel_info_v1_interface.version;
	if (manager_version > (uint32_t)zcosmic_toplevel_manager_v1_interface.version)
		manager_version = zcosmic_toplevel_manager_v1_interface.version;
	
	s_ptoplevel_manager = wl_registry_bind (registry, manager_id, &zcosmic_toplevel_manager_v1_interface, manager_version);
	if (!s_ptoplevel_manager)
	{
		cd_error ("cannot bind cosmic_toplevel_manager!");
		return FALSE;
	}
	
	s_ptoplevel_info = wl_registry_bind (registry, info_id, &zcosmic_toplevel_info_v1_interface, info_version);
	if (!s_ptoplevel_info)
	{
		zcosmic_toplevel_manager_v1_destroy (s_ptoplevel_manager);
		s_ptoplevel_manager = NULL;
		cd_error ("cannot bind cosmic_toplevel_info!");
		return FALSE;
	}
	
	zcosmic_toplevel_manager_v1_add_listener(s_ptoplevel_manager, &cosmic_manager_interface, NULL);
	zcosmic_toplevel_info_v1_add_listener(s_ptoplevel_info, &cosmic_info_interface, NULL);
	
	// register window manager
	GldiWindowManagerBackend wmb;
	memset (&wmb, 0, sizeof (GldiWindowManagerBackend));
	wmb.get_active_window = gldi_wayland_wm_get_active_window;
	wmb.move_to_viewport_abs = _move_to_nth_desktop;
	// wmb.move_to_nth_desktop = _move_to_nth_desktop;
	wmb.show = _show;
	wmb.close = _close;
	// wmb.kill = _kill;
	wmb.minimize = _minimize;
	// wmb.lower = _lower;
	wmb.maximize = _maximize;
	wmb.set_fullscreen = _set_fullscreen;
	// wmb.set_above = _set_above;
	wmb.set_minimize_position = _set_minimize_position;
	wmb.set_thumbnail_area = _set_thumbnail_area;
	// wmb.set_window_border = _set_window_border;
	// wmb.get_icon_surface = _get_icon_surface;
	// wmb.get_thumbnail_surface = _get_thumbnail_surface;
	// wmb.get_texture = _get_texture;
	wmb.get_transient_for = _get_transient_for;
	// wmb.is_above_or_below = _is_above_or_below;
	// wmb.is_sticky = _is_sticky;
	// wmb.set_sticky = _set_sticky;
	wmb.can_minimize_maximize_close = _can_minimize_maximize_close;
	// wmb.get_id = _get_id;
	wmb.pick_window = gldi_wayland_wm_pick_window;
	wmb.flags = GINT_TO_POINTER (GLDI_WM_NO_VIEWPORT_OVERLAP | GLDI_WM_GEOM_REL_TO_VIEWPORT | GLDI_WM_HAVE_WORKSPACES);
	wmb.name = "Cosmic";
	gldi_windows_manager_register_backend (&wmb);
	
	gldi_wayland_wm_init (_destroy);
	
	return TRUE;
}


gboolean gldi_cosmic_toplevel_match_protocol (uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp (interface, zcosmic_toplevel_manager_v1_interface.name))
	{
		manager_found = TRUE;
		manager_id = id;
		manager_version = version;
		return TRUE;
	}
	if (!strcmp (interface, zcosmic_toplevel_info_v1_interface.name))
	{
		info_found = TRUE;
		info_id = id;
		info_version = version;
		return TRUE;
	}
	return FALSE;
}

#endif


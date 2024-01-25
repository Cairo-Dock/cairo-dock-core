/*
 * cairo-dock-foreign-toplevel.c
 * 
 * Interact with Wayland clients via the zwlr_foreign_toplevel_manager
 * protocol. See e.g. https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-foreign-toplevel-management-unstable-v1.xml
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
#include "wayland-wlr-foreign-toplevel-management-client-protocol.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-foreign-toplevel.h"
#include "cairo-dock-wayland-wm.h"

#include <stdio.h>


typedef struct zwlr_foreign_toplevel_handle_v1 wfthandle;


/********************************************************************
 * static variables used                                            */

static struct zwlr_foreign_toplevel_manager_v1* s_ptoplevel_manager = NULL;


/**********************************************************************
 * window manager interface                                           */

static void _show (GldiWindowActor *actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	GdkDisplay *dsp = gdk_display_get_default ();
	GdkSeat *seat = gdk_display_get_default_seat (dsp);
	struct wl_seat* wl_seat = gdk_wayland_seat_get_wl_seat (seat);
	zwlr_foreign_toplevel_handle_v1_activate (wactor->handle, wl_seat);
}
static void _close (GldiWindowActor *actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	zwlr_foreign_toplevel_handle_v1_close (wactor->handle);
}
static void _minimize (GldiWindowActor *actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	zwlr_foreign_toplevel_handle_v1_set_minimized (wactor->handle);
}
static void _maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	if (bMaximize) zwlr_foreign_toplevel_handle_v1_set_maximized (wactor->handle);
	else zwlr_foreign_toplevel_handle_v1_unset_maximized (wactor->handle);
}

static GldiWindowActor* _get_transient_for(GldiWindowActor* actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	wfthandle* parent = wactor->parent;
	if (!parent) return NULL;
	GldiWaylandWindowActor *pactor = zwlr_foreign_toplevel_handle_v1_get_user_data (parent);
	return (GldiWindowActor*)pactor;
}

static void _can_minimize_maximize_close ( G_GNUC_UNUSED GldiWindowActor *actor, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose)
{
	// we don't know, set everything to true
	*bCanMinimize = TRUE;
	*bCanMaximize = TRUE;
	*bCanClose = TRUE;
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
	
	zwlr_foreign_toplevel_handle_v1_set_rectangle(wactor->handle, surface, x, y, w, h);
}

static void _set_minimize_position (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y)
{
	_set_thumbnail_area (actor, pContainerWidget, x, y, 1, 1);
}

static void _set_fullscreen (GldiWindowActor *actor, gboolean bFullScreen)
{
	if (!actor) return;
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	if (bFullScreen) zwlr_foreign_toplevel_handle_v1_set_fullscreen (wactor->handle, NULL);
	else zwlr_foreign_toplevel_handle_v1_unset_fullscreen (wactor->handle);
}




/**********************************************************************
 * callbacks                                                          */

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
		if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
			gldi_wayland_wm_activated (wactor, FALSE);
		else if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
			maximized_pending = TRUE;
		else if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
			minimized_pending = TRUE;
		else if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
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

static void _gldi_toplevel_parent_cb (void* data, G_GNUC_UNUSED wfthandle *handle, wfthandle *parent)
{
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	wactor->parent = parent;
}

/**********************************************************************
 * interface and object manager definitions                           */

static struct zwlr_foreign_toplevel_handle_v1_listener gldi_toplevel_handle_interface = {
    .title        = _gldi_toplevel_title_cb,
    .app_id       = _gldi_toplevel_appid_cb,
    .output_enter = _gldi_toplevel_output_enter_cb,
    .output_leave = _gldi_toplevel_output_leave_cb,
    .state        = _gldi_toplevel_state_cb,
    .done         = _gldi_toplevel_done_cb,
    .closed       = _gldi_toplevel_closed_cb,
    .parent       = _gldi_toplevel_parent_cb
};

/* register new toplevel */
static void _new_toplevel ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwlr_foreign_toplevel_manager_v1 *manager,
	wfthandle *handle)
{
	GldiWaylandWindowActor* wactor = gldi_wayland_wm_new_toplevel (handle);
	
	zwlr_foreign_toplevel_handle_v1_set_user_data (handle, wactor);
	GldiWindowActor *actor = (GldiWindowActor*)wactor;
	// hack required for minimize on click to work -- "pretend" that the window is in the middle of the screen
	actor->windowGeometry.x = cairo_dock_get_screen_width (0) / 2;
	actor->windowGeometry.y = cairo_dock_get_screen_height (0) / 2;
	actor->windowGeometry.width = 1;
	actor->windowGeometry.height = 1;
	
	/* note: we cannot do anything as long as we get app_id */
	zwlr_foreign_toplevel_handle_v1_add_listener (handle, &gldi_toplevel_handle_interface, wactor);
}

/* sent when toplevel management is no longer available -- maybe unload this module? */
static void _toplevel_manager_finished ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwlr_foreign_toplevel_manager_v1 *manager)
{
    cd_message ("Toplevel manager exited, taskbar will not be available!");
}

static struct zwlr_foreign_toplevel_manager_v1_listener gldi_toplevel_manager = {
    .toplevel = _new_toplevel,
    .finished = _toplevel_manager_finished,
};

static void _destroy (gpointer handle)
{
	zwlr_foreign_toplevel_handle_v1_destroy ((wfthandle*)handle);
}

static void gldi_zwlr_foreign_toplevel_manager_init ()
{
	if (!s_ptoplevel_manager) return;
	
	zwlr_foreign_toplevel_manager_v1_add_listener(s_ptoplevel_manager, &gldi_toplevel_manager, NULL);
	// register window manager
	GldiWindowManagerBackend wmb;
	memset (&wmb, 0, sizeof (GldiWindowManagerBackend));
	wmb.get_active_window = gldi_wayland_wm_get_active_window;
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
	wmb.name = "wlr";
	gldi_windows_manager_register_backend (&wmb);
	
	gldi_wayland_wm_init(_destroy);
	
}

static uint32_t protocol_id, protocol_version;
static gboolean protocol_found = FALSE;

gboolean gldi_wlr_foreign_toplevel_match_protocol (uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name))
	{
		protocol_found = TRUE;
		protocol_id = id;
		protocol_version = version;
		return TRUE;
	}
	return FALSE;
}


gboolean gldi_wlr_foreign_toplevel_try_init (struct wl_registry *registry)
{
	if (!protocol_found) return FALSE;
	
	if (protocol_version > zwlr_foreign_toplevel_manager_v1_interface.version)
	{
		protocol_version = zwlr_foreign_toplevel_manager_v1_interface.version;
	}
	
	s_ptoplevel_manager = wl_registry_bind (registry, protocol_id, &zwlr_foreign_toplevel_manager_v1_interface, protocol_version);
	if (s_ptoplevel_manager)
	{
		gldi_zwlr_foreign_toplevel_manager_init ();
		return TRUE;
	}
	
	cd_error ("Could not bind wlr-foreign-toplevel-manager!");
    return FALSE;
}


#endif


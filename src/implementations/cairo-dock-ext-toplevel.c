/*
 * cairo-dock-ext-toplevel.c
 * 
 * Interact with Wayland clients via the ext-toplevel protocols.
 * 
 * Copyright 2020-2025 Daniel Kondor <kondor.dani@gmail.com>
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
#include "wayland-ext-toplevel-list-client-protocol.h"
#include "wayland-ext-toplevel-state-client-protocol.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager-priv.h"
#include "cairo-dock-container-priv.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-visibility.h"
#include "cairo-dock-dock-priv.h"
#include "cairo-dock-log.h"
#include "cairo-dock-cosmic-toplevel.h"
#include "cairo-dock-wayland-wm.h"
#include "cairo-dock-ext-workspaces.h"

#include <stdio.h>

typedef struct ext_foreign_toplevel_handle_v1 ext_handle;
typedef struct ext_foreign_toplevel_state_handle_v1 state_handle;

static GldiObjectManager myExtToplevelObjectMgr;

struct _GldiExtWindowActor {
	GldiWaylandWindowActor wactor;
	state_handle *chandle; // extension object handle (the ext_handle is stored in wactor)
	uint32_t extra_state; // last received state -- used only to keep track of is_above
};
typedef struct _GldiExtWindowActor GldiExtWindowActor;

typedef enum {
	NB_NOTIFICATIONS_COSMIC_WINDOW_MANAGER = NB_NOTIFICATIONS_WAYLAND_WM_MANAGER
} CairoCosmicWMNotifications;

/**********************************************************************
 * static variables used                                              */

static struct ext_foreign_toplevel_list_v1 *s_ptoplevel_list = NULL;
static struct ext_foreign_toplevel_state_manager_v1 *s_ptoplevel_state = NULL;

static gboolean can_close = FALSE;
static gboolean can_activate = FALSE;
static gboolean can_maximize = FALSE;
static gboolean can_minimize = FALSE;
static gboolean can_fullscreen = FALSE;
static gboolean can_move_workspace = FALSE;
static gboolean can_sticky = FALSE;
static gboolean can_above = FALSE;

static gboolean list_found = FALSE;
static gboolean state_found = FALSE;
static uint32_t list_id, state_id, list_version, state_version;


/**********************************************************************
 * window manager interface -- toplevel manager                       */

static void _show (GldiWindowActor *actor)
{
	if (!can_activate) return;
	GldiExtWindowActor *wactor = (GldiExtWindowActor *)actor;
	GdkDisplay *dsp = gdk_display_get_default ();
	GdkSeat *seat = gdk_display_get_default_seat (dsp);
	struct wl_seat* wl_seat = gdk_wayland_seat_get_wl_seat (seat);
	ext_foreign_toplevel_state_handle_v1_activate (wactor->chandle, wl_seat);
}
static void _close (GldiWindowActor *actor)
{
	GldiExtWindowActor *wactor = (GldiExtWindowActor *)actor;
	ext_foreign_toplevel_state_handle_v1_close (wactor->chandle);
}
static void _minimize (GldiWindowActor *actor)
{
	GldiExtWindowActor *wactor = (GldiExtWindowActor *)actor;
	ext_foreign_toplevel_state_handle_v1_set_minimized (wactor->chandle);
}
static void _maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	GldiExtWindowActor *wactor = (GldiExtWindowActor *)actor;
	if (bMaximize) ext_foreign_toplevel_state_handle_v1_set_maximized (wactor->chandle);
	else ext_foreign_toplevel_state_handle_v1_unset_maximized (wactor->chandle);
}

static GldiWindowActor* _get_transient_for( G_GNUC_UNUSED GldiWindowActor* actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	ext_handle* parent = wactor->parent;
	if (!parent) return NULL;
	GldiWaylandWindowActor *pactor = ext_foreign_toplevel_handle_v1_get_user_data (parent);
	return (GldiWindowActor*)pactor;
}

static void _can_minimize_maximize_close ( G_GNUC_UNUSED GldiWindowActor *actor, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose)
{
	*bCanMinimize = can_minimize;
	*bCanMaximize = can_maximize;
	*bCanClose = can_close;
}

static void _set_thumbnail_area (GldiWindowActor *actor, GldiContainer* pContainer, int x, int y, int w, int h)
{
	if ( ! (actor && pContainer) ) return;
	if (w < 0 || h < 0) return;
	GldiExtWindowActor *wactor = (GldiExtWindowActor *)actor;
	GdkWindow* window = gldi_container_get_gdk_window (pContainer);
	if (!window) return;
	struct wl_surface* surface = gdk_wayland_window_get_wl_surface (window);
	if (!surface) return;
	
	ext_foreign_toplevel_state_handle_v1_set_rectangle (wactor->chandle, surface, x, y, w, h);
}

static void _set_fullscreen (GldiWindowActor *actor, gboolean bFullScreen)
{
	if (!actor) return;
	GldiExtWindowActor *wactor = (GldiExtWindowActor *)actor;
	if (bFullScreen) ext_foreign_toplevel_state_handle_v1_set_fullscreen (wactor->chandle, NULL);
	else ext_foreign_toplevel_state_handle_v1_unset_fullscreen (wactor->chandle);
}

static void _set_above (GldiWindowActor *actor, gboolean bAbove)
{
	if (!actor) return;
	GldiExtWindowActor *wactor = (GldiExtWindowActor *)actor;
	if (bAbove) ext_foreign_toplevel_state_handle_v1_set_always_on_top (wactor->chandle);
	else ext_foreign_toplevel_state_handle_v1_unset_always_on_top (wactor->chandle);
}

static void _is_above_or_below (GldiWindowActor *actor, gboolean *bIsAbove, gboolean *bIsBelow)
{
	GldiExtWindowActor *wactor = (GldiExtWindowActor *)actor;
	*bIsAbove = !!(wactor->extra_state & EXT_FOREIGN_TOPLEVEL_STATE_HANDLE_V1_STATE_ALWAYS_ON_TOP);
	*bIsBelow = FALSE; // not supported
}

static void _set_sticky (GldiWindowActor *actor, gboolean bSticky)
{
	GldiExtWindowActor *wactor = (GldiExtWindowActor *)actor;
	if (bSticky) ext_foreign_toplevel_state_handle_v1_set_sticky (wactor->chandle);
	else ext_foreign_toplevel_state_handle_v1_unset_sticky (wactor->chandle);
}


static void _capabilities_cb (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct ext_foreign_toplevel_state_manager_v1 *manager,
	G_GNUC_UNUSED uint32_t notifications, uint32_t actions)
{
	can_close = !!(actions & EXT_FOREIGN_TOPLEVEL_STATE_MANAGER_V1_ACTIONS_CLOSE);
	can_activate = !!(actions & EXT_FOREIGN_TOPLEVEL_STATE_MANAGER_V1_ACTIONS_ACTIVATED);
	can_maximize = !!(actions & EXT_FOREIGN_TOPLEVEL_STATE_MANAGER_V1_ACTIONS_MAXIMIZE);
	can_minimize = !!(actions & EXT_FOREIGN_TOPLEVEL_STATE_MANAGER_V1_ACTIONS_MINIMIZE);
	can_fullscreen = !!(actions & EXT_FOREIGN_TOPLEVEL_STATE_MANAGER_V1_ACTIONS_FULLSCREEN);
	can_move_workspace = FALSE; // not supported yet
	can_sticky = !!(actions & EXT_FOREIGN_TOPLEVEL_STATE_MANAGER_V1_ACTIONS_STICKY);
	can_above = !!(actions & EXT_FOREIGN_TOPLEVEL_STATE_MANAGER_V1_ACTIONS_ALWAYS_ON_TOP);
}

static void _get_supported_actions (gboolean *bCanFullscreen, gboolean *bCanSticky, gboolean *bCanBelow, gboolean *bCanAbove, gboolean *bCanKill)
{
	if (bCanFullscreen) *bCanFullscreen = can_fullscreen;
	if (bCanSticky) *bCanSticky = can_sticky;
	if (bCanBelow) *bCanBelow = FALSE; // not supported
	if (bCanAbove) *bCanAbove = can_above;
	if (bCanKill) *bCanKill = FALSE;
}

/**********************************************************************
 * callbacks -- toplevel info                                         */

static void _gldi_toplevel_title_cb (void *data, G_GNUC_UNUSED ext_handle *handle, const char *title)
{
	gldi_wayland_wm_title_changed (data, title, FALSE);
}

static void _gldi_toplevel_appid_cb (void *data, G_GNUC_UNUSED ext_handle *handle, const char *app_id)
{
	gldi_wayland_wm_appid_changed (data, app_id, FALSE);
}

static void _gldi_toplevel_output_enter_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED state_handle *handle, G_GNUC_UNUSED struct wl_output *output)
{
	/* TODO -- or maybe we don't care about this? */
}
static void _gldi_toplevel_output_leave_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED state_handle *handle, G_GNUC_UNUSED struct wl_output *output)
{
	
}


/** Manage pending activation of windows.
 * TODO: this was copied from cosmic-toplevel, but not sure if it applies
 * here as well:
 * Weirdly, multiple windows can be in the "activated" state, e.g. when moving a window:
 * 1. consider window A and B open, with A being active
 * 2. start moving window A
 *   -> get state event with activated == TRUE for window B
 * 3. end the move
 *   -> get state event with activated == FALSE for window B
 * (no events are received for window A)
 * This sequence would result in assuming that no window is active anymore.
 * To work this around, we maintain a hash table with all windows that are in an
 * "activated" state. We apply the following rules:
 *   -- if the table has > 1 elements, do nothing (do not signal / update the currently active window)
 *   -- if the table has 1 element, signal it as the currently active window
 *   -- if the table has 0 elements, signal that no window is active
 */
static GHashTable *s_hPendingActivate = NULL;

static gboolean _find_dummy (G_GNUC_UNUSED gpointer x, G_GNUC_UNUSED gpointer y, G_GNUC_UNUSED gpointer z) { return TRUE; }

static void _update_active (GldiWaylandWindowActor *wactor, gboolean bActive)
{
	gboolean bChange = FALSE;
	if (bActive) bChange = g_hash_table_add (s_hPendingActivate, wactor);
	else bChange = g_hash_table_remove (s_hPendingActivate, wactor);
	
	if (bChange)
	{
		// we always signal deactivation as this window might have been the active one
		// (if this is not the case, this is a harmless no-op)
		if (!bActive) gldi_wayland_wm_activated (wactor, FALSE, FALSE);
		
		if (g_hash_table_size (s_hPendingActivate) == 1)
		{
			GldiWaylandWindowActor *new_active;
			if (bActive) new_active = wactor; // we know this is the newly activated window
			else new_active = g_hash_table_find (s_hPendingActivate, _find_dummy, NULL);
			gldi_wayland_wm_activated (new_active, TRUE, FALSE);
		}
	}
}

static void _gldi_toplevel_state_cb (void *data, G_GNUC_UNUSED state_handle *handle, uint32_t states)
{
	if (!data) return;
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)data;
	gboolean activated_pending = !!(states & EXT_FOREIGN_TOPLEVEL_STATE_HANDLE_V1_STATE_ACTIVATED);
	gboolean maximized_pending = !!(states & EXT_FOREIGN_TOPLEVEL_STATE_HANDLE_V1_STATE_MAXIMIZED);
	gboolean minimized_pending = !!(states & EXT_FOREIGN_TOPLEVEL_STATE_HANDLE_V1_STATE_MINIMIZED);
	gboolean fullscreen_pending = !!(states & EXT_FOREIGN_TOPLEVEL_STATE_HANDLE_V1_STATE_FULLSCREEN);
	gboolean sticky_pending = !!(states & EXT_FOREIGN_TOPLEVEL_STATE_HANDLE_V1_STATE_STICKY);
	
	cd_debug ("wactor: %p (%s), activated: %d", wactor, wactor->actor.cName ? wactor->actor.cName : "(no name)", activated_pending);
	
	_update_active (wactor, activated_pending);
	gldi_wayland_wm_maximized_changed (wactor, maximized_pending, FALSE);
	gldi_wayland_wm_minimized_changed (wactor, minimized_pending, FALSE);
	gldi_wayland_wm_fullscreen_changed (wactor, fullscreen_pending, FALSE);
	gldi_wayland_wm_sticky_changed (wactor, sticky_pending, FALSE);
	
	GldiExtWindowActor *tl_actor = (GldiExtWindowActor*)data;
	tl_actor->extra_state = states;
}

static void _gldi_toplevel_done_cb (void *data, G_GNUC_UNUSED ext_handle *handle)
{
	gldi_wayland_wm_done (data);
}

static void _gldi_toplevel_closed_cb (void *data, G_GNUC_UNUSED ext_handle *handle)
{
	_update_active ((GldiWaylandWindowActor*)data, FALSE);
	gldi_wayland_wm_closed (data, TRUE);
}

static void _gldi_toplevel_parent_cb (void* data, G_GNUC_UNUSED state_handle *handle, ext_handle *parent)
{
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	wactor->parent = parent;
}

static void _dummy_str (G_GNUC_UNUSED void* data, G_GNUC_UNUSED ext_handle* handle, G_GNUC_UNUSED const char* dummy)
{
	/* don't care */
}

/**********************************************************************
 * interface and object manager definitions                           */

static struct ext_foreign_toplevel_state_handle_v1_listener gldi_toplevel_state_handle_interface = {
    .state        = _gldi_toplevel_state_cb,
    .output_enter = _gldi_toplevel_output_enter_cb,
    .output_leave = _gldi_toplevel_output_leave_cb,
    .parent       = _gldi_toplevel_parent_cb,
};

static struct ext_foreign_toplevel_handle_v1_listener gldi_toplevel_handle_interface = {
    .closed       = _gldi_toplevel_closed_cb,
    .done         = _gldi_toplevel_done_cb,
    .title        = _gldi_toplevel_title_cb,
    .app_id       = _gldi_toplevel_appid_cb,
    .identifier   = _dummy_str
};

/* register new toplevel -- now with ext-foreign-toplevel interface */
static void _new_toplevel ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct ext_foreign_toplevel_list_v1 *manager,
	ext_handle *handle)
{
	GldiExtWindowActor* cactor = (GldiExtWindowActor*)gldi_object_new (&myExtToplevelObjectMgr, handle);
	cactor->chandle = ext_foreign_toplevel_state_manager_v1_get_state_handle (s_ptoplevel_state, handle);
	
	ext_foreign_toplevel_handle_v1_set_user_data (handle, cactor);
	ext_foreign_toplevel_state_handle_v1_set_user_data (cactor->chandle, cactor);
	GldiWindowActor *actor = (GldiWindowActor*)cactor;
	// set initial position to something sensible (does not really matter)
	actor->windowGeometry.x = cairo_dock_get_screen_width (0) / 2;
	actor->windowGeometry.y = cairo_dock_get_screen_height (0) / 2;
	actor->windowGeometry.width = 1;
	actor->windowGeometry.height = 1;
	
	/* note: we cannot do anything as long as we get app_id */
	ext_foreign_toplevel_state_handle_v1_add_listener (cactor->chandle, &gldi_toplevel_state_handle_interface, cactor);
	ext_foreign_toplevel_handle_v1_add_listener (handle, &gldi_toplevel_handle_interface, cactor);
}

/* sent when toplevel management is no longer available -- maybe unload this module? */
static void _toplevel_manager_finished ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct ext_foreign_toplevel_list_v1 *manager)
{
    cd_warning ("Toplevel list exited, taskbar will not be available!");
}


static struct ext_foreign_toplevel_state_manager_v1_listener ext_toplevel_state_interface = {
    .capabilities = _capabilities_cb,
};

static struct ext_foreign_toplevel_list_v1_listener ext_toplevel_list_interface = {
	.toplevel = _new_toplevel,
	.finished = _toplevel_manager_finished
};


static void _reset_object (GldiObject* obj)
{
	GldiExtWindowActor* pactor = (GldiExtWindowActor*)obj;
	if (pactor)
	{
		ext_foreign_toplevel_state_handle_v1_destroy (pactor->chandle);
		ext_foreign_toplevel_handle_v1_destroy ((ext_handle*)pactor->wactor.handle);
	}
}


gboolean gldi_ext_toplevel_try_init (struct wl_registry *registry)
{
	if (!(state_found && list_found)) return FALSE;
	
	if (list_version > (uint32_t)ext_foreign_toplevel_list_v1_interface.version)
		list_version = ext_foreign_toplevel_list_v1_interface.version;
	if (state_version > (uint32_t)ext_foreign_toplevel_state_manager_v1_interface.version)
		state_version = ext_foreign_toplevel_state_manager_v1_interface.version;
		
	s_ptoplevel_list = wl_registry_bind (registry, list_id, &ext_foreign_toplevel_list_v1_interface, list_version);
	if (!s_ptoplevel_list)
	{
		cd_error ("cannot bind ext_foreign_toplevel_list!");
		return FALSE;
	}
	
	s_ptoplevel_state = wl_registry_bind (registry, state_id, &ext_foreign_toplevel_state_manager_v1_interface, state_version);
	if (!s_ptoplevel_state)
	{
		cd_error ("cannot bind ext_toplevel_state_manager!");
		ext_foreign_toplevel_list_v1_destroy (s_ptoplevel_list);
		s_ptoplevel_list = NULL;
		return FALSE;
	}
	
	// register window manager
	GldiWindowManagerBackend wmb;
	memset (&wmb, 0, sizeof (GldiWindowManagerBackend));
	wmb.get_active_window = gldi_wayland_wm_get_active_window;
	// wmb.move_to_viewport_abs = _move_to_nth_desktop;
	// wmb.move_to_nth_desktop = _move_to_nth_desktop;
	wmb.show = _show;
	wmb.close = _close;
	// wmb.kill = _kill;
	wmb.minimize = _minimize;
	// wmb.lower = _lower;
	wmb.maximize = _maximize;
	wmb.set_fullscreen = _set_fullscreen;
	wmb.set_above = _set_above;
	wmb.set_thumbnail_area = _set_thumbnail_area;
	// wmb.set_window_border = _set_window_border;
	// wmb.get_icon_surface = _get_icon_surface;
	// wmb.get_thumbnail_surface = _get_thumbnail_surface;
	// wmb.get_texture = _get_texture;
	wmb.get_transient_for = _get_transient_for;
	wmb.is_above_or_below = _is_above_or_below;
	wmb.set_sticky = _set_sticky;
	wmb.can_minimize_maximize_close = _can_minimize_maximize_close;
	// wmb.get_id = _get_id;
	wmb.pick_window = gldi_wayland_wm_pick_window;
	wmb.get_supported_actions = _get_supported_actions;
	int flags = GLDI_WM_NO_VIEWPORT_OVERLAP | GLDI_WM_GEOM_REL_TO_VIEWPORT;
	wmb.flags = GINT_TO_POINTER (flags);
	wmb.name = "ext-toplevel";
	gldi_windows_manager_register_backend (&wmb);
	
	gldi_wayland_wm_init (NULL);
	s_hPendingActivate = g_hash_table_new (NULL, NULL);
	
	// extend the generic Wayland toplevel object manager
	memset (&myExtToplevelObjectMgr, 0, sizeof (GldiObjectManager));
	myExtToplevelObjectMgr.cName        = "ext-toplevel-state";
	myExtToplevelObjectMgr.iObjectSize  = sizeof (GldiExtWindowActor);
	// interface
	myExtToplevelObjectMgr.init_object  = NULL;
	myExtToplevelObjectMgr.reset_object = _reset_object;
	// signals
	gldi_object_install_notifications (&myExtToplevelObjectMgr, NB_NOTIFICATIONS_COSMIC_WINDOW_MANAGER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myExtToplevelObjectMgr), &myWaylandWMObjectMgr);
	
	// add our listeners (now that we registered our object, so we can handle events)
	ext_foreign_toplevel_list_v1_add_listener (s_ptoplevel_list, &ext_toplevel_list_interface, NULL);
	ext_foreign_toplevel_state_manager_v1_add_listener (s_ptoplevel_state, &ext_toplevel_state_interface, NULL);

	return TRUE;
}


gboolean gldi_ext_toplevel_match_protocol (uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp (interface, ext_foreign_toplevel_state_manager_v1_interface.name))
	{
		state_found = TRUE;
		state_id = id;
		state_version = version;
		return TRUE;
	}
	if (!strcmp (interface, ext_foreign_toplevel_list_v1_interface.name))
	{
		list_found = TRUE;
		list_id = id;
		list_version = version;
		return TRUE;
	}
	return FALSE;
}

#endif


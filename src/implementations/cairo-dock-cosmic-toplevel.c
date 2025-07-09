/*
 * cairo-dock-cosmic-toplevel.c
 * 
 * Interact with Wayland clients via the cosmic_toplevel_info_unstable_v1
 * and cosmic_toplevel_management_unstable_v1 protocol.
 * See for more info: https://github.com/pop-os/cosmic-protocols
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
#include "wayland-cosmic-toplevel-management-client-protocol.h"
#include "wayland-cosmic-toplevel-info-client-protocol.h"
#include "wayland-cosmic-workspace-client-protocol.h"
#include "wayland-ext-toplevel-list-client-protocol.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-log.h"
#include "cairo-dock-cosmic-toplevel.h"
#include "cairo-dock-wayland-wm.h"
#include "cairo-dock-ext-workspaces.h"

#include <stdio.h>


typedef struct ext_foreign_toplevel_handle_v1 ext_handle;
typedef struct zcosmic_toplevel_handle_v1 cosmic_handle;

static GldiObjectManager myCosmicWindowObjectMgr;

struct _GldiCosmicWindowActor {
	GldiWaylandWindowActor wactor;
	cosmic_handle *chandle; // extension object handle (the ext_handle is stored in wactor)
};
typedef struct _GldiCosmicWindowActor GldiCosmicWindowActor;

typedef enum {
	NB_NOTIFICATIONS_COSMIC_WINDOW_MANAGER = NB_NOTIFICATIONS_WINDOWS
} CairoCosmicWMNotifications;

/**********************************************************************
 * static variables used                                              */

static struct ext_foreign_toplevel_list_v1 *s_ptoplevel_list = NULL;
static struct zcosmic_toplevel_info_v1* s_ptoplevel_info = NULL;
static struct zcosmic_toplevel_manager_v1* s_ptoplevel_manager = NULL;

static gboolean can_close = FALSE;
static gboolean can_activate = FALSE;
static gboolean can_maximize = FALSE;
static gboolean can_minimize = FALSE;
static gboolean can_fullscreen = FALSE;
static gboolean can_move_workspace = FALSE;

static gboolean list_found = FALSE;
static gboolean manager_found = FALSE;
static gboolean info_found = FALSE;
static uint32_t list_id, manager_id, info_id, list_version, manager_version, info_version;

/**********************************************************************
 * window manager interface -- toplevel manager                       */

static void _move_to_nth_desktop (GldiWindowActor *actor, G_GNUC_UNUSED int iNumDesktop,
	int x, int y)
{
	if (!(s_ws_output && can_move_workspace)) return;
	if (manager_version < 4) return; // ext_workspace support was added at version 4 only
	GldiCosmicWindowActor *wactor = (GldiCosmicWindowActor *)actor;
	struct ext_workspace_handle_v1 *ws = gldi_ext_workspaces_get_handle (x, y);
	//!! TODO: we need a valid wl_output here !!
	if (ws) zcosmic_toplevel_manager_v1_move_to_ext_workspace (s_ptoplevel_manager, wactor->chandle, ws, s_ws_output);
}

static void _show (GldiWindowActor *actor)
{
	if (!can_activate) return;
	GldiCosmicWindowActor *wactor = (GldiCosmicWindowActor *)actor;
	GdkDisplay *dsp = gdk_display_get_default ();
	GdkSeat *seat = gdk_display_get_default_seat (dsp);
	struct wl_seat* wl_seat = gdk_wayland_seat_get_wl_seat (seat);
	zcosmic_toplevel_manager_v1_activate (s_ptoplevel_manager, wactor->chandle, wl_seat);
}
static void _close (GldiWindowActor *actor)
{
	GldiCosmicWindowActor *wactor = (GldiCosmicWindowActor *)actor;
	zcosmic_toplevel_manager_v1_close (s_ptoplevel_manager, wactor->chandle);
}
static void _minimize (GldiWindowActor *actor)
{
	GldiCosmicWindowActor *wactor = (GldiCosmicWindowActor *)actor;
	zcosmic_toplevel_manager_v1_set_minimized (s_ptoplevel_manager, wactor->chandle);
}
static void _maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	GldiCosmicWindowActor *wactor = (GldiCosmicWindowActor *)actor;
	if (bMaximize) zcosmic_toplevel_manager_v1_set_maximized (s_ptoplevel_manager, wactor->chandle);
	else zcosmic_toplevel_manager_v1_unset_maximized (s_ptoplevel_manager, wactor->chandle);
}

static GldiWindowActor* _get_transient_for( G_GNUC_UNUSED GldiWindowActor* actor)
{
	return NULL; /* -- no parent event
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	wfthandle* parent = wactor->parent;
	if (!parent) return NULL;
	GldiWaylandWindowActor *pactor = zcosmic_toplevel_handle_v1_get_user_data (parent);
	return (GldiWindowActor*)pactor; */
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
static void _set_thumbnail_area (GldiWindowActor *actor, GldiContainer* pContainer, int x, int y, int w, int h)
{
	if ( ! (actor && pContainer) ) return;
	if (w < 0 || h < 0) return;
	GldiCosmicWindowActor *wactor = (GldiCosmicWindowActor *)actor;
	GdkWindow* window = gldi_container_get_gdk_window (pContainer);
	if (!window) return;
	struct wl_surface* surface = gdk_wayland_window_get_wl_surface (window);
	if (!surface) return;
	
	zcosmic_toplevel_manager_v1_set_rectangle(s_ptoplevel_manager, wactor->chandle, surface, x, y, w, h);
}

static void _set_minimize_position (GldiWindowActor *actor, GldiContainer* pContainer, int x, int y)
{
	_set_thumbnail_area (actor, pContainer, x, y, 1, 1);
}

static void _set_fullscreen (GldiWindowActor *actor, gboolean bFullScreen)
{
	if (!actor) return;
	GldiCosmicWindowActor *wactor = (GldiCosmicWindowActor *)actor;
	if (bFullScreen) zcosmic_toplevel_manager_v1_set_fullscreen (s_ptoplevel_manager, wactor->chandle, NULL);
	else zcosmic_toplevel_manager_v1_unset_fullscreen (s_ptoplevel_manager, wactor->chandle);
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

static void _gldi_toplevel_title_cb (void *data, G_GNUC_UNUSED ext_handle *handle, const char *title)
{
	gldi_wayland_wm_title_changed (data, title, FALSE);
}

static void _gldi_toplevel_appid_cb (void *data, G_GNUC_UNUSED ext_handle *handle, const char *app_id)
{
	gldi_wayland_wm_appid_changed (data, app_id, FALSE);
}

static void _gldi_toplevel_output_enter_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED cosmic_handle *handle, G_GNUC_UNUSED struct wl_output *output)
{
	/* TODO -- or maybe we don't care about this? */
}
static void _gldi_toplevel_output_leave_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED cosmic_handle *handle, G_GNUC_UNUSED struct wl_output *output)
{
	
}

static void _gldi_toplevel_state_cb (void *data, G_GNUC_UNUSED cosmic_handle *handle, struct wl_array *state)
{
	if (!data) return;
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	gboolean activated_pending = FALSE;
	gboolean maximized_pending = FALSE;
	gboolean minimized_pending = FALSE;
	gboolean fullscreen_pending = FALSE;
	int i;
	uint32_t* stdata = (uint32_t*)state->data;
	for (i = 0; i*sizeof(uint32_t) < state->size; i++)
	{
		if (stdata[i] == ZCOSMIC_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
		{
			activated_pending = TRUE;
			gldi_wayland_wm_stack_on_top ((GldiWindowActor*)wactor);
		}
		else if (stdata[i] == ZCOSMIC_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
			maximized_pending = TRUE;
		else if (stdata[i] == ZCOSMIC_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
			minimized_pending = TRUE;
		else if (stdata[i] == ZCOSMIC_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
			fullscreen_pending = TRUE;
		//!! TODO: sticky !!
	}
	
	gldi_wayland_wm_activated (wactor, activated_pending, FALSE);
	gldi_wayland_wm_maximized_changed (wactor, maximized_pending, FALSE);
	gldi_wayland_wm_minimized_changed (wactor, minimized_pending, FALSE);
	gldi_wayland_wm_fullscreen_changed (wactor, fullscreen_pending, FALSE);
}

static void _gldi_toplevel_done_cb (void *data, G_GNUC_UNUSED ext_handle *handle)
{
	gldi_wayland_wm_done (data);
}

static void _gldi_toplevel_closed_cb (void *data, G_GNUC_UNUSED ext_handle *handle)
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


static void _ext_workspace_entered (void *data, G_GNUC_UNUSED cosmic_handle *handle, struct ext_workspace_handle_v1 *wshandle)
{
	gldi_ext_workspaces_update_window ((GldiWindowActor*)data, wshandle);
	gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, data);
}

static void _ext_workspace_left (void*, cosmic_handle*, struct ext_workspace_handle_v1*)
{
	
}

static void _geometry_cb (void* data, G_GNUC_UNUSED cosmic_handle* handle,
	G_GNUC_UNUSED struct wl_output *output, int x, int y, int w, int h)
{
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	//!! keep track of the output !!
	actor->windowGeometry.width = w;
	actor->windowGeometry.height = h;
	actor->windowGeometry.x = x;
	actor->windowGeometry.y = y;
	if (wactor->init_done && actor->bDisplayed)
		gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_SIZE_POSITION_CHANGED, actor);
}

static void _ws_dummy (void*, cosmic_handle*, struct zcosmic_workspace_handle_v1*)
{
	
}

static void _dummy_str (void*, cosmic_handle*, const char*)
{
	/* don't care */
}

static void _dummy_void (void*, cosmic_handle*)
{
	/* don't care */
}


/**********************************************************************
 * interface and object manager definitions                           */

static struct zcosmic_toplevel_manager_v1_listener cosmic_manager_interface = {
	.capabilities = _capabilities_cb
};

static struct zcosmic_toplevel_handle_v1_listener gldi_cosmic_handle_interface = {
    .closed       = _dummy_void,
    .done         = _dummy_void,
    .title        = _dummy_str,
    .app_id       = _dummy_str,
    .output_enter = _gldi_toplevel_output_enter_cb,
    .output_leave = _gldi_toplevel_output_leave_cb,
    .workspace_enter = _ws_dummy,
    .workspace_leave = _ws_dummy,
    .state        = _gldi_toplevel_state_cb,
    .geometry     = _geometry_cb,
    .ext_workspace_enter = _ext_workspace_entered,
    .ext_workspace_leave = _ext_workspace_left
//    .parent       = _gldi_toplevel_parent_cb,
};

static struct ext_foreign_toplevel_handle_v1_listener gldi_toplevel_handle_interface = {
    .closed       = _gldi_toplevel_closed_cb,
    .done         = _gldi_toplevel_done_cb,
    .title        = _gldi_toplevel_title_cb,
    .app_id       = _gldi_toplevel_appid_cb,
    .identifier   = (void(*)(void*, ext_handle*, const char*)) _dummy_str
};

/* register new toplevel -- now with ext-foreign-toplevel interface */
static void _new_toplevel ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct ext_foreign_toplevel_list_v1 *manager,
	ext_handle *handle)
{
	GldiCosmicWindowActor* cactor = (GldiCosmicWindowActor*)gldi_object_new (&myCosmicWindowObjectMgr, handle);
	cactor->chandle = zcosmic_toplevel_info_v1_get_cosmic_toplevel (s_ptoplevel_info, handle);
	
	ext_foreign_toplevel_handle_v1_set_user_data (handle, cactor);
	zcosmic_toplevel_handle_v1_set_user_data (cactor->chandle, cactor);
	GldiWindowActor *actor = (GldiWindowActor*)cactor;
	// hack required for minimize on click to work -- "pretend" that the window is in the middle of the screen
	actor->windowGeometry.x = cairo_dock_get_screen_width (0) / 2;
	actor->windowGeometry.y = cairo_dock_get_screen_height (0) / 2;
	actor->windowGeometry.width = 0;
	actor->windowGeometry.height = 0;
	
	/* note: we cannot do anything as long as we get app_id */
	zcosmic_toplevel_handle_v1_add_listener (cactor->chandle, &gldi_cosmic_handle_interface, cactor);
	ext_foreign_toplevel_handle_v1_add_listener (handle, &gldi_toplevel_handle_interface, cactor);
}

/* sent when toplevel management is no longer available -- maybe unload this module? */
static void _toplevel_manager_finished ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct ext_foreign_toplevel_list_v1 *manager)
{
    cd_warning ("Toplevel list exited, taskbar will not be available!");
}


static void _new_toplevel_dummy (void*, struct zcosmic_toplevel_info_v1*, cosmic_handle*)
{
	/* no-op, with version >= 2 we should not receive this event anymore;
	 * however, we still provide a handler just in case, to avoid crashing */
}

static void _toplevel_manager_finished_dummy (void*, struct zcosmic_toplevel_info_v1*)
{
	/* no-op, with version >= 2, this does not matter */
}

static struct zcosmic_toplevel_info_v1_listener cosmic_info_interface = {
    .toplevel = _new_toplevel_dummy,
    .finished = _toplevel_manager_finished_dummy,
    .done = _toplevel_manager_finished_dummy
};

static struct ext_foreign_toplevel_list_v1_listener ext_toplevel_list_interface = {
	.toplevel = _new_toplevel,
	.finished = _toplevel_manager_finished
};


static void _reset_object (GldiObject* obj)
{
	GldiCosmicWindowActor* pactor = (GldiCosmicWindowActor*)obj;
	if (pactor)
	{
		zcosmic_toplevel_handle_v1_destroy (pactor->chandle);
		ext_foreign_toplevel_handle_v1_destroy ((ext_handle*)pactor->wactor.handle);
	}
}

gboolean gldi_cosmic_toplevel_try_init (struct wl_registry *registry)
{
	if (!(manager_found && info_found && list_found)) return FALSE;
	
	if (list_version > (uint32_t)ext_foreign_toplevel_list_v1_interface.version)
		list_version = ext_foreign_toplevel_list_v1_interface.version;
	if (info_version > (uint32_t)zcosmic_toplevel_info_v1_interface.version)
		info_version = zcosmic_toplevel_info_v1_interface.version;
	if (manager_version > (uint32_t)zcosmic_toplevel_manager_v1_interface.version)
		manager_version = zcosmic_toplevel_manager_v1_interface.version;
	
	if (info_version < 2)
	{
		cd_warning ("too old version of cosmic_toplevel_info, will not use it");
		return FALSE;
	}
	
	s_ptoplevel_list = wl_registry_bind (registry, list_id, &ext_foreign_toplevel_list_v1_interface, list_version);
	if (!s_ptoplevel_list)
	{
		cd_error ("cannot bind ext_foreign_toplevel_info!");
		return FALSE;
	}
	
	s_ptoplevel_manager = wl_registry_bind (registry, manager_id, &zcosmic_toplevel_manager_v1_interface, manager_version);
	if (!s_ptoplevel_manager)
	{
		cd_error ("cannot bind cosmic_toplevel_manager!");
		ext_foreign_toplevel_list_v1_destroy (s_ptoplevel_list);
		s_ptoplevel_list = NULL;
		return FALSE;
	}
	
	s_ptoplevel_info = wl_registry_bind (registry, info_id, &zcosmic_toplevel_info_v1_interface, info_version);
	if (!s_ptoplevel_info)
	{
		zcosmic_toplevel_manager_v1_destroy (s_ptoplevel_manager);
		ext_foreign_toplevel_list_v1_destroy (s_ptoplevel_list);
		s_ptoplevel_list = NULL;
		s_ptoplevel_manager = NULL;
		cd_error ("cannot bind cosmic_toplevel_info!");
		return FALSE;
	}
	
	ext_foreign_toplevel_list_v1_add_listener (s_ptoplevel_list, &ext_toplevel_list_interface, NULL);
	zcosmic_toplevel_manager_v1_add_listener (s_ptoplevel_manager, &cosmic_manager_interface, NULL);
	zcosmic_toplevel_info_v1_add_listener (s_ptoplevel_info, &cosmic_info_interface, NULL);
	
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
	int flags = GLDI_WM_NO_VIEWPORT_OVERLAP | GLDI_WM_GEOM_REL_TO_VIEWPORT;
	if (info_version >= 2) flags |= GLDI_WM_HAVE_WINDOW_GEOMETRY;
	if (info_version >= 3) flags |= GLDI_WM_HAVE_WORKSPACES; // we only use ext-workspaces now, which are only available with version >= 3
	wmb.flags = GINT_TO_POINTER (flags);
	wmb.name = "Cosmic";
	gldi_windows_manager_register_backend (&wmb);
	
	gldi_wayland_wm_init (NULL);
	
	// extend the generic Wayland toplevel object manager
	memset (&myCosmicWindowObjectMgr, 0, sizeof (GldiObjectManager));
	myCosmicWindowObjectMgr.cName        = "cosmic-window-manager";
	myCosmicWindowObjectMgr.iObjectSize  = sizeof (GldiCosmicWindowActor);
	// interface
	myCosmicWindowObjectMgr.init_object  = NULL;
	myCosmicWindowObjectMgr.reset_object = _reset_object;
	// signals
	gldi_object_install_notifications (&myCosmicWindowObjectMgr, NB_NOTIFICATIONS_COSMIC_WINDOW_MANAGER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myCosmicWindowObjectMgr), &myWaylandWMObjectMgr);
	
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


/*
 * cairo-dock-plasma-window-management.c
 * 
 * Interact with Wayland clients via the plasma-window-management
 * protocol. See e.g. https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/plasma-window-management.xml
 * 
 * Copyright 2021-2024 Daniel Kondor <kondor.dani@gmail.com>
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

/** TODO
 * - Support for other states (sticky, set above / below)
 * - Support for using the supplied icon (icon_changed and themed_icon_name_changed events)
 * - Support for killing a process (via the supplied pid)
 * - Keeping track whether it is possible to minimize / maximize / fullscreen a window
 * 		(part of state, should be reported by the _can_minimize_maximize_close() function
 * 		TODO: what to do if a change in this is reported via the state_changed event?
 * 		Is there a need to send a notification about this?)
 * - Use the reported location of windows, working together with cairo-dock-dock-visibility functionality
 * - Keeping track which virtual desktop / workspace each window is on, support sending to other one
 * 		(this probably needs support for the plasma-virtual-desktop protocol)
 */

#include <gdk/gdkwayland.h>
#include "wayland-plasma-window-management-client-protocol.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-plasma-window-manager.h"
#include "cairo-dock-wayland-wm.h"
#include "cairo-dock-plasma-virtual-desktop.h"

#include <stdio.h>


typedef struct org_kde_plasma_window pwhandle;


// window manager interface

static void _move_to_nth_desktop (GldiWindowActor *actor, int iNumDesktop,
	G_GNUC_UNUSED int iDeltaViewportX, G_GNUC_UNUSED int iDeltaViewportY)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	const char *old_desktop_id = gldi_plasma_virtual_desktop_get_id (actor->iNumDesktop);
	const char *desktop_id = gldi_plasma_virtual_desktop_get_id (iNumDesktop);
	if (desktop_id) org_kde_plasma_window_request_enter_virtual_desktop (wactor->handle, desktop_id);
	if (old_desktop_id) org_kde_plasma_window_request_leave_virtual_desktop (wactor->handle, old_desktop_id);
}

static void _show (GldiWindowActor *actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	org_kde_plasma_window_set_state (wactor->handle,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
}
static void _close (GldiWindowActor *actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	org_kde_plasma_window_close (wactor->handle);
}
static void _minimize (GldiWindowActor *actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	org_kde_plasma_window_set_state (wactor->handle,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
}
static void _maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	org_kde_plasma_window_set_state (wactor->handle,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED,
		bMaximize ? ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED : 0);
}
static void _fullscreen (GldiWindowActor *actor, gboolean bFullScreen)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	org_kde_plasma_window_set_state (wactor->handle,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN,
		bFullScreen ? ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN : 0);
}

static GldiWindowActor* _get_transient_for(GldiWindowActor* actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor *)actor;
	pwhandle* parent = wactor->parent;
	if (!parent) return NULL;
	GldiWaylandWindowActor *pactor = org_kde_plasma_window_get_user_data (parent);
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
	
	org_kde_plasma_window_set_minimized_geometry(wactor->handle, surface, x, y, w, h);
}

static void _set_minimize_position (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y)
{
	_set_thumbnail_area (actor, pContainerWidget, x, y, 1, 1);
}


/**  callbacks  **/
static void _gldi_toplevel_title_cb (void *data, G_GNUC_UNUSED pwhandle *handle, const char *title)
{
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	gldi_wayland_wm_title_changed (wactor, title, wactor->init_done);
}

static void _gldi_toplevel_appid_cb (void *data, G_GNUC_UNUSED pwhandle *handle, const char *app_id)
{
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	gldi_wayland_wm_appid_changed (wactor, app_id, wactor->init_done);
}

static void _gldi_toplevel_state_cb (void *data, G_GNUC_UNUSED pwhandle *handle, uint32_t flags)
{
	if (!data) return;
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	
	gldi_wayland_wm_maximized_changed (wactor, !!(flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED), FALSE);
	gldi_wayland_wm_minimized_changed (wactor, !!(flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED), FALSE);
	gldi_wayland_wm_fullscreen_changed (wactor, !!(flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN), FALSE);
	gldi_wayland_wm_attention_changed (wactor, !!(flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION), FALSE);
	gldi_wayland_wm_skip_changed (wactor, !!(flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR), FALSE);
	if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE) gldi_wayland_wm_activated (wactor, FALSE);
	if (wactor->init_done) gldi_wayland_wm_done (wactor);
}
	

/* this is sent after the initial state has been sent, i.e. only once */
static void _gldi_toplevel_done_cb ( void *data, G_GNUC_UNUSED pwhandle *handle)
{
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	wactor->init_done = TRUE;
	gldi_wayland_wm_done (wactor);
}

static void _gldi_toplevel_closed_cb (void *data, G_GNUC_UNUSED pwhandle *handle)
{
	gldi_wayland_wm_closed (data, TRUE);
}

static void _gldi_toplevel_parent_cb (void* data, G_GNUC_UNUSED pwhandle *handle, pwhandle *parent)
{
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	wactor->parent = parent;
	if (wactor->init_done) gldi_wayland_wm_done (wactor);
}

static void _gldi_toplevel_geometry_cb (void* data, G_GNUC_UNUSED pwhandle *handle, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
	//!! TODO !!
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	actor->windowGeometry.width = w;
	actor->windowGeometry.height = h;
	actor->windowGeometry.x = x;
	actor->windowGeometry.y = y;
	if (wactor->init_done && actor->bDisplayed)
		gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_SIZE_POSITION_CHANGED, actor);
}

static void _gldi_toplevel_virtual_desktop_changed (G_GNUC_UNUSED void* data, G_GNUC_UNUSED pwhandle *handle, G_GNUC_UNUSED int32_t x)
{
	/* don't care */
}

static void _virtual_desktop_entered (void *data, G_GNUC_UNUSED pwhandle *handle, const char *desktop_id)
{
	GldiWindowActor* actor = (GldiWindowActor*)data;
	int i = gldi_plasma_virtual_desktop_get_index (desktop_id);
	if (i >= 0)
	{
		actor->iNumDesktop = i;
		gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, actor);
	}
}

/* dummy callback shared between all that take a const char* parameter */
static void _gldi_toplevel_dummy_cb (G_GNUC_UNUSED void* data, G_GNUC_UNUSED pwhandle *handle, G_GNUC_UNUSED const char *name)
{
	/* don't care */
}

static void _gldi_toplevel_icon_changed_cb (G_GNUC_UNUSED void* data, G_GNUC_UNUSED pwhandle *handle)
{
	/* don't care */
}

static void _gldi_toplevel_pid_changed_cb (G_GNUC_UNUSED void* data, G_GNUC_UNUSED pwhandle *handle, G_GNUC_UNUSED uint32_t pid)
{
	/* don't care */
}

static void _gldi_toplevel_application_menu_cb (G_GNUC_UNUSED void* data, G_GNUC_UNUSED pwhandle *handle,
	G_GNUC_UNUSED const char *service_name, G_GNUC_UNUSED const char *object_path)
{
	/* don't care */
}

static struct org_kde_plasma_window_listener gldi_toplevel_handle_interface = {
    .title_changed        = _gldi_toplevel_title_cb,
    .app_id_changed       = _gldi_toplevel_appid_cb,
    .state_changed = _gldi_toplevel_state_cb,
    .virtual_desktop_changed = _gldi_toplevel_virtual_desktop_changed,
    .themed_icon_name_changed = _gldi_toplevel_dummy_cb,
    .unmapped = _gldi_toplevel_closed_cb,
    .initial_state = _gldi_toplevel_done_cb,
    .parent_window = _gldi_toplevel_parent_cb,
    .geometry = _gldi_toplevel_geometry_cb,
    .icon_changed = _gldi_toplevel_icon_changed_cb,
    .pid_changed = _gldi_toplevel_pid_changed_cb,
    .virtual_desktop_entered = _virtual_desktop_entered,
    .virtual_desktop_left = _gldi_toplevel_dummy_cb,
    .application_menu = _gldi_toplevel_application_menu_cb,
    .activity_entered = _gldi_toplevel_dummy_cb,
    .activity_left = _gldi_toplevel_dummy_cb
};


static void _destroy (gpointer obj)
{
	org_kde_plasma_window_destroy((pwhandle*)obj);
}

/* register new toplevel */
static void _new_toplevel ( G_GNUC_UNUSED void *data, struct org_kde_plasma_window_management *manager,
	G_GNUC_UNUSED uint32_t id, const char *uuid)
{
	// fprintf(stderr,"New toplevel: %p\n", handle);
	pwhandle* handle = org_kde_plasma_window_management_get_window_by_uuid (manager, uuid);
	
	GldiWaylandWindowActor* wactor = gldi_wayland_wm_new_toplevel (handle);
	org_kde_plasma_window_set_user_data (handle, wactor);
	GldiWindowActor *actor = (GldiWindowActor*)wactor;
	// set initial position to something sensible in case we don't get it updated
	actor->windowGeometry.x = cairo_dock_get_screen_width (0) / 2;
	actor->windowGeometry.y = cairo_dock_get_screen_height (0) / 2;
	actor->windowGeometry.width = 1;
	actor->windowGeometry.height = 1;
	
	/* note: we cannot do anything as long as we get app_id */
	org_kde_plasma_window_add_listener (handle, &gldi_toplevel_handle_interface, wactor);
}

static void _window_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct org_kde_plasma_window_management *manager,
	G_GNUC_UNUSED uint32_t id)
{
	/* don't care -- we use the above event with the uuid */
}

static gboolean s_bDesktopIsVisible = FALSE;

static void _show_desktop_changed_cb ( G_GNUC_UNUSED void *data,
	G_GNUC_UNUSED  struct org_kde_plasma_window_management *org_kde_plasma_window_management, uint32_t state)
{
	s_bDesktopIsVisible = !!state;
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_VISIBILITY_CHANGED);
}

static void _stacking_order_changed_cb( G_GNUC_UNUSED void *data,
	G_GNUC_UNUSED struct org_kde_plasma_window_management *org_kde_plasma_window_management, G_GNUC_UNUSED struct wl_array *ids)
{
	/* don't care */
}

static void _stacking_order_uuid_changed_cb ( G_GNUC_UNUSED void *data,
	G_GNUC_UNUSED struct org_kde_plasma_window_management *org_kde_plasma_window_management, G_GNUC_UNUSED const char *uuids)
{
	/* don't care */
}

static struct org_kde_plasma_window_management_listener gldi_toplevel_manager = {
    .show_desktop_changed = _show_desktop_changed_cb,
    .window = _window_cb,
    .stacking_order_changed = _stacking_order_changed_cb,
    .stacking_order_uuid_changed = _stacking_order_uuid_changed_cb,
    .window_with_uuid = _new_toplevel,
};

static struct org_kde_plasma_window_management* s_ptoplevel_manager = NULL;
static uint32_t protocol_id, protocol_version;
static gboolean protocol_found = FALSE;

/// Desktop management functions
static gboolean _desktop_is_visible (void) { return s_bDesktopIsVisible; }
static gboolean _show_hide_desktop (gboolean bShow)
{
	org_kde_plasma_window_management_show_desktop (s_ptoplevel_manager, bShow);
	return TRUE;
}

static void gldi_plasma_window_manager_init ()
{
	org_kde_plasma_window_management_add_listener(s_ptoplevel_manager, &gldi_toplevel_manager, NULL);
	// register window manager
	GldiWindowManagerBackend wmb;
	memset (&wmb, 0, sizeof (GldiWindowManagerBackend));
	wmb.get_active_window = gldi_wayland_wm_get_active_window;
	wmb.move_to_nth_desktop = _move_to_nth_desktop;
	wmb.show = _show;
	wmb.close = _close;
	// wmb.kill = _kill;
	wmb.minimize = _minimize;
	// wmb.lower = _lower;
	wmb.maximize = _maximize;
	wmb.set_fullscreen = _fullscreen;
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
	wmb.name = "plasma";
	gldi_windows_manager_register_backend (&wmb);
	
	GldiDesktopManagerBackend dmb;
	memset (&dmb, 0, sizeof (GldiDesktopManagerBackend));
	dmb.desktop_is_visible = _desktop_is_visible;
	dmb.show_hide_desktop = _show_hide_desktop;
	gldi_desktop_manager_register_backend (&dmb, "plasma-window-management");
	
	gldi_wayland_wm_init (_destroy);
}


gboolean gldi_plasma_window_manager_match_protocol (uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp(interface, org_kde_plasma_window_management_interface.name))
	{
		protocol_found = TRUE;
		protocol_id = id;
		protocol_version = version;
		return TRUE;
	}
	return FALSE;
}

gboolean gldi_plasma_window_manager_try_init (struct wl_registry *registry)
{
	if (!protocol_found) return FALSE;
	
	if (protocol_version > (uint32_t)org_kde_plasma_window_management_interface.version)
	{
		protocol_version = org_kde_plasma_window_management_interface.version;
	}
	
	s_ptoplevel_manager = wl_registry_bind (registry, protocol_id, &org_kde_plasma_window_management_interface, protocol_version);
	if (s_ptoplevel_manager)
	{
		gldi_plasma_window_manager_init ();
		return TRUE;
	}
	
	cd_error ("Could not bind plasma-window-manager!");
    return FALSE;
}


/*
 * cairo-dock-plasma-window-management.c
 * 
 * Interact with Wayland clients via the plasma-window-management
 * protocol. See e.g. https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/plasma-window-management.xml
 * 
 * Copyright 2021 Daniel Kondor <kondor.dani@gmail.com>
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
 * - Fix issue where clicking on an active app to minimize it does not work
 */

#include "gldi-config.h"
#ifdef HAVE_WAYLAND

#include <gdk/gdkwayland.h>
#include "plasma-window-management-client-protocol.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-desktop-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-X-manager.h"
#include "cairo-dock-utils.h"
#include "cairo-dock-log.h"
#include "cairo-dock-plasma-window-manager.h"

#include <stdio.h>


static GldiObjectManager myPWMObjectMgr;

typedef struct org_kde_plasma_window pwhandle;


struct _GldiPWindowActor {
	GldiWindowActor actor;
	// plasma-window-management specific
	// handle to toplevel object
	pwhandle *handle;
	// store parent handle, if supplied
	// NOTE: windows with parent are ignored (not shown in the taskbar)
	pwhandle *parent;
	gboolean init_done; // set to true after initial state has been received
	uint32_t state; // current state
};
typedef struct _GldiPWindowActor GldiPWindowActor;



// window manager interface

static void _show (GldiWindowActor *actor)
{
	GldiPWindowActor *wactor = (GldiPWindowActor *)actor;
	org_kde_plasma_window_set_state (wactor->handle,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
}
static void _close (GldiWindowActor *actor)
{
	GldiPWindowActor *wactor = (GldiPWindowActor *)actor;
	org_kde_plasma_window_close (wactor->handle);
}
static void _minimize (GldiWindowActor *actor)
{
	GldiPWindowActor *wactor = (GldiPWindowActor *)actor;
	org_kde_plasma_window_set_state (wactor->handle,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
}
static void _maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	GldiPWindowActor *wactor = (GldiPWindowActor *)actor;
	org_kde_plasma_window_set_state (wactor->handle,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED,
		bMaximize ? ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED : 0);
}
static void _fullscreen (GldiWindowActor *actor, gboolean bFullScreen)
{
	GldiPWindowActor *wactor = (GldiPWindowActor *)actor;
	org_kde_plasma_window_set_state (wactor->handle,
		ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN,
		bFullScreen ? ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN : 0);
}

static GldiWindowActor* _get_transient_for(GldiWindowActor* actor)
{
	GldiPWindowActor *wactor = (GldiPWindowActor *)actor;
	pwhandle* parent = wactor->parent;
	if (!parent) return NULL;
	GldiPWindowActor *pactor = org_kde_plasma_window_get_user_data (parent);
	return (GldiWindowActor*)pactor;
}

/* current active window -- last one to receive activated signal */
static GldiWindowActor* s_pActiveWindow = NULL;
/* maybe new active window -- this is set if the activated signal is
 * received for a window that is not "created" yet, i.e. has not done
 * the initial init yet or has a parent */
static GldiWindowActor* s_pMaybeActiveWindow = NULL;
/* our own config window -- will only work if the docks themselves are not reported */
static GldiWindowActor* s_pSelf = NULL;
static GldiWindowActor* _get_active_window (void)
{
	return s_pActiveWindow;
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
	GldiPWindowActor *wactor = (GldiPWindowActor *)actor;
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



// extra callback for when a new app is activated
// this is useful for e.g. interactively selecting a window
static void (*s_activated_callback)(GldiPWindowActor* wactor, void* data) = NULL;
static void* s_activated_callback_data = NULL;

// callbacks 
static void _gldi_toplevel_title_cb (void *data, G_GNUC_UNUSED pwhandle *handle, const char *title)
{
	GldiPWindowActor* wactor = (GldiPWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	g_free (actor->cName);
	actor->cName = g_strdup ((gchar *)title);
	if (wactor->init_done && !wactor->parent) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_NAME_CHANGED, actor);
}

static void _toplevel_created(GldiPWindowActor *wactor)
{
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_CREATED, actor);
	if (actor == s_pMaybeActiveWindow)
	{
		s_pActiveWindow = actor;
		gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ACTIVATED, actor);
		if (s_activated_callback) s_activated_callback(wactor, s_activated_callback_data);
	}
	if (!strcmp(actor->cClass, "cairo-dock")) s_pSelf = actor;
}

static void _check_toplevel_visible(GldiPWindowActor *wactor)
{
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	gboolean old_displayed = actor->bDisplayed;
	if (actor->cClass && !wactor->parent && !(wactor->state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR))
		actor->bDisplayed = TRUE;
	else actor->bDisplayed = FALSE;
	
	if (wactor->init_done && old_displayed != actor->bDisplayed)
	{
		if (actor->bDisplayed)
		{
			_toplevel_created (wactor);
		}
		else
		{
			if (actor == s_pSelf) s_pSelf = NULL;
			if (actor == s_pActiveWindow) s_pActiveWindow = NULL;
			gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
		}
	}
}

static void _gldi_toplevel_appid_cb (void *data, G_GNUC_UNUSED pwhandle *handle, const char *app_id)
{
	GldiPWindowActor* wactor = (GldiPWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	
	g_free (actor->cClass);
	actor->cClass = g_strdup ((gchar *)app_id);
	
	_check_toplevel_visible (wactor);
}

static void _gldi_toplevel_state_cb (void *data, G_GNUC_UNUSED pwhandle *handle, uint32_t flags)
{
	if (!data) return;
	GldiPWindowActor* wactor = (GldiPWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	gboolean activated = FALSE;
	uint32_t changed = flags ^ wactor->state;
	wactor->state = flags;
	gboolean bHiddenChanged     = FALSE;
	gboolean bMaximizedChanged  = FALSE;
	gboolean bFullScreenChanged = FALSE;
	gboolean bAttentionChanged  = FALSE;
	gboolean bSkipChanged       = FALSE;
	
	if (changed & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE) {
		if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE) activated = TRUE;
		else s_pActiveWindow = NULL;
	}
	if (changed & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED) bHiddenChanged = TRUE;
	if (changed & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED) bMaximizedChanged = TRUE;
	if (changed & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN) bFullScreenChanged = TRUE;
	if (changed & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION) bAttentionChanged = TRUE;
	if (changed & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR) bSkipChanged = TRUE;
	actor->bIsHidden = !!(flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
	actor->bIsMaximized = !!(flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED);
	actor->bIsFullScreen = !!(flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN);
	actor->bDemandsAttention = !!(flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION);
	
	if (bSkipChanged) _check_toplevel_visible (wactor);
	
	if (activated)
	{
		s_pMaybeActiveWindow = actor;
		if (actor->bDisplayed && s_pActiveWindow != actor)
		{
			s_pActiveWindow = actor;
			gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ACTIVATED, actor);
			if (s_activated_callback) s_activated_callback(wactor, s_activated_callback_data);
		}
		else if (wactor->init_done && wactor->parent)
		{
			GldiPWindowActor* pwactor = wactor;
			do {
				pwactor = org_kde_plasma_window_get_user_data(pwactor->parent);
			} while (pwactor->parent);
			GldiWindowActor* pactor = (GldiWindowActor*)pwactor;
			if (!pwactor->init_done) s_pMaybeActiveWindow = pactor;
			else if (pactor->bDisplayed && s_pActiveWindow != pactor)
			{
				s_pActiveWindow = pactor;
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ACTIVATED, pactor);
				if (s_activated_callback) s_activated_callback(pwactor, s_activated_callback_data);
			}
		}
	}
					
	if ( (bHiddenChanged || bMaximizedChanged || bFullScreenChanged) && actor->bDisplayed)
		gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_STATE_CHANGED, actor, bHiddenChanged, bMaximizedChanged, bFullScreenChanged);
	
	if (bAttentionChanged && actor->bDisplayed)
		gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ATTENTION_CHANGED, actor);
}

/* this is sent after the initial state has been sent, i.e. only once */
static void _gldi_toplevel_done_cb ( void *data, G_GNUC_UNUSED pwhandle *handle)
{
	GldiPWindowActor* wactor = (GldiPWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	wactor->init_done = TRUE;
	if (actor->bDisplayed) _toplevel_created (wactor);
}

static void _gldi_toplevel_closed_cb (void *data, G_GNUC_UNUSED pwhandle *handle)
{
	GldiPWindowActor* wactor = (GldiPWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	if (actor->bDisplayed) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
	if (actor == s_pSelf) s_pSelf = NULL;
	if (actor == s_pActiveWindow) s_pActiveWindow = NULL;
	gldi_object_unref (GLDI_OBJECT(actor));
}

static void _gldi_toplevel_parent_cb (void* data, G_GNUC_UNUSED pwhandle *handle, pwhandle *parent)
{
	GldiPWindowActor* wactor = (GldiPWindowActor*)data;
	wactor->parent = parent;
	_check_toplevel_visible (wactor);
}

static void _gldi_toplevel_geometry_cb (void* data, G_GNUC_UNUSED pwhandle *handle, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
	GldiPWindowActor* wactor = (GldiPWindowActor*)data;
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
    .virtual_desktop_entered = _gldi_toplevel_dummy_cb,
    .virtual_desktop_left = _gldi_toplevel_dummy_cb,
    .application_menu = _gldi_toplevel_application_menu_cb,
    .activity_entered = _gldi_toplevel_dummy_cb,
    .activity_left = _gldi_toplevel_dummy_cb
};


typedef enum {
	NB_NOTIFICATIONS_WFT_MANAGER = NB_NOTIFICATIONS_WINDOWS
	} CairoWFTManagerNotifications;

static void init_object (GldiObject *obj, gpointer attr)
{
	GldiPWindowActor* actor = (GldiPWindowActor*)obj;
	actor->handle = (pwhandle*)attr;
}

static void reset_object (GldiObject* obj)
{
	GldiPWindowActor* actor = (GldiPWindowActor*)obj;
	if (obj && actor->handle) org_kde_plasma_window_destroy(actor->handle);
}

/* register new toplevel */
static void _new_toplevel ( G_GNUC_UNUSED void *data, struct org_kde_plasma_window_management *manager,
	G_GNUC_UNUSED uint32_t id, const char *uuid)
{
	// fprintf(stderr,"New toplevel: %p\n", handle);
	pwhandle* handle = org_kde_plasma_window_management_get_window_by_uuid (manager, uuid);
	
	GldiPWindowActor* wactor = (GldiPWindowActor*)gldi_object_new (&myPWMObjectMgr, handle);
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

static void _show_desktop_changed_cb ( G_GNUC_UNUSED void *data,
	G_GNUC_UNUSED  struct org_kde_plasma_window_management *org_kde_plasma_window_management, G_GNUC_UNUSED uint32_t state)
{
	/* don't care */
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


/* Facility to ask the user to pick a window */
struct wft_pick_window_response {
	GldiPWindowActor* wactor;
	GtkDialog* dialog;
	gboolean first_activation;
};

static void _pick_window_cb (GldiPWindowActor* wactor, void* data)
{
	struct wft_pick_window_response* res = (struct wft_pick_window_response*)data;
	if(!res->first_activation) {
		if(!strcmp(wactor->actor.cClass, "cairo-dock")) {
			res->first_activation = TRUE;
			return;
		}
	}
	res->wactor = wactor;
	gtk_dialog_response(res->dialog, 0);
}

static GldiWindowActor* _pick_window (GtkWindow *pParentWindow)
{
	struct wft_pick_window_response res;
	GtkWidget* dialog = gtk_message_dialog_new(pParentWindow,
		GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CANCEL,
		_("Select an open window by clicking on it or choosing it from the taskbar"));
	res.dialog = GTK_DIALOG(dialog);
	res.wactor = NULL;
	res.first_activation = FALSE;
	s_activated_callback = _pick_window_cb;
	s_activated_callback_data = &res;
	gtk_dialog_run(GTK_DIALOG(dialog));
	s_activated_callback = NULL;
	s_activated_callback_data = NULL;
	gtk_widget_destroy(dialog);
	if (s_pSelf) _show(s_pSelf);
	return (GldiWindowActor*)res.wactor;
}


static void gldi_plasma_window_manager_init ()
{
	org_kde_plasma_window_management_add_listener(s_ptoplevel_manager, &gldi_toplevel_manager, NULL);
	// register window manager
	GldiWindowManagerBackend wmb;
	memset (&wmb, 0, sizeof (GldiWindowManagerBackend));
	wmb.get_active_window = _get_active_window;
	// wmb.move_to_nth_desktop = _move_to_nth_desktop;
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
	wmb.pick_window = _pick_window;
	gldi_windows_manager_register_backend (&wmb);
	
	
	// register toplevel object manager
	// Object Manager
	memset (&myPWMObjectMgr, 0, sizeof (GldiObjectManager));
	myPWMObjectMgr.cName   = "Wayland-plasma-window-manager";
	myPWMObjectMgr.iObjectSize    = sizeof (GldiPWindowActor);
	// interface
	myPWMObjectMgr.init_object    = init_object;
	myPWMObjectMgr.reset_object   = reset_object;
	// signals
	gldi_object_install_notifications (&myPWMObjectMgr, NB_NOTIFICATIONS_WFT_MANAGER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myPWMObjectMgr), &myWindowObjectMgr);
	
}

gboolean gldi_plasma_window_manager_try_bind (struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp(interface, org_kde_plasma_window_management_interface.name))
    {
		if (version > (uint32_t)org_kde_plasma_window_management_interface.version)
		{
			version = org_kde_plasma_window_management_interface.version;
		}
        s_ptoplevel_manager = wl_registry_bind (registry, id, &org_kde_plasma_window_management_interface, version);
		if (s_ptoplevel_manager)
		{
			gldi_plasma_window_manager_init ();
			return TRUE;
		}
		else cd_message ("Could not bind plasma-window-manager!");
    }
    return FALSE;
}


#endif


/*
 * cairo-dock-foreign-toplevel.c
 * 
 * Interact with Wayland clients via the zwlr_foreign_toplevel_manager
 * protocol. See e.g. https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-foreign-toplevel-management-unstable-v1.xml
 * 
 * Copyright 2020-2023 Daniel Kondor <kondor.dani@gmail.com>
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
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-desktop-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-X-manager.h"
#include "cairo-dock-utils.h"
#include "cairo-dock-log.h"
#include "cairo-dock-foreign-toplevel.h"

#include <stdio.h>


typedef struct zwlr_foreign_toplevel_handle_v1 wfthandle;

struct _GldiWFTWindowActor {
	GldiWindowActor actor;
	// foreign-toplevel specific handle to toplevel object
	wfthandle *handle;
	// store parent handle, if supplied
	// NOTE: windows with parent are ignored (not shown in the taskbar)
	wfthandle *parent;
	
	gchar* cClassPending; // new app_id received
	gchar* cTitlePending; // new title received
	
	gboolean maximized_pending; // maximized state received
	gboolean minimized_pending; // minimized state received
	gboolean fullscreen_pending; // fullscreen state received
	gboolean close_pending; // this window has been closed
	
	gboolean in_queue; // this actor has been added to the s_pending_queue
};
typedef struct _GldiWFTWindowActor GldiWFTWindowActor;


/********************************************************************
 * static variables used                                            */

static GldiObjectManager myWFTObjectMgr;

static struct zwlr_foreign_toplevel_manager_v1* s_ptoplevel_manager = NULL;

/* current active window -- last one to receive activated signal */
static GldiWindowActor* s_pActiveWindow = NULL;
/* maybe new active window -- this is set if the activated signal is
 * received for a window that is not "created" yet, i.e. has not done
 * the initial init yet or has a parent */
static GldiWindowActor* s_pMaybeActiveWindow = NULL;
/* our own config window -- will only work if the docks themselves are not
 * reported */
static GldiWindowActor* s_pSelf = NULL;

// extra callback for when a new app is activated
// this is useful for e.g. interactively selecting a window
static void (*s_activated_callback)(GldiWFTWindowActor* wactor, void* data) = NULL;
static void* s_activated_callback_data = NULL;

/* Facilities for the reentrant processing of events.
 * This is needed as we might receive events while a notification from
 * us is processed by other components of Cairo-Dock (by GTK / GDK
 * functions that initiate a roundtrip to the compositor).
 * However, handling of window creation is not reentrant, so we have to
 * take care to not send nested notifications. */
GQueue s_pending_queue = G_QUEUE_INIT;
GldiWFTWindowActor* s_pCurrent = NULL;


/**********************************************************************
 * window manager interface                                           */

static void _show (GldiWindowActor *actor)
{
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	GdkDisplay *dsp = gdk_display_get_default ();
	GdkSeat *seat = gdk_display_get_default_seat (dsp);
	struct wl_seat* wl_seat = gdk_wayland_seat_get_wl_seat (seat);
	zwlr_foreign_toplevel_handle_v1_activate (wactor->handle, wl_seat);
}
static void _close (GldiWindowActor *actor)
{
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	zwlr_foreign_toplevel_handle_v1_close (wactor->handle);
}
static void _minimize (GldiWindowActor *actor)
{
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	zwlr_foreign_toplevel_handle_v1_set_minimized (wactor->handle);
}
static void _maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	if (bMaximize) zwlr_foreign_toplevel_handle_v1_set_maximized (wactor->handle);
	else zwlr_foreign_toplevel_handle_v1_unset_maximized (wactor->handle);
}

static GldiWindowActor* _get_transient_for(GldiWindowActor* actor)
{
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	wfthandle* parent = wactor->parent;
	if (!parent) return NULL;
	GldiWFTWindowActor *pactor = zwlr_foreign_toplevel_handle_v1_get_user_data (parent);
	return (GldiWindowActor*)pactor;
}

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
static void _set_minimize_position (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y)
{
	if ( ! (actor && pContainerWidget) ) return;
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	GdkWindow* window = gtk_widget_get_window (pContainerWidget);
	if (!window) return;
	struct wl_surface* surface = gdk_wayland_window_get_wl_surface (window);
	if (!surface) return;
	
	zwlr_foreign_toplevel_handle_v1_set_rectangle(wactor->handle, surface, x, y, 1, 1);
}

static void _set_thumbnail_area (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y, int w, int h)
{
	if ( ! (actor && pContainerWidget) ) return;
	if (w < 0 || h < 0) return;
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	GdkWindow* window = gtk_widget_get_window (pContainerWidget);
	if (!window) return;
	struct wl_surface* surface = gdk_wayland_window_get_wl_surface (window);
	if (!surface) return;
	
	zwlr_foreign_toplevel_handle_v1_set_rectangle(wactor->handle, surface, x, y, w, h);
}

static void _set_fullscreen (GldiWindowActor *actor, gboolean bFullScreen)
{
	if (!actor) return;
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	if (bFullScreen) zwlr_foreign_toplevel_handle_v1_set_fullscreen (wactor->handle, NULL);
	else zwlr_foreign_toplevel_handle_v1_unset_fullscreen (wactor->handle);
}


/* Facility to ask the user to pick a window */
struct wft_pick_window_response {
	GldiWFTWindowActor* wactor;
	GtkDialog* dialog;
	gboolean first_activation;
};

static void _pick_window_cb (GldiWFTWindowActor* wactor, void* data)
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



/**********************************************************************
 * callbacks                                                          */

static void _gldi_toplevel_title_cb (void *data, G_GNUC_UNUSED wfthandle *handle, const char *title)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
	g_free (wactor->cTitlePending);
	wactor->cTitlePending = g_strdup ((gchar *)title);
}

static void _gldi_toplevel_appid_cb (void *data, G_GNUC_UNUSED wfthandle *handle, const char *app_id)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
	g_free (wactor->cClassPending);
	wactor->cClassPending = g_strdup ((gchar *)app_id);
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
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
	wactor->maximized_pending = FALSE;
	wactor->minimized_pending = FALSE;
	wactor->fullscreen_pending = FALSE;
	int i;
	uint32_t* stdata = (uint32_t*)state->data;
	for (i = 0; i*sizeof(uint32_t) < state->size; i++)
	{
		if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
			s_pMaybeActiveWindow = (GldiWindowActor*)wactor;
		else if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
			wactor->maximized_pending = TRUE;
		else if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
			wactor->minimized_pending = TRUE;
		else if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
			wactor->fullscreen_pending = TRUE;
	}
}

/* Process pending updates for a window's title and send a notification
 * if notify == TRUE. Return whether the title has in fact changed. */
static gboolean _update_title (GldiWFTWindowActor* wactor, gboolean notify)
{
	if (wactor->cTitlePending)
	{
		GldiWindowActor* actor = (GldiWindowActor*)wactor;
		g_free(actor->cName);
		actor->cName = wactor->cTitlePending;
		wactor->cTitlePending = NULL;
		if (notify) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_NAME_CHANGED, actor);
		return TRUE;
	}
	return FALSE;
}

/* Process changes in the window state (minimized, maximized, fullscreen)
 * and send a notification about any change if notify == TRUE. Returns
 * whether any of the properties have changed. */
static gboolean _update_state (GldiWFTWindowActor* wactor, gboolean notify)
{
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	
	gboolean bHiddenChanged = (wactor->minimized_pending != actor->bIsHidden);
	if (bHiddenChanged) actor->bIsHidden = wactor->minimized_pending;
	gboolean bMaximizedChanged = (wactor->maximized_pending != actor->bIsMaximized);
	if (bMaximizedChanged) actor->bIsMaximized  = wactor->maximized_pending;
	gboolean bFullScreenChanged = (wactor->fullscreen_pending != actor->bIsFullScreen);
	if (bFullScreenChanged) actor->bIsFullScreen = wactor->fullscreen_pending;
	
	gboolean any_change = (bHiddenChanged || bMaximizedChanged || bFullScreenChanged);
	
	if (notify && any_change)
		gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_STATE_CHANGED, actor, bHiddenChanged, bMaximizedChanged, bFullScreenChanged);
	
	return any_change;
}

static void _gldi_toplevel_done_cb ( void *data, G_GNUC_UNUSED wfthandle *handle)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
	
	if(s_pCurrent) {
		/* We are being called in a nested way, avoid sending
		 * notifications by queuing them for later.
		 * 
		 * This is necessary, since:
		 * 1. If we call gldi_object_notify(), it may initiate a roundtrip with
		 * 		the Wayland server, resulting in more data arriving on the
		 * 		foreign-toplevel protocol as well, so we may end up in this
		 * 		function in a reentrant way.
		 * 2. At the same time, the WM functions are not reentrant, so we should
		 * 		NOT call gldi_object_notify() with WM-related stuff again.
		 * 		Specifically, this could result in subdock objects and / or icons
		 * 		being duplicated (for newly created windows).
		 * 
		 * To avoid this, we defer any processing until gldi_object_notify()
		 * returns. This can result in some events being missed (e.g. minimize
		 * immediately followed by unminimize, or two title / app_id changes in
		 * quick succession), but these cases are not a serious problem. */
		if (wactor != s_pCurrent && !wactor->in_queue)
		{
			g_queue_push_tail(&s_pending_queue, wactor); // add to queue
			wactor->in_queue = TRUE;
		}
		return;
	}
	
	do {
		// Set that we are already potentially processing a notification for this actor.
		s_pCurrent = wactor;
		wactor->in_queue = FALSE;
		
		GldiWindowActor* actor = (GldiWindowActor*)wactor;
		gchar* cOldClass = NULL;
		gchar* cOldWmClass = NULL;
		
		while(1) {
			// free possible leftover values from the previous iteration
			g_free(cOldClass);
			g_free(cOldWmClass);
			cOldClass = NULL;
			cOldWmClass = NULL;
			
			// Process all possible fields
			if(wactor->close_pending) {
				// this window was closed, just notify the taskbar and free it
				if (actor->bDisplayed) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
				if (actor == s_pSelf) s_pSelf = NULL;
				if (actor == s_pActiveWindow) s_pActiveWindow = NULL;
				if (actor == s_pMaybeActiveWindow) s_pMaybeActiveWindow = NULL;
				gldi_object_unref (GLDI_OBJECT(actor));
				break;
			}
			
			gboolean class_changed = FALSE;
			if (wactor->cClassPending)
			{
				/* we have a new class; this might mean a change or that we
				 * newly display (or hide) this window in the taskbar */
				class_changed = TRUE;
				cOldClass = actor->cClass;
				cOldWmClass = actor->cWmClass;
				actor->cWmClass = wactor->cClassPending;
				actor->cClass = gldi_window_parse_class (actor->cWmClass, actor->cName);
				wactor->cClassPending = NULL;
			}
			gboolean displayed = FALSE; // whether this window should be displayed
			if (!wactor->parent && actor->cClass) displayed = TRUE;
			
			if (!actor->bDisplayed && !displayed)
			{
				// not displaying this actor, but we can still update its properties without sending notifications
				_update_title(wactor, FALSE);
				_update_state(wactor, FALSE);
				break; 
			}
			if (!actor->bDisplayed && displayed)
			{
				// this actor was not displayed so far but should be
				actor->bDisplayed = TRUE;
				// we also process additional changes, but no need to send notifications
				_update_title(wactor, FALSE);
				_update_state(wactor, FALSE);
				
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_CREATED, actor);
				continue; /* check for other changes that might have happened while processing the above */
			}
			if (actor->bDisplayed && !displayed)
			{
				// should hide this actor
				actor->bDisplayed = FALSE;
				// we also process additional changes, but no need to send notifications
				_update_title(wactor, FALSE);
				_update_state(wactor, FALSE);
				
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
				if (actor == s_pSelf) s_pSelf = NULL;
				if (actor == s_pActiveWindow) s_pActiveWindow = NULL;
				continue;
			}
			
			if (class_changed)
			{
				if (actor == s_pSelf) s_pSelf = NULL;
				if (!strcmp(actor->cClass, "cairo-dock")) s_pSelf = actor;
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_CLASS_CHANGED, actor, cOldClass, cOldWmClass);
				continue;
			}
			
			// check if the title changed (and send a notification if it did)
			if (_update_title (wactor, TRUE)) continue;
			// check if other properties have changed (and send a notification)
			if (_update_state (wactor, TRUE)) continue;
			
			if (actor == s_pMaybeActiveWindow)
			{
				s_pActiveWindow = actor;
				s_pMaybeActiveWindow = NULL;
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ACTIVATED, actor);
				if (s_activated_callback) s_activated_callback(wactor, s_activated_callback_data);
				continue;
			}
			
			break;
		}
		
		/* possible leftover values from the last iteration */
		g_free(cOldClass);
		g_free(cOldWmClass);
		
		s_pCurrent = NULL;
		wactor = g_queue_pop_head(&s_pending_queue); // note: it is OK to call this on an empty queue
	} while (wactor);
}

static void _gldi_toplevel_closed_cb (void *data, G_GNUC_UNUSED wfthandle *handle)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
	wactor->close_pending = TRUE;
	// this will process this event immediately, or add to the queue of possible events
	// (no done event will be sent for this window)
	_gldi_toplevel_done_cb(wactor, NULL);
}

static void _gldi_toplevel_parent_cb (void* data, G_GNUC_UNUSED wfthandle *handle, wfthandle *parent)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
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

typedef enum {
	NB_NOTIFICATIONS_WFT_MANAGER = NB_NOTIFICATIONS_WINDOWS
} CairoWFTManagerNotifications;

static void init_object (GldiObject *obj, gpointer attr)
{
	GldiWFTWindowActor* actor = (GldiWFTWindowActor*)obj;
	actor->handle = (wfthandle*)attr;
}

static void reset_object (GldiObject* obj)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)obj;
	
	if (wactor)
	{
		g_free(wactor->cClassPending);
		g_free(wactor->cTitlePending);
		if (wactor->handle) zwlr_foreign_toplevel_handle_v1_destroy(wactor->handle);
	}
}

/* register new toplevel */
static void _new_toplevel ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwlr_foreign_toplevel_manager_v1 *manager,
	wfthandle *handle)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)gldi_object_new (&myWFTObjectMgr, handle);
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

static void gldi_zwlr_foreign_toplevel_manager_init ()
{
	if (!s_ptoplevel_manager) return;
	
	zwlr_foreign_toplevel_manager_v1_add_listener(s_ptoplevel_manager, &gldi_toplevel_manager, NULL);
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
	wmb.pick_window = _pick_window;
	gldi_windows_manager_register_backend (&wmb);
	
	
	// register toplevel object manager
	// Object Manager
	memset (&myWFTObjectMgr, 0, sizeof (GldiObjectManager));
	myWFTObjectMgr.cName   = "Wayland-wlr-foreign-toplevel-manager";
	myWFTObjectMgr.iObjectSize    = sizeof (GldiWFTWindowActor);
	// interface
	myWFTObjectMgr.init_object    = init_object;
	myWFTObjectMgr.reset_object   = reset_object;
	// signals
	gldi_object_install_notifications (&myWFTObjectMgr, NB_NOTIFICATIONS_WFT_MANAGER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myWFTObjectMgr), &myWindowObjectMgr);
	
}

gboolean gldi_zwlr_foreign_toplevel_manager_try_bind (struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name))
    {
		if (version > zwlr_foreign_toplevel_manager_v1_interface.version)
		{
			version = zwlr_foreign_toplevel_manager_v1_interface.version;
		}
        s_ptoplevel_manager = wl_registry_bind (registry, id, &zwlr_foreign_toplevel_manager_v1_interface, version);
		if (s_ptoplevel_manager)
		{
			gldi_zwlr_foreign_toplevel_manager_init ();
			return TRUE;
		}
		else cd_message ("Could not bind foreign-toplevel-manager!");
    }
    return FALSE;
}


#endif


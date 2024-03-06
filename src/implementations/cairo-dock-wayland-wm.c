/*
 * cairo-dock-wayland-wm.c -- common interface for window / taskbar
 * 	management facilities on Wayland
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

#include "cairo-dock-windows-manager.h"
#include "cairo-dock-desktop-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-wayland-wm.h"
#include "cairo-dock-utils.h"
#include "cairo-dock-log.h"

extern CairoDock* g_pMainDock;

static GldiObjectManager myWaylandWMObjectMgr;

static void (*s_handle_destroy_cb)(gpointer handle) = NULL;

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
static void (*s_activated_callback)(GldiWaylandWindowActor* wactor, void* data) = NULL;
static void* s_activated_callback_data = NULL;


/* Facilities for the reentrant processing of events.
 * This is needed as we might receive events while a notification from
 * us is processed by other components of Cairo-Dock (by GTK / GDK
 * functions that initiate a roundtrip to the compositor).
 * However, handling of window creation is not reentrant, so we have to
 * take care to not send nested notifications. */
GQueue s_pending_queue = G_QUEUE_INIT;
GldiWaylandWindowActor* s_pCurrent = NULL;

static guint s_iIdle = 0;

/* Facility to ask the user to pick a window */
struct wft_pick_window_response {
	GldiWaylandWindowActor* wactor;
	GtkDialog* dialog;
	gboolean first_activation;
};

static void _pick_window_cb (GldiWaylandWindowActor* wactor, void* data)
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

GldiWindowActor* gldi_wayland_wm_pick_window (GtkWindow *pParentWindow)
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
	if (s_pSelf) gldi_window_show(s_pSelf);
	return (GldiWindowActor*)res.wactor;
}



void gldi_wayland_wm_title_changed (GldiWaylandWindowActor *wactor, const char *title, gboolean notify)
{
	g_free (wactor->cTitlePending);
	wactor->cTitlePending = g_strdup ((gchar *)title);
	if (notify) gldi_wayland_wm_done (wactor);
}

// note: this might create / remove the corresponding icon
void gldi_wayland_wm_appid_changed (GldiWaylandWindowActor *wactor, const char *app_id, gboolean notify)
{
	g_free (wactor->cClassPending);
	wactor->cClassPending = g_strdup ((gchar *)app_id);
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_maximized_changed (GldiWaylandWindowActor *wactor, gboolean maximized, gboolean notify)
{
	wactor->maximized_pending = maximized;
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_minimized_changed (GldiWaylandWindowActor *wactor, gboolean minimized, gboolean notify)
{
	wactor->minimized_pending = minimized;
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_fullscreen_changed (GldiWaylandWindowActor *wactor, gboolean fullscreen, gboolean notify)
{
	wactor->fullscreen_pending = fullscreen;
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_attention_changed (GldiWaylandWindowActor *wactor, gboolean attention, gboolean notify)
{
	wactor->attention_pending = attention;
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_skip_changed (GldiWaylandWindowActor *wactor, gboolean skip, gboolean notify)
{
	wactor->skip_taskbar = skip;
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_closed (GldiWaylandWindowActor *wactor, gboolean notify)
{
	wactor->close_pending = TRUE;
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_activated (GldiWaylandWindowActor *wactor, gboolean notify)
{
	s_pMaybeActiveWindow = (GldiWindowActor*)wactor;
	if (notify) gldi_wayland_wm_done (wactor);
}


/* Process pending updates for a window's title and send a notification
 * if notify == TRUE. Return whether the title has in fact changed. */
static gboolean _update_title (GldiWaylandWindowActor* wactor, gboolean notify)
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
static gboolean _update_state (GldiWaylandWindowActor* wactor, gboolean notify)
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

static gboolean _update_attention (GldiWaylandWindowActor *wactor, gboolean notify)
{
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	gboolean changed = (wactor->attention_pending != actor->bDemandsAttention);
	if (changed) actor->bDemandsAttention = wactor->attention_pending;
	if (notify && changed) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ATTENTION_CHANGED, actor);
	return changed;
}

static gboolean _idle_done (G_GNUC_UNUSED gpointer data)
{
	GldiWaylandWindowActor *wactor = g_queue_pop_head(&s_pending_queue);
	s_iIdle = 0;
	if (wactor)
	{
		gldi_wayland_wm_done (wactor);
	}
	return FALSE;
}

void gldi_wayland_wm_done (GldiWaylandWindowActor *wactor)
{
	if(s_pCurrent || !g_pMainDock) {
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
		/* Note: adding icons will not work properly if the main dock has
		 * not initialized yet. If this is the case, we set a callback to
		 * process the queued toplevels later. */
		if (!g_pMainDock) s_iIdle = g_idle_add (_idle_done, NULL);
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
			if (!wactor->parent && actor->cClass && !wactor->skip_taskbar) {
				displayed = TRUE;
				if (!strcmp (actor->cClass, "cairo-dock"))
				{
					// we don't display our own desklets (note: no general "skip taskbar"
					// hint, so we have to recognize them based on the app_id and title)
					const gchar *tmp = wactor->cTitlePending ? wactor->cTitlePending : actor->cName;
					if (tmp && !strcmp (tmp, "cairo-dock-desklet")) displayed = FALSE;
				}
			}
			
			if (!actor->bDisplayed && !displayed)
			{
				// not displaying this actor, but we can still update its properties without sending notifications
				_update_title(wactor, FALSE);
				_update_state(wactor, FALSE);
				_update_attention(wactor, FALSE);
				break; 
			}
			if (!actor->bDisplayed && displayed)
			{
				// this actor was not displayed so far but should be
				actor->bDisplayed = TRUE;
				// we also process additional changes, but no need to send notifications
				_update_title(wactor, FALSE);
				_update_state(wactor, FALSE);
				_update_attention(wactor, FALSE);
				
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
				_update_attention(wactor, FALSE);
				
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
			// update the needs-attention property
			if (_update_attention(wactor, TRUE)) continue;
				
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

GldiWindowActor* gldi_wayland_wm_get_active_window ()
{
	return s_pActiveWindow;
}

GldiWaylandWindowActor* gldi_wayland_wm_new_toplevel (gpointer handle)
{
	return (GldiWaylandWindowActor*)gldi_object_new (&myWaylandWMObjectMgr, handle);
}


typedef enum {
	NB_NOTIFICATIONS_WAYLAND_WM_MANAGER = NB_NOTIFICATIONS_WINDOWS
} CairoWaylandWMManagerNotifications;

static void _init_object (GldiObject *obj, gpointer attr)
{
	GldiWaylandWindowActor* actor = (GldiWaylandWindowActor*)obj;
	actor->handle = attr;
}

static void _reset_object (GldiObject* obj)
{
	GldiWaylandWindowActor* wactor = (GldiWaylandWindowActor*)obj;
	
	if (wactor)
	{
		g_free(wactor->cClassPending);
		g_free(wactor->cTitlePending);
		if (wactor->handle && s_handle_destroy_cb) s_handle_destroy_cb(wactor->handle);
	}
}


void gldi_wayland_wm_init (GldiWaylandWMHandleDestroyFunc destroy_cb)
{
	s_handle_destroy_cb = destroy_cb;
	
	// register toplevel object manager
	// Object Manager
	memset (&myWaylandWMObjectMgr, 0, sizeof (GldiObjectManager));
	myWaylandWMObjectMgr.cName        = "Wayland-WM-manager";
	myWaylandWMObjectMgr.iObjectSize  = sizeof (GldiWaylandWindowActor);
	// interface
	myWaylandWMObjectMgr.init_object  = _init_object;
	myWaylandWMObjectMgr.reset_object = _reset_object;
	// signals
	gldi_object_install_notifications (&myWaylandWMObjectMgr, NB_NOTIFICATIONS_WAYLAND_WM_MANAGER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myWaylandWMObjectMgr), &myWindowObjectMgr);
}



#endif // HAVE_WAYLAND


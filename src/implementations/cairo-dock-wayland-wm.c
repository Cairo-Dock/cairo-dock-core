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

#include "cairo-dock-windows-manager-priv.h"
#include "cairo-dock-desktop-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-wayland-wm.h"
#include "cairo-dock-utils.h"
#include "cairo-dock-log.h"

extern CairoDock* g_pMainDock;

GldiObjectManager myWaylandWMObjectMgr;

enum NextChange {
	NC_GEOMETRY,
	NC_VIEWPORT,
	NC_ATTENTION,
	NC_STICKY,
	NC_TITLE,
	NC_STATE,
	NC_NO_CHANGE
};

static void _set_pending_change (GldiWaylandWindowActor *wactor, enum NextChange ch)
{
	if ((int)ch < wactor->next_change_pending)
		wactor->next_change_pending = ch;
}

static void (*s_handle_destroy_cb)(gpointer handle) = NULL;

/* current active window */
static GldiWindowActor* s_pActiveWindow = NULL;
/* last window to receive activated signal, might have been deactivated by now */
static GldiWindowActor* s_pLastActiveWindow = NULL;
/* maybe new active window -- this is set if the activated signal is
 * received for a window that is not "created" yet, i.e. has not done
 * the initial init yet or has a parent */
static GldiWindowActor* s_pMaybeActiveWindow = NULL;
/* our own config window -- will only work if the docks themselves are not
 * reported */
static GldiWindowActor* s_pSelf = NULL;
/* internal counter for updating windows' stacking order */
static int s_iStackCounter = 0;
guint s_sidStackNotify = 0;
static int s_iNumWindow = 1;  // used to order appli icons by age (=creation date).

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
static GQueue s_pending_queue = G_QUEUE_INIT;
static GldiWaylandWindowActor* s_pCurrent = NULL;
static gboolean s_bProcessing = FALSE;

static guint s_iIdle = 0;

/* Function to call in our idle handler before processing changes to any
 * window. The main purpose is to let the desktop manager update the
 * geometry if needed. */
static void (*s_fNotify)(void) = NULL;

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
	_set_pending_change (wactor, NC_TITLE);
	if (notify) gldi_wayland_wm_done (wactor);
}

// note: this might create / remove the corresponding icon
void gldi_wayland_wm_appid_changed (GldiWaylandWindowActor *wactor, const char *app_id, gboolean notify)
{
	g_free (wactor->cClassPending);
	wactor->cClassPending = g_strdup ((gchar *)app_id);
	// app-ids should not contain spaces, but some compositors can add extra info, e.g. Wayfire can add an IPC ID
	char *tmp = strchr (wactor->cClassPending, ' ');
	if (tmp)
	{
		if (*(tmp+1)) wactor->cClassExtra = tmp + 1;
		else wactor->cClassExtra = NULL;
		*tmp = 0;
		
	}
	else wactor->cClassExtra = NULL;
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_maximized_changed (GldiWaylandWindowActor *wactor, gboolean maximized, gboolean notify)
{
	wactor->maximized_pending = maximized;
	_set_pending_change (wactor, NC_STATE);
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_minimized_changed (GldiWaylandWindowActor *wactor, gboolean minimized, gboolean notify)
{
	wactor->minimized_pending = minimized;
	_set_pending_change (wactor, NC_STATE);
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_fullscreen_changed (GldiWaylandWindowActor *wactor, gboolean fullscreen, gboolean notify)
{
	wactor->fullscreen_pending = fullscreen;
	_set_pending_change (wactor, NC_STATE);
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_attention_changed (GldiWaylandWindowActor *wactor, gboolean attention, gboolean notify)
{
	wactor->attention_pending = attention;
	_set_pending_change (wactor, NC_ATTENTION);
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_skip_changed (GldiWaylandWindowActor *wactor, gboolean skip, gboolean notify)
{
	wactor->skip_taskbar = skip;
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_sticky_changed (GldiWaylandWindowActor *wactor, gboolean sticky, gboolean notify)
{
	wactor->sticky_pending = sticky;
	_set_pending_change (wactor, NC_STICKY);
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_closed (GldiWaylandWindowActor *wactor, gboolean notify)
{
	wactor->close_pending = TRUE;
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_activated (GldiWaylandWindowActor *wactor, gboolean activated, gboolean notify)
{
	if (activated)
	{
		s_pMaybeActiveWindow = (GldiWindowActor*)wactor;
		wactor->unfocused_pending = FALSE;
		if (wactor->init_done && s_activated_callback) s_activated_callback (wactor, s_activated_callback_data);
	}
	else
	{
		wactor->unfocused_pending = TRUE;
		if (s_pMaybeActiveWindow == (GldiWindowActor*)wactor)
			s_pMaybeActiveWindow = NULL;
	}
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_viewport_changed (GldiWaylandWindowActor *wactor, int viewport_x, int viewport_y, gboolean notify)
{
	wactor->pending_viewport_x = viewport_x;
	wactor->pending_viewport_y = viewport_y;
	_set_pending_change (wactor, NC_VIEWPORT);
	if (notify) gldi_wayland_wm_done (wactor);
}

void gldi_wayland_wm_geometry_changed (GldiWaylandWindowActor *wactor, const GtkAllocation *new_geom, gboolean notify)
{
	wactor->pending_window_geometry = *new_geom;
	_set_pending_change (wactor, NC_GEOMETRY);
	if (notify) gldi_wayland_wm_done (wactor);
}


/* Process pending updates for a window's title and send a notification
 * if notify == TRUE. Returns whether a notification has been sent. */
static gboolean _update_title (GldiWaylandWindowActor* wactor, gboolean notify)
{
	if (wactor->cTitlePending)
	{
		GldiWindowActor* actor = (GldiWindowActor*)wactor;
		g_free(actor->cName);
		actor->cName = wactor->cTitlePending;
		wactor->cTitlePending = NULL;
		if (notify)
		{
			gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_NAME_CHANGED, actor);
			return TRUE;
		}
	}
	return FALSE;
}

/* Process changes in the window state (minimized, maximized, fullscreen)
 * and send a notification about any change if notify == TRUE.
 * Returns whether any notification has been sent. */
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
	
	return (notify && any_change);
}

static gboolean _update_attention (GldiWaylandWindowActor *wactor, gboolean notify)
{
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	gboolean changed = (wactor->attention_pending != actor->bDemandsAttention);
	if (changed) actor->bDemandsAttention = wactor->attention_pending;
	if (notify && changed) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ATTENTION_CHANGED, actor);
	return (notify && changed);
}

static gboolean _update_sticky (GldiWaylandWindowActor *wactor, gboolean notify)
{
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	gboolean changed = (wactor->sticky_pending != actor->bIsSticky);
	if (changed) actor->bIsSticky = wactor->sticky_pending;
	// a change in stickyness can be seen as a change in the desktop position
	if (notify && changed) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, actor);
	return (notify && changed);
}

static gboolean _update_viewport (GldiWaylandWindowActor *wactor, gboolean notify)
{
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	gboolean changed = FALSE;
	if (wactor->pending_viewport_x != actor->iViewPortX)
	{
		actor->iViewPortX = wactor->pending_viewport_x;
		changed = TRUE;
	}
	if (wactor->pending_viewport_y != actor->iViewPortY)
	{
		actor->iViewPortY = wactor->pending_viewport_y;
		changed = TRUE;
	}
	if (notify && changed) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, actor);
	return (changed && changed);
}

static gboolean _update_geometry (GldiWaylandWindowActor *wactor, gboolean notify)
{
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	gboolean changed = FALSE;
	if (wactor->pending_window_geometry.width > 0 && wactor->pending_window_geometry.height > 0)
	{
		if (wactor->pending_window_geometry.x != actor->windowGeometry.x)
		{
			actor->windowGeometry.x = wactor->pending_window_geometry.x;
			changed = TRUE;
		}
		if (wactor->pending_window_geometry.y != actor->windowGeometry.y)
		{
			actor->windowGeometry.y = wactor->pending_window_geometry.y;
			changed = TRUE;
		}
		if (wactor->pending_window_geometry.width != actor->windowGeometry.width)
		{
			actor->windowGeometry.width = wactor->pending_window_geometry.width;
			changed = TRUE;
		}
		if (wactor->pending_window_geometry.height != actor->windowGeometry.height)
		{
			actor->windowGeometry.height = wactor->pending_window_geometry.height;
			changed = TRUE;
		}
	}
	if (notify && changed) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_SIZE_POSITION_CHANGED, actor);
	return (notify && changed);
}


#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
#define FALLTHROUGH [[fallthrough]];
#elif defined(__GNUC__)
#define FALLTHROUGH __attribute__((fallthrough));
#else
#define FALLTHROUGH
#endif

/** Update the pending state of a window.
 *  If notify == TRUE, the appropriate notifications will be emitted and this function will return TRUE
 *  after the first changed state. */
static gboolean _update_all_pending_state (GldiWaylandWindowActor *wactor, gboolean notify)
{
	switch (wactor->next_change_pending)
	{
		// note: order has to be the same in which the enum members are defined above as
		// we rely on falling through the cases to check all changes
		case NC_GEOMETRY:
			if (_update_geometry (wactor, notify))
			{
				wactor->next_change_pending = NC_VIEWPORT;
				return TRUE;
			}
			FALLTHROUGH
		case NC_VIEWPORT:
			if (_update_viewport (wactor, notify))
			{
				wactor->next_change_pending = NC_ATTENTION;
				return TRUE;
			}
			FALLTHROUGH
		case NC_ATTENTION:
			if (_update_attention(wactor, notify))
			{
				wactor->next_change_pending = NC_STICKY;
				return TRUE;
			}
			FALLTHROUGH
		case NC_STICKY:
			if (_update_sticky (wactor, notify))
			{
				wactor->next_change_pending = NC_TITLE;
				return TRUE;
			}
			FALLTHROUGH
		case NC_TITLE:
			if (_update_title(wactor, notify))
			{
				wactor->next_change_pending = NC_STATE;
				return TRUE;
			}
			FALLTHROUGH
		case NC_STATE:
			if (_update_state(wactor, notify))
			{
				wactor->next_change_pending = NC_NO_CHANGE;
				return TRUE;
			}
	}
	
	wactor->next_change_pending = NC_NO_CHANGE; // we processed all changes
	return FALSE;
}

#undef FALLTHROUGH

static void _done_internal (void);
static gboolean _idle_done (G_GNUC_UNUSED gpointer data)
{
	if (s_fNotify) s_fNotify ();
	s_iIdle = 0;
	_done_internal ();
	return FALSE;
}

void gldi_wayland_wm_done (GldiWaylandWindowActor *wactor)
{	
	/* We avoid sending notifications by queuing them for later to
	 * do so when idle.
	 * 
	 * This is necessary, since:
	 * 1. The function calling us is likely an event handler for a Wayland
	 * 		object, which means that some other component is dispatching
	 * 		Wayland events.
	 * 2. If we call gldi_object_notify(), it may mess up internal state
	 * 		that is not expected. Also it may initiate a roundtrip with
	 * 		the Wayland server which is best to avoid.
	 * 3. Also the WM functions themselves are not reentrant, so calling
	 * 		them from an event handler can result in subdock objects
	 * 		and / or icons being duplicated.
	 * 
	 * To avoid problems with reentrancy, we defer any processing until our
	 * main loop is idle. This can result in some events being missed (e.g.
	 * minimize immediately followed by unminimize, or two title / app_id
	 * changes in quick succession), but these cases are not a serious problem. */
	if (wactor != s_pCurrent && !wactor->in_queue)
	{
		g_queue_push_tail(&s_pending_queue, wactor); // add to queue
		wactor->in_queue = TRUE;
	}
	
	if (!s_bProcessing && !s_iIdle) s_iIdle = g_idle_add (_idle_done, NULL);
	return;
}
	
static void _done_internal (void)
{
	s_bProcessing = TRUE;
	gboolean bUnfocused = FALSE; // whether the current active window lost focus
	// note: it is OK to call this on an empty queue, will return NULL
	GldiWaylandWindowActor *wactor = g_queue_pop_head (&s_pending_queue);
	
	while (wactor) {
		// Set that we are already potentially processing a notification for this actor.
		s_pCurrent = wactor;
		wactor->in_queue = FALSE;
		
		// mark that we received all initial info about this window
		wactor->init_done = TRUE;
		
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
				if (actor == s_pLastActiveWindow) s_pLastActiveWindow = NULL;
				if (actor == s_pMaybeActiveWindow) s_pMaybeActiveWindow = NULL;
				gldi_object_unref (GLDI_OBJECT(actor));
				gldi_wayland_wm_notify_stack_change ();
				bUnfocused = TRUE;
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
					if (tmp && !strncmp (tmp, "cairo-dock-desklet", 18)) displayed = FALSE;
				}
			}
			
			if (!actor->bDisplayed && !displayed)
			{
				// not displaying this actor, but we can still update its properties without sending notifications
				_update_all_pending_state (wactor, FALSE);
				break; 
			}
			if (!actor->bDisplayed && displayed)
			{
				// this actor was not displayed so far but should be
				actor->bDisplayed = TRUE;
				// we also process additional changes, but no need to send notifications
				_update_all_pending_state (wactor, FALSE);
				
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_CREATED, actor);
				continue; /* check for other changes that might have happened while processing the above */
			}
			if (actor->bDisplayed && !displayed)
			{
				// should hide this actor
				actor->bDisplayed = FALSE;
				// we also process additional changes, but no need to send notifications
				_update_all_pending_state (wactor, FALSE);
				
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
				if (actor == s_pSelf) s_pSelf = NULL;
				if (actor == s_pActiveWindow) s_pActiveWindow = NULL;
				if (actor == s_pLastActiveWindow) s_pLastActiveWindow = NULL;
				if (actor == s_pMaybeActiveWindow) s_pMaybeActiveWindow = NULL;
				continue;
			}
			
			if (class_changed)
			{
				if (actor == s_pSelf) s_pSelf = NULL;
				if (!strcmp (actor->cClass, "cairo-dock")) s_pSelf = actor;
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_CLASS_CHANGED, actor, cOldClass, cOldWmClass);
				continue;
			}
			
			// check all remaining state 
			if (_update_all_pending_state (wactor, TRUE)) continue;
			
			// either one of the following two conditions will hold
			if (wactor->unfocused_pending)
			{
				// we set that we have no active window, but we do not send a notification
				// since another window might be activated as well
				if (s_pActiveWindow == actor)
				{
					s_pActiveWindow = NULL;
					bUnfocused = TRUE;
				}
				wactor->unfocused_pending = FALSE;
			}
			
			if (actor == s_pMaybeActiveWindow)
			{
				if (s_pActiveWindow != actor)
				{
					s_pActiveWindow = actor;
					s_pLastActiveWindow = actor;
					bUnfocused = FALSE;
					gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ACTIVATED, actor);
				}
				s_pMaybeActiveWindow = NULL;
				continue;
			}
			
			break;
		}
		
		/* possible leftover values from the last iteration */
		g_free(cOldClass);
		g_free(cOldWmClass);
		
		s_pCurrent = NULL;
		wactor = g_queue_pop_head (&s_pending_queue); // note: it is OK to call this on an empty queue
		
		if (!wactor)
		{
			// notify if the active window has been unfocused
			if (bUnfocused && !s_pActiveWindow)
				gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ACTIVATED, NULL);
			bUnfocused = FALSE;
			
			// re-check the queue in case we added more toplevels during the above notifications
			wactor = g_queue_pop_head (&s_pending_queue);
		}
	}
	
	s_bProcessing = FALSE;
}


static void _update_window_stack (void *ptr, G_GNUC_UNUSED void* dummy)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)ptr;
	GldiWindowActor *actor = (GldiWindowActor*)ptr;
	actor->iStackOrder = wactor->stacking_order_pending;
}

static gboolean _stack_change_notify (G_GNUC_UNUSED void* dummy)
{
	gldi_windows_foreach_unordered (_update_window_stack, NULL);
	gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_Z_ORDER_CHANGED, NULL);
	s_sidStackNotify = 0;
	return G_SOURCE_REMOVE;
}

void gldi_wayland_wm_notify_stack_change (void)
{
	if (!s_sidStackNotify) s_sidStackNotify = g_idle_add (_stack_change_notify, NULL);
}

static void _restack_windows (void* ptr, G_GNUC_UNUSED void* dummy)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)ptr;
	s_iStackCounter++;
	wactor->stacking_order_pending = s_iStackCounter;
}

static int _stack_compare (const void *x, const void *y)
{
	GldiWindowActor *actor1 = (GldiWindowActor*)x;
	GldiWindowActor *actor2 = (GldiWindowActor*)y;
	return (actor1->iStackOrder - actor2->iStackOrder);
}

void gldi_wayland_wm_stack_on_top (GldiWindowActor *actor)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)actor;
	
	if (s_iStackCounter == INT_MAX)
	{
		// need to re-start counting from zero
		s_iStackCounter = 0;
		GPtrArray *array = gldi_window_manager_get_all ();
		g_ptr_array_sort (array, _stack_compare);
		g_ptr_array_foreach (array, _restack_windows, NULL);
		g_ptr_array_free (array, TRUE); // TRUE: free the underlying memory used to hold the pointers
	}
	
	s_iStackCounter++;
	wactor->stacking_order_pending = s_iStackCounter;
	gldi_wayland_wm_notify_stack_change ();
}

GldiWindowActor* gldi_wayland_wm_get_active_window ()
{
	return s_pActiveWindow;
}

GldiWindowActor* gldi_wayland_wm_get_last_active_window ()
{
	return s_pLastActiveWindow;
}

GldiWaylandWindowActor* gldi_wayland_wm_new_toplevel (gpointer handle)
{
	return (GldiWaylandWindowActor*)gldi_object_new (&myWaylandWMObjectMgr, handle);
}

void gldi_wayland_wm_set_pre_notify_function (void (*func)(void))
{
	s_fNotify = func;
}


static void _init_object (GldiObject *obj, gpointer attr)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)obj;
	wactor->handle = attr;
	wactor->next_change_pending = NC_NO_CHANGE;
	GldiWindowActor *actor = (GldiWindowActor*)wactor;
	actor->iAge = s_iNumWindow;
	if (s_iNumWindow == INT_MAX) s_iNumWindow = 1;
	else s_iNumWindow++;
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


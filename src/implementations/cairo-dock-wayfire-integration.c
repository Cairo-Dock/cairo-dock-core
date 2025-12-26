/**
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

#include "gldi-config.h"
#include "cairo-dock-wayfire-integration.h"
#include "cairo-dock-wayland-wm.h"
#include "cairo-dock-dock-priv.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-visibility.h"
#include "cairo-dock-log.h"

#ifdef HAVE_JSON

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json.h>
#ifdef HAVE_EVDEV
#include <libevdev/libevdev.h>
#endif

#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"  // bIsHidden
#include "cairo-dock-icon-factory.h"  // pAppli
#include "cairo-dock-container-priv.h"  // gldi_container_get_gdk_window
#include "cairo-dock-class-manager-priv.h"
#include "cairo-dock-keybinder.h"


/***********************************************************************
 * private data, reading / writing and some housekeeping
 ***********************************************************************/

static const char default_socket[] = "/tmp/wayfire-wayland-1.socket";

static GIOChannel *s_pIOChannel = NULL; // GIOChannel encapsulating the above socket
static unsigned int s_sidIO = 0; // source ID for the above added to the main loop

struct cb_data {
	gpointer cb; // callback function (either CairoDockDesktopManagerActionResult or CairoDockGrabKeyResult)
	gpointer user_data;
	GldiShortkey *pBinding; // keybinding that is being added (if this callback belongs to a keybinding call)
};
GQueue s_cb_queue; // queue of callbacks for submitted actions -- contains cb_data structs or NULLs if an action does not have a callback

struct wf_keybinding
{
	guint keycode;
	guint modifiers;
	uint64_t id;
};
GList *s_pBindings = NULL; // list of registered keybindings (wf_keybinding structs)

// Helper function to signal failure and free one element of the queue
void _cb_fail (gpointer data)
{
	if (data)
	{
		struct cb_data *cb = (struct cb_data*)data;
		if (cb->pBinding)
		{
			cb->pBinding->bSuccess = FALSE;
			if (cb->cb) ((CairoDockGrabKeyResult) cb->cb) (cb->pBinding);
			gldi_object_unref (GLDI_OBJECT (cb->pBinding)); // ref was taken in _bind_key ()
		}
		else if (cb->cb)
			((CairoDockDesktopManagerActionResult) cb->cb) (FALSE, cb->user_data);
		g_free (cb);
	}
}

static void _vis_handle_broken_socket (void);
void _handle_broken_socket (void)
{
	if (s_sidIO) g_source_remove (s_sidIO); // will close the socket and free s_pIOChannel as well
	s_sidIO = 0;
	s_pIOChannel = NULL;
	g_queue_clear_full (&s_cb_queue, _cb_fail);
	g_list_free_full (s_pBindings, g_free);
	s_pBindings = NULL;
	_vis_handle_broken_socket ();
}

/*
 * Read a message from socket and return its contents or NULL if there
 * is no open socket. The caller should free() the returned string when done.
 * The length of the message is stored in msg_len (if not NULL).
 * Note: this will block until there is a message to read.
 */
static char* _read_msg_full (uint32_t* out_len, GIOChannel *pIOChannel)
{
	const size_t header_size = 4;
	char header[header_size];
	if (g_io_channel_read_chars (pIOChannel, header, header_size, NULL, NULL) != G_IO_STATUS_NORMAL)
	{
		//!! TODO: should this be a failure or is it possible to recover?
		cd_warning ("Error reading from Wayfire's socket");
		return NULL;
	}
	
	uint32_t msg_len;
	memcpy(&msg_len, header, header_size);
	gsize n_read = 0;
	char* msg = (char*)g_malloc (msg_len + 1);
	if (msg_len)
	{
		if (g_io_channel_read_chars (pIOChannel, msg, msg_len, &n_read, NULL) != G_IO_STATUS_NORMAL
			|| n_read != msg_len)
		{
			//!! TODO: should we care if the whole message cannot be read at once?
			cd_warning ("Error reading from Wayfire's socket");
			g_free (msg);
			return NULL;
		}
	}
	msg[msg_len] = 0;
	if (out_len) *out_len = msg_len;
	return msg;
}
/* same as above, but use our static variable */
static char* _read_msg (uint32_t* out_len)
{
	if (! s_pIOChannel) return NULL;
	return _read_msg_full (out_len, s_pIOChannel);
}

/* Send a message to the socket. Return 0 on success, -1 on failure */
static int _send_msg_full (const char* msg, uint32_t len, GIOChannel *pIOChannel)
{
	const size_t header_size = 4;
	gsize n_written = 0;
	char header[header_size];
	memcpy(header, &len, header_size);
	if ((g_io_channel_write_chars (pIOChannel, header, header_size, &n_written, NULL) != G_IO_STATUS_NORMAL) ||
		(g_io_channel_write_chars (pIOChannel, msg, len, &n_written, NULL) != G_IO_STATUS_NORMAL) ||
		n_written != len)
	{
		cd_warning ("Error writing to Wayfire's socket");
		return -1;
	}
	return 0;
}

/* Call a Wayfire IPC method and try to check if it was successful. */
static void _call_ipc (struct json_object* data, CairoDockDesktopManagerActionResult cb, gpointer user_data) {
	if (! s_pIOChannel)
	{
		// socket was already closed; we should not call _handle_broken_socket () again
		if (cb) cb (FALSE, user_data);
		return;
	}
	
	size_t len;
	const char *tmp = json_object_to_json_string_length (data, JSON_C_TO_STRING_SPACED, &len);
	if (!(tmp && len))
	{
		if (cb) cb (FALSE, user_data);
		return;
	}
	
	if (_send_msg_full (tmp, len, s_pIOChannel) < 0)
	{
		if (cb) cb (FALSE, user_data);
		_handle_broken_socket ();
		return;
	}
	
	// save the callback if provided
	if (cb)
	{
		struct cb_data *data = g_new0 (struct cb_data, 1);
		data->cb = cb;
		data->user_data = user_data;
		g_queue_push_tail (&s_cb_queue, data);
	}
	else g_queue_push_tail (&s_cb_queue, NULL); // dummy element
}

static void _call_ipc_method_no_data (const char *method)
{
	struct json_object *obj = json_object_new_object ();
	json_object_object_add (obj, "method", json_object_new_string (method));
	json_object_object_add (obj, "data", json_object_new_object ());
	_call_ipc (obj, NULL, NULL);
	json_object_put (obj);
}


/***********************************************************************
 * basic desktop manager functionality (scale, expo)
 ***********************************************************************/

/* Start scale on the current workspace */
static void _present_windows() {
	_call_ipc_method_no_data ("scale/toggle");
}

/* Start scale including all views of the given class */
static void _present_class(const gchar *cClass, CairoDockDesktopManagerActionResult cb, gpointer user_data) {
	const gchar *cWmClass = cairo_dock_get_class_wm_class (cClass);
	if (cWmClass) 
	{
		struct json_object *obj = json_object_new_object ();
		json_object_object_add (obj, "method", json_object_new_string ("scale_ipc_filter/activate_appid"));
		struct json_object *data = json_object_new_object ();
		json_object_object_add (data, "all_workspaces", json_object_new_boolean (1));
		json_object_object_add (data, "app_id", json_object_new_string (cWmClass));
		json_object_object_add (obj, "data", data);
		_call_ipc (obj, cb, user_data);
		json_object_put (obj);
	}
	else if (cb) cb (FALSE, user_data);
}


/* Start expo on the current output */
static void _present_desktops() {
	_call_ipc_method_no_data ("expo/toggle");
}

/* Toggle show desktop functionality (i.e. minimize / unminimize all views).
 * Note: bShow argument is ignored, we don't know if the desktop is shown / hidden */
static void _show_hide_desktop(G_GNUC_UNUSED gboolean bShow) {
	_call_ipc_method_no_data ("wm-actions/toggle_showdesktop");
}


/***********************************************************************
 * keybindings
 ***********************************************************************/

static void _binding_registered (gboolean bSuccess, GldiShortkey *pBinding, CairoDockGrabKeyResult cb, uint64_t id)
{
	pBinding->bSuccess = bSuccess;
	
	if (bSuccess)
	{
		struct wf_keybinding *binding = g_new (struct wf_keybinding, 1);
		binding->keycode = pBinding->keycode;
		binding->modifiers = pBinding->modifiers;
		binding->id = id;
	
		s_pBindings = g_list_prepend (s_pBindings, binding);
	}
	
	if (cb) cb (pBinding);
	gldi_object_unref (GLDI_OBJECT (pBinding)); // ref was taken in _bind_key () -- note that this might immediately unbind if was unrefed outside
}

#ifdef HAVE_EVDEV
static void _bind_key (GldiShortkey *pBinding, gboolean grab, CairoDockGrabKeyResult cb)
{
	if (! s_pIOChannel)
	{
		// socket was already closed; we should not call _handle_broken_socket () again
		// (in this case, our binding list should be empty as well)
		pBinding->bSuccess = FALSE;
		if (cb) cb (pBinding);
		return;
	}
	
	guint keycode = pBinding->keycode;
	guint modifiers = pBinding->modifiers;
	
	if (grab)
	{
		const char *name = libevdev_event_code_get_name (EV_KEY, keycode - 8); // -8: difference between X11 and Linux hardware keycodes
		if (!name)
		{
			cd_warning ("Unknown keycode: %u", keycode);
			pBinding->bSuccess = FALSE;
			if (cb) cb (pBinding);
			return;
		}
		
		GString *str = g_string_sized_new (40); // all modifiers + some long key name
		
		// first process modifiers (note: GDK_SUPER_MASK and GDK_META_MASK have been removed by the caller)
		if (modifiers & GDK_CONTROL_MASK) g_string_append (str, "<ctrl>");
		if (modifiers & GDK_MOD1_MASK) g_string_append (str, "<alt>");
		if (modifiers & GDK_SHIFT_MASK) g_string_append (str, "<shift>");
		if (modifiers & GDK_MOD4_MASK) g_string_append (str, "<super>");
		
		g_string_append (str, name);
		
		cd_debug ("Trying to register binding: %u, %u -- %s", keycode, modifiers, str->str);
		
		struct json_object *obj = json_object_new_object ();
		json_object_object_add (obj, "method", json_object_new_string ("command/register-binding"));
		struct json_object *data = json_object_new_object ();
		json_object_object_add (data, "binding", json_object_new_string_len (str->str, str->len));
		json_object_object_add (data, "exec-always", json_object_new_boolean (1)); // unsure, maybe FALSE would be better?
		json_object_object_add (obj, "data", data);
		
		size_t len;
		const char *tmp = json_object_to_json_string_length (obj, JSON_C_TO_STRING_SPACED, &len);
		if (!(tmp && len))
		{
			json_object_put (obj);
			g_string_free (str, TRUE);
			pBinding->bSuccess = FALSE;
			if (cb) cb (pBinding);
			return;
		}
		
		int ret = _send_msg_full (tmp, len, s_pIOChannel);
		json_object_put (obj);
		g_string_free (str, TRUE);
		
		if (ret < 0)
		{
			pBinding->bSuccess = FALSE;
			if (cb) cb (pBinding);
			_handle_broken_socket ();
			return;
		}
		
		// add a callback
		struct cb_data *cb1 = g_new0 (struct cb_data, 1);
		cb1->pBinding = pBinding;
		cb1->cb = cb;
		gldi_object_ref (GLDI_OBJECT (pBinding)); // need to keep a reference in case the caller destroys it before our callback
		g_queue_push_tail (&s_cb_queue, cb1);
	}
	else if (pBinding->bSuccess) // only try to remove if it was successfully registered
	{
		GList *it = s_pBindings;
		while (it)
		{
			struct wf_keybinding *binding = (struct wf_keybinding*)it->data;
			
			if (binding->keycode == keycode && binding->modifiers == modifiers)
			{
				struct json_object *obj = json_object_new_object ();
				json_object_object_add (obj, "method", json_object_new_string ("command/unregister-binding"));
				struct json_object *data = json_object_new_object ();
				json_object_object_add (data, "binding-id", json_object_new_uint64 (binding->id));
				json_object_object_add (obj, "data", data);
				_call_ipc (obj, NULL, NULL); // we don't care about the result in this case
				json_object_put (obj);
				
				s_pBindings = g_list_delete_link (s_pBindings, it);
				g_free (binding);
				break;
			}
			
			it = it->next;
		}
		
		if (!it) cd_warning ("Cannot find binding to unregister");
	}
}
#endif


/***********************************************************************
 * watch extra view state
 ***********************************************************************/

static void _start_watch_events (void);

// whether we are watching sticky events
static gboolean s_bHaveAbove = FALSE; // whether Wayfire provides the "view-above" event
static gboolean s_bCanSticky = FALSE;
static gboolean s_bCanAbove = FALSE;
static gboolean s_bNeedStickyAbove = FALSE; // toplevel manager requested the above functionality

static gboolean s_bWatchWMEvents = FALSE;
// hash table mapping view IPC IDs to window actors
static GHashTable *s_pActorMap = NULL; // key: IPC ID, value: GldiWaylandWindowActor*

// extra data to store with views (in theory, we could stuff this into a 64-bit int, but cleaner to allocate an extra struct)
typedef struct _wf_view_data {
	uint32_t id; // IPC ID of this view
	gboolean bAbove; // whether this view is always on top
} wf_view_data;

// function to free view data in s_pActorMap
static void _reset_view_data (gpointer data)
{
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)data;
	if (!wactor->pExtraData) return;
	g_free (wactor->pExtraData);
	wactor->pExtraData = NULL;
}

static gboolean _wf_on_window_created (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	if (!(s_bCanAbove || s_bCanSticky)) return GLDI_NOTIFICATION_LET_PASS;
	if (!GLDI_OBJECT_IS_WAYLAND_WINDOW (actor)) return GLDI_NOTIFICATION_LET_PASS;
	
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)actor;
	if (!wactor->cClassExtra) return GLDI_NOTIFICATION_LET_PASS;
	
	const char *tmp = strstr (wactor->cClassExtra, "wf-ipc-");
	if (tmp)
	{
		tmp += 7; // strlen ("wf-ipc-")
		if (*tmp)
		{
			const char *end = NULL;
			errno = 0;
			uint32_t id = (uint32_t)strtoul (tmp, (char**)&end, 10);
			if (errno == 0 && end != tmp)
			{
				// valid ID
				wf_view_data *pData = g_new0 (wf_view_data, 1);
				pData->id = id;
				wactor->pExtraData = pData; // pExtraData should be NULL here
				g_hash_table_insert (s_pActorMap, GUINT_TO_POINTER (id), wactor); // note: will invalidate any previous value
			}
			else cd_warning ("Error parsing IPC ID: %s", tmp - 7);
		}
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _wf_on_window_destroyed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	if (!(s_bCanAbove || s_bCanSticky)) return GLDI_NOTIFICATION_LET_PASS;
	if (!GLDI_OBJECT_IS_WAYLAND_WINDOW (actor)) return GLDI_NOTIFICATION_LET_PASS;
	
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)actor;
	if (!wactor->pExtraData) return GLDI_NOTIFICATION_LET_PASS;
	
	wf_view_data *pData = (wf_view_data*)wactor->pExtraData;
	wactor->pExtraData = NULL;
	g_hash_table_remove (s_pActorMap, GUINT_TO_POINTER (pData->id));
	g_free (pData);
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _wf_on_window_class_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor,
	G_GNUC_UNUSED const gchar *cOldClass, G_GNUC_UNUSED const gchar *cOldWmClass)
{
	_wf_on_window_destroyed (NULL, actor);
	_wf_on_window_created (NULL, actor);
	
	return GLDI_NOTIFICATION_LET_PASS;
}


static void _add_existing_actor (gpointer ptr, G_GNUC_UNUSED gpointer data)
{
	_wf_on_window_created (NULL, (GldiWindowActor*)ptr);
}

static void _init_sticky_above (void)
{
	if (! (s_bCanAbove || s_bCanSticky)) return;
	if (s_bWatchWMEvents) return; // already started
	
	s_pActorMap = g_hash_table_new_full (NULL, NULL, NULL, _reset_view_data);
	
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_CREATED,
		(GldiNotificationFunc) _wf_on_window_created,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_DESTROYED,
		(GldiNotificationFunc) _wf_on_window_destroyed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_CLASS_CHANGED,
		(GldiNotificationFunc) _wf_on_window_class_changed,
		GLDI_RUN_FIRST, NULL);
	
	// Add all existing views
	gldi_windows_foreach_unordered (_add_existing_actor, NULL);
	
	// Start listening to events to track the actual state
	s_bWatchWMEvents = TRUE;
	_start_watch_events ();
}


void gldi_wf_init_sticky_above (void)
{
	if (s_bCanAbove || s_bCanSticky) _init_sticky_above ();
	else s_bNeedStickyAbove = TRUE;
}

void gldi_wf_can_sticky_above (gboolean *bCanSticky, gboolean *bCanAbove)
{
	if (bCanSticky) *bCanSticky = s_bCanSticky && s_bWatchWMEvents;
	if (bCanAbove) *bCanAbove = s_bCanAbove && s_bWatchWMEvents;
}

void gldi_wf_set_sticky (GldiWindowActor *actor, gboolean bSticky)
{
	if (! (s_bCanSticky && s_bWatchWMEvents)) return;
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)actor;
	if (!wactor->pExtraData) return;
	
	wf_view_data *pData = (wf_view_data*)wactor->pExtraData;
	uint32_t id = pData->id;
	
	struct json_object *obj = json_object_new_object ();
	json_object_object_add (obj, "method", json_object_new_string ("wm-actions/set-sticky"));
	struct json_object *data = json_object_new_object ();
	json_object_object_add (data, "state", json_object_new_boolean (!!bSticky));
	json_object_object_add (data, "view-id", json_object_new_uint64 (id));
	json_object_object_add (obj, "data", data);
	_call_ipc (obj, NULL, NULL); // no callback -- we will be notified of a state change
	json_object_put (obj);
}

static void _set_above_cb (gboolean bSuccess, gpointer ptr)
{
	if (bSuccess)
	{
		// let's try to set the bAbove for this view
		wf_view_data *pData = (wf_view_data*)ptr;
		GldiWaylandWindowActor *wactor = g_hash_table_lookup (s_pActorMap, GUINT_TO_POINTER (pData->id));
		if (wactor && wactor->pExtraData)
		{
			wf_view_data *pActorData = (wf_view_data*)wactor->pExtraData;
			pActorData->bAbove = pData->bAbove;
		}
	}
	g_free (ptr);
}

void gldi_wf_set_above (GldiWindowActor *actor, gboolean bAbove)
{
	if (! (s_bCanAbove && s_bWatchWMEvents)) return;
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)actor;
	if (!wactor->pExtraData) return;
	
	wf_view_data *pData = (wf_view_data*)wactor->pExtraData;
	uint32_t id = pData->id;
	
	struct json_object *obj = json_object_new_object ();
	json_object_object_add (obj, "method", json_object_new_string ("wm-actions/set-always-on-top"));
	struct json_object *data = json_object_new_object ();
	json_object_object_add (data, "state", json_object_new_boolean (!!bAbove));
	json_object_object_add (data, "view-id", json_object_new_uint64 (id));
	json_object_object_add (obj, "data", data);
	
	wf_view_data *cbdata = NULL;
	if (!s_bHaveAbove)
	{
		cbdata = g_new0 (wf_view_data, 1);
		cbdata->id = id;
		cbdata->bAbove = bAbove;
	}
	_call_ipc (obj, cbdata ? _set_above_cb : NULL, cbdata); // we give the ID as the parameter in case wactor is destroyed before the callback
	json_object_put (obj);
}

void gldi_wf_is_above_or_below (GldiWindowActor *actor, gboolean *bIsAbove, gboolean *bIsBelow)
{
	if (bIsBelow) *bIsBelow = FALSE;
	if (bIsAbove) *bIsAbove = FALSE;
	else return;
	
	GldiWaylandWindowActor *wactor = (GldiWaylandWindowActor*)actor;
	if (!wactor->pExtraData) return;
	
	wf_view_data *pData = (wf_view_data*)wactor->pExtraData;
	*bIsAbove = pData->bAbove;
}

/***********************************************************************
 * dock visibility backend
 ***********************************************************************/

// private data

// whether we are watching events for any dock (dock visibility)
static gboolean s_bWatchVisEvents = FALSE;
// whether we registered a watch (we can only do this once)
static gboolean s_bWatchRegistered = FALSE;
// whether we received Wayfire's configuration
static gboolean s_bGotConfig = FALSE;
// whether we received the initial list of views already
static gboolean s_bListViewsDone = FALSE;
// hash table tracking window positions (note: we track positions
// separately, so dock visibility tracking will work even if we
// don't receive IPC IDs)
typedef struct _wf_window_pos {
	GdkRectangle rect;
	uint32_t output_id;
} wf_window_pos;
static GHashTable *s_pWindowPos = NULL; // key: IPC ID, value: wf_window_pos*
static uint32_t s_last_active = (uint32_t)-1; // last known active view (-1 means none)

// data attached to each dock
typedef struct _wf_vis_data {
	gboolean bListening; // whether we are keeping track of this dock's overlaps
	int x, y; // note: can be negative (although not expected)
	int w, h;
	uint32_t output_id; // ID of the output the dock is on
} wf_vis_data;

static gboolean _free_vis_data_for_dock (G_GNUC_UNUSED gpointer data, GldiObject *pObj)
{
	CairoDock *pDock = CAIRO_DOCK (pObj);
	if (pDock->pVisibilityData) g_free (pDock->pVisibilityData);
	pDock->pVisibilityData = NULL; // need to set to NULL, since our refresh () function may still be called
	return GLDI_NOTIFICATION_LET_PASS;
}

// get the visibility data for this dock, or allocate it if not yet available
static wf_vis_data *_get_dock_vis_data (CairoDock *pDock)
{
	if (pDock->pVisibilityData) return (wf_vis_data*) pDock->pVisibilityData;
	
	wf_vis_data *data = g_new0 (wf_vis_data, 1);
	pDock->pVisibilityData = data;
	
	// add a weak ref to free the data when this dock is destroyed
	gldi_object_register_notification (GLDI_OBJECT (pDock), NOTIFICATION_DESTROY,
		(GldiNotificationFunc) _free_vis_data_for_dock, FALSE, NULL);
	
	return data;
}

static void _free_vis_data_and_show (CairoDock *pDock, G_GNUC_UNUSED void *ptr)
{
	if (pDock->pVisibilityData)
	{
		gldi_object_remove_notification (GLDI_OBJECT (pDock), NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _free_vis_data_for_dock, NULL);
		g_free (pDock->pVisibilityData);
		pDock->pVisibilityData = NULL;
	}
	if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP ||
		pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
	{
		// avoid the dock staying hidden in this case
		if (cairo_dock_is_temporary_hidden (pDock))
			cairo_dock_deactivate_temporary_auto_hide (pDock);
	}
}

static void _reset_dock_vis_data (CairoDock *pDock, G_GNUC_UNUSED void *ptr)
{
	if (pDock->pVisibilityData)
	{
		wf_vis_data* data = (wf_vis_data*)pDock->pVisibilityData;
		data->w = -1; // will be ignored if we restart monitoring until
		data->h = -1; // we get valid coordinates again
	}
}

static void _visibility_stop (void)
{
	s_bWatchVisEvents = FALSE; // any remaining incoming events will be discarded
	if (!s_bWatchWMEvents) s_bListViewsDone = FALSE;
	gldi_docks_foreach_root ((GFunc)_reset_dock_vis_data, NULL);
	g_hash_table_remove_all (s_pWindowPos);
}


// Check if a dock overlaps with a given rectangle.
// Caller should ensure that pDock->pVisibilityData != NULL
static gboolean _check_overlap (CairoDock *pDock, wf_window_pos *pWindowPos)
{
	wf_vis_data *dock_rect = (wf_vis_data*) pDock->pVisibilityData;
	if (dock_rect->w < 0 || dock_rect->h < 0) return FALSE; // we are not monitoring this dock
	if (dock_rect->output_id != pWindowPos->output_id) return FALSE; // not on the same ouput
	
	int dx, dy, dw, dh;
	if (pDock->container.bIsHorizontal)
	{
		dw = pDock->iMinDockWidth;
		dh = pDock->iMinDockHeight;
		dx = (pDock->container.iWidth - pDock->iMinDockWidth)/2;
		dy = (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iMinDockHeight : 0);
	}
	else
	{
		dw = pDock->iMinDockHeight;
		dh = pDock->iMinDockWidth;
		dx = (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iMinDockHeight : 0);
		dy = (pDock->container.iWidth - pDock->iMinDockWidth)/2;
	}
	
	dx += dock_rect->x;
	dy += dock_rect->y;
	
	GdkRectangle *pArea = &(pWindowPos->rect);
	return ((pArea->x < dx + dw) && (pArea->x + pArea->width > dx) &&
		(pArea->y < dy + dh) && (pArea->y + pArea->height > dy));
}

static gboolean _check_overlap2 (G_GNUC_UNUSED gpointer key, gpointer value, gpointer user_data)
{
	return _check_overlap ((CairoDock*)user_data, (wf_window_pos*)value);
}

static void _wf_hide_show_if_on_our_way (CairoDock *pDock, wf_window_pos *pRect)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP)
		return ;
	
	gboolean bShow = TRUE;
	if (pRect && pDock->pVisibilityData) bShow = ! _check_overlap (pDock, pRect);
	
	if (bShow)
	{
		if (cairo_dock_is_temporary_hidden (pDock))
			cairo_dock_deactivate_temporary_auto_hide (pDock);
	}
	else if (!cairo_dock_is_temporary_hidden (pDock))
		cairo_dock_activate_temporary_auto_hide (pDock);
}

static gboolean _wf_dock_has_overlapping_window (CairoDock *pDock)
{
	return g_hash_table_find (s_pWindowPos, _check_overlap2, pDock) != NULL;
}

static void _wf_hide_if_overlap_or_show_if_no_overlapping_window (CairoDock *pDock, gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	
	wf_window_pos *pRect = (wf_window_pos*)data;
	
	if (pRect && _check_overlap (pDock, pRect))
	{
		// dock should be hidden
		if (!cairo_dock_is_temporary_hidden (pDock))
			cairo_dock_activate_temporary_auto_hide (pDock);
	}
	else if (cairo_dock_is_temporary_hidden (pDock))
	{
		// dock should be shown if it does not overlap with any other window
		if (!_wf_dock_has_overlapping_window (pDock))
			cairo_dock_deactivate_temporary_auto_hide (pDock);
	}
}

static void _wf_hide_if_any_overlap_or_show (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (cairo_dock_is_temporary_hidden (pDock))
	{
		if (!_wf_dock_has_overlapping_window (pDock))
		{
			cairo_dock_deactivate_temporary_auto_hide (pDock);
		}
	}
	else
	{
		if (_wf_dock_has_overlapping_window (pDock))
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
}


static void _check_should_listen_dock (CairoDock *pDock, void *ptr)
{
	gboolean *pShouldListen = (gboolean*)ptr;
	*pShouldListen |= (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP ||
		pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY);
}

static void _vis_handle_broken_socket (void)
{
	if (s_bWatchWMEvents)
	{
		gldi_object_remove_notification (&myWindowObjectMgr, NOTIFICATION_WINDOW_CREATED,
			(GldiNotificationFunc) _wf_on_window_created, NULL);
		gldi_object_remove_notification (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESTROYED,
			(GldiNotificationFunc) _wf_on_window_destroyed, NULL);
		gldi_object_remove_notification (&myWindowObjectMgr, NOTIFICATION_WINDOW_CLASS_CHANGED,
			(GldiNotificationFunc) _wf_on_window_class_changed, NULL);
	}
	
	s_bWatchRegistered = FALSE;
	s_bWatchVisEvents = FALSE;
	s_bWatchWMEvents = FALSE;
	s_bListViewsDone = FALSE;
	gldi_docks_foreach_root ((GFunc)_free_vis_data_and_show, NULL);
	g_hash_table_remove_all (s_pWindowPos);
	g_hash_table_remove_all (s_pActorMap);
}

static void _add_vis_watch_cb (gboolean bSuccess, G_GNUC_UNUSED gpointer user_data);
static void _list_views_cb (const struct json_object *arr);
static void _process_view (const gchar *event, const struct json_object *view);
static void _got_configuration (struct json_object *conf);
static void _visibility_refresh (CairoDock *pDock)
{
	gboolean bShouldListen = (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP ||
		pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY);
	
	if (! bShouldListen)
	{
		if (pDock->pVisibilityData) ((wf_vis_data*)pDock->pVisibilityData)->bListening = FALSE;
		
		// re-check all root docks (can be the case when pDock is being destroyed,
		// so pVisibilityData == NULL for it already)
		gldi_docks_foreach_root ((GFunc)_check_should_listen_dock, &bShouldListen);
		if (! bShouldListen && s_bWatchVisEvents) _visibility_stop ();
		return;
	}
	
	// in this case, we need to listen to events from this dock
	wf_vis_data *data = _get_dock_vis_data (pDock);
	data->bListening = TRUE;
	if (s_bWatchVisEvents) // already listening, but need to re-check this dock
	{
		if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
			_wf_hide_if_any_overlap_or_show (pDock, NULL);
		else
		{
			// hide if overlap with the active window
			if (s_last_active == (uint32_t)-1)
			{
				if (cairo_dock_is_temporary_hidden (pDock))
					cairo_dock_deactivate_temporary_auto_hide (pDock);
			}
			else
			{
				wf_window_pos *pRect = g_hash_table_lookup (s_pWindowPos, GUINT_TO_POINTER (s_last_active));
				if (!pRect) cd_warning ("Cannot find the active window's coordinates!"); // should not happen
				else _wf_hide_show_if_on_our_way (pDock, pRect);
			}
		}
		return;
	}
	
	// Start listening to events now
	// ensure that we have a valid hash table
	if (! s_pWindowPos)
	{
		s_pWindowPos = g_hash_table_new_full (
			NULL, // hash func -- use g_direct_hash (keys are just integers)
			NULL, // key equal func -- use direct comparison
			NULL, // key destroy notify
			g_free // value destroy notify
		);
	}
	
	s_bWatchVisEvents = TRUE;
	_start_watch_events ();
}

static void _start_watch_events (void)
{
	// start listening to Wayfire's events
	if (s_bWatchRegistered)
	{
		if (s_bGotConfig) _add_vis_watch_cb (TRUE, NULL); // otherwise, we'll handle this from _got_configuration ()
		return;
	}
	
	s_bWatchRegistered = TRUE; // we can only try once
	
	// check first whether the view-above event is available
	const gchar *req = "{\"method\": \"wayfire/configuration\", \"data\": { } }";
	int ret = _send_msg_full (req, strlen (req), s_pIOChannel);
	if (ret < 0)
	{
		// socket error, abort
		_handle_broken_socket ();
		return;
	}
	
	// add a callback to check if we succeeded
	struct cb_data *cb1 = g_new0 (struct cb_data, 1);
	cb1->cb = _got_configuration;
	g_queue_push_tail (&s_cb_queue, cb1);
}
	
static void _got_configuration (struct json_object *conf)
{	
	struct json_object *version = json_object_object_get (conf, "api-version");
	if (version)
	{
		uint64_t ver = json_object_get_uint64 (version);
		s_bHaveAbove = (ver >= 20251226);
	}
	else cd_warning ("Cannot parse Wayfire version!");
	
	s_bGotConfig = TRUE;
	
	const gchar *watch_req =
	s_bHaveAbove ?
"{\"method\": \"window-rules/events/watch\", \"data\": {"
"\"events\": [\"view-focused\", \"view-geometry-changed\", \"view-minimized\","
"\"view-mapped\", \"view-unmapped\", \"view-sticky\", \"view-always-on-top\"] } }" :
"{\"method\": \"window-rules/events/watch\", \"data\": {"
"\"events\": [\"view-focused\", \"view-geometry-changed\", \"view-minimized\","
"\"view-mapped\", \"view-unmapped\", \"view-sticky\"] } }";

	int ret = _send_msg_full (watch_req, strlen (watch_req), s_pIOChannel);
	if (ret < 0)
	{
		// socket error, abort
		_handle_broken_socket ();
		return;
	}
	
	// add a callback to check if we succeeded
	struct cb_data *cb1 = g_new0 (struct cb_data, 1);
	cb1->cb = _add_vis_watch_cb;
	g_queue_push_tail (&s_cb_queue, cb1);
}

static void _add_vis_watch_cb (gboolean bSuccess, G_GNUC_UNUSED gpointer user_data)
{
	if (!bSuccess)
	{
		cd_warning ("Cannot add Wayfire event watch");
		return;
	}
	
	if (! (s_bWatchVisEvents || s_bWatchWMEvents)) return; // if cancelled in the meantime
	
	// now we are watching events, but we need to call list-views, since
	// we may not get events about all of them
	const gchar *list_req = "{\"method\": \"window-rules/list-views\", \"data\": {} }";
	int ret = _send_msg_full (list_req, strlen (list_req), s_pIOChannel);
	if (ret < 0)
	{
		// socket error, abort
		_handle_broken_socket ();
		return;
	}
	
	// add a special callback -- need to be careful as this call will return a json array instead of the usual dict
	struct cb_data *cb1 = g_new0 (struct cb_data, 1);
	cb1->cb = _list_views_cb;
	g_queue_push_tail (&s_cb_queue, cb1);
}

static void _list_views_cb (const struct json_object *arr)
{
	if (! (s_bWatchVisEvents || s_bWatchWMEvents)) return; // if cancelled before getting results
	
	size_t i;
	size_t len = json_object_array_length (arr);
	for (i = 0; i < len; i++)
	{
		struct json_object *tmp = json_object_array_get_idx (arr, i);
		if (tmp) _process_view (NULL, tmp);
	}
	
	if (s_bWatchVisEvents)
	{
		gldi_docks_foreach_root ((GFunc)_wf_hide_if_any_overlap_or_show, NULL);
		wf_window_pos *pRect = (s_last_active != (uint32_t)-1) ? g_hash_table_lookup (s_pWindowPos, GUINT_TO_POINTER (s_last_active)) : NULL;
		if (pRect) gldi_docks_foreach_root ((GFunc)_wf_hide_show_if_on_our_way, pRect);
	}
	
	s_bListViewsDone = TRUE;
}


static gboolean _get_int_value (const struct json_object *obj, const gchar *key, int32_t *res)
{
	const struct json_object *tmp = json_object_object_get (obj, key);
	if (!tmp || ! json_object_is_type (tmp, json_type_int)) return FALSE;
	errno = 0;
	int64_t res2 = json_object_get_int64 (tmp);
	if (errno || res2 > G_MAXINT32) return FALSE;
	*res = (int32_t) res2;
	return TRUE;
}

static gboolean _get_view_geometry (const struct json_object *view, wf_window_pos *rect)
{
	const struct json_object *tmp = json_object_object_get (view, "output-id");
	if (!tmp || !json_object_is_type (tmp, json_type_int)) return FALSE;
	
	errno = 0;
	rect->output_id = (uint32_t)json_object_get_uint64 (tmp); // note: Wayfire IPC IDs are 32-bit
	if (errno) return FALSE;
	
	tmp = json_object_object_get (view, "geometry");
	if (!tmp || ! json_object_is_type (tmp, json_type_object))
		return FALSE;
	
	return (_get_int_value (tmp, "x", &rect->rect.x) && _get_int_value (tmp, "y", &rect->rect.y) &&
		_get_int_value (tmp, "width", &rect->rect.width) && _get_int_value (tmp, "height", &rect->rect.height));
}

// Process a view, either because of an event, or from the initial reply to
// list-views (in this case, event == NULL).
// Note: view can be NULL for some events (e.g. focus change with no newly focused view)
static void _process_view (const gchar *event, const struct json_object *view)
{
	if (! (s_bWatchVisEvents || s_bWatchWMEvents)) return; // if we are not listening (cannot unregister watch, so we will keep getting events)
	
	{
		struct json_object *tmp = view ? json_object_object_get (view, "app-id") : NULL;
		cd_debug ("event: %s, app-id: %s", event, tmp ? json_object_get_string (tmp) : "(none)");
	}
	
	// check if this is the active view losing focus
	if (!view)
	{
		// in this case event != NULL
		if (!strcmp (event, "view-focused"))
		{
			if (s_bListViewsDone && s_bWatchVisEvents) gldi_docks_foreach_root ((GFunc)_wf_hide_show_if_on_our_way, NULL);
			s_last_active = (uint32_t)-1;
		}
		else cd_warning ("Unexpected event with no view data: %s", event);
		return;
	}
	
	// first, get the view's ID -- Wayfire uses 32-bit integers for now
	uint32_t id;
	errno = 0;
	struct json_object *tmp = json_object_object_get (view, "id");
	if (tmp) id = (uint32_t) json_object_get_uint64 (tmp);
	else errno = 1;
	if (errno)
	{
		cd_warning ("Cannot parse view ID");
		return; // TODO: should we cancel?
	}
	
	gboolean bDelete = event && ! strcmp (event, "view-unmapped");
	if (! bDelete)
	{
		// also can be true if a view was minimized
		tmp = json_object_object_get (view, "minimized");
		if (tmp && json_object_is_type (tmp, json_type_boolean) && json_object_get_boolean (tmp))
			bDelete = TRUE;
	}
	
	if (bDelete)
	{
		gboolean bWasActive = (id == s_last_active);
		if (bWasActive) s_last_active = (uint32_t)-1;
		
		if (s_bListViewsDone && s_bWatchVisEvents)
		{
			if (bWasActive) gldi_docks_foreach_root ((GFunc)_wf_hide_show_if_on_our_way, NULL);
			gldi_docks_foreach_root ((GFunc)_wf_hide_if_overlap_or_show_if_no_overlapping_window, NULL);
		}
		if (s_bWatchVisEvents) g_hash_table_remove (s_pWindowPos, GUINT_TO_POINTER (id));
		return;
	}
	
	// add or update this view -- only if it is interesting to us
	// first, check if it is mapped (we don't care about unmapped views)
	tmp = json_object_object_get (view, "mapped");
	if (!tmp || ! json_object_is_type (tmp, json_type_boolean))
	{
		cd_warning ("No 'mapped' property for view %u", id);
		return;
	}
	if (! json_object_get_boolean (tmp)) return; // nothing else to do
	
	tmp = json_object_object_get (view, "type");
	const char *str = tmp ? json_object_get_string (tmp) : NULL;
	if (!str)
	{
		cd_warning ("Cannot get type for view %u", id);
		return;
	}
	if (!strcmp (str, "toplevel"))
	{
		// normal toplevel view (including XWayland views as well)
		
		if (s_bWatchVisEvents)
		{
			// get and update the view's geometry
			wf_window_pos rect;
			gboolean bGeomChanged = FALSE;
			if (! _get_view_geometry (view, &rect))
			{
				cd_warning ("Cannot parse geometry for view %u", id);
				return;
			}
			
			wf_window_pos *pRect = g_hash_table_lookup (s_pWindowPos, GUINT_TO_POINTER (id));
			if (!pRect)
			{
				pRect = g_new (wf_window_pos, 1);
				g_hash_table_insert (s_pWindowPos, GUINT_TO_POINTER (id), pRect);
				bGeomChanged = TRUE;
			}
			else bGeomChanged = (pRect->rect.x != rect.rect.x) || (pRect->rect.y != rect.rect.y) ||
				(pRect->rect.width != rect.rect.width) || (pRect->rect.height != rect.rect.height) ||
				(pRect->output_id != rect.output_id);
			if (bGeomChanged)
			{
				pRect->rect.x = rect.rect.x;
				pRect->rect.y = rect.rect.y;
				pRect->rect.width  = rect.rect.width;
				pRect->rect.height = rect.rect.height;
				pRect->output_id   = rect.output_id;
			}
			
			// check if this is the active window
			tmp = json_object_object_get (view, "activated");
			if (!tmp || ! json_object_is_type (tmp, json_type_boolean))
			{
				cd_warning ("No 'activated' property for view %u", id);
				return;
			}
			gboolean bActive = json_object_get_boolean (tmp);
			
			if (bActive && (s_last_active != id || bGeomChanged))
			{
				s_last_active = id;
				if (s_bListViewsDone) gldi_docks_foreach_root ((GFunc)_wf_hide_show_if_on_our_way, pRect);
			}
			if (s_bListViewsDone && bGeomChanged)
				gldi_docks_foreach_root ((GFunc)_wf_hide_if_overlap_or_show_if_no_overlapping_window, pRect);
		}
		
		if (s_bWatchWMEvents)
		{
			gboolean bInitialState = !event || !strcmp (event, "view-mapped");
			gboolean bStickyChanged = !bInitialState && event && !strcmp (event, "view-sticky");
			gboolean bAboveChanged  = !bInitialState && event && !strcmp (event, "view-always-on-top");
			
			if (bInitialState || bStickyChanged || bAboveChanged)
			{
				// note: s_pActorMap != NULL if s_bWatchWMEvents == TRUE
				GldiWaylandWindowActor *wactor = g_hash_table_lookup (s_pActorMap, GUINT_TO_POINTER (id));
				if (wactor)
				{
					//!! TODO: if we have not received the WINDOW_CREATED signal yet, we cannot set these !!
					if (bInitialState || bStickyChanged)
					{
						tmp = json_object_object_get (view, "sticky");
						if (!tmp || ! json_object_is_type (tmp, json_type_boolean))
							cd_warning ("No 'sticky' property for view %u", id);
						else gldi_wayland_wm_sticky_changed (wactor, json_object_get_boolean (tmp), TRUE);
					}
					if ((bInitialState || bAboveChanged) && wactor->pExtraData && s_bHaveAbove)
					{
						wf_view_data *pData = (wf_view_data*)wactor->pExtraData;
						
						tmp = json_object_object_get (view, "always-on-top");
						if (!tmp || ! json_object_is_type (tmp, json_type_boolean))
							cd_warning ("No 'always-on-top' property for view %u", id);
						else pData->bAbove = json_object_get_boolean (tmp);
					}
				}
			}
		}
	}
	else if (! (strcmp (str, "background") && strcmp (str, "panel") && strcmp (str, "overlay")))
	{
		// a layer-shell view
		tmp = json_object_object_get (view, "app-id");
		if (!tmp  || ! json_object_is_type (tmp, json_type_string))
		{
			cd_warning ("No app-id for view %u", id);
			return;
		}
		str = json_object_get_string (tmp);
		if (! strncmp (str, "cairo-dock-", 11))
		{
			// this is one of our docks
			CairoDock *pDock = gldi_dock_get (str + 11);
			// not an error if it does not exist, we could have just
			// removed it before getting this message
			// (or there could be another Cairo-Dock process running)
			if (pDock && pDock->iRefCount == 0)
			{
				wf_vis_data *data = _get_dock_vis_data (pDock);
				
				wf_window_pos rect;
				if (! _get_view_geometry (view, &rect))
				{
					cd_warning ("Cannot parse geometry for view %u", id);
					return;
				}
				
				data->x = rect.rect.x;
				data->y = rect.rect.y;
				data->w = rect.rect.width;
				data->h = rect.rect.height;
				data->output_id = rect.output_id;
				
				if (data->bListening && s_bListViewsDone)
				{
					if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP)
					{
						// we only need to test if we have an active view
						wf_window_pos *pRect = (s_last_active != (uint32_t)-1) ?
							g_hash_table_lookup (s_pWindowPos, GUINT_TO_POINTER (s_last_active)) : NULL;
						if (pRect) _wf_hide_show_if_on_our_way (pDock, pRect);
					}
					else if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
					{
						_wf_hide_if_any_overlap_or_show (pDock, NULL);
					}
				}
			}
		}
	}
}


/***********************************************************************
 * parse incoming messages, init
 ***********************************************************************/

static gboolean _socket_cb (G_GNUC_UNUSED GIOChannel *pSource, GIOCondition cond, G_GNUC_UNUSED gpointer data)
{
	gboolean bContinue = FALSE;
	
	if ((cond & G_IO_HUP) || (cond & G_IO_ERR))
		cd_warning ("Wayfire socket disconnected");	
	else if (cond & G_IO_IN)
	{
		// we should be able to read at least one complete message
		uint32_t len2 = 0;
		char* tmp2 = _read_msg (&len2);
		if (tmp2)
		{
			struct json_object *res = json_tokener_parse (tmp2);
			if (res)
			{
				bContinue = TRUE; // we could read a valid JSON, the connection is OK
				gboolean bResult = FALSE;
				
				struct cb_data *peek_cb = g_queue_peek_head (&s_cb_queue); // OK to call with an empty queue
				gboolean is_array = json_object_is_type (res, json_type_array);
				gboolean is_list_cb = peek_cb && (peek_cb->cb == _list_views_cb);
				gboolean is_conf_cb = peek_cb && (peek_cb->cb == _got_configuration);
				if (is_list_cb)
				{
					// this is the reply to a list-views request, process it
					g_free (g_queue_pop_head (&s_cb_queue)); // we don't need the callback struct anymore
					if (is_array) _list_views_cb (res);
					else cd_warning ("Unexpected reply format to list-views request"); //!! TODO: cancel watching events !!
					
					json_object_put (res);
					g_free (tmp2);
					return TRUE;
				}
				else if (is_conf_cb)
				{
					// this is the reply to a wayfire/configuration request, process it
					g_free (g_queue_pop_head (&s_cb_queue)); // we don't need the callback struct anymore
					_got_configuration (res);
					json_object_put (res);
					g_free (tmp2);
					return TRUE;
				}
				
				struct json_object *event = json_object_object_get (res, "event");
				if (event)
				{
					// try to process an event that we are subscribed to
					const char *event_str = json_object_get_string (event);
					if (!event_str) cd_warning ("Cannot parse event name!");
					else
					{
						if (! strcmp (event_str, "command-binding"))
						{
							struct json_object *id_obj = json_object_object_get (res, "binding-id");
							if (id_obj)
							{
								errno = 0;
								uint64_t id = json_object_get_uint64 (id_obj);
								if (errno) cd_warning ("Cannot parse binding ID");
								else
								{
									GList *it = s_pBindings;
									while (it)
									{
										struct wf_keybinding *binding = (struct wf_keybinding*)it->data;
										if (binding->id == id)
										{
											// found
											gldi_object_notify (&myDesktopMgr, NOTIFICATION_SHORTKEY_PRESSED, binding->keycode, binding->modifiers);
											break;
										}
										it = it->next;
									}
									
									if (!it) cd_warning ("Binding event with unknown ID");
								}
							}
							else cd_warning ("Binding event without ID");
						}
						else
						{
							// likely a view related event
							_process_view (event_str, json_object_object_get (res, "view"));
						}
					}
				}
				else
				{
					// in this case, this is a reply to a request we sent before
					struct json_object *result = json_object_object_get (res, "result");
					if (result)
					{
						const char *value = json_object_get_string (result);
						if (value && !strcmp(value, "ok")) bResult = TRUE;
					}
					
					// consume the callback that belongs to this call
					if (g_queue_is_empty (&s_cb_queue))
						g_critical ("Missing callback in queue");
					else
					{
						struct cb_data *data = g_queue_pop_head (&s_cb_queue);
						if (data)
						{
							if (data->pBinding)
							{
								// result of registering a keybinding
								uint64_t id = 0;
								if (bResult)
								{
									struct json_object *id_obj = json_object_object_get (res, "binding-id");
									if (id_obj)
									{
										errno = 0;
										id = json_object_get_uint64 (id_obj);
										if (errno) bResult = FALSE;
									}
									else bResult = FALSE;
								}
								
								_binding_registered (bResult, data->pBinding, (CairoDockGrabKeyResult) data->cb, id);
							}
							else if (data->cb) ((CairoDockDesktopManagerActionResult) data->cb) (bResult, data->user_data);
							g_free (data);
						}
					}
				}
				
				json_object_put (res);
			}
			else cd_warning ("Invalid JSON returned by Wayfire");
			free(tmp2);
		}
		// if we have no message, warning already shown, we will stop the connection
		// as bContinue == FALSE
	}
	
	if (!bContinue)
	{
		s_sidIO = 0;
		s_pIOChannel = NULL;
		g_queue_clear_full (&s_cb_queue, _cb_fail);
		g_list_free_full (s_pBindings, g_free);
		s_pBindings = NULL;
		return FALSE; // will also free our GIOChannel and close the socket
	}
	
	return TRUE;
}

void cd_init_wayfire_backend (void) {
	g_queue_init (&s_cb_queue);
	
	const char* wf_socket = getenv("WAYFIRE_SOCKET");
	if (!wf_socket) wf_socket = default_socket;
	
	int wayfire_socket = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (wayfire_socket < 0) return;
	
	struct sockaddr_un sa;
	memset (&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy (sa.sun_path, wf_socket, sizeof (sa.sun_path) - 1);
	
	
	if (connect (wayfire_socket, (const struct sockaddr*)&sa, sizeof (sa))) {
		close (wayfire_socket);
		return;
	}
	
	GIOChannel *pIOChannel = g_io_channel_unix_new (wayfire_socket);
	if (!pIOChannel)
	{
		cd_warning ("Cannot create GIOChannel!");
		close (wayfire_socket);
		return;
	}
	
	g_io_channel_set_close_on_unref (pIOChannel, TRUE); // take ownership of wayfire_socket
	g_io_channel_set_encoding (pIOChannel, NULL, NULL); // otherwise GLib would try to validate received data as UTF-8
	g_io_channel_set_buffered (pIOChannel, FALSE); // we don't want GLib to block trying to read too much data
	
	// Check supported capabilities -- do this here so that we can register only
	// the supported functions.
	// Note: we need to do this synchronously, since we should register the desktop
	// manager backend before other components are initialized (e.g. keybindings).
	const char *msg = "{\"method\": \"list-methods\", \"data\": {}}";
	if (_send_msg_full (msg, strlen (msg), pIOChannel) < 0)
	{
		// warning already shown
		g_io_channel_unref (pIOChannel);
		return;
	}
	uint32_t len = 0;
	char *ret = _read_msg_full (&len, pIOChannel);
	if (!ret)
	{
		// warning already shown
		g_io_channel_unref (pIOChannel);
		return;
	}
	
	struct json_object *res = json_tokener_parse (ret);
	struct json_object *methods = json_object_object_get (res, "methods"); // should be an array
	if (! methods || ! json_object_is_type (methods, json_type_array))
	{
		cd_warning ("Error getting the list of available IPC methods!");
		g_io_channel_unref (pIOChannel);
		json_object_put (res);
		g_free (ret);
		return;
	}
	
	// we have a list of methods, need to check the expected ones
	GldiDesktopManagerBackend p;
	memset(&p, 0, sizeof (GldiDesktopManagerBackend));
	
	size_t n_methods = json_object_array_length (methods);
	size_t i;
	gboolean any_found = FALSE;
	gboolean watch_found = FALSE;
	gboolean list_found = FALSE;
	for (i = 0; i < n_methods; i++)
	{
		struct json_object *m = json_object_array_get_idx (methods, i);
		const char *value = json_object_get_string (m);
		if (! value) cd_warning ("Invalid json value among IPC methods!");
		else
		{
			if (! p.present_class && ! strcmp (value, "scale_ipc_filter/activate_appid"))
				{ p.present_class = _present_class; any_found = TRUE; }
			else if (! p.present_windows && ! strcmp (value, "scale/toggle"))
				{ p.present_windows = _present_windows; any_found = TRUE; }
			else if (! p.present_desktops && ! strcmp (value, "expo/toggle"))
				{ p.present_desktops = _present_desktops; any_found = TRUE; }
			else if (! p.show_hide_desktop && ! strcmp (value, "wm-actions/toggle_showdesktop"))
				{ p.show_hide_desktop = _show_hide_desktop; any_found = TRUE; }
			else if (! watch_found && ! strcmp (value, "window-rules/events/watch"))
				watch_found = TRUE;
			else if (! list_found && ! strcmp (value, "window-rules/list-views"))
				list_found = TRUE;
			else if (! s_bCanSticky && ! strcmp (value, "wm-actions/set-sticky"))
				s_bCanSticky = TRUE;
			else if (! s_bCanAbove && ! strcmp (value, "wm-actions/set-always-on-top"))
				s_bCanAbove = TRUE;
#ifdef HAVE_EVDEV
			else if (! p.grab_shortkey && ! strcmp (value, "command/register-binding"))
				{ p.grab_shortkey = _bind_key; any_found = TRUE; }
#endif
		}
	}
	
	json_object_put (res);
	g_free (ret);
	
	if (! (watch_found && list_found))
	{
		// need to be able to list and watch views to set these
		s_bCanSticky = FALSE;
		s_bCanAbove = FALSE;
	}
	
	if (any_found || (watch_found && list_found))
	{
		s_sidIO = g_io_add_watch (pIOChannel, G_IO_IN | G_IO_HUP | G_IO_ERR, _socket_cb, NULL);
		if (s_sidIO)
		{
			s_pIOChannel = pIOChannel;
			if (any_found) gldi_desktop_manager_register_backend (&p, "Wayfire");
			if (watch_found && list_found)
			{
				GldiDockVisibilityBackend dvb = {0};
				dvb.refresh = _visibility_refresh;
				dvb.has_overlapping_window = _wf_dock_has_overlapping_window;
				dvb.name = "Wayfire IPC";
				gldi_dock_visibility_register_backend (&dvb);
				
				if ((s_bCanAbove || s_bCanSticky) && s_bNeedStickyAbove)
					_init_sticky_above ();
			}
		}
		else cd_warning ("Cannot add socket IO event source!");
	}
	else cd_message ("Connected to Wayfire socket, but no methods available, not registering");
	
	g_io_channel_unref (pIOChannel); // note: ref taken by g_io_add_watch () if succesful
}

#else

void cd_init_wayfire_backend (void) {
	cd_message("Cairo-Dock was not built with Wayfire IPC support");
}

void gldi_wf_init_sticky_above (void) { }
void gldi_wf_can_sticky_above (gboolean *bCanSticky, gboolean *bCanAbove)
{
	if (bCanSticky) *bCanSticky = FALSE;
	if (bCanAbove) *bCanAbove = FALSE;
}
void gldi_wf_set_sticky (G_GNUC_UNUSED GldiWindowActor *actor, G_GNUC_UNUSED gboolean bSticky) { }
void gldi_wf_set_above (G_GNUC_UNUSED GldiWindowActor *actor, G_GNUC_UNUSED gboolean bAbove) { }
void gldi_wf_is_above_or_below (G_GNUC_UNUSED GldiWindowActor *actor, gboolean *bIsAbove, gboolean *bIsBelow)
{
	if (bIsAbove) *bIsAbove = FALSE;
	if (bIsBelow) *bIsBelow = FALSE;
}

#endif


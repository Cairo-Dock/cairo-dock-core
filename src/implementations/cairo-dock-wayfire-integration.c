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
#include "cairo-dock-log.h"

#ifdef HAVE_JSON

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json.h>
#include <libevdev/libevdev.h>

#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"  // bIsHidden
#include "cairo-dock-icon-factory.h"  // pAppli
#include "cairo-dock-container-priv.h"  // gldi_container_get_gdk_window
#include "cairo-dock-class-manager-priv.h"

static const char default_socket[] = "/tmp/wayfire-wayland-1.socket";

static GIOChannel *s_pIOChannel = NULL; // GIOChannel encapsulating the above socket
static unsigned int s_sidIO = 0; // source ID for the above added to the main loop

struct cb_data {
	CairoDockDesktopManagerActionResult cb;
	gpointer user_data;
	gboolean is_binding; // our own callback for registering keybindings -- in this case, cb is NULL, but the following fields are valid
	guint keycode;
	guint modifiers;
};
GQueue s_cb_queue; // queue of callbacks for submitted actions -- contains cb_data structs or NULLs if an action does not have a callback

// Helper function to signal failure and free one element of the queue
void _cb_fail (gpointer data)
{
	if (data)
	{
		struct cb_data *cb = (struct cb_data*)data;
		cb->cb (FALSE, cb->user_data);
		g_free (cb);
	}
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
		//!! TODO: will we try to call this function with no data to read?
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
		(g_io_channel_write_chars (pIOChannel, msg, len, &n_written, NULL) != G_IO_STATUS_NORMAL))
	{
		cd_warning ("Error writing to Wayfire's socket");
		return -1;
	}
	return 0;
}
/* same as above, but use our static variable */
static int _send_msg (const char* msg, uint32_t len)
{
	if (! s_pIOChannel) return -1;
	return _send_msg_full (msg, len, s_pIOChannel);
}

/* Call a Wayfire IPC method and try to check if it was successful. */
static void _call_ipc(struct json_object* data, CairoDockDesktopManagerActionResult cb, gpointer user_data) {
	size_t len;
	const char *tmp = json_object_to_json_string_length (data, JSON_C_TO_STRING_SPACED, &len);
	if (!(tmp && len))
	{
		if (cb) cb (FALSE, user_data);
		return;
	}
	
	if(_send_msg(tmp, len) < 0)
	{
		//!! TODO: we should probably tear down our socket here since this means a serious error !!
		if (cb) cb (FALSE, user_data);
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

struct wf_keybinding
{
	guint keycode;
	guint modifiers;
	uint64_t id;
};

GList *s_pBindings = NULL;

static void _binding_registered (gboolean bSuccess, guint keycode, guint modifiers, uint64_t id)
{
	if (bSuccess) cd_debug ("Binding registered: %u, %u -- %lu", keycode, modifiers, id);
	else cd_warning ("Cannot register binding: %u, %u", keycode, modifiers);
	
	if (!bSuccess) return; //!! TODO: signal error in this case
	
	struct wf_keybinding *binding = g_new (struct wf_keybinding, 1);
	binding->keycode = keycode;
	binding->modifiers = modifiers;
	binding->id = id;
	
	s_pBindings = g_list_prepend (s_pBindings, binding);
}

#ifdef HAVE_EVDEV
static gboolean _bind_key (guint keycode, guint modifiers, gboolean grab)
{
	if (grab)
	{
		const char *name = libevdev_event_code_get_name (EV_KEY, keycode - 8); // difference between X11 and Linux hardware keycodes
		if (!name)
		{
			cd_warning ("Unknown keycode: %u", keycode);
			return FALSE;
		}
		
		GString *str = g_string_sized_new (40); // all modifiers + some long key name
		
		// first process modifiers (note: GDK_SUPER_MASK and GDK_META_MASK have been removed by the caller)
		if (modifiers & GDK_CONTROL_MASK) g_string_append (str, "<ctrl>");
		if (modifiers & GDK_MOD1_MASK) g_string_append (str, "<alt>");
		if (modifiers & GDK_SHIFT_MASK) g_string_append (str, "<shift>");
		if (modifiers & GDK_MOD4_MASK) g_string_append (str, "<super>"); //!! TODO: verify !!
		
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
			return FALSE;
		}
		
		int ret = _send_msg(tmp, len);
		json_object_put (obj);
		g_string_free (str, TRUE);
		
		if (ret < 0) return FALSE; //!! TODO: destroy the socket?
		
		// add a callback
		struct cb_data *cb = g_new0 (struct cb_data, 1);
		cb->is_binding = TRUE;
		cb->keycode = keycode;
		cb->modifiers = modifiers;
		g_queue_push_tail (&s_cb_queue, cb);
	}
	//!! TODO: remove bindings
	else
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
	
	return TRUE;
}
#endif

/*
static gboolean _set_current_desktop(G_GNUC_UNUSED int iDesktopNumber, int iViewportNumberX, int iViewportNumberY) {
	// note: iDesktopNumber is always ignored, we only have one desktop
	gboolean bSuccess = FALSE;
	return bSuccess;
}
*/

/*
static void _workspace_changed (G_GNUC_UNUSED DBusGProxy *proxy, G_GNUC_UNUSED uint32_t output, int32_t x, int32_t y, G_GNUC_UNUSED gpointer data)
{
	// TODO: Wayfire can have independent workspaces on different outputs, this cannot be handled in the current scenario
	g_desktopGeometry.iCurrentViewportX = x;
	g_desktopGeometry.iCurrentViewportY = y;
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
}
*/
/*
static void _unregister_wayfire_backend() {
	// ??
}
*/


gboolean _socket_cb (G_GNUC_UNUSED GIOChannel *pSource, GIOCondition cond, G_GNUC_UNUSED gpointer data)
{
	gboolean bContinue = FALSE;
	
	if ((cond & G_IO_HUP) || (cond & G_IO_ERR))
		cd_warning ("Wayfire socket disconnected");	
	else if (cond & G_IO_IN)
	{
		// we should be able to read at least one complete message
		uint32_t len2 = 0;
		char* tmp2 = _read_msg(&len2);
		if (tmp2)
		{
			struct json_object *res = json_tokener_parse (tmp2);
			if (res)
			{
				bContinue = TRUE; // we could read a valid JSON, the connection is OK
				gboolean bResult = FALSE;
				
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
							if (data->is_binding)
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
								
								_binding_registered (bResult, data->keycode, data->modifiers, id);
							}
							else data->cb (bResult, data->user_data);
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
#ifdef HAVE_EVDEV
			else if (! p.grab_shortkey && ! strcmp (value, "command/register-binding"))
				{ p.grab_shortkey = _bind_key; any_found = TRUE; }
#endif
		}
	}
	
	json_object_put (res);
	g_free (ret);
	
	if (any_found)
	{
		s_sidIO = g_io_add_watch (pIOChannel, G_IO_IN | G_IO_HUP | G_IO_ERR, _socket_cb, NULL);
		if (s_sidIO)
		{
			s_pIOChannel = pIOChannel;
			gldi_desktop_manager_register_backend (&p, "Wayfire");
		}
		else cd_warning ("Cannot add socket IO event source!");
	}
	else
	{
		cd_message ("Connected to Wayfire socket, but no methods available, not registering");
		g_io_channel_unref (pIOChannel);
	}
	
	g_io_channel_unref (pIOChannel); // note: ref taken by g_io_add_watch () if succesful
}

#else

void cd_init_wayfire_backend() {
	cd_message("Cairo-Dock was not built with Wayfire IPC support");
}

#endif


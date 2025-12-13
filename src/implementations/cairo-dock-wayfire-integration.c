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

#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"  // bIsHidden
#include "cairo-dock-icon-factory.h"  // pAppli
#include "cairo-dock-container-priv.h"  // gldi_container_get_gdk_window
#include "cairo-dock-class-manager-priv.h"

static const char default_socket[] = "/tmp/wayfire-wayland-1.socket";

static GIOChannel *s_pIOChannel = NULL; // GIOChannel encapsulating the above socket

/*
 * Read a message from socket and return its contents or NULL if there
 * is no open socket. The caller should free() the returned string when done.
 * The length of the message is stored in msg_len (if not NULL).
 * Note: this will block until there is a message to read.
 */
static char* _read_msg (uint32_t* out_len)
{
	if (! s_pIOChannel) return NULL;
	
	const size_t header_size = 4;
	char header[header_size];
	if (g_io_channel_read_chars (s_pIOChannel, header, header_size, NULL, NULL) != G_IO_STATUS_NORMAL)
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
		if (g_io_channel_read_chars (s_pIOChannel, msg, msg_len, &n_read, NULL) != G_IO_STATUS_NORMAL
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

/* Send a message to the socket. Return 0 on success, -1 on failure */
static int _send_msg (const char* msg, uint32_t len)
{
	if (! s_pIOChannel) return -1;
	
	const size_t header_size = 4;
	gsize n_written = 0;
	char header[header_size];
	memcpy(header, &len, header_size);
	if ((g_io_channel_write_chars (s_pIOChannel, header, header_size, &n_written, NULL) != G_IO_STATUS_NORMAL) ||
		(g_io_channel_write_chars (s_pIOChannel, msg, len, &n_written, NULL) != G_IO_STATUS_NORMAL))
	{
		cd_warning ("Error writing to Wayfire's socket");
		return -1;
	}
	return 0;
}

/* Call a Wayfire IPC method and try to check if it was successful. */
static gboolean _call_ipc(struct json_object* data) {
	size_t len;
	const char *tmp = json_object_to_json_string_length (data, JSON_C_TO_STRING_SPACED, &len);
	if (!(tmp && len)) return FALSE;
	
	if(_send_msg(tmp, len) < 0) return FALSE;
	
	uint32_t len2 = 0;
	char* tmp2 = _read_msg(&len2);
	if(!tmp2) return FALSE;
	
	struct json_object *res = json_tokener_parse (tmp2);
	struct json_object *result = json_object_object_get (res, "result");
	gboolean ret = FALSE;
	if (result)
	{
		const char *value = json_object_get_string (result);
		if (value && !strcmp(value, "ok")) ret = TRUE;
	}
	
	json_object_put (res);
	free(tmp2);
	
	return ret;
}

static gboolean _call_ipc_method_no_data (const char *method)
{
	struct json_object *obj = json_object_new_object ();
	json_object_object_add (obj, "method", json_object_new_string (method));
	json_object_object_add (obj, "data", json_object_new_object ());
	gboolean ret = _call_ipc (obj);
	json_object_put (obj);
	return ret;
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
		gboolean ret = _call_ipc (obj);
		json_object_put (obj);
		if (cb) cb (ret, user_data);
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

void cd_init_wayfire_backend (void) {
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
	
	s_pIOChannel = g_io_channel_unix_new (wayfire_socket);
	if (!s_pIOChannel)
	{
		cd_warning ("Cannot create GIOChannel!");
		close (wayfire_socket);
		return;
	}
	
	g_io_channel_set_close_on_unref (s_pIOChannel, TRUE); // take ownership of wayfire_socket
	g_io_channel_set_encoding (s_pIOChannel, NULL, NULL); // otherwise GLib would try to validate received data as UTF-8
	g_io_channel_set_buffered (s_pIOChannel, FALSE); // we don't want GLib to block trying to read too much data
	
	// Check supported capabilities -- do this here so that we can register only
	// the supported functions.
	// Note: we need to do this synchronously, since we should register the desktop
	// manager backend before other components are initialized (e.g. keybindings).
	const char *msg = "{\"method\": \"list-methods\", \"data\": {}}";
	if (_send_msg (msg, strlen (msg)) < 0)
	{
		// warning already shown
		g_io_channel_unref (s_pIOChannel);
		return;
	}
	uint32_t len = 0;
	char *ret = _read_msg (&len);
	if (!ret)
	{
		// warning already shown
		g_io_channel_unref (s_pIOChannel);
		return;
	}
	
	struct json_object *res = json_tokener_parse (ret);
	struct json_object *methods = json_object_object_get (res, "methods"); // should be an array
	if (! methods || ! json_object_is_type (methods, json_type_array))
	{
		cd_warning ("Error getting the list of available IPC methods!");
		g_io_channel_unref (s_pIOChannel);
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
		}
	}
	
	json_object_put (res);
	g_free (ret);
	
	if (any_found) gldi_desktop_manager_register_backend (&p, "Wayfire");
}

#else

void cd_init_wayfire_backend() {
	cd_message("Cairo-Dock was not built with Wayfire IPC support");
}

#endif


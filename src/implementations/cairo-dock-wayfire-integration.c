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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cairo-dock-log.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"  // bIsHidden
#include "cairo-dock-icon-factory.h"  // pAppli
#include "cairo-dock-container.h"  // gldi_container_get_gdk_window
#include "cairo-dock-class-manager.h"
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_sync
#include "cairo-dock-wayfire-integration.h"

static const char expo_toggle[] = "{\"method\": \"expo/toggle\", \"data\": {}}";
static const char scale_toggle[] = "{\"method\": \"scale/toggle\", \"data\": {}}";
static const char scale_filter_base[] = "{\"method\": \"scale_ipc_filter/activate_appid\", \"data\": {\"all_workspaces\": true, \"app_id\": \"%s\"}}";
static const char toggle_showdesktop[] = "{\"method\": \"wm-actions/toggle_showdesktop\", \"data\": {}}";
static const char default_socket[] = "/tmp/wayfire-wayland-1.socket";

int wayfire_socket = -1; // socket connection to Wayfire

/* helpers to read and write data */
static int _read_data(char* buf, size_t n) {
	size_t s = 0;
	do {
		ssize_t tmp = read(wayfire_socket, buf + s, n);
		if(tmp < 0) {
			// read error -- TODO: error message
			close(wayfire_socket);
			wayfire_socket = -1;
			return -1;
		}
		s += tmp;
		n -= tmp;
	} while(n);
	return 0;
}

static int _write_data(const char* buf, size_t n) {
	size_t s = 0;
	do {
		ssize_t tmp = write(wayfire_socket, buf + s, n);
		if(tmp < 0) {
			// TODO: error message
			close(wayfire_socket);
			wayfire_socket = -1;
			return -1;
		}
		s += tmp;
		n -= tmp;
	} while(n);
	return 0;
}

/*
 * Read a message from socket and return its contents or NULL if there
 * is no open socket. The caller should free() the returned string when done.
 * The length of the message is stored in msg_len (if not NULL).
 * Note: this will block until there is a message to read.
 * TOOD: do this async by adding it to the main loop?
 */
static char* _read_msg(uint32_t* out_len)
{
	if (wayfire_socket == -1) return NULL;
	
	const size_t header_size = 4;
	char header[header_size];
	if(_read_data(header, header_size) < 0) return NULL;
	
	uint32_t msg_len;
	memcpy(&msg_len, header, header_size);
	char* msg = (char*)malloc(msg_len + 1);
	if(msg_len) if(_read_data(msg, msg_len) < 0) return NULL;
	msg[msg_len] = 0;
	if(out_len) *out_len = msg_len;
	return msg;
}

/* Send a message to the socket. Return 0 on success, -1 on failure */
static int _send_msg(const char* msg, uint32_t len) {
	if (wayfire_socket == -1) return -1;
	
	const size_t header_size = 4;
	char header[header_size];
	memcpy(header, &len, header_size);
	if(_write_data(header, header_size) < 0) return -1;
	return _write_data(msg, len);
}


/* Very simple check for the content of a JSON response.
 * TODO: use a proper parser library here! */

static size_t _skip_space(const char* x, size_t i, size_t len) {
	for(; i < len; i++) {
		if(! (x[i] == ' ' || x[i] == '\t' || x[i] == '\n' || x[i] == '\r') ) break;
	}
	return i;
}

static gboolean _check_json_simple(const char* json, const char* key, const char* value, size_t len) {
	gboolean found = FALSE;
	size_t i = 0;
	int quote = 0;
	int escape = 0;
	int key_next = 0;
	int key_found = 0;
	
	i = _skip_space(json, i, len);
	if(i == len || json[i] != '{') return FALSE;
	i = _skip_space(json, i + 1, len);
	if(i >= len) return FALSE;
	key_next = 1;
	
	while(i < len) {
		if(json[i] == 0) return FALSE;
		if(key_next) {
			/* we expect a json key, which is a string enclosed by double quotes */
			if(json[i] != '"') return FALSE;
			i++;
			size_t j = i;
			for(; j < len; j++) {
				if(json[j] == 0) return FALSE;
				if(escape) escape = 0;
				else if(json[j] == '\\') escape = 1;
				else if(json[j] == '"') break;
			}
			if(j == len) return FALSE;
			if(!found && strncmp(json + i, key, j - i) == 0) key_found = 1;
			i = _skip_space(json, j + 1, len);
			if(i + 1 >= len || json[i] != ':') return FALSE;
			key_next = 0;
		}
		else {
			/* we expect a value or we are skipping stuff */
			if(json[i] == '{' || json[i] == '[') {
				char c = json[i];
				char c2 = (c == '{') ? '}' : ']';
				int tmplevel = 1;
				for(i++; i < len; i++) {
					if(json[i] == 0) return FALSE;
					if(quote) {
						if(escape) escape = 0;
						else if(json[i] == '\\') escape = 1;
						else if(json[i] == '"') quote = 0;
					}
					else if(json[i] == '"') quote = 1;
					else if(json[i] == c) tmplevel++;
					else if(json[i] == c2) {
						tmplevel--;
						if(tmplevel == 0) break;
					}
				}
				i = _skip_space(json, i+1, len);
				if(i >= len) return FALSE;
			}
			else if(json[i] == '"') {
				/* string */
				i++;
				size_t j = i;
				for(; j < len; j++) {
					if(json[j] == 0) return FALSE;
					if(escape) escape = 0;
					else if(json[j] == '\\') escape = 1;
					else if(json[j] == '"') break;
				}
				if(j == len) return FALSE;
				if(key_found && strncmp(json + i, value, j - i) == 0) found = TRUE;
				i = j + 1;
			}
			else {
				/* skip everything */
				for(; i < len; i++) {
					if(json[i] == 0) return FALSE;
					if(json[i] == ',') break;
					if(json[i] == '"') return FALSE; /* invalid */
				}
			}
			key_found = 0;
			if(!key_next) {
				i = _skip_space(json, i, len);
				if(i == len) return FALSE;
				if(json[i] == 0) return FALSE;
				if(json[i] == '}') break; /* we are at the first level here */
				if(json[i] != ',') return FALSE;
				key_next = 1;
			}
		}
		i = _skip_space(json, i + 1, len);
	}
	return found;
}

/* Call a Wayfire IPC method and try to check if it was successful. */
static gboolean _call_ipc(const char* msg, size_t len) {
	if(_send_msg(msg, len) < 0) return FALSE;
	size_t len2 = 0;
	char* tmp = _read_msg(&len2);
	if(!tmp) return FALSE;
	gboolean res = _check_json_simple(tmp, "result", "ok", len2);
	free(tmp);
	return res;
}

/* Start scale on the current workspace */
static gboolean _present_windows() {
	return _call_ipc(scale_toggle, strlen(scale_toggle));
}

/* Start scale including all views of the given class */
static gboolean _present_class(const gchar *cClass) {
	size_t l1 = strlen(scale_filter_base);
	size_t l2 = strlen(cClass);
	size_t s = l1 + l2 + 1;
	char* msg = (char*)malloc(s);
	if(!msg) return FALSE;
	s = snprintf(msg, s, scale_filter_base, cClass);
	gboolean res = _call_ipc(msg, s);
	free(msg);
	return res;
}

/* Start expo on the current output */
static gboolean _present_desktops() {
	return _call_ipc(expo_toggle, strlen(scale_toggle));
}

/* Toggle show destop functionality (i.e. minimize / unminimize all views).
 * Note: bShow argument is ignored, we don't know if the desktop is shown / hidden */
static gboolean _show_hide_desktop(G_GNUC_UNUSED gboolean bShow) {
	return _call_ipc(toggle_showdesktop, strlen(toggle_showdesktop));
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

void cd_init_wayfire_backend() {
	const char* wf_socket = getenv("WAYFIRE_SOCKET");
	if(!wf_socket) wf_socket = default_socket;
	
	wayfire_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if(wayfire_socket < 0) return;
	
	struct sockaddr_un sa;
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, wf_socket, sizeof(sa.sun_path) - 1);
	
	
	if(connect(wayfire_socket, (const struct sockaddr*)&sa, sizeof(sa))) {
		close(wayfire_socket);
		wayfire_socket = -1;
		return;
	}
	
	GldiDesktopManagerBackend p;
	memset(&p, 0, sizeof (GldiDesktopManagerBackend));
	
	p.present_class = _present_class;
	p.present_windows = _present_windows;
	p.present_desktops = _present_desktops;
	p.show_hide_desktop = _show_hide_desktop;
	// p.set_current_desktop = _set_current_desktop;
	
	gldi_desktop_manager_register_backend (&p, "Wayfire");
}



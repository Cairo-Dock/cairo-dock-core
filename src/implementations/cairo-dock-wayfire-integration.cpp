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
#include <nlohmann/json.hpp>

#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"  // bIsHidden
#include "cairo-dock-icon-factory.h"  // pAppli
#include "cairo-dock-container.h"  // gldi_container_get_gdk_window
#include "cairo-dock-class-manager.h"
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_sync

static const char default_socket[] = "/tmp/wayfire-wayland-1.socket";

int wayfire_socket = -1; // socket connection to Wayfire

/* helpers to read and write data */
static int _read_data(char* buf, size_t n) {
	size_t s = 0;
	do {
		ssize_t tmp = read(wayfire_socket, buf + s, n);
		if(tmp < 0) {
			cd_warning("Error reading data from Wayfire's IPC socket!");
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
			cd_warning("Error writing data to Wayfire's IPC socket!");
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
	if(msg_len) if(_read_data(msg, msg_len) < 0) { free(msg); return NULL; }
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

/* Call a Wayfire IPC method and try to check if it was successful. */
static gboolean _call_ipc(const nlohmann::json& data) {
	std::string tmp = data.dump();
	if(_send_msg(tmp.c_str(), tmp.length()) < 0) return FALSE;
	
	uint32_t len2 = 0;
	char* tmp2 = _read_msg(&len2);
	if(!tmp2) return FALSE;
	
	nlohmann::json res = nlohmann::json::parse(tmp2, nullptr, false);
	free(tmp2);
	
	if(!res.contains("result")) return FALSE;
	return (res["result"] == "ok");
}

/* Start scale on the current workspace */
static gboolean _present_windows() {
	return _call_ipc({{"method", "scale/toggle"}, {"data", {}}});
}

/* Start scale including all views of the given class */
static gboolean _present_class(const gchar *cClass) {
	const gchar *cWmClass = cairo_dock_get_class_wm_class (cClass);
	return _call_ipc({{"method", "scale_ipc_filter/activate_appid"}, {"data", {{"all_workspaces", true}, {"app_id", cWmClass}}}});
}

/* Start expo on the current output */
static gboolean _present_desktops() {
	return _call_ipc({{"method", "expo/toggle"}, {"data", {}}});
}

/* Toggle show destop functionality (i.e. minimize / unminimize all views).
 * Note: bShow argument is ignored, we don't know if the desktop is shown / hidden */
static gboolean _show_hide_desktop(G_GNUC_UNUSED gboolean bShow) {
	return _call_ipc({{"method", "wm-actions/toggle_showdesktop"}, {"data", {}}});
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

#else

void cd_init_wayfire_backend() {
	cd_message("Cairo-Dock was not built with Wayfire IPC support");
}

#endif


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
#include "cairo-dock-niri-integration.h"
#include "cairo-dock-log.h"

#ifdef HAVE_JSON

#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-wayland-manager-priv.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <json.h>
#include <glib.h>

static GIOChannel *s_pIOChannel = NULL; // GIOChannel encapsulating the socket used for IPC
static unsigned int s_sidIO = 0; // source ID for the above added to the main loop

static gboolean s_bVersionRequest = FALSE; // we are expecting a response to the version request


static void _request_overview (void)
{
	if (!s_pIOChannel) return;
	
	const char *request = "{\"Action\": {\"ToggleOverview\": {}}}\n";
	gsize n_written = 0;
	const gsize len = strlen (request);
	if ((g_io_channel_write_chars (s_pIOChannel, request, len, &n_written, NULL) != G_IO_STATUS_NORMAL)
		|| n_written != len)
	{
		cd_warning ("Error writing to niri's socket");
		if (s_sidIO) g_source_remove (s_sidIO); // will close the socket and free s_pIOChannel as well
		s_sidIO = 0;
		s_pIOChannel = NULL;
		return;
	}
	g_io_channel_flush (s_pIOChannel, NULL);
}

static gboolean _socket_cb (G_GNUC_UNUSED GIOChannel *pSource, GIOCondition cond, G_GNUC_UNUSED gpointer data)
{
	gboolean bContinue = FALSE;
	gboolean bRegister = FALSE;
	
	if ((cond & G_IO_HUP) || (cond & G_IO_ERR))
		cd_warning ("Niri socket disconnected");
	else if (cond & G_IO_IN)
	{
		// we should read exactly one line and parse it as JSON
		gchar *line = NULL;
		gsize len = 0;
		if (g_io_channel_read_line (s_pIOChannel, &line, &len, NULL, NULL) != G_IO_STATUS_NORMAL)
			cd_warning ("Error reading from niri's socket");
		else if (len > 0) // note: is it an error to read a length of 0 ?
		{
			struct json_object *res = json_tokener_parse (line);
			if (res)
			{
				bContinue = TRUE;
				if (json_object_is_type (res, json_type_object))
				{
					struct json_object *err = json_object_object_get (res, "Err");
					struct json_object *ok  = json_object_object_get (res, "Ok");
					if (err) cd_warning ("Error communicating with niri: %s", json_object_get_string (err));
					else if (ok && s_bVersionRequest)
					{
						gldi_wayland_set_compositor_type (WAYLAND_COMPOSITOR_NIRI);
						
						// parse niri version
						struct json_object *ver_obj = json_object_object_get (ok, "Version");
						if (ver_obj && json_object_is_type (ver_obj, json_type_string))
						{
							const char *ver_str = json_object_get_string (ver_obj);
							const char *end = NULL;
							errno = 0;
							double ver = g_ascii_strtod (ver_str, (char**)&end);
							if (errno || end == ver_str)
								cd_warning ("Cannot parse niri version: %s", ver_str);
							else
							{
								if (ver >= 25.05)
								{
									bRegister = TRUE;
									cd_message ("found niri version >= 25.05, registering");
								}
								else cd_message ("niri version too old, not registering");
							}
						}
						else cd_warning ("Unexpected JSON value for niri version");
					}
				}
				else cd_warning ("Unexpected JSON value");
			}
			else cd_warning ("Invalid JSON returned by niri");
			json_object_put (res);
		}
		g_free (line);
	}
	
	if (s_bVersionRequest)
	{
		s_bVersionRequest = FALSE;
		if (bRegister)
		{
			// register our desktop manager backend
			GldiDesktopManagerBackend p;
			memset(&p, 0, sizeof (GldiDesktopManagerBackend));
			// note: niri's overview is in-between the Scale and Expose features of
			// other WMs, so we call it for both cases
			p.present_windows  = _request_overview;
			p.present_desktops = _request_overview;
			gldi_desktop_manager_register_backend (&p, "niri");
		}
		else bContinue = FALSE;
	}
	
	if (!bContinue)
	{
		s_sidIO = 0;
		s_pIOChannel = NULL;
		// note: returning FALSE will free our GIOChannel and close the socket
	}
	
	return bContinue;
}

void cd_init_niri_backend (void) {
	const char* socket_name = getenv("NIRI_SOCKET");
	if (!socket_name)
	{
		cd_message ("NIRI_SOCKET not set, not connecting");
		return;
	}
	
	int niri_socket = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (niri_socket < 0) return;
	
	struct sockaddr_un sa;
	memset (&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy (sa.sun_path, socket_name, sizeof (sa.sun_path) - 1);
	
	
	if (connect (niri_socket, (const struct sockaddr*)&sa, sizeof (sa))) {
		close (niri_socket);
		return;
	}
	
	GIOChannel *pIOChannel = g_io_channel_unix_new (niri_socket);
	if (!pIOChannel)
	{
		cd_warning ("Cannot create GIOChannel!");
		close (niri_socket);
		return;
	}
	
	g_io_channel_set_close_on_unref (pIOChannel, TRUE); // take ownership of niri_socket
	// g_io_channel_set_encoding (pIOChannel, NULL, NULL); // otherwise GLib would try to validate received data as UTF-8
	g_io_channel_set_buffered (pIOChannel, FALSE); // we don't want GLib to block trying to read too much data
	
	s_bVersionRequest = TRUE;
	s_sidIO = g_io_add_watch (pIOChannel, G_IO_IN | G_IO_HUP | G_IO_ERR, _socket_cb, NULL);
	if (s_sidIO)
	{
		s_pIOChannel = pIOChannel;
	
		// Check niri version -- we need at least 25.05 for the overview function
		const char *msg = "\"Version\"\n";
		gsize n_written = 0;
		if ((g_io_channel_write_chars (pIOChannel, msg, 10, &n_written, NULL) != G_IO_STATUS_NORMAL)
			|| n_written != 10)
		{
			cd_warning ("Error writing to niri's socket");
			g_source_remove (s_sidIO); // will close the socket and free s_pIOChannel as well
			s_sidIO = 0;
			s_pIOChannel = NULL;
		}
		else g_io_channel_flush (pIOChannel, NULL);
	}
	else cd_warning ("Cannot add socket IO to main loop!");
	g_io_channel_unref (pIOChannel); // note: ref taken by g_io_add_watch () if succesful
}

#else

void cd_init_niri_backend (void) {
	cd_message("Cairo-Dock was not built with Niri IPC support");
}

#endif


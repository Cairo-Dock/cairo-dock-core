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

#include "cairo-dock-log.h"
#include "cairo-dock-dbus.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-gnome-shell-integration.h"

static DBusGProxy *s_pGSProxy = NULL;
static gboolean s_DashIsVisible = FALSE;
static gint s_iSidShowDash = 0;

#define CD_GS_BUS "org.gnome.Shell"
#define CD_GS_OBJECT "/org/gnome/Shell"
#define CD_GS_INTERFACE "org.gnome.Shell"


static gboolean _show_dash (G_GNUC_UNUSED gpointer data)
{
	dbus_g_proxy_call_no_reply (s_pGSProxy, "Eval",
		G_TYPE_STRING, "Main.overview._dash.actor.show();",
		G_TYPE_INVALID,
		G_TYPE_INVALID);
	s_iSidShowDash = 0;
	return FALSE;
}
static void _hide_dash (void)
{
	dbus_g_proxy_call_no_reply (s_pGSProxy, "Eval",
		G_TYPE_STRING, "Main.overview._dash.actor.hide();",
		G_TYPE_INVALID,
		G_TYPE_INVALID);  // hide the dash, since it's completely redundant with the dock ("Main.panel.actor.hide()" to hide the top panel)
	if (s_iSidShowDash != 0)
	{
		g_source_remove (s_iSidShowDash);
		s_iSidShowDash = 0;
	}
	
	if (s_DashIsVisible)
		s_iSidShowDash = g_timeout_add_seconds (8, _show_dash, NULL);
}

static gboolean present_overview (void)
{
	gboolean bSuccess = FALSE;
	if (s_pGSProxy != NULL)
	{
		_hide_dash ();
		
		dbus_g_proxy_call_no_reply (s_pGSProxy, "Eval",
			G_TYPE_STRING, "Main.overview.toggle();",
			G_TYPE_INVALID,
			G_TYPE_INVALID);  // no reply, because this method doesn't output anything (we get an error if we use 'dbus_g_proxy_call')
		bSuccess = TRUE;
	}
	return bSuccess;
}

static gboolean present_class (const gchar *cClass)
{
	cd_debug ("%s (%s)", __func__, cClass);
	GList *pIcons = (GList*)cairo_dock_list_existing_appli_with_class (cClass);
	if (pIcons == NULL)
		return FALSE;
	
	gboolean bSuccess = FALSE;
	if (s_pGSProxy != NULL)
	{
		_hide_dash ();
		
		const gchar *cWmClass = cairo_dock_get_class_wm_class (cClass);
		int iNumDesktop, iViewPortX, iViewPortY;
		gldi_desktop_get_current (&iNumDesktop, &iViewPortX, &iViewPortY);
		int iWorkspace = iNumDesktop * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY + iViewPortX + iViewPortY * g_desktopGeometry.iNbViewportX;
		/// Note: there is actually a bug in Gnome-Shell 3.6, in workspace.js in '_onCloneSelected': 'wsIndex' should be let undefined, because here we switch to a window that is on a different workspace.
		gchar *code = g_strdup_printf ("Main.overview.show(); \
		let windows = global.get_window_actors(); \
		let ws = Main.overview._viewSelector._workspacesDisplay._workspacesViews[0]._workspaces[%d]; \
		for (let i = 0; i < windows.length; i++) { \
			let win = windows[i]; \
			let metaWin = win.get_meta_window(); \
			let index = ws._lookupIndex (metaWin);  \
			if (metaWin.get_wm_class() == '%s') \
				{ if (index == -1) ws._addWindowClone(win); } \
			else \
				{ if (index != -1) { let clone = ws._windows[index]; ws._windows.splice(index, 1); ws._windowOverlays.splice(index, 1); clone.destroy(); } }\
		}", iWorkspace, cWmClass);  // _workspacesViews[0] seems to be for the first monitor, we need some feedback here with multi-screens.
		dbus_g_proxy_call_no_reply (s_pGSProxy, "Eval",
			G_TYPE_STRING, code,
			G_TYPE_INVALID,
			G_TYPE_INVALID);  // no reply, because this method doesn't output anything (we get an error if we use 'dbus_g_proxy_call')
		g_free (code);
		bSuccess = TRUE;
	}
	return bSuccess;
}


static void _register_gs_backend (void)
{
	GldiDesktopManagerBackend p;
	memset(&p, 0, sizeof (GldiDesktopManagerBackend));
	
	p.present_class = present_class;
	p.present_windows = present_overview;
	p.present_desktops = present_overview;
	
	gldi_desktop_manager_register_backend (&p, "GNOME Shell");
}

static void _unregister_gs_backend (void)
{
	//cairo_dock_wm_register_backend (NULL);
}

static void _on_gs_owner_changed (G_GNUC_UNUSED const gchar *cName, gboolean bOwned, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Gnome-Shell is on the bus (%d)", bOwned);
	
	if (bOwned)  // set up the proxies
	{
		g_return_if_fail (s_pGSProxy == NULL);
		
		s_pGSProxy = cairo_dock_create_new_session_proxy (
			CD_GS_BUS,
			CD_GS_OBJECT,
			CD_GS_INTERFACE);
		
		gchar *cResult = NULL;
		gboolean bSuccess = FALSE;
		dbus_g_proxy_call (s_pGSProxy, "Eval", NULL,
			G_TYPE_STRING, "Main.overview._dash.actor.visible;",
			G_TYPE_INVALID,
			G_TYPE_BOOLEAN, &bSuccess,
			G_TYPE_STRING, &cResult,
			G_TYPE_INVALID);
		s_DashIsVisible = (!cResult || strcmp (cResult, "true") == 0);
		
		_register_gs_backend ();
	}
	else if (s_pGSProxy != NULL)
	{
		g_object_unref (s_pGSProxy);
		s_pGSProxy = NULL;
		
		_unregister_gs_backend ();
	}
}
static void _on_detect_gs (gboolean bPresent, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Gnome-Shell is present: %d", bPresent);
	if (bPresent)
	{
		_on_gs_owner_changed (CD_GS_BUS, TRUE, NULL);
	}
	cairo_dock_watch_dbus_name_owner (CD_GS_BUS,
		(CairoDockDbusNameOwnerChangedFunc) _on_gs_owner_changed,
		NULL);
}
void cd_init_gnome_shell_backend (void)
{
	// discard the Gnome-Flashback session
	// it provides org.gnome.Shell, but actually relies on Metacity or Compiz, not GnomeShell
	const gchar *session = g_getenv("XDG_CURRENT_DESKTOP");  // XDG_CURRENT_DESKTOP is set by gdm-x-session based on the 'DesktopNames' field in the /usr/share/xsessions/<session-name>.desktop (whereas XDG_SESSION_DESKTOP is set based on the finename of the session)
	if (session && g_str_has_prefix (session, "GNOME-Flashback"))
		return;
	
	// detect GnomeShell
	cairo_dock_dbus_detect_application_async (CD_GS_BUS,
		(CairoDockOnAppliPresentOnDbus) _on_detect_gs,
		NULL);
}

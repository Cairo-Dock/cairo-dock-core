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
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.bShowAppli
#include "cairo-dock-X-manager.h"
#include "cairo-dock-gnome-shell-integration.h"

static DBusGProxy *s_pGSProxy = NULL;

#define CD_GS_BUS "org.gnome.Shell"
#define CD_GS_OBJECT "/org/gnome/Shell"
#define CD_GS_INTERFACE "org.gnome.Shell"


static gboolean present_overview (void)
{
	gboolean bSuccess = FALSE;
	if (s_pGSProxy != NULL)
	{
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
		const gchar *cWmClass = cairo_dock_get_class_wm_class (cClass);
		int iNumDesktop, iViewPortX, iViewPortY;
		cairo_dock_get_current_desktop_and_viewport (&iNumDesktop, &iViewPortX, &iViewPortY);
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
	CairoDockWMBackend *p = g_new0 (CairoDockWMBackend, 1);
	
	p->present_class = present_class;
	p->present_windows = present_overview;
	p->present_desktops = present_overview;
	p->show_widget_layer = NULL;
	p->set_on_widget_layer = NULL;
	
	cairo_dock_wm_register_backend (p);
}

static void _unregister_gs_backend (void)
{
	cairo_dock_wm_register_backend (NULL);
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
		
		if (myTaskbarParam.bShowAppli)
			dbus_g_proxy_call_no_reply (s_pGSProxy, "Eval",
				G_TYPE_STRING, "Main.overview._dash.actor.hide();",
				G_TYPE_INVALID,
				G_TYPE_INVALID);  // hide the dash, since it's completely redundant with the dock ("Main.panel.actor.hide()" to hide the top panel)
		
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
	cairo_dock_dbus_detect_application_async (CD_GS_BUS,
		(CairoDockOnAppliPresentOnDbus) _on_detect_gs,
		NULL);
}

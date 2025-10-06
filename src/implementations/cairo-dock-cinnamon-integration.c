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
#include "cairo-dock-class-manager-priv.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-cinnamon-integration.h"

static DBusGProxy *s_pProxy = NULL;

#define CD_CINNAMON_BUS "org.Cinnamon"
#define CD_CINNAMON_OBJECT "/org/Cinnamon"
#define CD_CINNAMON_INTERFACE "org.Cinnamon"

/// Note: this has been tested with Cinnamon-1.6 on Mint-14 and Cinnamon-1.7.4 on Ubuntu-13.04

static gboolean _eval (const gchar *cmd)
{
	gboolean bSuccess = FALSE;
	if (s_pProxy != NULL)
	{
		gchar *cResult = NULL;
		GError *error = NULL;
		dbus_g_proxy_call (s_pProxy, "Eval", &error,
			G_TYPE_STRING, cmd,
			G_TYPE_INVALID,
			G_TYPE_BOOLEAN, &bSuccess,
			G_TYPE_STRING, &cResult,
			G_TYPE_INVALID);
		if (error)
		{
			cd_warning (error->message);
			g_error_free (error);
		}
		if (cResult)
		{
			cd_debug ("%s", cResult);
			g_free (cResult);
		}
	}
	return bSuccess;
}

static gboolean present_overview (void)
{
	return _eval ("Main.overview.toggle();");
}

static gboolean present_expo (void)
{
	return _eval ("Main.expo.toggle();");
}

static gboolean present_class (const gchar *cClass)
{
	cd_debug ("%s", cClass);
	GList *pIcons = (GList*)cairo_dock_list_existing_appli_with_class (cClass);
	if (pIcons == NULL)
		return FALSE;
	
	gboolean bSuccess = FALSE;
	if (s_pProxy != NULL)
	{
		const gchar *cWmClass = cairo_dock_get_class_wm_class (cClass);
		int iNumDesktop, iViewPortX, iViewPortY;
		gldi_desktop_get_current (&iNumDesktop, &iViewPortX, &iViewPortY);
		int iWorkspace = iNumDesktop * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY + iViewPortX + iViewPortY * g_desktopGeometry.iNbViewportX;
		gchar *code = g_strdup_printf ("Main.overview.show(); \
		let windows = global.get_window_actors(); \
		let ws = Main.overview.workspacesView._workspaces[%d]._monitors[0]; \
		for (let i = 0; i < windows.length; i++) \
		{ \
			let win = windows[i]; \
			let metaWin = win.get_meta_window(); \
			let index = ws._lookupIndex(metaWin); \
			if (metaWin.get_wm_class() == '%s') \
				{ if (index == -1) ws._addWindowClone(win); } \
				else \
				{ if (index != -1) { let clone = ws._windows[index]; ws._windows.splice(index, 1);clone.destroy(); } \
			} \
		}", iWorkspace, cWmClass);
		cd_debug ("eval(%s)", code);
		bSuccess = _eval (code);
		g_free (code);
	}
	return bSuccess;
}

static void _register_cinnamon_backend (void)
{
	GldiDesktopManagerBackend p;
	memset(&p, 0, sizeof (GldiDesktopManagerBackend));
	
	p.present_class = present_class;
	p.present_windows = present_overview;
	p.present_desktops = present_expo;
	
	gldi_desktop_manager_register_backend (&p, "Cinnamon");
}

static void _unregister_cinnamon_backend (void)
{
	//cairo_dock_wm_register_backend (NULL);
}

static void _on_cinnamon_owner_changed (G_GNUC_UNUSED const gchar *cName, gboolean bOwned, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Cinnamon is on the bus (%d)", bOwned);
	
	if (bOwned)  // set up the proxy
	{
		g_return_if_fail (s_pProxy == NULL);
		
		s_pProxy = cairo_dock_create_new_session_proxy (
			CD_CINNAMON_BUS,
			CD_CINNAMON_OBJECT,
			CD_CINNAMON_INTERFACE);
		
		_register_cinnamon_backend ();
	}
	else if (s_pProxy != NULL)
	{
		g_object_unref (s_pProxy);
		s_pProxy = NULL;
		
		_unregister_cinnamon_backend ();
	}
}
static void _on_detect_cinnamon (gboolean bPresent, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Cinnamon is present: %d", bPresent);
	if (bPresent)
	{
		_on_cinnamon_owner_changed (CD_CINNAMON_BUS, TRUE, NULL);
	}
	cairo_dock_watch_dbus_name_owner (CD_CINNAMON_BUS,
		(CairoDockDbusNameOwnerChangedFunc) _on_cinnamon_owner_changed,
		NULL);
}
void cd_init_cinnamon_backend (void)
{
	cairo_dock_dbus_detect_application_async (CD_CINNAMON_BUS,
		(CairoDockOnAppliPresentOnDbus) _on_detect_cinnamon,
		NULL);
}

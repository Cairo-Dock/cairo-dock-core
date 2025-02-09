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
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dbus.h"
#include "cairo-dock-X-utilities.h"  // cairo_dock_get_X_display
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-windows-manager.h"  // gldi_window_get_id
#include "cairo-dock-class-manager.h"
#include "cairo-dock-icon-manager.h"  // myIconsParam.fAmplitude
#include "cairo-dock-kwin-integration.h"
#ifdef HAVE_WAYLAND_PROTOCOLS
#include "cairo-dock-plasma-window-manager.h" // gldi_plasma_window_manager_get_uuid
#endif
#include "cairo-dock-X-manager.h" // gldi_X_manager_get_window_xid

static DBusGProxy *s_pKwinAccelProxy = NULL;
static DBusGProxy *s_pPlasmaAccelProxy = NULL;
static DBusGProxy *s_pWindowViewProxy = NULL;

#define CD_KGLOBALACCEL_BUS "org.kde.kglobalaccel"
#define CD_KGLOBALACCEL_KWIN_OBJECT "/component/kwin"
#define CD_KGLOBALACCEL_PLASMA_OBJECT "/component/plasmashell"
#define CD_KGLOBALACCEL_INTERFACE "org.kde.kglobalaccel.Component"

#define CD_WINDOWVIEW_OBJECT "/org/kde/KWin/Effect/WindowView1"
#define CD_WINDOWVIEW_BUS "org.kde.KWin.Effect.WindowView1"
#define CD_WINDOWVIEW_INTERFACE "org.kde.KWin.Effect.WindowView1"

static gboolean bRegistered = FALSE;

static gboolean present_windows (void)
{
	gboolean bSuccess = FALSE;
	if (s_pKwinAccelProxy != NULL)
	{
		GError *erreur = NULL;
		bSuccess = dbus_g_proxy_call (s_pKwinAccelProxy, "invokeShortcut", &erreur,
			G_TYPE_STRING, "ExposeAll",
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("Kwin ExposeAll error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static gboolean present_class (const gchar *cClass)
{
#ifdef HAVE_WAYLAND_PROTOCOLS
	gboolean bIsWayland = gldi_container_is_wayland_backend ();
#endif
	
	gboolean bSuccess = FALSE;
	if (s_pWindowViewProxy != NULL)
	{
		GError *erreur = NULL;
		
		const GList *pIcons = cairo_dock_list_existing_appli_with_class (cClass);
		unsigned int len = g_list_length ((GList*)pIcons); //!! TODO: why does this not take a const GList ??
		const char **uuids = g_new0 (const char*, len + 1);
		
		unsigned int i;
		for (i = 0; pIcons; pIcons = pIcons->next)
		{
			Icon *pOneIcon = pIcons->data;
#ifdef HAVE_WAYLAND_PROTOCOLS
			if (bIsWayland) uuids[i] = gldi_plasma_window_manager_get_uuid (pOneIcon->pAppli);
			else
#endif
			{
				unsigned long xid = gldi_X_manager_get_window_xid (pOneIcon->pAppli);
				if (xid) uuids[i] = g_strdup_printf ("%lu", xid);
			}
			if (uuids[i]) i++;
		}
		
		if (!i)
			cd_warning ("No uuids found for class: %s", cClass);
		else
		{
			bSuccess = dbus_g_proxy_call (s_pWindowViewProxy, "activate", &erreur,
				G_TYPE_STRV, uuids,
				G_TYPE_INVALID,
				G_TYPE_INVALID);
			if (erreur)
			{
				cd_warning ("Kwin ExposeAll error: %s", erreur->message);
				g_error_free (erreur);
				bSuccess = FALSE;
			}
		}
#ifdef HAVE_WAYLAND_PROTOCOLS
		if (bIsWayland) g_free (uuids);
		else
#endif
			g_strfreev ((char**)uuids);
	}
	return bSuccess;
}

static gboolean present_desktops (void)
{
	gboolean bSuccess = FALSE;
	if (s_pKwinAccelProxy != NULL)
	{
		GError *erreur = NULL;
		bSuccess = dbus_g_proxy_call (s_pKwinAccelProxy, "invokeShortcut", &erreur,
			G_TYPE_STRING, "ShowDesktopGrid",
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("Kwin ShowDesktopGrid error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static gboolean show_widget_layer (void)
{
	gboolean bSuccess = FALSE;
	if (s_pPlasmaAccelProxy != NULL)
	{
		GError *erreur = NULL;
		bSuccess = dbus_g_proxy_call (s_pPlasmaAccelProxy, "invokeShortcut", &erreur,
			G_TYPE_STRING, "show dashboard",
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("Plasma-desktop 'Show Dashboard' error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static void _register_kwin_backend (void)
{
	// only register once, we do not unregister anyway
	if (bRegistered) return;
	bRegistered = TRUE;
	GldiDesktopManagerBackend p;
	memset(&p, 0, sizeof (GldiDesktopManagerBackend));
	
	p.present_class = present_class;
	p.present_windows = present_windows;
	p.present_desktops = present_desktops;
	p.show_widget_layer = show_widget_layer;
	p.set_on_widget_layer = NULL;  // the Dashboard is not a real widget layer :-/
	
	gldi_desktop_manager_register_backend (&p, "KWin");
}

static void _unregister_kwin_backend (void)
{
	//cairo_dock_wm_register_backend (NULL);
}

static void _on_kwin_owner_changed (G_GNUC_UNUSED const gchar *cName, gboolean bOwned, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Kwin is on the bus (%d)", bOwned);
	
	if (bOwned)  // set up the proxies
	{
		g_return_if_fail (s_pKwinAccelProxy == NULL);
		
		s_pKwinAccelProxy = cairo_dock_create_new_session_proxy (
			CD_KGLOBALACCEL_BUS,
			CD_KGLOBALACCEL_KWIN_OBJECT,
			CD_KGLOBALACCEL_INTERFACE);
		
		s_pPlasmaAccelProxy = cairo_dock_create_new_session_proxy (
			CD_KGLOBALACCEL_BUS,
			CD_KGLOBALACCEL_PLASMA_OBJECT,
			CD_KGLOBALACCEL_INTERFACE);
		
		_register_kwin_backend ();
	}
	else
	{
		if (s_pKwinAccelProxy)
		{
			g_object_unref (s_pKwinAccelProxy);
			s_pKwinAccelProxy = NULL;
		}
		if (s_pPlasmaAccelProxy)
		{
			g_object_unref (s_pPlasmaAccelProxy);
			s_pPlasmaAccelProxy = NULL;
		}
		
		_unregister_kwin_backend ();
	}
}
static void _on_detect_kwin (gboolean bPresent, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Kwin is present: %d", bPresent);
	if (bPresent)
	{
		_on_kwin_owner_changed (CD_KGLOBALACCEL_BUS, TRUE, NULL);
	}
	cairo_dock_watch_dbus_name_owner (CD_KGLOBALACCEL_BUS,
		(CairoDockDbusNameOwnerChangedFunc) _on_kwin_owner_changed,
		NULL);
}

static void _on_windowview_owner_changed (G_GNUC_UNUSED const gchar *cName, gboolean bOwned, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Kwin is on the bus (%d)", bOwned);
	
	if (bOwned)  // set up the proxies
	{
		g_return_if_fail (s_pWindowViewProxy == NULL);
		
		s_pWindowViewProxy = cairo_dock_create_new_session_proxy (
			CD_WINDOWVIEW_BUS,
			CD_WINDOWVIEW_OBJECT,
			CD_WINDOWVIEW_INTERFACE);
		
		_register_kwin_backend ();
	}
	else
	{
		if (s_pWindowViewProxy)
		{
			g_object_unref (s_pWindowViewProxy);
			s_pWindowViewProxy = NULL;
		}
		
		//!! TODO: this is a no-op, do not call it
		_unregister_kwin_backend ();
	}
}
static void _on_detect_windowview (gboolean bPresent, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("KWin.Effect.WindowView1 is present: %d", bPresent);
	if (bPresent)
	{
		_on_windowview_owner_changed (CD_WINDOWVIEW_BUS, TRUE, NULL);
	}
	cairo_dock_watch_dbus_name_owner (CD_WINDOWVIEW_BUS,
		(CairoDockDbusNameOwnerChangedFunc) _on_windowview_owner_changed,
		NULL);
}

void cd_init_kwin_backend (void)
{
	cairo_dock_dbus_detect_application_async (CD_KGLOBALACCEL_BUS,
		(CairoDockOnAppliPresentOnDbus) _on_detect_kwin,
		NULL);
	cairo_dock_dbus_detect_application_async (CD_WINDOWVIEW_BUS,
		(CairoDockOnAppliPresentOnDbus) _on_detect_windowview,
		NULL);
}


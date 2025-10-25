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

#include <glib.h>
#include <gio/gio.h>

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"  // cairo_dock_get_X_display
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-windows-manager-priv.h"  // gldi_window_get_id
#include "cairo-dock-class-manager-priv.h"
#include "cairo-dock-icon-manager.h"  // myIconsParam.fAmplitude
#include "cairo-dock-kwin-integration.h"
#ifdef HAVE_WAYLAND_PROTOCOLS
#include "cairo-dock-plasma-window-manager.h" // gldi_plasma_window_manager_get_uuid
#endif
#include "cairo-dock-X-manager.h" // gldi_X_manager_get_window_xid

static GDBusProxy *s_pKwinAccelProxy = NULL;
static GDBusProxy *s_pPlasmaAccelProxy = NULL;
static GDBusProxy *s_pWindowViewProxy = NULL;

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
		
		GVariant *res = g_dbus_proxy_call_sync (s_pKwinAccelProxy, "invokeShortcut",
			g_variant_new ("(s)", "ExposeAll"), G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &erreur);
		if (erreur)
		{
			cd_warning ("Kwin ExposeAll error: %s", erreur->message);
			g_error_free (erreur);
		}
		else
		{
			g_variant_unref (res);
			bSuccess = TRUE;
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
				
		unsigned int i;
		GVariantBuilder builder;
#if GLIB_CHECK_VERSION (2, 84, 0)
		g_variant_builder_init_static
#else
		g_variant_builder_init
#endif
			(&builder, G_VARIANT_TYPE ("as"));
		const GList *pIcons = cairo_dock_list_existing_appli_with_class (cClass);
		for (i = 0; pIcons; pIcons = pIcons->next)
		{
			Icon *pOneIcon = pIcons->data;
#ifdef HAVE_WAYLAND_PROTOCOLS
			if (bIsWayland)
			{
				g_variant_builder_add (&builder, "s", gldi_plasma_window_manager_get_uuid (pOneIcon->pAppli));
				i++;
			}
			else
#endif
			{
				unsigned long xid = gldi_X_manager_get_window_xid (pOneIcon->pAppli);
				if (xid)
				{
					g_variant_builder_add_value (&builder, g_variant_new_take_string (g_strdup_printf ("%lu", xid)));
					i++;
				}
			}
		}
		
		if (!i)
		{
			cd_warning ("No uuids found for class: %s", cClass);
			g_variant_builder_clear (&builder);
		}
		else
		{
			GVariant *res = g_dbus_proxy_call_sync (s_pWindowViewProxy, "activate",
				g_variant_new ("(as)", &builder), G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &erreur);
			if (erreur)
			{
				cd_warning ("Kwin ExposeAll error: %s", erreur->message);
				g_error_free (erreur);
			}
			else
			{
				g_variant_unref (res);
				bSuccess = TRUE;
			}
		}
	}
	return bSuccess;
}

static gboolean present_desktops (void)
{
	gboolean bSuccess = FALSE;
	if (s_pKwinAccelProxy != NULL)
	{
		GError *erreur = NULL;
		GVariant *res = g_dbus_proxy_call_sync (s_pKwinAccelProxy, "invokeShortcut",
			g_variant_new ("(s)", "ShowDesktopGrid"), G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &erreur);
		if (erreur)
		{
			cd_warning ("Kwin ShowDesktopGrid error: %s", erreur->message);
			g_error_free (erreur);
		}
		else
		{
			g_variant_unref (res);
			bSuccess = TRUE;
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
		GVariant *res = g_dbus_proxy_call_sync (s_pPlasmaAccelProxy, "invokeShortcut",
			g_variant_new ("(s)", "show dashboard"), G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, &erreur);
		if (erreur)
		{
			cd_warning ("Plasma-desktop 'Show Dashboard' error: %s", erreur->message);
			g_error_free (erreur);
		}
		else
		{
			g_variant_unref (res);
			bSuccess = TRUE;
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

/*
static void _unregister_kwin_backend (void)
{
	cairo_dock_wm_register_backend (NULL);
}
*/

static void _on_name_appeared (GDBusConnection *connection, const gchar *name,
	G_GNUC_UNUSED const gchar *name_owner, G_GNUC_UNUSED gpointer data)
{
	GError *erreur = NULL;
	
	if (!strcmp (name, CD_KGLOBALACCEL_BUS))
	{
		g_return_if_fail (s_pKwinAccelProxy == NULL);
		g_return_if_fail (s_pPlasmaAccelProxy == NULL);
		
		//!! TODO: use async versions ?
		s_pKwinAccelProxy = g_dbus_proxy_new_sync (connection,
			G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
			NULL, // GDBusInterfaceInfo -- we could supply this, but work anyway
			name,
			CD_KGLOBALACCEL_KWIN_OBJECT,
			CD_KGLOBALACCEL_INTERFACE,
			NULL,
			&erreur);
		if (erreur)
		{
			cd_warning ("Error creating KWin proxy: %s", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		
		s_pPlasmaAccelProxy = g_dbus_proxy_new_sync (connection,
			G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
			NULL, // GDBusInterfaceInfo -- we could supply this, but work anyway
			name,
			CD_KGLOBALACCEL_PLASMA_OBJECT,
			CD_KGLOBALACCEL_INTERFACE,
			NULL,
			&erreur);
		if (erreur)
		{
			cd_warning ("Error creating KWin proxy: %s", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		
		if (s_pPlasmaAccelProxy || s_pKwinAccelProxy) _register_kwin_backend ();
	}
	else if (!strcmp (name, CD_WINDOWVIEW_BUS))
	{
		g_return_if_fail (s_pWindowViewProxy == NULL);
		
		s_pWindowViewProxy = g_dbus_proxy_new_sync (connection,
			G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
			NULL, // GDBusInterfaceInfo -- we could supply this, but work anyway
			name,
			CD_WINDOWVIEW_OBJECT,
			CD_WINDOWVIEW_INTERFACE,
			NULL,
			&erreur);
		if (erreur)
		{
			cd_warning ("Error creating KWin proxy: %s", erreur->message);
			g_error_free (erreur);
			erreur = NULL;
		}
		
		if (s_pWindowViewProxy) _register_kwin_backend ();
	}
}

static void _on_name_vanished (G_GNUC_UNUSED GDBusConnection *connection, const gchar *name, G_GNUC_UNUSED gpointer user_data)
{
	if (!strcmp (name, CD_KGLOBALACCEL_BUS))
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
	}
	else if (!strcmp (name, CD_WINDOWVIEW_BUS))
	{
		if (s_pWindowViewProxy)
		{
			g_object_unref (s_pWindowViewProxy);
			s_pWindowViewProxy = NULL;
		}
	}
	
	//!! TODO: we cannot "unregister" our backend, although it would not matter much, since there is no alternative to use anyway
}

void cd_init_kwin_backend (void)
{
	g_bus_watch_name (G_BUS_TYPE_SESSION, CD_KGLOBALACCEL_BUS, G_BUS_NAME_WATCHER_FLAGS_NONE,
		_on_name_appeared, _on_name_vanished, NULL, NULL);
	g_bus_watch_name (G_BUS_TYPE_SESSION, CD_WINDOWVIEW_BUS, G_BUS_NAME_WATCHER_FLAGS_NONE,
		_on_name_appeared, _on_name_vanished, NULL, NULL);
}


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

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dbus.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-kwin-integration.h"

static DBusGProxy *s_pKwinAccelProxy = NULL;
static DBusGProxy *s_pPlasmaAccelProxy = NULL;

#define CD_KWIN_BUS "org.kde.kwin"
#define CD_KGLOBALACCEL_BUS "org.kde.kglobalaccel"
#define CD_KGLOBALACCEL_KWIN_OBJECT "/component/kwin"
#define CD_KGLOBALACCEL_PLASMA_OBJECT "/component/plasma_desktop"
#define CD_KGLOBALACCEL_INTERFACE "org.kde.kglobalaccel.Component"


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
	g_print ("%s (%s)\n", __func__, cClass);
	GList *pIcons = (GList*)cairo_dock_list_existing_appli_with_class (cClass);
	if (pIcons == NULL)
		return FALSE;
	
	Atom aPresentWindows = XInternAtom (cairo_dock_get_Xdisplay(), "_KDE_PRESENT_WINDOWS_GROUP", False);
	Window *data = g_new0 (Window, g_list_length (pIcons));
	Icon *pOneIcon;
	GList *ic;
	int i = 0;
	for (ic = pIcons; ic != NULL; ic = ic->next)
	{
		pOneIcon = ic->data;
		data[i++] = pOneIcon->Xid;
	}
	XChangeProperty(cairo_dock_get_Xdisplay(), data[0], aPresentWindows, aPresentWindows, 32, PropModeReplace, (unsigned char *)data, i);
	g_free (data);
	return TRUE;
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
			G_TYPE_STRING, "Show Dashboard",
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("Plasma-desktop 'Show Dashboard' error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return FALSE;
}


static void _register_kwin_backend (void)
{
	CairoDockWMBackend *p = g_new0 (CairoDockWMBackend, 1);
	
	p->present_class = present_class;
	p->present_windows = present_windows;
	p->present_desktops = present_desktops;
	p->show_widget_layer = show_widget_layer;
	p->set_on_widget_layer = NULL;  // the Dashboard is not a real widget layer :-/
	
	cairo_dock_wm_register_backend (p);
}

static void _unregister_kwin_backend (void)
{
	cairo_dock_wm_register_backend (NULL);
}

static void _on_kwin_owner_changed (gboolean bOwned, gpointer data)
{
	g_print ("Kwin is on the bus (%d)\n", bOwned);
	
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
	else if (s_pKwinAccelProxy != NULL)
	{
		g_object_unref (s_pKwinAccelProxy);
		s_pKwinAccelProxy = NULL;
		
		_unregister_kwin_backend ();
	}
}
static void _on_detect_kwin (gboolean bPresent, gpointer data)
{
	g_print ("Kwin is present: %d\n", bPresent);
	if (bPresent)
	{
		_on_kwin_owner_changed (TRUE, NULL);
	}
	cairo_dock_watch_dbus_name_owner (CD_KWIN_BUS,
		(CairoDockDbusNameOwnerChangedFunc) _on_kwin_owner_changed,
		NULL);
}
void cd_init_kwin_backend (void)
{
	DBusGProxyCall *call = cairo_dock_dbus_detect_application_async (CD_KWIN_BUS,
		(CairoDockOnAppliPresentOnDbus) _on_detect_kwin,
		NULL);
}

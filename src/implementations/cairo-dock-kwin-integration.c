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
	#ifdef HAVE_X11
	cd_debug ("%s (%s)", __func__, cClass);
	GList *pIcons = (GList*)cairo_dock_list_existing_appli_with_class (cClass);
	if (pIcons == NULL)
		return FALSE;
	Display *dpy = cairo_dock_get_X_display ();
	if (! dpy)
		return FALSE;
	
	Atom aPresentWindows = XInternAtom (dpy, "_KDE_PRESENT_WINDOWS_GROUP", False);
	Window *data = g_new0 (Window, g_list_length (pIcons));
	Icon *pOneIcon;
	GldiWindowActor *xactor;
	GList *ic;
	int i = 0;
	for (ic = pIcons; ic != NULL; ic = ic->next)
	{
		pOneIcon = ic->data;
		xactor = (GldiWindowActor*)pOneIcon->pAppli;
		data[i++] = gldi_window_get_id (xactor);
	}
	XChangeProperty(dpy, data[0], aPresentWindows, aPresentWindows, 32, PropModeReplace, (unsigned char *)data, i);
	g_free (data);
	return TRUE;
	#else
	(void)cClass;  // avoid unused parameter
	return FALSE;
	#endif
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
	return bSuccess;
}

#define x_icon_geometry(icon, pDock) (pDock->container.iWindowPositionX + icon->fXAtRest + (pDock->container.iWidth - pDock->fFlatDockWidth) / 2 + (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2))
#define y_icon_geometry(icon, pDock) (pDock->container.iWindowPositionY + icon->fDrawY - icon->fHeight * myIconsParam.fAmplitude * pDock->fMagnitudeMax)
/* Not used
static void _set_one_icon_geometry_for_window_manager (Icon *icon, CairoDock *pDock)
{
	cd_debug ("%s (%s)", __func__, icon?icon->cName:"none");
	long data[1+6];
	if (icon != NULL)
	{
		data[0] = 1;  // 1 preview.
		data[1+0] = 5;  // 5 elements for the current preview: X id, x, y, w, h
		data[1+1] = icon->Xid;
		
		int iX, iY;
		iX = x_icon_geometry (icon, pDock);
		iY = y_icon_geometry (icon, pDock);  // il faudrait un fYAtRest ...
		// iWidth = icon->fWidth;
		// iHeight = icon->fHeight * (1. + 2*myIconsParam.fAmplitude * pDock->fMagnitudeMax);  // on elargit en haut et en bas, pour gerer les cas ou l'icone grossirait vers le haut ou vers le bas.
		
		if (pDock->container.bIsHorizontal)
		{
			data[1+2] = iX;
			data[1+3] = (pDock->container.bDirectionUp ? -50 - 200: 50 + 200);
		}
		else
		{
			data[1+2] = iY;
			data[1+3] = iX - 200/2;
		}
		data[1+4] = 200;
		data[1+5] = 200;
	}
	else
	{
		data[0] = 0;
		
	}
	
	Atom atom = XInternAtom (gdk_x11_get_default_xdisplay(), "_KDE_WINDOW_PREVIEW", False);
	Window Xid = gldi_container_get_Xid (CAIRO_CONTAINER (pDock));
	XChangeProperty (gdk_x11_get_default_xdisplay(), Xid, atom, atom, 32, PropModeReplace, (const unsigned char*)data, 1+6);
}
*/
/* Not used
static gboolean _on_enter_icon (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bStartAnimation)
{
	if (CAIRO_DOCK_IS_APPLI (pIcon))
	{
		_set_one_icon_geometry_for_window_manager (pIcon, pDock);
	}
	else
	{
		_set_one_icon_geometry_for_window_manager (NULL, pDock);
	}
	return GLDI_NOTIFICATION_LET_PASS;
}
*/
static void _register_kwin_backend (void)
{
	GldiDesktopManagerBackend p;
	memset(&p, 0, sizeof (GldiDesktopManagerBackend));
	
	p.present_class = present_class;
	p.present_windows = present_windows;
	p.present_desktops = present_desktops;
	p.show_widget_layer = show_widget_layer;
	p.set_on_widget_layer = NULL;  // the Dashboard is not a real widget layer :-/
	
	gldi_desktop_manager_register_backend (&p, "KWin");
	
	/*gldi_object_register_notification (&myContainerObjectMgr,
		NOTIFICATION_ENTER_ICON,
		(GldiNotificationFunc) _on_enter_icon,
		GLDI_RUN_FIRST, NULL);*/
}

static void _unregister_kwin_backend (void)
{
	//cairo_dock_wm_register_backend (NULL);
	/*gldi_object_remove_notification (&myContainerObjectMgr,
		NOTIFICATION_ENTER_ICON,
		(GldiNotificationFunc) _on_enter_icon, NULL);*/
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
	else if (s_pKwinAccelProxy != NULL)
	{
		g_object_unref (s_pKwinAccelProxy);
		s_pKwinAccelProxy = NULL;
		
		_unregister_kwin_backend ();
	}
}
static void _on_detect_kwin (gboolean bPresent, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Kwin is present: %d", bPresent);
	if (bPresent)
	{
		_on_kwin_owner_changed (CD_KWIN_BUS, TRUE, NULL);
	}
	cairo_dock_watch_dbus_name_owner (CD_KWIN_BUS,
		(CairoDockDbusNameOwnerChangedFunc) _on_kwin_owner_changed,
		NULL);
}
void cd_init_kwin_backend (void)
{
	cairo_dock_dbus_detect_application_async (CD_KWIN_BUS,
		(CairoDockOnAppliPresentOnDbus) _on_detect_kwin,
		NULL);
}

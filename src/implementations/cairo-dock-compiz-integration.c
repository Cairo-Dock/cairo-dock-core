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
#include <X11/Xatom.h>
#include <gdk/gdkx.h>  // GDK_WINDOW_XID
#endif

#include "cairo-dock-log.h"
#include "cairo-dock-dbus.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"  // bIsHidden
#include "cairo-dock-icon-factory.h"  // pAppli
#include "cairo-dock-container.h"  // gldi_container_get_gdk_window
#include "cairo-dock-class-manager.h"
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_sync
#include "cairo-dock-X-utilities.h"  // cairo_dock_get_X_display, cairo_dock_change_nb_viewports
#include "cairo-dock-compiz-integration.h"

static DBusGProxy *s_pScaleProxy = NULL;
static DBusGProxy *s_pExposeProxy = NULL;
static DBusGProxy *s_pWidgetLayerProxy = NULL;
static DBusGProxy *s_pHSizeProxy = NULL;
static DBusGProxy *s_pVSizeProxy = NULL;

#ifdef HAVE_X11
static inline Window _get_root_Xid (void)
{
	Display *dpy = cairo_dock_get_X_display ();
	return (dpy ? DefaultRootWindow(dpy) : 0);
}
#else
#define _get_root_Xid(...) 0
#endif

static gboolean present_windows (void)
{
	int root = _get_root_Xid();
	if (! root)
		return FALSE;
	gboolean bSuccess = FALSE;
	if (s_pScaleProxy != NULL)
	{
		GError *erreur = NULL;
		bSuccess = dbus_g_proxy_call (s_pScaleProxy, "activate", &erreur,
			G_TYPE_STRING, "root",
			G_TYPE_INT, root,
			G_TYPE_STRING, "",
			G_TYPE_STRING, "",
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("compiz scale error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static gboolean present_class (const gchar *cClass)
{
	cd_debug ("%s (%s)", __func__, cClass);
	const GList *pIcons = cairo_dock_list_existing_appli_with_class (cClass);
	if (pIcons == NULL)
		return FALSE;
	
	gboolean bAllHidden = TRUE;
	Icon *pOneIcon;
	const GList *ic;
	for (ic = pIcons; ic != NULL; ic = ic->next)
	{
		pOneIcon = ic->data;
		bAllHidden &= pOneIcon->pAppli->bIsHidden;
	}
	if (bAllHidden)
		return FALSE;
	
	int root = _get_root_Xid();
	if (! root)
		return FALSE;
	
	gboolean bSuccess = FALSE;
	if (s_pScaleProxy != NULL)
	{
		GError *erreur = NULL;
		const gchar *cWmClass = cairo_dock_get_class_wm_class (cClass);
		gchar *cMatch;
		if (cWmClass)
			cMatch = g_strdup_printf ("class=%s", cWmClass);
		else
			cMatch = g_strdup_printf ("class=.%s*", cClass+1);
		cd_message ("Compiz: match '%s'", cMatch);
		bSuccess = dbus_g_proxy_call (s_pScaleProxy, "activate", &erreur,
			G_TYPE_STRING, "root",
			G_TYPE_INT, root,
			G_TYPE_STRING, "match",
			G_TYPE_STRING, cMatch,
			G_TYPE_INVALID,
			G_TYPE_INVALID);  // in oldest version of Compiz (< 0.9.8), it doesn't present windows of other viewports
		g_free (cMatch);
		if (erreur)
		{
			cd_warning ("compiz scale error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static gboolean present_desktops (void)
{
	int root = _get_root_Xid();
	if (! root)
		return FALSE;
	
	gboolean bSuccess = FALSE;
	if (s_pExposeProxy != NULL)
	{
		GError *erreur = NULL;
		bSuccess = dbus_g_proxy_call (s_pExposeProxy, "activate", &erreur,
			G_TYPE_STRING, "root",
			G_TYPE_INT, root,
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("compiz expo error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static gboolean show_widget_layer (void)
{
	int root = _get_root_Xid();
	if (! root)
		return FALSE;
	
	gboolean bSuccess = FALSE;
	if (s_pWidgetLayerProxy != NULL)
	{
		GError *erreur = NULL;
		bSuccess = dbus_g_proxy_call (s_pWidgetLayerProxy, "activate", &erreur,
			G_TYPE_STRING, "root",
			G_TYPE_INT, root,
			G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (erreur)
		{
			cd_warning ("compiz widget layer error: %s", erreur->message);
			g_error_free (erreur);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

#ifdef HAVE_X11
static void _on_got_active_plugins (DBusGProxy *proxy, DBusGProxyCall *call_id, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("%s ()", __func__);
	// get the active plug-ins.
	GError *error = NULL;
	gchar **plugins = NULL;
	dbus_g_proxy_end_call (proxy,
		call_id,
		&error,
		G_TYPE_STRV,
		&plugins,
		G_TYPE_INVALID);
	if (error)
	{
		cd_warning ("compiz active plug-ins error: %s", error->message);
		g_error_free (error);
		return;
	}
	g_return_if_fail (plugins != NULL);
	
	// look for the 'widget' plug-in.
	gboolean bFound = FALSE;
	int i;
	for (i = 0; plugins[i] != NULL; i++)
	{
		if (strcmp (plugins[i], "widget") == 0)
		{
			bFound = TRUE;
			break;
		}
	}
	
	// if not present, add it to the list and send it back to Compiz.
	if (!bFound)  // then we parsed all the 'plugins' array, so i = nb-elements.
	{
		gchar **plugins2 = g_new0 (gchar*, i+2);  // +1 for 'widget' and +1 for NULL
		memcpy (plugins2, plugins, i * sizeof (gchar*));
		plugins2[i] = (gchar*)"widget";  // elements of 'plugins2' are not freed.

		if (cd_is_the_new_compiz ())
		{
			gchar *cPluginsList = g_strjoinv (",", plugins2);
			cd_debug ("Compiz Plugins List: %s", cPluginsList);
			cairo_dock_launch_command_printf ("bash "SHARE_DATA_DIR"/scripts/help_scripts.sh \"compiz_new_replace_list_plugins\" \"%s\"",
				NULL,
				cPluginsList);
			g_free (cPluginsList);
		}
		else
		{	// It seems it doesn't work with Compiz 0.9 :-? => compiz (core) - Warn: Can't set Value with type 12 to option "active_plugins" with type 11 (with dbus-send too...)
			dbus_g_proxy_call_no_reply (proxy,
				"set",
				G_TYPE_STRV,
				plugins2,
				G_TYPE_INVALID);
		}
		
		g_free (plugins2);  // elements belong to 'plugins' or are const.
	}
	g_strfreev (plugins);
}
static gboolean _check_widget_plugin (G_GNUC_UNUSED gpointer data)
{
	// first get the active plug-ins.
	DBusGProxy *pActivePluginsProxy = cairo_dock_create_new_session_proxy (
		CD_COMPIZ_BUS,
		cd_is_the_new_compiz () ?
			CD_COMPIZ_OBJECT"/core/screen0/active_plugins":
			CD_COMPIZ_OBJECT"/core/allscreens/active_plugins",
		CD_COMPIZ_INTERFACE);
	
	dbus_g_proxy_begin_call (pActivePluginsProxy,
		"get",
		(DBusGProxyCallNotify) _on_got_active_plugins,
		NULL,
		NULL,
		G_TYPE_INVALID);
	///g_object_unref (pActivePluginsProxy);
	
	return FALSE;
}
#endif  // else not used
static gboolean set_on_widget_layer (GldiContainer *pContainer, gboolean bOnWidgetLayer)
{
	#ifdef HAVE_X11
	static gboolean s_bChecked = TRUE;
	static Atom s_aCompizWidget = None;
	
	cd_debug ("%s ()", __func__);
	Window Xid = GDK_WINDOW_XID (gldi_container_get_gdk_window(pContainer));
	Display *dpy = cairo_dock_get_X_display ();
	if (! dpy)
		return FALSE;
	if (s_aCompizWidget == None)
		s_aCompizWidget = XInternAtom (dpy, "_COMPIZ_WIDGET", False);
	
	if (bOnWidgetLayer)
	{
		// the first time, trigger a check to ensure the 'widget' plug-in is operationnal.
		if (s_bChecked)
		{
			g_timeout_add_seconds (2, _check_widget_plugin, NULL);
			s_bChecked = FALSE;
		}
		// set the _COMPIZ_WIDGET atom on the window to mark it.
		gulong widget = 1;
		XChangeProperty (dpy,
			Xid,
			s_aCompizWidget,
			XA_WINDOW, 32, PropModeReplace,
			(guchar *) &widget, 1);
	}
	else
	{
		XDeleteProperty (dpy,
			Xid,
			s_aCompizWidget);
	}
	return TRUE;
	#else
	(void)pContainer;  // avoid unused parameter
	(void)bOnWidgetLayer;  // avoid unused parameter
	return FALSE;
	#endif
}

/* Only add workspaces with Compiz: We shouldn't add desktops when using Compiz
 * and with this method, Compiz saves the new state
 */
static gboolean _compiz_set_nb_viewports (int X, int Y)
{
	gboolean bSuccess = FALSE;
	if (s_pHSizeProxy != NULL && s_pVSizeProxy != NULL)
	{
		GError *error = NULL;
		bSuccess = dbus_g_proxy_call (s_pHSizeProxy, "set", &error,
			G_TYPE_INT, X,
			G_TYPE_INVALID, G_TYPE_INVALID);
		if (error)
		{
			cd_warning ("compiz HSize error: %s", error->message);
			g_error_free (error);
			error = NULL;
			bSuccess = FALSE;
		}
		error = NULL;
		bSuccess &= dbus_g_proxy_call (s_pVSizeProxy, "set", &error,
			G_TYPE_INT, Y,
			G_TYPE_INVALID, G_TYPE_INVALID);
		if (error)
		{
			cd_warning ("compiz VSize error: %s", error->message);
			g_error_free (error);
			bSuccess = FALSE;
		}
	}
	return bSuccess;
}

static void _add_workspace (void)
{
	cairo_dock_change_nb_viewports (1, _compiz_set_nb_viewports);
}

static void _remove_workspace (void)
{
	cairo_dock_change_nb_viewports (-1, _compiz_set_nb_viewports);
}


static void _register_compiz_backend (void)
{
	GldiDesktopManagerBackend p;
	memset(&p, 0, sizeof (GldiDesktopManagerBackend));
	
	p.present_class = present_class;
	p.present_windows = present_windows;
	p.present_desktops = present_desktops;
	p.show_widget_layer = show_widget_layer;
	p.set_on_widget_layer = set_on_widget_layer;
	p.add_workspace = _add_workspace;
	p.remove_last_workspace = _remove_workspace;
	
	
	gldi_desktop_manager_register_backend (&p, "Compiz");
}

static void _unregister_compiz_backend (void)
{
	//cairo_dock_wm_register_backend (NULL);
}

gboolean cd_is_the_new_compiz (void)
{
	static gboolean s_bNewCompiz = FALSE;
	static gboolean s_bHasBeenChecked = FALSE;  // only check it once, as it's likely to not change.
	if (!s_bHasBeenChecked)
	{
		s_bHasBeenChecked = TRUE;
		gchar *cVersion = cairo_dock_launch_command_sync ("compiz --version");
		if (cVersion != NULL)
		{
			gchar *str = strchr (cVersion, ' ');  // "compiz 0.8.6"
			if (str != NULL)
			{
				int iMajorVersion, iMinorVersion, iMicroVersion;
				cairo_dock_get_version_from_string (str+1, &iMajorVersion, &iMinorVersion, &iMicroVersion);
				if (iMajorVersion > 0 || iMinorVersion > 8)
					s_bNewCompiz = TRUE;
			}
		}
		g_free (cVersion);
		cd_debug ("NewCompiz: %d", s_bNewCompiz);
	}
	return s_bNewCompiz;
}

static void _on_compiz_owner_changed (G_GNUC_UNUSED const gchar *cName, gboolean bOwned, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Compiz is on the bus (%d)", bOwned);
	
	if (bOwned)  // set up the proxies
	{
		g_return_if_fail (s_pScaleProxy == NULL);
		
		gboolean bNewCompiz = cd_is_the_new_compiz ();
		
		s_pScaleProxy = cairo_dock_create_new_session_proxy (
			CD_COMPIZ_BUS,
			bNewCompiz ?
				CD_COMPIZ_OBJECT"/scale/screen0/initiate_all_key":
				CD_COMPIZ_OBJECT"/scale/allscreens/initiate_all_key",
			CD_COMPIZ_INTERFACE);
		
		s_pExposeProxy = cairo_dock_create_new_session_proxy (
			CD_COMPIZ_BUS,
			bNewCompiz ?
				CD_COMPIZ_OBJECT"/expo/screen0/expo_button":
				CD_COMPIZ_OBJECT"/expo/allscreens/expo_button",
			CD_COMPIZ_INTERFACE);
		
		s_pWidgetLayerProxy = cairo_dock_create_new_session_proxy (
			CD_COMPIZ_BUS,
			bNewCompiz ?
				CD_COMPIZ_OBJECT"/widget/screen0/toggle_button":
				CD_COMPIZ_OBJECT"/widget/allscreens/toggle_button",
			CD_COMPIZ_INTERFACE);
		
		s_pHSizeProxy = cairo_dock_create_new_session_proxy (
			CD_COMPIZ_BUS,
			bNewCompiz ?
				CD_COMPIZ_OBJECT"/core/screen0/hsize":
				CD_COMPIZ_OBJECT"/core/allscreens/hsize",
			CD_COMPIZ_INTERFACE);
		
		s_pVSizeProxy = cairo_dock_create_new_session_proxy (
			CD_COMPIZ_BUS,
			bNewCompiz ?
				CD_COMPIZ_OBJECT"/core/screen0/vsize":
				CD_COMPIZ_OBJECT"/core/allscreens/vsize",
			CD_COMPIZ_INTERFACE);
		
		_register_compiz_backend ();
	}
	else if (s_pScaleProxy != NULL)
	{
		g_object_unref (s_pScaleProxy);
		s_pScaleProxy = NULL;
		g_object_unref (s_pExposeProxy);
		s_pExposeProxy = NULL;
		g_object_unref (s_pWidgetLayerProxy);
		s_pWidgetLayerProxy = NULL;
		g_object_unref (s_pHSizeProxy);
		s_pHSizeProxy = NULL;
		g_object_unref (s_pVSizeProxy);
		s_pVSizeProxy = NULL;
		
		_unregister_compiz_backend ();
	}
}
static void _on_detect_compiz (gboolean bPresent, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Compiz is present: %d", bPresent);
	if (bPresent)
	{
		_on_compiz_owner_changed (CD_COMPIZ_BUS, TRUE, NULL);
	}
	cairo_dock_watch_dbus_name_owner (CD_COMPIZ_BUS,
		(CairoDockDbusNameOwnerChangedFunc) _on_compiz_owner_changed,
		NULL);
}
void cd_init_compiz_backend (void)
{
	cairo_dock_dbus_detect_application_async (CD_COMPIZ_BUS,
		(CairoDockOnAppliPresentOnDbus) _on_detect_compiz,
		NULL);
}

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

#include <stdlib.h>
#include <string.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-gui-backend.h"
#include "implementations/cairo-dock-compiz-integration.h"
#include "applet-struct.h"
#include "applet-tips-dialog.h"
#include "applet-composite.h"
#include "applet-notifications.h"

#define CAIRO_DOCK_WIKI_URL "http://wiki.glx-dock.org" // it's in French if the locale is FR with Firefox. If not, the user can choose its language.

//\___________ Define here the action to be taken when the user left-clicks on your icon or on its subdock or your desklet. The icon and the container that were clicked are available through the macros CD_APPLET_CLICKED_ICON and CD_APPLET_CLICKED_CONTAINER. CD_APPLET_CLICKED_ICON may be NULL if the user clicked in the container but out of icons.
CD_APPLET_ON_CLICK_BEGIN
	if (myData.iSidGetParams == 0 && myData.iSidTestComposite == 0)  // if we're testing the composite, don't pop up a dialog that could disturb the composite dialog.
		cairo_dock_show_tips ();
	
CD_APPLET_ON_CLICK_END


//\___________ Same as ON_CLICK, but with middle-click.
CD_APPLET_ON_MIDDLE_CLICK_BEGIN
	cairo_dock_show_main_gui ();
	
CD_APPLET_ON_MIDDLE_CLICK_END



//\___________ Define here the entries you want to add to the menu when the user right-clicks on your icon or on its subdock or your desklet. The icon and the container that were clicked are available through the macros CD_APPLET_CLICKED_ICON and CD_APPLET_CLICKED_CONTAINER. CD_APPLET_CLICKED_ICON may be NULL if the user clicked in the container but out of icons. The menu where you can add your entries is available throught the macro CD_APPLET_MY_MENU; you can add sub-menu to it if you want.
static void _cd_show_config (GtkMenuItem *menu_item, gpointer data)
{
	cairo_dock_show_main_gui ();
}

static void _cd_show_help_gui (GtkMenuItem *menu_item, gpointer data)
{
	cairo_dock_show_module_gui ("Help");
}
static void _launch_url (const gchar *cURL)
{
	if  (! cairo_dock_fm_launch_uri (cURL))
	{
		gchar *cCommand = g_strdup_printf ("\
which xdg-open > /dev/null && xdg-open %s || \
which firefox > /dev/null && firefox %s || \
which konqueror > /dev/null && konqueror %s || \
which iceweasel > /dev/null && konqueror %s || \
which opera > /dev/null && opera %s ",
			cURL,
			cURL,
			cURL,
			cURL,
			cURL);  // pas super beau mais efficace ^_^
		int r = system (cCommand);
		g_free (cCommand);
	}
}
static void _cd_show_help_online (GtkMenuItem *menu_item, gpointer data)
{
	_launch_url (CAIRO_DOCK_WIKI_URL);
}

static gboolean _is_gnome_panel_running (void) // only for Gnome2
{
	gboolean bResult = FALSE;
	gchar *cWhich = cairo_dock_launch_command_sync ("which gconftool-2");
	if (cWhich != NULL && *cWhich == '/')
	{
		gchar *cPanel = cairo_dock_launch_command_sync ("gconftool-2 -g '/desktop/gnome/session/required_components/panel'");
		if (cPanel && strcmp (cPanel, "gnome-panel") == 0)
			bResult = TRUE;
		g_free (cPanel);
	}
	g_free (cWhich);
	return bResult;
}

static void _cd_remove_gnome_panel (GtkMenuItem *menu_item, gpointer data)
{
	int r = system ("gconftool-2 -s '/desktop/gnome/session/required_components/panel' --type string \"\"");
}

static void _on_got_active_plugins (DBusGProxy *proxy, DBusGProxyCall *call_id, gpointer data)
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
	
	// look for the 'Unity' plug-in.
	gboolean bFound = FALSE;
	int i;
	for (i = 0; plugins[i] != NULL; i++)
	{
		cd_debug ("Compiz Plugin: %s", plugins[i]);
		if (strcmp (plugins[i], "unityshell") == 0)
		{
			bFound = TRUE;
			break;
		}
	}

	if (data != NULL)
	{	// only check if unity is Running
		data = &bFound;
		g_strfreev (plugins);
		return;
	}

	// if present, remove it from the list and send it back to Compiz.
	if (bFound)
	{
		g_free (plugins[i]);  // remove this entry
		plugins[i] = NULL;
		i ++;
		for (;plugins[i] != NULL; i++)  // move all other entries after it to the left.
		{
			plugins[i-1] = plugins[i];
			plugins[i] = NULL;
		}
		/*dbus_g_proxy_call_no_reply (proxy,
			"set",
			G_TYPE_STRV,
			plugins,
			G_TYPE_INVALID);*/ // It seems it doesn't work :-? => compiz (core) - Warn: Can't set Value with type 12 to option "active_plugins" with type 11 (with dbus-send too...)
		gchar *cPluginsList = g_strjoinv (",", plugins);
		cd_debug ("Compiz Plugins List: %s", cPluginsList);
		cairo_dock_launch_command_printf ("bash "MY_APPLET_SHARE_DATA_DIR"/scripts/help_scripts.sh \"compiz_new_replace_list_plugins\" \"%s\"",
			NULL,
			cPluginsList);
		g_free (cPluginsList);
	}
	else
	{
		cairo_dock_remove_dialog_if_any (myIcon);
		cairo_dock_show_temporary_dialog_with_icon (D_("Unity is already disabled."),
			myIcon, myContainer, 10e3, "same icon");
	}
	g_strfreev (plugins);
}

static void _cd_remove_unity (GtkMenuItem *menu_item, gpointer data)
{
	// first get the active plug-ins.
	DBusGProxy *pActivePluginsProxy = cairo_dock_create_new_session_proxy (
		CD_COMPIZ_BUS,
		CD_COMPIZ_OBJECT"/core/screen0/active_plugins",
		CD_COMPIZ_INTERFACE);
	
	dbus_g_proxy_begin_call (pActivePluginsProxy,
		"get",
		(DBusGProxyCallNotify) _on_got_active_plugins,
		NULL,
		NULL,
		G_TYPE_INVALID);
	///g_object_unref (pActivePluginsProxy);
}

static gboolean _is_unity_running (void)
{
	if (cd_is_the_new_compiz ())
		return FALSE; // it's just to not have useless warning (but it will launch 'compiz --version' command)

	DBusGProxy *pActivePluginsProxy = cairo_dock_create_new_session_proxy (
		CD_COMPIZ_BUS,
		CD_COMPIZ_OBJECT"/core/screen0/active_plugins",
		CD_COMPIZ_INTERFACE);

	gboolean bResult = FALSE;

	dbus_g_proxy_begin_call (pActivePluginsProxy,
		"get",
		(DBusGProxyCallNotify) _on_got_active_plugins,
		&bResult, // data != NULL => only check if unity is running
		NULL,
		G_TYPE_INVALID);

	return bResult;
}


CD_APPLET_ON_BUILD_MENU_BEGIN
	GtkWidget *pSubMenu = CD_APPLET_CREATE_MY_SUB_MENU ();
		CD_APPLET_ADD_ABOUT_IN_MENU (pSubMenu);
	gchar *cLabel = g_strdup_printf ("%s (%s)", D_("Open global settings"), D_("middle-click"));
	CD_APPLET_ADD_IN_MENU_WITH_STOCK (cLabel, GTK_STOCK_PREFERENCES, _cd_show_config, CD_APPLET_MY_MENU);
	g_free (cLabel);
	GdkScreen *pScreen = gdk_screen_get_default ();
	if (! gdk_screen_is_composited (pScreen))
		CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Activate composite"), GTK_STOCK_EXECUTE, cd_help_enable_composite, pSubMenu);
	if (_is_gnome_panel_running ())
		CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Disable the gnome-panel"), GTK_STOCK_REMOVE, _cd_remove_gnome_panel, pSubMenu);
	if (_is_unity_running ())
		CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Disable Unity"), GTK_STOCK_REMOVE, _cd_remove_unity, pSubMenu);
	CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Help"), GTK_STOCK_HELP, _cd_show_help_gui, CD_APPLET_MY_MENU);
	CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Online help"), GTK_STOCK_HELP, _cd_show_help_online, CD_APPLET_MY_MENU);
CD_APPLET_ON_BUILD_MENU_END

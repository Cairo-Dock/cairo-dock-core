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

#define CAIRO_DOCK_WIKI_URL "https://github.com/Cairo-Dock/cairo-dock-core/wiki" // new Wiki


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
static void _cd_show_config (G_GNUC_UNUSED GtkMenuItem *menu_item, G_GNUC_UNUSED gpointer data)
{
	cairo_dock_show_main_gui ();
}

static void _cd_show_help_gui (G_GNUC_UNUSED GtkMenuItem *menu_item, G_GNUC_UNUSED gpointer data)
{
	cairo_dock_show_items_gui (myIcon, myContainer, myApplet, -1);
}

static void _launch_url (const gchar *cURL)
{
	if  (! cairo_dock_fm_launch_uri (cURL))
	{
		gchar *cCommand = g_strdup_printf ("\
which xdg-open > /dev/null && xdg-open \"%s\" & || \
which firefox > /dev/null && firefox \"%s\" & || \
which konqueror > /dev/null && konqueror \"%s\" & || \
which iceweasel > /dev/null && iceweasel \"%s\" & || \
which chromium-browser > /dev/null && chromium-browser \"%s\" & || \
which midori > /dev/null && midori \"%s\" & || \
which epiphany > /dev/null && epiphany \"%s\" & || \
which opera > /dev/null && opera \"%s\" &",
			cURL,
			cURL,
			cURL,
			cURL,
			cURL,
			cURL,
			cURL,
			cURL);  // not very nice but it works ^_^
		int r = system (cCommand);
		if (r < 0)
			cd_warning ("Not able to launch this command: %s", cCommand);
		g_free (cCommand);
	}
}
static void _cd_show_help_online (G_GNUC_UNUSED GtkMenuItem *menu_item, G_GNUC_UNUSED gpointer data)
{
	_launch_url (CAIRO_DOCK_WIKI_URL);
}


CD_APPLET_ON_BUILD_MENU_BEGIN
	gchar *cLabel = g_strdup_printf ("%s (%s)", D_("Open global settings"), D_("middle-click"));
	CD_APPLET_ADD_IN_MENU_WITH_STOCK (cLabel, GLDI_ICON_NAME_PREFERENCES, _cd_show_config, CD_APPLET_MY_MENU);
	g_free (cLabel);
	GdkScreen *pScreen = gdk_screen_get_default ();
	if (! gdk_screen_is_composited (pScreen))
		CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Activate composite"), GLDI_ICON_NAME_EXECUTE, cd_help_enable_composite, CD_APPLET_MY_MENU);
	CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Help"), GLDI_ICON_NAME_HELP, _cd_show_help_gui, CD_APPLET_MY_MENU);
	CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Online help"), GLDI_ICON_NAME_HELP, _cd_show_help_online, CD_APPLET_MY_MENU);
CD_APPLET_ON_BUILD_MENU_END

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

#include <string.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "cairo-dock-config.h"
#include "cairo-dock-gui-main.h"
#include "cairo-dock-gui-simple.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-switch.h"

extern gchar *g_cCairoDockDataDir;

static gboolean s_bAdvancedMode = FALSE;
void cairo_dock_load_user_gui_backend (int iMode)  // 0 = simple
{
	if (iMode == 1)
	{
		cairo_dock_register_main_gui_backend ();
		s_bAdvancedMode = TRUE;
	}
	else
	{
		cairo_dock_register_simple_gui_backend ();
		s_bAdvancedMode = FALSE;
	}
}

static void on_click_switch_mode (GtkButton *button, gpointer data)
{
	cairo_dock_close_gui ();
	
	int iNewMode = (s_bAdvancedMode ? 0 : 1);
	
	gchar *cConfFilePath = g_strdup_printf ("%s/.cairo-dock", g_cCairoDockDataDir);
	cairo_dock_update_conf_file (cConfFilePath,
		G_TYPE_INT, "Gui", "mode", iNewMode,
		G_TYPE_INVALID);
	g_free (cConfFilePath);
	
	cairo_dock_load_user_gui_backend (iNewMode);
	
	cairo_dock_show_main_gui ();
}
GtkWidget *cairo_dock_make_switch_gui_button (void)
{
	GtkWidget *pSwitchButton = gtk_button_new_with_label (s_bAdvancedMode ? _("Simple Mode") : _("Advanced Mode"));
	if (!s_bAdvancedMode)
		gtk_widget_set_tooltip_text (pSwitchButton, _("The advanced mode lets you tweak every single parameter of the dock. It is a powerful tool to customise your current theme."));
	
	GtkWidget *pImage = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (pSwitchButton), pImage);
	g_signal_connect (G_OBJECT (pSwitchButton), "clicked", G_CALLBACK(on_click_switch_mode), NULL);
	return pSwitchButton;
}

/*
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


#ifndef __CAIRO_DOCK_GUI_COMMONS__
#define  __CAIRO_DOCK_GUI_COMMONS__

#include <glib.h>
#include <gtk/gtk.h>

/**
*@file cairo-dock-gui-commons.h Common helpers for GUI management.
*/


G_BEGIN_DECLS

void cairo_dock_update_desklet_widgets (CairoDesklet *pDesklet, GSList *pWidgetList);

void cairo_dock_update_desklet_visibility_widgets (CairoDesklet *pDesklet, GSList *pWidgetList);

void cairo_dock_update_is_detached_widget (gboolean bIsDetached, GSList *pWidgetList);


/**
 * Popup a menu under the given widget.
 * Can be used as a callback for the clicked event.
 * @param pWidget The widget whose position and size will be considered.
 *   Can be NULL, the menu will pop under the mouse.
 * @param pMenu The given menu to popup.
 * @code
 * g_signal_connect (
 * 	G_OBJECT (pButton),
 * 	"clicked",
 * 	G_CALLBACK (cairo_dock_popup_menu_under_widget),
 * 	GTK_MENU (pMenu)
 * 	);
 * @endcode
 */
void cairo_dock_popup_menu_under_widget (GtkWidget *pWidget, GtkMenu *pMenu);

gchar *cairo_dock_get_third_party_applets_link (void);

/**
 * Look for an icon for GUI
 * @param cGroupName The name of the module
 * @param cIcon The name of an icon, or its path, or a GTK icon
 * @param cShareDataDir The directory of the module (where we can find an 'icon' file)
 * @param iSize The best size for the new icon
 * @param bFastLoad To not check if the file exists or not
 */
gchar *cairo_dock_get_icon_for_gui (const gchar *cGroupName, const gchar *cIcon, const gchar *cShareDataDir, gint iSize, gboolean bFastLoad);

G_END_DECLS
#endif

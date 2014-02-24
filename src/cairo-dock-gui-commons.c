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
#include <sys/stat.h>
#define __USE_POSIX
#include <time.h>
#include <glib/gstdio.h>

#include "config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-desklet-factory.h"  // cairo_dock_desklet_is_sticky
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-task.h"
#include "cairo-dock-log.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_get_width
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-gui-commons.h"
#include "cairo-dock-icon-manager.h" // cairo_dock_search_icon_s_path

#define CAIRO_DOCK_PLUGINS_EXTRAS_URL "http://extras.glx-dock.org"

extern gchar *g_cCairoDockDataDir;


void cairo_dock_update_desklet_widgets (CairoDesklet *pDesklet, GSList *pWidgetList)
{
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GtkWidget *pOneWidget;
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "locked");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), pDesklet->bPositionLocked);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "size");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	g_signal_handlers_block_matched (pOneWidget,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), pDesklet->container.iWidth);
	g_signal_handlers_unblock_matched (pOneWidget,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
	if (pGroupKeyWidget->pSubWidgetList->next != NULL)
	{
		pOneWidget = pGroupKeyWidget->pSubWidgetList->next->data;
		g_signal_handlers_block_matched (pOneWidget,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), pDesklet->container.iHeight);
		g_signal_handlers_unblock_matched (pOneWidget,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
	}
	
	int iRelativePositionX = (pDesklet->container.iWindowPositionX + pDesklet->container.iWidth/2 <= gldi_desktop_get_width()/2 ? pDesklet->container.iWindowPositionX : pDesklet->container.iWindowPositionX - gldi_desktop_get_width());
	int iRelativePositionY = (pDesklet->container.iWindowPositionY + pDesklet->container.iHeight/2 <= gldi_desktop_get_height()/2 ? pDesklet->container.iWindowPositionY : pDesklet->container.iWindowPositionY - gldi_desktop_get_height());
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "x position");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), iRelativePositionX);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "y position");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), iRelativePositionY);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "rotation");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_range_set_value (GTK_RANGE (pOneWidget), pDesklet->fRotation/G_PI*180.);
}

void cairo_dock_update_desklet_visibility_widgets (CairoDesklet *pDesklet, GSList *pWidgetList)
{
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GtkWidget *pOneWidget;
	gboolean bIsSticky = gldi_desklet_is_sticky (pDesklet);
	CairoDeskletVisibility iVisibility = pDesklet->iVisibility;
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "accessibility");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_combo_box_set_active (GTK_COMBO_BOX (pOneWidget), iVisibility);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "sticky");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), bIsSticky);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "locked");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), pDesklet->bPositionLocked);
}

void cairo_dock_update_is_detached_widget (gboolean bIsDetached, GSList *pWidgetList)
{
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GtkWidget *pOneWidget;
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "initially detached");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), bIsDetached);
}


#define DISTANT_DIR "3.3.0"
gchar *cairo_dock_get_third_party_applets_link (void)
{
	return g_strdup_printf (CAIRO_DOCK_PLUGINS_EXTRAS_URL"/"DISTANT_DIR);
}


/**
 * Return absolute position of the bottom left corner of a widget to place a menu.
 * use with @see cairo_dock_place_menu_at_position
 * see @see cairo_dock_popup_menu_under_widget for an example
 */
GtkRequisition *cairo_dock_get_widget_bottom_position (GtkWidget *pWidget)
{
	GtkRequisition *pPosition = g_new (GtkRequisition, 1);
	gint dx = 0, dy = 0;
	GtkRequisition req;
	
	// Top left drawable window coordinates on screen. (inside decorations)
	// We need to get the toplevel widget origin. 
	// Otherwise, it would give wrong results if called on the widget in a scrolled area.
	// (drawable surface origin can even be out of the screen).
	GtkWidget *pTopLevel = gtk_widget_get_toplevel (pWidget);
	gdk_window_get_origin (gtk_widget_get_window (pTopLevel), &pPosition->width, &pPosition->height);
	
	// Widget position inside the window.
	gtk_widget_translate_coordinates (pWidget, pTopLevel, 0, 0, &dx, &dy);
	
	// Widget height.
	#if (GTK_MAJOR_VERSION < 3)
		gtk_widget_size_request (pWidget, &req);
	#else
		gtk_widget_get_preferred_size (pWidget, &req, NULL);
	#endif
	
	pPosition->width += dx;
	pPosition->height += dy + req.height;

	return pPosition;
}


/**
 * Convenient GtkMenuPositionFunc callback function to position menu.
 * use with @see cairo_dock_get_widget_bottom_position
 */
void cairo_dock_place_menu_at_position (G_GNUC_UNUSED GtkMenu *menu, gint *x, gint *y, gboolean *push_in, GtkRequisition *pPosition)
{
	*push_in = TRUE;
	if (pPosition != NULL)
	{
		*x = pPosition->width;
		*y = pPosition->height;
	}
}


/**
 * Callback for the button-press-event that popup the given menu.
 */
void cairo_dock_popup_menu_under_widget (GtkWidget *pWidget, GtkMenu *pMenu)
{
	GtkRequisition *pPosition = pWidget == NULL ? NULL : cairo_dock_get_widget_bottom_position (pWidget);
	
	gtk_menu_popup (pMenu,
		NULL,
		NULL,
		(GtkMenuPositionFunc) cairo_dock_place_menu_at_position,
		pPosition,
		0, // pEventButton->button
		gtk_get_current_event_time ()); // pEventButton->time
	
	if (pPosition != NULL)
		g_free (pPosition);
}

/**
 * Look for an icon for GUI
 */
gchar * cairo_dock_get_icon_for_gui (const gchar *cGroupName, const gchar *cIcon, const gchar *cShareDataDir, gint iSize, gboolean bFastLoad)
{
	gchar *cIconPath = NULL;

	if (cIcon)
	{
		if (*cIcon == '/')  // on ecrase les chemins des icons d'applets.
		{
			if (bFastLoad)
				return g_strdup (cIcon);

			cIconPath = g_strdup_printf ("%s/config-panel/%s.png", g_cCairoDockDataDir, cGroupName);
			if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS))
			{
				g_free (cIconPath);
				cIconPath = g_strdup (cIcon);
			}
		}
		else if (strncmp (cIcon, "gtk-", 4) == 0)
		{
			cIconPath = g_strdup (cIcon);
		}
		else  // categorie ou module interne.
		{
			if (! bFastLoad)
			{
				cIconPath = g_strconcat (g_cCairoDockDataDir, "/config-panel/", cIcon, NULL);
				if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS))
				{
					g_free (cIconPath);
					cIconPath = g_strconcat (CAIRO_DOCK_SHARE_DATA_DIR"/icons/", cIcon, NULL);
					if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS)) // if we just have the name of an application
					{
						g_free (cIconPath);
						cIconPath = NULL;
					}
				}
			}
			if (cIconPath == NULL)
			{
				cIconPath = cairo_dock_search_icon_s_path (cIcon, iSize);
				if (cIconPath == NULL) // maybe we don't have any icon with this name
					cIconPath = cShareDataDir ? g_strdup_printf ("%s/icon", cShareDataDir) : g_strdup (cIcon);
			}
		}
	}
	else if (cShareDataDir)
		cIconPath = g_strdup_printf ("%s/icon", cShareDataDir);

	return cIconPath;
}

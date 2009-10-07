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


#ifndef __CAIRO_DOCK_GUI_CALLBACKS__
#define  __CAIRO_DOCK_GUI_CALLBACKS__

#include <gtk/gtk.h>
#include "cairo-dock-gui-manager.h"

G_BEGIN_DECLS


void on_click_category_button (GtkButton *button, gpointer data);
void on_click_all_button (GtkButton *button, gpointer data);
void on_click_back_button (GtkButton *button, gpointer data);

void on_click_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription);
void on_enter_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription);
void on_leave_group_button (GtkButton *button, gpointer *data);


void on_click_apply (GtkButton *button, GtkWidget *pWindow);
void on_click_ok (GtkButton *button, GtkWidget *pWindow);
void on_click_quit (GtkButton *button, GtkWidget *pWindow);
gboolean on_delete_main_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop);

void on_click_activate_given_group (GtkToggleButton *button, CairoDockGroupDescription *pGroupDescription);
void on_click_activate_current_group (GtkToggleButton *button, gpointer *data);


void on_click_normal_apply (GtkButton *button, GtkWidget *pWindow);
void on_click_normal_ok (GtkButton *button, GtkWidget *pWindow);
void on_click_normal_quit (GtkButton *button, GtkWidget *pWindow);
gboolean on_delete_normal_gui (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop);


void on_click_launcher_apply (GtkButton *button, GtkWidget *pWindow);
void on_click_launcher_quit (GtkButton *button, GtkWidget *pWindow);
gboolean on_delete_launcher_gui (GtkWidget *pWidget, GdkEvent *event, gpointer data);



void cairo_dock_reset_filter_state (void);
void cairo_dock_activate_filter (GtkEntry *pEntry, gpointer data);
void cairo_dock_toggle_all_words (GtkToggleButton *pButton, gpointer data);
void cairo_dock_toggle_search_in_tooltip (GtkToggleButton *pButton, gpointer data);
void cairo_dock_toggle_highlight_words (GtkToggleButton *pButton, gpointer data);
void cairo_dock_toggle_hide_others (GtkToggleButton *pButton, gpointer data);
void cairo_dock_clear_filter (GtkButton *pButton, GtkEntry *pEntry);


G_END_DECLS
#endif

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

#ifndef __CAIRO_DOCK_WIDGET_THEMES__
#define  __CAIRO_DOCK_WIDGET_THEMES__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
#include "cairo-dock-widget.h"
G_BEGIN_DECLS


typedef struct _ThemesWidget ThemesWidget;

struct _ThemesWidget {
	CDWidget widget;  // base class
	gchar *cInitConfFile;  // conf file used to build the widget
	GtkWindow *pMainWindow;  // main window, needed to make the waiting dialog modal
	GldiTask *pImportTask;  // task to import a theme from the server
	GldiTask *pListTask;  // task to list the themes on the server
	GtkWidget *pTreeView;  // tree view for the complete themes list
	GtkWidget *pCombo;  // combo for the user themes
	GtkWidget *pWaitingDialog;  // modal dialog to show a progress bar during importation
	guint iSidPulse;  // timer to make the progress bar move
};

#define THEMES_WIDGET(w) ((ThemesWidget*)(w))

#define IS_THEMES_WIDGET(w) (w && CD_WIDGET(w)->iType == WIDGET_THEMES)

ThemesWidget *cairo_dock_themes_widget_new (GtkWindow *pMainWindow);


G_END_DECLS
#endif

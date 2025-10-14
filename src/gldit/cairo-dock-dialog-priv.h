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


#ifndef __CAIRO_DIALOG_PRIV__
#define  __CAIRO_DIALOG_PRIV__

#include "cairo-dock-dialog-factory.h"
#include "cairo-dock-dialog-manager.h"
G_BEGIN_DECLS

#define CAIRO_DIALOG_FIRST_BUTTON 0
#define CAIRO_DIALOG_ENTER_KEY -1
#define CAIRO_DIALOG_ESCAPE_KEY -2
#define CAIRO_DIALOG_MIN_SIZE 20
#define CAIRO_DIALOG_TEXT_MARGIN 3
#define CAIRO_DIALOG_BUTTON_OFFSET 3
#define CAIRO_DIALOG_VGAP 4
#define CAIRO_DIALOG_BUTTON_GAP 16

/* Functions defined in cairo-dock-dialog-factory.c */

void gldi_dialog_init_internals (CairoDialog *pDialog, CairoDialogAttr *pAttribute);

// should be elsewhere (note: used by desklet-factory as well for widgets in desklets)
GtkWidget *cairo_dock_steal_widget_from_its_container (GtkWidget *pWidget);

void gldi_dialog_set_icon (CairoDialog *pDialog, const gchar *cImageFilePath);
void gldi_dialog_set_icon_surface (CairoDialog *pDialog, cairo_surface_t *pNewIconSurface, int iNewIconSize);


/* Functions defined in cairo-dock-dialog-manager.c */

void gldi_dialogs_refresh_all (void);

void gldi_dialogs_replace_all (void);

CairoDialog *gldi_dialogs_foreach (GCompareFunc callback, gpointer data);
/** Notify the dialog's dock that the dialog is hidden or destroyed.
 *  This generates a "leave" event for the mouse and "unfreezes" the
 *  dock as well.
 *@param pDialog the dialog being hidden or destroyed.
 */
void gldi_dialog_leave (CairoDialog *pDialog);

void gldi_register_dialogs_manager (void);

G_END_DECLS
#endif


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

#ifndef __CAIRO_DOCK_APPLICATION_FACTORY__
#define  __CAIRO_DOCK_APPLICATION_FACTORY__

#include <glib.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS


void cairo_dock_initialize_application_factory (Display *pXDisplay);

/** Create an icon that represents a window.
*@param Xid ID of the window.
*@param XParentWindow ID of the parent window set if the child window has been ignored but is demanding the attention.
*@return the newly allocated icon.
*/
G_GNUC_MALLOC Icon * cairo_dock_new_appli_icon (Window Xid, Window *XParentWindow);


G_END_DECLS
#endif

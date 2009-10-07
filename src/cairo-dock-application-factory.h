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

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cairo_dock_initialize_application_factory (Display *pXDisplay);

//void cairo_dock_unregister_pid (Icon *icon);

cairo_surface_t *cairo_dock_create_surface_from_xpixmap (Pixmap Xid, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight);
cairo_surface_t *cairo_dock_create_surface_from_xwindow (Window Xid, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight);

CairoDock *cairo_dock_manage_appli_class (Icon *icon, CairoDock *pMainDock);

Icon * cairo_dock_create_icon_from_xwindow (cairo_t *pSourceContext, Window Xid, CairoDock *pDock);


void cairo_dock_Xproperty_changed (Icon *icon, Atom aProperty, int iState, CairoDock *pDock);

void cairo_dock_appli_demands_attention (Icon *icon);
void cairo_dock_appli_stops_demanding_attention (Icon *icon);


G_END_DECLS
#endif

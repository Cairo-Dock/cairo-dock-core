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


#ifndef __CAIRO_DOCK_INTERNAL_DESKLETS__
#define  __CAIRO_DOCK_INTERNAL_DESKLETS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"


typedef struct _CairoConfigDesklets CairoConfigDesklets;
#ifndef _INTERNAL_MODULE_
extern CairoConfigDesklets myDesklets;
#endif
G_BEGIN_DECLS

struct _CairoConfigDesklets {
	gchar *cDeskletDecorationsName;
	gint iDeskletButtonSize;
	gchar *cRotateButtonImage;
	gchar *cRetachButtonImage;
	gchar *cDepthRotateButtonImage;
	gchar *cNoInputButtonImage;
	};


DEFINE_PRE_INIT (Desklets);

G_END_DECLS
#endif

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

#ifndef __CAIRO_STYLE_MANAGER__
#define  __CAIRO_STYLE_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-surface-factory.h"  // CairoDockLabelDescription
#include "cairo-dock-manager.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-style-manager.h This class defines the global style used by all widgets (Docks, Dialogs, Desklets, Menus, Icons).
* This includes background color, outline color, text color, linewidth, corner radius.
*
*/

// manager
typedef struct _GldiStyleParam GldiStyleParam;

#ifndef _MANAGER_DEF_
extern GldiStyleParam myStyleParam;
extern GldiManager myStyleMgr;
#endif

// params
struct _GldiStyleParam {
	gboolean bUseSystemColors;
	gdouble fBgColor[4];
	gdouble fLineColor[4];
	gint iLineWidth;
	gint iCornerRadius;
	GldiTextDescription textDescription;
	};

/// signals
typedef enum {
	/// notification called when the menu is being built on a container. data : {Icon, GldiContainer, GtkMenu, gboolean*}
	NOTIFICATION_STYLE_CHANGED,
	NB_NOTIFICATIONS_STYLE
	} GldiStyleNotifications;



void gldi_style_color_shade (double *icolor, double shade, double *ocolor);


void gldi_style_colors_freeze (void);

int gldi_style_colors_get_index (void);


void gldi_style_colors_set_bg_color (cairo_t *pCairoContext);

void gldi_style_colors_set_selected_bg_color (cairo_t *pCairoContext);

void gldi_style_colors_set_line_color (cairo_t *pCairoContext);

void gldi_style_colors_set_text_color (cairo_t *pCairoContext);

void gldi_style_colors_paint_bg_color (cairo_t *pCairoContext, int iWidth);


void gldi_register_style_manager (void);

G_END_DECLS
#endif

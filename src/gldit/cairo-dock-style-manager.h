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

#ifndef __GLDI_STYLE_MANAGER__
#define  __GLDI_STYLE_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-style-facility.h"  // GldiTextDescription
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
	/// notification called when the global style has changed
	NOTIFICATION_STYLE_CHANGED = NB_NOTIFICATIONS_OBJECT,
	NB_NOTIFICATIONS_STYLE
	} GldiStyleNotifications;


/** Get the value of a color. In case the color is actually a pattern, it gives its dominant color.
 * This function is really only useful when you need to have a color for sure (rather than potentially a pattern/texture), or when you need to apply the color with some transformation. Most of the time, you only want to use the gldi_style_colors_set_* functions.
*@param iColorType type of the color
*@param pColor output color
*/
void gldi_style_color_get (GldiStyleColors iColorType, GldiColor *pColor);


// block/unblock the change signal of the global style; call it before and after your code.
void gldi_style_colors_freeze (void);

// get the current stamp of the global style; each time the global style changes, the stamp is increased.
int gldi_style_colors_get_stamp (void);

/** Set the global background color on a context, with or without the alpha component.
*@param pCairoContext a context
*@param bUseAlpha TRUE to use the alpha, FALSE to set it fully opaque
*/
void gldi_style_colors_set_bg_color_full (cairo_t *pCairoContext, gboolean bUseAlpha);

/** Set the global background color on a context.
*@param pCairoContext a context
*/
#define gldi_style_colors_set_bg_color(pCairoContext) gldi_style_colors_set_bg_color_full (pCairoContext, TRUE)

/** Set the global selected color on a context.
*@param pCairoContext a context
*/
void gldi_style_colors_set_selected_bg_color (cairo_t *pCairoContext);

/** Set the global line color on a context.
*@param pCairoContext a context
*/
void gldi_style_colors_set_line_color (cairo_t *pCairoContext);

/** Set the global text color on a context.
*@param pCairoContext a context
*/
void gldi_style_colors_set_text_color (cairo_t *pCairoContext);

/** Set the global separator color on a context.
*@param pCairoContext a context
*/
void gldi_style_colors_set_separator_color (cairo_t *pCairoContext);

/** Set the global child color on a context.
*@param pCairoContext a context
*/
void gldi_style_colors_set_child_color (cairo_t *pCairoContext);

/** Paint a context with a horizontal alpha gradation. If the alpha is negative, the global style is used to find the alpha.
*@param pCairoContext a context
*@param iWidth width of the gradation
*@param fAlpha alpha to use
*/
void gldi_style_colors_paint_bg_color_with_alpha (cairo_t *pCairoContext, int iWidth, double fAlpha);

void gldi_register_style_manager (void);

G_END_DECLS
#endif

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

#ifndef __GLDI_STYLE_FACILITY__
#define  __GLDI_STYLE_FACILITY__

#include <glib.h>

#include "cairo-dock-struct.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-style-facility.h This file provides a few functions dealing with style elements like colors and text.
*
*/

/// Available types of color
typedef enum {
	GLDI_COLOR_BG,
	GLDI_COLOR_SELECTED,
	GLDI_COLOR_LINE,
	GLDI_COLOR_TEXT,
	GLDI_COLOR_SEPARATOR,
	GLDI_COLOR_CHILD,
	GLDI_NB_COLORS
	} GldiStyleColors;

struct _GldiColor {
	GdkRGBA rgba;  /// maybe we'll handle a double color later, to have simple linear patterns...
	};

/** Shade a color, making it darker if it's light, and lighter if it's dark. Note that the opposite behavior can be obtained by passing a negative shade value. Alpha is copied unchanged. Both pointers can be the same.
*@param icolor input color
*@param shade amount of light to add/remove, <= 1.
*@param ocolor output color
*/
void gldi_style_color_shade (GldiColor *icolor, double shade, GldiColor *ocolor);

gchar *_get_default_system_font (void);

void _get_color_from_pattern (cairo_pattern_t *pPattern, GldiColor *color);

#define gldi_color_set_cairo(pCairoContext, pColor) cairo_set_source_rgba (pCairoContext, (pColor)->rgba.red, (pColor)->rgba.green, (pColor)->rgba.blue, (pColor)->rgba.alpha)
#define gldi_color_set_cairo_rgb(pCairoContext, pColor) cairo_set_source_rgb (pCairoContext, (pColor)->rgba.red, (pColor)->rgba.green, (pColor)->rgba.blue)
#define gldi_color_set_opengl(pColor) glColor4f ((pColor)->rgba.red, (pColor)->rgba.green, (pColor)->rgba.blue, (pColor)->rgba.alpha)
#define gldi_color_set_opengl_rgb(pColor) glColor3f ((pColor)->rgba.red, (pColor)->rgba.green, (pColor)->rgba.blue)
#define gldi_color_compare(pColor1, pColor2) memcmp (pColor1, pColor2, sizeof(GldiColor))  // return 0 if equal


/// Description of the rendering of a text.
struct _GldiTextDescription {
	/// font.
	gchar *cFont;
	/// pango font
	PangoFontDescription *fd;
	/// size in pixels
	gint iSize;
	/// whether to draw the decorations (frame and outline) or not
	gboolean bNoDecorations;
	/// whether to use the default colors or the colors defined below
	gboolean bUseDefaultColors;
	/// text color
	GldiColor fColorStart;
	/// background color
	GldiColor fBackgroundColor;
	/// outline color
	GldiColor fLineColor;
	/// TRUE to stroke the outline of the characters (in black).
	gboolean bOutlined;
	/// margin around the text, it is also the dimension of the frame if available.
	gint iMargin;
	/// whether to use Pango markups or not (markups are html-like marks, like <b>...</b>; using markups force you to escape some characters like "&" -> "&amp;")
	gboolean bUseMarkup;
	/// maximum width allowed, in ratio of the screen's width. Carriage returns will be inserted if necessary. 0 means no limit.
	gdouble fMaxRelativeWidth;
};

void gldi_text_description_free (GldiTextDescription *pTextDescription);
void gldi_text_description_copy (GldiTextDescription *pDestTextDescription, GldiTextDescription *pOrigTextDescription);
GldiTextDescription *gldi_text_description_duplicate (GldiTextDescription *pTextDescription);

void gldi_text_description_reset (GldiTextDescription *pTextDescription);

void gldi_text_description_set_font (GldiTextDescription *pTextDescription, gchar *cFont);

#define gldi_text_description_get_size(pTextDescription) (pTextDescription)->iSize

#define gldi_text_description_get_description(pTextDescription) (pTextDescription)->fd


G_END_DECLS
#endif

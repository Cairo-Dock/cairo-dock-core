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

#include <stdlib.h>

#include "cairo-dock-log.h"
#include "cairo-dock-dialog-manager.h"  // myDialogsParam
#include "cairo-dock-style-colors.h"


/* simple config:
 * color: system / custom (= bg color, custom text color = white or black automatically, outline color = opaque bg color automatically)
 * 
 * system -> update params to system, don't modify colors
 * custom -> update params to custom, update colors
 * 
 * 
 * Dialogs & Menus: system / custom
 * bg color
 * text color
 * outline color
 * 
 * system -> get label fg color, menu bg color, menu-item bg color [, menu outline color]
 * custom -> css/cairo
 * 
 * Labels: auto / custom
 * bg color
 * text color
 * outline color
 * 
 * */

#if GTK_MAJOR_VERSION > 2
static const double alpha = 0.85;  // min alpha (max is 1)

static GtkStyleContext *s_pStyle = NULL;
static GdkRGBA s_menu_bg_color;
static cairo_pattern_t *s_menu_bg_pattern = NULL;
static GdkRGBA s_menuitem_bg_color;
static cairo_pattern_t *s_menuitem_bg_pattern = NULL;
static GdkRGBA s_text_color;
static int s_iMenuItemColorId = 1;
static gboolean s_bIgnoreStyleChange = FALSE;
#endif

static double hue2rgb (double p, double q, double t)
{
	if(t < 0) t += 1;
	if(t > 1) t -= 1;
	if(t < 1./6) return p + (q - p) * 6 * t;
	if(t < 1./2) return q;
	if(t < 2./3) return p + (q - p) * (2./3 - t) * 6;
	return p;
}
static void hslToRgb (double h, double s, double l, double *r, double *g, double *b)
{
	if (s == 0) // achromatic
	{
		*r = *g = *b = l;
	}
	else
	{
		double q = (l < 0.5 ? l * (1 + s) : l + s - l * s);
		double p = 2 * l - q;
		*r = hue2rgb(p, q, h + 1./3);
		*g = hue2rgb(p, q, h);
		*b = hue2rgb(p, q, h - 1./3);
	}
}
static void rgbToHsl (double r, double g, double b, double *h_, double *s_, double *l_)
{
	double max = MAX (MAX (r, g), b), min = MIN (MIN (r, g), b);
	double h, s, l = (max + min) / 2;

	if(max == min)  // achromatic
	{
		h = s = 0;
	}
	else
	{
		double d = max - min;
		s = (l > 0.5 ? d / (2 - max - min) : d / (max + min));
		if (max == r)
			h = (g - b) / d + (g < b ? 6 : 0);
		else if (max == g)
			h = (b - r) / d + 2;
		else
			h = (r - g) / d + 4;
		h /= 6;
	}

	*h_ = h;
	*s_ = s;
	*l_ = l;
}
void gldi_style_color_shade (double *icolor, double shade, double *ocolor)
{
	double h, s, l;
	rgbToHsl (icolor[0], icolor[1], icolor[2], &h, &s, &l);
	
	if (l > .5)
		l -= shade;
	else
		l += shade;
	
	hslToRgb (h, s, l, &ocolor[0], &ocolor[1], &ocolor[2]);
}

#if GTK_MAJOR_VERSION > 2
static void _on_style_changed (G_GNUC_UNUSED GtkStyleContext *_style, G_GNUC_UNUSED gpointer data)
{
	if (! s_bIgnoreStyleChange)
	{
		g_print ("style changed\n");
		if (s_menu_bg_pattern != NULL)
		{
			cairo_pattern_destroy (s_menu_bg_pattern);
			s_menu_bg_pattern = NULL;
		}
		if (s_menuitem_bg_pattern != NULL)
		{
			cairo_pattern_destroy (s_menuitem_bg_pattern);
			s_menuitem_bg_pattern = NULL;
		}
		s_iMenuItemColorId ++;  // invalidate menu-items' text color
		
		if (myDialogsParam.bUseSystemColors)
		{
			GtkStyleContext *style = gtk_style_context_new();
			gtk_style_context_set_screen (style, gdk_screen_get_default());
			GtkWidgetPath *path;
			int pos;
			
			// get text color
			path = gtk_widget_path_new();
			pos = gtk_widget_path_append_type (path, GTK_TYPE_MENU);
			gtk_widget_path_iter_add_class (path, pos, GTK_STYLE_CLASS_MENU);
			pos = gtk_widget_path_append_type (path, GTK_TYPE_MENU_ITEM);
			gtk_widget_path_iter_add_class (path, pos, GTK_STYLE_CLASS_MENUITEM);
			gtk_style_context_set_path (style, path);
			gtk_widget_path_free (path);
			gtk_style_context_add_class (style, GTK_STYLE_CLASS_MENU);
			gtk_style_context_add_class (style, GTK_STYLE_CLASS_MENUITEM);
			
			gtk_style_context_get_color (style, GTK_STATE_FLAG_NORMAL, &s_text_color);
			g_print ("text color: %.2f;%.2f;%.2f;%.2f\n", s_text_color.red, s_text_color.green, s_text_color.blue, s_text_color.alpha);
			
			// get selected bg color
			gtk_style_context_get_background_color (style, GTK_STATE_PRELIGHT, (GdkRGBA*)&s_menuitem_bg_color);
			if (s_menuitem_bg_color.alpha == 0)
			{
				gtk_style_context_get (style, GTK_STATE_FLAG_PRELIGHT,
					GTK_STYLE_PROPERTY_BACKGROUND_IMAGE, &s_menuitem_bg_pattern,
					NULL);
				if (s_menuitem_bg_pattern == NULL)
				{
					s_menuitem_bg_color.red = s_menuitem_bg_color.green = s_menuitem_bg_color.blue = s_menuitem_bg_color.alpha = 1.;
				}
			}
			g_print ("menuitem color: %.2f;%.2f;%.2f;%.2f; %p\n", s_menuitem_bg_color.red, s_menuitem_bg_color.green, s_menuitem_bg_color.blue, s_menuitem_bg_color.alpha, s_menuitem_bg_pattern);
			
			gtk_style_context_remove_class (style, GTK_STYLE_CLASS_MENUITEM);
			gtk_style_context_remove_class (style, GTK_STYLE_CLASS_MENU);
			
			// get bg color
			path = gtk_widget_path_new();
			pos = gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
			gtk_widget_path_iter_add_class (path, pos, GTK_STYLE_CLASS_BACKGROUND);
			pos = gtk_widget_path_append_type (path, GTK_TYPE_MENU);
			gtk_widget_path_iter_add_class (path, pos, GTK_STYLE_CLASS_MENU);
			gtk_style_context_set_path (style, path);
			gtk_widget_path_free (path);
			gtk_style_context_add_class (style, GTK_STYLE_CLASS_BACKGROUND);
			gtk_style_context_add_class (style, GTK_STYLE_CLASS_MENU);
			gtk_style_context_invalidate (style);  // force the context to be reconstructed
			
			gtk_style_context_get_background_color (style, GTK_STATE_FLAG_NORMAL, (GdkRGBA*)&s_menu_bg_color);
			if (s_menu_bg_color.alpha == 0)
			{
				gtk_style_context_get (style, GTK_STATE_NORMAL,
					GTK_STYLE_PROPERTY_BACKGROUND_IMAGE, &s_menu_bg_pattern,
					NULL);
				if (s_menu_bg_pattern == NULL)
				{
					s_menu_bg_color.red = s_menu_bg_color.green = s_menu_bg_color.blue = s_menu_bg_color.alpha = 1.;  // shouldn't happen
				}
			}
			g_print ("menu color: %.2f;%.2f;%.2f;%.2f; %p\n", s_menu_bg_color.red, s_menu_bg_color.green, s_menu_bg_color.blue, s_menu_bg_color.alpha, s_menu_bg_pattern);
			
			gtk_style_context_remove_class (style, GTK_STYLE_CLASS_MENU);
			gtk_style_context_remove_class (style, GTK_STYLE_CLASS_BACKGROUND);
			
			g_object_unref (style);
		}
	}
	else g_print (" style changed ignored\n");
}
#endif

void gldi_style_colors_freeze (void)
{
	#if GTK_MAJOR_VERSION > 2
	s_bIgnoreStyleChange = ! s_bIgnoreStyleChange;
	#endif
}

int gldi_style_colors_get_index (void)
{
	#if GTK_MAJOR_VERSION > 2
	return s_iMenuItemColorId;
	#else
	return 0;
	#endif
}

void gldi_style_colors_init (void)
{
	#if GTK_MAJOR_VERSION > 2
	if (s_pStyle != NULL)
		return;
	
	// init a style context
	s_pStyle = gtk_style_context_new();
	gtk_style_context_set_screen (s_pStyle, gdk_screen_get_default());
	g_signal_connect (s_pStyle, "changed", G_CALLBACK(_on_style_changed), NULL);
	#endif
}

void gldi_style_colors_reload (void)
{
	#if GTK_MAJOR_VERSION > 2
	_on_style_changed (s_pStyle, NULL);
	#endif
}


void gldi_style_colors_set_bg_color (cairo_t *pCairoContext)
{
	#if GTK_MAJOR_VERSION > 2
	if (myDialogsParam.bUseSystemColors)
	{
		if (s_menu_bg_pattern)
			cairo_set_source (pCairoContext, s_menu_bg_pattern);
		else
			cairo_set_source_rgba (pCairoContext, s_menu_bg_color.red, s_menu_bg_color.green, s_menu_bg_color.blue, s_menu_bg_color.alpha);
	}
	else
	#endif
	{
		cairo_set_source_rgba (pCairoContext, myDialogsParam.fDialogColor[0], myDialogsParam.fDialogColor[1], myDialogsParam.fDialogColor[2], myDialogsParam.fDialogColor[3]);
	}
}

void gldi_style_colors_set_selected_bg_color (cairo_t *pCairoContext)
{
	#if GTK_MAJOR_VERSION > 2
	if (myDialogsParam.bUseSystemColors)
	{
		if (s_menuitem_bg_pattern)
			cairo_set_source (pCairoContext, s_menuitem_bg_pattern);
		else
			cairo_set_source_rgba (pCairoContext, s_menuitem_bg_color.red, s_menuitem_bg_color.green, s_menuitem_bg_color.blue, 1.);
	}
	else
	#endif
	{
		double r = myDialogsParam.fDialogColor[0], g = myDialogsParam.fDialogColor[1], b = myDialogsParam.fDialogColor[2];
		double h, s, l;
		rgbToHsl (r, g, b, &h, &s, &l);
		if (l > .5)
			l -= .2;
		else
			l += .2;
		hslToRgb (h, s, l, &r, &g, &b);
		
		cairo_set_source_rgba (pCairoContext, r, g, b, 1.);
	}
}

void gldi_style_colors_set_line_color (cairo_t *pCairoContext)
{
	#if GTK_MAJOR_VERSION > 2
	if (myDialogsParam.bUseSystemColors)
	{
		if (s_menu_bg_pattern)
			cairo_set_source (pCairoContext, s_menu_bg_pattern);
		else
			cairo_set_source_rgb (pCairoContext, s_menu_bg_color.red, s_menu_bg_color.green, s_menu_bg_color.blue);  /// shade a little ?...
	}
	else
	#endif
	{
		cairo_set_source_rgba (pCairoContext, myDialogsParam.fLineColor[0], myDialogsParam.fLineColor[1], myDialogsParam.fLineColor[2], myDialogsParam.fLineColor[3]);
	}
}

void gldi_style_colors_set_text_color (cairo_t *pCairoContext)
{
	#if GTK_MAJOR_VERSION > 2
	if (myDialogsParam.bUseSystemColors)
	{
		cairo_set_source_rgb (pCairoContext, s_text_color.red, s_text_color.green, s_text_color.blue);
	}
	else
	#endif
	{
		cairo_set_source_rgb (pCairoContext, myDialogsParam.dialogTextDescription.fColorStart[0], myDialogsParam.dialogTextDescription.fColorStart[1], myDialogsParam.dialogTextDescription.fColorStart[2]);
	}
}

void gldi_style_colors_paint_bg_color (cairo_t *pCairoContext, int iWidth)
{
	// paint bg: 
	// pattern -> a=1
	// color -> if c.a == 1: a=.75 else c.a
	// or
	// option transparency
	// a=1: paint
	// else: mask
	#if GTK_MAJOR_VERSION > 2
	if (myDialogsParam.bUseSystemColors && s_menu_bg_pattern)
	{
		cairo_paint (pCairoContext);
	}
	else
	{
		cairo_pattern_t *pGradationPattern;
		pGradationPattern = cairo_pattern_create_linear (
			0, 0,
			iWidth, 0);
		cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
		cairo_pattern_add_color_stop_rgba (pGradationPattern,
			0.,
			1., 1., 1., 1.);
		cairo_pattern_add_color_stop_rgba (pGradationPattern,
			1.,
			1., 1., 1., alpha);  // bg color with horizontal alpha gradation
		cairo_mask (pCairoContext, pGradationPattern);
		
		cairo_pattern_destroy (pGradationPattern);
	}
	#else
	(void)iWidth;
	cairo_paint (pCairoContext);
	#endif
}

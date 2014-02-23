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

#include <cairo.h>
#include <gtk/gtk.h>
#include <GL/gl.h>

#include "cairo-dock-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_open_key_file
#define _MANAGER_DEF_
#include "cairo-dock-style-manager.h"

// public (manager, config, data)
GldiStyleParam myStyleParam;
GldiManager myStyleMgr;

// dependancies
extern gchar *g_cCurrentThemePath;

// private
#if GTK_MAJOR_VERSION > 2
static GtkStyleContext *s_pStyle = NULL;
static GdkRGBA s_menu_bg_color;
static cairo_pattern_t *s_menu_bg_pattern = NULL;
static GdkRGBA s_menuitem_bg_color;
static cairo_pattern_t *s_menuitem_bg_pattern = NULL;
static GdkRGBA s_text_color;
static int s_iStyleStamp = 1;
static gboolean s_bIgnoreStyleChange = FALSE;
#endif

/*
 * style mgr: bg color, line color, text color, linewidth, radius, font -> style
 * 
 * config reload / theme changed -> index ++, notify style changed -> icons mgr (reload labels, quickinfo), dock mgr (reload bg, update size), dialogs mgr (update size, reload text), clock/keyboard-indic (reload text buffers)
 * 
 * get config -> set font text desc -> fd, size
 * 
 * icons mgr::get config -> set font text desc or copy style mgr -> fd, size
 * 
 * clock::get config -> set font text desc or copy style mgr -> fd, size -> set weight
 * 
 * notification -> update text desc, reload text buffers
 * 
 * 
 * set_color(style) is style null, use system colors
 * or
 * if (default) set_color, else cairo_set_rgba
 * 
 * 
 * dialog mgr: auto/custom
 *   auto -> copy from style mgr or set use_system to TRUE
 *   custom -> get from config
 * or
 *   auto -> style = NULL, use style mgr's style
 *   custom -> fill a style
 * 
 * progressbars: auto/custom
 *   auto -> bg color = bg color -> bg color + .3, outline color = outline color
 *   custom -> bg color gradation, outline color
 * 
 * active indic: auto/custom/image, fill = frame / bg
 *   auto -> bg color = bg color or outline color
 *   custom -> bg color = color, radius, linewidth
 *   image -> image
 * 
 * docks: auto/gradation/image
 *   auto -> style mgr
 *   gradation -> bg colors __.--> outline color, radius, linewidth
 *   image -> image path ____/
 * 
 */

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
	if (s == 0)  // achromatic
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
	if (l > 1.) l = 1.;
	if (l < 0.) l = 0.;
	
	hslToRgb (h, s, l, &ocolor[0], &ocolor[1], &ocolor[2]);
}


#if GTK_MAJOR_VERSION > 2
static void _on_style_changed (G_GNUC_UNUSED GtkStyleContext *_style, gpointer data)
{
	if (! s_bIgnoreStyleChange)
	{
		cd_message ("style changed (%d)", GPOINTER_TO_INT (data));
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
		
		s_iStyleStamp ++;  // invalidate menu-items' text color
		
		if (myStyleParam.bUseSystemColors)
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
			cd_debug ("text color: %.2f;%.2f;%.2f;%.2f", s_text_color.red, s_text_color.green, s_text_color.blue, s_text_color.alpha);
			
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
			cd_debug ("menuitem color: %.2f;%.2f;%.2f;%.2f; %p", s_menuitem_bg_color.red, s_menuitem_bg_color.green, s_menuitem_bg_color.blue, s_menuitem_bg_color.alpha, s_menuitem_bg_pattern);
			
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
			cd_debug ("menu color: %.2f;%.2f;%.2f;%.2f; %p", s_menu_bg_color.red, s_menu_bg_color.green, s_menu_bg_color.blue, s_menu_bg_color.alpha, s_menu_bg_pattern);
			
			gtk_style_context_remove_class (style, GTK_STYLE_CLASS_MENU);
			gtk_style_context_remove_class (style, GTK_STYLE_CLASS_BACKGROUND);
			
			g_object_unref (style);
			
			gboolean bNotify = GPOINTER_TO_INT(data);
			if (bNotify && ! cairo_dock_is_loading())
				gldi_object_notify (&myStyleMgr, NOTIFICATION_STYLE_CHANGED);
		}
	}
	else cd_debug (" style changed ignored");
}
#endif

void gldi_style_colors_freeze (void)
{
	#if GTK_MAJOR_VERSION > 2
	s_bIgnoreStyleChange = ! s_bIgnoreStyleChange;
	#endif
}

int gldi_style_colors_get_stamp (void)
{
	#if GTK_MAJOR_VERSION > 2
	return s_iStyleStamp;
	#else
	return 0;
	#endif
}


void gldi_style_colors_set_bg_color_full (cairo_t *pCairoContext, gboolean bUseAlpha)
{
	#if GTK_MAJOR_VERSION > 2
	if (myStyleParam.bUseSystemColors)
	{
		if (pCairoContext)
		{
			if (s_menu_bg_pattern)
				cairo_set_source (pCairoContext, s_menu_bg_pattern);
			else
				cairo_set_source_rgba (pCairoContext, s_menu_bg_color.red, s_menu_bg_color.green, s_menu_bg_color.blue, bUseAlpha ? s_menu_bg_color.alpha : 1.);
		}
		else
		{
			/// TODO: if s_menu_bg_pattern != NULL, load it into a texture and apply it...
			glColor4f (s_menu_bg_color.red, s_menu_bg_color.green, s_menu_bg_color.blue, bUseAlpha ? s_menu_bg_color.alpha : 1.);
		}
	}
	else
	#endif
	{
		if (pCairoContext)
			cairo_set_source_rgba (pCairoContext, myStyleParam.fBgColor[0], myStyleParam.fBgColor[1], myStyleParam.fBgColor[2], bUseAlpha ? myStyleParam.fBgColor[3] : 1.);
		else
			glColor4f (myStyleParam.fBgColor[0], myStyleParam.fBgColor[1], myStyleParam.fBgColor[2], bUseAlpha ? myStyleParam.fBgColor[3] : 1.);
	}
}

void gldi_style_colors_set_selected_bg_color (cairo_t *pCairoContext)
{
	#if GTK_MAJOR_VERSION > 2
	if (myStyleParam.bUseSystemColors)
	{
		if (pCairoContext)
		{
			if (s_menuitem_bg_pattern)
				cairo_set_source (pCairoContext, s_menuitem_bg_pattern);
			else
				cairo_set_source_rgba (pCairoContext, s_menuitem_bg_color.red, s_menuitem_bg_color.green, s_menuitem_bg_color.blue, 1.);
		}
		else
		{
			glColor4f (s_menuitem_bg_color.red, s_menuitem_bg_color.green, s_menuitem_bg_color.blue, s_menuitem_bg_color.alpha);
		}
	}
	else
	#endif
	{
		double r = myStyleParam.fBgColor[0], g = myStyleParam.fBgColor[1], b = myStyleParam.fBgColor[2];
		double h, s, l;
		rgbToHsl (r, g, b, &h, &s, &l);
		if (l > .5)
			l -= .2;
		else
			l += .2;
		hslToRgb (h, s, l, &r, &g, &b);
		
		if (pCairoContext)
			cairo_set_source_rgba (pCairoContext, r, g, b, myStyleParam.fBgColor[3]);
		else
			glColor4f (r, g, b, myStyleParam.fBgColor[3]);
	}
}

void gldi_style_colors_set_line_color (cairo_t *pCairoContext)
{
	#if GTK_MAJOR_VERSION > 2
	if (myStyleParam.bUseSystemColors)
	{
		if (pCairoContext)
		{
			if (s_menu_bg_pattern)
				cairo_set_source (pCairoContext, s_menu_bg_pattern);
			else
				cairo_set_source_rgb (pCairoContext, s_menu_bg_color.red, s_menu_bg_color.green, s_menu_bg_color.blue);  /// shade a little ?...
		}
		else
		{
			glColor3f (s_menu_bg_color.red, s_menu_bg_color.green, s_menu_bg_color.blue);  /// shade a little ?...
		}
	}
	else
	#endif
	{
		if (pCairoContext)
			cairo_set_source_rgba (pCairoContext, myStyleParam.fLineColor[0], myStyleParam.fLineColor[1], myStyleParam.fLineColor[2], myStyleParam.fLineColor[3]);
		else
			glColor4f (myStyleParam.fLineColor[0], myStyleParam.fLineColor[1], myStyleParam.fLineColor[2], myStyleParam.fLineColor[3]);
	}
}

void gldi_style_colors_set_text_color (cairo_t *pCairoContext)
{
	#if GTK_MAJOR_VERSION > 2
	if (myStyleParam.bUseSystemColors)
	{
		cairo_set_source_rgb (pCairoContext, s_text_color.red, s_text_color.green, s_text_color.blue);
	}
	else
	#endif
	{
		cairo_set_source_rgb (pCairoContext, myStyleParam.textDescription.fColorStart[0], myStyleParam.textDescription.fColorStart[1], myStyleParam.textDescription.fColorStart[2]);
	}
}

void gldi_style_colors_paint_bg_color_with_alpha (cairo_t *pCairoContext, int iWidth, double fAlpha)
{
	#if GTK_MAJOR_VERSION > 2
	if (fAlpha < 0)  // alpha is not defined => take it from the global style
	{
		if (! (myStyleParam.bUseSystemColors && s_menu_bg_pattern))
		{
			fAlpha = (myStyleParam.bUseSystemColors ? s_menu_bg_color.alpha : myStyleParam.fBgColor[3]);
		}
	}
	if (fAlpha >= 0)  // alpha is now defined => use it
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
			1., 1., 1., fAlpha);  // bg color with horizontal alpha gradation
		cairo_mask (pCairoContext, pGradationPattern);
		
		cairo_pattern_destroy (pGradationPattern);
	}
	else
	{
		cairo_paint (pCairoContext);
	}
	#else
	(void)iWidth;
	if (fAlpha >= 0)
		cairo_paint_with_alpha (pCairoContext, fAlpha);
	else
		cairo_paint (pCairoContext);
	#endif
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	#if GTK_MAJOR_VERSION > 2
	if (s_pStyle != NULL)
		return;
	
	// init a style context
	s_pStyle = gtk_style_context_new();
	gtk_style_context_set_screen (s_pStyle, gdk_screen_get_default());
	g_signal_connect (s_pStyle, "changed", G_CALLBACK(_on_style_changed), GINT_TO_POINTER (TRUE));
	#endif
}

  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, GldiStyleParam *pStyleParam)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pStyleParam->bUseSystemColors = (cairo_dock_get_integer_key_value (pKeyFile, "Style", "colors", &bFlushConfFileNeeded, 1, NULL, NULL) == 0);
	
	if (! g_key_file_has_key (pKeyFile, "Style", "line color", NULL))  // old params (< 3.4)
	{
		// get the old params from the Dialog module's config
		gchar *cRenderingConfFile = g_strdup_printf ("%s/plug-ins/dialog-rendering/dialog-rendering.conf", g_cCurrentThemePath);
		GKeyFile *keyfile = cairo_dock_open_key_file (cRenderingConfFile);
		g_free (cRenderingConfFile);
		
		gchar *cRenderer = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "decorator", &bFlushConfFileNeeded, "comics", NULL, NULL);
		if (cRenderer)
		{
			cRenderer[0] = g_ascii_toupper (cRenderer[0]);
			
			cairo_dock_get_double_list_key_value (keyfile, cRenderer, "line color", &bFlushConfFileNeeded, pStyleParam->fLineColor, 4, NULL, NULL, NULL);
			g_key_file_set_double_list (pKeyFile, "Style", "line color", pStyleParam->fLineColor, 4);
			
			pStyleParam->iLineWidth = g_key_file_get_integer (keyfile, cRenderer, "border", NULL);
			g_key_file_set_integer (pKeyFile, "Style", "linewidth", pStyleParam->iLineWidth);
			
			pStyleParam->iCornerRadius = g_key_file_get_integer (keyfile, cRenderer, "corner", NULL);
			g_key_file_set_integer (pKeyFile, "Style", "corner", pStyleParam->iCornerRadius);
			
			g_free (cRenderer);
		}
		g_key_file_free (keyfile);
		
		bFlushConfFileNeeded = TRUE;
	}
	else
	{
		pStyleParam->iCornerRadius = g_key_file_get_integer (pKeyFile, "Style", "corner", NULL);
		pStyleParam->iLineWidth = g_key_file_get_integer (pKeyFile, "Style", "linewidth", NULL);
		cairo_dock_get_double_list_key_value (pKeyFile, "Style", "line color", &bFlushConfFileNeeded, pStyleParam->fLineColor, 4, NULL, NULL, NULL);
	}
	
	double bg_color[4] = {1.0, 1.0, 1.0, 0.7};
	cairo_dock_get_double_list_key_value (pKeyFile, "Style", "bg color", &bFlushConfFileNeeded, pStyleParam->fBgColor, 4, bg_color, "Dialogs", "background color");
	
	gboolean bCustomFont = cairo_dock_get_boolean_key_value (pKeyFile, "Style", "custom font", &bFlushConfFileNeeded, FALSE, "Dialogs", "custom");
	gchar *cFont = (bCustomFont ? cairo_dock_get_string_key_value (pKeyFile, "Style", "font", &bFlushConfFileNeeded, NULL, "Dialogs", "message police") : cairo_dock_get_default_system_font ());
	gldi_text_description_set_font (&pStyleParam->textDescription, cFont);
	
	double text_color[3] = {0., 0., 0.};
	cairo_dock_get_double_list_key_value (pKeyFile, "Style", "text color", &bFlushConfFileNeeded, pStyleParam->textDescription.fColorStart, 3, text_color, "Dialogs", "text color");
	
	return bFlushConfFileNeeded;
}

static void reset_config (GldiStyleParam *pStyleParam)
{
	gldi_text_description_reset (&pStyleParam->textDescription);
}

  ////////////
 /// LOAD ///
////////////

static void load (void)
{
	#if GTK_MAJOR_VERSION > 2
	if (myStyleParam.bUseSystemColors)
		_on_style_changed (s_pStyle, NULL);
	#endif
}

  //////////////
 /// RELOAD ///
//////////////

static void reload (GldiStyleParam *pPrevStyleParam, GldiStyleParam *pStyleParam)
{
	cd_message ("reload style mgr...");
	#if GTK_MAJOR_VERSION > 2
	if (pPrevStyleParam->bUseSystemColors != pStyleParam->bUseSystemColors)
		_on_style_changed (s_pStyle, NULL);  // load or invalidate the previous style
	else
		s_iStyleStamp ++;  // just invalidate
	#endif
	gldi_object_notify (&myStyleMgr, NOTIFICATION_STYLE_CHANGED);
}

  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	
}

  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_style_manager (void)
{
	// Manager
	memset (&myStyleMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myStyleMgr), &myManagerObjectMgr, NULL);
	myStyleMgr.cModuleName  = "Style";
	// interface
	myStyleMgr.init         = init;
	myStyleMgr.load         = load;
	myStyleMgr.unload       = unload;
	myStyleMgr.reload       = (GldiManagerReloadFunc)reload;
	myStyleMgr.get_config   = (GldiManagerGetConfigFunc)get_config;
	myStyleMgr.reset_config = (GldiManagerResetConfigFunc)reset_config;
	// Config
	myStyleMgr.pConfig = (GldiManagerConfigPtr)&myStyleParam;
	myStyleMgr.iSizeOfConfig = sizeof (GldiStyleParam);
	// data
	myStyleMgr.pData = (GldiManagerDataPtr)NULL;
	myStyleMgr.iSizeOfData = 0;
	
	// signals
	gldi_object_install_notifications (&myStyleMgr, NB_NOTIFICATIONS_STYLE);
}

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
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-surface-factory.h"  // cairo_dock_create_blank_surface
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_open_key_file
#define _MANAGER_DEF_
#include "cairo-dock-style-manager.h"

// public (manager, config, data)
GldiStyleParam myStyleParam;
GldiManager myStyleMgr;

// dependancies
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;

// private
static GtkStyleContext *s_pStyle = NULL;
static GdkRGBA s_menu_bg_color;
static cairo_pattern_t *s_menu_bg_pattern = NULL;
static GLuint s_menu_bg_texture = 0;
static GdkRGBA s_menuitem_bg_color;
static cairo_pattern_t *s_menuitem_bg_pattern = NULL;
static GdkRGBA s_text_color;
static int s_iStyleStamp = 1;
static gboolean s_bIgnoreStyleChange = FALSE;


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
		if (s_menu_bg_texture != 0)
		{
			_cairo_dock_delete_texture (s_menu_bg_texture);
			s_menu_bg_texture = 0;
		}
		if (s_menuitem_bg_pattern != NULL)
		{
			cairo_pattern_destroy (s_menuitem_bg_pattern);
			s_menuitem_bg_pattern = NULL;
		}
		
		s_iStyleStamp ++;  // invalidate previous style
		
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
			#if ! GTK_CHECK_VERSION (3,12,0) // Style contexts are now invalidated automatically.
			gtk_style_context_invalidate (style);  // force the context to be reconstructed
			#endif

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
				else if (g_bUseOpenGL)
				{
					cairo_surface_t *pSurface = cairo_dock_create_blank_surface (20, 20);
					cairo_t *pCairoContext = cairo_create (pSurface);
					cairo_set_source (pCairoContext, s_menu_bg_pattern);
					cairo_paint (pCairoContext);
					cairo_destroy (pCairoContext);
					
					s_menu_bg_texture = cairo_dock_create_texture_from_surface (pSurface);
					cairo_surface_destroy (pSurface);
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

void gldi_style_colors_freeze (void)
{
	s_bIgnoreStyleChange = ! s_bIgnoreStyleChange;
}

int gldi_style_colors_get_stamp (void)
{
	return s_iStyleStamp;
}

static void _get_bg_color (GldiColor *pColor)
{
	if (myStyleParam.bUseSystemColors)
	{
		if (s_menu_bg_pattern)
			_get_color_from_pattern (s_menu_bg_pattern, pColor);
		else
			memcpy (&pColor->rgba, &s_menu_bg_color, sizeof(GdkRGBA));
	}
	else
	{
		memcpy (pColor, &myStyleParam.fBgColor, sizeof(GldiColor));
	}
}

void gldi_style_color_get (GldiStyleColors iColorType, GldiColor *pColor)
{
	switch (iColorType)
	{
		case GLDI_COLOR_BG:
			_get_bg_color (pColor);
		break;
		case GLDI_COLOR_SELECTED:
			if (myStyleParam.bUseSystemColors)
			{
				if (s_menuitem_bg_pattern)
					_get_color_from_pattern (s_menuitem_bg_pattern, pColor);
				else
					pColor->rgba = s_menuitem_bg_color;
			}
			else
			{
				gldi_style_color_shade (&myStyleParam.fBgColor, GLDI_COLOR_SHADE_MEDIUM, pColor);
			}
		break;
		case GLDI_COLOR_LINE:
			if (myStyleParam.bUseSystemColors)
			{
				if (s_menu_bg_pattern)
					_get_color_from_pattern (s_menu_bg_pattern, pColor);
				else
				{
					gldi_style_color_shade (&s_menu_bg_color, -GLDI_COLOR_SHADE_LIGHT, pColor);
				}
				pColor->rgba.alpha = 1.;
			}
			else
			{
				memcpy (pColor, &myStyleParam.fLineColor, sizeof(GldiColor));
			}
		break;
		case GLDI_COLOR_TEXT:
			if (myStyleParam.bUseSystemColors)
			{
				pColor->rgba = s_text_color;
			}
			else
			{
				memcpy (pColor, &myStyleParam.textDescription.fColorStart, sizeof(GldiColor));
				pColor->rgba.alpha = 1.;
			}
		break;
		case GLDI_COLOR_SEPARATOR:
			_get_bg_color (pColor);
			gldi_style_color_shade (pColor, GLDI_COLOR_SHADE_MEDIUM, pColor);
			pColor->rgba.alpha = 1.;
		break;
		case GLDI_COLOR_CHILD:
			_get_bg_color (pColor);
			gldi_style_color_shade (pColor, GLDI_COLOR_SHADE_STRONG, pColor);
		break;
		default:
		break;
	}
}

void gldi_style_colors_set_bg_color_full (cairo_t *pCairoContext, gboolean bUseAlpha)
{
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
			if (s_menu_bg_texture != 0)
			{
				_cairo_dock_enable_texture ();
				glColor4f (1., 1., 1., 1.);
				glBindTexture (GL_TEXTURE_2D, s_menu_bg_texture);
			}
			else
				glColor4f (s_menu_bg_color.red, s_menu_bg_color.green, s_menu_bg_color.blue, bUseAlpha ? s_menu_bg_color.alpha : 1.);
		}
	}
	else
	{
		if (pCairoContext)
		{
			cairo_set_source_rgba (pCairoContext, myStyleParam.fBgColor.rgba.red, myStyleParam.fBgColor.rgba.green, myStyleParam.fBgColor.rgba.blue, bUseAlpha ? myStyleParam.fBgColor.rgba.alpha : 1.);
		}
		else
			glColor4f (myStyleParam.fBgColor.rgba.red, myStyleParam.fBgColor.rgba.green, myStyleParam.fBgColor.rgba.blue, bUseAlpha ? myStyleParam.fBgColor.rgba.alpha : 1.);
	}
}

void gldi_style_colors_set_selected_bg_color (cairo_t *pCairoContext)
{
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
	{
		GldiColor color;
		gldi_style_color_shade (&myStyleParam.fBgColor, GLDI_COLOR_SHADE_MEDIUM, &color);
		if (pCairoContext)
			gldi_color_set_cairo (pCairoContext, &color);
		else
			gldi_color_set_opengl (&color);
	}
}

void gldi_style_colors_set_line_color (cairo_t *pCairoContext)
{
	if (myStyleParam.bUseSystemColors)
	{
		if (pCairoContext)
		{
			if (s_menu_bg_pattern)
				cairo_set_source (pCairoContext, s_menu_bg_pattern);
			else
			{
				GldiColor color;
				gldi_style_color_shade (&s_menu_bg_color, -GLDI_COLOR_SHADE_LIGHT, &color);
				cairo_set_source_rgb (pCairoContext, color.rgba.red, color.rgba.green, color.rgba.blue);
			}
		}
		else
		{
			GldiColor color;
			gldi_style_color_shade (&s_menu_bg_color, -GLDI_COLOR_SHADE_LIGHT, &color);
			glColor3f (color.rgba.red, color.rgba.green, color.rgba.blue);
		}
	}
	else
	{
		if (pCairoContext)
			gldi_color_set_cairo (pCairoContext, &myStyleParam.fLineColor);
		else
			gldi_color_set_opengl (&myStyleParam.fLineColor);
	}
}

void gldi_style_colors_set_text_color (cairo_t *pCairoContext)
{
	if (myStyleParam.bUseSystemColors)
	{
		if (pCairoContext)
			cairo_set_source_rgb (pCairoContext, s_text_color.red, s_text_color.green, s_text_color.blue);
		else
			glColor3f (s_text_color.red, s_text_color.green, s_text_color.blue);
	}
	else
	{
		if (pCairoContext)
			gldi_color_set_cairo_rgb (pCairoContext, &myStyleParam.textDescription.fColorStart);
		else
			gldi_color_set_opengl_rgb (&myStyleParam.textDescription.fColorStart);
	}
}

void gldi_style_colors_set_separator_color (cairo_t *pCairoContext)
{
	GldiColor color;
	_get_bg_color (&color);
	gldi_style_color_shade (&color, GLDI_COLOR_SHADE_MEDIUM, &color);
	if (pCairoContext)
		gldi_color_set_cairo_rgb (pCairoContext, &color);  // alpha set to 1
	else
		gldi_color_set_opengl_rgb (&color);
}

void gldi_style_colors_set_child_color (cairo_t *pCairoContext)
{
	GldiColor color;
	_get_bg_color (&color);
	gldi_style_color_shade (&color, GLDI_COLOR_SHADE_STRONG, &color);
	if (pCairoContext)
		gldi_color_set_cairo (pCairoContext, &color);
	else
		gldi_color_set_opengl (&color);
}

void gldi_style_colors_paint_bg_color_with_alpha (cairo_t *pCairoContext, int iWidth, double fAlpha)
{
	if (fAlpha < 0)  // alpha is not defined => take it from the global style
	{
		if (! (myStyleParam.bUseSystemColors && s_menu_bg_pattern))
		{
			fAlpha = (myStyleParam.bUseSystemColors ? s_menu_bg_color.alpha : myStyleParam.fBgColor.rgba.alpha);
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
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	if (s_pStyle != NULL)
		return;

	// init a style context
	s_pStyle = gtk_style_context_new();
	gtk_style_context_set_screen (s_pStyle, gdk_screen_get_default());
	g_signal_connect (s_pStyle, "changed", G_CALLBACK(_on_style_changed), GINT_TO_POINTER (TRUE));  // TRUE => throw a notification
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
			
			cairo_dock_get_color_key_value (keyfile, cRenderer, "line color", &bFlushConfFileNeeded, &pStyleParam->fLineColor, NULL, NULL, NULL);
			g_key_file_set_double_list (pKeyFile, "Style", "line color", (double*)&pStyleParam->fLineColor.rgba, 4);
			
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
		cairo_dock_get_color_key_value (pKeyFile, "Style", "line color", &bFlushConfFileNeeded, &pStyleParam->fLineColor, NULL, NULL, NULL);
	}
	
	GldiColor bg_color = {{1.0, 1.0, 1.0, 0.7}};
	cairo_dock_get_color_key_value (pKeyFile, "Style", "bg color", &bFlushConfFileNeeded, &pStyleParam->fBgColor, &bg_color, "Dialogs", "background color");
	
	gboolean bCustomFont = cairo_dock_get_boolean_key_value (pKeyFile, "Style", "custom font", &bFlushConfFileNeeded, FALSE, "Dialogs", "custom");
	gchar *cFont = (bCustomFont ? cairo_dock_get_string_key_value (pKeyFile, "Style", "font", &bFlushConfFileNeeded, NULL, "Dialogs", "message police") : _get_default_system_font ());
	gldi_text_description_set_font (&pStyleParam->textDescription, cFont);
	
	GldiColor text_color = {{0., 0., 0., 1.}};
	cairo_dock_get_color_key_value (pKeyFile, "Style", "text color", &bFlushConfFileNeeded, &pStyleParam->textDescription.fColorStart, &text_color, "Dialogs", "text color");
	
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
	if (myStyleParam.bUseSystemColors)
		_on_style_changed (s_pStyle, NULL);  // NULL => don't notify
}

  //////////////
 /// RELOAD ///
//////////////

static void reload (GldiStyleParam *pPrevStyleParam, GldiStyleParam *pStyleParam)
{
	cd_message ("reload style mgr...");
	if (pPrevStyleParam->bUseSystemColors != pStyleParam->bUseSystemColors)
		_on_style_changed (s_pStyle, NULL);  // load or invalidate the previous style, NULL => don't notify (it's done just after)
	else
		s_iStyleStamp ++;  // just invalidate
	gldi_object_notify (&myStyleMgr, NOTIFICATION_STYLE_CHANGED);
}

  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	if (s_menu_bg_texture != 0)
	{
		_cairo_dock_delete_texture (s_menu_bg_texture);
		s_menu_bg_texture = 0;
	}
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

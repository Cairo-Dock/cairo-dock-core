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
#if GTK_CHECK_VERSION (3, 10, 0)
#include "gtk3imagemenuitem.h"
#endif

#include "cairo-dock-container.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_get_icon_container
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_get_height
#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-menu.h"

  ////////////
 /// MENU ///
/////////////

#if GTK_MAJOR_VERSION > 2
const int ah = 8;  // arrow height
const int aw = 10;  // arrow demi-width
const int r = 8;  // corner radius
const int l = 1;  // outline width, 0 for no outline
const double alpha = 0.75;  // min alpha (max is 1)
const double fAlign = 0.5;

static void _rendering_draw_outline (GtkWidget *pWidget, cairo_t *pCairoContext)
{
	int iMarginPosition = -1;
	if (g_object_get_data (G_OBJECT (pWidget), "gldi-icon") != NULL)
		iMarginPosition = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pWidget), "gldi-margin-position"));
	
	// draw the outline and set the clip
	GtkAllocation alloc;
	gtk_widget_get_allocation (pWidget, &alloc);
	
	int w = alloc.width, h = alloc.height;
	int x, y;
	gdk_window_get_position (gtk_widget_get_window (gtk_widget_get_toplevel(pWidget)), &x, &y);
	int iAimedX = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(pWidget), "gldi-aimed-x"));
	int iAimedY = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(pWidget), "gldi-aimed-y"));
	int _ah = ah - l;  // we want the tip of the arrow to reach the border, not the middle of the stroke
	int _aw = aw;
	double dx, dy;
	
	double fRadius = r, fLineWidth = l;
	double fDockOffsetX = fRadius + fLineWidth/2;
	double fDockOffsetY = fLineWidth/2;
	double fFrameWidth, fFrameHeight;
	fFrameWidth = w - 2*r - l;
	fFrameHeight = h;
	switch (iMarginPosition)
	{
		case 0:  // bottom
			fFrameHeight -= ah;
		break;
		case 1:  // top
			fFrameHeight -= ah;
			fDockOffsetY += ah;
		break;
		case 2:  // right
			fFrameWidth -= ah;
		break;
		case 3:  // left
			fFrameWidth -= ah;
			fDockOffsetX += ah;
		break;
		default:
		break;
	}
	
	cairo_move_to (pCairoContext, fDockOffsetX, fDockOffsetY);
	
	if (iMarginPosition == 1)  // top arrow
	{
		dx = MIN (w - r - 2*aw, MAX (r, iAimedX - x - aw));
		cairo_line_to (pCairoContext, dx, fDockOffsetY);
		cairo_line_to (pCairoContext, MIN (w, MAX (0, iAimedX - x)), fDockOffsetY - _ah);
		cairo_line_to (pCairoContext, dx + 2*aw, fDockOffsetY);
		cairo_line_to (pCairoContext, fFrameWidth, fDockOffsetY);
	}
	else
		cairo_rel_line_to (pCairoContext, fFrameWidth, 0);
	
	//\_________________ Coin haut droit.
	cairo_arc (pCairoContext,
		fDockOffsetX + fFrameWidth, fDockOffsetY + fRadius,
		fRadius,
		-G_PI/2, 0.);
	if (iMarginPosition == 2)  // right arrow
	{
		if (h < 2*aw + 2*r)
			_aw = (h - 2*r) / 2;
		dy = MIN (h - r - 2*aw, MAX (r, iAimedY - y - _aw));
		cairo_line_to (pCairoContext, w - ah, dy);
		cairo_line_to (pCairoContext, w - ah + _ah, MAX (0, iAimedY - y));
		cairo_line_to (pCairoContext, w - ah, dy + 2*_aw);
		cairo_line_to (pCairoContext, w - ah, h - r);
	}
	else
		cairo_rel_line_to (pCairoContext, 0, (fFrameHeight + fLineWidth - fRadius * 2));
	
	//\_________________ Coin bas droit.
	cairo_arc (pCairoContext,
		fDockOffsetX + fFrameWidth, fDockOffsetY + fFrameHeight - fLineWidth/2 - fRadius,
		fRadius,
		0., G_PI/2);
	
	if (iMarginPosition == 0)  // bottom arrow
	{
		dx = MIN (w - r - 2*aw, MAX (r, iAimedX - x - aw));
		cairo_line_to (pCairoContext, dx + 2*aw, fFrameHeight);
		cairo_line_to (pCairoContext, MIN (w, MAX (0, iAimedX - x)), fFrameHeight + _ah);
		cairo_line_to (pCairoContext, dx, fFrameHeight);
		cairo_line_to (pCairoContext, fDockOffsetX, fFrameHeight);
	}
	else
		cairo_rel_line_to (pCairoContext, - fFrameWidth, 0);
	
	//\_________________ Coin bas gauche.
	cairo_arc (pCairoContext,
		fDockOffsetX, fDockOffsetY + fFrameHeight - fLineWidth/2 - fRadius,
		fRadius,
		G_PI/2, G_PI);
	
	if (iMarginPosition == 3)  // left arrow
	{
		if (h < 2*aw + 2*r)
			_aw = (h - 2*r) / 2;
		dy = MIN (h - r - 2*aw, MAX (r, iAimedY - y - _aw));
		cairo_line_to (pCairoContext, ah, dy);
		cairo_line_to (pCairoContext, ah - _ah, MAX (0, iAimedY - y));
		cairo_line_to (pCairoContext, ah, dy + 2*_aw);
		cairo_line_to (pCairoContext, ah, r);
	}
	else
		cairo_rel_line_to (pCairoContext, 0, (- fFrameHeight - fLineWidth + fRadius * 2));
	//\_________________ Coin haut gauche.
	cairo_arc (pCairoContext,
		fDockOffsetX, fDockOffsetY + fRadius,
		fRadius,
		G_PI, -G_PI/2);
	
	GdkRGBA bg_normal;
	GtkStyleContext *style = gtk_widget_get_style_context (pWidget);
	gtk_style_context_get_background_color (style, GTK_STATE_NORMAL, (GdkRGBA*)&bg_normal);  // bg color, taken from the GTK theme
	/// TODO: handle the case where there is a pattern instead of a color... how to draw the outline then ?... maybe we should fall back to a custom color ?...
	
	if (l != 0)  // draw the outline with same color as bg, but opaque
	{
		cairo_set_source_rgba (pCairoContext, bg_normal.red, bg_normal.green, bg_normal.blue, 1.);
		cairo_stroke_preserve (pCairoContext);
	}
	
	cairo_clip (pCairoContext);  // clip
	
	// draw the background
	cairo_set_source_rgba (pCairoContext, bg_normal.red, bg_normal.green, bg_normal.blue, 1.);
	cairo_pattern_t *pGradationPattern;
	pGradationPattern = cairo_pattern_create_linear (
		0,
		0,
		alloc.width,
		0);
	cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		0.,
		1., 1., 1.,
		1.);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		1.,
		1., 1., 1.,
		alpha);  // bg color with horizontal alpha gradation
	cairo_mask (pCairoContext, pGradationPattern);
	
	cairo_pattern_destroy (pGradationPattern);
}

static gboolean _draw_menu (GtkWidget *pWidget,
	cairo_t *pCairoContext,
	GtkWidget *menu)
{
	// erase the default background
	cairo_dock_erase_cairo_context (pCairoContext);
	
	// draw the background/outline and set the clip
	_rendering_draw_outline (pWidget, pCairoContext);
	
	// draw the items
	GtkWidgetClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_FROM_INSTANCE (menu)));
	
	cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 1.0);
	
	parent_class->draw (pWidget, pCairoContext);
	
	return TRUE;
}
#endif


GtkWidget *gldi_menu_new (G_GNUC_UNUSED Icon *pIcon)
{
	GtkWidget *pMenu = gtk_menu_new ();
	
	gldi_menu_init (pMenu, pIcon);
	
	return pMenu;
}

static gboolean _on_icon_destroyed (GtkWidget *pMenu, G_GNUC_UNUSED Icon *pIcon)
{
	g_object_set_data (G_OBJECT (pMenu), "gldi-icon", NULL);
	return GLDI_NOTIFICATION_LET_PASS;
}

static void _on_menu_destroyed (GtkWidget *pMenu, G_GNUC_UNUSED gpointer data)
{
	Icon *pIcon = g_object_get_data (G_OBJECT(pMenu), "gldi-icon");
	if (pIcon)
		gldi_object_remove_notification (pIcon,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_icon_destroyed,
			NULL);
}

#if GTK_MAJOR_VERSION > 2
static void (*f) (GtkWidget *widget, gint for_size, gint *minimum_size, gint *natural_size) = NULL;
static void _get_preferred_height_for_width (GtkWidget *widget,
	gint for_size,
	gint *minimum_size,
	gint *natural_size)
{
	f (widget, for_size, minimum_size, natural_size);
	if (g_object_get_data (G_OBJECT (widget), "gldi-icon") != NULL)
	{
		int iMarginPosition = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "gldi-margin-position"));
		switch (iMarginPosition)
		{
			case 0:  // bottom
			case 1:  // top
				if (natural_size) *natural_size = *natural_size + ah;
				if (minimum_size) *minimum_size = *minimum_size + ah;
			break;
			case 2:  // right
			case 3:  // left
			default:
			break;
		}
	}
}

static void (*ff) (GtkWidget *widget, gint *minimum_size, gint *natural_size) = NULL;
static void _get_preferred_width (GtkWidget *widget,
	gint *minimum_size,
	gint *natural_size)
{
	ff (widget, minimum_size, natural_size);
	if (g_object_get_data (G_OBJECT (widget), "gldi-icon") != NULL)
	{
		int iMarginPosition = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "gldi-margin-position"));
		
		switch (iMarginPosition)
		{
			case 0:  // bottom
			case 1:  // top
			default:
			break;
			case 2:  // right
			case 3:  // left
				if (natural_size) *natural_size = *natural_size + ah;
				if (minimum_size) *minimum_size = *minimum_size + ah;
			break;
		}
	}
}

static void _on_menu_deactivated (GtkMenuShell *pMenu, G_GNUC_UNUSED gpointer data)
{
	Icon *pIcon = g_object_get_data (G_OBJECT(pMenu), "gldi-icon");
	if (pIcon->iHideLabel > 0)
	{
		pIcon->iHideLabel --;
		GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
		if (pIcon->iHideLabel == 0 && pContainer)
			gtk_widget_queue_draw (pContainer->pWidget);
	}
}
#endif

void gldi_menu_init (G_GNUC_UNUSED GtkWidget *pMenu, G_GNUC_UNUSED Icon *pIcon)
{
	#if (CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1)
	gtk_menu_set_reserve_toggle_size (GTK_MENU(pMenu), TRUE);
	#endif
	
	#if GTK_MAJOR_VERSION > 2
	// connect to 'draw' event to draw the menu (background and items)
	GtkWidget *pWindow = gtk_widget_get_toplevel (pMenu);
	cairo_dock_set_default_rgba_visual (pWindow);
	
	g_signal_connect (G_OBJECT (pMenu),
		"draw",
		G_CALLBACK (_draw_menu),
		pMenu);
	
	if (!f)
	{
		GtkWidgetClass *widget_class = g_type_class_ref (GTK_TYPE_MENU);
		
		///GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS (pMenu);
		f = widget_class->get_preferred_height_for_width;
		widget_class->get_preferred_height_for_width = _get_preferred_height_for_width;
		
		ff = widget_class->get_preferred_width;
		widget_class->get_preferred_width = _get_preferred_width;
		g_type_class_unref (widget_class);
		g_print ("%p/%p\n", f, ff);
	}
	#endif
	
	if (pIcon != NULL)  // the menu points on an icon
	{
		GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
		if (pContainer != NULL)
		{
			#if GTK_MAJOR_VERSION > 2
			// init the rendering
			/// --> align, margin-height
			
			
			// define where the menu will point, and allocate some space to draw the arrow
			int iMarginPosition;  // b, t, r, l
			int y0 = pContainer->iWindowPositionY + pIcon->fDrawY;
			if (pContainer->bDirectionUp)
				y0 += pIcon->fHeight * pIcon->fScale - pIcon->image.iHeight;  // the icon might not be maximised yet
			int Hs = (pContainer->bIsHorizontal ? gldi_desktop_get_height() : gldi_desktop_get_width());
			if (pContainer->bIsHorizontal)
			{
				iMarginPosition = (y0 > Hs/2 ? 0 : 1);
			}
			else
			{
				iMarginPosition = (y0 > Hs/2 ? 2 : 3);
			}
			g_object_set_data (G_OBJECT (pMenu), "gldi-margin-position", GINT_TO_POINTER (iMarginPosition));
			
			g_signal_connect (G_OBJECT (pMenu),
				"deactivate",
				G_CALLBACK (_on_menu_deactivated),
				NULL);  // show the icon's label back when the menu is hidden
			#endif
		}
		
		// link it to the icon
		g_object_set_data (G_OBJECT (pMenu), "gldi-icon", pIcon);
		gldi_object_register_notification (pIcon,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_icon_destroyed,
			GLDI_RUN_AFTER, pMenu);  // when the icon is destroyed, unlink the menu from it
		g_signal_connect (G_OBJECT (pMenu),
			"destroy",
			G_CALLBACK (_on_menu_destroyed),
			NULL);  // when the menu is destroyed, unregister the above notification on the icon
	}
}

static void _place_menu_on_icon (GtkMenu *menu, gint *x, gint *y, gboolean *push_in, G_GNUC_UNUSED gpointer data)
{
	*push_in = FALSE;
	Icon *pIcon = g_object_get_data (G_OBJECT(menu), "gldi-icon");
	GldiContainer *pContainer = (pIcon ? cairo_dock_get_icon_container (pIcon) : NULL);
	int x0 = pContainer->iWindowPositionX + pIcon->fDrawX;
	int y0 = pContainer->iWindowPositionY + pIcon->fDrawY;
	if (pContainer->bDirectionUp)
		y0 += pIcon->fHeight * pIcon->fScale - pIcon->image.iHeight;  // the icon might not be maximised yet
	
	int w, h;  // taille menu
	GtkRequisition requisition;
	#if (GTK_MAJOR_VERSION < 3)
	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);
	#else
	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &requisition, NULL);
	#endif
	w = requisition.width;
	h = requisition.height;
	#if GTK_MAJOR_VERSION > 2
	int w_ = w - 2 * r;
	int h_ = h - 2 * r - ah;
	#endif
	
	/// TODO: use iMarginPosition...
	int iAimedX, iAimedY;
	int Hs = (pContainer->bIsHorizontal ? gldi_desktop_get_height() : gldi_desktop_get_width());
	//g_print ("%d;%d %dx%d\n", x0, y0, w, h);
	if (pContainer->bIsHorizontal)
	{
		iAimedX = x0 + pIcon->image.iWidth/2;
		#if GTK_MAJOR_VERSION > 2
		*x = MAX (0, iAimedX - fAlign * w_ - r);
		#else
		*x = iAimedX;
		#endif
		if (y0 > Hs/2)  // pContainer->bDirectionUp
		{
			*y = y0 - h;
			iAimedY = y0;
		}
		else
		{
			*y = y0 + pIcon->fHeight * pIcon->fScale;
			iAimedY = y0 + pIcon->image.iHeight;
		}
	}
	else
	{
		iAimedY = x0 + pIcon->image.iWidth/2;
		#if GTK_MAJOR_VERSION > 2
		*y = MIN (iAimedY - fAlign * h_ - r, gldi_desktop_get_height() - h);
		#else
		*y = MIN (iAimedY, gldi_desktop_get_height() - h);
		#endif
		if (y0 > Hs/2)  // pContainer->bDirectionUp
		{
			*x = y0 - w;
			iAimedX = y0;
		}
		else
		{
			*x = y0 + pIcon->image.iHeight;
			iAimedX = y0 + pIcon->image.iHeight;
		}
	}
	g_object_set_data (G_OBJECT(menu), "gldi-aimed-x", GINT_TO_POINTER(iAimedX));
	g_object_set_data (G_OBJECT(menu), "gldi-aimed-y", GINT_TO_POINTER(iAimedY));
}
static void _popup_menu (GtkWidget *menu, guint32 time)
{
	Icon *pIcon = g_object_get_data (G_OBJECT(menu), "gldi-icon");
	GldiContainer *pContainer = (pIcon ? cairo_dock_get_icon_container (pIcon) : NULL);
	
	// setup the menu for the container
	if (pContainer && pContainer->iface.setup_menu)
		pContainer->iface.setup_menu (pContainer, pIcon, menu);
	
	#if GTK_MAJOR_VERSION > 2
	if (pIcon && pContainer)  // hide the icon's label, since menus are placed right above the icon (and therefore, the arrow overlaps the label, which makes it hard to see if both colors are similar).
	{
		if (pIcon->iHideLabel == 0 && pContainer)
			gtk_widget_queue_draw (pContainer->pWidget);
		pIcon->iHideLabel ++;
	}
	#endif
	
	gtk_widget_show_all (GTK_WIDGET (menu));
	
	gtk_menu_popup (GTK_MENU (menu),
		NULL,
		NULL,
		pIcon != NULL && pContainer != NULL ? _place_menu_on_icon : NULL,
		NULL,
		0,
		time);
}
static gboolean _popup_menu_delayed (GtkWidget *menu)
{
	_popup_menu (menu, 0);
	return FALSE;
}
void gldi_menu_popup (GtkWidget *menu)
{
	if (menu == NULL)
		return;
	
	guint32 t = gtk_get_current_event_time();
	cd_debug ("gtk_get_current_event_time: %d", t);
	if (t > 0)
	{
		_popup_menu (menu, t);
	}
	else  // 'gtk_menu_popup' is buggy and doesn't work if not triggered directly by an X event :-/ so in this case, we run it with a delay (200ms is the minimal value that always works).
	{
		g_timeout_add (250, (GSourceFunc)_popup_menu_delayed, menu);
	}
}


  /////////////////
 /// MENU ITEM ///
/////////////////

#if GTK_MAJOR_VERSION > 2
const int N = 10;

static void
get_arrow_size (GtkWidget *widget,
                GtkWidget *child,
                gint      *size,
                gint      *spacing)
{
	PangoContext     *context;
	PangoFontMetrics *metrics;
	gfloat            arrow_scaling;
	gint              arrow_spacing;

	g_assert (size);

	gtk_widget_style_get (widget,
		"arrow-scaling", &arrow_scaling,
		"arrow-spacing", &arrow_spacing,
		NULL);

	if (spacing != NULL)
		*spacing = arrow_spacing;

	context = gtk_widget_get_pango_context (child);

	metrics = pango_context_get_metrics (context,
									   pango_context_get_font_description (context),
									   pango_context_get_language (context));

	*size = (PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
						 pango_font_metrics_get_descent (metrics)));

	pango_font_metrics_unref (metrics);

	*size = *size * arrow_scaling;
}
static gboolean _draw_menu_item (GtkWidget *widget,
	cairo_t *cr,
	G_GNUC_UNUSED gpointer data)
{
	GtkStateFlags state;
	GtkStyleContext *context;
	GtkWidget *child;
	gint x, y, w, h, width, height;
	guint border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

	state = gtk_widget_get_state_flags (widget);
	context = gtk_widget_get_style_context (widget);
	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);
	
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_MENUITEM);
	
	x = border_width;
	y = border_width;
	w = width - border_width * 2;
	h = height - border_width * 2;
	
	child = gtk_bin_get_child (GTK_BIN (widget));
	
	// draw the background
	GdkRGBA bg_color;
	cairo_pattern_t *pattern = NULL;
	
	GdkRGBA bg_color1, bg_color2;
	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_PRELIGHT, (GdkRGBA*)&bg_color1);
	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, (GdkRGBA*)&bg_color2);
	int n = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "gldi-step"));
	double a = (double)n/N;
	
	bg_color.red = a * bg_color1.red + (1-a) * bg_color2.red;
	bg_color.blue = a * bg_color1.blue + (1-a) * bg_color2.blue;
	bg_color.green = a * bg_color1.green + (1-a) * bg_color2.green;
	bg_color.alpha = a * bg_color1.alpha + (1-a) * bg_color2.alpha;
	
	if (state & GTK_STATE_FLAG_PRELIGHT)
		gtk_style_context_get (context, GTK_STATE_FLAG_PRELIGHT,
			GTK_STYLE_PROPERTY_BACKGROUND_IMAGE, &pattern,
			NULL);  /// TODO: maybe we could keep this value...
	
	cairo_save (cr);
	
	int r=6, l=0;
	cairo_dock_draw_rounded_rectangle (cr, r, l, w - 2*r, h);
	cairo_clip (cr);
	
	//g_print ("%.2f;%.2f;%.2f;%.2f\n", bg_color1.red, bg_color1.green, bg_color1.blue, bg_color1.alpha);
	//g_print ("%.2f;%.2f;%.2f;%.2f\n", bg_color2.red, bg_color2.green, bg_color2.blue, bg_color2.alpha);
	if (bg_color.alpha != 0)
	{
		cairo_set_source_rgba (cr, bg_color.red, bg_color.green, bg_color.blue, bg_color.alpha);
		cairo_paint (cr);
	}
	else if (pattern)
	{
		cairo_set_source (cr, pattern);
		cairo_paint_with_alpha (cr, a);
	}
	
	cairo_restore (cr);
	if (pattern)
		cairo_pattern_destroy (pattern);
	
	// draw the arrow in case of a sub-menu
	if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget)))
	{
		gint arrow_x, arrow_y;
		gint arrow_size, spacing;
		GtkTextDirection direction;
		gdouble angle;

		direction = gtk_widget_get_direction (widget);
		get_arrow_size (widget, child, &arrow_size, &spacing);

		if (direction == GTK_TEXT_DIR_LTR)
		{
		  arrow_x = x + w - arrow_size - spacing;
		  angle = G_PI / 2;
		}
		else
		{
		  arrow_x = x + spacing;
		  angle = (3 * G_PI) / 2;
		}

		arrow_y = y + (h - arrow_size) / 2;

		gtk_render_arrow (context, cr, angle, arrow_x, arrow_y, arrow_size);
	}  // we don't draw separator items
	
	// draw the item's content
	GtkWidgetClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_FROM_INSTANCE (widget)));
	parent_class = g_type_class_peek_parent (parent_class);
	
	cairo_set_source_rgba (cr, 1, 1, 1, 1);

	parent_class->draw (widget, cr);
	return TRUE;  // intercept
}

static gboolean _update_menu_item (GtkWidget* pMenuItem, G_GNUC_UNUSED gpointer data)
{
	int n = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(pMenuItem), "gldi-step"));
	gboolean bInside = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(pMenuItem), "gldi-inside"));
	
	if (bInside)
	{
		if (n < N-1)
			n ++;
	}
	else
	{
		if (n > 0)
			n --;
	}
	g_object_set_data (G_OBJECT(pMenuItem), "gldi-step", GINT_TO_POINTER (n));
	
	gtk_widget_queue_draw (pMenuItem);
	
	if (n == 0 || n == N)
	{
		guint iSidAnimation = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT(pMenuItem), "gldi-animation"));
		if (iSidAnimation != 0)
		{
			g_source_remove (iSidAnimation);
			g_object_set_data (G_OBJECT(pMenuItem), "gldi-animation", NULL);
			return FALSE;
		}
	}
	return TRUE;
}
static gboolean _on_select_menu_item (GtkWidget* pMenuItem, G_GNUC_UNUSED gpointer data)
{
	guint iSidAnimation = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT(pMenuItem), "gldi-animation"));
	if (iSidAnimation != 0)
		g_source_remove (iSidAnimation);
	iSidAnimation = g_timeout_add (10, (GSourceFunc)_update_menu_item, pMenuItem);
	g_object_set_data (G_OBJECT(pMenuItem), "gldi-animation", GUINT_TO_POINTER (iSidAnimation));
	
	g_object_set_data (G_OBJECT(pMenuItem), "gldi-inside", GINT_TO_POINTER (TRUE));
	
	return FALSE;
}
static gboolean _on_deselect_menu_item (GtkWidget* pMenuItem, G_GNUC_UNUSED gpointer data)
{
	guint iSidAnimation = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT(pMenuItem), "gldi-animation"));
	if (iSidAnimation != 0)
		g_source_remove (iSidAnimation);
	iSidAnimation = g_timeout_add (30, (GSourceFunc)_update_menu_item, pMenuItem);
	g_object_set_data (G_OBJECT(pMenuItem), "gldi-animation", GUINT_TO_POINTER (iSidAnimation));
	
	g_object_set_data (G_OBJECT(pMenuItem), "gldi-inside", GINT_TO_POINTER (FALSE));
	
	return FALSE;
}
static void _on_destroy_menu_item (GtkWidget* pMenuItem, G_GNUC_UNUSED gpointer data)
{
	guint iSidAnimation = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT(pMenuItem), "gldi-animation"));
	if (iSidAnimation != 0)
	{
		g_source_remove (iSidAnimation);
		g_object_set_data (G_OBJECT(pMenuItem), "gldi-animation", NULL);
	}
}

static void (*g) (GtkWidget *widget, GtkAllocation *allocation) = NULL;

static void _size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkWidget *pMenu = gtk_widget_get_parent (widget);
	Icon *pIcon = g_object_get_data (G_OBJECT(pMenu), "gldi-icon");
	if (pIcon)
	{
		int iMarginPosition = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pMenu), "gldi-margin-position"));
		switch (iMarginPosition)
		{
			case 0:  // bottom
			break;
			case 1:  // top
				allocation->y += ah;
			break;
			case 2:  // right
				allocation->width -= ah;
			break;
			case 3:  // left
				allocation->x += ah;
				allocation->width -= ah;
			break;
			default: break;
		}
	}
	g (widget, allocation);
}
#endif

GtkWidget *gldi_menu_item_new_full (const gchar *cLabel, const gchar *cImage, gboolean bUseMnemonic, GtkIconSize iSize)
{
	if (iSize == 0)
		iSize = GTK_ICON_SIZE_MENU;
	
	GtkWidget *pMenuItem;
	if (! cImage)
	{
		if (! cLabel)
			pMenuItem = gtk_menu_item_new ();
		else
			pMenuItem =  (bUseMnemonic ? gtk_menu_item_new_with_mnemonic (cLabel) : gtk_menu_item_new_with_label (cLabel));
	}
	else
	{
		GtkWidget *image = NULL;
#if (! GTK_CHECK_VERSION (3, 10, 0)) || (CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1)
		if (*cImage == '/')
		{
			int size;
			gtk_icon_size_lookup (iSize, &size, NULL);
			GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cImage, size, size, NULL);
			image = gtk_image_new_from_pixbuf (pixbuf);
			g_object_unref (pixbuf);
		}
		else if (*cImage != '\0')
		{
			#if GTK_CHECK_VERSION (3, 10, 0)
			image = gtk_image_new_from_icon_name (cImage, iSize);  /// actually, this will not work until we replace all the gtk-stock names by standard icon names... which is a PITA, and will be done do later
			#else
			image = gtk_image_new_from_stock (cImage, iSize);
			#endif
		}
#endif
		
#if GTK_CHECK_VERSION (3, 10, 0)
		#if (CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1)
		if (! cLabel)
			pMenuItem = gtk3_image_menu_item_new ();
		else
			pMenuItem = (bUseMnemonic ? gtk3_image_menu_item_new_with_mnemonic (cLabel) : gtk3_image_menu_item_new_with_label (cLabel));
		gtk3_image_menu_item_set_image (GTK3_IMAGE_MENU_ITEM (pMenuItem), image);
		#else
		if (! cLabel)
			pMenuItem = gtk_menu_item_new ();
		else
			pMenuItem = (bUseMnemonic ? gtk_menu_item_new_with_mnemonic (cLabel) : gtk_menu_item_new_with_label (cLabel));
		#endif
#else
		if (! cLabel)
			pMenuItem = gtk_image_menu_item_new ();
		else
			pMenuItem = (bUseMnemonic ? gtk_image_menu_item_new_with_mnemonic (cLabel) : gtk_image_menu_item_new_with_label (cLabel));
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);
		#if ((CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1) && (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16))
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (pMenuItem), TRUE);
		#endif
#endif
	}
	gtk_widget_show_all (pMenuItem);  // show immediately, so that the menu-item is realized when the menu is popped up
	
	// the following code is to draw the menu item; this is more like a proof of concept
	#if GTK_MAJOR_VERSION > 2
	g_signal_connect (G_OBJECT (pMenuItem),
		"draw",
		G_CALLBACK (_draw_menu_item),
		NULL);
	g_signal_connect (G_OBJECT (pMenuItem),
		"select",
		G_CALLBACK (_on_select_menu_item),
		NULL);
	g_signal_connect (G_OBJECT (pMenuItem),
		"deselect",
		G_CALLBACK (_on_deselect_menu_item),
		NULL);
	g_signal_connect (G_OBJECT (pMenuItem),
		"destroy",
		G_CALLBACK (_on_destroy_menu_item),
		NULL);
	
	if (!g)
	{
		GtkWidgetClass *widget_class = g_type_class_ref (GTK_TYPE_MENU_ITEM);
		g = widget_class->size_allocate;
		widget_class->size_allocate = _size_allocate;  // all other menu-item types will call this function after their own function
		g_type_class_unref (widget_class);
	}
	#endif
	
	return pMenuItem;
}


void gldi_menu_item_set_image (GtkWidget *pMenuItem, GtkWidget *image)
{
#if GTK_CHECK_VERSION (3, 10, 0)
	#if (CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1)
	gtk3_image_menu_item_set_image (GTK3_IMAGE_MENU_ITEM (pMenuItem), image);
	#else
	g_object_unref (image);
	#endif
#else
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);
	#if ((CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1) && (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16))
	gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (pMenuItem), TRUE);
	#endif
#endif
}

GtkWidget *gldi_menu_item_get_image (GtkWidget *pMenuItem)
{
	#if GTK_CHECK_VERSION (3, 10, 0)
	return gtk3_image_menu_item_get_image (GTK3_IMAGE_MENU_ITEM (pMenuItem));
	#else
	return gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (pMenuItem));
	#endif
}

GtkWidget *gldi_menu_item_new_with_action (const gchar *cLabel, const gchar *cImage, GCallback pFunction, gpointer pData)
{
	GtkWidget *pMenuItem = gldi_menu_item_new (cLabel, cImage);
	if (pFunction)
		g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK (pFunction), pData);
	return pMenuItem;
}

GtkWidget *gldi_menu_item_new_with_submenu (const gchar *cLabel, const gchar *cImage, GtkWidget **pSubMenuPtr)
{
	GtkIconSize iSize;
	if (cImage && (*cImage == '/' || *cImage == '\0'))  // for icons that are not stock-icons, we choose a bigger size; the reason is that these icons usually don't have a 16x16 version, and don't scale very well to such a small size (most of the time, it's the icon of an application, or the cairo-dock or recent-documents icon (note: for these 2, we could make a small version)). it's a workaround and a better solution may exist ^^
		iSize = GTK_ICON_SIZE_LARGE_TOOLBAR;
	else
		iSize = 0;
	GtkWidget *pMenuItem = gldi_menu_item_new_full (cLabel, cImage, FALSE, iSize);
	GtkWidget *pSubMenu = gldi_submenu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenu);
	
	*pSubMenuPtr = pSubMenu;
	return pMenuItem;
}

GtkWidget *gldi_menu_add_item (GtkWidget *pMenu, const gchar *cLabel, const gchar *cImage, GCallback pFunction, gpointer pData)
{
	GtkWidget *pMenuItem = gldi_menu_item_new_with_action (cLabel, cImage, pFunction, pData);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	return pMenuItem;
}

GtkWidget *gldi_menu_add_sub_menu_full (GtkWidget *pMenu, const gchar *cLabel, const gchar *cImage, GtkWidget **pMenuItemPtr)
{
	GtkWidget *pSubMenu;
	GtkWidget *pMenuItem = gldi_menu_item_new_with_submenu (cLabel, cImage, &pSubMenu);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	if (pMenuItemPtr)
		*pMenuItemPtr = pMenuItem;
	return pSubMenu; 
}

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
#include <math.h>  // fabs

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
#include "cairo-dock-backends-manager.h"  // cairo_dock_get_dialog_decorator
#include "cairo-dock-dialog-manager.h"  // myDialogsParam
#include "cairo-dock-style-manager.h"
#include "cairo-dock-menu.h"

#if GTK_MAJOR_VERSION > 2
static gboolean _draw_menu_item (GtkWidget *widget, cairo_t *cr, G_GNUC_UNUSED gpointer data);
static gboolean _on_select_menu_item (GtkWidget* pMenuItem, G_GNUC_UNUSED gpointer data);
static gboolean _on_deselect_menu_item (GtkWidget* pMenuItem, G_GNUC_UNUSED gpointer data);
static void _on_destroy_menu_item (GtkWidget* pMenuItem, G_GNUC_UNUSED gpointer data);
#endif

  ////////////
 /// MENU ///
/////////////

#if GTK_MAJOR_VERSION > 2

static void _init_menu_style (void)
{
	static GtkCssProvider *cssProvider = NULL;
	static int index = 0;
	
	if (index == gldi_style_colors_get_index())
		return;
	index = gldi_style_colors_get_index();
	g_print ("%s (%d)\n", __func__, index);
	
	if (myDialogsParam.bUseDefaultColors && myStyleParam.bUseSystemColors)
	{
		if (cssProvider != NULL)
		{
			gldi_style_colors_freeze ();
			gtk_style_context_remove_provider_for_screen (gdk_screen_get_default(), GTK_STYLE_PROVIDER(cssProvider));
			gldi_style_colors_freeze ();
			g_object_unref (cssProvider);
			cssProvider = NULL;
		}
	}
	else
	{
		if (cssProvider == NULL)
		{
			cssProvider = gtk_css_provider_new ();
			gldi_style_colors_freeze ();
			gtk_style_context_add_provider_for_screen (gdk_screen_get_default(), GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
			gldi_style_colors_freeze ();
		}
		
		double *bg_color = (myDialogsParam.bUseDefaultColors ? myStyleParam.fBgColor : myDialogsParam.fBgColor);
		double *text_color = (myDialogsParam.bUseDefaultColors ? myStyleParam.textDescription.fColorStart : myDialogsParam.dialogTextDescription.fColorStart);
		
		double rgb[4];
		gldi_style_color_shade (bg_color, .2, rgb);
		double rgbs[4];
		gldi_style_color_shade (bg_color, .2, rgbs);
		double rgbb[4];
		gldi_style_color_shade (bg_color, .3, rgbb);
		
		gchar *css = g_strdup_printf ("@define-color menuitem_bg_color rgb (%d, %d, %d); \
		@define-color menuitem_text_color rgb (%d, %d, %d); \
		@define-color menuitem_insensitive_text_color rgba (%d, %d, %d, .5); \
		@define-color menuitem_separator_color rgb (%d, %d, %d); \
		@define-color menuitem_bg_color2 rgb (%d, %d, %d); \
		@define-color menu_bg_color rgba (%d, %d, %d, %d); \
		.menuitem2 { \
			text-shadow: none; \
			border-image: none; \
			box-shadow: none; \
			background: transparent; \
			color: @menuitem_text_color; \
		} \
		.menuitem2 GtkImage { \
			background: transparent; \
		} \
		.menuitem2.separator, \
		.menuitem2 .separator { \
			color: @menuitem_separator_color; \
			border-width: 1px; \
			border-style: solid; \
			border-image: none; \
			border-color: @menuitem_separator_color; \
			border-bottom-color: alpha (@menuitem_separator_color, 0.6); \
			border-right-color: alpha (@menuitem_separator_color, 0.6); \
		} \
		.menuitem2:hover, \
		.menuitem2 *:hover { \
			background: @menuitem_bg_color; \
			background-image: none; \
			text-shadow: none; \
			border-image: none; \
			box-shadow: none; \
			color: @menuitem_text_color; \
		} \
		.menuitem2 *:insensitive { \
			text-shadow: none; \
			color: @menuitem_insensitive_text_color; \
		} \
		.menuitem2 .entry, \
		.menuitem2.entry { \
			background: @menuitem_bg_color; \
			border-image: none; \
			border-color: transparent; \
			color: @menuitem_text_color; \
		} \
		.menuitem2 .button, \
		.menuitem2.button { \
			background: @menuitem_bg_color; \
			background-image: none; \
			box-shadow: none; \
			border-color: transparent; \
			padding: 2px; \
		} \
		.menuitem2 .scale, \
		.menuitem2.scale { \
			background: @menuitem_bg_color2; \
			background-image: none; \
			color: @menuitem_text_color; \
			border-image: none; \
		} \
		.menuitem2 .scale.left, \
		.menuitem2.scale.left { \
			background: @menuitem_bg_color; \
			background-image: none; \
			border-image: none; \
		} \
		.menuitem2 .scale.slider, \
		.menuitem2.scale.slider { \
			background: @menuitem_text_color; \
			background-image: none; \
			border-image: none; \
		} \
		.menuitem2 GtkCalendar, \
		.menuitem2 GtkCalendar.button, \
		.menuitem2 GtkCalendar.header, \
		.menuitem2 GtkCalendar.view { \
			background-color: @menuitem_bg_color; \
			background-image: none; \
			color: @menuitem_text_color; \
		} \
		.menuitem2 GtkCalendar { \
			background-color: @menuitem_bg_color2; \
			background-image: none; \
		} \
		.menuitem2 GtkCalendar:inconsistent { \
			color: shade (@menuitem_bg_color2, 0.6); \
		} \
		.menu2 { \
			background: @menu_bg_color; \
			background-image: none; \
			color: @menuitem_text_color; \
		}",
		(int)(rgb[0]*255), (int)(rgb[1]*255), (int)(rgb[2]*255),
		(int)(text_color[0]*255), (int)(text_color[1]*255), (int)(text_color[2]*255),
		(int)(text_color[0]*255), (int)(text_color[1]*255), (int)(text_color[2]*255),
		(int)(rgbs[0]*255), (int)(rgbs[1]*255), (int)(rgbs[2]*255),
		(int)(rgbb[0]*255), (int)(rgbb[1]*255), (int)(rgbb[2]*255),
		(int)(bg_color[0]*255), (int)(bg_color[1]*255), (int)(bg_color[2]*255), (int)(bg_color[3]*255));  // we also define ".menu", so that custom widgets (like in the SoundMenu) can get our colors.
		
		gldi_style_colors_freeze ();
		gtk_css_provider_load_from_data (cssProvider,
			css, -1, NULL);  // (should) clear any previously loaded information
		gldi_style_colors_freeze ();
		g_free (css);
	}
}

static gboolean _draw_menu (GtkWidget *pWidget,
	cairo_t *pCairoContext,
	G_GNUC_UNUSED GtkWidget *menu)
{
	// erase the default background
	cairo_dock_erase_cairo_context (pCairoContext);
	
	// draw the background/outline and set the clip
	CairoDialogDecorator *pDecorator = cairo_dock_get_dialog_decorator (myDialogsParam.cDecoratorName);
	if (pDecorator)
		pDecorator->render_menu (pWidget, pCairoContext);
	
	// draw the items
	cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 1.0);
	
	GtkWidgetClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_FROM_INSTANCE (pWidget)));
	parent_class = g_type_class_peek_parent (parent_class);  // skip the direct parent (GtkBin, which does anyway nothing usually), because dbusmenu-gtk draws it
	parent_class->draw (pWidget, pCairoContext);
	
	return TRUE;
}

static void _set_margin_position (GtkWidget *pMenu, GldiMenuParams *pParams)
{
	if (pParams == NULL)
		pParams = g_object_get_data (G_OBJECT (pMenu), "gldi-params");
	g_return_if_fail (pParams);
	Icon *pIcon = pParams->pIcon;
	g_return_if_fail (pIcon);
	GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
	g_return_if_fail (pContainer);
	
	// define where the menu will point
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
	
	// store the result, and allocate some space to draw the arrow
	if (iMarginPosition != pParams->iMarginPosition)  // margin position is now defined or has changed -> update it on the menu
	{
		g_print ("margin position: %d -> %d\n", pParams->iMarginPosition, iMarginPosition);
		// store the value
		pParams->iMarginPosition = iMarginPosition;
		
		// get/add a css (gtk_widget_set_margin_xxx doesn't work as expected :-/)
		GtkCssProvider *cssProvider = pParams->cssProvider;
		if (cssProvider)  // unfortunately, GTK doesn't update correctly a css provider if we load some new data inside (the previous padding values are kept, along with the new ones, although 'gtk_css_provider_load_from_data' is supposed to "clear any previously loaded information"), so we have to remove/add it :-/
		{
			gtk_style_context_remove_provider (gtk_widget_get_style_context(pMenu), GTK_STYLE_PROVIDER(cssProvider));
			g_object_unref (cssProvider);
			cssProvider = NULL;
			pParams->cssProvider = NULL;
		}
		if (cssProvider == NULL)
		{
			cssProvider = gtk_css_provider_new ();
			gtk_style_context_add_provider (gtk_widget_get_style_context(pMenu), GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);  // this adds a reference on the provider, plus the one we own
			pParams->cssProvider = cssProvider;
			g_print (" + css\n");
		}
		
		// load the new padding rule into the css
		gchar *css = NULL;
		int ah = pParams->iArrowHeight;
		int b=0, t=0, r=0, l=0;
		switch (iMarginPosition)
		{
			case 0: b = ah; break;
			case 1: t = ah; break;
			case 2: r = ah; break;
			case 3: l = ah; break;
			default: break;
		}
		css = g_strdup_printf ("GtkMenu { \
			padding-bottom: %dpx; \
			padding-top: %dpx; \
			padding-right: %dpx; \
			padding-left: %dpx; \
		}", b, t, r, l);  // we must define all the paddings, else if the margin position changes, clearing the css won't make the padding disappear :-/
		gtk_css_provider_load_from_data (cssProvider,
			css, -1, NULL);
		g_free (css);
	}
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
	GldiMenuParams *pParams = g_object_get_data (G_OBJECT (pMenu), "gldi-params");
	if (pParams)
		pParams->pIcon = NULL;
	return GLDI_NOTIFICATION_LET_PASS;
}

static void _on_menu_destroyed (GtkWidget *pMenu, G_GNUC_UNUSED gpointer data)
{
	GldiMenuParams *pParams = g_object_get_data (G_OBJECT (pMenu), "gldi-params");
	if (!pParams)
		return;
	Icon *pIcon = pParams->pIcon;
	if (pIcon)
		gldi_object_remove_notification (pIcon,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_icon_destroyed,
			NULL);
	#if GTK_MAJOR_VERSION > 2
	if (pParams->cssProvider)
	{
		g_object_unref (pParams->cssProvider);  /// need to remove the provider from the style context ?... probably not since the style context will be destroyed and will release its reference on the provider
	}
	g_free (pParams);
	#endif
}

#if GTK_MAJOR_VERSION > 2
static void _on_menu_deactivated (GtkMenuShell *pMenu, G_GNUC_UNUSED gpointer data)
{
	GldiMenuParams *pParams = g_object_get_data (G_OBJECT (pMenu), "gldi-params");
	if (!pParams)
		return;
	Icon *pIcon = pParams->pIcon;
	if (pIcon->iHideLabel > 0)
	{
		pIcon->iHideLabel --;
		GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
		if (pIcon->iHideLabel == 0 && pContainer)
			gtk_widget_queue_draw (pContainer->pWidget);
	}
}
#endif

void gldi_menu_init (G_GNUC_UNUSED GtkWidget *pMenu, Icon *pIcon)
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
	
	_init_menu_style ();
	
	gtk_style_context_add_class (gtk_widget_get_style_context (pMenu), "menu2");
	#endif
	
	// set params
	GldiMenuParams *pParams = g_new0 (GldiMenuParams, 1);
	g_object_set_data (G_OBJECT (pMenu), "gldi-params", pParams);
	g_signal_connect (G_OBJECT (pMenu),
		"destroy",
		G_CALLBACK (_on_menu_destroyed),
		NULL);
		
	// init a main menu
	if (pIcon != NULL)  // the menu points on an icon
	{
		// link it to the icon
		g_object_set_data (G_OBJECT (pMenu), "gldi-icon", pIcon);
		pParams->pIcon = pIcon;
		gldi_object_register_notification (pIcon,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_icon_destroyed,
			GLDI_RUN_AFTER, pMenu);  // when the icon is destroyed, unlink the menu from it; when the menu is destroyed, the above notification will be unregistered on the icon in the "destroy" callback
		
		GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
		if (pContainer != NULL)
		{
			#if GTK_MAJOR_VERSION > 2
			// init the rendering --> align, margin-height
			CairoDialogDecorator *pDecorator = cairo_dock_get_dialog_decorator (myDialogsParam.cDecoratorName);
			if (pDecorator)
				pDecorator->setup_menu (pMenu);
			
			// define where the menu will point, and allocate some space to draw the arrow
			pParams->iMarginPosition = -1;
			_set_margin_position (pMenu, pParams);
			
			// show the icon's label back when the menu is hidden
			g_signal_connect (G_OBJECT (pMenu),
				"deactivate",
				G_CALLBACK (_on_menu_deactivated),
				NULL);
			
			gchar *css = NULL;
			const gchar *orientation = "";
			int ah = pParams->iArrowHeight;
			switch (pParams->iMarginPosition)
			{
				case 0: orientation = "bottom";  break;
				case 1: orientation = "top";  break;
				case 2: orientation = "right";  break;
				case 3: orientation = "left";  break;
				default: break;
			}
			
			GtkCssProvider *cssProvider = gtk_css_provider_new ();
			css = g_strdup_printf ("GtkMenu { \
				padding-%s: %dpx; \
			}", orientation, ah);
			gtk_css_provider_load_from_data (cssProvider,
				css, -1, NULL);
			gtk_style_context_add_provider (gtk_widget_get_style_context(pMenu), GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
			g_free (css);
			#endif
		}
	}
}

static void _place_menu_on_icon (GtkMenu *menu, gint *x, gint *y, gboolean *push_in, G_GNUC_UNUSED gpointer data)
{
	*push_in = FALSE;
	GldiMenuParams *pParams = g_object_get_data (G_OBJECT(menu), "gldi-params");
	g_return_if_fail (pParams != NULL);
	
	Icon *pIcon = pParams->pIcon;
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
	
	/// TODO: use iMarginPosition...
	#if GTK_MAJOR_VERSION > 2
	double fAlign = pParams->fAlign;
	int r = pParams->iRadius;
	int ah = pParams->iArrowHeight;
	int w_, h_;
	#endif
	int iAimedX, iAimedY;
	int Hs = (pContainer->bIsHorizontal ? gldi_desktop_get_height() : gldi_desktop_get_width());
	if (pContainer->bIsHorizontal)
	{
		iAimedX = x0 + pIcon->image.iWidth/2;
		#if GTK_MAJOR_VERSION > 2
		w_ = w - 2 * r;
		h_ = h - 2 * r - ah;
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
		w_ = w - 2 * r - ah;
		h_ = h - 2 * r;
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
	pParams->iAimedX = iAimedX;
	pParams->iAimedY = iAimedY;
}

static void _init_menu_item (GtkWidget *pMenuItem)
{
	int index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pMenuItem), "gldi-text-color"));
	if (index == 0)
	{
		// the following code is to draw the menu item; this is more like a proof of concept
		#if GTK_MAJOR_VERSION > 2
		/**g_signal_connect (G_OBJECT (pMenuItem),
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
			NULL);*/
		#endif
		
		gtk_style_context_add_class (gtk_widget_get_style_context (pMenuItem), "menuitem2");
		
		g_object_set_data (G_OBJECT (pMenuItem), "gldi-text-color", GINT_TO_POINTER(1));
	}
	
	GtkWidget *pSubMenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (pMenuItem));
	if (pSubMenu != NULL)  /// TODO: if it's a sub-menu not made by us (for instance, the NetworkManager indicator), set the drawing callback...
		gtk_container_forall (GTK_CONTAINER (pSubMenu), (GtkCallback) _init_menu_item, NULL);
}

static void _popup_menu (GtkWidget *menu, guint32 time)
{
	GldiMenuParams *pParams = g_object_get_data (G_OBJECT(menu), "gldi-params");
	g_return_if_fail (pParams != NULL);
	
	Icon *pIcon = pParams->pIcon;
	GldiContainer *pContainer = (pIcon ? cairo_dock_get_icon_container (pIcon) : NULL);
	
	// setup the menu for the container
	if (pContainer && pContainer->iface.setup_menu)
		pContainer->iface.setup_menu (pContainer, pIcon, menu);
	
	#if GTK_MAJOR_VERSION > 2
	{
		_init_menu_style ();
	}
	
	if (pIcon && pContainer)
	{
		// hide the icon's label, since menus are placed right above the icon (and therefore, the arrow overlaps the label, which makes it hard to see if both colors are similar).
		if (pIcon->iHideLabel == 0 && pContainer)
			gtk_widget_queue_draw (pContainer->pWidget);
		pIcon->iHideLabel ++;
		
		// ensure margin position is still correct
		_set_margin_position (menu, pParams);
	}
	#endif
	
	gtk_widget_show_all (GTK_WIDGET (menu));
	
	// to draw the items ourselves
	gtk_container_forall (GTK_CONTAINER (menu), (GtkCallback) _init_menu_item, NULL);
	
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
const int dt1 = 20;
const int dt2 = 30;

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
	//GtkStateFlags state;
	GtkStyleContext *context;
	GtkWidget *child;
	gint x, y, w, h, width, height;
	guint border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
	
	child = gtk_bin_get_child (GTK_BIN (widget));
	
	//state = gtk_widget_get_state_flags (widget);
	context = gtk_widget_get_style_context (widget);
	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);
	
	x = border_width;
	y = border_width;
	w = width - border_width * 2;
	h = height - border_width * 2;
	
	// draw the background
	int n = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "gldi-step"));
	if (n != 0)  // so we only draw the barkground if the item is or was selected
	{
		double a = (double)n/N;
		
		cairo_save (cr);
		
		int r=6, l=0;
		cairo_dock_draw_rounded_rectangle (cr, r, l, w - 2*r, h);
		cairo_clip (cr);
		
		gldi_style_colors_set_selected_bg_color (cr);
		cairo_paint_with_alpha (cr, a);
		
		cairo_restore (cr);
	}
	
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
	}
	else if (!child)  // separator
	{
		gboolean wide_separators;
		gint     separator_height;
		GtkBorder padding;
		
		g_print ("SEPARATOR\n");
		gtk_style_context_get_padding (context, 0, &padding);
		gtk_widget_style_get (widget,
			"wide-separators",    &wide_separators,
			"separator-height",   &separator_height,
			NULL);
		if (wide_separators)
			gtk_render_frame (context, cr,
				x + padding.left,
				y + padding.top,
				w - padding.left - padding.right,
				separator_height);
		else
			gtk_render_line (context, cr,
				x + padding.left,
				y + padding.top,
				x + w - padding.right - 1,
				y + padding.top);
	}
	
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
		if (n < N)
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
		}
		return FALSE;
	}
	return TRUE;
}
static gboolean _on_select_menu_item (GtkWidget* pMenuItem, G_GNUC_UNUSED gpointer data)
{
	guint iSidAnimation = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT(pMenuItem), "gldi-animation"));
	if (iSidAnimation != 0)
		g_source_remove (iSidAnimation);
	iSidAnimation = g_timeout_add (dt1, (GSourceFunc)_update_menu_item, pMenuItem);
	g_object_set_data (G_OBJECT(pMenuItem), "gldi-animation", GUINT_TO_POINTER (iSidAnimation));
	
	g_object_set_data (G_OBJECT(pMenuItem), "gldi-inside", GINT_TO_POINTER (TRUE));
	
	return FALSE;
}
static gboolean _on_deselect_menu_item (GtkWidget* pMenuItem, G_GNUC_UNUSED gpointer data)
{
	guint iSidAnimation = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT(pMenuItem), "gldi-animation"));
	if (iSidAnimation != 0)
		g_source_remove (iSidAnimation);
	iSidAnimation = g_timeout_add (dt2, (GSourceFunc)_update_menu_item, pMenuItem);
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

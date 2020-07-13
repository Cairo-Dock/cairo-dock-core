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

extern gchar *g_cCurrentThemePath;


static gboolean _draw_menu_item (GtkWidget *widget, cairo_t *cr, G_GNUC_UNUSED gpointer data);

  ////////////
 /// MENU ///
/////////////


void _init_menu_style (void)
{
	static GtkCssProvider *cssProvider = NULL;
	/**static int s_stamp = 0;
	if (s_stamp == gldi_style_colors_get_stamp())  // if the style has not changed since we last called this function, there is nothing to do
		return;
	s_stamp = gldi_style_colors_get_stamp();
	cd_debug ("%s (%d)", __func__, s_stamp);*/
	cd_debug ("%s (%d)", __func__, myDialogsParam.bUseDefaultColors);
	
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
		// make a css provider
		if (cssProvider == NULL)
		{
			cssProvider = gtk_css_provider_new ();
			gldi_style_colors_freeze ();
			gtk_style_context_add_provider_for_screen (gdk_screen_get_default(), GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
			gldi_style_colors_freeze ();
		}
		
		// css header: define colors from the global style
		GldiColor bg_color;
		if (myDialogsParam.bUseDefaultColors)
			gldi_style_color_get (GLDI_COLOR_BG, &bg_color);
		else
			bg_color = myDialogsParam.fBgColor;
		GldiColor text_color;
		if (myDialogsParam.bUseDefaultColors)
			gldi_style_color_get (GLDI_COLOR_TEXT, &text_color);
		else
			text_color = myDialogsParam.dialogTextDescription.fColorStart;
		GldiColor rgb;  // menuitem bg color: a little darker/lighter than the menu's bg color; also separator color (with no alpha)
		gldi_style_color_shade (&bg_color, GLDI_COLOR_SHADE_MEDIUM, &rgb);
		GldiColor rgbb;  // menuitem border color and menuitem's child bg color (for instance, calendar, scale, etc): a little darker/lighter than the menuitem bg color
		gldi_style_color_shade (&bg_color, GLDI_COLOR_SHADE_STRONG, &rgbb);
		
		gchar *cssheader = g_strdup_printf ("@define-color menuitem_bg_color rgba (%d, %d, %d, %f); \n\
		@define-color menuitem_text_color rgb (%d, %d, %d); \n\
		@define-color menuitem_insensitive_text_color rgba (%d, %d, %d, .5); \n\
		@define-color menuitem_separator_color rgb (%d, %d, %d); \n\
		@define-color menuitem_child_bg_color rgba (%d, %d, %d, %f); \n\
		@define-color menu_bg_color rgba (%d, %d, %d, %f);\n",
			(int)(rgb.rgba.red*255), (int)(rgb.rgba.green*255), (int)(rgb.rgba.blue*255), rgb.rgba.alpha,
			(int)(text_color.rgba.red*255), (int)(text_color.rgba.green*255), (int)(text_color.rgba.blue*255),
			(int)(text_color.rgba.red*255), (int)(text_color.rgba.green*255), (int)(text_color.rgba.blue*255),
			(int)(rgb.rgba.red*255), (int)(rgb.rgba.green*255), (int)(rgb.rgba.blue*255),
			(int)(rgbb.rgba.red*255), (int)(rgbb.rgba.green*255), (int)(rgbb.rgba.blue*255), rgbb.rgba.alpha,
			(int)(bg_color.rgba.red*255), (int)(bg_color.rgba.green*255), (int)(bg_color.rgba.blue*255), bg_color.rgba.alpha);
		
		// css body: load a custom file if it exists
		gchar *cCustomCss = NULL;
		gchar *cCustomCssFile = g_strdup_printf ("%s/menu.css", g_cCurrentThemePath);  // this is mainly for advanced customizing and to be able to work around some gtk themes that could pose problems; avoid using it in public themes, since it's not available to normal user from the config window
		if (g_file_test (cCustomCssFile, G_FILE_TEST_EXISTS))
		{
			gsize length = 0;
			g_file_get_contents (cCustomCssFile,
				&cCustomCss,
				&length,
				NULL);
		}
		
		gchar *css;
		if (cCustomCss != NULL)
		{
			css = g_strconcat (cssheader, cCustomCss, NULL);
		}
		else
		{
			css = g_strconcat (cssheader,
			".gldimenuitem * { \
				/*engine: none;*/ \
				-unico-focus-border-color: alpha (@menuitem_child_bg_color, .6); \
				-unico-focus-fill-color: alpha (@menuitem_child_bg_color, .2); \
			} \
			.gldimenuitem { \
				text-shadow: none; \
				border-image: none; \
				box-shadow: none; \
				background: transparent; \
				color: @menuitem_text_color; \
				border-color: transparent; \
				-unico-border-gradient: none; \
				-unico-inner-stroke-width: 0px; \
				-unico-outer-stroke-width: 0px; \
				-unico-bullet-color: transparent; \
				-unico-glow-color: transparent; \
				-unico-glow-radius: 0; \
			} \
			.gldimenuitem GtkImage, \
			.gldimenuitem .image { \
				background: transparent; \
			} \
			.gldimenuitem.separator, \
			.gldimenuitem .separator { \
				color: @menuitem_separator_color; \
				background-color: @menuitem_separator_color; \
				border-width: 1px; \
				border-style: solid; \
				border-image: none; \
				border-color: @menuitem_separator_color; \
				border-bottom-color: alpha (@menuitem_separator_color, 0.6); \
				border-right-color: alpha (@menuitem_separator_color, 0.6); \
				-unico-inner-stroke-color: transparent; \
			} \
			.gldimenuitem:hover{ \
				background-color: @menuitem_bg_color; \
				background-image: none; \
				text-shadow: none; \
				border-image: none; \
				box-shadow: none; \
				color: @menuitem_text_color; \
				border-radius: 5px; \
				border-style: solid; \
				border-color: @menuitem_child_bg_color; \
				-unico-inner-stroke-color: transparent; \
			} \
			.gldimenuitem *:disabled { \
				text-shadow: none; \
				color: @menuitem_insensitive_text_color; \
				background: transparent; \
			} \
			.gldimenuitem .entry, \
			.gldimenuitem.entry { \
				background: @menuitem_bg_color; \
				border-width: 1px; \
				border-style: solid; \
				border-image: none; \
				border-color: @menuitem_child_bg_color; \
				color: @menuitem_text_color; \
				-unico-border-gradient: none; \
				-unico-border-width: 0px; \
				-unico-inner-stroke-width: 0px; \
				-unico-outer-stroke-width: 0px; \
			} \
			.gldimenuitem .button, \
			.gldimenuitem.button { \
				background-color: @menuitem_bg_color; \
				background-image: none; \
				box-shadow: none; \
				border-image: none; \
				border-color: @menuitem_child_bg_color; \
				border-width: 1px; \
				border-style: solid;padding: 2px; \
				-unico-focus-outer-stroke-color: transparent; \
			} \
			.gldimenuitem .scale, \
			.gldimenuitem.scale { \
				background-color: @menuitem_bg_color; \
				background-image: none; \
				color: @menuitem_text_color; \
				border-width: 1px; \
				border-style: solid; \
				border-image: none; \
				border-color: @menuitem_child_bg_color; \
				-unico-border-width: 0px; \
				-unico-inner-stroke-width: 0px; \
				-unico-outer-stroke-width: 0px; \
			} \
			.gldimenuitem .scale.left, \
			.gldimenuitem.scale.left { \
				background-color: @menuitem_bg_color; \
				background-image: none; \
				border-image: none; \
				-unico-border-gradient: none; \
				-unico-inner-stroke-color: transparent; \
				-unico-inner-stroke-gradient: none; \
				-unico-inner-stroke-width: 0px; \
				-unico-outer-stroke-color: transparent; \
				-unico-outer-stroke-gradient: none; \
				-unico-outer-stroke-width: 0; \
			} \
			.gldimenuitem .scale.slider, \
			.gldimenuitem.scale.slider { \
				background-color: @menuitem_text_color; \
				background-image: none; \
				border-image: none; \
			} \
			.gldimenuitem GtkCalendar, \
			.gldimenuitem GtkCalendar.button, \
			.gldimenuitem GtkCalendar.header, \
			.gldimenuitem GtkCalendar.view { \
				background-color: @menuitem_bg_color; \
				background-image: none; \
				color: @menuitem_text_color; \
			} \
			.gldimenuitem GtkCalendar { \
				background-color: @menuitem_child_bg_color; \
				background-image: none; \
			} \
			.gldimenuitem GtkCalendar:indeterminate { \
				color: shade (@menuitem_child_bg_color, 0.6); \
			} \
			.gldimenuitem .toolbar .button, \
			.gldimenuitem column-header .button  { \
				color: @menuitem_text_color; \
				text-shadow: none; \
			} \
			.gldimenuitem row { \
				color: @menuitem_text_color; \
				text-shadow: none; \
				background-color: @menu_bg_color; \
				background-image: none; \
			} \
			.gldimenuitem row:selected { \
				color: @menuitem_text_color; \
				text-shadow: none; \
				background-color: @menuitem_bg_color; \
				background-image: none; \
				border-color: @menuitem_child_bg_color; \
			} \
			.gldimenuitem .check, \
			.gldimenuitem.check{ \
				color: @menuitem_text_color; \
				background-color: @menuitem_bg_color; \
				background-image: none; \
				border-width: 1px; \
				border-style: solid; \
				border-image: none; \
				border-color: @menuitem_child_bg_color; \
				-unico-focus-outer-stroke-color: transparent; \
				-unico-focus-inner-stroke-color: transparent; \
				-unico-inner-stroke-width: 0px; \
				-unico-outer-stroke-width: 0px; \
				-unico-border-gradient: none; \
				-unico-border-width: 0px; \
				-unico-border-gradient: none; \
				-unico-bullet-color: @menuitem_text_color; \
				-unico-bullet-outline-color: @menuitem_text_color; \
				-unico-border-gradient: none; \
			} \
			.gldimenu { \
				background-color: @menu_bg_color; \
				background-image: none; \
				color: @menuitem_text_color; \
			} \
			.window-frame { \
				box-shadow: none; \
			}",
			NULL);  // we also define ".menu", so that custom widgets (like in the SoundMenu) can get our colors. Note that we don't redefine Gtk's menuitem, because we want to keep normal menus for GUI
			// for "entry", using "background-color" will not affect entries inside another widget (like a box), we actually have to use "background" ... (TBC with gtk > 3.6)
			// for ".window-frame": remove shadow added by some WMs (Marco/Metacity) to the menu (LP #1407880)
		}
		
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
	// reset the clip set by GTK, to allow us draw in the margin of the widget
	cairo_reset_clip(pCairoContext);
	
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
	// new positioning code should work on both X11 and Wayland, but, by default, it is used
	// only on Wayland, unless it is specifically requested in the cmake configuration
	gboolean use_new_positioning = FALSE;
	#if (CAIRO_DOCK_USE_NEW_POSITIONING_ON_X11 == 1)
		use_new_positioning = TRUE;
	#else
		use_new_positioning = gldi_container_is_wayland_backend ();
	#endif
	if (use_new_positioning)
	{
		if (pContainer->bIsHorizontal)
		{
			if (pContainer->bDirectionUp) iMarginPosition = 0;
			else iMarginPosition = 1;
		}
		else
		{
			if (pContainer->bDirectionUp) iMarginPosition = 2;
			else iMarginPosition = 3;
		}
	}
	else
	{
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
	}
	
	// store the result, and allocate some space to draw the arrow
	if (iMarginPosition != pParams->iMarginPosition)  // margin position is now defined or has changed -> update it on the menu
	{
		// store the value
		pParams->iMarginPosition = iMarginPosition;
		
		// get/add a css
		// actually gtk_widget_set_margin_xxx works, but then GTK adds a translation to the cairo_context, forcing each renderer to offset its drawing by gtk_widget_get_margin_xxx()
		// also, gtk_widget_get_allocation() doesn't take into account the margin, forcing each renderer to add it
		// so in the end it's better not to use it
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
		css = g_strdup_printf ("GtkMenu,menu { \
			padding-bottom: %dpx; \
			padding-top: %dpx; \
			padding-right: %dpx; \
			padding-left: %dpx; \
		}", b, t, r, l);  // we must define all the paddings, else if the margin position changes, clearing the css won't make the padding disappear; also, 'GtkMenu' is old
		gtk_css_provider_load_from_data (cssProvider,
			css, -1, NULL);
		g_free (css);
	}
}

GtkWidget *gldi_menu_new (Icon *pIcon)
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
	/* Steal data: with GTK 3.14, we receive two 'popup' signals and for the
	 * second one, a 'destroy' signal has already been sent! Then, 'pParams'
	 * will not be correct... https://bugzilla.gnome.org/738537
	 */
	GldiMenuParams *pParams = g_object_steal_data (G_OBJECT (pMenu), "gldi-params");
	if (!pParams)
		return;

	Icon *pIcon = pParams->pIcon;
	if (pIcon)
		gldi_object_remove_notification (pIcon,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_icon_destroyed,
			pMenu);
	if (pParams->cssProvider)
	{
		g_object_unref (pParams->cssProvider);  /// need to remove the provider from the style context ?... probably not since the style context will be destroyed and will release its reference on the provider
	}
	g_free (pParams);
}

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

void gldi_menu_init (GtkWidget *pMenu, Icon *pIcon)
{
	g_return_if_fail (g_object_get_data (G_OBJECT (pMenu), "gldi-params") == NULL);
	
	#if (CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1)
	gtk_menu_set_reserve_toggle_size (GTK_MENU(pMenu), TRUE);
	#endif

	// connect to 'draw' event to draw the menu (background and items)
	GtkWidget *pWindow = gtk_widget_get_toplevel (pMenu);
	cairo_dock_set_default_rgba_visual (pWindow);
	
	g_signal_connect (G_OBJECT (pMenu),
		"draw",
		G_CALLBACK (_draw_menu),
		pMenu);

	gtk_style_context_add_class (gtk_widget_get_style_context (pMenu), "gldimenu");

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
			
			// set transient for (parent relationship; needed for positioning on Wayland)
			// note: it is an error to try to map (and position) a popup
			// relative to a window that is not mapped; we need to take care of this
			GtkWindow *tmp = GTK_WINDOW (pContainer->pWidget);
			while (tmp && !gtk_widget_get_mapped (GTK_WIDGET(tmp)))
				tmp = gtk_window_get_transient_for (tmp);
			gtk_window_set_transient_for (GTK_WINDOW (pWindow), tmp);
		}
	}
}


static void _menu_realized_cb (GtkWidget *widget, gpointer user_data)
{
	GldiMenuParams *pParams = (GldiMenuParams*)user_data;
	g_return_if_fail (pParams != NULL);
	
	int w, h;  // taille menu
	GtkRequisition requisition;
	gtk_widget_get_preferred_size (widget, NULL, &requisition);  // retrieve the natural size; Note: before gtk3.10 we used the minimum size but it's now incorrect; the natural size works for prior versions too.
	w = requisition.width;
	h = requisition.height;
	
	Icon *pIcon = pParams->pIcon;
	
	gldi_container_calculate_aimed_point (pIcon, w, h, pParams->iMarginPosition, &(pParams->iAimedX), &(pParams->iAimedY));
	
	gint menuX, menuY;
	gtk_window_get_position (GTK_WINDOW (gtk_widget_get_toplevel (widget)), &menuX, &menuY);
	// g_print ("menu aimed at: %d, %d; position: %d, %d\n", pParams->iAimedX, pParams->iAimedY, menuX, menuY);
	pParams->iAimedX += menuX;
	pParams->iAimedY += menuY;
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
	gtk_widget_get_preferred_size (GTK_WIDGET (menu), NULL, &requisition);  // retrieve the natural size; Note: before gtk3.10 we used the minimum size but it's now incorrect; the natural size works for prior versions too.
	w = requisition.width;
	h = requisition.height;

	/// TODO: use iMarginPosition...
	double fAlign = pParams->fAlign;
	int r = pParams->iRadius;
	int ah = pParams->iArrowHeight;
	int w_, h_;
	int iAimedX, iAimedY;
	int Hs = (pContainer->bIsHorizontal ? gldi_desktop_get_height() : gldi_desktop_get_width());
	if (pContainer->bIsHorizontal)
	{
		iAimedX = x0 + pIcon->image.iWidth/2;
		w_ = w - 2 * r;
		h_ = h - 2 * r - ah;
		*x = MAX (0, iAimedX - fAlign * w_ - r);
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
		w_ = w - 2 * r - ah;
		h_ = h - 2 * r;
		*y = MIN (iAimedY - fAlign * h_ - r, gldi_desktop_get_height() - h);
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
	GtkWidget *pSubMenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (pMenuItem));
	
	// add our class on the menu-item; the style of this class is (will be) defined in a css, which will override the default gtkmenuitem style.
	gboolean bStyleIsSet = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (pMenuItem), "gldi-style-set"));
	if (! bStyleIsSet)  // not done yet -> do it once
	{
		// draw the menu items; actually, we only want to draw the separators, which are not well drawn by GTK
		g_signal_connect (G_OBJECT (pMenuItem),
			"draw",
			G_CALLBACK (_draw_menu_item),
			NULL);
		
		gtk_style_context_add_class (gtk_widget_get_style_context (pMenuItem), "gldimenuitem");
		
		if (pSubMenu != NULL)  // if this item has a sub-menu, init it as well
			gldi_menu_init (pSubMenu, NULL);
		
		g_object_set_data (G_OBJECT (pMenuItem), "gldi-style-set", GINT_TO_POINTER(1));
		
	}
	
	// iterate on sub-menu's items
	if (pSubMenu != NULL)
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

	// init each items (and sub-menus), in case it contains some foreign GtkMenuItems (for instance in case of an indicator menu or the gtk recent files sub-menu, which can have new items at any time)
	gtk_container_forall (GTK_CONTAINER (menu), (GtkCallback) _init_menu_item, NULL);  // init each menu-item style
	
	if (pIcon && pContainer)
	{
		// hide the icon's label, since menus are placed right above the icon (and therefore, the arrow overlaps the label, which makes it hard to see if both colors are similar).
		if (pIcon->iHideLabel == 0 && pContainer)
			gtk_widget_queue_draw (pContainer->pWidget);
		pIcon->iHideLabel ++;
		
		// ensure margin position is still correct
		_set_margin_position (menu, pParams);
	}
	
	// new positioning code should work on both X11 and Wayland, but, by default, it is used
	// only on Wayland, unless it is specifically requested in the cmake configuration
	gboolean use_new_positioning = FALSE;
	#if (CAIRO_DOCK_USE_NEW_POSITIONING_ON_X11 == 1)
		use_new_positioning = TRUE;
	#else
		use_new_positioning = gldi_container_is_wayland_backend ();
	#endif
	
	if (use_new_positioning)
		g_signal_connect (GTK_WIDGET (menu), "realize",
			G_CALLBACK (_menu_realized_cb), pParams);
	
	gtk_widget_show_all (GTK_WIDGET (menu));
	
	if (use_new_positioning)
	{
		if (pContainer && pIcon)
		{
			GdkRectangle rect = {0, 0, 1, 1};
			GdkGravity rect_anchor = GDK_GRAVITY_NORTH;
			GdkGravity menu_anchor = GDK_GRAVITY_SOUTH;
			gldi_container_calculate_rect (pContainer, pIcon, &rect, &rect_anchor, &menu_anchor);
			gtk_menu_popup_at_rect (GTK_MENU (menu), gtk_widget_get_window (pContainer->pWidget),
				&rect, rect_anchor, menu_anchor, NULL);
		}
		else
		{
			gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
		}
	}
	else
	{
		gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			pIcon != NULL && pContainer != NULL ? _place_menu_on_icon : NULL,
			NULL,
			0,
			time);
	}
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


static gboolean _draw_menu_item (GtkWidget *widget,
	cairo_t *cr,
	G_GNUC_UNUSED gpointer data)
{
	if (! GTK_IS_SEPARATOR_MENU_ITEM(widget))  // not a separator => skip drawing anything and let GTK handle it
		return FALSE;
	
	// get menu's geometry
	guint border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
	gint width = gtk_widget_get_allocated_width (widget);
	gint height = gtk_widget_get_allocated_height (widget);

	gint x, y, w;
	x = border_width;
	y = border_width;
	w = width - border_width * 2;

	// get the line color of the menu
	GldiColor rgb;
	if (myDialogsParam.bUseDefaultColors)
		gldi_style_color_get (GLDI_COLOR_LINE, &rgb);
	else
		rgb = myDialogsParam.fLineColor;
	
	// make a pattern with the alpha channel: 0 - 0.1 ---- .9 - 1
	int mb = w*.05;  // margin to border
	cairo_pattern_t *pattern;
	pattern = cairo_pattern_create_linear (x+mb, y, x+w-mb, y);
	cairo_pattern_add_color_stop_rgba (pattern, 0., rgb.rgba.red, rgb.rgba.green, rgb.rgba.blue, rgb.rgba.alpha*.1);
	cairo_pattern_add_color_stop_rgba (pattern, .1, rgb.rgba.red, rgb.rgba.green, rgb.rgba.blue, rgb.rgba.alpha);
	cairo_pattern_add_color_stop_rgba (pattern, .9, rgb.rgba.red, rgb.rgba.green, rgb.rgba.blue, rgb.rgba.alpha);
	cairo_pattern_add_color_stop_rgba (pattern, 1., rgb.rgba.red, rgb.rgba.green, rgb.rgba.blue, rgb.rgba.alpha*.1);
	cairo_set_source (cr, pattern);
	
	// draw the separator as a 1px line with a margin from the border
	cairo_move_to(cr, x+mb, y);
	cairo_set_line_width (cr, 1);
	cairo_line_to(cr, x+w-mb, y);
	cairo_stroke(cr);
	cairo_pattern_destroy (pattern);
	
	return TRUE;  // intercept
}

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
			if (pixbuf)
			{
				image = gtk_image_new_from_pixbuf (pixbuf);
				g_object_unref (pixbuf);
			}
		}
		else if (*cImage != '\0')
		{
			image = gtk_image_new_from_icon_name (cImage, iSize);
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
		#if (CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1)
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (pMenuItem), TRUE);
		#endif
#endif
	}

	_init_menu_item (pMenuItem);

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
	#if (CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1)
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

void gldi_menu_add_separator (GtkWidget *pMenu)
{
	GtkWidget *pMenuItem = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	_init_menu_item (pMenuItem);
}

gboolean GLDI_IS_IMAGE_MENU_ITEM (GtkWidget *pMenuItem)  // defined as a function to not export gtk3imagemenuitem.h
{
	#if GTK_CHECK_VERSION (3, 10, 0)
	return GTK3_IS_IMAGE_MENU_ITEM (pMenuItem);
	#else
	return GTK_IS_IMAGE_MENU_ITEM (pMenuItem);
	#endif
}

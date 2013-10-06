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

void gldi_menu_init (G_GNUC_UNUSED GtkWidget *pMenu, G_GNUC_UNUSED Icon *pIcon)
{
	#if (CAIRO_DOCK_FORCE_ICON_IN_MENUS == 1)
	gtk_menu_set_reserve_toggle_size (GTK_MENU(pMenu), TRUE);
	#endif
	
	if (pIcon != NULL)  // the menu points on an icon
	{
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
	
	int Hs = (pContainer->bIsHorizontal ? gldi_desktop_get_height() : gldi_desktop_get_width());
	//g_print ("%d;%d %dx%d\n", x0, y0, w, h);
	if (pContainer->bIsHorizontal)
	{
		*x = x0;
		if (y0 > Hs/2)  // pContainer->bDirectionUp
			*y = y0 - h;
		else
			*y = y0 + pIcon->fHeight * pIcon->fScale;
	}
	else
	{
		*y = MIN (x0, gldi_desktop_get_height() - h);
		if (y0 > Hs/2)  // pContainer->bDirectionUp
			*x = y0 - w;
		else
			*x = y0 + pIcon->fHeight * pIcon->fScale;
	}
}
static void _popup_menu (GtkWidget *menu, guint32 time)
{
	Icon *pIcon = g_object_get_data (G_OBJECT(menu), "gldi-icon");
	GldiContainer *pContainer = (pIcon ? cairo_dock_get_icon_container (pIcon) : NULL);
	
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

GtkWidget *gldi_menu_item_new_full (const gchar *cLabel, const gchar *cImage, gboolean bUseMnemonic, GtkIconSize iSize)
{
	if (iSize == 0)
		iSize = GTK_ICON_SIZE_MENU;
	
	GtkWidget *pMenuItem;
	if (! cImage)
	{
		if (! cLabel)
			return gtk_menu_item_new ();
		else
			return (bUseMnemonic ? gtk_menu_item_new_with_mnemonic (cLabel) : gtk_menu_item_new_with_label (cLabel));
	}
	
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

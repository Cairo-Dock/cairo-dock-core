
#ifndef __CAIRO_DOCK_MENU__
#define  __CAIRO_DOCK_MENU__

#include <gtk/gtk.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


/**
*Ajoute une entree avec une icone GTK a un menu deja existant.
*@param cLabel nom de l'entree, tel qu'il apparaitra dans le menu.
*@param gtkStock nom d'une icone de GTK.
*@param pFunction fonction appelee lors de la selection de cette entree.
*@param pMenu GtkWidget du menu auquel on rajoutera l'entree.
*@param pData donnees passees en parametre de la fonction (doit contenir myApplet).
*/
#define CAIRO_DOCK_ADD_IN_MENU_WITH_STOCK_AND_DATA(cLabel, gtkStock, pFunction, pMenu, pData) do { \
	pMenuItem = gtk_image_menu_item_new_with_label (cLabel); \
	image = gtk_image_new_from_stock (gtkStock, GTK_ICON_SIZE_MENU); \
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image); \
	gtk_menu_shell_append  (GTK_MENU_SHELL (pMenu), pMenuItem); \
	g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK(pFunction), pData); } while (0)

GtkWidget *cairo_dock_build_menu (Icon *icon, CairoContainer *pContainer);


gboolean cairo_dock_notification_build_menu (gpointer *pUserData, Icon *icon, CairoContainer *pContainer, GtkWidget *menu);

void cairo_dock_delete_menu (GtkMenuShell *menu, CairoDock *pDock);

G_END_DECLS
#endif

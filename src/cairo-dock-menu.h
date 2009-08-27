
#ifndef __CAIRO_DOCK_MENU__
#define  __CAIRO_DOCK_MENU__

#include <gtk/gtk.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-menu.h This class manages the menu of Cairo-Dock.
* It is called on a left click on a Container, builds a main menu, and notifies everybody about it, so that the menu is completed.
*/

/** Build the main menu of a Container.
*@param icon the icon that was left-clicked, or NULL if none.
*@param pContainer the container that was left-clicked.
*/
GtkWidget *cairo_dock_build_menu (Icon *icon, CairoContainer *pContainer);


gboolean cairo_dock_notification_build_menu (gpointer *pUserData, Icon *icon, CairoContainer *pContainer, GtkWidget *menu);


void cairo_dock_delete_menu (GtkMenuShell *menu, CairoDock *pDock);

/** Pop-up a menu on a container. In the case of a dock, it prevents this one from shrinking down.
*@param menu the menu.
*@param pContainer the container that was clicked.
*/
void cairo_dock_popup_menu_on_container (GtkWidget *menu, CairoContainer *pContainer);

/*
*Ajoute une entree avec une icone (GTK ou chemin) a un menu deja existant.
*@param cLabel nom de l'entree, tel qu'il apparaitra dans le menu.
*@param gtkStock nom d'une icone de GTK ou chemin complet d'une image quelconque.
*@param pFunction fonction appelee lors de la selection de cette entree.
*@param pMenu GtkWidget du menu auquel on rajoutera l'entree.
*@param pData donnees passees en parametre de la fonction (doit contenir myApplet).
*/
GtkWidget *cairo_dock_add_in_menu_with_stock_and_data (const gchar *cLabel, const gchar *gtkStock, GFunc pFunction, GtkWidget *pMenu, gpointer pData);


G_END_DECLS
#endif

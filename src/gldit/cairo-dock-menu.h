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

#ifndef __GLDI_MENU__
#define  __GLDI_MENU__

#include "cairo-dock-struct.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-menu.h This class defines the Menu. They are classical menus, but with a custom looking.
*/

struct _GldiMenuParams {
	Icon *pIcon;
	gint iMarginPosition;
	gint iAimedX, iAimedY;
	gdouble fAlign;
	gint iRadius;  // actually it's more an horizontal padding/offset
	gint iArrowHeight;
	GtkCssProvider *cssProvider;  // a css to define the margins of the menu
};
typedef struct _GldiMenuParams GldiMenuParams;

struct _GldiMenuItemParams {
	guint iSidAnimation;
	gint iStep;
	gboolean bInside;
};


void _init_menu_style (void);

/** Creates a new menu that will point on a given Icon. If the Icon is NULL, it will be placed under the mouse.
 * @param pIcon the icon, or NULL
 * @return the new menu.
 */
GtkWidget *gldi_menu_new (Icon *pIcon);

/** Creates a new sub-menu. It's just a menu that doesn't point on an Icon/Container.
 */
#define gldi_submenu_new(...) gldi_menu_new (NULL)

/** Initialize a menu, so that it can be drawn and placed correctly.
 * It's useful if the menu was created beforehand (like a DbusMenu).
 * @param pIcon the icon, or NULL
 */
void gldi_menu_init (GtkWidget *pMenu, Icon *pIcon);

/** Pop-up a menu. The menu is placed above the icon, or above the container, or above the mouse, depending on how it has been initialized.
*@param menu the menu.
*@param event an event to which the menu is popped up in response (NULL to use the current GTK event)
*/
void gldi_menu_popup_full (GtkWidget *menu, const GdkEvent *event);
#define gldi_menu_popup(menu) gldi_menu_popup_full (menu, NULL)


/** Creates a menu-item, with a label and an image. The child widget of the menu-item is a gtk-label.
 * If the label is NULL, the child widget will be NULL too (this is useful if the menu-item will hold a custom widget).
 * @param cLabel the label, or NULL
 * @param cImage the image path or name, or NULL
 * @param bUseMnemonic whether to use the mnemonic inside the label or not
 * @param iSize size of the image, or 0 to use the default size
 * @return the new menu-item.
 */
GtkWidget *gldi_menu_item_new_full (const gchar *cLabel, const gchar *cImage, gboolean bUseMnemonic, GtkIconSize iSize);

/** A convenient function to create a menu-item with a label and an image.
 * @param cLabel the label, or NULL
 * @param cImage the image path or name, or NULL
 * @return the new menu-item.
 */
#define gldi_menu_item_new(cLabel, cImage) gldi_menu_item_new_full (cLabel, cImage, FALSE, 0)

/** A convenient function to create a menu-item with a label, an image, and an associated action.
 * @param cLabel the label, or NULL
 * @param cImage the image path or name, or NULL
 * @param pFunction the callback
 * @param pData the data passed to the callback
 * @return the new menu-item.
 */
GtkWidget *gldi_menu_item_new_with_action (const gchar *cLabel, const gchar *cImage, GCallback pFunction, gpointer pData);

/** A convenient function to create a menu-item with a label, an image, and an associated sub-menu.
 * @param cLabel the label
 * @param cImage the image path or name, or NULL
 * @param pSubMenuPtr pointer that will contain the new sub-menu, or NULL
 * @return the new menu-item.
 */
GtkWidget *gldi_menu_item_new_with_submenu (const gchar *cLabel, const gchar *cImage, GtkWidget **pSubMenuPtr);


/** Sets a gtk-image on a menu-item. This is useful if the image can't be given by a name or path (for instance, loaded from a cairo surface).
 * @param pMenuItem the menu-item
 * @param image the image
 */
void gldi_menu_item_set_image (GtkWidget *pMenuItem, GtkWidget *image);

/** Gets the image of a menu-item.
 * @param pMenuItem the menu-item
 * @return the gtk-image
 */
GtkWidget *gldi_menu_item_get_image (GtkWidget *pMenuItem);


/** A convenient function to add an item to a given menu.
 * @param pMenu the menu
 * @param cLabel the label, or NULL
 * @param cImage the image path or name, or NULL
 * @param pFunction the callback
 * @param pData the data passed to the callback
 * @return the new menu-entry that has been added.
 */
GtkWidget *gldi_menu_add_item (GtkWidget *pMenu, const gchar *cLabel, const gchar *cImage, GCallback pFunction, gpointer pData);
#define cairo_dock_add_in_menu_with_stock_and_data(cLabel, gtkStock, pFunction, pMenu, pData) gldi_menu_add_item (pMenu, cLabel, gtkStock, pFunction, pData)

/** A convenient function to add a sub-menu to a given menu.
 * @param pMenu the menu
 * @param cLabel the label, or NULL
 * @param cImage the image path or name, or NULL
 * @param pMenuItemPtr pointer that will contain the new menu-item, or NULL
 * @return the new sub-menu that has been added.
 */
GtkWidget *gldi_menu_add_sub_menu_full (GtkWidget *pMenu, const gchar *cLabel, const gchar *cImage, GtkWidget **pMenuItemPtr);

/** A convenient function to add a sub-menu to a given menu.
 * @param pMenu the menu
 * @param cLabel the label, or NULL
 * @param cImage the image path or name, or NULL
 * @return the new sub-menu that has been added.
 */
#define gldi_menu_add_sub_menu(pMenu, cLabel, cImage) gldi_menu_add_sub_menu_full (pMenu, cLabel, cImage, NULL)
#define cairo_dock_create_sub_menu(cLabel, pMenu, cImage) gldi_menu_add_sub_menu (pMenu, cLabel, cImage)

/** A convenient function to add a separator to a given menu.
 * @param pMenu the menu
 */
void gldi_menu_add_separator (GtkWidget *pMenu);

gboolean GLDI_IS_IMAGE_MENU_ITEM (GtkWidget *pMenuItem);

G_END_DECLS
#endif

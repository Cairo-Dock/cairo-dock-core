/* GTK - The GIMP Toolkit
 * Copyright (C) Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK3_IMAGE_MENU_ITEM_H__
#define __GTK3_IMAGE_MENU_ITEM_H__


#include <gtk/gtk.h>


G_BEGIN_DECLS

#define GTK3_TYPE_IMAGE_MENU_ITEM            (gtk3_image_menu_item_get_type ())
#define GTK3_IMAGE_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK3_TYPE_IMAGE_MENU_ITEM, Gtk3ImageMenuItem))
#define GTK3_IMAGE_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK3_TYPE_IMAGE_MENU_ITEM, Gtk3ImageMenuItemClass))
#define GTK3_IS_IMAGE_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK3_TYPE_IMAGE_MENU_ITEM))
#define GTK3_IS_IMAGE_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK3_TYPE_IMAGE_MENU_ITEM))
#define GTK3_IMAGE_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK3_TYPE_IMAGE_MENU_ITEM, Gtk3ImageMenuItemClass))


typedef struct _Gtk3ImageMenuItem              Gtk3ImageMenuItem;
typedef struct _Gtk3ImageMenuItemPrivate       Gtk3ImageMenuItemPrivate;
typedef struct _Gtk3ImageMenuItemClass         Gtk3ImageMenuItemClass;

struct _Gtk3ImageMenuItem
{
  GtkMenuItem menu_item;

  /*< private >*/
  Gtk3ImageMenuItemPrivate *priv;
};

struct _Gtk3ImageMenuItemClass
{
  GtkMenuItemClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType	   gtk3_image_menu_item_get_type          (void) G_GNUC_CONST;
GtkWidget* gtk3_image_menu_item_new               (void);
GtkWidget* gtk3_image_menu_item_new_with_label    (const gchar      *label);
GtkWidget* gtk3_image_menu_item_new_with_mnemonic (const gchar      *label);
GtkWidget* gtk3_image_menu_item_new_from_stock    (const gchar      *stock_id,
                                                  GtkAccelGroup    *accel_group);
void       gtk3_image_menu_item_set_always_show_image (Gtk3ImageMenuItem *image_menu_item,
                                                      gboolean          always_show);
gboolean   gtk3_image_menu_item_get_always_show_image (Gtk3ImageMenuItem *image_menu_item);
void       gtk3_image_menu_item_set_image         (Gtk3ImageMenuItem *image_menu_item,
                                                  GtkWidget        *image);
GtkWidget* gtk3_image_menu_item_get_image         (Gtk3ImageMenuItem *image_menu_item);
void       gtk3_image_menu_item_set_use_stock     (Gtk3ImageMenuItem *image_menu_item,
						  gboolean          use_stock);
gboolean   gtk3_image_menu_item_get_use_stock     (Gtk3ImageMenuItem *image_menu_item);
void       gtk3_image_menu_item_set_accel_group   (Gtk3ImageMenuItem *image_menu_item, 
						  GtkAccelGroup    *accel_group);

G_END_DECLS

#endif /* __GTK3_IMAGE_MENU_ITEM_H__ */

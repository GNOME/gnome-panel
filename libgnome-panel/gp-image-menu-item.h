/*
 * Copyright (C) 2018-2020 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GP_IMAGE_MENU_ITEM_H
#define GP_IMAGE_MENU_ITEM_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * GP_TYPE_IMAGE_MENU_ITEM:
 *
 * The type for GpImageMenuItem.
 */
#define GP_TYPE_IMAGE_MENU_ITEM (gp_image_menu_item_get_type ())
G_DECLARE_DERIVABLE_TYPE (GpImageMenuItem, gp_image_menu_item,
                          GP, IMAGE_MENU_ITEM, GtkMenuItem)

/**
 * GpImageMenuItemClass:
 * @parent_class: The parent class.
 *
 * The class structure for the #GpImageMenuItem class.
 */
struct _GpImageMenuItemClass
{
  GtkMenuItemClass parent_class;

  /*< private >*/
  gpointer         padding[10];
};

GtkWidget *gp_image_menu_item_new               (void);

GtkWidget *gp_image_menu_item_new_with_label    (const gchar     *label);

GtkWidget *gp_image_menu_item_new_with_mnemonic (const gchar     *label);

void       gp_image_menu_item_set_image         (GpImageMenuItem *self,
                                                 GtkWidget       *image);

G_END_DECLS

#endif

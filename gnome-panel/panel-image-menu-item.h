/* GTK - The GIMP Toolkit
 * Copyright (C) Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * This is modified version of gtkimagemenuitem.h copied from GTK+
 * 3.8.9 release:
 * https://git.gnome.org/browse/gtk+/plain/gtk/gtkimagemenuitem.h?id=3.8.9
 */

#ifndef PANEL_IMAGE_MENU_ITEM_H
#define PANEL_IMAGE_MENU_ITEM_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PANEL_TYPE_IMAGE_MENU_ITEM         (panel_image_menu_item_get_type ())
#define PANEL_IMAGE_MENU_ITEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_IMAGE_MENU_ITEM, PanelImageMenuItem))
#define PANEL_IMAGE_MENU_ITEM_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    PANEL_TYPE_IMAGE_MENU_ITEM, PanelImageMenuItemClass))
#define PANEL_IS_IMAGE_MENU_ITEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_IMAGE_MENU_ITEM))
#define PANEL_IS_IMAGE_MENU_ITEM_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    PANEL_TYPE_IMAGE_MENU_ITEM))
#define PANEL_IMAGE_MENU_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  PANEL_TYPE_IMAGE_MENU_ITEM, PanelImageMenuItemClass))

typedef struct _PanelImageMenuItem        PanelImageMenuItem;
typedef struct _PanelImageMenuItemPrivate PanelImageMenuItemPrivate;
typedef struct _PanelImageMenuItemClass   PanelImageMenuItemClass;

struct _PanelImageMenuItem {
  GtkMenuItem                parent;
  PanelImageMenuItemPrivate *priv;
};

struct _PanelImageMenuItemClass {
  GtkMenuItemClass parent_class;
};

GType	   panel_image_menu_item_get_type              (void) G_GNUC_CONST;
GtkWidget* panel_image_menu_item_new                   (void);
GtkWidget* panel_image_menu_item_new_with_label        (const gchar        *label);
GtkWidget* panel_image_menu_item_new_with_mnemonic     (const gchar        *label);
void       panel_image_menu_item_set_always_show_image (PanelImageMenuItem *image_menu_item,
                                                        gboolean            always_show);
gboolean   panel_image_menu_item_get_always_show_image (PanelImageMenuItem *image_menu_item);
void       panel_image_menu_item_set_image             (PanelImageMenuItem *image_menu_item,
                                                        GtkWidget          *image);
GtkWidget* panel_image_menu_item_get_image             (PanelImageMenuItem *image_menu_item);

G_END_DECLS

#endif

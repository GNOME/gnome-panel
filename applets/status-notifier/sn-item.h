/*
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SN_ITEM_H
#define SN_ITEM_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SN_TYPE_ITEM sn_item_get_type ()
G_DECLARE_DERIVABLE_TYPE (SnItem, sn_item, SN, ITEM, GtkButton)

typedef enum
{
  SN_ITEM_CATEGORY_APPLICATION_STATUS,
  SN_ITEM_CATEGORY_COMMUNICATIONS,
  SN_ITEM_CATEGORY_SYSTEM_SERVICES,
  SN_ITEM_CATEGORY_HARDWARE
} SnItemCategory;

struct _SnItemClass
{
  GtkButtonClass parent_class;

  const gchar * (* get_id)       (SnItem *item);
  const gchar * (* get_category) (SnItem *item);
};

const gchar    *sn_item_get_id          (SnItem *item);

SnItemCategory  sn_item_get_category    (SnItem *item);

const gchar    *sn_item_get_bus_name    (SnItem *item);
const gchar    *sn_item_get_object_path (SnItem *item);

void            sn_item_emit_ready      (SnItem *item);

G_END_DECLS

#endif

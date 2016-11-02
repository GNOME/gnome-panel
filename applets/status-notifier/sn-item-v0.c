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

#include "config.h"

#include "sn-item-v0.h"

struct _SnItemV0
{
  SnItem parent;
};

G_DEFINE_TYPE (SnItemV0, sn_item_v0, SN_TYPE_ITEM)

static void
sn_item_v0_class_init (SnItemV0Class *v0_class)
{
}

static void
sn_item_v0_init (SnItemV0 *v0)
{
}

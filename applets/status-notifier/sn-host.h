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

#ifndef SN_HOST_H
#define SN_HOST_H

#include "sn-item.h"

G_BEGIN_DECLS

#define SN_TYPE_HOST sn_host_get_type ()
G_DECLARE_INTERFACE (SnHost, sn_host, SN, HOST, GObject)

struct _SnHostInterface
{
  GTypeInterface parent;
};

void sn_host_emit_item_added   (SnHost *host,
                                SnItem *item);

void sn_host_emit_item_removed (SnHost *host,
                                SnItem *item);

G_END_DECLS

#endif

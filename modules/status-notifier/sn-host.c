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

#include "sn-applet.h"
#include "sn-host.h"
#include "sn-item.h"

enum
{
  SIGNAL_ITEM_ADDED,
  SIGNAL_ITEM_REMOVED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_INTERFACE (SnHost, sn_host, G_TYPE_OBJECT)

static void
sn_host_default_init (SnHostInterface *iface)
{
  GParamSpec *spec;

  spec = g_param_spec_object ("applet", "Applet", "Applet",
                              SN_TYPE_APPLET,
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                              G_PARAM_STATIC_STRINGS);

  g_object_interface_install_property (iface, spec);

  signals[SIGNAL_ITEM_ADDED] =
    g_signal_new ("item-added", G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, SN_TYPE_ITEM);

  signals[SIGNAL_ITEM_REMOVED] =
    g_signal_new ("item-removed", G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, SN_TYPE_ITEM);
}

void
sn_host_emit_item_added (SnHost *host,
                         SnItem *item)
{
  g_signal_emit (host, signals[SIGNAL_ITEM_ADDED], 0, item);
}

void sn_host_emit_item_removed (SnHost *host,
                                SnItem *item)
{
  g_signal_emit (host, signals[SIGNAL_ITEM_REMOVED], 0, item);
}

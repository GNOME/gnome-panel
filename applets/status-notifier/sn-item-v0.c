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
#include "sn-item-v0-gen.h"

struct _SnItemV0
{
  SnItem        parent;

  GCancellable *cancellable;
  SnItemV0Gen  *proxy;
};

G_DEFINE_TYPE (SnItemV0, sn_item_v0, SN_TYPE_ITEM)

static void
proxy_ready_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  SnItemV0 *v0;
  SnItemV0Gen *proxy;
  GError *error;

  error = NULL;
  proxy = sn_item_v0_gen_proxy_new_for_bus_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  v0 = SN_ITEM_V0 (user_data);
  v0->proxy = proxy;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }
}

static void
sn_item_v0_constructed (GObject *object)
{
  SnItemV0 *v0;
  SnItem *item;

  v0 = SN_ITEM_V0 (object);
  item = SN_ITEM (v0);

  G_OBJECT_CLASS (sn_item_v0_parent_class)->constructed (object);

  v0->cancellable = g_cancellable_new ();
  sn_item_v0_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                    G_DBUS_PROXY_FLAGS_NONE,
                                    sn_item_get_bus_name (item),
                                    sn_item_get_object_path (item),
                                    v0->cancellable,
                                    proxy_ready_cb, v0);
}

static void
sn_item_v0_dispose (GObject *object)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (object);

  g_cancellable_cancel (v0->cancellable);
  g_clear_object (&v0->cancellable);
  g_clear_object (&v0->proxy);

  G_OBJECT_CLASS (sn_item_v0_parent_class)->dispose (object);
}

static void
sn_item_v0_class_init (SnItemV0Class *v0_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (v0_class);

  object_class->constructed = sn_item_v0_constructed;
  object_class->dispose = sn_item_v0_dispose;
}

static void
sn_item_v0_init (SnItemV0 *v0)
{
}

SnItem *
sn_item_v0_new (const gchar *bus_name,
                const gchar *object_path)
{
  return g_object_new (SN_TYPE_ITEM_V0,
                       "bus-name", bus_name,
                       "object-path", object_path,
                       NULL);
}

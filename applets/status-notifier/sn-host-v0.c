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

#include "sn-host-v0.h"
#include "sn-watcher-v0-gen.h"

#define SN_HOST_BUS_NAME "org.kde.StatusNotifierHost"
#define SN_HOST_OBJECT_PATH "/StatusNotifierHost"
#define SN_ITEM_OBJECT_PATH "/StatusNotifierItem"
#define SN_WATCHER_BUS_NAME "org.kde.StatusNotifierWatcher"
#define SN_WATCHER_OBJECT_PATH "/StatusNotifierWatcher"

struct _SnHostV0
{
  SnHostV0GenSkeleton  parent;

  gchar               *bus_name;
  gchar               *object_path;
  guint                bus_name_id;

  guint                watcher_id;
  SnWatcherV0Gen      *watcher;
};

static void
sn_host_v0_gen_init (SnHostV0GenIface *iface)
{
}

static void
sn_host_init (SnHostInterface *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (SnHostV0, sn_host_v0, SN_TYPE_HOST_V0_GEN_SKELETON,
                         G_IMPLEMENT_INTERFACE (SN_TYPE_HOST_V0_GEN, sn_host_v0_gen_init)
                         G_IMPLEMENT_INTERFACE (SN_TYPE_HOST, sn_host_init))

static void
get_bus_name_and_object_path (const gchar  *service,
                              gchar       **bus_name,
                              gchar       **object_path)
{
  gchar *tmp;

  g_assert (*bus_name == NULL);
  g_assert (*object_path == NULL);

  tmp = g_strstr_len (service, -1, "/");
  if (tmp != NULL)
    {
      gchar **strings;

      strings = g_strsplit (service, "/", 2);

      *bus_name = g_strdup (strings[0]);
      *object_path = g_strdup (tmp);

      g_strfreev (strings);
    }
  else
    {
      *bus_name = g_strdup (service);
      *object_path = g_strdup (SN_ITEM_OBJECT_PATH);
    }
}

static void
item_registered_cb (SnWatcherV0Gen *watcher,
                    const gchar    *service,
                    SnHostV0       *v0)
{
  gchar *bus_name;
  gchar *object_path;

  bus_name = NULL;
  object_path = NULL;

  get_bus_name_and_object_path (service, &bus_name, &object_path);

  g_debug ("item: bus name - %s, object path - %s", bus_name, object_path);

  g_free (bus_name);
  g_free (object_path);
}

static void
item_unregistered_cb (SnWatcherV0Gen *watcher,
                      const gchar    *service,
                      SnHostV0       *v0)
{
}

static void
register_host_cb (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  SnHostV0 *v0;
  GError *error;
  gchar **items;
  gint i;

  v0 = SN_HOST_V0 (user_data);

  error = NULL;
  sn_watcher_v0_gen_call_register_host_finish (v0->watcher, res, &error);

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  g_signal_connect (v0->watcher, "item-registered",
                    G_CALLBACK (item_registered_cb), v0);

  g_signal_connect (v0->watcher, "item-unregistered",
                    G_CALLBACK (item_unregistered_cb), v0);

  items = sn_watcher_v0_gen_dup_registered_items (v0->watcher);

  for (i = 0; items[i] != NULL; i++)
    {
      gchar *bus_name;
      gchar *object_path;

      bus_name = NULL;
      object_path = NULL;

      get_bus_name_and_object_path (items[i], &bus_name, &object_path);

      g_debug ("item: bus name - %s, object path - %s", bus_name, object_path);

      g_free (bus_name);
      g_free (object_path);
    }

  g_strfreev (items);
}

static void
proxy_ready_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  SnHostV0 *v0;
  GError *error;

  v0 = SN_HOST_V0 (user_data);

  error = NULL;
  v0->watcher = sn_watcher_v0_gen_proxy_new_finish (res, &error);

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  sn_watcher_v0_gen_call_register_host (v0->watcher, v0->object_path, NULL,
                                        register_host_cb, v0);
}

static void
name_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  sn_watcher_v0_gen_proxy_new (connection, G_DBUS_PROXY_FLAGS_NONE,
                               SN_WATCHER_BUS_NAME, SN_WATCHER_OBJECT_PATH,
                               NULL, proxy_ready_cb, user_data);
}

static void
name_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  SnHostV0 *v0;

  v0 = SN_HOST_V0 (user_data);

  g_clear_object (&v0->watcher);
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  SnHostV0 *v0;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;

  v0 = SN_HOST_V0 (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (v0);

  error = NULL;
  g_dbus_interface_skeleton_export (skeleton, connection,
                                    v0->object_path, &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  v0->watcher_id = g_bus_watch_name (G_BUS_TYPE_SESSION, SN_WATCHER_BUS_NAME,
                                     G_BUS_NAME_WATCHER_FLAGS_NONE,
                                     name_appeared_cb, name_vanished_cb,
                                     v0, NULL);
}

static void
sn_host_v0_dispose (GObject *object)
{
  SnHostV0 *v0;

  v0 = SN_HOST_V0 (object);

  if (v0->bus_name_id > 0)
    {
      g_bus_unown_name (v0->bus_name_id);
      v0->bus_name_id = 0;
    }

  if (v0->watcher_id > 0)
    {
      g_bus_unwatch_name (v0->watcher_id);
      v0->watcher_id = 0;
    }

  g_clear_object (&v0->watcher);

  G_OBJECT_CLASS (sn_host_v0_parent_class)->dispose (object);
}

static void
sn_host_v0_finalize (GObject *object)
{
  SnHostV0 *v0;

  v0 = SN_HOST_V0 (object);

  g_clear_pointer (&v0->bus_name, g_free);
  g_clear_pointer (&v0->object_path, g_free);

  G_OBJECT_CLASS (sn_host_v0_parent_class)->finalize (object);
}

static void
sn_host_v0_class_init (SnHostV0Class *v0_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (v0_class);

  object_class->dispose = sn_host_v0_dispose;
  object_class->finalize = sn_host_v0_finalize;
}

static void
sn_host_v0_init (SnHostV0 *v0)
{
  GBusNameOwnerFlags flags;
  static guint id;

  flags = G_BUS_NAME_OWNER_FLAGS_NONE;
  id++;

  v0->bus_name = g_strdup_printf ("%s-%d-%d", SN_HOST_BUS_NAME, getpid (), id);
  v0->object_path = g_strdup_printf ("%s/%d", SN_HOST_OBJECT_PATH,id);

  v0->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION, v0->bus_name, flags,
                                    bus_acquired_cb, NULL, NULL, v0, NULL);
}

SnHost *
sn_host_v0_new (void)
{
  return g_object_new (SN_TYPE_HOST_V0, NULL);
}

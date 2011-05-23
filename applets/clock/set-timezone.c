/*
 * Copyright (C) 2011 Codethink Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gio/gio.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "set-timezone.h"

#define CACHE_VALIDITY_SEC 2

#define MECHANISM_BUS_NAME    "org.gnome.SettingsDaemon.DateTimeMechanism"
#define MECHANISM_OBJECT_PATH "/"
#define MECHANISM_INTERFACE   "org.gnome.SettingsDaemon.DateTimeMechanism"

typedef struct {
  gboolean in_progress;
  gint     value;
  guint64  stamp;
} Cache;

static Cache can_set_time_cache;
static Cache can_set_timezone_cache;

static GDBusConnection *
get_system_bus (GError **error)
{
  static GDBusConnection *system;
  static gboolean initialised;
  static GError *saved_error;

  if (!initialised)
    {
      system = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &saved_error);
      initialised = TRUE;
    }

  if (system == NULL && error)
    *error = g_error_copy (saved_error);

  return system;
}

static void
can_set_call_finished (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  Cache *cache = user_data;
  GVariant *reply;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
                                         result, NULL);

  if (reply != NULL)
    {
      g_variant_get (reply, "(i)", &cache->value);
      g_variant_unref (reply);
    }

  cache->stamp = g_get_monotonic_time ();
  cache->in_progress = FALSE;
}

static int
can_set (Cache *cache, const gchar *method_name)
{
  guint64 now = g_get_monotonic_time ();

  if (now - cache->stamp > (CACHE_VALIDITY_SEC * 1000000))
    {
      if (!cache->in_progress)
        {
          GDBusConnection *system_bus = get_system_bus (NULL);

          if (system_bus != NULL)
            g_dbus_connection_call (system_bus, MECHANISM_BUS_NAME,
                                    MECHANISM_OBJECT_PATH, MECHANISM_INTERFACE,
                                    method_name, NULL, G_VARIANT_TYPE ("(i)"),
                                    G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                                    can_set_call_finished, cache);

          /* Even if the system bus was NULL, we want to set this in
           * order to effectively wedge ourselves from ever trying
           * again.
           */
          cache->in_progress = TRUE;
        }
    }

  return cache->value;
}

gint
can_set_system_timezone (void)
{
  return can_set (&can_set_timezone_cache, "CanSetTimezone");
}

gint
can_set_system_time (void)
{
  return can_set (&can_set_time_cache, "CanSetTime");
}

gboolean
set_system_timezone_finish (GAsyncResult  *result,
                            GError       **error)
{
  GDBusConnection *system_bus = get_system_bus (NULL);
  GVariant *reply;

  /* detect if we set an error due to being unable to get the system bus */
  if (g_simple_async_result_is_valid (result, NULL, set_system_timezone_async))
    {
      g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                             error);
      return FALSE;
    }

  g_assert (system_bus != NULL);

  reply = g_dbus_connection_call_finish (system_bus, result, error);

  if (reply != NULL)
    g_variant_unref (reply);

  return reply != NULL;
}

void
set_system_timezone_async (const gchar         *tz,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GDBusConnection *system_bus;
  GError *error = NULL;

  system_bus = get_system_bus (&error);

  if (system_bus == NULL)
    {
      GSimpleAsyncResult *simple;

      simple = g_simple_async_result_new (NULL, callback, user_data,
                                          set_system_timezone_async);
      g_simple_async_result_set_from_error (simple, error);
      g_simple_async_result_complete_in_idle (simple);
      g_object_unref (simple);
      g_error_free (error);
    }

  g_dbus_connection_call (system_bus, MECHANISM_BUS_NAME,
                          MECHANISM_OBJECT_PATH, MECHANISM_INTERFACE,
                          "SetTimezone", g_variant_new ("(s)", tz),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                          callback, user_data);
}

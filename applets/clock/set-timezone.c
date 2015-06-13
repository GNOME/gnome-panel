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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gio/gio.h>
#include <polkit/polkit.h>

#include "set-timezone.h"

#define CACHE_VALIDITY_SEC 20

#define MECHANISM_BUS_NAME    "org.freedesktop.timedate1"
#define MECHANISM_OBJECT_PATH "/org/freedesktop/timedate1"
#define MECHANISM_INTERFACE   "org.freedesktop.timedate1"

typedef struct {
  gint     value;
  guint64  stamp;
} Cache;

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

static int
can_set (Cache *cache, const gchar *method_name)
{
  guint64 now = g_get_monotonic_time ();

  if (now - cache->stamp > (CACHE_VALIDITY_SEC * 1000000))
    {
      PolkitAuthority *authority;
      PolkitSubject   *subject;
      PolkitAuthorizationResult *res;

      authority = polkit_authority_get_sync (NULL, NULL);
      subject = polkit_unix_session_new_for_process_sync (getpid (), NULL, NULL);

      res = polkit_authority_check_authorization_sync (authority,
                                                       subject,
                                                       "org.freedesktop.timedate1.set-timezone",
                                                       NULL,
                                                       POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE,
                                                       NULL,
                                                       NULL);

	cache->stamp = g_get_monotonic_time ();

        if (res == NULL)
          cache->value = 0;
        else
          {
            if (polkit_authorization_result_get_is_authorized (res))
              cache->value = 2;
            else if (polkit_authorization_result_get_is_challenge (res))
              cache->value = 1;
            else
              cache->value = 0;

            g_object_unref (res);
          }

        g_object_unref (authority);
        g_object_unref (subject);
    }

  return cache->value;
}

gint
can_set_system_timezone (void)
{
  return can_set (&can_set_timezone_cache, "CanSetTimezone");
}

gboolean
set_system_timezone_finish (GAsyncResult  *result,
                            GError       **error)
{
  GDBusConnection *system_bus = get_system_bus (NULL);
  GVariant *reply;

  if (g_task_is_valid (result, NULL))
    return g_task_propagate_boolean (G_TASK (result), error);

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
      GTask *task;

      task = g_task_new (NULL, NULL, callback, user_data);

      g_task_return_error (task, error);
      g_object_unref (task);
    }

  g_dbus_connection_call (system_bus, MECHANISM_BUS_NAME,
                          MECHANISM_OBJECT_PATH, MECHANISM_INTERFACE,
                          "SetTimezone", g_variant_new ("(sb)", tz, TRUE),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                          callback, user_data);
}

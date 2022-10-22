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

#include "set-timezone.h"

#define MECHANISM_BUS_NAME    "org.freedesktop.timedate1"
#define MECHANISM_OBJECT_PATH "/org/freedesktop/timedate1"
#define MECHANISM_INTERFACE   "org.freedesktop.timedate1"

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

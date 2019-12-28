/*
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "gp-session.h"

#include <gio/gio.h>

struct _GpSession
{
  GObject       parent;

  gboolean      replace;
  char         *startup_id;

  guint         name_id;
  gboolean      name_acquired;

  GCancellable *cancellable;

  GDBusProxy   *session_manager;

  char         *client_id;
  GDBusProxy   *client_private;
};

enum
{
  PROP_0,

  PROP_REPLACE,
  PROP_STARTUP_ID,

  LAST_PROP
};

static GParamSpec *session_properties[LAST_PROP] = { NULL };

enum
{
  NAME_LOST,

  SESSION_READY,
  END_SESSION,

  LAST_SIGNAL
};

static guint session_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpSession, gp_session, G_TYPE_OBJECT)

static void
respond_to_end_session (GpSession *self)
{
  g_dbus_proxy_call (self->client_private,
                     "EndSessionResponse",
                     g_variant_new ("(bs)", TRUE, ""),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     NULL,
                     NULL);
}

static void
g_signal_cb (GDBusProxy *proxy,
             char       *sender_name,
             char       *signal_name,
             GVariant   *parameters,
             GpSession  *self)
{
  if (g_strcmp0 (signal_name, "QueryEndSession") == 0)
    respond_to_end_session (self);
  else if (g_strcmp0 (signal_name, "EndSession") == 0)
    respond_to_end_session (self);
  else if (g_strcmp0 (signal_name, "Stop") == 0)
    g_signal_emit (self, session_signals[END_SESSION], 0);
}

static void
client_private_ready_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GError *error;
  GDBusProxy *proxy;
  GpSession *self;

  error = NULL;
  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to get a client proxy: %s", error->message);

      g_error_free (error);
      return;
    }

  self = GP_SESSION (user_data);
  self->client_private = proxy;

  g_signal_connect (self->client_private,
                    "g-signal",
                    G_CALLBACK (g_signal_cb),
                    self);
}

static void
register_client_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  GError *error;
  GVariant *variant;
  GpSession *self;

  error = NULL;
  variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to register client: %s", error->message);

      g_error_free (error);
      return;
    }

  self = GP_SESSION (user_data);

  g_variant_get (variant, "(o)", &self->client_id);
  g_variant_unref (variant);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                            NULL,
                            "org.gnome.SessionManager",
                            self->client_id,
                            "org.gnome.SessionManager.ClientPrivate",
                            self->cancellable,
                            client_private_ready_cb,
                            self);
}

static void
session_manager_ready_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GError *error;
  GDBusProxy *proxy;
  GpSession *self;

  error = NULL;
  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to get session manager proxy: %s", error->message);

      g_error_free (error);
      return;
    }

  self = GP_SESSION (user_data);
  self->session_manager = proxy;

  g_signal_emit (self, session_signals[SESSION_READY], 0);
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  GpSession *self;
  GDBusProxyFlags flags;

  self = GP_SESSION (user_data);
  self->name_acquired = TRUE;

  flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
          G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS;

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            flags,
                            NULL,
                            "org.gnome.SessionManager",
                            "/org/gnome/SessionManager",
                            "org.gnome.SessionManager",
                            self->cancellable,
                            session_manager_ready_cb,
                            self);
}

static void
name_lost_cb (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
  GpSession *self;

  self = GP_SESSION (user_data);

  g_signal_emit (self, session_signals[NAME_LOST], 0, self->name_acquired);
}

static void
gp_session_constructed (GObject *object)
{
  GpSession *self;
  GBusNameOwnerFlags flags;

  self = GP_SESSION (object);

  G_OBJECT_CLASS (gp_session_parent_class)->constructed (object);

  flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
  if (self->replace)
    flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

  self->name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  "org.gnome.Panel",
                                  flags,
                                  NULL,
                                  name_acquired_cb,
                                  name_lost_cb,
                                  self,
                                  NULL);
}

static void
gp_session_dispose (GObject *object)
{
  GpSession *self;

  self = GP_SESSION (object);

  if (self->name_id != 0)
    {
      g_bus_unown_name (self->name_id);
      self->name_id = 0;
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->session_manager);
  g_clear_object (&self->client_private);

  G_OBJECT_CLASS (gp_session_parent_class)->dispose (object);
}

static void
gp_session_finalize (GObject *object)
{
  GpSession *self;

  self = GP_SESSION (object);

  g_clear_pointer (&self->startup_id, g_free);
  g_clear_pointer (&self->client_id, g_free);

  G_OBJECT_CLASS (gp_session_parent_class)->finalize (object);
}

static void
gp_session_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GpSession *self;

  self = GP_SESSION (object);

  switch (property_id)
    {
      case PROP_REPLACE:
        self->replace = g_value_get_boolean (value);
        break;

      case PROP_STARTUP_ID:
        self->startup_id = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  session_properties[PROP_REPLACE] =
    g_param_spec_boolean ("replace",
                          "replace",
                          "replace",
                          FALSE,
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS);

  session_properties[PROP_STARTUP_ID] =
    g_param_spec_string ("startup-id",
                         "startup-id",
                         "startup-id",
                         "",
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     session_properties);
}

static void
install_signals (void)
{
  session_signals[NAME_LOST] =
    g_signal_new ("name-lost",
                  GP_TYPE_SESSION,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  session_signals[SESSION_READY] =
    g_signal_new ("session-ready",
                  GP_TYPE_SESSION,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  session_signals[END_SESSION] =
    g_signal_new ("end-session",
                  GP_TYPE_SESSION,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
gp_session_class_init (GpSessionClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = gp_session_constructed;
  object_class->dispose = gp_session_dispose;
  object_class->finalize = gp_session_finalize;
  object_class->set_property = gp_session_set_property;

  install_properties (object_class);
  install_signals ();
}

static void
gp_session_init (GpSession *self)
{
  self->cancellable = g_cancellable_new ();
}

GpSession *
gp_session_new (gboolean    replace,
                const char *startup_id)
{
  return g_object_new (GP_TYPE_SESSION,
                       "replace", replace,
                       "startup-id", startup_id,
                       NULL);
}

void
gp_session_register (GpSession *self)
{
  g_dbus_proxy_call (self->session_manager,
                     "RegisterClient",
                     g_variant_new ("(ss)", "gnome-panel", self->startup_id),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     register_client_cb,
                     self);
}

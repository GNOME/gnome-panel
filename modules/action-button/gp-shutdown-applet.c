/*
 * Copyright (C) 2020 Alberts Muktupāvels
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
#include "gp-shutdown-applet.h"

#include <glib/gi18n-lib.h>

#include "gpab-session-manager-gen.h"

struct _GpShutdownApplet
{
  GpActionButtonApplet   parent;

  GpabSessionManagerGen *session_manager;

  gboolean               can_shutdown;
};

G_DEFINE_TYPE (GpShutdownApplet, gp_shutdown_applet, GP_TYPE_ACTION_BUTTON_APPLET)

static void
lockdown_changed (GpShutdownApplet *self)
{
  GpLockdownFlags lockdowns;
  gboolean applet_sensitive;

  lockdowns = gp_applet_get_lockdowns (GP_APPLET (self));

  applet_sensitive = TRUE;

  if ((lockdowns & GP_LOCKDOWN_FLAGS_APPLET) == GP_LOCKDOWN_FLAGS_APPLET ||
      (lockdowns & GP_LOCKDOWN_FLAGS_LOG_OUT) == GP_LOCKDOWN_FLAGS_LOG_OUT ||
      !self->can_shutdown)
    applet_sensitive = FALSE;

  gtk_widget_set_sensitive (GTK_WIDGET (self), applet_sensitive);
}

static void
lockdowns_cb (GpApplet         *applet,
              GParamSpec       *pspec,
              GpShutdownApplet *self)
{
  lockdown_changed (self);
}

static void
setup_applet (GpShutdownApplet *self)
{
  const char *text;
  AtkObject *atk;

  gp_action_button_applet_set_icon_name (GP_ACTION_BUTTON_APPLET (self),
                                         "system-shutdown");

  text = _("Power off the computer");

  atk = gtk_widget_get_accessible (GTK_WIDGET (self));
  atk_object_set_name (atk, text);
  atk_object_set_description (atk, text);

  gtk_widget_set_tooltip_text (GTK_WIDGET (self), text);

  g_object_bind_property (self,
                          "enable-tooltips",
                          self,
                          "has-tooltip",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  lockdown_changed (self);
}

static void
can_shutdown_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  gboolean can_shutdown;
  GError *error;
  GpShutdownApplet *self;

  can_shutdown = FALSE;
  error = NULL;

  gpab_session_manager_gen_call_can_shutdown_finish (GPAB_SESSION_MANAGER_GEN (source),
                                                     &can_shutdown,
                                                     res,
                                                     &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  self = GP_SHUTDOWN_APPLET (user_data);
  self->can_shutdown = can_shutdown;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  lockdown_changed (self);
}

static void
session_manager_ready_cb (GObject      *source,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GError *error;
  GpabSessionManagerGen *session_manager;
  GpShutdownApplet *self;

  error = NULL;
  session_manager = gpab_session_manager_gen_proxy_new_for_bus_finish (res,
                                                                       &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  self = GP_SHUTDOWN_APPLET (user_data);
  self->session_manager = session_manager;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  gpab_session_manager_gen_call_can_shutdown (self->session_manager,
                                              NULL,
                                              can_shutdown_cb,
                                              self);
}

static void
gp_shutdown_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_shutdown_applet_parent_class)->constructed (object);
  setup_applet (GP_SHUTDOWN_APPLET (object));
}

static void
gp_shutdown_applet_dispose (GObject *object)
{
  GpShutdownApplet *self;

  self = GP_SHUTDOWN_APPLET (object);

  g_clear_object (&self->session_manager);

  G_OBJECT_CLASS (gp_shutdown_applet_parent_class)->dispose (object);
}

static void
shutdown_cb (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  GError *error;

  error = NULL;
  gpab_session_manager_gen_call_shutdown_finish (GPAB_SESSION_MANAGER_GEN (source),
                                                 res,
                                                 &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (error)
    {
      g_warning ("Could not ask session manager to shut down: %s",
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
gp_shutdown_applet_clicked (GpActionButtonApplet *applet)
{
  GpShutdownApplet *self;

  self = GP_SHUTDOWN_APPLET (applet);

  if (!self->session_manager)
    {
      g_warning ("Session manager service not available.");
      return;
    }

  gpab_session_manager_gen_call_shutdown (self->session_manager,
                                          NULL,
                                          shutdown_cb,
                                          self);
}

static void
gp_shutdown_applet_class_init (GpShutdownAppletClass *self_class)
{
  GObjectClass *object_class;
  GpActionButtonAppletClass *action_button_applet_class;

  object_class = G_OBJECT_CLASS (self_class);
  action_button_applet_class = GP_ACTION_BUTTON_APPLET_CLASS (self_class);

  object_class->constructed = gp_shutdown_applet_constructed;
  object_class->dispose = gp_shutdown_applet_dispose;

  action_button_applet_class->clicked = gp_shutdown_applet_clicked;
}

static void
gp_shutdown_applet_init (GpShutdownApplet *self)
{
  gpab_session_manager_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              "org.gnome.SessionManager",
                                              "/org/gnome/SessionManager",
                                              NULL,
                                              session_manager_ready_cb,
                                              self);

  g_signal_connect (self,
                    "notify::lockdowns",
                    G_CALLBACK (lockdowns_cb),
                    self);
}

gboolean
gp_shutdown_applet_is_disabled (GpLockdownFlags   flags,
                                char            **reason)
{
  if ((flags & GP_LOCKDOWN_FLAGS_LOG_OUT) != GP_LOCKDOWN_FLAGS_LOG_OUT)
    return FALSE;

  *reason = g_strdup (_("Disabled because “disable-log-out” setting in "
                        "“org.gnome.desktop.lockdown” GSettings schema is "
                        "set to true."));

  return TRUE;
}

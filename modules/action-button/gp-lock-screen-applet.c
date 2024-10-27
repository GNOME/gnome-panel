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
#include "gp-lock-screen-applet.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n-lib.h>
#include <systemd/sd-journal.h>

#include "gpab-screensaver-gen.h"

struct _GpLockScreenApplet
{
  GpActionButtonApplet  parent;

  GDesktopAppInfo      *lock_panel;

  GpabScreensaverGen   *screensaver;
};

G_DEFINE_TYPE (GpLockScreenApplet,
               gp_lock_screen_applet,
               GP_TYPE_ACTION_BUTTON_APPLET)

static void
error_response_cb (GtkWidget *widget,
                   int        response_id,
                   gpointer   user_data)
{
  gtk_widget_destroy (widget);
}

static void
show_error_message (GtkWindow  *parent,
                    const char *primary_text,
                    const char *secondary_text)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s",
                                   primary_text);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%s",
                                            secondary_text);

  g_signal_connect (dialog, "response", G_CALLBACK (error_response_cb), NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
lockdown_changed (GpLockScreenApplet *self)
{
  GpLockdownFlags lockdowns;
  gboolean applet_sensitive;
  gboolean properties_enabled;
  GAction *action;

  lockdowns = gp_applet_get_lockdowns (GP_APPLET (self));

  applet_sensitive = TRUE;

  if ((lockdowns & GP_LOCKDOWN_FLAGS_APPLET) == GP_LOCKDOWN_FLAGS_APPLET ||
      (lockdowns & GP_LOCKDOWN_FLAGS_LOCK_SCREEN) == GP_LOCKDOWN_FLAGS_LOCK_SCREEN)
    applet_sensitive = FALSE;

  gtk_widget_set_sensitive (GTK_WIDGET (self), applet_sensitive);

  action = gp_applet_menu_lookup_action (GP_APPLET (self), "lock-screen");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), applet_sensitive);

  properties_enabled = (lockdowns & GP_LOCKDOWN_FLAGS_LOCKED_DOWN) != GP_LOCKDOWN_FLAGS_LOCKED_DOWN &&
                       (lockdowns & GP_LOCKDOWN_FLAGS_LOCK_SCREEN) != GP_LOCKDOWN_FLAGS_LOCK_SCREEN &&
                       self->lock_panel != NULL;

  action = gp_applet_menu_lookup_action (GP_APPLET (self), "properties");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), properties_enabled);
}

static void
lockdowns_cb (GpApplet           *applet,
              GParamSpec         *pspec,
              GpLockScreenApplet *self)
{
  lockdown_changed (self);
}

static void
lock_cb (GObject      *source,
         GAsyncResult *res,
         gpointer      user_data)
{
  GError *error;

  error = NULL;
  gpab_screensaver_gen_call_lock_finish (GPAB_SCREENSAVER_GEN (source),
                                         res,
                                         &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (error)
    {
      g_warning ("Could not ask screensaver to lock: %s",
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
lock_screen (GpLockScreenApplet *self)
{
  if (!self->screensaver)
    {
      g_warning ("Screensaver service not available.");
      return;
    }

  gpab_screensaver_gen_call_lock (self->screensaver,
                                  NULL,
                                  lock_cb,
                                  self);
}

static void
lock_screen_cb (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  lock_screen (GP_LOCK_SCREEN_APPLET (user_data));
}

static void
set_active_cb (GObject      *source,
               GAsyncResult *res,
               gpointer      user_data)
{
  GError *error;

  error = NULL;
  gpab_screensaver_gen_call_set_active_finish (GPAB_SCREENSAVER_GEN (source),
                                               res,
                                               &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (error)
    {
      g_warning ("Could not ask screensaver to activate: %s",
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
screensaver_cb (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GpLockScreenApplet *self;

  self = GP_LOCK_SCREEN_APPLET (user_data);

  if (!self->screensaver)
    {
      g_warning ("Screensaver service not available.");
      return;
    }

  gpab_screensaver_gen_call_set_active (self->screensaver,
                                        TRUE,
                                        NULL,
                                        set_active_cb,
                                        self);
}

static void
child_setup (gpointer user_data)
{
  GAppInfo *info;
  const gchar *id;
  gint stdout_fd;
  gint stderr_fd;

  info = G_APP_INFO (user_data);
  id = g_app_info_get_id (info);

  stdout_fd = sd_journal_stream_fd (id, LOG_INFO, FALSE);
  if (stdout_fd >= 0)
    {
      dup2 (stdout_fd, STDOUT_FILENO);
      close (stdout_fd);
    }

  stderr_fd = sd_journal_stream_fd (id, LOG_WARNING, FALSE);
  if (stderr_fd >= 0)
    {
      dup2 (stderr_fd, STDERR_FILENO);
      close (stderr_fd);
    }
}

static void
close_pid (GPid     pid,
           gint     status,
           gpointer user_data)
{
  g_spawn_close_pid (pid);
}

static void
pid_cb (GDesktopAppInfo *info,
        GPid             pid,
        gpointer         user_data)
{
  g_child_watch_add (pid, close_pid, NULL);
}

static void
properties_cb (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GpLockScreenApplet *self;
  GdkDisplay *display;
  GdkAppLaunchContext *context;
  GSpawnFlags flags;
  GError *error;

  self = GP_LOCK_SCREEN_APPLET (user_data);

  g_assert (self->lock_panel != NULL);

  display = gdk_display_get_default ();
  context = gdk_display_get_app_launch_context (display);
  flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;

  error = NULL;
  g_desktop_app_info_launch_uris_as_manager (self->lock_panel,
                                             NULL,
                                             G_APP_LAUNCH_CONTEXT (context),
                                             flags,
                                             child_setup,
                                             self->lock_panel,
                                             pid_cb,
                                             NULL,
                                             &error);

  g_object_unref (context);

  if (error != NULL)
    {
      show_error_message (NULL,
                          _("Could not launch application"),
                          error->message);

      g_error_free (error);
    }
}

static const GActionEntry lock_screen_menu_actions[] =
{
  { "lock-screen", lock_screen_cb, NULL, NULL, NULL },
  { "screensaver", screensaver_cb, NULL, NULL, NULL },
  { "properties", properties_cb, NULL, NULL, NULL },
  { NULL }
};

static void
setup_menu (GpLockScreenApplet *self)
{
  GpApplet *applet;
  const char *resource;

  applet = GP_APPLET (self);

  resource = "/org/gnome/gnome-panel/modules/action-button/gp-lock-screen-menu.ui";
  gp_applet_setup_menu_from_resource (applet, resource, lock_screen_menu_actions);

  lockdown_changed (self);
}

static void
setup_applet (GpLockScreenApplet *self)
{
  const char *text;
  AtkObject *atk;

  setup_menu (self);

  gp_action_button_applet_set_icon_name (GP_ACTION_BUTTON_APPLET (self),
                                         "system-lock-screen");

  text = _("Protect your computer from unauthorized use");

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
screensaver_proxy_ready_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  GError *error;
  GpabScreensaverGen *screensaver;
  GpLockScreenApplet *self;

  error = NULL;
  screensaver = gpab_screensaver_gen_proxy_new_for_bus_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  self = GP_LOCK_SCREEN_APPLET (user_data);
  self->screensaver = screensaver;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }
}

static void
gp_lock_screen_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_lock_screen_applet_parent_class)->constructed (object);
  setup_applet (GP_LOCK_SCREEN_APPLET (object));
}

static void
gp_lock_screen_applet_dispose (GObject *object)
{
  GpLockScreenApplet *self;

  self = GP_LOCK_SCREEN_APPLET (object);

  g_clear_object (&self->lock_panel);
  g_clear_object (&self->screensaver);

  G_OBJECT_CLASS (gp_lock_screen_applet_parent_class)->dispose (object);
}

static void
gp_lock_screen_applet_clicked (GpActionButtonApplet *applet)
{
  lock_screen (GP_LOCK_SCREEN_APPLET (applet));
}

static void
gp_lock_screen_applet_class_init (GpLockScreenAppletClass *self_class)
{
  GObjectClass *object_class;
  GpActionButtonAppletClass *action_button_applet_class;

  object_class = G_OBJECT_CLASS (self_class);
  action_button_applet_class = GP_ACTION_BUTTON_APPLET_CLASS (self_class);

  object_class->constructed = gp_lock_screen_applet_constructed;
  object_class->dispose = gp_lock_screen_applet_dispose;

  action_button_applet_class->clicked = gp_lock_screen_applet_clicked;
}

static void
gp_lock_screen_applet_init (GpLockScreenApplet *self)
{
  self->lock_panel = g_desktop_app_info_new ("gnome-lock-panel.desktop");

  gpab_screensaver_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          "org.gnome.ScreenSaver",
                                          "/org/gnome/ScreenSaver",
                                          NULL,
                                          screensaver_proxy_ready_cb,
                                          self);

  g_signal_connect (self,
                    "notify::lockdowns",
                    G_CALLBACK (lockdowns_cb),
                    self);
}

gboolean
gp_lock_screen_applet_is_disabled (GpLockdownFlags   flags,
                                   char            **reason)
{
  if ((flags & GP_LOCKDOWN_FLAGS_LOCK_SCREEN) != GP_LOCKDOWN_FLAGS_LOCK_SCREEN)
    return FALSE;

  *reason = g_strdup (_("Disabled because “disable-lock-screen” setting in "
                        "“org.gnome.desktop.lockdown” GSettings schema is "
                        "set to true."));

  return TRUE;
}

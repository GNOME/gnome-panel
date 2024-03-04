/*
 * Copyright (C) 2018 Alberts Muktupāvels
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
#include "gp-lock-logout.h"

#include "gp-dm-seat-gen.h"
#include "gp-login1-manager-gen.h"
#include "gp-menu-utils.h"
#include "gp-screensaver-gen.h"
#include "gp-session-manager-gen.h"

#include <gdm/gdm-user-switching.h>
#include <glib/gi18n-lib.h>
#include <libgnome-panel/gp-image-menu-item.h>

struct _GpLockLogout
{
  GObject              parent;

  gboolean             enable_tooltips;
  gboolean             locked_down;
  guint                menu_icon_size;

  GSettings           *lockdown;

  GpLogin1ManagerGen  *login1_manager;

  GpSessionManagerGen *session_manager;

  GpScreensaverGen    *screensaver;

  GpDmSeatGen         *seat;
};

enum
{
  PROP_0,

  PROP_ENABLE_TOOLTIPS,
  PROP_LOCKED_DOWN,
  PROP_MENU_ICON_SIZE,

  LAST_PROP
};

static GParamSpec *lock_logout_properties[LAST_PROP] = { NULL };

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint lock_logout_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpLockLogout, gp_lock_logout, G_TYPE_OBJECT)

static void
lockdown_changed_cb (GSettings    *settings,
                     const gchar  *key,
                     GpLockLogout *lock_logout)
{
  g_signal_emit (lock_logout, lock_logout_signals[CHANGED], 0);
}

static void
login1_manager_proxy_ready_cb (GObject      *source_object,
                               GAsyncResult *res,
                               gpointer      user_data)
{
  GError *error;
  GpLogin1ManagerGen *manager;
  GpLockLogout *lock_logout;

  error = NULL;
  manager = gp_login1_manager_gen_proxy_new_for_bus_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  lock_logout = GP_LOCK_LOGOUT (user_data);
  lock_logout->login1_manager = manager;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }
}

static void
session_manager_proxy_ready_cb (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
  GError *error;
  GpSessionManagerGen *manager;
  GpLockLogout *lock_logout;

  error = NULL;
  manager = gp_session_manager_gen_proxy_new_for_bus_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  lock_logout = GP_LOCK_LOGOUT (user_data);
  lock_logout->session_manager = manager;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }
}

static void
screensaver_proxy_ready_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  GError *error;
  GpScreensaverGen *screensaver;
  GpLockLogout *lock_logout;

  error = NULL;
  screensaver = gp_screensaver_gen_proxy_new_for_bus_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  lock_logout = GP_LOCK_LOGOUT (user_data);
  lock_logout->screensaver = screensaver;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }
}

static void
seat_ready_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GError *error;
  GpDmSeatGen *seat;
  GpLockLogout *lock_logout;

  error = NULL;
  seat = gp_dm_seat_gen_proxy_new_for_bus_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  lock_logout = GP_LOCK_LOGOUT (user_data);
  lock_logout->seat = seat;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }
}

static void
switch_to_greeter_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GError *error;

  error = NULL;
  gp_dm_seat_gen_call_switch_to_greeter_finish (GP_DM_SEAT_GEN (source_object),
                                                res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (error)
    {
      g_warning ("Failed to switch to greeter: %s", error->message);
      g_error_free (error);

      return;
    }
}

static void
switch_user_activate_cb (GtkWidget    *item,
                         GpLockLogout *lock_logout)
{
  if (lock_logout->seat != NULL)
    {
      gp_dm_seat_gen_call_switch_to_greeter (lock_logout->seat,
                                             NULL, switch_to_greeter_cb,
                                             lock_logout);
    }
  else
    {
      GError *error;

      error = NULL;
      if (!gdm_goto_login_session_sync (NULL, &error))
        g_warning ("Failed to switch to greeter: %s", error->message);

      g_clear_error (&error);
    }
}

static void
logout_cb (GObject      *source_object,
           GAsyncResult *res,
           gpointer      user_data)
{
  GError *error;

  error = NULL;
  gp_session_manager_gen_call_logout_finish (GP_SESSION_MANAGER_GEN (source_object),
                                             res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (error)
    {
      g_warning ("Could not ask session manager to logout: %s",
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
logout_activate_cb (GtkWidget    *item,
                    GpLockLogout *lock_logout)
{
  gp_session_manager_gen_call_logout (lock_logout->session_manager,
                                      0, NULL, logout_cb,
                                      lock_logout);
}

static void
lock_cb (GObject      *source_object,
         GAsyncResult *res,
         gpointer      user_data)
{
  GError *error;

  error = NULL;
  gp_screensaver_gen_call_lock_finish (GP_SCREENSAVER_GEN (source_object),
                                       res, &error);

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
lock_screen_activate_cb (GtkWidget    *item,
                         GpLockLogout *lock_logout)
{
  if (!lock_logout->screensaver)
    {
      g_warning ("Screensaver service not available.");
      return;
    }

  gp_screensaver_gen_call_lock (lock_logout->screensaver,
                                NULL, lock_cb,
                                lock_logout);
}

static void
hibernate_cb (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
  GError *error;

  error = NULL;
  gp_login1_manager_gen_call_hibernate_finish (GP_LOGIN1_MANAGER_GEN (source_object),
                                               res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (error)
    {
      g_warning ("Could not ask login1 manager to hibernate: %s",
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
hibernate_activate_cb (GtkWidget    *item,
                       GpLockLogout *lock_logout)
{
  gp_login1_manager_gen_call_hibernate (lock_logout->login1_manager,
                                        TRUE, NULL, hibernate_cb,
                                        lock_logout);
}

static void
suspend_cb (GObject      *source_object,
            GAsyncResult *res,
            gpointer      user_data)
{
  GError *error;

  error = NULL;
  gp_login1_manager_gen_call_suspend_finish (GP_LOGIN1_MANAGER_GEN (source_object),
                                             res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (error)
    {
      g_warning ("Could not ask login1 manager to suspend: %s",
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
suspend_activate_cb (GtkWidget    *item,
                     GpLockLogout *lock_logout)
{
  gp_login1_manager_gen_call_suspend (lock_logout->login1_manager,
                                      TRUE, NULL, suspend_cb,
                                      lock_logout);
}

static void
hybrid_sleep_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error;

  error = NULL;
  gp_login1_manager_gen_call_hybrid_sleep_finish (GP_LOGIN1_MANAGER_GEN (source_object),
                                                  res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (error)
    {
      g_warning ("Could not ask login1 manager to hybrid sleep: %s",
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
hybrid_sleep_activate_cb (GtkWidget    *item,
                          GpLockLogout *lock_logout)
{
  gp_login1_manager_gen_call_hybrid_sleep (lock_logout->login1_manager,
                                           TRUE, NULL, hybrid_sleep_cb,
                                           lock_logout);
}

static void
reboot_cb (GObject      *source_object,
           GAsyncResult *res,
           gpointer      user_data)
{
  GError *error;

  error = NULL;
  gp_session_manager_gen_call_reboot_finish (GP_SESSION_MANAGER_GEN (source_object),
                                             res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (error)
    {
      g_warning ("Could not ask session manager to reboot: %s",
                 error->message);

      g_error_free (error);
      return;
    }
}

static void
reboot_activate_cb (GtkWidget    *item,
                    GpLockLogout *lock_logout)
{
  gp_session_manager_gen_call_reboot (lock_logout->session_manager,
                                      NULL, reboot_cb, lock_logout);
}

static void
shutdown_cb (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  GError *error;

  error = NULL;
  gp_session_manager_gen_call_shutdown_finish (GP_SESSION_MANAGER_GEN (source_object),
                                               res, &error);

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
shutdown_activate_cb (GtkWidget    *item,
                      GpLockLogout *lock_logout)
{
  gp_session_manager_gen_call_shutdown (lock_logout->session_manager,
                                        NULL, shutdown_cb, lock_logout);
}

static gboolean
get_can_switch_user (GpLockLogout *lock_logout)
{
  if (lock_logout->seat != NULL &&
      !gp_dm_seat_gen_get_can_switch (lock_logout->seat))
    return FALSE;

  return TRUE;
}

static gboolean
get_can_hibernate (GpLockLogout *lock_logout)
{
  GError *error;
  gchar *result;
  gboolean can_hibernate;

  if (!lock_logout->login1_manager)
    {
      g_warning ("Login1 manager service not available.");
      return FALSE;
    }

  error = NULL;
  result = NULL;

  gp_login1_manager_gen_call_can_hibernate_sync (lock_logout->login1_manager,
                                                 &result, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Could not ask login1 manager if hibernate is available: %s",
                 error->message);

      g_error_free (error);
      return FALSE;
    }

  can_hibernate = FALSE;
  if (g_strcmp0 (result, "yes") == 0 || g_strcmp0 (result, "challenge") == 0)
    can_hibernate = TRUE;

  g_free (result);

  return can_hibernate;
}

static gboolean
get_can_suspend (GpLockLogout *lock_logout)
{
  GError *error;
  gchar *result;
  gboolean can_suspend;

  if (!lock_logout->login1_manager)
    {
      g_warning ("Login1 manager service not available.");
      return FALSE;
    }

  error = NULL;
  result = NULL;

  gp_login1_manager_gen_call_can_suspend_sync (lock_logout->login1_manager,
                                               &result, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Could not ask login1 manager if suspend is available: %s",
                 error->message);

      g_error_free (error);
      return FALSE;
    }

  can_suspend = FALSE;
  if (g_strcmp0 (result, "yes") == 0 || g_strcmp0 (result, "challenge") == 0)
    can_suspend = TRUE;

  g_free (result);

  return can_suspend;
}

static gboolean
get_can_hybrid_sleep (GpLockLogout *lock_logout)
{
  GError *error;
  gchar *result;
  gboolean can_hybrid_sleep;

  if (!lock_logout->login1_manager)
    {
      g_warning ("Login1 manager service not available.");
      return FALSE;
    }

  error = NULL;
  result = NULL;

  gp_login1_manager_gen_call_can_hybrid_sleep_sync (lock_logout->login1_manager,
                                                    &result, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Could not ask login1 manager if hybrid sleep is available: %s",
                 error->message);

      g_error_free (error);
      return FALSE;
    }

  can_hybrid_sleep = FALSE;
  if (g_strcmp0 (result, "yes") == 0 || g_strcmp0 (result, "challenge") == 0)
    can_hybrid_sleep = TRUE;

  g_free (result);

  return can_hybrid_sleep;
}

static gboolean
get_can_shutdown (GpLockLogout *lock_logout)
{
  GError *error;
  gboolean can_shutdown;

  if (!lock_logout->session_manager)
    {
      g_warning ("Session manager service not available.");
      return FALSE;
    }

  error = NULL;
  can_shutdown = FALSE;

  gp_session_manager_gen_call_can_shutdown_sync (lock_logout->session_manager,
                                                 &can_shutdown, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Could not ask session manager if shut down is available: %s",
                 error->message);

      g_error_free (error);
    }

  return can_shutdown;
}

static void
drag_data_get_cb (GtkWidget        *widget,
                  GdkDragContext   *context,
                  GtkSelectionData *selection_data,
                  guint             info,
                  guint             time,
                  const gchar      *drag_id)
{
  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target (selection_data),
                          8, (guchar *) drag_id, strlen (drag_id));
}

static void
free_drag_id (gchar    *drag_id,
              GClosure *closure)
{
  g_free (drag_id);
}

static GtkWidget *
create_menu_item (GpLockLogout *lock_logout,
                  const gchar  *icon_name,
                  const gchar  *label,
                  const gchar  *tooltip)
{
  GtkWidget *image;
  GtkWidget *item;

  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (image), lock_logout->menu_icon_size);

  item = gp_image_menu_item_new_with_label (label);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), image);
  gtk_widget_show (item);

  if (tooltip != NULL)
    {
      gtk_widget_set_tooltip_text (item, tooltip);

      g_object_bind_property (lock_logout, "enable-tooltips",
                              item, "has-tooltip",
                              G_BINDING_DEFAULT |
                              G_BINDING_SYNC_CREATE);
    }

  return item;
}

static void
setup_drag_source (GpLockLogout *self,
                   GtkWidget    *item,
                   const char   *icon_name,
                   const char   *iid)
{
  static const GtkTargetEntry drag_targets[] =
    {
      { (gchar *) "application/x-panel-applet-iid", 0, 0 }
    };

  if (self->locked_down)
    return;

  gtk_drag_source_set (item, GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
                       drag_targets, G_N_ELEMENTS (drag_targets),
                       GDK_ACTION_COPY);

  if (icon_name != NULL)
    gtk_drag_source_set_icon_name (item, icon_name);

  g_signal_connect_data (item, "drag-data-get",
                         G_CALLBACK (drag_data_get_cb),
                         g_strdup (iid),
                         (GClosureNotify) free_drag_id,
                         0);
}

static void
gp_lock_logout_constructed (GObject *object)
{
  GpLockLogout *lock_logout;
  const gchar *xdg_seat_path;

  lock_logout = GP_LOCK_LOGOUT (object);
  G_OBJECT_CLASS (gp_lock_logout_parent_class)->constructed (object);

  lock_logout->lockdown = g_settings_new ("org.gnome.desktop.lockdown");

  g_signal_connect (lock_logout->lockdown, "changed",
                    G_CALLBACK (lockdown_changed_cb), lock_logout);

  gp_login1_manager_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           "org.freedesktop.login1",
                                           "/org/freedesktop/login1",
                                           NULL,
                                           login1_manager_proxy_ready_cb,
                                           lock_logout);

  gp_session_manager_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                            G_DBUS_PROXY_FLAGS_NONE,
                                            "org.gnome.SessionManager",
                                            "/org/gnome/SessionManager",
                                            NULL,
                                            session_manager_proxy_ready_cb,
                                            lock_logout);

  gp_screensaver_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        "org.gnome.ScreenSaver",
                                        "/org/gnome/ScreenSaver",
                                        NULL,
                                        screensaver_proxy_ready_cb,
                                        lock_logout);

  xdg_seat_path = g_getenv ("XDG_SEAT_PATH");

  if (xdg_seat_path != NULL && *xdg_seat_path != '\0')
    {
      gp_dm_seat_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        "org.freedesktop.DisplayManager",
                                        xdg_seat_path,
                                        NULL,
                                        seat_ready_cb,
                                        lock_logout);
    }
}

static void
gp_lock_logout_dispose (GObject *object)
{
  GpLockLogout *lock_logout;

  lock_logout = GP_LOCK_LOGOUT (object);

  g_clear_object (&lock_logout->lockdown);
  g_clear_object (&lock_logout->login1_manager);
  g_clear_object (&lock_logout->session_manager);
  g_clear_object (&lock_logout->screensaver);
  g_clear_object (&lock_logout->seat);

  G_OBJECT_CLASS (gp_lock_logout_parent_class)->dispose (object);
}

static void
gp_lock_logout_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GpLockLogout *lock_logout;

  lock_logout = GP_LOCK_LOGOUT (object);

  switch (property_id)
    {
      case PROP_LOCKED_DOWN:
      case PROP_MENU_ICON_SIZE:
        g_assert_not_reached ();
        break;

      case PROP_ENABLE_TOOLTIPS:
        g_value_set_boolean (value, lock_logout->enable_tooltips);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
set_enable_tooltips (GpLockLogout *lock_logout,
                     gboolean      enable_tooltips)
{
  if (lock_logout->enable_tooltips == enable_tooltips)
    return;

  lock_logout->enable_tooltips = enable_tooltips;

  g_object_notify_by_pspec (G_OBJECT (lock_logout),
                            lock_logout_properties[PROP_ENABLE_TOOLTIPS]);
}

static void
gp_lock_logout_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GpLockLogout *lock_logout;

  lock_logout = GP_LOCK_LOGOUT (object);

  switch (property_id)
    {
      case PROP_ENABLE_TOOLTIPS:
        set_enable_tooltips (lock_logout, g_value_get_boolean (value));
        break;

      case PROP_LOCKED_DOWN:
        lock_logout->locked_down = g_value_get_boolean (value);
        break;

      case PROP_MENU_ICON_SIZE:
        lock_logout->menu_icon_size = g_value_get_uint (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  lock_logout_properties[PROP_ENABLE_TOOLTIPS] =
    g_param_spec_boolean ("enable-tooltips", "Enable Tooltips", "Enable Tooltips",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  lock_logout_properties[PROP_LOCKED_DOWN] =
    g_param_spec_boolean ("locked-down", "Locked Down", "Locked Down",
                          FALSE,
                          G_PARAM_CONSTRUCT | G_PARAM_WRITABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  lock_logout_properties[PROP_MENU_ICON_SIZE] =
    g_param_spec_uint ("menu-icon-size", "Menu Icon Size", "Menu Icon Size",
                       16, 48, 16,
                       G_PARAM_CONSTRUCT | G_PARAM_WRITABLE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     lock_logout_properties);
}

static void
install_signals (void)
{
  lock_logout_signals[CHANGED] =
    g_signal_new ("changed", GP_TYPE_LOCK_LOGOUT, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gp_lock_logout_class_init (GpLockLogoutClass *lock_logout_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (lock_logout_class);

  object_class->constructed = gp_lock_logout_constructed;
  object_class->dispose = gp_lock_logout_dispose;
  object_class->get_property = gp_lock_logout_get_property;
  object_class->set_property = gp_lock_logout_set_property;

  install_properties (object_class);
  install_signals ();
}

static void
gp_lock_logout_init (GpLockLogout *lock_logout)
{
}

GpLockLogout *
gp_lock_logout_new (void)
{
  return g_object_new (GP_TYPE_LOCK_LOGOUT,
                       NULL);
}

void
gp_lock_logout_append_to_menu (GpLockLogout *lock_logout,
                               GtkMenu      *menu)
{
  gboolean disable_user_switching;
  gboolean disable_log_out;
  const gchar *label;
  const gchar *tooltip;
  GtkWidget *switch_user;
  GtkWidget *logout;
  GtkWidget *lock_screen;
  GtkWidget *hibernate;
  GtkWidget *suspend;
  GtkWidget *hybrid_sleep;
  GtkWidget *reboot;
  GtkWidget *shutdown;
  gboolean can_shutdown;

  disable_user_switching = g_settings_get_boolean (lock_logout->lockdown,
                                                   "disable-user-switching");

  disable_log_out = g_settings_get_boolean (lock_logout->lockdown,
                                            "disable-log-out");

  switch_user = NULL;
  logout = NULL;
  lock_screen = NULL;

  /* Switch User */
  if (!disable_user_switching && get_can_switch_user (lock_logout))
    {
      label = _("Switch User");
      tooltip = NULL;

      switch_user = create_menu_item (lock_logout,
                                      "system-users",
                                      label,
                                      tooltip);

      g_signal_connect (switch_user, "activate",
                        G_CALLBACK (switch_user_activate_cb),
                        lock_logout);
    }

  /* Logout */
  if (!disable_log_out)
    {
      label = _("Log Out");
      tooltip = _("Log out of this session to log in as a different user");

      logout = create_menu_item (lock_logout,
                                 "system-log-out",
                                 label,
                                 tooltip);

      setup_drag_source (lock_logout,
                         logout,
                         "system-log-out",
                         "org.gnome.gnome-panel.action-button::logout");

      g_signal_connect (logout, "activate",
                        G_CALLBACK (logout_activate_cb),
                        lock_logout);
    }

  /* Lock Screen */
  if (!g_settings_get_boolean (lock_logout->lockdown, "disable-lock-screen"))
    {
      label = _("Lock Screen");
      tooltip = _("Protect your computer from unauthorized use");

      lock_screen = create_menu_item (lock_logout,
                                      "system-lock-screen",
                                      label,
                                      tooltip);

      setup_drag_source (lock_logout,
                         lock_screen,
                         "system-lock-screen",
                         "org.gnome.gnome-panel.action-button::lock-screen");

      g_signal_connect (lock_screen, "activate",
                        G_CALLBACK (lock_screen_activate_cb),
                        lock_logout);
    }

  if (switch_user != NULL ||
      logout != NULL ||
      lock_screen != NULL)
    {
      append_separator_if_needed (menu);

      if (switch_user != NULL)
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), switch_user);

      if (logout != NULL)
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), logout);

      if (lock_screen != NULL)
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), lock_screen);
    }

  if (disable_log_out)
    return;

  hibernate = NULL;
  suspend = NULL;
  hybrid_sleep = NULL;
  reboot = NULL;
  shutdown = NULL;

  /* Hibernate */
  if (get_can_hibernate (lock_logout))
    {
      label = _("Hibernate");
      tooltip = NULL;

      hibernate = create_menu_item (lock_logout,
                                    "gnome-panel-hibernate",
                                    label,
                                    tooltip);

      g_signal_connect (hibernate, "activate",
                        G_CALLBACK (hibernate_activate_cb),
                        lock_logout);
    }

  /* Suspend */
  if (get_can_suspend (lock_logout))
    {
      label = _("Suspend");
      tooltip = NULL;

      suspend = create_menu_item (lock_logout,
                                  "gnome-panel-suspend",
                                  label,
                                  tooltip);

      g_signal_connect (suspend, "activate",
                        G_CALLBACK (suspend_activate_cb),
                        lock_logout);
    }

  /* Hubrid Sleep */
  if (get_can_hybrid_sleep (lock_logout))
    {
      label = _("Hybrid Sleep");
      tooltip = NULL;

      hybrid_sleep = create_menu_item (lock_logout,
                                       "gnome-panel-suspend",
                                       label,
                                       tooltip);

      g_signal_connect (hybrid_sleep, "activate",
                        G_CALLBACK (hybrid_sleep_activate_cb),
                        lock_logout);
    }

  can_shutdown = get_can_shutdown (lock_logout);

  /* Reboot */
  if (can_shutdown)
    {
      label = _("Restart");
      tooltip = _("Restart the computer");

      reboot = create_menu_item (lock_logout,
                                 "view-refresh",
                                 label,
                                 tooltip);

      g_signal_connect (reboot, "activate",
                        G_CALLBACK (reboot_activate_cb),
                        lock_logout);
    }

  /* Shutdown */
  if (can_shutdown)
    {
      label = _("Power Off");
      tooltip = _("Power off the computer");

      shutdown = create_menu_item (lock_logout,
                                   "system-shutdown",
                                   label,
                                   tooltip);

      setup_drag_source (lock_logout,
                         shutdown,
                         "system-shutdown",
                         "org.gnome.gnome-panel.action-button::shutdown");

      g_signal_connect (shutdown, "activate",
                        G_CALLBACK (shutdown_activate_cb),
                        lock_logout);
    }

  if (hibernate != NULL ||
      suspend != NULL ||
      hybrid_sleep != NULL ||
      reboot != NULL ||
      shutdown != NULL)
    {
      append_separator_if_needed (menu);

      if (hibernate != NULL)
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), hibernate);

      if (suspend != NULL)
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), suspend);

      if (hybrid_sleep != NULL)
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), hybrid_sleep);

      if (reboot != NULL)
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), reboot);

      if (shutdown != NULL)
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), shutdown);
    }
}

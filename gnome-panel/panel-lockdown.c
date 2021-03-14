/* vim: set sw=8 et: */
/*
 * panel-lockdown.c: a lockdown tracker.
 *
 * Copyright (C) 2011 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Vincent Untz <vuntz@gnome.org>
 */

#include <libpanel-util/panel-cleanup.h>

#include "panel-schemas.h"

#include "panel-lockdown.h"

struct _PanelLockdownPrivate {
        GSettings *desktop_settings;
        GSettings *panel_settings;

        /* desktop-wide */
        gboolean   disable_command_line;
        gboolean   disable_lock_screen;
        gboolean   disable_log_out;
        gboolean   disable_switch_user;

        /* panel-specific */
        gboolean   panels_locked_down;
        gboolean   disable_force_quit;
        char     **disabled_applets;
};

enum {
        PROP_0,
        PROP_DISABLE_COMMAND_LINE,
        PROP_DISABLE_LOCK_SCREEN,
        PROP_DISABLE_LOG_OUT,
        PROP_DISABLE_SWITCH_USER,
        PROP_DISABLED_APPLETS,
        PROP_PANELS_LOCKED_DOWN,
        PROP_DISABLE_FORCE_QUIT
};

G_DEFINE_TYPE_WITH_PRIVATE (PanelLockdown, panel_lockdown, G_TYPE_OBJECT)

static GObject *
panel_lockdown_constructor (GType                  type,
                            guint                  n_construct_properties,
                            GObjectConstructParam *construct_properties)
{
        GObject       *obj;
        PanelLockdown *lockdown;

        obj = G_OBJECT_CLASS (panel_lockdown_parent_class)->constructor (type,
                                                                         n_construct_properties,
                                                                         construct_properties);

        lockdown = PANEL_LOCKDOWN (obj);

        lockdown->priv->desktop_settings = g_settings_new (PANEL_DESKTOP_LOCKDOWN_SCHEMA);
        lockdown->priv->panel_settings = g_settings_new (PANEL_LOCKDOWN_SCHEMA);

        g_settings_bind (lockdown->priv->desktop_settings,
                         PANEL_DESKTOP_DISABLE_COMMAND_LINE_KEY,
                         lockdown,
                         "disable-command-line",
                         G_SETTINGS_BIND_GET);

        g_settings_bind (lockdown->priv->desktop_settings,
                         PANEL_DESKTOP_DISABLE_LOCK_SCREEN_KEY,
                         lockdown,
                         "disable-lock-screen",
                         G_SETTINGS_BIND_GET);

        g_settings_bind (lockdown->priv->desktop_settings,
                         PANEL_DESKTOP_DISABLE_LOG_OUT_KEY,
                         lockdown,
                         "disable-log-out",
                         G_SETTINGS_BIND_GET);

        g_settings_bind (lockdown->priv->desktop_settings,
                         PANEL_DESKTOP_DISABLE_SWITCH_USER_KEY,
                         lockdown,
                         "disable-switch-user",
                         G_SETTINGS_BIND_GET);

        g_settings_bind (lockdown->priv->panel_settings,
                         PANEL_LOCKDOWN_DISABLED_APPLETS_KEY,
                         lockdown,
                         "disabled-applets",
                         G_SETTINGS_BIND_GET);

        g_settings_bind (lockdown->priv->panel_settings,
                         PANEL_LOCKDOWN_COMPLETE_LOCKDOWN_KEY,
                         lockdown,
                         "panels-locked-down",
                         G_SETTINGS_BIND_GET);

        g_settings_bind (lockdown->priv->panel_settings,
                         PANEL_LOCKDOWN_DISABLE_FORCE_QUIT_KEY,
                         lockdown,
                         "disable-force-quit",
                         G_SETTINGS_BIND_GET);

        return obj;
}

static void
_panel_lockdown_set_property_helper (PanelLockdown *lockdown,
                                     gboolean      *field,
                                     const GValue  *value,
                                     const char    *property)
{
        gboolean new;

        new = g_value_get_boolean (value);
        if (new == *field)
                return;

        *field = new;
        g_object_notify (G_OBJECT (lockdown), property);
}

static void
panel_lockdown_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
        PanelLockdown *lockdown;

        g_return_if_fail (PANEL_IS_LOCKDOWN (object));

        lockdown = PANEL_LOCKDOWN (object);

        switch (prop_id) {
        case PROP_DISABLE_COMMAND_LINE:
                _panel_lockdown_set_property_helper (lockdown,
                                                     &lockdown->priv->disable_command_line,
                                                     value,
                                                     "disable-command-line");
                break;
        case PROP_DISABLE_LOCK_SCREEN:
                _panel_lockdown_set_property_helper (lockdown,
                                                     &lockdown->priv->disable_lock_screen,
                                                     value,
                                                     "disable-lock-screen");
                break;
        case PROP_DISABLE_LOG_OUT:
                _panel_lockdown_set_property_helper (lockdown,
                                                     &lockdown->priv->disable_log_out,
                                                     value,
                                                     "disable-log-out");
                break;
        case PROP_DISABLE_SWITCH_USER:
                _panel_lockdown_set_property_helper (lockdown,
                                                     &lockdown->priv->disable_switch_user,
                                                     value,
                                                     "disable-switch-user");
                break;
        case PROP_DISABLED_APPLETS:
                g_strfreev (lockdown->priv->disabled_applets);
                lockdown->priv->disabled_applets = g_value_dup_boxed (value);
                break;
        case PROP_PANELS_LOCKED_DOWN:
                _panel_lockdown_set_property_helper (lockdown,
                                                     &lockdown->priv->panels_locked_down,
                                                     value,
                                                     "panels-locked-down");
                break;
        case PROP_DISABLE_FORCE_QUIT:
                _panel_lockdown_set_property_helper (lockdown,
                                                     &lockdown->priv->disable_force_quit,
                                                     value,
                                                     "disable-force-quit");
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
panel_lockdown_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
        PanelLockdown *lockdown;

        g_return_if_fail (PANEL_IS_LOCKDOWN (object));

        lockdown = PANEL_LOCKDOWN (object);

        switch (prop_id) {
        case PROP_DISABLE_COMMAND_LINE:
                g_value_set_boolean (value, lockdown->priv->disable_command_line);
                break;
        case PROP_DISABLE_LOCK_SCREEN:
                g_value_set_boolean (value, lockdown->priv->disable_lock_screen);
                break;
        case PROP_DISABLE_LOG_OUT:
                g_value_set_boolean (value, lockdown->priv->disable_log_out);
                break;
        case PROP_DISABLE_SWITCH_USER:
                g_value_set_boolean (value, lockdown->priv->disable_switch_user);
                break;
        case PROP_DISABLED_APPLETS:
                g_value_set_boxed (value, lockdown->priv->disabled_applets);
                break;
        case PROP_PANELS_LOCKED_DOWN:
                g_value_set_boolean (value, lockdown->priv->panels_locked_down);
                break;
        case PROP_DISABLE_FORCE_QUIT:
                g_value_set_boolean (value, lockdown->priv->disable_force_quit);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
panel_lockdown_dispose (GObject *object)
{
        PanelLockdown *lockdown;

        lockdown = PANEL_LOCKDOWN (object);

        if (lockdown->priv->desktop_settings)
                g_object_unref (lockdown->priv->desktop_settings);
        lockdown->priv->desktop_settings = NULL;

        if (lockdown->priv->panel_settings)
                g_object_unref (lockdown->priv->panel_settings);
        lockdown->priv->panel_settings = NULL;

        if (lockdown->priv->disabled_applets)
                g_strfreev (lockdown->priv->disabled_applets);
        lockdown->priv->disabled_applets = NULL;

        G_OBJECT_CLASS (panel_lockdown_parent_class)->dispose (object);
}

static void
panel_lockdown_init (PanelLockdown *lockdown)
{
        lockdown->priv = panel_lockdown_get_instance_private (lockdown);
}

static void
panel_lockdown_class_init (PanelLockdownClass *lockdown_class)
{
        GObjectClass *gobject_class;

        gobject_class = G_OBJECT_CLASS (lockdown_class);

        gobject_class->constructor  = panel_lockdown_constructor;
        gobject_class->set_property = panel_lockdown_set_property;
        gobject_class->get_property = panel_lockdown_get_property;
        gobject_class->dispose      = panel_lockdown_dispose;

        g_object_class_install_property (
                gobject_class,
                PROP_DISABLE_COMMAND_LINE,
                g_param_spec_boolean (
                        "disable-command-line",
                        "Disable command line",
                        "Whether command line is disabled or not",
                        TRUE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        g_object_class_install_property (
                gobject_class,
                PROP_DISABLE_LOCK_SCREEN,
                g_param_spec_boolean (
                        "disable-lock-screen",
                        "Disable lock screen",
                        "Whether lock screen is disabled or not",
                        TRUE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        g_object_class_install_property (
                gobject_class,
                PROP_DISABLE_LOG_OUT,
                g_param_spec_boolean (
                        "disable-log-out",
                        "Disable log out",
                        "Whether log out is disabled or not",
                        TRUE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        g_object_class_install_property (
                gobject_class,
                PROP_DISABLE_SWITCH_USER,
                g_param_spec_boolean (
                        "disable-switch-user",
                        "Disable user switching",
                        "Whether user switching is disabled or not",
                        TRUE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        g_object_class_install_property (
                gobject_class,
                PROP_DISABLED_APPLETS,
                g_param_spec_boxed (
                        "disabled-applets",
                        "Disabled applets",
                        "Applet IIDs to disable from loading",
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        g_object_class_install_property (
                gobject_class,
                PROP_PANELS_LOCKED_DOWN,
                g_param_spec_boolean (
                        "panels-locked-down",
                        "Full locked down of panels",
                        "Whether panels are fully locked down or not",
                        TRUE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        g_object_class_install_property (
                gobject_class,
                PROP_DISABLE_FORCE_QUIT,
                g_param_spec_boolean (
                        "disable-force-quit",
                        "Disable force quit",
                        "Whether force quit is disabled or not",
                        TRUE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

gboolean
panel_lockdown_is_applet_disabled (PanelLockdown *lockdown,
                                   const char    *iid)
{
        int i;

        g_return_val_if_fail (PANEL_IS_LOCKDOWN (lockdown), TRUE);

        for (i = 0; lockdown->priv->disabled_applets[i] != NULL; i++)
                if (g_strcmp0 (lockdown->priv->disabled_applets[i], iid) == 0)
                        return TRUE;

        return FALSE;
}

gboolean
panel_lockdown_get_disable_command_line (PanelLockdown *lockdown)
{
        g_return_val_if_fail (PANEL_IS_LOCKDOWN (lockdown), TRUE);

        return lockdown->priv->disable_command_line;
}

gboolean
panel_lockdown_get_panels_locked_down (PanelLockdown *lockdown)
{
        g_return_val_if_fail (PANEL_IS_LOCKDOWN (lockdown), TRUE);

        return lockdown->priv->panels_locked_down;
}

typedef struct {
        PanelLockdown       *lockdown;
        PanelLockdownNotify  callback;
        gpointer             callback_data;
        guint                handler_id;
} PanelLockdownNotifyData;

static void
_panel_lockdown_notify_data_destroy (gpointer data)
{
        PanelLockdownNotifyData *notify_data = data;

        g_signal_handler_disconnect (notify_data->lockdown,
                                     notify_data->handler_id);

        g_free (notify_data);
}

static void
panel_lockdown_on_notify_notified (GObject    *gobject,
                                   GParamSpec *pspec,
                                   gpointer    user_data)
{
        PanelLockdownNotifyData *notify_data = user_data;

        g_assert (notify_data->callback != NULL);
        g_assert ((gpointer) notify_data->lockdown == (gpointer) gobject);

        notify_data->callback (notify_data->lockdown,
                               notify_data->callback_data);
}

/* An object can only call this once per property.
 * User NULL property to notify for all lockdown changes. (except disabled_applets) */
void
panel_lockdown_on_notify (PanelLockdown       *lockdown,
                          const char          *property,
                          GObject             *object_while_alive,
                          PanelLockdownNotify  callback,
                          gpointer             callback_data)
{
        PanelLockdownNotifyData *notify_data;
        char *key;
        char *signal_name;

        g_return_if_fail (PANEL_IS_LOCKDOWN (lockdown));
        g_return_if_fail (G_IS_OBJECT (object_while_alive));
        g_return_if_fail (callback != NULL);

        notify_data = g_new0 (PanelLockdownNotifyData, 1);

        notify_data->lockdown      = lockdown;
        notify_data->callback      = callback;
        notify_data->callback_data = callback_data;
        notify_data->handler_id    = 0;

        if (property)
                key = g_strdup_printf ("panel-lockdown-%s", property);
        else
                key = g_strdup_printf ("panel-lockdown");
        g_object_set_data_full (object_while_alive, key,
                                notify_data,
                                _panel_lockdown_notify_data_destroy);
        g_free (key);

        if (property)
                signal_name = g_strdup_printf ("notify::%s", property);
        else
                signal_name = g_strdup_printf ("notify");
        notify_data->handler_id = g_signal_connect (lockdown, signal_name,
                                                    G_CALLBACK (panel_lockdown_on_notify_notified),
                                                    notify_data);
        g_free (signal_name);
}

PanelLockdown *
panel_lockdown_get (void)
{
        static PanelLockdown *shared_lockdown = NULL;

        if (shared_lockdown == NULL) {
                shared_lockdown = g_object_new (PANEL_TYPE_LOCKDOWN, NULL);
                panel_cleanup_register (panel_cleanup_unref_and_nullify,
                                        &shared_lockdown);
        }

        return shared_lockdown;
}

gboolean
panel_lockdown_get_disable_command_line_s (void)
{
        return panel_lockdown_get_disable_command_line (panel_lockdown_get ());
}

gboolean
panel_lockdown_get_panels_locked_down_s (void)
{
        return panel_lockdown_get_panels_locked_down (panel_lockdown_get ());
}

GpLockdownFlags
panel_lockdown_get_flags (PanelLockdown *lockdown,
                          const char    *iid)
{
        GpLockdownFlags flags;
        int i;

        g_return_val_if_fail (PANEL_IS_LOCKDOWN (lockdown), GP_LOCKDOWN_FLAGS_NONE);

        flags = GP_LOCKDOWN_FLAGS_NONE;

        for (i = 0; lockdown->priv->disabled_applets[i] != NULL; i++) {
                if (g_strcmp0 (lockdown->priv->disabled_applets[i], iid) == 0) {
                        flags |= GP_LOCKDOWN_FLAGS_APPLET;
                        break;
                }
        }

        if (lockdown->priv->disable_force_quit)
                flags |= GP_LOCKDOWN_FLAGS_FORCE_QUIT;

        if (lockdown->priv->panels_locked_down)
                flags |= GP_LOCKDOWN_FLAGS_LOCKED_DOWN;

        if (lockdown->priv->disable_command_line)
                flags |= GP_LOCKDOWN_FLAGS_COMMAND_LINE;

        if (lockdown->priv->disable_lock_screen)
                flags |= GP_LOCKDOWN_FLAGS_LOCK_SCREEN;

        if (lockdown->priv->disable_log_out)
                flags |= GP_LOCKDOWN_FLAGS_LOG_OUT;

        if (lockdown->priv->disable_switch_user)
                flags |= GP_LOCKDOWN_FLAGS_USER_SWITCHING;

        return flags;
}

GpLockdownFlags
panel_lockdown_get_flags_s (const char *iid)
{
        return panel_lockdown_get_flags (panel_lockdown_get (), iid);
}

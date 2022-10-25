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
#include "gp-run-applet.h"

#include <glib/gi18n-lib.h>

#include "panel-run-dialog.h"

struct _GpRunApplet
{
  GpActionButtonApplet parent;
};

G_DEFINE_TYPE (GpRunApplet, gp_run_applet, GP_TYPE_ACTION_BUTTON_APPLET)

static void
lockdown_changed (GpRunApplet *self)
{
  GpLockdownFlags lockdowns;
  gboolean applet_sensitive;

  lockdowns = gp_applet_get_lockdowns (GP_APPLET (self));

  applet_sensitive = TRUE;

  if ((lockdowns & GP_LOCKDOWN_FLAGS_APPLET) == GP_LOCKDOWN_FLAGS_APPLET ||
      (lockdowns & GP_LOCKDOWN_FLAGS_COMMAND_LINE) == GP_LOCKDOWN_FLAGS_COMMAND_LINE)
    applet_sensitive = FALSE;

  gtk_widget_set_sensitive (GTK_WIDGET (self), applet_sensitive);
}

static void
lockdowns_cb (GpApplet    *applet,
              GParamSpec  *pspec,
              GpRunApplet *self)
{
  lockdown_changed (self);
}

static void
setup_applet (GpRunApplet *self)
{
  const char *text;
  AtkObject *atk;

  gp_action_button_applet_set_icon_name (GP_ACTION_BUTTON_APPLET (self),
                                         "system-run");

  text = _("Run an application by typing a command or choosing from a list");

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
gp_run_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_run_applet_parent_class)->constructed (object);
  setup_applet (GP_RUN_APPLET (object));
}

static void
gp_run_applet_dispose (GObject *object)
{
  G_OBJECT_CLASS (gp_run_applet_parent_class)->dispose (object);
}

static void
gp_run_applet_clicked (GpActionButtonApplet *applet)
{
  panel_run_dialog_present (gtk_widget_get_screen (GTK_WIDGET (applet)),
                            gtk_get_current_event_time ());
}

static void
gp_run_applet_class_init (GpRunAppletClass *self_class)
{
  GObjectClass *object_class;
  GpActionButtonAppletClass *action_button_applet_class;

  object_class = G_OBJECT_CLASS (self_class);
  action_button_applet_class = GP_ACTION_BUTTON_APPLET_CLASS (self_class);

  object_class->constructed = gp_run_applet_constructed;
  object_class->dispose = gp_run_applet_dispose;

  action_button_applet_class->clicked = gp_run_applet_clicked;
}

static void
gp_run_applet_init (GpRunApplet *self)
{
  g_signal_connect (self,
                    "notify::lockdowns",
                    G_CALLBACK (lockdowns_cb),
                    self);
}

gboolean
gp_run_applet_is_disabled (GpLockdownFlags   flags,
                           char            **reason)
{
  if ((flags & GP_LOCKDOWN_FLAGS_COMMAND_LINE) != GP_LOCKDOWN_FLAGS_COMMAND_LINE)
    return FALSE;

  *reason = g_strdup (_("Disabled because “disable-command-line” setting in "
                        "“org.gnome.desktop.lockdown” GSettings schema is "
                        "set to true."));

  return TRUE;
}

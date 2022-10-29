/*
 * Copyright (C) 2022 Alberts Muktupāvels
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
#include "gp-error-applet.h"

#include <glib/gi18n.h>

#include "panel-lockdown.h"

enum
{
  RESPONSE_DONT_DELETE,
  RESPONSE_DELETE
};

struct _GpErrorApplet
{
  GpApplet      parent;

  GtkWidget     *button;
  GtkWidget     *image;

  char          *module_id;
  char          *applet_id;
  GError        *error;
  GpApplication *application;

  GtkWidget     *dialog;
};

enum
{
  DELETE,

  LAST_SIGNAL
};

static guint error_applet_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpErrorApplet, gp_error_applet, GP_TYPE_APPLET)

static void
response_cb (GtkWidget     *dialog,
             guint          response,
             GpErrorApplet *self)
{
  g_clear_pointer (&self->dialog, gtk_widget_destroy);

  if (response != RESPONSE_DELETE)
    return;

  g_signal_emit (self, error_applet_signals[DELETE], 0);
}

static void
append_details (GtkMessageDialog *dialog,
                GError           *error)
{
  GtkWidget *message_area;
  GtkWidget *frame;
  GtkStyleContext *context;
  GtkWidget *label;

  message_area = gtk_message_dialog_get_message_area (dialog);

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (message_area), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  context = gtk_widget_get_style_context (frame);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

  label = gtk_label_new (error->message);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_container_add (GTK_CONTAINER (frame), label);
  g_object_set (label, "margin", 12, NULL);
  gtk_widget_show (label);
}

static void
clicked_cb (GtkButton     *button,
            GpErrorApplet *self)
{
  PanelLockdown *lockdown;
  gboolean locked_down;
  GtkMessageType type;
  const char *delete_text;
  char *secondary_text;

  if (self->dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (self->dialog));
      return;
    }

  lockdown = gp_application_get_lockdown (self->application);
  locked_down = panel_lockdown_get_panels_locked_down (lockdown);

  type = locked_down ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING;

  self->dialog = gtk_message_dialog_new (NULL,
                                         0,
                                         type,
                                         GTK_BUTTONS_NONE,
                                         _("Failed to Load Applet"));

  delete_text = _("Do you want to delete the applet from your configuration?");
  secondary_text = g_strdup_printf (_("The panel encountered a problem while "
                                      "loading “<b>%s</b>” applet from "
                                      "“<b>%s</b>” module! %s"),
                                   self->applet_id,
                                   self->module_id,
                                   !locked_down ? delete_text : "");

  g_object_set (self->dialog,
                "secondary-use-markup", TRUE,
                "secondary-text", secondary_text,
                NULL);

  g_free (secondary_text);

  append_details (GTK_MESSAGE_DIALOG (self->dialog), self->error);

  if (locked_down)
    {
      gtk_dialog_add_buttons (GTK_DIALOG (self->dialog),
                              _("OK"), RESPONSE_DONT_DELETE,
                              NULL);
    }
  else
    {
      GtkWidget *btn;
      GtkStyleContext *context;

      btn = gtk_dialog_add_button (GTK_DIALOG (self->dialog),
                                   _("_Delete"),
                                   RESPONSE_DELETE);

      context = gtk_widget_get_style_context (btn);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_DESTRUCTIVE_ACTION);

      gtk_dialog_add_button (GTK_DIALOG (self->dialog),
                             _("D_on't Delete"),
                             RESPONSE_DONT_DELETE);
    }

  gtk_dialog_set_default_response (GTK_DIALOG (self->dialog),
                                   RESPONSE_DONT_DELETE);

  g_signal_connect (self->dialog,
                    "response",
                    G_CALLBACK (response_cb),
                    self);

  gtk_widget_realize (self->dialog);
  gdk_window_set_title (gtk_widget_get_window (self->dialog),
                        _("Error"));

  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (self->dialog), FALSE);
  gtk_window_set_urgency_hint (GTK_WINDOW (self->dialog), TRUE);
  gtk_window_present (GTK_WINDOW (self->dialog));
}

static void
applet_setup (GpErrorApplet *self)
{
  const char *tooltip;

  tooltip = _("Failed to load applet! Click for more info...");
  gtk_widget_set_tooltip_text (self->button, tooltip);
}

static void
update_icon (GpErrorApplet *self)
{
  const char *icon_name;
  guint icon_size;

  if (gp_applet_get_prefer_symbolic_icons (GP_APPLET (self)))
    icon_name = "dialog-error-symbolic";
  else
    icon_name = "dialog-error";

  gtk_image_set_from_icon_name (GTK_IMAGE (self->image),
                                icon_name,
                                GTK_ICON_SIZE_MENU);

  icon_size = gp_applet_get_panel_icon_size (GP_APPLET (self));
  gtk_image_set_pixel_size (GTK_IMAGE (self->image), icon_size);
}

static void
panel_icon_size_cb (GpApplet      *applet,
                    GParamSpec    *pspec,
                    GpErrorApplet *self)
{
  update_icon (self);
}

static void
prefer_symbolic_icons_cb (GpApplet      *applet,
                          GParamSpec    *pspec,
                          GpErrorApplet *self)
{
  update_icon (self);
}

static void
gp_error_applet_finalize (GObject *object)
{
  GpErrorApplet *self;

  self = GP_ERROR_APPLET (object);

  g_clear_pointer (&self->dialog, gtk_widget_destroy);
  g_clear_pointer (&self->error, g_error_free);
  g_clear_pointer (&self->module_id, g_free);
  g_clear_pointer (&self->applet_id, g_free);

  G_OBJECT_CLASS (gp_error_applet_parent_class)->finalize (object);
}

static gboolean
gp_error_applet_initable_init (GpApplet  *applet,
                               GError   **error)
{
  GpErrorApplet *self;

  self = GP_ERROR_APPLET (applet);

  g_signal_connect (applet,
                    "notify::panel-icon-size",
                    G_CALLBACK (panel_icon_size_cb),
                    self);

  g_signal_connect (applet,
                    "notify::prefer-symbolic-icons",
                    G_CALLBACK (prefer_symbolic_icons_cb),
                    self);

  update_icon (self);

  return TRUE;
}

static void
gp_error_applet_class_init (GpErrorAppletClass *self_class)
{
  GObjectClass *object_class;
  GpAppletClass *applet_class;

  object_class = G_OBJECT_CLASS (self_class);
  applet_class = GP_APPLET_CLASS (self_class);

  object_class->finalize = gp_error_applet_finalize;

  applet_class->initable_init = gp_error_applet_initable_init;

  error_applet_signals[DELETE] =
    g_signal_new ("delete",
                  GP_TYPE_ERROR_APPLET,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
gp_error_applet_init (GpErrorApplet *self)
{
  self->button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (self->button), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER (self), self->button);
  gtk_widget_show (self->button);

  g_signal_connect (self->button, "clicked", G_CALLBACK (clicked_cb), self);

  self->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (self->button), self->image);
  gtk_widget_show (self->image);
}

GpErrorApplet *
gp_error_applet_new (const char    *module_id,
                     const char    *applet_id,
                     GError        *error,
                     GpApplication *application)
{
  GpErrorApplet *self;

  self = g_initable_new (GP_TYPE_ERROR_APPLET, NULL, NULL,
                         "id", "error",
                         "gettext-domain", GETTEXT_PACKAGE,
                         NULL);

  self->module_id = g_strdup (module_id);
  self->applet_id = g_strdup (applet_id);
  self->error = g_error_copy (error);
  self->application = application;

  applet_setup (self);

  return self;
}

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
#include "gp-action-button-applet.h"

#include "gp-action-button.h"

typedef struct
{
  GtkWidget *button;
  GtkWidget *image;
} GpActionButtonAppletPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GpActionButtonApplet,
                                     gp_action_button_applet,
                                     GP_TYPE_APPLET)

static void
panel_icon_size_cb (GpApplet             *applet,
                    GParamSpec           *pspec,
                    GpActionButtonApplet *self)
{
  GpActionButtonAppletPrivate *priv;
  guint icon_size;

  priv = gp_action_button_applet_get_instance_private (self);

  icon_size = gp_applet_get_panel_icon_size (applet);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);
}

static void
clicked_cb (GtkWidget            *widget,
            GpActionButtonApplet *self)
{
  GP_ACTION_BUTTON_APPLET_GET_CLASS (self)->clicked (self);
}

static void
gp_action_button_applet_class_init (GpActionButtonAppletClass *self_class)
{
}

static void
gp_action_button_applet_init (GpActionButtonApplet *self)
{
  GpActionButtonAppletPrivate *priv;

  priv = gp_action_button_applet_get_instance_private (self);

  gp_applet_set_flags (GP_APPLET (self), GP_APPLET_FLAGS_EXPAND_MINOR);

  priv->button = gp_action_button_new ();
  gtk_container_add (GTK_CONTAINER (self), priv->button);
  gtk_widget_show (priv->button);

  priv->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (priv->button), priv->image);
  gtk_widget_show (priv->image);

  g_signal_connect (self,
                    "notify::panel-icon-size",
                    G_CALLBACK (panel_icon_size_cb),
                    self);

  g_signal_connect (priv->button,
                    "clicked",
                    G_CALLBACK (clicked_cb),
                    self);
}

void
gp_action_button_applet_set_icon_name (GpActionButtonApplet *self,
                                       const char           *icon_name)
{
  GpActionButtonAppletPrivate *priv;
  guint icon_size;

  priv = gp_action_button_applet_get_instance_private (self);

  gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
                                icon_name,
                                GTK_ICON_SIZE_MENU);

  icon_size = gp_applet_get_panel_icon_size (GP_APPLET (self));
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);
}

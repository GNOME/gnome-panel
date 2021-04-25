/*
 * Copyright (C) 2021 Alberts Muktupāvels
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
#include "gp-handle.h"

#include "panel-enums-gsettings.h"
#include "panel-typebuiltins.h"

#define HANDLE_SIZE 10

struct _GpHandle
{
  GtkWidget        parent;

  PanelOrientation orientation;
};

enum
{
  PROP_0,

  PROP_ORIENTATION,

  LAST_PROP
};

static GParamSpec *handle_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpHandle, gp_handle, GTK_TYPE_WIDGET)

static void
gp_handle_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GpHandle *self;

  self = GP_HANDLE (object);

  switch (property_id)
    {
      case PROP_ORIENTATION:
        g_value_set_enum (value, self->orientation);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gp_handle_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GpHandle *self;

  self = GP_HANDLE (object);

  switch (property_id)
    {
      case PROP_ORIENTATION:
        self->orientation = g_value_get_enum (value);
        gtk_widget_queue_resize (GTK_WIDGET (self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static gboolean
gp_handle_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (gtk_widget_get_toplevel (widget));

  gtk_render_handle (context,
                     cr,
                     0,
                     0,
                     gtk_widget_get_allocated_width (widget),
                     gtk_widget_get_allocated_height (widget));

  return FALSE;
}

static void
gp_handle_get_preferred_height (GtkWidget *widget,
                                gint      *minimum_height,
                                gint      *natural_height)
{
  GpHandle *self;

  self = GP_HANDLE (widget);

  if (self->orientation & PANEL_VERTICAL_MASK)
    *minimum_height = *natural_height = HANDLE_SIZE;
  else
    *minimum_height = *natural_height = 0;
}

static void
gp_handle_get_preferred_width (GtkWidget *widget,
                               gint      *minimum_width,
                               gint      *natural_width)
{
  GpHandle *self;

  self = GP_HANDLE (widget);

  if (self->orientation & PANEL_HORIZONTAL_MASK)
    *minimum_width = *natural_width = HANDLE_SIZE;
  else
    *minimum_width = *natural_width = 0;
}

static void
install_properties (GObjectClass *object_class)
{
  handle_properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation",
                       "orientation",
                       "orientation",
                       PANEL_TYPE_ORIENTATION,
                       PANEL_ORIENTATION_TOP,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     handle_properties);
}

static void
gp_handle_class_init (GpHandleClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->get_property = gp_handle_get_property;
  object_class->set_property = gp_handle_set_property;

  widget_class->draw = gp_handle_draw;
  widget_class->get_preferred_height = gp_handle_get_preferred_height;
  widget_class->get_preferred_width = gp_handle_get_preferred_width;

  install_properties (object_class);
}

static void
gp_handle_init (GpHandle *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

GtkWidget *
gp_handle_new (void)
{
  return g_object_new (GP_TYPE_HANDLE, NULL);
}

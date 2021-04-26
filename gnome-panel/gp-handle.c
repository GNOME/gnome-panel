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
  GtkStateFlags flags;
  int width;
  int height;
  GtkBorder margin;
  GtkBorder border;
  GtkBorder padding;
  int handle_width;
  int handle_height;

  context = gtk_widget_get_style_context (widget);
  flags = gtk_style_context_get_state (context);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  gtk_style_context_get_margin (context, flags, &margin);
  gtk_style_context_get_border (context, flags, &border);
  gtk_style_context_get_padding (context, flags, &padding);

  gtk_style_context_get (context,
                         flags,
                         "min-height", &handle_height,
                         "min-width", &handle_width,
                         NULL);

  width -= margin.left + border.left + padding.left + padding.right + border.right + margin.right;
  height -= margin.top + border.top + padding.top + padding.bottom + border.bottom + margin.bottom;

  gtk_render_check (context,
                    cr,
                    margin.left + border.left + padding.left + (width - handle_width) / 2,
                    margin.top + border.top + padding.top + (height - handle_height) / 2,
                    handle_width,
                    handle_height);

  return FALSE;
}

static void
gp_handle_get_preferred_height (GtkWidget *widget,
                                gint      *minimum_height,
                                gint      *natural_height)
{
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkBorder margin;
  GtkBorder border;
  GtkBorder padding;
  int handle_height;
  int height;

  context = gtk_widget_get_style_context (widget);
  flags = gtk_style_context_get_state (context);

  gtk_style_context_get_margin (context, flags, &margin);
  gtk_style_context_get_border (context, flags, &border);
  gtk_style_context_get_padding (context, flags, &padding);
  gtk_style_context_get (context, flags, "min-height", &handle_height, NULL);

  height = margin.top + border.top + padding.top;
  height += handle_height;
  height += padding.bottom + border.bottom + margin.bottom;

  *minimum_height = *natural_height = height;
}

static void
gp_handle_get_preferred_width (GtkWidget *widget,
                               gint      *minimum_width,
                               gint      *natural_width)
{
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkBorder margin;
  GtkBorder border;
  GtkBorder padding;
  int handle_width;
  int width;

  context = gtk_widget_get_style_context (widget);
  flags = gtk_style_context_get_state (context);

  gtk_style_context_get_margin (context, flags, &margin);
  gtk_style_context_get_border (context, flags, &border);
  gtk_style_context_get_padding (context, flags, &padding);
  gtk_style_context_get (context, flags, "min-width", &handle_width, NULL);

  width = margin.left + border.left + padding.left;
  width += handle_width;
  width += padding.right + border.right + margin.right;

  *minimum_width = *natural_width = width;
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

  gtk_widget_class_set_css_name (widget_class, "gp-handle");
}

static void
gp_handle_init (GpHandle *self)
{
  GtkWidget *widget;
  GtkStyleContext *context;

  widget = GTK_WIDGET (self);

  gtk_widget_set_has_window (widget, FALSE);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_add_class (context, "gp-text-color");
}

GtkWidget *
gp_handle_new (void)
{
  return g_object_new (GP_TYPE_HANDLE, NULL);
}

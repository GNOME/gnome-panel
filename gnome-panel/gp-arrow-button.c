/*
 * Copyright (C) 2015 Alberts Muktupāvels
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

#include <config.h>

#include <glib/gi18n.h>

#include "gp-arrow-button.h"

struct _GpArrowButton
{
  GtkButton     parent;

  GtkArrowType  arrow_type;
  gboolean      arrow_visible;

  GtkWidget    *arrow;
};

enum
{
  PROP_0,

  PROP_ARROW_TYPE,
  PROP_ARROW_VISIBLE,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpArrowButton, gp_arrow_button, GTK_TYPE_BUTTON)

static void
set_arrow_visible (GpArrowButton *button,
                   gboolean       arrow_visible)
{
  if (button->arrow_visible == arrow_visible)
    return;

  button->arrow_visible = arrow_visible;

  gtk_widget_set_visible (button->arrow, arrow_visible);
}

static void
set_arrow_type (GpArrowButton *button,
                GtkArrowType   arrow_type)
{
  const gchar *icon_name;
  GtkImage *image;

  if (button->arrow_type == arrow_type)
    return;

  button->arrow_type = arrow_type;

  switch (arrow_type)
    {
      case GTK_ARROW_UP:
        icon_name = "pan-up-symbolic";
        break;

      case GTK_ARROW_DOWN:
        icon_name = "pan-down-symbolic";
        break;

      case GTK_ARROW_LEFT:
        icon_name = "pan-start-symbolic";
        break;

      case GTK_ARROW_RIGHT:
        icon_name = "pan-end-symbolic";
        break;

      case GTK_ARROW_NONE:
      default:
        icon_name = NULL;
        break;
    }

  image = GTK_IMAGE (button->arrow);

  if (icon_name == NULL)
    {
      gtk_image_clear (image);
      set_arrow_visible (button, FALSE);

      return;
    }

  gtk_image_set_from_icon_name (image, icon_name, GTK_ICON_SIZE_MENU);
}

static void
gp_arrow_button_constructed (GObject *object)
{
  GtkWidget *widget;
  AtkObject *atk;

  G_OBJECT_CLASS (gp_arrow_button_parent_class)->constructed (object);

  widget = GTK_WIDGET (object);
  atk = gtk_widget_get_accessible (widget);

  atk_object_set_name (atk, _("Hide Panel"));
}

static void
gp_arrow_button_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GpArrowButton *button;

  button = GP_ARROW_BUTTON (object);

  switch (property_id)
    {
      case PROP_ARROW_TYPE:
        g_value_set_enum (value, button->arrow_type);
        break;

      case PROP_ARROW_VISIBLE:
        g_value_set_boolean (value, button->arrow_visible);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gp_arrow_button_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GpArrowButton *button;

  button = GP_ARROW_BUTTON (object);

  switch (property_id)
    {
      case PROP_ARROW_TYPE:
        set_arrow_type (button, g_value_get_enum (value));
        break;

      case PROP_ARROW_VISIBLE:
        set_arrow_visible (button, g_value_get_boolean (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gp_arrow_button_class_init (GpArrowButtonClass *button_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (button_class);
  widget_class = GTK_WIDGET_CLASS (button_class);

  object_class->constructed = gp_arrow_button_constructed;
  object_class->get_property = gp_arrow_button_get_property;
  object_class->set_property = gp_arrow_button_set_property;

  properties[PROP_ARROW_TYPE] =
    g_param_spec_enum ("arrow-type", "arrow-type", "arrow-type",
                       GTK_TYPE_ARROW_TYPE, GTK_ARROW_NONE,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                       G_PARAM_STATIC_STRINGS);

  properties[PROP_ARROW_VISIBLE] =
    g_param_spec_boolean ("arrow-visible", "arrow-visible", "arrow-visible",
                          TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_css_name (widget_class, "gp-arrow-button");
}

static void
gp_arrow_button_init (GpArrowButton *button)
{
  button->arrow = gtk_image_new ();
  button->arrow_type = GTK_ARROW_NONE;

  gtk_container_add (GTK_CONTAINER (button), button->arrow);
}

GtkWidget *
gp_arrow_button_new (GtkArrowType arrow_type)
{
  return g_object_new (GP_TYPE_ARROW_BUTTON,
                       "arrow-type", arrow_type,
                       NULL);
}

void
gp_arrow_button_set_arrow_type (GpArrowButton *self,
                                GtkArrowType   arrow_type)
{
  set_arrow_type (self, arrow_type);
}

GtkArrowType
gp_arrow_button_get_arrow_type (GpArrowButton *button)
{
  return button->arrow_type;
}

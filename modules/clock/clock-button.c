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
#include "clock-button.h"

#include <libgnome-panel/gp-utils.h>

struct _ClockButton
{
  GtkToggleButton  parent;

  GtkOrientation   orientation;
  GtkPositionType  position;
  guint            icon_size;

  double           angle;

  GtkWidget       *main_box;

  GtkWidget       *clock_label;

  GtkWidget       *weather_box;
  GtkWidget       *weather_image;
  GtkWidget       *temperature_label;

  char            *weather_icon;

  guint            update_angle_id;
};

G_DEFINE_TYPE (ClockButton, clock_button, GTK_TYPE_TOGGLE_BUTTON)

static void
update_weather_visibility (ClockButton *self)
{
  gboolean has_icon;
  gboolean has_temperature;

  has_icon = gtk_widget_get_visible (self->weather_image);
  has_temperature = gtk_widget_get_visible (self->temperature_label);

  gtk_widget_set_visible (self->weather_box, has_icon || has_temperature);
}

static void
update_weather_icon (ClockButton *self)
{
  if (self->weather_icon == NULL || *self->weather_icon == '\0')
    {
      gtk_image_clear (GTK_IMAGE (self->weather_image));
      gtk_widget_hide (self->weather_image);
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (self->weather_image),
                                    self->weather_icon,
                                    GTK_ICON_SIZE_MENU);

      gtk_image_set_pixel_size (GTK_IMAGE (self->weather_image),
                                self->icon_size);

      gtk_widget_show (self->weather_image);
    }

  update_weather_visibility (self);
}

static void
set_weather_text (ClockButton *self,
                  const char  *temperature)
{
  if (temperature == NULL)
    temperature = "";

  gtk_label_set_text (GTK_LABEL (self->temperature_label), temperature);
  gtk_widget_set_visible (self->temperature_label, *temperature != '\0');

  update_weather_visibility (self);
}

static void
set_text_gravity_to_auto (GtkWidget *widget)
{
  PangoLayout *layout;
  PangoContext *context;

  layout = gtk_label_get_layout (GTK_LABEL (widget));
  context = pango_layout_get_context (layout);

  pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
}

static void
set_tnum_font_feature (GtkWidget *widget)
{
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_font_features_new ("tnum"));

  gtk_label_set_attributes (GTK_LABEL (widget), attrs);
  pango_attr_list_unref (attrs);
}

static int
get_clock_width (ClockButton *self)
{
  GtkWidget *label;
  int width;

  label = gtk_label_new (gtk_label_get_text (GTK_LABEL (self->clock_label)));
  set_text_gravity_to_auto (label);
  set_tnum_font_feature (label);
  gp_add_text_color_class (label);
  gtk_widget_show (label);

  gtk_widget_get_preferred_width (label, &width, NULL);

  g_object_ref_sink (label);
  g_object_unref (label);

  return width;
}

static void
update_angle (ClockButton *self)
{
  double new_angle;

  new_angle = 0.0;

  if (self->orientation == GTK_ORIENTATION_VERTICAL)
    {
      GtkAllocation allocation;
      GtkStyleContext *context;
      GtkStateFlags state;
      GtkBorder padding;

      gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

      context = gtk_widget_get_style_context (GTK_WIDGET (self));
      state = gtk_style_context_get_state (context);

      gtk_style_context_get_padding (context, state, &padding);

      allocation.width -= padding.left + padding.right;

      if (allocation.width < get_clock_width (self))
        {
          if (self->position == GTK_POS_RIGHT)
            new_angle = 270.0;
          else if (self->position == GTK_POS_LEFT)
            new_angle = 90.0;
        }
    }

  if (self->angle == new_angle)
    return;

  gtk_label_set_angle (GTK_LABEL (self->clock_label), new_angle);
  gtk_label_set_angle (GTK_LABEL (self->temperature_label), new_angle);

  self->angle = new_angle;
}

static gboolean
update_angle_cb (gpointer user_data)
{
  ClockButton *self;

  self = CLOCK_BUTTON (user_data);

  update_angle (self);
  self->update_angle_id = 0;

  return G_SOURCE_REMOVE;
}

static void
clock_button_dispose (GObject *object)
{
  ClockButton *self;

  self = CLOCK_BUTTON (object);

  if (self->update_angle_id != 0)
    {
      g_source_remove (self->update_angle_id);
      self->update_angle_id = 0;
    }

  G_OBJECT_CLASS (clock_button_parent_class)->dispose (object);
}

static void
clock_button_finalize (GObject *object)
{
  ClockButton *self;

  self = CLOCK_BUTTON (object);

  g_clear_pointer (&self->weather_icon, g_free);

  G_OBJECT_CLASS (clock_button_parent_class)->finalize (object);
}

static void
clock_button_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  ClockButton *self;
  GtkAllocation old_allocation;

  self = CLOCK_BUTTON (widget);

  gtk_widget_get_allocation (widget, &old_allocation);
  GTK_WIDGET_CLASS (clock_button_parent_class)->size_allocate (widget,
                                                               allocation);

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    return;

  if (old_allocation.width != allocation->width &&
      self->update_angle_id == 0)
    {
      self->update_angle_id = g_idle_add (update_angle_cb, self);
      g_source_set_name_by_id (self->update_angle_id,
                               "[clock] update_angle_cb");
    }
}

static void
clock_button_class_init (ClockButtonClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->dispose = clock_button_dispose;
  object_class->finalize = clock_button_finalize;

  widget_class->size_allocate = clock_button_size_allocate;
}

static void
clock_button_init (ClockButton *self)
{
  gtk_widget_set_name (GTK_WIDGET (self), "clock-applet-button");
  gtk_button_set_relief (GTK_BUTTON (self), GTK_RELIEF_NONE);

  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  self->position = GTK_POS_TOP;
  self->icon_size = 16;

  self->angle = 0.0;

  self->main_box = gtk_box_new (self->orientation, 12);
  gtk_container_add (GTK_CONTAINER (self), self->main_box);

  self->weather_box = gtk_box_new (self->orientation, 6);
  gtk_box_pack_start (GTK_BOX (self->main_box),
                      self->weather_box,
                      TRUE,
                      TRUE,
                      0);

  self->clock_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (self->main_box),
                      self->clock_label,
                      TRUE,
                      TRUE,
                      0);

  self->weather_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (self->weather_box),
                      self->weather_image,
                      FALSE,
                      FALSE,
                      0);

  self->temperature_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (self->weather_box),
                      self->temperature_label,
                      FALSE,
                      FALSE,
                      0);

  set_text_gravity_to_auto (self->clock_label);
  set_tnum_font_feature (self->clock_label);

  gp_add_text_color_class (self->clock_label);
  gp_add_text_color_class (self->weather_image);
  gp_add_text_color_class (self->temperature_label);

  gtk_widget_show (self->main_box);
  gtk_widget_show (self->clock_label);
}

GtkWidget *
clock_button_new (void)
{
  return g_object_new (CLOCK_TYPE_BUTTON, NULL);
}

GtkWidget *
clock_button_get_weather_box (ClockButton *self)
{
  return self->weather_box;
}

void
clock_button_set_orientation (ClockButton    *self,
                              GtkOrientation  orientation)
{
  if (self->orientation == orientation)
    return;

  self->orientation = orientation;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->main_box),
                                  self->orientation);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->weather_box),
                                  self->orientation);
}

void
clock_button_set_position (ClockButton     *self,
                           GtkPositionType  position)
{
  if (self->position == position)
    return;

  self->position = position;

  update_angle (self);
}

void
clock_button_set_icon_size (ClockButton *self,
                            guint        icon_size)
{
  if (self->icon_size == icon_size)
    return;

  self->icon_size = icon_size;

  update_weather_icon (self);
}

void
clock_button_set_clock (ClockButton *self,
                        const char  *clock)
{
  gtk_label_set_text (GTK_LABEL (self->clock_label), clock);
  update_angle (self);
}

void
clock_button_set_weather (ClockButton *self,
                          const char  *icon_name,
                          const char  *temperature)
{
  if (g_strcmp0 (self->weather_icon, icon_name) != 0)
    {
      g_free (self->weather_icon);
      self->weather_icon = g_strdup (icon_name);

      update_weather_icon (self);
    }

  set_weather_text (self, temperature);
}

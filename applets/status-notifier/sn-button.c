/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#include "sn-button.h"

struct _SnButton
{
  GtkButton       parent;

  SnItem         *item;
  GtkOrientation  orientation;

  gint            size;
  GtkWidget      *image;
};

enum
{
  PROP_0,

  PROP_ITEM,
  PROP_ORIENTATION,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (SnButton, sn_button, GTK_TYPE_BUTTON)

static gint
get_pixel_size (SnButton *button)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  guint pixel_size;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  state = gtk_style_context_get_state (context);

  gtk_style_context_get_padding (context, state, &padding);

  if (button->orientation == GTK_ORIENTATION_HORIZONTAL)
    pixel_size = button->size - padding.top - padding.bottom;
  else
    pixel_size = button->size - padding.left - padding.right;

  return MAX (pixel_size, 0);
}

static void
sn_button_update_icon (SnButton *button)
{
  const gchar *icon_theme_path;
  const gchar *icon_name;
  GtkIconSize icon_size;
  gint pixel_size;

  if (button->size <= 0)
    return;

  icon_theme_path = sn_item_get_icon_theme_path (button->item);
  if (icon_theme_path != NULL)
    gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                       icon_theme_path);

  icon_name = sn_item_get_icon_name (button->item);
  if (icon_name == NULL)
    icon_name = "image-missing";

  icon_size = GTK_ICON_SIZE_MENU;
  pixel_size = get_pixel_size (button);

  gtk_image_set_from_icon_name (GTK_IMAGE (button->image), icon_name, icon_size);
  gtk_image_set_pixel_size (GTK_IMAGE (button->image), pixel_size);
}

static void
changed_cb (SnItem   *item,
            SnButton *button)
{
  sn_button_update_icon (button);
}

static void
sn_button_constructed (GObject *object)
{
  SnButton *button;

  button = SN_BUTTON (object);

  G_OBJECT_CLASS (sn_button_parent_class)->constructed (object);

  g_signal_connect (button->item, "changed",
                    G_CALLBACK (changed_cb), button);

  changed_cb (button->item, button);
}

static void
sn_button_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  SnButton *button;

  button = SN_BUTTON (object);

  switch (property_id)
    {
      case PROP_ITEM:
        button->item = g_value_get_object (value);
        break;

      case PROP_ORIENTATION:
        button->orientation = g_value_get_enum (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

typedef enum
{
  SN_EVENT_TYPE_NONE,
  SN_EVENT_TYPE_ACTIVATE,
  SN_EVENT_TYPE_SECONDARY_ACTIVATE,
  SN_EVENT_TYPE_CONTEXT_MENU
} SnEventType;

typedef struct
{
  SnButton    *button;

  SnEventType  type;

  gint         x;
  gint         y;
} SnEventData;

static SnEventData *
sn_event_data_new (SnButton    *button,
                   SnEventType  type,
                   gint         x,
                   gint         y)
{
  SnEventData *data;

  data = g_new0 (SnEventData, 1);

  data->button = g_object_ref (button);
  data->type = type;
  data->x = x;
  data->y = y;

  return data;
}

static void
sn_event_data_free (SnEventData *data)
{
  g_object_unref (data->button);
  g_free (data);
}

static void
position_menu (GtkMenu  *menu,
               gint     *x,
               gint     *y,
               gboolean *push_in,
               gpointer  user_data)
{
  SnEventData *data;

  data = (SnEventData *) user_data;

  *x = data->x;
  *y = data->y;

  sn_event_data_free (data);
}

static gboolean
handle_event (gpointer user_data)
{
  SnEventData *data;

  data = (SnEventData *) user_data;

  if (data->type == SN_EVENT_TYPE_ACTIVATE)
    sn_item_activate (data->button->item, data->x, data->y);
  else if (data->type == SN_EVENT_TYPE_SECONDARY_ACTIVATE)
    sn_item_secondary_activate (data->button->item, data->x, data->y);
  else if (data->type == SN_EVENT_TYPE_CONTEXT_MENU)
    sn_item_context_menu (data->button->item, data->x, data->y);

  sn_event_data_free (data);

  return G_SOURCE_REMOVE;
}

static gboolean
sn_button_button_press_event (GtkWidget      *widget,
                              GdkEventButton *event)
{
  SnButton *button;
  GdkWindow *window;
  GtkWidget *toplevel;
  gint x;
  gint y;
  gint width;
  gint height;
  GtkMenu *menu;
  SnEventData *data;

  if (event->button < 1 || event->button > 3)
    return GTK_WIDGET_CLASS (sn_button_parent_class)->button_press_event (widget, event);

  button = SN_BUTTON (widget);

  window = gtk_widget_get_window (widget);
  toplevel = gtk_widget_get_toplevel (widget);

  gdk_window_get_geometry (window, &x, &y, &width, &height);
  gtk_widget_translate_coordinates (widget, toplevel, x, y, &x, &y);

  if (button->orientation == GTK_ORIENTATION_HORIZONTAL)
    y += height;
  else
    x += width;

  menu = sn_item_get_menu (button->item);

  if (menu != NULL)
    {
      data = sn_event_data_new (button, SN_EVENT_TYPE_NONE, x, y);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, position_menu, data,
                      event->button, event->time);
    }
  else
    {
      SnEventType type;

      if (event->button == 1)
        type = SN_EVENT_TYPE_ACTIVATE;
      else if (event->button == 2)
        type = SN_EVENT_TYPE_SECONDARY_ACTIVATE;
      else if (event->button == 3)
        type = SN_EVENT_TYPE_CONTEXT_MENU;
      else
        type = SN_EVENT_TYPE_NONE;

      if (type != SN_EVENT_TYPE_NONE)
        {
          data = sn_event_data_new (button, type, x, y);
          g_timeout_add (200, handle_event, data);

          return GDK_EVENT_STOP;
        }
    }

  return GTK_WIDGET_CLASS (sn_button_parent_class)->button_press_event (widget, event);;
}

static gboolean
sn_button_scroll_event (GtkWidget      *widget,
                        GdkEventScroll *event)
{
  SnButton *button;
  GdkScrollDirection direction;
  SnItemOrientation orientation;
  gdouble dx;
  gdouble dy;
  gint delta;

  button = SN_BUTTON (widget);

  if (!gdk_event_get_scroll_direction ((GdkEvent *) event, &direction))
    {
      g_assert_not_reached ();
    }
  else
    {
      switch (direction)
        {
          case GDK_SCROLL_UP:
          case GDK_SCROLL_DOWN:
            orientation = SN_ITEM_ORIENTATION_VERTICAL;
            break;

          case GDK_SCROLL_LEFT:
          case GDK_SCROLL_RIGHT:
            orientation = SN_ITEM_ORIENTATION_HORIZONTAL;
            break;

          case GDK_SCROLL_SMOOTH:
          default:
            g_assert_not_reached ();
            break;
        }
    }

  if (!gdk_event_get_scroll_deltas ((GdkEvent *) event, &dx, &dy))
    {
      switch (direction)
        {
          case GDK_SCROLL_UP:
          case GDK_SCROLL_LEFT:
            delta = 1;
            break;

          case GDK_SCROLL_DOWN:
          case GDK_SCROLL_RIGHT:
            delta = -1;
            break;

          case GDK_SCROLL_SMOOTH:
          default:
            g_assert_not_reached ();
            break;
        }
    }
  else
    {
      if (dy != 0)
        delta = (gint) dy;
      else
        delta = (gint) dx;
    }

  sn_item_scroll (button->item, delta, orientation);

  return GDK_EVENT_STOP;
}

static void
sn_button_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  SnButton *button;
  gint size;

  button = SN_BUTTON (widget);

  GTK_WIDGET_CLASS (sn_button_parent_class)->size_allocate (widget, allocation);

  if (button->orientation == GTK_ORIENTATION_HORIZONTAL)
    size = allocation->height;
  else
    size = allocation->width;

  if (button->size == size)
    return;

  button->size = size;

  sn_button_update_icon (button);
}

static void
sn_button_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (sn_button_parent_class)->style_updated (widget);
}

static void
sn_button_install_properties (GObjectClass *object_class)
{
  properties[PROP_ITEM] =
    g_param_spec_object ("item", "item", "item", SN_TYPE_ITEM,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation", "orientation", "orientation",
                       GTK_TYPE_ORIENTATION, GTK_ORIENTATION_HORIZONTAL,
                       G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
sn_button_class_init (SnButtonClass *button_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (button_class);
  widget_class = GTK_WIDGET_CLASS (button_class);

  object_class->constructed = sn_button_constructed;
  object_class->set_property = sn_button_set_property;

  widget_class->button_press_event = sn_button_button_press_event;
  widget_class->scroll_event = sn_button_scroll_event;
  widget_class->size_allocate = sn_button_size_allocate;
  widget_class->style_updated = sn_button_style_updated;

  sn_button_install_properties (object_class);

  gtk_widget_class_set_css_name (widget_class, "sn-button");
}

static void
sn_button_init (SnButton *button)
{
  button->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (button), button->image);
  gtk_widget_show (button->image);

  gtk_widget_add_events (GTK_WIDGET (button), GDK_SCROLL_MASK);
}

GtkWidget *
sn_button_new (SnItem *item)
{
  return g_object_new (SN_TYPE_BUTTON, "item", item, NULL);
}

SnItem *
sn_button_get_item (SnButton *button)
{
  return button->item;
}

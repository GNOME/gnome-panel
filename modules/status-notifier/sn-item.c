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

#include "sn-dbus-menu.h"
#include "sn-item.h"

typedef struct
{
  SnApplet       *applet;

  gchar          *bus_name;
  gchar          *object_path;

  GtkOrientation  orientation;

  GtkMenu        *menu;
} SnItemPrivate;

enum
{
  PROP_0,

  PROP_APPLET,

  PROP_BUS_NAME,
  PROP_OBJECT_PATH,

  PROP_ORIENTATION,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

enum
{
  SIGNAL_READY,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (SnItem, sn_item, GTK_TYPE_BUTTON)

static void
sn_item_dispose (GObject *object)
{
  SnItem *item;
  SnItemPrivate *priv;

  item = SN_ITEM (object);
  priv = sn_item_get_instance_private (item);

  priv->applet = NULL;

  g_clear_object (&priv->menu);

  G_OBJECT_CLASS (sn_item_parent_class)->dispose (object);
}

static void
sn_item_finalize (GObject *object)
{
  SnItem *item;
  SnItemPrivate *priv;

  item = SN_ITEM (object);
  priv = sn_item_get_instance_private (item);

  g_clear_pointer (&priv->bus_name, g_free);
  g_clear_pointer (&priv->object_path, g_free);

  G_OBJECT_CLASS (sn_item_parent_class)->finalize (object);
}

static void
sn_item_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  SnItem *item;
  SnItemPrivate *priv;

  item = SN_ITEM (object);
  priv = sn_item_get_instance_private (item);

  switch (property_id)
    {
      case PROP_APPLET:
        g_assert_not_reached ();
        break;

      case PROP_BUS_NAME:
        g_value_set_string (value, priv->bus_name);
        break;

      case PROP_OBJECT_PATH:
        g_value_set_string (value, priv->object_path);
        break;

      case PROP_ORIENTATION:
        g_value_set_enum (value, priv->orientation);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
sn_item_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  SnItem *item;
  SnItemPrivate *priv;

  item = SN_ITEM (object);
  priv = sn_item_get_instance_private (item);

  switch (property_id)
    {
      case PROP_APPLET:
        g_assert (priv->applet == NULL);
        priv->applet = g_value_get_object (value);
        break;

      case PROP_BUS_NAME:
        priv->bus_name = g_value_dup_string (value);
        break;

      case PROP_OBJECT_PATH:
        priv->object_path = g_value_dup_string (value);
        break;

      case PROP_ORIENTATION:
        priv->orientation = g_value_get_enum (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static gboolean
sn_item_scroll_event (GtkWidget      *widget,
                      GdkEventScroll *event)
{
  SnItem *item;
  GdkScrollDirection direction;
  SnItemOrientation orientation;
  gdouble dx;
  gdouble dy;
  gint delta;

  item = SN_ITEM (widget);

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

  SN_ITEM_GET_CLASS (item)->scroll (item, delta, orientation);

  return GDK_EVENT_STOP;
}

static void
sn_item_ready (SnItem *item)
{
  const gchar *menu;
  SnItemPrivate *priv;

  menu = SN_ITEM_GET_CLASS (item)->get_menu (item);
  if (menu == NULL)
    return;

  if (menu == NULL || *menu == '\0' || g_strcmp0 (menu, "/") == 0)
    return;

  priv = sn_item_get_instance_private (item);
  priv->menu = sn_dbus_menu_new (priv->applet, priv->bus_name, menu);
  g_object_ref_sink (priv->menu);
}

static void
install_properties (GObjectClass *object_class)
{
  properties[PROP_APPLET] =
    g_param_spec_object ("applet", "Applet", "Applet",
                         SN_TYPE_APPLET,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_BUS_NAME] =
    g_param_spec_string ("bus-name", "bus-name", "bus-name", NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_OBJECT_PATH] =
    g_param_spec_string ("object-path", "object-path", "object-path", NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation", "orientation", "orientation",
                       GTK_TYPE_ORIENTATION, GTK_ORIENTATION_HORIZONTAL,
                       G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
install_signals (SnItemClass *item_class)
{
  signals[SIGNAL_READY] =
    g_signal_new ("ready", G_TYPE_FROM_CLASS (item_class), G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SnItemClass, ready), NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
sn_item_class_init (SnItemClass *item_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (item_class);
  widget_class = GTK_WIDGET_CLASS (item_class);

  object_class->dispose = sn_item_dispose;
  object_class->finalize = sn_item_finalize;
  object_class->get_property = sn_item_get_property;
  object_class->set_property = sn_item_set_property;

  widget_class->scroll_event = sn_item_scroll_event;

  item_class->ready = sn_item_ready;

  install_properties (object_class);
  install_signals (item_class);
}

static void
sn_item_init (SnItem *item)
{
  gtk_widget_add_events (GTK_WIDGET (item), GDK_SCROLL_MASK);
}

const gchar *
sn_item_get_id (SnItem *item)
{
  return SN_ITEM_GET_CLASS (item)->get_id (item);
}

SnItemCategory
sn_item_get_category (SnItem *item)
{
  const gchar *string;
  SnItemCategory category;

  string = SN_ITEM_GET_CLASS (item)->get_category (item);

  if (g_strcmp0 (string, "Hardware") == 0)
    category = SN_ITEM_CATEGORY_HARDWARE;
  else if (g_strcmp0 (string, "SystemServices") == 0)
    category = SN_ITEM_CATEGORY_SYSTEM_SERVICES;
  else if (g_strcmp0 (string, "Communications") == 0)
    category = SN_ITEM_CATEGORY_COMMUNICATIONS;
  else
    category = SN_ITEM_CATEGORY_APPLICATION_STATUS;

  return category;
}

GtkMenu *
sn_item_get_menu (SnItem *item)
{
  SnItemPrivate *priv;

  priv = sn_item_get_instance_private (item);

  return priv->menu;
}

SnApplet *
sn_item_get_applet (SnItem *item)
{
  SnItemPrivate *priv;

  priv = sn_item_get_instance_private (item);

  return priv->applet;
}

const gchar *
sn_item_get_bus_name (SnItem *item)
{
  SnItemPrivate *priv;

  priv = sn_item_get_instance_private (item);

  return priv->bus_name;
}

const gchar *
sn_item_get_object_path (SnItem *item)
{
  SnItemPrivate *priv;

  priv = sn_item_get_instance_private (item);

  return priv->object_path;
}

GtkOrientation
sn_item_get_orientation (SnItem *item)
{
  SnItemPrivate *priv;

  priv = sn_item_get_instance_private (item);

  return priv->orientation;
}

void
sn_item_emit_ready (SnItem *item)
{
  g_signal_emit (item, signals[SIGNAL_READY], 0);
}

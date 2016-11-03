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

#include "sn-item.h"

typedef struct
{
  gchar *bus_name;
  gchar *object_path;
} SnItemPrivate;

enum
{
  PROP_0,

  PROP_BUS_NAME,
  PROP_OBJECT_PATH,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (SnItem, sn_item, GTK_TYPE_BUTTON)

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
      case PROP_BUS_NAME:
        g_value_set_string (value, priv->bus_name);
        break;

      case PROP_OBJECT_PATH:
        g_value_set_string (value, priv->object_path);
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
      case PROP_BUS_NAME:
        priv->bus_name = g_value_dup_string (value);
        break;

      case PROP_OBJECT_PATH:
        priv->object_path = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  properties[PROP_BUS_NAME] =
    g_param_spec_string ("bus-name", "bus-name", "bus-name", NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_OBJECT_PATH] =
    g_param_spec_string ("object-path", "object-path", "object-path", NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
sn_item_class_init (SnItemClass *item_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (item_class);

  object_class->finalize = sn_item_finalize;
  object_class->get_property = sn_item_get_property;
  object_class->set_property = sn_item_set_property;

  install_properties (object_class);
}

static void
sn_item_init (SnItem *item)
{
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

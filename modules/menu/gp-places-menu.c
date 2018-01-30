/*
 * Copyright (C) 2018 Alberts Muktupāvels
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
#include "gp-places-menu.h"

struct _GpPlacesMenu
{
  GtkMenu   parent;

  GpApplet *applet;
};

enum
{
  PROP_0,

  PROP_APPLET,

  LAST_PROP
};

static GParamSpec *menu_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpPlacesMenu, gp_places_menu, GTK_TYPE_MENU)

static void
gp_places_menu_dispose (GObject *object)
{
  GpPlacesMenu *menu;

  menu = GP_PLACES_MENU (object);

  menu->applet = NULL;

  G_OBJECT_CLASS (gp_places_menu_parent_class)->dispose (object);
}

static void
gp_places_menu_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GpPlacesMenu *menu;

  menu = GP_PLACES_MENU (object);

  switch (property_id)
    {
      case PROP_APPLET:
        g_assert (menu->applet == NULL);
        menu->applet = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  menu_properties[PROP_APPLET] =
    g_param_spec_object ("applet", "Applet", "Applet",
                         GP_TYPE_APPLET,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, menu_properties);
}

static void
gp_places_menu_class_init (GpPlacesMenuClass *menu_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (menu_class);

  object_class->dispose = gp_places_menu_dispose;
  object_class->set_property = gp_places_menu_set_property;

  install_properties (object_class);
}

static void
gp_places_menu_init (GpPlacesMenu *menu)
{
}

GtkWidget *
gp_places_menu_new (GpApplet *applet)
{
  return g_object_new (GP_TYPE_PLACES_MENU,
                       "applet", applet,
                       NULL);
}

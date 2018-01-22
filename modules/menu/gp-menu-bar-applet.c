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
#include "gp-menu-bar-applet.h"
#include "gp-menu-bar.h"

struct _GpMenuBarApplet
{
  GpApplet   parent;

  GtkWidget *menu_bar;
};

G_DEFINE_TYPE (GpMenuBarApplet, gp_menu_bar_applet, GP_TYPE_APPLET)

static void
gp_menu_bar_applet_setup (GpMenuBarApplet *menu_bar)
{
  menu_bar->menu_bar = gp_menu_bar_new ();
  gtk_container_add (GTK_CONTAINER (menu_bar), menu_bar->menu_bar);
  gtk_widget_show (menu_bar->menu_bar);

  g_object_bind_property (menu_bar, "enable-tooltips",
                          menu_bar->menu_bar, "enable-tooltips",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_object_bind_property (menu_bar, "position",
                          menu_bar->menu_bar, "position",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
}

static void
gp_menu_bar_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_menu_bar_applet_parent_class)->constructed (object);
  gp_menu_bar_applet_setup (GP_MENU_BAR_APPLET (object));
}

static void
gp_menu_bar_applet_class_init (GpMenuBarAppletClass *menu_bar_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (menu_bar_class);

  object_class->constructed = gp_menu_bar_applet_constructed;
}

static void
gp_menu_bar_applet_init (GpMenuBarApplet *menu_bar)
{
  GpApplet *applet;

  applet = GP_APPLET (menu_bar);

  gp_applet_set_flags (applet, GP_APPLET_FLAGS_EXPAND_MINOR);
}

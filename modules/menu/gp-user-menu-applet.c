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
#include "gp-menu-bar.h"
#include "gp-menu-utils.h"
#include "gp-user-menu-applet.h"
#include "gp-user-menu.h"

#include <libgnome-panel/gp-image-menu-item.h>

struct _GpUserMenuApplet
{
  GpApplet   parent;

  GtkWidget *menu_bar;
};

G_DEFINE_TYPE (GpUserMenuApplet, gp_user_menu_applet, GP_TYPE_APPLET)

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       gpointer        user_data)
{
  return TRUE;
}

static void
panel_icon_size_cb (GpApplet   *applet,
                    GParamSpec *pspec,
                    GtkWidget  *icon)
{
  guint icon_size;

  icon_size = gp_applet_get_panel_icon_size (applet);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);
}

static void
append_user_item (GpUserMenuApplet *applet)
{
  guint icon_size;
  GtkWidget *icon;
  gchar *user_name;
  GtkWidget *item;
  GtkWidget *menu;

  icon_size = gp_applet_get_panel_icon_size (GP_APPLET (applet));
  icon = gtk_image_new_from_icon_name ("computer", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);

  g_signal_connect (applet, "notify::panel-icon-size",
                    G_CALLBACK (panel_icon_size_cb), icon);

  user_name = gp_menu_utils_get_user_name ();
  item = gp_image_menu_item_new_with_label (user_name);
  g_free (user_name);

  gtk_menu_shell_append (GTK_MENU_SHELL (applet->menu_bar), item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_widget_show (item);

  menu = gp_user_menu_new (GP_APPLET (applet));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

  g_signal_connect (menu, "button-press-event",
                    G_CALLBACK (button_press_event_cb), NULL);
}

static void
gp_user_menu_applet_setup (GpUserMenuApplet *user_menu)
{
  user_menu->menu_bar = gp_menu_bar_new ();
  gtk_container_add (GTK_CONTAINER (user_menu), user_menu->menu_bar);
  gtk_widget_show (user_menu->menu_bar);

  g_object_bind_property (user_menu, "enable-tooltips",
                          user_menu->menu_bar, "enable-tooltips",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_object_bind_property (user_menu, "position",
                          user_menu->menu_bar, "position",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  append_user_item (user_menu);
}

static void
gp_user_menu_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_user_menu_applet_parent_class)->constructed (object);
  gp_user_menu_applet_setup (GP_USER_MENU_APPLET (object));
}

static void
gp_user_menu_applet_class_init (GpUserMenuAppletClass *user_menu_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (user_menu_class);

  object_class->constructed = gp_user_menu_applet_constructed;
}

static void
gp_user_menu_applet_init (GpUserMenuApplet *user_menu)
{
  GpApplet *applet;

  applet = GP_APPLET (user_menu);

  gp_applet_set_flags (applet, GP_APPLET_FLAGS_EXPAND_MINOR);
}

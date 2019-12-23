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
#include "gp-lock-logout.h"
#include "gp-menu-bar.h"
#include "gp-menu-utils.h"
#include "gp-user-menu-applet.h"
#include "gp-user-menu.h"

#include <libgnome-panel/gp-image-menu-item.h>
#include <libgnome-panel/gp-utils.h>

struct _GpUserMenuApplet
{
  GpApplet      parent;

  GtkWidget    *menu_bar;

  GpLockLogout *lock_logout;
};

G_DEFINE_TYPE (GpUserMenuApplet, gp_user_menu_applet, GP_TYPE_APPLET)

static void
append_lock_logout (GtkMenu          *menu,
                    GpUserMenuApplet *applet)
{
  gp_lock_logout_append_to_menu (applet->lock_logout, menu);
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       gpointer        user_data)
{
  return TRUE;
}

static void
update_icon (GpApplet  *applet,
             GtkWidget *icon)
{
  const char *icon_name;
  guint icon_size;

  icon_name = "computer";
  if (gp_applet_get_prefer_symbolic_icons (applet))
    icon_name = "computer-symbolic";

  icon_size = gp_applet_get_panel_icon_size (applet);

  gtk_image_set_from_icon_name (GTK_IMAGE (icon), icon_name, GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);
}

static void
prefer_symbolic_icons_cb (GpApplet   *applet,
                          GParamSpec *pspec,
                          GtkWidget  *icon)
{
  update_icon (applet, icon);
}

static void
panel_icon_size_cb (GpApplet   *applet,
                    GParamSpec *pspec,
                    GtkWidget  *icon)
{
  update_icon (applet, icon);
}

static void
append_user_item (GpUserMenuApplet *applet)
{
  GtkWidget *icon;
  gchar *user_name;
  GtkWidget *item;
  GtkWidget *menu;

  icon = gtk_image_new ();
  gp_add_text_color_class (icon);

  g_signal_connect (applet, "notify::prefer-symbolic-icons",
                    G_CALLBACK (prefer_symbolic_icons_cb), icon);

  g_signal_connect (applet, "notify::panel-icon-size",
                    G_CALLBACK (panel_icon_size_cb), icon);

  update_icon (GP_APPLET (applet), icon);

  user_name = gp_menu_utils_get_user_name ();
  item = gp_image_menu_item_new_with_label (user_name);
  g_free (user_name);

  gtk_menu_shell_append (GTK_MENU_SHELL (applet->menu_bar), item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_widget_show (item);

  menu = gp_user_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

  g_object_bind_property (applet, "enable-tooltips",
                          menu, "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "locked-down",
                          menu, "locked-down",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "menu-icon-size",
                          menu, "menu-icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  applet->lock_logout = gp_lock_logout_new ();

  g_object_bind_property (applet, "enable-tooltips",
                          applet->lock_logout, "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "locked-down",
                          applet->lock_logout, "locked-down",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "menu-icon-size",
                          applet->lock_logout, "menu-icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_swapped (applet->lock_logout, "changed",
                            G_CALLBACK (gp_user_menu_reload), menu);

  gp_user_menu_set_append_func (GP_USER_MENU (menu),
                                (GpAppendMenuItemsFunc) append_lock_logout,
                                applet);

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
gp_user_menu_applet_dispose (GObject *object)
{
  GpUserMenuApplet *user_menu;

  user_menu = GP_USER_MENU_APPLET (object);

  g_clear_object (&user_menu->lock_logout);

  G_OBJECT_CLASS (gp_user_menu_applet_parent_class)->dispose (object);
}

static void
gp_user_menu_applet_class_init (GpUserMenuAppletClass *user_menu_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (user_menu_class);

  object_class->constructed = gp_user_menu_applet_constructed;
  object_class->dispose = gp_user_menu_applet_dispose;
}

static void
gp_user_menu_applet_init (GpUserMenuApplet *user_menu)
{
  GpApplet *applet;

  applet = GP_APPLET (user_menu);

  gp_applet_set_flags (applet, GP_APPLET_FLAGS_EXPAND_MINOR);
}

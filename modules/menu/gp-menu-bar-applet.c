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

#include <glib/gi18n-lib.h>
#include <libgnome-panel/gp-image-menu-item.h>

#include "gp-menu-bar-applet.h"
#include "gp-menu-bar.h"
#include "gp-menu.h"

struct _GpMenuBarApplet
{
  GpApplet   parent;

  GtkWidget *menu_bar;

  GtkWidget *applications_item;
  GtkWidget *applications_menu;
};

G_DEFINE_TYPE (GpMenuBarApplet, gp_menu_bar_applet, GP_TYPE_APPLET)

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       gpointer        user_data)
{
  return TRUE;
}

static gchar *
get_applications_menu (void)
{
  const gchar *xdg_menu_prefx;

  xdg_menu_prefx = g_getenv ("XDG_MENU_PREFIX");
  if (!xdg_menu_prefx || *xdg_menu_prefx == '\0')
    return g_strdup ("gnome-applications.menu");

  return g_strdup_printf ("%sapplications.menu", xdg_menu_prefx);
}

static void
append_applications_item (GpMenuBarApplet *applet)
{
  GtkWidget *icon;
  const gchar *tooltip;
  gchar *menu;

  icon = gtk_image_new_from_icon_name ("start-here", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), 16);

  applet->applications_item = gp_image_menu_item_new_with_label (_("Applications"));
  gtk_menu_shell_append (GTK_MENU_SHELL (applet->menu_bar), applet->applications_item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (applet->applications_item), icon);
  gtk_widget_show (applet->applications_item);

  tooltip = _("Browse and run installed applications");
  gtk_widget_set_tooltip_text (applet->applications_item, tooltip);

  menu = get_applications_menu ();

  applet->applications_menu = gp_menu_new_from_name (menu);
  g_free (menu);

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (applet->applications_item),
                             applet->applications_menu);

  g_signal_connect (applet->applications_menu, "button-press-event",
                    G_CALLBACK (button_press_event_cb), NULL);
}

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

  append_applications_item (menu_bar);
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

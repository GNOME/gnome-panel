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
#include <libgnome-panel/gp-action.h>
#include <libgnome-panel/gp-image-menu-item.h>
#include <libgnome-panel/gp-utils.h>

#include "gp-menu-bar-applet.h"
#include "gp-menu-bar.h"
#include "gp-menu-utils.h"
#include "gp-menu.h"
#include "gp-places-menu.h"

#define RESOURCE_PATH "/org/gnome/gnome-panel/modules/menu"

struct _GpMenuBarApplet
{
  GpApplet   parent;

  GtkWidget *menu_bar;

  GtkWidget *applications_item;
  GtkWidget *applications_menu;

  GtkWidget *places_item;
  GtkWidget *places_menu;

  GtkWidget *system_item;
  GtkWidget *system_menu;
};

static void gp_action_interface_init (GpActionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GpMenuBarApplet, gp_menu_bar_applet, GP_TYPE_APPLET,
                         G_IMPLEMENT_INTERFACE (GP_TYPE_ACTION, gp_action_interface_init))

static gboolean
gp_menu_bar_applet_handle_action (GpAction     *action,
                                  GpActionFlags flags,
                                  uint32_t      time)
{
  GpMenuBarApplet *applet;

  if ((flags & GP_ACTION_MAIN_MENU) != GP_ACTION_MAIN_MENU)
    return FALSE;

  applet = GP_MENU_BAR_APPLET (action);

  gtk_menu_shell_select_item (GTK_MENU_SHELL (applet->menu_bar),
                              applet->applications_item);

  return TRUE;
}

static void
gp_action_interface_init (GpActionInterface *iface)
{
  iface->handle_action = gp_menu_bar_applet_handle_action;
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       gpointer        user_data)
{
  if (event->button == 3)
    return TRUE;

  return FALSE;
}

static gchar *
get_settings_menu (void)
{
  const gchar *xdg_menu_prefx;

  xdg_menu_prefx = g_getenv ("XDG_MENU_PREFIX");
  if (!xdg_menu_prefx || *xdg_menu_prefx == '\0')
    return NULL;

  return g_strdup_printf ("%ssettings.menu", xdg_menu_prefx);
}

static void
update_icon (GpApplet  *applet,
             GtkWidget *icon)
{
  const char *icon_name;
  guint icon_size;

  icon_name = "start-here";
  if (gp_applet_get_prefer_symbolic_icons (applet))
    icon_name = "start-here-symbolic";

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
append_applications_item (GpMenuBarApplet *applet)
{
  GtkWidget *icon;
  const gchar *tooltip;
  gchar *menu;

  icon = gtk_image_new ();
  gp_add_text_color_class (icon);

  g_signal_connect (applet, "notify::prefer-symbolic-icons",
                    G_CALLBACK (prefer_symbolic_icons_cb), icon);

  g_signal_connect (applet, "notify::panel-icon-size",
                    G_CALLBACK (panel_icon_size_cb), icon);

  update_icon (GP_APPLET (applet), icon);

  applet->applications_item = gp_image_menu_item_new_with_label (_("Applications"));
  gtk_menu_shell_append (GTK_MENU_SHELL (applet->menu_bar), applet->applications_item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (applet->applications_item), icon);
  gtk_widget_show (applet->applications_item);

  tooltip = _("Browse and run installed applications");
  gtk_widget_set_tooltip_text (applet->applications_item, tooltip);

  menu = gp_menu_utils_get_applications_menu ();

  applet->applications_menu = gp_menu_new (menu, TRUE);
  g_free (menu);

  g_object_bind_property (applet, "enable-tooltips",
                          applet->applications_menu, "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "locked-down",
                          applet->applications_menu, "locked-down",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "menu-icon-size",
                          applet->applications_menu, "menu-icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (applet->applications_item),
                             applet->applications_menu);

  g_signal_connect (applet->applications_menu, "button-press-event",
                    G_CALLBACK (button_press_event_cb), NULL);
}

static void
append_places_item (GpMenuBarApplet *applet)
{
  const gchar *tooltip;

  applet->places_item = gtk_menu_item_new_with_label (_("Places"));
  gtk_menu_shell_append (GTK_MENU_SHELL (applet->menu_bar), applet->places_item);
  gtk_widget_show (applet->places_item);

  tooltip = _("Access documents, folders and network places");
  gtk_widget_set_tooltip_text (applet->places_item, tooltip);

  applet->places_menu = gp_places_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (applet->places_item),
                             applet->places_menu);

  g_object_bind_property (applet, "enable-tooltips",
                          applet->places_menu, "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "locked-down",
                          applet->places_menu, "locked-down",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "menu-icon-size",
                          applet->places_menu, "menu-icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_signal_connect (applet->places_menu, "button-press-event",
                    G_CALLBACK (button_press_event_cb), NULL);
}

static void
append_system_item (GpMenuBarApplet *applet)
{
  gchar *menu;
  const gchar *tooltip;

  menu = get_settings_menu ();
  if (menu == NULL)
    return;

  applet->system_item = gtk_menu_item_new_with_label (_("System"));
  gtk_menu_shell_append (GTK_MENU_SHELL (applet->menu_bar), applet->system_item);
  gtk_widget_show (applet->system_item);

  tooltip = _("Change system appearance and behavior, or get help");
  gtk_widget_set_tooltip_text (applet->system_item, tooltip);

  applet->system_menu = gp_menu_new (menu, FALSE);
  g_free (menu);

  g_object_bind_property (applet, "enable-tooltips",
                          applet->system_menu, "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "locked-down",
                          applet->system_menu, "locked-down",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (applet, "menu-icon-size",
                          applet->system_menu, "menu-icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (applet->system_item),
                             applet->system_menu);

  g_object_bind_property (applet->system_menu, "empty",
                          applet->system_item, "visible",
                          G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN |
                          G_BINDING_SYNC_CREATE);

  g_signal_connect (applet->system_menu, "button-press-event",
                    G_CALLBACK (button_press_event_cb), NULL);
}

static void
edit_menus_cb (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  const gchar *application;
  GDesktopAppInfo *app_info;

  application = "alacarte.desktop";
  app_info = g_desktop_app_info_new (application);

  if (app_info == NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (NULL, 0,
                                       GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                       _("Please install the '%s' application."),
                                       application);

      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_window_present (GTK_WINDOW (dialog));

      return;
    }

  gp_menu_utils_app_info_launch (app_info);
}

static const GActionEntry menu_bar_menu_actions[] =
  {
    { "edit-menus", edit_menus_cb, NULL, NULL, NULL },
    { NULL }
  };

static void
setup_menu (GpMenuBarApplet *menu_bar)
{
  GpApplet *applet;
  const gchar *resource;
  GAction *action;
  GBindingFlags flags;

  applet = GP_APPLET (menu_bar);

  resource = RESOURCE_PATH "/menu-bar-menu.ui";
  gp_applet_setup_menu_from_resource (applet, resource, menu_bar_menu_actions);

  action = gp_applet_menu_lookup_action (applet, "edit-menus");
  flags = G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE;

  g_object_bind_property (applet, "locked-down", action, "enabled", flags);
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
  append_places_item (menu_bar);
  append_system_item (menu_bar);

  setup_menu (menu_bar);
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

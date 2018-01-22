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

#include "gp-main-menu-applet.h"
#include "gp-menu-utils.h"

#define RESOURCE_PATH "/org/gnome/gnome-panel/modules/menu"

struct _GpMainMenuApplet
{
  GpApplet parent;
};

static void gp_action_interface_init (GpActionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GpMainMenuApplet, gp_main_menu_applet, GP_TYPE_APPLET,
                         G_IMPLEMENT_INTERFACE (GP_TYPE_ACTION, gp_action_interface_init))

static gboolean
gp_main_menu_applet_main_menu (GpAction *action,
                               guint32   time)
{
  return FALSE;
}

static void
gp_action_interface_init (GpActionInterface *iface)
{
  iface->main_menu = gp_main_menu_applet_main_menu;
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

  gp_menu_launch_app_info (app_info);
}

static const GActionEntry main_menu_menu_actions[] =
  {
    { "edit-menus", edit_menus_cb, NULL, NULL, NULL },
    { NULL }
  };

static void
setup_menu (GpMainMenuApplet *main_menu)
{
  GpApplet *applet;
  const gchar *resource;
  GAction *action;
  GBindingFlags flags;

  applet = GP_APPLET (main_menu);

  resource = RESOURCE_PATH "/main-menu-menu.ui";
  gp_applet_setup_menu_from_resource (applet, resource, main_menu_menu_actions);

  action = gp_applet_menu_lookup_action (applet, "edit-menus");
  flags = G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE;

  g_object_bind_property (applet, "locked-down", action, "enabled", flags);
}

static void
gp_main_menu_applet_setup (GpMainMenuApplet *main_menu)
{
  setup_menu (main_menu);
}

static void
gp_main_menu_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_main_menu_applet_parent_class)->constructed (object);
  gp_main_menu_applet_setup (GP_MAIN_MENU_APPLET (object));
}

static void
gp_main_menu_applet_class_init (GpMainMenuAppletClass *main_menu_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (main_menu_class);

  object_class->constructed = gp_main_menu_applet_constructed;
}

static void
gp_main_menu_applet_init (GpMainMenuApplet *main_menu)
{
  GpApplet *applet;

  applet = GP_APPLET (main_menu);

  gp_applet_set_flags (applet, GP_APPLET_FLAGS_EXPAND_MINOR);
}

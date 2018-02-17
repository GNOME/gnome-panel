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
#include "gp-user-menu.h"
#include "gp-menu-utils.h"

#include <libgnome-panel/gp-image-menu-item.h>

struct _GpUserMenu
{
  GtkMenu   parent;

  GpApplet *applet;

  gboolean  empty;

  guint     reload_id;

  gulong    locked_down_id;
  gulong    menu_icon_size_id;
};

enum
{
  PROP_0,

  PROP_APPLET,
  PROP_EMPTY,

  LAST_PROP
};

static GParamSpec *menu_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpUserMenu, gp_user_menu, GTK_TYPE_MENU)

static void
activate_cb (GtkWidget       *item,
             GDesktopAppInfo *info)
{
  gp_menu_utils_app_info_launch (info);
}

static void
drag_data_get_cb (GtkWidget        *widget,
                  GdkDragContext   *context,
                  GtkSelectionData *selection_data,
                  guint             info,
                  guint             time,
                  GDesktopAppInfo  *app_info)
{
  const gchar *filename;
  gchar *uris[2];

  filename = g_desktop_app_info_get_filename (app_info);
  if (filename == NULL)
    return;

  uris[0] = g_filename_to_uri (filename, NULL, NULL);
  uris[1] = NULL;

  gtk_selection_data_set_uris (selection_data, uris);
  g_free (uris[0]);
}

static void
append_control_center (GpUserMenu *menu)
{
  GDesktopAppInfo *info;
  const gchar *name;
  const gchar *description;
  GIcon *icon;
  GtkWidget *item;

  info = g_desktop_app_info_new ("gnome-control-center.desktop");
  if (info == NULL)
    return;

  name = g_app_info_get_display_name (G_APP_INFO (info));
  description = g_app_info_get_description (G_APP_INFO (info));
  icon = g_app_info_get_icon (G_APP_INFO (info));

  if (description == NULL)
    description = g_desktop_app_info_get_generic_name (info);

  item = gp_image_menu_item_new_with_label (name);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  if (icon != NULL)
    {
      guint icon_size;
      GtkWidget *image;

      icon_size = gp_applet_get_menu_icon_size (menu->applet);
      image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (GTK_IMAGE (image), icon_size);

      gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), image);
    }

  if (description != NULL)
    {
      gtk_widget_set_tooltip_text (item, description);

      g_object_bind_property (menu->applet, "enable-tooltips",
                              item, "has-tooltip",
                              G_BINDING_DEFAULT |
                              G_BINDING_SYNC_CREATE);
    }

  if (!gp_applet_get_locked_down (menu->applet))
    {
      static const GtkTargetEntry drag_targets[] =
        {
          { (gchar *) "text/uri-list", 0, 0 }
        };

      gtk_drag_source_set (item, GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
                           drag_targets, G_N_ELEMENTS (drag_targets),
                           GDK_ACTION_COPY);

      if (icon != NULL)
        gtk_drag_source_set_icon_gicon (item, icon);

      g_signal_connect_data (item, "drag-data-get",
                             G_CALLBACK (drag_data_get_cb),
                             g_object_ref (info),
                             (GClosureNotify) g_object_unref,
                             0);
    }

  g_signal_connect_data (item, "activate",
                         G_CALLBACK (activate_cb),
                         g_object_ref (info),
                         (GClosureNotify) g_object_unref,
                         0);

  g_object_unref (info);
}

static void
remove_item (GtkWidget *widget,
             gpointer   user_data)
{
  gtk_widget_destroy (widget);
}

static void
count_visible_item (GtkWidget *widget,
                    gpointer   user_data)
{
  gint *count = user_data;

  if (gtk_widget_is_visible (widget))
    (*count)++;
}

static void
menu_reload (GpUserMenu *menu)
{
  gint count;
  gboolean empty;

  gtk_container_foreach (GTK_CONTAINER (menu), remove_item, NULL);

  append_control_center (menu);

  count = 0;
  gtk_container_foreach (GTK_CONTAINER (menu), count_visible_item, &count);
  empty = count == 0;

  if (menu->empty == empty)
    return;

  menu->empty = empty;

  g_object_notify_by_pspec (G_OBJECT (menu), menu_properties[PROP_EMPTY]);
}

static gboolean
reload_cb (gpointer user_data)
{
  GpUserMenu *menu;

  menu = GP_USER_MENU (user_data);

  menu_reload (menu);
  menu->reload_id = 0;

  return G_SOURCE_REMOVE;
}

static void
queue_reload (GpUserMenu *menu)
{
  if (menu->reload_id != 0)
    return;

  menu->reload_id = g_timeout_add_full (G_PRIORITY_LOW, 200,
                                        reload_cb, menu,
                                        NULL);

  g_source_set_name_by_id (menu->reload_id, "[menu] reload_cb");
}

static void
locked_down_cb (GpApplet   *applet,
                GParamSpec *pspec,
                GpUserMenu *menu)
{
  queue_reload (menu);
}

static void
menu_icon_size_cb (GpApplet   *applet,
                   GParamSpec *pspec,
                   GpUserMenu *menu)
{
  queue_reload (menu);
}

static void
gp_user_menu_constructed (GObject *object)
{
  GpUserMenu *menu;

  menu = GP_USER_MENU (object);

  G_OBJECT_CLASS (gp_user_menu_parent_class)->constructed (object);

  menu->locked_down_id = g_signal_connect (menu->applet, "notify::locked-down",
                                           G_CALLBACK (locked_down_cb), menu);

  menu->menu_icon_size_id = g_signal_connect (menu->applet,
                                              "notify::menu-icon-size",
                                              G_CALLBACK (menu_icon_size_cb),
                                              menu);

  queue_reload (menu);
}

static void
gp_user_menu_dispose (GObject *object)
{
  GpUserMenu *menu;

  menu = GP_USER_MENU (object);

  if (menu->reload_id != 0)
    {
      g_source_remove (menu->reload_id);
      menu->reload_id = 0;
    }

  if (menu->locked_down_id != 0)
    {
      g_signal_handler_disconnect (menu->applet, menu->locked_down_id);
      menu->locked_down_id = 0;
    }

  if (menu->menu_icon_size_id != 0)
    {
      g_signal_handler_disconnect (menu->applet, menu->menu_icon_size_id);
      menu->menu_icon_size_id = 0;
    }

  menu->applet = NULL;

  G_OBJECT_CLASS (gp_user_menu_parent_class)->dispose (object);
}

static void
gp_user_menu_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GpUserMenu *menu;

  menu = GP_USER_MENU (object);

  switch (property_id)
    {
      case PROP_APPLET:
        g_assert_not_reached ();
        break;

      case PROP_EMPTY:
        g_value_set_boolean (value, menu->empty);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gp_user_menu_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GpUserMenu *menu;

  menu = GP_USER_MENU (object);

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

  menu_properties[PROP_EMPTY] =
    g_param_spec_boolean ("empty", "Empty", "Empty",
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, menu_properties);
}

static void
gp_user_menu_class_init (GpUserMenuClass *menu_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (menu_class);

  object_class->constructed = gp_user_menu_constructed;
  object_class->dispose = gp_user_menu_dispose;
  object_class->get_property = gp_user_menu_get_property;
  object_class->set_property = gp_user_menu_set_property;

  install_properties (object_class);
}

static void
gp_user_menu_init (GpUserMenu *menu)
{
}

GtkWidget *
gp_user_menu_new (GpApplet *applet)
{
  return g_object_new (GP_TYPE_USER_MENU,
                       "applet", applet,
                       NULL);
}

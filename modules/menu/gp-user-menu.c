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
  GtkMenu                parent;

  gboolean               enable_tooltips;
  gboolean               locked_down;
  guint                  menu_icon_size;

  gboolean               empty;

  GpAppendMenuItemsFunc  append_func;
  gpointer               append_data;

  guint                  reload_id;
};

enum
{
  PROP_0,

  PROP_ENABLE_TOOLTIPS,
  PROP_LOCKED_DOWN,
  PROP_MENU_ICON_SIZE,

  PROP_EMPTY,

  LAST_PROP
};

static GParamSpec *menu_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpUserMenu, gp_user_menu, GTK_TYPE_MENU)

static void
unref_object (GObject  *object,
              GClosure *closure)
{
  g_object_unref (object);
}

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

  info = g_desktop_app_info_new ("org.gnome.Settings.desktop");
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
      GtkWidget *image;

      image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (GTK_IMAGE (image), menu->menu_icon_size);

      gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), image);
    }

  if (description != NULL)
    {
      gtk_widget_set_tooltip_text (item, description);

      g_object_bind_property (menu, "enable-tooltips",
                              item, "has-tooltip",
                              G_BINDING_DEFAULT |
                              G_BINDING_SYNC_CREATE);
    }

  if (!menu->locked_down)
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
                             (GClosureNotify) unref_object,
                             0);
    }

  g_signal_connect_data (item, "activate",
                         G_CALLBACK (activate_cb),
                         g_object_ref (info),
                         (GClosureNotify) unref_object,
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
menu_reload (GpUserMenu *menu)
{
  GList *children;
  gboolean empty;

  gtk_container_foreach (GTK_CONTAINER (menu), remove_item, NULL);

  append_control_center (menu);

  if (menu->append_func != NULL)
    menu->append_func (GTK_MENU (menu), menu->append_data);

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  empty = g_list_length (children) == 0;
  g_list_free (children);

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
gp_user_menu_constructed (GObject *object)
{
  GpUserMenu *menu;

  menu = GP_USER_MENU (object);

  G_OBJECT_CLASS (gp_user_menu_parent_class)->constructed (object);

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
      case PROP_LOCKED_DOWN:
      case PROP_MENU_ICON_SIZE:
        g_assert_not_reached ();
        break;

      case PROP_ENABLE_TOOLTIPS:
        g_value_set_boolean (value, menu->enable_tooltips);
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
set_enable_tooltips (GpUserMenu *menu,
                     gboolean    enable_tooltips)
{
  if (menu->enable_tooltips == enable_tooltips)
    return;

  menu->enable_tooltips = enable_tooltips;

  g_object_notify_by_pspec (G_OBJECT (menu),
                            menu_properties[PROP_ENABLE_TOOLTIPS]);
}

static void
set_locked_down (GpUserMenu *menu,
                 gboolean   locked_down)
{
  if (menu->locked_down == locked_down)
    return;

  menu->locked_down = locked_down;
  queue_reload (menu);
}

static void
set_menu_icon_size (GpUserMenu *menu,
                    guint       menu_icon_size)
{
  if (menu->menu_icon_size == menu_icon_size)
    return;

  menu->menu_icon_size = menu_icon_size;
  queue_reload (menu);
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

      case PROP_ENABLE_TOOLTIPS:
        set_enable_tooltips (menu, g_value_get_boolean (value));
        break;

      case PROP_LOCKED_DOWN:
        set_locked_down (menu, g_value_get_boolean (value));
        break;

      case PROP_MENU_ICON_SIZE:
        set_menu_icon_size (menu, g_value_get_uint (value));
        break;

      case PROP_EMPTY:
        g_assert_not_reached ();
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  menu_properties[PROP_ENABLE_TOOLTIPS] =
    g_param_spec_boolean ("enable-tooltips", "Enable Tooltips", "Enable Tooltips",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  menu_properties[PROP_LOCKED_DOWN] =
    g_param_spec_boolean ("locked-down", "Locked Down", "Locked Down",
                          FALSE,
                          G_PARAM_CONSTRUCT | G_PARAM_WRITABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  menu_properties[PROP_MENU_ICON_SIZE] =
    g_param_spec_uint ("menu-icon-size", "Menu Icon Size", "Menu Icon Size",
                       16, 24, 16,
                       G_PARAM_CONSTRUCT | G_PARAM_WRITABLE |
                       G_PARAM_EXPLICIT_NOTIFY |
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
gp_user_menu_new (void)
{
  return g_object_new (GP_TYPE_USER_MENU,
                       NULL);
}

void
gp_user_menu_set_append_func (GpUserMenu            *user_menu,
                              GpAppendMenuItemsFunc  append_func,
                              gpointer               user_data)
{
  user_menu->append_func = append_func;
  user_menu->append_data = user_data;

  queue_reload (user_menu);
}

void
gp_user_menu_reload (GpUserMenu *user_menu)
{
  menu_reload (user_menu);
}

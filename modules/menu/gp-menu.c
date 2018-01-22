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

#include <gio/gdesktopappinfo.h>
#include <gmenu-tree.h>
#include <libgnome-panel/gp-image-menu-item.h>

#include "gp-menu-utils.h"
#include "gp-menu.h"

struct _GpMenu
{
  GtkMenu    parent;

  gchar     *name;
  GpApplet  *applet;

  GMenuTree *tree;

  guint      load_id;
};

enum
{
  PROP_0,

  PROP_NAME,
  PROP_APPLET,

  LAST_PROP
};

static GParamSpec *menu_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpMenu, gp_menu, GTK_TYPE_MENU)

static void directory_to_menu_items (GMenuTreeDirectory *directory,
                                     GtkWidget          *widget,
                                     GpMenu             *menu);

static void
activate_cb (GtkWidget       *item,
             GDesktopAppInfo *info)
{
  gp_menu_launch_app_info (info);
}

static void
append_directory (GtkMenuShell  *shell,
                  GMenuTreeIter *iter,
                  GpMenu        *menu)
{
  GMenuTreeDirectory *directory;
  GtkWidget *submenu;
  const gchar *name;
  GIcon *icon;
  GtkWidget *item;

  directory = gmenu_tree_iter_get_directory (iter);
  submenu = gtk_menu_new ();

  directory_to_menu_items (directory, submenu, menu);

  name = gmenu_tree_directory_get_name (directory);
  icon = gmenu_tree_directory_get_icon (directory);
  gmenu_tree_item_unref (directory);

  item = gp_image_menu_item_new_with_label (name);
  gtk_menu_shell_append (shell, item);
  gtk_widget_show (item);

  if (icon != NULL)
    {
      GtkWidget *image;

      image = gtk_image_new ();

      gtk_image_set_from_gicon (GTK_IMAGE (image), icon, GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

      gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), image);
    }

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
}

static void
append_entry (GtkMenuShell  *shell,
              GMenuTreeIter *iter,
              GpMenu        *menu)
{
  GMenuTreeEntry *entry;
  GDesktopAppInfo *info;
  const gchar *name;
  const gchar *description;
  GIcon *icon;
  GtkWidget *item;

  entry = gmenu_tree_iter_get_entry (iter);
  info = gmenu_tree_entry_get_app_info (entry);
  gmenu_tree_item_unref (entry);

  name = g_app_info_get_display_name (G_APP_INFO (info));
  description = g_app_info_get_description (G_APP_INFO (info));
  icon = g_app_info_get_icon (G_APP_INFO (info));

  item = gp_image_menu_item_new_with_label (name);
  gtk_menu_shell_append (shell, item);
  gtk_widget_show (item);

  if (icon != NULL)
    {
      GtkWidget *image;

      image = gtk_image_new ();

      gtk_image_set_from_gicon (GTK_IMAGE (image), icon, GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

      gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), image);
    }

  if (description == NULL)
    description = g_desktop_app_info_get_generic_name (info);

  if (description != NULL)
    {
      gtk_widget_set_tooltip_text (item, description);

      g_object_bind_property (menu->applet, "enable-tooltips",
                              item, "has-tooltip",
                              G_BINDING_DEFAULT |
                              G_BINDING_SYNC_CREATE);
    }

  g_signal_connect (item, "activate", G_CALLBACK (activate_cb), info);
}

static void
append_separator (GtkMenuShell *shell)
{
  GtkWidget *item;

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (shell, item);
  gtk_widget_show (item);

  gtk_widget_set_sensitive (item, FALSE);
}

static void
directory_to_menu_items (GMenuTreeDirectory *directory,
                         GtkWidget          *widget,
                         GpMenu             *menu)
{
  GMenuTreeIter *iter;
  GMenuTreeItemType next_type;

  iter = gmenu_tree_directory_iter (directory);
  next_type = gmenu_tree_iter_next (iter);

  while (next_type != GMENU_TREE_ITEM_INVALID)
    {
      switch (next_type)
        {
          case GMENU_TREE_ITEM_DIRECTORY:
            append_directory (GTK_MENU_SHELL (widget), iter, menu);
            break;

          case GMENU_TREE_ITEM_ENTRY:
            append_entry (GTK_MENU_SHELL (widget), iter, menu);
            break;

          case GMENU_TREE_ITEM_SEPARATOR:
            append_separator (GTK_MENU_SHELL (widget));
            break;

          case GMENU_TREE_ITEM_ALIAS:
            break;

          case GMENU_TREE_ITEM_HEADER:
            break;

          case GMENU_TREE_ITEM_INVALID:
          default:
            g_assert_not_reached ();
            break;
        }

      next_type = gmenu_tree_iter_next (iter);
    }

  gmenu_tree_iter_unref (iter);
}

static void
remove_item (GtkWidget *widget,
             gpointer   user_data)
{
  gtk_widget_destroy (widget);
}

static void
menu_tree_load (GpMenu *menu)
{
  GError *error;
  GMenuTreeDirectory *directory;

  gtk_container_foreach (GTK_CONTAINER (menu), remove_item, NULL);

  error = NULL;
  if (!gmenu_tree_load_sync (menu->tree, &error))
    {
      g_warning ("Failed to load menu: %s", error->message);
      g_clear_error (&error);

      return;
    }

  directory = gmenu_tree_get_directory_from_path (menu->tree, "/");
  directory_to_menu_items (directory, GTK_WIDGET (menu), menu);
  gmenu_tree_item_unref (directory);
}

static gboolean
menu_tree_load_cb (gpointer user_data)
{
  GpMenu *menu;

  menu = GP_MENU (user_data);

  menu_tree_load (menu);
  menu->load_id = 0;

  return G_SOURCE_REMOVE;
}

static void
menu_tree_changed_cb (GMenuTree *tree,
                      GpMenu    *menu)
{
  if (menu->load_id != 0)
    return;

  menu->load_id = g_timeout_add_full (G_PRIORITY_LOW, 200,
                                      menu_tree_load_cb, menu,
                                      NULL);

  g_source_set_name_by_id (menu->load_id, "[menu] menu_tree_load_cb");
}

static void
gp_menu_constructed (GObject *object)
{
  GpMenu *menu;
  GMenuTreeFlags flags;

  menu = GP_MENU (object);

  G_OBJECT_CLASS (gp_menu_parent_class)->constructed (object);

  flags = GMENU_TREE_FLAGS_SORT_DISPLAY_NAME;
  menu->tree = gmenu_tree_new (menu->name, flags);

  g_signal_connect (menu->tree, "changed",
                    G_CALLBACK (menu_tree_changed_cb), menu);

  menu_tree_changed_cb (menu->tree, menu);
}

static void
gp_menu_dispose (GObject *object)
{
  GpMenu *menu;

  menu = GP_MENU (object);

  menu->applet = NULL;

  g_clear_object (&menu->tree);

  if (menu->load_id != 0)
    {
      g_source_remove (menu->load_id);
      menu->load_id = 0;
    }

  G_OBJECT_CLASS (gp_menu_parent_class)->dispose (object);
}

static void
gp_menu_finalize (GObject *object)
{
  GpMenu *menu;

  menu = GP_MENU (object);

  g_clear_pointer (&menu->name, g_free);

  G_OBJECT_CLASS (gp_menu_parent_class)->finalize (object);
}

static void
gp_menu_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  switch (property_id)
    {
      case PROP_NAME:
        g_assert_not_reached ();
        break;

      case PROP_APPLET:
        g_assert_not_reached ();
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gp_menu_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GpMenu *menu;

  menu = GP_MENU (object);

  switch (property_id)
    {
      case PROP_NAME:
        g_assert (menu->name == NULL);
        menu->name = g_value_dup_string (value);
        break;

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
  menu_properties[PROP_NAME] =
    g_param_spec_string ("name", "Name", "Name",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  menu_properties[PROP_APPLET] =
    g_param_spec_object ("applet", "Applet", "Applet",
                         GP_TYPE_APPLET,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, menu_properties);
}

static void
gp_menu_class_init (GpMenuClass *menu_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (menu_class);

  object_class->constructed = gp_menu_constructed;
  object_class->dispose = gp_menu_dispose;
  object_class->finalize = gp_menu_finalize;
  object_class->get_property = gp_menu_get_property;
  object_class->set_property = gp_menu_set_property;

  install_properties (object_class);
}

static void
gp_menu_init (GpMenu *menu)
{
}

GtkWidget *
gp_menu_new (GpApplet    *applet,
             const gchar *name)
{
  return g_object_new (GP_TYPE_MENU,
                       "applet", applet,
                       "name", name,
                       NULL);
}

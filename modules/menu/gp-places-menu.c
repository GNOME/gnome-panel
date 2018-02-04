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

#include "gp-bookmarks.h"
#include "gp-menu-utils.h"
#include "gp-places-menu.h"

#define MAX_ITEMS_OR_SUBMENU 8

struct _GpPlacesMenu
{
  GtkMenu      parent;

  GpApplet    *applet;

  guint        reload_id;

  GpBookmarks *bookmarks;
  GtkWidget   *bookmarks_menu;

  gulong       locked_down_id;
  gulong       menu_icon_size_id;
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
bookmark_activate_cb (GtkWidget   *item,
                      const gchar *uri)
{
  gp_menu_utils_show_uri (uri, NULL, GDK_CURRENT_TIME);
}

static void
bookmark_drag_data_get_cb (GtkWidget        *widget,
                           GdkDragContext   *context,
                           GtkSelectionData *selection_data,
                           guint             info,
                           guint             time,
                           const gchar      *uri)
{
  gchar *uris[2];

  uris[0] = (gchar *) uri;
  uris[1] = NULL;

  gtk_selection_data_set_uris (selection_data, uris);
}

static void
append_bookmark (GpBookmarks  *bookmarks,
                 GpBookmark   *bookmark,
                 GpPlacesMenu *menu)
{
  GtkWidget *add_menu;
  guint icon_size;
  GtkWidget *icon;
  GtkWidget *item;

  add_menu = menu->bookmarks_menu ? menu->bookmarks_menu : GTK_WIDGET (menu);

  if (bookmark->icon != NULL)
    icon = gtk_image_new_from_gicon (bookmark->icon, GTK_ICON_SIZE_MENU);
  else
    icon = gtk_image_new_from_icon_name ("folder", GTK_ICON_SIZE_MENU);

  icon_size = gp_applet_get_menu_icon_size (menu->applet);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);

  item = gp_image_menu_item_new_with_label (bookmark->label);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_menu_shell_append (GTK_MENU_SHELL (add_menu), item);
  gtk_widget_show (item);

  if (bookmark->tooltip != NULL)
    {
      gtk_widget_set_tooltip_text (item, bookmark->tooltip);

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

      if (bookmark->icon != NULL)
        gtk_drag_source_set_icon_gicon (item, bookmark->icon);

      g_signal_connect_data (item, "drag-data-get",
                             G_CALLBACK (bookmark_drag_data_get_cb),
                             g_file_get_uri (bookmark->file),
                             (GClosureNotify) g_free,
                             0);
    }

  g_signal_connect_data (item, "activate",
                         G_CALLBACK (bookmark_activate_cb),
                         g_file_get_uri (bookmark->file),
                         (GClosureNotify) g_free,
                         0);
}

static void
append_bookmarks (GpPlacesMenu *menu)
{
  if (gp_bookmarks_get_count (menu->bookmarks) > MAX_ITEMS_OR_SUBMENU)
    {
      guint icon_size;
      GtkWidget *icon;
      GtkWidget *item;

      icon_size = gp_applet_get_menu_icon_size (menu->applet);
      icon = gtk_image_new_from_icon_name ("user-bookmarks", GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);

      item = gp_image_menu_item_new_with_label (_("Bookmarks"));
      gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      menu->bookmarks_menu = gtk_menu_new ();
      g_object_add_weak_pointer (G_OBJECT (item), (gpointer *) &menu->bookmarks_menu);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu->bookmarks_menu);
    }

  gp_bookmarks_foreach (menu->bookmarks,
                        (GpBookmarksForeachFunc) append_bookmark,
                        menu);
}

static void
remove_item (GtkWidget *widget,
             gpointer   user_data)
{
  gtk_widget_destroy (widget);
}

static void
menu_reload (GpPlacesMenu *menu)
{
  gtk_container_foreach (GTK_CONTAINER (menu), remove_item, NULL);
  g_assert (menu->bookmarks_menu == NULL);

  append_bookmarks (menu);
}

static gboolean
reload_cb (gpointer user_data)
{
  GpPlacesMenu *menu;

  menu = GP_PLACES_MENU (user_data);

  menu_reload (menu);
  menu->reload_id = 0;

  return G_SOURCE_REMOVE;
}

static void
queue_reload (GpPlacesMenu *menu)
{
  if (menu->reload_id != 0)
    return;

  menu->reload_id = g_timeout_add_full (G_PRIORITY_LOW, 200,
                                        reload_cb, menu,
                                        NULL);

  g_source_set_name_by_id (menu->reload_id, "[menu] reload_cb");
}

static void
bookmarks_changed_cb (GpBookmarks  *bookmarks,
                      GpPlacesMenu *menu)
{
  queue_reload (menu);
}

static void
locked_down_cb (GpApplet     *applet,
                GParamSpec   *pspec,
                GpPlacesMenu *menu)
{
  queue_reload (menu);
}

static void
menu_icon_size_cb (GpApplet     *applet,
                   GParamSpec   *pspec,
                   GpPlacesMenu *menu)
{
  queue_reload (menu);
}

static void
gp_places_menu_constructed (GObject *object)
{
  GpPlacesMenu *menu;

  menu = GP_PLACES_MENU (object);

  G_OBJECT_CLASS (gp_places_menu_parent_class)->constructed (object);

  menu->bookmarks = gp_bookmarks_new ();
  g_signal_connect (menu->bookmarks, "changed",
                    G_CALLBACK (bookmarks_changed_cb), menu);

  menu->locked_down_id = g_signal_connect (menu->applet, "notify::locked-down",
                                           G_CALLBACK (locked_down_cb), menu);

  menu->menu_icon_size_id = g_signal_connect (menu->applet,
                                              "notify::menu-icon-size",
                                              G_CALLBACK (menu_icon_size_cb),
                                              menu);

  queue_reload (menu);
}

static void
gp_places_menu_dispose (GObject *object)
{
  GpPlacesMenu *menu;

  menu = GP_PLACES_MENU (object);

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

  g_clear_object (&menu->bookmarks);

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

  object_class->constructed = gp_places_menu_constructed;
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

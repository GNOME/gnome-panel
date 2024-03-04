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
#include "gp-recent-menu.h"
#include "gp-volumes.h"

#define MAX_ITEMS_OR_SUBMENU 8

struct _GpPlacesMenu
{
  GtkMenu      parent;

  gboolean     enable_tooltips;
  gboolean     locked_down;
  guint        menu_icon_size;

  guint        reload_id;

  GpBookmarks *bookmarks;
  GtkWidget   *bookmarks_menu;

  GpVolumes   *volumes;
  GtkWidget   *volumes_local_menu;
  GtkWidget   *volumes_remote_menu;
};

enum
{
  PROP_0,

  PROP_ENABLE_TOOLTIPS,
  PROP_LOCKED_DOWN,
  PROP_MENU_ICON_SIZE,

  LAST_PROP
};

static GParamSpec *menu_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpPlacesMenu, gp_places_menu, GTK_TYPE_MENU)

static void
free_uri (gchar    *uri,
          GClosure *closure)
{
  g_free (uri);
}

static void
unref_object (GObject  *object,
              GClosure *closure)
{
  g_object_unref (object);
}

static void
poll_for_media_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  GError *error;

  error = NULL;
  if (!g_drive_poll_for_media_finish (G_DRIVE (object), res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        {
          gchar *name;
          gchar *message;

          name = g_drive_get_name (G_DRIVE (object));
          message = g_strdup_printf (_("Unable to scan %s for media changes"), name);
          g_free (name);

          gp_menu_utils_show_error_dialog (message, error);
          g_free (message);
        }
    }

  g_clear_error (&error);
}

static void
drive_activate_cb (GtkWidget *item,
                   GDrive    *drive)
{
  g_drive_poll_for_media (drive, NULL, poll_for_media_cb, NULL);
}

static void
mount_cb (GObject         *object,
          GAsyncResult    *res,
          GMountOperation *operation)
{
  GError *error;

  error = NULL;
  if (g_volume_mount_finish (G_VOLUME (object), res, &error))
    {
      GMount *mount;
      GFile *root;
      gchar *uri;

      mount = g_volume_get_mount (G_VOLUME (object));
      root = g_mount_get_root (mount);
      g_object_unref (mount);

      uri = g_file_get_uri (root);
      g_object_unref (root);

      gp_menu_utils_launch_uri (uri);
      g_free (uri);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        {
          gchar *name;
          gchar *message;

          name = g_volume_get_name (G_VOLUME (object));
          message = g_strdup_printf (_("Unable to mount %s"), name);
          g_free (name);

          gp_menu_utils_show_error_dialog (message, error);
          g_free (message);
        }
    }

  g_object_unref (operation);
  g_clear_error (&error);
}

static void
volume_activate_cb (GtkWidget *item,
                    GVolume   *volume)
{
  GMountMountFlags flags;
  GMountOperation *operation;

  flags = G_MOUNT_MOUNT_NONE;
  operation = gtk_mount_operation_new (NULL);

  g_volume_mount (volume, flags, operation, NULL,
                  (GAsyncReadyCallback) mount_cb,
                  operation);
}

static void
uri_activate_cb (GtkWidget   *item,
                 const gchar *uri)
{
  gp_menu_utils_launch_uri (uri);
}

static void
uri_drag_data_get_cb (GtkWidget        *widget,
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

static GtkWidget *
create_menu_item (GpPlacesMenu *menu,
                  GFile        *file,
                  GIcon        *icon,
                  const gchar  *icon_name,
                  const gchar  *label,
                  const gchar  *tooltip)
{
  GtkWidget *image;
  GtkWidget *item;

  g_assert (file != NULL);
  g_assert (icon != NULL || icon_name != NULL);
  g_assert (label != NULL);

  if (icon != NULL)
    image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
  else
    image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);

  gtk_image_set_pixel_size (GTK_IMAGE (image), menu->menu_icon_size);

  item = gp_image_menu_item_new_with_label (label);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), image);

  if (tooltip != NULL)
    {
      gtk_widget_set_tooltip_text (item, tooltip);

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
      else
        gtk_drag_source_set_icon_name (item, icon_name);

      g_signal_connect_data (item, "drag-data-get",
                             G_CALLBACK (uri_drag_data_get_cb),
                             g_file_get_uri (file),
                             (GClosureNotify) free_uri,
                             0);
    }

  g_signal_connect_data (item, "activate",
                         G_CALLBACK (uri_activate_cb),
                         g_file_get_uri (file),
                         (GClosureNotify) free_uri,
                         0);

  return item;
}

static void
append_bookmark (GpBookmarks  *bookmarks,
                 GpBookmark   *bookmark,
                 GpPlacesMenu *menu)
{
  GtkWidget *add_menu;
  GtkWidget *item;

  add_menu = menu->bookmarks_menu ? menu->bookmarks_menu : GTK_WIDGET (menu);

  item = create_menu_item (menu, bookmark->file, bookmark->icon, "folder",
                           bookmark->label, bookmark->tooltip);

  gtk_menu_shell_append (GTK_MENU_SHELL (add_menu), item);
  gtk_widget_show (item);
}

static void
append_local_drive (GpVolumes    *volumes,
                    GDrive       *drive,
                    GpPlacesMenu *menu)
{
  GIcon *icon;
  gchar *label;
  gchar *tooltip;
  GtkWidget *image;
  GtkWidget *item;
  GtkWidget *add_menu;

  icon = g_drive_get_icon (drive);
  label = g_drive_get_name (drive);
  tooltip = g_strdup_printf (_("Rescan %s"), label);

  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (image), menu->menu_icon_size);

  item = gp_image_menu_item_new_with_label (label);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), image);

  gtk_widget_set_tooltip_text (item, tooltip);
  g_object_bind_property (menu, "enable-tooltips", item, "has-tooltip",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_object_unref (icon);
  g_free (tooltip);
  g_free (label);

  add_menu = menu->volumes_local_menu ? menu->volumes_local_menu : GTK_WIDGET (menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (add_menu), item);
  gtk_widget_show (item);

  g_signal_connect_data (item, "activate",
                         G_CALLBACK (drive_activate_cb),
                         g_object_ref (drive),
                         (GClosureNotify) unref_object,
                         0);
}

static void
append_local_volume (GpVolumes    *volumes,
                     GVolume      *volume,
                     GpPlacesMenu *menu)
{
  GIcon *icon;
  gchar *label;
  gchar *tooltip;
  GtkWidget *image;
  GtkWidget *item;
  GtkWidget *add_menu;

  icon = g_volume_get_icon (volume);
  label = g_volume_get_name (volume);
  tooltip = g_strdup_printf (_("Mount %s"), label);

  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (image), menu->menu_icon_size);

  item = gp_image_menu_item_new_with_label (label);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), image);

  gtk_widget_set_tooltip_text (item, tooltip);
  g_object_bind_property (menu, "enable-tooltips", item, "has-tooltip",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_object_unref (icon);
  g_free (tooltip);
  g_free (label);

  add_menu = menu->volumes_local_menu ? menu->volumes_local_menu : GTK_WIDGET (menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (add_menu), item);
  gtk_widget_show (item);

  g_signal_connect_data (item, "activate",
                         G_CALLBACK (volume_activate_cb),
                         g_object_ref (volume),
                         (GClosureNotify) unref_object,
                         0);
}

static void
append_local_mount (GpVolumes    *volumes,
                    GMount       *mount,
                    GpPlacesMenu *menu)
{
  GFile *file;
  GIcon *icon;
  gchar *label;
  GtkWidget *item;
  GtkWidget *add_menu;

  file = g_mount_get_root (mount);
  icon = g_mount_get_icon (mount);
  label = g_mount_get_name (mount);

  item = create_menu_item (menu, file, icon, NULL, label, NULL);

  add_menu = menu->volumes_local_menu ? menu->volumes_local_menu : GTK_WIDGET (menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (add_menu), item);
  gtk_widget_show (item);

  g_object_unref (file);
  g_object_unref (icon);
  g_free (label);
}

static void
append_remote_mount (GpVolumes    *volumes,
                    GMount       *mount,
                    GpPlacesMenu *menu)
{
  GFile *file;
  GIcon *icon;
  gchar *label;
  GtkWidget *item;
  GtkWidget *add_menu;

  file = g_mount_get_root (mount);
  icon = g_mount_get_icon (mount);
  label = g_mount_get_name (mount);

  item = create_menu_item (menu, file, icon, NULL, label, NULL);

  add_menu = menu->volumes_remote_menu ? menu->volumes_remote_menu : GTK_WIDGET (menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (add_menu), item);
  gtk_widget_show (item);

  g_object_unref (file);
  g_object_unref (icon);
  g_free (label);
}

static void
append_home_dir (GpPlacesMenu *menu)
{
  GFile *file;
  gchar *label;
  const gchar *tooltip;
  GtkWidget *item;

  file = g_file_new_for_path (g_get_home_dir ());

  label = gp_menu_utils_get_label_for_file (file);
  tooltip = _("Open your personal folder");

  item = create_menu_item (menu, file, NULL, "user-home", label, tooltip);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  g_object_unref (file);
  g_free (label);
}

static void
append_desktop_dir (GpPlacesMenu *menu)
{
  GFile *file;
  const gchar *label;
  const gchar *tooltip;
  GtkWidget *item;

  file = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));

  /* Translators: Desktop is used here as in "Desktop Folder"
   * (this is not the Desktop environment).
   */
  label = C_("Desktop Folder", "Desktop");
  tooltip = _("Open the contents of your desktop in a folder");

  item = create_menu_item (menu, file, NULL, "user-desktop", label, tooltip);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  g_object_unref (file);
}

static void
append_bookmarks (GpPlacesMenu *menu)
{
  if (gp_bookmarks_get_count (menu->bookmarks) > MAX_ITEMS_OR_SUBMENU)
    {
      GtkWidget *icon;
      GtkWidget *item;

      icon = gtk_image_new_from_icon_name ("user-bookmarks", GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (GTK_IMAGE (icon), menu->menu_icon_size);

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
append_separator (GpPlacesMenu *menu)
{
  GtkWidget *item;

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  gtk_widget_set_sensitive (item, FALSE);
}

static void
append_computer (GpPlacesMenu *menu)
{
  GFile *file;
  const gchar *label;
  const gchar *tooltip;
  GtkWidget *item;

  file = g_file_new_for_uri ("computer://");

  label = _("Computer");
  tooltip = _("Browse all local and remote disks and folders accessible from this computer");

  item = create_menu_item (menu, file, NULL, "computer", label, tooltip);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  g_object_unref (file);
}

static void
append_local_volumes (GpPlacesMenu *menu)
{
  if (gp_volumes_get_local_count (menu->volumes) > MAX_ITEMS_OR_SUBMENU)
    {
      GtkWidget *icon;
      GtkWidget *item;

      icon = gtk_image_new_from_icon_name ("drive-removable-media", GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (GTK_IMAGE (icon), menu->menu_icon_size);

      item = gp_image_menu_item_new_with_label (_("Removable Media"));
      gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      menu->volumes_local_menu = gtk_menu_new ();
      g_object_add_weak_pointer (G_OBJECT (item), (gpointer *) &menu->volumes_local_menu);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu->volumes_local_menu);
    }

  gp_volumes_foreach_local_drives (menu->volumes,
                                   (GpVolumesForeachDrivesFunc) append_local_drive,
                                   menu);

  gp_volumes_foreach_local_volumes (menu->volumes,
                                    (GpVolumesForeachVolumesFunc) append_local_volume,
                                    menu);

  gp_volumes_foreach_local_mounts (menu->volumes,
                                   (GpVolumesForeachMountsFunc) append_local_mount,
                                   menu);
}

static void
append_network (GpPlacesMenu *menu)
{
  GFile *file;
  const gchar *label;
  const gchar *tooltip;
  GtkWidget *item;

  file = g_file_new_for_uri ("network://");

  label = _("Network");
  tooltip = _("Browse bookmarked and local network locations");

  item = create_menu_item (menu, file, NULL, "network-workgroup", label, tooltip);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  g_object_unref (file);
}

static void
append_remote_volumes (GpPlacesMenu *menu)
{
  if (gp_volumes_get_remote_count (menu->volumes) > MAX_ITEMS_OR_SUBMENU)
    {
      GtkWidget *icon;
      GtkWidget *item;

      icon = gtk_image_new_from_icon_name ("network-server", GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (GTK_IMAGE (icon), menu->menu_icon_size);

      item = gp_image_menu_item_new_with_label (_("Network Places"));
      gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      menu->volumes_remote_menu = gtk_menu_new ();
      g_object_add_weak_pointer (G_OBJECT (item), (gpointer *) &menu->volumes_remote_menu);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu->volumes_remote_menu);
    }

  gp_volumes_foreach_remote_mounts (menu->volumes,
                                    (GpVolumesForeachMountsFunc) append_remote_mount,
                                    menu);
}

static void
append_recent_menu (GpPlacesMenu *menu)
{
  GtkWidget *icon;
  GtkWidget *item;
  GtkWidget *recent_menu;

  icon = gtk_image_new_from_icon_name ("document-open-recent", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), menu->menu_icon_size);

  item = gp_image_menu_item_new_with_label (_("Recent Documents"));
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  recent_menu = gp_recent_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), recent_menu);

  g_object_bind_property (menu, "enable-tooltips",
                          recent_menu, "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (menu, "menu-icon-size",
                          recent_menu, "menu-icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (recent_menu, "empty",
                          item, "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN |
                          G_BINDING_SYNC_CREATE);
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
  g_assert (menu->volumes_local_menu == NULL);
  g_assert (menu->volumes_remote_menu == NULL);

  append_home_dir (menu);
  append_desktop_dir (menu);
  append_bookmarks (menu);

  append_separator (menu);
  append_computer (menu);
  append_local_volumes (menu);

  append_separator (menu);
  append_network (menu);
  append_remote_volumes (menu);

  append_separator (menu);
  append_recent_menu (menu);
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
volumes_changed_cb (GpVolumes    *volumes,
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

  menu->volumes = gp_volumes_new ();
  g_signal_connect (menu->volumes, "changed",
                    G_CALLBACK (volumes_changed_cb), menu);

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

  g_clear_object (&menu->bookmarks);
  g_clear_object (&menu->volumes);

  G_OBJECT_CLASS (gp_places_menu_parent_class)->dispose (object);
}

static void
gp_places_menu_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GpPlacesMenu *menu;

  menu = GP_PLACES_MENU (object);

  switch (property_id)
    {
      case PROP_LOCKED_DOWN:
        g_assert_not_reached ();
        break;

      case PROP_ENABLE_TOOLTIPS:
        g_value_set_boolean (value, menu->enable_tooltips);
        break;

      case PROP_MENU_ICON_SIZE:
        g_value_set_uint (value, menu->menu_icon_size);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
set_enable_tooltips (GpPlacesMenu *menu,
                     gboolean      enable_tooltips)
{
  if (menu->enable_tooltips == enable_tooltips)
    return;

  menu->enable_tooltips = enable_tooltips;

  g_object_notify_by_pspec (G_OBJECT (menu),
                            menu_properties[PROP_ENABLE_TOOLTIPS]);
}

static void
set_locked_down (GpPlacesMenu *menu,
                 gboolean      locked_down)
{
  if (menu->locked_down == locked_down)
    return;

  menu->locked_down = locked_down;
  queue_reload (menu);
}

static void
set_menu_icon_size (GpPlacesMenu *menu,
                    guint         menu_icon_size)
{
  if (menu->menu_icon_size == menu_icon_size)
    return;

  menu->menu_icon_size = menu_icon_size;
  queue_reload (menu);
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
      case PROP_ENABLE_TOOLTIPS:
        set_enable_tooltips (menu, g_value_get_boolean (value));
        break;

      case PROP_LOCKED_DOWN:
        set_locked_down (menu, g_value_get_boolean (value));
        break;

      case PROP_MENU_ICON_SIZE:
        set_menu_icon_size (menu, g_value_get_uint (value));
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
                       16, 48, 16,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                       G_PARAM_EXPLICIT_NOTIFY |
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
  object_class->get_property = gp_places_menu_get_property;
  object_class->set_property = gp_places_menu_set_property;

  install_properties (object_class);
}

static void
gp_places_menu_init (GpPlacesMenu *menu)
{
}

GtkWidget *
gp_places_menu_new (void)
{
  return g_object_new (GP_TYPE_PLACES_MENU,
                       NULL);
}

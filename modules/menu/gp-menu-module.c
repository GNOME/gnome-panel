/*
 * Copyright (C) 2004 Vincent Untz
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     Vincent Untz <vincent@vuntz.net>
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmenu-tree.h>
#include <libgnome-panel/gp-image-menu-item.h>
#include <libgnome-panel/gp-module.h>

#include "gp-lock-logout.h"
#include "gp-main-menu-applet.h"
#include "gp-menu-bar-applet.h"
#include "gp-menu-button-applet.h"
#include "gp-menu-utils.h"
#include "gp-menu.h"
#include "gp-user-menu-applet.h"
#include "gp-user-menu.h"
#include "gp-places-menu.h"

typedef struct
{
  gchar              *filename;
  gchar              *menu_path;

  GIcon              *icon;
  gchar              *text;
} DirectroyData;

static gchar *
make_text (const gchar *name,
           const gchar *desc)
{
  const gchar *real_name;
  gchar *result;

  real_name = name ? name : _("(empty)");

  if (desc != NULL && *desc != '\0')
    result = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>\n%s",
                                      real_name, desc);
  else
    result = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>",
                                      real_name);

  return result;
}

static DirectroyData *
directory_data_new (GMenuTreeDirectory *directory,
                    const gchar        *filename)
{
  DirectroyData *data;
  GIcon *icon;

  data = g_new0 (DirectroyData, 1);

  data->filename = g_strdup (filename);
  data->menu_path = gmenu_tree_directory_make_path (directory, NULL);

  icon = gmenu_tree_directory_get_icon (directory);
  data->icon = icon ? g_object_ref (icon) : NULL;
  data->text = make_text (gmenu_tree_directory_get_name (directory),
                          gmenu_tree_directory_get_comment (directory));

  return data;
}

static void
directory_data_free (gpointer user_data)
{
  DirectroyData *data;

  data = (DirectroyData *) user_data;

  g_free (data->filename);
  g_free (data->menu_path);

  g_clear_object (&data->icon);
  g_free (data->text);

  g_free (data);
}

typedef struct
{
  GpInitialSetupDialog *dialog;

  GtkTreeStore         *store;
  GSList               *directories;
} MenuButtonData;

static MenuButtonData *
menu_button_data_new (GpInitialSetupDialog *dialog)
{
  MenuButtonData *data;

  data = g_new0 (MenuButtonData, 1);

  data->dialog = dialog;

  data->store = NULL;
  data->directories = NULL;

  return data;
}

static void
menu_button_data_free (gpointer user_data)
{
  MenuButtonData *data;

  data = (MenuButtonData *) user_data;

  g_clear_object (&data->store);
  g_slist_free_full (data->directories, directory_data_free);

  g_free (data);
}

static void
populate_from_root (GtkTreeStore       *store,
                    GtkTreeIter        *parent,
                    GMenuTreeDirectory *directory,
                    const gchar        *menu,
                    MenuButtonData     *data);

static void
append_directory (GtkTreeStore       *store,
                  GtkTreeIter        *parent,
                  GMenuTreeDirectory *directory,
                  const gchar        *menu,
                  MenuButtonData     *data)
{
  DirectroyData *dir_data;
  GtkTreeIter iter;

  dir_data = directory_data_new (directory, menu);
  data->directories = g_slist_prepend (data->directories, dir_data);

  gtk_tree_store_append (store, &iter, parent);
  gtk_tree_store_set (store, &iter,
                      0, dir_data->icon,
                      1, dir_data->text,
                      2, dir_data,
                      -1);

  populate_from_root (store, &iter, directory, menu, data);
}

static void
populate_from_root (GtkTreeStore       *store,
                    GtkTreeIter        *parent,
                    GMenuTreeDirectory *directory,
                    const gchar        *menu,
                    MenuButtonData     *data)
{
  GMenuTreeIter *iter;
  GMenuTreeItemType next_type;

  iter = gmenu_tree_directory_iter (directory);

  next_type = gmenu_tree_iter_next (iter);
  while (next_type != GMENU_TREE_ITEM_INVALID)
    {
      if (next_type == GMENU_TREE_ITEM_DIRECTORY)
        {
          GMenuTreeDirectory *dir;

          dir = gmenu_tree_iter_get_directory (iter);
          append_directory (store, parent, dir, menu, data);
          gmenu_tree_item_unref (dir);
        }

      next_type = gmenu_tree_iter_next (iter);
    }

  gmenu_tree_iter_unref (iter);
}

static void
populate_model_from_menu (GtkTreeStore   *store,
                          const gchar    *menu,
                          gboolean        separator,
                          MenuButtonData *data)
{
  GMenuTree *tree;
  GMenuTreeDirectory *root;

  tree = gmenu_tree_new (menu, GMENU_TREE_FLAGS_SORT_DISPLAY_NAME);

  if (!gmenu_tree_load_sync (tree, NULL))
    {
      g_object_unref (tree);
      return;
    }

  root = gmenu_tree_get_root_directory (tree);

  if (root == NULL)
    {
      g_object_unref (tree);
      return;
    }

  if (separator)
    {
      GtkTreeIter iter;

      gtk_tree_store_append (store, &iter, NULL);
      gtk_tree_store_set (store, &iter, 0, NULL, 1, NULL, 2, NULL, -1);
    }

  populate_from_root (store, NULL, root, menu, data);
  gmenu_tree_item_unref (root);
  g_object_unref (tree);
}

static void
populate_model (GtkTreeStore   *store,
                MenuButtonData *data)
{
  gchar *menu;

  menu = gp_menu_utils_get_applications_menu ();
  populate_model_from_menu (store, menu, FALSE, data);
  g_free (menu);

  menu = g_strdup ("gnomecc.menu");
  populate_model_from_menu (store, menu, TRUE, data);
  g_free (menu);
}

static const gchar *
filename_to_scheme (const gchar *filename)
{
  if (g_str_has_suffix (filename, "applications.menu"))
    return "applications";
  else if (g_strcmp0 (filename, "gnomecc.menu") == 0)
    return "gnomecc";

  return NULL;
}

static void
selection_changed_cb (GtkTreeSelection *selection,
                      MenuButtonData   *data)
{
  gboolean done;
  GtkTreeModel *model;
  GtkTreeIter iter;

  done = FALSE;
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      DirectroyData *dir_data;

      dir_data = NULL;
      gtk_tree_model_get (model, &iter, 2, &dir_data, -1);

      if (dir_data != NULL)
        {
          const gchar *scheme;
          gchar *menu_path;
          GVariant *variant;

          scheme = filename_to_scheme (dir_data->filename);
          menu_path = g_strdup_printf ("%s:%s", scheme, dir_data->menu_path);

          variant = g_variant_new_string (menu_path);
          g_free (menu_path);

          gp_initial_setup_dialog_set_setting (data->dialog, "menu-path", variant);
          done = TRUE;
        }
    }

  gp_initial_setup_dialog_set_done (data->dialog, done);
}

static void
menu_button_initial_setup_dialog (GpInitialSetupDialog *dialog)
{
  MenuButtonData *data;
  GtkWidget *scrolled;
  GtkWidget *tree_view;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  data = menu_button_data_new (dialog);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled), 460);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scrolled), 480);
  gtk_widget_show (scrolled);

  tree_view = gtk_tree_view_new ();
  gtk_container_add (GTK_CONTAINER (scrolled), tree_view);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
  gtk_widget_show (tree_view);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  g_signal_connect (selection, "changed",
                    G_CALLBACK (selection_changed_cb), data);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_add_attribute (column, renderer, "gicon", 0);

  g_object_set (renderer,
                "stock-size", GTK_ICON_SIZE_DND,
                "xpad", 4, "ypad", 4,
                NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer, "markup", 1);

  g_object_set (renderer,
                "ellipsize", PANGO_ELLIPSIZE_END,
                "xpad", 4, "ypad", 4,
                NULL);

  data->store = gtk_tree_store_new (3, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_POINTER);
  populate_model (data->store, data);

  gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view),
                           GTK_TREE_MODEL (data->store));

  gp_initial_setup_dialog_add_content_widget (dialog,
                                              scrolled,
                                              data,
                                              menu_button_data_free);
}

static GpAppletInfo *
menu_get_applet_info (const gchar *id)
{
  GpGetAppletTypeFunc type_func;
  const gchar *name;
  const gchar *description;
  const gchar *icon;
  GpInitialSetupDialogFunc initial_setup_func;
  GpAppletInfo *info;

  initial_setup_func = NULL;

  if (g_strcmp0 (id, "main-menu") == 0)
    {
      type_func = gp_main_menu_applet_get_type;
      name = _("Main Menu");
      description = _("The main GNOME menu");
      icon = "start-here";
    }
  else if (g_strcmp0 (id, "menu-button") == 0)
    {
      type_func = gp_menu_button_applet_get_type;
      name = _("Menu Button");
      description = _("A custom menu button");
      icon = "start-here";

      initial_setup_func = menu_button_initial_setup_dialog;
    }
  else if (g_strcmp0 (id, "menu-bar") == 0)
    {
      type_func = gp_menu_bar_applet_get_type;
      name = _("Menu Bar");
      description = _("A custom menu bar");
      icon = "start-here";
    }
  else if (g_strcmp0 (id, "user-menu") == 0)
    {
      type_func = gp_user_menu_applet_get_type;
      name = _("User menu");
      description = _("Menu to change your settings and log out");
      icon = "computer";
    }
  else
    {
      g_assert_not_reached ();
      return NULL;
    }

  info = gp_applet_info_new (type_func, name, description, icon);

  if (initial_setup_func != NULL)
    gp_applet_info_set_initial_setup_dialog (info, initial_setup_func);

  return info;
}

static const gchar *
menu_get_applet_id_from_iid (const gchar *iid)
{
  if (g_strcmp0 (iid, "PanelInternalFactory::MenuBar") == 0)
    return "menu-bar";

  if (g_strcmp0 (iid, "PanelInternalFactory::MenuButton") == 0)
    return "menu-button";

  if (g_strcmp0 (iid, "PanelInternalFactory::UserMenu") == 0)
    return "user-menu";

  return NULL;
}

typedef struct
{
  gboolean      enable_tooltips;
  gboolean      locked_down;
  guint         menu_icon_size;

  GpLockLogout *lock_logout;
} StandaloneMenuData;

static void
standalone_menu_data_free (gpointer user_data)
{
  StandaloneMenuData *data;

  data = (StandaloneMenuData *) user_data;

  g_object_unref (data->lock_logout);
  g_free (data);
}

static void
append_places_item (StandaloneMenuData *data,
                    GtkMenu            *menu)
{
  GtkWidget *icon;
  GtkWidget *item;
  GtkWidget *places_menu;

  icon = gtk_image_new_from_icon_name ("folder", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), data->menu_icon_size);

  item = gp_image_menu_item_new_with_label (_("Places"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_widget_show (item);

  places_menu = gp_places_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), places_menu);

  g_object_set (places_menu,
                "enable-tooltips", data->enable_tooltips,
                "locked-down", data->locked_down,
                "menu-icon-size", data->menu_icon_size,
                NULL);
}

static void
append_lock_logout (GtkMenu            *menu,
                    StandaloneMenuData *data)
{
  gp_lock_logout_append_to_menu (data->lock_logout, menu);
}

static void
append_user_item (StandaloneMenuData *data,
                  GtkMenu            *menu)
{

  GtkWidget *icon;
  gchar *user_name;
  GtkWidget *item;
  GtkWidget *user_menu;

  icon = gtk_image_new_from_icon_name ("computer", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), data->menu_icon_size);

  user_name = gp_menu_utils_get_user_name ();
  item = gp_image_menu_item_new_with_label (user_name);
  g_free (user_name);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_widget_show (item);

  user_menu = gp_user_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), user_menu);

  g_object_set (user_menu,
                "enable-tooltips", data->enable_tooltips,
                "locked-down", data->locked_down,
                "menu-icon-size", data->menu_icon_size,
                NULL);

  g_object_bind_property (user_menu, "empty", item, "visible",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE |
                          G_BINDING_INVERT_BOOLEAN);

  gp_user_menu_set_append_func (GP_USER_MENU (user_menu),
                                (GpAppendMenuItemsFunc) append_lock_logout,
                                data);
}

static void
append_menu_items_cb (GtkMenu            *menu,
                      StandaloneMenuData *data)
{
  append_separator_if_needed (GTK_MENU (menu));
  append_places_item (data, menu);
  append_user_item (data, menu);
}

static GtkWidget *
menu_get_standalone_menu (gboolean enable_tooltips,
                          gboolean locked_down,
                          guint    menu_icon_size)
{
  StandaloneMenuData *data;
  gchar *filename;
  GtkWidget *menu;

  data = g_new0 (StandaloneMenuData, 1);

  data->enable_tooltips = enable_tooltips;
  data->locked_down = locked_down;
  data->menu_icon_size = menu_icon_size;

  filename = gp_menu_utils_get_applications_menu ();

  menu = g_object_new (GP_TYPE_MENU,
                       "name", filename,
                       "required", TRUE,
                       "enable-tooltips", data->enable_tooltips,
                       "locked-down", data->locked_down,
                       "menu-icon-size", data->menu_icon_size,
                       NULL);

  data->lock_logout = g_object_new (GP_TYPE_LOCK_LOGOUT,
                                    "enable-tooltips", data->enable_tooltips,
                                    "locked-down", data->locked_down,
                                    "menu-icon-size", data->menu_icon_size,
                                    NULL);

  gp_menu_set_append_func (GP_MENU (menu),
                           (GpAppendMenuItemsFunc) append_menu_items_cb,
                           data);

  g_object_set_data_full (G_OBJECT (menu), "data", data,
                          standalone_menu_data_free);
  g_free (filename);

  return menu;
}

static void
menu_loaded_cb (GtkWidget *widget,
                gpointer   user_data)
{
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *window;
  GdkRectangle rect;
  GdkSeat *seat;
  GdkDevice *device;
  GdkEvent *event;

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  window = gdk_screen_get_root_window (screen);

  rect.x = 0;
  rect.y = 0;
  rect.width = 1;
  rect.height = 1;

  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);

  gdk_window_get_device_position (window, device,
                                  &rect.x, &rect.y,
                                  NULL);

  event = gdk_event_new (GDK_BUTTON_PRESS);
  gdk_event_set_device (event, device);

  gtk_menu_popup_at_rect (GTK_MENU (widget), window, &rect,
                          GDK_GRAVITY_SOUTH_EAST,
                          GDK_GRAVITY_NORTH_WEST,
                          event);

  gdk_event_free (event);
}

static gboolean
main_menu_func (GpModule      *module,
                GpActionFlags  action,
                uint32_t       time)
{
  GSettings *general_settings;
  GSettings *lockdown_settings;
  gboolean enable_tooltips;
  gboolean locked_down;
  guint menu_icon_size;
  GtkWidget *menu;

  general_settings = g_settings_new ("org.gnome.gnome-panel.general");
  lockdown_settings = g_settings_new ("org.gnome.gnome-panel.lockdown");

  enable_tooltips = g_settings_get_boolean (general_settings, "enable-tooltips");
  locked_down = g_settings_get_boolean (lockdown_settings, "locked-down");
  menu_icon_size = g_settings_get_enum (general_settings, "menu-icon-size");

  g_object_unref (lockdown_settings);
  g_object_unref (general_settings);

  menu = menu_get_standalone_menu (enable_tooltips,
                                   locked_down,
                                   menu_icon_size);

  g_object_ref_sink (menu);

  g_signal_connect (menu, "deactivate", G_CALLBACK (g_object_unref), NULL);
  g_signal_connect (menu, "loaded", G_CALLBACK (menu_loaded_cb), NULL);

  return TRUE;
}

void
gp_module_load (GpModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  gp_module_set_gettext_domain (module, GETTEXT_PACKAGE);

  gp_module_set_abi_version (module, GP_MODULE_ABI_VERSION);

  gp_module_set_id (module, "org.gnome.gnome-panel.menu");
  gp_module_set_version (module, PACKAGE_VERSION);

  gp_module_set_applet_ids (module, "main-menu", "menu-bar",
                            "menu-button", "user-menu", NULL);

  gp_module_set_get_applet_info (module, menu_get_applet_info);
  gp_module_set_compatibility (module, menu_get_applet_id_from_iid);

  gp_module_set_actions (module,
                         GP_ACTION_MAIN_MENU,
                         main_menu_func);
}

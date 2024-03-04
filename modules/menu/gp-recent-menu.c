/*
 * Copyright (C) 2002 James Willcox
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
 *     James Willcox <jwillcox@gnome.org>
 */

#include "config.h"
#include "gp-recent-menu.h"
#include "gp-menu-utils.h"

#include <glib/gi18n-lib.h>
#include <libgnome-panel/gp-image-menu-item.h>

struct _GpRecentMenu
{
  GtkMenu    parent;

  gboolean   enable_tooltips;
  guint      menu_icon_size;

  gboolean   empty;

  GtkWidget *clear_dialog;

  guint      reload_id;

  gulong     changed_id;
};

enum
{
  PROP_0,

  PROP_ENABLE_TOOLTIPS,
  PROP_MENU_ICON_SIZE,

  PROP_EMPTY,

  LAST_PROP
};

static GParamSpec *menu_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpRecentMenu, gp_recent_menu, GTK_TYPE_MENU)

static void
unref_recent_info (GtkRecentInfo *info,
                   GClosure      *closure)
{
  gtk_recent_info_unref (info);
}

static void
activate_cb (GtkWidget     *item,
             GtkRecentInfo *info)
{
  const gchar *uri;

  uri = gtk_recent_info_get_uri (info);

  gp_menu_utils_launch_uri (uri);
}

static gint
items_sort_func (GtkRecentInfo *a,
                 GtkRecentInfo *b)
{
  return gtk_recent_info_get_modified (b) - gtk_recent_info_get_modified (a);
}

static void
append_recent_items (GpRecentMenu *menu)
{
  GtkRecentManager *manager;
  GList *items;
  gint count;
  GList *l;

  manager = gtk_recent_manager_get_default ();

  items = gtk_recent_manager_get_items (manager);
  items = g_list_sort (items, (GCompareFunc) items_sort_func);
  count = 0;

  for (l = items; l != NULL; l = l->next)
    {
      GtkRecentInfo *info;
      GIcon *icon;
      GtkWidget *image;
      const gchar *label;
      GtkWidget *item;
      GtkWidget *child;
      gchar *uri_display;

      info = l->data;

      icon = gtk_recent_info_get_gicon (info);
      image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (GTK_IMAGE (image), menu->menu_icon_size);
      g_clear_object (&icon);

      label = gtk_recent_info_get_display_name (info);
      item = gp_image_menu_item_new_with_label (label);
      gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), image);

      child = gtk_bin_get_child (GTK_BIN (item));
      if (GTK_IS_LABEL (child))
        {
          gtk_label_set_ellipsize (GTK_LABEL (child), PANGO_ELLIPSIZE_END);
          gtk_label_set_max_width_chars (GTK_LABEL (child), 30);
        }

      uri_display = gtk_recent_info_get_uri_display (info);
      if (uri_display != NULL)
        {
          gchar *tooltip;

          tooltip = g_strdup_printf (_("Open '%s'"), uri_display);
          g_free (uri_display);

          gtk_widget_set_tooltip_text (item, tooltip);
          g_free (tooltip);

          g_object_bind_property (menu, "enable-tooltips",
                                  item, "has-tooltip",
                                  G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        }

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      g_signal_connect_data (item, "activate",
                             G_CALLBACK (activate_cb),
                             gtk_recent_info_ref (info),
                             (GClosureNotify) unref_recent_info,
                             0);

      if (++count == 10)
        break;
    }

  g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);
}

static void
response_cb (GtkWidget    *widget,
             gint          response,
             GpRecentMenu *menu)
{
  if (response == GTK_RESPONSE_ACCEPT)
    {
      GtkRecentManager *manager;

      manager = gtk_recent_manager_get_default ();
      gtk_recent_manager_purge_items (manager, NULL);
    }

  gtk_widget_destroy (widget);
}

static void
clear_cb (GtkWidget    *item,
          GpRecentMenu *menu)
{
  GtkDialogFlags flags;
  const gchar *message;
  GtkWidget *dialog;
  const gchar *text;

  if (menu->clear_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (menu->clear_dialog));
      return;
    }

  flags = 0;
  message = _("Clear the Recent Documents list?");

  dialog = gtk_message_dialog_new (NULL, flags, GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE, "%s", message);

  text = _("If you clear the Recent Documents list, you clear the following:\n"
           "\342\200\242 All items from the Places \342\206\222 Recent Documents menu item.\n"
           "\342\200\242 All items from the recent documents list in all applications.");

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", text);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("C_lear"), GTK_RESPONSE_ACCEPT,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);

  g_signal_connect (dialog, "response", G_CALLBACK (response_cb), menu);

  menu->clear_dialog = dialog;
  g_object_add_weak_pointer (G_OBJECT (menu->clear_dialog),
                             (gpointer *) &menu->clear_dialog);

  gtk_widget_show (dialog);
}

static void
append_clear (GpRecentMenu *menu)
{
  GtkWidget *item;
  GtkWidget *icon;
  const gchar *tooltip;

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, FALSE);
  gtk_widget_show (item);

  icon = gtk_image_new_from_icon_name ("edit-clear-all", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), menu->menu_icon_size);

  item = gp_image_menu_item_new_with_label (_("Clear Recent Documents..."));
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  tooltip = _("Clear all items from the recent documents list");
  gtk_widget_set_tooltip_text (item, tooltip);

  g_object_bind_property (menu, "enable-tooltips", item, "has-tooltip",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_signal_connect (item, "activate", G_CALLBACK (clear_cb), menu);
}

static void
remove_item (GtkWidget *widget,
             gpointer   user_data)
{
  gtk_widget_destroy (widget);
}

static void
menu_reload (GpRecentMenu *menu)
{
  GList *children;
  gboolean empty;

  gtk_container_foreach (GTK_CONTAINER (menu), remove_item, NULL);

  append_recent_items (menu);
  append_clear (menu);

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  empty = g_list_length (children) - 2 == 0;
  g_list_free (children);

  if (menu->empty == empty)
    return;

  menu->empty = empty;

  g_object_notify_by_pspec (G_OBJECT (menu), menu_properties[PROP_EMPTY]);
}

static gboolean
reload_cb (gpointer user_data)
{
  GpRecentMenu *menu;

  menu = GP_RECENT_MENU (user_data);

  menu_reload (menu);
  menu->reload_id = 0;

  return G_SOURCE_REMOVE;
}

static void
queue_reload (GpRecentMenu *menu)
{
  if (menu->reload_id != 0)
    return;

  menu->reload_id = g_timeout_add_full (G_PRIORITY_LOW, 200,
                                        reload_cb, menu,
                                        NULL);

  g_source_set_name_by_id (menu->reload_id, "[menu] reload_cb");
}

static void
changed_cb (GtkRecentManager *manager,
            GpRecentMenu     *menu)
{
  queue_reload (menu);
}

static void
gp_recent_menu_constructed (GObject *object)
{
  GpRecentMenu *menu;
  GtkRecentManager *manager;

  menu = GP_RECENT_MENU (object);

  G_OBJECT_CLASS (gp_recent_menu_parent_class)->constructed (object);

  manager = gtk_recent_manager_get_default ();
  menu->changed_id = g_signal_connect (manager, "changed",
                                       G_CALLBACK (changed_cb), menu);

  queue_reload (menu);
}

static void
gp_recent_menu_dispose (GObject *object)
{
  GpRecentMenu *menu;

  menu = GP_RECENT_MENU (object);

  if (menu->reload_id != 0)
    {
      g_source_remove (menu->reload_id);
      menu->reload_id = 0;
    }

  if (menu->changed_id != 0)
    {
      GtkRecentManager *manager;

      manager = gtk_recent_manager_get_default ();

      g_signal_handler_disconnect (manager, menu->changed_id);
      menu->changed_id = 0;
    }

  g_clear_pointer (&menu->clear_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (gp_recent_menu_parent_class)->dispose (object);
}

static void
gp_recent_menu_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GpRecentMenu *menu;

  menu = GP_RECENT_MENU (object);

  switch (property_id)
    {
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
set_enable_tooltips (GpRecentMenu *menu,
                     gboolean      enable_tooltips)
{
  if (menu->enable_tooltips == enable_tooltips)
    return;

  menu->enable_tooltips = enable_tooltips;

  g_object_notify_by_pspec (G_OBJECT (menu),
                            menu_properties[PROP_ENABLE_TOOLTIPS]);
}

static void
set_menu_icon_size (GpRecentMenu *menu,
                    guint         menu_icon_size)
{
  if (menu->menu_icon_size == menu_icon_size)
    return;

  menu->menu_icon_size = menu_icon_size;
  queue_reload (menu);
}

static void
gp_recent_menu_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GpRecentMenu *menu;

  menu = GP_RECENT_MENU (object);

  switch (property_id)
    {
      case PROP_ENABLE_TOOLTIPS:
        set_enable_tooltips (menu, g_value_get_boolean (value));
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

  menu_properties[PROP_MENU_ICON_SIZE] =
    g_param_spec_uint ("menu-icon-size", "Menu Icon Size", "Menu Icon Size",
                       16, 48, 16,
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
gp_recent_menu_class_init (GpRecentMenuClass *menu_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (menu_class);

  object_class->constructed = gp_recent_menu_constructed;
  object_class->dispose = gp_recent_menu_dispose;
  object_class->get_property = gp_recent_menu_get_property;
  object_class->set_property = gp_recent_menu_set_property;

  install_properties (object_class);
}

static void
gp_recent_menu_init (GpRecentMenu *menu)
{
}

GtkWidget *
gp_recent_menu_new (void)
{
  return g_object_new (GP_TYPE_RECENT_MENU,
                       NULL);
}

/*
 * Copyright (C) 2016-2018 Alberts Muktupāvels
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

#include <libgnome-panel/gp-image-menu-item.h>

#include "sn-dbus-menu-item.h"

static void
set_image_from_icon_name (GpImageMenuItem *item,
                          const gchar     *icon_name,
                          guint            icon_size)
{
  GtkWidget *image;

  image = gtk_image_new ();

  gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (image), icon_size);

  gp_image_menu_item_set_image (item, image);
}

static GdkPixbuf *
get_pixbuf_at_size (GdkPixbuf *pixbuf,
                    gint       size)
{
  gint width;
  gint height;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  if (width != size || height != size)
    {
      GdkPixbuf *scaled;

      scaled = gdk_pixbuf_scale_simple (pixbuf, size, size,
                                        GDK_INTERP_BILINEAR);

      if (scaled != NULL)
        return scaled;
    }

  return g_object_ref (pixbuf);
}

static void
set_image_from_pixbuf (GpImageMenuItem *item,
                       GdkPixbuf       *pixbuf,
                       guint            icon_size)
{
  GtkWidget *image;
  GdkPixbuf *tmp;

  image = gtk_image_new ();

  tmp = get_pixbuf_at_size (pixbuf, icon_size);
  gtk_image_set_from_pixbuf (GTK_IMAGE (image), tmp);
  g_object_unref (tmp);

  gp_image_menu_item_set_image (item, image);
}

static void
update_icon (SnDBusMenuItem *item)
{
  GpImageMenuItem *menu_item;
  guint icon_size;

  if (!GP_IS_IMAGE_MENU_ITEM (item->item))
    return;

  menu_item = GP_IMAGE_MENU_ITEM (item->item);
  icon_size = gp_applet_get_menu_icon_size (GP_APPLET (item->applet));

  if (item->icon_name)
    set_image_from_icon_name (menu_item, item->icon_name, icon_size);
  else if (item->icon_data)
    set_image_from_pixbuf (menu_item, item->icon_data, icon_size);
  else
    gp_image_menu_item_set_image (menu_item, NULL);
}

static GdkPixbuf *
pixbuf_new (GVariant *variant)
{
  gsize length;
  const guchar *data;
  GInputStream *stream;
  GdkPixbuf *pixbuf;
  GError *error;

  data = g_variant_get_fixed_array (variant, &length, sizeof (guchar));

  if (length == 0)
    return NULL;

  stream = g_memory_input_stream_new_from_data (data, length, NULL);

  if (stream == NULL)
    return NULL;

  error = NULL;
  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &error);
  g_object_unref (stream);

  if (error != NULL)
    {
      g_warning ("Unable to build GdkPixbuf from icon data: %s", error->message);
      g_error_free (error);
    }

  return pixbuf;
}

static SnShortcut *
sn_shortcut_new (guint           key,
                 GdkModifierType mask)
{
  SnShortcut *shortcut;

  shortcut = g_new0 (SnShortcut, 1);

  shortcut->key = key;
  shortcut->mask = mask;

  return shortcut;
}

static SnShortcut **
sn_shortcuts_new (GVariant *variant)
{
  GPtrArray *array;
  GVariantIter shortcuts;
  GVariantIter *shortcut;

  if (variant == NULL || g_variant_iter_init (&shortcuts, variant) == 0)
    return NULL;

  array = g_ptr_array_new ();
  while (g_variant_iter_next (&shortcuts, "as", &shortcut))
    {
      guint key;
      GdkModifierType mask;
      const gchar *string;

      key = 0;
      mask = 0;

      while (g_variant_iter_next (shortcut, "&s", &string))
        {
          if (g_strcmp0 (string, "Control") == 0)
            mask |= GDK_CONTROL_MASK;
          else if (g_strcmp0 (string, "Alt") == 0)
            mask |= GDK_MOD1_MASK;
          else if (g_strcmp0 (string, "Shift") == 0)
            mask |= GDK_SHIFT_MASK;
          else if (g_strcmp0 (string, "Super") == 0)
            mask |= GDK_SUPER_MASK;
          else
            gtk_accelerator_parse (string, &key, NULL);
        }

      g_ptr_array_add (array,sn_shortcut_new (key, mask));
      g_variant_iter_free (shortcut);
    }

  g_ptr_array_add (array, NULL);
  return (SnShortcut **) g_ptr_array_free (array, FALSE);
}

static void
sn_shortcuts_free (SnShortcut **shortcuts)
{
  guint i;

  if (shortcuts == NULL)
    return;

  for (i = 0; shortcuts[i] != NULL; i++)
    g_free (shortcuts[i]);

  g_free (shortcuts);
}

static void
menu_icon_size_cb (GpApplet       *applet,
                   GParamSpec     *pspec,
                   SnDBusMenuItem *item)
{
  update_icon (item);
}

SnDBusMenuItem *
sn_dbus_menu_item_new (SnApplet *applet,
                       GVariant *props)
{
  SnDBusMenuItem *item;
  GVariantIter iter;
  const gchar *prop;
  GVariant *value;

  item = g_new0 (SnDBusMenuItem, 1);

  item->applet = applet;

  item->enabled = TRUE;
  item->toggle_state = -1;
  item->visible = TRUE;

  g_variant_iter_init (&iter, props);
  while (g_variant_iter_next (&iter, "{&sv}", &prop, &value))
    {
      if (g_strcmp0 (prop, "accessible-desc") == 0)
        item->accessible_desc = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (prop, "children-display") == 0)
        item->children_display = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (prop, "disposition") == 0)
        item->disposition = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (prop, "enabled") == 0)
        item->enabled = g_variant_get_boolean (value);
      else if (g_strcmp0 (prop, "icon-name") == 0)
        item->icon_name = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (prop, "icon-data") == 0)
        item->icon_data = pixbuf_new (value);
      else if (g_strcmp0 (prop, "label") == 0)
        item->label = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (prop, "shortcut") == 0)
        item->shortcuts = sn_shortcuts_new (value);
      else if (g_strcmp0 (prop, "toggle-type") == 0)
        item->toggle_type = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (prop, "toggle-state") == 0)
        item->toggle_state = g_variant_get_int32 (value);
      else if (g_strcmp0 (prop, "type") == 0)
        item->type = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (prop, "visible") == 0)
        item->visible = g_variant_get_boolean (value);
      else
        g_debug ("unknown property '%s'", prop);

      g_variant_unref (value);
    }

  if (g_strcmp0 (item->type, "separator") == 0)
    {
      item->item = gtk_separator_menu_item_new ();
    }
  else
    {
      if (g_strcmp0 (item->toggle_type, "checkmark") == 0)
        {
          item->item = gtk_check_menu_item_new ();
          gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item->item), TRUE);
        }
      else if (g_strcmp0 (item->toggle_type, "radio") == 0)
        {
          AtkObject *accessible;

          item->item = gtk_check_menu_item_new ();
          gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (item->item), TRUE);
          gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item->item), TRUE);

          accessible = gtk_widget_get_accessible (item->item);
          atk_object_set_role (accessible, ATK_ROLE_RADIO_MENU_ITEM);
        }
      else
        {
          item->item = gp_image_menu_item_new ();

          item->menu_icon_size_id = g_signal_connect (item->applet,
                                                      "notify::menu-icon-size",
                                                      G_CALLBACK (menu_icon_size_cb),
                                                      item);

          update_icon (item);
        }

      if (g_strcmp0 (item->children_display, "submenu") == 0)
        {
          GtkWidget *submenu;

          submenu = gtk_menu_new ();
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (item->item), submenu);

          item->submenu = GTK_MENU (submenu);
          g_object_ref_sink (item->submenu);
        }

      gtk_menu_item_set_label (GTK_MENU_ITEM (item->item), item->label);
      gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item->item), TRUE);

      if (item->shortcuts)
        {
          guint i;

          for (i = 0; item->shortcuts[i] != NULL; i++)
            {
            }
        }

      if (item->toggle_state != -1 && GTK_IS_CHECK_MENU_ITEM (item->item))
        {
          GtkCheckMenuItem *check;

          check = GTK_CHECK_MENU_ITEM (item->item);

          if (item->toggle_state == 1)
            gtk_check_menu_item_set_active (check, TRUE);
          else if (item->toggle_state == 0)
            gtk_check_menu_item_set_active (check, FALSE);
        }
    }

  gtk_widget_set_sensitive (item->item, item->enabled);
  gtk_widget_set_visible (item->item, item->visible);

  g_object_ref_sink (item->item);
  return item;
}

void
sn_dbus_menu_item_free (gpointer data)
{
  SnDBusMenuItem *item;

  item = (SnDBusMenuItem *) data;
  if (item == NULL)
    return;

  if (item->menu_icon_size_id != 0)
    {
      g_signal_handler_disconnect (item->applet, item->menu_icon_size_id);
      item->menu_icon_size_id = 0;
    }

  g_clear_pointer (&item->accessible_desc, g_free);
  g_clear_pointer (&item->children_display, g_free);
  g_clear_pointer (&item->disposition, g_free);
  g_clear_pointer (&item->icon_name, g_free);
  g_clear_object (&item->icon_data);
  g_clear_pointer (&item->label, g_free);
  g_clear_pointer (&item->shortcuts, sn_shortcuts_free);
  g_clear_pointer (&item->toggle_type, g_free);
  g_clear_pointer (&item->type, g_free);

  gtk_widget_destroy (item->item);
  g_clear_object (&item->item);
  g_clear_object (&item->submenu);

  g_free (item);
}

void
sn_dbus_menu_item_update_props (SnDBusMenuItem *item,
                                GVariant       *props)
{
  GVariantIter iter;
  const gchar *prop;
  GVariant *value;

  g_variant_iter_init (&iter, props);
  while (g_variant_iter_next (&iter, "{&sv}", &prop, &value))
    {
      if (g_strcmp0 (prop, "accessible-desc") == 0)
        {
          g_free (item->accessible_desc);
          item->accessible_desc = g_variant_dup_string (value, NULL);
        }
      else if (g_strcmp0 (prop, "children-display") == 0)
        {
          g_free (item->children_display);
          item->children_display = g_variant_dup_string (value, NULL);
        }
      else if (g_strcmp0 (prop, "disposition") == 0)
        {
          g_free (item->disposition);
          item->disposition = g_variant_dup_string (value, NULL);
        }
      else if (g_strcmp0 (prop, "enabled") == 0)
        {
          item->enabled = g_variant_get_boolean (value);
          gtk_widget_set_sensitive (item->item, item->enabled);
        }
      else if (g_strcmp0 (prop, "icon-name") == 0)
        {
          g_free (item->icon_name);
          item->icon_name = g_variant_dup_string (value, NULL);

          update_icon (item);
        }
      else if (g_strcmp0 (prop, "icon-data") == 0)
        {
          g_clear_object (&item->icon_data);
          item->icon_data = pixbuf_new (value);

          update_icon (item);
        }
      else if (g_strcmp0 (prop, "label") == 0)
        {
          g_free (item->label);
          item->label = g_variant_dup_string (value, NULL);

          if (!GTK_IS_SEPARATOR_MENU_ITEM (item->item))
            gtk_menu_item_set_label (GTK_MENU_ITEM (item->item), item->label);
        }
      else if (g_strcmp0 (prop, "shortcut") == 0)
        {
          sn_shortcuts_free (item->shortcuts);
          item->shortcuts = sn_shortcuts_new (value);
        }
      else if (g_strcmp0 (prop, "toggle-type") == 0)
        {
          g_free (item->toggle_type);
          item->toggle_type = g_variant_dup_string (value, NULL);
        }
      else if (g_strcmp0 (prop, "toggle-state") == 0)
        {
          item->toggle_state = g_variant_get_int32 (value);

          if (item->toggle_state != -1 && GTK_IS_CHECK_MENU_ITEM (item->item))
            {
              GtkCheckMenuItem *check;

              check = GTK_CHECK_MENU_ITEM (item->item);

              g_signal_handler_block (item->item, item->activate_id);

              if (item->toggle_state == 1)
                gtk_check_menu_item_set_active (check, TRUE);
              else if (item->toggle_state == 0)
                gtk_check_menu_item_set_active (check, FALSE);

              g_signal_handler_unblock (item->item, item->activate_id);
            }
        }
      else if (g_strcmp0 (prop, "type") == 0)
        {
          g_free (item->type);
          item->type = g_variant_dup_string (value, NULL);
        }
      else if (g_strcmp0 (prop, "visible") == 0)
        {
          item->visible = g_variant_get_boolean (value);
          gtk_widget_set_visible (item->item, item->visible);
        }
      else
        {
          g_debug ("updating unknown property - '%s'", prop);
        }

      g_variant_unref (value);
    }
}

void
sn_dbus_menu_item_remove_props (SnDBusMenuItem *item,
                                GVariant       *props)
{
  GVariantIter iter;
  const gchar *prop;

  g_variant_iter_init (&iter, props);
  while (g_variant_iter_next (&iter, "&s", &prop))
    {
      if (g_strcmp0 (prop, "accessible-desc") == 0)
        {
          g_clear_pointer (&item->accessible_desc, g_free);
        }
      else if (g_strcmp0 (prop, "children-display") == 0)
        {
          g_clear_pointer (&item->children_display, g_free);
        }
      else if (g_strcmp0 (prop, "disposition") == 0)
        {
          g_clear_pointer (&item->disposition, g_free);
        }
      else if (g_strcmp0 (prop, "enabled") == 0)
        {
          item->enabled = TRUE;
          gtk_widget_set_sensitive (item->item, item->enabled);
        }
      else if (g_strcmp0 (prop, "icon-name") == 0)
        {
          g_clear_pointer (&item->icon_name, g_free);
          update_icon (item);
        }
      else if (g_strcmp0 (prop, "icon-data") == 0)
        {
          g_clear_object (&item->icon_data);
          update_icon (item);
        }
      else if (g_strcmp0 (prop, "label") == 0)
        {
          g_clear_pointer (&item->label, g_free);
          if (!GTK_IS_SEPARATOR_MENU_ITEM (item->item))
            gtk_menu_item_set_label (GTK_MENU_ITEM (item->item), item->label);
        }
      else if (g_strcmp0 (prop, "shortcut") == 0)
        {
          g_clear_pointer (&item->shortcuts, sn_shortcuts_free);
        }
      else if (g_strcmp0 (prop, "toggle-type") == 0)
        {
          g_clear_pointer (&item->toggle_type, g_free);
        }
      else if (g_strcmp0 (prop, "toggle-state") == 0)
        {
          item->toggle_state = -1;
        }
      else if (g_strcmp0 (prop, "type") == 0)
        {
          g_clear_pointer (&item->type, g_free);
        }
      else if (g_strcmp0 (prop, "visible") == 0)
        {
          item->visible = TRUE;
          gtk_widget_set_visible (item->item, item->visible);
        }
      else
        {
          g_debug ("removing unknown property - '%s'", prop);
        }
    }
}

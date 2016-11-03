/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#include "sn-item-v0.h"
#include "sn-item-v0-gen.h"

#define SN_ITEM_INTERFACE "org.kde.StatusNotifierItem"

typedef struct
{
  gchar      *icon_name;
  GdkPixbuf **icon_pixmap;
  gchar      *title;
  gchar      *text;
} SnTooltip;

struct _SnItemV0
{
  SnItem         parent;

  GCancellable  *cancellable;
  SnItemV0Gen   *proxy;

  gchar         *id;
  gchar         *category;
  gchar         *status;

  gchar         *title;
  gint32         window_id;
  gchar         *icon_name;
  GdkPixbuf    **icon_pixmap;
  gchar         *overlay_icon_name;
  GdkPixbuf    **overlay_icon_pixmap;
  gchar         *attention_icon_name;
  GdkPixbuf    **attention_icon_pixmap;
  gchar         *attention_movie_name;
  SnTooltip     *tooltip;
  gchar         *icon_theme_path;
  gchar         *menu;
  gboolean       item_is_menu;

  guint          update_id;
};

G_DEFINE_TYPE (SnItemV0, sn_item_v0, SN_TYPE_ITEM)

static void
update (SnItemV0 *v0)
{
  gboolean visible;

  visible = g_strcmp0 (v0->status, "Passive") != 0;
  gtk_widget_set_visible (GTK_WIDGET (v0), visible);
}

static gboolean
update_cb (gpointer user_data)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (user_data);

  v0->update_id = 0;
  update (v0);

  return G_SOURCE_REMOVE;
}

static void
queue_update (SnItemV0 *v0)
{
  if (v0->update_id != 0)
    return;

  v0->update_id = g_timeout_add (10, update_cb, v0);
  g_source_set_name_by_id (v0->update_id, "[status-notifier] update_cb");
}

static GdkPixbuf **
icon_pixmap_new (GVariant *variant)
{
  GVariantIter iter;
  gsize n_pixbufs;
  GdkPixbuf **pixbufs;
  guint i;
  gint width;
  gint height;
  GVariant *value;

  if (variant == NULL)
    return NULL;

  n_pixbufs = g_variant_iter_init (&iter, variant);
  if (n_pixbufs == 0)
    return NULL;

  pixbufs = g_new0 (GdkPixbuf *, n_pixbufs + 1);
  i = 0;

  while (g_variant_iter_next (&iter, "(ii@ay)", &width, &height, &value))
    {
      GBytes *bytes;
      gint rowstride;

      bytes = g_variant_get_data_as_bytes (value);
      rowstride = g_bytes_get_size (bytes) / height;

      pixbufs[i++] = gdk_pixbuf_new_from_bytes (bytes, GDK_COLORSPACE_RGB,
                                                TRUE, 8, width, height,
                                                rowstride);

      g_bytes_unref (bytes);
      g_variant_unref (value);
    }

  pixbufs[i] = NULL;

  return pixbufs;
}

static void
icon_pixmap_free (GdkPixbuf **data)
{
  gint i;

  if (data == NULL)
    return;

  for (i = 0; data[i] != NULL; i++)
    g_object_unref (data[i]);

  g_free (data);
}

static SnTooltip *
sn_tooltip_new (GVariant *variant)
{
  const gchar *icon_name;
  GVariant *icon_pixmap;
  const gchar *title;
  const gchar *text;
  SnTooltip *tooltip;

  if (variant == NULL)
    return NULL;

  g_variant_get (variant, "(&s@a(iiay)&s&s)",
                 &icon_name, &icon_pixmap,
                 &title, &text);

  tooltip = g_new0 (SnTooltip, 1);

  tooltip->icon_name = g_strdup (icon_name);
  tooltip->icon_pixmap = icon_pixmap_new (icon_pixmap);
  tooltip->title = g_strdup (title);
  tooltip->text = g_strdup (text);

  g_variant_unref (icon_pixmap);
  return tooltip;
}

static void
sn_tooltip_free (SnTooltip *tooltip)
{
  if (tooltip == NULL)
    return;

  g_free (tooltip->icon_name);
  icon_pixmap_free (tooltip->icon_pixmap);
  g_free (tooltip->title);
  g_free (tooltip->text);

  g_free (tooltip);
}

static GVariant *
get_property (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data,
              gboolean     *cancelled)
{
  GVariant *variant;
  GError *error;
  GVariant *property;

  error = NULL;
  variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                           res, &error);

  *cancelled = FALSE;
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      *cancelled = TRUE;
      g_error_free (error);
      return NULL;
    }

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  g_variant_get (variant, "(v)", &property);
  g_variant_unref (variant);

  return property;
}

static void
update_property (SnItemV0            *v0,
                 const gchar         *property,
                 GAsyncReadyCallback  callback)
{
  GDBusProxy *proxy;
  SnItem *item;

  proxy = G_DBUS_PROXY (v0->proxy);
  item = SN_ITEM (v0);

  g_dbus_connection_call (g_dbus_proxy_get_connection (proxy),
                          sn_item_get_bus_name (item),
                          sn_item_get_object_path (item),
                          "org.freedesktop.DBus.Properties", "Get",
                          g_variant_new ("(ss)", SN_ITEM_INTERFACE, property),
                          G_VARIANT_TYPE ("(v)"),
                          G_DBUS_CALL_FLAGS_NONE, -1,
                          v0->cancellable, callback, v0);
}

static void
update_title (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
  SnItemV0 *v0;
  GVariant *variant;
  gboolean cancelled;

  variant = get_property (source_object, res, user_data, &cancelled);
  if (cancelled)
    return;

  v0 = SN_ITEM_V0 (user_data);

  g_clear_pointer (&v0->title, g_free);
  v0->title = g_variant_dup_string (variant, NULL);
  g_clear_pointer (&variant, g_variant_unref);

  queue_update (v0);
}

static void
new_title_cb (SnItemV0 *v0)
{
  update_property (v0, "Title", update_title);
}

static void
update_icon_name (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  SnItemV0 *v0;
  GVariant *variant;
  gboolean cancelled;

  variant = get_property (source_object, res, user_data, &cancelled);
  if (cancelled)
    return;

  v0 = SN_ITEM_V0 (user_data);

  g_clear_pointer (&v0->icon_name, g_free);
  v0->icon_name = g_variant_dup_string (variant, NULL);
  g_clear_pointer (&variant, g_variant_unref);

  queue_update (v0);
}

static void
update_icon_pixmap (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  SnItemV0 *v0;
  GVariant *variant;
  gboolean cancelled;

  variant = get_property (source_object, res, user_data, &cancelled);
  if (cancelled)
    return;

  v0 = SN_ITEM_V0 (user_data);

  g_clear_pointer (&v0->icon_pixmap, icon_pixmap_free);
  v0->icon_pixmap = icon_pixmap_new (variant);
  g_clear_pointer (&variant, g_variant_unref);

  queue_update (v0);
}

static void
new_icon_cb (SnItemV0 *v0)
{
  update_property (v0, "IconName", update_icon_name);
  update_property (v0, "IconPixmap", update_icon_pixmap);
}

static void
update_overlay_icon_name (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  SnItemV0 *v0;
  GVariant *variant;
  gboolean cancelled;

  variant = get_property (source_object, res, user_data, &cancelled);
  if (cancelled)
    return;

  v0 = SN_ITEM_V0 (user_data);

  g_clear_pointer (&v0->overlay_icon_name, g_free);
  v0->overlay_icon_name = g_variant_dup_string (variant, NULL);
  g_clear_pointer (&variant, g_variant_unref);

  queue_update (v0);
}

static void
update_overlay_icon_pixmap (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  SnItemV0 *v0;
  GVariant *variant;
  gboolean cancelled;

  variant = get_property (source_object, res, user_data, &cancelled);
  if (cancelled)
    return;

  v0 = SN_ITEM_V0 (user_data);

  g_clear_pointer (&v0->overlay_icon_pixmap, icon_pixmap_free);
  v0->overlay_icon_pixmap = icon_pixmap_new (variant);
  g_clear_pointer (&variant, g_variant_unref);

  queue_update (v0);
}

static void
new_overlay_icon_cb (SnItemV0 *v0)
{
  update_property (v0, "OverlayIconName", update_overlay_icon_name);
  update_property (v0, "OverlayIconPixmap", update_overlay_icon_pixmap);
}

static void
update_attention_icon_name (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  SnItemV0 *v0;
  GVariant *variant;
  gboolean cancelled;

  variant = get_property (source_object, res, user_data, &cancelled);
  if (cancelled)
    return;

  v0 = SN_ITEM_V0 (user_data);

  g_clear_pointer (&v0->attention_icon_name, g_free);
  v0->attention_icon_name = g_variant_dup_string (variant, NULL);
  g_clear_pointer (&variant, g_variant_unref);

  queue_update (v0);
}

static void
update_attention_icon_pixmap (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  SnItemV0 *v0;
  GVariant *variant;
  gboolean cancelled;

  variant = get_property (source_object, res, user_data, &cancelled);
  if (cancelled)
    return;

  v0 = SN_ITEM_V0 (user_data);

  g_clear_pointer (&v0->attention_icon_pixmap, icon_pixmap_free);
  v0->attention_icon_pixmap = icon_pixmap_new (variant);
  g_clear_pointer (&variant, g_variant_unref);

  queue_update (v0);
}

static void
new_attention_icon_cb (SnItemV0 *v0)
{
  update_property (v0, "AttentionIconName", update_attention_icon_name);
  update_property (v0, "AttentionIconPixmap", update_attention_icon_pixmap);
}

static void
update_tooltip (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  SnItemV0 *v0;
  GVariant *variant;
  gboolean cancelled;

  variant = get_property (source_object, res, user_data, &cancelled);
  if (cancelled)
    return;

  v0 = SN_ITEM_V0 (user_data);

  g_clear_pointer (&v0->tooltip, sn_tooltip_free);
  v0->tooltip = sn_tooltip_new (variant);
  g_clear_pointer (&variant, g_variant_unref);

  queue_update (v0);
}

static void
new_tooltip_cb (SnItemV0 *v0)
{
  update_property (v0, "ToolTip", update_tooltip);
}

static void
new_status_cb (SnItemV0 *v0,
               GVariant *parameters)
{
  GVariant *variant;

  variant = g_variant_get_child_value (parameters, 0);

  g_free (v0->status);
  v0->status = g_variant_dup_string (variant, NULL);
  g_variant_unref (variant);

  queue_update (v0);
}

static void
new_icon_theme_path_cb (SnItemV0 *v0,
                        GVariant *parameters)
{
  GVariant *variant;

  variant = g_variant_get_child_value (parameters, 0);

  g_free (v0->icon_theme_path);
  v0->icon_theme_path = g_variant_dup_string (variant, NULL);
  g_variant_unref (variant);

  queue_update (v0);
}

static void
g_properties_changed_cb (GDBusProxy *proxy,
                         GVariant   *changed_properties,
                         GStrv       invalidated_properties,
                         SnItemV0   *v0)
{
  gchar *debug;

  debug = g_variant_print (changed_properties, FALSE);
  g_debug ("g_properties_changed_cb: %s", debug);
  g_free (debug);
}

static void
g_signal_cb (GDBusProxy *proxy,
             gchar      *sender_name,
             gchar      *signal_name,
             GVariant   *parameters,
             SnItemV0   *v0)
{
  if (g_strcmp0 (signal_name, "NewTitle") == 0)
    new_title_cb (v0);
  else if (g_strcmp0 (signal_name, "NewIcon") == 0)
    new_icon_cb (v0);
  else if (g_strcmp0 (signal_name, "NewOverlayIcon") == 0)
    new_overlay_icon_cb (v0);
  else if (g_strcmp0 (signal_name, "NewAttentionIcon") == 0)
    new_attention_icon_cb (v0);
  else if (g_strcmp0 (signal_name, "NewToolTip") == 0)
    new_tooltip_cb (v0);
  else if (g_strcmp0 (signal_name, "NewStatus") == 0)
    new_status_cb (v0, parameters);
  else if (g_strcmp0 (signal_name, "NewIconThemePath") == 0)
    new_icon_theme_path_cb (v0, parameters);
  else
    g_assert_not_reached ();
}

static void
get_all_cb (GObject      *source_object,
            GAsyncResult *res,
            gpointer      user_data)
{
  SnItemV0 *v0;
  GVariant *properties;
  GError *error;
  GVariantIter *iter;
  gchar *key;
  GVariant *value;

  error = NULL;
  properties = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                              res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  v0 = SN_ITEM_V0 (user_data);

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  g_variant_get (properties, "(a{sv})", &iter);
  while (g_variant_iter_next (iter, "{sv}", &key, &value))
    {
      if (g_strcmp0 (key, "Category") == 0)
        v0->category = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "Id") == 0)
        v0->id = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "Title") == 0)
        v0->title = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "Status") == 0)
        v0->status = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "WindowId") == 0)
        v0->window_id = g_variant_get_int32 (value);
      else if (g_strcmp0 (key, "IconName") == 0)
        v0->icon_name = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "IconPixmap") == 0)
        v0->icon_pixmap = icon_pixmap_new (value);
      else if (g_strcmp0 (key, "OverlayIconName") == 0)
        v0->overlay_icon_name = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "OverlayIconPixmap") == 0)
        v0->overlay_icon_pixmap = icon_pixmap_new (value);
      else if (g_strcmp0 (key, "AttentionIconName") == 0)
        v0->attention_icon_name = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "AttentionIconPixmap") == 0)
        v0->attention_icon_pixmap = icon_pixmap_new (value);
      else if (g_strcmp0 (key, "AttentionMovieName") == 0)
        v0->attention_movie_name = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "ToolTip") == 0)
        v0->tooltip = sn_tooltip_new (value);
      else if (g_strcmp0 (key, "IconThemePath") == 0)
        v0->icon_theme_path = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "Menu") == 0)
        v0->menu = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "ItemIsMenu") == 0)
        v0->item_is_menu = g_variant_get_boolean (value);
      else
        g_assert_not_reached ();
    }

  g_variant_iter_free (iter);
  g_variant_unref (properties);

  if (v0->id == NULL || v0->category == NULL || v0->status == NULL)
    {
      SnItem *item;
      const gchar *bus_name;
      const gchar *object_path;

      item = SN_ITEM (v0);
      bus_name = sn_item_get_bus_name (item);
      object_path = sn_item_get_object_path (item);

      g_warning ("Invalid Status Notifier Item (%s, %s)",
                 bus_name, object_path);

      return;
    }

  g_signal_connect (v0->proxy, "g-properties-changed",
                    G_CALLBACK (g_properties_changed_cb), v0);

  g_signal_connect (v0->proxy, "g-signal",
                    G_CALLBACK (g_signal_cb), v0);

  update (v0);
  sn_item_emit_ready (SN_ITEM (v0));
}

static void
proxy_ready_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  SnItemV0 *v0;
  SnItemV0Gen *proxy;
  GError *error;

  error = NULL;
  proxy = sn_item_v0_gen_proxy_new_for_bus_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  v0 = SN_ITEM_V0 (user_data);
  v0->proxy = proxy;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  g_dbus_connection_call (g_dbus_proxy_get_connection (G_DBUS_PROXY (proxy)),
                          sn_item_get_bus_name (SN_ITEM (v0)),
                          sn_item_get_object_path (SN_ITEM (v0)),
                          "org.freedesktop.DBus.Properties", "GetAll",
                          g_variant_new ("(s)", SN_ITEM_INTERFACE),
                          G_VARIANT_TYPE ("(a{sv})"),
                          G_DBUS_CALL_FLAGS_NONE, -1,
                          v0->cancellable, get_all_cb, v0);
}

static void
sn_item_v0_constructed (GObject *object)
{
  SnItemV0 *v0;
  SnItem *item;

  v0 = SN_ITEM_V0 (object);
  item = SN_ITEM (v0);

  G_OBJECT_CLASS (sn_item_v0_parent_class)->constructed (object);

  v0->cancellable = g_cancellable_new ();
  sn_item_v0_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                    G_DBUS_PROXY_FLAGS_NONE,
                                    sn_item_get_bus_name (item),
                                    sn_item_get_object_path (item),
                                    v0->cancellable,
                                    proxy_ready_cb, v0);
}

static void
sn_item_v0_dispose (GObject *object)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (object);

  g_cancellable_cancel (v0->cancellable);
  g_clear_object (&v0->cancellable);
  g_clear_object (&v0->proxy);

  if (v0->update_id != 0)
    {
      g_source_remove (v0->update_id);
      v0->update_id = 0;
    }

  G_OBJECT_CLASS (sn_item_v0_parent_class)->dispose (object);
}

static void
sn_item_v0_finalize (GObject *object)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (object);

  g_clear_pointer (&v0->id, g_free);
  g_clear_pointer (&v0->category, g_free);
  g_clear_pointer (&v0->status, g_free);

  g_clear_pointer (&v0->title, g_free);
  g_clear_pointer (&v0->icon_name, g_free);
  g_clear_pointer (&v0->icon_pixmap, icon_pixmap_free);
  g_clear_pointer (&v0->overlay_icon_name, g_free);
  g_clear_pointer (&v0->overlay_icon_pixmap, icon_pixmap_free);
  g_clear_pointer (&v0->attention_icon_name, g_free);
  g_clear_pointer (&v0->attention_icon_pixmap, icon_pixmap_free);
  g_clear_pointer (&v0->attention_movie_name, g_free);
  g_clear_pointer (&v0->tooltip, sn_tooltip_free);
  g_clear_pointer (&v0->icon_theme_path, g_free);
  g_clear_pointer (&v0->menu, g_free);

  G_OBJECT_CLASS (sn_item_v0_parent_class)->finalize (object);
}

static const gchar *
sn_item_v0_get_id (SnItem *item)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (item);

  return v0->id;
}

static const gchar *
sn_item_v0_get_category (SnItem *item)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (item);

  return v0->category;
}

static void
sn_item_v0_class_init (SnItemV0Class *v0_class)
{
  GObjectClass *object_class;
  SnItemClass *item_class;

  object_class = G_OBJECT_CLASS (v0_class);
  item_class = SN_ITEM_CLASS (v0_class);

  object_class->constructed = sn_item_v0_constructed;
  object_class->dispose = sn_item_v0_dispose;
  object_class->finalize = sn_item_v0_finalize;

  item_class->get_id = sn_item_v0_get_id;
  item_class->get_category = sn_item_v0_get_category;
}

static void
sn_item_v0_init (SnItemV0 *v0)
{
}

SnItem *
sn_item_v0_new (const gchar *bus_name,
                const gchar *object_path)
{
  return g_object_new (SN_TYPE_ITEM_V0,
                       "bus-name", bus_name,
                       "object-path", object_path,
                       NULL);
}

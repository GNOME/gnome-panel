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

#include <math.h>

#include "sn-item.h"
#include "sn-item-v0.h"
#include "sn-item-v0-gen.h"

#define SN_ITEM_INTERFACE "org.kde.StatusNotifierItem"

typedef struct
{
  cairo_surface_t *surface;
  gint             width;
  gint             height;
} SnIconPixmap;

typedef struct
{
  gchar         *icon_name;
  SnIconPixmap **icon_pixmap;
  gchar         *title;
  gchar         *text;
} SnTooltip;

struct _SnItemV0
{
  SnItem         parent;

  GtkWidget     *image;

  GCancellable  *cancellable;
  SnItemV0Gen   *proxy;

  gchar         *id;
  gchar         *category;
  gchar         *status;

  gchar         *title;
  guint32        window_id;
  gchar         *icon_name;
  SnIconPixmap **icon_pixmap;
  gchar         *overlay_icon_name;
  SnIconPixmap **overlay_icon_pixmap;
  gchar         *attention_icon_name;
  SnIconPixmap **attention_icon_pixmap;
  gchar         *attention_movie_name;
  SnTooltip     *tooltip;
  gchar         *icon_theme_path;
  gchar         *menu;
  gboolean       item_is_menu;

  guint          update_id;

  gulong         panel_icon_size_id;
};

G_DEFINE_TYPE (SnItemV0, sn_item_v0, SN_TYPE_ITEM)

static cairo_surface_t *
scale_surface (SnIconPixmap   *pixmap,
               GtkOrientation  orientation,
               gint            size)
{
  gdouble ratio;
  gdouble new_width;
  gdouble new_height;
  gdouble scale_x;
  gdouble scale_y;
  gint width;
  gint height;
  cairo_content_t content;
  cairo_surface_t *scaled;
  cairo_t *cr;

  ratio = pixmap->width / (gdouble) pixmap->height;
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      new_height = (gdouble) size;
      new_width = new_height * ratio;
    }
  else
    {
      new_width = (gdouble) size;
      new_height = new_width * ratio;
    }

  scale_x = new_width / pixmap->width;
  scale_y = new_height / pixmap->height;

  width = ceil (new_width);
  height = ceil (new_height);

  content = CAIRO_CONTENT_COLOR_ALPHA;
  scaled = cairo_surface_create_similar (pixmap->surface, content, width, height);
  cr = cairo_create (scaled);

  cairo_scale (cr, scale_x, scale_y);
  cairo_set_source_surface (cr, pixmap->surface, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
  return scaled;
}

static gint
compare_size (gconstpointer a,
              gconstpointer b,
              gpointer      user_data)
{
  SnIconPixmap *p1;
  SnIconPixmap *p2;
  GtkOrientation orientation;

  p1 = (SnIconPixmap *) a;
  p2 = (SnIconPixmap *) b;
  orientation = GPOINTER_TO_UINT (user_data);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    return p1->height - p2->height;
  else
    return p1->width - p2->width;
}

static GList *
get_pixmaps_sorted (SnItemV0       *v0,
                    GtkOrientation  orientation,
                    gint            size)
{
  GList *pixmaps;
  guint i;

  pixmaps = NULL;
  for (i = 0; v0->icon_pixmap[i] != NULL; i++)
    pixmaps = g_list_prepend (pixmaps, v0->icon_pixmap[i]);

  return g_list_sort_with_data (pixmaps, compare_size,
                                GUINT_TO_POINTER (orientation));
}

static cairo_surface_t *
get_surface (SnItemV0       *v0,
             GtkOrientation  orientation,
             gint            size)
{
  GList *pixmaps;
  cairo_surface_t *surface;
  SnIconPixmap *best;
  GList *l;

  g_assert (v0->icon_pixmap != NULL && v0->icon_pixmap[0] != NULL);

  pixmaps = get_pixmaps_sorted (v0, orientation, size);
  surface = NULL;
  best = NULL;

  for (l = pixmaps; l != NULL; l = l->next)
    {
      SnIconPixmap *pixmap;

      pixmap = (SnIconPixmap *) l->data;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (pixmap->height == size)
            {
              surface = pixmap->surface;
              break;
            }
          else if (pixmap->height > size)
            {
              best = pixmap;
              break;
            }
        }
      else
        {
          if (pixmap->width == size)
            {
              surface = pixmap->surface;
              break;
            }
          else if (pixmap->width > size)
            {
              best = pixmap;
              break;
            }
        }

      best = pixmap;
    }

  g_list_free (pixmaps);

  g_assert (surface != NULL || best != NULL);

  if (surface != NULL)
    return cairo_surface_reference (surface);

  return scale_surface (best, orientation, size);
}

static void
update (SnItemV0 *v0)
{
  GtkImage *image;
  SnApplet *applet;
  guint icon_size;
  SnTooltip *tip;
  gboolean visible;

  image = GTK_IMAGE (v0->image);

  applet = sn_item_get_applet (SN_ITEM (v0));
  icon_size = gp_applet_get_panel_icon_size (GP_APPLET (applet));

  if (v0->icon_name != NULL && *v0->icon_name != '\0')
    {
      GtkIconTheme *icon_theme;

      icon_theme = gtk_icon_theme_get_default ();

      gtk_icon_theme_rescan_if_needed (icon_theme);
      gtk_image_set_from_icon_name (image, v0->icon_name, GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (image, icon_size);
    }
  else if (v0->icon_pixmap != NULL && v0->icon_pixmap[0] != NULL)
    {
      cairo_surface_t *surface;

      surface = get_surface (v0, sn_item_get_orientation (SN_ITEM (v0)), icon_size);
      gtk_image_set_from_surface (image, surface);
      cairo_surface_destroy (surface);
    }
  else
    {
      gtk_image_set_from_icon_name (image, "image-missing", GTK_ICON_SIZE_MENU);
      gtk_image_set_pixel_size (image, icon_size);
    }

  tip = v0->tooltip;

  if (tip != NULL)
    {
      gchar *markup;

      markup = NULL;

      if ((tip->title != NULL && *tip->title != '\0') &&
          (tip->text != NULL && *tip->text != '\0'))
        {
          markup = g_strdup_printf ("%s\n%s", tip->title, tip->text);
        }
      else if (tip->title != NULL && *tip->title != '\0')
        {
          markup = g_strdup (tip->title);
        }
      else if (tip->text != NULL && *tip->text != '\0')
        {
          markup = g_strdup (tip->text);
        }

      gtk_widget_set_tooltip_markup (GTK_WIDGET (v0), markup);
      g_free (markup);

      if (markup != NULL)
        {
          g_object_bind_property (applet, "enable-tooltips",
                                  v0, "has-tooltip",
                                  G_BINDING_DEFAULT |
                                  G_BINDING_SYNC_CREATE);
        }
    }
  else
    {
      gtk_widget_set_tooltip_markup (GTK_WIDGET (v0), NULL);
    }

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

static cairo_surface_t *
surface_from_variant (GVariant *variant,
                      gint      width,
                      gint      height)
{
  cairo_format_t format;
  gint stride;
  guint32 *data;
  guchar *p;
  gint y;

  format = CAIRO_FORMAT_ARGB32;
  stride = cairo_format_stride_for_width (format, width);
  data = (guint32 *) g_variant_get_data (variant);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  {
    gint i;

    for (i = 0; i < width * height; i++)
      data[i] = GUINT32_FROM_BE (data[i]);
  }
#endif

  /* CAIRO_FORMAT_ARGB32 wants premultiplied alpha, but data sent from
   * client is not (and must not be) premultiplied alpha.
   */

  p = (guchar *) data;
  for (y = 0; y < height; y++)
    {
      gint x;

      for (x = 0; x < width; x++)
        {
          guchar alpha;

          alpha = p[x * 4 + 3];

          p[x * 4 + 0] = (p[x * 4 + 0] * alpha) / 255;
          p[x * 4 + 1] = (p[x * 4 + 1] * alpha) / 255;
          p[x * 4 + 2] = (p[x * 4 + 2] * alpha) / 255;
        }

      p += stride;
    }

  return cairo_image_surface_create_for_data ((guchar *) data, format,
                                              width, height, stride);
}

static cairo_surface_t *
icon_surface_new (GVariant *variant,
                  gint      width,
                  gint      height)
{
  cairo_surface_t *surface;
  cairo_surface_t *tmp;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    return NULL;

  tmp = surface_from_variant (variant, width, height);
  if (cairo_surface_status (tmp) != CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (surface);
      return NULL;
    }

  cr = cairo_create (surface);
  if (cairo_status (cr) != CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (surface);
      cairo_surface_destroy (tmp);
      return NULL;
    }

  cairo_set_source_surface (cr, tmp, 0, 0);
  cairo_paint (cr);

  cairo_surface_destroy (tmp);
  cairo_destroy (cr);

  return surface;
}

static SnIconPixmap **
icon_pixmap_new (GVariant *variant)
{
  GPtrArray *array;
  GVariantIter iter;
  gint width;
  gint height;
  GVariant *value;

  if (variant == NULL || g_variant_iter_init (&iter, variant) == 0)
    return NULL;

  array = g_ptr_array_new ();
  while (g_variant_iter_next (&iter, "(ii@ay)", &width, &height, &value))
    {
      cairo_surface_t *surface;

      if (width == 0 || height == 0)
        {
          g_variant_unref (value);
          continue;
        }

      surface = icon_surface_new (value, width, height);
      g_variant_unref (value);

      if (surface != NULL)
        {
          SnIconPixmap *pixmap;

          pixmap = g_new0 (SnIconPixmap, 1);

          pixmap->surface = surface;
          pixmap->width = width;
          pixmap->height = height;

          g_ptr_array_add (array, pixmap);
        }
    }

  g_ptr_array_add (array, NULL);
  return (SnIconPixmap **) g_ptr_array_free (array, FALSE);
}

static void
icon_pixmap_free (SnIconPixmap **data)
{
  gint i;

  if (data == NULL)
    return;

  for (i = 0; data[i] != NULL; i++)
    {
      cairo_surface_destroy (data[i]->surface);
      g_free (data[i]);
    }

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

  if (!g_variant_is_of_type (variant, G_VARIANT_TYPE ("(sa(iiay)ss)")))
    {
      g_warning ("Type for 'ToolTip' property should be '(sa(iiay)ss)' "
                 "but got '%s'", g_variant_get_type_string (variant));

      return NULL;
    }

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

  if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS))
    {
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

  if (v0->icon_theme_path != NULL)
    {
      GtkIconTheme *icon_theme;

      icon_theme = gtk_icon_theme_get_default ();

      gtk_icon_theme_append_search_path (icon_theme, v0->icon_theme_path);
    }

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
    g_debug ("signal '%s' not handled!", signal_name);
}

static void
panel_icon_size_cb (GpApplet   *applet,
                    GParamSpec *pspec,
                    SnItemV0   *v0)
{
  queue_update (v0);
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
  SnApplet *applet;

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
        {
          if (g_variant_is_of_type (value, G_VARIANT_TYPE ("u")))
            v0->window_id = g_variant_get_uint32 (value);
          else
            v0->window_id = 0;
        }
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
        g_debug ("property '%s' not handled!", key);

      g_variant_unref (value);
      g_free (key);
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

  if (v0->icon_theme_path != NULL)
    {
      GtkIconTheme *icon_theme;

      icon_theme = gtk_icon_theme_get_default ();

      gtk_icon_theme_append_search_path (icon_theme, v0->icon_theme_path);
    }

  g_signal_connect (v0->proxy, "g-properties-changed",
                    G_CALLBACK (g_properties_changed_cb), v0);

  g_signal_connect (v0->proxy, "g-signal",
                    G_CALLBACK (g_signal_cb), v0);

  applet = sn_item_get_applet (SN_ITEM (v0));
  v0->panel_icon_size_id = g_signal_connect (applet, "notify::panel-icon-size",
                                             G_CALLBACK (panel_icon_size_cb), v0);

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

  if (v0->panel_icon_size_id != 0)
    {
      SnApplet *applet;

      applet = sn_item_get_applet (SN_ITEM (v0));

      g_signal_handler_disconnect (applet, v0->panel_icon_size_id);
      v0->panel_icon_size_id = 0;
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

static const gchar *
sn_item_v0_get_menu (SnItem *item)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (item);

  return v0->menu;
}

static void
context_menu_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (user_data);

  sn_item_v0_gen_call_context_menu_finish (v0->proxy, res, NULL);
}

static void
sn_item_v0_context_menu (SnItem *item,
                         gint    x,
                         gint    y)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (item);

  sn_item_v0_gen_call_context_menu (v0->proxy, x, y, NULL,
                                    context_menu_cb, v0);
}

static void
activate_cb (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (user_data);

  sn_item_v0_gen_call_activate_finish (v0->proxy, res, NULL);
}

static void
sn_item_v0_activate (SnItem *item,
                     gint    x,
                     gint    y)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (item);

  sn_item_v0_gen_call_activate (v0->proxy, x, y, NULL,
                                activate_cb, v0);
}

static void
secondary_activate_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (user_data);

  sn_item_v0_gen_call_secondary_activate_finish (v0->proxy, res, NULL);
}

static void
sn_item_v0_secondary_activate (SnItem *item,
                               gint    x,
                               gint    y)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (item);

  sn_item_v0_gen_call_secondary_activate (v0->proxy, x, y, NULL,
                                          secondary_activate_cb, v0);
}

static void
scroll_cb (GObject      *source_object,
           GAsyncResult *res,
           gpointer      user_data)
{
  SnItemV0 *v0;

  v0 = SN_ITEM_V0 (user_data);

  sn_item_v0_gen_call_scroll_finish (v0->proxy, res, NULL);
}

static void
sn_item_v0_scroll (SnItem            *item,
                   gint               delta,
                   SnItemOrientation  orientation)
{
  SnItemV0 *v0;
  const gchar *tmp;

  v0 = SN_ITEM_V0 (item);

  switch (orientation)
    {
      case SN_ITEM_ORIENTATION_VERTICAL:
        tmp = "Vertical";
        break;

      case SN_ITEM_ORIENTATION_HORIZONTAL:
      default:
        tmp = "Horizontal";
        break;
    }

  sn_item_v0_gen_call_scroll (v0->proxy, delta, tmp, NULL, scroll_cb, v0);
}

static void
sn_item_v0_class_init (SnItemV0Class *v0_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  SnItemClass *item_class;

  object_class = G_OBJECT_CLASS (v0_class);
  widget_class = GTK_WIDGET_CLASS (v0_class);
  item_class = SN_ITEM_CLASS (v0_class);

  object_class->constructed = sn_item_v0_constructed;
  object_class->dispose = sn_item_v0_dispose;
  object_class->finalize = sn_item_v0_finalize;

  item_class->get_id = sn_item_v0_get_id;
  item_class->get_category = sn_item_v0_get_category;
  item_class->get_menu = sn_item_v0_get_menu;

  item_class->context_menu = sn_item_v0_context_menu;
  item_class->activate = sn_item_v0_activate;
  item_class->secondary_activate = sn_item_v0_secondary_activate;
  item_class->scroll = sn_item_v0_scroll;

  gtk_widget_class_set_css_name (widget_class, "sn-item");
}

static void
sn_item_v0_init (SnItemV0 *v0)
{
  v0->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (v0), v0->image);
  gtk_widget_show (v0->image);
}

SnItem *
sn_item_v0_new (SnApplet    *applet,
                const gchar *bus_name,
                const gchar *object_path)
{
  return g_object_new (SN_TYPE_ITEM_V0,
                       "applet", applet,
                       "bus-name", bus_name,
                       "object-path", object_path,
                       NULL);
}

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

#include <gio/gio.h>

#include "gp-theme.h"
#include "panel-enums-gsettings.h"

struct _GpTheme
{
  GObject         parent;

  gchar          *toplevel_id;
  GSettings      *settings;

  GtkCssProvider *css;

  gboolean        composited;
  GtkOrientation  orientation;
};

enum
{
  PROP_0,

  PROP_TOPLEVEL_ID,
  PROP_COMPOSITED,
  PROP_ORIENTATION,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpTheme, gp_theme, G_TYPE_OBJECT)

static gchar *
get_color (GpTheme     *theme,
           const gchar *key)
{
  gchar *color;
  GdkRGBA rgba;

  color = g_settings_get_string (theme->settings, key);
  if (!gdk_rgba_parse (&rgba, color))
    {
      g_free (color);
      return NULL;
    }

  if (theme->composited)
    return color;

  rgba.alpha = 1;

  g_free (color);
  color = gdk_rgba_to_string (&rgba);

  return color;
}

static void
append_custom_bg (GpTheme *theme,
                  GString *string)
{
  gboolean custom_bg_color;
  gboolean custom_bg_image;
  gchar *bg_color;
  gchar *bg_image;

  custom_bg_color = g_settings_get_boolean (theme->settings, "custom-bg-color");
  custom_bg_image = g_settings_get_boolean (theme->settings, "custom-bg-image");

  if (!custom_bg_color && !custom_bg_image)
    return;

  bg_color = get_color (theme, "bg-color");
  bg_image = g_settings_get_string (theme->settings, "bg-image");

  g_string_append_printf (string, "#%s.background {\n", theme->toplevel_id);

  if (custom_bg_color && bg_color != NULL &&
      custom_bg_image && *bg_image != '\0')
    {
      g_string_append_printf (string, "\tbackground-color: %s;\n", bg_color);
      g_string_append_printf (string, "\tbackground-image: url('%s');\n", bg_image);
    }
  else if (custom_bg_color && bg_color != NULL)
    {
      g_string_append_printf (string, "\tbackground: %s;\n", bg_color);
    }
  else if (custom_bg_image && *bg_image != '\0')
    {
      if (theme->composited)
        g_string_append (string, "\tbackground-color: transparent;\n");

      g_string_append_printf (string, "\tbackground-image: url('%s');\n", bg_image);
    }

  if (custom_bg_image && *bg_image != '\0')
    {
      PanelBackgroundImageStyle style;

      style = g_settings_get_enum (theme->settings, "bg-image-style");

      switch (style)
        {
          case PANEL_BACKGROUND_IMAGE_STYLE_STRETCH:
            g_string_append (string, "\tbackground-repeat: no-repeat;\n");
            g_string_append (string, "\tbackground-size: 100% 100%;\n");
            break;

          case PANEL_BACKGROUND_IMAGE_STYLE_FIT:
            if (theme->orientation == GTK_ORIENTATION_HORIZONTAL)
              {
                g_string_append (string, "\tbackground-repeat: repeat-x;\n");
                g_string_append (string, "\tbackground-size: auto 100%;\n");
              }
            else
              {
                g_string_append (string, "\tbackground-repeat: repeat-y;\n");
                g_string_append (string, "\tbackground-size: 100% auto;\n");
              }
            break;

          case PANEL_BACKGROUND_IMAGE_STYLE_NONE:
          default:
            g_string_append (string, "\tbackground-repeat: repeat;\n");
            break;
        }

      if (g_settings_get_boolean (theme->settings, "bg-image-rotate"))
        {
        }
    }

  g_string_append (string, "}\n");

  g_free (bg_image);
  g_free (bg_color);
}

static void
append_custom_fg (GpTheme *theme,
                  GString *string)
{
  gchar *fg_color;

  if (!g_settings_get_boolean (theme->settings, "custom-fg-color"))
    return;

  fg_color = get_color (theme, "fg-color");
  if (fg_color == NULL)
    return;

  g_string_append_printf (string, "#%s .gp-text-color {\n", theme->toplevel_id);
  g_string_append_printf (string, "\tcolor: %s;\n", fg_color);
  g_string_append (string, "}\n");

  g_free (fg_color);
}

static void
changed_cb (GSettings   *settings,
            const gchar *key,
            GpTheme     *theme)
{
  GString *string;
  gchar *css;

  string = g_string_new (NULL);

  append_custom_bg (theme, string);
  append_custom_fg (theme, string);

  css = g_string_free (string, FALSE);
  gtk_css_provider_load_from_data (theme->css, css, -1, NULL);
  g_free (css);
}

static void
gp_theme_constructed (GObject *object)
{
  GpTheme *theme;
  const gchar *schema;
  gchar *path;

  G_OBJECT_CLASS (gp_theme_parent_class)->constructed (object);

  theme = GP_THEME (object);
  schema = "org.gnome.gnome-panel.toplevel.theme";
  path = g_strdup_printf ("/org/gnome/gnome-panel/layout/toplevels/%s/theme/",
                          theme->toplevel_id);

  theme->settings = g_settings_new_with_path (schema, path);
  g_free (path);

  g_signal_connect (theme->settings, "changed", G_CALLBACK (changed_cb), theme);
  changed_cb (theme->settings, NULL, theme);
}

static void
gp_theme_dispose (GObject *object)
{
  GpTheme *theme;

  theme = GP_THEME (object);

  if (theme->css != NULL)
    {
      GdkScreen *screen;
      GtkStyleProvider *provider;

      screen = gdk_screen_get_default ();
      provider = GTK_STYLE_PROVIDER (theme->css);

      gtk_style_context_remove_provider_for_screen (screen, provider);
    }

  g_clear_object (&theme->settings);
  g_clear_object (&theme->css);

  G_OBJECT_CLASS (gp_theme_parent_class)->dispose (object);
}

static void
gp_theme_finalize (GObject *object)
{
  GpTheme *theme;

  theme = GP_THEME (object);

  g_free (theme->toplevel_id);

  G_OBJECT_CLASS (gp_theme_parent_class)->finalize (object);
}

static void
gp_theme_set_property (GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GpTheme *theme;

  theme = GP_THEME (object);

  switch (property_id)
    {
      case PROP_TOPLEVEL_ID:
        theme->toplevel_id = g_value_dup_string (value);
        break;

      case PROP_COMPOSITED:
        theme->composited = g_value_get_boolean (value);
        break;

      case PROP_ORIENTATION:
        theme->orientation = g_value_get_enum (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  properties[PROP_TOPLEVEL_ID] =
    g_param_spec_string ("toplevel-id", "toplevel-id", "toplevel-id",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_COMPOSITED] =
    g_param_spec_boolean ("composited", "composited", "composited",
                          TRUE, G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS);

  properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation", "orientation", "orientation",
                       GTK_TYPE_ORIENTATION, GTK_ORIENTATION_HORIZONTAL,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gp_theme_class_init (GpThemeClass *theme_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (theme_class);

  object_class->constructed = gp_theme_constructed;
  object_class->dispose = gp_theme_dispose;
  object_class->finalize = gp_theme_finalize;
  object_class->set_property = gp_theme_set_property;

  install_properties (object_class);
}

static void
gp_theme_init (GpTheme *theme)
{
  GdkScreen *screen;
  GtkStyleProvider *provider;
  guint priority;

  screen = gdk_screen_get_default ();

  theme->css = gtk_css_provider_new ();
  provider = GTK_STYLE_PROVIDER (theme->css);
  priority = GTK_STYLE_PROVIDER_PRIORITY_USER + 100;

  gtk_style_context_add_provider_for_screen (screen, provider, priority);
}

GpTheme *
gp_theme_new (const gchar    *toplevel_id,
              gboolean        composited,
              GtkOrientation  orientation)
{
  return g_object_new (GP_TYPE_THEME,
                       "toplevel-id", toplevel_id,
                       "composited", composited,
                       "orientation", orientation,
                       NULL);
}

void
gp_theme_set_composited (GpTheme  *theme,
                         gboolean  composited)
{
  if (theme->composited == composited)
    return;

  theme->composited = composited;

  changed_cb (theme->settings, NULL, theme);
}

void
gp_theme_set_orientation (GpTheme        *theme,
                          GtkOrientation  orientation)
{
  if (theme->orientation == orientation)
    return;

  theme->orientation = orientation;

  changed_cb (theme->settings, NULL, theme);
}

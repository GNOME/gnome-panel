/*
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "gp-application.h"

#include <gtk/gtk.h>

#include "panel-enums-gsettings.h"
#include "panel-layout.h"
#include "panel-toplevel.h"

typedef struct
{
  const char *name;
  const char *dir;
  const char *variant;
  gboolean    has_dark_variant;
} GpSupportedTheme;

struct _GpApplication
{
  GObject           parent;

  GSettings        *general_settings;

  gulong            theme_name_id;
  gulong            prefer_dark_id;

  GtkStyleProvider *provider;

  PanelLayout      *layout;

  GHashTable       *toplevels;
};

static void initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GpApplication, gp_application, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init))

static GpSupportedTheme supported_themes[] =
{
  { "Adwaita", "Adwaita", NULL, TRUE },
  { "Adwaita-dark", "Adwaita", "dark", FALSE },
  { "HighContrast", "HighContrast", NULL, FALSE },
  { "HighContrastInverse", "HighContrast", "inverse", FALSE },
  { "Yaru", "Yaru", NULL, FALSE },
  { "Yaru-light", "Yaru", NULL, FALSE },
  { "Yaru-dark", "Yaru", NULL, FALSE },
  { NULL, NULL, FALSE, FALSE }
};

static char *
get_theme_resource (GpSupportedTheme *theme,
                    gboolean          prefer_dark)
{
  char *filename;
  const char *resource_base;
  char *resource;

  if (theme->variant != NULL)
    filename = g_strdup_printf ("gnome-panel-%s.css", theme->variant);
  else if (theme->has_dark_variant && prefer_dark)
    filename = g_strdup ("gnome-panel-dark.css");
  else
    filename = g_strdup ("gnome-panel.css");

  resource_base = "/org/gnome/gnome-panel/theme";
  resource = g_strdup_printf ("%s/%s/%s", resource_base, theme->dir, filename);
  g_free (filename);

  return resource;
}

static gboolean
is_theme_supported (const char        *theme_name,
                    GpSupportedTheme **theme)
{
  int i;

  for (i = 0; supported_themes[i].name != NULL; i++)
    {
      if (g_strcmp0 (supported_themes[i].name, theme_name) == 0)
        {
          *theme = &supported_themes[i];
          return TRUE;
        }
    }

  return FALSE;
}

static void
update_theme (GpApplication *self)
{
  GdkScreen *screen;
  GtkSettings *settings;
  char *theme_name;
  gboolean prefer_dark;
  GpSupportedTheme *theme;
  char *resource;
  guint priority;
  GtkCssProvider *css;

  screen = gdk_screen_get_default ();
  settings = gtk_settings_get_default ();

  if (self->provider != NULL)
    {
      gtk_style_context_remove_provider_for_screen (screen, self->provider);
      g_clear_object (&self->provider);
    }

  g_object_get (settings,
                "gtk-theme-name", &theme_name,
                "gtk-application-prefer-dark-theme", &prefer_dark,
                NULL);

  if (is_theme_supported (theme_name, &theme))
    {
      resource = get_theme_resource (theme, prefer_dark);
      priority = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;
    }
  else
    {
      resource = g_strdup ("/org/gnome/gnome-panel/theme/fallback.css");
      priority = GTK_STYLE_PROVIDER_PRIORITY_FALLBACK;
    }

  css = gtk_css_provider_new ();
  self->provider = GTK_STYLE_PROVIDER (css);

  gtk_css_provider_load_from_resource (css, resource);
  gtk_style_context_add_provider_for_screen (screen, self->provider, priority);

  g_free (theme_name);
  g_free (resource);
}

static void
gtk_theme_name_changed_cb (GtkSettings   *settings,
                           GParamSpec    *pspec,
                           GpApplication *self)
{
  update_theme (self);
}

static void
prefer_dark_changed_cb (GtkSettings   *settings,
                        GParamSpec    *pspec,
                        GpApplication *self)
{
  update_theme (self);
}

static void
theme_variant_changed_cb (GSettings     *settings,
                          const char    *key,
                          GpApplication *self)
{
  PanelThemeVariant variant;

  variant = g_settings_get_enum (settings, key);

  if (variant == PANEL_THEME_VARIANT_SYSTEM)
    {
      gtk_settings_reset_property (gtk_settings_get_default (),
                                   "gtk-application-prefer-dark-theme");
    }
  else
    {
      g_object_set (gtk_settings_get_default (),
                    "gtk-application-prefer-dark-theme",
                    variant == PANEL_THEME_VARIANT_DARK,
                    NULL);
    }

  update_theme (self);
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  GpApplication *self;

  self = GP_APPLICATION (initable);

  return panel_layout_load (self->layout, error);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = initable_init;
}

static void
gp_application_dispose (GObject *object)
{
  GpApplication *self;

  self = GP_APPLICATION (object);

  if (self->theme_name_id != 0)
    {
      g_signal_handler_disconnect (gtk_settings_get_default (),
                                   self->theme_name_id);

      self->theme_name_id = 0;
    }

  if (self->prefer_dark_id != 0)
    {
      g_signal_handler_disconnect (gtk_settings_get_default (),
                                   self->prefer_dark_id);

      self->prefer_dark_id = 0;
    }

  g_clear_object (&self->general_settings);
  g_clear_object (&self->provider);
  g_clear_object (&self->layout);

  g_clear_pointer (&self->toplevels, g_hash_table_destroy);

  G_OBJECT_CLASS (gp_application_parent_class)->dispose (object);
}

static void
gp_application_class_init (GpApplicationClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gp_application_dispose;
}

static void
gp_application_init (GpApplication *self)
{
  self->general_settings = g_settings_new ("org.gnome.gnome-panel.general");

  g_signal_connect (self->general_settings,
                    "changed::theme-variant",
                    G_CALLBACK (theme_variant_changed_cb),
                    self);

  self->theme_name_id = g_signal_connect (gtk_settings_get_default (),
                                          "notify::gtk-theme-name",
                                          G_CALLBACK (gtk_theme_name_changed_cb),
                                          self);

  self->prefer_dark_id = g_signal_connect (gtk_settings_get_default (),
                                           "notify::gtk-application-prefer-dark-theme",
                                           G_CALLBACK (prefer_dark_changed_cb),
                                           self);

  theme_variant_changed_cb (self->general_settings, "theme-variant", self);

  self->layout = panel_layout_new (self);

  self->toplevels = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           (GDestroyNotify) gtk_widget_destroy);
}

GpApplication *
gp_application_new (GError **error)
{
  return g_initable_new (GP_TYPE_APPLICATION,
                         NULL, error,
                         NULL);
}

PanelLayout *
gp_application_get_layout (GpApplication *self)
{
  return self->layout;
}

void
gp_application_add_toplevel (GpApplication *self,
                             PanelToplevel *toplevel)
{
  g_hash_table_insert (self->toplevels,
                       g_strdup (panel_toplevel_get_id (toplevel)),
                       toplevel);
}

void
gp_application_remove_toplevel (GpApplication *self,
                                PanelToplevel *toplevel)
{
  g_hash_table_remove (self->toplevels,
                       panel_toplevel_get_id (toplevel));
}

GList *
gp_application_get_toplevels (GpApplication *self)
{
  return g_hash_table_get_values (self->toplevels);
}

PanelToplevel *
gp_application_get_toplevel_by_id (GpApplication *self,
                                   const char    *id)
{
  return g_hash_table_lookup (self->toplevels, id);
}

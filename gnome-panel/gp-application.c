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

#include "applet.h"
#include "gp-applet-manager.h"
#include "gp-module-manager.h"
#include "panel-action-protocol.h"
#include "panel-enums-gsettings.h"
#include "panel-layout.h"
#include "panel-lockdown.h"
#include "panel-multiscreen.h"
#include "panel-schemas.h"
#include "panel-toplevel.h"
#include "panel-widget.h"

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
  GSettings        *interface_settings;

  gulong            theme_name_id;
  gulong            prefer_dark_id;

  GtkStyleProvider *provider;

  GpModuleManager  *module_manager;
  GpAppletManager  *applet_manager;

  GpActionProtocol *action_protocol;

  PanelLockdown    *lockdown;

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
  char *color_scheme;

  variant = g_settings_get_enum (self->general_settings, PANEL_GENERAL_THEME_VARIANT_KEY);

  if (variant == PANEL_THEME_VARIANT_SYSTEM)
    {
      color_scheme = g_settings_get_string (self->interface_settings, "color-scheme");

      if (g_strcmp0 (color_scheme, "prefer-dark") == 0)
        {
          g_object_set (gtk_settings_get_default (),
                        "gtk-application-prefer-dark-theme",
                        TRUE,
                        NULL);
        }
      else
        {
          gtk_settings_reset_property (gtk_settings_get_default (),
                                       "gtk-application-prefer-dark-theme");
        }

      g_free (color_scheme);
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

static void
set_enable_tooltip (AppletInfo *applet,
                    gpointer    user_data)
{
  gp_applet_set_enable_tooltips (panel_applet_get_applet (applet),
                                 *(gboolean *) user_data);
}

static void
enable_tooltips_cb (GSettings     *settings,
                    const char    *key,
                    GpApplication *self)
{
  gboolean enable_tooltips;

  enable_tooltips = g_settings_get_boolean (self->general_settings,
                                            "enable-tooltips");

  panel_applet_foreach (NULL, set_enable_tooltip, &enable_tooltips);
}

static void
set_prefer_symbolic_icons (AppletInfo *applet,
                           gpointer    user_data)
{
  gp_applet_set_prefer_symbolic_icons (panel_applet_get_applet (applet),
                                       *(gboolean *) user_data);
}

static void
prefer_symbolic_icons_cb (GSettings     *settings,
                          const char    *key,
                          GpApplication *self)
{
  gboolean prefer_symbolic_icons;

  prefer_symbolic_icons = g_settings_get_boolean (self->general_settings,
                                                  "prefer-symbolic-icons");

  panel_applet_foreach (NULL, set_prefer_symbolic_icons, &prefer_symbolic_icons);
}

static void
set_menu_icon_size (AppletInfo *applet,
                    gpointer    user_data)
{
  gp_applet_set_menu_icon_size (panel_applet_get_applet (applet),
                                *(guint *) user_data);
}

static void
menu_icon_size_cb (GSettings     *settings,
                   const char    *key,
                   GpApplication *self)
{
  guint menu_icon_size;

  menu_icon_size = g_settings_get_enum (self->general_settings,
                                        "menu-icon-size");

  panel_applet_foreach (NULL, set_menu_icon_size, &menu_icon_size);
}

static void
set_panel_icon_size (AppletInfo *applet,
                     gpointer    user_data)
{
  gp_applet_set_panel_icon_size (panel_applet_get_applet (applet),
                                 *(guint *) user_data);
}

static void
update_icon_size (GpApplication *self)
{
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, self->toplevels);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      PanelWidget *panel;
      guint panel_icon_size;

      panel = panel_toplevel_get_panel_widget (PANEL_TOPLEVEL (value));
      panel_icon_size = panel_widget_get_icon_size (panel);

      panel_applet_foreach (panel, set_panel_icon_size, &panel_icon_size);
    }
}

static void
panel_max_icon_size_cb (GSettings     *settings,
                        const char    *key,
                        GpApplication *self)
{
  update_icon_size (self);
}

static void
panel_icon_spacing_cb (GSettings     *settings,
                       const char    *key,
                       GpApplication *self)
{
  update_icon_size (self);
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

  g_clear_pointer (&self->toplevels, g_hash_table_destroy);

  g_clear_object (&self->general_settings);
  g_clear_object (&self->interface_settings);
  g_clear_object (&self->provider);
  g_clear_object (&self->module_manager);
  g_clear_object (&self->applet_manager);
  g_clear_object (&self->action_protocol);
  g_clear_object (&self->lockdown);
  g_clear_object (&self->layout);

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
  self->interface_settings = g_settings_new ("org.gnome.desktop.interface");

  g_signal_connect (self->general_settings,
                    "changed::theme-variant",
                    G_CALLBACK (theme_variant_changed_cb),
                    self);

  g_signal_connect (self->general_settings,
                    "changed::enable-tooltips",
                    G_CALLBACK (enable_tooltips_cb),
                    self);

  g_signal_connect (self->general_settings,
                    "changed::prefer-symbolic-icons",
                    G_CALLBACK (prefer_symbolic_icons_cb),
                    self);

  g_signal_connect (self->general_settings,
                    "changed::menu-icon-size",
                    G_CALLBACK (menu_icon_size_cb),
                    self);

  g_signal_connect (self->general_settings,
                    "changed::panel-max-icon-size",
                    G_CALLBACK (panel_max_icon_size_cb),
                    self);

  g_signal_connect (self->general_settings,
                    "changed::panel-icon-spacing",
                    G_CALLBACK (panel_icon_spacing_cb),
                    self);

  g_signal_connect (self->interface_settings,
                    "changed::color-scheme",
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

  self->module_manager = gp_module_manager_new ();
  self->applet_manager = gp_applet_manager_new (self);

  self->action_protocol = gp_action_protocol_new (self);

  panel_multiscreen_init ();

  self->lockdown = panel_lockdown_new ();
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

GSettings *
gp_application_get_general_settings (GpApplication *self)
{
  return self->general_settings;
}

GpModuleManager *
gp_application_get_module_manager (GpApplication *self)
{
  return self->module_manager;
}

GpAppletManager *
gp_application_get_applet_manager (GpApplication *self)
{
  return self->applet_manager;
}

PanelLockdown *
gp_application_get_lockdown (GpApplication  *self)
{
  return self->lockdown;
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

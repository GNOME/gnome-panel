/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *     Mark McLoughlin <mark@skynet.ie>
 */

#include "config.h"
#include "gp-menu-button-applet.h"

#include <glib/gi18n-lib.h>
#include <libgnome-panel/gp-image-menu-item.h>
#include <libgnome-panel/gp-utils.h>

#include "gp-lock-logout.h"
#include "gp-menu-button.h"
#include "gp-menu-utils.h"
#include "gp-menu.h"
#include "gp-places-menu.h"
#include "gp-user-menu.h"

#define MENU_BUTTON_SCHEMA "org.gnome.gnome-panel.applet.menu-button"
#define RESOURCE_PATH "/org/gnome/gnome-panel/modules/menu"

typedef struct
{
  GSettings    *settings;

  GtkWidget    *button;
  GtkWidget    *image;
  GtkWidget    *arrow;

  gboolean      custom_menu;

  GtkWidget    *menu;
  GpLockLogout *lock_logout;
} GpMenuButtonAppletPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GpMenuButtonApplet, gp_menu_button_applet, GP_TYPE_APPLET)

static void
update_arrow (GpMenuButtonApplet *menu_button,
              GtkPositionType     position)
{
  GpMenuButtonAppletPrivate *priv;
  const gchar *icon_name;
  GtkAlign halign;
  GtkAlign valign;

  priv = gp_menu_button_applet_get_instance_private (menu_button);

  switch (position)
    {
      case GTK_POS_LEFT:
        icon_name = "go-next-symbolic";
        halign = GTK_ALIGN_END;
        valign = GTK_ALIGN_START;
        break;

      case GTK_POS_RIGHT:
        icon_name = "go-previous-symbolic";
        halign = GTK_ALIGN_START;
        valign = GTK_ALIGN_START;
        break;

      case GTK_POS_BOTTOM:
        icon_name = "go-up-symbolic";
        halign = GTK_ALIGN_END;
        valign = GTK_ALIGN_START;
        break;

      case GTK_POS_TOP:
      default:
        icon_name = "go-down-symbolic";
        halign = GTK_ALIGN_START;
        valign = GTK_ALIGN_END;
        break;
    }

  gtk_image_set_from_icon_name (GTK_IMAGE (priv->arrow), icon_name,
                                GTK_ICON_SIZE_MENU);

  gtk_widget_set_halign (priv->arrow, halign);
  gtk_widget_set_valign (priv->arrow, valign);
}

static void
update_icon (GpMenuButtonApplet *menu_button)
{
  GpMenuButtonAppletPrivate *priv;
  GIcon *icon;
  gchar *custom_icon;
  GtkPositionType position;

  priv = gp_menu_button_applet_get_instance_private (menu_button);

  icon = NULL;
  custom_icon = g_settings_get_string (priv->settings, "custom-icon");

  if (*custom_icon != '\0')
    icon = g_themed_icon_new (custom_icon);

  if (icon == NULL && priv->menu != NULL && priv->custom_menu)
    icon = gp_menu_get_icon (GP_MENU (priv->menu));

  if (icon == NULL)
    {
      const char *icon_name;

      icon_name = "start-here";
      if (gp_applet_get_prefer_symbolic_icons (GP_APPLET (menu_button)))
        icon_name = "start-here-symbolic";

      icon = g_themed_icon_new (icon_name);
    }

  gtk_image_set_from_gicon (GTK_IMAGE (priv->image), icon, GTK_ICON_SIZE_MENU);

  g_object_unref (icon);
  g_free (custom_icon);

  position = gp_applet_get_position (GP_APPLET (menu_button));
  update_arrow (menu_button, position);
}

static gchar *
split_menu_uri (const gchar  *menu_uri,
                gchar       **menu_scheme)
{
  gchar *p;

  if (*menu_uri == '\0')
    return NULL;

  p = strchr (menu_uri, ':');

  if (!p || p == menu_uri)
    return NULL;

  g_assert (menu_scheme != NULL);
  *menu_scheme = g_strndup (menu_uri, p - menu_uri);

  if (*(++p) != '/')
    {
      g_free (*menu_scheme);
      *menu_scheme = NULL;
      return NULL;
    }

  /* skip extra leading slashes */
  while (*p != '\0' && *(p + 1) == '/')
    p++;

  return g_strdup (p);
}

static void
menu_icon_size_cb (GpApplet   *applet,
                   GParamSpec *pspec,
                   GtkImage   *image)
{
  guint icon_size;

  icon_size = gp_applet_get_menu_icon_size (applet);
  gtk_image_set_pixel_size (image, icon_size);
}

static void
append_places_item (GpMenuButtonApplet *menu_button,
                    GtkMenu            *menu)
{
  guint icon_size;
  GtkWidget *icon;
  GtkWidget *item;
  GtkWidget *places_menu;

  icon_size = gp_applet_get_menu_icon_size (GP_APPLET (menu_button));
  icon = gtk_image_new_from_icon_name ("folder", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);

  g_signal_connect_object (menu_button, "notify::menu-icon-size",
                           G_CALLBACK (menu_icon_size_cb), icon, 0);

  item = gp_image_menu_item_new_with_label (_("Places"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_widget_show (item);

  places_menu = gp_places_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), places_menu);

  g_object_bind_property (menu_button, "enable-tooltips",
                          places_menu, "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (menu_button, "locked-down",
                          places_menu, "locked-down",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (menu_button, "menu-icon-size",
                          places_menu, "menu-icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);
}

static void
append_lock_logout (GtkMenu                   *menu,
                    GpMenuButtonAppletPrivate *priv)
{
  gp_lock_logout_append_to_menu (priv->lock_logout, menu);
}

static void
append_user_item (GpMenuButtonApplet *menu_button,
                  GtkMenu            *menu)
{
  GpMenuButtonAppletPrivate *priv;
  guint icon_size;
  GtkWidget *icon;
  gchar *user_name;
  GtkWidget *item;
  GtkWidget *user_menu;

  priv = gp_menu_button_applet_get_instance_private (menu_button);

  icon_size = gp_applet_get_menu_icon_size (GP_APPLET (menu_button));
  icon = gtk_image_new_from_icon_name ("computer", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);

  g_signal_connect_object (menu_button, "notify::menu-icon-size",
                           G_CALLBACK (menu_icon_size_cb), icon, 0);

  user_name = gp_menu_utils_get_user_name ();
  item = gp_image_menu_item_new_with_label (user_name);
  g_free (user_name);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_widget_show (item);

  user_menu = gp_user_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), user_menu);

  g_object_bind_property (menu_button, "enable-tooltips",
                          user_menu, "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (menu_button, "locked-down",
                          user_menu, "locked-down",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (menu_button, "menu-icon-size",
                          user_menu, "menu-icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (user_menu, "empty", item, "visible",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE |
                          G_BINDING_INVERT_BOOLEAN);

  gp_user_menu_set_append_func (GP_USER_MENU (user_menu),
                                (GpAppendMenuItemsFunc) append_lock_logout,
                                priv);
}

static void
append_menu_items_cb (GtkMenu            *menu,
                      GpMenuButtonApplet *menu_button)
{
  append_separator_if_needed (menu);
  append_places_item (menu_button, menu);
  append_user_item (menu_button, menu);
}

static gboolean
validate_scheme (const gchar  *scheme,
                 gchar       **menu_file)
{
  if (g_strcmp0 (scheme, "applications") == 0)
    {
      *menu_file = gp_menu_utils_get_applications_menu ();
      return TRUE;
    }
  else if (g_strcmp0 (scheme, "gnomecc") == 0)
    {
      *menu_file = g_strdup ("gnomecc.menu");
      return TRUE;
    }

  return FALSE;
}

static void
update_menu (GpMenuButtonApplet *menu_button)
{
  GpMenuButtonAppletPrivate *priv;
  gchar *menu_path;
  gchar *scheme;
  gchar *path;

  priv = gp_menu_button_applet_get_instance_private (menu_button);

  g_clear_object (&priv->menu);
  g_clear_object (&priv->lock_logout);

  menu_path = g_settings_get_string (priv->settings, "menu-path");

  scheme = NULL;
  path = split_menu_uri (menu_path, &scheme);
  g_free (menu_path);

  if (path != NULL)
    {
      gchar *menu_file;

      menu_file = NULL;
      if (validate_scheme (scheme, &menu_file))
        {
          priv->menu = gp_menu_new (menu_file, TRUE);
          gp_menu_set_path (GP_MENU (priv->menu), path);

          priv->custom_menu = TRUE;

          g_signal_connect_swapped (priv->menu, "loaded",
                                    G_CALLBACK (update_icon), menu_button);
        }

      g_free (menu_file);
      g_free (scheme);
      g_free (path);
    }

  if (priv->menu == NULL)
    {
      gchar *menu_file;

      menu_file = gp_menu_utils_get_applications_menu ();
      priv->menu = gp_menu_new (menu_file, TRUE);
      g_free (menu_file);

      g_assert (priv->lock_logout == NULL);
      priv->lock_logout = gp_lock_logout_new ();

      g_object_bind_property (menu_button, "enable-tooltips",
                              priv->lock_logout, "enable-tooltips",
                              G_BINDING_DEFAULT |
                              G_BINDING_SYNC_CREATE);

      g_object_bind_property (menu_button, "locked-down",
                              priv->lock_logout, "locked-down",
                              G_BINDING_DEFAULT |
                              G_BINDING_SYNC_CREATE);

      g_object_bind_property (menu_button, "menu-icon-size",
                              priv->lock_logout, "menu-icon-size",
                              G_BINDING_DEFAULT |
                              G_BINDING_SYNC_CREATE);

      g_signal_connect_swapped (priv->lock_logout, "changed",
                                G_CALLBACK (gp_menu_reload), priv->menu);

      gp_menu_set_append_func (GP_MENU (priv->menu),
                               (GpAppendMenuItemsFunc) append_menu_items_cb,
                               menu_button);

      priv->custom_menu = FALSE;
    }

  g_object_bind_property (menu_button, "enable-tooltips",
                          priv->menu, "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (menu_button, "locked-down",
                          priv->menu, "locked-down",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (menu_button, "menu-icon-size",
                          priv->menu, "menu-icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_ref_sink (priv->menu);
}

static void
settings_changed_cb (GSettings          *settings,
                     const gchar        *key,
                     GpMenuButtonApplet *menu_button)
{
  GpMenuButtonAppletPrivate *priv;

  priv = gp_menu_button_applet_get_instance_private (menu_button);

  if (g_strcmp0 (key, "custom-icon") == 0)
    {
      update_icon (menu_button);
    }
  else if (g_strcmp0 (key, "menu-path") == 0)
    {
      update_menu (menu_button);
      update_icon (menu_button);
    }
  else if (g_strcmp0 (key, "show-arrow") == 0)
    {
      gboolean show_arrow;

      show_arrow = g_settings_get_boolean (priv->settings, "show-arrow");
      gtk_widget_set_visible (priv->arrow, show_arrow);
    }
  else if (g_strcmp0 (key, "tooltip") == 0)
    {
      gchar *tooltip;
      gboolean enable_tooltips;

      tooltip = g_settings_get_string (priv->settings, "tooltip");
      gtk_widget_set_tooltip_text (priv->button, *tooltip != '\0' ? tooltip : NULL);
      g_free (tooltip);

      g_object_get (menu_button, "enable-tooltips", &enable_tooltips, NULL);
      gtk_widget_set_has_tooltip (priv->button, enable_tooltips);
    }
}

static gint
arrow_size_from_icon_size (guint icon_size)
{
  return 16 * (icon_size / 36.0);
}

static void
prefer_symbolic_icons_cb (GpApplet           *applet,
                          GParamSpec         *pspec,
                          GpMenuButtonApplet *menu_button)
{
  update_icon (menu_button);
}

static void
panel_icon_size_cb (GpApplet           *applet,
                    GParamSpec         *pspec,
                    GpMenuButtonApplet *menu_button)
{
  GpMenuButtonAppletPrivate *priv;
  guint icon_size;
  guint arrow_size;

  priv = gp_menu_button_applet_get_instance_private (menu_button);

  icon_size = gp_applet_get_panel_icon_size (applet);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);

  arrow_size = arrow_size_from_icon_size (icon_size);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->arrow), arrow_size);
}

static void
edit_menus_cb (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  const gchar *application;
  GDesktopAppInfo *app_info;

  application = "alacarte.desktop";
  app_info = g_desktop_app_info_new (application);

  if (app_info == NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (NULL, 0,
                                       GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                       _("Please install the '%s' application."),
                                       application);

      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_window_present (GTK_WINDOW (dialog));

      return;
    }

  gp_menu_utils_app_info_launch (app_info);
}

static const GActionEntry menu_button_menu_actions[] =
  {
    { "edit-menus", edit_menus_cb, NULL, NULL, NULL },
    { NULL }
  };

static void
setup_menu (GpMenuButtonApplet *menu_button)
{
  GpApplet *applet;
  const gchar *resource;
  GAction *action;
  GBindingFlags flags;

  applet = GP_APPLET (menu_button);

  resource = RESOURCE_PATH "/main-menu-menu.ui";
  gp_applet_setup_menu_from_resource (applet, resource, menu_button_menu_actions);

  action = gp_applet_menu_lookup_action (applet, "edit-menus");
  flags = G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE;

  g_object_bind_property (applet, "locked-down", action, "enabled", flags);
}

static gboolean
button_press_event_cb (GtkWidget          *widget,
                       GdkEvent           *event,
                       GpMenuButtonApplet *menu_button)
{
  GdkEventButton *button;

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  button = (GdkEventButton *) event;
  if (button->button == GDK_BUTTON_SECONDARY)
    return FALSE;

  gp_menu_button_applet_popup_menu (menu_button, event);

  return TRUE;
}

static void
clicked_cb (GtkWidget          *widget,
            GpMenuButtonApplet *menu_button)
{
  gp_menu_button_applet_popup_menu (menu_button, NULL);
}

static void
setup_button (GpMenuButtonApplet *menu_button)
{
  GpMenuButtonAppletPrivate *priv;
  GtkWidget *overlay;
  gboolean show_arrow;
  GtkStyleContext *style;
  guint icon_size;
  guint arrow_size;
  gchar *tooltip;

  priv = gp_menu_button_applet_get_instance_private (menu_button);

  priv->button = gp_menu_button_new ();
  gtk_container_add (GTK_CONTAINER (menu_button), priv->button);
  gtk_widget_show (priv->button);

  g_signal_connect (priv->button, "button-press-event",
                    G_CALLBACK (button_press_event_cb), menu_button);

  g_signal_connect (priv->button, "clicked",
                    G_CALLBACK (clicked_cb), menu_button);

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (priv->button), overlay);
  gtk_widget_set_halign (overlay, GTK_ALIGN_FILL);
  gtk_widget_set_valign (overlay, GTK_ALIGN_FILL);
  gtk_widget_show (overlay);

  priv->image = gtk_image_new ();
  gp_add_text_color_class (priv->image);
  gtk_container_add (GTK_CONTAINER (overlay), priv->image);
  gtk_widget_show (priv->image);

  style = gtk_widget_get_style_context (priv->image);
  gtk_style_context_add_class (style, "icon");

  show_arrow = g_settings_get_boolean (priv->settings, "show-arrow");

  priv->arrow = gtk_image_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_MENU);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), priv->arrow);
  gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (overlay), priv->arrow, TRUE);
  gtk_widget_set_visible (priv->arrow, show_arrow);

  style = gtk_widget_get_style_context (priv->arrow);
  gtk_style_context_add_class (style, "arrow");

  icon_size = gp_applet_get_panel_icon_size (GP_APPLET (menu_button));
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);

  arrow_size = arrow_size_from_icon_size (icon_size);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->arrow), arrow_size);

  tooltip = g_settings_get_string (priv->settings, "tooltip");
  gtk_widget_set_tooltip_text (priv->button, *tooltip != '\0' ? tooltip : NULL);
  g_free (tooltip);

  g_object_bind_property (menu_button, "enable-tooltips",
                          priv->button, "has-tooltip",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  update_menu (menu_button);
  update_icon (menu_button);
}

static void
gp_menu_button_applet_setup (GpMenuButtonApplet *menu_button)
{
  GpMenuButtonAppletPrivate *priv;

  priv = gp_menu_button_applet_get_instance_private (menu_button);

  priv->settings = gp_applet_settings_new (GP_APPLET (menu_button),
                                           MENU_BUTTON_SCHEMA);

  g_signal_connect (priv->settings, "changed",
                    G_CALLBACK (settings_changed_cb), menu_button);

  g_signal_connect (menu_button, "notify::prefer-symbolic-icons",
                    G_CALLBACK (prefer_symbolic_icons_cb), menu_button);

  g_signal_connect (menu_button, "notify::panel-icon-size",
                    G_CALLBACK (panel_icon_size_cb), menu_button);

  setup_menu (menu_button);
  setup_button (menu_button);
}

static void
gp_menu_button_applet_dispose (GObject *object)
{
  GpMenuButtonApplet *menu_button;
  GpMenuButtonAppletPrivate *priv;

  menu_button = GP_MENU_BUTTON_APPLET (object);
  priv = gp_menu_button_applet_get_instance_private (menu_button);

  g_clear_object (&priv->menu);
  g_clear_object (&priv->lock_logout);
  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (gp_menu_button_applet_parent_class)->dispose (object);
}

static gboolean
gp_menu_button_applet_initial_setup (GpApplet  *applet,
                                     GVariant  *initial_settings,
                                     GError   **error)
{
  GSettings *settings;
  const gchar *menu_path;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  settings = gp_applet_settings_new (applet, MENU_BUTTON_SCHEMA);

  menu_path = NULL;
  if (g_variant_lookup (initial_settings, "menu-path", "&s", &menu_path))
    g_settings_set_string (settings, "menu-path", menu_path);

  g_object_unref (settings);

  return TRUE;
}

static gboolean
gp_menu_button_applet_initable_init (GpApplet  *applet,
                                     GError   **error)
{
  gp_menu_button_applet_setup (GP_MENU_BUTTON_APPLET (applet));

  return TRUE;
}

static void
gp_menu_button_applet_placement_changed (GpApplet        *applet,
                                         GtkOrientation   orientation,
                                         GtkPositionType  position)
{
  GpMenuButtonApplet *menu_button;

  menu_button = GP_MENU_BUTTON_APPLET (applet);

  update_arrow (menu_button, position);
}

static void
gp_menu_button_applet_class_init (GpMenuButtonAppletClass *menu_button_class)
{
  GObjectClass *object_class;
  GpAppletClass *applet_class;

  object_class = G_OBJECT_CLASS (menu_button_class);
  applet_class = GP_APPLET_CLASS (menu_button_class);

  object_class->dispose = gp_menu_button_applet_dispose;

  applet_class->initial_setup = gp_menu_button_applet_initial_setup;
  applet_class->initable_init = gp_menu_button_applet_initable_init;
  applet_class->placement_changed = gp_menu_button_applet_placement_changed;
}

static void
gp_menu_button_applet_init (GpMenuButtonApplet *menu_button)
{
  GpApplet *applet;

  applet = GP_APPLET (menu_button);

  gp_applet_set_flags (applet, GP_APPLET_FLAGS_EXPAND_MINOR);
}

gboolean
gp_menu_button_applet_popup_menu (GpMenuButtonApplet *menu_button,
                                  GdkEvent           *event)
{
  GpMenuButtonAppletPrivate *priv;

  priv = gp_menu_button_applet_get_instance_private (menu_button);

  if (priv->menu == NULL)
    return FALSE;

  gp_applet_popup_menu_at_widget (GP_APPLET (menu_button),
                                  GTK_MENU (priv->menu),
                                  priv->button,
                                  event);

  return TRUE;
}

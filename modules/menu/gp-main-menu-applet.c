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

#include <glib/gi18n-lib.h>
#include <libgnome-panel/gp-action.h>
#include <libgnome-panel/gp-image-menu-item.h>

#include "gp-lock-logout.h"
#include "gp-main-menu-applet.h"
#include "gp-menu-button.h"
#include "gp-menu-utils.h"
#include "gp-menu.h"
#include "gp-places-menu.h"
#include "gp-user-menu.h"

#define MAIN_MENU_SCHEMA "org.gnome.gnome-panel.applet.main-menu"
#define RESOURCE_PATH "/org/gnome/gnome-panel/modules/menu"

struct _GpMainMenuApplet
{
  GpApplet      parent;

  GSettings    *settings;

  GtkWidget    *button;
  GtkWidget    *image;
  GtkWidget    *arrow;

  gboolean      custom_menu;

  GtkWidget    *menu;
  GpLockLogout *lock_logout;
};

static void gp_action_interface_init (GpActionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GpMainMenuApplet, gp_main_menu_applet, GP_TYPE_APPLET,
                         G_IMPLEMENT_INTERFACE (GP_TYPE_ACTION, gp_action_interface_init))

static void
popup_menu (GpMainMenuApplet *main_menu,
            GdkEvent         *event)
{
  GdkGravity widget_anchor;
  GdkGravity menu_anchor;

  switch (gp_applet_get_position (GP_APPLET (main_menu)))
    {
      case GTK_POS_TOP:
        widget_anchor = GDK_GRAVITY_SOUTH_WEST;
        menu_anchor = GDK_GRAVITY_NORTH_WEST;
        break;

      case GTK_POS_LEFT:
        widget_anchor = GDK_GRAVITY_NORTH_EAST;
        menu_anchor = GDK_GRAVITY_NORTH_WEST;
        break;

      case GTK_POS_RIGHT:
        widget_anchor = GDK_GRAVITY_NORTH_WEST;
        menu_anchor = GDK_GRAVITY_NORTH_EAST;
        break;

      case GTK_POS_BOTTOM:
        widget_anchor = GDK_GRAVITY_NORTH_WEST;
        menu_anchor = GDK_GRAVITY_SOUTH_WEST;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  gtk_menu_popup_at_widget (GTK_MENU (main_menu->menu),
                            main_menu->button,
                            widget_anchor, menu_anchor,
                            event);
}

static gboolean
gp_main_menu_applet_main_menu (GpAction *action,
                               guint32   time)
{
  GpMainMenuApplet *main_menu;

  main_menu = GP_MAIN_MENU_APPLET (action);

  if (main_menu->menu == NULL)
    return FALSE;

  popup_menu (main_menu, NULL);

  return TRUE;
}

static void
gp_action_interface_init (GpActionInterface *iface)
{
  iface->main_menu = gp_main_menu_applet_main_menu;
}

static void
update_arrow (GpMainMenuApplet *main_menu,
              GtkPositionType   position)
{
  const gchar *icon_name;
  GtkAlign halign;
  GtkAlign valign;

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

  gtk_image_set_from_icon_name (GTK_IMAGE (main_menu->arrow), icon_name,
                                GTK_ICON_SIZE_MENU);

  gtk_widget_set_halign (main_menu->arrow, halign);
  gtk_widget_set_valign (main_menu->arrow, valign);
}

static void
update_icon (GpMainMenuApplet *main_menu)
{
  GIcon *icon;
  gchar *custom_icon;
  GtkPositionType position;

  icon = NULL;
  custom_icon = g_settings_get_string (main_menu->settings, "custom-icon");

  if (*custom_icon != '\0')
    icon = g_themed_icon_new (custom_icon);

  if (icon == NULL &&
      main_menu->menu != NULL &&
      main_menu->custom_menu)
    icon = gp_menu_get_icon (GP_MENU (main_menu->menu));

  if (icon == NULL)
    icon = g_themed_icon_new ("start-here");

  gtk_image_set_from_gicon (GTK_IMAGE (main_menu->image), icon,
                            GTK_ICON_SIZE_MENU);

  g_object_unref (icon);
  g_free (custom_icon);

  position = gp_applet_get_position (GP_APPLET (main_menu));
  update_arrow (main_menu, position);
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

static gchar *
get_applications_menu (void)
{
  const gchar *xdg_menu_prefx;

  xdg_menu_prefx = g_getenv ("XDG_MENU_PREFIX");
  if (!xdg_menu_prefx || *xdg_menu_prefx == '\0')
    return g_strdup ("gnome-applications.menu");

  return g_strdup_printf ("%sapplications.menu", xdg_menu_prefx);
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
append_places_item (GpMainMenuApplet *main_menu,
                    GtkMenu          *menu)
{
  guint icon_size;
  GtkWidget *icon;
  GtkWidget *item;
  GtkWidget *places_menu;

  icon_size = gp_applet_get_menu_icon_size (GP_APPLET (main_menu));
  icon = gtk_image_new_from_icon_name ("folder", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);

  g_signal_connect_object (main_menu, "notify::menu-icon-size",
                           G_CALLBACK (menu_icon_size_cb), icon, 0);

  item = gp_image_menu_item_new_with_label (_("Places"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_widget_show (item);

  places_menu = gp_places_menu_new (GP_APPLET (main_menu));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), places_menu);
}

static void
append_user_item (GpMainMenuApplet *main_menu,
                  GtkMenu          *menu)
{
  guint icon_size;
  GtkWidget *icon;
  gchar *user_name;
  GtkWidget *item;
  GtkWidget *user_menu;

  icon_size = gp_applet_get_menu_icon_size (GP_APPLET (main_menu));
  icon = gtk_image_new_from_icon_name ("computer", GTK_ICON_SIZE_MENU);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);

  g_signal_connect_object (main_menu, "notify::menu-icon-size",
                           G_CALLBACK (menu_icon_size_cb), icon, 0);

  user_name = gp_menu_utils_get_user_name ();
  item = gp_image_menu_item_new_with_label (user_name);
  g_free (user_name);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (item), icon);
  gtk_widget_show (item);

  user_menu = gp_user_menu_new (GP_APPLET (main_menu));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), user_menu);

  g_object_bind_property (user_menu, "empty", item, "visible",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE |
                          G_BINDING_INVERT_BOOLEAN);
}

static void
append_menu_items_cb (GtkMenu          *menu,
                      GpMainMenuApplet *main_menu)
{
  append_separator_if_needed (menu);
  append_places_item (main_menu, menu);
  append_user_item (main_menu, menu);

  gp_lock_logout_append_to_menu (main_menu->lock_logout, menu);
}

static gboolean
validate_scheme (const gchar  *scheme,
                 gchar       **menu_file)
{
  if (g_strcmp0 (scheme, "applications") == 0)
    {
      *menu_file = get_applications_menu ();
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
update_menu (GpMainMenuApplet *main_menu)
{
  gchar *menu_path;
  gchar *scheme;
  gchar *path;

  g_clear_object (&main_menu->menu);
  g_clear_object (&main_menu->lock_logout);

  menu_path = g_settings_get_string (main_menu->settings, "menu-path");

  scheme = NULL;
  path = split_menu_uri (menu_path, &scheme);
  g_free (menu_path);

  if (path != NULL)
    {
      gchar *menu_file;

      menu_file = NULL;
      if (validate_scheme (scheme, &menu_file))
        {
          main_menu->menu = gp_menu_new (GP_APPLET (main_menu), menu_file, TRUE);
          gp_menu_set_path (GP_MENU (main_menu->menu), path);

          main_menu->custom_menu = TRUE;

          g_signal_connect_swapped (main_menu->menu, "loaded",
                                    G_CALLBACK (update_icon), main_menu);
        }

      g_free (menu_file);
      g_free (scheme);
      g_free (path);
    }

  if (main_menu->menu == NULL)
    {
      gchar *menu_file;

      menu_file = get_applications_menu ();
      main_menu->menu = gp_menu_new (GP_APPLET (main_menu), menu_file, TRUE);
      g_free (menu_file);

      g_assert (main_menu->lock_logout == NULL);
      main_menu->lock_logout = gp_lock_logout_new (GP_APPLET (main_menu));

      g_signal_connect_swapped (main_menu->lock_logout, "changed",
                                G_CALLBACK (gp_menu_reload), main_menu->menu);

      gp_menu_set_append_func (GP_MENU (main_menu->menu),
                               (GpAppendMenuItemsFunc) append_menu_items_cb,
                               main_menu);

      main_menu->custom_menu = FALSE;
    }

  g_object_ref_sink (main_menu->menu);
}

static void
settings_changed_cb (GSettings        *settings,
                     const gchar      *key,
                     GpMainMenuApplet *main_menu)
{
  if (g_strcmp0 (key, "custom-icon") == 0)
    {
      update_icon (main_menu);
    }
  else if (g_strcmp0 (key, "menu-path") == 0)
    {
      update_menu (main_menu);
      update_icon (main_menu);
    }
  else if (g_strcmp0 (key, "show-arrow") == 0)
    {
      gboolean show_arrow;

      show_arrow = g_settings_get_boolean (main_menu->settings, "show-arrow");
      gtk_widget_set_visible (main_menu->image, show_arrow);
    }
  else if (g_strcmp0 (key, "tooltip") == 0)
    {
      gchar *tooltip;
      gboolean enable_tooltips;

      tooltip = g_settings_get_string (main_menu->settings, "tooltip");
      gtk_widget_set_tooltip_text (main_menu->button, *tooltip != '\0' ? tooltip : NULL);
      g_free (tooltip);

      g_object_get (main_menu, "enable-tooltips", &enable_tooltips, NULL);
      gtk_widget_set_has_tooltip (main_menu->button, enable_tooltips);
    }
}

static gint
arrow_size_from_icon_size (guint icon_size)
{
  return 16 * (icon_size / 36.0);
}

static void
panel_icon_size_cb (GpApplet         *applet,
                    GParamSpec       *pspec,
                    GpMainMenuApplet *main_menu)
{
  guint icon_size;
  guint arrow_size;

  icon_size = gp_applet_get_panel_icon_size (applet);
  gtk_image_set_pixel_size (GTK_IMAGE (main_menu->image), icon_size);

  arrow_size = arrow_size_from_icon_size (icon_size);
  gtk_image_set_pixel_size (GTK_IMAGE (main_menu->arrow), arrow_size);
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

static const GActionEntry main_menu_menu_actions[] =
  {
    { "edit-menus", edit_menus_cb, NULL, NULL, NULL },
    { NULL }
  };

static void
setup_menu (GpMainMenuApplet *main_menu)
{
  GpApplet *applet;
  const gchar *resource;
  GAction *action;
  GBindingFlags flags;

  applet = GP_APPLET (main_menu);

  resource = RESOURCE_PATH "/main-menu-menu.ui";
  gp_applet_setup_menu_from_resource (applet, resource, main_menu_menu_actions);

  action = gp_applet_menu_lookup_action (applet, "edit-menus");
  flags = G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE;

  g_object_bind_property (applet, "locked-down", action, "enabled", flags);
}

static gboolean
button_press_event_cb (GtkWidget        *widget,
                       GdkEvent         *event,
                       GpMainMenuApplet *main_menu)
{
  GdkEventButton *button;

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  button = (GdkEventButton *) event;
  if (button->button == GDK_BUTTON_SECONDARY)
    return FALSE;

  popup_menu (main_menu, event);

  return TRUE;
}

static void
clicked_cb (GtkWidget        *widget,
            GpMainMenuApplet *main_menu)
{
  popup_menu (main_menu, NULL);
}

static void
setup_button (GpMainMenuApplet *main_menu)
{
  GtkWidget *overlay;
  gboolean show_arrow;
  GtkStyleContext *style;
  guint icon_size;
  guint arrow_size;
  gchar *tooltip;

  main_menu->button = gp_menu_button_new ();
  gtk_container_add (GTK_CONTAINER (main_menu), main_menu->button);
  gtk_widget_show (main_menu->button);

  g_signal_connect (main_menu->button, "button-press-event",
                    G_CALLBACK (button_press_event_cb), main_menu);

  g_signal_connect (main_menu->button, "clicked",
                    G_CALLBACK (clicked_cb), main_menu);

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (main_menu->button), overlay);
  gtk_widget_set_halign (overlay, GTK_ALIGN_FILL);
  gtk_widget_set_valign (overlay, GTK_ALIGN_FILL);
  gtk_widget_show (overlay);

  main_menu->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (overlay), main_menu->image);
  gtk_widget_show (main_menu->image);

  style = gtk_widget_get_style_context (main_menu->image);
  gtk_style_context_add_class (style, "icon");

  show_arrow = g_settings_get_boolean (main_menu->settings, "show-arrow");

  main_menu->arrow = gtk_image_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_MENU);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), main_menu->arrow);
  gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (overlay), main_menu->arrow, TRUE);
  gtk_widget_set_visible (main_menu->arrow, show_arrow);

  style = gtk_widget_get_style_context (main_menu->arrow);
  gtk_style_context_add_class (style, "arrow");

  icon_size = gp_applet_get_panel_icon_size (GP_APPLET (main_menu));
  gtk_image_set_pixel_size (GTK_IMAGE (main_menu->image), icon_size);

  arrow_size = arrow_size_from_icon_size (icon_size);
  gtk_image_set_pixel_size (GTK_IMAGE (main_menu->arrow), arrow_size);

  tooltip = g_settings_get_string (main_menu->settings, "tooltip");
  gtk_widget_set_tooltip_text (main_menu->button, *tooltip != '\0' ? tooltip : NULL);
  g_free (tooltip);

  g_object_bind_property (main_menu, "enable-tooltips",
                          main_menu->button, "has-tooltip",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  update_menu (main_menu);
  update_icon (main_menu);
}

static void
gp_main_menu_applet_setup (GpMainMenuApplet *main_menu)
{
  main_menu->settings = gp_applet_settings_new (GP_APPLET (main_menu),
                                                MAIN_MENU_SCHEMA);

  g_signal_connect (main_menu->settings, "changed",
                    G_CALLBACK (settings_changed_cb), main_menu);

  g_signal_connect (main_menu, "notify::panel-icon-size",
                    G_CALLBACK (panel_icon_size_cb), main_menu);

  setup_menu (main_menu);
  setup_button (main_menu);
}

static void
gp_main_menu_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_main_menu_applet_parent_class)->constructed (object);
  gp_main_menu_applet_setup (GP_MAIN_MENU_APPLET (object));
}

static void
gp_main_menu_applet_dispose (GObject *object)
{
  GpMainMenuApplet *main_menu;

  main_menu = GP_MAIN_MENU_APPLET (object);

  g_clear_object (&main_menu->menu);
  g_clear_object (&main_menu->lock_logout);
  g_clear_object (&main_menu->settings);

  G_OBJECT_CLASS (gp_main_menu_applet_parent_class)->dispose (object);
}

static void
gp_main_menu_applet_placement_changed (GpApplet        *applet,
                                       GtkOrientation   orientation,
                                       GtkPositionType  position)
{
  GpMainMenuApplet *main_menu;

  main_menu = GP_MAIN_MENU_APPLET (applet);

  update_arrow (main_menu, position);
}

static void
gp_main_menu_applet_class_init (GpMainMenuAppletClass *main_menu_class)
{
  GObjectClass *object_class;
  GpAppletClass *applet_class;

  object_class = G_OBJECT_CLASS (main_menu_class);
  applet_class = GP_APPLET_CLASS (main_menu_class);

  object_class->constructed = gp_main_menu_applet_constructed;
  object_class->dispose = gp_main_menu_applet_dispose;

  applet_class->placement_changed = gp_main_menu_applet_placement_changed;
}

static void
gp_main_menu_applet_init (GpMainMenuApplet *main_menu)
{
  GpApplet *applet;

  applet = GP_APPLET (main_menu);

  gp_applet_set_flags (applet, GP_APPLET_FLAGS_EXPAND_MINOR);
}

/*
 * Copyright (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2002 Sun Microsystems Inc
 * Copyright (C) 2004 Vincent Untz
 * Copyright (C) 2020 Alberts Muktupāvels
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
 *     Federico Mena
 *     George Lebl <jirka@5z.com>
 *     Mark McLoughlin <mark@skynet.ie>
 *     Miguel de Icaza
 *     Vincent Untz <vincent@vuntz.net>
 */

#include "config.h"
#include "gp-launcher-applet.h"

#include <glib/gi18n-lib.h>
#include <gmenu-tree.h>
#include <systemd/sd-journal.h>

#include "gp-launcher-button.h"
#include "gp-launcher-properties.h"
#include "gp-launcher-utils.h"

#define LAUNCHER_SCHEMA "org.gnome.gnome-panel.applet.launcher"

typedef struct
{
  GIcon *icon;
  gchar *text;
  gchar *path;
} ApplicationData;

typedef struct
{
  GpInitialSetupDialog *dialog;
  GtkTreeStore         *store;
  GSList               *applications;
} LauncherData;

typedef struct
{
  GSettings    *settings;

  GtkWidget    *button;
  GtkWidget    *image;

  char         *location;
  GKeyFile     *key_file;
  GFileMonitor *monitor;

  GtkWidget    *properties;
} GpLauncherAppletPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GpLauncherApplet, gp_launcher_applet, GP_TYPE_APPLET)

static ApplicationData *
application_data_new (GIcon       *icon,
                      const gchar *text,
                      const gchar *path)
{
  ApplicationData *data;

  data = g_new0 (ApplicationData, 1);

  data->icon = icon ? g_object_ref (icon) : NULL;
  data->text = g_strdup (text);
  data->path = g_strdup (path);

  return data;
}

static void
application_data_free (gpointer user_data)
{
  ApplicationData *data;

  data = (ApplicationData *) user_data;

  g_clear_object (&data->icon);
  g_free (data->text);
  g_free (data->path);
  g_free (data);
}

static LauncherData *
launcher_data_new (GpInitialSetupDialog *dialog)
{
  LauncherData *data;

  data = g_new0 (LauncherData, 1);
  data->dialog = dialog;

  return data;
}

static void
launcher_data_free (gpointer user_data)
{
  LauncherData *data;

  data = (LauncherData *) user_data;

  g_clear_object (&data->store);
  g_slist_free_full (data->applications, application_data_free);
  g_free (data);
}

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

static void
populate_from_root (GtkTreeStore       *store,
                    GtkTreeIter        *parent,
                    GMenuTreeDirectory *directory,
                    LauncherData       *data);

static void
append_directory (GtkTreeStore       *store,
                  GtkTreeIter        *parent,
                  GMenuTreeDirectory *directory,
                  LauncherData       *data)
{
  GIcon *icon;
  gchar *text;
  ApplicationData *app_data;
  GtkTreeIter iter;

  icon = gmenu_tree_directory_get_icon (directory);
  text = make_text (gmenu_tree_directory_get_name (directory),
                    gmenu_tree_directory_get_comment (directory));

  app_data = application_data_new (icon, text, NULL);
  data->applications = g_slist_prepend (data->applications, app_data);
  g_free (text);

  gtk_tree_store_append (store, &iter, parent);
  gtk_tree_store_set (store, &iter,
                      0, app_data->icon,
                      1, app_data->text,
                      2, NULL,
                      -1);

  populate_from_root (store, &iter, directory, data);
}

static void
append_entry (GtkTreeStore   *store,
              GtkTreeIter    *parent,
              GMenuTreeEntry *entry,
              LauncherData   *data)
{
  GAppInfo *app_info;
  GIcon *icon;
  gchar *text;
  const gchar *path;
  ApplicationData *app_data;
  GtkTreeIter iter;

  app_info = G_APP_INFO (gmenu_tree_entry_get_app_info (entry));

  icon = g_app_info_get_icon (app_info);
  text = make_text (g_app_info_get_display_name (app_info),
                    g_app_info_get_description (app_info));
  path = gmenu_tree_entry_get_desktop_file_path (entry);

  app_data = application_data_new (icon, text, path);
  data->applications = g_slist_prepend (data->applications, app_data);
  g_free (text);

  gtk_tree_store_append (store, &iter, parent);
  gtk_tree_store_set (store, &iter,
                      0, app_data->icon,
                      1, app_data->text,
                      2, app_data,
                      -1);
}

static void
append_alias (GtkTreeStore   *store,
              GtkTreeIter    *parent,
              GMenuTreeAlias *alias,
              LauncherData   *data)
{
  GMenuTreeItemType type;

  type = gmenu_tree_alias_get_aliased_item_type (alias);

  if (type == GMENU_TREE_ITEM_DIRECTORY)
    {
      GMenuTreeDirectory *dir;

      dir = gmenu_tree_alias_get_aliased_directory (alias);
      append_directory (store, parent, dir, data);
      gmenu_tree_item_unref (dir);
    }
  else if (type == GMENU_TREE_ITEM_ENTRY)
    {
      GMenuTreeEntry *entry;

      entry = gmenu_tree_alias_get_aliased_entry (alias);
      append_entry (store, parent, entry, data);
      gmenu_tree_item_unref (entry);
    }
}

static void
populate_from_root (GtkTreeStore       *store,
                    GtkTreeIter        *parent,
                    GMenuTreeDirectory *directory,
                    LauncherData       *data)
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
          append_directory (store, parent, dir, data);
          gmenu_tree_item_unref (dir);
        }
      else if (next_type == GMENU_TREE_ITEM_ENTRY)
        {
          GMenuTreeEntry *entry;

          entry = gmenu_tree_iter_get_entry (iter);
          append_entry (store, parent, entry, data);
          gmenu_tree_item_unref (entry);
        }
      else if (next_type == GMENU_TREE_ITEM_ALIAS)
        {
          GMenuTreeAlias *alias;

          alias = gmenu_tree_iter_get_alias (iter);
          append_alias (store, parent, alias, data);
          gmenu_tree_item_unref (alias);
        }

      next_type = gmenu_tree_iter_next (iter);
    }

  gmenu_tree_iter_unref (iter);
}

static void
populate_model_from_menu (GtkTreeStore *store,
                          const gchar  *menu,
                          gboolean      separator,
                          LauncherData *data)
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

  populate_from_root (store, NULL, root, data);
  gmenu_tree_item_unref (root);
  g_object_unref (tree);
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
populate_model (GtkTreeStore *store,
                LauncherData *data)
{
  gchar *menu;

  menu = get_applications_menu ();
  populate_model_from_menu (store, menu, FALSE, data);
  g_free (menu);

  menu = g_strdup ("gnomecc.menu");
  populate_model_from_menu (store, menu, TRUE, data);
  g_free (menu);
}

static void
selection_changed_cb (GtkTreeSelection *selection,
                      LauncherData     *data)
{
  gboolean done;
  GtkTreeModel *model;
  GtkTreeIter iter;

  done = FALSE;
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      ApplicationData *app_data;

      app_data = NULL;
      gtk_tree_model_get (model, &iter, 2, &app_data, -1);

      if (app_data != NULL)
        {
          GVariant *variant;

          variant = g_variant_new_string (app_data->path);

          gp_initital_setup_dialog_set_setting (data->dialog, "location", variant);
          done = TRUE;
        }
    }

  gp_initital_setup_dialog_set_done (data->dialog, done);
}

static gboolean
is_this_drop_ok (GtkWidget      *widget,
                 GdkDragContext *context)
{
  GtkWidget *source;
  GdkAtom text_uri_list;
  GList *list_targets;
  GList *l;

  source = gtk_drag_get_source_widget (context);

  if (source == widget)
    return FALSE;

  if (!(gdk_drag_context_get_actions (context) & GDK_ACTION_COPY))
    return FALSE;

  text_uri_list = gdk_atom_intern_static_string ("text/uri-list");

  list_targets = gdk_drag_context_list_targets (context);

  for (l = list_targets; l != NULL; l = l->next)
    {
      GdkAtom atom;

      atom = GDK_POINTER_TO_ATOM (l->data);

      if (atom == text_uri_list)
        return TRUE;
    }

  return FALSE;
}

static void launch (GpLauncherApplet *self,
                    GList            *uris);

static void
drag_data_received_cb (GtkWidget        *widget,
                       GdkDragContext   *context,
                       gint              x,
                       gint              y,
                       GtkSelectionData *data,
                       guint             info,
                       guint             time,
                       GpLauncherApplet *self)
{
  const guchar *selection_data;
  char **uris;
  GList *uri_list;
  int i;

  selection_data = gtk_selection_data_get_data (data);
  uris = g_uri_list_extract_uris ((const char *) selection_data);

  uri_list = NULL;
  for (i = 0; uris[i] != NULL; i++)
    uri_list = g_list_prepend (uri_list, uris[i]);

  uri_list = g_list_reverse (uri_list);
  launch (self, uri_list);

  g_list_free (uri_list);
  g_strfreev (uris);

  gtk_drag_finish (context, TRUE, FALSE, time);
}

static gboolean
drag_drop_cb (GtkWidget        *widget,
              GdkDragContext   *context,
              gint              x,
              gint              y,
              guint             time,
              GpLauncherApplet *self)
{
  GdkAtom text_uri_list;

  if (!is_this_drop_ok (widget, context))
    return FALSE;

  text_uri_list = gdk_atom_intern_static_string ("text/uri-list");

  gtk_drag_get_data (widget, context, text_uri_list, time);

  return TRUE;
}

static void
drag_leave_cb (GtkWidget        *widget,
               GdkDragContext   *context,
               guint             time,
               GpLauncherApplet *self)
{
  gtk_drag_unhighlight (widget);
}

static gboolean
drag_motion_cb (GtkWidget        *widget,
                GdkDragContext   *context,
                gint              x,
                gint              y,
                guint             time,
                GpLauncherApplet *self)
{
  if (!is_this_drop_ok (widget, context))
    return FALSE;

  gdk_drag_status (context, GDK_ACTION_COPY, time);
  gtk_drag_highlight (widget);

  return TRUE;
}

static void
setup_drop_destination (GpLauncherApplet *self)
{
  GtkTargetList *target_list;
  GdkAtom target;

  gtk_drag_dest_set (GTK_WIDGET (self), 0, NULL, 0, 0);

  target_list = gtk_target_list_new (NULL, 0);

  target = gdk_atom_intern_static_string ("text/uri-list");
  gtk_target_list_add (target_list, target, 0, 0);

  gtk_drag_dest_set_target_list (GTK_WIDGET (self), target_list);
  gtk_target_list_unref (target_list);

  g_signal_connect (self, "drag-data-received",
                    G_CALLBACK (drag_data_received_cb), self);

  g_signal_connect (self, "drag-drop",
                    G_CALLBACK (drag_drop_cb), self);

  g_signal_connect (self, "drag-leave",
                    G_CALLBACK (drag_leave_cb), self);

  g_signal_connect (self, "drag-motion",
                    G_CALLBACK (drag_motion_cb), self);
}

/* zoom factor, steps and delay if composited (factor must be odd) */
#define ZOOM_FACTOR 5
#define ZOOM_STEPS  14
#define ZOOM_DELAY 10

typedef struct
{
  int              size;
  int              size_start;
  int              size_end;
  GtkPositionType  position;
  double           opacity;
  GIcon           *icon;
  guint            timeout_id;
  GtkWidget       *win;
} ZoomData;

static gboolean
zoom_draw_cb (GtkWidget *widget,
              cairo_t   *cr,
              ZoomData  *zoom)
{
  GtkIconInfo *icon_info;
  GdkPixbuf *pixbuf;
  int width;
  int height;
  int x;
  int y;

  icon_info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                              zoom->icon,
                                              zoom->size,
                                              GTK_ICON_LOOKUP_FORCE_SIZE);

  if (icon_info == NULL)
    return FALSE;

  pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
  g_object_unref (icon_info);

  if (pixbuf == NULL)
    return FALSE;

  gtk_window_get_size (GTK_WINDOW (zoom->win), &width, &height);

  switch (zoom->position)
    {
      case GTK_POS_TOP:
        x = (width - gdk_pixbuf_get_width (pixbuf)) / 2;
        y = 0;
        break;

      case GTK_POS_BOTTOM:
        x = (width - gdk_pixbuf_get_width (pixbuf)) / 2;
        y = height - gdk_pixbuf_get_height (pixbuf);
        break;

      case GTK_POS_LEFT:
        x = 0;
        y = (height - gdk_pixbuf_get_height (pixbuf)) / 2;
        break;

      case GTK_POS_RIGHT:
        x = width - gdk_pixbuf_get_width (pixbuf);
        y = (height - gdk_pixbuf_get_height (pixbuf)) / 2;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);
  g_object_unref (pixbuf);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_paint_with_alpha (cr, MAX (zoom->opacity, 0));

  return FALSE;
}

static gboolean
zoom_timeout_cb (gpointer user_data)
{
  ZoomData *zoom;

  zoom = user_data;

  if (zoom->size >= zoom->size_end)
    {
      gtk_widget_destroy (zoom->win);
      g_object_unref (zoom->icon);

      g_free (zoom);

      return G_SOURCE_REMOVE;
    }

  zoom->size += MAX ((zoom->size_end - zoom->size_start) / ZOOM_STEPS, 1);
  zoom->opacity -= 1.0 / (ZOOM_STEPS + 1.0);

  gtk_widget_queue_draw (zoom->win);

  return G_SOURCE_CONTINUE;
}

static void
draw_zoom_animation (GpLauncherApplet *self,
                     int               x,
                     int               y,
                     int               width,
                     int               height,
                     GIcon            *icon,
                     GtkPositionType   position)
{
  ZoomData *zoom;
  GdkScreen *screen;
  GdkVisual *visual;
  int wx;
  int wy;

  width += 2;
  height += 2;

  zoom = g_new0 (ZoomData, 1);

  zoom->size = MIN (width, height);
  zoom->size_start = zoom->size;
  zoom->size_end = zoom->size * ZOOM_FACTOR;
  zoom->position = position;
  zoom->opacity = 1.0;
  zoom->icon = g_object_ref (icon);
  zoom->timeout_id = 0;

  zoom->win = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_window_set_keep_above (GTK_WINDOW (zoom->win), TRUE);
  gtk_window_set_decorated (GTK_WINDOW (zoom->win), FALSE);
  gtk_widget_set_app_paintable (zoom->win, TRUE);

  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  visual = gdk_screen_get_rgba_visual (screen);
  gtk_widget_set_visual (zoom->win, visual);

  gtk_window_set_gravity (GTK_WINDOW (zoom->win), GDK_GRAVITY_STATIC);
  gtk_window_set_default_size (GTK_WINDOW (zoom->win),
                               width * ZOOM_FACTOR,
                               height * ZOOM_FACTOR);

  switch (position)
    {
      case GTK_POS_TOP:
        wx = x - width * (ZOOM_FACTOR / 2);
        wy = y;
        break;

      case GTK_POS_BOTTOM:
        wx = x - width * (ZOOM_FACTOR / 2);
        wy = y - height * (ZOOM_FACTOR - 1);
        break;

      case GTK_POS_LEFT:
        wx = x;
        wy = y - height * (ZOOM_FACTOR / 2);
        break;

      case GTK_POS_RIGHT:
        wx = x - width * (ZOOM_FACTOR - 1);
        wy = y - height * (ZOOM_FACTOR / 2);
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  g_signal_connect (zoom->win, "draw", G_CALLBACK (zoom_draw_cb), zoom);

  gtk_window_move (GTK_WINDOW (zoom->win), wx, wy);
  gtk_widget_realize (zoom->win);
  gtk_widget_show (zoom->win);

  zoom->timeout_id = g_timeout_add (ZOOM_DELAY, zoom_timeout_cb, zoom);
  g_source_set_name_by_id (zoom->timeout_id, "[gnome-panel] zoom_timeout_cb");
}

static void
launch_animation (GpLauncherApplet *self)
{
  GpLauncherAppletPrivate *priv;
  GdkScreen *screen;
  GtkSettings *settings;
  gboolean enable_animations;
  GIcon *icon;
  int x;
  int y;
  GtkAllocation allocation;
  GtkPositionType position;

  priv = gp_launcher_applet_get_instance_private (self);

  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  settings = gtk_widget_get_settings (GTK_WIDGET (self));

  enable_animations = TRUE;
  g_object_get (settings, "gtk-enable-animations", &enable_animations, NULL);

  if (!enable_animations || !gdk_screen_is_composited (screen))
    return;

  icon = NULL;
  gtk_image_get_gicon (GTK_IMAGE (priv->image), &icon, NULL);

  if (icon == NULL)
    return;

  gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (self)), &x, &y);
  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

  position = gp_applet_get_position (GP_APPLET (self));

  draw_zoom_animation (self,
                       x,
                       y,
                       allocation.width,
                       allocation.height,
                       icon,
                       position);
}

static void
child_setup (gpointer user_data)
{
  GAppInfo *info;
  const gchar *id;
  gint stdout_fd;
  gint stderr_fd;

  info = G_APP_INFO (user_data);
  id = g_app_info_get_id (info);

  stdout_fd = sd_journal_stream_fd (id, LOG_INFO, FALSE);
  if (stdout_fd >= 0)
    {
      dup2 (stdout_fd, STDOUT_FILENO);
      close (stdout_fd);
    }

  stderr_fd = sd_journal_stream_fd (id, LOG_WARNING, FALSE);
  if (stderr_fd >= 0)
    {
      dup2 (stderr_fd, STDERR_FILENO);
      close (stderr_fd);
    }
}

static void
close_pid (GPid     pid,
           gint     status,
           gpointer user_data)
{
  g_spawn_close_pid (pid);
}

static void
pid_cb (GDesktopAppInfo *info,
        GPid             pid,
        gpointer         user_data)
{
  g_child_watch_add (pid, close_pid, NULL);
}

static void
launch (GpLauncherApplet *self,
        GList            *uris)
{
  GpLauncherAppletPrivate *priv;
  char *type;
  char *command;

  priv = gp_launcher_applet_get_instance_private (self);

  type = NULL;
  command = NULL;

  if (!gp_launcher_read_from_key_file (priv->key_file,
                                       NULL,
                                       &type,
                                       NULL,
                                       &command,
                                       NULL,
                                       NULL))
    return;

  launch_animation (self);

  if (g_strcmp0 (type, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) == 0)
    {
      GDesktopAppInfo *app_info;

      app_info = g_desktop_app_info_new_from_keyfile (priv->key_file);

      if (app_info != NULL)
        {
          GSpawnFlags flags;
          GError *error;

          flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;

          error = NULL;
          g_desktop_app_info_launch_uris_as_manager (app_info,
                                                     uris,
                                                     NULL,
                                                     flags,
                                                     child_setup,
                                                     app_info,
                                                     pid_cb,
                                                     NULL,
                                                     &error);

          if (error != NULL)
            {
              gp_launcher_show_error_message (NULL,
                                              _("Could not launch application"),
                                              error->message);

              g_error_free (error);
            }

          g_object_unref (app_info);
        }
      else
        {
          char *error_message;

          error_message = g_strdup_printf (_("Can not execute “%s” command line."),
                                           command);

          gp_launcher_show_error_message (NULL,
                                          _("Could not launch application"),
                                          error_message);

          g_free (error_message);
        }
    }
  else if (g_strcmp0 (type, G_KEY_FILE_DESKTOP_TYPE_LINK) == 0)
    {
      GError *error;

      error = NULL;
      gtk_show_uri_on_window (NULL,
                              command,
                              gtk_get_current_event_time (),
                              &error);

      if (error != NULL)
        {
          gp_launcher_show_error_message (NULL,
                                          _("Could not open location"),
                                          error->message);

          g_error_free (error);
        }
    }

  g_free (type);
  g_free (command);
}

static void
launcher_error (GpLauncherApplet *self,
                const char       *error)
{
  GpLauncherAppletPrivate *priv;
  guint icon_size;

  priv = gp_launcher_applet_get_instance_private (self);

  gtk_widget_set_tooltip_text (GTK_WIDGET (self), error);

  gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
                                "gnome-panel-launcher",
                                GTK_ICON_SIZE_MENU);

  icon_size = gp_applet_get_panel_icon_size (GP_APPLET (self));
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);
}

static void
update_icon (GpLauncherApplet *self,
             const char       *icon_name)
{
  GpLauncherAppletPrivate *priv;
  GIcon *icon;
  guint icon_size;

  priv = gp_launcher_applet_get_instance_private (self);
  icon = NULL;

  if (icon_name != NULL && *icon_name != '\0')
    {
      if (g_path_is_absolute (icon_name))
        {
          GFile *file;

          file = g_file_new_for_path (icon_name);
          icon = g_file_icon_new (file);
          g_object_unref (file);
        }
      else
        {
          char *p;

          /* Work around a common mistake in desktop files */
          if ((p = strrchr (icon_name, '.')) != NULL &&
              (strcmp (p, ".png") == 0 ||
               strcmp (p, ".xpm") == 0 ||
               strcmp (p, ".svg") == 0))
            *p = '\0';

          icon = g_themed_icon_new (icon_name);
        }
    }

  if (icon == NULL)
    icon = g_themed_icon_new ("gnome-panel-launcher");

  gtk_image_set_from_gicon (GTK_IMAGE (priv->image), icon, GTK_ICON_SIZE_MENU);
  g_object_unref (icon);

  icon_size = gp_applet_get_panel_icon_size (GP_APPLET (self));
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);
}

static void
update_tooltip (GpLauncherApplet *self,
                const char       *name,
                const char       *comment)
{
  char *tooltip;

  if (name != NULL && *name != '\0' && comment != NULL && *comment != '\0')
    tooltip = g_strdup_printf ("%s\n%s", name, comment);
  else if (name != NULL && *name != '\0')
    tooltip = g_strdup (name);
  else if (comment != NULL && *comment != '\0')
    tooltip = g_strdup (comment);
  else
    tooltip = NULL;

  gtk_widget_set_tooltip_text (GTK_WIDGET (self), tooltip);
  g_free (tooltip);

  g_object_bind_property (self,
                          "enable-tooltips",
                          self,
                          "has-tooltip",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
}

static void
update_launcher (GpLauncherApplet *self)
{
  GpLauncherAppletPrivate *priv;
  GError *error;
  char *error_message;
  char *icon;
  char *name;
  char *comment;
  AtkObject *atk;

  priv = gp_launcher_applet_get_instance_private (self);

  error = NULL;
  error_message = NULL;

  if (!g_key_file_load_from_file (priv->key_file,
                                  priv->location,
                                  G_KEY_FILE_NONE,
                                  &error))
    {
      error_message = g_strdup_printf (_("Failed to load key file “%s”: %s"),
                                       priv->location,
                                       error->message);

      g_error_free (error);

      launcher_error (self, error_message);
      g_free (error_message);

      return;
    }

  icon = NULL;
  name = NULL;
  comment = NULL;

  if (!gp_launcher_read_from_key_file (priv->key_file,
                                       &icon,
                                       NULL,
                                       &name,
                                       NULL,
                                       &comment,
                                       &error_message))
    {
      launcher_error (self, error_message);
      g_free (error_message);

      return;
    }

  update_icon (self, icon);
  update_tooltip (self, name, comment);

  atk = gtk_widget_get_accessible (GTK_WIDGET (self));
  atk_object_set_name (atk, name != NULL ? name : "");
  atk_object_set_description (atk, comment != NULL ? comment : "");

  g_free (icon);
  g_free (name);
  g_free (comment);
}

static void
lockdown_changed (GpLauncherApplet *self)
{
  GpLauncherAppletPrivate *priv;
  GpLockdownFlags lockdowns;
  gboolean applet_sensitive;
  GAction *action;
  gboolean properties_enabled;

  priv = gp_launcher_applet_get_instance_private (self);

  lockdowns = gp_applet_get_lockdowns (GP_APPLET (self));
  applet_sensitive = TRUE;

  if ((lockdowns & GP_LOCKDOWN_FLAGS_APPLET) == GP_LOCKDOWN_FLAGS_APPLET)
    applet_sensitive = FALSE;

  if ((lockdowns & GP_LOCKDOWN_FLAGS_COMMAND_LINE) == GP_LOCKDOWN_FLAGS_COMMAND_LINE &&
      g_str_has_prefix (priv->location, g_get_home_dir ()))
    applet_sensitive = FALSE;

  gtk_widget_set_sensitive (GTK_WIDGET (self), applet_sensitive);

  properties_enabled = (lockdowns & GP_LOCKDOWN_FLAGS_LOCKED_DOWN) != GP_LOCKDOWN_FLAGS_LOCKED_DOWN;

  action = gp_applet_menu_lookup_action (GP_APPLET (self), "properties");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), properties_enabled);
}

static void
file_changed_cb (GFileMonitor     *monitor,
                 GFile            *file,
                 GFile            *other_file,
                 GFileMonitorEvent event_type,
                 GpLauncherApplet *self)
{
  update_launcher (self);
  lockdown_changed (self);
}

static void
location_changed (GpLauncherApplet *self)
{
  GpLauncherAppletPrivate *priv;
  GFile *file;

  priv = gp_launcher_applet_get_instance_private (self);

  g_clear_pointer (&priv->location, g_free);
  g_clear_pointer (&priv->key_file, g_key_file_unref);
  g_clear_object (&priv->monitor);

  priv->location = g_settings_get_string (priv->settings, "location");

  if (!g_path_is_absolute (priv->location))
    {
      char *launchers_dir;
      char *filename;

      launchers_dir = gp_launcher_get_launchers_dir ();

      filename = g_build_filename (launchers_dir, priv->location, NULL);
      g_free (launchers_dir);

      g_free (priv->location);
      priv->location = filename;
    }

  priv->key_file = g_key_file_new ();

  file = g_file_new_for_path (priv->location);
  priv->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
  g_file_monitor_set_rate_limit (priv->monitor, 200);
  g_object_unref (file);

  g_signal_connect (priv->monitor,
                    "changed",
                    G_CALLBACK (file_changed_cb),
                    self);

  update_launcher (self);
}

static void
location_changed_cb (GSettings        *settings,
                     const char       *key,
                     GpLauncherApplet *self)
{
  location_changed (self);
}

static void
lockdowns_cb (GpApplet         *applet,
              GParamSpec       *pspec,
              GpLauncherApplet *self)
{
  lockdown_changed (self);
}

static void
panel_icon_size_cb (GpApplet         *applet,
                    GParamSpec       *pspec,
                    GpLauncherApplet *self)
{
  GpLauncherAppletPrivate *priv;
  guint icon_size;

  priv = gp_launcher_applet_get_instance_private (self);

  icon_size = gp_applet_get_panel_icon_size (applet);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);
}

static void
launch_cb (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  launch (GP_LAUNCHER_APPLET (user_data), NULL);
}

static void
properties_cb (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GpLauncherApplet *self;
  GpLauncherAppletPrivate *priv;

  self = GP_LAUNCHER_APPLET (user_data);
  priv = gp_launcher_applet_get_instance_private (self);

  if (priv->properties != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->properties));
      return;
    }

  priv->properties = gp_launcher_properties_new (priv->settings);
  g_object_add_weak_pointer (G_OBJECT (priv->properties),
                             (gpointer *) &priv->properties);

  gtk_window_present (GTK_WINDOW (priv->properties));
}

static const GActionEntry launcher_menu_actions[] =
  {
    { "launch", launch_cb, NULL, NULL, NULL },
    { "properties", properties_cb, NULL, NULL, NULL },
    { NULL }
  };

static void
setup_menu (GpLauncherApplet *self)
{
  GpApplet *applet;
  const gchar *resource;

  applet = GP_APPLET (self);

  resource = GP_LAUNCHER_APPLET_GET_CLASS (self)->get_menu_resource ();
  gp_applet_setup_menu_from_resource (applet, resource, launcher_menu_actions);

  lockdown_changed (self);
}

static void
clicked_cb (GtkWidget        *widget,
            GpLauncherApplet *self)
{
  launch (self, NULL);
}

static void
setup_button (GpLauncherApplet *self)
{
  GpLauncherAppletPrivate *priv;
  guint icon_size;

  priv = gp_launcher_applet_get_instance_private (self);

  priv->button = gp_launcher_button_new ();
  gtk_container_add (GTK_CONTAINER (self), priv->button);
  gtk_widget_show (priv->button);

  g_signal_connect (priv->button, "clicked",
                    G_CALLBACK (clicked_cb), self);

  priv->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (priv->button), priv->image);
  gtk_widget_show (priv->image);

  icon_size = gp_applet_get_panel_icon_size (GP_APPLET (self));
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);
}

static void
gp_launcher_applet_setup (GpLauncherApplet *self)
{
  GpLauncherAppletPrivate *priv;

  priv = gp_launcher_applet_get_instance_private (self);

  priv->settings = gp_applet_settings_new (GP_APPLET (self), LAUNCHER_SCHEMA);

  g_signal_connect (priv->settings,
                    "changed::location",
                    G_CALLBACK (location_changed_cb),
                    self);

  g_signal_connect (self,
                    "notify::lockdowns",
                    G_CALLBACK (lockdowns_cb),
                    self);

  g_signal_connect (self,
                    "notify::panel-icon-size",
                    G_CALLBACK (panel_icon_size_cb),
                    self);

  setup_menu (self);
  setup_button (self);

  setup_drop_destination (self);

  location_changed (self);
}

static void
gp_launcher_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_launcher_applet_parent_class)->constructed (object);
  gp_launcher_applet_setup (GP_LAUNCHER_APPLET (object));
}

static void
gp_launcher_applet_dispose (GObject *object)
{
  GpLauncherApplet *self;
  GpLauncherAppletPrivate *priv;

  self = GP_LAUNCHER_APPLET (object);
  priv = gp_launcher_applet_get_instance_private (self);

  g_clear_object (&priv->settings);

  g_clear_pointer (&priv->key_file, g_key_file_unref);
  g_clear_object (&priv->monitor);

  g_clear_pointer (&priv->properties, gtk_widget_destroy);

  G_OBJECT_CLASS (gp_launcher_applet_parent_class)->dispose (object);
}

static void
gp_launcher_applet_finalize (GObject *object)
{
  GpLauncherApplet *self;
  GpLauncherAppletPrivate *priv;

  self = GP_LAUNCHER_APPLET (object);
  priv = gp_launcher_applet_get_instance_private (self);

  g_clear_pointer (&priv->location, g_free);

  G_OBJECT_CLASS (gp_launcher_applet_parent_class)->finalize (object);
}

static void
gp_launcher_applet_initial_setup (GpApplet *applet,
                                  GVariant *initial_settings)
{
  GSettings *settings;
  const char *location;

  settings = gp_applet_settings_new (applet, LAUNCHER_SCHEMA);

  location = NULL;
  if (g_variant_lookup (initial_settings, "location", "&s", &location))
    {
      g_settings_set_string (settings, "location", location);
    }
  else
    {
      const char *type;
      const char *icon;
      const char *name;
      const char *command;
      const char *comment;
      GKeyFile *file;
      char *filename;
      GError *error;

      type = NULL;
      icon = NULL;
      name = NULL;
      command = NULL;
      comment = NULL;

      g_variant_lookup (initial_settings, "type", "&s", &type);
      g_variant_lookup (initial_settings, "icon", "&s", &icon);
      g_variant_lookup (initial_settings, "name", "&s", &name);
      g_variant_lookup (initial_settings, "command", "&s", &command);
      g_variant_lookup (initial_settings, "comment", "&s", &comment);

      file = g_key_file_new ();

      g_key_file_set_string (file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_VERSION, "1.0");

      g_key_file_set_string (file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_TYPE,
                             type);

      g_key_file_set_string (file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_ICON,
                             icon);

      g_key_file_set_string (file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_NAME,
                             name);

      if (comment != NULL)
        {
          g_key_file_set_string (file,
                                 G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_COMMENT,
                                 comment);
        }

      if (g_strcmp0 (type, "Application") == 0)
        {
          gboolean terminal;

          g_key_file_set_string (file,
                                 G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_EXEC,
                                 command);

          if (g_variant_lookup (initial_settings, "terminal", "b", &terminal))
            {
              g_key_file_set_boolean (file,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                                      terminal);
            }
        }
      else if (g_strcmp0 (type, "Link") == 0)
        {
          g_key_file_set_string (file,
                                 G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_URL,
                                 command);
        }
      else
        {
          g_assert_not_reached ();
        }

      filename = gp_launcher_get_unique_filename ();

      error = NULL;
      if (!g_key_file_save_to_file (file, filename, &error))
        {
          g_warning ("%s", error->message);
          g_error_free (error);
        }
      else
        {
          char *basename;

          basename = g_path_get_basename (filename);
          g_settings_set_string (settings, "location", basename);
          g_free (basename);
        }

      g_key_file_unref (file);
      g_free (filename);
    }

  g_object_unref (settings);
}

static void
delete_cb (GObject      *source_object,
           GAsyncResult *res,
           gpointer      user_data)
{
  GError *error;

  error = NULL;
  g_file_delete_finish (G_FILE (source_object), res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to delete launcher file: %s", error->message);

      g_error_free (error);
      return;
    }
}

static void
gp_launcher_applet_remove_from_panel (GpApplet *applet)
{
  GpLauncherApplet *self;
  GpLauncherAppletPrivate *priv;
  char *launchers_dir;

  self = GP_LAUNCHER_APPLET (applet);
  priv = gp_launcher_applet_get_instance_private (self);

  launchers_dir = gp_launcher_get_launchers_dir ();

  if (g_str_has_prefix (priv->location, launchers_dir))
    {
      GFile *file;

      file = g_file_new_for_path (priv->location);

      g_file_delete_async (file, G_PRIORITY_DEFAULT, NULL, delete_cb, NULL);
      g_object_unref (file);
    }

  g_free (launchers_dir);
}

static const char *
gp_launcher_applet_get_menu_resource (void)
{
  return GRESOURCE_PREFIX "/launcher-menu.ui";
}

static void
gp_launcher_applet_class_init (GpLauncherAppletClass *self_class)
{
  GObjectClass *object_class;
  GpAppletClass *applet_class;

  object_class = G_OBJECT_CLASS (self_class);
  applet_class = GP_APPLET_CLASS (self_class);

  object_class->constructed = gp_launcher_applet_constructed;
  object_class->dispose = gp_launcher_applet_dispose;
  object_class->finalize = gp_launcher_applet_finalize;

  applet_class->initial_setup = gp_launcher_applet_initial_setup;
  applet_class->remove_from_panel = gp_launcher_applet_remove_from_panel;

  self_class->get_menu_resource = gp_launcher_applet_get_menu_resource;
}

static void
gp_launcher_applet_init (GpLauncherApplet *self)
{
  GpApplet *applet;

  applet = GP_APPLET (self);

  gp_applet_set_flags (applet, GP_APPLET_FLAGS_EXPAND_MINOR);
}

void
gp_launcher_applet_initial_setup_dialog (GpInitialSetupDialog *dialog)
{
  LauncherData *data;
  GtkWidget *scrolled;
  GtkWidget *tree_view;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  data = launcher_data_new (dialog);

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

  gp_initital_setup_dialog_add_content_widget (dialog, scrolled, data,
                                               launcher_data_free);
}

/*
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
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libgnome-desktop/gnome-systemd.h>
#include <systemd/sd-journal.h>

#include "gp-menu-utils.h"

static gchar *
get_file_description (GFile *file)
{
  GFileInfo *info;
  GFileQueryInfoFlags flags;
  const gchar *attribute;
  gchar *description;

  flags = G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS;
  info = g_file_query_info (file, "standard::description", flags, NULL, NULL);

  if (info == NULL)
    return NULL;

  attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
  description = g_strdup (g_file_info_get_attribute_string (info, attribute));
  g_object_unref (info);

  return description;
}

static gchar *
get_file_display_name (GFile    *file,
                       gboolean  use_fallback)
{
  GFileInfo *info;
  GFileQueryInfoFlags flags;
  gchar *description;

  flags = G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS;
  info = g_file_query_info (file, "standard::display-name", flags, NULL, NULL);
  description = NULL;

  if (info != NULL)
    {
      description = g_strdup (g_file_info_get_display_name (info));
      g_object_unref (info);
    }

  if (description == NULL && use_fallback)
    {
      gchar *basename;

      basename = g_file_get_basename (file);
      description = g_filename_display_name (basename);
      g_free (basename);
    }

  return description;
}

static GFile *
get_file_root (GFile *file)
{
  GFile *parent;
  GFile *root;

  parent = g_file_get_parent (file);
  if (parent == NULL)
    return g_object_ref (file);

  root = parent;
  while ((parent = g_file_get_parent (root)) != NULL)
    {
      g_object_unref (root);
      root = parent;
    }

  return root;
}

static gchar *
get_root_label (GFile *file)
{
  GFile *root;
  gchar *label;
  gboolean is_equal;
  gchar *display_name;
  gchar *ret;

  root = get_file_root (file);
  label = get_file_description (root);

  if (label == NULL)
    label = get_file_display_name (root, FALSE);

  if (label == NULL)
    label = g_file_get_uri_scheme (root);

  is_equal = g_file_equal (file, root);
  g_object_unref (root);

  if (is_equal)
    return label;

  display_name = get_file_display_name (file, TRUE);

  /* Translators: the first string is the name of a gvfs method, and the
   * second string is a path. For example, "Trash: some-directory". It
   * means that the directory called "some-directory" is in the trash.
   */
  ret = g_strdup_printf (_("%1$s: %2$s"), label, display_name);

  g_free (display_name);
  g_free (label);

  return ret;
}

static GIcon *
get_icon_if_mount (GFile *file)
{
  GMount *mount;
  GIcon *icon;

  mount = g_file_find_enclosing_mount (file, NULL, NULL);
  if (mount == NULL)
    return NULL;

  icon = g_mount_get_icon (mount);
  g_object_unref (mount);

  return icon;
}

static gchar *
get_label_if_mount (GFile *file)
{
  GMount *mount;
  gchar *label;

  mount = g_file_find_enclosing_mount (file, NULL, NULL);
  if (mount == NULL)
    return NULL;

  label = g_mount_get_name (mount);
  g_object_unref (mount);

  return label;
}

static GIcon *
get_icon_if_trash (GFile *file)
{
  gchar *uri;
  gboolean is_trash;
  GFile *root;
  GFileQueryInfoFlags flags;
  GFileInfo *info;
  GIcon *icon;

  uri = g_file_get_uri (file);
  is_trash = g_str_has_prefix (uri, "trash:");
  g_free (uri);

  if (!is_trash)
    return NULL;

  root = get_file_root (file);
  flags = G_FILE_QUERY_INFO_NONE;

  info = g_file_query_info (root, "standard::icon", flags, NULL, NULL);
  g_object_unref (root);

  if (info == NULL)
    return NULL;

  icon = g_object_ref (g_file_info_get_icon (info));
  g_object_unref (info);

  return icon;
}

static gchar *
get_label_if_home_dir (GFile *file)
{
  GFile *compare;
  gboolean is_home_dir;

  compare = g_file_new_for_path (g_get_home_dir ());
  is_home_dir = g_file_equal (file, compare);
  g_object_unref (compare);

  if (!is_home_dir)
    return NULL;

  return g_strdup (_("Home"));
}

static gchar *
get_label_if_root_dir (GFile *file)
{
  GFile *compare;
  gboolean is_root_dir;

  compare = g_file_new_for_path ("/");
  is_root_dir = g_file_equal (file, compare);
  g_object_unref (compare);

  if (!is_root_dir)
    return NULL;

  /* Translators: this is the same string as the one found in nautilus */
  return g_strdup (_("File System"));
}

static gchar *
get_label_if_file (GFile *file)
{
  gchar *uri;
  gboolean is_file;
  gchar *label;

  uri = g_file_get_uri (file);
  is_file = g_str_has_prefix (uri, "file:");
  g_free (uri);

  if (!is_file)
    return NULL;

  label = get_label_if_home_dir (file);
  if (label != NULL)
    return label;

  label = get_label_if_root_dir (file);
  if (label != NULL)
    return label;

  label = get_file_description (file);
  if (label != NULL)
    return label;

  return get_file_display_name (file, TRUE);
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
  const gchar *app_name;

  g_child_watch_add (pid, close_pid, NULL);

  app_name = g_app_info_get_id (G_APP_INFO (info));
  if (app_name == NULL)
    app_name = g_app_info_get_executable (G_APP_INFO (info));

  /* Start async request; we don't care about the result */
  gnome_start_systemd_scope (app_name,
                             pid,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
}

static gboolean
app_info_launch_uris (GDesktopAppInfo  *info,
                      GList            *uris,
                      GError          **error)
{
  GdkDisplay *display;
  GdkAppLaunchContext *context;
  GSpawnFlags flags;
  gboolean ret;

  display = gdk_display_get_default ();
  context = gdk_display_get_app_launch_context (display);
  flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;

  ret = g_desktop_app_info_launch_uris_as_manager (info,
                                                   uris,
                                                   G_APP_LAUNCH_CONTEXT (context),
                                                   flags,
                                                   child_setup,
                                                   info,
                                                   pid_cb,
                                                   NULL,
                                                   error);

  g_object_unref (context);

  return ret;
}

static GAppInfo *
get_app_info_for_uri (const gchar  *uri,
                      GError      **error)
{
  GAppInfo *app_info;
  gchar *scheme;
  GFile *file;

  app_info = NULL;
  scheme = g_uri_parse_scheme (uri);

  if (scheme && scheme[0] != '\0')
    app_info = g_app_info_get_default_for_uri_scheme (scheme);

  g_free (scheme);

  if (app_info != NULL)
    return app_info;

  file = g_file_new_for_uri (uri);
  app_info = g_file_query_default_handler (file, NULL, error);
  g_object_unref (file);

  return app_info;
}

static gboolean
launch_uri (const char  *uri,
            GError     **error)
{
  GAppInfo *app_info;

  app_info = get_app_info_for_uri (uri, error);

  if (app_info != NULL)
    {
      GList *uris;
      gboolean success;

      uris = g_list_append (NULL, (gchar *) uri);
      success = app_info_launch_uris (G_DESKTOP_APP_INFO (app_info),
                                      uris, error);

      g_object_unref (app_info);
      g_list_free (uris);

      return success;
    }

  return FALSE;
}

static void
launch_uri_show_error_dialog (const char *uri,
                              GError     *error)
{
  char *message;

  message = g_strdup_printf (_("Could not open location '%s'"), uri);

  gp_menu_utils_show_error_dialog (message, error);
  g_free (message);
}

static void
mount_enclosing_volume_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  GFile *file;
  GMountOperation *operation;
  GError *error;

  file = G_FILE (source_object);
  operation = G_MOUNT_OPERATION (user_data);
  error = NULL;

  if (g_file_mount_enclosing_volume_finish (file, res, &error))
    {
      char *uri;

      uri = g_file_get_uri (file);

      if (!launch_uri (uri, &error))
        {
          launch_uri_show_error_dialog (uri, error);
          g_clear_error (&error);
        }

      g_free (uri);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        {
          char *uri;

          uri = g_file_get_uri (file);
          launch_uri_show_error_dialog (uri, error);
          g_free (uri);
        }

      g_clear_error (&error);
    }

  g_object_unref (operation);
}

void
gp_menu_utils_app_info_launch (GDesktopAppInfo *app_info)
{
  GError *error;
  const gchar *display_name;
  gchar *message;

  error = NULL;
  if (app_info_launch_uris (app_info, NULL, &error))
    return;

  display_name = g_app_info_get_display_name (G_APP_INFO (app_info));
  message = g_strdup_printf (_("Could not launch '%s'"), display_name);

  gp_menu_utils_show_error_dialog (message, error);

  g_clear_error (&error);
  g_free (message);
}

void
gp_menu_utils_launch_uri (const gchar *uri)
{
  GError *error;

  error = NULL;
  if (launch_uri (uri, &error))
    return;

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED))
    {
      GFile *file;
      GMountOperation *operation;

      file = g_file_new_for_uri (uri);
      operation = gtk_mount_operation_new (NULL);

      g_file_mount_enclosing_volume (file,
                                     G_MOUNT_MOUNT_NONE,
                                     operation,
                                     NULL,
                                     mount_enclosing_volume_cb,
                                     operation);

      g_clear_error (&error);
      g_object_unref (file);

      return;
    }

  launch_uri_show_error_dialog (uri, error);
  g_clear_error (&error);
}

GIcon *
gp_menu_utils_get_icon_for_file (GFile *file)
{
  GIcon *icon;
  GFileQueryInfoFlags flags;
  GFileInfo *info;

  icon = get_icon_if_mount (file);
  if (icon != NULL)
    return icon;

  icon = get_icon_if_trash (file);
  if (icon != NULL)
    return icon;

  flags = G_FILE_QUERY_INFO_NONE;
  info = g_file_query_info (file, "standard::icon", flags, NULL, NULL);

  if (info == NULL)
    return NULL;

  icon = g_object_ref (g_file_info_get_icon (info));
  g_object_unref (info);

  return icon;
}

gchar *
gp_menu_utils_get_label_for_file (GFile *file)
{
  gchar *label;

  label = get_label_if_mount (file);
  if (label != NULL)
    return label;

  label = get_label_if_file (file);
  if (label != NULL)
    return label;

  label = get_file_description (file);
  if (label != NULL)
    return label;

  return get_root_label (file);
}

void
gp_menu_utils_show_error_dialog (const gchar *message,
                                 GError      *error)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                   "%s", message);

  if (error != NULL)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", error->message);

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));
}

gchar *
gp_menu_utils_get_user_name (void)
{
  const gchar *name;
  gchar *user_name;

  name = g_get_real_name ();
  if (name == NULL || *name == '\0' || g_strcmp0 (name, "Unknown") == 0)
    name = g_get_user_name ();

  user_name = NULL;
  if (name != NULL)
    user_name = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);

  if (user_name == NULL)
    user_name = g_strdup (name != NULL ? name : "Unknown");

  return user_name;
}

gchar *
gp_menu_utils_get_applications_menu (void)
{
  const gchar *xdg_menu_prefx;

  xdg_menu_prefx = g_getenv ("XDG_MENU_PREFIX");
  if (!xdg_menu_prefx || *xdg_menu_prefx == '\0')
    return g_strdup ("gnome-applications.menu");

  return g_strdup_printf ("%sapplications.menu", xdg_menu_prefx);
}

void
append_separator_if_needed (GtkMenu *menu)
{
  GList *children;
  GList *last;
  GtkWidget *item;

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  last = g_list_last (children);

  if (last == NULL)
    return;

  if (GTK_IS_SEPARATOR_MENU_ITEM (last->data))
    {
      g_list_free (children);
      return;
    }

  g_list_free (children);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  gtk_widget_set_sensitive (item, FALSE);
}

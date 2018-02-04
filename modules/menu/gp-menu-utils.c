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

  g_object_ref (file);
  while ((parent = g_file_get_parent (file)) != NULL)
    {
      g_object_unref (file);
      file = parent;
    }

  return file;
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
pid_cb (GDesktopAppInfo *info,
        GPid             pid,
        gpointer         user_data)
{
  g_child_watch_add (pid, (GChildWatchFunc) g_spawn_close_pid, NULL);
}

void
gp_menu_utils_launch_app_info (GDesktopAppInfo *app_info)
{
  GSpawnFlags flags;
  GError *error;
  gboolean ret;

  flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
  error = NULL;

  ret = g_desktop_app_info_launch_uris_as_manager (app_info, NULL, NULL,
                                                   flags, child_setup, app_info,
                                                   pid_cb, NULL,
                                                   &error);

  if (ret == FALSE)
    {
      const gchar *display_name;
      GtkWidget *dialog;

      display_name = g_app_info_get_display_name (G_APP_INFO (app_info));
      dialog = gtk_message_dialog_new (NULL, 0,
                                       GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                       _("Could not launch '%s'"),
                                       display_name);

      if (error != NULL)
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", error->message);

      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_window_present (GTK_WINDOW (dialog));
    }

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

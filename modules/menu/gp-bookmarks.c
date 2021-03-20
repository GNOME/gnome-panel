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

#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "gp-bookmarks.h"
#include "gp-menu-utils.h"

#define MAX_BOOKMARKS 100

struct _GpBookmarks
{
  GObject       parent;

  GFileMonitor *monitor;
  gulong        changed_id;

  GSList       *bookmarks;
};

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint bookmarks_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpBookmarks, gp_bookmarks, G_TYPE_OBJECT)

static void
gp_bookmark_free (gpointer data)
{
  GpBookmark *bookmark;

  bookmark = (GpBookmark *) data;

  g_clear_object (&bookmark->file);
  g_clear_object (&bookmark->icon);
  g_clear_pointer (&bookmark->label, g_free);
  g_clear_pointer (&bookmark->tooltip, g_free);

  g_free (bookmark);
}

static GpBookmark *
gp_bookmark_new (const gchar *uri,
                 const gchar *label)
{
  GFile *file;
  GpBookmark *bookmark;
  gchar *bookmark_label;
  gchar *display_name;

  file = g_file_new_for_uri (uri);

  if (g_file_is_native (file) && !g_file_query_exists (file, NULL))
    {
      g_object_unref (file);
      return NULL;
    }

  bookmark_label = label ? g_strstrip (g_strdup (label)) : NULL;
  if (!bookmark_label || *bookmark_label == '\0')
    {
      g_free (bookmark_label);
      bookmark_label = gp_menu_utils_get_label_for_file (file);
    }

  if (bookmark_label == NULL)
    {
      g_object_unref (file);
      return NULL;
    }

  bookmark = g_new0 (GpBookmark, 1);

  bookmark->file = file;
  bookmark->icon = gp_menu_utils_get_icon_for_file (file);
  bookmark->label = bookmark_label;

  display_name = g_file_get_parse_name (file);
  bookmark->tooltip = g_strdup_printf (_("Open '%s'"), display_name);
  g_free (display_name);

  return bookmark;
}

static void
read_bookmarks (GpBookmarks *bookmarks,
                GFile       *file)
{
  gchar *contents;
  gchar **lines;
  guint count;
  guint i;

  if (bookmarks->bookmarks != NULL)
    {
      g_slist_free_full (bookmarks->bookmarks, gp_bookmark_free);
      bookmarks->bookmarks = NULL;
    }

  contents = NULL;

  if (!g_file_load_contents (file, NULL, &contents, NULL, NULL, NULL))
    return;

  lines = g_strsplit (contents, "\n", -1);
  count = 0;

  for (i = 0; lines[i] != NULL; i++)
    {
      GpBookmark *bookmark;
      gchar **line;

      if (*lines[i] == '\0')
        continue;

      if (!g_utf8_validate (lines[i], -1, NULL))
        continue;

      line = g_strsplit (lines[i], " ", 2);
      bookmark = gp_bookmark_new (line[0], line[1]);
      g_strfreev (line);

      if (bookmark == NULL)
        continue;

      bookmarks->bookmarks = g_slist_prepend (bookmarks->bookmarks, bookmark);

      /* We use a hard limit to avoid having users shooting their
       * own feet, and to avoid crashing the system if a misbehaving
       * application creates a big bookmars file.
       */
      if (++count > MAX_BOOKMARKS)
        break;
    }

  bookmarks->bookmarks = g_slist_reverse (bookmarks->bookmarks);

  g_strfreev (lines);
  g_free (contents);
}

static void
changed_cb (GFileMonitor      *monitor,
            GFile             *file,
            GFile             *other_file,
            GFileMonitorEvent  event_type,
            GpBookmarks       *bookmarks)
{
  gboolean reload;

  reload = FALSE;

  switch (event_type)
    {
      case G_FILE_MONITOR_EVENT_CHANGED:
      case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
      case G_FILE_MONITOR_EVENT_DELETED:
      case G_FILE_MONITOR_EVENT_CREATED:
        reload = TRUE;
        break;

      case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
      case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
      case G_FILE_MONITOR_EVENT_UNMOUNTED:
      case G_FILE_MONITOR_EVENT_MOVED:
      case G_FILE_MONITOR_EVENT_RENAMED:
      case G_FILE_MONITOR_EVENT_MOVED_IN:
      case G_FILE_MONITOR_EVENT_MOVED_OUT:
      default:
        break;
    }

  if (!reload)
    return;

  read_bookmarks (bookmarks, file);

  g_signal_emit (bookmarks, bookmarks_signals[CHANGED], 0);
}

static GFile *
get_bookmarks_file (void)
{
  GFile *file;
  gchar *filename;

  filename = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "bookmarks", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  return file;
}

static void
gp_bookmarks_constructed (GObject *object)
{
  GpBookmarks *bookmarks;
  GFile *file;
  GError *error;

  bookmarks = GP_BOOKMARKS (object);

  G_OBJECT_CLASS (gp_bookmarks_parent_class)->constructed (object);

  file = get_bookmarks_file ();
  read_bookmarks (bookmarks, file);

  error = NULL;
  bookmarks->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE,
                                            NULL, &error);

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
  else
    {
      bookmarks->changed_id = g_signal_connect (bookmarks->monitor, "changed",
                                                G_CALLBACK (changed_cb), bookmarks);
    }

  g_object_unref (file);
}

static void
gp_bookmarks_dispose (GObject *object)
{
  GpBookmarks *bookmarks;

  bookmarks = GP_BOOKMARKS (object);

  if (bookmarks->changed_id != 0)
    {
      g_signal_handler_disconnect (bookmarks->monitor, bookmarks->changed_id);
      bookmarks->changed_id = 0;
    }

  if (bookmarks->monitor != NULL)
    {
      g_file_monitor_cancel (bookmarks->monitor);
      g_object_unref (bookmarks->monitor);
    }

  if (bookmarks->bookmarks != NULL)
    {
      g_slist_free_full (bookmarks->bookmarks, gp_bookmark_free);
      bookmarks->bookmarks = NULL;
    }

  G_OBJECT_CLASS (gp_bookmarks_parent_class)->dispose (object);
}

static void
gp_bookmarks_class_init (GpBookmarksClass *bookmarks_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (bookmarks_class);

  object_class->constructed = gp_bookmarks_constructed;
  object_class->dispose = gp_bookmarks_dispose;

  bookmarks_signals[CHANGED] =
    g_signal_new ("changed", GP_TYPE_BOOKMARKS, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gp_bookmarks_init (GpBookmarks *bookmarks)
{
}

GpBookmarks *
gp_bookmarks_new (void)
{
  return g_object_new (GP_TYPE_BOOKMARKS, NULL);
}

guint
gp_bookmarks_get_count (GpBookmarks *bookmarks)
{
  return g_slist_length (bookmarks->bookmarks);
}

void
gp_bookmarks_foreach (GpBookmarks            *bookmarks,
                      GpBookmarksForeachFunc  foreach_func,
                      gpointer                user_data)
{
  GSList *l;

  for (l = bookmarks->bookmarks; l != NULL; l = l->next)
    foreach_func (bookmarks, l->data, user_data);
}

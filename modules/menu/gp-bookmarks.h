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

#ifndef GP_BOOKMARKS_H
#define GP_BOOKMARKS_H

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct
{
  GFile *file;

  GIcon *icon;
  gchar *label;

  gchar *tooltip;
} GpBookmark;

#define GP_TYPE_BOOKMARKS (gp_bookmarks_get_type ())
G_DECLARE_FINAL_TYPE (GpBookmarks, gp_bookmarks, GP, BOOKMARKS, GObject)

typedef void (* GpBookmarksForeachFunc) (GpBookmarks *bookmarks,
                                         GpBookmark  *bookmark,
                                         gpointer     user_data);

GpBookmarks *gp_bookmarks_new       (void);

guint        gp_bookmarks_get_count (GpBookmarks            *bookmarks);

void         gp_bookmarks_foreach   (GpBookmarks            *bookmarks,
                                     GpBookmarksForeachFunc  foreach_func,
                                     gpointer                user_data);

G_END_DECLS

#endif

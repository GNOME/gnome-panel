/*
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
 */

#ifndef GP_LAUNCHER_UTILS_H
#define GP_LAUNCHER_UTILS_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean  gp_launcher_read_from_key_file  (GKeyFile    *key_file,
                                           char       **icon,
                                           char       **type,
                                           char       **name,
                                           char       **command,
                                           char       **comment,
                                           char       **error);

gboolean  gp_launcher_validate            (const char  *icon,
                                           const char  *type,
                                           const char  *name,
                                           const char  *command,
                                           const char  *comment,
                                           char       **error);

gboolean  gp_launcher_validate_key_file   (GKeyFile    *key_file,
                                           char       **error);

char     *gp_launcher_get_launchers_dir   (void);

char     *gp_launcher_get_unique_filename (void);

void      gp_launcher_show_error_message  (GtkWindow   *parent,
                                           const char  *primary_text,
                                           const char  *secondary_text);

G_END_DECLS

#endif

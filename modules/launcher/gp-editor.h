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

#ifndef GP_EDITOR_H
#define GP_EDITOR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  GP_EDITOR_TYPE_NONE,
  GP_EDITOR_TYPE_APPLICATION,
  GP_EDITOR_TYPE_TERMINAL_APPLICATION,
  GP_EDITOR_TYPE_DIRECTORY,
  GP_EDITOR_TYPE_FILE,
} GpEditorType;

#define GP_TYPE_EDITOR (gp_editor_get_type ())
G_DECLARE_FINAL_TYPE (GpEditor, gp_editor, GP, EDITOR, GtkBox)

GtkWidget    *gp_editor_new             (gboolean      edit);

const char   *gp_editor_get_icon        (GpEditor     *self);

void          gp_editor_set_icon        (GpEditor     *self,
                                         const char   *icon);

GpEditorType  gp_editor_get_editor_type (GpEditor     *self);

void          gp_editor_set_editor_type (GpEditor     *self,
                                         GpEditorType  type);

const char   *gp_editor_get_name        (GpEditor     *self);

void          gp_editor_set_name        (GpEditor     *self,
                                         const char   *name);

const char   *gp_editor_get_command     (GpEditor     *self);

void          gp_editor_set_command     (GpEditor     *self,
                                         const char   *command);

const char   *gp_editor_get_comment     (GpEditor     *self);

void          gp_editor_set_comment     (GpEditor     *self,
                                         const char   *comment);

G_END_DECLS

#endif

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

#include "config.h"
#include "gp-custom-launcher-applet.h"

#include "gp-editor.h"
#include "gp-launcher-utils.h"

typedef struct
{
  GpInitialSetupDialog *dialog;
  GpEditor             *editor;
} LauncherData;

struct _GpCustomLauncherApplet
{
  GpLauncherApplet parent;
};

G_DEFINE_TYPE (GpCustomLauncherApplet,
               gp_custom_launcher_applet,
               GP_TYPE_LAUNCHER_APPLET)

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

  g_free (data);
}

static void
check_required_info (LauncherData *data)
{
  gboolean done;
  GpEditorType type;
  const char *type_string;

  type = gp_editor_get_editor_type (data->editor);
  type_string = NULL;

  if (type == GP_EDITOR_TYPE_APPLICATION ||
      type == GP_EDITOR_TYPE_TERMINAL_APPLICATION)
    {
      type_string = G_KEY_FILE_DESKTOP_TYPE_APPLICATION;
    }
  else if (type == GP_EDITOR_TYPE_DIRECTORY ||
           type == GP_EDITOR_TYPE_FILE)
    {
      type_string = G_KEY_FILE_DESKTOP_TYPE_LINK;
    }

  done = gp_launcher_validate (gp_editor_get_name (data->editor),
                               type_string,
                               gp_editor_get_name (data->editor),
                               gp_editor_get_command (data->editor),
                               gp_editor_get_comment (data->editor),
                               NULL);

  gp_initital_setup_dialog_set_done (data->dialog, done);
}

static void
icon_changed_cb (GpEditor     *editor,
                 LauncherData *data)
{
  const char *icon;
  GVariant *variant;

  icon = gp_editor_get_icon (editor);
  variant = icon != NULL ? g_variant_new_string (icon) : NULL;
  gp_initital_setup_dialog_set_setting (data->dialog, "icon", variant);

  check_required_info (data);
}

static void
type_changed_cb (GpEditor     *editor,
                 LauncherData *data)
{
  GpEditorType type;
  const char *type_string;
  GVariant *variant;

  type = gp_editor_get_editor_type (editor);
  type_string = NULL;

  if (type == GP_EDITOR_TYPE_APPLICATION ||
      type == GP_EDITOR_TYPE_TERMINAL_APPLICATION)
    {
      type_string = G_KEY_FILE_DESKTOP_TYPE_APPLICATION;
    }
  else if (type == GP_EDITOR_TYPE_DIRECTORY ||
           type == GP_EDITOR_TYPE_FILE)
    {
      type_string = G_KEY_FILE_DESKTOP_TYPE_LINK;
    }

  variant = type_string != NULL ? g_variant_new_string (type_string) : NULL;
  gp_initital_setup_dialog_set_setting (data->dialog, "type", variant);

  if (type == GP_EDITOR_TYPE_TERMINAL_APPLICATION)
    {
      variant = g_variant_new_boolean (TRUE);
      gp_initital_setup_dialog_set_setting (data->dialog, "terminal", variant);
    }
  else
    {
      gp_initital_setup_dialog_set_setting (data->dialog, "terminal", NULL);
    }

  check_required_info (data);
}

static void
name_changed_cb (GpEditor     *editor,
                 LauncherData *data)
{
  const char *name;
  GVariant *variant;

  name = gp_editor_get_name (editor);
  variant = name != NULL ? g_variant_new_string (name) : NULL;
  gp_initital_setup_dialog_set_setting (data->dialog, "name", variant);

  check_required_info (data);
}

static void
command_changed_cb (GpEditor     *editor,
                    LauncherData *data)
{
  const char *command;
  GVariant *variant;

  command = gp_editor_get_command (editor);
  variant = command != NULL ? g_variant_new_string (command) : NULL;
  gp_initital_setup_dialog_set_setting (data->dialog, "command", variant);

  check_required_info (data);
}

static void
comment_changed_cb (GpEditor     *editor,
                    LauncherData *data)
{
  const char *comment;
  GVariant *variant;

  comment = gp_editor_get_comment (editor);
  variant = comment != NULL ? g_variant_new_string (comment) : NULL;
  gp_initital_setup_dialog_set_setting (data->dialog, "comment", variant);

  check_required_info (data);
}

static const char *
gp_custom_launcher_applet_get_menu_resource (void)
{
  return GRESOURCE_PREFIX "/custom-launcher-menu.ui";
}

static void
gp_custom_launcher_applet_class_init (GpCustomLauncherAppletClass *self_class)
{
  GpLauncherAppletClass *launcher_class;

  launcher_class = GP_LAUNCHER_APPLET_CLASS (self_class);

  launcher_class->get_menu_resource = gp_custom_launcher_applet_get_menu_resource;
}

static void
gp_custom_launcher_applet_init (GpCustomLauncherApplet *self)
{
}

void
gp_custom_launcher_applet_initial_setup_dialog (GpInitialSetupDialog *dialog)
{
  GtkWidget *editor;
  LauncherData *data;
  GVariant *variant;

  editor = gp_editor_new (FALSE);

  data = launcher_data_new (dialog);
  data->editor = GP_EDITOR (editor);

  g_signal_connect (editor, "icon-changed", G_CALLBACK (icon_changed_cb), data);
  g_signal_connect (editor, "type-changed", G_CALLBACK (type_changed_cb), data);
  g_signal_connect (editor, "name-changed", G_CALLBACK (name_changed_cb), data);
  g_signal_connect (editor, "command-changed", G_CALLBACK (command_changed_cb), data);
  g_signal_connect (editor, "comment-changed", G_CALLBACK (comment_changed_cb), data);

  variant = gp_initital_setup_dialog_get_setting (dialog, "command");
  if (variant != NULL)
    {
      gp_editor_set_command (GP_EDITOR (editor),
                             g_variant_get_string (variant, NULL));
    }

  icon_changed_cb (data->editor, data);
  type_changed_cb (data->editor, data);

  gp_initital_setup_dialog_add_content_widget (dialog, editor, data,
                                               launcher_data_free);
}

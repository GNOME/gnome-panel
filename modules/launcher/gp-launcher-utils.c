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
#include "gp-launcher-utils.h"

#include <glib/gi18n-lib.h>

static void
error_response_cb (GtkWidget *widget,
                   int        response_id,
                   gpointer   user_data)
{
  gtk_widget_destroy (widget);
}

gboolean
gp_launcher_read_from_key_file (GKeyFile  *key_file,
                                char     **icon,
                                char     **type,
                                char     **name,
                                char     **command,
                                char     **comment,
                                GError   **error)
{
  char *start_group;
  char *type_string;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (icon == NULL || *icon == NULL, FALSE);
  g_return_val_if_fail (type == NULL || *type == NULL, FALSE);
  g_return_val_if_fail (name == NULL || *name == NULL, FALSE);
  g_return_val_if_fail (command == NULL || *command == NULL, FALSE);
  g_return_val_if_fail (comment == NULL || *comment == NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  start_group = g_key_file_get_start_group (key_file);
  if (start_group == NULL ||
      g_strcmp0 (start_group, G_KEY_FILE_DESKTOP_GROUP) != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("Launcher does not start with required “%s” group."),
                   G_KEY_FILE_DESKTOP_GROUP);

      g_free (start_group);
      return FALSE;
    }

  g_free (start_group);
  type_string = g_key_file_get_string (key_file,
                                       G_KEY_FILE_DESKTOP_GROUP,
                                       G_KEY_FILE_DESKTOP_KEY_TYPE,
                                       NULL);

  if (type_string == NULL ||
      (g_strcmp0 (type_string, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) != 0 &&
       g_strcmp0 (type_string, G_KEY_FILE_DESKTOP_TYPE_LINK) != 0))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("Launcher has invalid Type key value “%s”."),
                   type_string != NULL ? type_string : "(null)");

      g_free (type_string);
      return FALSE;
    }

  if (icon != NULL)
    {
      *icon = g_key_file_get_locale_string (key_file,
                                            G_KEY_FILE_DESKTOP_GROUP,
                                            G_KEY_FILE_DESKTOP_KEY_ICON,
                                            NULL,
                                            NULL);
    }

  if (type != NULL)
    *type = g_strdup (type_string);

  if (name != NULL)
    {
      *name = g_key_file_get_locale_string (key_file,
                                            G_KEY_FILE_DESKTOP_GROUP,
                                            "X-GNOME-FullName",
                                            NULL,
                                            NULL);

      if (*name == NULL)
        {
          *name = g_key_file_get_locale_string (key_file,
                                                G_KEY_FILE_DESKTOP_GROUP,
                                                G_KEY_FILE_DESKTOP_KEY_NAME,
                                                NULL,
                                                NULL);
        }
    }

  if (command != NULL)
    {
      if (g_strcmp0 (type_string, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) == 0)
        {
          *command = g_key_file_get_string (key_file,
                                            G_KEY_FILE_DESKTOP_GROUP,
                                            G_KEY_FILE_DESKTOP_KEY_EXEC,
                                            NULL);
        }
      else if (g_strcmp0 (type_string, G_KEY_FILE_DESKTOP_TYPE_LINK) == 0)
        {
          *command = g_key_file_get_string (key_file,
                                            G_KEY_FILE_DESKTOP_GROUP,
                                            G_KEY_FILE_DESKTOP_KEY_URL,
                                            NULL);
        }
    }

  if (comment != NULL)
    {
      *comment = g_key_file_get_locale_string (key_file,
                                               G_KEY_FILE_DESKTOP_GROUP,
                                               G_KEY_FILE_DESKTOP_KEY_COMMENT,
                                               NULL,
                                               NULL);
    }

  g_free (type_string);

  return TRUE;
}

gboolean
gp_launcher_validate (const char  *icon,
                      const char  *type,
                      const char  *name,
                      const char  *command,
                      const char  *comment,
                      GError     **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (icon == NULL || *icon == '\0')
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           _("The icon of the launcher is not set."));

      return FALSE;
    }

  if (type == NULL || *type == '\0')
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           _("The type of the launcher is not set."));

      return FALSE;
    }

  if (g_strcmp0 (type, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) != 0 &&
      g_strcmp0 (type, G_KEY_FILE_DESKTOP_TYPE_LINK) != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("The type of the launcher must be “%s” or “%s”."),
                   G_KEY_FILE_DESKTOP_TYPE_APPLICATION,
                   G_KEY_FILE_DESKTOP_TYPE_LINK);

      return FALSE;
    }

  if (name == NULL || *name == '\0')
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           _("The name of the launcher is not set."));

      return FALSE;
    }

  if (command == NULL || *command == '\0')
    {
      if (g_strcmp0 (type, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) == 0)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("The command of the launcher is not set."));
        }
      else if (g_strcmp0 (type, G_KEY_FILE_DESKTOP_TYPE_LINK) == 0)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("The location of the launcher is not set."));
        }

      return FALSE;
    }

  return TRUE;
}

gboolean
gp_launcher_validate_key_file (GKeyFile  *key_file,
                               GError   **error)
{
  char *icon;
  char *type;
  char *name;
  char *command;
  char *comment;
  gboolean valid;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  icon = NULL;
  type = NULL;
  name = NULL;
  command = NULL;
  comment = NULL;

  if (!gp_launcher_read_from_key_file (key_file,
                                       &icon,
                                       &type,
                                       &name,
                                       &command,
                                       &comment,
                                       error))
    return FALSE;

  valid = gp_launcher_validate (icon, type, name, command, comment, error);

  g_free (icon);
  g_free (type);
  g_free (name);
  g_free (command);
  g_free (comment);

  return valid;
}

char *
gp_launcher_get_launchers_dir (void)
{
  char *dir;

  dir = g_build_filename (g_get_user_config_dir (),
                          "gnome-panel",
                          "launchers",
                          NULL);

  if (g_mkdir_with_parents (dir, 0700) == -1)
    g_warning ("Failed to create %s: %s", dir, g_strerror (errno));

  return dir;
}

char *
gp_launcher_get_unique_filename (void)
{
  char *launchers_dir;
  char *filename;

  launchers_dir = gp_launcher_get_launchers_dir ();
  filename = NULL;

  do
    {
      char *uuid;
      char *desktop;

      g_free (filename);

      uuid = g_uuid_string_random ();
      desktop = g_strdup_printf ("%s.desktop", uuid);
      g_free (uuid);

      filename = g_build_filename (launchers_dir, desktop, NULL);
      g_free (desktop);
    }
  while (g_file_test (filename, G_FILE_TEST_EXISTS));

  g_free (launchers_dir);

  return filename;
}

void
gp_launcher_show_error_message (GtkWindow  *parent,
                                const char *primary_text,
                                const char *secondary_text)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s",
                                   primary_text);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%s",
                                            secondary_text);

  g_signal_connect (dialog, "response", G_CALLBACK (error_response_cb), NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

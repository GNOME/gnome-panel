/*
 * Copyright (C) 2008 Novell, Inc.
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
 *     Vincent Untz <vincent@vuntz.net>
 */

#include "config.h"
#include "gp-launcher-properties.h"

#include <glib/gi18n-lib.h>

#include "gp-editor.h"
#include "gp-launcher-utils.h"

enum
{
  GP_RESPONSE_REVERT
};

struct _GpLauncherProperties
{
  GtkDialog  parent;

  GpEditor  *editor;
  GtkWidget *revert_button;

  GSettings *settings;

  GKeyFile  *file;
  GKeyFile  *revert_file;

  gboolean   dirty;

  guint      save_id;
};

enum
{
  PROP_0,

  PROP_SETTINGS,

  LAST_PROP
};

static GParamSpec *properties_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpLauncherProperties, gp_launcher_properties, GTK_TYPE_DIALOG)

static void
show_error_message (GpLauncherProperties *self,
                    const char           *error_message)
{
  gp_launcher_show_error_message (GTK_WINDOW (self),
                                  _("Could not save launcher"),
                                  error_message);
}

static gboolean
get_launcher_filename (GpLauncherProperties  *self,
                       char                 **filename)
{
  char *location;
  char *launchers_dir;

  g_assert (*filename == NULL);

  location = g_settings_get_string (self->settings, "location");
  launchers_dir = gp_launcher_get_launchers_dir ();

  if (g_path_is_absolute (location) &&
      !g_str_has_prefix (location, launchers_dir))
    {
      *filename = gp_launcher_get_unique_filename ();

      g_free (location);
      g_free (launchers_dir);

      return TRUE;
    }
  else if (!g_path_is_absolute (location))
    {
      *filename = g_build_filename (launchers_dir, location, NULL);

      g_free (location);
      g_free (launchers_dir);

      return FALSE;
    }

  *filename = location;

  g_free (launchers_dir);

  return FALSE;
}

static gboolean
launcher_save (GpLauncherProperties *self,
               gboolean              interactive)
{
  gboolean location_changed;
  char *filename;
  GError *error;

  if (self->save_id != 0)
    {
      g_source_remove (self->save_id);
      self->save_id = 0;
    }

  if (!self->dirty)
    return TRUE;

  error = NULL;
  if (!gp_launcher_validate_key_file (self->file, &error))
    {
      if (interactive)
        show_error_message (self, error->message);
      g_error_free (error);

      return FALSE;
    }

  filename = NULL;
  location_changed = get_launcher_filename (self, &filename);

  error = NULL;
  if (!g_key_file_save_to_file (self->file, filename, &error))
    {
      if (interactive)
        show_error_message (self, error->message);

      g_error_free (error);
      g_free (filename);

      return FALSE;
    }

  if (location_changed)
    {
      char *basename;

      basename = g_path_get_basename (filename);
      g_settings_set_string (self->settings, "location", basename);
      g_free (basename);
    }

  g_free (filename);

  self->dirty = FALSE;

  return TRUE;
}

static gboolean
save_cb (gpointer user_data)
{
  GpLauncherProperties *self;

  self = GP_LAUNCHER_PROPERTIES (user_data);
  self->save_id = 0;

  launcher_save (self, FALSE);

  return G_SOURCE_REMOVE;
}

static void
launcher_changed (GpLauncherProperties *self)
{
  self->dirty = TRUE;

  gtk_dialog_set_response_sensitive (GTK_DIALOG (self),
                                     GP_RESPONSE_REVERT,
                                     TRUE);

  if (self->save_id != 0)
    g_source_remove (self->save_id);

  self->save_id = g_timeout_add_seconds (2, save_cb, self);
  g_source_set_name_by_id (self->save_id, "[gnome-panel] save_cb");
}

static void
remove_locale_key (GKeyFile   *key_file,
                   const char *key)
{
  char **keys;
  size_t key_len;
  int i;

  keys = g_key_file_get_keys (key_file, G_KEY_FILE_DESKTOP_GROUP, NULL, NULL);
  if (keys == NULL)
    return;

  key_len = strlen (key);

  for (i = 0; keys[i] != NULL; i++)
    {
      size_t len;

      if (strncmp (keys[i], key, key_len) != 0)
        continue;

      len = strlen (keys[i]);

      if (len == key_len || keys[i][key_len] == '[')
        {
          g_key_file_remove_key (key_file,
                                 G_KEY_FILE_DESKTOP_GROUP,
                                 keys[i],
                                 NULL);
        }
    }

  g_strfreev (keys);
}

static void
icon_changed_cb (GpEditor             *editor,
                 GpLauncherProperties *self)
{
  const char *icon;

  icon = gp_editor_get_icon (editor);

  remove_locale_key (self->file, G_KEY_FILE_DESKTOP_KEY_ICON);

  if (icon != NULL && *icon != '\0')
    {
      g_key_file_set_string (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_ICON,
                             icon);
    }

  launcher_changed (self);
}

static void
type_changed_cb (GpEditor             *editor,
                 GpLauncherProperties *self)
{
  GpEditorType type;
  const char *type_key;
  const char *command;

  type = gp_editor_get_editor_type (editor);
  command = gp_editor_get_command (editor);
  type_key = NULL;

  if (type == GP_EDITOR_TYPE_APPLICATION ||
      type == GP_EDITOR_TYPE_TERMINAL_APPLICATION)
    {
      type_key = G_KEY_FILE_DESKTOP_TYPE_APPLICATION;
    }
  else if (type == GP_EDITOR_TYPE_DIRECTORY ||
           type == GP_EDITOR_TYPE_FILE)
    {
      type_key = G_KEY_FILE_DESKTOP_TYPE_LINK;
    }

  g_key_file_set_string (self->file,
                         G_KEY_FILE_DESKTOP_GROUP,
                         G_KEY_FILE_DESKTOP_KEY_TYPE,
                         type_key);

  if (type == GP_EDITOR_TYPE_APPLICATION ||
      type == GP_EDITOR_TYPE_TERMINAL_APPLICATION)
    {
      g_key_file_remove_key (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_URL,
                             NULL);

      g_key_file_set_string (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_EXEC,
                             command);

      if (type == GP_EDITOR_TYPE_TERMINAL_APPLICATION)
        {
          g_key_file_set_boolean (self->file,
                                 G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                                 TRUE);
        }
      else
        {
          g_key_file_remove_key (self->file,
                                 G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                                 NULL);
        }
    }
  else if (type == GP_EDITOR_TYPE_DIRECTORY ||
           type == GP_EDITOR_TYPE_FILE)
    {
      g_key_file_remove_key (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                             NULL);

      g_key_file_remove_key (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_EXEC,
                             NULL);

      g_key_file_set_string (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_URL,
                             command);
    }
  else
    {
      g_assert_not_reached ();
    }

  launcher_changed (self);
}

static void
name_changed_cb (GpEditor             *editor,
                 GpLauncherProperties *self)
{
  const char *name;

  name = gp_editor_get_name (editor);

  remove_locale_key (self->file, "X-GNOME-FullName");
  remove_locale_key (self->file, G_KEY_FILE_DESKTOP_KEY_NAME);

  if (name != NULL && *name != '\0')
    {
      g_key_file_set_string (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_NAME,
                             name);
    }

  launcher_changed (self);
}

static void
command_changed_cb (GpEditor             *editor,
                    GpLauncherProperties *self)
{
  const char *command;
  GpEditorType type;

  command = gp_editor_get_command (editor);
  type = gp_editor_get_editor_type (editor);

  if (type == GP_EDITOR_TYPE_APPLICATION ||
      type == GP_EDITOR_TYPE_TERMINAL_APPLICATION)
    {
      g_key_file_set_string (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_EXEC,
                             command);
    }
  else if (type == GP_EDITOR_TYPE_DIRECTORY ||
           type == GP_EDITOR_TYPE_FILE)
    {
      g_key_file_set_string (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_URL,
                             command);
    }
  else
    {
      g_assert_not_reached ();
    }

  launcher_changed (self);
}

static void
comment_changed_cb (GpEditor             *editor,
                    GpLauncherProperties *self)
{
  const char *comment;

  comment = gp_editor_get_comment (editor);

  remove_locale_key (self->file, G_KEY_FILE_DESKTOP_KEY_COMMENT);

  if (comment != NULL && *comment != '\0')
    {
      g_key_file_set_string (self->file,
                             G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_COMMENT,
                             comment);
    }

  launcher_changed (self);
}

static void
fill_editor_from_file (GpLauncherProperties *self,
                       GKeyFile             *key_file)
{
  char *icon;
  char *type_string;
  char *name;
  char *command;
  char *comment;
  gboolean terminal;
  GpEditorType type;

  icon = NULL;
  type_string = NULL;
  name = NULL;
  command = NULL;
  comment = NULL;

  if (!gp_launcher_read_from_key_file (key_file,
                                       &icon,
                                       &type_string,
                                       &name,
                                       &command,
                                       &comment,
                                       NULL))
    return;

  terminal = g_key_file_get_boolean (key_file,
                                     G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                                     NULL);

  if (g_strcmp0 (type_string, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) == 0)
    {
      if (terminal)
        type = GP_EDITOR_TYPE_TERMINAL_APPLICATION;
      else
        type = GP_EDITOR_TYPE_APPLICATION;
    }
  else if (g_strcmp0 (type_string, G_KEY_FILE_DESKTOP_TYPE_LINK) == 0)
    {
      GFile *file;
      char *path;

      file = g_file_new_for_uri (command);
      path = g_file_get_path (file);
      g_object_unref (file);

      if (file != NULL && g_file_test (path, G_FILE_TEST_IS_DIR))
        type = GP_EDITOR_TYPE_DIRECTORY;
      else
        type = GP_EDITOR_TYPE_FILE;

      g_free (path);
    }
  else
    {
      type = GP_EDITOR_TYPE_NONE;
    }

  gp_editor_set_icon (self->editor, icon);
  gp_editor_set_editor_type (self->editor, type);
  gp_editor_set_name (self->editor, name);
  gp_editor_set_command (self->editor, command);
  gp_editor_set_comment (self->editor, comment);

  g_free (icon);
  g_free (type_string);
  g_free (name);
  g_free (command);
  g_free (comment);
}

static void
response_cb (GtkWidget            *widget,
             int                   response_id,
             GpLauncherProperties *self)
{
  if (response_id == GTK_RESPONSE_CLOSE)
    {
      if (launcher_save (self, TRUE))
        gtk_widget_destroy (widget);
    }
  else if (response_id == GP_RESPONSE_REVERT)
    {
      fill_editor_from_file (self, self->revert_file);
      gtk_dialog_set_response_sensitive (GTK_DIALOG (self),
                                         GP_RESPONSE_REVERT,
                                         FALSE);
    }
  else if (response_id == GTK_RESPONSE_DELETE_EVENT)
    {
      fill_editor_from_file (self, self->revert_file);
      launcher_save (self, FALSE);
    }
}

static void
gp_launcher_properties_constructed (GObject *object)
{
  GpLauncherProperties *self;
  char *location;
  GError *error;
  GKeyFileFlags flags;

  self = GP_LAUNCHER_PROPERTIES (object);

  G_OBJECT_CLASS (gp_launcher_properties_parent_class)->constructed (object);

  location = g_settings_get_string (self->settings, "location");

  if (!g_path_is_absolute (location))
    {
      char *launchers_dir;
      char *filename;

      launchers_dir = gp_launcher_get_launchers_dir ();

      filename = g_build_filename (launchers_dir, location, NULL);
      g_free (launchers_dir);

      g_free (location);
      location = filename;
    }

  flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

  self->file = g_key_file_new ();
  self->revert_file = g_key_file_new ();

  error = NULL;
  g_key_file_load_from_file (self->file, location, flags, &error);

  if (error != NULL)
    {
      g_warning ("Failed to load key file “%s”: %s", location, error->message);
      g_error_free (error);
      g_free (location);

      return;
    }

  g_key_file_load_from_file (self->revert_file, location, flags, NULL);
  g_free (location);

  fill_editor_from_file (self, self->file);

  g_signal_connect (self->editor,
                    "icon-changed",
                    G_CALLBACK (icon_changed_cb),
                    self);

  g_signal_connect (self->editor,
                    "type-changed",
                    G_CALLBACK (type_changed_cb),
                    self);

  g_signal_connect (self->editor,
                    "name-changed",
                    G_CALLBACK (name_changed_cb),
                    self);

  g_signal_connect (self->editor,
                    "command-changed",
                    G_CALLBACK (command_changed_cb),
                    self);

  g_signal_connect (self->editor,
                    "comment-changed",
                    G_CALLBACK (comment_changed_cb),
                    self);
}

static void
gp_launcher_properties_dispose (GObject *object)
{
  GpLauncherProperties *self;

  self = GP_LAUNCHER_PROPERTIES (object);

  if (self->save_id != 0)
    {
      g_source_remove (self->save_id);
      self->save_id = 0;
    }

  g_clear_object (&self->settings);

  g_clear_pointer (&self->file, g_key_file_unref);
  g_clear_pointer (&self->revert_file, g_key_file_unref);

  G_OBJECT_CLASS (gp_launcher_properties_parent_class)->dispose (object);
}

static void
gp_launcher_properties_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GpLauncherProperties *self;

  self = GP_LAUNCHER_PROPERTIES (object);

  switch (property_id)
    {
      case PROP_SETTINGS:
        g_assert (self->settings == NULL);
        self->settings = g_value_dup_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  properties_properties[PROP_SETTINGS] =
    g_param_spec_object ("settings",
                         "settings",
                         "settings",
                         G_TYPE_SETTINGS,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     properties_properties);
}

static void
gp_launcher_properties_class_init (GpLauncherPropertiesClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = gp_launcher_properties_constructed;
  object_class->dispose = gp_launcher_properties_dispose;
  object_class->set_property = gp_launcher_properties_set_property;

  install_properties (object_class);
}

static void
gp_launcher_properties_init (GpLauncherProperties *self)
{
  GtkWidget *content_area;
  GtkWidget *editor;

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);
  gtk_box_set_spacing (GTK_BOX (content_area), 6);

  editor = gp_editor_new (TRUE);
  self->editor = GP_EDITOR (editor);

  gtk_container_add (GTK_CONTAINER (content_area), editor);
  gtk_widget_show (editor);

  self->revert_button = gtk_dialog_add_button (GTK_DIALOG (self),
                                               _("_Revert"),
                                               GP_RESPONSE_REVERT);

  gtk_dialog_set_response_sensitive (GTK_DIALOG (self),
                                     GP_RESPONSE_REVERT,
                                     FALSE);

  gtk_dialog_add_button (GTK_DIALOG (self),
                         _("_Close"),
                         GTK_RESPONSE_CLOSE);

  gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_CLOSE);

  g_signal_connect (self, "response", G_CALLBACK (response_cb), self);
}

GtkWidget *
gp_launcher_properties_new (GSettings *settings)
{
  return g_object_new (GP_TYPE_LAUNCHER_PROPERTIES,
                       "title", _("Launcher Properties"),
                       "settings", settings,
                       NULL);
}

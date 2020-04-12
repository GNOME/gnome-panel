/*
 * Copyright (C) 2004 - 2006 Vincent Untz
 * Copyright (C) 2010 Novell, Inc.
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
 *     Vincent Untz <vuntz@gnome.org>
 */

#include "config.h"
#include "gp-editor.h"

#include <glib/gi18n-lib.h>

#include "gp-icon-name-chooser.h"

#define FALLBACK_ICON "gnome-panel-launcher"

typedef struct
{
  GpEditorType  type;
  const char   *name;
} GpTypeComboItem;

static GpTypeComboItem type_combo_items[] =
{
  { GP_EDITOR_TYPE_APPLICATION, N_("Application") },
  { GP_EDITOR_TYPE_TERMINAL_APPLICATION, N_("Application in Terminal") },
  { GP_EDITOR_TYPE_DIRECTORY, N_("Directory") },
  { GP_EDITOR_TYPE_FILE, N_("File") },
  { GP_EDITOR_TYPE_NONE, NULL, }
};

struct _GpEditor
{
  GtkBox        parent;

  gboolean      edit;

  GtkIconTheme *icon_theme;

  char         *icon;
  GtkWidget    *icon_button;
  GtkWidget    *icon_image;
  GtkWidget    *icon_chooser;

  GtkTreeModel *type_model;
  GtkWidget    *type_label;
  GtkWidget    *type_combo;

  GtkWidget    *name_label;
  GtkWidget    *name_entry;

  GtkWidget    *command_label;
  GtkWidget    *command_entry;
  GtkWidget    *command_browse;
  GtkWidget    *command_chooser;

  GtkWidget    *comment_label;
  GtkWidget    *comment_entry;
};

enum
{
  PROP_0,

  PROP_EDIT,

  LAST_PROP
};

static GParamSpec *editor_properties[LAST_PROP] = { NULL };

enum
{
  ICON_CHANGED,
  TYPE_CHANGED,
  NAME_CHANGED,
  COMMAND_CHANGED,
  COMMENT_CHANGED,

  LAST_SIGNAL
};

static guint editor_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpEditor, gp_editor, GTK_TYPE_BOX)

static GpEditorType
get_editor_type (GpEditor *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GpEditorType type;

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->type_combo), &iter))
    return GP_EDITOR_TYPE_NONE;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->type_combo));
  gtk_tree_model_get (model, &iter, 1, &type, -1);

  return type;
}

static char *
filename_to_exec_uri (const char *filename)
{
  GString *exec_uri;
  const char *c;

  if (filename == NULL)
    return g_strdup ("");

  if (strchr (filename, ' ') == NULL)
    return g_strdup (filename);

  exec_uri = g_string_new_len (NULL, strlen (filename));

  g_string_append_c (exec_uri, '"');

  for (c = filename; *c != '\0'; c++)
    {
      if (*c == '"')
        g_string_append (exec_uri, "\\\"");
      else
        g_string_append_c (exec_uri, *c);
    }

  g_string_append_c (exec_uri, '"');

  return g_string_free (exec_uri, FALSE);
}

static void
update_icon_image (GpEditor *self)
{
  const char *icon;
  GtkIconSize size;
  int px_size;

  icon = gp_editor_get_icon (self);
  size = GTK_ICON_SIZE_DIALOG;
  px_size = 48;

  if (g_path_is_absolute (self->icon))
    {
      GdkPixbuf *pixbuf;

      pixbuf = gdk_pixbuf_new_from_file_at_size (icon, px_size, px_size, NULL);
      gtk_image_set_from_pixbuf (GTK_IMAGE (self->icon_image), pixbuf);
      g_clear_object (&pixbuf);
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (self->icon_image), icon, size);
      gtk_image_set_pixel_size (GTK_IMAGE (self->icon_image), px_size);
    }
}

static void
icon_name_changed (GpEditor   *self,
                   const char *icon_name)
{
  g_clear_pointer (&self->icon, g_free);
  self->icon = g_strdup (icon_name);

  g_signal_emit (self, editor_signals[ICON_CHANGED], 0);
  update_icon_image (self);
}

static void
icon_chooser_destroy_cb (GtkWidget *widget,
                         GpEditor  *self)
{
  self->icon_chooser = NULL;
}

static void
icon_selected_cb (GpIconNameChooser *chooser,
                  const char        *icon_name,
                  GpEditor          *self)
{
  icon_name_changed (self, icon_name);
}

static void
choose_icon_name_activate_cb (GtkMenuItem *item,
                              GpEditor    *self)
{
  if (self->icon_chooser != NULL &&
      GP_IS_ICON_NAME_CHOOSER (self->icon_chooser))
    {
      gtk_window_present (GTK_WINDOW (self->icon_chooser));
      return;
    }

  g_clear_pointer (&self->icon_chooser, gtk_widget_destroy);

  self->icon_chooser = gp_icon_name_chooser_new ();

  g_signal_connect (self->icon_chooser,
                    "icon-selected",
                    G_CALLBACK (icon_selected_cb),
                    self);

  g_signal_connect (self->icon_chooser,
                    "destroy",
                    G_CALLBACK (icon_chooser_destroy_cb),
                    self);

  gtk_window_set_destroy_with_parent (GTK_WINDOW (self->icon_chooser), TRUE);
  gtk_window_present (GTK_WINDOW (self->icon_chooser));

  if (self->icon != NULL && !g_path_is_absolute (self->icon))
    gp_icon_name_chooser_set_icon_name (GP_ICON_NAME_CHOOSER (self->icon_chooser),
                                        self->icon);
}

static void
icon_chooser_update_preview_cb (GtkFileChooser *chooser,
                                GtkImage       *preview)
{
  gchar *filename;
  GdkPixbuf *pixbuf;

  filename = gtk_file_chooser_get_preview_filename (chooser);
  if (!filename)
    return;

  pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 128, 128, NULL);
  g_free (filename);

  gtk_file_chooser_set_preview_widget_active (chooser, !!pixbuf);
  gtk_image_set_from_pixbuf (preview, pixbuf);
  g_clear_object (&pixbuf);
}

static void
icon_chooser_response_cb (GtkFileChooser *chooser,
                          gint            response_id,
                          GpEditor       *self)
{
  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      g_clear_pointer (&self->icon, g_free);
      self->icon = gtk_file_chooser_get_filename (chooser);

      g_signal_emit (self, editor_signals[ICON_CHANGED], 0);
      update_icon_image (self);
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
choose_icon_file_activate_cb (GtkMenuItem *item,
                              GpEditor    *self)
{
  GtkWidget *toplevel;
  GtkFileChooser *chooser;
  GtkFileFilter *filter;
  GtkWidget *preview;

  if (self->icon_chooser != NULL &&
      GTK_IS_FILE_CHOOSER_DIALOG (self->icon_chooser))
    {
      gtk_window_present (GTK_WINDOW (self->icon_chooser));
      return;
    }

  g_clear_pointer (&self->icon_chooser, gtk_widget_destroy);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  self->icon_chooser = gtk_file_chooser_dialog_new (_("Choose Icon File"),
                                                    GTK_WINDOW (toplevel),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    _("_Cancel"),
                                                    GTK_RESPONSE_CANCEL,
                                                    _("_Open"),
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);

  chooser = GTK_FILE_CHOOSER (self->icon_chooser);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_chooser_set_filter (chooser, filter);

  preview = gtk_image_new ();
  gtk_file_chooser_set_preview_widget (chooser, preview);

  if (self->icon != NULL && g_path_is_absolute (self->icon))
    gtk_file_chooser_set_filename (chooser, self->icon);

  g_signal_connect (chooser,
                    "response",
                    G_CALLBACK (icon_chooser_response_cb),
                    self);

  g_signal_connect (chooser,
                    "update-preview",
                    G_CALLBACK (icon_chooser_update_preview_cb),
                    preview);

  g_signal_connect (chooser,
                    "destroy",
                    G_CALLBACK (icon_chooser_destroy_cb),
                    self);

  gtk_window_set_destroy_with_parent (GTK_WINDOW (chooser), TRUE);
  gtk_window_present (GTK_WINDOW (chooser));
}

static GtkWidget *
create_icon_button (GpEditor *self)
{
  GtkWidget *button;
  GtkWidget *menu;
  GtkWidget *item;

  button = gtk_menu_button_new ();

  self->icon_image = gtk_image_new_from_icon_name (FALLBACK_ICON,
                                                   GTK_ICON_SIZE_DIALOG);

  gtk_image_set_pixel_size (GTK_IMAGE (self->icon_image), 48);
  gtk_container_add (GTK_CONTAINER (button), self->icon_image);
  gtk_widget_show (self->icon_image);

  menu = gtk_menu_new ();
  gtk_menu_button_set_popup (GTK_MENU_BUTTON (button), menu);

  item = gtk_menu_item_new_with_label (_("Choose Icon Name"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  g_signal_connect (item,
                    "activate",
                    G_CALLBACK (choose_icon_name_activate_cb),
                    self);

  item = gtk_menu_item_new_with_label (_("Choose Icon File"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  g_signal_connect (item,
                    "activate",
                    G_CALLBACK (choose_icon_file_activate_cb),
                    self);

  return button;
}

static void
command_chooser_destroy_cb (GtkWidget *widget,
                            GpEditor  *self)
{
  self->command_chooser = NULL;
}

static void
command_chooser_response_cb (GtkFileChooser *chooser,
                             int             response_id,
                             GpEditor       *self)
{
  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GpEditorType type;
      char *text;
      char *uri;

      type = get_editor_type (self);
      uri = NULL;

      switch (type)
        {
          case GP_EDITOR_TYPE_APPLICATION:
          case GP_EDITOR_TYPE_TERMINAL_APPLICATION:
            text = gtk_file_chooser_get_filename (chooser);
            uri = filename_to_exec_uri (text);
            g_free (text);
            break;

          case GP_EDITOR_TYPE_DIRECTORY:
          case GP_EDITOR_TYPE_FILE:
            uri = gtk_file_chooser_get_uri (chooser);
            break;

          case GP_EDITOR_TYPE_NONE:
          default:
            break;
        }

      gtk_entry_set_text (GTK_ENTRY (self->command_entry), uri);
      g_free (uri);
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
command_browse_clicked_cb (GtkButton *button,
                           GpEditor  *self)
{
  GtkWidget *toplevel;
  GtkWindow *parent;
  GpEditorType type;
  GtkFileChooserAction action;
  const char *title;
  gboolean local_only;
  GtkFileChooser *chooser;

  if (self->command_chooser != NULL)
    {
      gtk_window_present (GTK_WINDOW (self->command_chooser));
      return;
    }

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  parent = GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL;

  type = get_editor_type (self);
  action = GTK_FILE_CHOOSER_ACTION_OPEN;
  title = NULL;
  local_only = TRUE;

  switch (type)
    {
      case GP_EDITOR_TYPE_APPLICATION:
      case GP_EDITOR_TYPE_TERMINAL_APPLICATION:
        action = GTK_FILE_CHOOSER_ACTION_OPEN;
        title = _("Choose an application...");
        break;

      case GP_EDITOR_TYPE_DIRECTORY:
        action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        title = _("Choose a directory...");
        break;

      case GP_EDITOR_TYPE_FILE:
        action = GTK_FILE_CHOOSER_ACTION_OPEN;
        title = _("Choose a file...");
        local_only = FALSE;
        break;

      case GP_EDITOR_TYPE_NONE:
      default:
        break;
    }

  self->command_chooser = gtk_file_chooser_dialog_new (title, parent, action,
                                                       _("_Cancel"),
                                                       GTK_RESPONSE_CANCEL,
                                                       _("_Open"),
                                                       GTK_RESPONSE_ACCEPT,
                                                       NULL);

  chooser = GTK_FILE_CHOOSER (self->command_chooser);
  gtk_file_chooser_set_local_only (chooser, local_only);

  g_signal_connect (chooser, "response",
                    G_CALLBACK (command_chooser_response_cb),
                    self);

  g_signal_connect (chooser, "destroy",
                    G_CALLBACK (command_chooser_destroy_cb),
                    self);

  gtk_window_set_destroy_with_parent (GTK_WINDOW (chooser), TRUE);
  gtk_window_present (GTK_WINDOW (chooser));
}

static void
name_changed_cb (GtkEditable *editable,
                 GpEditor    *self)
{
  g_signal_emit (self, editor_signals[NAME_CHANGED], 0);
}

static void
command_changed_cb (GtkEditable *editable,
                    GpEditor    *self)
{
  GpEditorType type;

  type = get_editor_type (self);

  if (type == GP_EDITOR_TYPE_APPLICATION ||
      type == GP_EDITOR_TYPE_TERMINAL_APPLICATION)
    {
      const char *exec;
      char *icon_name;

      exec = gp_editor_get_command (self);
      icon_name = g_path_get_basename (exec);

      if (gtk_icon_theme_has_icon (self->icon_theme, icon_name) &&
          g_strcmp0 (icon_name, self->icon) != 0)
        icon_name_changed (self, icon_name);

      g_free (icon_name);
    }

  g_signal_emit (self, editor_signals[COMMAND_CHANGED], 0);
}

static void
comment_changed_cb (GtkEditable *editable,
                    GpEditor    *self)
{
  g_signal_emit (self, editor_signals[COMMENT_CHANGED], 0);
}

static void
type_combo_changed_cb (GtkComboBox *combo,
                       GpEditor    *self)
{
  GpEditorType type;
  const char *text;
  const char *title;
  GtkFileChooserAction action;
  gboolean local_only;
  char *bold;

  type = get_editor_type (self);
  text = NULL;
  title = NULL;
  action = GTK_FILE_CHOOSER_ACTION_OPEN;
  local_only = TRUE;

  switch (type)
    {
      case GP_EDITOR_TYPE_APPLICATION:
      case GP_EDITOR_TYPE_TERMINAL_APPLICATION:
        text = _("Comm_and:");
        title = _("Choose an application...");
        action = GTK_FILE_CHOOSER_ACTION_OPEN;
        break;

      case GP_EDITOR_TYPE_DIRECTORY:
        text = _("_Location:");
        title = _("Choose a directory...");
        action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        break;

      case GP_EDITOR_TYPE_FILE:
        text = _("_Location:");
        title = _("Choose a file...");
        action = GTK_FILE_CHOOSER_ACTION_OPEN;
        local_only = FALSE;
        break;

      case GP_EDITOR_TYPE_NONE:
      default:
        break;
    }

  bold = g_strdup_printf ("<b>%s</b>", text);
  gtk_label_set_markup_with_mnemonic (GTK_LABEL (self->command_label), bold);
  g_free (bold);

  if (self->command_chooser != NULL)
    {
      GtkFileChooser *chooser;

      chooser = GTK_FILE_CHOOSER (self->command_chooser);

      gtk_file_chooser_set_action (chooser, action);
      gtk_file_chooser_set_local_only (chooser, local_only);
      gtk_window_set_title (GTK_WINDOW (chooser), title);
    }

  g_signal_emit (self, editor_signals[TYPE_CHANGED], 0);
}

static gboolean
type_visible_func (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   gpointer      user_data)
{
  GpEditor *self;
  gboolean visible;
  GpEditorType active_type;
  GpEditorType type;

  self = GP_EDITOR (user_data);

  if (!self->edit)
    return TRUE;

  visible = FALSE;
  active_type = get_editor_type (self);

  gtk_tree_model_get (model, iter, 1, &type, -1);

  if (active_type == GP_EDITOR_TYPE_APPLICATION ||
      active_type == GP_EDITOR_TYPE_TERMINAL_APPLICATION)
    {
      visible = type == GP_EDITOR_TYPE_APPLICATION ||
                type == GP_EDITOR_TYPE_TERMINAL_APPLICATION;
    }
  else if (active_type == GP_EDITOR_TYPE_DIRECTORY)
    {
      visible = type == GP_EDITOR_TYPE_DIRECTORY;
    }
  else if (active_type == GP_EDITOR_TYPE_FILE)
    {
      visible = type == GP_EDITOR_TYPE_FILE;
    }

  return visible;
}

static void
setup_type_combo (GpEditor *self)
{
  GtkListStore *store;
  GtkCellLayout *layout;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  guint i;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
  self->type_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (self->type_model),
                                          type_visible_func,
                                          self,
                                          NULL);

  layout = GTK_CELL_LAYOUT (self->type_combo);
  renderer = gtk_cell_renderer_text_new ();

  gtk_cell_layout_pack_start (layout, renderer, TRUE);
  gtk_cell_layout_set_attributes (layout, renderer, "text", 0, NULL);

  for (i = 0; type_combo_items[i].type != GP_EDITOR_TYPE_NONE; i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          0, _(type_combo_items[i].name),
                          1, type_combo_items[i].type,
                          -1);
    }

  g_signal_connect (self->type_combo, "changed",
                    G_CALLBACK (type_combo_changed_cb), self);

  gtk_combo_box_set_model (GTK_COMBO_BOX (self->type_combo), self->type_model);
  gtk_combo_box_set_active (GTK_COMBO_BOX (self->type_combo), 0);
  g_object_unref (store);
}

static GtkWidget *
label_new_with_mnemonic (const gchar *text)
{
  GtkWidget *label;
  char *bold;

  bold = g_strdup_printf ("<b>%s</b>", text);
  label = gtk_label_new_with_mnemonic (bold);
  g_free (bold);

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  gtk_widget_show (label);

  return label;
}

static void
gp_editor_dispose (GObject *object)
{
  GpEditor *self;

  self = GP_EDITOR (object);

  g_clear_object (&self->icon_theme);

  g_clear_object (&self->type_model);

  g_clear_pointer (&self->icon_chooser, gtk_widget_destroy);
  g_clear_pointer (&self->command_chooser, gtk_widget_destroy);

  G_OBJECT_CLASS (gp_editor_parent_class)->dispose (object);
}

static void
gp_editor_finalize (GObject *object)
{
  GpEditor *self;

  self = GP_EDITOR (object);

  g_clear_pointer (&self->icon, g_free);

  G_OBJECT_CLASS (gp_editor_parent_class)->finalize (object);
}

static void
gp_editor_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GpEditor *self;

  self = GP_EDITOR (object);

  switch (property_id)
    {
      case PROP_EDIT:
        self->edit = g_value_get_boolean (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  editor_properties[PROP_EDIT] =
    g_param_spec_boolean ("edit",
                          "edit",
                          "edit",
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, editor_properties);
}

static void
install_signals (void)
{
  editor_signals[ICON_CHANGED] =
    g_signal_new ("icon-changed", GP_TYPE_EDITOR, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  editor_signals[TYPE_CHANGED] =
    g_signal_new ("type-changed", GP_TYPE_EDITOR, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  editor_signals[NAME_CHANGED] =
    g_signal_new ("name-changed", GP_TYPE_EDITOR, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  editor_signals[COMMAND_CHANGED] =
    g_signal_new ("command-changed", GP_TYPE_EDITOR, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  editor_signals[COMMENT_CHANGED] =
    g_signal_new ("comment-changed", GP_TYPE_EDITOR, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gp_editor_class_init (GpEditorClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gp_editor_dispose;
  object_class->finalize = gp_editor_finalize;
  object_class->set_property = gp_editor_set_property;

  install_properties (object_class);
  install_signals ();
}

static void
gp_editor_init (GpEditor *self)
{
  GtkWidget *hbox;
  GtkWidget *grid;

  self->icon_theme = gtk_icon_theme_new ();

  /* Icon */
  self->icon = NULL;
  self->icon_button = create_icon_button (self);

  gtk_box_pack_start (GTK_BOX (self), self->icon_button, FALSE, FALSE, 0);
  gtk_widget_set_valign (self->icon_button, GTK_ALIGN_START);
  gtk_widget_show (self->icon_button);

  /* Grid */
  grid = gtk_grid_new ();
  gtk_box_pack_end (GTK_BOX (self), grid, TRUE, TRUE, 0);
  gtk_widget_show (grid);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  /* Type */
  self->type_label = label_new_with_mnemonic (_("_Type:"));
  self->type_combo = gtk_combo_box_new ();
  gtk_grid_attach (GTK_GRID (grid), self->type_label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), self->type_combo, 1, 0, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (self->type_label), self->type_combo);
  gtk_widget_set_hexpand (self->type_combo, TRUE);
  gtk_widget_show (self->type_combo);

  /* Name */
  self->name_label = label_new_with_mnemonic (_("_Name:"));
  self->name_entry = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (grid), self->name_label, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), self->name_entry, 1, 1, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (self->name_label), self->name_entry);
  gtk_widget_set_hexpand (self->name_entry, TRUE);
  gtk_widget_show (self->name_entry);

  g_signal_connect (self->name_entry, "changed",
                    G_CALLBACK (name_changed_cb), self);

  gtk_widget_grab_focus (self->name_entry);

  /* Command */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_widget_show (hbox);

  self->command_label = label_new_with_mnemonic (_("Comm_and:"));

  self->command_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), self->command_entry, TRUE, TRUE, 0);
  gtk_widget_show (self->command_entry);

  self->command_browse = gtk_button_new_with_mnemonic (_("_Browse..."));
  gtk_box_pack_start (GTK_BOX (hbox), self->command_browse, FALSE, FALSE, 0);
  gtk_widget_show (self->command_browse);

  gtk_grid_attach (GTK_GRID (grid), self->command_label, 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), hbox, 1, 2, 1, 1);

  g_signal_connect (self->command_browse, "clicked",
                    G_CALLBACK (command_browse_clicked_cb), self);

  g_signal_connect (self->command_entry, "changed",
                    G_CALLBACK (command_changed_cb), self);

  /* Comment */
  self->comment_label = label_new_with_mnemonic (_("Co_mment:"));
  self->comment_entry = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (grid), self->comment_label, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), self->comment_entry, 1, 3, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (self->comment_label), self->comment_entry);
  gtk_widget_set_hexpand (self->comment_entry, TRUE);
  gtk_widget_show (self->comment_entry);

  g_signal_connect (self->comment_entry, "changed",
                    G_CALLBACK (comment_changed_cb), self);

  setup_type_combo (self);
}

GtkWidget *
gp_editor_new (gboolean edit)
{
  return g_object_new (GP_TYPE_EDITOR,
                       "edit", edit,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "spacing", 12,
                       NULL);
}

const char *
gp_editor_get_icon (GpEditor *self)
{
  if (self->icon != NULL)
    return self->icon;

  return FALLBACK_ICON;
}

void
gp_editor_set_icon (GpEditor   *self,
                    const char *icon)
{
  g_clear_pointer (&self->icon_chooser, gtk_widget_destroy);

  if (g_strcmp0 (self->icon, icon) == 0)
    return;

  g_clear_pointer (&self->icon, g_free);
  self->icon = g_strdup (icon);

  if (self->icon != NULL)
    {
      char *p;

      /* Work around a common mistake in desktop files */
      if ((p = strrchr (self->icon, '.')) != NULL &&
          (strcmp (p, ".png") == 0 ||
           strcmp (p, ".xpm") == 0 ||
           strcmp (p, ".svg") == 0))
        *p = '\0';
    }

  update_icon_image (self);
}

GpEditorType
gp_editor_get_editor_type (GpEditor *self)
{
  return get_editor_type (self);
}

void
gp_editor_set_editor_type (GpEditor     *self,
                           GpEditorType  type)
{
  GtkTreeIter iter;

  gtk_tree_model_get_iter_first (self->type_model, &iter);

  do
    {
      GpEditorType tmp;

      gtk_tree_model_get (self->type_model, &iter, 1, &tmp, -1);

      if (type != tmp)
        continue;

      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self->type_combo), &iter);
    }
  while (gtk_tree_model_iter_next (self->type_model, &iter));

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (self->type_model));
}

const char *
gp_editor_get_name (GpEditor *self)
{
  return gtk_entry_get_text (GTK_ENTRY (self->name_entry));
}

void
gp_editor_set_name (GpEditor   *self,
                    const char *name)
{
  if (name == NULL)
    name = "";

  gtk_entry_set_text (GTK_ENTRY (self->name_entry), name);
}

const char *
gp_editor_get_command (GpEditor *self)
{
  return gtk_entry_get_text (GTK_ENTRY (self->command_entry));
}

void
gp_editor_set_command (GpEditor   *self,
                       const char *command)
{
  if (command == NULL)
    command = "";

  gtk_entry_set_text (GTK_ENTRY (self->command_entry), command);
}

const char *
gp_editor_get_comment (GpEditor *self)
{
  return gtk_entry_get_text (GTK_ENTRY (self->comment_entry));
}

void
gp_editor_set_comment (GpEditor   *self,
                       const char *comment)
{
  if (comment == NULL)
    comment = "";

  gtk_entry_set_text (GTK_ENTRY (self->comment_entry), comment);
}

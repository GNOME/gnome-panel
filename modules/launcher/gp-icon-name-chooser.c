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
#include "gp-icon-name-chooser.h"

#include <glib/gi18n-lib.h>

struct _GpIconNameChooser
{
  GtkWindow              parent;

  GtkIconTheme          *icon_theme;

  GtkWidget             *header_bar;
  GtkWidget             *search_button;
  GtkWidget             *select_button;

  GtkWidget             *search_bar;
  GtkWidget             *search_entry;

  GtkWidget             *context_list;

  GtkListStore          *icon_store;
  GtkTreeModelFilter    *icon_filter;
  GtkWidget             *icon_view;
  GtkCellRendererPixbuf *pixbuf_cell;
  GtkCellRendererText   *name_cell;

  GtkWidget             *standard_button;

  char                  *selected_context;
  char                  *selected_icon;
};

enum
{
  ICON_SELECTED,

  CLOSE,

  LAST_SIGNAL
};

static guint chooser_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpIconNameChooser, gp_icon_name_chooser, GTK_TYPE_WINDOW)

typedef struct
{
  const char  *name;
  const char  *directory;
  const char **icons;
} GpStandardIcons;

static const char *action_icons[] =
{
  "address-book-new",
  "application-exit",
  "appointment-new",
  "call-start",
  "call-stop",
  "contact-new",
  "document-new",
  "document-open",
  "document-open-recent",
  "document-page-setup",
  "document-print",
  "document-print-preview",
  "document-properties",
  "document-revert",
  "document-save",
  "document-save-as",
  "document-send",
  "edit-clear",
  "edit-copy",
  "edit-cut",
  "edit-delete",
  "edit-find",
  "edit-find-replace",
  "edit-paste",
  "edit-redo",
  "edit-select-all",
  "edit-undo",
  "folder-new",
  "format-indent-less",
  "format-indent-more",
  "format-justify-center",
  "format-justify-fill",
  "format-justify-left",
  "format-justify-right",
  "format-text-direction-ltr",
  "format-text-direction-rtl",
  "format-text-bold",
  "format-text-italic",
  "format-text-underline",
  "format-text-strikethrough",
  "go-bottom",
  "go-down",
  "go-first",
  "go-home",
  "go-jump",
  "go-last",
  "go-next",
  "go-previous",
  "go-top",
  "go-up",
  "help-about",
  "help-contents",
  "help-faq",
  "insert-image",
  "insert-link",
  "insert-object",
  "insert-text",
  "list-add",
  "list-remove",
  "mail-forward",
  "mail-mark-important",
  "mail-mark-junk",
  "mail-mark-notjunk",
  "mail-mark-read",
  "mail-mark-unread",
  "mail-message-new",
  "mail-reply-all",
  "mail-reply-sender",
  "mail-send",
  "mail-send-receive",
  "media-eject",
  "media-playback-pause",
  "media-playback-start",
  "media-playback-stop",
  "media-record",
  "media-seek-backward",
  "media-seek-forward",
  "media-skip-backward",
  "media-skip-forward",
  "object-flip-horizontal",
  "object-flip-vertical",
  "object-rotate-left",
  "object-rotate-right",
  "process-stop",
  "system-lock-screen",
  "system-log-out",
  "system-run",
  "system-search",
  "system-reboot",
  "system-shutdown",
  "tools-check-spelling",
  "view-fullscreen",
  "view-refresh",
  "view-restore",
  "view-sort-ascending",
  "view-sort-descending",
  "window-close",
  "window-new",
  "zoom-fit-best",
  "zoom-in",
  "zoom-original",
  "zoom-out",
  NULL
};

static const char *animation_icons[] =
{
  "process-working",
  NULL
};

static const char *application_icons[] =
{
  "accessories-calculator",
  "accessories-character-map",
  "accessories-dictionary",
  "accessories-text-editor",
  "help-browser",
  "multimedia-volume-control",
  "preferences-desktop-accessibility",
  "preferences-desktop-font",
  "preferences-desktop-keyboard",
  "preferences-desktop-locale",
  "preferences-desktop-multimedia",
  "preferences-desktop-screensaver",
  "preferences-desktop-theme",
  "preferences-desktop-wallpaper",
  "system-file-manager",
  "system-software-install",
  "system-software-update",
  "utilities-system-monitor",
  "utilities-terminal",
  NULL
};

static const char *category_icons[] =
{
  "applications-accessories",
  "applications-development",
  "applications-engineering",
  "applications-games",
  "applications-graphics",
  "applications-internet",
  "applications-multimedia",
  "applications-office",
  "applications-other",
  "applications-science",
  "applications-system",
  "applications-utilities",
  "preferences-desktop",
  "preferences-desktop-peripherals",
  "preferences-desktop-personal",
  "preferences-other",
  "preferences-system",
  "preferences-system-network",
  "system-help",
  NULL
};

static const char *device_icons[] =
{
  "audio-card",
  "audio-input-microphone",
  "battery",
  "camera-photo",
  "camera-video",
  "camera-web",
  "computer",
  "drive-harddisk",
  "drive-optical",
  "drive-removable-media",
  "input-gaming",
  "input-keyboard",
  "input-mouse",
  "input-tablet",
  "media-flash",
  "media-floppy",
  "media-optical",
  "media-tape",
  "modem",
  "multimedia-player",
  "network-wired",
  "network-wireless",
  "pda",
  "phone",
  "printer",
  "scanner",
  "video-display",
  NULL
};

static const char *emblem_icons[] =
{
  "emblem-default",
  "emblem-documents",
  "emblem-downloads",
  "emblem-favorite",
  "emblem-important",
  "emblem-mail",
  "emblem-photos",
  "emblem-readonly",
  "emblem-shared",
  "emblem-symbolic-link",
  "emblem-synchronized",
  "emblem-system",
  "emblem-unreadable",
  NULL
};

static const char *emotion_icons[] =
{
  "face-angel",
  "face-angry",
  "face-cool",
  "face-crying",
  "face-devilish",
  "face-embarrassed",
  "face-kiss",
  "face-laugh",
  "face-monkey",
  "face-plain",
  "face-raspberry",
  "face-sad",
  "face-sick",
  "face-smile",
  "face-smile-big",
  "face-smirk",
  "face-surprise",
  "face-tired",
  "face-uncertain",
  "face-wink",
  "face-worried",
  NULL
};

static const char *international_icons[] =
{
  NULL
};

static const char *mime_type_icons[] =
{
  "application-x-executable",
  "audio-x-generic",
  "font-x-generic",
  "image-x-generic",
  "package-x-generic",
  "text-html",
  "text-x-generic",
  "text-x-generic-template",
  "text-x-script",
  "video-x-generic",
  "x-office-address-book",
  "x-office-calendar",
  "x-office-document",
  "x-office-presentation",
  "x-office-spreadsheet",
  NULL
};

static const char *place_icons[] =
{
  "folder",
  "folder-remote",
  "network-server",
  "network-workgroup",
  "start-here",
  "user-bookmarks",
  "user-desktop",
  "user-home",
  "user-trash",
  NULL
};

static const char *status_icons[] =
{
  "appointment-missed",
  "appointment-soon",
  "audio-volume-high",
  "audio-volume-low",
  "audio-volume-medium",
  "audio-volume-muted",
  "battery-caution",
  "battery-low",
  "dialog-error",
  "dialog-information",
  "dialog-password",
  "dialog-question",
  "dialog-warning",
  "folder-drag-accept",
  "folder-open",
  "folder-visiting",
  "image-loading",
  "image-missing",
  "mail-attachment",
  "mail-unread",
  "mail-read",
  "mail-replied",
  "mail-signed",
  "mail-signed-verified",
  "media-playlist-repeat",
  "media-playlist-shuffle",
  "network-error",
  "network-idle",
  "network-offline",
  "network-receive",
  "network-transmit",
  "network-transmit-receive",
  "printer-error",
  "printer-printing",
  "security-high",
  "security-medium",
  "security-low",
  "software-update-available",
  "software-update-urgent",
  "sync-error",
  "sync-synchronizing",
  "task-due",
  "task-past-due",
  "user-available",
  "user-away",
  "user-idle",
  "user-offline",
  "user-trash-full",
  "weather-clear",
  "weather-clear-night",
  "weather-few-clouds",
  "weather-few-clouds-night",
  "weather-fog",
  "weather-overcast",
  "weather-severe-alert",
  "weather-showers",
  "weather-showers-scattered",
  "weather-snow",
  "weather-storm",
  NULL
};

static GpStandardIcons standard_icon_names[] =
{
  { "Actions", "actions", action_icons },
  { "Animations", "animations", animation_icons },
  { "Applications", "apps", application_icons },
  { "Categories", "categories", category_icons },
  { "Devices", "devices", device_icons },
  { "Emblems", "emblems", emblem_icons },
  { "Emotes", "emotes", emotion_icons },
  { "International", "intl", international_icons },
  { "MimeTypes", "mimetypes", mime_type_icons },
  { "Places", "places", place_icons },
  { "Status", "status", status_icons },
  { NULL }
};

static gboolean
is_standard_icon_name (const char *icon_name,
                       const char *context)
{
  int i;

  for (i = 0; standard_icon_names[i].name != NULL; i++)
    {
      int j;

      if (g_strcmp0 (context, standard_icon_names[i].name) != 0)
        continue;

      for (j = 0; standard_icon_names[i].icons[j] != NULL; j++)
        {
          if (g_strcmp0 (icon_name, standard_icon_names[i].icons[j]) == 0)
            return TRUE;
        }
    }

  return FALSE;
}

static GtkWidget *
create_context_row (const char *context,
                    const char *title,
                    gboolean    standard)
{
  GtkWidget *row;
  GtkStyleContext *style;
  GtkWidget *label;

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  g_object_set_data_full (G_OBJECT (row), "context", g_strdup (context), g_free);
  g_object_set_data (G_OBJECT (row), "standard", GUINT_TO_POINTER (standard));
  gtk_widget_show (row);

  style = gtk_widget_get_style_context (row);
  gtk_style_context_add_class (style, "context-row");

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), .0);
  gtk_container_add (GTK_CONTAINER (row), label);
  gtk_widget_show (label);

  return row;
}

static void
load_icon_names (GpIconNameChooser *self)
{
  GtkWidget *row;
  GList *contexts;
  GList *l1;

  row = create_context_row ("All", _("All"), TRUE);
  gtk_list_box_prepend (GTK_LIST_BOX (self->context_list), row);
  gtk_list_box_select_row (GTK_LIST_BOX (self->context_list),
                           GTK_LIST_BOX_ROW (row));

  contexts = gtk_icon_theme_list_contexts (self->icon_theme);

  for (l1 = contexts; l1 != NULL; l1 = l1->next)
    {
      const char *context;
      gboolean standard;
      int i;
      GList *icons;
      GList *l2;

      context = l1->data;

      standard = FALSE;
      for (i = 0; standard_icon_names[i].name != NULL; i++)
        {
          if (g_strcmp0 (context, standard_icon_names[i].name) == 0)
            {
              standard = TRUE;
              break;
            }
        }

      row = create_context_row (context, _(context), standard);
      gtk_list_box_prepend (GTK_LIST_BOX (self->context_list), row);

      icons = gtk_icon_theme_list_icons (self->icon_theme, context);

      for (l2 = icons; l2 != NULL; l2 = l2->next)
        {
          const char *icon_name;

          icon_name = l2->data;

          standard = is_standard_icon_name (icon_name, context);

          gtk_list_store_insert_with_values (self->icon_store,
                                             NULL,
                                             -1,
                                             0, context,
                                             1, icon_name,
                                             2, standard,
                                             -1);
        }

      g_list_free_full (icons, g_free);
    }

  g_list_free_full (contexts, g_free);
}

static void
cancel_button_clicked_cb (GtkButton         *button,
                          GpIconNameChooser *self)
{
  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
select_button_clicked_cb (GtkButton         *button,
                          GpIconNameChooser *self)
{
  g_signal_emit (self, chooser_signals[ICON_SELECTED], 0, self->selected_icon);
  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
search_entry_search_changed_cb (GtkSearchEntry    *entry,
                                GpIconNameChooser *self)
{
  gtk_icon_view_unselect_all (GTK_ICON_VIEW (self->icon_view));
  gtk_tree_model_filter_refilter (self->icon_filter);
}

static void
context_list_row_selected_cb (GtkListBox        *box,
                              GtkListBoxRow     *row,
                              GpIconNameChooser *self)
{
  const char *context;

  if (row != NULL)
    context = g_object_get_data (G_OBJECT (row), "context");
  else
    context = "All";

  if (g_strcmp0 (self->selected_context, context) == 0)
    return;

  g_clear_pointer (&self->selected_context, g_free);
  self->selected_context = g_strdup (context);

  gtk_icon_view_unselect_all (GTK_ICON_VIEW (self->icon_view));
  gtk_tree_model_filter_refilter (self->icon_filter);
}

static void
icon_view_item_activated_cb (GtkIconView       *iconview,
                             GtkTreePath       *path,
                             GpIconNameChooser *self)
{
  select_button_clicked_cb (GTK_BUTTON (self->select_button), self);
}

static void
icon_view_selection_changed_cb (GtkIconView       *icon_view,
                                GpIconNameChooser *self)
{
  GList *selected_items;
  GtkTreeModel *model;
  GtkTreeIter iter;
  char *icon_name;

  selected_items = gtk_icon_view_get_selected_items (icon_view);

  if (selected_items == NULL)
    {
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->header_bar), NULL);
      gtk_widget_set_sensitive (self->select_button, FALSE);

      g_clear_pointer (&self->selected_icon, g_free);
      return;
    }

  model = GTK_TREE_MODEL (self->icon_filter);

  gtk_tree_model_get_iter (model, &iter, selected_items->data);
  gtk_tree_model_get (model, &iter, 1, &icon_name, -1);

  g_list_free_full (selected_items, (GDestroyNotify) gtk_tree_path_free);

  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->header_bar), icon_name);
  gtk_widget_set_sensitive (self->select_button, icon_name != NULL);

  g_clear_pointer (&self->selected_icon, g_free);
  self->selected_icon = icon_name;
}

static void
standard_check_button_toggled_cb (GtkToggleButton   *toggle_button,
                                  GpIconNameChooser *self)
{
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->context_list));
  gtk_tree_model_filter_refilter (self->icon_filter);
}

static void
close_cb (GpIconNameChooser *self,
          gpointer           user_data)
{
  gtk_widget_destroy (GTK_WIDGET (self));
}

static gboolean
key_press_event_cb (GtkWidget    *window,
                    GdkEvent     *event,
                    GtkSearchBar *search_bar)
{
  return gtk_search_bar_handle_event (search_bar, event);
}

static gboolean
filter_contexts_func (GtkListBoxRow *row,
                      gpointer       user_data)
{
  GpIconNameChooser *self;

  self = GP_ICON_NAME_CHOOSER (user_data);

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->standard_button)))
    return TRUE;

  return GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "standard"));
}

static int
sort_contexts_func (GtkListBoxRow *row1,
                    GtkListBoxRow *row2,
                    gpointer       user_data)
{
  const char *context1;
  const char *context2;

  context1 = g_object_get_data (G_OBJECT (row1), "context");
  context2 = g_object_get_data (G_OBJECT (row2), "context");

  if (g_strcmp0 (context1, "All") == 0)
    return -1;
  else if (g_strcmp0 (context2, "All") == 0)
    return 1;

  return g_strcmp0 (context1, context2);
}

static gboolean
icon_visible_func (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   gpointer      user_data)

{
  GpIconNameChooser *self;
  char *context;
  char *icon_name;
  gboolean standard;
  gboolean visible;

  self = GP_ICON_NAME_CHOOSER (user_data);

  gtk_tree_model_get (model, iter,
                      0, &context,
                      1, &icon_name,
                      2, &standard,
                      -1);

  if (icon_name == NULL)
    {
      visible = FALSE;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->standard_button)) &&
           !standard)
    {
      visible = FALSE;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->search_button)))
    {
      const char *search_text;

      search_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));

      visible = (g_strcmp0 (self->selected_context, "All") == 0 ||
                 g_strcmp0 (self->selected_context, context) == 0) &&
                strstr (icon_name, search_text) != NULL;
    }
  else
    {
      visible = g_strcmp0 (self->selected_context, "All") == 0 ||
                g_strcmp0 (self->selected_context, context) == 0;
    }

  g_free (context);
  g_free (icon_name);

  return visible;
}

static void
gp_icon_name_chooser_dispose (GObject *object)
{
  GpIconNameChooser *self;

  self = GP_ICON_NAME_CHOOSER (object);

  g_clear_object (&self->icon_theme);

  G_OBJECT_CLASS (gp_icon_name_chooser_parent_class)->dispose (object);
}

static void
gp_icon_name_chooser_finalize (GObject *object)
{
  GpIconNameChooser *self;

  self = GP_ICON_NAME_CHOOSER (object);

  g_clear_pointer (&self->selected_context, g_free);
  g_clear_pointer (&self->selected_icon, g_free);

  G_OBJECT_CLASS (gp_icon_name_chooser_parent_class)->finalize (object);
}

static void
install_signals (void)
{
  chooser_signals[ICON_SELECTED] =
    g_signal_new ("icon-selected",
                  GP_TYPE_ICON_NAME_CHOOSER,
                  0,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  chooser_signals[CLOSE] =
    g_signal_new ("close",
                  GP_TYPE_ICON_NAME_CHOOSER,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
gp_icon_name_chooser_class_init (GpIconNameChooserClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;
  const char *resource_name;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->dispose = gp_icon_name_chooser_dispose;
  object_class->finalize = gp_icon_name_chooser_finalize;

  install_signals ();

  binding_set = gtk_binding_set_by_class (widget_class);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  resource_name = GRESOURCE_PREFIX "/gp-icon-name-chooser.ui";
  gtk_widget_class_set_template_from_resource (widget_class, resource_name);

  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, header_bar);

  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked_cb);

  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, search_button);

  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, select_button);
  gtk_widget_class_bind_template_callback (widget_class, select_button_clicked_cb);

  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, search_bar);
  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, search_entry);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_search_changed_cb);

  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, context_list);
  gtk_widget_class_bind_template_callback (widget_class, context_list_row_selected_cb);

  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, icon_store);
  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, icon_filter);
  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, icon_view);
  gtk_widget_class_bind_template_callback (widget_class, icon_view_item_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, icon_view_selection_changed_cb);
  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, pixbuf_cell);
  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, name_cell);

  gtk_widget_class_bind_template_child (widget_class, GpIconNameChooser, standard_button);
  gtk_widget_class_bind_template_callback (widget_class, standard_check_button_toggled_cb);
}

static void
gp_icon_name_chooser_init (GpIconNameChooser *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->icon_theme = gtk_icon_theme_new ();

  g_object_bind_property (self->search_button,
                          "active",
                          self->search_bar,
                          "search-mode-enabled",
                          G_BINDING_BIDIRECTIONAL);

  g_signal_connect (self, "close", G_CALLBACK (close_cb), NULL);

  g_signal_connect (self,
                    "key-press-event",
                    G_CALLBACK (key_press_event_cb),
                    self->search_bar);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->context_list),
                                filter_contexts_func,
                                self,
                                NULL);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->context_list),
                              sort_contexts_func,
                              self,
                              NULL);

  gtk_tree_model_filter_set_visible_func (self->icon_filter,
                                          icon_visible_func,
                                          self,
                                          NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->icon_store),
                                        1,
                                        GTK_SORT_ASCENDING);

  g_object_set (self->name_cell, "xalign", 0.5, NULL);

  load_icon_names (self);
}

GtkWidget *
gp_icon_name_chooser_new (void)
{
  return g_object_new (GP_TYPE_ICON_NAME_CHOOSER, NULL);
}

void
gp_icon_name_chooser_set_icon_name (GpIconNameChooser *self,
                                    const char        *icon_name)
{
  GtkTreeModel *model;
  gboolean valid;
  GtkTreeIter iter;
  GtkTreePath *path;

  if (!gtk_icon_theme_has_icon (self->icon_theme, icon_name))
    return;

  g_clear_pointer (&self->selected_icon, g_free);
  self->selected_icon = g_strdup (icon_name);

  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->header_bar), self->selected_icon);
  gtk_widget_set_sensitive (self->select_button, self->selected_icon != NULL);

  model = GTK_TREE_MODEL (self->icon_filter);
  valid = gtk_tree_model_get_iter_first (model, &iter);
  path = NULL;

  while (valid)
    {
      char *tmp;

      gtk_tree_model_get (model, &iter, 1, &tmp, -1);

      if (g_strcmp0 (self->selected_icon, tmp) == 0)
        {
          path = gtk_tree_model_get_path (model, &iter);
          g_free (tmp);
          break;
        }

      valid = gtk_tree_model_iter_next (model, &iter);
      g_free (tmp);
    }

  if (path == NULL)
    return;

  gtk_icon_view_select_path (GTK_ICON_VIEW (self->icon_view), path);
  gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (self->icon_view),
                                path,
                                TRUE,
                                0.5,
                                0.5);

  gtk_tree_path_free (path);
}

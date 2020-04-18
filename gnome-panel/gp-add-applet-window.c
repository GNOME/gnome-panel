/*
 * Copyright (C) 2004 Vincent Untz
 * Copyright (C) 2018-2020 Alberts Muktupāvels
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

#include <glib/gi18n.h>

#include "gp-add-applet-window.h"
#include "gp-applet-row.h"
#include "gp-module-manager.h"
#include "libgnome-panel/gp-applet-info-private.h"
#include "libgnome-panel/gp-module-private.h"
#include "libpanel-util/panel-glib.h"
#include "panel-applets-manager.h"

struct _GpAddAppletWindow
{
  GtkWindow        parent;

  GpModuleManager *module_manager;
  PanelToplevel   *toplevel;

  GtkWidget       *header_bar;
  GtkWidget       *search_button;

  GtkWidget       *search_bar;
  GtkWidget       *modules_box;
  GtkWidget       *not_found;

  gchar           *search_text;
};

enum
{
  PROP_0,

  PROP_MODULE_MANAGER,
  PROP_TOPLEVEL,

  LAST_PROP
};

static GParamSpec *window_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpAddAppletWindow, gp_add_applet_window, GTK_TYPE_WINDOW)

static void
label_make_bold (GtkWidget *label)
{
  PangoLayout *layout;
  PangoAttrList *attrs;

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  layout = gtk_label_get_layout (GTK_LABEL (label));
  attrs = pango_layout_get_attributes (layout);

  if (!attrs)
    {
      attrs = pango_attr_list_new ();
      pango_layout_set_attributes (layout, attrs);
      pango_attr_list_unref (attrs);
    }

  pango_attr_list_change (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
}

static void
header_func_cb (GtkListBoxRow *row,
                GtkListBoxRow *before,
                gpointer       user_data)
{
  GtkWidget *separator;

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

  gtk_list_box_row_set_header (row, separator);
}

static gboolean
filter_func_cb (GtkListBoxRow *row,
                gpointer       user_data)
{
  GpAddAppletWindow *window;
  gboolean searching;
  GpAppletInfo *info;
  gboolean retval;

  window = GP_ADD_APPLET_WINDOW (user_data);
  searching = window->search_text && window->search_text[0] != '\0';

  if (!searching)
    return TRUE;

  info = gp_applet_row_get_info (GP_APPLET_ROW (row));

  if (!info)
    return FALSE;

  retval = FALSE;

  if (info->name)
    retval |= panel_g_utf8_strstrcase (info->name, window->search_text) != NULL;

  if (info->description)
    retval |= panel_g_utf8_strstrcase (info->description, window->search_text) != NULL;

  return retval;
}

static gint
sort_func_cb (GtkListBoxRow *row1,
              GtkListBoxRow *row2,
              gpointer       user_data)
{
  GpAppletInfo *info1;
  GpAppletInfo *info2;

  info1 = gp_applet_row_get_info (GP_APPLET_ROW (row1));
  info2 = gp_applet_row_get_info (GP_APPLET_ROW (row2));

  return g_utf8_collate (info1->name, info2->name);
}

typedef struct
{
  PanelToplevel       *toplevel;
  PanelObjectPackType  pack_type;
  int                  pack_index;
  char                *iid;
} InitialSetupData;

static InitialSetupData *
initial_setup_data_new (PanelToplevel       *toplevel,
                        PanelObjectPackType  pack_type,
                        int                  pack_index,
                        const char          *iid)
{
  InitialSetupData *data;

  data = g_new0 (InitialSetupData, 1);

  data->toplevel = toplevel;
  data->pack_type = pack_type;
  data->pack_index = pack_index;
  data->iid = g_strdup (iid);

  return data;
}

static void
initial_setup_data_free (gpointer user_data)
{
  InitialSetupData *data;

  data = (InitialSetupData *) user_data;

  g_free (data->iid);
  g_free (data);
}

static void
initial_setup_dialog_cb (GpInitialSetupDialog *dialog,
                         gboolean              canceled,
                         gpointer              user_data)
{
  InitialSetupData *data;
  GVariant *initial_settings;

  if (canceled)
    return;

  data = (InitialSetupData *) user_data;

  initial_settings = gp_initital_setup_dialog_get_settings (dialog);

  panel_applet_frame_create (data->toplevel,
                             data->pack_type,
                             data->pack_index,
                             data->iid,
                             initial_settings);

  g_variant_unref (initial_settings);
}

static void
row_activated_cb (GtkListBox        *box,
                  GtkListBoxRow     *row,
                  GpAddAppletWindow *self)
{
  const char *iid;
  PanelWidget *panel;
  PanelObjectPackType pack_type;
  int pack_index;
  InitialSetupData *data;

  iid = gp_applet_row_get_iid (GP_APPLET_ROW (row));

  pack_type = PANEL_OBJECT_PACK_START;

  panel = panel_toplevel_get_panel_widget (self->toplevel);
  pack_index = panel_widget_get_new_pack_index (panel, pack_type);

  data = initial_setup_data_new (self->toplevel, pack_type, pack_index, iid);

  if (panel_applets_manager_open_initial_setup_dialog (iid,
                                                       NULL,
                                                       GTK_WINDOW (self),
                                                       initial_setup_dialog_cb,
                                                       data,
                                                       initial_setup_data_free))
    return;

  panel_applet_frame_create (self->toplevel, pack_type, pack_index, iid, NULL);
}

static void
add_module (GpAddAppletWindow *window,
            GpModule          *module)
{
  GtkWidget *module_box;
  GtkWidget *header_box;
  GtkWidget *id_label;
  gchar *version;
  GtkWidget *version_label;
  GtkWidget *list_frame;
  GtkWidget *list_box;
  const gchar *const *applets;
  guint i;

  module_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (window->modules_box), module_box, FALSE, FALSE, 0);
  gtk_widget_show (module_box);

  header_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (module_box), header_box, FALSE, FALSE, 0);
  gtk_widget_show (header_box);

  id_label = gtk_label_new (gp_module_get_id (module));
  gtk_box_pack_start (GTK_BOX (header_box), id_label, FALSE, FALSE, 0);
  label_make_bold (id_label);
  gtk_widget_show (id_label);

  version = g_strdup_printf ("<small>%s</small>", gp_module_get_version (module));

  version_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (version_label), version);
  gtk_box_pack_end (GTK_BOX (header_box), version_label, FALSE, FALSE, 0);
  gtk_widget_set_valign (version_label, GTK_ALIGN_END);
  gtk_widget_show (version_label);
  g_free (version);

  list_frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (module_box), list_frame, FALSE, FALSE, 0);
  gtk_widget_show (list_frame);

  list_box = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list_box), GTK_SELECTION_NONE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (list_box), FALSE);
  g_object_set_data (G_OBJECT (module_box), "list-box", list_box);
  gtk_container_add (GTK_CONTAINER (list_frame), list_box);
  gtk_widget_show (list_box);

  g_signal_connect (list_box,
                    "row-activated",
                    G_CALLBACK (row_activated_cb),
                    window);

  applets = gp_module_get_applets (module);
  for (i = 0; applets[i] != NULL; i++)
    {
      GError *error;
      GpAppletInfo *info;
      GtkWidget *row;

      error = NULL;
      info = gp_module_get_applet_info (module, applets[i], &error);

      if (info == NULL)
        {
          g_warning ("%s", error->message);
          g_error_free (error);

          continue;
        }

      row = gp_applet_row_new (module, applets[i]);
      gtk_list_box_prepend (GTK_LIST_BOX (list_box), row);
      gtk_widget_show (row);
    }

  gtk_list_box_set_header_func (GTK_LIST_BOX (list_box),
                                header_func_cb,
                                window,
                                NULL);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (list_box),
                                filter_func_cb, window,
                                NULL);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (list_box),
                              sort_func_cb, window,
                              NULL);
}

static void
destroy_module_cb (GtkWidget *widget,
                    gpointer   user_data)
{
  gtk_widget_destroy (widget);
}

static void
rebuild_modules_list (GpAddAppletWindow *window)
{
  GtkContainer *modules_box;
  GList *modules, *l;
  GList *children;

  modules_box = GTK_CONTAINER (window->modules_box);
  gtk_container_foreach (modules_box, destroy_module_cb, NULL);

  modules = gp_module_manager_get_modules (window->module_manager);

  for (l = modules; l != NULL; l = l->next)
    add_module (window, GP_MODULE (l->data));

  g_list_free (modules);
  children = gtk_container_get_children (modules_box);

  if (children != NULL)
    {
      gtk_widget_hide (window->not_found);
      gtk_widget_show (window->modules_box);
      g_list_free (children);
    }
  else
    {
      gtk_widget_hide (window->modules_box);
      gtk_widget_show (window->not_found);
    }
}

static void
setup_header_bar (GpAddAppletWindow *window)
{
  GtkWidget *find_image;

  window->header_bar = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), window->header_bar);
  gtk_widget_show (window->header_bar);

  gtk_header_bar_set_title (GTK_HEADER_BAR (window->header_bar), _("Add to Panel"));
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (window->header_bar), TRUE);

  window->search_button = gtk_toggle_button_new ();
  gtk_header_bar_pack_end (GTK_HEADER_BAR (window->header_bar), window->search_button);
  gtk_widget_show (window->search_button);

  find_image = gtk_image_new_from_icon_name ("edit-find-symbolic", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (window->search_button), find_image);
  gtk_widget_show (find_image);
}

static void
foreach_modules_cb (GtkWidget *widget,
                    gpointer   user_data)
{
  GtkListBox *list_box;
  GList *children, *l;

  gtk_widget_set_visible (widget, FALSE);
  list_box = g_object_get_data (G_OBJECT (widget), "list-box");

  if (list_box == NULL)
    return;

  gtk_list_box_invalidate_filter (list_box);

  children = gtk_container_get_children (GTK_CONTAINER (list_box));
  if (children == NULL)
    return;

  for (l = children; l != NULL; l = l->next)
    {
      if (gtk_widget_get_child_visible (l->data))
        {
          gtk_widget_set_visible (widget, TRUE);
          break;
        }
    }

  g_list_free (children);
}

static void
search_changed_cb (GtkSearchEntry    *entry,
                   GpAddAppletWindow *window)
{
  GList *children, *l;
  gboolean visible;

  g_clear_pointer (&window->search_text, g_free);
  window->search_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

  gtk_container_foreach (GTK_CONTAINER (window->modules_box),
                         foreach_modules_cb, window);

  children = gtk_container_get_children (GTK_CONTAINER (window->modules_box));
  visible = FALSE;

  if (children != NULL)
    {
      for (l = children; l != NULL; l = l->next)
        {
          if (gtk_widget_get_visible (l->data))
            {
              visible = TRUE;
              break;
            }
        }

      g_list_free (children);
    }

  if (visible)
    {
      gtk_widget_hide (window->not_found);
      gtk_widget_show (window->modules_box);
    }
  else
    {
      gtk_widget_hide (window->modules_box);
      gtk_widget_show (window->not_found);
    }
}

static gboolean
window_key_press_event_cb (GtkWidget    *window,
                           GdkEvent     *event,
                           GtkSearchBar *search_bar)
{
  return gtk_search_bar_handle_event (search_bar, event);
}

static void
setup_not_found_label (GtkWidget *label)
{
  GtkStyleContext *context;
  PangoLayout *layout;
  PangoAttrList *attrs;

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_vexpand (label, TRUE);

  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "dim-label");

  layout = gtk_label_get_layout (GTK_LABEL (label));
  attrs = pango_layout_get_attributes (layout);

  if (!attrs)
    {
      attrs = pango_attr_list_new ();
      pango_layout_set_attributes (layout, attrs);
      pango_attr_list_unref (attrs);
    }

  pango_attr_list_change (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  pango_attr_list_change (attrs, pango_attr_scale_new (1.5));
}

static void
setup_window_content (GpAddAppletWindow *window)
{
  GtkWidget *content;
  GtkWidget *search_entry;
  GtkWidget *scrolled_window;
  GtkWidget *box;
  GtkWidget *label;

  content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), content);
  gtk_widget_show (content);

  window->search_bar = gtk_search_bar_new ();
  gtk_box_pack_start (GTK_BOX (content), window->search_bar, FALSE, FALSE, 0);
  gtk_widget_show (window->search_bar);

  search_entry = gtk_search_entry_new ();
  gtk_container_add (GTK_CONTAINER (window->search_bar), search_entry);
  gtk_widget_set_size_request (search_entry, 400, -1);
  gtk_widget_show (search_entry);

  g_object_bind_property (window->search_button, "active",
                          window->search_bar, "search-mode-enabled",
                          G_BINDING_BIDIRECTIONAL);

  g_signal_connect (search_entry, "search-changed",
                    G_CALLBACK (search_changed_cb),
                    window);

  g_signal_connect (window, "key-press-event",
                    G_CALLBACK (window_key_press_event_cb),
                    window->search_bar);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (content), scrolled_window, TRUE, TRUE, 0);
  gtk_widget_show (scrolled_window);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (scrolled_window), box);
  gtk_widget_show (box);

  window->modules_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 24);
  gtk_box_pack_start (GTK_BOX (box), window->modules_box, TRUE, TRUE, 0);
  g_object_set (box, "margin", 32, NULL);
  gtk_widget_show (window->modules_box);

  window->not_found = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (box), window->not_found, TRUE, TRUE, 0);

  label = gtk_label_new (_("No applets found"));
  gtk_box_pack_start (GTK_BOX (window->not_found), label, FALSE, FALSE, 0);
  setup_not_found_label (label);
  gtk_widget_show (label);
}

static void
update_subtitle (GpAddAppletWindow *self)
{
  const char *name;
  char *title;

  name = panel_toplevel_get_name (self->toplevel);

  if (name != NULL && *name != '\0')
    title = g_strdup_printf (_("Find an item to add to “%s”"), name);
  else
    title = g_strdup (_("Find an item to add to the panel"));

  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->header_bar), title);
  g_free (title);
}

static void
toplevel_name_changed_cb (GObject           *object,
                          GParamSpec        *pspec,
                          GpAddAppletWindow *self)
{
  update_subtitle (self);
}

static void
gp_add_applet_window_constructed (GObject *object)
{
  GpAddAppletWindow *self;

  self = GP_ADD_APPLET_WINDOW (object);

  G_OBJECT_CLASS (gp_add_applet_window_parent_class)->constructed (object);

  g_signal_connect_object (self->toplevel,
                           "notify::name",
                           G_CALLBACK (toplevel_name_changed_cb),
                           self,
                           0);

  rebuild_modules_list (self);
  update_subtitle (self);
}

static void
gp_add_applet_window_finalize (GObject *object)
{
  GpAddAppletWindow *window;

  window = GP_ADD_APPLET_WINDOW (object);

  g_clear_pointer (&window->search_text, g_free);

  G_OBJECT_CLASS (gp_add_applet_window_parent_class)->finalize (object);
}

static void
gp_add_applet_window_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GpAddAppletWindow *window;

  window = GP_ADD_APPLET_WINDOW (object);

  switch (property_id)
    {
      case PROP_MODULE_MANAGER:
        window->module_manager = g_value_get_object (value);
        break;

      case PROP_TOPLEVEL:
        g_assert (window->toplevel == NULL);
        window->toplevel = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  window_properties[PROP_MODULE_MANAGER] =
    g_param_spec_object ("module-manager",
                         "GpModuleManager",
                         "GpModuleManager",
                         GP_TYPE_MODULE_MANAGER,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  window_properties[PROP_TOPLEVEL] =
    g_param_spec_object ("toplevel",
                         "toplevel",
                         "toplevel",
                         PANEL_TYPE_TOPLEVEL,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     window_properties);
}

static void
gp_add_applet_window_class_init (GpAddAppletWindowClass *window_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (window_class);

  object_class->constructed = gp_add_applet_window_constructed;
  object_class->finalize = gp_add_applet_window_finalize;
  object_class->set_property = gp_add_applet_window_set_property;

  install_properties (object_class);
}

static void
gp_add_applet_window_init (GpAddAppletWindow *window)
{
  setup_header_bar (window);
  setup_window_content (window);

  gtk_window_set_default_size (GTK_WINDOW (window), 600, 480);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
}

GtkWidget *
gp_add_applet_window_new (GpModuleManager *manager,
                          PanelToplevel   *toplevel)
{
  return g_object_new (GP_TYPE_ADD_APPLET_WINDOW,
                       "module-manager", manager,
                       "toplevel", toplevel,
                       NULL);
}

/*
 * Copyright (C) 2016 Alberts Muktupāvels
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
#include <libgnome-panel/gp-applet-info.h>

#include "gp-properties-dialog.h"
#include "panel-schemas.h"
#include "panel-applets-manager.h"
#include "gp-applet-list-row.h"
#include "panel-layout.h"

struct _GpPropertiesDialog
{
  GtkWindow  parent;

  gchar     *toplevel_id;

  GSettings *toplevel;
  GSettings *theme;

  GtkWidget *toplevel_writable;
  GtkWidget *theme_writable;

  GtkWidget *orientation;
  GtkWidget *alignment;
  GtkWidget *size;
  GtkWidget *expand;
  GtkWidget *auto_hide;
  GtkWidget *enable_buttons;
  GtkWidget *enable_arrows;

  GtkWidget *custom_bg_color;
  GtkWidget *bg_color_box;
  GtkWidget *bg_color;

  GtkWidget *custom_bg_image;
  GtkWidget *bg_image_box;
  GtkWidget *bg_image;
  GtkWidget *tile;
  GtkWidget *stretch;
  GtkWidget *fit;
  GtkWidget *rotate;

  GtkWidget *custom_fg_color;
  GtkWidget *fg_color_box;
  GtkWidget *fg_color;

  GtkWidget *applet_box;
  GtkWidget *applet_box_left;
  GtkWidget *applet_box_center;
  GtkWidget *applet_box_right;
};

enum
{
  PROP_0,

  PROP_TOPLEVEL_ID,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpPropertiesDialog, gp_properties_dialog, GTK_TYPE_WINDOW)

static gboolean
style_get (GValue   *value,
           GVariant *variant,
           gpointer  user_data)
{
  const gchar *str;

  str = g_variant_get_string (variant, NULL);

  if (g_strcmp0 (str, (const gchar *) user_data) == 0)
    g_value_set_boolean (value, TRUE);
  else
    g_value_set_boolean (value, FALSE);

  return TRUE;
}

static GVariant *
style_set (const GValue       *value,
           const GVariantType *expected_type,
           gpointer            user_data)
{
  if (g_value_get_boolean (value))
    return g_variant_new_string ((const gchar *) user_data);

  return NULL;
}

static void
bg_image_changed_cb (GSettings          *settings,
                     const gchar        *key,
                     GpPropertiesDialog *dialog)
{
  GtkFileChooser *chooser;
  gchar *uri;

  chooser = GTK_FILE_CHOOSER (dialog->bg_image);
  uri = g_settings_get_string (dialog->theme, key);

  if (!uri)
    gtk_file_chooser_unselect_all (chooser);
  else
    gtk_file_chooser_set_uri (chooser, uri);

  g_free (uri);
}

static void
bg_image_file_set_cb (GtkFileChooserButton *button,
                      GpPropertiesDialog   *dialog)
{
  gchar *uri;

  uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
  if (!uri)
    uri = g_strdup ("");

  g_settings_set_string (dialog->theme, "bg-image", uri);
  g_free (uri);
}

static void
custom_bg_color_toggled_cb (GtkToggleButton    *button,
                            GpPropertiesDialog *dialog)
{
  gboolean sensitive;

  sensitive = gtk_toggle_button_get_active (button);

  gtk_widget_set_sensitive (dialog->bg_color_box, sensitive);
}

static void
custom_bg_image_toggled_cb (GtkToggleButton    *button,
                            GpPropertiesDialog *dialog)
{
  gboolean sensitive;

  sensitive = gtk_toggle_button_get_active (button);

  gtk_widget_set_sensitive (dialog->bg_image_box, sensitive);
}

static void
custom_fg_color_toggled_cb (GtkToggleButton    *button,
                            GpPropertiesDialog *dialog)
{
  gboolean sensitive;

  sensitive = gtk_toggle_button_get_active (button);

  gtk_widget_set_sensitive (dialog->fg_color_box, sensitive);
}

static gboolean
color_get (GValue   *value,
           GVariant *variant,
           gpointer  user_data)
{
  const gchar *color;
  GdkRGBA rgba;

  g_variant_get (variant, "&s", &color);

  if (!gdk_rgba_parse (&rgba, color))
    return FALSE;

  g_value_set_boxed (value, &rgba);

  return TRUE;
}

static GVariant *
color_set (const GValue       *value,
           const GVariantType *expected_type,
           gpointer            user_data)
{
  GdkRGBA *rgba;
  gchar *color;
  GVariant *variant;

  rgba = g_value_get_boxed (value);

  if (!rgba)
    return NULL;

  color = gdk_rgba_to_string (rgba);
  variant = g_variant_new_string (color);
  g_free (color);

  return variant;
}

static void
setup_toplevel_bindings (GpPropertiesDialog *dialog)
{
  GtkComboBoxText *text;

  text = GTK_COMBO_BOX_TEXT (dialog->orientation);

  gtk_combo_box_text_append (text, "top", NC_("Orientation", "Top"));
  gtk_combo_box_text_append (text, "bottom", NC_("Orientation", "Bottom"));
  gtk_combo_box_text_append (text, "left", NC_("Orientation", "Left"));
  gtk_combo_box_text_append (text, "right", NC_("Orientation", "Right"));

  text = GTK_COMBO_BOX_TEXT (dialog->alignment);

  gtk_combo_box_text_append (text, "start", NC_("Alignment", "Start"));
  gtk_combo_box_text_append (text, "center", NC_("Alignment", "Center"));
  gtk_combo_box_text_append (text, "end", NC_("Alignment", "End"));

  g_object_bind_property (dialog->expand,
                          "active",
                          dialog->alignment,
                          "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN);

  g_settings_bind (dialog->toplevel, "orientation",
                   dialog->orientation, "active-id",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (dialog->toplevel, "alignment",
                   dialog->alignment, "active-id",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (dialog->toplevel, "size",
                   dialog->size, "value",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (dialog->toplevel, "expand",
                   dialog->expand, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (dialog->toplevel, "auto-hide",
                   dialog->auto_hide, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (dialog->toplevel, "enable-buttons",
                   dialog->enable_buttons, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (dialog->toplevel, "enable-arrows",
                   dialog->enable_arrows, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
setup_theme_bindings (GpPropertiesDialog *dialog)
{
  g_settings_bind (dialog->theme, "custom-bg-color",
                   dialog->custom_bg_color, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (dialog->theme, "bg-color",
                                dialog->bg_color, "rgba",
                                G_SETTINGS_BIND_DEFAULT,
                                color_get, color_set,
                                NULL, NULL);

  g_settings_bind (dialog->theme, "custom-bg-image",
                   dialog->custom_bg_image, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_writable (dialog->theme, "bg-image",
                            dialog->bg_image, "sensitive",
                            FALSE);

  g_signal_connect (dialog->theme, "changed::bg-image",
                    G_CALLBACK (bg_image_changed_cb), dialog);

  g_settings_bind_with_mapping (dialog->theme, "bg-image-style",
                                dialog->tile, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                style_get, style_set,
                                (gpointer) "none",
                                NULL);

  g_settings_bind_with_mapping (dialog->theme, "bg-image-style",
                                dialog->stretch, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                style_get, style_set,
                                (gpointer) "stretch",
                                NULL);

  g_settings_bind_with_mapping (dialog->theme, "bg-image-style",
                                dialog->fit, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                style_get, style_set,
                                (gpointer) "fit",
                                NULL);

  g_settings_bind (dialog->theme, "bg-image-rotate",
                   dialog->rotate, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (dialog->theme, "custom-fg-color",
                   dialog->custom_fg_color, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (dialog->theme, "fg-color",
                                dialog->fg_color, "rgba",
                                G_SETTINGS_BIND_DEFAULT,
                                color_get, color_set,
                                NULL, NULL);

  custom_bg_color_toggled_cb (GTK_TOGGLE_BUTTON (dialog->custom_bg_color), dialog);
  custom_bg_image_toggled_cb (GTK_TOGGLE_BUTTON (dialog->custom_bg_image), dialog);
  custom_fg_color_toggled_cb (GTK_TOGGLE_BUTTON (dialog->custom_fg_color), dialog);
  bg_image_changed_cb (dialog->theme, "bg-image", dialog);
}

static char *
get_applet_iid (AppletInfo *info)
{
  return g_settings_get_string (info->settings, PANEL_OBJECT_IID_KEY);
}

static char *
get_applet_id (AppletInfo *info)
{
  return g_strrstr (get_applet_iid(info), "::") + 2;
}

static PanelObjectPackType
get_applet_pack_type (AppletInfo *info)
{
  return g_settings_get_enum (info->settings, PANEL_OBJECT_PACK_TYPE_KEY);
}

static int
get_applet_pack_index (AppletInfo *info)
{
  return g_settings_get_int (info->settings, PANEL_OBJECT_PACK_INDEX_KEY);
}

static GpModule *
get_module_from_id (GpModuleManager *manager,
                    char            *iid)
{
  const gchar *applet_id;
  gchar *module_id;
  GpModule *module;

  applet_id = g_strrstr (iid, "::");

  if (!applet_id)
    return FALSE;

  module_id = g_strndup (iid, strlen (iid) - strlen (applet_id));

  module = gp_module_manager_get_module (manager, module_id);
  g_free (module_id);

  return module;
}

static void
insert_applet_entry (GpPropertiesDialog *dialog,
                     AppletInfo         *info,
                     GtkWidget          *applet_entry)
{
  PanelObjectPackType pack_type;
  int pack_index;

  pack_type = get_applet_pack_type (info);
  pack_index = get_applet_pack_index (info);

  g_object_set_data (G_OBJECT (applet_entry), "pack-index", GINT_TO_POINTER (pack_index));

  if (pack_type == PANEL_OBJECT_PACK_START)
    {
      gtk_container_add (GTK_CONTAINER (dialog->applet_box_left), applet_entry);
    }

  if (pack_type == PANEL_OBJECT_PACK_CENTER)
    {
      gtk_container_add (GTK_CONTAINER (dialog->applet_box_center), applet_entry);
    }

  if (pack_type == PANEL_OBJECT_PACK_END)
    {
      gtk_container_add (GTK_CONTAINER (dialog->applet_box_right), applet_entry);
    }
}

static gint
applet_sort_func (GtkListBoxRow *row1,
                  GtkListBoxRow *row2,
                  gpointer       user_data)
{
  gpointer index_1;
  gpointer index_2;

  index_1 = g_object_get_data (G_OBJECT (row1), "pack-index");
  index_2 = g_object_get_data (G_OBJECT (row2), "pack-index");

  return GPOINTER_TO_INT (index_1) - GPOINTER_TO_INT (index_2);
}

static gint
applet_sort_func_reverse (GtkListBoxRow *row1,
                          GtkListBoxRow *row2,
                          gpointer       user_data)
{
  gpointer index_1;
  gpointer index_2;

  index_1 = g_object_get_data (G_OBJECT (row1), "pack-index");
  index_2 = g_object_get_data (G_OBJECT (row2), "pack-index");

  return GPOINTER_TO_INT (index_2) - GPOINTER_TO_INT (index_1);
}

static void
row_activated_cb (GtkListBox         *box,
                  GtkListBoxRow      *row,
                  GpPropertiesDialog *self)
{
  AppletInfo *info;

  info = gp_applet_list_row_get_applet_info (GP_APPLET_LIST_ROW (row));

  gtk_container_remove (GTK_CONTAINER (box), GTK_WIDGET (row));

  panel_layout_delete_object (panel_applet_get_id (info));
}

static void
setup_applet_box_structure (GpPropertiesDialog *dialog)
{
  gtk_list_box_set_sort_func (GTK_LIST_BOX (dialog->applet_box_left), applet_sort_func, NULL, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (dialog->applet_box_center), applet_sort_func, NULL, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (dialog->applet_box_right), applet_sort_func_reverse, NULL, NULL);

  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (dialog->applet_box_left), FALSE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (dialog->applet_box_center), FALSE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (dialog->applet_box_right), FALSE);

  g_signal_connect (dialog->applet_box_left,
                    "row-activated",
                    G_CALLBACK (row_activated_cb),
                    dialog);

  g_signal_connect (dialog->applet_box_center,
                    "row-activated",
                    G_CALLBACK (row_activated_cb),
                    dialog);

  g_signal_connect (dialog->applet_box_right,
                    "row-activated",
                    G_CALLBACK (row_activated_cb),
                    dialog);
}

static void
setup_applet_box_add_applets (GpPropertiesDialog *dialog,
                              GSList             *applets)
{
  GSList *iter;
  GpModuleManager *manager;
  GpModule *module;

  manager = panel_applets_manager_get_module_manager ();

  for (iter = applets; iter; iter = iter->next)
    {
      AppletInfo *info;
      GtkWidget *applet_entry;
      const char * applet_toplevel_id;
      const char * applet_id;

      info = iter->data;

      applet_toplevel_id = panel_applet_get_toplevel_id (info);

      if (g_strcmp0 (applet_toplevel_id, dialog->toplevel_id) != 0)
        continue;

      module = get_module_from_id (manager, get_applet_iid (info));
      applet_id = get_applet_id (info);

      applet_entry = gp_applet_list_row_new (module, applet_id, info);

      insert_applet_entry (dialog, info, applet_entry);
    }

  gtk_widget_show_all (dialog->applet_box);
}

static void
setup_applet_box (GpPropertiesDialog *dialog)
{
  GSList *applets;

  setup_applet_box_structure (dialog);

  applets = panel_applet_list_applets ();

  setup_applet_box_add_applets (dialog, applets);
}

static gboolean
all_keys_writable (GSettings    *settings,
                   const gchar **keys)
{
  guint i;

  for (i = 0; keys[i] != NULL; i++)
    {
      if (!g_settings_is_writable (settings, keys[i]))
        return FALSE;
    }

  return TRUE;
}

static gboolean
toplevel_writable_change_event_cb (GSettings          *settings,
                                   guint               key,
                                   GpPropertiesDialog *dialog)
{
  const gchar *toplevel[] = {
    "orientation", "alignment", "size", "expand", "auto-hide",
    "enable-buttons", "enable-arrows", NULL
  };
  gboolean writable;

  writable = all_keys_writable (settings, toplevel);
  gtk_widget_set_visible (dialog->toplevel_writable, !writable);

  return TRUE;
}

static gboolean
theme_writable_change_event_cb (GSettings          *settings,
                                guint               key,
                                GpPropertiesDialog *dialog)
{
  const gchar *theme[] = {
    "custom-bg-color", "bg-color", "custom-bg-image",
    "bg-image", "bg-image-style", "bg-image-rotate",
    "custom-fg-color", "fg-color", NULL
  };
  gboolean writable;

  writable = all_keys_writable (settings, theme);
  gtk_widget_set_visible (dialog->theme_writable, !writable);

  return TRUE;
}

static void
setup_writability (GpPropertiesDialog *dialog)
{
  g_signal_connect (dialog->toplevel, "writable-change-event",
                    G_CALLBACK (toplevel_writable_change_event_cb),
                    dialog);

  g_signal_connect (dialog->theme, "writable-change-event",
                    G_CALLBACK (theme_writable_change_event_cb),
                    dialog);

  toplevel_writable_change_event_cb (dialog->toplevel, 0, dialog);
  theme_writable_change_event_cb (dialog->theme, 0, dialog);
}

static void
setup_bindings (GpPropertiesDialog *dialog)
{
  setup_toplevel_bindings (dialog);
  setup_theme_bindings (dialog);
}

static void
gp_properties_dialog_constructed (GObject *object)
{
  GpPropertiesDialog *dialog;
  const gchar *id;
  const gchar *toplevel;
  gchar *path;

  dialog = GP_PROPERTIES_DIALOG (object);

  G_OBJECT_CLASS (gp_properties_dialog_parent_class)->constructed (object);

  id = dialog->toplevel_id;
  toplevel = "org.gnome.gnome-panel.toplevel";

  path = g_strdup_printf ("/org/gnome/gnome-panel/layout/toplevels/%s/", id);
  dialog->toplevel = g_settings_new_with_path (toplevel, path);
  dialog->theme = g_settings_get_child (dialog->toplevel, "theme");
  g_free (path);

  setup_writability (dialog);
  setup_bindings (dialog);
  setup_applet_box (dialog);
}

static void
gp_properties_dialog_dispose (GObject *object)
{
  GpPropertiesDialog *dialog;

  dialog = GP_PROPERTIES_DIALOG (object);

  g_clear_object (&dialog->toplevel);
  g_clear_object (&dialog->theme);

  G_OBJECT_CLASS (gp_properties_dialog_parent_class)->dispose (object);
}

static void
gp_properties_dialog_finalize (GObject *object)
{
  GpPropertiesDialog *dialog;

  dialog = GP_PROPERTIES_DIALOG (object);

  g_free (dialog->toplevel_id);

  G_OBJECT_CLASS (gp_properties_dialog_parent_class)->finalize (object);
}

static void
gp_properties_dialog_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GpPropertiesDialog *dialog;

  dialog = GP_PROPERTIES_DIALOG (object);

  switch (property_id)
    {
      case PROP_TOPLEVEL_ID:
        dialog->toplevel_id = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  properties[PROP_TOPLEVEL_ID] =
    g_param_spec_string ("toplevel-id", "toplevel-id", "toplevel-id",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
bind_template (GtkWidgetClass *widget_class)
{
  const gchar *resource;

  resource = "/org/gnome/panel/gp-properties-dialog.ui";

  gtk_widget_class_set_template_from_resource (widget_class, resource);

  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, toplevel_writable);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, theme_writable);

  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, orientation);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, alignment);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, size);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, expand);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, auto_hide);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, enable_buttons);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, enable_arrows);

  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, custom_bg_color);
  gtk_widget_class_bind_template_callback (widget_class, custom_bg_color_toggled_cb);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, bg_color_box);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, bg_color);

  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, custom_bg_image);
  gtk_widget_class_bind_template_callback (widget_class, custom_bg_image_toggled_cb);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, bg_image_box);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, bg_image);
  gtk_widget_class_bind_template_callback (widget_class, bg_image_file_set_cb);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, tile);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, stretch);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, fit);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, rotate);

  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, custom_fg_color);
  gtk_widget_class_bind_template_callback (widget_class, custom_fg_color_toggled_cb);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, fg_color_box);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, fg_color);

  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, applet_box);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, applet_box_left);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, applet_box_center);
  gtk_widget_class_bind_template_child (widget_class, GpPropertiesDialog, applet_box_right);
}

static void
gp_properties_dialog_class_init (GpPropertiesDialogClass *dialog_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (dialog_class);
  widget_class = GTK_WIDGET_CLASS (dialog_class);

  object_class->constructed = gp_properties_dialog_constructed;
  object_class->dispose = gp_properties_dialog_dispose;
  object_class->finalize = gp_properties_dialog_finalize;
  object_class->set_property = gp_properties_dialog_set_property;

  install_properties (object_class);
  bind_template (widget_class);
}

static void
gp_properties_dialog_init (GpPropertiesDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
}

GtkWidget *
gp_properties_dialog_new (const gchar *toplevel_id)
{
  return g_object_new (GP_TYPE_PROPERTIES_DIALOG,
                       "toplevel-id", toplevel_id,
                       NULL);
}

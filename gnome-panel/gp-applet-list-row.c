/*
 * Copyright (C) 2020 Alberts Muktupāvels
 * Copyright (C) 2021 Sebastian Geiger
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
#include "gp-applet-list-row.h"

#include <glib/gi18n.h>

#include "gp-applet-manager.h"
#include "panel-layout.h"
#include "panel-lockdown.h"

struct _GpAppletListRow
{
    GtkListBoxRow  parent;

    GpModule      *module;
    char          *applet_id;

    AppletInfo    *info;

    GtkWidget     *event_box;

    GtkWidget     *about_dialog;
};

enum
{
    PROP_0,

    PROP_MODULE,
    PROP_APPLET_ID,
    PROP_APPLET_INFO,

    LAST_PROP
};

static GParamSpec *row_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpAppletListRow, gp_applet_list_row, GTK_TYPE_LIST_BOX_ROW)

static void
help_cb (GtkMenuItem     *menuitem,
         GpAppletListRow *self)
{
  gp_module_show_help (self->module, NULL, self->applet_id, NULL);
}

static void
about_cb (GtkMenuItem     *menuitem,
          GpAppletListRow *self)
{
  if (self->about_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (self->about_dialog));
      return;
    }

  self->about_dialog = gp_module_create_about_dialog (self->module,
                                                      NULL,
                                                      self->applet_id);

  if (self->about_dialog == NULL)
    return;

  g_object_add_weak_pointer (G_OBJECT (self->about_dialog),
                             (gpointer *) &self->about_dialog);

  gtk_window_present (GTK_WINDOW (self->about_dialog));
}

static void
setup_view_more_button (GpAppletListRow *self,
                        GtkWidget       *button,
                        GpAppletInfo    *info)
{
  GtkWidget *image;
  GtkWidget *menu;
  gboolean sensitive;
  GtkWidget *item;

  image = gtk_image_new_from_icon_name ("view-more-symbolic",
                                        GTK_ICON_SIZE_MENU);

  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

  menu = gtk_menu_new ();
  sensitive = FALSE;

  gtk_menu_button_set_popup (GTK_MENU_BUTTON (button), menu);
  gtk_widget_set_halign (menu, GTK_ALIGN_END);

  if (info == NULL)
    {
      gtk_widget_set_sensitive (button, FALSE);
      return;
    }

  if (info->help_uri && info->help_uri[0] != '\0')
    {
      sensitive = TRUE;
      item = gtk_menu_item_new_with_label (_("Help"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      g_signal_connect (item, "activate", G_CALLBACK (help_cb), self);
    }

  if (info->about_dialog_func != NULL)
    {
      sensitive = TRUE;
      item = gtk_menu_item_new_with_label (_("About"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      g_signal_connect (item, "activate", G_CALLBACK (about_cb), self);
    }

  gtk_widget_set_sensitive (button, sensitive);
}

static void
lockdown_changed_cb (PanelLockdown *lockdown,
                     gpointer       user_data)
{
  GpAppletListRow *self;
  PanelWidget *panel;
  GpApplication *application;
  GpAppletManager *applet_manager;
  PanelLayout *layout;

  self = GP_APPLET_LIST_ROW (user_data);

  panel = panel_applet_get_panel_widget (self->info);
  application = panel_toplevel_get_application (panel->toplevel);
  applet_manager = gp_application_get_applet_manager (application);
  layout = gp_application_get_layout (application);
  lockdown = gp_application_get_lockdown (application);

  if (!panel_layout_is_writable (layout) ||
      panel_lockdown_get_panels_locked_down (lockdown) ||
      gp_applet_manager_is_applet_disabled (applet_manager,
                                            gp_module_get_id (self->module),
                                            self->applet_id,
                                            NULL))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
      return;
    }

  gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
}

static void
remove_clicked_cb (GtkButton       *button,
                   GpAppletListRow *self)
{
  GtkListBoxRow *row;

  row = GTK_LIST_BOX_ROW (self);

  GTK_LIST_BOX_ROW_GET_CLASS (row)->activate (row);
}

static GtkWidget *
error_label_new (const char *error)
{
  GtkWidget *label;
  GtkStyleContext *context;

  label = gtk_label_new (error);

  g_object_set (label, "margin", 6, NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (context, "error");

  return label;
}

static void
setup_row (GpAppletListRow *self)
{
  GError *error;
  GpAppletInfo *info;
  GtkWidget *hbox;
  GtkWidget *icon_image;
  GtkWidget *vbox;
  GtkWidget *remove_button;
  GtkWidget *menu_button;
  GtkStyleContext *remove_button_style_context;
  char *name;
  PanelWidget *panel;
  GpApplication *application;
  PanelLockdown *lockdown;

  error = NULL;
  info = gp_module_get_applet_info (self->module, self->applet_id, &error);

  self->event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (self), self->event_box);
  gtk_widget_show (self->event_box);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (self->event_box), hbox);
  gtk_widget_show (hbox);

  g_object_set (hbox,
                "margin-start", 6,
                "margin-end", 6,
                NULL);

  if (error != NULL)
    {
      icon_image = gtk_image_new_from_icon_name ("dialog-error",
                                                 GTK_ICON_SIZE_DND);
    }
  else
    {
      icon_image = gtk_image_new_from_icon_name (info->icon_name,
                                                 GTK_ICON_SIZE_DND);
    }

  gtk_image_set_pixel_size (GTK_IMAGE (icon_image), 32);
  gtk_box_pack_start (GTK_BOX (hbox), icon_image, FALSE, FALSE, 0);
  gtk_widget_show (icon_image);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  if (error != NULL)
    {
      GtkWidget *label;

      label = error_label_new (error->message);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_label_set_width_chars (GTK_LABEL (label), 30);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    }
  else
    {
      GtkWidget *title_label;
      GtkWidget *description_label;

      title_label = gtk_label_new (NULL);
      name = g_strdup_printf ("<b>%s</b>", info->name);
      gtk_label_set_markup (GTK_LABEL (title_label), name);
      g_free (name);

      gtk_box_pack_start (GTK_BOX (vbox), title_label, FALSE, FALSE, 0);
      gtk_label_set_xalign (GTK_LABEL (title_label), 0);
      gtk_widget_show (title_label);

      description_label = gtk_label_new (info->description);
      gtk_box_pack_start (GTK_BOX (vbox), description_label, FALSE, FALSE, 0);
      gtk_label_set_max_width_chars (GTK_LABEL (description_label), 20);
      gtk_label_set_line_wrap (GTK_LABEL (description_label), TRUE);
      gtk_label_set_xalign (GTK_LABEL (description_label), 0);
      gtk_widget_show (description_label);
    }

  remove_button = gtk_button_new_with_label (_("Remove"));
  gtk_box_pack_start (GTK_BOX (hbox), remove_button, FALSE, FALSE, 0);
  gtk_widget_set_valign (remove_button, GTK_ALIGN_CENTER);

  remove_button_style_context = gtk_widget_get_style_context (remove_button);
  gtk_style_context_add_class (remove_button_style_context,
                               GTK_STYLE_CLASS_DESTRUCTIVE_ACTION);

  gtk_widget_show (remove_button);

  g_signal_connect (remove_button, "clicked", G_CALLBACK (remove_clicked_cb), self);

  menu_button = gtk_menu_button_new ();
  gtk_box_pack_end (GTK_BOX (hbox), menu_button, FALSE, FALSE, 0);
  gtk_widget_set_valign (menu_button, GTK_ALIGN_CENTER);
  setup_view_more_button (self, menu_button, info);
  gtk_widget_show (menu_button);

  panel = panel_applet_get_panel_widget (self->info);
  application = panel_toplevel_get_application (panel->toplevel);
  lockdown = gp_application_get_lockdown (application);

  panel_lockdown_on_notify (lockdown,
                            NULL,
                            G_OBJECT (self),
                            lockdown_changed_cb,
                            self);

  lockdown_changed_cb (lockdown, self);

  g_clear_error (&error);
}

static void
gp_applet_list_row_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_applet_list_row_parent_class)->constructed (object);
  setup_row (GP_APPLET_LIST_ROW (object));
}

static void
gp_applet_list_row_dispose (GObject *object)
{
  GpAppletListRow *self;

  self = GP_APPLET_LIST_ROW (object);

  g_clear_object (&self->module);

  g_clear_pointer (&self->about_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (gp_applet_list_row_parent_class)->dispose (object);
}

static void
gp_applet_list_row_finalize (GObject *object)
{
  GpAppletListRow *self;

  self = GP_APPLET_LIST_ROW (object);

  g_clear_pointer (&self->applet_id, g_free);

  G_OBJECT_CLASS (gp_applet_list_row_parent_class)->finalize (object);
}

static void
gp_applet_list_row_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GpAppletListRow *self;

  self = GP_APPLET_LIST_ROW (object);

  switch (property_id)
    {
      case PROP_MODULE:
        g_assert (self->module == NULL);
        self->module = g_value_dup_object (value);
        break;

      case PROP_APPLET_ID:
        g_assert (self->applet_id == NULL);
        self->applet_id = g_value_dup_string (value);
        break;

      case PROP_APPLET_INFO:
        g_assert (self->info == NULL);
        self->info = g_value_get_pointer (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  row_properties[PROP_MODULE] =
      g_param_spec_object ("module",
                           "module",
                           "module",
                           GP_TYPE_MODULE,
                           G_PARAM_WRITABLE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

  row_properties[PROP_APPLET_ID] =
      g_param_spec_string ("applet-id",
                           "applet-id",
                           "applet-id",
                           NULL,
                           G_PARAM_WRITABLE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

  row_properties[PROP_APPLET_INFO] =
      g_param_spec_pointer("applet-info",
                           "applet-info",
                           "applet-info",
                           G_PARAM_WRITABLE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, row_properties);
}

static void
gp_applet_list_row_class_init (GpAppletListRowClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = gp_applet_list_row_constructed;
  object_class->dispose = gp_applet_list_row_dispose;
  object_class->finalize = gp_applet_list_row_finalize;
  object_class->set_property = gp_applet_list_row_set_property;

  install_properties (object_class);
}

static void
gp_applet_list_row_init (GpAppletListRow *self)
{
}

GtkWidget *
gp_applet_list_row_new (GpModule   *module,
                        const char *applet_id,
                        AppletInfo *info)
{
  return g_object_new (GP_TYPE_APPLET_LIST_ROW,
                       "module", module,
                       "applet-id", applet_id,
                       "applet-info", info,
                       NULL);
}

AppletInfo *
gp_applet_list_row_get_applet_info (GpAppletListRow *self)
{
  return self->info;
}

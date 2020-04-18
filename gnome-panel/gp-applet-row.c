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
#include "gp-applet-row.h"

#include <glib/gi18n.h>

#include "panel-applets-manager.h"
#include "panel-layout.h"
#include "panel-lockdown.h"

struct _GpAppletRow
{
  GtkListBoxRow  parent;

  GpModule      *module;
  char          *applet_id;

  char          *iid;

  GtkWidget     *event_box;

  GtkWidget     *about_dialog;
};

enum
{
  PROP_0,

  PROP_MODULE,
  PROP_APPLET_ID,

  LAST_PROP
};

static GParamSpec *row_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpAppletRow, gp_applet_row, GTK_TYPE_LIST_BOX_ROW)

static void
help_cb (GtkMenuItem *menuitem,
         GpAppletRow *self)
{
  gp_module_show_help (self->module, NULL, self->applet_id, NULL);
}

static void
about_cb (GtkMenuItem *menuitem,
          GpAppletRow *self)
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
setup_view_more_button (GpAppletRow  *self,
                        GtkWidget    *button,
                        GpAppletInfo *info)
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
drag_data_get_cb (GtkWidget        *widget,
                  GdkDragContext   *context,
                  GtkSelectionData *data,
                  guint             info,
                  guint             time,
                  GpAppletRow      *self)
{
  gtk_selection_data_set (data,
                          gtk_selection_data_get_target (data),
                          8,
                          (const guchar *) self->iid,
                          strlen (self->iid));
}

static void
setup_drag_source (GpAppletRow *self)
{
  GpAppletInfo *info;
  GdkModifierType modifiers;
  GdkDragAction actions;
  GtkTargetList *target_list;
  GdkAtom target;

  info = gp_module_get_applet_info (self->module, self->applet_id, NULL);

  modifiers = GDK_BUTTON1_MASK | GDK_BUTTON2_MASK;
  actions = GDK_ACTION_COPY;

  gtk_drag_source_set (self->event_box, modifiers, NULL, 0, actions);
  gtk_drag_source_set_icon_name (self->event_box, info->icon_name);

  target_list = gtk_target_list_new (NULL, 0);

  target = gdk_atom_intern_static_string ("application/x-panel-applet-iid");
  gtk_target_list_add (target_list, target, 0, 0);

  gtk_drag_source_set_target_list (self->event_box, target_list);
  gtk_target_list_unref (target_list);

  g_signal_connect (self->event_box,
                    "drag-data-get",
                    G_CALLBACK (drag_data_get_cb),
                    self);
}

static void
lockdown_changed_cb (PanelLockdown *lockdown,
                     gpointer       user_data)
{
  GpAppletRow *self;

  self = GP_APPLET_ROW (user_data);

  if (!panel_layout_is_writable () ||
      panel_lockdown_get_panels_locked_down_s () ||
      panel_applets_manager_is_applet_disabled (self->iid, NULL))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
      gtk_drag_source_unset (self->event_box);
      return;
    }

  gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
  setup_drag_source (self);
}

static void
add_clicked_cb (GtkButton   *button,
                GpAppletRow *self)
{
  GtkListBoxRow *row;

  row = GTK_LIST_BOX_ROW (self);

  GTK_LIST_BOX_ROW_GET_CLASS (row)->activate (row);
}

static void
setup_row (GpAppletRow *self)
{
  GpAppletInfo *info;
  GtkWidget *hbox;
  GtkWidget *icon_image;
  GtkWidget *vbox;
  GtkWidget *add_button;
  GtkWidget *menu_button;
  GtkWidget *title_label;
  GtkWidget *description_label;

  info = gp_module_get_applet_info (self->module, self->applet_id, NULL);
  g_assert (info != NULL);

  self->iid = g_strdup_printf ("%s::%s",
                               gp_module_get_id (self->module),
                               self->applet_id);

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

  icon_image = gtk_image_new_from_icon_name (info->icon_name, GTK_ICON_SIZE_DND);
  gtk_image_set_pixel_size (GTK_IMAGE (icon_image), 32);
  gtk_box_pack_start (GTK_BOX (hbox), icon_image, FALSE, FALSE, 0);
  gtk_widget_show (icon_image);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  add_button = gtk_button_new_with_label (_("Add"));
  gtk_box_pack_start (GTK_BOX (hbox), add_button, FALSE, FALSE, 0);
  gtk_widget_set_valign (add_button, GTK_ALIGN_CENTER);
  gtk_widget_show (add_button);

  g_signal_connect (add_button, "clicked", G_CALLBACK (add_clicked_cb), self);

  menu_button = gtk_menu_button_new ();
  gtk_box_pack_end (GTK_BOX (hbox), menu_button, FALSE, FALSE, 0);
  gtk_widget_set_valign (menu_button, GTK_ALIGN_CENTER);
  setup_view_more_button (self, menu_button, info);
  gtk_widget_show (menu_button);

  title_label = gtk_label_new (info->name);
  gtk_box_pack_start (GTK_BOX (vbox), title_label, FALSE, FALSE, 0);
  gtk_label_set_xalign (GTK_LABEL (title_label), 0);
  label_make_bold (title_label);
  gtk_widget_show (title_label);

  description_label = gtk_label_new (info->description);
  gtk_box_pack_start (GTK_BOX (vbox), description_label, FALSE, FALSE, 0);
  gtk_label_set_max_width_chars (GTK_LABEL (description_label), 20);
  gtk_label_set_line_wrap (GTK_LABEL (description_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (description_label), 0);
  gtk_widget_show (description_label);

  panel_lockdown_on_notify (panel_lockdown_get (),
                            NULL,
                            G_OBJECT (self),
                            lockdown_changed_cb,
                            self);

  lockdown_changed_cb (panel_lockdown_get (), self);
}

static void
gp_applet_row_constructed (GObject *object)
{
  G_OBJECT_CLASS (gp_applet_row_parent_class)->constructed (object);
  setup_row (GP_APPLET_ROW (object));
}

static void
gp_applet_row_dispose (GObject *object)
{
  GpAppletRow *self;

  self = GP_APPLET_ROW (object);

  g_clear_object (&self->module);

  g_clear_pointer (&self->about_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (gp_applet_row_parent_class)->dispose (object);
}

static void
gp_applet_row_finalize (GObject *object)
{
  GpAppletRow *self;

  self = GP_APPLET_ROW (object);

  g_clear_pointer (&self->applet_id, g_free);

  G_OBJECT_CLASS (gp_applet_row_parent_class)->finalize (object);
}

static void
gp_applet_row_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GpAppletRow *self;

  self = GP_APPLET_ROW (object);

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

  g_object_class_install_properties (object_class, LAST_PROP, row_properties);
}

static void
gp_applet_row_class_init (GpAppletRowClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = gp_applet_row_constructed;
  object_class->dispose = gp_applet_row_dispose;
  object_class->finalize = gp_applet_row_finalize;
  object_class->set_property = gp_applet_row_set_property;

  install_properties (object_class);
}

static void
gp_applet_row_init (GpAppletRow *self)
{
}

GtkWidget *
gp_applet_row_new (GpModule   *module,
                   const char *applet_id)
{
  return g_object_new (GP_TYPE_APPLET_ROW,
                       "module", module,
                       "applet-id", applet_id,
                       NULL);
}

GpAppletInfo *
gp_applet_row_get_info (GpAppletRow *self)
{
  return gp_module_get_applet_info (self->module, self->applet_id, NULL);
}

const char *
gp_applet_row_get_iid (GpAppletRow *self)
{
  return self->iid;
}

/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Vincent Untz
 * Copyright (C) 2018 Alberts Muktupāvels
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
 *     Mark McLoughlin <mark@skynet.ie>
 *     Vincent Untz <vincent@vuntz.net>
 */

#include "config.h"

#include <libgnome-panel/gp-utils.h>

#include "gp-menu-bar.h"

struct _GpMenuBar
{
  GtkMenuBar      parent;

  gboolean        enable_tooltips;
  GtkPositionType position;

  gdouble         label_angle;
  gfloat          label_xalign;
  gfloat          label_yalign;
};

enum
{
  PROP_0,

  PROP_ENABLE_TOOLTIPS,
  PROP_POSITION,

  LAST_PROP
};

static GParamSpec *menu_bar_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GpMenuBar, gp_menu_bar, GTK_TYPE_MENU_BAR)

static void
activate_cb (GtkMenuItem *menu_item,
             gpointer     user_data)
{
  GtkWidget *toplevel;

  gtk_widget_set_has_tooltip (GTK_WIDGET (menu_item), FALSE);

  /* Remove focus that would be drawn on the currently focused
   * child of the toplevel. See bug #308632.
   */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menu_item));
  if (gtk_widget_is_toplevel (toplevel))
    gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
}

static void
update_label (GtkWidget *widget,
              GpMenuBar *bar)
{
  GtkWidget *child;
  GtkLabel *label;
  PangoLayout *layout;
  PangoContext *context;

  if (!GTK_IS_MENU_ITEM (widget))
    return;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (!GTK_IS_LABEL (child))
    return;

  label = GTK_LABEL (child);

  gtk_label_set_angle (label, bar->label_angle);

  gtk_label_set_xalign (label, bar->label_xalign);
  gtk_label_set_yalign (label, bar->label_yalign);

  layout = gtk_label_get_layout (label);
  context = pango_layout_get_context (layout);

  pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
}

static void
set_position (GpMenuBar       *bar,
              GtkPositionType  position)
{
  GtkPackDirection pack_direction;

  if (bar->position == position)
    return;

  bar->position = position;

  switch (position)
    {
      case GTK_POS_LEFT:
        pack_direction = GTK_PACK_DIRECTION_BTT;

        bar->label_angle = 90.0;
        bar->label_xalign = 0.5;
        bar->label_yalign = 0.0;
        break;

      case GTK_POS_RIGHT:
        pack_direction = GTK_PACK_DIRECTION_TTB;

        bar->label_angle = 270.0;
        bar->label_xalign = 0.5;
        bar->label_yalign = 0.0;
        break;

      case GTK_POS_TOP:
      case GTK_POS_BOTTOM:
      default:
        pack_direction = GTK_PACK_DIRECTION_LTR;

        bar->label_angle = 0.0;
        bar->label_xalign = 0.0;
        bar->label_yalign = 0.5;
        break;
    }

  gtk_menu_bar_set_pack_direction (GTK_MENU_BAR (bar), pack_direction);
  gtk_menu_bar_set_child_pack_direction (GTK_MENU_BAR (bar), pack_direction);

  gtk_container_foreach (GTK_CONTAINER (bar), (GtkCallback) update_label, bar);
}

static void
gp_menu_bar_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GpMenuBar *bar;

  bar = GP_MENU_BAR (object);

  switch (property_id)
    {
      case PROP_ENABLE_TOOLTIPS:
        g_value_set_boolean (value, bar->enable_tooltips);
        break;

      case PROP_POSITION:
        g_value_set_enum (value, bar->position);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gp_menu_bar_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GpMenuBar *bar;

  bar = GP_MENU_BAR (object);

  switch (property_id)
    {
      case PROP_ENABLE_TOOLTIPS:
        bar->enable_tooltips = g_value_get_boolean (value);
        break;

      case PROP_POSITION:
        set_position (bar, g_value_get_enum (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  menu_bar_properties[PROP_ENABLE_TOOLTIPS] =
    g_param_spec_boolean ("enable-tooltips", "Enable Tooltips", "Enable Tooltips",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);

  menu_bar_properties[PROP_POSITION] =
    g_param_spec_enum ("position", "Position", "Position",
                       GTK_TYPE_POSITION_TYPE, GTK_POS_TOP,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     menu_bar_properties);
}

static gboolean
gp_menu_bar_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  gboolean ret;
  GtkStyleContext *context;
  gdouble width;
  gdouble height;

  ret = GTK_WIDGET_CLASS (gp_menu_bar_parent_class)->draw (widget, cr);

  if (!gtk_widget_has_focus (widget))
    return ret;

  context = gtk_widget_get_style_context (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_focus (context, cr, 0, 0, width, height);

  return ret;
}

static void
set_has_tooltip (GtkWidget *widget,
                 gpointer   user_data)
{
  gtk_widget_set_has_tooltip (widget, TRUE);
}

static void
gp_menu_bar_remove (GtkContainer *container,
                    GtkWidget    *widget)
{
  GBinding *binding;
  gpointer signal_id;

  binding = g_object_steal_data (G_OBJECT (widget), "binding");
  g_assert (binding != NULL);

  g_binding_unbind (binding);

  signal_id = g_object_steal_data (G_OBJECT (widget), "signal-id");
  g_assert (signal_id != NULL);

  g_signal_handler_disconnect (widget, GPOINTER_TO_SIZE (signal_id));

  GTK_CONTAINER_CLASS (gp_menu_bar_parent_class)->remove (container, widget);
}

static void
gp_menu_bar_deactivate (GtkMenuShell *menu_shell)
{
  GpMenuBar *bar;

  bar = GP_MENU_BAR (menu_shell);

  GTK_MENU_SHELL_CLASS (gp_menu_bar_parent_class)->deactivate (menu_shell);

  if (!bar->enable_tooltips)
    return;

  gtk_container_foreach (GTK_CONTAINER (menu_shell), set_has_tooltip, NULL);
}

static void
gp_menu_bar_insert (GtkMenuShell *menu_shell,
                    GtkWidget    *child,
                    gint          position)
{
  GBinding *binding;
  gulong signal_id;

  GTK_MENU_SHELL_CLASS (gp_menu_bar_parent_class)->insert (menu_shell,
                                                           child,
                                                           position);

  binding = g_object_bind_property (menu_shell, "enable-tooltips",
                                    child, "has-tooltip",
                                    G_BINDING_DEFAULT |
                                    G_BINDING_SYNC_CREATE);

  signal_id = g_signal_connect (child, "activate",
                                G_CALLBACK (activate_cb), NULL);

  g_object_set_data (G_OBJECT (child), "binding", binding);
  g_object_set_data (G_OBJECT (child), "signal-id",
                     GSIZE_TO_POINTER (signal_id));

  update_label (child, GP_MENU_BAR (menu_shell));
  gp_add_text_color_class (child);
}

static void
gp_menu_bar_class_init (GpMenuBarClass *bar_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkMenuShellClass *menu_shell_class;

  object_class = G_OBJECT_CLASS (bar_class);
  widget_class = GTK_WIDGET_CLASS (bar_class);
  container_class = GTK_CONTAINER_CLASS (bar_class);
  menu_shell_class = GTK_MENU_SHELL_CLASS (bar_class);

  object_class->get_property = gp_menu_bar_get_property;
  object_class->set_property = gp_menu_bar_set_property;

  widget_class->draw = gp_menu_bar_draw;

  container_class->remove = gp_menu_bar_remove;

  menu_shell_class->deactivate = gp_menu_bar_deactivate;
  menu_shell_class->insert = gp_menu_bar_insert;

  install_properties (object_class);
}

static void
gp_menu_bar_init (GpMenuBar *bar)
{
  gtk_widget_set_can_focus (GTK_WIDGET (bar), TRUE);

  bar->label_angle = 0.0;
  bar->label_xalign = 0.0;
  bar->label_yalign = 0.5;
}

GtkWidget *
gp_menu_bar_new (void)
{
  return g_object_new (GP_TYPE_MENU_BAR, NULL);
}

/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * This is modified version of gtkimagemenuitem.c copied from GTK+
 * 3.8.9 release:
 * https://git.gnome.org/browse/gtk+/plain/gtk/gtkimagemenuitem.c?id=3.8.9
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "panel-image-menu-item.h"

struct _PanelImageMenuItemPrivate {
  GtkWidget *image;

  gchar     *label;
  guint      always_show_image : 1;
};

enum {
  PROP_0,
  PROP_IMAGE,
  PROP_ALWAYS_SHOW_IMAGE
};

static void panel_image_menu_item_destroy              (GtkWidget        *widget);
static void panel_image_menu_item_get_preferred_width  (GtkWidget        *widget,
                                                        gint             *minimum,
                                                        gint             *natural);
static void panel_image_menu_item_get_preferred_height (GtkWidget        *widget,
                                                        gint             *minimum,
                                                        gint             *natural);
static void panel_image_menu_item_get_preferred_height_for_width (GtkWidget *widget,
                                                                  gint       width,
                                                                  gint      *minimum,
                                                                  gint      *natural);
static void panel_image_menu_item_size_allocate        (GtkWidget        *widget,
                                                        GtkAllocation    *allocation);
static void panel_image_menu_item_map                  (GtkWidget        *widget);
static void panel_image_menu_item_remove               (GtkContainer     *container,
                                                        GtkWidget        *child);
static void panel_image_menu_item_toggle_size_request  (GtkMenuItem      *menu_item,
                                                        gint             *requisition);
static void panel_image_menu_item_set_label            (GtkMenuItem      *menu_item,
                                                        const gchar      *label);
static const gchar * panel_image_menu_item_get_label   (GtkMenuItem *menu_item);

static void panel_image_menu_item_forall               (GtkContainer    *container,
                                                        gboolean         include_internals,
                                                        GtkCallback      callback,
                                                        gpointer         callback_data);

static void panel_image_menu_item_finalize             (GObject         *object);
static void panel_image_menu_item_set_property         (GObject         *object,
                                                        guint            prop_id,
                                                        const GValue    *value,
                                                        GParamSpec      *pspec);
static void panel_image_menu_item_get_property         (GObject         *object,
                                                        guint            prop_id,
                                                        GValue          *value,
                                                        GParamSpec      *pspec);
static void panel_image_menu_item_screen_changed       (GtkWidget        *widget,
                                                        GdkScreen        *previous_screen);

static void panel_image_menu_item_recalculate          (PanelImageMenuItem *image_menu_item);

G_DEFINE_TYPE_WITH_PRIVATE (PanelImageMenuItem, panel_image_menu_item, GTK_TYPE_MENU_ITEM)

static void
panel_image_menu_item_class_init (PanelImageMenuItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass*) klass;
  GtkMenuItemClass *menu_item_class = (GtkMenuItemClass*) klass;
  GtkContainerClass *container_class = (GtkContainerClass*) klass;

  widget_class->destroy = panel_image_menu_item_destroy;
  widget_class->screen_changed = panel_image_menu_item_screen_changed;
  widget_class->get_preferred_width = panel_image_menu_item_get_preferred_width;
  widget_class->get_preferred_height = panel_image_menu_item_get_preferred_height;
  widget_class->get_preferred_height_for_width = panel_image_menu_item_get_preferred_height_for_width;
  widget_class->size_allocate = panel_image_menu_item_size_allocate;
  widget_class->map = panel_image_menu_item_map;

  container_class->forall = panel_image_menu_item_forall;
  container_class->remove = panel_image_menu_item_remove;

  menu_item_class->toggle_size_request = panel_image_menu_item_toggle_size_request;
  menu_item_class->set_label           = panel_image_menu_item_set_label;
  menu_item_class->get_label           = panel_image_menu_item_get_label;

  gobject_class->finalize     = panel_image_menu_item_finalize;
  gobject_class->set_property = panel_image_menu_item_set_property;
  gobject_class->get_property = panel_image_menu_item_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_IMAGE,
                                   g_param_spec_object ("image",
                                                        _("Image widget"),
                                                        _("Child widget to appear next to the menu text"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ALWAYS_SHOW_IMAGE,
                                   g_param_spec_boolean ("always-show-image",
                                                         _("Always show image"),
                                                         _("Whether the image will always be shown"),
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
panel_image_menu_item_init (PanelImageMenuItem *image_menu_item)
{
  PanelImageMenuItemPrivate *priv;

  image_menu_item->priv = G_TYPE_INSTANCE_GET_PRIVATE (image_menu_item,
                                                       PANEL_TYPE_IMAGE_MENU_ITEM,
                                                       PanelImageMenuItemPrivate);
  priv = image_menu_item->priv;

  priv->image = NULL;
  priv->label  = NULL;
}

static void
panel_image_menu_item_finalize (GObject *object)
{
  PanelImageMenuItemPrivate *priv = PANEL_IMAGE_MENU_ITEM (object)->priv;

  g_free (priv->label);
  priv->label  = NULL;

  G_OBJECT_CLASS (panel_image_menu_item_parent_class)->finalize (object);
}

static void
panel_image_menu_item_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_IMAGE:
      panel_image_menu_item_set_image (image_menu_item, (GtkWidget *) g_value_get_object (value));
      break;
    case PROP_ALWAYS_SHOW_IMAGE:
      panel_image_menu_item_set_always_show_image (image_menu_item, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
panel_image_menu_item_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_IMAGE:
      g_value_set_object (value, panel_image_menu_item_get_image (image_menu_item));
      break;
    case PROP_ALWAYS_SHOW_IMAGE:
      g_value_set_boolean (value, panel_image_menu_item_get_always_show_image (image_menu_item));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
show_image (PanelImageMenuItem *image_menu_item)
{
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;

  if (priv->always_show_image)
    return TRUE;

  return FALSE;
}

static void
panel_image_menu_item_map (GtkWidget *widget)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (widget);
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;

  GTK_WIDGET_CLASS (panel_image_menu_item_parent_class)->map (widget);

  if (priv->image)
    g_object_set (priv->image,
                  "visible", show_image (image_menu_item),
                  NULL);
}

static void
panel_image_menu_item_destroy (GtkWidget *widget)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (widget);
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;

  if (priv->image)
    gtk_container_remove (GTK_CONTAINER (image_menu_item),
                          priv->image);

  GTK_WIDGET_CLASS (panel_image_menu_item_parent_class)->destroy (widget);
}

static void
panel_image_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                           gint        *requisition)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (menu_item);
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;
  GtkPackDirection pack_dir;
  GtkWidget *parent;
  GtkWidget *widget = GTK_WIDGET (menu_item);

  parent = gtk_widget_get_parent (widget);

  if (GTK_IS_MENU_BAR (parent))
    pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));
  else
    pack_dir = GTK_PACK_DIRECTION_LTR;

  *requisition = 0;

  if (priv->image && gtk_widget_get_visible (priv->image))
    {
      GtkRequisition image_requisition;
      guint toggle_spacing;

      gtk_widget_get_preferred_size (priv->image, &image_requisition, NULL);

      gtk_widget_style_get (GTK_WIDGET (menu_item),
                            "toggle-spacing", &toggle_spacing,
                            NULL);

      if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
        {
          if (image_requisition.width > 0)
            *requisition = image_requisition.width + toggle_spacing;
        }
      else
        {
          if (image_requisition.height > 0)
            *requisition = image_requisition.height + toggle_spacing;
        }
    }
}

static void
panel_image_menu_item_recalculate (PanelImageMenuItem *image_menu_item)
{
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;
  const gchar               *resolved_label = priv->label;

  if (priv->label)
    gtk_menu_item_set_use_underline (GTK_MENU_ITEM (image_menu_item), TRUE);

  GTK_MENU_ITEM_CLASS (panel_image_menu_item_parent_class)->set_label (GTK_MENU_ITEM (image_menu_item), resolved_label);
}

static void
panel_image_menu_item_set_label (GtkMenuItem *menu_item,
                                 const gchar *label)
{
  PanelImageMenuItemPrivate *priv = PANEL_IMAGE_MENU_ITEM (menu_item)->priv;

  if (priv->label != label)
    {
      g_free (priv->label);
      priv->label = g_strdup (label);

      panel_image_menu_item_recalculate (PANEL_IMAGE_MENU_ITEM (menu_item));

      g_object_notify (G_OBJECT (menu_item), "label");
    }
}

static const gchar *
panel_image_menu_item_get_label (GtkMenuItem *menu_item)
{
  PanelImageMenuItemPrivate *priv = PANEL_IMAGE_MENU_ITEM (menu_item)->priv;

  return priv->label;
}

static void
panel_image_menu_item_get_preferred_width (GtkWidget *widget,
                                           gint      *minimum,
                                           gint      *natural)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (widget);
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;
  GtkPackDirection pack_dir;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);

  if (GTK_IS_MENU_BAR (parent))
    pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));
  else
    pack_dir = GTK_PACK_DIRECTION_LTR;

  GTK_WIDGET_CLASS (panel_image_menu_item_parent_class)->get_preferred_width (widget, minimum, natural);

  if ((pack_dir == GTK_PACK_DIRECTION_TTB || pack_dir == GTK_PACK_DIRECTION_BTT) &&
      priv->image &&
      gtk_widget_get_visible (priv->image))
    {
      gint child_minimum, child_natural;

      gtk_widget_get_preferred_width (priv->image, &child_minimum, &child_natural);

      *minimum = MAX (*minimum, child_minimum);
      *natural = MAX (*natural, child_natural);
    }
}

static void
panel_image_menu_item_get_preferred_height (GtkWidget *widget,
                                            gint      *minimum,
                                            gint      *natural)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (widget);
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;
  gint child_height = 0;
  GtkPackDirection pack_dir;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);

  if (GTK_IS_MENU_BAR (parent))
    pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));
  else
    pack_dir = GTK_PACK_DIRECTION_LTR;

  if (priv->image && gtk_widget_get_visible (priv->image))
    {
      GtkRequisition child_requisition;

      gtk_widget_get_preferred_size (priv->image, &child_requisition, NULL);

      child_height = child_requisition.height;
    }

  GTK_WIDGET_CLASS (panel_image_menu_item_parent_class)->get_preferred_height (widget, minimum, natural);

  if (pack_dir == GTK_PACK_DIRECTION_RTL || pack_dir == GTK_PACK_DIRECTION_LTR)
    {
      *minimum = MAX (*minimum, child_height);
      *natural = MAX (*natural, child_height);
    }
}

static void
panel_image_menu_item_get_preferred_height_for_width (GtkWidget *widget,
                                                      gint       width,
                                                      gint      *minimum,
                                                      gint      *natural)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (widget);
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;
  gint child_height = 0;
  GtkPackDirection pack_dir;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);

  if (GTK_IS_MENU_BAR (parent))
    pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));
  else
    pack_dir = GTK_PACK_DIRECTION_LTR;

  if (priv->image && gtk_widget_get_visible (priv->image))
    {
      GtkRequisition child_requisition;

      gtk_widget_get_preferred_size (priv->image, &child_requisition, NULL);

      child_height = child_requisition.height;
    }

  GTK_WIDGET_CLASS (panel_image_menu_item_parent_class)->get_preferred_height_for_width (widget, width, minimum, natural);

  if (pack_dir == GTK_PACK_DIRECTION_RTL || pack_dir == GTK_PACK_DIRECTION_LTR)
    {
      *minimum = MAX (*minimum, child_height);
      *natural = MAX (*natural, child_height);
    }
}

static void
panel_image_menu_item_size_allocate (GtkWidget     *widget,
                                     GtkAllocation *allocation)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (widget);
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;
  GtkAllocation widget_allocation;
  GtkPackDirection pack_dir;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);

  if (GTK_IS_MENU_BAR (parent))
    pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));
  else
    pack_dir = GTK_PACK_DIRECTION_LTR;

  GTK_WIDGET_CLASS (panel_image_menu_item_parent_class)->size_allocate (widget, allocation);

  if (priv->image && gtk_widget_get_visible (priv->image))
    {
      gint x, y, offset;
      GtkStyleContext *context;
      GtkStateFlags state;
      GtkBorder padding;
      GtkRequisition child_requisition;
      GtkAllocation child_allocation;
      guint horizontal_padding, toggle_spacing;
      gint toggle_size = 0;

      panel_image_menu_item_toggle_size_request (GTK_MENU_ITEM (image_menu_item), &toggle_size);

      gtk_widget_style_get (widget,
                            "horizontal-padding", &horizontal_padding,
                            "toggle-spacing", &toggle_spacing,
                            NULL);

      /* Man this is lame hardcoding action, but I can't
       * come up with a solution that's really better.
       */

      gtk_widget_get_preferred_size (priv->image, &child_requisition, NULL);

      gtk_widget_get_allocation (widget, &widget_allocation);

      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_padding (context, state, &padding);
      offset = gtk_container_get_border_width (GTK_CONTAINER (image_menu_item));

      if (pack_dir == GTK_PACK_DIRECTION_LTR ||
          pack_dir == GTK_PACK_DIRECTION_RTL)
        {
          if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) ==
              (pack_dir == GTK_PACK_DIRECTION_LTR))
            x = offset + horizontal_padding + padding.left +
               (toggle_size - toggle_spacing - child_requisition.width) / 2;
          else
            x = widget_allocation.width - offset - horizontal_padding - padding.right -
              toggle_size + toggle_spacing +
              (toggle_size - toggle_spacing - child_requisition.width) / 2;

          y = (widget_allocation.height - child_requisition.height) / 2;
        }
      else
        {
          if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) ==
              (pack_dir == GTK_PACK_DIRECTION_TTB))
            y = offset + horizontal_padding + padding.top +
              (toggle_size - toggle_spacing - child_requisition.height) / 2;
          else
            y = widget_allocation.height - offset - horizontal_padding - padding.bottom -
              toggle_size + toggle_spacing +
              (toggle_size - toggle_spacing - child_requisition.height) / 2;

          x = (widget_allocation.width - child_requisition.width) / 2;
        }

      child_allocation.width = child_requisition.width;
      child_allocation.height = child_requisition.height;
      child_allocation.x = widget_allocation.x + MAX (x, 0);
      child_allocation.y = widget_allocation.y + MAX (y, 0);

      gtk_widget_size_allocate (priv->image, &child_allocation);
    }
}

static void
panel_image_menu_item_forall (GtkContainer   *container,
                              gboolean        include_internals,
                              GtkCallback     callback,
                              gpointer        callback_data)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (container);
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;

  GTK_CONTAINER_CLASS (panel_image_menu_item_parent_class)->forall (container,
                                                                    include_internals,
                                                                    callback,
                                                                    callback_data);

  if (include_internals && priv->image)
    (* callback) (priv->image, callback_data);
}

GtkWidget*
panel_image_menu_item_new (void)
{
  return g_object_new (PANEL_TYPE_IMAGE_MENU_ITEM, NULL);
}

GtkWidget*
panel_image_menu_item_new_with_label (const gchar *label)
{
  return g_object_new (PANEL_TYPE_IMAGE_MENU_ITEM,
                       "label", label,
                       NULL);
}

GtkWidget*
panel_image_menu_item_new_with_mnemonic (const gchar *label)
{
  return g_object_new (PANEL_TYPE_IMAGE_MENU_ITEM,
                       "use-underline", TRUE,
                       "label", label,
                       NULL);
}

void
panel_image_menu_item_set_always_show_image (PanelImageMenuItem *image_menu_item,
                                             gboolean            always_show)
{
  PanelImageMenuItemPrivate *priv;

  g_return_if_fail (PANEL_IS_IMAGE_MENU_ITEM (image_menu_item));

  priv = image_menu_item->priv;

  if (priv->always_show_image != always_show)
    {
      priv->always_show_image = always_show;

      if (priv->image)
        {
          if (show_image (image_menu_item))
            gtk_widget_show (priv->image);
          else
            gtk_widget_hide (priv->image);
        }

      g_object_notify (G_OBJECT (image_menu_item), "always-show-image");
    }
}

gboolean
panel_image_menu_item_get_always_show_image (PanelImageMenuItem *image_menu_item)
{
  g_return_val_if_fail (PANEL_IS_IMAGE_MENU_ITEM (image_menu_item), FALSE);

  return image_menu_item->priv->always_show_image;
}

void
panel_image_menu_item_set_image (PanelImageMenuItem *image_menu_item,
                                 GtkWidget          *image)
{
  PanelImageMenuItemPrivate *priv;

  g_return_if_fail (PANEL_IS_IMAGE_MENU_ITEM (image_menu_item));

  priv = image_menu_item->priv;

  if (image == priv->image)
    return;

  if (priv->image)
    gtk_container_remove (GTK_CONTAINER (image_menu_item),
                          priv->image);

  priv->image = image;

  if (image == NULL)
    return;

  gtk_widget_set_parent (image, GTK_WIDGET (image_menu_item));
  g_object_set (image,
                "visible", show_image (image_menu_item),
                "no-show-all", TRUE,
                NULL);

  g_object_notify (G_OBJECT (image_menu_item), "image");
}

GtkWidget*
panel_image_menu_item_get_image (PanelImageMenuItem *image_menu_item)
{
  g_return_val_if_fail (PANEL_IS_IMAGE_MENU_ITEM (image_menu_item), NULL);

  return image_menu_item->priv->image;
}

static void
panel_image_menu_item_remove (GtkContainer *container,
                              GtkWidget    *child)
{
  PanelImageMenuItem *image_menu_item = PANEL_IMAGE_MENU_ITEM (container);
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;

  if (child == priv->image)
    {
      gboolean widget_was_visible;

      widget_was_visible = gtk_widget_get_visible (child);

      gtk_widget_unparent (child);
      priv->image = NULL;

      if (widget_was_visible &&
          gtk_widget_get_visible (GTK_WIDGET (container)))
        gtk_widget_queue_resize (GTK_WIDGET (container));

      g_object_notify (G_OBJECT (image_menu_item), "image");
    }
  else
    {
      GTK_CONTAINER_CLASS (panel_image_menu_item_parent_class)->remove (container, child);
    }
}

static void
show_image_change_notify (PanelImageMenuItem *image_menu_item)
{
  PanelImageMenuItemPrivate *priv = image_menu_item->priv;

  if (priv->image)
    {
      if (show_image (image_menu_item))
        gtk_widget_show (priv->image);
      else
        gtk_widget_hide (priv->image);
    }
}

static void
panel_image_menu_item_screen_changed (GtkWidget *widget,
                                      GdkScreen *previous_screen)
{
  if (!gtk_widget_has_screen (widget))
    return;

  show_image_change_notify (PANEL_IMAGE_MENU_ITEM (widget));
}

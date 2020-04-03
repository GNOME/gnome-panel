/*
 * Copyright (C) 2018-2020 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

/**
 * SECTION: gp-image-menu-item
 * @title: GpImageMenuItem
 * @short_description: a widget for a menu item with an icon
 * @include: libgnome-panel/gp-image-menu-item.h
 *
 * A #GpImageMenuItem is a menu item which has an icon next to the text label.
 */

/**
 * GpImageMenuItem:
 *
 * #GpImageMenuItem is an opaque data structure and can only be accessed using
 * the following functions.
 */

#include "config.h"
#include "gp-image-menu-item.h"

typedef struct
{
  GtkWidget *image;
} GpImageMenuItemPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GpImageMenuItem,
                            gp_image_menu_item,
                            GTK_TYPE_MENU_ITEM)

static void
update_css_class (GpImageMenuItem *self)
{
  GpImageMenuItemPrivate *priv;
  gboolean image_only;
  GtkWidget *child;
  GtkStyleContext *context;

  priv = gp_image_menu_item_get_instance_private (self);
  image_only = TRUE;

  if (priv->image == NULL)
    image_only = FALSE;

  child = gtk_bin_get_child (GTK_BIN (self));

  if (child != NULL && GTK_IS_LABEL (child))
    {
      const char *text;

      text = gtk_label_get_text (GTK_LABEL (child));
      if (text != NULL && *text != '\0')
        image_only = FALSE;
    }
  else if (child != NULL)
    {
      image_only = FALSE;
    }

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (!image_only)
    gtk_style_context_remove_class (context, "image-only");
  else
    gtk_style_context_add_class (context, "image-only");
}

static void
notify_label_cb (GpImageMenuItem *self,
                 GParamSpec      *pspec,
                 gpointer         user_data)
{
  update_css_class (self);
}

static GtkPackDirection
get_pack_direction (GpImageMenuItem *self)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (self));

  if (GTK_IS_MENU_BAR (parent))
    return gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));

  return GTK_PACK_DIRECTION_LTR;
}

static void
gp_image_menu_item_destroy (GtkWidget *widget)
{
  GpImageMenuItem *self;
  GpImageMenuItemPrivate *priv;

  self = GP_IMAGE_MENU_ITEM (widget);
  priv = gp_image_menu_item_get_instance_private (self);

  if (priv->image)
    gtk_container_remove (GTK_CONTAINER (self), priv->image);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->destroy (widget);
}

static void
gp_image_menu_item_get_preferred_height (GtkWidget *widget,
                                         gint      *minimum_height,
                                         gint      *natural_height)
{
  GpImageMenuItem *self;
  GpImageMenuItemPrivate *priv;
  GtkPackDirection pack_dir;

  self = GP_IMAGE_MENU_ITEM (widget);
  priv = gp_image_menu_item_get_instance_private (self);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->get_preferred_height (widget,
                                                                            minimum_height,
                                                                            natural_height);

  if (!priv->image || !gtk_widget_get_visible (priv->image))
    return;

  pack_dir = get_pack_direction (self);
  if (pack_dir == GTK_PACK_DIRECTION_RTL || pack_dir == GTK_PACK_DIRECTION_LTR)
    {
      GtkRequisition image_requisition;

      gtk_widget_get_preferred_size (priv->image, &image_requisition, NULL);

      *minimum_height = MAX (*minimum_height, image_requisition.height);
      *natural_height = MAX (*natural_height, image_requisition.height);
    }
}

static void
gp_image_menu_item_get_preferred_height_for_width (GtkWidget *widget,
                                                   gint       width,
                                                   gint      *minimum_height,
                                                   gint      *natural_height)
{
  GpImageMenuItem *self;
  GpImageMenuItemPrivate *priv;
  GtkPackDirection pack_dir;

  self = GP_IMAGE_MENU_ITEM (widget);
  priv = gp_image_menu_item_get_instance_private (self);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->get_preferred_height_for_width (widget,
                                                                                      width,
                                                                                      minimum_height,
                                                                                      natural_height);

  if (!priv->image || !gtk_widget_get_visible (priv->image))
    return;

  pack_dir = get_pack_direction (self);
  if (pack_dir == GTK_PACK_DIRECTION_RTL || pack_dir == GTK_PACK_DIRECTION_LTR)
    {
      GtkRequisition image_requisition;

      gtk_widget_get_preferred_size (priv->image, &image_requisition, NULL);

      *minimum_height = MAX (*minimum_height, image_requisition.height);
      *natural_height = MAX (*natural_height, image_requisition.height);
    }
}

static void
gp_image_menu_item_get_preferred_width (GtkWidget *widget,
                                        gint      *minimum_width,
                                        gint      *natural_width)
{
  GpImageMenuItem *self;
  GpImageMenuItemPrivate *priv;
  GtkPackDirection pack_dir;

  self = GP_IMAGE_MENU_ITEM (widget);
  priv = gp_image_menu_item_get_instance_private (self);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->get_preferred_width (widget,
                                                                           minimum_width,
                                                                           natural_width);

  if (!priv->image || !gtk_widget_get_visible (priv->image))
    return;

  pack_dir = get_pack_direction (self);
  if (pack_dir == GTK_PACK_DIRECTION_TTB || pack_dir == GTK_PACK_DIRECTION_BTT)
    {
      gint image_minimum_width;
      gint image_natural_width;

      gtk_widget_get_preferred_width (priv->image,
                                      &image_minimum_width,
                                      &image_natural_width);

      *minimum_width = MAX (*minimum_width, image_minimum_width);
      *natural_width = MAX (*natural_width, image_natural_width);
    }
}

static void
gp_image_menu_item_size_allocate (GtkWidget     *widget,
                                  GtkAllocation *allocation)
{
  GpImageMenuItem *self;
  GpImageMenuItemPrivate *priv;
  GtkAllocation widget_allocation;
  GtkRequisition image_requisition;
  GtkPackDirection pack_dir;
  GtkTextDirection text_dir;
  GtkAllocation image_allocation;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  gint toggle_size;
  gint x;
  gint y;

  self = GP_IMAGE_MENU_ITEM (widget);
  priv = gp_image_menu_item_get_instance_private (self);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->size_allocate (widget,
                                                                     allocation);

  if (!priv->image || !gtk_widget_get_visible (priv->image))
    return;

  gtk_widget_get_allocation (widget, &widget_allocation);
  gtk_widget_get_preferred_size (priv->image, &image_requisition, NULL);

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);
  gtk_style_context_get_padding (context, state, &padding);

  toggle_size = 0;
  gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (self), &toggle_size);

  pack_dir = get_pack_direction (self);
  text_dir = gtk_widget_get_direction (widget);

  if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
    {
      if ((text_dir == GTK_TEXT_DIR_LTR) == (pack_dir == GTK_PACK_DIRECTION_LTR))
        {
          x = padding.left + (toggle_size - image_requisition.width) / 2;
        }
      else
        {
          x = widget_allocation.width - padding.right - toggle_size +
              (toggle_size - image_requisition.width) / 2;
        }

      y = (widget_allocation.height - image_requisition.height) / 2;
    }
  else
    {
      if ((text_dir == GTK_TEXT_DIR_LTR) == (pack_dir == GTK_PACK_DIRECTION_TTB))
        {
          y = padding.top + (toggle_size - image_requisition.height) / 2;
        }
      else
        {
          y = widget_allocation.height - padding.bottom - toggle_size +
              (toggle_size - image_requisition.height) / 2;
        }

      x = (widget_allocation.width - image_requisition.width) / 2;
    }

  image_allocation.x = widget_allocation.x + MAX (0, x);
  image_allocation.y = widget_allocation.y + MAX (0, y);
  image_allocation.width = image_requisition.width;
  image_allocation.height = image_requisition.height;

  gtk_widget_size_allocate (priv->image, &image_allocation);
}

static void
gp_image_menu_item_add (GtkContainer *container,
                        GtkWidget    *widget)
{
  GpImageMenuItem *self;

  self = GP_IMAGE_MENU_ITEM (container);

  GTK_CONTAINER_CLASS (gp_image_menu_item_parent_class)->add (container, widget);
  update_css_class (self);
}

static void
gp_image_menu_item_forall (GtkContainer *container,
                           gboolean      include_internals,
                           GtkCallback   callback,
                           gpointer      callback_data)
{
  GpImageMenuItem *self;
  GpImageMenuItemPrivate *priv;

  self = GP_IMAGE_MENU_ITEM (container);
  priv = gp_image_menu_item_get_instance_private (self);

  GTK_CONTAINER_CLASS (gp_image_menu_item_parent_class)->forall (container,
                                                                 include_internals,
                                                                 callback,
                                                                 callback_data);

  if (include_internals && priv->image)
    (* callback) (priv->image, callback_data);
}

static void
gp_image_menu_item_remove (GtkContainer *container,
                           GtkWidget    *widget)
{
  GpImageMenuItem *self;
  GpImageMenuItemPrivate *priv;
  gboolean image_visible;

  self = GP_IMAGE_MENU_ITEM (container);
  priv = gp_image_menu_item_get_instance_private (self);

  if (priv->image != widget)
    {
      GTK_CONTAINER_CLASS (gp_image_menu_item_parent_class)->remove (container,
                                                                     widget);

      update_css_class (self);
      return;
    }

  image_visible = gtk_widget_get_visible (widget);
  gtk_widget_unparent (widget);

  priv->image = NULL;

  if (image_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
    gtk_widget_queue_resize (GTK_WIDGET (container));

  update_css_class (self);
}

static void
gp_image_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                        gint        *requisition)
{
  GpImageMenuItem *self;
  GpImageMenuItemPrivate *priv;
  GtkRequisition image_requisition;
  GtkPackDirection pack_dir;

  self = GP_IMAGE_MENU_ITEM (menu_item);
  priv = gp_image_menu_item_get_instance_private (self);

  *requisition = 0;

  if (!priv->image || !gtk_widget_get_visible (priv->image))
    return;

  gtk_widget_get_preferred_size (priv->image, &image_requisition, NULL);

  pack_dir = get_pack_direction (self);
  if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
    {
      if (image_requisition.width > 0)
        *requisition = image_requisition.width;
    }
  else
    {
      if (image_requisition.height > 0)
        *requisition = image_requisition.height;
    }
}

static void
gp_image_menu_item_class_init (GpImageMenuItemClass *self_class)
{
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkMenuItemClass *menu_item_class;

  widget_class = GTK_WIDGET_CLASS (self_class);
  container_class = GTK_CONTAINER_CLASS (self_class);
  menu_item_class = GTK_MENU_ITEM_CLASS (self_class);

  widget_class->destroy = gp_image_menu_item_destroy;
  widget_class->get_preferred_height = gp_image_menu_item_get_preferred_height;
  widget_class->get_preferred_height_for_width = gp_image_menu_item_get_preferred_height_for_width;
  widget_class->get_preferred_width = gp_image_menu_item_get_preferred_width;
  widget_class->size_allocate = gp_image_menu_item_size_allocate;

  container_class->add = gp_image_menu_item_add;
  container_class->forall = gp_image_menu_item_forall;
  container_class->remove = gp_image_menu_item_remove;

  menu_item_class->toggle_size_request = gp_image_menu_item_toggle_size_request;

  gtk_container_class_handle_border_width (container_class);
}

static void
gp_image_menu_item_init (GpImageMenuItem *self)
{
  GtkStyleContext *context;

  g_signal_connect (self, "notify::label", G_CALLBACK (notify_label_cb), NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "gp-image-menu-item");
}

/**
 * gp_image_menu_item_new:
 *
 * Creates a new #GpImageMenuItem.
 *
 * Returns: (transfer full): a newly created #GpImageMenuItem.
 */
GtkWidget *
gp_image_menu_item_new (void)
{
  return g_object_new (GP_TYPE_IMAGE_MENU_ITEM, NULL);
}

/**
 * gp_image_menu_item_new_with_label:
 * @label: the text of the menu item
 *
 * Creates a new #GpImageMenuItem containing a label.
 *
 * Returns: (transfer full): a newly created #GpImageMenuItem.
 */
GtkWidget *
gp_image_menu_item_new_with_label (const gchar *label)
{
  return g_object_new (GP_TYPE_IMAGE_MENU_ITEM,
                       "label", label,
                       NULL);
}

/**
 * gp_image_menu_item_new_with_mnemonic:
 * @label: the text of the menu item
 *
 * Creates a new #GpImageMenuItem containing a label. The label will
 * be created using gtk_label_new_with_mnemonic(), so underscores in
 * @label indicate the mnemonic for the menu item.
 *
 * Returns: (transfer full): a newly created #GpImageMenuItem.
 */
GtkWidget *
gp_image_menu_item_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GP_TYPE_IMAGE_MENU_ITEM,
                       "label", label,
                       "use-underline", TRUE,
                       NULL);
}

/**
 * gp_image_menu_item_set_image:
 * @self: a #GpImageMenuItem
 * @image: (allow-none): a widget to set as the image for the menu item
 *
 * Sets the image of @item to the given widget.
 */
void
gp_image_menu_item_set_image (GpImageMenuItem *self,
                              GtkWidget       *image)
{
  GpImageMenuItemPrivate *priv;

  priv = gp_image_menu_item_get_instance_private (self);

  if (priv->image == image)
    return;

  if (priv->image)
    gtk_container_remove (GTK_CONTAINER (self), priv->image);

  priv->image = image;

  update_css_class (self);

  if (image == NULL)
    return;

  gtk_widget_set_parent (image, GTK_WIDGET (self));
  gtk_widget_show (image);
}

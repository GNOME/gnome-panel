/*
 * Copyright (C) 2018 Alberts Muktupāvels
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

#include "config.h"
#include "gp-image-menu-item.h"

struct _GpImageMenuItem
{
  GtkMenuItem  parent;

  GtkWidget   *image;
};

G_DEFINE_TYPE (GpImageMenuItem, gp_image_menu_item, GTK_TYPE_MENU_ITEM)

static GtkPackDirection
get_pack_direction (GpImageMenuItem *item)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (item));

  if (GTK_IS_MENU_BAR (parent))
    return gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));

  return GTK_PACK_DIRECTION_LTR;
}

static void
gp_image_menu_item_destroy (GtkWidget *widget)
{
  GpImageMenuItem *item;

  item = GP_IMAGE_MENU_ITEM (widget);

  if (item->image)
    gtk_container_remove (GTK_CONTAINER (item), item->image);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->destroy (widget);
}

static void
gp_image_menu_item_get_preferred_height (GtkWidget *widget,
                                         gint      *minimum_height,
                                         gint      *natural_height)
{
  GpImageMenuItem *item;
  GtkPackDirection pack_dir;

  item = GP_IMAGE_MENU_ITEM (widget);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->get_preferred_height (widget,
                                                                            minimum_height,
                                                                            natural_height);

  if (!item->image || !gtk_widget_get_visible (item->image))
    return;

  pack_dir = get_pack_direction (item);
  if (pack_dir == GTK_PACK_DIRECTION_RTL || pack_dir == GTK_PACK_DIRECTION_LTR)
    {
      GtkRequisition image_requisition;

      gtk_widget_get_preferred_size (item->image, &image_requisition, NULL);

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
  GpImageMenuItem *item;
  GtkPackDirection pack_dir;

  item = GP_IMAGE_MENU_ITEM (widget);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->get_preferred_height_for_width (widget,
                                                                                      width,
                                                                                      minimum_height,
                                                                                      natural_height);

  if (!item->image || !gtk_widget_get_visible (item->image))
    return;

  pack_dir = get_pack_direction (item);
  if (pack_dir == GTK_PACK_DIRECTION_RTL || pack_dir == GTK_PACK_DIRECTION_LTR)
    {
      GtkRequisition image_requisition;

      gtk_widget_get_preferred_size (item->image, &image_requisition, NULL);

      *minimum_height = MAX (*minimum_height, image_requisition.height);
      *natural_height = MAX (*natural_height, image_requisition.height);
    }
}

static void
gp_image_menu_item_get_preferred_width (GtkWidget *widget,
                                        gint      *minimum_width,
                                        gint      *natural_width)
{
  GpImageMenuItem *item;
  GtkPackDirection pack_dir;

  item = GP_IMAGE_MENU_ITEM (widget);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->get_preferred_width (widget,
                                                                           minimum_width,
                                                                           natural_width);

  if (!item->image || !gtk_widget_get_visible (item->image))
    return;

  pack_dir = get_pack_direction (item);
  if (pack_dir == GTK_PACK_DIRECTION_TTB || pack_dir == GTK_PACK_DIRECTION_BTT)
    {
      gint image_minimum_width;
      gint image_natural_width;

      gtk_widget_get_preferred_width (item->image,
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
  GpImageMenuItem *item;
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

  item = GP_IMAGE_MENU_ITEM (widget);

  GTK_WIDGET_CLASS (gp_image_menu_item_parent_class)->size_allocate (widget,
                                                                     allocation);

  if (!item->image || !gtk_widget_get_visible (item->image))
    return;

  gtk_widget_get_allocation (widget, &widget_allocation);
  gtk_widget_get_preferred_size (item->image, &image_requisition, NULL);

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);
  gtk_style_context_get_padding (context, state, &padding);

  toggle_size = 0;
  gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (item), &toggle_size);

  pack_dir = get_pack_direction (item);
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

  gtk_widget_size_allocate (item->image, &image_allocation);
}

static void
gp_image_menu_item_forall (GtkContainer *container,
                           gboolean      include_internals,
                           GtkCallback   callback,
                           gpointer      callback_data)
{
  GpImageMenuItem *item;

  item = GP_IMAGE_MENU_ITEM (container);

  GTK_CONTAINER_CLASS (gp_image_menu_item_parent_class)->forall (container,
                                                                 include_internals,
                                                                 callback,
                                                                 callback_data);

  if (include_internals && item->image)
    (* callback) (item->image, callback_data);
}

static void
gp_image_menu_item_remove (GtkContainer *container,
                           GtkWidget    *widget)
{
  GpImageMenuItem *item;
  gboolean image_visible;

  item = GP_IMAGE_MENU_ITEM (container);

  if (item->image != widget)
    {
      GTK_CONTAINER_CLASS (gp_image_menu_item_parent_class)->remove (container,
                                                                     widget);

      return;
    }

  image_visible = gtk_widget_get_visible (widget);
  gtk_widget_unparent (widget);

  item->image = NULL;

  if (image_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gp_image_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                        gint        *requisition)
{
  GpImageMenuItem *item;
  GtkRequisition image_requisition;
  GtkPackDirection pack_dir;

  item = GP_IMAGE_MENU_ITEM (menu_item);
  *requisition = 0;

  if (!item->image || !gtk_widget_get_visible (item->image))
    return;

  gtk_widget_get_preferred_size (item->image, &image_requisition, NULL);

  pack_dir = get_pack_direction (item);
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
gp_image_menu_item_class_init (GpImageMenuItemClass *item_class)
{
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkMenuItemClass *menu_item_class;

  widget_class = GTK_WIDGET_CLASS (item_class);
  container_class = GTK_CONTAINER_CLASS (item_class);
  menu_item_class = GTK_MENU_ITEM_CLASS (item_class);

  widget_class->destroy = gp_image_menu_item_destroy;
  widget_class->get_preferred_height = gp_image_menu_item_get_preferred_height;
  widget_class->get_preferred_height_for_width = gp_image_menu_item_get_preferred_height_for_width;
  widget_class->get_preferred_width = gp_image_menu_item_get_preferred_width;
  widget_class->size_allocate = gp_image_menu_item_size_allocate;

  container_class->forall = gp_image_menu_item_forall;
  container_class->remove = gp_image_menu_item_remove;

  menu_item_class->toggle_size_request = gp_image_menu_item_toggle_size_request;

  gtk_container_class_handle_border_width (container_class);
}

static void
gp_image_menu_item_init (GpImageMenuItem *item)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (item));
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
 * @item: a #GpImageMenuItem
 * @image: (allow-none): a widget to set as the image for the menu item
 *
 * Sets the image of @item to the given widget.
 */
void
gp_image_menu_item_set_image (GpImageMenuItem *item,
                              GtkWidget       *image)
{
  if (item->image == image)
    return;

  if (item->image)
    gtk_container_remove (GTK_CONTAINER (item), item->image);

  item->image = image;
  if (image == NULL)
    return;

  gtk_widget_set_parent (image, GTK_WIDGET (item));
  gtk_widget_show (image);
}

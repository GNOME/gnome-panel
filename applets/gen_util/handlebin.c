/* GTK - The GIMP Toolkit
 * Copyright (C) 2002 Red Hat Inc.
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include <string.h>
#include <libgnome/libgnome.h>
#include "handlebin.h"

#define HANDLE_SIZE 10

enum {
  PROP_0,
  PROP_SHADOW_TYPE
};


static void foo_handle_class_init    (FooHandleClass  *klass);
static void foo_handle_init          (FooHandle       *handle);
static void foo_handle_set_property (GObject      *object,
                                     guint         param_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);
static void foo_handle_get_property (GObject     *object,
                                     guint        param_id,
                                     GValue      *value,
                                     GParamSpec  *pspec);
static void foo_handle_paint         (GtkWidget      *widget,
                                      GdkRectangle   *area);
static gint foo_handle_expose        (GtkWidget      *widget,
                                      GdkEventExpose *event);
static void foo_handle_size_request  (GtkWidget      *widget,
                                      GtkRequisition *requisition);
static void foo_handle_size_allocate (GtkWidget      *widget,
                                      GtkAllocation  *allocation);
static void foo_handle_remove        (GtkContainer   *container,
                                      GtkWidget      *child);
static void foo_handle_forall        (GtkContainer   *container,
                                      gboolean	     include_internals,
                                      GtkCallback     callback,
                                      gpointer        callback_data);

static void foo_handle_compute_child_allocation      (FooHandle      *handle,
                                                      GtkAllocation *child_allocation);
static void foo_handle_real_compute_child_allocation (FooHandle      *handle,
                                                      GtkAllocation *child_allocation);

static GtkBinClass *parent_class = NULL;


GtkType
foo_handle_get_type (void)
{
  static GType handle_type = 0;

  if (!handle_type)
    {      
      static const GTypeInfo handle_info =
      {
        sizeof (FooHandleClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) foo_handle_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (FooHandle),
        0,              /* n_preallocs */
        (GInstanceInitFunc) foo_handle_init
      };

      handle_type = g_type_register_static (GTK_TYPE_BIN,
                                            "FooHandle",
                                            &handle_info,
                                            0);
    }

  return handle_type;
}

static void
foo_handle_class_init (FooHandleClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass*) class;
  object_class = GTK_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  container_class = GTK_CONTAINER_CLASS (class);

  parent_class = gtk_type_class (gtk_bin_get_type ());

  gobject_class->set_property = foo_handle_set_property;
  gobject_class->get_property = foo_handle_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow_type",
                                                      _("Handle shadow"),
                                                      _("Appearance of the handle border."),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_ETCHED_IN,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  
  widget_class->expose_event = foo_handle_expose;
  widget_class->size_request = foo_handle_size_request;
  widget_class->size_allocate = foo_handle_size_allocate;

  container_class->remove = foo_handle_remove;
  container_class->forall = foo_handle_forall;

  class->compute_child_allocation = foo_handle_real_compute_child_allocation;
}

static void
foo_handle_init (FooHandle *handle)
{
  handle->shadow_type = GTK_SHADOW_ETCHED_IN;
}

static void 
foo_handle_set_property (GObject         *object,
                         guint            prop_id,
                         const GValue    *value,
                         GParamSpec      *pspec)
{
  FooHandle *handle;

  handle = FOO_HANDLE (object);

  switch (prop_id)
    {
    case PROP_SHADOW_TYPE:
      foo_handle_set_shadow_type (handle, g_value_get_enum (value));
      break;
    default:      
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
foo_handle_get_property (GObject         *object,
                         guint            prop_id,
                         GValue          *value,
                         GParamSpec      *pspec)
{
  FooHandle *handle;

  handle = FOO_HANDLE (object);

  switch (prop_id)
    {
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, handle->shadow_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkWidget*
foo_handle_new (void)
{
  FooHandle *handle;

  handle = g_object_new (FOO_TYPE_HANDLE, NULL);

  return GTK_WIDGET (handle);
}

static void
foo_handle_remove (GtkContainer *container,
                   GtkWidget    *child)
{
  FooHandle *handle = FOO_HANDLE (container);

  GTK_CONTAINER_CLASS (parent_class)->remove (container, child);
}

static void
foo_handle_forall (GtkContainer *container,
                   gboolean      include_internals,
                   GtkCallback   callback,
                   gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  FooHandle *handle = FOO_HANDLE (container);

  if (bin->child)
    (* callback) (bin->child, callback_data);
}

void
foo_handle_set_shadow_type (FooHandle      *handle,
                            GtkShadowType  type)
{
  g_return_if_fail (FOO_IS_HANDLE (handle));

  if ((GtkShadowType) handle->shadow_type != type)
    {
      handle->shadow_type = type;
      g_object_notify (G_OBJECT (handle), "shadow_type");
      
      gtk_widget_queue_resize (GTK_WIDGET (handle));
    }
}

/**
 * foo_handle_get_shadow_type:
 * @handle: a #FooHandle
 *
 * Retrieves the shadow type of the handle. See
 * foo_handle_set_shadow_type().
 *
 * Return value: the current shadow type of the handle.
 **/
GtkShadowType
foo_handle_get_shadow_type (FooHandle *handle)
{
  g_return_val_if_fail (FOO_IS_HANDLE (handle), GTK_SHADOW_ETCHED_IN);

  return handle->shadow_type;
}

void
foo_handle_set_orientation (FooHandle     *handle,
                            GtkOrientation orientation)
{
  g_return_if_fail (FOO_IS_HANDLE (handle));

  if (handle->orientation != orientation)
    {
      handle->orientation = orientation;
      
      gtk_widget_queue_resize (GTK_WIDGET (handle));
    }
}

static void
foo_handle_paint (GtkWidget    *widget,
                  GdkRectangle *area)
{
  FooHandle *handle;
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      handle = FOO_HANDLE (widget);

      gtk_paint_shadow (widget->style, widget->window,
                        GTK_STATE_NORMAL, handle->shadow_type,
                        area, widget, "frame",
                        handle->frame_rect.x,
                        handle->frame_rect.y,
                        handle->frame_rect.width,
                        handle->frame_rect.height);

      gtk_paint_handle (widget->style, widget->window,
                        GTK_WIDGET_STATE (widget),
                        GTK_SHADOW_OUT,
                        area, widget, "handlebox",
                        handle->handle_rect.x,
                        handle->handle_rect.y,
                        handle->handle_rect.width,
                        handle->handle_rect.height,
                        handle->orientation);
    }
}

static gboolean
foo_handle_expose (GtkWidget      *widget,
                   GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      foo_handle_paint (widget, &event->area);

      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }

  return FALSE;
}

static void
foo_handle_size_request (GtkWidget      *widget,
                         GtkRequisition *requisition)
{
  FooHandle *handle = FOO_HANDLE (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkRequisition child_requisition;  
  int extra_w, extra_h;
  
  requisition->width = 0;
  requisition->height = 0;
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width = MAX (requisition->width, child_requisition.width);
      requisition->height += child_requisition.height;
    }

  if (handle->shadow_type == GTK_SHADOW_NONE)
    {
      extra_w = 0;
      extra_h = 0;
    }
  else
    {
      extra_w = widget->style->xthickness * 2;
      extra_h = widget->style->ythickness * 2;
    }
  
  requisition->width += (GTK_CONTAINER (widget)->border_width + extra_w);
  requisition->height += (GTK_CONTAINER (widget)->border_width + extra_h);

  if (handle->orientation == GTK_ORIENTATION_HORIZONTAL)
    requisition->width += HANDLE_SIZE;
  else
    requisition->height += HANDLE_SIZE;
}

static void
foo_handle_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
  FooHandle *handle = FOO_HANDLE (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation new_allocation;

  widget->allocation = *allocation;

  foo_handle_compute_child_allocation (handle, &new_allocation);
  
  /* If the child allocation changed, that means that the frame is drawn
   * in a new place, so we must redraw the entire widget.
   */
  if (GTK_WIDGET_MAPPED (widget) &&
      (new_allocation.x != handle->child_allocation.x ||
       new_allocation.y != handle->child_allocation.y ||
       new_allocation.width != handle->child_allocation.width ||
       new_allocation.height != handle->child_allocation.height))
    gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    gtk_widget_size_allocate (bin->child, &new_allocation);
  
  handle->child_allocation = new_allocation;
}

static void
foo_handle_compute_child_allocation (FooHandle      *handle,
                                     GtkAllocation *child_allocation)
{
  g_return_if_fail (FOO_IS_HANDLE (handle));
  g_return_if_fail (child_allocation != NULL);

  FOO_HANDLE_GET_CLASS (handle)->compute_child_allocation (handle, child_allocation);
}

static void
foo_handle_real_compute_child_allocation (FooHandle      *handle,
                                          GtkAllocation *child_allocation)
{
  GtkWidget *widget = GTK_WIDGET (handle);
  GtkAllocation *allocation = &widget->allocation;
  int xpad, ypad;
  
  ypad = handle->shadow_type == GTK_SHADOW_NONE ? 0 : 
    widget->style->ythickness;
  xpad = handle->shadow_type == GTK_SHADOW_NONE ? 0 : 
    widget->style->xthickness;

  handle->frame_rect.x = (GTK_CONTAINER (handle)->border_width);
  handle->frame_rect.y = (GTK_CONTAINER (handle)->border_width);
  handle->frame_rect.width = allocation->width;
  handle->frame_rect.height = allocation->height;

  if (handle->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      handle->handle_rect.x = handle->frame_rect.x + xpad;
      handle->handle_rect.y = handle->frame_rect.y + ypad;
      handle->handle_rect.width = HANDLE_SIZE;
      handle->handle_rect.height = handle->frame_rect.height - ypad * 2;

      child_allocation->x = handle->handle_rect.x + handle->handle_rect.width;
      child_allocation->y = handle->handle_rect.y;
    }
  else
    {
      handle->handle_rect.x = handle->frame_rect.x + xpad;
      handle->handle_rect.y = handle->frame_rect.y + ypad;
      handle->handle_rect.width =  handle->frame_rect.width - xpad * 2;
      handle->handle_rect.height = HANDLE_SIZE;

      child_allocation->x = handle->handle_rect.x;
      child_allocation->y = handle->handle_rect.y + handle->handle_rect.height;
    }
  
  child_allocation->width = allocation->width - handle->handle_rect.x - xpad;
  child_allocation->height = allocation->height - handle->handle_rect.y - ypad; 
  if (handle->orientation == GTK_ORIENTATION_HORIZONTAL)
    child_allocation->width -= handle->handle_rect.width;
  else
    child_allocation->height -= handle->handle_rect.height;
  
  child_allocation->width = MAX (1, child_allocation->width);  
  child_allocation->height = MAX (1, child_allocation->height);
  
  child_allocation->x += allocation->x;
  child_allocation->y += allocation->y;
}


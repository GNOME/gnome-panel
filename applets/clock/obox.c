/* OBox Copyright (C) 2002 Red Hat Inc. based on GtkHBox */
/* GTK - The GIMP Toolkit
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

#include "obox.h"

#include <gtk/gtk.h>

static void clock_obox_size_request  (GtkWidget       *widget,
				      GtkRequisition  *requisition);
static void clock_obox_size_allocate (GtkWidget       *widget,
				      GtkAllocation   *allocation);


G_DEFINE_TYPE (ClockOBox, clock_obox, GTK_TYPE_BOX)

static void
clock_obox_class_init (ClockOBoxClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->size_request = clock_obox_size_request;
  widget_class->size_allocate = clock_obox_size_allocate;
}

static void
clock_obox_init (ClockOBox *obox)
{
  obox->orientation = GTK_ORIENTATION_HORIZONTAL;
  obox->reverse_order = FALSE;
}

GtkWidget*
clock_obox_new (void)
{
  ClockOBox *obox;

  obox = g_object_new (CLOCK_TYPE_OBOX, NULL);

  return GTK_WIDGET (obox);
}

static GtkWidgetClass*
get_class (ClockOBox *obox)     
{
  GtkWidgetClass *klass;

  switch (obox->orientation)
    {
    case GTK_ORIENTATION_HORIZONTAL:
      klass = GTK_WIDGET_CLASS (g_type_class_peek (GTK_TYPE_HBOX));
      break;
    case GTK_ORIENTATION_VERTICAL:
      klass = GTK_WIDGET_CLASS (g_type_class_peek (GTK_TYPE_VBOX));
      break;
    default:
      g_assert_not_reached ();
      klass = NULL;
      break;
    }

  return klass;
}

static void
clock_obox_size_request (GtkWidget      *widget,
		         GtkRequisition *requisition)
{
  GtkWidgetClass *klass;
  ClockOBox *obox;

  obox = CLOCK_OBOX (widget);

  klass = get_class (obox);

  klass->size_request (widget, requisition);
}

static void
clock_obox_size_allocate (GtkWidget     *widget,
		          GtkAllocation *allocation)
{
  GtkWidgetClass *klass;
  ClockOBox *obox;

  obox = CLOCK_OBOX (widget);

  klass = get_class (obox);

  klass->size_allocate (widget, allocation);
}

void
clock_obox_set_orientation (ClockOBox         *obox,
                            GtkOrientation  orientation)
{
  g_return_if_fail (CLOCK_IS_OBOX (obox));

  if (obox->orientation == orientation)
    return;
  
  obox->orientation = orientation;

  gtk_widget_queue_resize (GTK_WIDGET (obox));
}

void
clock_obox_set_reverse_order (ClockOBox *obox,
                              gboolean  reverse_order)
{
  GtkBox *box;

  g_return_if_fail (CLOCK_IS_OBOX (obox));

  if (obox->reverse_order == reverse_order)
    return;
  
  obox->reverse_order = reverse_order;
  box = GTK_BOX (obox);
  box->children = g_list_reverse (box->children);

  gtk_widget_queue_resize (GTK_WIDGET (obox));
}

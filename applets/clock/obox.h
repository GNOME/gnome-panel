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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __CLOCK_OBOX_H__
#define __CLOCK_OBOX_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define CLOCK_TYPE_OBOX            (clock_obox_get_type ())
#define CLOCK_OBOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLOCK_TYPE_OBOX, ClockOBox))
#define CLOCK_OBOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLOCK_TYPE_OBOX, ClockOBoxClass))
#define CLOCK_IS_OBOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLOCK_TYPE_OBOX))
#define CLOCK_IS_OBOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLOCK_TYPE_OBOX))
#define CLOCK_OBOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLOCK_TYPE_OBOX, ClockOBoxClass))


typedef struct _ClockOBox       ClockOBox;
typedef struct _ClockOBoxClass  ClockOBoxClass;

struct _ClockOBox
{
  GtkBox box;

  GtkOrientation orientation;
  gboolean       reverse_order;
};

struct _ClockOBoxClass
{
  GtkBoxClass parent_class;
};


GType	   clock_obox_get_type (void) G_GNUC_CONST;
GtkWidget* clock_obox_new      (void);

void clock_obox_set_orientation (ClockOBox       *obox,
                                 GtkOrientation  orientation);
void clock_obox_set_reverse_order (ClockOBox *obox,
                                   gboolean  reverse_order);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CLOCK_OBOX_H__ */

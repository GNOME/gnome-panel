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

#ifndef __TRAY_OBOX_H__
#define __TRAY_OBOX_H__

#include <gtk/gtkbox.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define TRAY_TYPE_OBOX            (tray_obox_get_type ())
#define TRAY_OBOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRAY_TYPE_OBOX, TrayOBox))
#define TRAY_OBOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TRAY_TYPE_OBOX, TrayOBoxClass))
#define TRAY_IS_OBOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRAY_TYPE_OBOX))
#define TRAY_IS_OBOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRAY_TYPE_OBOX))
#define TRAY_OBOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TRAY_TYPE_OBOX, TrayOBoxClass))


typedef struct _TrayOBox       TrayOBox;
typedef struct _TrayOBoxClass  TrayOBoxClass;

struct _TrayOBox
{
  GtkBox box;

  GtkOrientation orientation;
};

struct _TrayOBoxClass
{
  GtkBoxClass parent_class;
};


GType	   tray_obox_get_type (void) G_GNUC_CONST;
GtkWidget* tray_obox_new      (void);

void tray_obox_set_orientation (TrayOBox       *obox,
                                GtkOrientation  orientation);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __TRAY_OBOX_H__ */

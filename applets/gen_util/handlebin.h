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

#ifndef __GTK_HANDLE_H__
#define __GTK_HANDLE_H__

#include <gdk/gdk.h>
#include <gtk/gtkbin.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define FOO_TYPE_HANDLE                  (foo_handle_get_type ())
#define FOO_HANDLE(obj)                  (GTK_CHECK_CAST ((obj), FOO_TYPE_HANDLE, FooHandle))
#define FOO_HANDLE_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), FOO_TYPE_HANDLE, FooHandleClass))
#define FOO_IS_HANDLE(obj)               (GTK_CHECK_TYPE ((obj), FOO_TYPE_HANDLE))
#define FOO_IS_HANDLE_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), FOO_TYPE_HANDLE))
#define FOO_HANDLE_GET_CLASS(obj)        (GTK_CHECK_GET_CLASS ((obj), FOO_TYPE_HANDLE, FooHandleClass))


typedef struct _FooHandle       FooHandle;
typedef struct _FooHandleClass  FooHandleClass;

struct _FooHandle
{
  GtkBin bin;

  gint16 shadow_type;
  gint16 orientation;

  GtkAllocation child_allocation;
  GdkRectangle frame_rect;
  GdkRectangle handle_rect;
};

struct _FooHandleClass
{
  GtkBinClass parent_class;

  void (*compute_child_allocation) (FooHandle *handle, GtkAllocation *allocation);
};


GtkType    foo_handle_get_type         (void) G_GNUC_CONST;
GtkWidget* foo_handle_new              (void);
void       foo_handle_set_shadow_type  (FooHandle      *handle,
                                        GtkShadowType   type);
GtkShadowType foo_handle_get_shadow_type (FooHandle    *handle);
void       foo_handle_set_orientation   (FooHandle     *handle,
                                         GtkOrientation orientation);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __FOO_HANDLE_H__ */

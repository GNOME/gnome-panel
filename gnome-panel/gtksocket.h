/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_SOCKET_H__
#define __GTK_SOCKET_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_SOCKET(obj)          GTK_CHECK_CAST (obj, gtk_socket_get_type (), GtkSocket)
#define GTK_SOCKET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_socket_get_type (), GtkSocketClass)
#define GTK_IS_SOCKET(obj)       GTK_CHECK_TYPE (obj, gtk_socket_get_type ())


typedef struct _GtkSocket        GtkSocket;
typedef struct _GtkSocketClass   GtkSocketClass;

struct _GtkSocket
{
  GtkContainer container;

  guint16 request_width;
  guint16 request_height;
  guint16 current_width;
  guint16 current_height;
  
  GdkWindow *plug_window;
  gint same_app : 1;
  gint focus_in : 1;
  gint have_size : 1;
};

struct _GtkSocketClass
{
  GtkContainerClass parent_class;
};


GtkWidget*     gtk_socket_new      ();
guint          gtk_socket_get_type (void );
void           gtk_socket_steal    (GtkSocket *socket,
				    guint32 id);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SOCKET_H__ */

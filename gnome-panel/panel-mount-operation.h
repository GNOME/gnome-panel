/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Based on eel-mount-operation.h - Gtk+ implementation for GMountOperation

   Copyright (C) 2007 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef PANEL_MOUNT_OPERATION_H
#define PANEL_MOUNT_OPERATION_H

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define PANEL_TYPE_MOUNT_OPERATION         (panel_mount_operation_get_type ())
#define PANEL_MOUNT_OPERATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_MOUNT_OPERATION, PanelMountOperation))
#define PANEL_MOUNT_OPERATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_MOUNT_OPERATION, PanelMountOperationClass))
#define PANEL_IS_MOUNT_OPERATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_MOUNT_OPERATION))
#define PANEL_IS_MOUNT_OPERATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_MOUNT_OPERATION))
#define PANEL_MOUNT_OPERATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_MOUNT_OPERATION, PanelMountOperationClass))

typedef struct PanelMountOperation	    PanelMountOperation;
typedef struct PanelMountOperationClass       PanelMountOperationClass;
typedef struct PanelMountOperationPrivate     PanelMountOperationPrivate;

struct PanelMountOperation
{
	GMountOperation parent_instance;
	
	PanelMountOperationPrivate *priv;
};

struct PanelMountOperationClass 
{
	GMountOperationClass parent_class;


	/* signals: */

	void (* active_changed) (PanelMountOperation *operation,
				 gboolean is_active);
};

GType            panel_mount_operation_get_type (void);
GMountOperation *panel_mount_operation_new      (GtkWindow *parent);

G_END_DECLS

#endif /* PANEL_MOUNT_OPERATION_H */



/*
 * panel-applet-container.c: a container for applets.
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef __PANEL_APPLET_CONTAINER_H__
#define __PANEL_APPLET_CONTAINER_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include "panel.h"

G_BEGIN_DECLS

#define PANEL_TYPE_APPLET_CONTAINER            (panel_applet_container_get_type ())
#define PANEL_APPLET_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_APPLET_CONTAINER, PanelAppletContainer))
#define PANEL_APPLET_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PANEL_TYPE_APPLET_CONTAINER, PanelAppletContainerClass))
#define PANEL_IS_APPLET_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_APPLET_CONTAINER))
#define PANEL_IS_APPLET_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_APPLET_CONTAINER))
#define PANEL_APPLET_CONTAINER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PANEL_TYPE_APPLET_CONTAINER, PanelAppletContainerClass))

#define PANEL_APPLET_CONTAINER_ERROR           (panel_applet_container_error_quark())

typedef enum {
	PANEL_APPLET_CONTAINER_INVALID_APPLET,
	PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY
} PanelAppletContainerError;

typedef struct _PanelAppletContainer        PanelAppletContainer;
typedef struct _PanelAppletContainerClass   PanelAppletContainerClass;
typedef struct _PanelAppletContainerPrivate PanelAppletContainerPrivate;

struct _PanelAppletContainer {
	GtkEventBox parent;

	PanelAppletContainerPrivate *priv;
};

struct _PanelAppletContainerClass {
	GtkEventBoxClass parent_class;

	/* Signals */
	void (*applet_broken)          (PanelAppletContainer *container);
	void (*applet_move)            (PanelAppletContainer *container);
	void (*applet_remove)          (PanelAppletContainer *container);
	void (*child_property_changed) (PanelAppletContainer *container,
					const gchar          *property_name,
					GVariant             *value);
};

GType      panel_applet_container_get_type                (void) G_GNUC_CONST;
GQuark     panel_applet_container_error_quark             (void) G_GNUC_CONST;
GtkWidget *panel_applet_container_new                     (void);


void       panel_applet_container_add                     (PanelAppletContainer *container,
							   GdkScreen            *screen,
							   const gchar          *iid,
							   GCancellable        *cancellable,
							   GAsyncReadyCallback  callback,
							   gpointer             user_data,
							   GVariant            *properties);
gboolean   panel_applet_container_add_finish              (PanelAppletContainer *container,
							   GAsyncResult         *result,
							   GError              **error);
void       panel_applet_container_child_popup_menu        (PanelAppletContainer *container,
							   guint                 button,
							   guint32               timestamp,
							   GCancellable         *cancellable,
							   GAsyncReadyCallback   callback,
							   gpointer              user_data);
gboolean   panel_applet_container_child_popup_menu_finish (PanelAppletContainer *container,
							   GAsyncResult         *result,
							   GError              **error);
void       panel_applet_container_child_popup_edit_menu   (PanelAppletContainer *container,
							   guint                 button,
							   guint32               timestamp,
							   GCancellable         *cancellable,
							   GAsyncReadyCallback   callback,
							   gpointer              user_data);
gboolean   panel_applet_container_child_popup_edit_menu_finish (PanelAppletContainer *container,
								GAsyncResult         *result,
								GError              **error);

void       panel_applet_container_child_set               (PanelAppletContainer *container,
							   const gchar          *property_name,
							   const GVariant       *value,
							   GCancellable         *cancellable,
							   GAsyncReadyCallback   callback,
							   gpointer              user_data);
gboolean   panel_applet_container_child_set_finish        (PanelAppletContainer *container,
							   GAsyncResult         *result,
							   GError              **error);
void       panel_applet_container_child_get               (PanelAppletContainer *container,
							   const gchar          *property_name,
							   GCancellable         *cancellable,
							   GAsyncReadyCallback   callback,
							   gpointer              user_data);
GVariant  *panel_applet_container_child_get_finish        (PanelAppletContainer *container,
							   GAsyncResult         *result,
							   GError              **error);

G_END_DECLS

#endif /* __PANEL_APPLET_CONTAINER_H__ */

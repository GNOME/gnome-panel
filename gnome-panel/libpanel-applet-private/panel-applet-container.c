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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <panel-applets-manager.h>

#include "panel-applet-container.h"
#include "panel-marshal.h"

struct _PanelAppletContainerPrivate {
	GDBusProxy *applet_proxy;

	guint       name_watcher_id;
	gchar      *bus_name;

	gchar      *iid;
	guint32     uid;

	GHashTable *pending_ops;
};

enum {
	APPLET_MOVE,
	APPLET_REMOVE,
	CHILD_PROPERTY_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
	const gchar *name;
	const gchar *dbus_name;
} AppletPropertyInfo;

static const AppletPropertyInfo applet_properties [] = {
	{ "settings-path", "SettingsPath" },
	{ "orient",        "Orient" },
	{ "size-hints",    "SizeHints" },
	{ "flags",         "Flags" },
	{ "locked-down",   "LockedDown" }
};

#define PANEL_APPLET_CONTAINER_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_APPLET_CONTAINER, PanelAppletContainerPrivate))

#define PANEL_APPLET_BUS_NAME            "org.gnome.panel.applet.%s"
#define PANEL_APPLET_FACTORY_INTERFACE   "org.gnome.panel.applet.AppletFactory"
#define PANEL_APPLET_FACTORY_OBJECT_PATH "/org/gnome/panel/applet/%s"
#define PANEL_APPLET_INTERFACE           "org.gnome.panel.applet.Applet"

G_DEFINE_TYPE (PanelAppletContainer, panel_applet_container, GTK_TYPE_EVENT_BOX);

GQuark
panel_applet_container_error_quark (void)
{
	return g_quark_from_static_string ("panel-applet-container-error-quark");
}

static void
panel_applet_container_init (PanelAppletContainer *container)
{
	container->priv = PANEL_APPLET_CONTAINER_GET_PRIVATE (container);

	container->priv->pending_ops = g_hash_table_new_full (g_direct_hash,
							      g_direct_equal,
							      NULL,
							      (GDestroyNotify) g_object_unref);
}

static void
panel_applet_container_setup (PanelAppletContainer *container)
{
	GtkWidget *applet;

	applet = panel_applets_manager_get_applet_widget (container->priv->iid, container->priv->uid);
	gtk_container_add (GTK_CONTAINER (container), applet);
}

static void
panel_applet_container_cancel_pending_operations (PanelAppletContainer *container)
{
	GList *keys, *l;

	if (!container->priv->pending_ops)
		return;

	keys = g_hash_table_get_keys (container->priv->pending_ops);
	for (l = keys; l; l = g_list_next (l)) {
		GCancellable *cancellable;

		cancellable = G_CANCELLABLE (g_hash_table_lookup (container->priv->pending_ops,
								  l->data));
		g_cancellable_cancel (cancellable);
	}
	g_list_free (keys);
}

static void
panel_applet_container_dispose (GObject *object)
{
	PanelAppletContainer *container = PANEL_APPLET_CONTAINER (object);

	if (container->priv->pending_ops) {
		panel_applet_container_cancel_pending_operations (container);
		g_hash_table_destroy (container->priv->pending_ops);
		container->priv->pending_ops = NULL;
	}

	if (container->priv->bus_name) {
		g_free (container->priv->bus_name);
		container->priv->bus_name = NULL;
	}

	if (container->priv->iid) {
		g_free (container->priv->iid);
		container->priv->iid = NULL;
	}

	if (container->priv->name_watcher_id > 0) {
		g_bus_unwatch_name (container->priv->name_watcher_id);
		container->priv->name_watcher_id = 0;
	}

	if (container->priv->applet_proxy) {
		g_object_unref (container->priv->applet_proxy);
		container->priv->applet_proxy = NULL;
	}

	G_OBJECT_CLASS (panel_applet_container_parent_class)->dispose (object);
}

static void
panel_applet_container_class_init (PanelAppletContainerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (PanelAppletContainerPrivate));

	gobject_class->dispose = panel_applet_container_dispose;

	signals[APPLET_MOVE] =
		g_signal_new ("applet-move",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PanelAppletContainerClass, applet_move),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[APPLET_REMOVE] =
		g_signal_new ("applet-remove",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PanelAppletContainerClass, applet_remove),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[CHILD_PROPERTY_CHANGED] =
		g_signal_new ("child-property-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE |
		              G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS,
			      G_STRUCT_OFFSET (PanelAppletContainerClass, child_property_changed),
			      NULL,
			      NULL,
			      panel_marshal_VOID__STRING_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_POINTER);
}

static const AppletPropertyInfo *
panel_applet_container_child_property_get_info (const gchar *property_name)
{
	gsize i;

	g_assert (property_name != NULL);

	for (i = 0; i < G_N_ELEMENTS (applet_properties); i++) {
		if (g_ascii_strcasecmp (applet_properties[i].name, property_name) == 0)
			return &applet_properties[i];
	}

	return NULL;
}

GtkWidget *
panel_applet_container_new (void)
{
	GtkWidget *container;

	container = GTK_WIDGET (g_object_new (PANEL_TYPE_APPLET_CONTAINER, NULL));

	return container;
}

static void
panel_applet_container_child_signal (GDBusProxy           *proxy,
				     gchar                *sender_name,
				     gchar                *signal_name,
				     GVariant             *parameters,
				     PanelAppletContainer *container)
{
	if (g_strcmp0 (signal_name, "Move") == 0) {
		g_signal_emit (container, signals[APPLET_MOVE], 0);
	} else if (g_strcmp0 (signal_name, "RemoveFromPanel") == 0) {
		g_signal_emit (container, signals[APPLET_REMOVE], 0);
	}
}

static void
on_property_changed (GDBusConnection      *connection,
		     const gchar          *sender_name,
		     const gchar          *object_path,
		     const gchar          *interface_name,
		     const gchar          *signal_name,
		     GVariant             *parameters,
		     PanelAppletContainer *container)
{
	GVariant    *props;
	GVariantIter iter;
	GVariant    *value;
	gchar       *key;

	g_variant_get (parameters, "(s@a{sv}*)", NULL, &props, NULL);

	g_variant_iter_init (&iter, props);
	while (g_variant_iter_loop (&iter, "{sv}", &key, &value)) {
		if (g_strcmp0 (key, "Flags") == 0) {
			g_signal_emit (container, signals[CHILD_PROPERTY_CHANGED],
				       g_quark_from_string ("flags"),
				       "flags", value);
		} else if (g_strcmp0 (key, "SizeHints") == 0) {
			g_signal_emit (container, signals[CHILD_PROPERTY_CHANGED],
				       g_quark_from_string ("size-hints"),
				       "size-hints", value);
		}
	}

	g_variant_unref (props);
}

static void
on_proxy_appeared (GObject      *source_object,
		   GAsyncResult *res,
		   gpointer      user_data)
{
	GTask                *task = G_TASK (user_data);
	PanelAppletContainer *container;
	GDBusProxy           *proxy;
	GError               *error = NULL;

	proxy = g_dbus_proxy_new_finish (res, &error);

	if (!proxy) {
		g_task_return_error (task, error);
		g_object_unref (task);

		return;
	}

	container = PANEL_APPLET_CONTAINER (g_async_result_get_source_object (G_ASYNC_RESULT (task)));

	container->priv->applet_proxy = proxy;
	g_signal_connect (container->priv->applet_proxy, "g-signal",
			  G_CALLBACK (panel_applet_container_child_signal),
			  container);
	g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (proxy),
					    g_dbus_proxy_get_name (proxy),
					    "org.freedesktop.DBus.Properties",
					    "PropertiesChanged",
					    g_dbus_proxy_get_object_path (proxy),
					    PANEL_APPLET_INTERFACE,
					    G_DBUS_SIGNAL_FLAGS_NONE,
					    (GDBusSignalCallback) on_property_changed,
					    container, NULL);

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);

	panel_applet_container_setup (container);
	g_object_unref (container);
}

static void
get_applet_cb (GObject      *source_object,
	       GAsyncResult *res,
	       gpointer      user_data)
{
	GDBusConnection      *connection = G_DBUS_CONNECTION (source_object);
	GTask                *task = G_TASK (user_data);
	PanelAppletContainer *container;
	GVariant             *retvals;
	const gchar          *applet_path;
	GError               *error = NULL;

	retvals = g_dbus_connection_call_finish (connection, res, &error);

	if (!retvals) {
		g_task_return_error (task, error);
		g_object_unref (task);

		return;
	}

	container = PANEL_APPLET_CONTAINER (g_async_result_get_source_object (G_ASYNC_RESULT (task)));

	g_variant_get (retvals, "(&ou)", &applet_path, &container->priv->uid);

	g_dbus_proxy_new (connection,
			  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
			  NULL,
			  container->priv->bus_name,
			  applet_path,
			  PANEL_APPLET_INTERFACE,
			  NULL,
			  (GAsyncReadyCallback) on_proxy_appeared,
			  task);

	g_variant_unref (retvals);

	g_object_unref (container);
}

typedef struct {
	GTask        *task;
	gchar        *factory_id;
	GVariant     *parameters;
	GCancellable *cancellable;
} AppletFactoryData;

static void
applet_factory_data_free (AppletFactoryData *data)
{
	g_free (data->factory_id);
	if (data->cancellable)
		g_object_unref (data->cancellable);

	g_free (data);
}

static void
on_factory_appeared (GDBusConnection   *connection,
		     const gchar       *name,
		     const gchar       *name_owner,
		     AppletFactoryData *data)
{
	PanelAppletContainer *container;
	gchar                *object_path;

	container = PANEL_APPLET_CONTAINER (g_async_result_get_source_object (G_ASYNC_RESULT (data->task)));
	container->priv->bus_name = g_strdup (name_owner);
	g_object_unref (container);

	object_path = g_strdup_printf (PANEL_APPLET_FACTORY_OBJECT_PATH, data->factory_id);
	g_dbus_connection_call (connection,
				name_owner,
				object_path,
				PANEL_APPLET_FACTORY_INTERFACE,
				"GetApplet",
				data->parameters,
				G_VARIANT_TYPE ("(ou)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				data->cancellable,
				get_applet_cb,
				data->task);
	g_free (object_path);
}

static void
panel_applet_container_get_applet (PanelAppletContainer *container,
				   GdkScreen            *screen,
				   const gchar          *iid,
				   GVariant             *props,
				   GCancellable         *cancellable,
				   GAsyncReadyCallback   callback,
				   gpointer              user_data)
{
	GTask              *task;
	AppletFactoryData  *data;
	gchar              *bus_name;
	gchar              *factory_id;
	gchar              *applet_id;

	task = g_task_new (container, cancellable, callback, user_data);

	applet_id = g_strrstr (iid, "::");

	if (!applet_id) {
		g_task_return_new_error (task,
		                         PANEL_APPLET_CONTAINER_ERROR,
		                         PANEL_APPLET_CONTAINER_INVALID_APPLET,
		                         "Invalid applet iid: %s", iid);
		g_object_unref (task);

		return;
	}

	factory_id = g_strndup (iid, strlen (iid) - strlen (applet_id));
	applet_id += 2;

	data = g_new (AppletFactoryData, 1);
	data->task = task;
	data->factory_id = factory_id;
	data->parameters = g_variant_new ("(s*)", applet_id, props);
	data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

	bus_name = g_strdup_printf (PANEL_APPLET_BUS_NAME, factory_id);

	container->priv->iid = g_strdup (iid);
	container->priv->name_watcher_id =
		g_bus_watch_name (G_BUS_TYPE_SESSION,
				  bus_name,
				  G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
				  (GBusNameAppearedCallback) on_factory_appeared,
				  NULL,
				  data,
				  (GDestroyNotify) applet_factory_data_free);

	g_free (bus_name);
}

void
panel_applet_container_add (PanelAppletContainer *container,
			    GdkScreen            *screen,
			    const gchar          *iid,
			    GCancellable         *cancellable,
			    GAsyncReadyCallback   callback,
			    gpointer              user_data,
			    GVariant             *properties)
{
	g_return_if_fail (PANEL_IS_APPLET_CONTAINER (container));
	g_return_if_fail (iid != NULL);

	panel_applet_container_cancel_pending_operations (container);

	panel_applet_container_get_applet (container, screen, iid, properties,
					   cancellable, callback, user_data);
}

gboolean
panel_applet_container_add_finish (PanelAppletContainer *container,
				   GAsyncResult         *result,
				   GError              **error)
{
	g_return_val_if_fail (g_task_is_valid (result, container), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/* Child Properties */
static void
set_applet_property_cb (GObject      *source_object,
			GAsyncResult *res,
			gpointer      user_data)
{
	GDBusConnection      *connection = G_DBUS_CONNECTION (source_object);
	GTask                *task = G_TASK (user_data);
	PanelAppletContainer *container;
	GVariant             *retvals;
	GError               *error = NULL;

	container = PANEL_APPLET_CONTAINER (g_async_result_get_source_object (G_ASYNC_RESULT (task)));
	g_hash_table_remove (container->priv->pending_ops, task);
	g_object_unref (container);

	retvals = g_dbus_connection_call_finish (connection, res, &error);

	if (!retvals) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Error setting property: %s\n", error->message);

		g_task_return_error (task, error);
		g_object_unref (task);

		return;
	}

	g_variant_unref (retvals);

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

void
panel_applet_container_child_set (PanelAppletContainer *container,
				  const gchar          *property_name,
				  const GVariant       *value,
				  GCancellable         *cancellable,
				  GAsyncReadyCallback   callback,
				  gpointer              user_data)
{
	GDBusProxy               *proxy;
	const AppletPropertyInfo *info;
	GTask                    *task;

	proxy = container->priv->applet_proxy;

	if (!proxy)
		return;

	task = g_task_new (container, cancellable, callback, user_data);

	info = panel_applet_container_child_property_get_info (property_name);

	if (!info) {
		g_task_return_new_error (task,
		                         PANEL_APPLET_CONTAINER_ERROR,
		                         PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY,
		                         "%s: Applet has no child property named `%s'",
		                         G_STRLOC, property_name);
		g_object_unref (task);

		return;
	}

	if (cancellable)
		g_object_ref (cancellable);
	else
		cancellable = g_cancellable_new ();

	g_hash_table_insert (container->priv->pending_ops, task, cancellable);

	g_dbus_connection_call (g_dbus_proxy_get_connection (proxy),
				g_dbus_proxy_get_name (proxy),
				g_dbus_proxy_get_object_path (proxy),
				"org.freedesktop.DBus.Properties",
				"Set",
				g_variant_new ("(ssv)",
					       g_dbus_proxy_get_interface_name (proxy),
					       info->dbus_name,
					       value),
				NULL,
				G_DBUS_CALL_FLAGS_NO_AUTO_START,
				-1, cancellable,
				set_applet_property_cb,
				task);
}

gboolean
panel_applet_container_child_set_finish (PanelAppletContainer *container,
					 GAsyncResult         *result,
					 GError              **error)
{
	g_return_val_if_fail (g_task_is_valid (result, container), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
get_applet_property_cb (GObject      *source_object,
			GAsyncResult *res,
			gpointer      user_data)
{
	GDBusConnection      *connection = G_DBUS_CONNECTION (source_object);
	GTask                *task = G_TASK (user_data);
	PanelAppletContainer *container;
	GVariant             *retvals;
	GError               *error = NULL;
	GVariant             *value;
	GVariant             *item;

	container = PANEL_APPLET_CONTAINER (g_async_result_get_source_object (G_ASYNC_RESULT (task)));
	g_hash_table_remove (container->priv->pending_ops, task);
	g_object_unref (container);

	retvals = g_dbus_connection_call_finish (connection, res, &error);

	if (!retvals) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Error getting property: %s\n", error->message);

		g_task_return_error (task, error);
		g_object_unref (task);

		return;
	}

	item = g_variant_get_child_value (retvals, 0);
	value = g_variant_get_variant (item);
	g_variant_unref (item);

	g_variant_unref (retvals);

	g_task_return_pointer (task, value, (GDestroyNotify) g_variant_unref);
	g_object_unref (task);
}

void
panel_applet_container_child_get (PanelAppletContainer *container,
				  const gchar          *property_name,
				  GCancellable         *cancellable,
				  GAsyncReadyCallback   callback,
				  gpointer              user_data)
{
	GDBusProxy               *proxy;
	const AppletPropertyInfo *info;
	GTask                    *task;

	proxy = container->priv->applet_proxy;

	if (!proxy)
		return;

	task = g_task_new (container, cancellable, callback, user_data);

	info = panel_applet_container_child_property_get_info (property_name);

	if (!info) {
		g_task_return_new_error (task,
		                         PANEL_APPLET_CONTAINER_ERROR,
		                         PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY,
		                         "%s: Applet has no child property named `%s'",
		                         G_STRLOC, property_name);
		g_object_unref (task);

		return;
	}

	if (cancellable)
		g_object_ref (cancellable);
	else
		cancellable = g_cancellable_new ();

	g_hash_table_insert (container->priv->pending_ops, task, cancellable);

	g_dbus_connection_call (g_dbus_proxy_get_connection (proxy),
				g_dbus_proxy_get_name (proxy),
				g_dbus_proxy_get_object_path (proxy),
				"org.freedesktop.DBus.Properties",
				"Get",
				g_variant_new ("(ss)",
					       g_dbus_proxy_get_interface_name (proxy),
					       info->dbus_name),
				G_VARIANT_TYPE ("(v)"),
				G_DBUS_CALL_FLAGS_NO_AUTO_START,
				-1, cancellable,
				get_applet_property_cb,
				task);
}

GVariant *
panel_applet_container_child_get_finish (PanelAppletContainer *container,
					 GAsyncResult         *result,
					 GError              **error)
{
	GVariant *variant;

	g_return_val_if_fail (g_task_is_valid (result, container), NULL);

	variant = g_task_propagate_pointer (G_TASK (result), error);

	if (!variant)
		return NULL;

	return g_variant_ref (variant);
}

static void
child_popup_menu_cb (GObject      *source_object,
		     GAsyncResult *res,
		     gpointer      user_data)
{
	GDBusConnection *connection = G_DBUS_CONNECTION (source_object);
	GTask           *task = G_TASK (user_data);
	GVariant        *retvals;
	GError          *error = NULL;

	retvals = g_dbus_connection_call_finish (connection, res, &error);

	if (!retvals) {
		g_task_return_error (task, error);
		g_object_unref (task);

		return;
	}

	g_variant_unref (retvals);

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

static void
popup_menu (PanelAppletContainer *container,
            const gchar          *method_name,
            guint                 button,
            guint32               timestamp,
            GCancellable         *cancellable,
            GAsyncReadyCallback   callback,
            gpointer              user_data)
{
	GTask *task;
	GDBusProxy *proxy;

	proxy = container->priv->applet_proxy;

	if (!proxy)
		return;

	task = g_task_new (container, cancellable, callback, user_data);

	g_dbus_connection_call (g_dbus_proxy_get_connection (proxy),
	                        g_dbus_proxy_get_name (proxy),
	                        g_dbus_proxy_get_object_path (proxy),
	                        PANEL_APPLET_INTERFACE,
	                        method_name,
	                        g_variant_new ("(uu)", button, timestamp),
	                        NULL,
	                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
	                        -1, cancellable,
	                        child_popup_menu_cb,
	                        task);
}

static gboolean
popup_menu_finish (PanelAppletContainer *container,
                   GAsyncResult         *result,
                   GError              **error)
{
	g_return_val_if_fail (g_task_is_valid (result, container), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

void
panel_applet_container_child_popup_menu (PanelAppletContainer *container,
					 guint                 button,
					 guint32               timestamp,
					 GCancellable         *cancellable,
					 GAsyncReadyCallback   callback,
					 gpointer              user_data)
{
	popup_menu (container, "PopupMenu", button, timestamp,
	            cancellable, callback, user_data);
}

gboolean
panel_applet_container_child_popup_menu_finish (PanelAppletContainer *container,
						GAsyncResult         *result,
						GError              **error)
{
	return popup_menu_finish (container, result, error);
}

void
panel_applet_container_child_popup_edit_menu (PanelAppletContainer *container,
					      guint                 button,
					      guint32               timestamp,
					      GCancellable         *cancellable,
					      GAsyncReadyCallback   callback,
					      gpointer              user_data)
{
	popup_menu (container, "PopupEditMenu", button, timestamp,
	            cancellable, callback, user_data);
}

gboolean
panel_applet_container_child_popup_edit_menu_finish (PanelAppletContainer *container,
						     GAsyncResult         *result,
						     GError              **error)
{
	return popup_menu_finish (container, result, error);
}

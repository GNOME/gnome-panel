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

#include <string.h>
#include <gobject/gvaluecollector.h>
#include <eggdbus/eggdbus.h>
#include "panel-applet-container.h"
#include "_panelapplet.h"

struct _PanelAppletContainerPrivate {
	EggDBusConnection  *connection;
	EggDBusObjectProxy *applet_proxy;
	gchar              *bus_name;

	GtkWidget          *socket;

	GHashTable         *pending_ops;
};

enum {
	APPLET_BROKEN,
	APPLET_MOVE,
	APPLET_REMOVE,
	APPLET_LOCK,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
	const gchar *name;
	GType        type;
	const gchar *signature;
	const gchar *dbus_name;
} AppletPropertyInfo;

static const AppletPropertyInfo applet_properties [] = {
	{ "prefs-key",   G_TYPE_STRING,  "s",  "PrefsKey" },
	{ "orient",      G_TYPE_UINT,    "u",  "Orient" },
	{ "size",        G_TYPE_UINT,    "u",  "Size" },
	{ "size-hints",  G_TYPE_OBJECT,  "i",  "SizeHints" },
	{ "background",  G_TYPE_STRING,  "s",  "Background" },
	{ "flags",       G_TYPE_UINT,    "u",  "Flags" },
	{ "locked",      G_TYPE_BOOLEAN, "b",  "Locked" },
	{ "locked-down", G_TYPE_BOOLEAN, "b",  "LockedDown" }
};

#define PANEL_APPLET_CONTAINER_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_APPLET_CONTAINER, PanelAppletContainerPrivate))

#define PANEL_APPLET_BUS_NAME            "org.gnome.panel.applet.%s"
#define PANEL_APPLET_FACTORY_INTERFACE   "org.gnome.panel.applet.AppletFactory"
#define PANEL_APPLET_FACTORY_OBJECT_PATH "/org/gnome/panel/applet/%s"
#define PANEL_APPLET_INTERFACE           "org.gnome.panel.applet.Applet"

static gboolean panel_applet_container_plug_removed (PanelAppletContainer *container);

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

	container->priv->socket = gtk_socket_new ();
	g_signal_connect_swapped (container->priv->socket, "plug-removed",
				  G_CALLBACK (panel_applet_container_plug_removed),
				  container);

	container->priv->pending_ops = g_hash_table_new_full (g_direct_hash,
							      g_direct_equal,
							      NULL,
							      (GDestroyNotify)g_object_unref);

	gtk_container_add (GTK_CONTAINER (container),
			   container->priv->socket);
	gtk_widget_show (container->priv->socket);
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

	signals[APPLET_BROKEN] =
		g_signal_new ("applet-broken",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PanelAppletContainerClass, applet_broken),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
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
	signals[APPLET_LOCK] =
		g_signal_new ("applet-lock",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PanelAppletContainerClass, applet_lock),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);
}

static _PanelAppletOrient
get_panel_applet_orient (PanelOrientation orientation)
{
	switch (orientation) {
	case PANEL_ORIENTATION_TOP:
		return _PANEL_APPLET_ORIENT_DOWN;
	case PANEL_ORIENTATION_BOTTOM:
		return _PANEL_APPLET_ORIENT_UP;
	case PANEL_ORIENTATION_LEFT:
		return _PANEL_APPLET_ORIENT_RIGHT;
	case PANEL_ORIENTATION_RIGHT:
		return _PANEL_APPLET_ORIENT_LEFT;
	default:
		g_assert_not_reached ();
		break;
	}
}

static const AppletPropertyInfo *
panel_applet_container_child_property_get_info (const gchar *property_name)
{
	gint i;

	g_assert (property_name != NULL);

	for (i = 0; i < G_N_ELEMENTS (applet_properties); i++) {
		if (g_ascii_strcasecmp (applet_properties[i].name, property_name) == 0)
			return &applet_properties[i];
	}

	return NULL;
}

static void
set_applet_property_cb (GObject      *source_object,
			GAsyncResult *res,
			gpointer      user_data)
{
	GSimpleAsyncResult   *result = G_SIMPLE_ASYNC_RESULT (user_data);
	PanelAppletContainer *container;
	GError               *error = NULL;

	if (!egg_dbus_properties_set_finish (EGG_DBUS_PROPERTIES (source_object), res, &error)) {
		if (!g_error_matches (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_CANCELLED))
			g_warning ("Error setting property: %s\n", error->message);
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	}

	container = PANEL_APPLET_CONTAINER (g_async_result_get_source_object (G_ASYNC_RESULT (result)));
	g_hash_table_remove (container->priv->pending_ops, result);
	g_simple_async_result_complete (result);
	g_object_unref (result);

	/* g_async_result_get_source_object returns new ref */
	g_object_unref (container);
}

static void
panel_applet_container_set_applet_property (PanelAppletContainer *container,
					    const gchar          *property_name,
					    EggDBusVariant       *variant,
					    GCancellable         *cancellable,
					    GAsyncReadyCallback   callback,
					    gpointer              user_data)
{
	EggDBusObjectProxy *proxy = container->priv->applet_proxy;
	GSimpleAsyncResult *result;

	if (!proxy)
		return;

	result = g_simple_async_result_new (G_OBJECT (container),
					    callback,
					    user_data,
					    panel_applet_container_set_applet_property);

	if (cancellable)
		g_object_ref (cancellable);
	else
		cancellable = g_cancellable_new ();
	g_hash_table_insert (container->priv->pending_ops, result, cancellable);

	egg_dbus_properties_set (EGG_DBUS_QUERY_INTERFACE_PROPERTIES (proxy),
				 EGG_DBUS_CALL_FLAGS_NONE,
				 PANEL_APPLET_INTERFACE,
				 property_name,
				 variant,
				 cancellable,
				 set_applet_property_cb,
				 result);
}

static void
get_applet_property_cb (GObject      *source_object,
			GAsyncResult *res,
			gpointer      user_data)
{
	GSimpleAsyncResult   *result = G_SIMPLE_ASYNC_RESULT (user_data);
	PanelAppletContainer *container;
	EggDBusVariant       *variant = NULL;
	GError               *error = NULL;

	if (!egg_dbus_properties_get_finish (EGG_DBUS_PROPERTIES (source_object), &variant, res, &error)) {
		if (!g_error_matches (error, EGG_DBUS_ERROR, EGG_DBUS_ERROR_CANCELLED))
			g_warning ("Error getting property: %s\n", error->message);
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	} else {
		g_simple_async_result_set_op_res_gpointer (result, variant,
							   (GDestroyNotify)g_object_unref);
	}

	container = PANEL_APPLET_CONTAINER (g_async_result_get_source_object (G_ASYNC_RESULT (result)));
	g_hash_table_remove (container->priv->pending_ops, result);
	g_simple_async_result_complete (result);
	g_object_unref (result);

	/* g_async_result_get_source_object returns new ref */
	g_object_unref (container);
}

static void
panel_applet_container_get_applet_property (PanelAppletContainer *container,
					    const gchar          *property_name,
					    GCancellable         *cancellable,
					    GAsyncReadyCallback   callback,
					    gpointer              user_data)
{
	EggDBusObjectProxy *proxy = container->priv->applet_proxy;
	GSimpleAsyncResult *result;

	if (!proxy)
		return;

	result = g_simple_async_result_new (G_OBJECT (container),
					    callback,
					    user_data,
					    panel_applet_container_get_applet_property);
	if (cancellable)
		g_object_ref (cancellable);
	else
		cancellable = g_cancellable_new ();
	g_hash_table_insert (container->priv->pending_ops, result, cancellable);

	egg_dbus_properties_get (EGG_DBUS_QUERY_INTERFACE_PROPERTIES (proxy),
				 EGG_DBUS_CALL_FLAGS_NONE,
				 PANEL_APPLET_INTERFACE,
				 property_name,
				 cancellable,
				 get_applet_property_cb,
				 result);
}

GtkWidget *
panel_applet_container_new (void)
{
	GtkWidget *container;

	container = GTK_WIDGET (g_object_new (PANEL_TYPE_APPLET_CONTAINER, NULL));

	return container;
}

static void
panel_applet_container_child_move (_PanelApplet         *instance,
				   PanelAppletContainer *container)
{
	g_signal_emit_by_name (container, "applet-move");
}

static void
panel_applet_container_child_remove (_PanelApplet         *instance,
				     PanelAppletContainer *container)
{
	g_signal_emit_by_name (container, "applet-remove");
}

static void
panel_applet_container_child_lock (_PanelApplet         *instance,
				   PanelAppletContainer *container)
{
	g_signal_emit_by_name (container, "applet-lock", TRUE);
}

static void
panel_applet_container_child_unlock (_PanelApplet         *instance,
				     PanelAppletContainer *container)
{
	g_signal_emit_by_name (container, "applet-lock", FALSE);
}

static void
panel_applet_container_child_size_hints_changed (_PanelApplet         *instance,
						 GParamSpec           *pscpec,
						 PanelAppletContainer *container)
{
	/* FIXME: this is very inefficient, EgDBusChanged already
	 * contains a map with the new prop values, but we don't have
	 * access to that signal. Calling g_object_get() here would
	 * call GetAll synchronously so we emit the prop change and
	 * frame will ask again the value of the prop
	 */
	g_signal_emit_by_name (container, "child-notify::size-hints", pscpec);
}

static void
panel_applet_container_child_flags_changed (_PanelApplet         *instance,
					    GParamSpec           *pscpec,
					    PanelAppletContainer *container)
{
	/* FIXME: this is very inefficient, EgDBusChanged already
	 * contains a map with the new prop values, but we don't have
	 * access to that signal. Calling g_object_get() here would
	 * call GetAll synchronously so we emit the prop change and
	 * frame will ask again the value of the prop
	 */
	g_signal_emit_by_name (container, "child-notify::flags", pscpec);
}

static gboolean
panel_applet_container_plug_removed (PanelAppletContainer *container)
{
	if (!container->priv->applet_proxy)
		return FALSE;

	panel_applet_container_cancel_pending_operations (container);
	g_object_unref (container->priv->applet_proxy);
	container->priv->applet_proxy = NULL;

	g_signal_emit (container, signals[APPLET_BROKEN], 0);

	/* Continue destroying, in case of reloading
	 * a new frame widget is created
	 */
	return FALSE;
}

static void
get_applet_cb (GObject      *source_object,
	       GAsyncResult *res,
	       gpointer      user_data)
{
	EggDBusConnection    *connection = EGG_DBUS_CONNECTION (source_object);
	GSimpleAsyncResult   *result = G_SIMPLE_ASYNC_RESULT (user_data);
	PanelAppletContainer *container;
	EggDBusMessage       *reply;
	guint32               xid = 0;
	GError               *error = NULL;

	container = PANEL_APPLET_CONTAINER (g_async_result_get_source_object (G_ASYNC_RESULT (result)));
	reply = egg_dbus_connection_send_message_with_reply_finish (connection, res, &error);
	if (reply) {
		gchar *applet_path;

		if (egg_dbus_message_extract_object_path (reply, &applet_path, &error)) {
			container->priv->applet_proxy =
				egg_dbus_connection_get_object_proxy (container->priv->connection,
								      container->priv->bus_name,
								      applet_path);
			g_signal_connect (_PANEL_QUERY_INTERFACE_APPLET (container->priv->applet_proxy),
					  "move",
					  G_CALLBACK (panel_applet_container_child_move),
					  container);
			g_signal_connect (_PANEL_QUERY_INTERFACE_APPLET (container->priv->applet_proxy),
					  "remove-from-panel",
					  G_CALLBACK (panel_applet_container_child_remove),
					  container);
			g_signal_connect (_PANEL_QUERY_INTERFACE_APPLET (container->priv->applet_proxy),
					  "lock",
					  G_CALLBACK (panel_applet_container_child_lock),
					  container);
			g_signal_connect (_PANEL_QUERY_INTERFACE_APPLET (container->priv->applet_proxy),
					  "unlock",
					  G_CALLBACK (panel_applet_container_child_unlock),
					  container);
			/* Frame is only interested in size-hints and flags so
			 * we don't notify any other property changes.
			 * Connecting directly to egg-dbus-changed would be better
			 * but it doesn't work :-(
			 */
			g_signal_connect (_PANEL_QUERY_INTERFACE_APPLET (container->priv->applet_proxy),
					  "notify::size-hints",
					  G_CALLBACK (panel_applet_container_child_size_hints_changed),
					  container);
			g_signal_connect (_PANEL_QUERY_INTERFACE_APPLET (container->priv->applet_proxy),
					  "notify::flags",
					  G_CALLBACK (panel_applet_container_child_flags_changed),
					  container);
			g_free (applet_path);
			egg_dbus_message_extract_uint (reply, &xid, &error);
		}
		g_object_unref (reply);
	}

	if (error) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	}

	g_simple_async_result_complete (result);
	g_object_unref (result);

	if (xid > 0)
		gtk_socket_add_id (GTK_SOCKET (container->priv->socket), xid);

	/* g_async_result_get_source_object returns new ref */
	g_object_unref (container);
}

static void
panel_applet_container_get_applet (PanelAppletContainer *container,
				   const gchar          *iid,
				   EggDBusHashMap       *props,
				   GCancellable         *cancellable,
				   GAsyncReadyCallback   callback,
				   gpointer              user_data)
{
	EggDBusMessage     *message;
	GSimpleAsyncResult *result;
	gint                screen;
	gchar              *bus_name;
	gchar              *object_path;
	gchar              *factory_id;
	gchar              *applet_id;

	result = g_simple_async_result_new (G_OBJECT (container),
					    callback,
					    user_data,
					    panel_applet_container_get_applet);

	if (!container->priv->connection) {
		container->priv->connection = egg_dbus_connection_get_for_bus (EGG_DBUS_BUS_TYPE_SESSION);
		if (!container->priv->connection) {
			g_simple_async_result_set_error (result,
							 EGG_DBUS_ERROR,
							 EGG_DBUS_ERROR_DBUS_FAILED,
							 "%s", "Failed to connect to the D-BUS daemon");
			g_simple_async_result_complete (result);
			g_object_unref (result);

			return;
		}
	}

	applet_id = g_strrstr (iid, "::");
	if (!applet_id) {
		g_simple_async_result_set_error (result,
						 PANEL_APPLET_CONTAINER_ERROR,
						 PANEL_APPLET_CONTAINER_INVALID_APPLET,
						 "Invalid applet iid: %s", iid);
		g_simple_async_result_complete (result);
		g_object_unref (result);

		return;
	}

	factory_id = g_strndup (iid, strlen (iid) - strlen (applet_id));
	applet_id += 2;

	screen = gdk_screen_get_number (gtk_widget_get_screen (container->priv->socket));

	object_path = g_strdup_printf (PANEL_APPLET_FACTORY_OBJECT_PATH, factory_id);
	bus_name = g_strdup_printf (PANEL_APPLET_BUS_NAME, factory_id);

	if (container->priv->bus_name)
		g_free (container->priv->bus_name);
	container->priv->bus_name = bus_name;

	message = egg_dbus_connection_new_message_for_method_call (container->priv->connection,
								   NULL,
								   bus_name,
								   object_path,
								   PANEL_APPLET_FACTORY_INTERFACE,
								   "GetApplet");
	egg_dbus_message_append_string (message, applet_id, NULL);
	egg_dbus_message_append_int (message, screen, NULL);
	egg_dbus_message_append_map (message, props, "s", "v", NULL);

	egg_dbus_connection_send_message_with_reply (container->priv->connection,
						     EGG_DBUS_CALL_FLAGS_NONE,
						     message,
						     NULL,
						     cancellable,
						     get_applet_cb,
						     result);
	g_free (factory_id);
	g_free (object_path);
	g_object_unref (message);
}

static EggDBusHashMap *
contruct_properties_map (PanelAppletContainer *container,
			 const gchar          *first_prop_name,
			 va_list               var_args)
{
	EggDBusHashMap *map;
	const gchar    *name;

	map = egg_dbus_hash_map_new (G_TYPE_STRING,
				     g_free,
				     EGG_DBUS_TYPE_VARIANT,
				     g_object_unref);

	name = first_prop_name;
	while (name) {
		const AppletPropertyInfo *info;
		EggDBusVariant           *variant;
		GValue                   *value;
		gchar                    *error = NULL;

		info = panel_applet_container_child_property_get_info (name);
		if (!info) {
			g_warning ("%s: Applet has no child property named `%s'",
				   G_STRLOC, name);
			break;
		}

		value = g_new0 (GValue, 1);
		g_value_init (value, info->type);
		G_VALUE_COLLECT (value, var_args, 0, &error);
		if (error) {
			g_warning ("%s: %s", G_STRLOC, error);
			g_free (error);
			g_value_unset (value);
			g_free (value);

			break;
		}

		if (G_VALUE_HOLDS_STRING (value) &&
		    strlen (g_value_get_string (value)) == 0) {
			name = va_arg (var_args, gchar*);
			continue;
		}

		/* For some reason libpanel-applet and panel use
		 * a different logic for orientation, so we need
		 * to convert it. We should fix this.
		 */
		if (strcmp (name, "orient") == 0) {
			_PanelAppletOrient orient;

			orient = get_panel_applet_orient (g_value_get_uint (value));
			g_value_set_uint (value, orient);
		}

		variant = egg_dbus_variant_new_for_gvalue (value, info->signature);
		egg_dbus_hash_map_insert (map, g_strdup (name), variant);
		name = va_arg (var_args, gchar*);
	}

	return map;
}

void
panel_applet_container_add (PanelAppletContainer *container,
			    const gchar          *iid,
			    GCancellable         *cancellable,
			    GAsyncReadyCallback   callback,
			    gpointer              user_data,
			    const gchar          *first_prop_name,
			    ...)
{
	EggDBusHashMap *map;
	va_list         var_args;

	g_return_if_fail (PANEL_IS_APPLET_CONTAINER (container));
	g_return_if_fail (iid != NULL);

	panel_applet_container_cancel_pending_operations (container);

	va_start (var_args, first_prop_name);
	map = contruct_properties_map (container, first_prop_name, var_args);
	va_end (var_args);

	panel_applet_container_get_applet (container, iid, map, cancellable,
					   callback, user_data);
	g_object_unref (map);
}

gboolean
panel_applet_container_add_finish (PanelAppletContainer *container,
				   GAsyncResult         *result,
				   GError              **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == panel_applet_container_get_applet);

	return !g_simple_async_result_propagate_error (simple, error);
}

/* Child Properties */
void
panel_applet_container_child_set_property (PanelAppletContainer *container,
					   const gchar          *property_name,
					   const GValue         *value,
					   GCancellable         *cancellable,
					   GAsyncReadyCallback   callback,
					   gpointer              user_data)
{
	const AppletPropertyInfo *info;
	EggDBusVariant           *variant;

	info = panel_applet_container_child_property_get_info (property_name);
	if (!info) {
		g_simple_async_report_error_in_idle (G_OBJECT (container),
						     callback, user_data,
						     PANEL_APPLET_CONTAINER_ERROR,
						     PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY,
						     "%s: Applet has no child property named `%s'",
						     G_STRLOC, property_name);
		return;
	}

	variant = egg_dbus_variant_new_for_gvalue (value, info->signature);
	panel_applet_container_set_applet_property (container,
						    info->dbus_name,
						    variant,
						    cancellable,
						    callback,
						    user_data);
	g_object_unref (variant);
}

gboolean
panel_applet_container_child_set_property_finish (PanelAppletContainer *container,
						  GAsyncResult         *result,
						  GError              **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == panel_applet_container_set_applet_property);

	return !g_simple_async_result_propagate_error (simple, error);
}

void
panel_applet_container_child_set_uint (PanelAppletContainer *container,
				       const gchar          *property_name,
				       guint                 value,
				       GCancellable         *cancellable,
				       GAsyncReadyCallback   callback,
				       gpointer              user_data)
{
	const AppletPropertyInfo *info;
	EggDBusVariant           *variant;

	info = panel_applet_container_child_property_get_info (property_name);
	if (!info) {
		g_simple_async_report_error_in_idle (G_OBJECT (container),
						     callback, user_data,
						     PANEL_APPLET_CONTAINER_ERROR,
						     PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY,
						     "%s: Applet has no child property named `%s'",
						     G_STRLOC, property_name);
		return;
	}

	variant = egg_dbus_variant_new_for_uint (value);
	panel_applet_container_set_applet_property (container,
						    info->dbus_name,
						    variant,
						    cancellable,
						    callback,
						    user_data);
	g_object_unref (variant);
}

gboolean
panel_applet_container_child_set_uint_finish (PanelAppletContainer *container,
					      GAsyncResult         *result,
					      GError              **error)
{
	return panel_applet_container_child_set_property_finish (container, result, error);
}

void
panel_applet_container_child_set_boolean (PanelAppletContainer *container,
					  const gchar          *property_name,
					  gboolean              value,
					  GCancellable         *cancellable,
					  GAsyncReadyCallback   callback,
					  gpointer              user_data)
{
	const AppletPropertyInfo *info;
	EggDBusVariant           *variant;

	info = panel_applet_container_child_property_get_info (property_name);
	if (!info) {
		g_simple_async_report_error_in_idle (G_OBJECT (container),
						     callback, user_data,
						     PANEL_APPLET_CONTAINER_ERROR,
						     PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY,
						     "%s: Applet has no child property named `%s'",
						     G_STRLOC, property_name);
		return;
	}

	variant = egg_dbus_variant_new_for_boolean (value);
	panel_applet_container_set_applet_property (container,
						    info->dbus_name,
						    variant,
						    cancellable,
						    callback,
						    user_data);
	g_object_unref (variant);
}

gboolean
panel_applet_container_child_set_boolean_finish (PanelAppletContainer *container,
						 GAsyncResult         *result,
						 GError              **error)
{
	return panel_applet_container_child_set_property_finish (container, result, error);
}

void
panel_applet_container_child_set_string (PanelAppletContainer *container,
					 const gchar          *property_name,
					 const gchar          *value,
					 GCancellable         *cancellable,
					 GAsyncReadyCallback   callback,
					 gpointer              user_data)
{
	const AppletPropertyInfo *info;
	EggDBusVariant           *variant;

	info = panel_applet_container_child_property_get_info (property_name);
	if (!info) {
		g_simple_async_report_error_in_idle (G_OBJECT (container),
						     callback, user_data,
						     PANEL_APPLET_CONTAINER_ERROR,
						     PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY,
						     "%s: Applet has no child property named `%s'",
						     G_STRLOC, property_name);
		return;
	}

	variant = egg_dbus_variant_new_for_string (value);
	panel_applet_container_set_applet_property (container,
						    info->dbus_name,
						    variant,
						    cancellable,
						    callback,
						    user_data);
	g_object_unref (variant);
}

gboolean
panel_applet_container_child_set_string_finish (PanelAppletContainer *container,
						GAsyncResult         *result,
						GError              **error)
{
	return panel_applet_container_child_set_property_finish (container, result, error);
}

void
panel_applet_container_child_set_size_hints (PanelAppletContainer *container,
					     const gint           *size_hints,
					     guint                 n_hints,
					     GCancellable         *cancellable,
					     GAsyncReadyCallback   callback,
					     gpointer              user_data)
{
	const AppletPropertyInfo *info;
	EggDBusVariant           *variant;
	EggDBusArraySeq          *seq;
	gint                      i;

	info = panel_applet_container_child_property_get_info ("size-hints");

	seq = egg_dbus_array_seq_new (G_TYPE_INT, NULL, NULL, NULL);
	for (i = 0; i < n_hints; i++)
		egg_dbus_array_seq_add_fixed (seq, (guint64)size_hints[i]);
	variant = egg_dbus_variant_new_for_seq (seq, info->signature);
	g_object_unref (seq);
	panel_applet_container_set_applet_property (container,
						    info->dbus_name,
						    variant,
						    cancellable,
						    callback,
						    user_data);
	g_object_unref (variant);
}

gboolean
panel_applet_container_child_set_size_hints_finish (PanelAppletContainer *container,
						    GAsyncResult         *result,
						    GError              **error)
{
	return panel_applet_container_child_set_property_finish (container, result, error);
}

void
panel_applet_container_child_set_orientation (PanelAppletContainer *container,
					      PanelOrientation      orientation,
					      GCancellable         *cancellable,
					      GAsyncReadyCallback   callback,
					      gpointer              user_data)
{
	panel_applet_container_child_set_uint (container, "orient",
					       get_panel_applet_orient (orientation),
					       cancellable, callback, user_data);
}

gboolean
panel_applet_container_child_set_orientation_finish (PanelAppletContainer *container,
						     GAsyncResult         *result,
						     GError              **error)
{
	return panel_applet_container_child_set_property_finish (container, result, error);
}



void
panel_applet_container_child_get_property (PanelAppletContainer *container,
					   const gchar          *property_name,
					   GCancellable         *cancellable,
					   GAsyncReadyCallback   callback,
					   gpointer              user_data)
{
	const AppletPropertyInfo *info;

	info = panel_applet_container_child_property_get_info (property_name);
	if (!info) {
		g_simple_async_report_error_in_idle (G_OBJECT (container),
						     callback, user_data,
						     PANEL_APPLET_CONTAINER_ERROR,
						     PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY,
						     "%s: Applet has no child property named `%s'",
						     G_STRLOC, property_name);
		return;
	}

	panel_applet_container_get_applet_property (container,
						    info->dbus_name,
						    cancellable,
						    callback,
						    user_data);
}

gboolean
panel_applet_container_child_get_property_finish (PanelAppletContainer *container,
						  GValue               *value,
						  GAsyncResult         *result,
						  GError              **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
	EggDBusVariant     *variant;

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	variant = EGG_DBUS_VARIANT (g_simple_async_result_get_op_res_gpointer (simple));
	*value = *egg_dbus_variant_get_gvalue (variant);
	g_value_copy (egg_dbus_variant_get_gvalue (variant), value);

	return TRUE;
}

void
panel_applet_container_child_get_uint (PanelAppletContainer *container,
				       const gchar          *property_name,
				       GCancellable         *cancellable,
				       GAsyncReadyCallback   callback,
				       gpointer              user_data)
{
	panel_applet_container_child_get_property (container,
						   property_name,
						   cancellable,
						   callback,
						   user_data);
}

gboolean
panel_applet_container_child_get_uint_finish (PanelAppletContainer *container,
					      guint                *value,
					      GAsyncResult         *result,
					      GError              **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
	EggDBusVariant     *variant;

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	variant = EGG_DBUS_VARIANT (g_simple_async_result_get_op_res_gpointer (simple));
	*value = egg_dbus_variant_get_uint (variant);

	return TRUE;
}

void
panel_applet_container_child_get_boolean (PanelAppletContainer *container,
					  const gchar          *property_name,
					  GCancellable         *cancellable,
					  GAsyncReadyCallback   callback,
					  gpointer              user_data)
{
	panel_applet_container_child_get_property (container,
						   property_name,
						   cancellable,
						   callback,
						   user_data);
}

gboolean
panel_applet_container_child_get_boolean_finish (PanelAppletContainer *container,
						 gboolean             *value,
						 GAsyncResult         *result,
						 GError              **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
	EggDBusVariant     *variant;

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	variant = EGG_DBUS_VARIANT (g_simple_async_result_get_op_res_gpointer (simple));
	*value = egg_dbus_variant_get_boolean (variant);

	return TRUE;
}

void
panel_applet_container_child_get_string (PanelAppletContainer *container,
					 const gchar          *property_name,
					 GCancellable         *cancellable,
					 GAsyncReadyCallback   callback,
					 gpointer              user_data)
{
	panel_applet_container_child_get_property (container,
						   property_name,
						   cancellable,
						   callback,
						   user_data);
}

gboolean
panel_applet_container_child_get_string_finish (PanelAppletContainer *container,
						gchar               **value,
						GAsyncResult         *result,
						GError              **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
	EggDBusVariant     *variant;

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	variant = EGG_DBUS_VARIANT (g_simple_async_result_get_op_res_gpointer (simple));
	*value = g_strdup (egg_dbus_variant_get_string (variant));

	return TRUE;
}

void
panel_applet_container_child_get_size_hints (PanelAppletContainer *container,
					     GCancellable         *cancellable,
					     GAsyncReadyCallback   callback,
					     gpointer              user_data)
{
	panel_applet_container_child_get_property (container,
						   "size-hints",
						   cancellable,
						   callback,
						   user_data);
}

gboolean
panel_applet_container_child_get_size_hints_finish (PanelAppletContainer *container,
						    gint                **size_hints,
						    guint                *n_elements,
						    GAsyncResult         *result,
						    GError              **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
	EggDBusVariant     *variant;
	EggDBusArraySeq    *seq;
	gint                i;

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	variant = EGG_DBUS_VARIANT (g_simple_async_result_get_op_res_gpointer (simple));
	seq = egg_dbus_variant_get_seq (variant);
	*n_elements = egg_dbus_array_seq_get_size (seq);
	*size_hints = *n_elements > 0 ? g_new (gint, *n_elements) : NULL;
	for (i = 0; i < *n_elements; i++) {
		size_hints[0][i] = egg_dbus_array_seq_get_fixed (seq, i);

	}
	g_object_unref (seq);

	return TRUE;
}

static void
child_popup_menu_cb (GObject      *source_object,
		     GAsyncResult *res,
		     gpointer      user_data)
{
	EggDBusConnection  *connection = EGG_DBUS_CONNECTION (source_object);
	GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);
	EggDBusMessage     *reply;
	GError             *error = NULL;

	reply = egg_dbus_connection_send_message_with_reply_finish (connection, res, &error);
	if (error) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	}

	if (reply)
		g_object_unref (reply);

	g_simple_async_result_complete (result);
	g_object_unref (result);
}

void
panel_applet_container_child_popup_menu (PanelAppletContainer *container,
					 guint                 button,
					 guint32               timestamp,
					 GCancellable         *cancellable,
					 GAsyncReadyCallback   callback,
					 gpointer              user_data)
{
	EggDBusMessage     *message;
	GSimpleAsyncResult *result;
	EggDBusObjectProxy *proxy = container->priv->applet_proxy;

	if (!proxy)
		return;

	result = g_simple_async_result_new (G_OBJECT (container),
					    callback,
					    user_data,
					    panel_applet_container_child_popup_menu);

	message = egg_dbus_connection_new_message_for_method_call (container->priv->connection,
								   NULL,
								   egg_dbus_object_proxy_get_name (proxy),
								   egg_dbus_object_proxy_get_object_path (proxy),
								   PANEL_APPLET_INTERFACE,
								   "PopupMenu");
	egg_dbus_message_append_uint (message, button, NULL);
	egg_dbus_message_append_uint (message, timestamp, NULL);

	egg_dbus_connection_send_message_with_reply (container->priv->connection,
						     EGG_DBUS_CALL_FLAGS_NONE,
						     message,
						     NULL,
						     cancellable,
						     child_popup_menu_cb,
						     result);
	g_object_unref (message);

}

gboolean
panel_applet_container_child_popup_menu_finish (PanelAppletContainer *container,
						GAsyncResult         *result,
						GError              **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == panel_applet_container_child_popup_menu);

	return !g_simple_async_result_propagate_error (simple, error);
}

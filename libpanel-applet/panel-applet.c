/*
 * panel-applet.c: panel applet writing library.
 *
 * Copyright (c) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2001 Sun Microsystems, Inc.
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
 * Authors:
 *     Mark McLoughlin <mark@skynet.ie>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <cairo.h>
#include <cairo-gobject.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <X11/Xatom.h>
#include <cairo-xlib.h>

#include "panel-applet.h"
#include "panel-applet-bindings.h"
#include "panel-applet-factory.h"
#include "panel-applet-marshal.h"
#include "panel-applet-enums.h"

/**
 * SECTION:applet
 * @short_description: a widget embedded in a panel.
 * @stability: Unstable
 *
 * Applets are small applications that are embedded in the GNOME panel. They
 * can be used to give quick access to some features, or to display the state
 * of something specific.
 *
 * The #PanelApplet API hides all of the embedding process as it handles all
 * the communication with the GNOME panel. It is a subclass of #GtkBin, so you
 * can add any kind of widgets to it.
 *
 * See the <link linkend="getting-started">Getting Started</link> section to
 * learn how to properly use #PanelApplet.
 */

/**
 * SECTION:applet-factory
 * @short_description: the factory that will create applets.
 * @stability: Unstable
 *
 * This API is used to create an <link
 * linkend="getting-started.concepts.applet-factory">applet factory</link>. You
 * need to call one and only one of these functions to get applets working in
 * your binary.
 */

#define PANEL_APPLET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_APPLET, PanelAppletPrivate))

struct _PanelAppletPrivate {
	GtkWidget         *plug;
	GtkWidget         *applet;
	GConfClient       *client;
	GDBusConnection   *connection;

	char              *id;
	GClosure          *closure;
	char              *object_path;
	guint              object_id;
	char              *settings_path;
	char              *prefs_key;

	GtkUIManager      *ui_manager;
	GtkActionGroup    *applet_action_group;
	GtkActionGroup    *panel_action_group;

	PanelAppletFlags   flags;
	PanelAppletOrient  orient;
	guint              size;
	char              *background;
	GtkWidget         *background_widget;

	int                previous_width;
	int                previous_height;

        int               *size_hints;
        int                size_hints_len;

	gboolean           moving_focus_out;

	gboolean           locked_down;
};

enum {
        CHANGE_ORIENT,
        CHANGE_SIZE,
        CHANGE_BACKGROUND,
	MOVE_FOCUS_OUT_OF_APPLET,
        LAST_SIGNAL
};

static guint panel_applet_signals [LAST_SIGNAL];

enum {
	PROP_0,
	PROP_ID,
	PROP_CLOSURE,
	PROP_CONNECTION,
	PROP_SETTINGS_PATH,
	PROP_PREFS_KEY,
	PROP_ORIENT,
	PROP_SIZE,
	PROP_BACKGROUND,
	PROP_FLAGS,
	PROP_SIZE_HINTS,
	PROP_LOCKED_DOWN
};

static void       panel_applet_handle_background   (PanelApplet       *applet);
static GtkAction *panel_applet_menu_get_action     (PanelApplet       *applet,
						    const gchar       *action);
static void       panel_applet_menu_update_actions (PanelApplet       *applet);
static void       panel_applet_menu_cmd_remove     (GtkAction         *action,
						    PanelApplet       *applet);
static void       panel_applet_menu_cmd_move       (GtkAction         *action,
						    PanelApplet       *applet);
static void       panel_applet_register_object     (PanelApplet       *applet);

static const gchar panel_menu_ui[] =
	"<ui>\n"
	"  <popup name=\"PanelAppletPopup\" action=\"PopupAction\">\n"
	"    <placeholder name=\"AppletItems\"/>\n"
	"  </popup>\n"
	"  <popup name=\"PanelAppletEditPopup\" action=\"PopupEditAction\">\n"
	"    <menuitem name=\"MoveItem\" action=\"Move\"/>\n"
	"    <menuitem name=\"RemoveItem\" action=\"Remove\"/>\n"
	"  </popup>\n"
	"</ui>\n";

static const GtkActionEntry menu_entries[] = {
	{ "PopupAction", NULL, "Popup Action",
	  NULL, NULL,
	  NULL },
	{ "Remove", GTK_STOCK_REMOVE, N_("_Remove From Panel"),
	  NULL, NULL,
	  G_CALLBACK (panel_applet_menu_cmd_remove) },
	{ "Move", NULL, N_("_Move"),
	  NULL, NULL,
	  G_CALLBACK (panel_applet_menu_cmd_move) }
};

G_DEFINE_TYPE (PanelApplet, panel_applet, GTK_TYPE_EVENT_BOX)

#define PANEL_APPLET_INTERFACE   "org.gnome.panel.applet.Applet"
#define PANEL_APPLET_OBJECT_PATH "/org/gnome/panel/applet/%s/%d"

static void
panel_applet_associate_schemas_in_dir (GConfClient  *client,
				       const gchar  *prefs_key,
				       const gchar  *schema_dir,
				       GError      **error)
{
	GSList *list, *l;

	g_assert (error != NULL);

	list = gconf_client_all_entries (client, schema_dir, error);

	if (*error != NULL)
		return;

	for (l = list; l; l = l->next) {
		GConfEntry  *entry = l->data;
		const gchar *schema_key;
		GConfEntry  *applet_entry;
		const gchar *applet_schema_key;
		gchar       *key;
		gchar       *tmp;

		schema_key = gconf_entry_get_key (entry);
		tmp = g_path_get_basename (schema_key);

		if (strchr (tmp, '-'))
			g_warning ("Applet key '%s' contains a hyphen. Please "
				   "use underscores in gconf keys\n", tmp);

		key = g_strdup_printf ("%s/%s", prefs_key, tmp);
		g_free (tmp);

		/* Associating a schema is potentially expensive, so let's try
		 * to avoid this by doing it only when needed. So we check if
		 * the key is already correctly associated. */

		applet_entry = gconf_client_get_entry (client, key,
						       NULL, TRUE, NULL);
		if (applet_entry)
			applet_schema_key = gconf_entry_get_schema_name (applet_entry);
		else
			applet_schema_key = NULL;

		if (g_strcmp0 (schema_key, applet_schema_key) != 0) {
			gconf_engine_associate_schema (client->engine,
						       key, schema_key, error);

			if (applet_entry == NULL ||
			    gconf_entry_get_value (applet_entry) == NULL ||
			    gconf_entry_get_is_default (applet_entry)) {
				/* unset the key: gconf_client_get_entry()
				 * brought an invalid entry in the client
				 * cache, and we want to fix this */
				gconf_client_unset (client, key, NULL);
			}
		}

		g_free (key);

		if (applet_entry)
			gconf_entry_unref (applet_entry);
		gconf_entry_unref (entry);

		if (*error) {
			g_slist_free (list);
			return;
		}
	}

	g_slist_free (list);

	list = gconf_client_all_dirs (client, schema_dir, error);

	for (l = list; l; l = l->next) {
		gchar *subdir = l->data;
		gchar *prefs_subdir;
		gchar *schema_subdir;
		gchar *tmp;

		tmp = g_path_get_basename (subdir);

		prefs_subdir  = g_strdup_printf ("%s/%s", prefs_key, tmp);
		schema_subdir = g_strdup_printf ("%s/%s", schema_dir, tmp);

		panel_applet_associate_schemas_in_dir (client,
						       prefs_subdir,
						       schema_subdir,
						       error);

		g_free (prefs_subdir);
		g_free (schema_subdir);
		g_free (subdir);
		g_free (tmp);

		if (*error) {
			g_slist_free (list);
			return;
		}
	}

	g_slist_free (list);
}

/**
 * panel_applet_add_preferences:
 * @applet: a #PanelApplet.
 * @schema_dir: a GConf path of a schema directory.
 * @error: a #GError, or %NULL.
 *
 * Associates the per-instance GConf directory of @applet to the schemas
 * defined in @schema_dir. This must be called if the applet will save
 * per-instance settings, to ensure that each key in the per-instance GConf
 * directory has a defined type, sane default and documentation.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
void
panel_applet_add_preferences (PanelApplet  *applet,
			      const gchar  *schema_dir,
			      GError      **error)
{
	GError **_error = NULL;
	GError  *our_error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));
	g_return_if_fail (schema_dir != NULL);

	if (!applet->priv->prefs_key)
		return;

	/* panel_applet_associate_schemas_in_dir() requires a non-NULL error */
	if (error)
		_error = error;
	else
		_error = &our_error;

	panel_applet_associate_schemas_in_dir (applet->priv->client,
					       applet->priv->prefs_key,
					       schema_dir,
					       _error);

	if (!error && our_error)
		g_error_free (our_error);
}

/**
 * panel_applet_get_preferences_key:
 * @applet: a #PanelApplet.
 *
 * Gets the GConf path to the per-instance settings of @applet.
 *
 * Returns: a copy of the GConf path to the per-instance settings of @applet.
 *
 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
 **/
char *
panel_applet_get_preferences_key (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (!applet->priv->prefs_key)
		return NULL;

	return g_strdup (applet->priv->prefs_key);
}

static void
panel_applet_set_preferences_key (PanelApplet *applet,
				  const char  *prefs_key)
{
	if (applet->priv->prefs_key == prefs_key)
		return;

	if (g_strcmp0 (applet->priv->prefs_key, prefs_key) == 0)
		return;

	if (applet->priv->prefs_key) {
		gconf_client_remove_dir (applet->priv->client,
					 applet->priv->prefs_key,
					 NULL);

		g_free (applet->priv->prefs_key);
		applet->priv->prefs_key = NULL;
	}

	if (prefs_key) {
		applet->priv->prefs_key = g_strdup (prefs_key);

		gconf_client_add_dir (applet->priv->client,
				      applet->priv->prefs_key,
				      GCONF_CLIENT_PRELOAD_RECURSIVE,
				      NULL);
	}

	g_object_notify (G_OBJECT (applet), "prefs-key");
}

/**
 * panel_applet_settings_new:
 * @applet: a #PanelApplet.
 * @schema: the name of the schema.
 *
 * Creates a new #GSettings object for the per-instance settings of @applet,
 * with a given schema.
 *
 * Returns: a new #GSettings object for the per-instance settings of @applet.
 **/
GSettings *
panel_applet_settings_new (PanelApplet *applet,
			   const char  *schema)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);
	g_return_val_if_fail (schema != NULL, NULL);

	if (!applet->priv->settings_path)
		return NULL;

	return g_settings_new_with_path (schema, applet->priv->settings_path);
}

static void
panel_applet_set_settings_path (PanelApplet *applet,
				const char  *settings_path)
{
	if (applet->priv->settings_path == settings_path)
		return;

	if (g_strcmp0 (applet->priv->settings_path, settings_path) == 0)
		return;

	g_free (applet->priv->settings_path);
	applet->priv->settings_path = NULL;

	if (settings_path)
		applet->priv->settings_path = g_strdup (settings_path);

	g_object_notify (G_OBJECT (applet), "settings-path");
}

/**
 * panel_applet_get_flags:
 * @applet: a #PanelApplet.
 *
 * Gets the #PanelAppletFlags of @applet.
 *
 * Returns: the #PanelAppletFlags of @applet.
 **/
PanelAppletFlags
panel_applet_get_flags (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), PANEL_APPLET_FLAGS_NONE);

	return applet->priv->flags;
}

/**
 * panel_applet_set_flags:
 * @applet: a #PanelApplet.
 * @flags: #PanelAppletFlags to use for @applet.
 *
 * Sets the #PanelAppletFlags of @applet. Most of the time, at least
 * %PANEL_APPLET_EXPAND_MINOR should be used.
 **/
void
panel_applet_set_flags (PanelApplet      *applet,
			PanelAppletFlags  flags)
{
	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (applet->priv->flags == flags)
		return;

	applet->priv->flags = flags;

	g_object_notify (G_OBJECT (applet), "flags");

	if (applet->priv->connection) {
		GVariantBuilder *builder;
		GVariantBuilder *invalidated_builder;
		GError          *error = NULL;

		builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
		invalidated_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));

		g_variant_builder_add (builder, "{sv}", "Flags",
				       g_variant_new_uint32 (applet->priv->flags));

		g_dbus_connection_emit_signal (applet->priv->connection,
					       NULL,
					       applet->priv->object_path,
					       "org.freedesktop.DBus.Properties",
					       "PropertiesChanged",
					       g_variant_new ("(sa{sv}as)",
							      PANEL_APPLET_INTERFACE,
							      builder,
							      invalidated_builder),
					       &error);
		if (error) {
			g_printerr ("Failed to send signal PropertiesChanged::Flags: %s\n",
				    error->message);
			g_error_free (error);
		}
	}
}

static void
panel_applet_size_hints_ensure (PanelApplet *applet,
				int          new_size)
{
	if (applet->priv->size_hints && applet->priv->size_hints_len < new_size) {
		g_free (applet->priv->size_hints);
		applet->priv->size_hints = g_new (gint, new_size);
	} else if (!applet->priv->size_hints) {
		applet->priv->size_hints = g_new (gint, new_size);
	}
	applet->priv->size_hints_len = new_size;
}

static gboolean
panel_applet_size_hints_changed (PanelApplet *applet,
				 const int   *size_hints,
				 int          n_elements,
				 int          base_size)
{
	gint i;

	if (!applet->priv->size_hints)
		return TRUE;

	if (applet->priv->size_hints_len != n_elements)
		return TRUE;

	for (i = 0; i < n_elements; i++) {
		if (size_hints[i] + base_size != applet->priv->size_hints[i])
			return TRUE;
	}

	return FALSE;
}

/**
 * panel_applet_set_size_hints:
 * @applet: a #PanelApplet.
 * @size_hints: array of sizes.
 * @n_elements: length of @size_hints.
 * @base_size: base size of the applet.
 *
 * Give hints to the panel about sizes @applet is comfortable with. This is
 * generally useful for applets that can take a lot of space, in case the panel
 * gets full and needs to restrict the size of some applets.
 *
 * @size_hints should have an even number of sizes. It is an array of (max,
 * min) pairs where min(i) > max(i + 1).
 *
 * @base_size will be added to all sizes in @size_hints, and is therefore a way
 * to guarantee a minimum size to @applet.
 *
 * The panel will try to allocate a size that is acceptable to @applet, i.e. in
 * one of the (@base_size + max, @base_size + min) ranges.
 *
 * %PANEL_APPLET_EXPAND_MAJOR must be set for @applet to use size hints.
 **/
void
panel_applet_set_size_hints (PanelApplet *applet,
			     const int   *size_hints,
			     int          n_elements,
			     int          base_size)
{
	gint i;

	/* Make sure property has really changed to avoid bus traffic */
	if (!panel_applet_size_hints_changed (applet, size_hints, n_elements, base_size))
		return;

	panel_applet_size_hints_ensure (applet, n_elements);
	for (i = 0; i < n_elements; i++)
		applet->priv->size_hints[i] = size_hints[i] + base_size;

	g_object_notify (G_OBJECT (applet), "size-hints");

	if (applet->priv->connection) {
		GVariantBuilder *builder;
		GVariantBuilder *invalidated_builder;
		GVariant       **children;
		GError          *error = NULL;

		builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
		invalidated_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));

		children = g_new (GVariant *, applet->priv->size_hints_len);
		for (i = 0; i < n_elements; i++)
			children[i] = g_variant_new_int32 (applet->priv->size_hints[i]);
		g_variant_builder_add (builder, "{sv}", "SizeHints",
				       g_variant_new_array (G_VARIANT_TYPE_INT32,
							    children, applet->priv->size_hints_len));
		g_free (children);

		g_dbus_connection_emit_signal (applet->priv->connection,
					       NULL,
					       applet->priv->object_path,
					       "org.freedesktop.DBus.Properties",
					       "PropertiesChanged",
					       g_variant_new ("(sa{sv}as)",
							      PANEL_APPLET_INTERFACE,
							      builder,
							      invalidated_builder),
					       &error);
		if (error) {
			g_printerr ("Failed to send signal PropertiesChanged::SizeHints: %s\n",
				    error->message);
			g_error_free (error);
		}
	}
}

/**
 * panel_applet_get_size:
 * @applet: a #PanelApplet.
 *
 * Gets the size of the panel @applet is on. For a horizontal panel, the
 * size if the height of the panel; for a vertical panel, the size is the width
 * of the panel.
 *
 * Returns: the size of the panel @applet is on.
 *
 * Deprecated: 3.0: Use the allocation of @applet instead.
 **/
guint
panel_applet_get_size (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), 0);

	return applet->priv->size;
}

/* Applets cannot set their size, so API is not public. */
static void
panel_applet_set_size (PanelApplet *applet,
		       guint        size)
{
	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (applet->priv->size == size)
		return;

	applet->priv->size = size;
	g_signal_emit (G_OBJECT (applet),
		       panel_applet_signals [CHANGE_SIZE],
		       0, size);

	g_object_notify (G_OBJECT (applet), "size");
}

/**
 * panel_applet_get_orient:
 * @applet: a #PanelApplet.
 *
 * Gets the #PanelAppletOrient of @applet.
 *
 * Returns: the #PanelAppletOrient of @applet.
 **/
PanelAppletOrient
panel_applet_get_orient (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), 0);

	return applet->priv->orient;
}

/* Applets cannot set their orientation, so API is not public. */
static void
panel_applet_set_orient (PanelApplet      *applet,
			 PanelAppletOrient orient)
{
	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (applet->priv->orient == orient)
		return;

	applet->priv->orient = orient;
	g_signal_emit (G_OBJECT (applet),
		       panel_applet_signals [CHANGE_ORIENT],
		       0, orient);

	g_object_notify (G_OBJECT (applet), "orient");
}

/**
 * panel_applet_get_locked_down:
 * @applet: a #PanelApplet.
 *
 * Gets whether the panel @applet is on is locked down or not. A locked down
 * applet should not allow any change to its configuration.
 *
 * Returns: %TRUE if the panel @applet is on is locked down, %FALSE otherwise.
 **/
gboolean
panel_applet_get_locked_down (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), FALSE);

	return applet->priv->locked_down;
}

/* Applets cannot set the lockdown state, so API is not public. */
static void
panel_applet_set_locked_down (PanelApplet *applet,
			      gboolean     locked_down)
{
	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (applet->priv->locked_down == locked_down)
		return;

	applet->priv->locked_down = locked_down;
	panel_applet_menu_update_actions (applet);

	g_object_notify (G_OBJECT (applet), "locked-down");
}

static Atom _net_wm_window_type = None;
static Atom _net_wm_window_type_dock = None;
static Atom _net_active_window = None;

static void
panel_applet_init_atoms (Display *xdisplay)
{
	if (_net_wm_window_type == None)
		_net_wm_window_type = XInternAtom (xdisplay,
						   "_NET_WM_WINDOW_TYPE",
						   False);

	if (_net_wm_window_type_dock == None)
		_net_wm_window_type_dock = XInternAtom (xdisplay,
							"_NET_WM_WINDOW_TYPE_DOCK",
							False);

	if (_net_active_window == None)
		_net_active_window = XInternAtom (xdisplay,
						  "_NET_ACTIVE_WINDOW",
						  False);
}

static Window
panel_applet_find_toplevel_dock_window (PanelApplet *applet,
					Display	    *xdisplay)
{
	GtkWidget  *toplevel;
	Window	    xwin;
	Window	    root, parent, *child;
	int	    num_children;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (applet));
	if (!gtk_widget_get_realized (toplevel))
		return None;

	xwin = GDK_WINDOW_XID (gtk_widget_get_window (toplevel));

	child = NULL;
	parent = root = None;
	do {
		Atom	type_return;
		Atom	window_type;
		int	format_return;
		gulong	number_return, bytes_after_return;
		guchar *data_return;

		XGetWindowProperty (xdisplay,
				    xwin,
				    _net_wm_window_type,
				    0, 1, False,
				    XA_ATOM,
				    &type_return, &format_return,
				    &number_return,
				    &bytes_after_return,
				    &data_return);

		if (type_return == XA_ATOM) {
			window_type = *(Atom *) data_return;

			XFree (data_return);
			data_return = NULL;

			if (window_type == _net_wm_window_type_dock)
				return xwin;
		}

		if (!XQueryTree (xdisplay,
			   xwin,
			   &root, &parent, &child,
			   (guint *) &num_children)) {
			   return None;
		}

		if (child && num_children > 0)
			XFree (child);

		xwin = parent;

	} while (xwin != None && xwin != root);

	return None;
}

/**
 * panel_applet_request_focus:
 * @applet: a #PanelApplet.
 * @timestamp: the timestamp of the user interaction (typically a button or key
 * press event) which triggered this call.
 *
 * Requests focus for @applet. There is no guarantee that @applet will
 * successfully get focus after that call.
 **/
/* This function
 *   1) Gets the window id of the panel that contains the applet
 *	using XQueryTree and XGetWindowProperty to find an ancestor
 *	window with the _NET_WM_WINDOW_TYPE_DOCK window type.
 *   2) Sends a _NET_ACTIVE_WINDOW message to get that panel focused
 */
void
panel_applet_request_focus (PanelApplet	 *applet,
			    guint32	  timestamp)
{
	GdkScreen  *screen;
	GdkWindow  *root;
	GdkDisplay *display;
	Display	   *xdisplay;
	Window	    dock_xwindow;
	Window	    xroot;
	XEvent	    xev;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	screen	= gtk_widget_get_screen (GTK_WIDGET (applet));
	root	= gdk_screen_get_root_window (screen);
	display = gdk_screen_get_display (screen);

	xdisplay = GDK_DISPLAY_XDISPLAY (display);
	xroot	 = GDK_WINDOW_XID (root);

	panel_applet_init_atoms (xdisplay);

	dock_xwindow = panel_applet_find_toplevel_dock_window (applet, xdisplay);
	if (dock_xwindow == None)
		return;

	xev.xclient.type	 = ClientMessage;
	xev.xclient.serial	 = 0;
	xev.xclient.send_event	 = True;
	xev.xclient.window	 = dock_xwindow;
	xev.xclient.message_type = _net_active_window;
	xev.xclient.format	 = 32;
	xev.xclient.data.l[0]	 = 1; /* requestor type; we're an app, I guess */
	xev.xclient.data.l[1]	 = timestamp;
	xev.xclient.data.l[2]	 = None; /* "currently active window", supposedly */
	xev.xclient.data.l[3]	 = 0;
	xev.xclient.data.l[4]	 = 0;

	XSendEvent (xdisplay,
		    xroot, False,
		    SubstructureRedirectMask | SubstructureNotifyMask,
		    &xev);
}

static GtkAction *
panel_applet_menu_get_action (PanelApplet *applet,
			      const gchar *action)
{
	return gtk_action_group_get_action (applet->priv->panel_action_group, action);
}

static void
panel_applet_menu_update_actions (PanelApplet *applet)
{
	gboolean locked_down = applet->priv->locked_down;

	g_object_set (panel_applet_menu_get_action (applet, "Move"),
		      "visible", !locked_down,
		      NULL);
	g_object_set (panel_applet_menu_get_action (applet, "Remove"),
		      "visible", !locked_down,
		      NULL);
}

static void
panel_applet_menu_cmd_remove (GtkAction   *action,
			      PanelApplet *applet)
{
	GError *error = NULL;

	if (!applet->priv->connection)
		return;

	g_dbus_connection_emit_signal (applet->priv->connection,
				       NULL,
				       applet->priv->object_path,
				       PANEL_APPLET_INTERFACE,
				       "RemoveFromPanel",
				       NULL, &error);
	if (error) {
		g_printerr ("Failed to send signal RemoveFromPanel: %s\n",
			    error->message);
		g_error_free (error);
	}
}

static void
panel_applet_menu_cmd_move (GtkAction   *action,
			    PanelApplet *applet)
{
	GError *error = NULL;

	if (!applet->priv->connection)
		return;

	g_dbus_connection_emit_signal (applet->priv->connection,
				       NULL,
				       applet->priv->object_path,
				       PANEL_APPLET_INTERFACE,
				       "Move",
				       NULL, &error);
	if (error) {
		g_printerr ("Failed to send signal RemoveFromPanel: %s\n",
			    error->message);
		g_error_free (error);
	}
}

/**
 * panel_applet_setup_menu:
 * @applet: a #PanelApplet.
 * @xml: a menu XML string.
 * @action_group: a #GtkActionGroup.
 *
 * Sets up the context menu of @applet. @xml is a #GtkUIManager UI definition,
 * describing how to display the menu items. @action_group contains the
 * various #GtkAction that are referenced in @xml.
 *
 * See also the <link linkend="getting-started.context-menu">Context
 * Menu</link> section.
 **/
void
panel_applet_setup_menu (PanelApplet    *applet,
			 const gchar    *xml,
			 GtkActionGroup *action_group)
{
	gchar  *new_xml;
	GError *error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));
	g_return_if_fail (xml != NULL);

	if (applet->priv->applet_action_group)
		return;

	applet->priv->applet_action_group = g_object_ref (action_group);
	gtk_ui_manager_insert_action_group (applet->priv->ui_manager,
					    action_group, 0);

	new_xml = g_strdup_printf ("<ui><popup name=\"PanelAppletPopup\" action=\"AppletItems\">"
				   "<placeholder name=\"AppletItems\">%s\n</placeholder>\n"
				   "</popup></ui>\n", xml);
	gtk_ui_manager_add_ui_from_string (applet->priv->ui_manager, new_xml, -1, &error);
	g_free (new_xml);
	gtk_ui_manager_ensure_update (applet->priv->ui_manager);
	if (error) {
		g_warning ("Error merging menus: %s\n", error->message);
		g_error_free (error);
	}
}

/**
 * panel_applet_setup_menu_from_file:
 * @applet: a #PanelApplet.
 * @filename: path to a menu XML file.
 * @action_group: a #GtkActionGroup.
 *
 * Sets up the context menu of @applet. @filename is the path to a menu XML
 * file, containing a #GtkUIManager UI definition that describes how to display
 * the menu items. @action_group contains the various #GtkAction that
 * are referenced in @xml.
 *
 * See also the <link linkend="getting-started.context-menu">Context
 * Menu</link> section.
 **/
void
panel_applet_setup_menu_from_file (PanelApplet    *applet,
				   const gchar    *filename,
				   GtkActionGroup *action_group)
{
	gchar  *xml = NULL;
	GError *error = NULL;

	if (g_file_get_contents (filename, &xml, NULL, &error)) {
		panel_applet_setup_menu (applet, xml, action_group);
	} else {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_free (xml);
}

static void
panel_applet_finalize (GObject *object)
{
	PanelApplet *applet = PANEL_APPLET (object);

	if (applet->priv->connection) {
		if (applet->priv->object_id)
			g_dbus_connection_unregister_object (applet->priv->connection,
							     applet->priv->object_id);
		applet->priv->object_id = 0;
		g_object_unref (applet->priv->connection);
		applet->priv->connection = NULL;
	}

	if (applet->priv->object_path) {
		g_free (applet->priv->object_path);
		applet->priv->object_path = NULL;
	}

	panel_applet_set_preferences_key (applet, NULL);

	panel_applet_bindings_clean (applet->priv->client);

	if (applet->priv->client)
		g_object_unref (applet->priv->client);
	applet->priv->client = NULL;

	if (applet->priv->applet_action_group) {
		g_object_unref (applet->priv->applet_action_group);
		applet->priv->applet_action_group = NULL;
	}

	if (applet->priv->panel_action_group) {
		g_object_unref (applet->priv->panel_action_group);
		applet->priv->panel_action_group = NULL;
	}

	if (applet->priv->ui_manager) {
		g_object_unref (applet->priv->ui_manager);
		applet->priv->ui_manager = NULL;
	}

	g_free (applet->priv->size_hints);
	g_free (applet->priv->prefs_key);
	g_free (applet->priv->settings_path);
	g_free (applet->priv->background);
	g_free (applet->priv->id);

	/* closure is owned by the factory */
	applet->priv->closure = NULL;

	G_OBJECT_CLASS (panel_applet_parent_class)->finalize (object);
}

static gboolean
container_has_focusable_child (GtkContainer *container)
{
	GtkWidget *child;
	GList *list;
	GList *t;
	gboolean retval = FALSE;

	list = gtk_container_get_children (container);

	for (t = list; t; t = t->next) {
		child = GTK_WIDGET (t->data);
		if (gtk_widget_get_can_focus (child)) {
			retval = TRUE;
			break;
		} else if (GTK_IS_CONTAINER (child)) {
			retval = container_has_focusable_child (GTK_CONTAINER (child));
			if (retval)
				break;
		}
	}
	g_list_free (list);
	return retval;
}

static void
panel_applet_position_menu (GtkMenu   *menu,
			    int       *x,
			    int       *y,
			    gboolean  *push_in,
			    GtkWidget *widget)
{
	PanelApplet    *applet;
	GtkAllocation   allocation;
	GtkRequisition  requisition;
	GdkScreen      *screen;
	int             menu_x = 0;
	int             menu_y = 0;
	int             pointer_x;
	int             pointer_y;

	g_return_if_fail (PANEL_IS_APPLET (widget));

	applet = PANEL_APPLET (widget);

	screen = gtk_widget_get_screen (widget);

	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &requisition, NULL);
	gdk_window_get_origin (gtk_widget_get_window (widget),
			       &menu_x, &menu_y);
	gtk_widget_get_pointer (widget, &pointer_x, &pointer_y);

	gtk_widget_get_allocation (widget, &allocation);

	menu_x += allocation.x;
	menu_y += allocation.y;

	if (applet->priv->orient == PANEL_APPLET_ORIENT_UP ||
	    applet->priv->orient == PANEL_APPLET_ORIENT_DOWN) {
		if (gtk_widget_get_direction (GTK_WIDGET (menu)) != GTK_TEXT_DIR_RTL) {
			if (pointer_x < allocation.width &&
			    requisition.width < pointer_x)
				menu_x += MIN (pointer_x,
					       allocation.width - requisition.width);
		} else {
			menu_x += allocation.width - requisition.width;
			if (pointer_x > 0 && pointer_x < allocation.width &&
			    pointer_x < allocation.width - requisition.width) {
				menu_x -= MIN (allocation.width - pointer_x,
					       allocation.width - requisition.width);
			}
		}
		menu_x = MIN (menu_x, gdk_screen_get_width (screen) - requisition.width);

		if (menu_y > gdk_screen_get_height (screen) / 2)
			menu_y -= requisition.height;
		else
			menu_y += allocation.height;
	} else  {
		if (pointer_y < allocation.height &&
		    requisition.height < pointer_y)
			menu_y += MIN (pointer_y, allocation.height - requisition.height);
		menu_y = MIN (menu_y, gdk_screen_get_height (screen) - requisition.height);

		if (menu_x > gdk_screen_get_width (screen) / 2)
			menu_x -= requisition.width;
		else
			menu_x += allocation.width;

	}

	*x = menu_x;
	*y = menu_y;
	*push_in = TRUE;
}

static void
panel_applet_menu_popup (PanelApplet *applet,
			 guint        button,
			 guint32      time)
{
	GtkWidget *menu;
	GList     *children, *l;
	gboolean   visible = FALSE;

	menu = gtk_ui_manager_get_widget (applet->priv->ui_manager,
					  "/PanelAppletPopup");

	children = gtk_container_get_children (GTK_CONTAINER (menu));
	for (l = children; l != NULL; l = l->next) {
		visible = gtk_widget_get_visible (GTK_WIDGET (l->data));
		if (visible)
			break;
	}
	g_list_free (children);

	if (!visible)
		return;

	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL,
			(GtkMenuPositionFunc) panel_applet_position_menu,
			applet,
			button, time);
}

static void
panel_applet_edit_menu_popup (PanelApplet *applet,
			      guint        button,
			      guint32      time)
{
	GtkWidget *menu;

	if (applet->priv->locked_down)
		return;

	menu = gtk_ui_manager_get_widget (applet->priv->ui_manager,
					  "/PanelAppletEditPopup");
	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL,
			NULL,
			applet,
			button, time);
}

static gboolean
panel_applet_can_focus (GtkWidget *widget)
{
	/*
	 * A PanelApplet widget can focus if it has a tooltip or it does 
	 * not have any focusable children.
	 */
	if (gtk_widget_get_has_tooltip (widget))
		return TRUE;

	if (!PANEL_IS_APPLET (widget))
		return FALSE;

	return !container_has_focusable_child (GTK_CONTAINER (widget));
}

/* Taken from libbonoboui/bonobo/bonobo-plug.c */
static gboolean
panel_applet_button_event (GtkWidget      *widget,
			   GdkEventButton *event)
{
	GdkWindow *window;
	GdkWindow *socket_window;
	XEvent     xevent;

	if (!gtk_widget_is_toplevel (widget))
		return FALSE;

	window = gtk_widget_get_window (widget);
	socket_window = gtk_plug_get_socket_window (GTK_PLUG (widget));

	if (event->type == GDK_BUTTON_PRESS) {
		xevent.xbutton.type = ButtonPress;

		/* X does an automatic pointer grab on button press
		 * if we have both button press and release events
		 * selected.
		 * We don't want to hog the pointer on our parent.
		 */
		gdk_device_ungrab (event->device, GDK_CURRENT_TIME);
	} else {
		xevent.xbutton.type = ButtonRelease;
	}

	xevent.xbutton.display     = GDK_WINDOW_XDISPLAY (window);
	xevent.xbutton.window      = GDK_WINDOW_XID (socket_window);
	xevent.xbutton.root        = GDK_WINDOW_XID (gdk_screen_get_root_window
							 (gdk_window_get_screen (window)));
	/*
	 * FIXME: the following might cause
	 *        big problems for non-GTK apps
	 */
	xevent.xbutton.x           = 0;
	xevent.xbutton.y           = 0;
	xevent.xbutton.x_root      = 0;
	xevent.xbutton.y_root      = 0;
	xevent.xbutton.state       = event->state;
	xevent.xbutton.button      = event->button;
	xevent.xbutton.same_screen = TRUE; /* FIXME ? */

	gdk_error_trap_push ();

	XSendEvent (GDK_WINDOW_XDISPLAY (window),
		    GDK_WINDOW_XID (socket_window),
		    False, NoEventMask, &xevent);

	gdk_error_trap_pop_ignored ();

	return TRUE;
}

static gboolean
panel_applet_button_press (GtkWidget      *widget,
			   GdkEventButton *event)
{
	PanelApplet *applet = PANEL_APPLET (widget);

	if (!container_has_focusable_child (GTK_CONTAINER (applet))) {
		if (!gtk_widget_has_focus (widget)) {
			gtk_widget_set_can_focus (widget, TRUE);
			gtk_widget_grab_focus (widget);
		}
	}

	if (event->button == 3) {
		guint modifiers;

		modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

		if (modifiers == panel_applet_bindings_get_mouse_button_modifier_keymask ())
			panel_applet_edit_menu_popup (applet, event->button, event->time);
		else
			panel_applet_menu_popup (applet, event->button, event->time);

		return TRUE;
	}

	return panel_applet_button_event (applet->priv->plug, event);
}

static gboolean
panel_applet_button_release (GtkWidget      *widget,
			     GdkEventButton *event)
{
	PanelApplet *applet = PANEL_APPLET (widget);

	return panel_applet_button_event (applet->priv->plug, event);
}

static gboolean
panel_applet_key_press_event (GtkWidget   *widget,
			      GdkEventKey *event)
{
	gboolean is_popup = FALSE;
	gboolean is_edit_popup = FALSE;

	panel_applet_bindings_key_event_is_popup (event, &is_popup, &is_edit_popup);

	if (is_edit_popup)
		panel_applet_edit_menu_popup (PANEL_APPLET (widget), 3, event->time);
	else if (is_popup)
		panel_applet_menu_popup (PANEL_APPLET (widget), 3, event->time);

	return (is_popup || is_edit_popup);
}

static GtkSizeRequestMode
panel_applet_get_request_mode (GtkWidget *widget)
{
        PanelApplet *applet = PANEL_APPLET (widget);
        PanelAppletOrient orientation;

        orientation = panel_applet_get_orient (applet);
        if (orientation == PANEL_APPLET_ORIENT_UP ||
            orientation == PANEL_APPLET_ORIENT_DOWN)
                return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;

        return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
panel_applet_get_preferred_width (GtkWidget *widget,
                                  int       *minimum_width,
                                  int       *natural_width)
{
        int focus_width = 0;

        GTK_WIDGET_CLASS (panel_applet_parent_class)->get_preferred_width (widget,
                                                                           minimum_width,
                                                                           natural_width);
        if (!panel_applet_can_focus (widget))
                return;

        /* We are deliberately ignoring focus-padding here to
         * save valuable panel real estate.
         */
        gtk_widget_style_get (widget,
                              "focus-line-width", &focus_width,
                              NULL);

        *minimum_width += 2 * focus_width;
        *natural_width += 2 * focus_width;
}

static void
panel_applet_get_preferred_height (GtkWidget *widget,
                                  int       *minimum_height,
                                  int       *natural_height)
{
        int focus_width = 0;

        GTK_WIDGET_CLASS (panel_applet_parent_class)->get_preferred_height (widget,
                                                                            minimum_height,
                                                                            natural_height);
        if (!panel_applet_can_focus (widget))
                return;

        /* We are deliberately ignoring focus-padding here to
         * save valuable panel real estate.
         */
        gtk_widget_style_get (widget,
                              "focus-line-width", &focus_width,
                              NULL);

        *minimum_height += 2 * focus_width;
        *natural_height += 2 * focus_width;
}

static void
panel_applet_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
	GtkAllocation  child_allocation;
	GtkBin        *bin;
	GtkWidget     *child;
	int            border_width;
	int            focus_width = 0;
	PanelApplet   *applet;

	if (!panel_applet_can_focus (widget)) {
		GTK_WIDGET_CLASS (panel_applet_parent_class)->size_allocate (widget, allocation);
	} else {
		/*
		 * We are deliberately ignoring focus-padding here to
		 * save valuable panel real estate.
		 */
		gtk_widget_style_get (widget,
				      "focus-line-width", &focus_width,
				      NULL);

		border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

		gtk_widget_set_allocation (widget, allocation);
		bin = GTK_BIN (widget);

		child_allocation.x = focus_width;
		child_allocation.y = focus_width;

		child_allocation.width  = MAX (allocation->width  - border_width * 2, 0);
		child_allocation.height = MAX (allocation->height - border_width * 2, 0);

		if (gtk_widget_get_realized (widget))
			gdk_window_move_resize (gtk_widget_get_window (widget),
						allocation->x + border_width,
						allocation->y + border_width,
						child_allocation.width,
						child_allocation.height);

		child_allocation.width  = MAX (child_allocation.width  - 2 * focus_width, 0);
		child_allocation.height = MAX (child_allocation.height - 2 * focus_width, 0);

		child = gtk_bin_get_child (bin);
		if (child)
			gtk_widget_size_allocate (child, &child_allocation);
	}

	applet = PANEL_APPLET (widget);

	if (applet->priv->previous_height != allocation->height ||
	    applet->priv->previous_width  != allocation->width) {
		applet->priv->previous_height = allocation->height;
		applet->priv->previous_width = allocation->width;

		panel_applet_handle_background (applet);
	}
}

static gboolean
panel_applet_draw (GtkWidget *widget,
		   cairo_t   *cr)
{
        GtkStyleContext *context;
	int border_width;
	int focus_width = 0;
	gdouble x, y, width, height;

	GTK_WIDGET_CLASS (panel_applet_parent_class)->draw (widget, cr);

        if (!gtk_widget_has_focus (widget))
		return FALSE;

        width = gtk_widget_get_allocated_width (widget);
        height = gtk_widget_get_allocated_height (widget);

	/*
	 * We are deliberately ignoring focus-padding here to
	 * save valuable panel real estate.
	 */
	gtk_widget_style_get (widget,
			      "focus-line-width", &focus_width,
			      NULL);

	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

	x = 0; // FIXME: border_width ?
	y = 0; // FIXME: border_width ?
	width  -= 2 * border_width;
	height -= 2 * border_width;

        context = gtk_widget_get_style_context (widget);
        gtk_style_context_save (context);

        cairo_save (cr);
        gtk_render_focus (context, cr, x, y, width, height);
        cairo_restore (cr);

        gtk_style_context_restore (context);

	return FALSE;
}

static gboolean 
panel_applet_focus (GtkWidget        *widget,
		    GtkDirectionType  dir)
{
	gboolean ret;
	GtkWidget *previous_focus_child;
	PanelApplet *applet;

	g_return_val_if_fail (PANEL_IS_APPLET (widget), FALSE);

	applet = PANEL_APPLET (widget);
	if (applet->priv->moving_focus_out) {
		/*
		 * Applet will retain focus if there is nothing else on the
		 * panel to get focus
		 */
		applet->priv->moving_focus_out = FALSE;
		return FALSE;
	}

	previous_focus_child = gtk_container_get_focus_child (GTK_CONTAINER (widget));
	if (!previous_focus_child && !gtk_widget_has_focus (widget)) {
		if (gtk_widget_get_has_tooltip (widget)) {
			gtk_widget_set_can_focus (widget, TRUE);
			gtk_widget_grab_focus (widget);
			gtk_widget_set_can_focus (widget, FALSE);
			return TRUE;
		}
	}
	ret = GTK_WIDGET_CLASS (panel_applet_parent_class)->focus (widget, dir);

	if (!ret && !previous_focus_child) {
		if (!gtk_widget_has_focus (widget))  {
			/*
			 * Applet does not have a widget which can focus so set
			 * the focus on the applet unless it already had focus
			 * because it had a tooltip.
			 */ 
			gtk_widget_set_can_focus (widget, TRUE);
			gtk_widget_grab_focus (widget);
			gtk_widget_set_can_focus (widget, FALSE);
			ret = TRUE;
		}
	}

	return ret;
}

static cairo_surface_t *
panel_applet_create_foreign_surface_for_display (GdkDisplay *display,
                                                 GdkVisual  *visual,
                                                 Window      xid)
{
        Window window;
        gint x, y;
        guint width, height, border, depth;

        if (!XGetGeometry (GDK_DISPLAY_XDISPLAY (display), xid, &window,
                           &x, &y, &width, &height, &border, &depth))
                return NULL;

        return cairo_xlib_surface_create (GDK_DISPLAY_XDISPLAY (display),
                                          xid, gdk_x11_visual_get_xvisual (visual),
                                          width, height);
}

static cairo_pattern_t *
panel_applet_get_pattern_from_pixmap (PanelApplet *applet,
                                      Window       xid,
                                      int          x,
                                      int          y)
{
	GdkWindow       *window;
	int              width;
	int              height;
        cairo_t         *cr;
        cairo_surface_t *background;
        cairo_surface_t *surface;
        cairo_matrix_t   matrix;
        cairo_pattern_t *pattern;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (!gtk_widget_get_realized (GTK_WIDGET (applet)))
		return NULL;

        window = gtk_widget_get_window (GTK_WIDGET (applet));

        gdk_error_trap_push ();
        background = panel_applet_create_foreign_surface_for_display (gdk_window_get_display (window),
                                                                      gdk_window_get_visual (window),
                                                                      xid);
        gdk_error_trap_pop_ignored ();

        /* background can be NULL if the user changes the background very fast.
         * We'll get the next update, so it's not a big deal. */
        if (!background ||
            cairo_surface_status (background) != CAIRO_STATUS_SUCCESS) {
                if (background)
                        cairo_surface_destroy (background);
                return NULL;
        }

        width = gdk_window_get_width (window);
        height = gdk_window_get_height (window);
        surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);

        gdk_error_trap_push ();
        cr = cairo_create (surface);
        cairo_set_source_surface (cr, background, -x, -y);
        cairo_rectangle (cr, 0, 0, width, height);
        cairo_fill (cr);
        gdk_error_trap_pop_ignored ();

        cairo_surface_destroy (background);

	pattern = NULL;

        if (cairo_status (cr) == CAIRO_STATUS_SUCCESS) {
                pattern = cairo_pattern_create_for_surface (surface);
                cairo_matrix_init_translate (&matrix, 0, 0);
                cairo_matrix_scale (&matrix, width, height);
                cairo_pattern_set_matrix (pattern, &matrix);
                cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);
        }

        cairo_destroy (cr);
        cairo_surface_destroy (surface);

	return pattern;
}

/**
 * panel_applet_get_background:
 * @applet: a #PanelApplet.
 *
 * Gets the background pattern for @applet, or %NULL if there is none.
 *
 * Returns: (transfer full): a new #cairo_pattern_t to use as background for
 * @applet.
 **/
cairo_pattern_t *
panel_applet_get_background (PanelApplet *applet)
{
        cairo_pattern_t *pattern = NULL;
        GVariant        *variant;
        GVariantIter     iter;
        GError          *error = NULL;

        g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (!gtk_widget_get_realized (GTK_WIDGET (applet)) || !applet->priv->background)
		return NULL;

        variant = g_variant_parse (NULL, applet->priv->background,
                                   NULL, NULL, &error);
        if (!variant) {
                g_warning ("Error parsing background %s: %s\n", applet->priv->background, error->message);
                g_error_free (error);
                return NULL;
        }

        g_variant_iter_init (&iter, variant);
        switch (g_variant_iter_n_children (&iter)) {
        case 4: {
                gdouble red, green, blue, alpha;

                g_variant_get (variant, "(dddd)", &red, &green, &blue, &alpha);
                pattern = cairo_pattern_create_rgba (red, green, blue, alpha);
        }
                break;
        case 3: {
                guint32 xid;
                int x, y;

                g_variant_get (variant, "(uii)", &xid, &x, &y);
                pattern = panel_applet_get_pattern_from_pixmap (applet, xid, x, y);
                if (!pattern)
                        g_warning ("Failed to get pixmap %d, %d, %d", xid, x, y);
        }
                break;
        default:
                break;
        }

        g_variant_unref (variant);

        return pattern;
}

static void
panel_applet_set_background_string (PanelApplet *applet,
				    const gchar *background)
{
	if (applet->priv->background == background)
		return;

	if (g_strcmp0 (applet->priv->background, background) == 0)
		return;

	if (applet->priv->background)
		g_free (applet->priv->background);
	applet->priv->background = background ? g_strdup (background) : NULL;
	panel_applet_handle_background (applet);

	g_object_notify (G_OBJECT (applet), "background");
}

static GtkStyleProperties *
_panel_applet_get_widget_style_properties (GtkWidget *widget,
                                           gboolean   create_if_needed)
{
        GtkStyleProperties *properties;

        properties = g_object_get_data (G_OBJECT (widget),
                                        "panel-applet-style-props");

        if (!properties && create_if_needed) {
                properties = gtk_style_properties_new ();
                g_object_set_data_full (G_OBJECT (widget),
                                        "panel-applet-style-props",
                                        properties,
                                        (GDestroyNotify) g_object_unref);
        }

        return properties;
}

static void
_panel_applet_reset_widget_style_properties (GtkWidget *widget)
{
        GtkStyleProperties *properties;

        properties = _panel_applet_get_widget_style_properties (widget, FALSE);

        if (properties)
                gtk_style_context_remove_provider (gtk_widget_get_style_context (widget),
                                                   GTK_STYLE_PROVIDER (properties));

        g_object_set_data (G_OBJECT (widget), "panel-applet-style-props", NULL);
}

static void
panel_applet_update_background_for_widget (GtkWidget       *widget,
					   cairo_pattern_t *pattern)
{
        GtkStyleProperties *properties;

        gtk_widget_reset_style (widget);

        if (!pattern) {
                _panel_applet_reset_widget_style_properties (widget);
                return;
        }

        properties = _panel_applet_get_widget_style_properties (widget, TRUE);

        switch (cairo_pattern_get_type (pattern)) {
        case CAIRO_PATTERN_TYPE_SOLID: {
                GdkRGBA color;

                cairo_pattern_get_rgba (pattern, &color.red, &color.green, &color.blue, &color.alpha);
                gtk_style_properties_set (properties, GTK_STATE_FLAG_NORMAL,
                                          "background-color", &color,
                                          "background-image", NULL,
                                          NULL);
        }
                break;
        case CAIRO_PATTERN_TYPE_SURFACE:
                gtk_style_properties_set (properties, GTK_STATE_FLAG_NORMAL,
					  /* background-color can't be NULL,
					   * but is ignored anyway */
                                          "background-image", pattern,
                                          NULL);
                break;
        default:
                break;
        }

	/* Note: this actually replaces the old properties, since it's the same
	 * pointer */
        gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
                                        GTK_STYLE_PROVIDER (properties),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
panel_applet_handle_background (PanelApplet *applet)
{
        cairo_pattern_t *pattern;

	pattern = panel_applet_get_background (applet);

	if (applet->priv->background_widget)
		panel_applet_update_background_for_widget (applet->priv->background_widget,
							   pattern);

        g_signal_emit (G_OBJECT (applet),
                        panel_applet_signals [CHANGE_BACKGROUND],
                        0, pattern);
        if (pattern)
		cairo_pattern_destroy (pattern);
}

static void
panel_applet_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (panel_applet_parent_class)->realize (widget);

	if (PANEL_APPLET (widget)->priv->background)
		panel_applet_handle_background (PANEL_APPLET (widget));
}

static void
panel_applet_move_focus_out_of_applet (PanelApplet      *applet,
				       GtkDirectionType  dir)
{
	GtkWidget *toplevel;

	applet->priv->moving_focus_out = TRUE;
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (applet));
	g_return_if_fail (toplevel);

	gtk_widget_child_focus (toplevel, dir);
	applet->priv->moving_focus_out = FALSE;
}

static void
panel_applet_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	PanelApplet *applet = PANEL_APPLET (object);

	switch (prop_id) {
	case PROP_ID:
		g_value_set_string (value, applet->priv->id);
		break;
	case PROP_CLOSURE:
		g_value_set_pointer (value, applet->priv->closure);
		break;
	case PROP_CONNECTION:
		g_value_set_object (value, applet->priv->connection);
		break;
	case PROP_SETTINGS_PATH:
		g_value_set_string (value, applet->priv->settings_path);
		break;
	case PROP_PREFS_KEY:
		g_value_set_string (value, applet->priv->prefs_key);
		break;
	case PROP_ORIENT:
		g_value_set_uint (value, applet->priv->orient);
		break;
	case PROP_SIZE:
		g_value_set_uint (value, applet->priv->size);
		break;
	case PROP_BACKGROUND:
		g_value_set_string (value, applet->priv->background);
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, applet->priv->flags);
		break;
	case PROP_SIZE_HINTS: {
		GVariant **children;
		GVariant  *variant;
		gint       i;

		children = g_new (GVariant *, applet->priv->size_hints_len);
		for (i = 0; i < applet->priv->size_hints_len; i++)
			children[i] = g_variant_new_int32 (applet->priv->size_hints[i]);
		variant = g_variant_new_array (G_VARIANT_TYPE_INT32,
					       children, applet->priv->size_hints_len);
		g_free (children);
		g_value_set_pointer (value, variant);
	}
		break;
	case PROP_LOCKED_DOWN:
		g_value_set_boolean (value, applet->priv->locked_down);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
panel_applet_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	PanelApplet *applet = PANEL_APPLET (object);

	switch (prop_id) {
	case PROP_ID:
		applet->priv->id = g_value_dup_string (value);
		break;
	case PROP_CLOSURE:
		applet->priv->closure = g_value_get_pointer (value);
		/* We know closure should not be NULL, but we'll fail in
		 * panel_applet_constructed() with a proper message if this is
		 * the case. */
		if (applet->priv->closure)
			g_closure_set_marshal (applet->priv->closure,
					       panel_applet_marshal_BOOLEAN__STRING);
		break;
	case PROP_CONNECTION:
		applet->priv->connection = g_value_dup_object (value);
		break;
	case PROP_SETTINGS_PATH:
		panel_applet_set_settings_path (applet, g_value_get_string (value));
		break;
	case PROP_PREFS_KEY:
		panel_applet_set_preferences_key (applet, g_value_get_string (value));
		break;
	case PROP_ORIENT:
		panel_applet_set_orient (applet, g_value_get_uint (value));
		break;
	case PROP_SIZE:
		panel_applet_set_size (applet, g_value_get_uint (value));
		break;
	case PROP_BACKGROUND:
		panel_applet_set_background_string (applet, g_value_get_string (value));
		break;
	case PROP_FLAGS:
		panel_applet_set_flags (applet, g_value_get_uint (value));
		break;
	case PROP_SIZE_HINTS: {
		const int *size_hints;
		gsize      n_elements;

		size_hints = g_variant_get_fixed_array (g_value_get_pointer (value),
							&n_elements, sizeof (gint32));
		panel_applet_set_size_hints (applet, size_hints, n_elements, 0);
	}
		break;
	case PROP_LOCKED_DOWN:
		panel_applet_set_locked_down (applet, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
add_tab_bindings (GtkBindingSet   *binding_set,
		  GdkModifierType  modifiers,
		  GtkDirectionType direction)
{
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, modifiers,
				      "move_focus_out_of_applet", 1,
				      GTK_TYPE_DIRECTION_TYPE, direction);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, modifiers,
				      "move_focus_out_of_applet", 1,
				      GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
panel_applet_setup (PanelApplet *applet)
{
	GValue   value = {0, };
	GArray  *params;
	gint     i;
	gboolean ret;

	g_assert (applet->priv->id != NULL &&
		  applet->priv->closure != NULL);

	params = g_array_sized_new (FALSE, TRUE, sizeof (GValue), 2);
	value.g_type = 0;
	g_value_init (&value, G_TYPE_OBJECT);
	g_value_set_object (&value, G_OBJECT (applet));
	g_array_append_val (params, value);

	value.g_type = 0;
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, applet->priv->id);
	g_array_append_val (params, value);

	value.g_type = 0;
	g_value_init (&value, G_TYPE_BOOLEAN);

	g_closure_invoke (applet->priv->closure,
			  &value, params->len,
			  (GValue *) params->data,
			  NULL);

	for (i = 0; i < params->len; i++)
		g_value_unset (&g_array_index (params, GValue, i));
	g_array_free (params, TRUE);

	ret = g_value_get_boolean (&value);
	g_value_unset (&value);

	if (!ret) { /* FIXME */
		g_warning ("need to free the control here");

		return;
	}
}

static void
panel_applet_init (PanelApplet *applet)
{
	applet->priv = PANEL_APPLET_GET_PRIVATE (applet);

	applet->priv->flags  = PANEL_APPLET_FLAGS_NONE;
	applet->priv->orient = PANEL_APPLET_ORIENT_UP;
	applet->priv->size   = 24;

	applet->priv->client = gconf_client_get_default ();

	applet->priv->panel_action_group = gtk_action_group_new ("PanelActions");
	gtk_action_group_set_translation_domain (applet->priv->panel_action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (applet->priv->panel_action_group,
				      menu_entries,
				      G_N_ELEMENTS (menu_entries),
				      applet);

	applet->priv->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (applet->priv->ui_manager,
					    applet->priv->panel_action_group, 1);
	gtk_ui_manager_add_ui_from_string (applet->priv->ui_manager,
					   panel_menu_ui, -1, NULL);


	panel_applet_bindings_init (applet->priv->client);


	applet->priv->plug = gtk_plug_new (0);
	g_signal_connect_swapped (G_OBJECT (applet->priv->plug), "embedded",
				  G_CALLBACK (panel_applet_setup),
				  applet);

	gtk_widget_set_events (GTK_WIDGET (applet),
			       GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK);

	gtk_container_add (GTK_CONTAINER (applet->priv->plug), GTK_WIDGET (applet));
}

static void
panel_applet_constructed (GObject *object)
{
	PanelApplet *applet = PANEL_APPLET (object);

	if (!applet->priv->connection || !applet->priv->closure || !applet->priv->id) {
		g_printerr ("Bad use of PanelApplet API: you should not create a PanelApplet object yourself. Please use panel_applet_factory_main() instead.\n");
		g_assert_not_reached ();
	}

	panel_applet_register_object (applet);
}

static void
panel_applet_class_init (PanelAppletClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
	GtkBindingSet *binding_set;

	gobject_class->get_property = panel_applet_get_property;
	gobject_class->set_property = panel_applet_set_property;
	gobject_class->constructed = panel_applet_constructed;
        gobject_class->finalize = panel_applet_finalize;

	klass->move_focus_out_of_applet = panel_applet_move_focus_out_of_applet;

	widget_class->button_press_event = panel_applet_button_press;
	widget_class->button_release_event = panel_applet_button_release;
	widget_class->key_press_event = panel_applet_key_press_event;
	widget_class->get_request_mode = panel_applet_get_request_mode;
        widget_class->get_preferred_width = panel_applet_get_preferred_width;
        widget_class->get_preferred_height = panel_applet_get_preferred_height;
	widget_class->size_allocate = panel_applet_size_allocate;
	widget_class->draw = panel_applet_draw;
	widget_class->focus = panel_applet_focus;
	widget_class->realize = panel_applet_realize;

	g_type_class_add_private (klass, sizeof (PanelAppletPrivate));

	/**
	 * PanelApplet:id: (skip)
	 *
	 * Implementation detail.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      "Id",
							      "The Applet identifier",
							      NULL,
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_READWRITE));
	/**
	 * PanelApplet:closure: (skip)
	 *
	 * Implementation detail.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_CLOSURE,
					 g_param_spec_pointer ("closure",
							       "GClosure",
							       "The Applet closure",
							       G_PARAM_CONSTRUCT_ONLY |
							       G_PARAM_READWRITE));
	/**
	 * PanelApplet:connection: (skip)
	 *
	 * Implementation detail.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_CONNECTION,
					 g_param_spec_object ("connection",
							      "Connection",
							      "The DBus Connection",
							      G_TYPE_DBUS_CONNECTION,
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_READWRITE));
	/**
	 * PanelApplet:settings-path:
	 *
	 * The GSettings path to the per-instance settings of the applet.
	 *
	 * This property gets set when the applet gets embedded.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_SETTINGS_PATH,
					 g_param_spec_string ("settings-path",
							      "SettingsPath",
							      "GSettings path to per-instance settings",
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * PanelApplet:prefs-key:
	 *
	 * The GConf path to the per-instance settings of the applet.
	 *
	 * This property gets set when the applet gets embedded.
	 *
	 * Deprecated: 3.0: Use #GSettings to store per-instance settings.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_PREFS_KEY,
					 g_param_spec_string ("prefs-key",
							      "PrefsKey",
							      "GConf Preferences Key",
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * PanelApplet:orient:
	 *
	 * The #PanelAppletOrient of the applet.
	 *
	 * This property gets set when the applet gets embedded, and can change
	 * when the panel position changes.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_ORIENT,
					 g_param_spec_uint ("orient",
							    "Orient",
							    "Panel Applet Orientation",
							    PANEL_APPLET_ORIENT_FIRST,
							    PANEL_APPLET_ORIENT_LAST,
							    PANEL_APPLET_ORIENT_UP,
							    G_PARAM_READWRITE));
	/**
	 * PanelApplet:size:
	 *
	 * The size of the panel the applet is on. For a horizontal panel, the
	 * size if the height of the panel; for a vertical panel, the size is
	 * the width of the panel.
	 *
	 * This property gets set when the applet gets embedded, and can change
	 * when the panel size changes.
         *
	 * Deprecated: 3.0: Use the allocation of @applet instead.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_SIZE,
					 g_param_spec_uint ("size",
							    "Size",
							    "Panel Applet Size",
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE));
	/**
	 * PanelApplet:background: (skip)
	 *
	 * Implementation detail.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_BACKGROUND,
					 g_param_spec_string ("background",
							      "Background",
							      "Panel Applet Background",
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * PanelApplet:flags:
	 *
	 * The #PanelAppletFlags of the applet.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_FLAGS,
					 g_param_spec_uint ("flags",
							    "Flags",
							    "Panel Applet flags",
							    PANEL_APPLET_FLAGS_NONE,
							    PANEL_APPLET_FLAGS_ALL,
							    PANEL_APPLET_FLAGS_NONE,
							    G_PARAM_READWRITE));
	/**
	 * PanelApplet:size-hints:
	 *
	 * The size hints set for the applet. See panel_applet_set_size_hints().
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_SIZE_HINTS,
					 /* FIXME: value_array? */
					 g_param_spec_pointer ("size-hints",
							       "SizeHints",
							       "Size hints of the applet",
							       G_PARAM_READWRITE));
	/**
	 * PanelApplet:locked-down:
	 *
	 * Whether the panel the applet is on is locked down.
	 **/
	g_object_class_install_property (gobject_class,
					 PROP_LOCKED_DOWN,
					 g_param_spec_boolean ("locked-down",
							       "LockedDown",
							       "Whether the panel the applet is on is locked down",
							       FALSE,
							       G_PARAM_READWRITE));

        /**
         * PanelApplet::change-orient:
         * @applet: the #PanelApplet which emitted the signal.
         * @orient: the new #PanelAppletOrient of @applet.
         *
         * Emitted when the #PanelAppletOrient of @applet has changed.
         **/
	panel_applet_signals [CHANGE_ORIENT] =
                g_signal_new ("change_orient",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelAppletClass, change_orient),
                              NULL,
			      NULL,
                              g_cclosure_marshal_VOID__UINT,
                              G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

        /**
         * PanelApplet::change-size:
         * @applet: the #PanelApplet which emitted the signal.
         * @size: the new size of the panel @applet is on.
         *
         * Emitted when the size of the panel @applet is on has changed.
	 *
	 * Deprecated: 3.0: Use the #GtkWidget::size-allocate signal instead.
         **/
	panel_applet_signals [CHANGE_SIZE] =
                g_signal_new ("change_size",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelAppletClass, change_size),
                              NULL,
			      NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE,
			      1,
			      G_TYPE_INT);

        /**
         * PanelApplet::change-background:
         * @applet: the #PanelApplet which emitted the signal.
         * @pattern: the new background pattern for @applet, or %NULL if there is none.
         *
         * Emitted when the background of @applet has changed.
         **/
	panel_applet_signals [CHANGE_BACKGROUND] =
                g_signal_new ("change_background",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelAppletClass, change_background),
                              NULL,
			      NULL,
                              g_cclosure_marshal_VOID__BOXED,
                              G_TYPE_NONE,
			      1,
			      CAIRO_GOBJECT_TYPE_PATTERN);

        /**
         * PanelApplet::move-focus-out-of-applet: (skip)
         * @applet: the #PanelApplet which emitted the signal.
         * @direction: the move direction.
         *
	 * Emitted when the focus is moved out of @applet. This is an
	 * implementation detail.
         **/
	panel_applet_signals [MOVE_FOCUS_OUT_OF_APPLET] =
                g_signal_new ("move_focus_out_of_applet",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (PanelAppletClass, move_focus_out_of_applet),
                              NULL,
			      NULL,
                              g_cclosure_marshal_VOID__ENUM,
                              G_TYPE_NONE,
			      1,
			      GTK_TYPE_DIRECTION_TYPE);

	binding_set = gtk_binding_set_by_class (gobject_class);
	add_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
	add_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
	add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
	add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
}

static void
method_call_cb (GDBusConnection       *connection,
		const gchar           *sender,
		const gchar           *object_path,
		const gchar           *interface_name,
		const gchar           *method_name,
		GVariant              *parameters,
		GDBusMethodInvocation *invocation,
		gpointer               user_data)
{
	PanelApplet *applet = PANEL_APPLET (user_data);

	if (g_strcmp0 (method_name, "PopupMenu") == 0) {
		guint button;
		guint time;

		g_variant_get (parameters, "(uu)", &button, &time);
		panel_applet_menu_popup (applet, button, time);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "PopupEditMenu") == 0) {
		guint button;
		guint time;

		g_variant_get (parameters, "(uu)", &button, &time);
		panel_applet_edit_menu_popup (applet, button, time);

		g_dbus_method_invocation_return_value (invocation, NULL);
	}
}

static GVariant *
get_property_cb (GDBusConnection *connection,
		 const gchar     *sender,
		 const gchar     *object_path,
		 const gchar     *interface_name,
		 const gchar     *property_name,
		 GError         **error,
		 gpointer         user_data)
{
	PanelApplet *applet = PANEL_APPLET (user_data);
	GVariant    *retval = NULL;

	if (g_strcmp0 (property_name, "SettingsPath") == 0) {
		retval = g_variant_new_string (applet->priv->settings_path ?
					       applet->priv->settings_path : "");
	} else if (g_strcmp0 (property_name, "PrefsKey") == 0) {
		retval = g_variant_new_string (applet->priv->prefs_key ?
					       applet->priv->prefs_key : "");
	} else if (g_strcmp0 (property_name, "Orient") == 0) {
		retval = g_variant_new_uint32 (applet->priv->orient);
	} else if (g_strcmp0 (property_name, "Size") == 0) {
		retval = g_variant_new_uint32 (applet->priv->size);
	} else if (g_strcmp0 (property_name, "Background") == 0) {
		retval = g_variant_new_string (applet->priv->background ?
					       applet->priv->background : "");
	} else if (g_strcmp0 (property_name, "Flags") == 0) {
		retval = g_variant_new_uint32 (applet->priv->flags);
	} else if (g_strcmp0 (property_name, "SizeHints") == 0) {
		GVariant **children;
		gint       i;

		children = g_new (GVariant *, applet->priv->size_hints_len);
		for (i = 0; i < applet->priv->size_hints_len; i++)
			children[i] = g_variant_new_int32 (applet->priv->size_hints[i]);
		retval = g_variant_new_array (G_VARIANT_TYPE_INT32,
					      children, applet->priv->size_hints_len);
		g_free (children);
	} else if (g_strcmp0 (property_name, "LockedDown") == 0) {
		retval = g_variant_new_boolean (applet->priv->locked_down);
	}

	return retval;
}

static gboolean
set_property_cb (GDBusConnection *connection,
		 const gchar     *sender,
		 const gchar     *object_path,
		 const gchar     *interface_name,
		 const gchar     *property_name,
		 GVariant        *value,
		 GError         **error,
		 gpointer         user_data)
{
	PanelApplet *applet = PANEL_APPLET (user_data);

	if (g_strcmp0 (property_name, "SettingsPath") == 0) {
		panel_applet_set_settings_path (applet, g_variant_get_string (value, NULL));
	} else if (g_strcmp0 (property_name, "PrefsKey") == 0) {
		panel_applet_set_preferences_key (applet, g_variant_get_string (value, NULL));
	} else if (g_strcmp0 (property_name, "Orient") == 0) {
		panel_applet_set_orient (applet, g_variant_get_uint32 (value));
	} else if (g_strcmp0 (property_name, "Size") == 0) {
		panel_applet_set_size (applet, g_variant_get_uint32 (value));
	} else if (g_strcmp0 (property_name, "Background") == 0) {
		panel_applet_set_background_string (applet, g_variant_get_string (value, NULL));
	} else if (g_strcmp0 (property_name, "Flags") == 0) {
		panel_applet_set_flags (applet, g_variant_get_uint32 (value));
	} else if (g_strcmp0 (property_name, "SizeHints") == 0) {
		const int *size_hints;
		gsize      n_elements;

		size_hints = g_variant_get_fixed_array (value, &n_elements, sizeof (gint32));
		panel_applet_set_size_hints (applet, size_hints, n_elements, 0);
	} else if (g_strcmp0 (property_name, "LockedDown") == 0) {
		panel_applet_set_locked_down (applet, g_variant_get_boolean (value));
	}

	return TRUE;
}

static const gchar introspection_xml[] =
	"<node>"
	  "<interface name='org.gnome.panel.applet.Applet'>"
	    "<method name='PopupMenu'>"
	      "<arg name='button' type='u' direction='in'/>"
	      "<arg name='time' type='u' direction='in'/>"
	    "</method>"
	    "<method name='PopupEditMenu'>"
	      "<arg name='button' type='u' direction='in'/>"
	      "<arg name='time' type='u' direction='in'/>"
	    "</method>"
	    "<property name='SettingsPath' type='s' access='readwrite'/>"
	    "<property name='PrefsKey' type='s' access='readwrite'/>"
	    "<property name='Orient' type='u' access='readwrite' />"
	    "<property name='Size' type='u' access='readwrite'/>"
	    "<property name='Background' type='s' access='readwrite'/>"
	    "<property name='Flags' type='u' access='readwrite'/>"
	    "<property name='SizeHints' type='ai' access='readwrite'/>"
	    "<property name='LockedDown' type='b' access='readwrite'/>"
	    "<signal name='Move' />"
	    "<signal name='RemoveFromPanel' />"
	  "</interface>"
	"</node>";

static const GDBusInterfaceVTable interface_vtable = {
	method_call_cb,
	get_property_cb,
	set_property_cb
};

static GDBusNodeInfo *introspection_data = NULL;

static void
panel_applet_register_object (PanelApplet *applet)
{
	GError     *error = NULL;
	static gint id = 0;

	if (!introspection_data)
		introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	applet->priv->object_path = g_strdup_printf (PANEL_APPLET_OBJECT_PATH, applet->priv->id, id++);
	applet->priv->object_id =
		g_dbus_connection_register_object (applet->priv->connection,
						   applet->priv->object_path,
						   introspection_data->interfaces[0],
						   &interface_vtable,
						   applet, NULL,
						   &error);
	if (!applet->priv->object_id) {
		g_printerr ("Failed to register object %s: %s\n", applet->priv->object_path, error->message);
		g_error_free (error);
	}
}

static void
panel_applet_factory_main_finalized (gpointer data,
				     GObject *object)
{
	gtk_main_quit ();

	if (introspection_data) {
		g_dbus_node_info_unref (introspection_data);
		introspection_data = NULL;
	}
}

static int (*_x_error_func) (Display *, XErrorEvent *);

static int
_x_error_handler (Display *display, XErrorEvent *error)
{
	if (!error->error_code)
		return 0;

	/* If we got a BadDrawable or a BadWindow, we ignore it for now.
	 * FIXME: We need to somehow distinguish real errors from
	 * X-server-induced errors. Keeping a list of windows for which we
	 * will ignore BadDrawables would be a good idea. */
	if (error->error_code == BadDrawable ||
	    error->error_code == BadWindow)
		return 0;

	return _x_error_func (display, error);
}

/*
 * To do graphical embedding in the X window system, GNOME Panel
 * uses the classic foreign-window-reparenting trick. The
 * GtkPlug/GtkSocket widgets are used for this purpose. However,
 * serious robustness problems arise if the GtkSocket end of the
 * connection unexpectedly dies. The X server sends out DestroyNotify
 * events for the descendants of the GtkPlug (i.e., your embedded
 * component's windows) in effectively random order. Furthermore, if
 * you happened to be drawing on any of those windows when the
 * GtkSocket was destroyed (a common state of affairs), an X error
 * will kill your application.
 *
 * To solve this latter problem, GNOME Panel sets up its own X error
 * handler which ignores certain X errors that might have been
 * caused by such a scenario. Other X errors get passed to gdk_x_error
 * normally.
 */
static void
_panel_applet_setup_x_error_handler (void)
{
	static gboolean error_handler_setup = FALSE;

	if (error_handler_setup)
		return;

	error_handler_setup = TRUE;

	_x_error_func = XSetErrorHandler (_x_error_handler);
}

static int
_panel_applet_factory_main_internal (const gchar               *factory_id,
				     gboolean                   out_process,
				     GType                      applet_type,
				     PanelAppletFactoryCallback callback,
				     gpointer                   user_data)
{
	PanelAppletFactory *factory;
	GClosure           *closure;

	g_return_val_if_fail (factory_id != NULL, 1);
	g_return_val_if_fail (callback != NULL, 1);
	g_assert (g_type_is_a (applet_type, PANEL_TYPE_APPLET));

	if (out_process)
		_panel_applet_setup_x_error_handler ();

	closure = g_cclosure_new (G_CALLBACK (callback), user_data, NULL);
	factory = panel_applet_factory_new (factory_id, applet_type, closure);
	g_closure_unref (closure);

	if (panel_applet_factory_register_service (factory)) {
		if (out_process) {
			g_object_weak_ref (G_OBJECT (factory),
					   panel_applet_factory_main_finalized,
					   NULL);
			gtk_main ();
		}

		return 0;
	}

	g_object_unref (factory);

	return 1;
}

/**
 * panel_applet_factory_main:
 * @factory_id: identifier of an applet factory.
 * @applet_type: GType of the applet this factory creates.
 * @callback: (scope call): callback to be called when a new applet is created.
 * @data: (closure): callback data.
 *
 * Creates the applet factory for @factory_id, so that the factory can create
 * instances of the applet types it is associated with.
 *
 * Applet instances created by the applet factory will use @applet_type as
 * GType. Unless you subclass #PanelApplet, you should use %PANEL_TYPE_APPLET
 * as @applet_type.
 *
 * On creation of the applet instances, @callback is called to setup the
 * applet. If @callback returns %FALSE, the creation of the applet instance is
 * cancelled.
 *
 * If using C, it is recommended to use #PANEL_APPLET_OUT_PROCESS_FACTORY
 * instead as it will create a main() function for you.
 *
 * It can only be used once, and is incompatible with the use of
 * %PANEL_APPLET_IN_PROCESS_FACTORY and %PANEL_APPLET_OUT_PROCESS_FACTORY.
 *
 * Returns: 0 on success, 1 if there is an error.
 **/
int
panel_applet_factory_main (const gchar               *factory_id,
			   GType                      applet_type,
			   PanelAppletFactoryCallback callback,
			   gpointer                   data)
{
	return _panel_applet_factory_main_internal (factory_id, TRUE, applet_type,
						    callback, data);
}

/**
 * panel_applet_factory_setup_in_process: (skip)
 * @factory_id: identifier of an applet factory.
 * @applet_type: GType of the applet this factory creates.
 * @callback: (scope call): callback to be called when a new applet is created.
 * @data: (closure): callback data.
 *
 * Creates the applet factory for @factory_id, so that the factory can create
 * instances of the applet types it is associated with.
 *
 * Applet instances created by the applet factory will use @applet_type as
 * GType. Unless you subclass #PanelApplet, you should use %PANEL_TYPE_APPLET
 * as @applet_type.
 *
 * On creation of the applet instances, @callback is called to setup the
 * applet. If @callback returns %FALSE, the creation of the applet instance is
 * cancelled.
 *
 * It can only be used once, and is incompatible with the use of
 * %PANEL_APPLET_IN_PROCESS_FACTORY and %PANEL_APPLET_OUT_PROCESS_FACTORY.
 *
 * Returns: 0 on success, 1 if there is an error.
 **/
int
panel_applet_factory_setup_in_process (const gchar               *factory_id,
				       GType                      applet_type,
				       PanelAppletFactoryCallback callback,
				       gpointer                   data)
{
	return _panel_applet_factory_main_internal (factory_id, FALSE, applet_type,
						    callback, data);
}

/**
 * panel_applet_set_background_widget:
 * @applet: a #PanelApplet.
 * @widget: a #GtkWidget.
 *
 * Configure #PanelApplet to automatically draw the background of the applet on
 * @widget. It is generally enough to call this function with @applet as
 * @widget.
 **/
void
panel_applet_set_background_widget (PanelApplet *applet,
				    GtkWidget   *widget)
{
	applet->priv->background_widget = widget;

	if (widget && gtk_widget_get_realized (widget)) {
		cairo_pattern_t *pattern;

		pattern = panel_applet_get_background (applet);
		panel_applet_update_background_for_widget (widget, pattern);
		if (pattern)
			cairo_pattern_destroy (pattern);
	}
}

guint32
panel_applet_get_xid (PanelApplet *applet,
		      GdkScreen   *screen)
{
	gtk_window_set_screen (GTK_WINDOW (applet->priv->plug), screen);
	gtk_widget_show (applet->priv->plug);

	return gtk_plug_get_id (GTK_PLUG (applet->priv->plug));
}

const gchar *
panel_applet_get_object_path (PanelApplet *applet)
{
	return applet->priv->object_path;
}

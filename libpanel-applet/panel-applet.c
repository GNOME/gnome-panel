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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
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
#include <gtk/gtk.h>

#include "panel-applet.h"
#include "panel-applet-private.h"
#include "panel-applet-bindings.h"
#include "panel-applet-factory.h"
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

struct _PanelAppletPrivate {
	GtkWidget         *applet;
	GDBusConnection   *connection;

	char              *id;
	GClosure          *closure;
	char              *object_path;
	guint              object_id;
	char              *settings_path;

	GtkBuilder         *builder;
	GSimpleActionGroup *applet_action_group;
	GSimpleActionGroup *panel_action_group;

	PanelAppletFlags   flags;
	PanelAppletOrient  orient;

        int               *size_hints;
        int                size_hints_len;

	gboolean           locked_down;
};

enum {
        CHANGE_ORIENT,
        LAST_SIGNAL
};

static guint panel_applet_signals [LAST_SIGNAL];

enum {
	PROP_0,
	PROP_ID,
	PROP_CLOSURE,
	PROP_CONNECTION,
	PROP_SETTINGS_PATH,
	PROP_ORIENT,
	PROP_FLAGS,
	PROP_SIZE_HINTS,
	PROP_LOCKED_DOWN
};

static GAction   *panel_applet_menu_get_action     (PanelApplet       *applet,
						    const gchar       *action);
static void       panel_applet_menu_update_actions (PanelApplet       *applet);
static void       panel_applet_menu_cmd_remove     (GSimpleAction     *action,
						    GVariant      *parameter,
						    gpointer       user_data);
static void       panel_applet_menu_cmd_move       (GSimpleAction     *action,
						    GVariant      *parameter,
						    gpointer       user_data);
static void       panel_applet_register_object     (PanelApplet       *applet);

static const gchar panel_menu_ui[] =
	"<interface>\n"
	" <menu id=\"panel-applet-popup\">"
	" </menu>"
	" <menu id=\"panel-applet-edit-popup\">"
	"   <section>"
	"     <item>"
	"       <attribute name=\"label\" translatable=\"no\">%s</attribute>"
	"       <attribute name=\"action\">libpanel-applet.remove</attribute>"
	"     </item>"
	"     <item>"
	"       <attribute name=\"label\" translatable=\"no\">%s</attribute>"
	"       <attribute name=\"action\">libpanel-applet.move</attribute>"
	"     </item>"
	"   </section>"
	" </menu>"
	"</interface>\n";

static const GActionEntry menu_entries[] = {
	{ "remove", panel_applet_menu_cmd_remove, NULL, NULL, NULL },
	{ "move",   panel_applet_menu_cmd_move,   NULL, NULL, NULL }
};

G_DEFINE_TYPE_WITH_PRIVATE (PanelApplet, panel_applet, GTK_TYPE_EVENT_BOX)

#define PANEL_APPLET_INTERFACE   "org.gnome.panel.applet.Applet"
#define PANEL_APPLET_OBJECT_PATH "/org/gnome/panel/applet/%s/%d"

/**
 * panel_applet_settings_new:
 * @applet: a #PanelApplet.
 * @schema: the name of the schema.
 *
 * Creates a new #GSettings object for the per-instance settings of @applet,
 * with a given schema.
 *
 * Returns: (transfer full): a new #GSettings object for the per-instance settings of @applet.
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

/**
 * panel_applet_get_gtk_orientation:
 * @applet: a #PanelApplet
 *
 * Gets the #GtkOrientation of @applet.
 *
 * Returns: the #GtkOrientation of @applet.
 *
 * Since: 3.18
 */
GtkOrientation
panel_applet_get_gtk_orientation (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), GTK_ORIENTATION_HORIZONTAL);

	if (applet->priv->orient == PANEL_APPLET_ORIENT_LEFT ||
	    applet->priv->orient == PANEL_APPLET_ORIENT_RIGHT)
		return GTK_ORIENTATION_VERTICAL;

	return GTK_ORIENTATION_HORIZONTAL;
}

/**
 * panel_applet_add_text_class:
 * @applet: a #PanelApplet
 * @widget: a #GtkWidget
 *
 * Use this function to add css class to widgets that are visible on panel
 * and shows text.
 *
 * Since: 3.22
 */
void
panel_applet_add_text_class (PanelApplet *applet,
                             GtkWidget   *widget)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_add_class (context, "gp-text-color");
}

/* Applets cannot set their orientation, so API is not public. */
static void
panel_applet_set_orient (PanelApplet      *applet,
			 PanelAppletOrient orient)
{
	GtkStyleContext *context;

	g_return_if_fail (PANEL_IS_APPLET (applet));

	if (applet->priv->orient == orient)
		return;

	applet->priv->orient = orient;

	context = gtk_widget_get_style_context (GTK_WIDGET (applet));
	switch (orient) {
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);
		gtk_style_context_remove_class (context, GTK_STYLE_CLASS_VERTICAL);
		break;
	case PANEL_APPLET_ORIENT_LEFT:
	case PANEL_APPLET_ORIENT_RIGHT:
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_VERTICAL);
		gtk_style_context_remove_class (context, GTK_STYLE_CLASS_HORIZONTAL);
		break;
	default:
		g_assert_not_reached();
		break;
	}
	gtk_widget_reset_style (GTK_WIDGET (applet));

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

/**
 * panel_applet_request_focus:
 * @applet: a #PanelApplet.
 * @timestamp: the timestamp of the user interaction (typically a button or key
 * press event) which triggered this call.
 *
 * Requests focus for @applet. There is no guarantee that @applet will
 * successfully get focus after that call.
 **/
void
panel_applet_request_focus (PanelApplet	 *applet,
			    guint32	  timestamp)
{
  GtkWidget *toplevel;
  GdkWindow *window;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (applet));
  if (!toplevel)
    return;

  window = gtk_widget_get_window (toplevel);
  if (!window)
    return;

  gdk_window_focus (window, timestamp);
}

static GAction *
panel_applet_menu_get_action (PanelApplet *applet,
			      const gchar *action)
{
	return g_action_map_lookup_action (G_ACTION_MAP (applet->priv->panel_action_group),
	                                   action);
}

static void
panel_applet_menu_update_actions (PanelApplet *applet)
{
	gboolean locked_down;
	GAction *action;

	locked_down = applet->priv->locked_down;

	action = panel_applet_menu_get_action (applet, "Move");
	if (action)
		g_object_set (action, "enabled", !locked_down, NULL);

	action = panel_applet_menu_get_action (applet, "Remove");
	if (action)
		g_object_set (action, "enabled", !locked_down, NULL);
}

static void
panel_applet_menu_cmd_remove (GSimpleAction *action,
			      GVariant      *parameter,
			      gpointer       user_data)
{
	PanelApplet *applet = PANEL_APPLET (user_data);
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
panel_applet_menu_cmd_move (GSimpleAction *action,
			    GVariant      *parameter,
			    gpointer       user_data)
{
	PanelApplet *applet = PANEL_APPLET (user_data);
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
 * @action_group: a #GSimpleActionGroup.
 * @translation_domain: gettext translation domain
 *
 * Sets up the context menu of @applet. @xml is a #GtkUIManager UI definition,
 * describing how to display the menu items. @action_group contains the
 * various #GSimpleAction that are referenced in @xml.
 *
 * See also the <link linkend="getting-started.context-menu">Context
 * Menu</link> section.
 **/
void
panel_applet_setup_menu (PanelApplet    *applet,
			 const gchar    *xml,
			 GSimpleActionGroup *action_group,
			 const gchar        *translation_domain)
{
	gchar  *new_xml;
	GError *error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));
	g_return_if_fail (xml != NULL);

	if (applet->priv->applet_action_group)
		return;

	gtk_builder_set_translation_domain (applet->priv->builder, translation_domain);

	applet->priv->applet_action_group = g_object_ref (action_group);

	new_xml = g_strdup_printf ("<interface><menu id=\"panel-applet-popup\">%s</menu></interface>\n", xml);
	gtk_builder_add_from_string (applet->priv->builder, new_xml, -1, &error);
	g_free (new_xml);

	if (error) {
		g_warning ("Error merging menus: %s\n", error->message);
		g_error_free (error);
	}
}

/**
 * panel_applet_setup_menu_from_file:
 * @applet: a #PanelApplet.
 * @filename: path to a menu XML file.
 * @action_group: a #GSimpleActionGroup.
 * @translation_domain: gettext translation domain
 *
 * Sets up the context menu of @applet. @filename is the path to a menu XML
 * file, containing a #GtkUIManager UI definition that describes how to display
 * the menu items. @action_group contains the various #GSimpleAction that
 * are referenced in @xml.
 *
 * See also the <link linkend="getting-started.context-menu">Context
 * Menu</link> section.
 **/
void
panel_applet_setup_menu_from_file (PanelApplet    *applet,
				   const gchar    *filename,
				   GSimpleActionGroup *action_group,
				   const gchar        *translation_domain)
{
	gchar  *xml = NULL;
	GError *error = NULL;

	if (g_file_get_contents (filename, &xml, NULL, &error)) {
		panel_applet_setup_menu (applet, xml, action_group, translation_domain);
	} else {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_free (xml);
}

/**
 * panel_applet_setup_menu_from_resource:
 * @applet: a #PanelApplet.
 * @resource_path: a resource path
 * @action_group: a #GSimpleActionGroup.
 * @translation_domain: gettext translation domain
 *
 * Sets up the context menu of @applet. @filename is a resource path to a menu
 * XML file, containing a #GtkUIManager UI definition that describes how to
 * display the menu items. @action_group contains the various #GSimpleAction that
 * are referenced in @xml.
 *
 * See also the <link linkend="getting-started.context-menu">Context
 * Menu</link> section.
 *
 * Since: 3.4
 **/
void
panel_applet_setup_menu_from_resource (PanelApplet    *applet,
				       const gchar    *resource_path,
				       GSimpleActionGroup *action_group,
				       const gchar        *translation_domain)
{
	GBytes *bytes;
	GError *error = NULL;

	bytes = g_resources_lookup_data (resource_path,
					 G_RESOURCE_LOOKUP_FLAGS_NONE,
					 &error);

	if (bytes) {
		panel_applet_setup_menu (applet,
					 g_bytes_get_data (bytes, NULL),
					 action_group,
					 translation_domain);
	} else {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_bytes_unref (bytes);
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

	if (applet->priv->applet_action_group) {
		g_object_unref (applet->priv->applet_action_group);
		applet->priv->applet_action_group = NULL;
	}

	if (applet->priv->panel_action_group) {
		g_object_unref (applet->priv->panel_action_group);
		applet->priv->panel_action_group = NULL;
	}

	if (applet->priv->builder) {
		g_object_unref (applet->priv->builder);
		applet->priv->builder = NULL;
	}

	g_free (applet->priv->size_hints);
	g_free (applet->priv->settings_path);
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
panel_applet_menu_popup (PanelApplet *applet,
                         GdkEvent    *event)
{
	GtkWidget *menu;
	GMenu     *gmenu;
	GList     *children, *l;
	gboolean   visible = FALSE;

	gmenu = G_MENU (gtk_builder_get_object (applet->priv->builder, "panel-applet-popup"));
	menu = gtk_menu_new_from_model (G_MENU_MODEL (gmenu));

	gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (applet), NULL);

	children = gtk_container_get_children (GTK_CONTAINER (menu));
	for (l = children; l != NULL; l = l->next) {
		visible = gtk_widget_get_visible (GTK_WIDGET (l->data));
		if (visible)
			break;
	}
	g_list_free (children);

	if (!visible)
		return;

	gtk_menu_popup_at_widget (GTK_MENU (menu),
	                          GTK_WIDGET (applet),
	                          GDK_GRAVITY_SOUTH_WEST,
	                          GDK_GRAVITY_NORTH_WEST,
	                          event);
}

static void
panel_applet_edit_menu_popup (PanelApplet *applet,
                              GdkEvent    *event)
{
	GtkWidget *menu;
	GMenu     *gmenu;

	if (applet->priv->locked_down)
		return;

	gmenu = G_MENU (gtk_builder_get_object (applet->priv->builder, "panel-applet-edit-popup"));
	menu = gtk_menu_new_from_model (G_MENU_MODEL (gmenu));

	gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (applet), NULL);
	gtk_menu_popup_at_widget (GTK_MENU (menu),
	                          GTK_WIDGET (applet),
	                          GDK_GRAVITY_SOUTH_WEST,
	                          GDK_GRAVITY_NORTH_WEST,
	                          event);
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
			panel_applet_edit_menu_popup (applet, (GdkEvent *) event);
		else
			panel_applet_menu_popup (applet, (GdkEvent *) event);

		return TRUE;
	}

	return FALSE;
}

static void
panel_applet_composited_changed (GtkWidget *widget)
{
	GdkScreen *screen;
	GdkVisual *visual;

	screen = gtk_widget_get_screen (widget);
	visual = gdk_screen_get_rgba_visual (screen);

	if (visual == NULL)
		visual = gdk_screen_get_system_visual (screen);

	gtk_widget_set_visual (widget, visual);
}

static gboolean
panel_applet_key_press_event (GtkWidget   *widget,
			      GdkEventKey *event)
{
	gboolean is_popup = FALSE;
	gboolean is_edit_popup = FALSE;

	panel_applet_bindings_key_event_is_popup (event, &is_popup, &is_edit_popup);

	if (is_edit_popup)
		panel_applet_edit_menu_popup (PANEL_APPLET (widget), (GdkEvent *) event);
	else if (is_popup)
		panel_applet_menu_popup (PANEL_APPLET (widget), (GdkEvent *) event);

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
panel_applet_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
	GtkAllocation  child_allocation;
	GtkBin        *bin;
	GtkWidget     *child;
	int            border_width;

	if (!panel_applet_can_focus (widget)) {
		GTK_WIDGET_CLASS (panel_applet_parent_class)->size_allocate (widget, allocation);
	} else {
		border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

		gtk_widget_set_allocation (widget, allocation);
		bin = GTK_BIN (widget);

		child_allocation.x = 0;
		child_allocation.y = 0;

		child_allocation.width  = MAX (allocation->width  - border_width * 2, 0);
		child_allocation.height = MAX (allocation->height - border_width * 2, 0);

		if (gtk_widget_get_realized (widget))
			gdk_window_move_resize (gtk_widget_get_window (widget),
						allocation->x + border_width,
						allocation->y + border_width,
						child_allocation.width,
						child_allocation.height);

		child = gtk_bin_get_child (bin);
		if (child)
			gtk_widget_size_allocate (child, &child_allocation);
	}
}

static gboolean
panel_applet_draw (GtkWidget *widget,
		   cairo_t   *cr)
{
        GtkStyleContext *context;
	int border_width;
	gdouble x, y, width, height;

	GTK_WIDGET_CLASS (panel_applet_parent_class)->draw (widget, cr);

        if (!gtk_widget_has_focus (widget))
		return FALSE;

        width = gtk_widget_get_allocated_width (widget);
        height = gtk_widget_get_allocated_height (widget);

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
	case PROP_ORIENT:
		g_value_set_uint (value, applet->priv->orient);
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
			g_closure_set_marshal (applet->priv->closure, g_cclosure_marshal_generic);
		break;
	case PROP_CONNECTION:
		applet->priv->connection = g_value_dup_object (value);
		break;
	case PROP_SETTINGS_PATH:
		panel_applet_set_settings_path (applet, g_value_get_string (value));
		break;
	case PROP_ORIENT:
		panel_applet_set_orient (applet, g_value_get_uint (value));
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
panel_applet_setup (PanelApplet *applet)
{
	GValue   value = {0, };
	GArray  *params;
	guint    i;
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
	GtkStyleContext *context;
	gchar *xml;

	applet->priv = panel_applet_get_instance_private (applet);

	applet->priv->flags  = PANEL_APPLET_FLAGS_NONE;
	applet->priv->orient = PANEL_APPLET_ORIENT_UP;

	applet->priv->panel_action_group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (applet->priv->panel_action_group),
	                                 menu_entries,
	                                 G_N_ELEMENTS (menu_entries),
	                                 applet);

	applet->priv->builder = gtk_builder_new ();

	xml = g_strdup_printf (panel_menu_ui, _("_Remove From Panel"), _("_Move"));
	gtk_builder_add_from_string (applet->priv->builder, xml, -1, NULL);
	g_free (xml);

	gtk_widget_insert_action_group (GTK_WIDGET (applet), "libpanel-applet",
	                                G_ACTION_GROUP (applet->priv->panel_action_group));

	gtk_widget_set_events (GTK_WIDGET (applet),
			       GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK);

	context = gtk_widget_get_style_context (GTK_WIDGET (applet));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);

	panel_applet_composited_changed (GTK_WIDGET (applet));
}

static void
panel_applet_constructed (GObject *object)
{
	PanelApplet *applet = PANEL_APPLET (object);

	if (!applet->priv->connection || !applet->priv->closure || !applet->priv->id) {
		g_printerr ("Bad use of PanelApplet API: you should not create a PanelApplet object yourself.\n");
		g_assert_not_reached ();
	}

	panel_applet_register_object (applet);
}

static void
panel_applet_class_init (PanelAppletClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

	gobject_class->get_property = panel_applet_get_property;
	gobject_class->set_property = panel_applet_set_property;
	gobject_class->constructed = panel_applet_constructed;
        gobject_class->finalize = panel_applet_finalize;

	widget_class->button_press_event = panel_applet_button_press;
	widget_class->composited_changed = panel_applet_composited_changed;
	widget_class->key_press_event = panel_applet_key_press_event;
	widget_class->get_request_mode = panel_applet_get_request_mode;
	widget_class->size_allocate = panel_applet_size_allocate;
	widget_class->draw = panel_applet_draw;
	widget_class->focus = panel_applet_focus;

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

	gtk_widget_class_set_css_name (widget_class, "panel-applet");
}

static GdkEvent *
button_press_event_new (PanelApplet *applet,
                        guint        button,
                        guint        time)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *device;
  GdkEvent *event;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);

  event = gdk_event_new (GDK_BUTTON_PRESS);

  event->button.time = time;
  event->button.button = button;

  gdk_event_set_device (event, device);

  return event;
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
	GdkEvent *event;

	if (g_strcmp0 (method_name, "PopupMenu") == 0) {
		guint button;
		guint time;

		g_variant_get (parameters, "(uu)", &button, &time);

		event = button_press_event_new (applet, button, time);
		panel_applet_menu_popup (applet, event);
		gdk_event_free (event);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "PopupEditMenu") == 0) {
		guint button;
		guint time;

		g_variant_get (parameters, "(uu)", &button, &time);

		event = button_press_event_new (applet, button, time);
		panel_applet_edit_menu_popup (applet, event);
		gdk_event_free (event);

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
	} else if (g_strcmp0 (property_name, "Orient") == 0) {
		retval = g_variant_new_uint32 (applet->priv->orient);
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
	} else if (g_strcmp0 (property_name, "Orient") == 0) {
		panel_applet_set_orient (applet, g_variant_get_uint32 (value));
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
	    "<property name='Orient' type='u' access='readwrite' />"
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

static int
_panel_applet_factory_main_internal (const gchar               *factory_id,
				     GType                      applet_type,
				     PanelAppletFactoryCallback callback,
				     gpointer                   user_data)
{
	PanelAppletFactory *factory;
	GClosure           *closure;

	g_return_val_if_fail (factory_id != NULL, 1);
	g_return_val_if_fail (callback != NULL, 1);
	g_assert (g_type_is_a (applet_type, PANEL_TYPE_APPLET));

	closure = g_cclosure_new (G_CALLBACK (callback), user_data, NULL);
	factory = panel_applet_factory_new (factory_id, applet_type, closure);
	g_closure_unref (closure);

	if (panel_applet_factory_register_service (factory)) {
		return 0;
	}

	g_object_unref (factory);

	return 1;
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
 * %PANEL_APPLET_IN_PROCESS_FACTORY.
 *
 * Returns: 0 on success, 1 if there is an error.
 **/
int
panel_applet_factory_setup_in_process (const gchar               *factory_id,
				       GType                      applet_type,
				       PanelAppletFactoryCallback callback,
				       gpointer                   data)
{
	return _panel_applet_factory_main_internal (factory_id, applet_type,
						    callback, data);
}

const gchar *
panel_applet_get_object_path (PanelApplet *applet)
{
	return applet->priv->object_path;
}

G_MODULE_EXPORT GtkWidget *
panel_applet_get_applet_widget (const gchar *factory_id,
                                guint        uid)
{
	GtkWidget *widget;

	widget = panel_applet_factory_get_applet_widget (factory_id, uid);
	if (!widget) {
		return NULL;
	}

	panel_applet_setup (PANEL_APPLET (widget));

	return widget;
}

/*
 * panel-applet.c: panel applet writing library.
 *
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
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtktooltips.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-types.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-item-handler.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-property-bag-client.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include "panel-applet.h"
#include "panel-applet-private.h"
#include "panel-applet-shell.h"
#include "panel-applet-marshal.h"
#include "panel-applet-enums.h"

struct _PanelAppletPrivate {
	PanelAppletShell           *shell;
	BonoboControl              *control;
	BonoboPropertyBag          *prop_sack;
	BonoboItemHandler          *item_handler;

	char                       *iid;
	GClosure                   *closure;
	gboolean                    bound;
	char                       *prefs_key;

	PanelAppletFlags            flags;
	PanelAppletOrient           orient;
	guint                       size;
	char                       *background;

	GdkPixmap                  *bg_pixmap;

        int                        *size_hints;
        int                         size_hints_len;

	gboolean                    moving_focus_out;
	int                         focusable_child;
	guint                       hierarchy_changed_id;
};

static GObjectClass *parent_class;

enum {
        CHANGE_ORIENT,
        CHANGE_SIZE,
        CHANGE_BACKGROUND,
	MOVE_FOCUS_OUT_OF_APPLET,
        SAVE_YOURSELF,
        LAST_SIGNAL
};

static guint panel_applet_signals [LAST_SIGNAL];

#define PROPERTY_ORIENT     "panel-applet-orient"
#define PROPERTY_SIZE       "panel-applet-size"
#define PROPERTY_BACKGROUND "panel-applet-background"
#define PROPERTY_FLAGS      "panel-applet-flags"
#define PROPERTY_SIZE_HINTS "panel-applet-size-hints"

enum {
	PROPERTY_ORIENT_IDX,
	PROPERTY_SIZE_IDX,
	PROPERTY_BACKGROUND_IDX,
	PROPERTY_FLAGS_IDX,
	PROPERTY_SIZE_HINTS_IDX,
};

static void
panel_applet_associate_schemas_in_dir (GConfClient  *client,
				       const gchar  *prefs_key,
				       const gchar  *schema_dir,
				       GError      **error)
{
	GSList *list, *l;

	list = gconf_client_all_entries (client, schema_dir, error);

	g_return_if_fail (*error == NULL);

	for (l = list; l; l = l->next) {
		GConfEntry *entry = l->data;
		gchar      *key;
		gchar      *tmp;

		tmp = g_path_get_basename (gconf_entry_get_key (entry));

		if (strchr (tmp, '-'))
			g_warning ("Applet key '%s' contains a hyphen. Please "
				   "use underscores in gconf keys\n", tmp);

		key = g_strdup_printf ("%s/%s", prefs_key, tmp);

		g_free (tmp);

		gconf_engine_associate_schema (
			client->engine, key, gconf_entry_get_key (entry), error);

		g_free (key);

		gconf_entry_free (entry);

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

		prefs_subdir  = g_strdup_printf ("%s/%s", prefs_key, subdir);
		schema_subdir = g_strdup_printf ("%s/%s", schema_dir, subdir);

		panel_applet_associate_schemas_in_dir (
			client, prefs_subdir, schema_subdir, error);

		g_free (prefs_subdir);
		g_free (schema_subdir);
		g_free (subdir);

		if (*error) {
			g_slist_free (list);
			return;
		}
	}

	g_slist_free (list);
}

void
panel_applet_add_preferences (PanelApplet  *applet,
			      const gchar  *schema_dir,
			      GError      **opt_error)
{
	GConfClient  *client;
	GError      **error = NULL;
	GError       *our_error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));
	g_return_if_fail (applet->priv->prefs_key != NULL);

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	client = gconf_client_get_default ();

	panel_applet_associate_schemas_in_dir (
		client, applet->priv->prefs_key, schema_dir, error);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": failed to add preferences from '%s' : '%s'",
			   schema_dir, our_error->message);
		g_error_free (our_error);
	}

	gconf_client_add_dir (
		client, applet->priv->prefs_key, GCONF_CLIENT_PRELOAD_NONE, NULL);
}

gchar *
panel_applet_get_preferences_key (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (!applet->priv->prefs_key)
		return NULL;

	return g_strdup (applet->priv->prefs_key);
}

PanelAppletFlags
panel_applet_get_flags (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), PANEL_APPLET_FLAGS_NONE);

	return applet->priv->flags;
}

void
panel_applet_set_flags (PanelApplet      *applet,
			PanelAppletFlags  flags)
{
	g_return_if_fail (PANEL_IS_APPLET (applet));

	applet->priv->flags = flags;
}

void
panel_applet_set_size_hints (PanelApplet      *applet,
			     const int        *size_hints,
			     int               n_elements,
			     int               base_size)
{
	int i;
	applet->priv->size_hints = g_realloc (applet->priv->size_hints, n_elements * sizeof (int));
	memcpy (applet->priv->size_hints, size_hints, n_elements * sizeof (int));
	applet->priv->size_hints_len = n_elements;

	for (i = 0; i < n_elements; i++)
		applet->priv->size_hints[i] += base_size;
}

/**
 * panel_applet_get_size:
 * @applet: A #PanelApplet.
 *
 * Returns the current size of the panel which
 * the applet is contained in.
 *
 * Return value: a #guint value.
 */
guint
panel_applet_get_size (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), 0);

	return applet->priv->size;
}

/**
 * panel_applet_get_orient
 * @applet: A #PanelApplet.
 *
 * Returns the current orientation of the panel which
 * the applet is contained in.
 *
 * Return value: a #PanelAppletOrient value.
 */
PanelAppletOrient
panel_applet_get_orient (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), 0);

	return applet->priv->orient;
}

/**
 * panel_applet_setup_menu:
 * @applet: A #PanelApplet.
 * @xml: The xml character string describing the popup menu.
 * @verb_list: The list of #BonoboUIVerbs for the menu.
 * @user_data: The user data pointer for the menu.
 *
 * Sets up a popup menu for @applet described by the xml 
 * string, @xml. See <link linkend="applet-writing">Applet Writing
 * </link> section for a description of the format of the xml.
 */
void
panel_applet_setup_menu (PanelApplet        *applet,
			 const gchar        *xml,
			 const BonoboUIVerb *verb_list,
			 gpointer            user_data)
{
	BonoboUIComponent *popup_component;

	g_return_if_fail (PANEL_IS_APPLET (applet));
	g_return_if_fail (xml != NULL && verb_list != NULL);

	popup_component = panel_applet_get_popup_component (applet);

	bonobo_ui_component_set (popup_component, "/", "<popups/>", NULL);

	bonobo_ui_component_set_translate (popup_component, "/popups", xml, NULL);

	bonobo_ui_component_add_verb_list_with_data (popup_component, verb_list, user_data);
}

/**
 * panel_applet_setup_menu_from_file:
 * @applet: A #PanelApplet.
 * @opt_datadir: The data directory - i.e. ${prefix}/share (optional).
 * @file: The file's name.
 * @opt_app_name: The application's name (optional).
 * @verb_list: The list of #BonoboUIVerbs for the menu.
 * @user_data: The user data pointer for the menu.
 *
 * Sets up a popup menu for @applet described by the xml 
 * file, @file. See &applet-writing for a description of
 * the format of the xml.
 */
void
panel_applet_setup_menu_from_file (PanelApplet        *applet, 
				   const gchar        *opt_datadir,
				   const gchar        *file,
				   const gchar        *opt_app_name,
				   const BonoboUIVerb *verb_list,
				   gpointer            user_data)
{
	BonoboUIComponent *popup_component;
	gchar             *app_name = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));
	g_return_if_fail (file != NULL && verb_list != NULL);

	if (!opt_datadir)
		opt_datadir = PANEL_APPLET_DATADIR;

	if (!opt_app_name)
		opt_app_name = app_name = g_strdup_printf ("%d", getpid ());

	popup_component = panel_applet_get_popup_component (applet);

	bonobo_ui_util_set_ui (popup_component, opt_datadir, file, opt_app_name, NULL);

	bonobo_ui_component_add_verb_list_with_data (popup_component, verb_list, user_data);

	if (app_name)
		g_free (app_name);
}

/**
 * panel_applet_get_control:
 * @applet: A #PanelApplet.
 *
 * Retrieves the #BonoboControl associated with @applet.
 *
 * Return value: A #BonobControl on success, %NULL on failure.
 */
BonoboControl *
panel_applet_get_control (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	return applet->priv->control;
}

/**
 * panel_applet_get_popup_component:
 * @applet: A #PanelApplet.
 *
 * Retrieves the #BonoboUIComponent used for popup menus associated
 * with @applet. 
 *
 * Return value: A #BonoboUIComponent on success, or %NULL on failure.
 */
BonoboUIComponent *
panel_applet_get_popup_component (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	return bonobo_control_get_popup_ui_component (applet->priv->control);
}

static void
panel_applet_finalize (GObject *object)
{
	PanelApplet *applet = PANEL_APPLET (object);

	if (applet->priv->prop_sack)
		bonobo_object_unref (
			BONOBO_OBJECT (applet->priv->prop_sack));

	g_free (applet->priv->size_hints);
	g_free (applet->priv->prefs_key);
	g_free (applet->priv->background);
	g_free (applet->priv->iid);

	g_free (applet->priv);
	applet->priv = NULL;

	parent_class->finalize (object);
}

static GtkWidget*
panel_applet_container_has_focusable_child (GtkWidget *widget)
{
	GtkContainer *container;
	GtkWidget *child;
	GList *list;
	GList *t;
	GtkWidget *retval = NULL;

	container = GTK_CONTAINER (widget);
	list = gtk_container_get_children (container);

	for (t = list; t; t = t->next) {
		child = GTK_WIDGET (t->data);
		if (GTK_WIDGET_CAN_FOCUS (child)) {
			retval = child;
			break;
		} else if (GTK_IS_CONTAINER (child)) {
			retval = panel_applet_container_has_focusable_child (child);
			if (retval)
				break;
		}
	}
	g_list_free (list);
	return retval;	
}

static void
panel_applet_hierarchy_changed_cb (GtkWidget *widget)
{
	PanelApplet *applet = PANEL_APPLET (widget);

	applet->priv->focusable_child = -1;
}

static gboolean
panel_applet_has_focusable_child (PanelApplet *applet)
{
	if (applet->priv->focusable_child == -1) {
		GtkWidget *focusable_child;

		if (!applet->priv->hierarchy_changed_id)
			applet->priv->hierarchy_changed_id = 
				g_signal_connect (applet, "hierarchy-changed", 
				  		  G_CALLBACK (panel_applet_hierarchy_changed_cb),
				  		  NULL);

		focusable_child = panel_applet_container_has_focusable_child (GTK_WIDGET (applet));
		applet->priv->focusable_child = (focusable_child != NULL);
	}
	return  (applet->priv->focusable_child != 0);
}

static void
panel_applet_position_menu (GtkMenu   *menu,
			    int       *x,
			    int       *y,
			    gboolean  *push_in,
			    GtkWidget *widget)
{
	PanelApplet    *applet;
	GtkRequisition  requisition;
	GdkScreen      *screen;
	int             menu_x = 0;
	int             menu_y = 0;

	g_return_if_fail (PANEL_IS_APPLET (widget));

	applet = PANEL_APPLET (widget);

	screen = gtk_widget_get_screen (widget);

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	gdk_window_get_origin (widget->window, &menu_x, &menu_y);

	menu_x += widget->allocation.x;
	menu_y += widget->allocation.y;

	if (applet->priv->orient == PANEL_APPLET_ORIENT_UP ||
	    applet->priv->orient == PANEL_APPLET_ORIENT_DOWN) {
		if (menu_y > gdk_screen_get_height (screen) / 2)
			menu_y -= requisition.height;
		else
			menu_y += widget->allocation.height;
	} else  {
		if (menu_x > gdk_screen_get_width (screen) / 2)
			menu_x -= requisition.width;
		else
			menu_x += widget->allocation.width;

	}

	*x = menu_x;
	*y = menu_y;
	*push_in = TRUE;
}

static gboolean
panel_applet_can_focus (GtkWidget *widget)
{
	/*
	 * A PanelApplet widget can focus if it has a tooltip or it does 
	 * not have any focusable children.
	 */
	if (gtk_tooltips_data_get (widget))
		return TRUE;

	if (!PANEL_IS_APPLET (widget))
		return FALSE;

	return !panel_applet_has_focusable_child (PANEL_APPLET (widget));
}

static gboolean
panel_applet_button_press (GtkWidget      *widget,
			   GdkEventButton *event)
{
	PanelApplet *applet = PANEL_APPLET (widget);

	if (!panel_applet_has_focusable_child (applet)) {
		if (!GTK_WIDGET_HAS_FOCUS (widget)) {
			GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
			gtk_widget_grab_focus (widget);
		}
	}

	if (event->button == 1)
		return TRUE;
	else if (event->button == 3) {
		bonobo_control_do_popup_full (
				applet->priv->control, 
				NULL, NULL,
				(GtkMenuPositionFunc) panel_applet_position_menu,
				applet,
				event->button,
				event->time);
		return TRUE;
	}

	return FALSE;
}

gboolean
_panel_applet_popup_menu (PanelApplet *applet,
			  guint button,
			  guint32 time)			  
{
	bonobo_control_do_popup_full (applet->priv->control, NULL, NULL,
				      (GtkMenuPositionFunc) panel_applet_position_menu,
				      applet, button, time);
	return TRUE;
}

static gboolean
panel_applet_popup_menu (PanelApplet *applet)
{
	return _panel_applet_popup_menu (applet, 3, GDK_CURRENT_TIME);
}

static void
panel_applet_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	int focus_width = 0;

	GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

	if (!panel_applet_can_focus (widget))
		return;

	/*
	 * We are deliberately ignoring focus-padding here to
	 * save valuable panel real estate.
	 */
	gtk_widget_style_get (widget,
			      "focus-line-width", &focus_width,
			      NULL);

	requisition->width  += 2 * focus_width;
	requisition->height += 2 * focus_width;
}

static void
panel_applet_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
	GtkAllocation  child_allocation;
	GtkBin        *bin;
	int            border_width;
	int            focus_width = 0;

	if (!panel_applet_can_focus (widget)) {
		GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
		return;
	}

	/*
	 * We are deliberately ignoring focus-padding here to
	 * save valuable panel real estate.
	 */
	gtk_widget_style_get (widget,
			      "focus-line-width", &focus_width,
			      NULL);

	border_width = GTK_CONTAINER (widget)->border_width;

	widget->allocation = *allocation;
	bin = GTK_BIN (widget);

 	child_allocation.x = focus_width;
 	child_allocation.y = focus_width;

	child_allocation.width  = MAX (allocation->width  - border_width * 2, 0);
	child_allocation.height = MAX (allocation->height - border_width * 2, 0);

	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window,
					allocation->x + GTK_CONTAINER (widget)->border_width,
					allocation->y + GTK_CONTAINER (widget)->border_width,
					child_allocation.width,
					child_allocation.height);

	child_allocation.width  = MAX (child_allocation.width  - 2 * focus_width, 0);
	child_allocation.height = MAX (child_allocation.height - 2 * focus_width, 0);

	if (bin->child)
		gtk_widget_size_allocate (bin->child, &child_allocation);
}

static gboolean
panel_applet_expose (GtkWidget      *widget,
		     GdkEventExpose *event) 
{
	int border_width;
	int focus_width = 0;
	int x, y, width, height;

	g_return_val_if_fail (PANEL_IS_APPLET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

        if (!GTK_WIDGET_HAS_FOCUS (widget))
		return FALSE;

	/*
	 * We are deliberately ignoring focus-padding here to
	 * save valuable panel real estate.
	 */
	gtk_widget_style_get (widget,
			      "focus-line-width", &focus_width,
			      NULL);

	border_width = GTK_CONTAINER (widget)->border_width;

	x = widget->allocation.x;
	y = widget->allocation.y;

	width  = widget->allocation.width  - 2 * border_width;
	height = widget->allocation.height - 2 * border_width;

	gtk_paint_focus (widget->style, widget->window,
			 GTK_WIDGET_STATE (widget),
			 &event->area, widget, "panel_applet",
			 x, y, width, height);

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

	previous_focus_child = GTK_CONTAINER (widget)->focus_child;
	 if (!previous_focus_child && !GTK_WIDGET_HAS_FOCUS (widget)) {
		GtkTooltipsData *tooltip;

		tooltip = gtk_tooltips_data_get (widget);
		if (tooltip) {
			GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
			gtk_widget_grab_focus (widget);
			GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
			return TRUE;
		}
	}
	ret = GTK_WIDGET_CLASS (parent_class)->focus (widget, dir);
	if (!ret && !previous_focus_child) {
 		if (!GTK_WIDGET_HAS_FOCUS (widget))  {
			/*
			 * Applet does not have a widget which can focus so set
			 * the focus on the applet unless it aleready had focus
			 * because it had a tooltip.
			 */ 
			GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
			gtk_widget_grab_focus (widget);
			ret = TRUE;
		}
	}

	return ret;
}

static gboolean
panel_applet_parse_color (const gchar *color_str,
			  GdkColor    *color)
{
	int r, g, b;

	g_assert (color_str && color);

	if (sscanf (color_str, "%4x%4x%4x", &r, &g, &b) != 3)
		return FALSE;

	color->red   = r;
	color->green = g;
	color->blue  = b;
		
	return TRUE;
}

static gboolean
panel_applet_parse_pixmap_str (const char *str,
			       GdkNativeWindow *xid,
			       int             *x,
			       int             *y)
{
	char **elements;
	char  *tmp;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (xid != NULL, FALSE);
	g_return_val_if_fail (x != NULL, FALSE);
	g_return_val_if_fail (y != NULL, FALSE);

	elements = g_strsplit (str, ",", -1);

	if (!elements)
		return FALSE;

	if (!elements [0] || !*elements [0] ||
	    !elements [1] || !*elements [1] ||
	    !elements [2] || !*elements [2])
		goto ERROR_AND_FREE;

	*xid = strtol (elements [0], &tmp, 10);
	if (tmp == elements [0])
		goto ERROR_AND_FREE;

	*x   = strtol (elements [1], &tmp, 10);
	if (tmp == elements [1])
		goto ERROR_AND_FREE;

	*y   = strtol (elements [2], &tmp, 10);
	if (tmp == elements [2])
		goto ERROR_AND_FREE;

 	g_strfreev (elements);
	return TRUE;

 ERROR_AND_FREE:
 	g_strfreev (elements);
	return FALSE;
}

static GdkPixmap *
panel_applet_get_pixmap (PanelApplet     *applet,
			 GdkNativeWindow  xid,
			 int              x,
			 int              y)
{
	GdkWindow *window;
	GdkPixmap *retval;
	GdkGC     *gc;
	int        width;
	int        height;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (!GTK_WIDGET_REALIZED (applet))
		return NULL;

	window = gdk_window_lookup (xid);
	if (window)
		g_object_ref (window);
	else
		window = gdk_window_foreign_new (xid);

	g_return_val_if_fail (window != NULL, NULL);

	gdk_drawable_get_size (GDK_DRAWABLE (GTK_WIDGET (applet)->window),
			       &width, &height);

	retval = gdk_pixmap_new (GTK_WIDGET (applet)->window, width, height, -1);

	gc = gdk_gc_new (GDK_DRAWABLE (GTK_WIDGET (applet)->window));

	g_return_val_if_fail (GDK_IS_GC (gc), NULL);

	gdk_draw_drawable (GDK_DRAWABLE (retval),
			   gc, 
			   GDK_DRAWABLE (window),
			   x, y,
			   0, 0,
			   width, height);

	g_object_unref (gc);
	g_object_unref (window);

	return retval;
}

static PanelAppletBackgroundType
panel_applet_handle_background_string (PanelApplet  *applet,
				       GdkColor     *color,
				       GdkPixmap   **pixmap)
{
	PanelAppletBackgroundType   retval;
	char                      **elements;

	retval = PANEL_NO_BACKGROUND;

	if (!GTK_WIDGET_REALIZED (applet) || !applet->priv->background)
		return retval;

	elements = g_strsplit (applet->priv->background, ":", -1);

	if (elements [0] && !strcmp (elements [0], "none" )) {
		retval = PANEL_NO_BACKGROUND;
		
	} else if (elements [0] && !strcmp (elements [0], "color")) {
		g_return_val_if_fail (color != NULL, PANEL_NO_BACKGROUND);

		if (!elements [1] || !panel_applet_parse_color (elements [1], color)) {

			g_warning (_("Incomplete '%s' background type received"), elements [0]);
			g_strfreev (elements);
			return PANEL_NO_BACKGROUND;
		}

		retval = PANEL_COLOR_BACKGROUND;

	} else if (elements [0] && !strcmp (elements [0], "pixmap")) {
		GdkNativeWindow pixmap_id;
		int             x, y;

		g_return_val_if_fail (pixmap != NULL, PANEL_NO_BACKGROUND);

		if (!panel_applet_parse_pixmap_str (elements [1], &pixmap_id, &x, &y)) {
			g_warning (_("Incomplete '%s' background type received: %s"),
				   elements [0], elements [1]);

			g_strfreev (elements);
			return PANEL_NO_BACKGROUND;
		}

		*pixmap = panel_applet_get_pixmap (applet, pixmap_id, x, y);
		if (!*pixmap) {
			g_warning (_("Failed to get pixmap %s"), elements [1]);
			g_strfreev (elements);
			return PANEL_NO_BACKGROUND;
		}

		retval = PANEL_PIXMAP_BACKGROUND;
	} else
		g_warning (_("Unknown background type received"));

	g_strfreev (elements);

	return retval;
}


/**
 * panel_applet_get_background
 * @applet: A #PanelApplet.
 * @color: A #GdkColor to be filled in.
 * @pixmap: Returned #GdkPixmap
 *
 * Returns the current background type. If the background
 * type is %PANEL_NO_BACKGROUND both @color and @pixmap will
 * be unaffected. If the background type is %PANEL_COLOR_BACKGROUND
 * then @color will contain the current panel background colour.
 * If the background type is %PANEL_PIXMAP_BACKGROUND, @pixmap will
 * contain a pointer to a #GdkPixmap which is a copy of the applet's
 * portion of the panel's background pixmap.
 * 
 * Return value: a #PanelAppletOrient value.
 */
PanelAppletBackgroundType
panel_applet_get_background (PanelApplet *applet,
			     GdkColor *color,
			     GdkPixmap **pixmap)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), PANEL_NO_BACKGROUND);

	/* initial sanity */
	if (pixmap != NULL)
		*pixmap = NULL;
	if (color != NULL)
		memset (color, 0, sizeof (GdkColor));

	return panel_applet_handle_background_string (applet, color, pixmap);
}

static void
panel_applet_get_prop (BonoboPropertyBag *sack,
                       BonoboArg         *arg,
		       guint              arg_id,
		       CORBA_Environment *ev,
		       gpointer           user_data)
{
	PanelApplet *applet = PANEL_APPLET (user_data);

	switch (arg_id) {
	case PROPERTY_ORIENT_IDX:
		BONOBO_ARG_SET_SHORT (arg, applet->priv->orient);
		break;
	case PROPERTY_SIZE_IDX:
		BONOBO_ARG_SET_SHORT (arg, applet->priv->size);
		break;
	case PROPERTY_BACKGROUND_IDX:
		BONOBO_ARG_SET_STRING (arg, applet->priv->background);
		break;
	case PROPERTY_FLAGS_IDX:
		BONOBO_ARG_SET_SHORT (arg, applet->priv->flags);
		break;
	case PROPERTY_SIZE_HINTS_IDX: {
		CORBA_sequence_CORBA_long *seq;
		int                        i;

		seq = arg->_value;

		seq->_length  = seq->_maximum = applet->priv->size_hints_len;
		seq->_buffer  = CORBA_sequence_CORBA_long_allocbuf (seq->_length);
		seq->_release = CORBA_TRUE;
		
		for (i = 0; i < applet->priv->size_hints_len; i++)
			seq->_buffer [i] = applet->priv->size_hints[i];
		}
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
panel_applet_handle_background (PanelApplet *applet)
{
	PanelAppletBackgroundType  type;
	GdkColor                   color;
	GdkPixmap                 *pixmap = NULL;

	type = panel_applet_handle_background_string (applet, &color, &pixmap);

	switch (type) {
	case PANEL_NO_BACKGROUND:
		g_signal_emit (G_OBJECT (applet),
			       panel_applet_signals [CHANGE_BACKGROUND],
			       0, PANEL_NO_BACKGROUND, NULL, NULL);
		break;
	case PANEL_COLOR_BACKGROUND:
		g_signal_emit (G_OBJECT (applet),
			       panel_applet_signals [CHANGE_BACKGROUND],
			       0, PANEL_COLOR_BACKGROUND, &color, NULL);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		g_signal_emit (G_OBJECT (applet),
			       panel_applet_signals [CHANGE_BACKGROUND],
			       0, PANEL_PIXMAP_BACKGROUND, NULL, pixmap);

		g_object_unref (pixmap);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
panel_applet_set_prop (BonoboPropertyBag *sack,
		       const BonoboArg   *arg,
		       guint              arg_id,
		       CORBA_Environment *ev,
		       gpointer           user_data)
{
	PanelApplet *applet = PANEL_APPLET (user_data);
	
	switch (arg_id) {
	case PROPERTY_ORIENT_IDX: {
		PanelAppletOrient orient;

		orient = BONOBO_ARG_GET_SHORT (arg);

		if (applet->priv->orient != orient) {
			applet->priv->orient = orient;

			g_signal_emit (G_OBJECT (applet),
				       panel_applet_signals [CHANGE_ORIENT],
				       0, orient);
		}
		}
		break;
	case PROPERTY_SIZE_IDX: {
		guint size;

		size = BONOBO_ARG_GET_SHORT (arg);

		if (applet->priv->size != size) {
			applet->priv->size = size;

			g_signal_emit (G_OBJECT (applet),
                                       panel_applet_signals [CHANGE_SIZE],
                                       0, size);
		}
		}
		break;
	case PROPERTY_BACKGROUND_IDX:
		if (applet->priv->background)
			g_free (applet->priv->background);

		applet->priv->background = g_strdup (BONOBO_ARG_GET_STRING (arg));

		panel_applet_handle_background (applet);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static BonoboPropertyBag *
panel_applet_property_bag (PanelApplet *applet)
{
	BonoboPropertyBag *sack;

	sack = bonobo_property_bag_new (panel_applet_get_prop, 
					panel_applet_set_prop,
					applet);

	bonobo_property_bag_add (sack,
				 PROPERTY_ORIENT,
				 PROPERTY_ORIENT_IDX,
				 BONOBO_ARG_SHORT,
				 NULL,
				 _("The Applet's containing Panel's orientation"),
				 Bonobo_PROPERTY_READABLE | Bonobo_PROPERTY_WRITEABLE);

	bonobo_property_bag_add (sack,
				 PROPERTY_SIZE,
				 PROPERTY_SIZE_IDX,
				 BONOBO_ARG_SHORT,
				 NULL,
				 _("The Applet's containing Panel's size in pixels"),
				 Bonobo_PROPERTY_READABLE | Bonobo_PROPERTY_WRITEABLE);

	bonobo_property_bag_add (sack,
				 PROPERTY_BACKGROUND,
				 PROPERTY_BACKGROUND_IDX,
				 BONOBO_ARG_STRING,
				 NULL,
				 _("The Applet's containing Panel's background color or pixmap"),
				 Bonobo_PROPERTY_READABLE | Bonobo_PROPERTY_WRITEABLE);

	bonobo_property_bag_add (sack,
				 PROPERTY_FLAGS,
				 PROPERTY_FLAGS_IDX,
				 BONOBO_ARG_SHORT,
				 NULL,
				 _("The Applet's flags"),
				 Bonobo_PROPERTY_READABLE);

	bonobo_property_bag_add (sack,
				 PROPERTY_SIZE_HINTS,
				 PROPERTY_SIZE_HINTS_IDX,
				 TC_CORBA_sequence_CORBA_long,
				 NULL,
				 _("Ranges that hint what sizes are acceptable for the applet"),
				 Bonobo_PROPERTY_READABLE);

	return sack;
}

static void
panel_applet_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	if (PANEL_APPLET (widget)->priv->background)
		panel_applet_handle_background (PANEL_APPLET (widget));
}

static void
panel_applet_control_bound (BonoboControl *control,
			    PanelApplet   *applet)
{
	gboolean ret;

	g_return_if_fail (PANEL_IS_APPLET (applet));
	g_return_if_fail (applet->priv->iid != NULL &&
			  applet->priv->closure != NULL);

	if (applet->priv->bound)
		return;

	bonobo_closure_invoke (applet->priv->closure,
			       G_TYPE_BOOLEAN, &ret,
			       PANEL_TYPE_APPLET, applet,
			       G_TYPE_STRING, applet->priv->iid,
			       0);


	if (!ret) { /* FIXME */
		g_warning ("need to free the control here");

		return;
	}

	applet->priv->bound = TRUE;
}

static Bonobo_Unknown
panel_applet_item_handler_get_object (BonoboItemHandler *handler,
				      const char        *item_name,
				      gboolean           only_if_exists,
				      gpointer           user_data,
				      CORBA_Environment *ev)
{
	PanelApplet *applet = user_data;
	GSList      *options;
	GSList      *l;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), CORBA_OBJECT_NIL);

	options = bonobo_item_option_parse (item_name);

	for (l = options; l; l = l->next) {
		BonoboItemOption *option = l->data;

		if (!option->value || !option->value [0])
			continue;

		if (!strcmp (option->key, "prefs_key") && !applet->priv->prefs_key)
			applet->priv->prefs_key = g_strdup (option->value);

		else if (!strcmp (option->key, "background"))
			bonobo_pbclient_set_string (BONOBO_OBJREF (applet->priv->prop_sack),
						    PROPERTY_BACKGROUND, option->value, NULL);

		else if (!strcmp (option->key, "orient")) {
			if (!strcmp (option->value, "up"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_ORIENT,
					PANEL_APPLET_ORIENT_UP, NULL);

			else if (!strcmp (option->value, "down"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_ORIENT,
					PANEL_APPLET_ORIENT_DOWN, NULL);

			else if (!strcmp (option->value, "left"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_ORIENT,
					PANEL_APPLET_ORIENT_LEFT, NULL);

			else if (!strcmp (option->value, "right"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_ORIENT,
					PANEL_APPLET_ORIENT_RIGHT, NULL);

		} else if (!strcmp (option->key, "size")) {
			if (!strcmp (option->value, "xx-small"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_SIZE,
					GNOME_Vertigo_PANEL_XX_SMALL, NULL);

			else if (!strcmp (option->value, "x-small"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_SIZE,
					GNOME_Vertigo_PANEL_X_SMALL, NULL);

			else if (!strcmp (option->value, "small"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_SIZE,
					GNOME_Vertigo_PANEL_SMALL, NULL);

			else if (!strcmp (option->value, "medium"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_SIZE,
					GNOME_Vertigo_PANEL_MEDIUM, NULL);

			else if (!strcmp (option->value, "large"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_SIZE,
					GNOME_Vertigo_PANEL_LARGE, NULL);

			else if (!strcmp (option->value, "x-large"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_SIZE,
					GNOME_Vertigo_PANEL_X_LARGE, NULL);

			else if (!strcmp (option->value, "xx-large"))
				bonobo_pbclient_set_short (
					BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_SIZE,
					GNOME_Vertigo_PANEL_XX_LARGE, NULL);
		}
	}

	bonobo_item_options_free (options);

	return bonobo_object_dup_ref (BONOBO_OBJREF (applet->priv->control), ev);
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
add_tab_bindings (GtkBindingSet   *binding_set,
		  GdkModifierType  modifiers,
		  GtkDirectionType direction)
{
	gtk_binding_entry_add_signal (binding_set, GDK_Tab, modifiers,
				      "move_focus_out_of_applet", 1,
				      GTK_TYPE_DIRECTION_TYPE, direction);	
	gtk_binding_entry_add_signal (binding_set, GDK_KP_Tab, modifiers,
				      "move_focus_out_of_applet", 1,
				      GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
panel_applet_class_init (PanelAppletClass *klass,
			 gpointer          dummy)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
	GtkBindingSet *binding_set;

	parent_class = g_type_class_peek_parent (klass);

	klass->move_focus_out_of_applet = panel_applet_move_focus_out_of_applet;

	widget_class->button_press_event = panel_applet_button_press;
	widget_class->size_request = panel_applet_size_request;
	widget_class->size_allocate = panel_applet_size_allocate;
	widget_class->expose_event = panel_applet_expose;
	widget_class->focus = panel_applet_focus;
	widget_class->realize = panel_applet_realize;

	gobject_class->finalize = panel_applet_finalize;

	panel_applet_signals [CHANGE_ORIENT] =
                g_signal_new ("change_orient",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelAppletClass, change_orient),
                              NULL,
			      NULL,
                              panel_applet_marshal_VOID__INT,
                              G_TYPE_NONE,
			      1,
			      G_TYPE_INT);

	panel_applet_signals [CHANGE_SIZE] =
                g_signal_new ("change_size",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelAppletClass, change_size),
                              NULL,
			      NULL,
                              panel_applet_marshal_VOID__INT,
                              G_TYPE_NONE,
			      1,
			      G_TYPE_INT);

	panel_applet_signals [CHANGE_BACKGROUND] =
                g_signal_new ("change_background",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelAppletClass, change_background),
                              NULL,
			      NULL,
                              panel_applet_marshal_VOID__ENUM_POINTER_OBJECT,
                              G_TYPE_NONE,
			      3,
			      PANEL_TYPE_PANEL_APPLET_BACKGROUND_TYPE,
			      G_TYPE_POINTER,
			      GDK_TYPE_PIXMAP);

	panel_applet_signals [MOVE_FOCUS_OUT_OF_APPLET] =
                g_signal_new ("move_focus_out_of_applet",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (PanelAppletClass, move_focus_out_of_applet),
                              NULL,
			      NULL,
                              panel_applet_marshal_VOID__ENUM,
                              G_TYPE_NONE,
			      1,
			      GTK_TYPE_DIRECTION_TYPE);

	binding_set = gtk_binding_set_by_class (object_class);
	add_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
	add_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
	add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
	add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
}

static void
panel_applet_instance_init (PanelApplet      *applet,
			    PanelAppletClass *klass)
{
	applet->priv = g_new0 (PanelAppletPrivate, 1);

	applet->priv->bound        = FALSE;

	applet->priv->flags        = PANEL_APPLET_FLAGS_NONE;
	applet->priv->orient       = PANEL_APPLET_ORIENT_UP;
	applet->priv->size         = GNOME_Vertigo_PANEL_MEDIUM;

	applet->priv->moving_focus_out = FALSE;

	gtk_widget_set_events (GTK_WIDGET (applet), 
			       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
}

GType
panel_applet_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelAppletClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_applet_class_init,
			NULL,
			NULL,
			sizeof (PanelApplet),
			0,
			(GInstanceInitFunc) panel_applet_instance_init,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_EVENT_BOX, "PanelApplet",
					       &info, 0);
	}

	return type;
}

static void
panel_applet_setup (PanelApplet *applet)
{
	PanelAppletPrivate *priv;

	priv = applet->priv;

	priv->control = bonobo_control_new (GTK_WIDGET (applet));

	g_signal_connect (priv->control, "set_frame",
			  G_CALLBACK (panel_applet_control_bound),
			  applet);

	priv->prop_sack = panel_applet_property_bag (applet);

	bonobo_control_set_properties (
			priv->control, BONOBO_OBJREF (priv->prop_sack), NULL);

	priv->shell = panel_applet_shell_new (applet);

	bonobo_object_add_interface (BONOBO_OBJECT (priv->control),
				     BONOBO_OBJECT (priv->shell));

	priv->item_handler = 
		bonobo_item_handler_new (
			NULL, panel_applet_item_handler_get_object, applet);

	bonobo_object_add_interface (BONOBO_OBJECT (priv->control),
				     BONOBO_OBJECT (priv->item_handler));

	g_signal_connect (applet, "popup_menu",
			  G_CALLBACK (panel_applet_popup_menu), NULL);

	priv->focusable_child = -1;
	priv->hierarchy_changed_id  = 0;
}

/**
 * panel_applet_new:
 *
 * Creates a new #PanelApplet.
 *
 * Return value: A #GtkWidget on success, %NULL on failure.
 */
GtkWidget *
panel_applet_new (void)
{
	PanelApplet *applet;

	applet = g_object_new (PANEL_TYPE_APPLET, NULL);

	panel_applet_setup (applet);

	return GTK_WIDGET (applet);
}

typedef struct {
	GType     applet_type;
	GClosure *closure;
} PanelAppletCallBackData;

static PanelAppletCallBackData *
panel_applet_callback_data_new (GType     applet_type,
				GClosure *closure)
{
	PanelAppletCallBackData *retval;

	retval = g_new0 (PanelAppletCallBackData, 1);

	retval->applet_type = applet_type;
	retval->closure     = closure;

	return retval;
}

static void
panel_applet_callback_data_free (PanelAppletCallBackData *data)
{
	g_closure_unref (data->closure);
	g_free (data);
}

static BonoboObject *
panel_applet_factory_callback (BonoboGenericFactory    *factory,
			       const char              *iid,
			       PanelAppletCallBackData *data)
{
	PanelApplet *applet;

	applet = g_object_new (data->applet_type, NULL);

	panel_applet_setup (applet);

	applet->priv->iid     = g_strdup (iid);
	applet->priv->closure = g_closure_ref (data->closure);

	bonobo_control_life_instrument (applet->priv->control);

	return BONOBO_OBJECT (applet->priv->control);
}

static void
panel_applet_all_controls_dead (void)
{
	if (!bonobo_control_life_get_count())
		bonobo_main_quit ();
}

/**
 * panel_applet_factory_main_closure:
 * @iid: The bonobo-activation iid of the factory.
 * @closure: The factory callback closure.
 *
 * A generic 'main' routine for applets. This should not normally be
 * used directly because it is invoked by #PANEL_APPLET_BONOBO_FACTORY.
 *
 * Return value: 0 on success, 1 on failure.
 */
int
panel_applet_factory_main_closure (const gchar *iid,
				   GType        applet_type,
				   GClosure    *closure)
{
	int                      retval;
	char                    *display_iid;
	PanelAppletCallBackData *data;

	g_return_val_if_fail (iid != NULL, 1);
	g_return_val_if_fail (closure != NULL, 1);

	g_assert (g_type_is_a (applet_type, PANEL_TYPE_APPLET));

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	bonobo_control_life_set_callback (panel_applet_all_controls_dead);

	closure = bonobo_closure_store (closure, panel_applet_marshal_BOOLEAN__STRING);

	data = panel_applet_callback_data_new (applet_type, closure);

	display_iid = bonobo_activation_make_registration_id (
		iid, DisplayString (gdk_display));
	retval = bonobo_generic_factory_main (
		display_iid,
		(BonoboFactoryCallback) panel_applet_factory_callback,
		data);
	g_free (display_iid);

	panel_applet_callback_data_free (data);

	return retval;
}

/**
 * panel_applet_factory_main:
 * @iid: The bonobo-activation iid of the factory.
 * @callback: The factory callback.
 * @data: The factory user data pointer.
 *
 * A generic 'main' routine for applets. This should not normally be
 * used directly because it is invoked by #PANEL_APPLET_BONOBO_FACTORY.
 *
 * Return value: 0 on success, 1 on failure.
 */
int
panel_applet_factory_main (const gchar                 *iid,
			   GType                        applet_type,
			   PanelAppletFactoryCallback   callback,
			   gpointer                     data)
{
	GClosure *closure;

	g_return_val_if_fail (iid != NULL, 1);
	g_return_val_if_fail (callback != NULL, 1);

	closure = g_cclosure_new (G_CALLBACK (callback), data, NULL);

	return panel_applet_factory_main_closure (iid, applet_type, closure);
}

Bonobo_Unknown
panel_applet_shlib_factory_closure (const char         *iid,
				    GType               applet_type,
				    PortableServer_POA  poa,
				    gpointer            impl_ptr,
				    GClosure           *closure,
				    CORBA_Environment  *ev)
{
	BonoboShlibFactory *factory;

	g_return_val_if_fail (iid != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (closure != NULL, CORBA_OBJECT_NIL);

	g_assert (g_type_is_a (applet_type, PANEL_TYPE_APPLET));

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	closure = bonobo_closure_store (closure, panel_applet_marshal_BOOLEAN__STRING);
       
	factory = bonobo_shlib_factory_new_closure (
			iid, poa, impl_ptr,
			g_cclosure_new (G_CALLBACK (panel_applet_factory_callback),
					panel_applet_callback_data_new (applet_type, closure),
					(GClosureNotify) panel_applet_callback_data_free));

        return CORBA_Object_duplicate (BONOBO_OBJREF (factory), ev);
}

Bonobo_Unknown
panel_applet_shlib_factory (const char                 *iid,
			    GType                       applet_type,
			    PortableServer_POA          poa,
			    gpointer                    impl_ptr,
			    PanelAppletFactoryCallback  callback,
			    gpointer                    user_data,
			    CORBA_Environment          *ev)
{
	g_return_val_if_fail (iid != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (callback != NULL, CORBA_OBJECT_NIL);

	return panel_applet_shlib_factory_closure (
			iid, applet_type, poa, impl_ptr,
			g_cclosure_new (G_CALLBACK (callback),
					user_data, NULL),
			ev);
}

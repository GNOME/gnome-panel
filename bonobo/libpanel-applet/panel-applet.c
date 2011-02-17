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

#include <cairo.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-types.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-item-handler.h>
#include <bonobo/bonobo-property-bag-client.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <X11/Xatom.h>

#include "panel-applet.h"
#include "panel-applet-private.h"
#include "panel-applet-shell.h"
#include "panel-applet-marshal.h"
#include "panel-applet-enums.h"

#define PANEL_APPLET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_APPLET, PanelAppletPrivate))

struct _PanelAppletPrivate {
	PanelAppletShell           *shell;
	BonoboControl              *control;
	BonoboPropertyBag          *prop_sack;
	BonoboItemHandler          *item_handler;
	GConfClient                *client;

	char                       *iid;
	GClosure                   *closure;
	gboolean                    bound;
	char                       *prefs_key;

	PanelAppletFlags            flags;
	PanelAppletOrient           orient;
	guint                       size;
	char                       *background;
	GtkWidget                  *background_widget;

	int                         previous_width;
	int                         previous_height;

        int                        *size_hints;
        int                         size_hints_len;

	gboolean                    moving_focus_out;

	gboolean                    locked_down;
};

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
#define PROPERTY_LOCKED_DOWN "panel-applet-locked-down"

enum {
	PROPERTY_ORIENT_IDX,
	PROPERTY_SIZE_IDX,
	PROPERTY_BACKGROUND_IDX,
	PROPERTY_FLAGS_IDX,
	PROPERTY_SIZE_HINTS_IDX,
	PROPERTY_LOCKED_DOWN_IDX
};

G_DEFINE_TYPE (PanelApplet, panel_applet, GTK_TYPE_EVENT_BOX)

static void panel_applet_handle_background (PanelApplet *applet);
static void panel_applet_setup             (PanelApplet *applet);

static void
panel_applet_associate_schemas_in_dir (GConfClient  *client,
				       const gchar  *prefs_key,
				       const gchar  *schema_dir,
				       GError      **error)
{
	GSList *list, *l;

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

void
panel_applet_add_preferences (PanelApplet  *applet,
			      const gchar  *schema_dir,
			      GError      **opt_error)
{
	GError **error = NULL;
	GError  *our_error = NULL;

	g_return_if_fail (PANEL_IS_APPLET (applet));
	g_return_if_fail (schema_dir != NULL);

	if (!applet->priv->prefs_key)
		return;

	if (opt_error)
		error = opt_error;
	else
		error = &our_error;

	panel_applet_associate_schemas_in_dir (applet->priv->client,
					       applet->priv->prefs_key,
					       schema_dir,
					       error);

	if (!opt_error && our_error) {
		g_warning (G_STRLOC ": failed to add preferences from '%s' : '%s'",
			   schema_dir, our_error->message);
		g_error_free (our_error);
	}
}

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
	g_return_if_fail (PANEL_IS_APPLET (applet));

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

	if (applet->priv->prop_sack != NULL)
		bonobo_pbclient_set_short (BONOBO_OBJREF (applet->priv->prop_sack), PROPERTY_FLAGS, flags, NULL);
	else
		applet->priv->flags = flags;
}

void
panel_applet_set_size_hints (PanelApplet      *applet,
			     const int        *size_hints,
			     int               n_elements,
			     int               base_size)
{
	CORBA_sequence_CORBA_long *seq;
	CORBA_Environment          env;
	CORBA_any                  any;
	int                        i;

	CORBA_exception_init (&env);

	seq = CORBA_sequence_CORBA_long__alloc ();
	seq->_length = seq->_maximum = n_elements;
	seq->_release = CORBA_TRUE;
	seq->_buffer  = CORBA_sequence_CORBA_long_allocbuf (seq->_length);

	for (i = 0; i < n_elements; i++)
		seq->_buffer [i] = size_hints [i] + base_size;

	any._type    = TC_CORBA_sequence_CORBA_long;
	any._release = CORBA_FALSE;
	any._value   = seq;

	Bonobo_PropertyBag_setValue (BONOBO_OBJREF (applet->priv->prop_sack),
				     PROPERTY_SIZE_HINTS,
				     &any,
				     &env);

	CORBA_free (seq);

	CORBA_exception_free (&env);
}

guint
panel_applet_get_size (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), 0);

	return applet->priv->size;
}

PanelAppletOrient
panel_applet_get_orient (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), 0);

	return applet->priv->orient;
}

gboolean
panel_applet_get_locked_down (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), FALSE);

	return applet->priv->locked_down;
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
	xroot	 = GDK_WINDOW_XWINDOW (root);

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
		opt_app_name = app_name = g_strdup_printf ("%lu",
							   (unsigned long) getpid ());

	popup_component = panel_applet_get_popup_component (applet);

	bonobo_ui_util_set_ui (popup_component, opt_datadir, file, opt_app_name, NULL);

	bonobo_ui_component_add_verb_list_with_data (popup_component, verb_list, user_data);

	if (app_name)
		g_free (app_name);
}

BonoboControl *
panel_applet_get_control (PanelApplet *applet)
{
	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	return applet->priv->control;
}

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

	panel_applet_set_preferences_key (applet, NULL);

	if (applet->priv->client)
		g_object_unref (applet->priv->client);
	applet->priv->client = NULL;

	if (applet->priv->prop_sack)
		bonobo_object_unref (
			BONOBO_OBJECT (applet->priv->prop_sack));
	applet->priv->prop_sack = NULL;

	g_free (applet->priv->size_hints);
	g_free (applet->priv->prefs_key);
	g_free (applet->priv->background);
	g_free (applet->priv->iid);

	if (applet->priv->closure)
		g_closure_unref (applet->priv->closure);
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

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

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

	GTK_WIDGET_CLASS (panel_applet_parent_class)->size_request (widget,
								    requisition);

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
panel_applet_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
	GtkAllocation allocation;
	int border_width;
	int focus_width = 0;
	int x, y, width, height;

	g_return_val_if_fail (PANEL_IS_APPLET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	GTK_WIDGET_CLASS (panel_applet_parent_class)->expose_event (widget,
								    event);

        if (!gtk_widget_has_focus (widget))
		return FALSE;

	gtk_widget_get_allocation (widget, &allocation);

	/*
	 * We are deliberately ignoring focus-padding here to
	 * save valuable panel real estate.
	 */
	gtk_widget_style_get (widget,
			      "focus-line-width", &focus_width,
			      NULL);

	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

	x = allocation.x;
	y = allocation.y;

	width  = allocation.width  - 2 * border_width;
	height = allocation.height - 2 * border_width;

	gtk_paint_focus (gtk_widget_get_style (widget),
			 gtk_widget_get_window (widget),
			 gtk_widget_get_state (widget),
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
	gboolean         display_grabbed;
	GdkPixmap       *pixmap;
	GdkDisplay      *display;
	GdkPixmap       *retval;
	GdkWindow       *window;
	int              width;
	int              height;
	cairo_t         *cr;
	cairo_pattern_t *pattern;

	g_return_val_if_fail (PANEL_IS_APPLET (applet), NULL);

	if (!gtk_widget_get_realized (GTK_WIDGET (applet)))
		return NULL;

	display = gdk_display_get_default ();
	display_grabbed = FALSE;

	window = gtk_widget_get_window (GTK_WIDGET (applet));

	pixmap = gdk_pixmap_lookup_for_display (display, xid);
	if (pixmap)
		g_object_ref (pixmap);
	else {
		display_grabbed = TRUE;
		gdk_x11_display_grab (display);
		pixmap = gdk_pixmap_foreign_new_for_display (display, xid);
	}

	/* This can happen if the user changes the background very fast.
	 * We'll get the next update, so it's not a big deal. */
	if (pixmap == NULL) {
		if (display_grabbed)
			gdk_x11_display_ungrab (display);
		return NULL;
	}

	gdk_drawable_get_size (GDK_DRAWABLE (window),
			       &width, &height);
	retval = gdk_pixmap_new (window,
				 width, height, -1);

	/* the pixmap has no colormap, and we need one */
	gdk_drawable_set_colormap (GDK_DRAWABLE (pixmap),
				   gdk_drawable_get_colormap (window));

	cr = gdk_cairo_create (GDK_DRAWABLE (retval));
	gdk_cairo_set_source_pixmap (cr, pixmap, -x, -y);
	pattern = cairo_get_source (cr);
	cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	cairo_destroy (cr);

	g_object_unref (pixmap);

	if (display_grabbed)
		gdk_x11_display_ungrab (display);

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

	if (!gtk_widget_get_realized (GTK_WIDGET (applet)) || !applet->priv->background)
		return retval;

	elements = g_strsplit (applet->priv->background, ":", -1);

	if (elements [0] && !strcmp (elements [0], "none" )) {
		retval = PANEL_NO_BACKGROUND;

	} else if (elements [0] && !strcmp (elements [0], "color")) {
		g_return_val_if_fail (color != NULL, PANEL_NO_BACKGROUND);

		if (!elements [1] || !panel_applet_parse_color (elements [1], color)) {

			g_warning ("Incomplete '%s' background type received", elements [0]);
			g_strfreev (elements);
			return PANEL_NO_BACKGROUND;
		}

		retval = PANEL_COLOR_BACKGROUND;

	} else if (elements [0] && !strcmp (elements [0], "pixmap")) {
		GdkNativeWindow pixmap_id;
		int             x, y;

		g_return_val_if_fail (pixmap != NULL, PANEL_NO_BACKGROUND);

		if (!panel_applet_parse_pixmap_str (elements [1], &pixmap_id, &x, &y)) {
			g_warning ("Incomplete '%s' background type received: %s",
				   elements [0], elements [1]);

			g_strfreev (elements);
			return PANEL_NO_BACKGROUND;
		}

		*pixmap = panel_applet_get_pixmap (applet, pixmap_id, x, y);
		if (!*pixmap) {
			g_warning ("Failed to get pixmap %s", elements [1]);
			g_strfreev (elements);
			return PANEL_NO_BACKGROUND;
		}

		retval = PANEL_PIXMAP_BACKGROUND;
	} else
		g_warning ("Unknown background type received");

	g_strfreev (elements);

	return retval;
}

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
			seq->_buffer [i] = applet->priv->size_hints [i];
	}
		break;
	case PROPERTY_LOCKED_DOWN_IDX:
		BONOBO_ARG_SET_BOOLEAN (arg, applet->priv->locked_down);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
panel_applet_update_background_for_widget (GtkWidget                 *widget,
					   PanelAppletBackgroundType  type,
					   GdkColor                  *color,
					   GdkPixmap                 *pixmap)
{
	GtkRcStyle *rc_style;
	GtkStyle   *style;

	/* reset style */
	gtk_widget_set_style (widget, NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (widget, rc_style);
	g_object_unref (rc_style);

	switch (type) {
	case PANEL_NO_BACKGROUND:
		break;
	case PANEL_COLOR_BACKGROUND:
		gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, color);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		style = gtk_style_copy (gtk_widget_get_style (widget));
		if (style->bg_pixmap[GTK_STATE_NORMAL])
			g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);
		style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
		gtk_widget_set_style (widget, style);
		g_object_unref (style);
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
	GdkPixmap                 *pixmap;

	type = panel_applet_get_background (applet, &color, &pixmap);

	if (applet->priv->background_widget)
		panel_applet_update_background_for_widget (applet->priv->background_widget,
							   type, &color, pixmap);

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
	case PROPERTY_FLAGS_IDX:
		applet->priv->flags = BONOBO_ARG_GET_SHORT (arg);
		break;
	case PROPERTY_SIZE_HINTS_IDX: {
		CORBA_sequence_CORBA_long *seq = arg->_value;
		int                        i;

		applet->priv->size_hints = g_realloc (applet->priv->size_hints,
						      seq->_length * sizeof (int));
		for (i = 0; i < seq->_length; i++)
			applet->priv->size_hints [i] = seq->_buffer [i];

		applet->priv->size_hints_len = seq->_length;;
	}
		break;
	case PROPERTY_LOCKED_DOWN_IDX:
		applet->priv->locked_down = BONOBO_ARG_GET_BOOLEAN (arg);
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
				 "The Applet's containing Panel's orientation",
				 Bonobo_PROPERTY_READABLE | Bonobo_PROPERTY_WRITEABLE);

	bonobo_property_bag_add (sack,
				 PROPERTY_SIZE,
				 PROPERTY_SIZE_IDX,
				 BONOBO_ARG_SHORT,
				 NULL,
				 "The Applet's containing Panel's size in pixels",
				 Bonobo_PROPERTY_READABLE | Bonobo_PROPERTY_WRITEABLE);

	bonobo_property_bag_add (sack,
				 PROPERTY_BACKGROUND,
				 PROPERTY_BACKGROUND_IDX,
				 BONOBO_ARG_STRING,
				 NULL,
				 "The Applet's containing Panel's background color or pixmap",
				 Bonobo_PROPERTY_READABLE | Bonobo_PROPERTY_WRITEABLE);

	bonobo_property_bag_add (sack,
				 PROPERTY_FLAGS,
				 PROPERTY_FLAGS_IDX,
				 BONOBO_ARG_SHORT,
				 NULL,
				 "The Applet's flags",
				 Bonobo_PROPERTY_READABLE);

	bonobo_property_bag_add (sack,
				 PROPERTY_SIZE_HINTS,
				 PROPERTY_SIZE_HINTS_IDX,
				 TC_CORBA_sequence_CORBA_long,
				 NULL,
				 "Ranges that hint what sizes are acceptable for the applet",
				 Bonobo_PROPERTY_READABLE);

	bonobo_property_bag_add (sack,
				 PROPERTY_LOCKED_DOWN,
				 PROPERTY_LOCKED_DOWN_IDX,
				 BONOBO_ARG_BOOLEAN,
				 NULL,
				 "The Applet's containing Panel is locked down",
				 Bonobo_PROPERTY_READABLE | Bonobo_PROPERTY_WRITEABLE);

	return sack;
}

static void
panel_applet_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (panel_applet_parent_class)->realize (widget);

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
			       NULL);


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
			panel_applet_set_preferences_key (applet, option->value);

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
		} else if (!strcmp (option->key, "locked_down")) {
			gboolean val = FALSE;
			if (option->value[0] == 'T' ||
			    option->value[0] == 't' ||
			    option->value[0] == 'Y' ||
			    option->value[0] == 'y' ||
			    atoi (option->value) != 0)
				val = TRUE;
			bonobo_pbclient_set_boolean (BONOBO_OBJREF (applet->priv->prop_sack),
						     PROPERTY_LOCKED_DOWN, val, NULL);
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

static GObject *
panel_applet_constructor (GType                  type,
			  guint                  n_construct_properties,
			  GObjectConstructParam *construct_properties)
{
	GObject     *obj;
	PanelApplet *applet;

	obj = G_OBJECT_CLASS (panel_applet_parent_class)->constructor (type,
								       n_construct_properties,
								       construct_properties);

	applet = PANEL_APPLET (obj);

	panel_applet_setup (applet);

	return obj;
}

static void
panel_applet_class_init (PanelAppletClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
	GtkBindingSet *binding_set;

	gobject_class->constructor = panel_applet_constructor;
	klass->move_focus_out_of_applet = panel_applet_move_focus_out_of_applet;

	widget_class->button_press_event = panel_applet_button_press;
	widget_class->size_request = panel_applet_size_request;
	widget_class->size_allocate = panel_applet_size_allocate;
	widget_class->expose_event = panel_applet_expose;
	widget_class->focus = panel_applet_focus;
	widget_class->realize = panel_applet_realize;

	gobject_class->finalize = panel_applet_finalize;

	g_type_class_add_private (klass, sizeof (PanelAppletPrivate));

	panel_applet_signals [CHANGE_ORIENT] =
                g_signal_new ("change_orient",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelAppletClass, change_orient),
                              NULL,
			      NULL,
                              panel_applet_marshal_VOID__UINT,
                              G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

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
                              panel_applet_marshal_VOID__ENUM_BOXED_OBJECT,
                              G_TYPE_NONE,
			      3,
			      PANEL_TYPE_PANEL_APPLET_BACKGROUND_TYPE,
			      GDK_TYPE_COLOR,
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
panel_applet_init (PanelApplet *applet)
{
	applet->priv = PANEL_APPLET_GET_PRIVATE (applet);

	applet->priv->client = gconf_client_get_default ();

	applet->priv->bound  = FALSE;
	applet->priv->flags  = PANEL_APPLET_FLAGS_NONE;
	applet->priv->orient = PANEL_APPLET_ORIENT_UP;
	applet->priv->size   = GNOME_Vertigo_PANEL_MEDIUM;

	applet->priv->moving_focus_out = FALSE;

	gtk_widget_set_events (GTK_WIDGET (applet),
			       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
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
}

GtkWidget *
panel_applet_new (void)
{
	PanelApplet *applet;

	applet = g_object_new (PANEL_TYPE_APPLET, NULL);

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
		iid, DisplayString (gdk_display_get_default ()));
	retval = bonobo_generic_factory_main (
		display_iid,
		(BonoboFactoryCallback) panel_applet_factory_callback,
		data);
	g_free (display_iid);

	panel_applet_callback_data_free (data);

	return retval;
}

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

void
panel_applet_set_background_widget (PanelApplet *applet,
				    GtkWidget   *widget)
{
	applet->priv->background_widget = widget;

	if (widget) {
		PanelAppletBackgroundType  type;
		GdkColor                   color;
		GdkPixmap                 *pixmap;

		type = panel_applet_get_background (applet, &color, &pixmap);
		panel_applet_update_background_for_widget (widget, type,
							   &color, pixmap);
		if (type == PANEL_PIXMAP_BACKGROUND)
			g_object_unref (pixmap);
	}
}

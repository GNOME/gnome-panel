/*
 * panel-menu-button.c: panel menu button
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-menu-button.h"

#include <string.h>
#include <glib/gi18n.h>

#include <gmenu-tree.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-launch.h>
#include <libpanel-util/panel-show.h>

#include "applet.h"
#include "panel-widget.h"
#include "panel-util.h"
#include "panel-globals.h"
#include "menu.h"
#include "panel-lockdown.h"
#include "panel-a11y.h"
#include "panel-layout.h"
#include "panel-icon-names.h"
#include "panel-schemas.h"

G_DEFINE_TYPE (PanelMenuButton, panel_menu_button, BUTTON_TYPE_WIDGET)

#define PANEL_MENU_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_MENU_BUTTON, PanelMenuButtonPrivate))

enum {
	PROP_0,
	PROP_MENU_PATH,
	PROP_CUSTOM_ICON,
	PROP_TOOLTIP,
	PROP_DND_ENABLED
};

typedef enum {
	FIRST_MENU,
	APPLICATIONS_MENU,
#define DEFAULT_MENU      APPLICATIONS_MENU
	GNOMECC_MENU,
	LAST_MENU
} MenuPathRoot;

typedef struct {
	MenuPathRoot  root_id;
	char         *scheme;
	char         *filename;
} MenuPathRootItem;

static MenuPathRootItem root_items [] = {
	{ APPLICATIONS_MENU, "applications", "applications.menu" },
	{ GNOMECC_MENU,      "gnomecc",      "gnomecc.menu"      }
};

struct _PanelMenuButtonPrivate {
	PanelToplevel         *toplevel;
	GSettings             *settings_instance;

	GtkWidget             *menu;

	char                  *menu_path;
	char                  *custom_icon;
	char                  *tooltip;

	MenuPathRoot           path_root;
	guint                  dnd_enabled : 1;
};

static void panel_menu_button_set_icon              (PanelMenuButton *button);

static AtkObject *panel_menu_button_get_accessible  (GtkWidget       *widget);

static const char *
panel_menu_path_root_to_filename (MenuPathRoot path_root)
{
	const char *retval;
	int         i;

	retval = NULL;

	for (i = 0; i < G_N_ELEMENTS (root_items); i++) {
		if (root_items [i].root_id == path_root) {
			retval = root_items [i].filename;
			break;
		}
	}

	return retval;
}

static const char *
panel_menu_filename_to_scheme (const char *filename)
{
	const char *retval;
	int         i;

	retval = NULL;
	
	if (!filename)
		return retval;

	for (i = 0; i < G_N_ELEMENTS (root_items); i++) {
		if (root_items [i].filename &&
		    !strncmp (filename, root_items [i].filename,
			      strlen (root_items [i].filename))) {
			retval = root_items [i].scheme;
			break;
		}
	}

	return retval;
}

static MenuPathRoot
panel_menu_scheme_to_path_root (const char *scheme)
{
	MenuPathRoot retval;
	int          i;

	retval = LAST_MENU;
	
	if (!scheme)
		return retval;

	for (i = 0; i < G_N_ELEMENTS (root_items); i++) {
		if (root_items [i].scheme &&
		    !strncmp (scheme, root_items [i].scheme,
			      strlen (root_items [i].scheme))) {
			retval = root_items [i].root_id;
			break;
		}
	}

	return retval;
}

static void
panel_menu_button_init (PanelMenuButton *button)
{
	button->priv = PANEL_MENU_BUTTON_GET_PRIVATE (button);

	button->priv->toplevel          = NULL;
	button->priv->settings_instance = NULL;

	button->priv->menu_path   = NULL;
	button->priv->custom_icon = NULL;
	button->priv->tooltip = NULL;

	button->priv->path_root       = LAST_MENU;
}

static void
panel_menu_button_finalize (GObject *object)
{
	PanelMenuButton *button = PANEL_MENU_BUTTON (object);

	if (button->priv->menu) {
		/* detaching the menu will kill our reference */
		gtk_menu_detach (GTK_MENU (button->priv->menu));
		button->priv->menu = NULL;
	}

	if (button->priv->settings_instance)
		g_object_unref (button->priv->settings_instance);
	button->priv->settings_instance = NULL;

	g_free (button->priv->menu_path);
	button->priv->menu_path = NULL;

	g_free (button->priv->custom_icon);
	button->priv->custom_icon = NULL;

	g_free (button->priv->tooltip);
	button->priv->tooltip = NULL;

	G_OBJECT_CLASS (panel_menu_button_parent_class)->finalize (object);
}

static void
panel_menu_button_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	PanelMenuButton *button;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (object));

	button = PANEL_MENU_BUTTON (object);

	switch (prop_id) {
	case PROP_MENU_PATH:
		g_value_set_string (value, button->priv->menu_path);
		break;
	case PROP_CUSTOM_ICON:
		g_value_set_string (value, button->priv->custom_icon);
		break;
	case PROP_TOOLTIP:
		g_value_set_string (value, button->priv->tooltip);
		break;
	case PROP_DND_ENABLED:
		g_value_set_boolean (value, button->priv->dnd_enabled);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_menu_button_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	PanelMenuButton *button;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (object));

	button = PANEL_MENU_BUTTON (object);

	switch (prop_id) {
	case PROP_MENU_PATH:
                panel_menu_button_set_menu_path (button, g_value_get_string (value));
                break;
	case PROP_CUSTOM_ICON:
                panel_menu_button_set_custom_icon (button, g_value_get_string (value));
                break;
	case PROP_TOOLTIP:
		panel_menu_button_set_tooltip (button, g_value_get_string (value));
		break;
	case PROP_DND_ENABLED:
		panel_menu_button_set_dnd_enabled (button, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
panel_menu_button_associate_panel (PanelMenuButton *button)
{
	PanelWidget *panel_widget = NULL;

	if (!button->priv->menu)
		return;

	if (button->priv->toplevel)
		panel_widget = panel_toplevel_get_panel_widget (button->priv->toplevel);

	panel_applet_menu_set_recurse (GTK_MENU (button->priv->menu), "menu_panel", panel_widget);
}

static void
panel_menu_button_parent_set (GtkWidget *widget,
			      GtkWidget *previous_parent)
{
	PanelMenuButton *button = PANEL_MENU_BUTTON (widget);
	GtkWidget       *parent;

	parent = gtk_widget_get_parent (widget);
	g_return_if_fail (!parent || PANEL_IS_WIDGET (parent));

	if (parent)
		button->priv->toplevel = PANEL_WIDGET (parent)->toplevel;
	else
		button->priv->toplevel = NULL;

	panel_menu_button_associate_panel (button);
	panel_menu_button_set_icon (button);

	if (GTK_WIDGET_CLASS (panel_menu_button_parent_class)->parent_set)
		GTK_WIDGET_CLASS (panel_menu_button_parent_class)->parent_set (widget, previous_parent);
}

static void
panel_menu_button_drag_data_get (GtkWidget        *widget,
				 GdkDragContext   *context,
				 GtkSelectionData *selection_data,
				 guint             info,
				 guint             time)
{
	char            *drag_data;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (widget));

	drag_data = g_strdup_printf ("MENU:%d", panel_find_applet_index (widget));

	gtk_selection_data_set (
		selection_data, gtk_selection_data_get_target (selection_data),
		8, (guchar *) drag_data, strlen (drag_data));

	g_free (drag_data);
}

static void
panel_menu_button_menu_deactivated (PanelMenuButton *button)
{
	panel_toplevel_pop_autohide_disabler (button->priv->toplevel);

	gtk_widget_unset_state_flags (GTK_WIDGET (button),
				      GTK_STATE_FLAG_PRELIGHT);

	button_widget_set_ignore_leave (BUTTON_WIDGET (button), FALSE);
}

static void 
panel_menu_button_menu_detacher	(PanelMenuButton *button)
{
	/*
	 * just in case someone still owns a reference to the
	 * menu (the menu may be up or some such other nonsense)
	 */
	g_signal_handlers_disconnect_by_func (button->priv->menu,
					      G_CALLBACK (panel_menu_button_menu_deactivated),
					      button);

	button->priv->menu = NULL;
}

static GtkWidget *
panel_menu_button_create_menu (PanelMenuButton *button)
{
	PanelWidget *panel_widget;

	if (button->priv->menu)
		return button->priv->menu;

	if (!button->priv->toplevel)
		return NULL;

	panel_widget = panel_toplevel_get_panel_widget (button->priv->toplevel);

	if (!PANEL_GLIB_STR_EMPTY (button->priv->menu_path) &&
	    button->priv->path_root > FIRST_MENU            &&
	    button->priv->path_root < LAST_MENU) {
		const char *filename;

		filename = panel_menu_path_root_to_filename (button->priv->path_root);
		button->priv->menu = create_applications_menu (filename,
							       button->priv->menu_path,
							       TRUE);
	} else
		button->priv->menu = create_main_menu (panel_widget);

	gtk_menu_attach_to_widget (GTK_MENU (button->priv->menu),
				   GTK_WIDGET (button),
				   (GtkMenuDetachFunc) panel_menu_button_menu_detacher);

	panel_menu_button_associate_panel (button);

	g_signal_connect_swapped (button->priv->menu, "deactivate",
				  G_CALLBACK (panel_menu_button_menu_deactivated),
				  button);

	return button->priv->menu;
}

void
panel_menu_button_popup_menu (PanelMenuButton *button,
			      guint            n_button,
			      guint32          activate_time)
{
	GdkScreen *screen;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	panel_menu_button_create_menu (button);

	panel_toplevel_push_autohide_disabler (button->priv->toplevel);

	button_widget_set_ignore_leave (BUTTON_WIDGET (button), TRUE);

	screen = gtk_window_get_screen (GTK_WINDOW (button->priv->toplevel));
	gtk_menu_set_screen (GTK_MENU (button->priv->menu), screen);

	gtk_menu_popup (GTK_MENU (button->priv->menu),
			NULL,
			NULL,
			(GtkMenuPositionFunc) panel_applet_position_menu,
			GTK_WIDGET (button),
			n_button,
			activate_time);
}

static void
panel_menu_button_pressed (GtkButton *gtk_button)
{
	PanelMenuButton *button;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (gtk_button));

	button = PANEL_MENU_BUTTON (gtk_button);

	if (GTK_BUTTON_CLASS (panel_menu_button_parent_class)->pressed)
		GTK_BUTTON_CLASS (panel_menu_button_parent_class)->pressed (gtk_button);

	panel_menu_button_popup_menu (button, 0, gtk_get_current_event_time());
}

static void
panel_menu_button_clicked (GtkButton *gtk_button)
{
	PanelMenuButton *button;
	GdkEvent        *event;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (gtk_button));

	button = PANEL_MENU_BUTTON (gtk_button);

	if (GTK_BUTTON_CLASS (panel_menu_button_parent_class)->clicked)
		GTK_BUTTON_CLASS (panel_menu_button_parent_class)->clicked (gtk_button);

	if ((event = gtk_get_current_event ())) {
		panel_menu_button_popup_menu (button,
					      event->button.button,
					      event->button.time);
		gdk_event_free (event);
	} else {
		panel_menu_button_popup_menu (button, 1, GDK_CURRENT_TIME);
	}
}

static void
panel_menu_button_class_init (PanelMenuButtonClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass   *) klass;
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;
	GtkButtonClass *button_class  = (GtkButtonClass *) klass;

	gobject_class->finalize     = panel_menu_button_finalize;
	gobject_class->get_property = panel_menu_button_get_property;
	gobject_class->set_property = panel_menu_button_set_property;

	widget_class->parent_set     = panel_menu_button_parent_set;
        widget_class->drag_data_get  = panel_menu_button_drag_data_get;
        widget_class->get_accessible = panel_menu_button_get_accessible;

	button_class->clicked = panel_menu_button_clicked;
	button_class->pressed = panel_menu_button_pressed;

	g_type_class_add_private (klass, sizeof (PanelMenuButtonPrivate));

	g_object_class_install_property (
			gobject_class,
			PROP_MENU_PATH,
                        g_param_spec_string ("menu-path",
					     "Menu Path",
					     "The path from which to construct the menu",
					     NULL,
					     G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_CUSTOM_ICON,
                        g_param_spec_string ("custom-icon",
					     "Custom Icon",
					     "The custom icon for the menu",
					     NULL,
					     G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_TOOLTIP,
			g_param_spec_string ("tooltip",
					     "Tooltip",
					     "Tooltip displayed for the menu",
					     NULL,
					     G_PARAM_READWRITE));

	g_object_class_install_property (
			gobject_class,
			PROP_DND_ENABLED,
                        g_param_spec_boolean ("dnd-enabled",
					      "Drag and drop enabled",
					      "Whether or not drag and drop is enabled on the widget",
					      FALSE,
					      G_PARAM_READWRITE));
}

static void
panel_menu_button_settings_changed (GSettings       *settings,
				    char            *key,
				    PanelMenuButton *button)
{
	char *value = NULL;

	if (g_strcmp0 (key, PANEL_MENU_BUTTON_MENU_PATH_KEY) == 0) {
		value = g_settings_get_string (settings, key);
		panel_menu_button_set_menu_path (button, value);
	} else if (g_strcmp0 (key, PANEL_MENU_BUTTON_CUSTOM_ICON_KEY) == 0) {
		value = g_settings_get_string (settings, key);
		panel_menu_button_set_custom_icon (button, value);
	} else if (g_strcmp0 (key, PANEL_MENU_BUTTON_TOOLTIP_KEY) == 0) {
		value = g_settings_get_string (settings, key);
		panel_menu_button_set_tooltip (button, value);
	}

	g_free (value);
}

static void
panel_menu_button_load_helper (const char  *menu_path,
			       const char  *custom_icon,
			       const char  *tooltip,
			       PanelWidget *panel,
			       const char  *id,
			       GSettings   *settings)
{
	PanelMenuButton *button;
	AppletInfo      *info;

	g_return_if_fail (panel != NULL);

	button = g_object_new (PANEL_TYPE_MENU_BUTTON,
			       "menu-path", menu_path,
			       "custom-icon", custom_icon,
			       "tooltip", tooltip,
			       "has-arrow", TRUE,
			       NULL);

	info = panel_applet_register (GTK_WIDGET (button),
				      panel,
				      PANEL_OBJECT_MENU, id,
				      settings,
				      NULL, NULL);
	if (!info) {
		gtk_widget_destroy (GTK_WIDGET (button));
		return;
	}

        if (panel_is_program_in_path ("alacarte") ||
	    panel_is_program_in_path ("gmenu-simple-editor"))
		panel_applet_add_callback (info, "edit", NULL,
					   _("_Edit Menus"),
					   panel_lockdown_get_not_panels_locked_down_s);

	panel_widget_set_applet_expandable (panel, GTK_WIDGET (button), FALSE, TRUE);
	panel_widget_set_applet_size_constrained (panel, GTK_WIDGET (button), TRUE);

	button->priv->settings_instance = panel_layout_get_instance_settings (
							settings,
							PANEL_MENU_BUTTON_SCHEMA);

	g_signal_connect (button->priv->settings_instance, "changed",
			  G_CALLBACK (panel_menu_button_settings_changed),
			  button);
}

static GIcon *
panel_menu_button_get_icon (PanelMenuButton *button)
{
	GMenuTreeDirectory *directory;
        GIcon              *retval;

	retval = NULL;

	if (!PANEL_GLIB_STR_EMPTY (button->priv->custom_icon))
		retval = g_themed_icon_new (button->priv->custom_icon);

	if (!retval                                         &&
	    !PANEL_GLIB_STR_EMPTY (button->priv->menu_path) &&
	    panel_menu_button_create_menu (button)) {
		directory = g_object_get_data (G_OBJECT (button->priv->menu),
					       "panel-menu-tree-directory");

		if (!directory) {
			GMenuTree *tree;

			if ((tree = g_object_get_data (G_OBJECT (button->priv->menu),
						       "panel-menu-tree"))) {
				directory = gmenu_tree_get_directory_from_path (tree,
										button->priv->menu_path);
				g_object_set_data_full (G_OBJECT (button->priv->menu),
							"panel-menu-tree-directory",
							directory,
							(GDestroyNotify) gmenu_tree_item_unref);
			}
		}

		if (directory)
			retval = g_object_ref (gmenu_tree_directory_get_icon (directory));
	}

	if (!retval)
		retval = g_themed_icon_new (PANEL_ICON_MAIN_MENU);

	return retval;
}

static void
panel_menu_button_set_icon (PanelMenuButton *button)
{
	GIcon *gicon;
	char  *icon;

	gicon = panel_menu_button_get_icon (button);
	icon = panel_util_get_icon_name_from_g_icon (gicon);

	button_widget_set_icon_name (BUTTON_WIDGET (button), icon);

	g_free (icon);
	g_object_unref (gicon);
}

static const char *
split_menu_uri (const char  *menu_uri,
		char       **menu_scheme)
{
	char *p;

	if (!menu_uri)
		return NULL;

	p = strchr (menu_uri, ':');

	if (!p || p == menu_uri)
		return NULL;

	if (menu_scheme)
		*menu_scheme = g_strndup (menu_uri, p - menu_uri);

	if (*(++p) != '/')
		return NULL;

	while (*p != '\0' && *(p + 1) == '/') p++;

	return p;
}

gboolean
panel_menu_button_is_main_menu (PanelMenuButton *button)
{
	g_return_val_if_fail (PANEL_IS_MENU_BUTTON (button), FALSE);

	return PANEL_GLIB_STR_EMPTY (button->priv->menu_path);
}

void
panel_menu_button_set_menu_path (PanelMenuButton *button,
				 const char      *menu_uri)
{
	const char *menu_path;
	char       *scheme;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	scheme    = NULL;
	menu_path = split_menu_uri (menu_uri, &scheme);

	if (!scheme)
		return;

	button->priv->path_root = panel_menu_scheme_to_path_root (scheme);
	g_free (scheme);

	if (!button->priv->menu_path && (!menu_path || !menu_path [0]))
		return;

	if (button->priv->menu_path && menu_path &&
	    !strcmp (button->priv->menu_path, menu_path))
		return;

	g_free (button->priv->menu_path);
	button->priv->menu_path = NULL;

	button->priv->menu_path = g_strdup (menu_path);

	if (button->priv->menu)
		gtk_menu_detach (GTK_MENU (button->priv->menu));
	button->priv->menu = NULL;

	panel_menu_button_set_icon (button);
}

void
panel_menu_button_set_custom_icon (PanelMenuButton *button,
				   const char      *custom_icon)
{
	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	g_free (button->priv->custom_icon);
	button->priv->custom_icon = NULL;

	if (custom_icon && custom_icon [0])
		button->priv->custom_icon = g_strdup (custom_icon);

	panel_menu_button_set_icon (button);
}

void
panel_menu_button_set_tooltip (PanelMenuButton *button,
			       const char      *tooltip)
{
	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	g_free (button->priv->tooltip);
	button->priv->tooltip = g_strdup (tooltip);

	panel_util_set_tooltip_text (GTK_WIDGET (button), tooltip);
}

void
panel_menu_button_load (PanelWidget *panel,
			const char  *id,
			GSettings   *settings)
{
	GSettings    *settings_instance;
	char         *scheme;
	MenuPathRoot  root;
	char         *menu_path;
	char         *custom_icon;
	char         *tooltip;

	settings_instance = panel_layout_get_instance_settings (settings,
								PANEL_MENU_BUTTON_SCHEMA);

	menu_path = g_settings_get_string (settings_instance,
					   PANEL_MENU_BUTTON_MENU_PATH_KEY);
	custom_icon = g_settings_get_string (settings_instance,
					     PANEL_MENU_BUTTON_CUSTOM_ICON_KEY);
	tooltip = g_settings_get_string (settings_instance,
					 PANEL_MENU_BUTTON_TOOLTIP_KEY);

	if (!PANEL_GLIB_STR_EMPTY (menu_path)) {
		scheme = g_strndup (menu_path, strcspn (menu_path, ":"));
		root = panel_menu_scheme_to_path_root (scheme);
		g_free (scheme);
	} else
		root = APPLICATIONS_MENU;

	if (root == LAST_MENU) {
		g_printerr ("Unknown menu scheme, cannot load menu button\n");

		g_free (menu_path);
		g_free (custom_icon);
		g_free (tooltip);
		g_object_unref (settings_instance);

		return;
	}

	panel_menu_button_load_helper (menu_path,
				       custom_icon,
				       tooltip,
				       panel,
				       id,
				       settings);

	g_free (menu_path);
	g_free (custom_icon);
	g_free (tooltip);

	g_object_unref (settings_instance);
}

gboolean
panel_menu_button_create (PanelToplevel       *toplevel,
			  PanelObjectPackType  pack_type,
			  int                  pack_index,
			  const char          *filename,
			  const char          *menu_path,
			  const char          *tooltip)
{
	char       *id;
	GSettings  *settings;
	GSettings  *settings_instance;
	const char *scheme;

	scheme = panel_menu_filename_to_scheme (filename);

	if (filename && !scheme) {
		g_warning ("Failed to find menu scheme for %s\n", filename);
		return FALSE;
	}

	id = panel_layout_object_create_start (PANEL_OBJECT_MENU, NULL,
					       panel_toplevel_get_id (toplevel),
					       pack_type, pack_index,
					       &settings);

	settings_instance = panel_layout_get_instance_settings (settings,
								PANEL_MENU_BUTTON_SCHEMA);

	if (!PANEL_GLIB_STR_EMPTY (menu_path) && scheme) {
		char *menu_uri;

		menu_uri = g_strconcat (scheme, ":", menu_path, NULL);

		g_settings_set_string (settings_instance,
				       PANEL_MENU_BUTTON_MENU_PATH_KEY,
				       menu_uri);

		g_free (menu_uri);
	}

	if (!PANEL_GLIB_STR_EMPTY (tooltip)) {
		g_settings_set_string (settings_instance,
				       PANEL_MENU_BUTTON_TOOLTIP_KEY,
				       tooltip);
	}

	panel_layout_object_create_finish (id);

	g_object_unref (settings_instance);
	g_object_unref (settings);
	g_free (id);

	return TRUE;
}
			  
void
panel_menu_button_invoke_menu (PanelMenuButton *button,
			       const char   *callback_name)
{
	GdkScreen *screen;

	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));
	g_return_if_fail (callback_name != NULL);

	screen = gtk_widget_get_screen (GTK_WIDGET (button));

	if (!strcmp (callback_name, "help")) {
		panel_show_help (screen, "user-guide", "gospanel-37", NULL);

	} else if (!strcmp (callback_name, "edit")) {
                GError *error = NULL;

		panel_launch_desktop_file_with_fallback ("alacarte.desktop",
							 "alacarte",
							 screen, &error);
		if (error) {
			g_error_free (error);
			panel_launch_desktop_file_with_fallback (
						"gmenu-simple-editor.desktop",
						"gmenu-simple-editor",
						screen, NULL);
		}
	}
}

void
panel_menu_button_set_dnd_enabled (PanelMenuButton *button,
				   gboolean         dnd_enabled)
{
	g_return_if_fail (PANEL_IS_MENU_BUTTON (button));

	dnd_enabled = dnd_enabled != FALSE;

	if (button->priv->dnd_enabled == dnd_enabled)
		return;

	if (dnd_enabled) {
		static GtkTargetEntry dnd_targets [] = {
			{ "application/x-panel-applet-internal", 0, 0 }
		};
		GIcon *icon;

		gtk_widget_set_has_window (GTK_WIDGET (button), TRUE);
		gtk_drag_source_set (GTK_WIDGET (button), GDK_BUTTON1_MASK,
				     dnd_targets, 1,
				     GDK_ACTION_COPY | GDK_ACTION_MOVE);

		icon = panel_menu_button_get_icon (button);
		if (icon != NULL) {
			gtk_drag_source_set_icon_gicon (GTK_WIDGET (button),
							icon);
			g_object_unref (icon);
		}

		gtk_widget_set_has_window (GTK_WIDGET (button), FALSE);
	} else
		gtk_drag_source_unset (GTK_WIDGET (button));
}

/*
 * An AtkObject implementation for PanelMenuButton.
 * We need all this just so we can create the menu in ref_child()
 *
 * See http://bugzilla.gnome.org/show_bug.cgi?id=138535 for details
 *
 * If we ever remove the on-demand creation of the menu, we should
 * can just remove all this again
 */

#define PANEL_IS_MENU_BUTTON_ACCESSIBLE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), panel_menu_button_accessible_get_type ()))

static GType panel_menu_button_accessible_get_type (void);
static gpointer parent_accessible_class = NULL;

static int
panel_menu_button_accessible_get_n_children (AtkObject *obj)
{
	g_return_val_if_fail (PANEL_IS_MENU_BUTTON_ACCESSIBLE (obj), 0);

#if GTK_CHECK_VERSION (2, 21, 0)
	return gtk_accessible_get_widget (GTK_ACCESSIBLE (obj)) ? 1 : 0;
#else
	return GTK_ACCESSIBLE (obj)->widget ? 1 : 0;
#endif
}

static AtkObject *
panel_menu_button_accessible_ref_child (AtkObject *obj,
					int        index)
{
	PanelMenuButton *button;
	GtkWidget       *menu;

	g_return_val_if_fail (PANEL_IS_MENU_BUTTON_ACCESSIBLE (obj), NULL);

	if (index != 0)
		return NULL;

#if GTK_CHECK_VERSION (2, 21, 0)
	if (!(button = PANEL_MENU_BUTTON (gtk_accessible_get_widget (GTK_ACCESSIBLE (obj)))))
#else
	if (!(button = PANEL_MENU_BUTTON (GTK_ACCESSIBLE (obj)->widget)))
#endif
		return NULL;

	if (!(menu = panel_menu_button_create_menu (button)))
		return NULL;
	/*
	 * This ensures that the menu is populated with all menu items
	 */
	g_signal_emit_by_name (menu, "show", NULL);

	return g_object_ref (gtk_widget_get_accessible (menu));
}

static const gchar *
panel_menu_button_accessible_get_name (AtkObject *obj)
{
	const char *name;

	name = ATK_OBJECT_CLASS (parent_accessible_class)->get_name(obj);
	if (name == NULL)
		name = _("Main Menu");

	return name;
}

static void
panel_menu_button_accessible_class_init (AtkObjectClass *klass)
{
	klass->get_n_children = panel_menu_button_accessible_get_n_children;
	klass->ref_child      = panel_menu_button_accessible_ref_child; 
	klass->get_name       = panel_menu_button_accessible_get_name; 

	parent_accessible_class = g_type_class_peek_parent (klass);
}

static GType
panel_menu_button_accessible_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo type_info = { 0 };
		GType     accessible_parent_type;

		type_info.class_init =
			(GClassInitFunc) panel_menu_button_accessible_class_init;

		accessible_parent_type =
			panel_a11y_query_accessible_parent_type (PANEL_TYPE_MENU_BUTTON,
								 &type_info);
 
		type = g_type_register_static (accessible_parent_type, 
					       "PanelMenuButtonAccessible", 
					       &type_info, 0);
	}

	return type;
}

static AtkObject *
panel_menu_button_accessible_new (GObject *obj)
{
	AtkObject *accessible;

	g_return_val_if_fail (PANEL_IS_MENU_BUTTON (obj), NULL);

	accessible = g_object_new (panel_menu_button_accessible_get_type (), NULL);
	atk_object_initialize (accessible, obj);

	return accessible;
}

static void
panel_menu_button_accessible_factory_class_init (AtkObjectFactoryClass *klass)
{
	klass->create_accessible   = panel_menu_button_accessible_new;
	klass->get_accessible_type = panel_menu_button_accessible_get_type;
}

static GType
panel_menu_button_accessible_factory_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (AtkObjectFactoryClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_menu_button_accessible_factory_class_init,
			NULL,
			NULL,
			sizeof (AtkObjectFactory),
			0,
			NULL,
			NULL
		};

		type = g_type_register_static (ATK_TYPE_OBJECT_FACTORY, 
					       "PanelMenuButtonAccessibleFactory",
					       &info, 0);
	}

	return type;
}

static AtkObject *
panel_menu_button_get_accessible (GtkWidget *widget)
{
	static gboolean first_time = TRUE;

	g_return_val_if_fail (widget != NULL, NULL);

	if (first_time && panel_a11y_get_is_a11y_enabled (widget))
		atk_registry_set_factory_type (atk_get_default_registry (),
					       PANEL_TYPE_MENU_BUTTON,
					       panel_menu_button_accessible_factory_get_type ());

	first_time = FALSE;
	 
	return GTK_WIDGET_CLASS (panel_menu_button_parent_class)->get_accessible (widget);
}

/*
 * Copyright (C) 2001 Red Hat, Inc
 * Copyright (C) 2001 Alexander Larsson
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alexander Larsson
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>
#include <string.h>

#include "wncklet.h"
#include "window-list.h"

#define WINDOW_LIST_ICON "gnome-panel-window-list"

struct _WindowListApplet
{
	GpApplet parent;

	WnckHandle *handle;
	GtkWidget *tasklist;

	gboolean include_all_workspaces;
	WnckTasklistGroupingType grouping;
	gboolean move_unminimized_windows;

	GtkOrientation orientation;

	GtkIconTheme *icon_theme;

	/* Properties: */
	GtkWidget *properties_dialog;
	GtkWidget *show_current_radio;
	GtkWidget *show_all_radio;
	GtkWidget *never_group_radio;
	GtkWidget *auto_group_radio;
	GtkWidget *always_group_radio;
	GtkWidget *minimized_windows_label;
	GtkWidget *move_minimized_radio;
	GtkWidget *change_workspace_radio;

	GSettings *settings;
};

G_DEFINE_TYPE (WindowListApplet, window_list_applet, GP_TYPE_APPLET)

static void
tasklist_update (WindowListApplet *tasklist)
{
	wnck_tasklist_set_grouping (WNCK_TASKLIST (tasklist->tasklist),
				    tasklist->grouping);
	wnck_tasklist_set_include_all_workspaces (WNCK_TASKLIST (tasklist->tasklist),
						  tasklist->include_all_workspaces);
	wnck_tasklist_set_switch_workspace_on_unminimize (WNCK_TASKLIST (tasklist->tasklist),
							  tasklist->move_unminimized_windows);
}

static void
response_cb (GtkWidget        *widget,
             gint              id,
             WindowListApplet *tasklist)
{
	gtk_widget_hide (widget);
}

static void
destroy_tasklist (GtkWidget        *widget,
                  WindowListApplet *tasklist)
{
	g_object_unref (tasklist->settings);
	tasklist->settings = NULL;

	if (tasklist->properties_dialog)
		gtk_widget_destroy (tasklist->properties_dialog);
}

static void
tasklist_properties_update_content_radio (WindowListApplet *tasklist)
{
	GtkWidget *button;
	
	if (tasklist->show_current_radio == NULL)
		return;

	if (tasklist->include_all_workspaces) {
		button = tasklist->show_all_radio;
	} else {
		button = tasklist->show_current_radio;
	}
	
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

	gtk_widget_set_sensitive (tasklist->minimized_windows_label,
				  tasklist->include_all_workspaces);
	gtk_widget_set_sensitive (tasklist->move_minimized_radio,
				  tasklist->include_all_workspaces);
	gtk_widget_set_sensitive (tasklist->change_workspace_radio,
				  tasklist->include_all_workspaces);
}

static void
display_all_workspaces_changed (GSettings        *settings,
                                const gchar      *key,
                                WindowListApplet *tasklist)
{
	gboolean value;

        value = g_settings_get_boolean (settings, key);

	tasklist->include_all_workspaces = (value != 0);
	tasklist_update (tasklist);

	tasklist_properties_update_content_radio (tasklist);
}

static GtkWidget *
get_grouping_button (WindowListApplet         *tasklist,
                     WnckTasklistGroupingType  type)
{
	switch (type) {
	default:
	case WNCK_TASKLIST_NEVER_GROUP:
		return tasklist->never_group_radio;
		break;
	case WNCK_TASKLIST_AUTO_GROUP:
		return tasklist->auto_group_radio;
		break;
	case WNCK_TASKLIST_ALWAYS_GROUP:
		return tasklist->always_group_radio;
		break;
	}
}

static void
group_windows_changed (GSettings        *settings,
                       const gchar      *key,
                       WindowListApplet *tasklist)
{
	GtkWidget *button;

	tasklist->grouping = g_settings_get_enum (settings, key);
	tasklist_update (tasklist);

	button = get_grouping_button (tasklist, tasklist->grouping);
        if (button &&
	    !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
}

static void
tasklist_update_unminimization_radio (WindowListApplet *tasklist)
{
	GtkWidget *button;
	
	if (tasklist->move_minimized_radio == NULL)
		return;

	if (tasklist->move_unminimized_windows) {
		button = tasklist->move_minimized_radio;
	} else {
		button = tasklist->change_workspace_radio;
	}
	
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
}


static void
move_unminimized_windows_changed (GSettings        *settings,
                                  const gchar      *key,
                                  WindowListApplet *tasklist)
{
	gboolean value;
	
	value = g_settings_get_boolean (settings, key);
	
	tasklist->move_unminimized_windows = (value != 0);
	tasklist_update (tasklist);

	tasklist_update_unminimization_radio (tasklist);
}

static void
setup_gsettings (WindowListApplet *tasklist)
{
        tasklist->settings =
          gp_applet_settings_new (GP_APPLET (tasklist),
                                     "org.gnome.gnome-panel.applet.window-list");

        g_signal_connect (tasklist->settings, "changed::display-all-workspaces",
                          G_CALLBACK (display_all_workspaces_changed), tasklist);

        g_signal_connect (tasklist->settings, "changed::group-windows",
                          G_CALLBACK (group_windows_changed), tasklist);

        g_signal_connect (tasklist->settings, "changed::move-unminimized-windows",
                          G_CALLBACK (move_unminimized_windows_changed), tasklist);
}

static void
group_windows_toggled (GtkToggleButton  *button,
                       WindowListApplet *tasklist)
{
	if (gtk_toggle_button_get_active (button)) {
		char *str;
		str = g_object_get_data (G_OBJECT (button), "group_value");
                g_settings_set_string (tasklist->settings, "group-windows", str);
	}
}

static void
move_minimized_toggled (GtkToggleButton  *button,
                        WindowListApplet *tasklist)
{
	g_settings_set_boolean (tasklist->settings, "move-unminimized-windows",
                                gtk_toggle_button_get_active (button));
}

static void
display_all_workspaces_toggled (GtkToggleButton  *button,
                                WindowListApplet *tasklist)
{
	g_settings_set_boolean (tasklist->settings, "display-all-workspaces",
                                gtk_toggle_button_get_active (button));
}

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

static void
setup_sensitivity (WindowListApplet *tasklist,
                   GtkBuilder       *builder,
                   const gchar      *wid1,
                   const gchar      *wid2,
                   const gchar      *wid3,
                   const gchar      *key)
{
	GtkWidget *w;

	if (g_settings_is_writable (tasklist->settings, key)) {
		return;
	}

	w = WID (wid1);
	g_assert (w != NULL);
	gtk_widget_set_sensitive (w, FALSE);

	if (wid2 != NULL) {
		w = WID (wid2);
		g_assert (w != NULL);
		gtk_widget_set_sensitive (w, FALSE);
	}
	if (wid3 != NULL) {
		w = WID (wid3);
		g_assert (w != NULL);
		gtk_widget_set_sensitive (w, FALSE);
	}

}

static void
setup_dialog (GtkBuilder       *builder,
              WindowListApplet *tasklist)
{
	GtkWidget *button;
	
	tasklist->show_current_radio = WID ("show_current_radio");
	tasklist->show_all_radio = WID ("show_all_radio");

	setup_sensitivity (tasklist, builder,
			   "show_current_radio",
			   "show_all_radio",
			   NULL,
			   "display-all-workspaces" /* key */);

	tasklist->never_group_radio = WID ("never_group_radio");
	tasklist->auto_group_radio = WID ("auto_group_radio");
	tasklist->always_group_radio = WID ("always_group_radio");

	setup_sensitivity (tasklist, builder,
			   "never_group_radio",
			   "auto_group_radio",
			   "always_group_radio",
			   "group-windows" /* key */);

	tasklist->minimized_windows_label = WID ("minimized_windows_label");
	tasklist->move_minimized_radio = WID ("move_minimized_radio");
	tasklist->change_workspace_radio = WID ("change_workspace_radio");

	setup_sensitivity (tasklist, builder,
			   "move_minimized_radio",
			   "change_workspace_radio",
			   NULL,
			   "move-unminimized-windows" /* key */);

	/* Window grouping: */
	button = get_grouping_button (tasklist, tasklist->grouping);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	g_object_set_data (G_OBJECT (tasklist->never_group_radio),
			   "group_value", (gpointer) "never-group");
	g_object_set_data (G_OBJECT (tasklist->auto_group_radio),
			   "group_value", (gpointer) "auto-group");
	g_object_set_data (G_OBJECT (tasklist->always_group_radio),
			   "group_value", (gpointer) "always-group");

	g_signal_connect (G_OBJECT (tasklist->never_group_radio), "toggled",
			  (GCallback) group_windows_toggled, tasklist);
	g_signal_connect (G_OBJECT (tasklist->auto_group_radio), "toggled",
			  (GCallback) group_windows_toggled, tasklist);
	g_signal_connect (G_OBJECT (tasklist->always_group_radio), "toggled",
			  (GCallback) group_windows_toggled, tasklist);

	/* move window when unminimizing: */
	tasklist_update_unminimization_radio (tasklist);
	g_signal_connect (G_OBJECT (tasklist->move_minimized_radio), "toggled",
			  (GCallback) move_minimized_toggled, tasklist);

	/* Tasklist content: */
	tasklist_properties_update_content_radio (tasklist);
	g_signal_connect (G_OBJECT (tasklist->show_all_radio), "toggled",
			  (GCallback) display_all_workspaces_toggled, tasklist);

	g_signal_connect_swapped (WID ("done_button"), "clicked",
				  (GCallback) gtk_widget_hide, 
				  tasklist->properties_dialog);
	g_signal_connect (tasklist->properties_dialog, "response",
			  G_CALLBACK (response_cb),
			  tasklist);
}

static void 
display_properties_dialog (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
	WindowListApplet *tasklist = (WindowListApplet *) user_data;

	if (tasklist->properties_dialog == NULL) {
		GtkBuilder *builder;

		builder = gtk_builder_new ();
		gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
		gtk_builder_add_from_resource (builder, WNCKLET_RESOURCE_PATH "window-list.ui", NULL);

		tasklist->properties_dialog = WID ("tasklist_properties_dialog");

		g_object_add_weak_pointer (G_OBJECT (tasklist->properties_dialog),
					   (void**) &tasklist->properties_dialog);

		setup_dialog (builder, tasklist);
		
		g_object_unref (builder);
	}

	gtk_window_set_icon_name (GTK_WINDOW (tasklist->properties_dialog),
				  WINDOW_LIST_ICON); 

	gtk_window_set_resizable (GTK_WINDOW (tasklist->properties_dialog), FALSE);
	gtk_window_present (GTK_WINDOW (tasklist->properties_dialog));
}

static const GActionEntry tasklist_menu_actions [] = {
        { "preferences", display_properties_dialog, NULL, NULL, NULL },
        { NULL }
};

static void
window_list_applet_fill (GpApplet *applet)
{
	WindowListApplet *tasklist;
	GAction *action;

	tasklist = WINDOW_LIST_APPLET (applet);

	setup_gsettings (tasklist);

	tasklist->include_all_workspaces = g_settings_get_boolean (tasklist->settings, "display-all-workspaces");
	tasklist->grouping = g_settings_get_enum (tasklist->settings, "group-windows");
	tasklist->move_unminimized_windows = g_settings_get_boolean (tasklist->settings, "move-unminimized-windows");

	tasklist->orientation = gp_applet_get_orientation (applet);

	tasklist->handle = wnck_handle_new (WNCK_CLIENT_TYPE_PAGER);
	tasklist->tasklist = wnck_tasklist_new_with_handle (tasklist->handle);
	tasklist->icon_theme = gtk_icon_theme_get_default ();

	wnck_tasklist_set_orientation (WNCK_TASKLIST (tasklist->tasklist), tasklist->orientation);

	g_object_bind_property (tasklist, "enable-tooltips",
	                        tasklist->tasklist, "tooltips-enabled",
	                        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	g_signal_connect (G_OBJECT (tasklist->tasklist), "destroy",
			  G_CALLBACK (destroy_tasklist),
			  tasklist);
	tasklist_update (tasklist);
	gtk_widget_show (tasklist->tasklist);

	gtk_container_add (GTK_CONTAINER (tasklist), tasklist->tasklist);

	gp_applet_setup_menu_from_resource (applet,
	                                    WNCKLET_RESOURCE_PATH "window-list-menu.ui",
	                                    tasklist_menu_actions);

	action = gp_applet_menu_lookup_action (applet, "preferences");
	g_object_bind_property (tasklist, "locked-down", action, "enabled",
				G_BINDING_DEFAULT|G_BINDING_INVERT_BOOLEAN|G_BINDING_SYNC_CREATE);

	gtk_widget_show (GTK_WIDGET (tasklist));
}

static void
window_list_applet_constructed (GObject *object)
{
	G_OBJECT_CLASS (window_list_applet_parent_class)->constructed (object);

	window_list_applet_fill (GP_APPLET (object));
}

static void
window_list_applet_dispose (GObject *object)
{
	WindowListApplet *tasklist;

	tasklist = WINDOW_LIST_APPLET (object);

	g_clear_object (&tasklist->handle);

	G_OBJECT_CLASS (window_list_applet_parent_class)->dispose (object);
}

static void
window_list_applet_placement_changed (GpApplet        *applet,
                                      GtkOrientation   orientation,
                                      GtkPositionType  position)
{
	WindowListApplet *tasklist;

	tasklist = WINDOW_LIST_APPLET (applet);

	if (orientation == tasklist->orientation)
		return;

	tasklist->orientation = orientation;

	wnck_tasklist_set_orientation (WNCK_TASKLIST (tasklist->tasklist), orientation);

	tasklist_update (tasklist);
}

static void
window_list_applet_class_init (WindowListAppletClass *tasklist_class)
{
	GObjectClass *object_class;
	GpAppletClass *applet_class;

	object_class = G_OBJECT_CLASS (tasklist_class);
	applet_class = GP_APPLET_CLASS (tasklist_class);

	object_class->constructed = window_list_applet_constructed;
	object_class->dispose = window_list_applet_dispose;

	applet_class->placement_changed = window_list_applet_placement_changed;
}

static void
window_list_applet_init (WindowListApplet *tasklist)
{
	GpAppletFlags flags;

	flags = GP_APPLET_FLAGS_EXPAND_MAJOR |
	        GP_APPLET_FLAGS_EXPAND_MINOR |
	        GP_APPLET_FLAGS_HAS_HANDLE;

	gp_applet_set_flags (GP_APPLET (tasklist), flags);
}

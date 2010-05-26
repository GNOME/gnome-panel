/* -*- mode: C; c-file-style: "linux" -*- */
/*
 * libwnck based tasklist applet.
 * (C) 2001 Red Hat, Inc
 * (C) 2001 Alexander Larsson 
 *
 * Authors: Alexander Larsson
 *
 */

#define WNCK_I_KNOW_THIS_IS_UNSTABLE 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <panel-applet.h>
#include <panel-applet-gconf.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>
#include <gconf/gconf-client.h>

#include "wncklet.h"
#include "window-list.h"

#define WINDOW_LIST_ICON "gnome-panel-window-list"

typedef struct {
	GtkWidget *applet;
	GtkWidget *tasklist;
	
	gboolean include_all_workspaces;
	WnckTasklistGroupingType grouping;
	gboolean move_unminimized_windows;
  
	GtkOrientation orientation;
	int size;

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

	GtkWidget *about;

	/* gconf listeners id */
	guint listeners [3];
} TasklistData;

static void display_properties_dialog (GtkAction    *action,
				       TasklistData *tasklist);
static void display_help_dialog       (GtkAction    *action,
				       TasklistData *tasklist);
static void display_about_dialog      (GtkAction    *action,
				       TasklistData *tasklist);

static void
tasklist_update (TasklistData *tasklist)
{
	if (tasklist->orientation == GTK_ORIENTATION_HORIZONTAL) {
		gtk_widget_set_size_request (GTK_WIDGET (tasklist->tasklist),
					     -1, tasklist->size);
	} else {
		gtk_widget_set_size_request (GTK_WIDGET (tasklist->tasklist),
					     tasklist->size, -1);
	}

	wnck_tasklist_set_grouping (WNCK_TASKLIST (tasklist->tasklist),
				    tasklist->grouping);
	wnck_tasklist_set_include_all_workspaces (WNCK_TASKLIST (tasklist->tasklist),
						  tasklist->include_all_workspaces);
	wnck_tasklist_set_switch_workspace_on_unminimize (WNCK_TASKLIST (tasklist->tasklist),
							  tasklist->move_unminimized_windows);
}

static void
response_cb (GtkWidget    *widget,
	     int           id,
	     TasklistData *tasklist)
{
	if (id == GTK_RESPONSE_HELP)
		wncklet_display_help (widget, "user-guide",
				      "windowlist-prefs", WINDOW_LIST_ICON);
	else
		gtk_widget_hide (widget);
}

static void
applet_realized (PanelApplet  *applet,
		 TasklistData *tasklist)
{
	tasklist->icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (tasklist->applet));
}

static void
applet_change_orient (PanelApplet       *applet,
		      PanelAppletOrient  orient,
		      TasklistData      *tasklist)
{
	GtkOrientation new_orient;
  
	switch (orient)	{
	case PANEL_APPLET_ORIENT_LEFT:
	case PANEL_APPLET_ORIENT_RIGHT:
		new_orient = GTK_ORIENTATION_VERTICAL;
		break;
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
	default:
		new_orient = GTK_ORIENTATION_HORIZONTAL;
		break;
	}
	
	if (new_orient == tasklist->orientation)
		return;
  
	tasklist->orientation = new_orient;

	tasklist_update (tasklist);
}

static void
applet_change_background (PanelApplet               *applet,
			  PanelAppletBackgroundType  type,
			  GdkColor                  *color,
			  GdkPixmap                 *pixmap,
			  TasklistData              *tasklist)
{
	switch (type) {
	case PANEL_NO_BACKGROUND:
		wnck_tasklist_set_button_relief (WNCK_TASKLIST (tasklist->tasklist),
						 GTK_RELIEF_NORMAL);
		break;
	case PANEL_COLOR_BACKGROUND:
	case PANEL_PIXMAP_BACKGROUND:
		wnck_tasklist_set_button_relief (WNCK_TASKLIST (tasklist->tasklist),
						 GTK_RELIEF_NONE);
		break;
	}
}

static void
applet_change_pixel_size (PanelApplet  *applet,
			  gint          size,
			  TasklistData *tasklist)
{
	if (tasklist->size == size)
		return;

	tasklist->size = size;
	
	tasklist_update (tasklist);
}

static void
destroy_tasklist(GtkWidget * widget, TasklistData *tasklist)
{
	GConfClient *client = gconf_client_get_default ();

	gconf_client_notify_remove (client, tasklist->listeners[0]);
	gconf_client_notify_remove (client, tasklist->listeners[1]);
	gconf_client_notify_remove (client, tasklist->listeners[2]);

	g_object_unref (G_OBJECT (client));
	
	tasklist->listeners[0] = 0;
	tasklist->listeners[1] = 0;
	tasklist->listeners[2] = 0;

	if (tasklist->properties_dialog)
		gtk_widget_destroy (tasklist->properties_dialog);

	if (tasklist->about)
		gtk_widget_destroy (tasklist->about);

        g_free (tasklist);
}

static const GtkActionEntry tasklist_menu_actions [] = {
	{ "TasklistPreferences", GTK_STOCK_PROPERTIES, N_("_Preferences"),
	  NULL, NULL,
	  G_CALLBACK (display_properties_dialog) },
	{ "TasklistHelp", GTK_STOCK_HELP, N_("_Help"),
	  NULL, NULL,
	  G_CALLBACK (display_help_dialog) },
	{ "TasklistAbout", GTK_STOCK_ABOUT, N_("_About"),
	  NULL, NULL,
	  G_CALLBACK (display_about_dialog) }
};

static void
tasklist_properties_update_content_radio (TasklistData *tasklist)
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
display_all_workspaces_changed (GConfClient  *client,
				guint         cnxn_id,
				GConfEntry   *entry,
				TasklistData *tasklist)
{
	gboolean value;

	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;
	
	value = gconf_value_get_bool (entry->value);
	
	tasklist->include_all_workspaces = (value != 0);
	tasklist_update (tasklist);

	tasklist_properties_update_content_radio (tasklist);
}

static WnckTasklistGroupingType
get_grouping_type (GConfValue *value)
{
	WnckTasklistGroupingType type = -1;
	const char *str;

	g_assert (value != NULL);

	/* Backwards compat for old type: */
	if (value->type == GCONF_VALUE_BOOL) {
		type = (gconf_value_get_bool (value)) ? WNCK_TASKLIST_AUTO_GROUP:WNCK_TASKLIST_NEVER_GROUP;

	} else if (value->type == GCONF_VALUE_STRING) {
		str = gconf_value_get_string (value);
		if (g_ascii_strcasecmp (str, "never") == 0) {
			type = WNCK_TASKLIST_NEVER_GROUP;
		} else if (g_ascii_strcasecmp (str, "auto") == 0) {
			type = WNCK_TASKLIST_AUTO_GROUP;
		} else if (g_ascii_strcasecmp (str, "always") == 0) {
			type = WNCK_TASKLIST_ALWAYS_GROUP;
		}
	}

	return type;
}

static GtkWidget *
get_grouping_button (TasklistData *tasklist,
		     WnckTasklistGroupingType type)
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
group_windows_changed (GConfClient  *client,
		       guint         cnxn_id,
		       GConfEntry   *entry,
		       TasklistData *tasklist)
{
	WnckTasklistGroupingType type;
	GtkWidget *button;

	if (!entry->value ||
	    (entry->value->type != GCONF_VALUE_BOOL &&
	     entry->value->type != GCONF_VALUE_STRING))
		return;

	type = get_grouping_type (entry->value);
	if (type == -1) {
		g_warning ("tasklist: Unknown value for GConf key 'group_windows'");
		return;
	}
		
	tasklist->grouping = type;
	tasklist_update (tasklist);

	button = get_grouping_button (tasklist, type);
        if (button &&
	    !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}
}

static void
tasklist_update_unminimization_radio (TasklistData *tasklist)
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
move_unminimized_windows_changed (GConfClient  *client,
				  guint         cnxn_id,
				  GConfEntry   *entry,
				  TasklistData *tasklist)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	tasklist->move_unminimized_windows = (value != 0);
	tasklist_update (tasklist);

	tasklist_update_unminimization_radio (tasklist);
}

static void
setup_gconf (TasklistData *tasklist)
{
	GConfClient *client;
	char *key;

	client = gconf_client_get_default ();

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (tasklist->applet),
					       "display_all_workspaces");
	tasklist->listeners[0] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)display_all_workspaces_changed,
				tasklist,
				NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (tasklist->applet),
					       "group_windows");
	tasklist->listeners[1] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)group_windows_changed,
				tasklist,
				NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (tasklist->applet),
					       "move_unminimized_windows");
	tasklist->listeners[2] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)move_unminimized_windows_changed,
				tasklist,
				NULL, NULL);
	g_free (key);

	g_object_unref (G_OBJECT (client));
}

static void
applet_size_request (GtkWidget      *widget,
		     GtkRequisition *requisition,
		     TasklistData   *tasklist)
{
	int len;
	const int *size_hints;
	GtkRequisition child_req;
    	WnckTasklist *wncktl = WNCK_TASKLIST (tasklist->tasklist);

	gtk_widget_get_child_requisition (tasklist->applet, &child_req);

	size_hints = wnck_tasklist_get_size_hint_list (wncktl, &len);
	g_assert (len % 2 == 0);

        panel_applet_set_size_hints (PANEL_APPLET (tasklist->applet),
		                     size_hints, len, 0);
}

static GdkPixbuf*
icon_loader_func (const char  *icon,
                  int          size,
                  unsigned int flags,
                  void        *data)
{
        TasklistData *tasklist;
	GdkPixbuf    *retval;
	char         *icon_no_extension;
	char         *p;
        
        tasklist = data;

	if (icon == NULL || strcmp (icon, "") == 0)
		return NULL;

	if (g_path_is_absolute (icon)) {
		if (g_file_test (icon, G_FILE_TEST_EXISTS)) {
			return gdk_pixbuf_new_from_file_at_size (icon,
								 size, size,
								 NULL);
		} else {
			char *basename;

			basename = g_path_get_basename (icon);
			retval = icon_loader_func (basename, size, flags, data);
			g_free (basename);

			return retval;
		}
	}

	/* This is needed because some .desktop files have an icon name *and*
	 * an extension as icon */
	icon_no_extension = g_strdup (icon);
	p = strrchr (icon_no_extension, '.');
	if (p &&
	    (strcmp (p, ".png") == 0 ||
	     strcmp (p, ".xpm") == 0 ||
	     strcmp (p, ".svg") == 0)) {
	    *p = 0;
	}

	retval = gtk_icon_theme_load_icon (tasklist->icon_theme,
					   icon_no_extension, size, 0, NULL);
	g_free (icon_no_extension);

	return retval;
}

gboolean
window_list_applet_fill (PanelApplet *applet)
{
	TasklistData *tasklist;
	GtkActionGroup *action_group;
	gchar *ui_path;
	GError *error;
	GConfValue *value;

	tasklist = g_new0 (TasklistData, 1);

	tasklist->applet = GTK_WIDGET (applet);

	panel_applet_set_flags (PANEL_APPLET (tasklist->applet),
				PANEL_APPLET_EXPAND_MAJOR |
				PANEL_APPLET_EXPAND_MINOR |
				PANEL_APPLET_HAS_HANDLE);

	panel_applet_add_preferences (applet, "/schemas/apps/window_list_applet/prefs", NULL);

	setup_gconf (tasklist);

	error = NULL;
	tasklist->include_all_workspaces = panel_applet_gconf_get_bool (applet, "display_all_workspaces", &error);
	if (error) {
		g_error_free (error);
		tasklist->include_all_workspaces = FALSE; /* Default value */
	}

	error = NULL;
	tasklist->grouping = -1;
	value = panel_applet_gconf_get_value (applet, "group_windows", &error);
	if (error) {
		g_error_free (error);
	} else if (value) {
		tasklist->grouping = get_grouping_type (value);
		gconf_value_free (value);
	}
	if (tasklist->grouping < 0)
		tasklist->grouping = WNCK_TASKLIST_AUTO_GROUP; /* Default value */
	
	error = NULL;
	tasklist->move_unminimized_windows = panel_applet_gconf_get_bool (applet, "move_unminimized_windows", &error);
	if (error) {
		g_error_free (error);
		tasklist->move_unminimized_windows = TRUE; /* Default value */
	}

	tasklist->size = panel_applet_get_size (applet);
	switch (panel_applet_get_orient (applet)) {
	case PANEL_APPLET_ORIENT_LEFT:
	case PANEL_APPLET_ORIENT_RIGHT:
		tasklist->orientation = GTK_ORIENTATION_VERTICAL;
		break;
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
	default:
		tasklist->orientation = GTK_ORIENTATION_HORIZONTAL;
		break;
	}

	tasklist->tasklist = wnck_tasklist_new (NULL);

        wnck_tasklist_set_icon_loader (WNCK_TASKLIST (tasklist->tasklist),
                                       icon_loader_func,
                                       tasklist,
                                       NULL);
        
	g_signal_connect (G_OBJECT (tasklist->tasklist), "destroy",
			  G_CALLBACK (destroy_tasklist),
			  tasklist);

	g_signal_connect (G_OBJECT (tasklist->applet), "size_request",
			  G_CALLBACK (applet_size_request),
			  tasklist);
	tasklist_update (tasklist);
	gtk_widget_show (tasklist->tasklist);

	gtk_container_add (GTK_CONTAINER (tasklist->applet), tasklist->tasklist);

	g_signal_connect (G_OBJECT (tasklist->applet),
			  "realize",
			  G_CALLBACK (applet_realized),
			  tasklist);
	g_signal_connect (G_OBJECT (tasklist->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient),
			  tasklist);
	g_signal_connect (G_OBJECT (tasklist->applet),
			  "change_size",
			  G_CALLBACK (applet_change_pixel_size),
			  tasklist);
	g_signal_connect (G_OBJECT (tasklist->applet),
			  "change_background",
			  G_CALLBACK (applet_change_background),
			  tasklist);

	panel_applet_set_background_widget (PANEL_APPLET (tasklist->applet),
					    GTK_WIDGET (tasklist->applet));

	action_group = gtk_action_group_new ("Tasklist Applet Actions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      tasklist_menu_actions,
				      G_N_ELEMENTS (tasklist_menu_actions),
				      tasklist);
	ui_path = g_build_filename (WNCK_MENU_UI_DIR, "window-list-menu.xml", NULL);
	panel_applet_setup_menu_from_file (PANEL_APPLET (tasklist->applet),
					   ui_path, action_group);
	g_free (ui_path);

	if (panel_applet_get_locked_down (PANEL_APPLET (tasklist->applet))) {
		GtkAction *action;

		action = gtk_action_group_get_action (action_group, "TasklistPreferences");
		gtk_action_set_visible (action, FALSE);
	}
	g_object_unref (action_group);

	gtk_widget_show (tasklist->applet);
	
	return TRUE;
}


static void
display_help_dialog (GtkAction    *action,
		     TasklistData *tasklist)
{
	wncklet_display_help (tasklist->applet, "user-guide",
			      "windowlist", WINDOW_LIST_ICON);
}

static void
display_about_dialog (GtkAction    *action,
		      TasklistData *tasklist)
{
	static const gchar *authors[] =
	{
		"Alexander Larsson <alla@lysator.liu.se>",
		NULL
	};
	const char *documenters [] = {
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		NULL
	};
	const char *translator_credits = _("translator-credits");

	wncklet_display_about (tasklist->applet, &tasklist->about,
			       _("Window List"),
			       "Copyright \xc2\xa9 2001-2002 Red Hat, Inc.",
			       _("The Window List shows a list of all windows in a set of buttons and lets you browse them."),
			       authors,
			       documenters,
			       translator_credits,
			       WINDOW_LIST_ICON,
			       "tasklist",
			       "Tasklist");
}

static void
group_windows_toggled (GtkToggleButton *button,
		       TasklistData    *tasklist)
{
	if (gtk_toggle_button_get_active (button)) {
		char *str;
		str = g_object_get_data (G_OBJECT (button), "group_value");
		panel_applet_gconf_set_string (PANEL_APPLET (tasklist->applet),
					       "group_windows", str,
					       NULL);
	}
}
static void
move_minimized_toggled (GtkToggleButton *button,
			TasklistData    *tasklist)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (tasklist->applet),
				     "move_unminimized_windows",
				     gtk_toggle_button_get_active (button),
				     NULL);
}
static void
display_all_workspaces_toggled (GtkToggleButton *button,
				TasklistData    *tasklist)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (tasklist->applet),
				     "display_all_workspaces",
				     gtk_toggle_button_get_active (button),
				     NULL);
}

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

static void
setup_sensitivity (TasklistData *tasklist,
		   GConfClient *client,
		   GtkBuilder *builder,
		   const char *wid1,
		   const char *wid2,
		   const char *wid3,
		   const char *key)
{
	PanelApplet *applet = PANEL_APPLET (tasklist->applet);
	char *fullkey;
	GtkWidget *w;

	fullkey = panel_applet_gconf_get_full_key (applet, key);

	if (gconf_client_key_is_writable (client, fullkey, NULL)) {
		g_free (fullkey);
		return;
	}
	g_free (fullkey);

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
setup_dialog (GtkBuilder   *builder,
	      TasklistData *tasklist)
{
	GConfClient *client;
	GtkWidget *button;
	
	client = gconf_client_get_default ();

	tasklist->show_current_radio = WID ("show_current_radio");
	tasklist->show_all_radio = WID ("show_all_radio");

	setup_sensitivity (tasklist, client, builder,
			   "show_current_radio",
			   "show_all_radio",
			   NULL,
			   "display_all_workspaces" /* key */);

	tasklist->never_group_radio = WID ("never_group_radio");
	tasklist->auto_group_radio = WID ("auto_group_radio");
	tasklist->always_group_radio = WID ("always_group_radio");

	setup_sensitivity (tasklist, client, builder,
			   "never_group_radio",
			   "auto_group_radio",
			   "always_group_radio",
			   "group_windows" /* key */);

	tasklist->minimized_windows_label = WID ("minimized_windows_label");
	tasklist->move_minimized_radio = WID ("move_minimized_radio");
	tasklist->change_workspace_radio = WID ("change_workspace_radio");

	setup_sensitivity (tasklist, client, builder,
			   "move_minimized_radio",
			   "change_workspace_radio",
			   NULL,
			   "move_unminimized_windows" /* key */);

	/* Window grouping: */
	button = get_grouping_button (tasklist, tasklist->grouping);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	g_object_set_data (G_OBJECT (tasklist->never_group_radio),
			   "group_value", "never");
	g_object_set_data (G_OBJECT (tasklist->auto_group_radio),
			   "group_value", "auto");
	g_object_set_data (G_OBJECT (tasklist->always_group_radio),
			   "group_value", "always");

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

	g_object_unref (G_OBJECT (client));
}



static void 
display_properties_dialog (GtkAction    *action,
			   TasklistData *tasklist)
{
	if (tasklist->properties_dialog == NULL) {
		GtkBuilder *builder;
		GError     *error;

		builder = gtk_builder_new ();
		gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);

		error = NULL;
		gtk_builder_add_from_file (builder, TASKLIST_BUILDERDIR "/window-list.ui", &error);
		if (error) {
			g_warning ("Error loading preferences: %s", error->message);
			g_error_free (error);
			return;
		}

		tasklist->properties_dialog = WID ("tasklist_properties_dialog");

		g_object_add_weak_pointer (G_OBJECT (tasklist->properties_dialog),
					   (void**) &tasklist->properties_dialog);

		setup_dialog (builder, tasklist);
		
		g_object_unref (builder);
	}

	gtk_window_set_icon_name (GTK_WINDOW (tasklist->properties_dialog),
				  WINDOW_LIST_ICON); 

	gtk_window_set_resizable (GTK_WINDOW (tasklist->properties_dialog), FALSE);
	gtk_window_set_screen (GTK_WINDOW (tasklist->properties_dialog),
			       gtk_widget_get_screen (tasklist->applet));
	gtk_window_present (GTK_WINDOW (tasklist->properties_dialog));
}

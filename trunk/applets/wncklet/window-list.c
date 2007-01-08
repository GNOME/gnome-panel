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

#include <gtk/gtkdialog.h>
#include <libgnomeui/gnome-help.h>
#include <glade/glade-xml.h>
#include <libwnck/libwnck.h>
#include <gconf/gconf-client.h>

#include "wncklet.h"
#include "window-list.h"

typedef struct {
	GtkWidget *applet;
	GtkWidget *tasklist;
	
	WnckScreen *screen;

	gboolean include_all_workspaces;
	WnckTasklistGroupingType grouping;
	gboolean move_unminimized_windows;
  
	GtkOrientation orientation;
	int size;
	gint maximum_size;

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

	GtkWidget *minimum_size_spin;
	GtkWidget *maximum_size_spin;
	GtkWidget *about;

	/* gconf listeners id */
	guint listeners [5];
} TasklistData;

static void display_properties_dialog (BonoboUIComponent *uic,
				       TasklistData      *tasklist,
				       const gchar       *verbname);
static void display_help_dialog       (BonoboUIComponent *uic,
				       TasklistData      *tasklist,
				       const gchar       *verbname);
static void display_about_dialog      (BonoboUIComponent *uic,
				       TasklistData      *tasklist,
				       const gchar       *verbname);

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
	if (id == GTK_RESPONSE_HELP) {
		GError *error = NULL;

		gnome_help_display_desktop_on_screen (
			NULL, "window-list", "window-list", "windowlist-prefs",
			gtk_widget_get_screen (tasklist->applet),
			&error);
		if (error) {
			GtkWidget *dialog;
			dialog = gtk_message_dialog_new (GTK_WINDOW (widget),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							  _("There was an error displaying help: %s"),
							 error->message);

			g_signal_connect (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy),
					  NULL);
	
			gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
			gtk_window_set_screen (GTK_WINDOW (dialog),
					       gtk_widget_get_screen (tasklist->applet));
			gtk_widget_show (dialog);
			g_error_free (error);
		}
	} else
		gtk_widget_hide (widget);
}

static void
applet_realized (PanelApplet  *applet,
		 TasklistData *tasklist)
{
	WnckScreen *screen;

	screen = wncklet_get_screen (GTK_WIDGET (applet));

	wnck_tasklist_set_screen (WNCK_TASKLIST (tasklist->tasklist), screen);

	tasklist->icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (tasklist->applet));
}

static void
applet_change_orient (PanelApplet       *applet,
		      PanelAppletOrient  orient,
		      TasklistData      *tasklist)
{
	GtkOrientation new_orient;
    	WnckTasklist *wncktl = WNCK_TASKLIST (tasklist->tasklist);
	gint minimum_size = 0;
  
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
	/* we need to unset minimum size on the wnck tasklist */
	if (tasklist->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	    	/* unset the minimum height and reset minimum width */
	        minimum_size = wnck_tasklist_get_minimum_height (wncktl); 
	        wnck_tasklist_set_minimum_height (wncktl, -1);
	        wnck_tasklist_set_minimum_width (wncktl, minimum_size);
	}
	else
	{
	    	/* unset the minimum width and reset minimum height */
	        minimum_size = wnck_tasklist_get_minimum_width (wncktl); 
	        wnck_tasklist_set_minimum_width (wncktl, -1);
	        wnck_tasklist_set_minimum_height (wncktl, minimum_size);
	}

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
	gconf_client_notify_remove (client, tasklist->listeners[3]);
	gconf_client_notify_remove (client, tasklist->listeners[4]);

	g_object_unref (G_OBJECT (client));
	
	tasklist->listeners[0] = 0;
	tasklist->listeners[1] = 0;
	tasklist->listeners[2] = 0;
	tasklist->listeners[3] = 0;
	tasklist->listeners[4] = 0;

	if (tasklist->properties_dialog)
		gtk_widget_destroy (tasklist->properties_dialog);

	if (tasklist->about)
		gtk_widget_destroy (tasklist->about);

        g_free (tasklist);
}

static const BonoboUIVerb tasklist_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("TasklistPreferences", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("TasklistHelp",        display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("TasklistAbout",       display_about_dialog),
        BONOBO_UI_VERB_END
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

/* GConf callback for changes in minimum_size */
static void
minimum_size_changed (GConfClient *client, guint cnxn_id,
		      GConfEntry *entry, TasklistData *tasklist)
{
    	WnckTasklist *wncktl = WNCK_TASKLIST (tasklist->tasklist);
	gint value;
	GtkSpinButton *button;

	if (!tasklist->minimum_size_spin)
		return;

	button = GTK_SPIN_BUTTON (tasklist->minimum_size_spin);

	if (!entry->value || entry->value->type != GCONF_VALUE_INT)
		return;
	
	value = gconf_value_get_int (entry->value);
	
	gtk_spin_button_set_value (button, value);
	if (tasklist->orientation == GTK_ORIENTATION_HORIZONTAL)
		wnck_tasklist_set_minimum_width (wncktl, value);
	else
		wnck_tasklist_set_minimum_height (wncktl, value);
}

/* GConf callback for changes in maximum_size */
static void
maximum_size_changed (GConfClient  *client, guint cnxn_id,
                      GConfEntry   *entry, TasklistData *tasklist)
{
	gint value;
	GtkSpinButton *button;

	if (!tasklist->maximum_size_spin)
		return;

	button = GTK_SPIN_BUTTON (tasklist->maximum_size_spin);
	if (!entry->value || entry->value->type != GCONF_VALUE_INT)
		return;
	
	value = gconf_value_get_int (entry->value);

	gtk_spin_button_set_value (button, value);
	tasklist->maximum_size = value;
        gtk_widget_queue_resize (GTK_WIDGET (tasklist->applet));
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
	
	key = panel_applet_gconf_get_full_key (PANEL_APPLET (tasklist->applet),
					       "minimum_size");
	tasklist->listeners[3] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)minimum_size_changed,
				tasklist,
				NULL, NULL);
	g_free (key);
	key = panel_applet_gconf_get_full_key (PANEL_APPLET (tasklist->applet),
					       "maximum_size");
	tasklist->listeners[4] = gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)maximum_size_changed,
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
	int i;
	int maximum_size;
	int *new_size_hints;
	int new_len = 0;
    	WnckTasklist *wncktl = WNCK_TASKLIST (tasklist->tasklist);
	gint minimum_size = 0;
	
	
	if (tasklist->orientation == GTK_ORIENTATION_HORIZONTAL)
		minimum_size = wnck_tasklist_get_minimum_width (wncktl);
	else
		minimum_size = wnck_tasklist_get_minimum_height (wncktl);
	
	gtk_widget_get_child_requisition (tasklist->applet, &child_req);
	
	size_hints = wnck_tasklist_get_size_hint_list (wncktl, &len);
	new_size_hints = g_new0 (int, len);
	
	/* size_hints is an array of (max, min) int pairs
	 * where min(i) > max (i+1)
	 * convert it to clipped values
	 */
	maximum_size = tasklist->maximum_size - minimum_size;
	g_assert (maximum_size >= 0);

	for (i = 0; i < len; i += 2) {
		if (size_hints[i + 1] <= maximum_size) {
		        /* this one should be stored */
			if (size_hints[i] > maximum_size)
			    	new_size_hints[new_len] = maximum_size;
			else
				new_size_hints[new_len] = size_hints[i];
			new_size_hints[new_len + 1] = size_hints[i + 1];
			new_len += 2;
		}
	}
        panel_applet_set_size_hints (PANEL_APPLET (tasklist->applet),
		                     new_size_hints, 
				     new_len, child_req.width - 1);
	g_free (new_size_hints);
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
	GError *error;
	GConfValue *value;

	gint sizepref;

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

	tasklist->screen = wncklet_get_screen (tasklist->applet);

	/* because the tasklist doesn't respond to signals at the moment */
	wnck_screen_force_update (tasklist->screen);

	tasklist->tasklist = wnck_tasklist_new (tasklist->screen);

        wnck_tasklist_set_icon_loader (WNCK_TASKLIST (tasklist->tasklist),
                                       icon_loader_func,
                                       tasklist,
                                       NULL);
        
	/* get size preferences */
	error = NULL;
	sizepref = panel_applet_gconf_get_int (applet, "minimum_size", &error);
	if (error) {
		sizepref = 50; /* Default value */
		g_error_free (error);
	}

	if (tasklist->orientation == GTK_ORIENTATION_HORIZONTAL)
		wnck_tasklist_set_minimum_width (WNCK_TASKLIST (tasklist->tasklist), sizepref);	  
	else
		wnck_tasklist_set_minimum_height (WNCK_TASKLIST (tasklist->tasklist), sizepref);	  

	error = NULL;
	sizepref = panel_applet_gconf_get_int (applet, "maximum_size", &error);
	if (error) {
		sizepref = 4096; /* Default value */
		g_error_free (error);
	}

	tasklist->maximum_size = sizepref;

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
	
	panel_applet_setup_menu_from_file (PANEL_APPLET (tasklist->applet),
					   NULL,
					   "GNOME_WindowListApplet.xml",
					   NULL,
					   tasklist_menu_verbs, 
					   tasklist);

	if (panel_applet_get_locked_down (PANEL_APPLET (tasklist->applet))) {
		BonoboUIComponent *popup_component;

		popup_component = panel_applet_get_popup_component (PANEL_APPLET (tasklist->applet));

		bonobo_ui_component_set_prop (popup_component,
					      "/commands/TasklistPreferences",
					      "hidden", "1",
					      NULL);
	}

	gtk_widget_show (tasklist->applet);
	
	return TRUE;
}


static void
display_help_dialog (BonoboUIComponent *uic,
		     TasklistData      *tasklist,
		     const gchar       *verbname)
{
	wncklet_display_help (tasklist->applet, "window-list",
			      "window-list", NULL);
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      TasklistData      *tasklist,
		      const gchar       *verbname)
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
			       "gnome-panel-window-list",
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

/* called when minimum width spin button is changed
 * check if it exceeds max width
 * saves numeric GConf preference values
 */
static void
spin_minimum_size_changed (GtkSpinButton *button, TasklistData *tasklist)
{
	GtkSpinButton *max_b = GTK_SPIN_BUTTON (tasklist->maximum_size_spin);
	PanelApplet *applet = PANEL_APPLET (tasklist->applet);
	gint prop_value = gtk_spin_button_get_value (button);
	gint max_size = gtk_spin_button_get_value (max_b);
	
	/* check if we exceed max width */
	if (prop_value > max_size)
	        panel_applet_gconf_set_int (applet, "maximum_size", 
			                    prop_value, NULL);
	panel_applet_gconf_set_int (applet, "minimum_size", prop_value, NULL);
}

/* called when maximum width spin button is changed
 * check if we drop below min width
 * saves numeric GConf preference values
 */
static void
spin_maximum_size_changed (GtkSpinButton *button, TasklistData *tasklist)
{
	GtkSpinButton *min_b = GTK_SPIN_BUTTON (tasklist->minimum_size_spin);
	PanelApplet *applet = PANEL_APPLET (tasklist->applet);
	gint prop_value = gtk_spin_button_get_value (button);
	gint min_size = gtk_spin_button_get_value (min_b);

	/* check if we drop below min width */
	if (prop_value < min_size)
		panel_applet_gconf_set_int (applet, "minimum_size", 
					    prop_value, NULL);
	panel_applet_gconf_set_int (applet, "maximum_size", prop_value, NULL);
}

static void
setup_sensitivity (TasklistData *tasklist,
		   GConfClient *client,
		   GladeXML *xml,
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

	w = glade_xml_get_widget (xml, wid1);
	g_assert (w != NULL);
	gtk_widget_set_sensitive (w, FALSE);

	if (wid2 != NULL) {
		w = glade_xml_get_widget (xml, wid2);
		g_assert (w != NULL);
		gtk_widget_set_sensitive (w, FALSE);
	}
	if (wid3 != NULL) {
		w = glade_xml_get_widget (xml, wid3);
		g_assert (w != NULL);
		gtk_widget_set_sensitive (w, FALSE);
	}

}

#define WID(s) glade_xml_get_widget (xml, s)

static void
setup_dialog (GladeXML     *xml,
	      TasklistData *tasklist)
{
	GConfClient *client;
	GtkWidget *button;
	gint sizepref;
	GError *error;
	
	client = gconf_client_get_default ();

	tasklist->show_current_radio = WID ("show_current_radio");
	tasklist->show_all_radio = WID ("show_all_radio");

	setup_sensitivity (tasklist, client, xml,
			   "show_current_radio",
			   "show_all_radio",
			   NULL,
			   "display_all_workspaces" /* key */);

	tasklist->never_group_radio = WID ("never_group_radio");
	tasklist->auto_group_radio = WID ("auto_group_radio");
	tasklist->always_group_radio = WID ("always_group_radio");

	setup_sensitivity (tasklist, client, xml,
			   "never_group_radio",
			   "auto_group_radio",
			   "always_group_radio",
			   "group_windows" /* key */);

	tasklist->minimized_windows_label = WID ("minimized_windows_label");
	tasklist->move_minimized_radio = WID ("move_minimized_radio");
	tasklist->change_workspace_radio = WID ("change_workspace_radio");

	setup_sensitivity (tasklist, client, xml,
			   "move_minimized_radio",
			   "change_workspace_radio",
			   NULL,
			   "move_unminimized_windows" /* key */);

	tasklist->minimum_size_spin = WID ("minimum_size");
	setup_sensitivity (tasklist, client, xml,
			   "minimum_size",
			   "minimum_size_label",
			   "minimum_size_post_label",
			   "minimum_size" /* key */);

	tasklist->maximum_size_spin = WID ("maximum_size");
	setup_sensitivity (tasklist, client, xml,
			   "maximum_size",
			   "maximum_size_label",
			   "maximum_size_post_label",
			   "maximum_size" /* key */);

	/* Window grouping: */
	button = get_grouping_button (tasklist, tasklist->grouping);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	g_object_set_data (G_OBJECT (tasklist->never_group_radio),
			   "group_value", "never");
	g_object_set_data (G_OBJECT (tasklist->auto_group_radio),
			   "group_value", "auto");
	g_object_set_data (G_OBJECT (tasklist->always_group_radio),
			   "group_value", "always");
	error = NULL;
	/* FIXME: what does one do in case of errors here ? */
	sizepref = panel_applet_gconf_get_int (PANEL_APPLET (tasklist->applet), 
	    				       "minimum_size", NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (tasklist->minimum_size_spin), sizepref);
	sizepref = panel_applet_gconf_get_int (PANEL_APPLET (tasklist->applet), 
	    				       "maximum_size", NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (tasklist->maximum_size_spin), sizepref);

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

	g_signal_connect (G_OBJECT (tasklist->minimum_size_spin), 
		          "value_changed", 
			  (GCallback) spin_minimum_size_changed,
			  tasklist);
	g_signal_connect (G_OBJECT (tasklist->maximum_size_spin), 
		          "value_changed", 
			  (GCallback) spin_maximum_size_changed,
			  tasklist);
	g_signal_connect_swapped (WID ("done_button"), "clicked",
				  (GCallback) gtk_widget_hide, 
				  tasklist->properties_dialog);
	g_signal_connect (tasklist->properties_dialog, "response",
			  G_CALLBACK (response_cb),
			  tasklist);

	g_object_unref (G_OBJECT (client));
}



static void 
display_properties_dialog (BonoboUIComponent *uic,
			   TasklistData      *tasklist,
			   const gchar       *verbname)
{
	if (tasklist->properties_dialog == NULL) {
		GladeXML  *xml;

		xml = glade_xml_new (TASKLIST_GLADEDIR "/window-list.glade", NULL, NULL);
		tasklist->properties_dialog = glade_xml_get_widget (xml, "tasklist_properties_dialog");

		g_object_add_weak_pointer (G_OBJECT (tasklist->properties_dialog),
					   (void**) &tasklist->properties_dialog);

		setup_dialog (xml, tasklist);
		
		g_object_unref (G_OBJECT (xml));
	}

	gtk_window_set_icon_name (GTK_WINDOW (tasklist->properties_dialog),
				  "gnome-panel-window-list"); 

	gtk_window_set_resizable (GTK_WINDOW (tasklist->properties_dialog), FALSE);
	gtk_window_set_screen (GTK_WINDOW (tasklist->properties_dialog),
			       gtk_widget_get_screen (tasklist->applet));
	gtk_window_present (GTK_WINDOW (tasklist->properties_dialog));
}

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

#include <gtk/gtk.h>
#include <libbonobo.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnome/gnome-desktop-item.h>
#include <glade/glade-xml.h>
#include <libwnck/libwnck.h>
#include <gconf/gconf-client.h>

#include "tasklist.h"

#include "egg-screen-help.h"

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

        GnomeIconTheme *icon_theme;
  
	/* Properties: */
	GtkWidget *properties_dialog;
	GtkWidget *show_current_radio;
	GtkWidget *show_all_radio;
	GtkWidget *never_group_radio;
	GtkWidget *auto_group_radio;
	GtkWidget *always_group_radio;
	GtkWidget *move_minimized_radio;
	GtkWidget *change_workspace_radio;

	GtkWidget *minimum_size_spin;
	GtkWidget *maximum_size_spin;

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
response_cb(GtkWidget * widget,int id, TasklistData *tasklist)
{
	if(id == GTK_RESPONSE_HELP) {

		GError *error = NULL;
		static GnomeProgram *applet_program = NULL;

		if (!applet_program) {
			int argc = 1;
			char *argv[2] = { "window-list" };
			applet_program = gnome_program_init ("window-list", VERSION,
							     LIBGNOME_MODULE,argc, argv,
							     GNOME_PROGRAM_STANDARD_PROPERTIES,NULL);
		}

		egg_help_display_desktop_on_screen (
			applet_program, "window-list",
			"window-list", "windowlist-prefs",
			gtk_widget_get_screen (tasklist->applet),
			&error);
		if (error) {
			GtkWidget *dialog;
			dialog = gtk_message_dialog_new (GTK_WINDOW(widget),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							  _("There was an error displaying help: %s"),
							 error->message);

			g_signal_connect (G_OBJECT (dialog), "response",
					  G_CALLBACK (gtk_widget_destroy),
					  NULL);
	
			gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
			gtk_window_set_screen (GTK_WINDOW (dialog),
					       gtk_widget_get_screen (tasklist->applet));
			gtk_widget_show (dialog);
			g_error_free (error);
		}
	}
	else
		gtk_widget_hide (widget);
}

static WnckScreen *
applet_get_screen (GtkWidget *applet)
{
	int screen_num;

	if (!gtk_widget_has_screen (applet))
		return wnck_screen_get_default ();

	screen_num = gdk_screen_get_number (gtk_widget_get_screen (applet));

	return wnck_screen_get (screen_num);
}

static void
applet_realized (PanelApplet  *applet,
		 TasklistData *tasklist)
{
	WnckScreen *screen;

	screen = applet_get_screen (GTK_WIDGET (applet));

	wnck_tasklist_set_screen (WNCK_TASKLIST (tasklist->tasklist), screen);
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
			  const gchar               *pixmap,
			  TasklistData              *tasklist)
{
	if (type == PANEL_NO_BACKGROUND) {
		GtkRcStyle *rc_style = gtk_rc_style_new ();

		gtk_widget_modify_style (tasklist->applet, rc_style);

		g_object_unref (rc_style);
	}
	else if (type == PANEL_COLOR_BACKGROUND) {
		gtk_widget_modify_bg (tasklist->applet,
				      GTK_STATE_NORMAL,
				      color);
	} else { /* pixmap */
		/* FIXME: Handle this when the panel support works again */
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

        g_object_unref (tasklist->icon_theme);
        tasklist->icon_theme = NULL;
        
	gconf_client_notify_remove (client, tasklist->listeners[0]);
	gconf_client_notify_remove (client, tasklist->listeners[1]);
	gconf_client_notify_remove (client, tasklist->listeners[2]);
	gconf_client_notify_remove (client, tasklist->listeners[3]);
	gconf_client_notify_remove (client, tasklist->listeners[4]);
	
	tasklist->listeners[0] = 0;
	tasklist->listeners[1] = 0;
	tasklist->listeners[2] = 0;
	tasklist->listeners[3] = 0;
	tasklist->listeners[4] = 0;

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
	g_free (key);}

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
        char *file;
        GdkPixbuf *pixbuf;
        
        tasklist = data;
        
        file = gnome_desktop_item_find_icon (tasklist->icon_theme,
                                             icon, size, 0);

        if (file == NULL)
                return NULL;

        pixbuf = gdk_pixbuf_new_from_file (file, NULL);

        g_free (file);

        return pixbuf;
}

gboolean
fill_tasklist_applet(PanelApplet *applet)
{
	TasklistData *tasklist;
	GError *error;
	GConfValue *value;

	gint sizepref;

	panel_applet_add_preferences (applet, "/schemas/apps/tasklist_applet/prefs", NULL);
	
	tasklist = g_new0 (TasklistData, 1);

	tasklist->applet = GTK_WIDGET (applet);

	setup_gconf (tasklist);

        tasklist->icon_theme = gnome_icon_theme_new ();
        
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

	tasklist->screen = applet_get_screen (tasklist->applet);

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
	
	panel_applet_set_flags (PANEL_APPLET (tasklist->applet),
				PANEL_APPLET_EXPAND_MAJOR |
				PANEL_APPLET_EXPAND_MINOR |
				PANEL_APPLET_HAS_HANDLE);

	gtk_widget_show (tasklist->applet);

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
	/* FIXME: initial background, this needs some panel-applet voodoo */
	g_signal_connect (G_OBJECT (tasklist->applet),
			  "change_background",
			  G_CALLBACK (applet_change_background),
			  tasklist);
	
	panel_applet_setup_menu_from_file (PANEL_APPLET (tasklist->applet),
					   NULL,
					   "GNOME_TasklistApplet.xml",
					   NULL,
					   tasklist_menu_verbs, 
					   tasklist);
	
	return TRUE;
}


static void
display_help_dialog (BonoboUIComponent *uic,
		     TasklistData      *tasklist,
		     const gchar       *verbname)
{
	GError *error = NULL;
	static GnomeProgram *applet_program = NULL;

	if (!applet_program) {
		int argc = 1;
		char *argv[2] = { "window-list" };
		applet_program = gnome_program_init ("window-list", VERSION,
						     LIBGNOME_MODULE,argc, argv,
						     GNOME_PROGRAM_STANDARD_PROPERTIES,NULL);
	}

	egg_help_display_desktop_on_screen (
			applet_program, "window-list",
			"window-list", NULL,
			gtk_widget_get_screen (tasklist->applet),
			&error);
	if (error) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						  _("There was an error displaying help: %s"),
						 error->message);

		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (tasklist->applet));
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      TasklistData      *tasklist,
		      const gchar       *verbname)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;
	gchar *file;
	
	static const gchar *authors[] =
	{
		"Alexander Larsson <alla@lysator.liu.se>",
		NULL
	};
	const char *documenters [] = {
	  NULL
	};
	const char *translator_credits = _("translator_credits");

	if (about) {
		gtk_window_set_screen (GTK_WINDOW (about),
				       gtk_widget_get_screen (tasklist->applet));
		gtk_widget_show (about);
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-windows.png", TRUE, NULL);
	pixbuf = gdk_pixbuf_new_from_file (file, NULL);
	g_free(file);

	about = gnome_about_new (_("Window List"), VERSION,
				 "Copyright \xc2\xa9 2001-2002 Red Hat, Inc.",
				 _("The Window List shows a list of all visible windows and lets you browse them."),
				 authors,
				 documenters,
				 strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
				 pixbuf);
	
	gtk_window_set_wmclass (GTK_WINDOW (about), "tasklist", "Tasklist");
	gtk_window_set_screen (GTK_WINDOW (about),
			       gtk_widget_get_screen (tasklist->applet));

	if (pixbuf) {
		gtk_window_set_icon (GTK_WINDOW (about), pixbuf);
		g_object_unref (pixbuf);
	}
	
	g_signal_connect (G_OBJECT(about), "destroy",
			  (GCallback)gtk_widget_destroyed, &about);
	
	gtk_widget_show (about);
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
	tasklist->never_group_radio = WID ("never_group_radio");
	tasklist->auto_group_radio = WID ("auto_group_radio");
	tasklist->always_group_radio = WID ("always_group_radio");
	tasklist->move_minimized_radio = WID ("move_minimized_radio");
	tasklist->change_workspace_radio = WID ("change_workspace_radio");

	tasklist->minimum_size_spin = WID ("minimum_size");
	tasklist->maximum_size_spin = WID ("maximum_size");

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


}



static void 
display_properties_dialog (BonoboUIComponent *uic,
			   TasklistData      *tasklist,
			   const gchar       *verbname)
{
	if (tasklist->properties_dialog == NULL) {
		GladeXML  *xml;

		xml = glade_xml_new (TASKLIST_GLADEDIR "/tasklist.glade", NULL, NULL);
		tasklist->properties_dialog = glade_xml_get_widget (xml, "tasklist_properties_dialog");

		g_object_add_weak_pointer (G_OBJECT (tasklist->properties_dialog),
					   (void**) &tasklist->properties_dialog);

		setup_dialog (xml, tasklist);
		
		g_object_unref (G_OBJECT (xml));
	}

	gtk_window_set_screen (GTK_WINDOW (tasklist->properties_dialog),
			       gtk_widget_get_screen (tasklist->applet));
	gtk_window_present (GTK_WINDOW (tasklist->properties_dialog));
}

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

#include <panel-applet.h>
#include <panel-applet-gconf.h>

#include <gtk/gtk.h>
#include <libbonobo.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <glade/glade-xml.h>
#include <libwnck/libwnck.h>
#include <gconf/gconf-client.h>

#include "tasklist.h"

typedef struct {
	GtkWidget *applet;

	GtkWidget *tasklist;
	
	WnckScreen *screen;

	gboolean include_all_workspaces;
	gboolean enable_grouping;
	gboolean move_unminimized_windows;
  
	GtkOrientation orientation;
	int size;

	/* Properties: */
	GtkWidget *properties_dialog;
	GtkWidget *show_current_radio;
	GtkWidget *show_all_radio;
	GtkWidget *group_windows_toggle;
	GtkWidget *move_minimized_radio;
	GtkWidget *change_workspace_radio;
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

	wnck_tasklist_set_allow_grouping (WNCK_TASKLIST (tasklist->tasklist),
					  tasklist->enable_grouping);
	wnck_tasklist_set_include_all_workspaces (WNCK_TASKLIST (tasklist->tasklist),
						  tasklist->include_all_workspaces);
	/* TODO: Set move_unminimized_windows */
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
			  const gchar               *pixmap,
			  TasklistData              *tasklist)
{
	if (type == PANEL_NO_BACKGROUND) {
		GtkRcStyle *rc_style = gtk_rc_style_new ();

		gtk_widget_modify_style (tasklist->applet, rc_style);
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
destroy_tasklist(GtkWidget * widget, gpointer data)
{
	/* TasklistData *tasklist = data; */
}

static const BonoboUIVerb tasklist_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("TasklistProperties", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("TasklistHelp",       display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("TasklistAbout",      display_about_dialog),
        BONOBO_UI_VERB_END
};

static const char tasklist_menu_xml [] =
	"<popup name=\"button3\">\n"
	"   <menuitem name=\"Tasklist Properties Item\" verb=\"TasklistProperties\" _label=\"Properties ...\"\n"
	"             pixtype=\"stock\" pixname=\"gtk-properties\"/>\n"
	"   <menuitem name=\"Tasklist Help Item\" verb=\"TasklistHelp\" _label=\"Help\"\n"
	"             pixtype=\"stock\" pixname=\"gtk-help\"/>\n"
	"   <menuitem name=\"Tasklist About Item\" verb=\"TasklistAbout\" _label=\"About ...\"\n"
	"             pixtype=\"stock\" pixname=\"gnome-stock-about\"/>\n"
	"</popup>\n";



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
	gboolean value = FALSE; /* Default value */
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_BOOL) {
		value = gconf_value_get_bool (entry->value);
	}
	
	tasklist->include_all_workspaces = (value != 0);
	tasklist_update (tasklist);

	tasklist_properties_update_content_radio (tasklist);
}

static void
group_windows_changed (GConfClient  *client,
		       guint         cnxn_id,
		       GConfEntry   *entry,
		       TasklistData *tasklist)
{
	gboolean value = FALSE; /* Default value */
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_BOOL) {
		value = gconf_value_get_bool (entry->value);
	}
	
	tasklist->enable_grouping = (value != 0);
	tasklist_update (tasklist);

	if (tasklist->group_windows_toggle &&
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tasklist->group_windows_toggle)) != tasklist->enable_grouping) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->group_windows_toggle),
					      tasklist->enable_grouping);
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
	gboolean value = FALSE; /* Default value */
	
	if (entry->value != NULL &&
	    entry->value->type == GCONF_VALUE_BOOL) {
		value = gconf_value_get_bool (entry->value);
	}
	
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
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)display_all_workspaces_changed,
				tasklist,
				NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (tasklist->applet),
					       "group_windows");
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)group_windows_changed,
				tasklist,
				NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (tasklist->applet),
					       "move_unminimized_windows");
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)move_unminimized_windows_changed,
				tasklist,
				NULL, NULL);
	g_free (key);
}


gboolean
fill_tasklist_applet(PanelApplet *applet)
{
	TasklistData *tasklist;
	GError *error;

	panel_applet_add_preferences (applet, "/schemas/apps/tasklist-applet/prefs", NULL);
	
	tasklist = g_new0 (TasklistData, 1);

	tasklist->applet = GTK_WIDGET (applet);

	setup_gconf (tasklist);
	
	error = NULL;
	tasklist->include_all_workspaces = panel_applet_gconf_get_bool (applet, "display_all_workspaces", &error);
	if (error) {
		g_error_free (error);
		tasklist->include_all_workspaces = FALSE; /* Default value */
	}

	error = NULL;
	tasklist->enable_grouping = panel_applet_gconf_get_bool (applet, "group_windows", &error);
	if (error) {
		g_error_free (error);
		tasklist->enable_grouping = TRUE; /* Default value */
	}

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

	/* FIXME: Needs to get the screen number from DISPLAY or the panel. */
	tasklist->screen = wnck_screen_get (0);

	/* because the tasklist doesn't respond to signals at the moment */
	wnck_screen_force_update (tasklist->screen);

	tasklist->tasklist = wnck_tasklist_new (tasklist->screen);

	g_signal_connect (G_OBJECT (tasklist->tasklist), "destroy",
			  G_CALLBACK (destroy_tasklist),
			  tasklist);

	tasklist_update (tasklist);
	gtk_widget_show (tasklist->tasklist);

	gtk_container_add (GTK_CONTAINER (tasklist->applet), tasklist->tasklist);
	
	panel_applet_set_flags (PANEL_APPLET (tasklist->applet),
				PANEL_APPLET_EXPAND_MAJOR | PANEL_APPLET_EXPAND_MINOR);

	gtk_widget_show (tasklist->applet);

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
	
	panel_applet_setup_menu (PANEL_APPLET (tasklist->applet), tasklist_menu_xml, tasklist_menu_verbs, tasklist);
	
	setup_gconf (tasklist);
	
	return TRUE;
}


static void
display_help_dialog (BonoboUIComponent *uic,
		     TasklistData      *tasklist,
		     const gchar       *verbname)
{
	/* FIXME: Implement this */
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      TasklistData      *tasklist,
		      const gchar       *verbname)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf;
	gchar *file;
	
	static const gchar *authors[] =
	{
		"Alexander Larsson <alla@lysator.liu.se>",
		NULL
	};

	if (about != NULL) {
		gtk_widget_show (about);
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	pixbuf = NULL;

	/* FIXME: This should not use gnome-clock! */
	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-clock.png", TRUE, NULL);
	if (!file) {
		g_warning (G_STRLOC ": gnome-clock.png cannot be found");
		pixbuf = gdk_pixbuf_new_from_file (file, NULL);
	}

	about = gnome_about_new (_("Tasklist Applet"), "1.0",
				 _("(c) 2001 Red Hat, Inc"),
				 _("The tasklist applet shows a list of all visible windows and let you browse them."),
				 authors,
				 NULL, /* documenters */
				 NULL, /* translator_credits */
				 pixbuf);
	
	gtk_window_set_wmclass (GTK_WINDOW (about), "tasklist", "Tasklist");

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
	panel_applet_gconf_set_bool (PANEL_APPLET (tasklist->applet),
				     "group_windows",
				     gtk_toggle_button_get_active (button),
				     NULL);
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


#define WID(s) glade_xml_get_widget (xml, s)

static void
setup_dialog (GladeXML     *xml,
	      TasklistData *tasklist)
{
	GConfClient *client;
	
	client = gconf_client_get_default ();

	tasklist->show_current_radio = WID ("show_current_radio");
	tasklist->show_all_radio = WID ("show_all_radio");
	tasklist->group_windows_toggle = WID ("group_windows_toggle");
	tasklist->move_minimized_radio = WID ("move_minimized_radio");
	tasklist->change_workspace_radio = WID ("change_workspace_radio");

	/* Window grouping: */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->group_windows_toggle),
				      tasklist->enable_grouping);
	g_signal_connect (G_OBJECT (tasklist->group_windows_toggle), "toggled",
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
				  (GCallback) gtk_widget_hide, tasklist->properties_dialog);

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

		setup_dialog (xml, tasklist);
		
		g_object_unref (G_OBJECT (xml));
	}

	gtk_window_present (GTK_WINDOW (tasklist->properties_dialog));
}

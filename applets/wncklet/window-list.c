/*
 * libwnck based tasklist applet.
 * (C) 2001 Red Hat, Inc
 * (C) 2001 Alexander Larsson 
 *
 * Authors: Alexander Larsson
 *
 */

#define WNCK_I_KNOW_THIS_IS_UNSTABLE 1

#include <panel-applet.h>

#include <gtk/gtk.h>
#include <libbonobo.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libwnck/libwnck.h>

#include "tasklist.h"

typedef struct {
	GtkWidget *applet;

	GtkWidget *tasklist;
	
	WnckScreen *screen;
	
	GtkOrientation orientation;
	int size;
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
	TasklistData *tasklist = data;
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

gboolean
fill_tasklist_applet(PanelApplet *applet)
{
	TasklistData *tasklist;
	
	tasklist = g_new0 (TasklistData, 1);

	tasklist->applet = GTK_WIDGET (applet);

	/* FIXME: We need to get the real initial panel data here */
	tasklist->size = 48;
	tasklist->orientation = GTK_ORIENTATION_HORIZONTAL;

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
	
	panel_applet_set_expand_flags (PANEL_APPLET (tasklist->applet),
				       TRUE, TRUE);

	gtk_widget_show (tasklist->applet);

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
	
	panel_applet_setup_menu (PANEL_APPLET (tasklist->applet), tasklist_menu_xml, tasklist_menu_verbs, tasklist);
	
	return TRUE;
}


static void 
display_properties_dialog (BonoboUIComponent *uic,
			   TasklistData      *tasklist,
			   const gchar       *verbname)
{
	/* FIXME: Implement this. */ 
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

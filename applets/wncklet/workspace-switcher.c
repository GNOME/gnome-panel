/*
 * libwnck based pager applet.
 * (C) 2001 Alexander Larsson 
 * (C) 2001 Red Hat, Inc
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

#include "pager.h"

typedef struct {
	GtkWidget *applet;

	GtkWidget *frame;
	GtkWidget *pager;
	
	WnckScreen *screen;
	
	GtkOrientation orientation;
	int n_rows;
	int size;
  
} PagerData;

static void display_properties_dialog (BonoboUIComponent *uic,
				       PagerData         *pager,
				       const gchar       *verbname);
static void display_help_dialog       (BonoboUIComponent *uic,
				       PagerData         *pager,
				       const gchar       *verbname);
static void display_about_dialog      (BonoboUIComponent *uic,
				       PagerData         *pager,
				       const gchar       *verbname);

static void
pager_update (PagerData *pager)
{
	int internal_size;

	/* 2 pixels of frame on each side */
	internal_size = pager->size - 2 * 2;
	
	if (pager->orientation == GTK_ORIENTATION_HORIZONTAL) {
		gtk_widget_set_size_request (GTK_WIDGET (pager->pager),
					     -1, internal_size);
	} else {
		gtk_widget_set_size_request (GTK_WIDGET (pager->pager),
					     internal_size, -1);
	}

	wnck_pager_set_orientation (WNCK_PAGER (pager->pager),
				    pager->orientation);
	wnck_pager_set_n_rows (WNCK_PAGER (pager->pager),
			       pager->n_rows);
}

static void
applet_change_orient (PanelApplet       *applet,
		      PanelAppletOrient  orient,
		      PagerData         *pager)
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

	if (new_orient == pager->orientation)
		return;
  
	pager->orientation = new_orient;
	pager_update (pager);
}


static void
applet_change_pixel_size (PanelApplet *applet,
			  gint         size,
			  PagerData   *pager)
{
	if (pager->size == size)
		return;

	pager->size = size;
	pager_update (pager);
}

static void
destroy_pager(GtkWidget * widget, gpointer data)
{
	PagerData *pager = data;
}

static const BonoboUIVerb pager_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("PagerProperties", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("PagerHelp",       display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("PagerAbout",      display_about_dialog),
        BONOBO_UI_VERB_END
};

static const char pager_menu_xml [] =
	"<popup name=\"button3\">\n"
	"   <menuitem name=\"Pager Properties Item\" verb=\"PagerProperties\" _label=\"Properties ...\"\n"
	"             pixtype=\"stock\" pixname=\"gtk-properties\"/>\n"
	"   <menuitem name=\"Pager Help Item\" verb=\"PagerHelp\" _label=\"Help\"\n"
	"             pixtype=\"stock\" pixname=\"gtk-help\"/>\n"
	"   <menuitem name=\"Pager About Item\" verb=\"PagerAbout\" _label=\"About ...\"\n"
	"             pixtype=\"stock\" pixname=\"gnome-stock-about\"/>\n"
	"</popup>\n";

BonoboObject *
make_pager_applet(void)
{
	PagerData *pager;
	
	pager = g_new0 (PagerData, 1);

	/* FIXME: Default value, should be from gconf? */
	pager->n_rows = 2;
	
	/* FIXME: We need to get the real initial panel data here */
	pager->size = 48;
	pager->orientation = GTK_ORIENTATION_HORIZONTAL;

	/* FIXME: Needs to get the screen number from DISPLAY or the panel. */
	pager->screen = wnck_screen_get (0);

	/* because the pager doesn't respond to signals at the moment */
	wnck_screen_force_update (pager->screen);

	pager->frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (pager->frame), GTK_SHADOW_IN);
	
	pager->pager = wnck_pager_new (pager->screen);

	g_signal_connect (G_OBJECT (pager->pager), "destroy",
			  G_CALLBACK (destroy_pager),
			  pager);

	pager_update (pager);
	
	gtk_widget_show (pager->pager);
	gtk_container_add (GTK_CONTAINER (pager->frame), pager->pager);

	gtk_widget_show (pager->frame);
	
	pager->applet = panel_applet_new (pager->frame);
	gtk_widget_show (pager->applet);

	g_signal_connect (G_OBJECT (pager->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient),
			  pager);
	g_signal_connect (G_OBJECT (pager->applet),
			  "change_size",
			  G_CALLBACK (applet_change_pixel_size),
			  pager);
	
	panel_applet_setup_menu (PANEL_APPLET (pager->applet), pager_menu_xml, pager_menu_verbs, pager);
	
	return BONOBO_OBJECT (panel_applet_get_control (PANEL_APPLET (pager->applet)));
}


static void 
display_properties_dialog (BonoboUIComponent *uic,
			   PagerData         *pager,
			   const gchar       *verbname)
{
	/* FIXME: Implement this. Should only need n_rows */ 
}

static void
display_help_dialog (BonoboUIComponent *uic,
		     PagerData         *pager,
		     const gchar       *verbname)
{
	/* FIXME: Implement this */
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      PagerData         *pager,
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

	about = gnome_about_new (_("Pager Applet"), "1.0",
				 _("(c) 2001 Red Hat, Inc."),
				 _("The pager applet shows you a small version of your workspaces that lets you manage the windows."),
				 authors,
				 NULL, /* documenters */
				 NULL, /* translator_credits */
				 pixbuf);
	
	gtk_window_set_wmclass (GTK_WINDOW (about), "pager", "Pager");

	if (pixbuf) {
		gtk_window_set_icon (GTK_WINDOW (about), pixbuf);
		g_object_unref (pixbuf);
	}
	
	g_signal_connect (G_OBJECT(about), "destroy",
			  (GCallback)gtk_widget_destroyed, &about);
	
	gtk_widget_show (about);
}

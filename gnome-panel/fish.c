/* fish.c - The msot useless applet, it's an ugly fish drawing
 * by the least sane person, yes you guessed, George, it's nice
 * to break the monotony of finals
 */

#include "gnome.h"
#include "applet-lib.h"
#include "applet-widget.h"

#include "fish1.xpm"
#include "fish2.xpm"
#include "fish3.xpm"

static GtkWidget *applet;
static GdkPixmap *pix[3];
static GdkPixmap *mask[3];
static gint curpix = 0;

static gint
destroy_applet(GtkWidget *widget, gpointer data)
{
	gtk_exit(0);
	return FALSE;
}

static gint
fish_timeout(gpointer data)
{
	GtkWidget *pixmap = data;

	curpix++;
	if(curpix>=3) curpix=0;
	gtk_pixmap_set(GTK_PIXMAP(pixmap),pix[curpix],mask[curpix]);
	gtk_widget_queue_draw(pixmap);
	return TRUE;
}

static GtkWidget *
create_fish_widget(GtkWidget *window)
{
	GtkWidget *frame;
	GtkWidget *pixmap;
	GtkStyle *style;

	style = gtk_widget_get_style(window);

	pix[0] = gdk_pixmap_create_from_xpm_d(window->window,  &mask[0],
                                              &style->bg[GTK_STATE_NORMAL],
                                              (gchar **)fish1_xpm);
	pix[1] = gdk_pixmap_create_from_xpm_d(window->window,  &mask[1],
                                              &style->bg[GTK_STATE_NORMAL],
                                              (gchar **)fish2_xpm);
	pix[2] = gdk_pixmap_create_from_xpm_d(window->window,  &mask[2],
                                              &style->bg[GTK_STATE_NORMAL],
                                              (gchar **)fish3_xpm);

        pixmap = gtk_pixmap_new(pix[0],mask[0]);
        gtk_widget_show(pixmap);

        curpix = 0;

        gtk_timeout_add(300,fish_timeout,pixmap);

        frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_IN);
        gtk_container_add(GTK_CONTAINER(frame),pixmap);

        return frame;
}

/*the most important dialog in the whole application*/
void
about_cb (AppletWidget *widget, gpointer data)
{
	GtkWidget *about;
	gchar *authors[] = {
	  "Wanda, the fish",
	  "with minor help from George",
	  NULL
	  };

	about = gnome_about_new ( "The GNOME Fish Applet", "3.4.7.4",
			"(C) 1998 the Free Software Foundation",
			authors,
			"This applet has no use what-so-ever. "
			"It only takes up disk space and "
			"compilation time, and if loaded it also "
			"takes up precious panel space and memory. "
			"If anyone is found using this applet, he "
			"should be promptly sent for a psychiatric "
			"evaluation.",
			NULL);
	gtk_widget_show (about);

	return;
}


int
main(int argc, char *argv[])
{
	GtkWidget *fish;

	panel_corba_register_arguments();

	gnome_init("fish_applet", NULL, argc, argv, 0, NULL);

	applet = applet_widget_new(argv[0]);
	if (!applet)
		g_error("Can't create applet!\n");

	gtk_widget_realize(applet);
	fish = create_fish_widget(applet);
	gtk_widget_show(fish);
	applet_widget_add(APPLET_WIDGET(applet), fish);
	gtk_widget_show(applet);
	gtk_signal_connect(GTK_OBJECT(applet),"destroy",
			   GTK_SIGNAL_FUNC(destroy_applet),
			   NULL);

	applet_widget_register_callback(APPLET_WIDGET(applet),
					"about",
					_("About"),
					about_cb,
					NULL);
	
	applet_widget_gtk_main();

	return 0;
}

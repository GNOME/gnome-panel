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

static void 
update_fortune_dialog()
{
  static GtkWidget * dialog = NULL;
  static GtkWidget * less = NULL; 
  GtkWidget * label;

  if ( dialog == NULL ) {
    dialog = 
      gnome_dialog_new(_("Wanda the Fish"), GNOME_STOCK_BUTTON_CLOSE, NULL);
    gnome_dialog_set_close(GNOME_DIALOG(dialog), TRUE);
    gnome_dialog_close_hides(GNOME_DIALOG(dialog), TRUE);

    less = gnome_less_new();
    label = gtk_label_new(_("Wanda the GNOME Fish Says:"));
    
    gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialog)->vbox), label,
		       FALSE, FALSE, GNOME_PAD);

    gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialog)->vbox), less,
		       TRUE, TRUE, GNOME_PAD);

    gnome_less_fixed_font(GNOME_LESS(less));

    gtk_widget_show(less);
    gtk_widget_show(label);
  }
  if (!GTK_WIDGET_VISIBLE(dialog)) gtk_widget_show(dialog);  
  gnome_less_show_command(GNOME_LESS(less), "fortune");
}

static gint 
fish_clicked_cb(GtkWidget * widget, GdkEventButton * e, 
		gpointer data)
{
  if (e->button != 1) {
    /* Ignore buttons 2 and 3 */
    return FALSE; 
  }

  update_fortune_dialog();

  return TRUE; 
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
	GtkWidget *event_box;
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

	event_box = gtk_event_box_new();
	gtk_widget_show(event_box);
	gtk_widget_set_events(event_box, GDK_BUTTON_PRESS_MASK);
	gtk_signal_connect(GTK_OBJECT(event_box), "button_press_event",
			   GTK_SIGNAL_FUNC(fish_clicked_cb), NULL);

        curpix = 0;

        gtk_timeout_add(300,fish_timeout,pixmap);

        frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_IN);
        gtk_container_add(GTK_CONTAINER(event_box),pixmap);
        gtk_container_add(GTK_CONTAINER(frame),event_box);

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

/* fish.c - The msot useless applet, it's an ugly fish drawing
 * by the least sane person, yes you guessed, George, it's nice
 * to break the monotony of finals
 */

#include <string.h>
#include <gnome.h>
#include <gdk_imlib.h>
#include "applet-lib.h"
#include "applet-widget.h"

#include "fish1.xpm"
#include "fish2.xpm"
#include "fish3.xpm"

typedef struct _fish_properties fish_properties;

struct _fish_properties {
  gchar * name;
};

static const fish_properties defaults = {
  "Wanda"
};

static fish_properties properties = { NULL };

static GtkWidget *applet;
static GdkImlibImage *pix[3];
static gint curpix = 0;

static GtkWidget * fortune_dialog = NULL;
static GtkWidget * fortune_label;

static void
load_properties(char *cfgpath)
{
  char *query;

  if (properties.name) g_free(properties.name);

  query = g_copy_strings(cfgpath,"name",NULL);
  properties.name = gnome_config_get_string(query);
  g_free(query);

  if (properties.name == NULL) properties.name = g_strdup(defaults.name);
}

static gchar *
splice_name(const gchar * format, const gchar * name)
{
  gchar * buf;
  gint len;
  len = strlen(name) + strlen(format);
  buf = g_malloc(len+1);
  g_snprintf(buf, len, format, name);
  return buf;
}

static void
apply_properties() 
{
  gchar * tmp;
  const gchar * title_format = _("%s the Fish");
  const gchar * label_format = _("%s the GNOME Fish Says:");

  if ( fortune_dialog != NULL ) { 
    tmp = splice_name(title_format, properties.name);
    gtk_window_set_title(GTK_WINDOW(fortune_dialog), tmp);
    g_free(tmp);
    
    tmp = splice_name(label_format, properties.name);
    gtk_label_set(GTK_LABEL(fortune_label), tmp);
    g_free(tmp);
  }
}

static void
apply_cb(GnomePropertyBox * pb, gint page, gpointer data)
{
  gchar * new_name;

  if (page != -1) return; /* Only honor global apply */

  new_name = gtk_entry_get_text(GTK_ENTRY(data));

  if (new_name) {
    g_free(properties.name);
    properties.name = g_strdup(new_name);
  }

  apply_properties();
}

static void 
properties_dialog(AppletWidget * w, gpointer data)
{
  GtkWidget * pb;
  GtkWidget * vbox;
  GtkWidget * entry;
  GtkWidget * label;

  pb = gnome_property_box_new();

  gtk_window_set_title(GTK_WINDOW(pb), _("GNOME Fish Properties"));

  vbox = gtk_vbox_new(GNOME_PAD, FALSE);
  gtk_container_border_width(GTK_CONTAINER(vbox), GNOME_PAD);

  label = gtk_label_new(_("Your GNOME Fish's Name:"));
  entry = gtk_entry_new();

  gtk_entry_set_text(GTK_ENTRY(entry), properties.name);

  gtk_signal_connect_object(GTK_OBJECT(entry), "changed",
			    GTK_SIGNAL_FUNC(gnome_property_box_changed),
			    GTK_OBJECT(pb));

  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, GNOME_PAD);
  gtk_box_pack_start(GTK_BOX(vbox), entry, TRUE, TRUE, GNOME_PAD);

  gnome_property_box_append_page(GNOME_PROPERTY_BOX(pb), vbox,
				 gtk_label_new(_("Name")));

  gtk_signal_connect(GTK_OBJECT(pb), "apply", GTK_SIGNAL_FUNC(apply_cb),
		     entry);

  gtk_widget_show_all(pb);
}

static void 
update_fortune_dialog()
{
  static GtkWidget * fortune_less = NULL; 

  if ( fortune_dialog == NULL ) {
    fortune_dialog = 
      gnome_dialog_new("", GNOME_STOCK_BUTTON_CLOSE, NULL);
    gnome_dialog_set_close(GNOME_DIALOG(fortune_dialog), TRUE);
    gnome_dialog_close_hides(GNOME_DIALOG(fortune_dialog), TRUE);

    fortune_less = gnome_less_new();
    fortune_label = gtk_label_new("");
    
    gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(fortune_dialog)->vbox), 
		       fortune_label,
		       FALSE, FALSE, GNOME_PAD);

    gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(fortune_dialog)->vbox), 
		       fortune_less,
		       TRUE, TRUE, GNOME_PAD);

    gnome_less_fixed_font(GNOME_LESS(fortune_less));

    gtk_widget_show(fortune_less);
    gtk_widget_show(fortune_label);
    apply_properties();
  }
  if (!GTK_WIDGET_VISIBLE(fortune_dialog)) gtk_widget_show(fortune_dialog);  
  gnome_less_show_command(GNOME_LESS(fortune_less), "fortune");
}

static gint
destroy_applet(GtkWidget *widget, gpointer data)
{
	gtk_exit(0);
	return FALSE;
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
	gtk_pixmap_set(GTK_PIXMAP(pixmap),pix[curpix]->pixmap,
					  pix[curpix]->shape_mask);
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

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	style = gtk_widget_get_style(window);

	pix[0] = gdk_imlib_create_image_from_xpm_data((gchar **)fish1_xpm);
	gdk_imlib_render (pix[0], pix[0]->rgb_width, pix[0]->rgb_height);
	pix[1] = gdk_imlib_create_image_from_xpm_data((gchar **)fish2_xpm);
	gdk_imlib_render (pix[1], pix[1]->rgb_width, pix[1]->rgb_height);
	pix[2] = gdk_imlib_create_image_from_xpm_data((gchar **)fish3_xpm);
	gdk_imlib_render (pix[2], pix[2]->rgb_width, pix[2]->rgb_height);

        pixmap = gtk_pixmap_new(pix[0]->pixmap,pix[0]->shape_mask);
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

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
        return frame;
}

/*the most important dialog in the whole application*/
void
about_cb (AppletWidget *widget, gpointer data)
{
	GtkWidget *about;
	gchar *authors[3];
	const gchar * author_format = _("%s the Fish");

	authors[0] = splice_name(author_format, properties.name);
	authors[1] = "(with minor help from George)";
	authors[2] = NULL;

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

	g_free(authors[0]);

	return;
}

static gint
applet_session_save(GtkWidget *w,
		    const char *cfgpath,
		    const char *globcfgpath)
{
	char *query;

	query = g_copy_strings(cfgpath,"name",NULL);
	gnome_config_set_string(query,properties.name);
	g_free(query);

	gnome_config_sync();
	gnome_config_drop_all();

	return FALSE;
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

	load_properties(APPLET_WIDGET(applet)->cfgpath);

	applet_widget_add(APPLET_WIDGET(applet), fish);
	gtk_widget_show(applet);
	gtk_signal_connect(GTK_OBJECT(applet),"destroy",
			   GTK_SIGNAL_FUNC(destroy_applet),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(applet),"session_save",
			   GTK_SIGNAL_FUNC(applet_session_save),
			   NULL);

	applet_widget_register_callback(APPLET_WIDGET(applet),
					"about",
					_("About..."),
					about_cb,
					NULL);


	applet_widget_register_callback(APPLET_WIDGET(applet),
					"properties",
					_("Properties..."),
					properties_dialog,
					NULL);

	
	applet_widget_gtk_main();

	return 0;
}

/* fish.c - The most useless applet, it's an ugly fish drawing
 * by the least sane person, yes you guessed, George, it's nice
 * to break the monotony of finals
 */

#include <config.h>
#include <string.h>
#include <gnome.h>
#include <gdk_imlib.h>
#include "applet-lib.h"
#include "applet-widget.h"

typedef struct _fish_properties fish_properties;

struct _fish_properties {
	char *name;
	char *image;
	int frames;
	float speed;
};

static fish_properties defaults = {
	"Wanda",
	NULL,
	3,
	0.3
};

static fish_properties properties = { NULL };

static GtkWidget *applet;
static GtkWidget *darea;
static GdkImlibImage *pix=NULL;
static int curpix = 0;
static int timeout_id = 0;

static GtkWidget * fortune_dialog = NULL;
static GtkWidget * fortune_label;

static void
load_properties(char *privcfgpath)
{
	char buf[256];
	if(!defaults.image)
		defaults.image = gnome_unconditional_pixmap_file ("fish/fishanim.png");

	gnome_config_push_prefix(privcfgpath);
	g_free(properties.name);
	g_snprintf(buf,256,"fish/name=%s",defaults.name);
	properties.name = gnome_config_get_string(buf);
	g_free(properties.image);
	g_snprintf(buf,256,"fish/image=%s",defaults.image);
	properties.image = gnome_config_get_string(buf);
	g_snprintf(buf,256,"fish/frames=%d",defaults.frames);
	properties.frames = gnome_config_get_int(buf);
	g_snprintf(buf,256,"fish/speed=%f",defaults.speed);
	properties.speed = gnome_config_get_float(buf);
	gnome_config_pop_prefix();

	if(pix)
		gdk_imlib_destroy_image(pix);
	
	pix = gdk_imlib_load_image(properties.image);
	gdk_imlib_render (pix, pix->rgb_width, pix->rgb_height);
}

static char *
splice_name(const char * format, const char * name)
{
	char * buf;
	int len;
	len = strlen(name) + strlen(format);
	buf = g_malloc(len+1);
	g_snprintf(buf, len, format, name);
	return buf;
}

static void
fish_draw(GtkWidget *darea)
{
	if(!GTK_WIDGET_REALIZED(darea))
		return;
	
	gdk_draw_pixmap(darea->window,
			darea->style->fg_gc[GTK_WIDGET_STATE(darea)],
			pix->pixmap,
			(pix->rgb_width*curpix)/properties.frames, 0,
			0, 0,
			-1, -1);
}

static int
fish_timeout(gpointer data)
{
	curpix++;
	if(curpix>=properties.frames) curpix=0;
	fish_draw(GTK_WIDGET(data));

	return TRUE;
}

static void
apply_properties(void) 
{
	char * tmp;
	const char * title_format = _("%s the Fish");
	const char * label_format = _("%s the GNOME Fish Says:");

	if (fortune_dialog != NULL) { 
		tmp = splice_name(title_format, properties.name);
		gtk_window_set_title(GTK_WINDOW(fortune_dialog), tmp);
		g_free(tmp);

		tmp = splice_name(label_format, properties.name);
		gtk_label_set(GTK_LABEL(fortune_label), tmp);
		g_free(tmp);
	}
	
	if(pix)
		gdk_imlib_destroy_image(pix);
	
	pix = gdk_imlib_load_image(properties.image);
	gdk_imlib_render (pix, pix->rgb_width, pix->rgb_height);

	gtk_drawing_area_size(GTK_DRAWING_AREA(darea),
			      pix->rgb_width/properties.frames,
			      pix->rgb_height);

	gtk_widget_set_usize(darea,
			     pix->rgb_width/properties.frames,
			     pix->rgb_height);

	if(timeout_id)
		gtk_timeout_remove(timeout_id);
        timeout_id = gtk_timeout_add(properties.speed*1000,
				     fish_timeout,darea);
}

static void
apply_cb(GnomePropertyBox * pb, int page, gpointer data)
{
	char *s;
	GtkWidget *name = gtk_object_get_data(GTK_OBJECT(pb),
					      "name");
	GtkWidget *image = gtk_object_get_data(GTK_OBJECT(pb),
					       "image");
	GtkAdjustment *frames =
		gtk_object_get_data(GTK_OBJECT(pb), "frames");
	GtkAdjustment *speed =
		gtk_object_get_data(GTK_OBJECT(pb), "speed");

	if (page != -1) return; /* Only honor global apply */

	s = gtk_entry_get_text(GTK_ENTRY(name));
	if (s) {
		g_free(properties.name);
		properties.name = g_strdup(s);
	}
	s = gtk_entry_get_text(GTK_ENTRY(image));
	if (s) {
		g_free(properties.image);
		properties.image = g_strdup(s);
	}
	properties.frames = frames->value;
	properties.speed = speed->value;

	apply_properties();
}

static void
close_cb(GnomePropertyBox * pb, gpointer data)
{
	GtkWidget *name = gtk_object_get_data(GTK_OBJECT(pb),
					      "name");
	GtkWidget *image = gtk_object_get_data(GTK_OBJECT(pb),
					       "image");
	gtk_signal_disconnect_by_data(GTK_OBJECT(name),pb);
	gtk_signal_disconnect_by_data(GTK_OBJECT(image),pb);
}

static void 
properties_dialog(AppletWidget *aw, gpointer data)
{
	GtkWidget *pb;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *w;
	GtkWidget *e;
	GtkAdjustment *adj;

	pb = gnome_property_box_new();

	gtk_window_set_title(GTK_WINDOW(pb), _("GNOME Fish Properties"));

	vbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_border_width(GTK_CONTAINER(vbox), GNOME_PAD);

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("Your GNOME Fish's Name:"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	e = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(e), properties.name);
	gtk_box_pack_start(GTK_BOX(hbox), e, TRUE, TRUE, 0);
	gtk_object_set_data(GTK_OBJECT(pb),"name",e);

	gtk_signal_connect_object(GTK_OBJECT(e), "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(pb));

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("The Animation Filename:"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	w = gnome_file_entry_new("fish_animation",_("Browse"));
	gtk_box_pack_start(GTK_BOX(hbox),w,TRUE,TRUE,0);
	e = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
	gtk_entry_set_text(GTK_ENTRY(e), properties.image);
	gtk_object_set_data(GTK_OBJECT(pb),"image",e);

	gtk_signal_connect_object(GTK_OBJECT(e), "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(pb));

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("Frames In Animation:"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *) gtk_adjustment_new (properties.frames,
						    0.0, 255.0, 1.0, 5.0, 0.0);
	w = gtk_spin_button_new(adj,0,0);
	gtk_widget_set_usize(w,70,0);
	gtk_box_pack_start(GTK_BOX(hbox),w,FALSE,FALSE,0);
	gtk_object_set_data(GTK_OBJECT(pb),"frames",adj);

	gtk_signal_connect_object(GTK_OBJECT(adj), "value_changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(pb));

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("Pause per frame (s):"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *) gtk_adjustment_new (properties.speed,
						    0.1, 10.0, 0.1, 1.0, 0.0);
	w = gtk_spin_button_new(adj,0,2);
	gtk_widget_set_usize(w,70,0);
	gtk_box_pack_start(GTK_BOX(hbox),w,FALSE,FALSE,0);
	gtk_object_set_data(GTK_OBJECT(pb),"speed",adj);

	gtk_signal_connect_object(GTK_OBJECT(adj), "value_changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(pb));

	gnome_property_box_append_page(GNOME_PROPERTY_BOX(pb), vbox,
				       gtk_label_new(_("Fish")));

	gtk_signal_connect(GTK_OBJECT(pb), "apply",
			   GTK_SIGNAL_FUNC(apply_cb),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(pb), "close",
			   GTK_SIGNAL_FUNC(close_cb),
			   NULL);

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

		gnome_less_set_fixed_font(GNOME_LESS(fortune_less),TRUE);

		gtk_widget_show(fortune_less);
		gtk_widget_show(fortune_label);
		apply_properties();
	}
	if (!GTK_WIDGET_VISIBLE(fortune_dialog)) gtk_widget_show(fortune_dialog);  
	gnome_less_show_command(GNOME_LESS(fortune_less), g_file_exists("/usr/games/fortune")?"/usr/games/fortune":"fortune");
}

static int 
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

static int
fish_expose(GtkWidget *darea, GdkEventExpose *event)
{
	gdk_draw_pixmap(darea->window,
			darea->style->fg_gc[GTK_WIDGET_STATE(darea)],
			pix->pixmap,
			((pix->rgb_width*curpix)/properties.frames)+
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
        return FALSE;
}

static GtkWidget *
create_fish_widget(GtkWidget *window)
{
	GtkWidget *frame;
	GtkStyle *style;

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	style = gtk_widget_get_style(window);
	
	darea = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(darea),
			      pix->rgb_width/properties.frames,
			      pix->rgb_height);
	gtk_widget_set_events(darea, gtk_widget_get_events(darea) |
			      GDK_BUTTON_PRESS_MASK);
	gtk_signal_connect(GTK_OBJECT(darea), "button_press_event",
			   GTK_SIGNAL_FUNC(fish_clicked_cb), NULL);
	gtk_signal_connect_after(GTK_OBJECT(darea), "realize",
			   GTK_SIGNAL_FUNC(fish_draw), NULL);
	gtk_signal_connect(GTK_OBJECT(darea), "expose_event",
			   GTK_SIGNAL_FUNC(fish_expose), NULL);
        gtk_widget_show(darea);

        curpix = 0;

        timeout_id = gtk_timeout_add(properties.speed*1000,
				     fish_timeout,darea);

        frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_IN);
        gtk_container_add(GTK_CONTAINER(frame),darea);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
        return frame;
}

/*the most important dialog in the whole application*/
void
about_cb (AppletWidget *widget, gpointer data)
{
	GtkWidget *about;
	char *authors[3];
	const char * author_format = _("%s the Fish");

	authors[0] = splice_name(author_format, properties.name);
	authors[1] = _("(with minor help from George)");
	authors[2] = NULL;

	about = gnome_about_new (_("The GNOME Fish Applet"), "3.4.7.4",
			"(C) 1998 the Free Software Foundation",
			(const char **)authors,
			_("This applet has no use what-so-ever. "
			  "It only takes up disk space and "
			  "compilation time, and if loaded it also "
			  "takes up precious panel space and memory. "
			  "If anyone is found using this applet, he "
			  "should be promptly sent for a psychiatric "
			  "evaluation."),
			NULL);
	gtk_widget_show (about);

	g_free(authors[0]);

	return;
}

static int
applet_save_session(GtkWidget *w,
		    const char *privcfgpath,
		    const char *globcfgpath)
{
	gnome_config_push_prefix(privcfgpath);
	gnome_config_set_string("fish/name",properties.name);
	gnome_config_set_string("fish/image",properties.image);
	gnome_config_set_int("fish/frames",properties.frames);
	gnome_config_set_float("fish/speed",properties.speed);
	gnome_config_pop_prefix();

	gnome_config_sync();
	gnome_config_drop_all();

	return FALSE;
}



int
main(int argc, char *argv[])
{
	GtkWidget *fish;

	/* Initialize the i18n stuff */
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	applet_widget_init_defaults("fish_applet", NULL, argc, argv, 0, NULL,
				    argv[0]);

	applet = applet_widget_new();
	if (!applet)
		g_error(_("Can't create applet!\n"));

	load_properties(APPLET_WIDGET(applet)->privcfgpath);

	gtk_widget_realize(applet);
	fish = create_fish_widget(applet);
	gtk_widget_show(fish);

	applet_widget_add(APPLET_WIDGET(applet), fish);
	gtk_widget_show(applet);
	gtk_signal_connect(GTK_OBJECT(applet),"save_session",
			   GTK_SIGNAL_FUNC(applet_save_session),
			   NULL);

	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."),
					      about_cb,
					      NULL);


	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "properties",
					      GNOME_STOCK_MENU_PROP,
					      _("Properties..."),
					      properties_dialog,
					      NULL);

	
	applet_widget_gtk_main();

	return 0;
}

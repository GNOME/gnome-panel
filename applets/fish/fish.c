/* fish.c - The most useless applet, it's an ugly fish drawing
 * by the least sane person, yes you guessed, George, it's nice
 * to break the monotony of finals
 */

#include <config.h>
#include <string.h>
#include <gnome.h>
#include <gdk_imlib.h>
#include <applet-widget.h>

typedef struct _FishProp FishProp;
struct _FishProp {
	char *name;
	char *image;
	int frames;
	float speed;
};

static FishProp defaults = {
	"Wanda",
	NULL,
	3,
	1.0
};

typedef struct _Fish Fish;
struct _Fish {
	FishProp prop;
	GtkWidget *applet;
	GtkWidget *frame;
	GtkWidget *darea;
	GdkPixmap *pix;
	int w,h;
	int curpix;
	int timeout_id;
	GtkWidget * fortune_dialog;
	GtkWidget * fortune_label;
	GtkWidget * fortune_less; 
	GtkWidget * aboutbox;
	GtkWidget * pb;
};

GtkWidget *bah_window = NULL;


static void
load_properties(Fish *fish)
{
	char buf[256];
	GdkImlibImage *pix;
	if(!defaults.image)
		defaults.image = gnome_unconditional_pixmap_file ("fish/fishanim.png");

	gnome_config_push_prefix(APPLET_WIDGET(fish->applet)->privcfgpath);

	g_free(fish->prop.name);
	g_snprintf(buf,256,"fish/name=%s",defaults.name);
	fish->prop.name = gnome_config_get_string(buf);

	g_free(fish->prop.image);
	g_snprintf(buf,256,"fish/image=%s",defaults.image);
	fish->prop.image = gnome_config_get_string(buf);

	g_snprintf(buf,256,"fish/frames=%d",defaults.frames);
	fish->prop.frames = gnome_config_get_int(buf);
	if(fish->prop.frames <= 0) fish->prop.frames = 1;

	g_snprintf(buf,256,"fish/speed=%f",defaults.speed);
	fish->prop.speed = gnome_config_get_float(buf);
	if(fish->prop.speed<0.1) fish->prop.speed = 0.1;
	if(fish->prop.speed>10.0) fish->prop.speed = 10.0;

	gnome_config_pop_prefix();

	if(fish->pix)
		gdk_pixmap_unref(fish->pix);
	
	pix = gdk_imlib_load_image(fish->prop.image);
	if(pix) {
		gdk_imlib_render (pix, pix->rgb_width,
				  pix->rgb_height);
		fish->w = pix->rgb_width;
		fish->h = pix->rgb_height;
		fish->pix = gdk_imlib_move_image(pix);
		gdk_imlib_destroy_image(pix);
	} else {
		g_assert(bah_window);
		g_assert(bah_window->window);
		fish->w = fish->prop.frames*48;
		fish->h = 48;
		fish->pix = gdk_pixmap_new(bah_window->window,
					   fish->w,fish->h,-1);
	}
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
fish_draw(GtkWidget *darea, Fish *fish)
{
	if(!GTK_WIDGET_REALIZED(fish->darea))
		return;
	
	gdk_draw_pixmap(fish->darea->window,
			fish->darea->style->fg_gc[GTK_WIDGET_STATE(fish->darea)],
			fish->pix,
			(fish->w*fish->curpix)/fish->prop.frames,
			0, 0, 0, -1, -1);
}

static int
fish_timeout(gpointer data)
{
	Fish *fish = data;
	fish->curpix++;
	if(fish->curpix>=fish->prop.frames) fish->curpix=0;
	fish_draw(fish->darea,fish);

	return TRUE;
}

static void
apply_properties(Fish *fish) 
{
	char * tmp;
	/* xgettext:no-c-format */
	const char * title_format = _("%s the Fish");
	const char * label_format = _("%s the GNOME Fish Says:");
	GdkImlibImage *pix;

	if (fish->fortune_dialog != NULL) { 
		tmp = splice_name(title_format, fish->prop.name);
		gtk_window_set_title(GTK_WINDOW(fish->fortune_dialog), tmp);
		g_free(tmp);

		tmp = splice_name(label_format, fish->prop.name);
		gtk_label_set_text(GTK_LABEL(fish->fortune_label), tmp);
		g_free(tmp);
	}
	
	if(fish->pix)
		gdk_pixmap_unref(fish->pix);
	
	pix = gdk_imlib_load_image(fish->prop.image);
	gdk_imlib_render (pix, pix->rgb_width,
			  pix->rgb_height);
	fish->w = pix->rgb_width;
	fish->h = pix->rgb_height;
	fish->pix = gdk_imlib_move_image(pix);
	gdk_imlib_destroy_image(pix);

	gtk_drawing_area_size(GTK_DRAWING_AREA(fish->darea),
			      fish->w/fish->prop.frames, fish->h);

	gtk_widget_set_usize(fish->darea, fish->w/fish->prop.frames, fish->h);

	if(fish->timeout_id)
		gtk_timeout_remove(fish->timeout_id);
        fish->timeout_id = gtk_timeout_add(fish->prop.speed*1000,
					   fish_timeout,fish);
	fish->curpix = 0;
}

static void
apply_cb(GnomePropertyBox * pb, int page, Fish *fish)
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
		g_free(fish->prop.name);
		fish->prop.name = g_strdup(s);
	}
	s = gnome_pixmap_entry_get_filename(GNOME_PIXMAP_ENTRY(image));
	if (s) {
		g_free(fish->prop.image);
		fish->prop.image = g_strdup(s);
	}
	fish->prop.frames = frames->value;
	fish->prop.speed = speed->value;

	apply_properties(fish);
}

static void 
properties_dialog(AppletWidget *aw, gpointer data)
{
        static GnomeHelpMenuEntry help_entry = { "fish_applet", "properties" };
	Fish *fish = data;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *w;
	GtkWidget *e;
	GtkAdjustment *adj;

	if(fish->pb) {
		gtk_widget_show(fish->pb);
		gdk_window_raise(fish->pb->window);
		return;
	}

	fish->pb = gnome_property_box_new();

	gtk_window_set_title(GTK_WINDOW(fish->pb), _("GNOME Fish Properties"));

	vbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), GNOME_PAD);

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("Your GNOME Fish's Name:"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	e = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(e), fish->prop.name);
	gtk_box_pack_start(GTK_BOX(hbox), e, TRUE, TRUE, 0);
	gtk_object_set_data(GTK_OBJECT(fish->pb),"name",e);

	gtk_signal_connect_object_while_alive(GTK_OBJECT(e), "changed",
					      GTK_SIGNAL_FUNC(gnome_property_box_changed),
					      GTK_OBJECT(fish->pb));

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("The Animation Filename:"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	w = gnome_pixmap_entry_new("fish_animation",_("Browse"),TRUE);
	gtk_box_pack_start(GTK_BOX(hbox),w,TRUE,TRUE,0);
	e = gnome_pixmap_entry_gtk_entry (GNOME_PIXMAP_ENTRY (w));
	gtk_entry_set_text(GTK_ENTRY(e), fish->prop.image);
	gtk_object_set_data(GTK_OBJECT(fish->pb),"image",w);

	gtk_signal_connect_object_while_alive(GTK_OBJECT(e), "changed",
					      GTK_SIGNAL_FUNC(gnome_property_box_changed),
					      GTK_OBJECT(fish->pb));

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("Frames In Animation:"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *) gtk_adjustment_new (fish->prop.frames,
						    1.0, 255.0, 1.0, 5.0, 0.0);
	w = gtk_spin_button_new(adj,0,0);
	gtk_widget_set_usize(w,70,0);
	gtk_box_pack_start(GTK_BOX(hbox),w,FALSE,FALSE,0);
	gtk_object_set_data(GTK_OBJECT(fish->pb),"frames",adj);

	gtk_signal_connect_object(GTK_OBJECT(adj), "value_changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(fish->pb));

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("Pause per frame (s):"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *) gtk_adjustment_new (fish->prop.speed,
						    0.1, 10.0, 0.1, 1.0, 0.0);
	w = gtk_spin_button_new(adj,0,2);
	gtk_widget_set_usize(w,70,0);
	gtk_box_pack_start(GTK_BOX(hbox),w,FALSE,FALSE,0);
	gtk_object_set_data(GTK_OBJECT(fish->pb),"speed",adj);

	gtk_signal_connect_object(GTK_OBJECT(adj), "value_changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(fish->pb));

	gnome_property_box_append_page(GNOME_PROPERTY_BOX(fish->pb), vbox,
				       gtk_label_new(_("Fish")));

	gtk_signal_connect(GTK_OBJECT(fish->pb), "apply",
			   GTK_SIGNAL_FUNC(apply_cb),fish);
	gtk_signal_connect(GTK_OBJECT(fish->pb), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed),&fish->pb);
	gtk_signal_connect(GTK_OBJECT(fish->pb), "help",
			   GTK_SIGNAL_FUNC(gnome_help_pbox_display),
			   &help_entry);

	gtk_widget_show_all(fish->pb);
}

static void 
update_fortune_dialog(Fish *fish)
{
        gboolean fortune_exists;

	if ( fish->fortune_dialog == NULL ) {
		fish->fortune_dialog = 
			gnome_dialog_new("", GNOME_STOCK_BUTTON_CLOSE, NULL);
		gnome_dialog_set_close(GNOME_DIALOG(fish->fortune_dialog),
				       TRUE);
		gnome_dialog_close_hides(GNOME_DIALOG(fish->fortune_dialog),
					 TRUE);

		fish->fortune_less = gnome_less_new();
		fish->fortune_label = gtk_label_new("");

		gnome_less_set_fixed_font(GNOME_LESS(fish->fortune_less),TRUE);
		
		{
			int i;
			char buf[82];
			GtkWidget *text = GTK_WIDGET(GNOME_LESS(fish->fortune_less)->text);
			GdkFont *font = GNOME_LESS(fish->fortune_less)->font;
			for(i=0;i<81;i++) buf[i]='X';
			buf[i]='\0';
			gtk_widget_set_usize(text,
					     gdk_string_width(font,buf)+30,
					     gdk_string_height(font,buf)*24+30);
		}

		gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(fish->fortune_dialog)->vbox), 
				   fish->fortune_label,
				   FALSE, FALSE, GNOME_PAD);

		gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(fish->fortune_dialog)->vbox), 
				   fish->fortune_less,
				   TRUE, TRUE, GNOME_PAD);

		gtk_widget_show(fish->fortune_less);
		gtk_widget_show(fish->fortune_label);
		apply_properties(fish);
	}
	if (!GTK_WIDGET_VISIBLE(fish->fortune_dialog))
		gtk_widget_show(fish->fortune_dialog);

	fortune_exists = g_file_exists("/usr/games/fortune") || 
	                 (gnome_is_program_in_path("fortune") != NULL);

	if (fortune_exists)
                gnome_less_show_command(GNOME_LESS(fish->fortune_less),
					g_file_exists("/usr/games/fortune")?
					"/usr/games/fortune":"fortune");
	else
                gnome_less_show_string(GNOME_LESS(fish->fortune_less),
				       "You do not have fortune installed.");
}

static int 
fish_clicked_cb(GtkWidget * widget, GdkEventButton * e, Fish *fish)
{
	if (e->button != 1) {
		/* Ignore buttons 2 and 3 */
		return FALSE; 
	}

	update_fortune_dialog(fish);

	return TRUE; 
}

static int
fish_expose(GtkWidget *darea, GdkEventExpose *event, Fish *fish)
{
	gdk_draw_pixmap(fish->darea->window,
			fish->darea->style->fg_gc[GTK_WIDGET_STATE(fish->darea)],
			fish->pix,
			((fish->w*fish->curpix)/fish->prop.frames)+
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
        return FALSE;
}

static void
create_fish_widget(Fish *fish)
{
	GtkStyle *style;

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	style = gtk_widget_get_style(fish->applet);
	
	fish->darea = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(fish->darea),
			      fish->w/fish->prop.frames, fish->h);
	gtk_widget_set_events(fish->darea, gtk_widget_get_events(fish->darea) |
			      GDK_BUTTON_PRESS_MASK);
	gtk_signal_connect(GTK_OBJECT(fish->darea), "button_press_event",
			   GTK_SIGNAL_FUNC(fish_clicked_cb), fish);
	gtk_signal_connect_after(GTK_OBJECT(fish->darea), "realize",
				 GTK_SIGNAL_FUNC(fish_draw), fish);
	gtk_signal_connect(GTK_OBJECT(fish->darea), "expose_event",
			   GTK_SIGNAL_FUNC(fish_expose), fish);
        gtk_widget_show(fish->darea);

        fish->curpix = 0;

        fish->timeout_id = gtk_timeout_add(fish->prop.speed*1000,
					   fish_timeout,fish);

        fish->frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(fish->frame),GTK_SHADOW_IN);
        gtk_container_add(GTK_CONTAINER(fish->frame),fish->darea);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
}

/*the most important dialog in the whole application*/
static void
about_cb (AppletWidget *widget, gpointer data)
{
	Fish *fish = data;
	char *authors[3];
	const char * author_format = _("%s the Fish");

	if(fish->aboutbox) {
		gtk_widget_show(fish->aboutbox);
		gdk_window_raise(fish->aboutbox->window);
		return;
	}

	authors[0] = splice_name(author_format, fish->prop.name);
	authors[1] = _("(with minor help from George)");
	authors[2] = NULL;

	fish->aboutbox =
		gnome_about_new (_("The GNOME Fish Applet"), "3.4.7.4",
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
	gtk_signal_connect(GTK_OBJECT(fish->aboutbox),"destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed), &fish->aboutbox);
	gtk_widget_show (fish->aboutbox);

	g_free(authors[0]);
}

static int
applet_save_session(GtkWidget *w,
		    const char *privcfgpath,
		    const char *globcfgpath,
		    Fish *fish)
{
	gnome_config_push_prefix(privcfgpath);
	gnome_config_set_string("fish/name",fish->prop.name);
	gnome_config_set_string("fish/image",fish->prop.image);
	gnome_config_set_int("fish/frames",fish->prop.frames);
	gnome_config_set_float("fish/speed",fish->prop.speed);
	gnome_config_pop_prefix();

	gnome_config_sync();
	gnome_config_drop_all();

	return FALSE;
}

static void
applet_destroy(GtkWidget *applet,Fish *fish)
{
	g_free(fish->prop.name);
	g_free(fish->prop.image);

	if(fish->pix)
		gdk_pixmap_unref(fish->pix);
	if(fish->timeout_id)
		gtk_timeout_remove(fish->timeout_id);
	if(fish->fortune_dialog)
		gtk_widget_destroy(fish->fortune_dialog);
	if(fish->aboutbox)
		gtk_widget_destroy(fish->aboutbox);
	if(fish->pb)
		gtk_widget_destroy(fish->pb);
	g_free(fish);
}
	

static CORBA_Object
wanda_activator(PortableServer_POA poa,
		const char *goad_id,
		const char **params,
		gpointer *impl_ptr,
		CORBA_Environment *ev)
{
  Fish *fish;

  fish = g_new0(Fish,1);
  
  fish->applet = applet_widget_new(goad_id);
  
  bah_window = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_widget_set_uposition(bah_window,gdk_screen_width()+1,
			   gdk_screen_height()+1);
  gtk_widget_show_now(bah_window);
  
  load_properties(fish);

  /*gtk_widget_realize(applet);*/
  create_fish_widget(fish);
  gtk_widget_show(fish->frame);
  applet_widget_add(APPLET_WIDGET(fish->applet), fish->frame);
  gtk_widget_show(fish->applet);

  gtk_signal_connect(GTK_OBJECT(fish->applet),"save_session",
		     GTK_SIGNAL_FUNC(applet_save_session),fish);
  gtk_signal_connect(GTK_OBJECT(fish->applet),"destroy",
		     GTK_SIGNAL_FUNC(applet_destroy),fish);

  applet_widget_register_stock_callback(APPLET_WIDGET(fish->applet),
					"about",
					GNOME_STOCK_MENU_ABOUT,
					_("About..."),
					about_cb,
					fish);


  applet_widget_register_stock_callback(APPLET_WIDGET(fish->applet),
					"properties",
					GNOME_STOCK_MENU_PROP,
					_("Properties..."),
					properties_dialog,
					fish);

  return applet_widget_corba_activate(fish->applet, poa, goad_id, params,
				      impl_ptr, ev);
}

static void
wanda_deactivator(PortableServer_POA poa,
		  const char *goad_id,
		  gpointer impl_ptr,
		  CORBA_Environment *ev)
{
  applet_widget_corba_deactivate(poa, goad_id, impl_ptr, ev);
}

#if 1 || defined(SHLIB_APPLETS)
static const char *repo_id[]={"IDL:GNOME/Applet:1.0", NULL};
static GnomePluginObject applets_list[] = {
  {repo_id, "fish_applet", NULL, "Wanda the Magnificient",
   &wanda_activator, &wanda_deactivator},
  {NULL}
};

GnomePlugin GNOME_Plugin_info = {
  applets_list, NULL
};
#else
int
main(int argc, char *argv[])
{
        gpointer wanda_impl;

	/* Initialize the i18n stuff */
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	applet_widget_init("fish_applet", VERSION, argc, argv, NULL, 0, NULL);

	APPLET_ACTIVATE(wanda_activator, "fish_applet", &wanda_impl);
	
	applet_widget_gtk_main();

	APPLET_DEACTIVATE(wanda_deactivator, "fish_applet", wanda_impl);

	return 0;
}
#endif

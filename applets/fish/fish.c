/* fish.c - The most useless applet, it's an ugly fish drawing
 * by the least sane person, yes you guessed, George, it's nice
 * to break the monotony of finals
 */

#include <config.h>
#include <string.h>
#include <time.h>
#include <gnome.h>
#include <applet-widget.h>

#include <libgnomeui/gnome-window-icon.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_alphagamma.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libart_lgpl/art_pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>

#include <unistd.h>

/* macro to always pass a non-null string */
#define sure_string(__x) ((__x)!=NULL?(__x):"")

int fools_day=1, fools_month=3;

typedef struct _FishProp FishProp;
struct _FishProp {
	char *name;
	char *image;
	int frames;
	float speed;
	gboolean rotate;
	char *command;
};

static FishProp defaults = {
	"Wanda", /*name*/
	NULL, /*image*/
	3, /*frames*/
	1.0, /*speed*/
	TRUE, /*rotate*/
	NULL /* command */
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
	gboolean april_fools;
	GtkWidget * fortune_dialog;
	GtkWidget * fortune_label;
	GtkWidget * fortune_less; 
	GtkWidget * aboutbox;
	GtkWidget * pb;
	int size;
	PanelOrientType orient;
};

#define IS_ROT(f) ((f)->prop.rotate && ((f)->orient == ORIENT_LEFT || (f)->orient == ORIENT_RIGHT))

GtkWidget *bah_window = NULL;

static gchar *
get_location(void)
{
	static gchar location[256];
	FILE *zone;
	
	/* Now we try old method. Works for glibc < 2.2 */
	zone = fopen("/etc/timezone", "r");
	if (zone != NULL) {
		fscanf(zone, "%255s", location);
		fclose(zone);
		return location;
	} else { /* We try new method for glibc 2.2 */
		gchar buf[256];
		int i, len, count;
		
		len = readlink("/etc/localtime", buf, sizeof(buf));
		if (len <= 0)
			return NULL;

		for (i = len, count = 0; (i > 0) && (count != 2); i--)
			if (buf[i] == '/')
				count++;

		if (count != 2)
			return NULL;

		memcpy(location, &buf[i + 2], len - i - 2);
		return location;
	}
	
	return NULL;
}

static void 
set_wanda_day(void)
{
	const gchar *spanish_timezones[] = {
		"Europe/Madrid",
		"Africa/Ceuta",
		"Atlantic/Canary",
		"America/Mexico_City",
		"Mexico/BajaSur",
		"Mexico/BajaNorte",
		"Mexico/General",
		NULL
	};
	gchar *location = get_location();
	int i;
	gboolean found = FALSE;
	
	if (location == NULL) /* We couldn't guess our location */
		return;
	
	for (i = 0; !found && spanish_timezones[i]; i++)
		if (!g_strcasecmp(spanish_timezones[i], location)) {
			/* Hah!, We are in Spain ot Mexico */
			/* Spanish fool's day: 28th December */
			fools_day = 28;
			fools_month = 11;
			found = TRUE;
		}
	
}

static void
load_image_file(Fish *fish)
{
	GdkPixbuf *pix;
	int w, h;
	
	if(fish->pix)
		gdk_pixmap_unref(fish->pix);

	g_assert(bah_window);
	g_assert(bah_window->window);
	
	pix = gdk_pixbuf_new_from_file(fish->prop.image);
	if(pix) {
		double affine[6];
		guchar *rgb;
		GdkGC *gc;
		int size;
		gint width, height;
		
		size = fish->size - 4;

		width = gdk_pixbuf_get_width (pix);
		height = gdk_pixbuf_get_height (pix);

		if(ABS(height - size)<6) {
			size = height;
		}

		h = size;
		w = width * ((double) h / height);
		
		affine[1] = affine[2] = affine[4] = affine[5] = 0;

		affine[0] = w / (double) width;
		affine[3] = h / (double) height;

		if (IS_ROT(fish)) {
			int t;
			double tmpaffine[6];
			t = w; w = h; h = t;

			art_affine_rotate(tmpaffine,270);
			art_affine_multiply(affine,affine,tmpaffine);
			art_affine_translate(tmpaffine,0,h);
			art_affine_multiply(affine,affine,tmpaffine);
		}

		if(fish->april_fools) {
			double tmpaffine[6];

			/* rotate the fish */
			art_affine_rotate(tmpaffine,180);
			art_affine_multiply(affine,affine,tmpaffine);
			art_affine_translate(tmpaffine,w,h);
			art_affine_multiply(affine,affine,tmpaffine);
		}
		
		rgb = g_new0(guchar,w*h*3);

		if (gdk_pixbuf_get_has_alpha (pix)) {
			art_rgb_rgba_affine (rgb,
					     0, 0, w, h, w * 3,
					     gdk_pixbuf_get_pixels (pix),
					     width, height, gdk_pixbuf_get_rowstride (pix),
					     affine,
					     ART_FILTER_NEAREST,
					     NULL);
		} else {
			art_rgb_affine (rgb,
					0, 0, w, h, w * 3,
					gdk_pixbuf_get_pixels (pix),
					width, height, gdk_pixbuf_get_rowstride (pix),
					affine,
					ART_FILTER_NEAREST,
					NULL);
		}
		
		gdk_pixbuf_unref(pix);

		fish->w = w;
		fish->h = h;
 		fish->pix = gdk_pixmap_new(bah_window->window,
 					   fish->w,fish->h,-1);
		gc = gdk_gc_new(fish->pix);

		if (fish->april_fools) {
			/* rgb has is packed, thus rowstride is w*3 so
			 * we can do the following.  It makes the whole
			 * image have an ugly industrial waste like tint
			 * to make it obvious that the fish is dead */
			art_rgb_run_alpha(rgb, 255, 128, 0, 70, w*h);
		}

		gdk_draw_rgb_image(fish->pix,gc,
				   0,0, w, h,
				   GDK_RGB_DITHER_NORMAL,
				   rgb, w*3);
		
		g_free(rgb);
	} else {
		if(IS_ROT(fish)) {
			fish->w = fish->size;
			fish->h = fish->prop.frames*fish->size;
		} else {
			fish->w = fish->prop.frames*fish->size;
			fish->h = fish->size;
		}
 		fish->pix = gdk_pixmap_new(bah_window->window,
 					   fish->w,fish->h,-1);
	}
}

static void
setup_size(Fish *fish)
{
	if(IS_ROT(fish)) {
		gtk_drawing_area_size(GTK_DRAWING_AREA(fish->darea), fish->w,
				      fish->h/fish->prop.frames);
		gtk_widget_set_usize(fish->darea, fish->w,
				     fish->h/fish->prop.frames);
	} else {
		gtk_drawing_area_size(GTK_DRAWING_AREA(fish->darea), 
				      fish->w/fish->prop.frames, fish->h);
		gtk_widget_set_usize(fish->darea, fish->w/fish->prop.frames,
				     fish->h);
	}
	gtk_widget_queue_resize(fish->darea);
	gtk_widget_queue_resize(fish->darea->parent);
	gtk_widget_queue_resize(fish->darea->parent->parent);
}

static void
load_properties(Fish *fish)
{
        char buf[4096];

        set_wanda_day(); /* Just to know where we are */

        if (defaults.image == NULL)
		defaults.image = gnome_unconditional_pixmap_file ("fish/fishanim.png");
	if (defaults.command == NULL) {
		defaults.command = gnome_is_program_in_path("fortune");

		/* fallback */
		if (defaults.command == NULL &&
		    g_file_exists ("/usr/games/fortune"))
			defaults.command = "/usr/games/fortune";

		/* uber fallback */
		if (defaults.command == NULL)
			defaults.command = "";
	}

	gnome_config_push_prefix(APPLET_WIDGET(fish->applet)->privcfgpath);

	g_free(fish->prop.name);
	g_snprintf(buf,sizeof(buf),"fish/name=%s",defaults.name);
	fish->prop.name = gnome_config_get_string(buf);

	g_free(fish->prop.image);
	g_snprintf(buf,sizeof(buf),"fish/image=%s",defaults.image);
	fish->prop.image = gnome_config_get_string(buf);

	g_snprintf(buf,sizeof(buf),"fish/frames=%d",defaults.frames);
	fish->prop.frames = gnome_config_get_int(buf);
	if(fish->prop.frames <= 0) fish->prop.frames = 1;

	g_snprintf(buf,sizeof(buf),"fish/speed=%f",defaults.speed);
	fish->prop.speed = gnome_config_get_float(buf);
	if(fish->prop.speed<0.1) fish->prop.speed = 0.1;
	if(fish->prop.speed>10.0) fish->prop.speed = 10.0;

	g_snprintf (buf, sizeof(buf), "fish/rotate=%s",
		    defaults.rotate ? "true" : "false");
	fish->prop.rotate = gnome_config_get_bool (buf);

	g_free (fish->prop.command);
	g_snprintf (buf, sizeof (buf), "fish/command=%s", defaults.command);
	fish->prop.command = gnome_config_get_string (buf);

	gnome_config_pop_prefix ();

	load_image_file (fish);
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
	
	if(IS_ROT(fish))
		gdk_draw_pixmap(fish->darea->window,
				fish->darea->style->fg_gc[GTK_WIDGET_STATE(fish->darea)],
				fish->pix,
				0,
				(fish->h*fish->curpix)/fish->prop.frames,
				0, 0, -1, -1);
	else
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
	time_t ourtime;
	struct tm *tm;

	time(&ourtime);
	tm = localtime(&ourtime);

	if(fish->april_fools) {
		if(tm->tm_mon != fools_month || tm->tm_mday != fools_day) {
			fish->april_fools = FALSE;
			load_image_file(fish);
			setup_size(fish);
			fish_timeout(fish);
		}
	} else {
		if(tm->tm_mon == fools_month && tm->tm_mday == fools_day) {
			fish->april_fools = TRUE;
			load_image_file(fish);
			setup_size(fish);
			fish_timeout(fish);
		}
	}

	/* on april fools, the fish id dead! */
	/* on "Santos Inocentes" (spanish fool's day) the fish is dead */
	if(!fish->april_fools) {
		fish->curpix++;
		if(fish->curpix>=fish->prop.frames) fish->curpix=0;
		fish_draw(fish->darea,fish);
	}

	return TRUE;
}

static void
apply_properties(Fish *fish) 
{
	char * tmp;
	/* xgettext:no-c-format */
	const char * title_format = _("%s the Fish");
	const char * label_format = _("%s the GNOME Fish Says:");

	if (fish->fortune_dialog != NULL) { 
		tmp = splice_name(title_format, fish->prop.name);
		gtk_window_set_title(GTK_WINDOW(fish->fortune_dialog), tmp);
		g_free(tmp);

		tmp = splice_name(label_format, fish->prop.name);
		gtk_label_set_text(GTK_LABEL(fish->fortune_label), tmp);
		g_free(tmp);
	}
	
	load_image_file(fish);

	setup_size(fish);

	if (fish->timeout_id != 0)
		gtk_timeout_remove (fish->timeout_id);
        fish->timeout_id = gtk_timeout_add (fish->prop.speed * 1000,
					    fish_timeout, fish);
	fish->curpix = 0;
	fish_timeout(fish);
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
	GtkWidget *rotate = gtk_object_get_data(GTK_OBJECT(pb),
						"rotate");
	GtkWidget *command = gtk_object_get_data(GTK_OBJECT(pb),
						 "command");

	if (page != -1) return; /* Only honor global apply */

	s = gtk_entry_get_text(GTK_ENTRY(name));
	if (s != NULL) {
		g_free(fish->prop.name);
		fish->prop.name = g_strdup(s);
	}
	s = gnome_pixmap_entry_get_filename(GNOME_PIXMAP_ENTRY(image));
	if (s != NULL) {
		g_free(fish->prop.image);
		fish->prop.image = g_strdup(s);
	}
	fish->prop.frames = frames->value;
	fish->prop.speed = speed->value;
	fish->prop.rotate =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rotate));
	s = gtk_entry_get_text (GTK_ENTRY (command));
	if (s != NULL) {
		g_free (fish->prop.command);
		fish->prop.command = g_strdup (s);

		/* We need more useful commands to warn on here */
		if (strncmp (s, "ps ", 3) == 0 ||
		    strcmp (s, "ps") == 0 ||
		    strncmp (s, "who ", 4) == 0 ||
		    strcmp (s, "who") == 0 ||
		    strcmp (s, "uptime") == 0 ||
		    strncmp (s, "tail ", 5) == 0)
			gnome_warning_dialog
				(_("Warning:  The command appears to be "
				   "something actually useful.\n"
				   "Since this is a useless applet, you "
				   "may not want to do this.\n"
				   "We strongly advise you against "
				   "usage of wanda for anything\n"
				   "which would make the applet "
				   "\"practical\" or useful."));
	}

	apply_properties (fish);
}

static void
phelp_cb (GtkWidget *w, gint tab, gpointer data)
{
	GnomeHelpMenuEntry help_entry = { "fish_applet",
					  "index.html#FISH-PREFS" };
	gnome_help_display (NULL, &help_entry);
}

static void 
properties_dialog (AppletWidget *aw, gpointer data)
{
	Fish *fish = data;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *w;
	GtkWidget *e;
	GtkAdjustment *adj;

	if (fish->pb != NULL) {
		gtk_widget_show(fish->pb);
		gdk_window_raise(fish->pb->window);
		return;
	}

	fish->pb = gnome_property_box_new();
	gtk_window_set_wmclass (GTK_WINDOW (fish->pb), "fish", "Fish");
	gtk_window_set_title(GTK_WINDOW(fish->pb), _("GNOME Fish Properties"));
	gnome_window_icon_set_from_file (GTK_WINDOW (fish->pb),
					 GNOME_ICONDIR"/gnome-fish.png");

	vbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), GNOME_PAD);

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("Your GNOME Fish's Name:"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	e = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(e), sure_string (fish->prop.name));
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
	gtk_entry_set_text(GTK_ENTRY(e), sure_string (fish->prop.image));
	gtk_object_set_data(GTK_OBJECT(fish->pb),"image",w);

	gtk_signal_connect_object_while_alive(GTK_OBJECT(e), "changed",
					      GTK_SIGNAL_FUNC(gnome_property_box_changed),
					      GTK_OBJECT(fish->pb));

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new(_("Command to execute when fish is clicked:"));
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	e = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(e), sure_string (fish->prop.command));
	gtk_box_pack_start(GTK_BOX(hbox), e, TRUE, TRUE, 0);
	gtk_object_set_data(GTK_OBJECT(fish->pb),"command",e);

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

	w = gtk_check_button_new_with_label(_("Rotate on vertical panels"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),fish->prop.rotate);
	gtk_signal_connect_object(GTK_OBJECT(w), "toggled",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(fish->pb));
	gtk_object_set_data(GTK_OBJECT(fish->pb),"rotate",w);
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, FALSE, 0);

	gnome_property_box_append_page(GNOME_PROPERTY_BOX(fish->pb), vbox,
				       gtk_label_new(_("Fish")));

	gtk_signal_connect(GTK_OBJECT(fish->pb), "apply",
			   GTK_SIGNAL_FUNC(apply_cb),fish);
	gtk_signal_connect(GTK_OBJECT(fish->pb), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed),&fish->pb);
	gtk_signal_connect(GTK_OBJECT(fish->pb), "help",
			   GTK_SIGNAL_FUNC(phelp_cb), NULL);

	gtk_widget_show_all(fish->pb);
}

static void 
update_fortune_dialog(Fish *fish)
{
	char *fortune_command;

	if ( fish->fortune_dialog == NULL ) {
		fish->fortune_dialog = 
			gnome_dialog_new("", GNOME_STOCK_BUTTON_CLOSE, NULL);
		gnome_dialog_set_close(GNOME_DIALOG(fish->fortune_dialog),
				       TRUE);
		gnome_dialog_close_hides(GNOME_DIALOG(fish->fortune_dialog),
					 TRUE);
		gtk_window_set_wmclass (GTK_WINDOW (fish->fortune_dialog), "fish", "Fish");
		gnome_window_icon_set_from_file (GTK_WINDOW (fish->fortune_dialog),
						 GNOME_ICONDIR"/gnome-fish.png");

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

		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (fish->fortune_dialog)->vbox), 
				    fish->fortune_label,
				    FALSE, FALSE, GNOME_PAD);

		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (fish->fortune_dialog)->vbox), 
				    fish->fortune_less,
				    TRUE, TRUE, GNOME_PAD);

		gtk_widget_show (fish->fortune_less);
		gtk_widget_show (fish->fortune_label);
		apply_properties (fish);
	}

	gtk_widget_show (fish->fortune_dialog);

	fortune_command = g_file_exists("/usr/games/fortune")?
		g_strdup("/usr/games/fortune"):
		gnome_is_program_in_path("fortune");

	if (fish->prop.command != NULL &&
	    fish->prop.command[0] != '\0') {
                gnome_less_show_command (GNOME_LESS(fish->fortune_less),
					 fish->prop.command);
	} else {
                gnome_less_show_string (GNOME_LESS(fish->fortune_less),
					_("You do not have fortune installed "
					  "or you have not specified a program "
					  "to run.\n\nPlease refer to fish "
					  "properties dialog."));
	}
}

static gboolean 
fish_clicked_cb (GtkWidget * widget, GdkEventButton * e, Fish *fish)
{
	if (e->button != 1) {
		/* Ignore buttons 2 and 3 */
		return FALSE; 
	}

	/* on 1st of april the fish is dead damnit */
	if (fish->april_fools) {
		gnome_ok_dialog(_("The water needs changing!\n"
				  "(Look at today's date)"));
		return TRUE;
	}

	update_fortune_dialog(fish);

	return TRUE; 
}

static int
fish_expose (GtkWidget *darea, GdkEventExpose *event, Fish *fish)
{
	if(IS_ROT(fish)) {
		gdk_draw_pixmap(fish->darea->window,
				fish->darea->style->fg_gc[GTK_WIDGET_STATE(fish->darea)],
				fish->pix,
				event->area.x,
				((fish->h*fish->curpix)/fish->prop.frames)+
				event->area.y,
				event->area.x, event->area.y,
				event->area.width, event->area.height);
	} else {
		gdk_draw_pixmap(fish->darea->window,
				fish->darea->style->fg_gc[GTK_WIDGET_STATE(fish->darea)],
				fish->pix,
				((fish->w*fish->curpix)/fish->prop.frames)+
				event->area.x, event->area.y,
				event->area.x, event->area.y,
				event->area.width, event->area.height);
	}
        return FALSE;
}

static void
create_fish_widget(Fish *fish)
{
	GtkStyle *style;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	style = gtk_widget_get_style(fish->applet);
	
	fish->darea = gtk_drawing_area_new();
	if(IS_ROT(fish)) {
		gtk_drawing_area_size(GTK_DRAWING_AREA(fish->darea), fish->w,
				      fish->h/fish->prop.frames);
	} else {
		gtk_drawing_area_size(GTK_DRAWING_AREA(fish->darea),
				      fish->w/fish->prop.frames, fish->h);
	}
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

static void
help_cb (AppletWidget *widget, gpointer data)
{
	GnomeHelpMenuEntry help_ref = { "fish_applet", "index.html"};
	gnome_help_display (NULL, &help_ref);
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
		gnome_about_new (_("The GNOME Fish Applet"), "3.4.7.4ac19",
				 "(C) 1998 the Free Software Foundation",
				 (const char **)authors,
				 _("This applet has no use what-so-ever. "
				   "It only takes up disk space and "
				   "compilation time, and if loaded it also "
				   "takes up precious panel space and memory. "
				   "If anyone is found using this applet, he "
				   "should be promptly sent for a psychiatric "
				   "evaluation."),
				 GNOME_ICONDIR"/gnome-fish.png");
	gtk_window_set_wmclass (GTK_WINDOW (fish->aboutbox), "fish", "Fish");
	gnome_window_icon_set_from_file (GTK_WINDOW (fish->aboutbox),
					 GNOME_ICONDIR"/gnome-fish.png");
	gtk_signal_connect (GTK_OBJECT (fish->aboutbox), "destroy",
			    GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			    &fish->aboutbox);
	gtk_widget_show (fish->aboutbox);

	g_free (authors[0]);
}

static int
applet_save_session (GtkWidget *w,
		     const char *privcfgpath,
		     const char *globcfgpath,
		     Fish *fish)
{
	gnome_config_push_prefix (privcfgpath);
	gnome_config_set_string ("fish/name",fish->prop.name);
	gnome_config_set_string ("fish/image",fish->prop.image);
	gnome_config_set_int ("fish/frames",fish->prop.frames);
	gnome_config_set_float ("fish/speed",fish->prop.speed);
	gnome_config_set_bool ("fish/rotate",fish->prop.rotate);
	gnome_config_set_string ("fish/command",fish->prop.command);
	gnome_config_pop_prefix();

	gnome_config_sync();
	gnome_config_drop_all();

	return FALSE;
}

static void
applet_destroy (GtkWidget *applet, Fish *fish)
{
	g_free (fish->prop.name);
	fish->prop.name = NULL;
	g_free (fish->prop.image);
	fish->prop.image = NULL;
	g_free (fish->prop.command);
	fish->prop.command = NULL;

	if (fish->pix != NULL)
		gdk_pixmap_unref(fish->pix);
	fish->pix = NULL;

	if (fish->timeout_id != 0)
		gtk_timeout_remove(fish->timeout_id);
	fish->timeout_id = 0;

	if (fish->fortune_dialog != NULL)
		gtk_widget_destroy(fish->fortune_dialog);
	fish->fortune_dialog = NULL;

	if (fish->aboutbox != NULL)
		gtk_widget_destroy(fish->aboutbox);
	fish->aboutbox = NULL;

	if (fish->pb != NULL)
		gtk_widget_destroy(fish->pb);
	fish->pb = NULL;

	g_free(fish);
}

static void
applet_change_orient(GtkWidget *w, PanelOrientType o, gpointer data)
{
	Fish *fish = data;
	
	fish->orient = o;
	
	load_image_file (fish);
	
	setup_size (fish);

	fish_timeout (fish);
}

static void
applet_change_pixel_size (GtkWidget *w, int size, gpointer data)
{
	Fish *fish = data;

	fish->size = size;
	
	load_image_file (fish);

	setup_size (fish);

	fish_timeout (fish);
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

	fish->size = PIXEL_SIZE_STANDARD;
	fish->orient = ORIENT_UP;

	fish->applet = applet_widget_new(goad_id);

	gtk_widget_push_visual(gdk_rgb_get_visual ());
	gtk_widget_push_colormap(gdk_rgb_get_cmap ());

	bah_window = gtk_window_new(GTK_WINDOW_POPUP);

	gtk_widget_pop_visual();
	gtk_widget_pop_colormap();

	gtk_widget_set_uposition(bah_window,gdk_screen_width()+1,
				 gdk_screen_height()+1);
	gtk_widget_show_now(bah_window);

	load_properties(fish);

	gtk_signal_connect(GTK_OBJECT(fish->applet),"change_orient",
			   GTK_SIGNAL_FUNC(applet_change_orient),
			   fish);
	gtk_signal_connect(GTK_OBJECT(fish->applet),"change_pixel_size",
			   GTK_SIGNAL_FUNC(applet_change_pixel_size),
			   fish);

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
					      "properties",
					      GNOME_STOCK_MENU_PROP,
					      _("Properties..."),
					      properties_dialog,
					      fish);

	applet_widget_register_stock_callback(APPLET_WIDGET(fish->applet),
					      "help",
					      GNOME_STOCK_PIXMAP_HELP,
					      _("Help"),
					      help_cb,
					      NULL);

	applet_widget_register_stock_callback(APPLET_WIDGET(fish->applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."),
					      about_cb,
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

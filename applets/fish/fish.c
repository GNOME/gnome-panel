/* fish.c - The most useless applet, it's an ugly fish drawing
 * by the least sane person, yes you guessed, George, it's nice
 * to break the monotony of finals
 */

#include <config.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <panel-applet.h>
#include <panel-applet-gconf.h>

#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/libart.h>

#define FISH_PREFS_NAME		"name"
#define FISH_PREFS_IMAGE	"image"
#define FISH_PREFS_COMMAND	"command"
#define FISH_PREFS_FRAMES	"frames"
#define FISH_PREFS_SPEED	"speed"
#define FISH_PREFS_ROTATE	"rotate"

/* macro to always pass a non-null string */
#define sure_string(__x) ((__x)!=NULL?(__x):"")

int fools_day=1, fools_month=3;

typedef struct {
	GtkWidget         *applet;
	GtkWidget         *frame;
	GtkWidget         *darea;
	GdkPixmap         *pix;

	int                w;
	int                h;
	int                curpix;
	int                timeout_id;
	gboolean           april_fools;

	GtkWidget         *fortune_dialog;
	GtkWidget         *fortune_label;
	GtkWidget         *fortune_less; 

	GtkWidget         *aboutbox;
	GtkWidget         *pb;

	gint               size;
	PanelAppletOrient  orient;
} Fish;

GtkWidget *bah_window = NULL;

static gboolean
fish_applet_rotate (Fish *fish)
{
	gboolean retval = FALSE;

	if (fish->orient == PANEL_APPLET_ORIENT_LEFT ||
	    fish->orient == PANEL_APPLET_ORIENT_RIGHT)
		retval = panel_applet_gconf_get_bool (PANEL_APPLET (fish->applet),
						      FISH_PREFS_ROTATE,
						      NULL);

	return retval;
}

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
load_image_file (Fish *fish)
{
	GdkPixbuf *pixbuf;
	GError    *error = NULL;
	double     affine [6];
	guchar    *rgb;
	GdkGC     *gc;
	gint       size, w, h, width, height;
	gchar     *image = NULL;
	gchar     *tmp;
	gint       frames;

	if (fish->pix)
		gdk_pixmap_unref (fish->pix);

	g_assert (bah_window && bah_window->window);

	frames = panel_applet_gconf_get_int (PANEL_APPLET (fish->applet), FISH_PREFS_FRAMES, NULL);

	tmp = panel_applet_gconf_get_string (PANEL_APPLET (fish->applet), FISH_PREFS_IMAGE, NULL);
	if (g_path_is_absolute (tmp))
		image = tmp;
	else {
		image = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
						   tmp, FALSE, NULL);
		g_free (tmp);
	}

	pixbuf = gdk_pixbuf_new_from_file (image, &error);
	if (error) {
		g_warning (G_STRLOC ": cannot open %s: %s", image, error->message);

		g_error_free (error);
		g_free (image);
		
		if (fish_applet_rotate (fish)) {
			fish->w = fish->size;
			fish->h = frames * fish->size;
		} else {
			fish->w = frames * fish->size;
			fish->h = fish->size;
		}

 		fish->pix = gdk_pixmap_new (bah_window->window,
					    fish->w, fish->h, -1);

		return;
	}

	size = fish->size - 4;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	if (ABS (height - size) < 6)
		size = height;

	h = size;
	w = width * ((double) h / height);
		
	affine [1] = affine [2] = affine [4] = affine [5] = 0;

	affine [0] = w / (double) width;
	affine [3] = h / (double) height;

	if (fish_applet_rotate (fish)) {
		double tmpaffine [6];
		int    t;

		t = w; w = h; h = t;

		art_affine_rotate (tmpaffine, 270);
		art_affine_multiply (affine, affine, tmpaffine);
		art_affine_translate (tmpaffine, 0, h);
		art_affine_multiply (affine, affine, tmpaffine);
	}

	if (fish->april_fools) {
		double tmpaffine[6];

		art_affine_rotate (tmpaffine, 180);
		art_affine_multiply (affine, affine, tmpaffine);
		art_affine_translate (tmpaffine, w, h);
		art_affine_multiply (affine, affine, tmpaffine);
	}
		
	rgb = g_new0 (guchar, w*h*3);

	if (gdk_pixbuf_get_has_alpha (pixbuf))
		art_rgb_rgba_affine (rgb, 0, 0, w, h, w * 3,
				     gdk_pixbuf_get_pixels (pixbuf),
				     width, height,
				     gdk_pixbuf_get_rowstride (pixbuf),
				     affine,
				     ART_FILTER_NEAREST,
				     NULL);
	else
		art_rgb_affine (rgb, 0, 0, w, h, w * 3,
				gdk_pixbuf_get_pixels (pixbuf),
				width, height,
				gdk_pixbuf_get_rowstride (pixbuf),
				affine,
				ART_FILTER_NEAREST,
				NULL);
		
	gdk_pixbuf_unref (pixbuf);

	fish->w = w;
	fish->h = h;

	fish->pix = gdk_pixmap_new (bah_window->window,
				    fish->w,fish->h,-1);

	gc = gdk_gc_new (fish->pix);

	if (fish->april_fools)
		/* 
		 * rgb has is packed, thus rowstride is w*3 so
		 * we can do the following.  It makes the whole
		 * image have an ugly industrial waste like tint
		 * to make it obvious that the fish is dead
		 */
		art_rgb_run_alpha (rgb, 255, 128, 0, 70, w*h);

	gdk_draw_rgb_image (fish->pix, gc, 0, 0, w, h,
			    GDK_RGB_DITHER_NORMAL,
			    rgb, w * 3);
		
	g_free (rgb);
	g_free (image);
}

static void
setup_size (Fish *fish)
{
	gint frames;

	frames = panel_applet_gconf_get_int (PANEL_APPLET (fish->applet), FISH_PREFS_FRAMES, NULL);

	if (fish_applet_rotate (fish)) {
		gtk_drawing_area_size (GTK_DRAWING_AREA (fish->darea),
				       fish->w,
				       fish->h / frames);

		gtk_widget_set_usize (fish->darea,
				      fish->w,
				      fish->h / frames);
	} else {
		gtk_drawing_area_size (GTK_DRAWING_AREA (fish->darea), 
				       fish->w / frames,
				       fish->h);

		gtk_widget_set_usize (fish->darea,
				      fish->w / frames,
				      fish->h);
	}

	gtk_widget_queue_resize (fish->darea);
	gtk_widget_queue_resize (fish->darea->parent);
	gtk_widget_queue_resize (fish->darea->parent->parent);
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
fish_draw (GtkWidget *darea,
	   Fish      *fish)
{
	gint frames;

	if (!GTK_WIDGET_REALIZED (fish->darea))
		return;

	frames = panel_applet_gconf_get_int (PANEL_APPLET (fish->applet), FISH_PREFS_FRAMES, NULL);

	if (fish_applet_rotate (fish))
		gdk_draw_pixmap (fish->darea->window,
				 fish->darea->style->fg_gc [GTK_WIDGET_STATE (fish->darea)],
				 fish->pix,
				 0,
				 (fish->h*fish->curpix) / frames,
				 0, 0, -1, -1);
	else
		gdk_draw_pixmap (fish->darea->window,
				 fish->darea->style->fg_gc [GTK_WIDGET_STATE (fish->darea)],
				 fish->pix,
				 (fish->w * fish->curpix) / frames,
				 0, 0, 0, -1, -1);
}

static int
fish_timeout(gpointer data)
{
	Fish      *fish = data;
	time_t     ourtime;
	struct tm *tm;


	time (&ourtime);
	tm = localtime (&ourtime);

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
	if (!fish->april_fools) {
		gint frames;

		frames = panel_applet_gconf_get_int (PANEL_APPLET (fish->applet), FISH_PREFS_FRAMES, NULL);

		fish->curpix++;
		if (fish->curpix >= frames)
			fish->curpix = 0;

		fish_draw (fish->darea, fish);
	}

	return TRUE;
}

static void
apply_properties(Fish *fish) 
{
	/* xgettext:no-c-format */
	const char *title_format = _("%s the Fish");
	const char *label_format = _("%s the GNOME Fish Says:");
	gdouble     speed;

	if (fish->fortune_dialog) { 
		gchar *name;     
		gchar *tmp;

		name = panel_applet_gconf_get_string (PANEL_APPLET (fish->applet),
						      FISH_PREFS_NAME,
						      NULL);

		tmp = splice_name (title_format, name);
		gtk_window_set_title (GTK_WINDOW (fish->fortune_dialog), tmp);
		g_free (tmp);

		tmp = splice_name (label_format, name);
		gtk_label_set_text (GTK_LABEL (fish->fortune_label), tmp);
		g_free (tmp);

		g_free (name);
	}
	
	load_image_file (fish);

	setup_size (fish);

	if (fish->timeout_id)
		gtk_timeout_remove (fish->timeout_id);

	speed = panel_applet_gconf_get_float (PANEL_APPLET (fish->applet), FISH_PREFS_SPEED, NULL);

        fish->timeout_id = gtk_timeout_add (speed * 1000, fish_timeout, fish);
	fish->curpix = 0;

	fish_timeout (fish);
}

static void
fish_properties_apply_callback (GnomePropertyBox *pb,
				int               page,
				Fish             *fish)
{
	GtkWidget     *name;
	GtkWidget     *image;
	GtkAdjustment *frames;
	GtkAdjustment *speed;
	GtkWidget     *rotate;
	GtkWidget     *command;
	const gchar   *text;
	gchar         *image_file;

	name    = gtk_object_get_data (GTK_OBJECT (pb), "name");
	image   = gtk_object_get_data (GTK_OBJECT (pb), "image");
	frames  = gtk_object_get_data (GTK_OBJECT (pb), "frames");
	speed   = gtk_object_get_data (GTK_OBJECT (pb), "speed");
	rotate  = gtk_object_get_data (GTK_OBJECT (pb), "rotate");
	command = gtk_object_get_data (GTK_OBJECT (pb), "command");

	if (page != -1) 
		return;

	text = gtk_entry_get_text (GTK_ENTRY (name));
	if (text)
		panel_applet_gconf_set_string (PANEL_APPLET (fish->applet),
					       FISH_PREFS_NAME,
					       text,
					       NULL);

	image_file = gnome_pixmap_entry_get_filename (GNOME_PIXMAP_ENTRY (image));
	if (image_file)
		panel_applet_gconf_set_string (PANEL_APPLET (fish->applet),
					       FISH_PREFS_IMAGE,
					       image_file,
					       NULL);

	text = gtk_entry_get_text (GTK_ENTRY (command));
	if (text) {
		panel_applet_gconf_set_string (PANEL_APPLET (fish->applet),
					       FISH_PREFS_COMMAND,
					       text,
					       NULL);

		/* We need more useful commands to warn on here */
		if (!strncmp (text, "ps ", 3)  ||
		    !strcmp  (text, "ps")      ||
		    !strncmp (text, "who ", 4) ||
		    !strcmp  (text, "who")     ||
		    !strcmp  (text, "uptime")  ||
		    !strncmp (text, "tail ", 5))
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

	panel_applet_gconf_set_int (PANEL_APPLET (fish->applet),
				    FISH_PREFS_FRAMES,
                                    frames->value,
				    NULL);

	panel_applet_gconf_set_float (PANEL_APPLET (fish->applet),
				      FISH_PREFS_SPEED,
				      speed->value,
				      NULL);

	panel_applet_gconf_set_bool (PANEL_APPLET (fish->applet),
				     FISH_PREFS_ROTATE,
				     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rotate)),
				     NULL);

	apply_properties (fish);
}

static void
phelp_cb (GtkWidget *w, gint tab, gpointer data)
{
#ifdef FIXME /* figure out new help stuff */

	GnomeHelpMenuEntry help_entry = { "fish_applet",
					  "index.html#FISH-PREFS" };
	gnome_help_display (NULL, &help_entry);
#endif
}

static void 
display_properties_dialog (BonoboUIComponent *uic,
			   Fish              *fish,
			   const gchar       *verbname)
{
	GtkWidget     *vbox;
	GtkWidget     *hbox;
	GtkWidget     *w;
	GtkWidget     *e;
	GtkAdjustment *adj;
	PanelApplet   *applet;
	gchar         *name;
	gchar         *image;
	gchar         *command;
	gint           frames;
	gdouble        speed;
	gboolean       rotate;

	if (fish->pb) {
		gtk_widget_show (fish->pb);
		gdk_window_raise (fish->pb->window);
		return;
	}

	applet = PANEL_APPLET (fish->applet);

	name    = panel_applet_gconf_get_string (applet, FISH_PREFS_NAME, NULL);
	image   = panel_applet_gconf_get_string (applet, FISH_PREFS_IMAGE, NULL);
	command = panel_applet_gconf_get_string (applet, FISH_PREFS_COMMAND, NULL);
	frames  = panel_applet_gconf_get_int    (applet, FISH_PREFS_FRAMES, NULL);
	speed   = panel_applet_gconf_get_float  (applet, FISH_PREFS_SPEED, NULL);
	rotate  = panel_applet_gconf_get_bool   (applet, FISH_PREFS_ROTATE, NULL);

	fish->pb = gnome_property_box_new ();
	gtk_window_set_wmclass (GTK_WINDOW (fish->pb), "fish", "Fish");
	gtk_window_set_title (GTK_WINDOW (fish->pb),
			      _("GNOME Fish Properties"));
	gnome_window_icon_set_from_file (GTK_WINDOW (fish->pb),
					 GNOME_ICONDIR "/gnome-fish.png");

	vbox = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new (_("Your GNOME Fish's Name:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	e = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY(e), name);
	gtk_box_pack_start (GTK_BOX (hbox), e, TRUE, TRUE, 0);
	gtk_object_set_data (GTK_OBJECT (fish->pb),"name",e);

	gtk_signal_connect_object_while_alive (GTK_OBJECT (e),
					       "changed",
					       GTK_SIGNAL_FUNC (gnome_property_box_changed),
					       GTK_OBJECT (fish->pb));

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new (_("The Animation Filename:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	w = gnome_pixmap_entry_new ("fish_animation", _("Browse"), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);
	e = gnome_pixmap_entry_gtk_entry (GNOME_PIXMAP_ENTRY (w));
	gtk_entry_set_text (GTK_ENTRY (e), image);
	gtk_object_set_data (GTK_OBJECT (fish->pb), "image", w);

	gtk_signal_connect_object_while_alive (GTK_OBJECT (e),
					       "changed",
					       GTK_SIGNAL_FUNC (gnome_property_box_changed),
					       GTK_OBJECT (fish->pb));

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new (_("Command to execute when fish is clicked:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	e = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (e), command);
	gtk_box_pack_start (GTK_BOX (hbox), e, TRUE, TRUE, 0);
	gtk_object_set_data (GTK_OBJECT (fish->pb), "command", e);

	gtk_signal_connect_object_while_alive (GTK_OBJECT (e),
					       "changed",
					       GTK_SIGNAL_FUNC (gnome_property_box_changed),
					       GTK_OBJECT (fish->pb));

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new (_("Frames In Animation:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *) gtk_adjustment_new (frames, 1.0, 255.0, 1.0, 5.0, 0.0);
	w = gtk_spin_button_new (adj, 0, 0);
	gtk_widget_set_usize (w, 70, 0);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_object_set_data (GTK_OBJECT (fish->pb), "frames", adj);

	gtk_signal_connect_object (GTK_OBJECT (adj),
				   "value_changed",
				   GTK_SIGNAL_FUNC (gnome_property_box_changed),
				   GTK_OBJECT (fish->pb));

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new (_("Pause per frame (s):"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *) gtk_adjustment_new (speed, 0.1, 10.0, 0.1, 1.0, 0.0);
	w = gtk_spin_button_new (adj, 0, 2);
	gtk_widget_set_usize (w, 70, 0);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_object_set_data (GTK_OBJECT (fish->pb), "speed", adj);

	gtk_signal_connect_object (GTK_OBJECT (adj),
				   "value_changed",
				   GTK_SIGNAL_FUNC (gnome_property_box_changed),
				   GTK_OBJECT (fish->pb));

	w = gtk_check_button_new_with_label (_("Rotate on vertical panels"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), rotate);
	gtk_signal_connect_object (GTK_OBJECT (w),
				   "toggled",
				   GTK_SIGNAL_FUNC (gnome_property_box_changed),
				   GTK_OBJECT (fish->pb));

	gtk_object_set_data (GTK_OBJECT (fish->pb), "rotate", w);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	gnome_property_box_append_page (GNOME_PROPERTY_BOX (fish->pb),
					vbox,
					gtk_label_new (_("Fish")));

	gtk_signal_connect (GTK_OBJECT(fish->pb),
			    "apply",
			    GTK_SIGNAL_FUNC (fish_properties_apply_callback),
			    fish);

	gtk_signal_connect (GTK_OBJECT(fish->pb),
			    "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			    &fish->pb);

	gtk_signal_connect (GTK_OBJECT (fish->pb),
			    "help",
			    GTK_SIGNAL_FUNC (phelp_cb),
			    NULL);

	gtk_widget_show_all (fish->pb);

	g_free (name);
	g_free (image);
	g_free (command);
}

#ifdef FIXME /* used in update_fortune_dialog */
static gchar *
fish_locate_fortune_command (Fish *fish)
{
	gchar *command;
	gchar *tmp;

	tmp = panel_applet_gconf_get_string (PANEL_APPLET (fish->applet), FISH_PREFS_COMMAND, NULL);
	if (!tmp)
		return NULL;

	if (g_path_is_absolute (tmp))
		return tmp;

	command = g_find_program_in_path (tmp);
	g_free (tmp);

	if (!command)
		if (g_file_test ("/usr/games/fortune", G_FILE_TEST_EXISTS))
			command = g_strdup ("/usr/games/fortune");

	return command;
}
#endif

static void 
update_fortune_dialog (Fish *fish)
{
#ifdef FIXME /* need to replace GnomeLess */
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

	fortune_command = fish_locate_fortune_command (fish);
	if (fortune_command) {
                gnome_less_show_command (GNOME_LESS(fish->fortune_less),
					 fortune_command);
	} else {
                gnome_less_show_string (GNOME_LESS(fish->fortune_less),
					_("You do not have fortune installed "
					  "or you have not specified a program "
					  "to run.\n\nPlease refer to fish "
					  "properties dialog."));
	}
	g_free (fortune_command);
#endif
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
fish_expose (GtkWidget      *darea,
	     GdkEventExpose *event,
	     Fish           *fish)
{
	gint frames;

	frames = panel_applet_gconf_get_int (PANEL_APPLET (fish->applet), FISH_PREFS_FRAMES, NULL);

	if (fish_applet_rotate (fish))
		gdk_draw_pixmap (fish->darea->window,
				 fish->darea->style->fg_gc [GTK_WIDGET_STATE (fish->darea)],
				 fish->pix,
				 event->area.x,
				 ((fish->h * fish->curpix) / frames) + event->area.y,
				 event->area.x, event->area.y,
				 event->area.width, event->area.height);
	else
		gdk_draw_pixmap (fish->darea->window,
				fish->darea->style->fg_gc [GTK_WIDGET_STATE (fish->darea)],
				fish->pix,
				((fish->w *fish->curpix) / frames) + event->area.x,
				event->area.y,
				event->area.x, event->area.y,
				event->area.width, event->area.height);

        return FALSE;
}

static void
create_fish_widget(Fish *fish)
{
	gint    frames;
	gdouble speed;

	frames = panel_applet_gconf_get_int   (PANEL_APPLET (fish->applet), FISH_PREFS_FRAMES, NULL);
	speed  = panel_applet_gconf_get_float (PANEL_APPLET (fish->applet), FISH_PREFS_SPEED, NULL);

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	fish->darea = gtk_drawing_area_new();

	if (fish_applet_rotate (fish))
		gtk_drawing_area_size (GTK_DRAWING_AREA (fish->darea),
				       fish->w,
				       fish->h / frames);
	else
		gtk_drawing_area_size (GTK_DRAWING_AREA (fish->darea),
				       fish->w / frames,
				       fish->h);

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

        fish->timeout_id = gtk_timeout_add (speed * 1000, fish_timeout, fish);

        gtk_container_add(GTK_CONTAINER(fish->frame),fish->darea);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
}

static void
display_help_dialog (BonoboUIComponent *uic,
		     Fish              *fish,
		     const gchar       *verbname)
{
#ifdef FIXME /* figure out new help stuff */

	GnomeHelpMenuEntry help_ref = { "fish_applet", "index.html"};

	gnome_help_display (NULL, &help_ref);
#endif
}

/*
 * the most important dialog in the whole application
 */
static void
display_about_dialog (BonoboUIComponent *uic,
		      Fish              *fish,
		      const gchar       *verbname)
{
	const gchar *author_format = _("%s the Fish");
	gchar       *authors [3];
	GdkPixbuf   *pixbuf;
	GError      *error = NULL;
	gchar       *file;
	gchar       *name;

	if (fish->aboutbox) {
		gtk_widget_show (fish->aboutbox);
		gdk_window_raise (fish->aboutbox->window);
		return;
	}

	name = panel_applet_gconf_get_string (PANEL_APPLET (fish->applet), FISH_PREFS_NAME, NULL);

	authors[0] = splice_name (author_format, name);
	authors[1] = _("(with minor help from George)");
	authors[2] = NULL;

	g_free (name);

	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-fish.png", FALSE, NULL);

	pixbuf = gdk_pixbuf_new_from_file (file, &error);

	g_free (file);

	if (error) {
		g_warning (G_STRLOC ": cannot open %s: %s", file, error->message);

		g_error_free (error);
	}
		
	fish->aboutbox =
		gnome_about_new (_("The GNOME Fish Applet"),
				 "3.4.7.4ac19",
				 "(C) 1998 the Free Software Foundation",
				 _("This applet has no use what-so-ever. "
				   "It only takes up disk space and "
				   "compilation time, and if loaded it also "
				   "takes up precious panel space and memory. "
				   "If anyone is found using this applet, he "
				   "should be promptly sent for a psychiatric "
				   "evaluation."),
				 (const char **)authors,
				 NULL,
				 NULL,
				 pixbuf);

	if (pixbuf)
		gdk_pixbuf_unref (pixbuf);

	gtk_window_set_wmclass (GTK_WINDOW (fish->aboutbox), "fish", "Fish");
	gnome_window_icon_set_from_file (GTK_WINDOW (fish->aboutbox),
					 GNOME_ICONDIR"/gnome-fish.png");
	gtk_signal_connect (GTK_OBJECT (fish->aboutbox), "destroy",
			    GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			    &fish->aboutbox);
	gtk_widget_show (fish->aboutbox);

	g_free (authors[0]);
}

static void
applet_destroy (GtkWidget *applet, Fish *fish)
{
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
applet_change_orient (PanelApplet       *applet,
		      PanelAppletOrient  orient,
		      Fish              *fish)
{
	if (fish->orient == orient)
		return;

	fish->orient = orient;
	
	load_image_file (fish);
	
	setup_size (fish);

	fish_timeout (fish);
}

static void
applet_change_pixel_size (PanelApplet *applet,
			  gint         size,
			  Fish        *fish)
{
	if (fish->size == size)
		return;

	fish->size = size;
	
	load_image_file (fish);

	setup_size (fish);

	fish_timeout (fish);
}

static const BonoboUIVerb fish_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("FishProperties", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("FishHelp",       display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("FishAbout",      display_about_dialog),

        BONOBO_UI_VERB_END
};

static const char fish_menu_xml [] =
	"<popup name=\"button3\">\n"
	"   <menuitem name=\"Fish Properties Item\" verb=\"FishProperties\" _label=\"Properties ...\"\n"
	"             pixtype=\"stock\" pixname=\"gtk-properties\"/>\n"
	"   <menuitem name=\"Fish Help Item\" verb=\"FishHelp\" _label=\"Help\"\n"
	"             pixtype=\"stock\" pixname=\"gtk-help\"/>\n"
	"   <menuitem name=\"Fish About Item\" verb=\"FishAbout\" _label=\"About ...\"\n"
	"             pixtype=\"stock\" pixname=\"gnome-stock-about\"/>\n"
	"</popup>\n";

static gboolean
fish_applet_fill (PanelApplet *applet)
{
	Fish *fish;

	fish = g_new0 (Fish, 1);

	fish->applet = GTK_WIDGET (applet);
	fish->size   = panel_applet_get_size (applet);
	fish->orient = panel_applet_get_orient (applet);

	panel_applet_add_preferences (applet, "/schemas/apps/fish-applet/prefs", NULL);

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	bah_window = gtk_window_new (GTK_WINDOW_POPUP);

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	gtk_widget_set_uposition (bah_window,
				  gdk_screen_width () + 1,
				  gdk_screen_height () + 1);
	gtk_widget_show_now (bah_window);

	fish->frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (fish->frame), GTK_SHADOW_IN);

	load_image_file (fish);

	create_fish_widget (fish);

	set_wanda_day ();

	gtk_container_add (GTK_CONTAINER (fish->applet), fish->frame);

	gtk_widget_show_all (GTK_WIDGET (fish->frame));

	gtk_widget_show (GTK_WIDGET (fish->applet));

	g_signal_connect (G_OBJECT (fish->applet),
			 "destroy",
			  G_CALLBACK (applet_destroy),
			  fish);

	g_signal_connect (G_OBJECT (fish->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient),
			  fish);

	g_signal_connect (G_OBJECT (fish->applet),
			  "change_size",
			  G_CALLBACK (applet_change_pixel_size),
			  fish);

	panel_applet_setup_menu (PANEL_APPLET (fish->applet),
				 fish_menu_xml,
				 fish_menu_verbs,
				 fish);
	
	return TRUE;
}

static gboolean
fishy_factory (PanelApplet *applet,
	       const gchar *iid,
	       gpointer     data)
{
	gboolean retval = FALSE;

	if (!strcmp (iid, "OAFIID:GNOME_FishApplet"))
		retval = fish_applet_fill (applet);

	return retval;
}

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_FishApplet_Factory",
			     "That-stupid-fish",
			     "0",
			     fishy_factory,
			     NULL)

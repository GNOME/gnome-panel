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
	GtkTextBuffer	  *fortune_buffer;

	GtkWidget         *aboutbox;
	GtkWidget         *pb;

	gint               size;
	PanelAppletOrient  orient;
} Fish;

/*
 * set_access_name_desc
 * Description : Set Accessible Name and Description.
 */
static void
set_access_name_desc (GtkWidget *widget, Fish *fish)
{
	AtkObject *obj;
	static GtkWidget *w = NULL;
	gchar *fish_name, *acc_desc, *acc_name;

	const gchar *name_format = _("%s the GNOME Fish");
	const gchar *desc_format = _("%s the GNOME Fish, a contemporary oracle");

	fish_name = panel_applet_gconf_get_string (PANEL_APPLET (fish->applet),
						   FISH_PREFS_NAME,
						   NULL);

	if (widget)
		w = widget;

	if (w) 
		obj = gtk_widget_get_accessible (GTK_WIDGET (w));
	else
		return;

	/* Return immediately if GAIL is not loaded */
	if (!GTK_IS_ACCESSIBLE (obj))
		return;

	acc_name = g_strdup_printf (name_format, fish_name);
	atk_object_set_name (obj, acc_name);
	g_free (acc_name);       

	acc_desc = g_strdup_printf (desc_format, fish_name);
	atk_object_set_description (obj, acc_desc);
	g_free (acc_desc);
}

static void
set_tooltip (GtkWidget *applet)
{
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new ();
	g_object_ref (tooltips);
	gtk_object_sink (GTK_OBJECT (tooltips));

	g_object_set_data (G_OBJECT (applet), "tooltips", tooltips);

	gtk_tooltips_set_tip (tooltips, applet,
			      _("GNOME Fish, the fortune teller"), NULL);
}

static void
destroy_tooltip (GtkWidget *applet)
{
	GtkTooltips *tooltips;

	tooltips = g_object_get_data (G_OBJECT (applet), "tooltips");
	if (tooltips) {
		g_object_unref (tooltips);
		g_object_set_data (G_OBJECT (applet), "tooltips", NULL);
	}
}

/*
 * set_relation
 * @widget : The Gtk widget whose corresponding label is @label
 * @label : The label for the @widget.
 * Description : This function establishes the labelled-by/for relation
 * between any Gtk widget and a Gtk label.
 */
static void
set_relation (GtkWidget *widget, GtkLabel *label)
{
	AtkObject      *aobject;
	AtkRelationSet *relation_set;
	AtkRelation    *relation;
	AtkObject      *targets [1];

	g_return_if_fail (GTK_IS_WIDGET(widget));
	g_return_if_fail (GTK_IS_LABEL(label));

	aobject = gtk_widget_get_accessible (widget);

	/* Set the ATK_RELATION_LABEL_FOR relation */
	gtk_label_set_mnemonic_widget (label, widget);

	/* return if gail is not loaded */
	if ( ! GTK_IS_ACCESSIBLE (aobject))
		return;

	targets [0] = gtk_widget_get_accessible (GTK_WIDGET (label));

	relation_set = atk_object_ref_relation_set (aobject);

	relation = atk_relation_new (targets, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (relation);
}

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
	GdkPixbuf *pixbuf = NULL;
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

	frames = panel_applet_gconf_get_int (PANEL_APPLET (fish->applet), FISH_PREFS_FRAMES, NULL);
	if (frames <= 0)
		frames = 1;

	tmp = panel_applet_gconf_get_string (PANEL_APPLET (fish->applet), FISH_PREFS_IMAGE, NULL);
	if (g_path_is_absolute (tmp)) {
		image = tmp;
	} else {
		image = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
						   tmp, FALSE, NULL);
		if (image == NULL)
			image = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
							   tmp, FALSE, NULL);
		g_free (tmp);
	}

	if (image != NULL)
		pixbuf = gdk_pixbuf_new_from_file (image, &error);
	if (image == NULL || error != NULL) {
		if (error != NULL) {
			g_warning (G_STRLOC ": cannot open %s: %s", image, error->message);

			g_error_free (error);
		}
		g_free (image);
		
		if (fish_applet_rotate (fish)) {
			fish->w = fish->size;
			fish->h = frames * fish->size;
		} else {
			fish->w = frames * fish->size;
			fish->h = fish->size;
		}
 		fish->pix = gdk_pixmap_new (GDK_ROOT_PARENT (),
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

	fish->pix = gdk_pixmap_new (GDK_ROOT_PARENT (),
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
	if (frames <= 0)
		frames = 1;

	if (fish_applet_rotate (fish)) {
		GTK_WIDGET (fish->darea)->requisition.width = fish->w;
		GTK_WIDGET (fish->darea)->requisition.height =
							fish->h / frames;
		gtk_widget_queue_resize (GTK_WIDGET (fish->darea));

		gtk_widget_set_usize (fish->darea,
				      fish->w,
				      fish->h / frames);
	} else {
		GTK_WIDGET (fish->darea)->requisition.width =
							fish->w / frames;
		GTK_WIDGET (fish->darea)->requisition.height = fish->h;
		gtk_widget_queue_resize (GTK_WIDGET (fish->darea));

		gtk_widget_set_usize (fish->darea,
				      fish->w / frames,
				      fish->h);
	}

	gtk_widget_queue_resize (fish->darea);
	gtk_widget_queue_resize (fish->darea->parent);
	gtk_widget_queue_resize (fish->darea->parent->parent);
}

static void
fish_draw (GtkWidget *darea,
	   Fish      *fish)
{
	gint frames;

	if (!GTK_WIDGET_REALIZED (fish->darea))
		return;

	frames = panel_applet_gconf_get_int (PANEL_APPLET (fish->applet), FISH_PREFS_FRAMES, NULL);
	if (frames <= 0)
		frames = 1;

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
		if (frames <= 0)
			frames = 1;

		fish->curpix++;
		if (fish->curpix >= frames)
			fish->curpix = 0;

		fish_draw (fish->darea, fish);
	}

	return TRUE;
}

static void
apply_dialog_properties (Fish *fish) 
{
	/* xgettext:no-c-format */
	const char *title_format = _("%s the Fish");
	const char *label_format = _("<big><big>%s the GNOME Fish Says:</big></big>");
	char *name, *tmp;

	if (fish->fortune_dialog == NULL)
		return;

	name = panel_applet_gconf_get_string (PANEL_APPLET (fish->applet),
					      FISH_PREFS_NAME,
					      NULL);

	tmp = g_strdup_printf (title_format, name);
	gtk_window_set_title (GTK_WINDOW (fish->fortune_dialog), tmp);
	g_free (tmp);

	tmp = g_strdup_printf (label_format, name);
	gtk_label_set_markup (GTK_LABEL (fish->fortune_label), tmp);
	g_free (tmp);

	g_free (name);
}

static void
apply_properties (Fish *fish) 
{
	gdouble speed;

	apply_dialog_properties (fish);
	
	load_image_file (fish);

	setup_size (fish);

	if (fish->timeout_id)
		gtk_timeout_remove (fish->timeout_id);

	speed = panel_applet_gconf_get_float (PANEL_APPLET (fish->applet), FISH_PREFS_SPEED, NULL);
	if (speed <= 0)
		speed = 1.0;

        fish->timeout_id = gtk_timeout_add (speed * 1000, fish_timeout, fish);
	fish->curpix = 0;

	fish_timeout (fish);
}

static void
fish_properties_apply (GtkDialog *pb, Fish *fish)
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
		    !strncmp (text, "tail ", 5)) {
			GtkWidget *w;
			w = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING,
						    GTK_BUTTONS_OK,
						    _("Warning:  The command appears to be "
						      "something actually useful.\n"
						      "Since this is a useless applet, you "
						      "may not want to do this.\n"
						      "We strongly advise you against "
						      "usage of wanda for anything\n"
						      "which would make the applet "
						      "\"practical\" or useful."));
			gtk_window_set_wmclass (GTK_WINDOW (w), "fish_useful_warning", "Fish");

			gtk_widget_show_all (w);

			g_signal_connect_swapped (G_OBJECT (w), "response",
						  G_CALLBACK (gtk_widget_destroy),
						  G_OBJECT (w));
		}
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

	set_access_name_desc (NULL,fish); 

	apply_properties (fish);
}

static void
phelp (void)
{
	GError *error = NULL;

	gnome_help_display_desktop (
		NULL, "fish-applet-2", "fish-applet-2", NULL, &error);
	if (error) {
		g_warning ("help error: %s\n", error->message);
		g_error_free (error);
	}
}

static void
changed_cb (GtkWidget *button, gpointer data)
{
	gtk_widget_set_sensitive (GTK_WIDGET(button), TRUE);
}

static void
response_cb (GtkDialog *dialog, gint id, gpointer data) 
{
	Fish *fish = data;

	switch (id) 
	{
		case GTK_RESPONSE_OK:
			fish_properties_apply (dialog, fish);
			gtk_widget_destroy (GTK_WIDGET(dialog));
			fish->pb = NULL;
			break;
		case GTK_RESPONSE_APPLY:
			fish_properties_apply (dialog, fish);
			break;
		case GTK_RESPONSE_HELP:
			phelp ();
			break;
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy (GTK_WIDGET(dialog));
			fish->pb = NULL;
			break;
	}
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
	GtkWidget     *l;
	GtkAdjustment *adj;
	PanelApplet   *applet;
	gchar         *name;
	gchar         *image;
	gchar         *command;
	gint           frames;
	gdouble        speed;
	gboolean       rotate;
	GtkWidget     *apply_button;

	if (fish->pb != NULL) {
		gtk_window_present (GTK_WINDOW (fish->pb));
		return;
	}

	applet = PANEL_APPLET (fish->applet);

	name    = panel_applet_gconf_get_string (applet, FISH_PREFS_NAME, NULL);
	image   = panel_applet_gconf_get_string (applet, FISH_PREFS_IMAGE, NULL);
	command = panel_applet_gconf_get_string (applet, FISH_PREFS_COMMAND, NULL);
	frames  = panel_applet_gconf_get_int    (applet, FISH_PREFS_FRAMES, NULL);
	speed   = panel_applet_gconf_get_float  (applet, FISH_PREFS_SPEED, NULL);
	rotate  = panel_applet_gconf_get_bool   (applet, FISH_PREFS_ROTATE, NULL);

	fish->pb = gtk_dialog_new ();
	gtk_dialog_add_button (GTK_DIALOG (fish->pb), 
				GTK_STOCK_HELP, GTK_RESPONSE_HELP);
	apply_button = gtk_dialog_add_button (GTK_DIALOG (fish->pb), 
				GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);
	gtk_widget_set_sensitive (GTK_WIDGET(apply_button), FALSE);
	gtk_dialog_add_buttons (GTK_DIALOG (fish->pb), 
				GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, 
				GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_window_set_wmclass (GTK_WINDOW (fish->pb), "fish", "Fish");

	gtk_dialog_set_default_response (GTK_DIALOG (fish->pb), GTK_RESPONSE_OK);

	gtk_window_set_title (GTK_WINDOW (fish->pb),
			      _("GNOME Fish Preferences"));
	gnome_window_icon_set_from_file (GTK_WINDOW (fish->pb),
					 GNOME_ICONDIR "/gnome-fish.png");

	vbox = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new_with_mnemonic (_("_Name of GNOME fish:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	e = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY(e), name);
	gtk_box_pack_start (GTK_BOX (hbox), e, TRUE, TRUE, 0);
	gtk_object_set_data (GTK_OBJECT (fish->pb),"name",e);

	set_relation (e, GTK_LABEL (w));

	gtk_signal_connect_object_while_alive (GTK_OBJECT (e),
					       "changed",
					       GTK_SIGNAL_FUNC (changed_cb),
					       GTK_OBJECT (apply_button));

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	l = gtk_label_new_with_mnemonic (_("_Animation filename:"));
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	w = gnome_pixmap_entry_new ("fish_animation", _("Browse"), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);
	e = gnome_pixmap_entry_gtk_entry (GNOME_PIXMAP_ENTRY (w));
	gtk_entry_set_text (GTK_ENTRY (e), image);
	gtk_object_set_data (GTK_OBJECT (fish->pb), "image", w);

	set_relation (e, GTK_LABEL (l));

	gtk_signal_connect_object_while_alive (GTK_OBJECT (e),
					       "changed",
					       GTK_SIGNAL_FUNC (changed_cb),
					       GTK_OBJECT (apply_button));

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new_with_mnemonic (_("Command to _execute when fish is clicked:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	e = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (e), command);
	gtk_box_pack_start (GTK_BOX (hbox), e, TRUE, TRUE, 0);
	gtk_object_set_data (GTK_OBJECT (fish->pb), "command", e);

	set_relation (e, GTK_LABEL (w));

	gtk_signal_connect_object_while_alive (GTK_OBJECT (e),
					       "changed",
					       GTK_SIGNAL_FUNC (changed_cb),
					       GTK_OBJECT (apply_button));

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new_with_mnemonic (_("Frame_s in animation:"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *) gtk_adjustment_new (frames, 1.0, 255.0, 1.0, 5.0, 0.0);
	e = gtk_spin_button_new (adj, 0, 0);
	gtk_widget_set_usize (e, 70, 0);
	gtk_box_pack_start (GTK_BOX (hbox), e, FALSE, FALSE, 0);
	gtk_object_set_data (GTK_OBJECT (fish->pb), "frames", adj);

	set_relation (e, GTK_LABEL (w));

	gtk_signal_connect_object (GTK_OBJECT (adj),
				   "value_changed",
				   GTK_SIGNAL_FUNC (changed_cb),
				   GTK_OBJECT (apply_button));

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	w = gtk_label_new_with_mnemonic (_("Pause _per frame (s):"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *) gtk_adjustment_new (speed, 0.1, 10.0, 0.1, 1.0, 0.0);
	e = gtk_spin_button_new (adj, 0, 2);
	gtk_widget_set_usize (e, 70, 0);
	gtk_box_pack_start (GTK_BOX (hbox), e, FALSE, FALSE, 0);
	gtk_object_set_data (GTK_OBJECT (fish->pb), "speed", adj);

	set_relation (e, GTK_LABEL (w));

	gtk_signal_connect_object (GTK_OBJECT (adj),
				   "value_changed",
				   GTK_SIGNAL_FUNC (changed_cb),
				   GTK_OBJECT (apply_button));

	w = gtk_check_button_new_with_mnemonic (_("_Rotate on vertical panels"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), rotate);
	gtk_signal_connect_object (GTK_OBJECT (w),
				   "toggled",
				   GTK_SIGNAL_FUNC (changed_cb),
				   GTK_OBJECT (apply_button));

	gtk_object_set_data (GTK_OBJECT (fish->pb), "rotate", w);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (fish->pb)->vbox), 
	                    vbox, TRUE, TRUE, 0);

	gtk_signal_connect (GTK_OBJECT(fish->pb),
			    "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			    &fish->pb);

	gtk_signal_connect (GTK_OBJECT (fish->pb),
			    "response",
			    GTK_SIGNAL_FUNC (response_cb),
			    fish);

	gtk_widget_show_all (fish->pb);

	g_free (name);
	g_free (image);
	g_free (command);
}

static void
something_fishy_going_on (const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 message);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_widget_show (dialog);
}

static gchar *
fish_locate_fortune_command (Fish *fish)
{
	char *retval = NULL;

	retval = panel_applet_gconf_get_string (PANEL_APPLET (fish->applet), FISH_PREFS_COMMAND, NULL);
	if (!retval) {
		something_fishy_going_on (_("Unable to get the name of the command to execute"));
		return NULL;
	}

	if (!g_path_is_absolute (retval)) {
		char *tmp;

		tmp = retval;
		retval = g_find_program_in_path (tmp);
		g_free (tmp);
	}

	if (!retval && g_file_test ("/usr/games/fortune", G_FILE_TEST_EXISTS))
		retval = g_strdup ("/usr/games/fortune");

	if (!retval)
		something_fishy_going_on (_("Unable to locate the command to execute"));

	return retval;
}

static void
insert_text (Fish *fish, const char *text)
{
	GtkTextIter iter;

	gtk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &iter, -1);

	gtk_text_buffer_insert (fish->fortune_buffer, &iter,
				text, -1);
}

static void
text_clear (Fish *fish)
{
	GtkTextIter begin, end;

	gtk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &begin, 0);
	gtk_text_buffer_get_iter_at_offset (fish->fortune_buffer, &end, -1);

	gtk_text_buffer_delete (fish->fortune_buffer, &begin, &end);

	/* insert an empty line */
	insert_text (fish, "\n");
}

static gboolean
response (GtkWidget *w, int response, gpointer data)
{
	gtk_widget_hide (w);
	return TRUE;
}

static gboolean
delete_event (GtkWidget *w, gpointer data)
{
	gtk_widget_hide (w);
	return TRUE;
}

static void 
update_fortune_dialog (Fish *fish)
{
	GError *error = NULL;
	char   *fortune_command;
	char   *output = NULL;
	char   *output_utf8;

	fortune_command = fish_locate_fortune_command (fish);
	if (!fortune_command)
		return;

	if ( fish->fortune_dialog == NULL ) {
		GtkWidget *view;
		GtkWidget *sw;
      
		fish->fortune_dialog = 
			gtk_dialog_new_with_buttons ("",
						     NULL /* parent */,
						     0 /* flags */,
						     GTK_STOCK_CLOSE,
						     GTK_RESPONSE_CLOSE,
						     NULL);

		gtk_dialog_set_default_response (GTK_DIALOG (fish->fortune_dialog), GTK_RESPONSE_CLOSE);

		g_signal_connect (G_OBJECT (fish->fortune_dialog), "delete_event",
				  G_CALLBACK (delete_event), NULL);
		g_signal_connect (G_OBJECT (fish->fortune_dialog), "response",
				  G_CALLBACK (response), NULL);
		gtk_window_set_wmclass (GTK_WINDOW (fish->fortune_dialog), "fish", "Fish");
		gnome_window_icon_set_from_file (GTK_WINDOW (fish->fortune_dialog),
						 GNOME_ICONDIR"/gnome-fish.png");

		gtk_window_set_default_size (GTK_WINDOW (fish->fortune_dialog),
					     MIN (600, gdk_screen_width () * 0.9),
					     MIN (350, gdk_screen_height () * 0.9));

		view = gtk_text_view_new ();
		gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
		gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
		gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 10);
		gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 10);
		fish->fortune_buffer =
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
						     GTK_SHADOW_IN);

		gtk_container_add (GTK_CONTAINER (sw), view);

		set_access_name_desc (view, fish);

		fish->fortune_label = gtk_label_new ("");

		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (fish->fortune_dialog)->vbox), 
				    fish->fortune_label,
				    FALSE, FALSE, GNOME_PAD);

		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (fish->fortune_dialog)->vbox), 
				    sw,
				    TRUE, TRUE, GNOME_PAD);

		apply_dialog_properties (fish);

		gtk_widget_show_all (fish->fortune_dialog);
	} else {
		set_access_name_desc (NULL, fish);
		gtk_window_present (GTK_WINDOW (fish->fortune_dialog));
	}

	text_clear (fish);

	g_spawn_command_line_sync (fortune_command, &output, NULL, NULL, &error);
	if (error) {
		char *message;

		message = g_strdup_printf (_("Unable to execute '%s'\n\nDetails : %s"),
					   fortune_command, error->message);
		something_fishy_going_on (message);
		g_free (message);
		g_error_free (error);
	}
	
	g_free (fortune_command);

	/* The output is not guarantied to be in UTF-8 format, most
	   likely it's just in ASCII-7 or in the user locale */
	if (!g_utf8_validate (output, -1, NULL))
	  output_utf8 = g_locale_to_utf8 (output, -1, NULL, NULL, NULL);
	else
	  output_utf8 = g_strdup (output);

	if (output_utf8) {
		insert_text (fish, output_utf8);
	} else {
		insert_text (fish, 
			     _("You do not have fortune installed "
			       "or you have not specified a program "
			       "to run.\n\nPlease refer to fish "
			       "properties dialog."));
	}
	g_free (output);
	g_free (output_utf8);
}

static void
change_water (Fish *fish)
{
	GtkWidget *w;
	w = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_INFO,
				    GTK_BUTTONS_OK,
				    _("The water needs changing!\n"
				    "(Look at today's date)"));
	gtk_window_set_wmclass (GTK_WINDOW (w), "fish", "Fish");

	gtk_widget_show_all (w);

	g_signal_connect_swapped (G_OBJECT (w), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  G_OBJECT (w));
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
		change_water (fish);
		return TRUE;
	}

	update_fortune_dialog(fish);

	return TRUE; 
}

static gboolean
fish_keypress_cb (GtkWidget * widget, GdkEventKey * e, Fish *fish)
{
	/* Invoke fortune dialog when Enter or Space key is pressed */
	switch (e->keyval)
	{
		case GDK_space:
		case GDK_KP_Space:
		case GDK_Return:
		case GDK_KP_Enter:
		case GDK_ISO_Enter:
		case GDK_3270_Enter:
			if (fish->april_fools) {
				change_water (fish);
				return TRUE;
			}

			update_fortune_dialog(fish);
			break;
		default:
			/* Other keys are not handled here */
			return FALSE;
	}

	return TRUE;
}

static int
fish_expose (GtkWidget      *darea,
	     GdkEventExpose *event,
	     Fish           *fish)
{
	int frames;

	frames = panel_applet_gconf_get_int (PANEL_APPLET (fish->applet), FISH_PREFS_FRAMES, NULL);
	if (frames <= 0)
		frames = 1;

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

	/* Sanity checking */
	if (frames <= 0)
		frames = 1;
	if (speed <= 0)
		speed = 1.0;

	fish->darea = gtk_drawing_area_new();

	if (fish_applet_rotate (fish)) {
		GTK_WIDGET (fish->darea)->requisition.width = fish->w;
		GTK_WIDGET (fish->darea)->requisition.height =
							fish->h / frames;
		gtk_widget_queue_resize (GTK_WIDGET (fish->darea));
		gtk_widget_set_usize (fish->darea,
				      fish->w,
				      fish->h / frames);
	} else {
		GTK_WIDGET (fish->darea)->requisition.width =
							fish->h / frames;
		GTK_WIDGET (fish->darea)->requisition.width = fish->w;

		gtk_widget_queue_resize (GTK_WIDGET (fish->darea));
		gtk_widget_set_usize (fish->darea,
				      fish->w / frames,
				      fish->h);
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

        fish->timeout_id = gtk_timeout_add (speed * 1000, fish_timeout, fish);

        gtk_container_add(GTK_CONTAINER(fish->frame),fish->darea);
}

static void
display_help_dialog (BonoboUIComponent *uic,
		     Fish              *fish,
		     const gchar       *verbname)
{
	GError *error = NULL;

	gnome_help_display_desktop (
		NULL, "fish-applet-2", "fish-applet-2", NULL, &error);
	if (error) {
		g_warning ("help error: %s\n", error->message);
		g_error_free (error);
	}
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

	if (fish->aboutbox != NULL) {
		gtk_window_present (GTK_WINDOW (fish->aboutbox));
		return;
	}

	name = panel_applet_gconf_get_string (PANEL_APPLET (fish->applet), FISH_PREFS_NAME, NULL);

	authors[0] = g_strdup_printf (author_format, name);
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
		gnome_about_new (_("Fish"),
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
	destroy_tooltip (GTK_WIDGET (fish->applet));

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

	/* not yet all loaded up */
	if (fish->frame == NULL)
		return;
	
	load_image_file (fish);
	
	setup_size (fish);

	fish_timeout (fish);
}

static void
applet_change_size (PanelApplet *applet,
		    gint         size,
		    Fish        *fish)
{
	if (fish->size == size)
		return;

	fish->size = size;

	/* not yet all loaded up */
	if (fish->frame == NULL)
		return;
	
	load_image_file (fish);

	setup_size (fish);

	fish_timeout (fish);
}

static const BonoboUIVerb fish_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("FishPreferences", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("FishHelp",        display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("FishAbout",       display_about_dialog),

        BONOBO_UI_VERB_END
};



static gboolean
fish_applet_fill (PanelApplet *applet)
{
	Fish *fish;

	fish = g_new0 (Fish, 1);

	fish->applet = GTK_WIDGET (applet);
	fish->size   = panel_applet_get_size (applet);
	fish->orient = panel_applet_get_orient (applet);

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
			  G_CALLBACK (applet_change_size),
			  fish);

	panel_applet_add_preferences (applet, "/schemas/apps/fish_applet/prefs", NULL);

	fish->frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (fish->frame), GTK_SHADOW_IN);

	load_image_file (fish);

	create_fish_widget (fish);
	
	set_wanda_day ();

	gtk_container_add (GTK_CONTAINER (fish->applet), fish->frame);

	set_tooltip (GTK_WIDGET (fish->applet));

	gtk_signal_connect (GTK_OBJECT (fish->applet), "key_press_event",
			    GTK_SIGNAL_FUNC (fish_keypress_cb), fish);

	gtk_widget_show_all (GTK_WIDGET (fish->frame));

	gtk_widget_show (GTK_WIDGET (fish->applet));
	
	setup_size (fish);

	panel_applet_setup_menu_from_file (PANEL_APPLET (fish->applet),
				 NULL,
				 "GNOME_FishApplet.xml",
				 NULL,
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
			     PANEL_TYPE_APPLET,
			     "That-stupid-fish",
			     "0",
			     fishy_factory,
			     NULL)

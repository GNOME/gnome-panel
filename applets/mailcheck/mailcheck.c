/*
 * GNOME panel mail check module.
 * (C) 1997, 1998 The Free Software Foundation
 *
 * Author: Miguel de Icaza
 *
 */

#include <config.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <config.h>
#include <gnome.h>
#include <applet-widget.h>
#include <gdk_imlib.h>

#include "mailcheck.h"

#define WIDGET_HEIGHT 48

GtkWidget *applet = NULL;

typedef struct _MailCheck MailCheck;
struct _MailCheck {
	char *mail_file;

	/* If set, the user has launched the mail viewer */
	int mailcleared;

	/* Does the user have any mail at all? */
	int anymail;

	/* New mail has arrived? */
	int newmail;

	/* Does the user have unread mail? */
	int unreadmail;

  guint update_freq;

  char *cmd;

	/* This holds either the drawing area or the label */
	GtkWidget *bin;

	/* The widget that holds the label with the mail information */
	GtkWidget *label;

	/* Points to whatever we have inside the bin */
	GtkWidget *containee;

	/* The drawing area */
	GtkWidget *da;
	GdkPixmap *email_pixmap;
	GdkBitmap *email_mask;

	/* handle for the timeout */
	int mail_timeout;

	/* how do we report the mail status */
	enum {
		REPORT_MAIL_USE_TEXT,
		REPORT_MAIL_USE_BITMAP,
		REPORT_MAIL_USE_ANIMATION
	} report_mail_mode;

	/* current frame on the animation */
	int nframe;

	/* number of frames on the pixmap */
	int frames;

	/* handle for the animation timeout handler */
	int animation_tag;

	/* for the selection routine */
	char *selected_pixmap_name;

	/* The property window */
	GtkWidget *property_window;
  GtkWidget *spin, *cmd_entry;
  gboolean anim_changed;

	char *mailcheck_text_only;

	char *animation_file;
};

#define WANT_BITMAPS(x) (x == REPORT_MAIL_USE_ANIMATION || x == REPORT_MAIL_USE_BITMAP)


static void close_callback (GtkWidget *widget, void *data);


static char *
mail_animation_filename (MailCheck *mc)
{
	if(!mc->animation_file) {
		mc->animation_file =
			gnome_unconditional_pixmap_file("mailcheck/email.png");
		if (g_file_exists (mc->animation_file))
			return g_strdup(mc->animation_file);
		g_free (mc->animation_file);
		mc->animation_file = NULL;
		return NULL;
	} else if (*mc->animation_file){
		if (g_file_exists (mc->animation_file))
			return g_strdup(mc->animation_file);
		g_free (mc->animation_file);
		mc->animation_file = NULL;
		return NULL;
	} else
		/*we are using text only, since the filename was ""!*/
		return NULL;
}

/*
 * Get file modification time, based upon the code
 * of Byron C. Darrah for coolmail and reused on fvwm95
 */
static void
check_mail_file_status (MailCheck *mc)
{
	static off_t oldsize = 0;
	struct stat s;
	off_t newsize;
	int status;

	status = stat (mc->mail_file, &s);
	if (status < 0){
		oldsize = 0;
		mc->anymail = mc->newmail = mc->unreadmail = 0;
		return;
	}
	
	newsize = s.st_size;
	mc->anymail = newsize > 0;
	mc->unreadmail = (s.st_mtime >= s.st_atime && newsize > 0);

	if (newsize >= oldsize && mc->unreadmail){
		mc->newmail = 1;
		mc->mailcleared = 0;
	} else
		mc->newmail = 0;
	oldsize = newsize;
}

static void
mailcheck_load_animation (MailCheck *mc, char *fname)
{
	int width, height;
	GdkImlibImage *im;

	im = gdk_imlib_load_image (fname);

	width = im->rgb_width;
	height = im->rgb_height;

	gdk_imlib_render (im, width, height);

	mc->email_pixmap = gdk_imlib_copy_image (im);
	mc->email_mask = gdk_imlib_copy_mask (im);

	gdk_imlib_destroy_image (im);
	
	/* yeah, they have to be square, in case you were wondering :-) */
	mc->frames = width / WIDGET_HEIGHT;
	if (mc->frames == 3)
		mc->report_mail_mode = REPORT_MAIL_USE_BITMAP;
	mc->nframe = 0;
}

static int
next_frame (gpointer data)
{
	MailCheck *mc = data;

	mc->nframe = (mc->nframe + 1) % mc->frames;
	if (mc->nframe == 0)
		mc->nframe = 1;
	gtk_widget_draw (mc->da, NULL);
	return TRUE;
}

static int
mail_check_timeout (gpointer data)
{
	MailCheck *mc = data;

  if(mc->cmd) {
    /* if we have to execute a command before checking for mail, we
       remove the mail-check timeout and re-add it after the command
       returns, just in case the execution takes too long. */
    gtk_timeout_remove(mc->mail_timeout);
    if(system(mc->cmd) == 127)
      g_warning("Couldn't execute command");
    mc->mail_timeout = gtk_timeout_add(mc->update_freq, mail_check_timeout, mc);
  }
                                      
	check_mail_file_status (mc);

	switch (mc->report_mail_mode){
	case REPORT_MAIL_USE_ANIMATION:
		if (mc->anymail){
			if (mc->newmail){
				if (mc->animation_tag == -1){
					mc->animation_tag = gtk_timeout_add (150, next_frame, mc);
					mc->nframe = 1;
				}
			} else {
				if (mc->animation_tag != -1){
					gtk_timeout_remove (mc->animation_tag);
					mc->animation_tag = -1;
				}
				mc->nframe = 1;
			}
		} else {
			if (mc->animation_tag != -1){
				gtk_timeout_remove (mc->animation_tag);
				mc->animation_tag = -1;
			}
			mc->nframe = 0;
		}
		gtk_widget_draw (mc->da, NULL);
		break;
		
	case REPORT_MAIL_USE_BITMAP:
		if (mc->anymail){
			if (mc->newmail)
				mc->nframe = 2;
			else
				mc->nframe = 1;
		} else
			mc->nframe = 0;
		gtk_widget_draw (mc->da, NULL);
		break;

	case REPORT_MAIL_USE_TEXT: {
		char *text;

		if (mc->anymail){
			if (mc->newmail)
				text = _("You have new mail.");
			else
				text = _("You have mail.");
		} else
			text = _("No mail.");
		gtk_label_set (GTK_LABEL (mc->label), text);
		break;
	}
	}
	return 1;
}

/*
 * this gets called when we have to redraw the nice icon
 */
static gint
icon_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	MailCheck *mc = data;
	gdk_draw_pixmap (mc->da->window, mc->da->style->black_gc,
			 mc->email_pixmap, mc->nframe * WIDGET_HEIGHT,
			 0, 0, 0, WIDGET_HEIGHT, WIDGET_HEIGHT);
	return TRUE;
}

static void
mailcheck_destroy (GtkWidget *widget, gpointer data)
{
	MailCheck *mc = data;
	mc->bin = NULL;

	if (mc->property_window)
		close_callback (NULL, mc);

  if(mc->cmd)
    g_free(mc->cmd);

	gtk_timeout_remove (mc->mail_timeout);
}

static GtkWidget *
create_mail_widgets (MailCheck *mc)
{
	char *fname = mail_animation_filename (mc);

	mc->bin = gtk_hbox_new (0, 0);

	/* This is so that the properties dialog is destroyed if the
	 * applet is removed from the panel while the dialog is
	 * active.
	 */
	gtk_signal_connect (GTK_OBJECT (mc->bin), "destroy",
			    (GtkSignalFunc) mailcheck_destroy,
			    mc);

	gtk_widget_show (mc->bin);
	check_mail_file_status (mc);
	
	mc->mail_timeout = gtk_timeout_add (mc->update_freq, mail_check_timeout, mc);

	/* The drawing area */
	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());
	mc->da = gtk_drawing_area_new ();
	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();
	gtk_widget_ref (mc->da);
	gtk_drawing_area_size (GTK_DRAWING_AREA(mc->da), 48, 48);
	gtk_signal_connect (GTK_OBJECT(mc->da), "expose_event", (GtkSignalFunc)icon_expose, mc);
	gtk_widget_set_events(GTK_WIDGET(mc->da),GDK_EXPOSURE_MASK);
	gtk_widget_show (mc->da);

	/* The label */
	mc->label = gtk_label_new ("");
	gtk_widget_show (mc->label);
	gtk_widget_ref (mc->label);
	
	if (fname && WANT_BITMAPS (mc->report_mail_mode)) {
		mailcheck_load_animation (mc,fname);
		mc->containee = mc->da;
	} else {
		mc->report_mail_mode = REPORT_MAIL_USE_TEXT;
		mc->containee = mc->label;
	}
	free (fname);
	gtk_container_add (GTK_CONTAINER (mc->bin), mc->containee);
	mail_check_timeout (mc);
	return mc->bin;
}

static void
set_selection (GtkWidget *widget, gpointer data)
{
	MailCheck *mc = gtk_object_get_user_data(GTK_OBJECT(widget));
  mc->selected_pixmap_name = data;
  mc->anim_changed = TRUE;

	gnome_property_box_changed (GNOME_PROPERTY_BOX (mc->property_window));
}

static void
property_box_changed(GtkWidget *widget, gpointer data) {
  MailCheck *mc = data;

  gnome_property_box_changed (GNOME_PROPERTY_BOX (mc->property_window));
}

static void
free_str (GtkWidget *widget, void *data)
{
	g_free (data);
}

static void
mailcheck_new_entry (MailCheck *mc, GtkWidget *menu, GtkWidget *item, char *s)
{
	gtk_menu_append (GTK_MENU (menu), item);

	gtk_object_set_user_data(GTK_OBJECT(item),mc);

	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC(set_selection), s);
	if (s != mc->mailcheck_text_only)
		gtk_signal_connect (GTK_OBJECT (item), "destroy",
				    GTK_SIGNAL_FUNC(free_str), s);
}

static GtkWidget *
mailcheck_get_animation_menu (MailCheck *mc)
{
	GtkWidget *omenu, *menu, *item;
	struct    dirent *e;
	char      *dname = gnome_unconditional_pixmap_file ("mailcheck");
	DIR       *dir;
	char      *basename = NULL;
	int       i = 0, select_item = 0;

	mc->selected_pixmap_name = mc->mailcheck_text_only;
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();

	item = gtk_menu_item_new_with_label (mc->mailcheck_text_only);
	gtk_widget_show (item);
	mailcheck_new_entry (mc, menu, item, mc->mailcheck_text_only);

	if (mc->animation_file){
		basename = strrchr (mc->animation_file, '/');
		if (basename)
			basename++;
	} else
		mc->animation_file = NULL;

	i = 1;
	dir = opendir (dname);
	if (dir){
		while ((e = readdir (dir)) != NULL){
			char *s;
			
			if (! (strstr (e->d_name, ".xpm") ||
			       strstr (e->d_name, ".png") ||
			       strstr (e->d_name, ".gif") ||
			       strstr (e->d_name, ".jpg")))
				continue;

			if (!mc->selected_pixmap_name)
				mc->selected_pixmap_name = s;
			if (basename && strcmp (basename, e->d_name) == 0)
				select_item = i;
			s = g_strdup (e->d_name);
			item = gtk_menu_item_new_with_label (s);
			i++;
			gtk_widget_show (item);
			
			mailcheck_new_entry (mc,menu, item, s);
		}
		closedir (dir);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), select_item);
	gtk_widget_show (omenu);
	return omenu;
}

static void
close_callback (GtkWidget *widget, gpointer data)
{
	MailCheck *mc = data;
	gtk_widget_destroy (mc->property_window);
	mc->property_window = NULL;
}

static void
load_new_pixmap (MailCheck *mc)
{
	gtk_widget_hide (mc->containee);
	gtk_container_remove (GTK_CONTAINER (mc->bin), mc->containee);
	
	if (mc->selected_pixmap_name == mc->mailcheck_text_only) {
		mc->report_mail_mode = REPORT_MAIL_USE_TEXT;
		mc->containee = mc->label;
		if(mc->animation_file) g_free(mc->animation_file);
		mc->animation_file = NULL;
	} else {
		char *fname = g_copy_strings ("mailcheck/", mc->selected_pixmap_name, NULL);
		char *full;
		
		full = gnome_unconditional_pixmap_file (fname);
		free (fname);
		
		mailcheck_load_animation (mc,full);
		mc->containee = mc->da;
		if(mc->animation_file) g_free(mc->animation_file);
		mc->animation_file = full;
	}
	mail_check_timeout (mc);
	gtk_widget_set_uposition (GTK_WIDGET (mc->containee), 0, 0);
	gtk_container_add (GTK_CONTAINER (mc->bin), mc->containee);
	gtk_widget_show (mc->containee);
}

static void
apply_properties_callback (GtkWidget *widget, gint button_num, gpointer data) {
  MailCheck *mc = (MailCheck *)data;

  mc->update_freq = (guint)(gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(mc->spin))*1000);
  gtk_timeout_remove(mc->mail_timeout);
  mc->mail_timeout = gtk_timeout_add(mc->update_freq, mail_check_timeout, mc);

  if(mc->cmd) {
    g_free(mc->cmd);
    mc->cmd = NULL;
  }

  if(strlen(gtk_entry_get_text(GTK_ENTRY(mc->cmd_entry))) > 0)
    mc->cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(mc->cmd_entry)));

  if(mc->anim_changed)
    load_new_pixmap(mc);

  mc->anim_changed = FALSE;
}
      
static GtkWidget *
mailcheck_properties_page (MailCheck *mc)
{
	GtkWidget *freq, *vbox, *hbox, *l, *entry;
  GtkObject *freq_a;
	
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  

  l = gtk_label_new(_("Execute"));
  gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

  mc->cmd_entry = gtk_entry_new();
  if(mc->cmd)
    gtk_entry_set_text(GTK_ENTRY(mc->cmd_entry), mc->cmd);
  gtk_signal_connect(GTK_OBJECT(mc->cmd_entry), "changed",
                     GTK_SIGNAL_FUNC(property_box_changed), mc);
  gtk_widget_show(mc->cmd_entry);
	gtk_box_pack_start (GTK_BOX (hbox), mc->cmd_entry, FALSE, FALSE, 0);

  l = gtk_label_new(_("before each update"));
  gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	l = gtk_label_new (_("Check for mail every"));
  gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

	freq_a = gtk_adjustment_new((float)mc->update_freq/1000, 0.1, 3600, 0.1, 5, 5);
	mc->spin  = gtk_spin_button_new (GTK_ADJUSTMENT (freq_a), 0.1, 1);
  gtk_signal_connect(GTK_OBJECT(freq_a), "value_changed",
                     GTK_SIGNAL_FUNC(property_box_changed), mc);
  gtk_signal_connect(GTK_OBJECT(mc->spin), "changed",
                     GTK_SIGNAL_FUNC(property_box_changed), mc);
	gtk_box_pack_start (GTK_BOX (hbox), mc->spin,  FALSE, FALSE, 0);
	gtk_widget_show(mc->spin);

	l = gtk_label_new (_("s"));
  gtk_widget_show(l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);  

	l = gtk_label_new (_("Select animation"));
	gtk_misc_set_alignment (GTK_MISC (l), 0.0, 0.5);
	gtk_widget_show (l);
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), mailcheck_get_animation_menu (mc), FALSE, FALSE, 0);

	return vbox;
}

static void
mailcheck_properties (AppletWidget *applet, gpointer data)
{
	GtkWidget *p;

	MailCheck *mc = data;

	if (mc->property_window != NULL) {
		gdk_window_raise(mc->property_window->window);
		return; /* Only one instance of the properties dialog! */
	}
	
	mc->property_window = gnome_property_box_new ();
	gtk_window_set_title (GTK_WINDOW (mc->property_window),
			      _("Mail check properties"));

	p = mailcheck_properties_page (mc);

	gnome_property_box_append_page (GNOME_PROPERTY_BOX(mc->property_window),
					p, gtk_label_new (_("Mail check")));

	gtk_signal_connect (GTK_OBJECT (mc->property_window), "apply",
			    GTK_SIGNAL_FUNC(apply_properties_callback), mc);
	gtk_signal_connect (GTK_OBJECT (mc->property_window), "destroy",
			    GTK_SIGNAL_FUNC(close_callback), mc);

	gtk_widget_show (mc->property_window);
}

static gint
applet_save_session(GtkWidget *w,
		    const char *privcfgpath,
		    const char *globcfgpath,
		    gpointer data)
{
	MailCheck *mc = data;

	gnome_config_push_prefix(privcfgpath);
	gnome_config_set_string("mail/animation_file",
                          mc->animation_file?mc->animation_file:"");
  gnome_config_set_int("mail/update_frequency", mc->update_freq);
  gnome_config_set_string("mail/exec_command",
                          mc->cmd?mc->cmd:"");
	gnome_config_pop_prefix();

	gnome_config_sync();
	gnome_config_drop_all();

	return FALSE;
}

static void
mailcheck_about(AppletWidget *a_widget, gpointer a_data)
{
	GtkWidget *about = NULL;
	static const gchar     *authors [] =
	{ "Miguel de Icaza <miguel@kernel.org>",
	  "Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>",
	  NULL };
	
	about = gnome_about_new ( _("Mail check Applet"), "1.0",
				    _("(c) 1998 the Free Software Foundation"),
				    authors,
				    _("Mail check notifies you when new mail is on your mailbox"),
				    NULL);
	gtk_widget_show(about);
}

GtkWidget *
make_mailcheck_applet(const gchar *goad_id)
{
	GtkWidget *mailcheck;
	MailCheck *mc;
	char *emailfile;
	char *query;

	mc = g_new(MailCheck,1);
	mc->animation_tag = -1;
	mc->animation_file = NULL;
	mc->property_window = NULL;
  mc->anim_changed = FALSE;

  mc->cmd = NULL;

	/*initial state*/
	mc->report_mail_mode = REPORT_MAIL_USE_ANIMATION;

	mc->mail_file = getenv ("MAIL");
	if (!mc->mail_file){
		char *user;
	
		if ((user = getenv("USER")) != NULL){
			mc->mail_file = g_malloc(strlen(user) + 20);
			sprintf(mc->mail_file, "/var/spool/mail/%s", user);
		} else {
			return NULL;
		}
	}

	applet = applet_widget_new(goad_id);
	if (!applet)
		g_error(_("Can't create applet!\n"));

	emailfile = gnome_unconditional_pixmap_file("mailcheck/email.png");

	query = g_copy_strings(APPLET_WIDGET(applet)->privcfgpath,
			       "mail/animation_file=",emailfile,NULL);
	mc->animation_file = gnome_config_get_string(query);
	g_free(query);

  query = g_copy_strings(APPLET_WIDGET(applet)->privcfgpath,
                         "mail/update_frequency=2000", NULL);
  mc->update_freq = gnome_config_get_int(query);
  g_free(query);

  query = g_copy_strings(APPLET_WIDGET(applet)->privcfgpath,
                         "mail/exec_command", NULL);
  mc->cmd = gnome_config_get_string(query);
  g_free(query);

	if(emailfile) g_free(emailfile);

	mc->mailcheck_text_only = _("Text only");
	mailcheck = create_mail_widgets (mc);
	gtk_widget_show(mailcheck);
	applet_widget_add (APPLET_WIDGET (applet), mailcheck);
	gtk_widget_show (applet);
	gtk_signal_connect(GTK_OBJECT(applet),"save_session",
			   GTK_SIGNAL_FUNC(applet_save_session),
			   mc);

	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "properties",
					      GNOME_STOCK_MENU_PROP,
					      _("Properties..."),
					      mailcheck_properties,
					      mc);

	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."),
					      mailcheck_about,
					      NULL);
	return applet;
}

/*
 * GNOME time/date display module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 *          Stuart Parmenter
 *          Jes Sorensen (applet sizing)
 *
 * Feel free to implement new look and feels :-)
 */

#include <stdio.h>
#include <config.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <config.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include <applet-widget.h>

#include "clock.h"

#define CLOCK_DEFAULT_HEIGHT	"48"
#define CLOCK_DEFAULT_WIDTH	"84"

typedef struct _ClockData ClockData;
typedef void (*ClockUpdateFunc) (ClockData *, time_t);
struct _ClockData {
	GtkWidget *applet;
	GtkWidget *clockw;
	GtkObject *height_adj;
	GtkObject *width_adj;
	GtkWidget *about_box;
	ClockUpdateFunc update_func;
	PanelOrientType orient;
	GnomePropertyBox *prop_win;

	gint timeout;
	gint timeouttime;
	gint hourformat;
        gint showdate;
	gint unixtime;

	gint prop_hourformat;
        gint prop_showdate;
	gint prop_unixtime;
	gint height;
	gint width;
};


static void clock_properties(AppletWidget *, gpointer);
static void clock_set_size(ClockData *cd);
static void about_cb (AppletWidget *widget, gpointer data);
static void destroy_about(GtkWidget *w, gpointer data);

typedef struct {
	GtkWidget *date;
	GtkWidget *time;
} ComputerClock;


static void
free_data(GtkWidget * widget, gpointer data)
{
	g_free(data);
}


void
clock_set_size(ClockData *cd)
{
	if (cd->height < 16)
		cd->height = 16;
	if (cd->width < 16)
		cd->width = 16;

	gtk_widget_set_usize (cd->clockw, cd->width, cd->height);
}


static void
destroy_about (GtkWidget *w, gpointer data)
{
	ClockData *cd = data;
}


static void
about_cb (AppletWidget *widget, gpointer data)
{
	ClockData *cd = data;
	char *authors[5];
  
	authors[0] = "Miguel de Icaza";
	authors[1] = "Federico Mena";
	authors[2] = "Stuart Parmenter";
	authors[3] = "Jes Sorensen";
	authors[4] = NULL;

	cd->about_box =
		gnome_about_new (_("The Clock Applet"), VERSION,
		     _(" (C) 1997-1999 The Free Software Foundation"),
		     (const char **) authors,
				 _("This applet will tell you the time."),
				 NULL);

	gtk_signal_connect (GTK_OBJECT (cd->about_box), "destroy",
			    GTK_SIGNAL_FUNC (destroy_about), cd);

	gtk_widget_show (cd->about_box);
}


static int
clock_timeout_callback(gpointer data)
{
	ClockData *cd = data;
	time_t current_time;
	
	time(&current_time);

	(*cd->update_func) (cd, current_time);

	if(cd->unixtime) {
		if (cd->timeouttime != 1000) {
			cd->timeouttime = 1000;
			cd->timeout = gtk_timeout_add(cd->timeouttime,
						      clock_timeout_callback,
						      cd);
			return FALSE;
		}
	} else {
		if (cd->timeouttime != 36000) {
			cd->timeouttime = 36000;
			cd->timeout = gtk_timeout_add(cd->timeouttime,
						      clock_timeout_callback,
						      cd);
			return FALSE;
		}
	}

	return TRUE;
}

static gint
applet_save_session(GtkWidget * w,
		    const char *privcfgpath,
		    const char *globcfgpath,
		    gpointer data)
{
	ClockData *cd = data;
	gnome_config_push_prefix(privcfgpath);
	gnome_config_set_int("clock/hourformat", cd->hourformat);
	gnome_config_set_int("clock/showdate", cd->showdate);
	gnome_config_set_int("clock/unixtime", cd->unixtime);
	gnome_config_set_int("clock/width", cd->width);
	gnome_config_set_int("clock/height", cd->height);
	gnome_config_pop_prefix();
	gnome_config_sync();
	gnome_config_drop_all();

	return FALSE;
}


static void
computer_clock_update_func(ClockData * cd, time_t current_time)
{
	ComputerClock *cc;
	struct tm *tm;
	char date[20], hour[20];

	cc = gtk_object_get_user_data(GTK_OBJECT(cd->clockw));

	tm = localtime(&current_time);

	if (cd->showdate && !cd->unixtime) {
	    if (cd->orient == ORIENT_LEFT || cd->orient == ORIENT_RIGHT) {
	        /* This format string is used, to display the actual day,
		   when showing a vertical panel.  For an explanation of
		   this format string type 'man strftime'.  */
	        if (strftime(date, 20, _("%a\n%b %d"), tm) == 20)
		    date[19] = '\0';
	    } else {
	        /* This format string is used, to display the actual day,
		   when showing a horizontal panel.  */
	        if (strftime(date, 20, _("%a %b %d"), tm) == 20)
		    date[19] = '\0';
	    }
	    gtk_label_set_text(GTK_LABEL(cc->date), date);
	    if (!GTK_WIDGET_VISIBLE(cc->date))
	         gtk_widget_show (cc->date);
	  } else {
	      if (GTK_WIDGET_VISIBLE(cc->date))
	          gtk_widget_hide (cc->date);
	  }

	if (cd->unixtime) {
	    if (cd->orient == ORIENT_LEFT || cd->orient == ORIENT_RIGHT) {
		    g_snprintf(hour,20,"%lu\n%lu",
			       (unsigned long)(current_time/100000L),
			       (unsigned long)(current_time%100000L));
	    } else {
		    g_snprintf(hour,20,"%lu",(unsigned long)current_time);
	    } 
	} else if (cd->hourformat == 0) {
	    /* This format string is used, to display the actual time in
	       12 hour format.  */
	    if (cd->orient == ORIENT_LEFT || cd->orient == ORIENT_RIGHT) {
	        if (strftime(hour, 20, _("%I:%M\n%p"), tm) == 20)
		        hour[19] = '\0';
	    } else {
	        if (strftime(hour, 20, _("%I:%M %p"), tm) == 20)
	                hour[19] = '\0';
	    } 
	} else if (cd->hourformat == 1) {
	    /* This format string is used, to display the actual time in
               24 hour format.  */
	    if (strftime(hour, 20, _("%H:%M"), tm) == 20)
			hour[19] = '\0';
	}
	gtk_label_set_text(GTK_LABEL(cc->time), hour);
}


static void
adj_value_changed_cb(GtkAdjustment *ignored, gpointer data)
{
	ClockData *cd = data;

	gnome_property_box_changed(GNOME_PROPERTY_BOX(cd->prop_win)); 
}


static void
create_computer_clock_widget(GtkWidget ** clock, ClockUpdateFunc * update_func)
{
	GtkWidget *frame;
	GtkWidget *align;
	GtkWidget *vbox;
	ComputerClock *cc;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_widget_show(frame);

	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_container_set_border_width(GTK_CONTAINER(align), 4);
	gtk_container_add(GTK_CONTAINER(frame), align);
	gtk_widget_show(align);

	vbox = gtk_vbox_new(FALSE, FALSE);
	gtk_container_add(GTK_CONTAINER(align), vbox);
	gtk_widget_show(vbox);

	cc = g_new(ComputerClock, 1);
	cc->date = gtk_label_new("hmm");
	cc->time = gtk_label_new("hmm?");

	gtk_box_pack_start_defaults(GTK_BOX(vbox), cc->date);
	gtk_box_pack_start_defaults(GTK_BOX(vbox), cc->time);
	gtk_widget_show(cc->date);
	gtk_widget_show(cc->time);

	gtk_object_set_user_data(GTK_OBJECT(frame), cc);
	gtk_signal_connect(GTK_OBJECT(frame), "destroy",
			   (GtkSignalFunc) free_data,
			   cc);

	*clock = frame;
	*update_func = computer_clock_update_func;
}

static void
destroy_clock(GtkWidget * widget, void *data)
{
	ClockData *cd = data;
	gtk_timeout_remove(cd->timeout);
	g_free(cd);
}

static void
create_clock_widget(ClockData *cd, GtkWidget * applet)
{
	GtkWidget *clock;
	struct tm *tm;
	time_t current_time;

	/*FIXME: different clock types here */
	create_computer_clock_widget(&clock, &cd->update_func);

	cd->clockw = clock;
	cd->applet = applet;

	cd->prop_win = NULL;

	cd->orient = ORIENT_UP;

	gtk_signal_connect(GTK_OBJECT(clock), "destroy",
			   (GtkSignalFunc) destroy_clock,
			   cd);

	clock_set_size(cd);
	/* Call the clock's update function so that it paints its first state */
	time(&current_time);
	(*cd->update_func) (cd, current_time);

	/* Install timeout handler */
        tm = localtime(&current_time);
	if(cd->unixtime)
		cd->timeouttime = 1000;
	else
		cd->timeouttime = 36000-(tm->tm_sec*600);
	cd->timeout = gtk_timeout_add(cd->timeouttime,
				      clock_timeout_callback,
				      cd);
}

/*these are commands sent over corba: */

/*this is when the panel orientation changes */
static void
applet_change_orient(GtkWidget * w, PanelOrientType o, gpointer data)
{
	ClockData *cd = data;
	time_t current_time;

	time(&current_time);
	cd->orient = o;
	(*cd->update_func) (cd, current_time);
}

GtkWidget *
make_clock_applet(const gchar * goad_id)
{
	ClockData *cd;
	GtkWidget *applet;

	applet = applet_widget_new(goad_id);
	if (!applet)
		g_error(_("Can't create applet!\n"));

	cd = g_new(ClockData, 1);

	gnome_config_push_prefix(APPLET_WIDGET(applet)->privcfgpath);
	cd->hourformat = gnome_config_get_int("clock/hourformat=0");
	cd->showdate = gnome_config_get_int("clock/showdate=1");
	cd->unixtime = gnome_config_get_int("clock/unixtime=0");
	cd->width = gnome_config_get_int_with_default
		("clock/width=" CLOCK_DEFAULT_WIDTH, NULL);
	cd->height = gnome_config_get_int_with_default
		("clock/height=" CLOCK_DEFAULT_HEIGHT, NULL);
	gnome_config_pop_prefix();

	create_clock_widget(cd,applet);

	/*we have to bind change_orient before we do applet_widget_add 
	   since we need to get an initial change_orient signal to set our
	   initial oriantation, and we get that during the _add call */
	gtk_signal_connect(GTK_OBJECT(applet), "change_orient",
			   GTK_SIGNAL_FUNC(applet_change_orient),
			   cd);

	gtk_widget_show(cd->clockw);
	applet_widget_add(APPLET_WIDGET(applet), cd->clockw);
	gtk_widget_show(applet);

	gtk_signal_connect(GTK_OBJECT(applet), "save_session",
			   GTK_SIGNAL_FUNC(applet_save_session),
			   cd);

	applet_widget_register_stock_callback (APPLET_WIDGET (cd->applet),
					       "about",
					       GNOME_STOCK_MENU_ABOUT,
					       _("About..."),
					       about_cb,
					       cd);

	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "properties",
					      GNOME_STOCK_MENU_PROP,
					      _("Properties..."),
					      clock_properties,
					      cd);

	return applet;
}

static void
apply_properties(GtkWidget * widget, gint button_num, gpointer data)
{
	ClockData *cd = data;
	time_t current_time;
	struct tm *tm;
	gint height, width, size_changed = FALSE;

	cd->hourformat = cd->prop_hourformat;
	cd->showdate = cd->prop_showdate;
	cd->unixtime = cd->prop_unixtime;

	/* Call the clock's update function so that it paints its first state */
	time(&current_time);
	(*cd->update_func) (cd, current_time);
	
	gtk_timeout_remove(cd->timeout);

	/* Install timeout handler */
        tm = localtime(&current_time);
	if(cd->unixtime)
		cd->timeouttime = 1000;
	else
		cd->timeouttime = 36000-(tm->tm_sec*600);
	cd->timeout = gtk_timeout_add(cd->timeouttime,
				      clock_timeout_callback,
				      cd);

	/*
	 * Update the size
	 */
	height = GTK_ADJUSTMENT (cd->height_adj)->value;
	width = GTK_ADJUSTMENT (cd->width_adj)->value;
	if ((height != cd->height) || (width != cd->width)) {
		size_changed = TRUE;
		cd->width = width;
		cd->height = height;
	}

	if (size_changed)
		clock_set_size(cd);
}

static void
close_properties(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->prop_win = NULL;
}

static void
set_hour_format_cb(GtkWidget * w, gpointer data)
{
	g_return_if_fail(w != NULL);
	if(GTK_TOGGLE_BUTTON(w)->active) {
		ClockData *cd = gtk_object_get_user_data(GTK_OBJECT(w));
		cd->prop_hourformat = (long)data;
		gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->prop_win));
	}
}

static void
set_show_date_cb(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->prop_showdate = GTK_TOGGLE_BUTTON(w)->active;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->prop_win));
}

static void
set_datasensitive_cb(GtkWidget * w, GtkWidget *wid)
{
	gtk_widget_set_sensitive(wid,!(GTK_TOGGLE_BUTTON(w)->active));
}


static void
set_unixtime_cb(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->prop_unixtime = GTK_TOGGLE_BUTTON(w)->active;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->prop_win));
}

static void
clock_properties(AppletWidget * applet, gpointer data)
{
        static GnomeHelpMenuEntry help_entry = { NULL, "properties-clock" };
        GtkWidget *hbox;
        GtkWidget *vbox;
        GtkWidget *hour_frame;
	GtkWidget *table;
	GtkWidget *twelvehour;
	GtkWidget *twentyfourhour;
	GtkWidget *showdate;
	GtkWidget *unixtime;
	GtkWidget *height_spin;
	GtkWidget *width_spin;
	GtkWidget *t, *l;
	ClockData *cd = data;

	help_entry.name = gnome_app_id;

	if(cd->prop_win) {
		gdk_window_raise(GTK_WIDGET (cd->prop_win)->window);
		return;
	}
	cd->prop_win = GNOME_PROPERTY_BOX (gnome_property_box_new ());

	gtk_window_set_title (
		GTK_WINDOW (&GNOME_PROPERTY_BOX (cd->prop_win)->dialog.window),
		_("Clock properties"));

	/*
	 * Clock Format
	 */
	t = gtk_hbox_new(FALSE, GNOME_PAD);
	gnome_property_box_append_page (GNOME_PROPERTY_BOX(cd->prop_win), t,
					gtk_label_new (_("Clock format")));

	hour_frame = gtk_frame_new(_("Time Format"));
	gtk_container_set_border_width(GTK_CONTAINER(hour_frame), GNOME_PAD);
	gtk_box_pack_start (GTK_BOX(t), hour_frame, FALSE, FALSE, 0);

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(hour_frame), table);
	gtk_widget_show(table);

	gtk_container_set_border_width(GTK_CONTAINER(table), GNOME_PAD);
	gtk_table_set_row_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);

	twelvehour = gtk_radio_button_new_with_label(NULL, _("12 hour"));
	gtk_table_attach(GTK_TABLE(table), twelvehour, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL | GTK_EXPAND,
			 0, 0);
	gtk_object_set_user_data(GTK_OBJECT(twelvehour), (gpointer) data);
	gtk_widget_show(twelvehour);

	twentyfourhour = gtk_radio_button_new_with_label(
			 gtk_radio_button_group(GTK_RADIO_BUTTON(twelvehour)),
					        _("24 hour"));

	gtk_table_attach(GTK_TABLE(table), twentyfourhour, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL | GTK_EXPAND,
			 0, 0);
	gtk_object_set_user_data(GTK_OBJECT(twentyfourhour), (gpointer) data);
	gtk_widget_show(twentyfourhour);

	switch (cd->hourformat) {
	case 0:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(twelvehour),
					    TRUE);
		break;

	case 1:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(twentyfourhour),
					    TRUE);
		break;
	}

	cd->prop_hourformat = cd->hourformat;

	gtk_signal_connect(GTK_OBJECT(twelvehour),
			   "toggled",
			   (GtkSignalFunc) set_hour_format_cb,
			   (gpointer) 0);
	gtk_signal_connect(GTK_OBJECT(twentyfourhour),
			   "toggled",
			   (GtkSignalFunc) set_hour_format_cb,
			   (gpointer) 1);
	
	vbox = gtk_vbox_new(FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start_defaults(GTK_BOX(t), vbox);
	gtk_widget_show(vbox);

	showdate = gtk_check_button_new_with_label(_("Show date"));
	gtk_box_pack_start_defaults(GTK_BOX(vbox), showdate);
	gtk_widget_show(showdate);
	
	if (cd->showdate)
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showdate),
				      TRUE);
	cd->prop_showdate = cd->showdate;
	
	gtk_signal_connect(GTK_OBJECT(showdate),
			   "toggled",
			   (GtkSignalFunc) set_show_date_cb,
			   data);

	unixtime = gtk_check_button_new_with_label(_("Unix time"));
	gtk_box_pack_start_defaults(GTK_BOX(vbox), unixtime);
	gtk_widget_show(unixtime);

	gtk_signal_connect(GTK_OBJECT(unixtime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   hour_frame);
	gtk_signal_connect(GTK_OBJECT(unixtime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   showdate);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(unixtime),
				     cd->unixtime);
	cd->prop_unixtime = cd->unixtime;
	
	gtk_signal_connect(GTK_OBJECT(unixtime), "toggled",
			   (GtkSignalFunc) set_unixtime_cb,
			   data);

	gtk_widget_show(hour_frame);
	gtk_widget_show(t);

	gtk_signal_connect(GTK_OBJECT(cd->prop_win), "apply",
			   GTK_SIGNAL_FUNC(apply_properties), data);
	gtk_signal_connect(GTK_OBJECT(cd->prop_win), "destroy",
			   GTK_SIGNAL_FUNC(close_properties), data);
	gtk_signal_connect(GTK_OBJECT(cd->prop_win), "help",
			   GTK_SIGNAL_FUNC(gnome_help_pbox_display),
			   &help_entry);

	/*
	 * General Properties
	 */
	t = gtk_table_new(0, 0, FALSE);
	gnome_property_box_append_page(GNOME_PROPERTY_BOX(cd->prop_win), t,
				       gtk_label_new (_("General")));

	/*
	 * Applet height
	 */
	l = gtk_label_new (_("Applet Height:")); 
	gtk_table_attach_defaults(GTK_TABLE(t), l, 0, 1, 0, 1);
	cd->height_adj = gtk_adjustment_new(cd->height, 16, 666, 1, 8, 8);
	height_spin = gtk_spin_button_new(GTK_ADJUSTMENT(cd->height_adj),
					  1, 0);
	gtk_table_attach_defaults(GTK_TABLE(t), height_spin, 1, 2, 0, 1);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(height_spin),
					  GTK_UPDATE_ALWAYS);
	gtk_signal_connect(GTK_OBJECT(cd->height_adj), "value_changed",
			   GTK_SIGNAL_FUNC(adj_value_changed_cb), cd);

	/*
	 * Applet width
	 */
	l = gtk_label_new (_("Applet Width:")); 
	gtk_table_attach_defaults ( GTK_TABLE (t), l, 0, 1, 1, 2 ); 

	cd->width_adj = gtk_adjustment_new(cd->width, 16, 666, 1, 8, 8);
	width_spin = gtk_spin_button_new(GTK_ADJUSTMENT(cd->width_adj),
					  1, 0);
	gtk_table_attach_defaults(GTK_TABLE(t), width_spin, 1, 2, 1, 2);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(width_spin),
					  GTK_UPDATE_ALWAYS);
	gtk_signal_connect(GTK_OBJECT(cd->width_adj), "value_changed",
			   GTK_SIGNAL_FUNC(adj_value_changed_cb), cd);

	gtk_widget_show_all(GTK_WIDGET(cd->prop_win));
}

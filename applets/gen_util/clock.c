/*
 * GNOME time/date display module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 *          Stuart Parmenter
 *
 * Feel free to implement new look and feels :-)
 */

#include "config.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include <libgnomeui/gnome-window-icon.h>
#include <applet-widget.h>

#include "clock.h"

#define INTERNETSECOND (864/4)

/* values for selection info */
enum {
  TARGET_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT
};

typedef struct _ClockData ClockData;
typedef void (*ClockUpdateFunc) (ClockData *, time_t);
struct _ClockData {
	GtkWidget *applet;
	GtkWidget *clockw;
	int timeout;
	int timeouttime;
	int hourformat;
        gboolean showdate;
	gboolean unixtime;
	gboolean internettime;
	gboolean gmt_time;
	gboolean showtooltip;
	ClockUpdateFunc update_func;
	PanelOrientType orient;
	int size;
	char *copied_string;

	GtkWidget *props;
	int prop_hourformat;
        gboolean prop_showdate;
	gboolean prop_unixtime;
	gboolean prop_internettime;
	gboolean prop_gmt_time;
	gboolean prop_showtooltip;
};

typedef struct {
	GtkWidget *time; /*the time label*/
	GtkWidget *align; /* Needed for changing the padding */
} ComputerClock;


static void clock_properties (AppletWidget *applet, gpointer data);
static void clock_about      (AppletWidget *applet, gpointer data);
static void help_cb	     (AppletWidget *w, gpointer data);
static void phelp_cb	     (GtkWidget *w, gint tab, gpointer data);

static void
free_data(GtkWidget * widget, gpointer data)
{
	g_free(data);
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
	} else if(cd->internettime) {
		if (cd->timeouttime != INTERNETSECOND) {
			cd->timeouttime = INTERNETSECOND;
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

static gboolean
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
	gnome_config_set_int("clock/internettime", cd->internettime);
	gnome_config_set_int("clock/showtooltip", cd->showtooltip);
	gnome_config_set_int("clock/gmt_time", cd->gmt_time);
	gnome_config_pop_prefix();
	gnome_config_sync();
	gnome_config_drop_all();

	return FALSE;
}

static float
get_itime (time_t current_time)
{
	struct tm *tm;
	float itime;
	time_t bmt;

	bmt = current_time + 3600;
	tm = gmtime(&bmt);
	itime = (tm->tm_hour*60+tm->tm_min+tm->tm_sec/60.0)*1000.0/1440.0;

	return itime;
}

static void
computer_clock_update_func(ClockData * cd, time_t current_time)
{
	ComputerClock *cc;
	struct tm *tm;
	GString *gs = g_string_new("");
	char date[256], hour[256], tooltip[256];

	cc = gtk_object_get_user_data(GTK_OBJECT(cd->clockw));

	if (cd->gmt_time)
		tm = gmtime(&current_time);
	else
		tm = localtime(&current_time);

	if (cd->unixtime) {
		if ((cd->orient == ORIENT_LEFT || cd->orient == ORIENT_RIGHT) &&
		    cd->size >= PIXEL_SIZE_STANDARD) {
			g_snprintf(hour,sizeof(hour),"%lu\n%05lu",
				   (unsigned long)(current_time/100000L),
				   (unsigned long)(current_time%100000L));
		} else {
			g_snprintf(hour,sizeof(hour),"%lu",(unsigned long)current_time);
		} 
		g_string_append(gs,hour);
	} else if (cd->internettime) {
		float itime = get_itime (current_time);
		g_snprintf(hour,sizeof(hour),"@%3.2f",itime);
		g_string_append(gs,hour);
	} else if (cd->hourformat == 12) {
		/* This format string is used, to display the actual time in
		   12 hour format.  */
		if ((cd->orient == ORIENT_LEFT || cd->orient == ORIENT_RIGHT) &&
		    cd->size >= PIXEL_SIZE_STANDARD) {
			if (strftime(hour, sizeof (hour), _("%I:%M\n%p"), tm) <= 0)
				strcpy (hour, "???");
		} else {
			if (strftime(hour, sizeof (hour), _("%I:%M %p"), tm) <= 0)
				strcpy (hour, "???");
		} 
		g_string_append(gs,hour);
	} else if (cd->hourformat == 24) {
		/* This format string is used, to display the actual time in
		   24 hour format.  */
		if (strftime(hour, sizeof (hour), _("%H:%M"), tm) <= 0)
			strcpy (hour, "???");
		g_string_append(gs,hour);
	}

	if (cd->showdate && !cd->unixtime && !cd->internettime) {
		if ((cd->orient == ORIENT_LEFT || cd->orient == ORIENT_RIGHT) &&
		    cd->size >= PIXEL_SIZE_STANDARD) {
			/* This format string is used, to display the actual day,
			   when showing a vertical panel.  For an explanation of
			   this format string type 'man strftime'.  */
			if (strftime(date, sizeof (date), _("%a\n%b %d"), tm) <= 0)
				strcpy (date, "???");
		} else {
			/* This format string is used, to display the actual day,
			   when showing a horizontal panel.  */
			if (strftime(date, sizeof (date), _("%a %b %d"), tm) <= 0)
				strcpy (date, "???");
		}
		if(cd->size < PIXEL_SIZE_STANDARD)
			g_string_append_c(gs,' ');
		else
			g_string_append_c(gs,'\n');
		g_string_append(gs,date);
	}
	
	/* Set the applets tooltip */
	if (cd->showtooltip && !cd->unixtime && !cd->internettime) {
		if (strftime(tooltip, sizeof (tooltip), _("%A, %B %d"), tm) <= 0)
			strcpy (tooltip, "???");
		applet_widget_set_tooltip(APPLET_WIDGET(cd->applet), tooltip);
	}
	
	/*if we are vertical, just make it char per line*/
	if ((cd->orient == ORIENT_LEFT || cd->orient == ORIENT_RIGHT) &&
	    cd->size < PIXEL_SIZE_STANDARD) {
		char *p;
		GString *gst = g_string_new("");
		for(p=gs->str;*p;p++) {
			if(p!=gs->str)
				g_string_append_c(gst,'\n');
			g_string_append_c(gst,*p);
		}
		g_string_free(gs,TRUE);
		gs = gst;
	}
	gtk_label_set_text(GTK_LABEL(cc->time), gs->str);
	applet_widget_queue_resize(APPLET_WIDGET(cd->applet));
	g_string_free(gs,TRUE);
}

static void
create_computer_clock_widget(GtkWidget ** clock, ClockUpdateFunc * update_func)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	ComputerClock *cc;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_widget_show(frame);

	cc = g_new0 (ComputerClock, 1);

	cc->align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_container_set_border_width(GTK_CONTAINER(cc->align), 3);
	gtk_container_add(GTK_CONTAINER(frame), cc->align);
	gtk_widget_show(cc->align);

	vbox = gtk_vbox_new(FALSE, FALSE);
	gtk_container_add(GTK_CONTAINER(cc->align), vbox);
	gtk_widget_show(vbox);

	cc->time = gtk_label_new("hmm?");

	gtk_box_pack_start_defaults(GTK_BOX(vbox), cc->time);
	gtk_widget_show(cc->time);

	gtk_object_set_user_data(GTK_OBJECT(frame), cc);
	gtk_signal_connect(GTK_OBJECT(frame), "destroy",
			   (GtkSignalFunc) free_data,
			   cc);

	*clock = frame;
	*update_func = computer_clock_update_func;
}

static void
destroy_clock(GtkWidget * widget, gpointer data)
{
	ClockData *cd = data;

	if (cd->timeout > 0)
		gtk_timeout_remove (cd->timeout);
	cd->timeout = 0;

	if(cd->props)
		gtk_widget_destroy(cd->props);
	cd->props = NULL;

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

	cd->props = NULL;

	cd->orient = ORIENT_UP;
	cd->size = PIXEL_SIZE_STANDARD;

	gtk_signal_connect(GTK_OBJECT(clock), "destroy",
			   (GtkSignalFunc) destroy_clock,
			   cd);
	/* Call the clock's update function so that it paints its first state */
	time(&current_time);
	(*cd->update_func) (cd, current_time);

	/* Install timeout handler */
	if (cd->gmt_time)
		tm = gmtime(&current_time);
	else
		tm = localtime(&current_time);
    
	if(cd->unixtime)
		cd->timeouttime = 1000;
        else if(cd->internettime)
                cd->timeouttime = INTERNETSECOND;
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
/*this is when the panel size changes */
static void
applet_change_pixel_size(GtkWidget * w, int size, gpointer data)
{
	ClockData *cd = data;
	time_t current_time;
	ComputerClock *cc;

	/* Adjust the padding on the text for small sizes of the panel (so the widget doesn't become bigger than the panel */
	cc = gtk_object_get_user_data(GTK_OBJECT(cd->clockw));
	gtk_container_set_border_width(GTK_CONTAINER(cc->align), ((size < PIXEL_SIZE_SMALL)? 0 : 3 ) );
	
	time(&current_time);
	cd->size = size;
	(*cd->update_func) (cd, current_time);
}

static void
copy_time (AppletWidget *applet, gpointer data)
{
	ClockData *cd = data;
	time_t current_time = time (NULL);
	struct tm *tm;
	char string[256];

	if (cd->gmt_time)
		tm = gmtime(&current_time);
	else
		tm = localtime(&current_time);

	if (cd->unixtime) {
		g_snprintf (string, sizeof(string), "%lu",
			    (unsigned long)current_time);
	} else if (cd->internettime) {
		float itime = get_itime (current_time);
		g_snprintf (string, sizeof (string), "@%3.2f", itime);
	} else if (cd->hourformat == 12) {
		/* This format string is used, to display the actual time in
		   12 hour format.  */
		if (strftime(string, sizeof (string), _("%I:%M %p"), tm) <= 0)
			strcpy (string, "???");
	} else if (cd->hourformat == 24) {
		/* This format string is used, to display the actual time in
		   24 hour format.  */
		if (strftime(string, sizeof (string), _("%H:%M"), tm) <= 0)
			strcpy (string, "???");
	}

	g_free (cd->copied_string);
	cd->copied_string = g_strdup (string);

	gtk_selection_owner_set (GTK_WIDGET (applet),
				 gdk_atom_intern ("CLIPBOARD", FALSE),
				 GDK_CURRENT_TIME);
}

static void
copy_date (AppletWidget *applet, gpointer data)
{
	ClockData *cd = data;
	time_t current_time = time (NULL);
	struct tm *tm;
	char string[256];

	if (cd->gmt_time)
		tm = gmtime(&current_time);
	else
		tm = localtime(&current_time);

	if (strftime(string, sizeof (string), _("%A, %B %d %Y"), tm) <= 0)
		strcpy (string, "???");
	
	g_free (cd->copied_string);
	cd->copied_string = g_strdup (string);

	gtk_selection_owner_set (GTK_WIDGET (applet),
                                 gdk_atom_intern ("CLIPBOARD", FALSE),
				 GDK_CURRENT_TIME);
}

/* current timestamp */
static void
copy_timestamp (AppletWidget *applet, gpointer data)
{
	ClockData *cd = data;
	time_t current_time = time (NULL);
	struct tm *tm;
	char string[256];

	tm = localtime(&current_time);

	if (strftime(string, sizeof (string),
		     /* RFC822 conformant date, likely not different for other
		      * locales I don't think */
		     _("%a, %d  %b  %Y %H:%M:%S %z"), tm) <= 0)
		strcpy (string, "???");
	
	g_free (cd->copied_string);
	cd->copied_string = g_strdup (string);

	gtk_selection_owner_set (GTK_WIDGET (applet),
                                 gdk_atom_intern ("CLIPBOARD", FALSE),
				 GDK_CURRENT_TIME);
}

static void
selection_handle (GtkWidget *widget, 
		  GtkSelectionData *selection_data,
		  int info, int time,
		  gpointer data)
{
	ClockData *cd = data;

	if (info == TARGET_STRING) {
		gtk_selection_data_set (selection_data,
					GDK_SELECTION_TYPE_STRING,
					8 * sizeof(gchar),
					(guchar *)cd->copied_string,
					strlen (cd->copied_string));
	} else if ((info == TARGET_TEXT) || (info == TARGET_COMPOUND_TEXT)) {
		guchar *text;
		GdkAtom encoding;
		gint format;
		gint new_length;

		gdk_string_to_compound_text (cd->copied_string,
					     &encoding, &format,
					     &text, &new_length);
		gtk_selection_data_set (selection_data, encoding, format,
					text, new_length);
		gdk_free_compound_text (text);
	}
}

GtkWidget *
make_clock_applet(const gchar * goad_id)
{
	ClockData *cd;
	GtkWidget *applet;
	GtkTargetEntry targets[] = {
		{ "STRING", TARGET_STRING },
		{ "TEXT",   TARGET_TEXT }, 
		{ "COMPOUND_TEXT", TARGET_COMPOUND_TEXT }
	};
	gint n_targets = sizeof(targets) / sizeof(targets[0]);

	applet = applet_widget_new(goad_id);
	if (!applet) {
		g_warning(_("Can't create applet!\n"));
		return NULL;
	}

	cd = g_new0 (ClockData, 1);

	cd->copied_string = NULL;

	gnome_config_push_prefix(APPLET_WIDGET(applet)->privcfgpath);

	/* A kluge to allow translation of default hour format to 12 or
	 * 24 hour format */
	{
		/* Do NOT translate the clock/hourformat part.  What you
		 * should change is the 12.  If your country code should use
		 * 12 hour format by default, leave it at 12, otherwise use 24
		 * for 24 hour format.  Those are the only two supported */
		char *transl = _("clock/hourformat=12");
		/* sanity */
		if (strncmp (transl, "clock/hourformat=",
			     strlen ("clock/hourformat=") != 0)) {
			g_warning("Clock applet string clock/hourformat=12 "
				  "was not translated correctly in this locale.");
			transl = "clock/hourformat=12";
		}
		cd->hourformat = gnome_config_get_int(transl);

		/* support the old syntax */
		if (cd->hourformat == 0)
			cd->hourformat = 12;
		else if (cd->hourformat == 1)
			cd->hourformat = 24;

		/* make sure it's a sane value, we use 24 otherwise */
		if (cd->hourformat != 12 && cd->hourformat != 24)
			cd->hourformat = 24;
	}
	/* if on a small screen don't show data by default */
	if(gdk_screen_width()<=800)
		cd->showdate = gnome_config_get_int("clock/showdate=0");
	else
		cd->showdate = gnome_config_get_int("clock/showdate=1");

	cd->unixtime = gnome_config_get_int("clock/unixtime=0");
	cd->internettime = gnome_config_get_int("clock/internettime=0");
	/* if on a small screen show tooltip with date by default */
	if(gdk_screen_width()<=800)
		cd->showtooltip = gnome_config_get_int("clock/showtooltip=1");
	else
		cd->showtooltip = gnome_config_get_int("clock/showtooltip=0");
	cd->gmt_time = gnome_config_get_int("clock/gmt_time=0");
	gnome_config_pop_prefix();

	create_clock_widget(cd, applet);

	/*we have to bind change_orient before we do applet_widget_add 
	  since we need to get an initial change_orient signal to set our
	  initial oriantation, and we get that during the _add call */
	gtk_signal_connect(GTK_OBJECT(applet), "change_orient",
			   GTK_SIGNAL_FUNC(applet_change_orient),
			   cd);
	/*similiar to the above in semantics*/
	gtk_signal_connect(GTK_OBJECT(applet), "change_pixel_size",
			   GTK_SIGNAL_FUNC(applet_change_pixel_size),
			   cd);

	gtk_widget_show(cd->clockw);
	applet_widget_add(APPLET_WIDGET(applet), cd->clockw);
	gtk_widget_show(applet);

	gtk_signal_connect(GTK_OBJECT(applet), "save_session",
			   GTK_SIGNAL_FUNC(applet_save_session),
			   cd);


	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "properties",
					      GNOME_STOCK_MENU_PROP,
					      _("Properties..."),
					      clock_properties,
					      cd);

	applet_widget_register_stock_callback (APPLET_WIDGET (applet),
					       "copy_time",
					       GNOME_STOCK_MENU_COPY,
					       _("Copy time"),
					       copy_time,
					       cd);

	applet_widget_register_stock_callback (APPLET_WIDGET (applet),
					       "copy_date",
					       GNOME_STOCK_MENU_COPY,
					       _("Copy date"),
					       copy_date,
					       cd);

	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "help",
					      GNOME_STOCK_PIXMAP_HELP,
					      _("Help"),
					      help_cb,
					      "index.html" );

	applet_widget_register_stock_callback(APPLET_WIDGET(applet),
					      "about",
					      GNOME_STOCK_MENU_ABOUT,
					      _("About..."),
					      clock_about,
					      NULL);

	gtk_selection_add_targets (GTK_WIDGET (applet),
                                   gdk_atom_intern ("CLIPBOARD", FALSE),
				   targets, n_targets);

        gtk_signal_connect (GTK_OBJECT (applet), "selection_get",
			    GTK_SIGNAL_FUNC (selection_handle), cd);

	return applet;
}

static void
apply_properties(GtkWidget * widget, gint button_num, gpointer data)
{
	ClockData *cd = data;
	time_t current_time;
	struct tm *tm;

	cd->hourformat = cd->prop_hourformat;
	cd->showdate = cd->prop_showdate;
	cd->unixtime = cd->prop_unixtime;
   	cd->internettime = cd->prop_internettime;
	cd->gmt_time = cd->prop_gmt_time;
	cd->showtooltip = cd->prop_showtooltip;

	/* Call the clock's update function so that it paints its first state */
	time(&current_time);
	(*cd->update_func) (cd, current_time);
	
	gtk_timeout_remove(cd->timeout);

	/* Install timeout handler */
	if (cd->gmt_time)
		tm = gmtime(&current_time);
	else
		tm = localtime(&current_time);

	if(cd->unixtime)
		cd->timeouttime = 1000;
        else if(cd->internettime)
                cd->timeouttime = INTERNETSECOND;
	else
		cd->timeouttime = 36000-(tm->tm_sec*600);
	cd->timeout = gtk_timeout_add(cd->timeouttime,
				      clock_timeout_callback,
				      cd);		      
	if(!cd->showtooltip || cd->unixtime || cd->internettime)
  		applet_widget_set_tooltip(APPLET_WIDGET(cd->applet), "");
				      
/* gtk_widget_queue_resize (cd->clockw);*/
}

static void
close_properties(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->props = NULL;
}

static void
set_hour_format_cb(GtkWidget * w, gpointer data)
{
	g_return_if_fail(w != NULL);
	if(GTK_TOGGLE_BUTTON(w)->active) {
		ClockData *cd = gtk_object_get_user_data(GTK_OBJECT(w));
		cd->prop_hourformat = GPOINTER_TO_INT(data);
		gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->props));
	}
}

static void
set_show_date_cb(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->prop_showdate = GTK_TOGGLE_BUTTON(w)->active;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->props));
}

static void
set_datasensitive_cb(GtkWidget * w, GtkWidget *wid)
{
	gtk_widget_set_sensitive(wid,!(GTK_TOGGLE_BUTTON(w)->active));
}

static void
set_internettime_cb(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->prop_internettime = GTK_TOGGLE_BUTTON(w)->active;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->props));
}

static void
set_unixtime_cb(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->prop_unixtime = GTK_TOGGLE_BUTTON(w)->active;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->props));
}

static void
set_gmt_time_cb(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->prop_gmt_time = GTK_TOGGLE_BUTTON(w)->active;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->props));
}

static void
set_show_tooltip_cb(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->prop_showtooltip = GTK_TOGGLE_BUTTON(w)->active;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->props));
}

static void
clock_properties(AppletWidget * applet, gpointer data)
{
        GtkWidget *hbox;
        GtkWidget *vbox;
        GtkWidget *hour_frame;
	GtkWidget *table;
	GtkWidget *twelvehour;
	GtkWidget *twentyfourhour;
	GtkWidget *showdate;
	GtkWidget *showtooltip;
	GtkWidget *unixtime;
	GtkWidget *internettime;
	GtkWidget *use_gmt_time;
	ClockData *cd = data;


	if (cd->props != NULL) {
		gtk_widget_show_now (cd->props);
		gdk_window_raise (cd->props->window);
		return;
	}

	cd->props = gnome_property_box_new();
	gtk_window_set_title (GTK_WINDOW (cd->props),
			      _("Clock properties"));
	gtk_window_set_wmclass (GTK_WINDOW (cd->props), "clock", "Clock");
	gnome_window_icon_set_from_file (GTK_WINDOW (cd->props),
					 GNOME_ICONDIR"/gnome-clock.png");

	hbox = gtk_hbox_new(FALSE, GNOME_PAD);

	hour_frame = gtk_frame_new(_("Time Format"));
	gtk_container_set_border_width(GTK_CONTAINER(hour_frame), GNOME_PAD);
	gtk_box_pack_start (GTK_BOX(hbox), hour_frame, FALSE, FALSE, 0);

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
	case 12:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(twelvehour),
					    TRUE);
		break;

	case 24:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(twentyfourhour),
					    TRUE);
		break;
	}

	cd->prop_hourformat = cd->hourformat;

	gtk_signal_connect(GTK_OBJECT(twelvehour),
			   "toggled",
			   (GtkSignalFunc) set_hour_format_cb,
			   (gpointer) 12);
	gtk_signal_connect(GTK_OBJECT(twentyfourhour),
			   "toggled",
			   (GtkSignalFunc) set_hour_format_cb,
			   (gpointer) 24);
	
	vbox = gtk_vbox_new(FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start_defaults(GTK_BOX(hbox), vbox);
	gtk_widget_show(vbox);

	showdate = gtk_check_button_new_with_label(_("Show date in applet"));
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

	showtooltip = gtk_check_button_new_with_label(_("Show date in tooltip"));
	gtk_box_pack_start_defaults(GTK_BOX(vbox), showtooltip);
	gtk_widget_show(showtooltip);
	
	if (cd->showtooltip)
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showtooltip),
				      TRUE);
	cd->prop_showtooltip = cd->showtooltip;
	
	gtk_signal_connect(GTK_OBJECT(showtooltip),
			   "toggled",
			   (GtkSignalFunc) set_show_tooltip_cb,
			   data);	

	use_gmt_time = gtk_check_button_new_with_label(_("Use GMT"));
	gtk_box_pack_start_defaults(GTK_BOX(vbox), use_gmt_time);
	gtk_widget_show(use_gmt_time);
	
	if (cd->gmt_time)
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_gmt_time),
				      TRUE);
	cd->prop_gmt_time = cd->gmt_time;
	
	gtk_signal_connect(GTK_OBJECT(use_gmt_time),
			   "toggled",
			   (GtkSignalFunc) set_gmt_time_cb,
			   data);	

	unixtime = gtk_check_button_new_with_label(_("Unix time"));
	gtk_box_pack_start_defaults(GTK_BOX(vbox), unixtime);
	gtk_widget_show(unixtime);

        internettime = gtk_check_button_new_with_label(_("Internet time"));
	gtk_box_pack_start_defaults(GTK_BOX(vbox), internettime);
	gtk_widget_show(internettime);
   
        gtk_signal_connect(GTK_OBJECT(unixtime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   hour_frame);
	gtk_signal_connect(GTK_OBJECT(unixtime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   showdate);
	gtk_signal_connect(GTK_OBJECT(unixtime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   showtooltip);
	gtk_signal_connect(GTK_OBJECT(unixtime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   use_gmt_time);
	gtk_signal_connect(GTK_OBJECT(unixtime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   internettime);
			   
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(unixtime),
				     cd->unixtime);
	cd->prop_unixtime = cd->unixtime;


/* internet time */
	gtk_signal_connect(GTK_OBJECT(internettime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   hour_frame);
	gtk_signal_connect(GTK_OBJECT(internettime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   showdate);
	gtk_signal_connect(GTK_OBJECT(internettime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   showtooltip);
	gtk_signal_connect(GTK_OBJECT(internettime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   use_gmt_time);   
	gtk_signal_connect(GTK_OBJECT(internettime), "toggled",
			   (GtkSignalFunc) set_datasensitive_cb,
			   unixtime);   
   
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(internettime),
				     cd->internettime);
	cd->prop_internettime = cd->internettime;
/* */
   
	gtk_signal_connect(GTK_OBJECT(unixtime), "toggled",
			   (GtkSignalFunc) set_unixtime_cb,
			   data);
	gtk_signal_connect(GTK_OBJECT(internettime), "toggled",
			   (GtkSignalFunc) set_internettime_cb,
			   data);

	gtk_widget_show(hour_frame);
	gtk_widget_show(hbox);

	gnome_property_box_append_page(GNOME_PROPERTY_BOX(cd->props), hbox,
				       gtk_label_new(_("Clock")));
	gtk_signal_connect(GTK_OBJECT(cd->props), "apply",
			   GTK_SIGNAL_FUNC(apply_properties), data);
	gtk_signal_connect(GTK_OBJECT(cd->props), "destroy",
			   GTK_SIGNAL_FUNC(close_properties), data);
	gtk_signal_connect(GTK_OBJECT(cd->props), "help",
			   GTK_SIGNAL_FUNC(phelp_cb),
			   "index.html#CLOCK-PREFS");

	gtk_widget_show(cd->props);
}

static void
help_cb (AppletWidget *w, gpointer data)
{
	GnomeHelpMenuEntry help_entry = { "clock_applet" };

	help_entry.path = data;
	gnome_help_display (NULL, &help_entry);
}

static void
phelp_cb (GtkWidget *w, gint tab, gpointer data)
{
	help_cb (NULL, data);
}

static void
clock_about (AppletWidget *applet, gpointer data)
{
	static GtkWidget   *about     = NULL;
	static const gchar *authors[] =
	{
		"Miguel de Icaza <miguel@kernel.org>",
		"Federico Mena <quartic@gimp.org>",
		"Stuart Parmenter <pavlov@innerx.net>",
		"George Lebl <jirka@5z.com>",
		NULL
	};

	if (about != NULL)
	{
		gtk_widget_show_now (about);
		gdk_window_raise (about->window);
		return;
	}
	
	about = gnome_about_new (_("Clock Applet"), "1.0",
				 _("(c) 1998-2001 the Free Software Foundation"),
				 authors,
				 _("The clock applet gives your panel a lightweight and simple display of the date and time"),
				 GNOME_ICONDIR"/gnome-clock.png");
	gtk_window_set_wmclass (GTK_WINDOW (about), "clock", "Clock");
	gnome_window_icon_set_from_file (GTK_WINDOW (about),
					 GNOME_ICONDIR"/gnome-clock.png");
	gtk_signal_connect (GTK_OBJECT(about), "destroy",
			    GTK_SIGNAL_FUNC(gtk_widget_destroyed), &about);
	gtk_widget_show (about);
}

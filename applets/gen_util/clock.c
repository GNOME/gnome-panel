/*
 * GNOME time/date display module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 *          Stuart Parmenter
 *          Alexander Larsson
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

#define G_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GNOME_DISABLE_DEPRECATED

#include <panel-applet.h>
#include <panel-applet-gconf.h>

#include <gtk/gtk.h>
#include <libbonobo.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnome/libgnome.h>
#include <gconf/gconf-client.h>

#include "clock.h"

#define INTERNETSECOND (864/4)

static const char* KEY_HOUR_FORMAT	= "hour_format";
static const char* KEY_SHOW_DATE 	= "show_date";
static const char* KEY_SHOW_TOOLTIP	= "show_tooltip";
static const char* KEY_GMT_TIME		= "gmt_time";
static const char* KEY_UNIX_TIME	= "unix_time";
static const char* KEY_INTERNET_TIME	= "internet_time";

extern GtkTooltips *panel_tooltips;

typedef struct _ClockData ClockData;

typedef void (*ClockUpdateFunc) (ClockData *, time_t);

struct _ClockData {
	GtkWidget *applet;
	GtkWidget *clockw;
  
	guint timeout;
	int timeouttime;
	int hourformat;
        gboolean showdate;
	gboolean unixtime;
	gboolean internettime;
	gboolean gmt_time;
	gboolean showtooltip;
	ClockUpdateFunc update_func;
	PanelAppletOrient orient;
	int size;

	GtkWidget *props;
};

typedef struct {
	GtkWidget *time; /*the time label*/
	GtkWidget *align; /* Needed for changing the padding */
} ComputerClock;


static void display_properties_dialog (BonoboUIComponent *uic,
				       ClockData         *cd,
				       const gchar       *verbname);
static void display_help_dialog       (BonoboUIComponent *uic,
				       ClockData         *cd,
				       const gchar       *verbname);
static void display_about_dialog      (BonoboUIComponent *uic,
				       ClockData         *cd,
				       const gchar       *verbname);

static int
clock_timeout_callback (gpointer data)
{
	ClockData *cd = data;
	time_t current_time;

	time (&current_time);

	(*cd->update_func) (cd, current_time);

	if (cd->unixtime) {
		if (cd->timeouttime != 1000) {
			cd->timeouttime = 1000;
			cd->timeout = g_timeout_add (cd->timeouttime,
						     clock_timeout_callback,
						     cd);
			return FALSE;
		}
	} else if(cd->internettime) {
		if (cd->timeouttime != INTERNETSECOND) {
			cd->timeouttime = INTERNETSECOND;
			cd->timeout = g_timeout_add (cd->timeouttime,
						     clock_timeout_callback,
						     cd);
			return FALSE;
		}	   
	} else {
		if (cd->timeouttime != 36000) {
			cd->timeouttime = 36000;
			cd->timeout = g_timeout_add (cd->timeouttime,
						     clock_timeout_callback,
						     cd);
			return FALSE;
		}
	}

	return TRUE;
}

static float
get_itime (time_t current_time)
{
	struct tm *tm;
	float itime;
	time_t bmt;

	bmt = current_time + 3600;
	tm = gmtime (&bmt);
	itime = (tm->tm_hour*60 + tm->tm_min + tm->tm_sec/60.0)*1000.0/1440.0;

	return itime;
}

static void
computer_clock_update_func (ClockData * cd,
			    time_t current_time)
{
	ComputerClock *cc;
	struct tm *tm;
	GString *gs = g_string_new("");
	char date[256], hour[256];
	char *utf8;
	
	cc = g_object_get_data (G_OBJECT (cd->clockw), "cc");

	if (cd->gmt_time)
		tm = gmtime (&current_time);
	else
		tm = localtime (&current_time);

	if (cd->unixtime) {
		if ((cd->orient == PANEL_APPLET_ORIENT_LEFT ||
		     cd->orient == PANEL_APPLET_ORIENT_RIGHT) &&
		    cd->size >= GNOME_Vertigo_PANEL_MEDIUM) {
			g_snprintf (hour, sizeof(hour), "%lu\n%05lu",
				    (unsigned long)(current_time / 100000L),
				    (unsigned long)(current_time % 100000L));
		} else {
			g_snprintf (hour, sizeof(hour), "%lu", (unsigned long)current_time);
		} 
		g_string_append (gs, hour);
	} else if (cd->internettime) {
		float itime = get_itime (current_time);
		g_snprintf (hour, sizeof(hour), "@%3.2f", itime);
		g_string_append (gs, hour);
	} else if (cd->hourformat == 12) {
		/* This format string is used, to display the actual time in
		   12 hour format.  */
		if ((cd->orient == PANEL_APPLET_ORIENT_LEFT ||
		     cd->orient == PANEL_APPLET_ORIENT_RIGHT) &&
		    cd->size >= GNOME_Vertigo_PANEL_MEDIUM) {
			if (strftime (hour, sizeof (hour), _("%I:%M\n%p"), tm) <= 0)
				strcpy (hour, "???");
		} else {
			if (strftime (hour, sizeof (hour), _("%I:%M %p"), tm) <= 0)
				strcpy (hour, "???");
		} 
		g_string_append (gs, hour);
	} else if (cd->hourformat == 24) {
		/* This format string is used, to display the actual time in
		   24 hour format.  */
		if (strftime (hour, sizeof (hour), _("%H:%M"), tm) <= 0)
			strcpy (hour, "???");
		g_string_append (gs,hour);
	}

	if (cd->showdate && !cd->unixtime && !cd->internettime) {
		if ((cd->orient == PANEL_APPLET_ORIENT_LEFT ||
		     cd->orient == PANEL_APPLET_ORIENT_RIGHT) &&
		    cd->size >= GNOME_Vertigo_PANEL_MEDIUM) {
			/* This format string is used, to display the actual day,
			   when showing a vertical panel.  For an explanation of
			   this format string type 'man strftime'.  */
			if (strftime (date, sizeof (date), _("%a\n%b %d"), tm) <= 0)
				strcpy (date, "???");
		} else {
			/* This format string is used, to display the actual day,
			   when showing a horizontal panel.  */
			if (strftime (date, sizeof (date), _("%a %b %d"), tm) <= 0)
				strcpy (date, "???");
		}
		if (cd->size < GNOME_Vertigo_PANEL_MEDIUM)
			g_string_append_c (gs,' ');
		else
			g_string_append_c (gs,'\n');
		g_string_append (gs, date);
	}

	/* Set the applets tooltip */
	if (cd->showtooltip && !cd->unixtime && !cd->internettime) {
		if (strftime (date, sizeof (date), _("%A, %B %d"), tm) <= 0)
			strcpy (date, "???");
		utf8 = g_locale_to_utf8 (date, -1, NULL, NULL, NULL);
  		gtk_tooltips_set_tip (panel_tooltips, GTK_WIDGET (cd->applet),
				      utf8, NULL);
		g_free (utf8);
	}
	
	/*if we are vertical, just make it char per line */
	if ((cd->orient == PANEL_APPLET_ORIENT_LEFT ||
	     cd->orient == PANEL_APPLET_ORIENT_RIGHT) &&
	    cd->size < GNOME_Vertigo_PANEL_MEDIUM) {
		char *p;
		GString *gst = g_string_new ("");
		
		for(p = gs->str; *p; p++) {
			if (p != gs->str)
				g_string_append_c (gst, '\n');
			g_string_append_c (gst, *p);
		}
		g_string_free (gs, TRUE);
		gs = gst;
	}

	utf8 = g_locale_to_utf8 (gs->str, -1, NULL, NULL, NULL);

	gtk_label_set_text (GTK_LABEL (cc->time), utf8);
	g_string_free (gs, TRUE);
	g_free (utf8);
}

static void
refresh_clock (ClockData *cd)
{
	time_t current_time;
	
	time (&current_time);
	(*cd->update_func) (cd, current_time);
}

static void
refresh_clock_timeout(ClockData *cd)
{
	time_t current_time;
	struct tm *tm;
	
	if (cd->timeout)
		g_source_remove (cd->timeout);

	time (&current_time);
	(*cd->update_func) (cd, current_time);
	
	/* Install timeout handler */
	if (cd->gmt_time)
		tm = gmtime (&current_time);
	else
		tm = localtime (&current_time);

	if(cd->unixtime)
		cd->timeouttime = 1000;
        else if (cd->internettime)
                cd->timeouttime = INTERNETSECOND;
	else
		cd->timeouttime = 36000-(tm->tm_sec*600);
	
	cd->timeout = g_timeout_add (cd->timeouttime,
				     clock_timeout_callback,
				     cd);
}

static void
create_computer_clock_widget (GtkWidget ** clock, ClockUpdateFunc * update_func)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	ComputerClock *cc;

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_widget_show (frame);

	cc = g_new0 (ComputerClock, 1);

	cc->align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_container_set_border_width (GTK_CONTAINER (cc->align), 3);
	gtk_container_add (GTK_CONTAINER (frame), cc->align);
	gtk_widget_show (cc->align);

	vbox = gtk_vbox_new (FALSE, FALSE);
	gtk_container_add (GTK_CONTAINER (cc->align), vbox);
	gtk_widget_show (vbox);

	cc->time = gtk_label_new ("hmm?");
	gtk_label_set_justify (GTK_LABEL (cc->time), GTK_JUSTIFY_CENTER);

	gtk_box_pack_start_defaults (GTK_BOX (vbox), cc->time);
	gtk_widget_show (cc->time);

	g_object_set_data_full (G_OBJECT (frame), "cc", cc, g_free);

	*clock = frame;
	*update_func = computer_clock_update_func;
}

static void
destroy_clock(GtkWidget * widget, gpointer data)
{
	ClockData *cd = data;

	if (cd->timeout > 0)
		g_source_remove (cd->timeout);
	
	cd->timeout = 0;

	if(cd->props)
		gtk_widget_destroy (cd->props);
	
	cd->props = NULL;

	g_free (cd);
}

static void
create_clock_widget (ClockData *cd)
{
	GtkWidget *clock;

	/*FIXME: different clock types here */
	create_computer_clock_widget (&clock, &cd->update_func);

	cd->clockw = clock;

	cd->props = NULL;

	cd->orient = panel_applet_get_orient (PANEL_APPLET (cd->applet));
	cd->size = panel_applet_get_size (PANEL_APPLET (cd->applet));

	g_signal_connect (G_OBJECT(clock), "destroy",
			  G_CALLBACK (destroy_clock),
			  cd);
	
	/* Refresh the clock so that it paints its first state */
	refresh_clock_timeout(cd);
}

/* this is when the panel orientation changes */

static void
applet_change_orient (PanelApplet       *applet,
		      PanelAppletOrient  orient,
		      ClockData         *cd)
{
	time_t current_time;

	time (&current_time);
	cd->orient = orient;
	(*cd->update_func) (cd, current_time);
}

static void
applet_change_background (PanelApplet               *applet,
			  PanelAppletBackgroundType  type,
			  GdkColor                  *color,
			  const gchar               *pixmap,
			  ClockData                 *cd)
{
  if (type == PANEL_NO_BACKGROUND)
    {
      GtkRcStyle *rc_style = gtk_rc_style_new ();

      gtk_widget_modify_style (cd->applet, rc_style);
    }
  else if (type == PANEL_COLOR_BACKGROUND)
    {
      gtk_widget_modify_bg (cd->applet,
			    GTK_STATE_NORMAL,
			    color);
    } else { /* pixmap */
      /* FIXME: Handle this when the panel support works again */
    }
}


/* this is when the panel size changes */
static void
applet_change_pixel_size (PanelApplet *applet,
			  gint         size,
			  ClockData   *cd)
{
	time_t current_time;
	ComputerClock *cc;

	/* Adjust the padding on the text for small sizes of the panel (so the widget doesn't become bigger than the panel */
	cc = g_object_get_data (G_OBJECT (cd->clockw), "cc");
	gtk_container_set_border_width (GTK_CONTAINER (cc->align),
					((size < GNOME_Vertigo_PANEL_SMALL)? 0 : 3 ) );
	
	time (&current_time);
	cd->size = size;
	(*cd->update_func) (cd, current_time);
}


static void
copy_time (BonoboUIComponent *uic,
	   ClockData         *cd,
	   const gchar       *verbname)
{
	time_t current_time = time (NULL);
	struct tm *tm;
	char string[256];
	char *utf8;

	if (cd->gmt_time)
		tm = gmtime (&current_time);
	else
		tm = localtime (&current_time);

	if (cd->unixtime) {
		g_snprintf (string, sizeof(string), "%lu",
			    (unsigned long)current_time);
	} else if (cd->internettime) {
		float itime = get_itime (current_time);
		g_snprintf (string, sizeof (string), "@%3.2f", itime);
	} else if (cd->hourformat == 12) {
		/* This format string is used, to display the actual time in
		   12 hour format.  */
		if (strftime (string, sizeof (string), _("%I:%M %p"), tm) <= 0)
			strcpy (string, "???");
	} else if (cd->hourformat == 24) {
		/* This format string is used, to display the actual time in
		   24 hour format.  */
		if (strftime (string, sizeof (string), _("%H:%M"), tm) <= 0)
			strcpy (string, "???");
	}

	utf8 = g_locale_to_utf8 (string, -1, NULL, NULL, NULL);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				utf8, -1);
	g_free (utf8);
}

static void
copy_date (BonoboUIComponent *uic,
	   ClockData         *cd,
	   const gchar       *verbname)
{
	time_t current_time = time (NULL);
	struct tm *tm;
	char string[256];
	char *utf8;

	if (cd->gmt_time)
		tm = gmtime (&current_time);
	else
		tm = localtime (&current_time);

	if (strftime (string, sizeof (string), _("%A, %B %d %Y"), tm) <= 0)
		strcpy (string, "???");
	
	utf8 = g_locale_to_utf8 (string, -1, NULL, NULL, NULL);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				utf8, -1);
	g_free (utf8);
}


/* current timestamp */
static void
copy_timestamp (BonoboUIComponent *uic,
		ClockData         *cd,
		const gchar       *verbname)
{
	time_t current_time = time (NULL);
	struct tm *tm;
	char string[256];
	char *utf8;

	tm = localtime (&current_time);

	if (strftime (string, sizeof (string),
		     /* RFC822 conformant date, likely not different for other
		      * locales I don't think */
		     _("%a, %d  %b  %Y %H:%M:%S %z"), tm) <= 0)
		strcpy (string, "???");
	
	utf8 = g_locale_to_utf8 (string, -1, NULL, NULL, NULL);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				utf8, -1);
	g_free (utf8);
}

static const BonoboUIVerb clock_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ClockProperties", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("ClockHelp",       display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("ClockAbout",      display_about_dialog),
	BONOBO_UI_UNSAFE_VERB ("ClockCopyTime",   copy_time),
	BONOBO_UI_UNSAFE_VERB ("ClockCopyDate",   copy_date),
        BONOBO_UI_VERB_END
};

static void
hour_format_changed (GConfClient  *client,
		     guint         cnxn_id,
		     GConfEntry   *entry,
		     ClockData *clock)
{
	int value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_INT)
		return;

	value = gconf_value_get_int (entry->value);
	
	if (value == 12 || value == 24)
		clock->hourformat = value;
	else
		clock->hourformat = 12;

	refresh_clock (clock);
}

static void
show_date_changed (GConfClient  *client,
		   guint         cnxn_id,
		   GConfEntry   *entry,
		   ClockData *clock)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	clock->showdate = (value != 0);
	refresh_clock (clock);
}

static void
internet_time_changed (GConfClient  *client,
		       guint         cnxn_id,
		       GConfEntry   *entry,
		       ClockData *clock)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	clock->internettime = (value != 0);
	refresh_clock_timeout (clock);
}

static void
unix_time_changed (GConfClient  *client,
		  guint         cnxn_id,
		  GConfEntry   *entry,
		  ClockData *clock)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	clock->unixtime = (value != 0);
	refresh_clock_timeout (clock);
}

static void
gmt_time_changed (GConfClient  *client,
		  guint         cnxn_id,
		  GConfEntry   *entry,
		  ClockData *clock)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	clock->gmt_time = (value != 0);
	refresh_clock_timeout (clock);
}

static void
show_tooltip_changed (GConfClient  *client,
		      guint         cnxn_id,
		      GConfEntry   *entry,
		      ClockData *clock)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	clock->showtooltip = (value != 0);
	refresh_clock (clock);
}

static void
setup_gconf (ClockData *clock)
{
	GConfClient *client;
	char *key;

	client = gconf_client_get_default ();

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_HOUR_FORMAT);
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)hour_format_changed,
				clock,
				NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_SHOW_DATE);
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)show_date_changed,
				clock,
				NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_SHOW_TOOLTIP);
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)show_tooltip_changed,
				clock,
				NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_GMT_TIME);
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)gmt_time_changed,
				clock,
				NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_UNIX_TIME);
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)unix_time_changed,
				clock,
				NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_INTERNET_TIME);
	gconf_client_notify_add(client, key,
				(GConfClientNotifyFunc)internet_time_changed,
				clock,
				NULL, NULL);
	g_free (key);
}

gboolean
fill_clock_applet(PanelApplet *applet)
{
	ClockData *cd;
	GError *error;
	
	panel_applet_add_preferences (applet, "/schemas/apps/clock-applet/prefs", NULL);
	
	cd = g_new0 (ClockData, 1);

	cd->applet = GTK_WIDGET (applet);

	setup_gconf (cd);

	error = NULL;
	cd->hourformat = panel_applet_gconf_get_bool (applet, KEY_HOUR_FORMAT, &error);
	if (error) {
		g_error_free (error);
		/* Do NOT translate the clock/hourformat= part.  What you
		 * should change is the number 12.  If your country code should use
		 * 12 hour format by default, leave it at 12, otherwise use 24
		 * for 24 hour format.  Those are the only two supported */
		char *transl = _("clock/hourformat=12");
		const int len = strlen ("clock/hourformat=");
		/* sanity */
		if (strncmp (transl, "clock/hourformat=", len)) {
			g_warning("Clock applet string clock/hourformat=12 "
				  "was not translated correctly in this locale.");
			cd->hourformat = 12;
		} else {
			cd->hourformat = atoi (transl + len);
			if (cd->hourformat != 12 && cd->hourformat != 24)
				cd->hourformat = 12;
		}
	}

	error = NULL;
	cd->showdate = panel_applet_gconf_get_bool (applet, KEY_SHOW_DATE, &error);
	if (error) {
		g_error_free (error);
		/* if on a small screen don't show data by default */
		if (gdk_screen_width () <= 800)
			cd->showdate = FALSE;
		else
			cd->showdate = TRUE;
	}

	error = NULL;
	cd->showtooltip = panel_applet_gconf_get_bool (applet, KEY_SHOW_TOOLTIP, &error);
	if (error) {
		g_error_free (error);
		cd->showtooltip = TRUE; /* Default value*/
	}

	cd->gmt_time = panel_applet_gconf_get_bool (applet, KEY_GMT_TIME, NULL);
	cd->unixtime = panel_applet_gconf_get_bool (applet, KEY_UNIX_TIME, NULL);
	cd->internettime = panel_applet_gconf_get_bool (applet, KEY_INTERNET_TIME, NULL);

	create_clock_widget (cd);

	gtk_container_add (GTK_CONTAINER (cd->applet), cd->clockw);

	gtk_widget_show (cd->applet);

	/* FIXME: Update this comment. */
	/* we have to bind change_orient before we do applet_widget_add 
	   since we need to get an initial change_orient signal to set our
	   initial oriantation, and we get that during the _add call */
	g_signal_connect (G_OBJECT (cd->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient),
			  cd);

	/* similiar to the above in semantics*/
	g_signal_connect (G_OBJECT (cd->applet),
			  "change_size",
			  G_CALLBACK (applet_change_pixel_size),
			  cd);

	/* FIXME: initial background, this needs some panel-applet voodoo */
	g_signal_connect (G_OBJECT (cd->applet),
			  "change_background",
			  G_CALLBACK (applet_change_background),
			  cd);

	panel_applet_setup_menu_from_file (PANEL_APPLET (cd->applet),
					   NULL,
					   "GNOME_ClockApplet.xml",
					   NULL,
					   clock_menu_verbs,
					   cd);
	
	return TRUE;
}

static void
set_datasensitive_cb (GtkWidget *w,
		      GtkWidget *wid)
{
	gtk_widget_set_sensitive (wid, !(GTK_TOGGLE_BUTTON (w)->active));
}

static void
set_hour_format_cb (GtkWidget *w,
		    gpointer data)
{
	if (GTK_TOGGLE_BUTTON (w)->active) {
		ClockData *clock = g_object_get_data (G_OBJECT (w), "user_data");
		panel_applet_gconf_set_int (PANEL_APPLET (clock->applet),
					    KEY_HOUR_FORMAT,
					    GPOINTER_TO_INT (data),
					    NULL);
	}
}

static void
set_show_date_cb (GtkWidget *w,
		  ClockData *clock)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_SHOW_DATE,
				     GTK_TOGGLE_BUTTON (w)->active,
				     NULL);
}

static void
set_internettime_cb (GtkWidget *w,
		     ClockData *clock)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_INTERNET_TIME,
				     GTK_TOGGLE_BUTTON (w)->active,
				     NULL);
}

static void
set_unixtime_cb (GtkWidget *w,
		 ClockData *clock)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_UNIX_TIME,
				     GTK_TOGGLE_BUTTON (w)->active,
				     NULL);
}

static void
set_gmt_time_cb (GtkWidget *w,
		 ClockData *clock)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_GMT_TIME,
				     GTK_TOGGLE_BUTTON (w)->active,
				     NULL);
}

static void
set_show_tooltip_cb (GtkWidget * w,
		     ClockData *clock)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_SHOW_TOOLTIP,
				     GTK_TOGGLE_BUTTON (w)->active,
				     NULL);
}

static void
properties_response_cb (GtkWidget *widget, gint id, gpointer data)
{
	
	if (id == GTK_RESPONSE_HELP) {
		GError *error = NULL;

		gnome_help_display_desktop (NULL, "clock", "clock",
				            "CLOCK-SETTINGS", &error);
		if (error) {
			g_warning ("help error: %s\n", error->message);
			g_error_free (error);
		}
	} else {
		gtk_widget_destroy (widget);
	}
	
	return;
}

static void 
display_properties_dialog (BonoboUIComponent *uic,
			   ClockData         *cd,
			   const gchar       *verbname)
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
	GdkPixbuf *pixbuf;
	gchar *file;

	if (cd->props != NULL) {
		gtk_window_present (GTK_WINDOW (cd->props));
		return;
	}

	cd->props = gtk_dialog_new_with_buttons (_("Clock Properties"), NULL, 0,
						 GTK_STOCK_HELP,
						 GTK_RESPONSE_HELP,
						 GTK_STOCK_CLOSE,
						 GTK_RESPONSE_CLOSE,
						 NULL);

	pixbuf = NULL;
	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-clock.png", TRUE, NULL);
	if (!file) {
		g_warning (G_STRLOC ": gnome-clock.png cannot be found");
		pixbuf = gdk_pixbuf_new_from_file (file, NULL);
	}

	if (pixbuf) {
		gtk_window_set_icon (GTK_WINDOW (cd->props), pixbuf);
		g_object_unref (pixbuf);
	}

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	
	hour_frame = gtk_frame_new (_("Time Format"));
	gtk_container_set_border_width (GTK_CONTAINER (hour_frame), GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (hbox), hour_frame, FALSE, FALSE, 0);

	table = gtk_table_new (2, 2, FALSE);
	gtk_container_add (GTK_CONTAINER (hour_frame), table);
	gtk_widget_show (table);

	gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD);
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);

	twelvehour = gtk_radio_button_new_with_label (NULL, _("12 hour"));
	gtk_table_attach (GTK_TABLE (table), twelvehour, 0, 1, 0, 1,
			  GTK_FILL, GTK_FILL | GTK_EXPAND,
			  0, 0);
	g_object_set_data (G_OBJECT (twelvehour), "user_data", cd);
	gtk_widget_show (twelvehour);

	twentyfourhour = gtk_radio_button_new_with_label (gtk_radio_button_get_group (GTK_RADIO_BUTTON (twelvehour)),
							  _("24 hour"));

	gtk_table_attach (GTK_TABLE (table), twentyfourhour, 0, 1, 1, 2,
			  GTK_FILL, GTK_FILL | GTK_EXPAND,
			  0, 0);
	g_object_set_data (G_OBJECT (twentyfourhour), "user_data", cd);
	gtk_widget_show (twentyfourhour);

	switch (cd->hourformat) {
	case 12:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (twelvehour),
					      TRUE);
		break;

	case 24:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (twentyfourhour),
					      TRUE);
		break;
	}

	g_signal_connect (G_OBJECT (twelvehour), "toggled",
			  G_CALLBACK (set_hour_format_cb),
			  GINT_TO_POINTER (12));
	g_signal_connect (G_OBJECT (twentyfourhour), "toggled",
			  G_CALLBACK (set_hour_format_cb),
			  GINT_TO_POINTER (24));
	
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), vbox);
	gtk_widget_show (vbox);

	showdate = gtk_check_button_new_with_label (_("Show date in applet"));
	gtk_box_pack_start_defaults (GTK_BOX (vbox), showdate);
	gtk_widget_show (showdate);
	
	if (cd->showdate)
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showdate),
					TRUE);
	g_signal_connect (G_OBJECT (showdate), "toggled",
			  G_CALLBACK (set_show_date_cb),
			  cd);	   

	showtooltip = gtk_check_button_new_with_label (_("Show date in tooltip"));
	gtk_box_pack_start_defaults (GTK_BOX (vbox), showtooltip);
	gtk_widget_show (showtooltip);
	
	if (cd->showtooltip)
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showtooltip),
					TRUE);
	g_signal_connect (G_OBJECT (showtooltip), "toggled",
			  G_CALLBACK (set_show_tooltip_cb),
			  cd);	

	use_gmt_time = gtk_check_button_new_with_label (_("Use GMT"));
	gtk_box_pack_start_defaults (GTK_BOX (vbox), use_gmt_time);
	gtk_widget_show (use_gmt_time);
	
	if (cd->gmt_time)
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_gmt_time),
					TRUE);
	g_signal_connect (G_OBJECT (use_gmt_time), "toggled",
			  G_CALLBACK (set_gmt_time_cb),
			  cd);	

	unixtime = gtk_check_button_new_with_label (_("Unix time"));
	gtk_box_pack_start_defaults (GTK_BOX (vbox), unixtime);
	gtk_widget_show (unixtime);

        internettime = gtk_check_button_new_with_label (_("Internet time"));
	gtk_box_pack_start_defaults (GTK_BOX (vbox), internettime);
	gtk_widget_show (internettime);
   
        g_signal_connect (G_OBJECT (unixtime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  hour_frame);
	g_signal_connect (G_OBJECT (unixtime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  showdate);
	g_signal_connect (G_OBJECT (unixtime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  use_gmt_time);
	g_signal_connect (G_OBJECT (unixtime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  internettime);
			   
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (unixtime),
				      cd->unixtime);

/* internet time */
	g_signal_connect (G_OBJECT (internettime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  hour_frame);
	g_signal_connect (G_OBJECT (internettime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  showdate);
	g_signal_connect (G_OBJECT (internettime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  use_gmt_time);   
	g_signal_connect (G_OBJECT (internettime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  unixtime);   
   
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (internettime),
				      cd->internettime);
   
	g_signal_connect (G_OBJECT (unixtime), "toggled",
			  G_CALLBACK (set_unixtime_cb),
			    cd);
	g_signal_connect (G_OBJECT (internettime), "toggled",
			  G_CALLBACK (set_internettime_cb),
			  cd);

	gtk_widget_show (hour_frame);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cd->props)->vbox), hbox, FALSE, FALSE, 0);

	
	g_signal_connect (G_OBJECT (cd->props), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &(cd->props));
	g_signal_connect (G_OBJECT (cd->props), "response",
			  G_CALLBACK (properties_response_cb), NULL);
				
	gtk_widget_show (cd->props);
}

static void
display_help_dialog (BonoboUIComponent *uic,
		     ClockData         *cd,
		     const gchar       *verbname)
{
	GError *error = NULL;

	gnome_help_display_desktop (NULL, "clock", "clock", NULL, &error);
	if (error) {
		g_warning ("help error: %s\n", error->message);
		g_error_free (error);
	}
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      ClockData         *cd,
		      const gchar       *verbname)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf;
	gchar *file;
	
	static const gchar *authors[] =
	{
		"Miguel de Icaza <miguel@kernel.org>",
		"Federico Mena <quartic@gimp.org>",
		"Stuart Parmenter <pavlov@innerx.net>",
		"George Lebl <jirka@5z.com>",
		"Alexander Larsson <alla@lysator.liu.se>",
		NULL
	};

	if (about != NULL)
	{
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	pixbuf = NULL;
	
	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-clock.png", TRUE, NULL);
	if (!file) {
		g_warning (G_STRLOC ": gnome-clock.png cannot be found");
		pixbuf = gdk_pixbuf_new_from_file (file, NULL);
	}

	about = gnome_about_new (_("Clock Applet"), "1.0",
				 _("(c) 1998-2001 the Free Software Foundation"),
				 _("The clock applet gives your panel a lightweight and simple display of the date and time"),
				 authors,
				 NULL, /* documenters */
				 NULL, /* translator_credits */
				 pixbuf);
	
	gtk_window_set_wmclass (GTK_WINDOW (about), "clock", "Clock");

	if (pixbuf) {
		gtk_window_set_icon (GTK_WINDOW (about), pixbuf);
		g_object_unref (pixbuf);
	}
	
	g_signal_connect (G_OBJECT(about), "destroy",
			  (GCallback)gtk_widget_destroyed, &about);
	
	gtk_widget_show (about);
}


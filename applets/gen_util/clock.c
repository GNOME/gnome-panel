/*
 * GNOME time/date display module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: George Lebl
 * 	    Gediminas Paulauskas
 */

#include "config.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <langinfo.h>

#include <panel-applet.h>
#include <panel-applet-gconf.h>

#include <gtk/gtk.h>
#include <libbonobo.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnome/libgnome.h>
#include <gconf/gconf-client.h>

#include "clock.h"

#include "egg-screen-help.h"

#define INTERNETSECOND (864)
#define INTERNETBEAT   (86400)

#define N_GCONF_PREFS 6
static const char* KEY_HOUR_FORMAT	= "hour_format";
static const char* KEY_SHOW_SECONDS 	= "show_seconds";
static const char* KEY_SHOW_DATE 	= "show_date";
static const char* KEY_GMT_TIME		= "gmt_time";
static const char* KEY_UNIX_TIME	= "unix_time";
static const char* KEY_INTERNET_TIME	= "internet_time";

typedef struct _ClockData ClockData;

struct _ClockData {
	/* widgets */
	GtkWidget *applet;
	GtkWidget *clockw;
	GtkWidget *props;
  
	/* preferences */
	int hourformat;
	gboolean showseconds;
	gboolean showdate;
	gboolean unixtime;
	gboolean internettime;
	gboolean gmt_time;
	
	/* runtime data */
	char *timeformat;
	guint timeout;
	int timeouttime;
	PanelAppletOrient orient;
	int size;

	guint listeners [N_GCONF_PREFS];
};

static void update_clock (ClockData * cd, time_t current_time);

static void set_atk_name_description (GtkWidget *widget,
                                      const char *name,
                                      const char *desc);
static void display_properties_dialog (BonoboUIComponent *uic,
				       ClockData         *cd,
				       const gchar       *verbname);
static void display_help_dialog       (BonoboUIComponent *uic,
				       ClockData         *cd,
				       const gchar       *verbname);
static void display_about_dialog      (BonoboUIComponent *uic,
				       ClockData         *cd,
				       const gchar       *verbname);

static void
set_tooltip (GtkWidget  *applet,
	     const char *tip)
{
	GtkTooltips *tooltips;

	tooltips = g_object_get_data (G_OBJECT (applet), "tooltips");
	if (!tooltips) {
		tooltips = gtk_tooltips_new ();
		g_object_ref (tooltips);
		gtk_object_sink (GTK_OBJECT (tooltips));
		g_object_set_data_full (
			G_OBJECT (applet), "tooltips", tooltips,
			(GDestroyNotify) g_object_unref);
	}

	gtk_tooltips_set_tip (tooltips, applet, tip, NULL);
}

static int
clock_timeout_callback (gpointer data)
{
	ClockData *cd = data;
	time_t current_time;

	time (&current_time);

	update_clock (cd, current_time);

	if (!cd->showseconds && !cd->unixtime) {
		if (!cd->internettime) {
			int sec = current_time % 60;
			if (sec != 0 || cd->timeouttime != 60000) {
				/* ensure next update is exactly on 0 seconds */
				cd->timeouttime = (60 - sec)*1000;
				cd->timeout = g_timeout_add (cd->timeouttime,
							     clock_timeout_callback,
							     cd);
				return FALSE;
			}
		} else {
			struct tm *tm;
			time_t bmt;
			long isec;

			/* BMT (Biel Mean Time) is GMT+1 */
			bmt = current_time + 3600;
			tm = gmtime (&bmt);
			isec = ((tm->tm_hour*3600 + tm->tm_min*60 + tm->tm_sec)*10) % 864;
			
			if (isec != 0 || cd->timeouttime != INTERNETBEAT) {
				/* ensure next update is exactly on beat limit */
				cd->timeouttime = (864 - isec)*100;
				cd->timeout = g_timeout_add (cd->timeouttime,
							     clock_timeout_callback,
							     cd);
				return FALSE;
			}
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

	/* BMT (Biel Mean Time) is GMT+1 */
	bmt = current_time + 3600;
	tm = gmtime (&bmt);
	itime = (tm->tm_hour*3600.0 + tm->tm_min*60.0 + tm->tm_sec)/86.4;

	return itime;
}

static void
update_timeformat (ClockData *cd)
{
 /* Show date in another line if panel is vertical, or
  * horizontal but large enough to hold two lines of text
  */
#define USE_TWO_LINE_FORMAT(cd) ((cd)->orient == PANEL_APPLET_ORIENT_LEFT  || \
                                 (cd)->orient == PANEL_APPLET_ORIENT_RIGHT || \
                                 (cd)->size >= GNOME_Vertigo_PANEL_MEDIUM)

	const char *time_format;
	const char *date_format;
	char       *clock_format;

	if (cd->hourformat == 12)
		time_format = cd->showseconds ? _("%l:%M:%S %p") : _("%l:%M %p");
	else
		time_format = cd->showseconds ? _("%H:%M:%S") : _("%H:%M");

	if (!cd->showdate)
		clock_format = g_strdup (time_format);

	else {
		date_format = _("%a %b %d");

		if (USE_TWO_LINE_FORMAT (cd))
			/* translators: reverse the order of these arguments
			 *              if the time should come before the
			 *              date on a clock in your locale.
			 */
			clock_format = g_strdup_printf (_("%s\n%s"),
							date_format, time_format);
		else
			/* translators: reverse the order of these arguments
			 *              if the time should come before the
			 *              date on a clock in your locale.
			 */
			clock_format = g_strdup_printf (_("%s, %s"),
							date_format, time_format);
	}

	g_free (cd->timeformat);
	cd->timeformat = g_locale_from_utf8 (clock_format, -1, NULL, NULL, NULL);
	g_free (clock_format);

#undef USE_TWO_LINE_FORMAT
}

/* sets accessible name and description for the widget */
static void
set_atk_name_description (GtkWidget  *widget,
			  const char *name,
			  const char *desc)
{
	AtkObject *obj;
	obj = gtk_widget_get_accessible (widget);

	/* return if gail is not loaded */
	if (!GTK_IS_ACCESSIBLE (obj))
		return;

	if (desc != NULL)
		atk_object_set_description (obj, desc);
	if (name != NULL)
		atk_object_set_name (obj, name);
} 

/* sets up ATK relation between the widgets */
static void
add_atk_relation (GtkWidget       *widget,
		  GSList          *list,
		  AtkRelationType  type)
{
	AtkRelationSet *relation_set;
	AtkObject      *aobj;

	aobj = gtk_widget_get_accessible (widget);
	if (!GTK_IS_ACCESSIBLE (aobj))
		return;

	relation_set = atk_object_ref_relation_set (aobj);

	if (list) {
		AtkObject  **accessible_array;
		AtkRelation *relation;
		GSList      *l;
		guint        len;
		int          i = 0;

		len = g_slist_length (list);
		accessible_array =
			(AtkObject **)g_malloc (sizeof (AtkObject *) * len);

		for (l = list, i = 0; l; l = l->next, i++)
			accessible_array [i] = gtk_widget_get_accessible (l->data);

		relation = atk_relation_new (accessible_array, len, type);
		atk_relation_set_add (relation_set, relation);
		g_object_unref (relation);
	}
}

static void
update_clock (ClockData * cd, time_t current_time)
{
	struct tm *tm;
	char date[256], hour[256];
	char *utf8, *loc;
	
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
	} else if (cd->internettime) {
		float itime = get_itime (current_time);
		if (cd->showseconds)
			g_snprintf (hour, sizeof (hour), "@%3.2f", itime);
		else
			g_snprintf (hour, sizeof (hour), "@%3.0f", itime);
	} else {
		if (strftime (hour, sizeof (hour), cd->timeformat, tm) <= 0)
			strcpy (hour, "???");
	}

	utf8 = g_locale_to_utf8 (hour, -1, NULL, NULL, NULL);
	gtk_label_set_text (GTK_LABEL (cd->clockw), utf8);
	g_free (utf8);

	/* Show date in tooltip */
	loc = g_locale_from_utf8 (_("%A %B %d"), -1, NULL, NULL, NULL);
	if (strftime (date, sizeof (date), loc, tm) <= 0)
		strcpy (date, "???");
	g_free (loc);

	utf8 = g_locale_to_utf8 (date, -1, NULL, NULL, NULL);
	set_tooltip (GTK_WIDGET (cd->applet), utf8);
	g_free (utf8);
}

static void
refresh_clock (ClockData *cd)
{
	time_t current_time;
	
	time (&current_time);
	update_clock (cd, current_time);
}

static void
refresh_clock_timeout(ClockData *cd)
{
	time_t current_time;
	
	update_timeformat (cd);

	if (cd->timeout)
		g_source_remove (cd->timeout);

	time (&current_time);
	update_clock (cd, current_time);
	
	if (cd->internettime) {
		if (cd->showseconds)
			cd->timeouttime = INTERNETSECOND;
		else {
			struct tm *tm;
			time_t bmt;
			long isec;

			/* BMT (Biel Mean Time) is GMT+1 */
			bmt = current_time + 3600;
			tm = gmtime (&bmt);
			isec = ((tm->tm_hour*3600 + tm->tm_min*60 + tm->tm_sec)*10) % 864;
			cd->timeouttime = (864 - isec)*100;
		}
	}
	else if(cd->unixtime || cd->showseconds)
		cd->timeouttime = 1000;
	else
		cd->timeouttime = (60 - current_time % 60)*1000;
	
	cd->timeout = g_timeout_add (cd->timeouttime,
	                             clock_timeout_callback,
	                             cd);
}

static void
destroy_clock(GtkWidget * widget, ClockData *cd)
{
	GConfClient *client;
	int          i;

	client = gconf_client_get_default ();

	for (i = 0; i < N_GCONF_PREFS; i++)
		gconf_client_notify_remove (
				client, cd->listeners [i]);

	if (cd->timeout > 0) {
		g_source_remove (cd->timeout);
		cd->timeout = 0;
	}

	if (cd->props) {
		gtk_widget_destroy (cd->props);
		cd->props = NULL;
	}

        g_free (cd->timeformat);
        
	g_free (cd);
}

static void
create_clock_widget (ClockData *cd)
{
	GtkWidget *clock;

	clock = gtk_label_new ("hmm?");
	gtk_label_set_justify (GTK_LABEL (clock), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap (GTK_LABEL (clock), TRUE);
	gtk_widget_show (clock);

	cd->clockw = clock;

	cd->props = NULL;

	cd->orient = panel_applet_get_orient (PANEL_APPLET (cd->applet));
	cd->size = panel_applet_get_size (PANEL_APPLET (cd->applet));

	g_signal_connect (G_OBJECT(clock), "destroy",
			  G_CALLBACK (destroy_clock),
			  cd);
	
	set_atk_name_description (GTK_WIDGET (cd->applet), NULL,
	                          _("Computer Clock"));

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
	update_clock (cd, current_time);
}

static void
applet_change_background (PanelApplet               *applet,
			  PanelAppletBackgroundType  type,
			  GdkColor                  *color,
			  GdkPixmap                 *pixmap,
			  ClockData                 *cd)
{
	if (type == PANEL_NO_BACKGROUND) {
		GtkRcStyle *rc_style;

		rc_style = gtk_rc_style_new ();

		gtk_widget_modify_style (cd->applet, rc_style);

		g_object_unref (rc_style);

	} else if (type == PANEL_COLOR_BACKGROUND)
		gtk_widget_modify_bg (cd->applet, GTK_STATE_NORMAL, color);

	/* else if (type == PANEL_PIXMAP_BACKGROUND)
	 * FIXME: Handle this when the panel support works again
	 */
}


/* this is when the panel size changes */
static void
applet_change_pixel_size (PanelApplet *applet,
			  gint         size,
			  ClockData   *cd)
{
	time_t current_time;
	
	time (&current_time);
	cd->size = size;

	update_timeformat (cd);
	update_clock (cd, current_time);
}


static void
copy_time (BonoboUIComponent *uic,
	   ClockData         *cd,
	   const gchar       *verbname)
{
	time_t current_time = time (NULL);
	char string[256];
	char *utf8;

	if (cd->unixtime) {
		g_snprintf (string, sizeof(string), "%lu",
			    (unsigned long)current_time);
	} else if (cd->internettime) {
		float itime = get_itime (current_time);
		if (cd->showseconds)
			g_snprintf (string, sizeof (string), "@%3.2f", itime);
		else
			g_snprintf (string, sizeof (string), "@%3.0f", itime);
	} else {
		struct tm *tm;
		char      *format;

		if (cd->hourformat == 12) {
			if (cd->showseconds)
				format = g_locale_from_utf8 (_("%I:%M:%S %p"), -1, NULL, NULL, NULL);
			else
				format = g_locale_from_utf8 (_("%I:%M %p"), -1, NULL, NULL, NULL);
		} else {
			if (cd->showseconds)
				format = g_locale_from_utf8 (_("%H:%M:%S"), -1, NULL, NULL, NULL);
			else
				format = g_locale_from_utf8 (_("%H:%M"), -1, NULL, NULL, NULL);
		}

		if (cd->gmt_time)
			tm = gmtime (&current_time);
		else
			tm = localtime (&current_time);

		if (strftime (string, sizeof (string), format, tm) <= 0)
			strcpy (string, "???");
		g_free (format);
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
	char *utf8, *loc;

	if (cd->gmt_time)
		tm = gmtime (&current_time);
	else
		tm = localtime (&current_time);

	loc = g_locale_from_utf8 (_("%A, %B %d %Y"), -1, NULL, NULL, NULL);
	if (strftime (string, sizeof (string), loc, tm) <= 0)
		strcpy (string, "???");
	g_free (loc);
	
	utf8 = g_locale_to_utf8 (string, -1, NULL, NULL, NULL);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				utf8, -1);
	g_free (utf8);
}

/* current timestamp */
static const BonoboUIVerb clock_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ClockPreferences", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("ClockHelp",        display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("ClockAbout",       display_about_dialog),
	BONOBO_UI_UNSAFE_VERB ("ClockCopyTime",    copy_time),
	BONOBO_UI_UNSAFE_VERB ("ClockCopyDate",    copy_date),
	BONOBO_UI_VERB_END
};

static void
hour_format_changed (GConfClient  *client,
                     guint         cnxn_id,
                     GConfEntry   *entry,
                     ClockData    *clock)
{
	int value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_INT)
		return;

	value = gconf_value_get_int (entry->value);
	
	if (value == 12 || value == 24)
		clock->hourformat = value;
	else
		clock->hourformat = 12;

	update_timeformat (clock);
	refresh_clock (clock);
}

static void
show_seconds_changed (GConfClient  *client,
                   guint         cnxn_id,
                   GConfEntry   *entry,
                   ClockData    *clock)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	clock->showseconds = (value != 0);
	refresh_clock_timeout (clock);
}

static void
show_date_changed (GConfClient  *client,
                   guint         cnxn_id,
                   GConfEntry   *entry,
                   ClockData    *clock)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	clock->showdate = (value != 0);
	update_timeformat (clock);
	refresh_clock (clock);
}

static void
internet_time_changed (GConfClient *client,
                       guint        cnxn_id,
                       GConfEntry  *entry,
                       ClockData   *clock)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	clock->internettime = (value != 0);
	refresh_clock_timeout (clock);
}

static void
unix_time_changed (GConfClient *client,
                   guint        cnxn_id,
                   GConfEntry  *entry,
                   ClockData   *clock)
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
                  ClockData    *clock)
{
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	clock->gmt_time = (value != 0);
	refresh_clock_timeout (clock);
}

static void
setup_gconf (ClockData *clock)
{
	GConfClient *client;
	char        *key;

	client = gconf_client_get_default ();

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_HOUR_FORMAT);
	clock->listeners [0] =
		gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc) hour_format_changed,
				clock, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_SHOW_SECONDS);
	clock->listeners [1] =
			gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc)show_seconds_changed,
				clock, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_SHOW_DATE);
	clock->listeners [2] =
			gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc) show_date_changed,
				clock, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_GMT_TIME);
	clock->listeners [3] =
			gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc) gmt_time_changed,
				clock, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_UNIX_TIME);
	clock->listeners [4] =
		gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc)unix_time_changed,
				clock, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_INTERNET_TIME);
	clock->listeners [5] =
		gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc)internet_time_changed,
				clock, NULL, NULL);
	g_free (key);
}

gboolean
fill_clock_applet(PanelApplet *applet)
{
	ClockData *cd;
	GError *error;
	
	panel_applet_add_preferences (applet, "/schemas/apps/clock_applet/prefs", NULL);
	
	cd = g_new0 (ClockData, 1);

	cd->applet = GTK_WIDGET (applet);

	setup_gconf (cd);

	error = NULL;
	cd->hourformat = panel_applet_gconf_get_int (applet, KEY_HOUR_FORMAT, &error);
	if (error || (cd->hourformat != 12 && cd->hourformat != 24)) {
		/* if value is not valid, set it according to locale */
		const char *am = nl_langinfo (AM_STR);
		cd->hourformat = (am[0] == '\0') ? 24 : 12;

		if (error)
			g_error_free (error);
	}

	cd->showseconds = panel_applet_gconf_get_bool (applet, KEY_SHOW_SECONDS, NULL);
	
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

	cd->gmt_time = panel_applet_gconf_get_bool (applet, KEY_GMT_TIME, NULL);
	cd->unixtime = panel_applet_gconf_get_bool (applet, KEY_UNIX_TIME, NULL);
	cd->internettime = panel_applet_gconf_get_bool (applet, KEY_INTERNET_TIME, NULL);

	cd->timeformat = NULL;

	create_clock_widget (cd);

	gtk_container_set_border_width (GTK_CONTAINER (cd->applet), 0);
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
set_show_seconds_cb (GtkWidget *w,
                     ClockData *clock)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_SHOW_SECONDS,
				     GTK_TOGGLE_BUTTON (w)->active,
				     NULL);
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
properties_response_cb (GtkWidget *widget,
			int        id,
			ClockData *cd)
{
	
	if (id == GTK_RESPONSE_HELP) {
		static GnomeProgram *applet_program = NULL;
		GError *error = NULL;

		if (!applet_program) {
			int argc = 1;
			char *argv[2] = { "clock" };
			applet_program = gnome_program_init ("clock", VERSION,
							      LIBGNOME_MODULE, argc, argv,
							      GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);
		}

		egg_help_display_desktop_on_screen (
				applet_program, "clock",
				"clock", "clock-settings",
				gtk_widget_get_screen (cd->applet),
				&error);

		if (error) {
			GtkWidget *dialog;
			dialog = gtk_message_dialog_new (GTK_WINDOW (widget),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							  _("There was an error displaying help: %s"),
							 error->message);

			g_signal_connect (G_OBJECT (dialog), "response",
					  G_CALLBACK (gtk_widget_destroy),
					  NULL);

			gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
			gtk_window_set_screen (GTK_WINDOW (dialog),
					       gtk_widget_get_screen (cd->applet));
			gtk_widget_show (dialog);
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
	GtkWidget *hour_frame;
	GtkWidget *type_box;
	GtkWidget *options_frame;
	GtkWidget *vbox;
	GtkWidget *twelvehour;
	GtkWidget *twentyfourhour;
	GtkWidget *showseconds;
	GtkWidget *showdate;
	GtkWidget *unixtime;
	GtkWidget *internettime;
	GtkWidget *use_gmt_time;
	GSList    *list;
	char      *file;

	if (cd->props) {
		gtk_window_set_screen (GTK_WINDOW (cd->props),
				       gtk_widget_get_screen (cd->applet));
		gtk_window_present (GTK_WINDOW (cd->props));
		return;
	}

	cd->props = gtk_dialog_new_with_buttons (_("Clock Preferences"), NULL, 0,
						 GTK_STOCK_HELP,
						 GTK_RESPONSE_HELP,
						 GTK_STOCK_CLOSE,
						 GTK_RESPONSE_CLOSE,
						 NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (cd->props), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (cd->props), GTK_RESPONSE_CLOSE);
	gtk_window_set_screen (GTK_WINDOW (cd->props),
			       gtk_widget_get_screen (cd->applet));
		
	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
	                                  "gnome-clock.png", TRUE, NULL);
	if (file) {
		GdkPixbuf *pixbuf;
		pixbuf = gdk_pixbuf_new_from_file (file, NULL);
		gtk_window_set_icon (GTK_WINDOW (cd->props), pixbuf);
		g_object_unref (pixbuf);
		g_free (file);
	}
	else
		g_warning (G_STRLOC ": gnome-clock.png cannot be found");

	hbox = gtk_hbox_new (FALSE, 3);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cd->props)->vbox), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 3);
	
	hour_frame = gtk_frame_new (_("Clock type"));
	gtk_box_pack_start (GTK_BOX (hbox), hour_frame, FALSE, FALSE, 0);
	gtk_widget_show (hour_frame);
	gtk_container_set_border_width (GTK_CONTAINER (hour_frame), 3);

	type_box = gtk_vbox_new (FALSE, 3);
	gtk_container_add (GTK_CONTAINER (hour_frame), type_box);
	gtk_widget_show (type_box);
	gtk_container_set_border_width (GTK_CONTAINER (type_box), 3);

	twelvehour = gtk_radio_button_new_with_mnemonic (NULL, _("_12 hour"));
	gtk_box_pack_start (GTK_BOX (type_box), twelvehour, FALSE, FALSE, 0);
	gtk_widget_show (twelvehour);
	g_object_set_data (G_OBJECT (twelvehour), "user_data", cd);

	twentyfourhour = gtk_radio_button_new_with_mnemonic (gtk_radio_button_get_group (GTK_RADIO_BUTTON (twelvehour)),
							     _("_24 hour"));
	gtk_box_pack_start (GTK_BOX (type_box), twentyfourhour, FALSE, FALSE, 0);
	gtk_widget_show (twentyfourhour);
	g_object_set_data (G_OBJECT (twentyfourhour), "user_data", cd);

	unixtime = gtk_radio_button_new_with_mnemonic (gtk_radio_button_get_group (GTK_RADIO_BUTTON (twelvehour)), _("UNI_X time"));
	gtk_box_pack_start (GTK_BOX (type_box), unixtime, FALSE, FALSE, 0);
	gtk_widget_show (unixtime);

	internettime = gtk_radio_button_new_with_mnemonic (gtk_radio_button_get_group (GTK_RADIO_BUTTON (twelvehour)), _("_Internet time"));
	gtk_box_pack_start (GTK_BOX (type_box), internettime, FALSE, FALSE, 0);
	gtk_widget_show (internettime);
   
	if (cd->unixtime)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (unixtime), TRUE);
	else if (cd->internettime)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (internettime), TRUE);
	else if (cd->hourformat == 12)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (twelvehour), TRUE);
	else if (cd->hourformat == 24)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (twentyfourhour), TRUE);
   
	g_signal_connect (G_OBJECT (unixtime), "toggled",
			  G_CALLBACK (set_unixtime_cb),
			  cd);
	g_signal_connect (G_OBJECT (internettime), "toggled",
			  G_CALLBACK (set_internettime_cb),
			  cd);
	g_signal_connect (G_OBJECT (twelvehour), "toggled",
			  G_CALLBACK (set_hour_format_cb),
			  GINT_TO_POINTER (12));
	g_signal_connect (G_OBJECT (twentyfourhour), "toggled",
			  G_CALLBACK (set_hour_format_cb),
			  GINT_TO_POINTER (24));

	options_frame = gtk_frame_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), options_frame, FALSE, FALSE, 0);
	gtk_widget_show (options_frame);
	gtk_container_set_border_width (GTK_CONTAINER (options_frame), 3);
	gtk_frame_set_shadow_type (GTK_FRAME (options_frame), GTK_SHADOW_NONE);
	
	vbox = gtk_vbox_new (FALSE, 3);
	gtk_container_add (GTK_CONTAINER (options_frame), vbox);
	gtk_widget_show (vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

	showseconds = gtk_check_button_new_with_mnemonic (_("Show _seconds"));
	gtk_box_pack_start (GTK_BOX (vbox), showseconds, FALSE, FALSE, 0);
	gtk_widget_show (showseconds);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showseconds),
	                              cd->showseconds);
	g_signal_connect (G_OBJECT (showseconds), "toggled",
			  G_CALLBACK (set_show_seconds_cb),
			  cd);	   

	showdate = gtk_check_button_new_with_mnemonic (_("Show _date"));
	gtk_box_pack_start (GTK_BOX (vbox), showdate, FALSE, FALSE, 0);
	gtk_widget_show (showdate);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showdate),
	                              cd->showdate);
	g_signal_connect (G_OBJECT (showdate), "toggled",
			  G_CALLBACK (set_show_date_cb),
			  cd);	   

	use_gmt_time = gtk_check_button_new_with_mnemonic (_("Use _UTC"));
	gtk_box_pack_start (GTK_BOX (vbox), use_gmt_time, FALSE, FALSE, 0);
	gtk_widget_show (use_gmt_time);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_gmt_time),
	                              cd->gmt_time);
	g_signal_connect (G_OBJECT (use_gmt_time), "toggled",
			  G_CALLBACK (set_gmt_time_cb),
			  cd);	

	/* Some combinations of options do not make sense */
	if (cd->unixtime) {
		gtk_widget_set_sensitive (showseconds, FALSE);
		gtk_widget_set_sensitive (showdate, FALSE);
		gtk_widget_set_sensitive (use_gmt_time, FALSE);
	}
	if (cd->internettime) {
		gtk_widget_set_sensitive (showdate, FALSE);
		gtk_widget_set_sensitive (use_gmt_time, FALSE);
	}
	g_signal_connect (G_OBJECT (unixtime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  showseconds);
	g_signal_connect (G_OBJECT (unixtime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  showdate);
	g_signal_connect (G_OBJECT (unixtime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  use_gmt_time);
	g_signal_connect (G_OBJECT (internettime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  showdate);
	g_signal_connect (G_OBJECT (internettime), "toggled",
			  G_CALLBACK (set_datasensitive_cb),
			  use_gmt_time);   
   
	g_signal_connect (G_OBJECT (cd->props), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &(cd->props));
	g_signal_connect (G_OBJECT (cd->props), "response",
			  G_CALLBACK (properties_response_cb), cd);

	/* sets up atk relation  */
	list = g_slist_append (NULL, twelvehour);
	list = g_slist_append (list, twentyfourhour);
	add_atk_relation (use_gmt_time, list, ATK_RELATION_CONTROLLED_BY);
	g_slist_free (list);

	list = g_slist_append (NULL, use_gmt_time);
	add_atk_relation (twelvehour, list, ATK_RELATION_CONTROLLER_FOR);
	add_atk_relation (twentyfourhour, list, ATK_RELATION_CONTROLLER_FOR);
	g_slist_free (list);
				
	gtk_widget_show (cd->props);
}

static void
display_help_dialog (BonoboUIComponent *uic,
		     ClockData         *cd,
		     const gchar       *verbname)
{
	GError *error = NULL;
	static GnomeProgram *applet_program = NULL;

	if (!applet_program) {
		int argc = 1;
		char *argv[2] = { "clock" };
		applet_program = gnome_program_init ("clock", VERSION,
						      LIBGNOME_MODULE, argc, argv,
						      GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);
	}

	egg_help_display_desktop_on_screen (
			applet_program, "clock", "clock",NULL,
			gtk_widget_get_screen (cd->applet),
			&error);
	if (error) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						  _("There was an error displaying help: %s"),
						 error->message);

		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_widget_get_screen (cd->applet));
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
display_about_dialog (BonoboUIComponent *uic,
		      ClockData         *cd,
		      const gchar       *verbname)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *pixbuf = NULL;
	gchar *file;
	
	static const gchar *authors[] =
	{
		"George Lebl <jirka@5z.com>",
		"Gediminas Paulauskas <menesis@delfi.lt>",
		NULL
	};
	static const char *documenters[] =
	{
		"Dan Mueth (d-mueth@uchicago.edu)",
		NULL
	};
	/* Translator credits */
	const char *translator_credits = _("translator_credits");

	if (about) {
		gtk_window_set_screen (GTK_WINDOW (about),
				       gtk_widget_get_screen (cd->applet));
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	pixbuf = NULL;
	
	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
	                                  "gnome-clock.png", TRUE, NULL);
	if (file) {
		pixbuf = gdk_pixbuf_new_from_file (file, NULL);
		g_free (file);
	} else
		g_warning (G_STRLOC ": gnome-clock.png cannot be found");

	about = gnome_about_new (_("Clock"), VERSION,
				 "Copyright \xc2\xa9 1998-2002 Free Software Foundation. Inc.",
				 _("The Clock displays the current time and date"),
				 authors,
				 documenters,
				 strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
				 pixbuf);
	
	gtk_window_set_wmclass (GTK_WINDOW (about), "clock", "Clock");
	gtk_window_set_screen (GTK_WINDOW (about),
			       gtk_widget_get_screen (cd->applet));

	if (pixbuf) {
		gtk_window_set_icon (GTK_WINDOW (about), pixbuf);
		g_object_unref (pixbuf);
	}
	
	g_signal_connect (G_OBJECT(about), "destroy",
			  (GCallback)gtk_widget_destroyed, &about);
	
	gtk_widget_show (about);
}

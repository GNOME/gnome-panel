/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * clock.c: the GNOME clock applet
 *
 * Copyright (C) 1997-2003 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Miguel de Icaza
 *      Frederico Mena
 *      Stuart Parmenter
 *      Alexander Larsson
 *      George Lebl
 *      Gediminas Paulauskas
 *      Mark McLoughlin
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

#include "egg-screen-help.h"

#define INTERNETSECOND (864)
#define INTERNETBEAT   (86400)

#define N_GCONF_PREFS 7

static const char* KEY_HOUR_FORMAT   = "hour_format";
static const char* KEY_SHOW_SECONDS  = "show_seconds";
static const char* KEY_SHOW_DATE     = "show_date";
static const char* KEY_GMT_TIME      = "gmt_time";
static const char* KEY_UNIX_TIME     = "unix_time";
static const char* KEY_INTERNET_TIME = "internet_time";
static const char* KEY_CONFIG_TOOL   = "config_tool";

typedef struct _ClockData ClockData;

struct _ClockData {
	/* widgets */
	GtkWidget *applet;
	GtkWidget *clockw;
        GtkWidget *toggle;
	GtkWidget *props;
  
	/* preferences */
	int hourformat;
	gboolean showseconds;
	gboolean showdate;
	gboolean unixtime;
	gboolean internettime;
	gboolean gmt_time;

        char *config_tool;
        
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
	     GtkWidget  *widget,
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

	gtk_tooltips_set_tip (tooltips, widget, tip, NULL);
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
	set_tooltip (cd->applet, cd->toggle, utf8);
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
	g_free (cd->config_tool);
	g_free (cd);
}

static gboolean
close_on_escape (GtkWidget   *widget,
		 GdkEventKey *event,
		 ClockData   *cd)
{
	if (event->keyval == GDK_Escape) {             
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->toggle), FALSE);
		return TRUE;
	}

	return FALSE;
}

static GtkWidget *
create_calendar (ClockData *cd,
		 GdkScreen *screen)
{
	GtkWindow *window;
	GtkWidget *calendar;

	window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));

	/* set dialog type so it will skip the tasklist,
	 * kinda broken. This window should really be override
	 * redirect I suppose. But I hate override redirect
	 * windows that don't have a pointer/keyboard grab.
	 */
	gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_decorated (window, FALSE);
	gtk_window_set_resizable (window, FALSE);
	gtk_window_stick (window);
	gtk_window_set_title (window, _("Calendar"));
			
	g_signal_connect (window, "key_press_event",
			  G_CALLBACK (close_on_escape), cd);
			
	calendar = gtk_calendar_new ();

	gtk_container_add (GTK_CONTAINER (window), calendar);

	gtk_widget_show (calendar);

	return GTK_WIDGET (window);
}

static void
present_calendar_popup (ClockData *cd,
			GtkWidget *window,
			GtkWidget *button)
{
	GtkRequisition  req;
	GdkScreen      *screen;
	int             button_w, button_h;
	int             screen_w, screen_h;
	int             x, y;
	int             w, h;
		
	/* Get root origin of the toggle button, and position above that. */
	gdk_window_get_origin (button->window, &x, &y);

	gtk_window_get_size (GTK_WINDOW (window), &w, &h);
	gtk_widget_size_request (window, &req);
	w = req.width;
	h = req.height;

	button_w = button->allocation.width;
	button_h = button->allocation.height;

	screen = gtk_window_get_screen (GTK_WINDOW (window));

	/* FIXME use xinerama extents for xinerama containing
	 * the applet.
	 */
	screen_w = gdk_screen_get_width (screen);
	screen_h = gdk_screen_get_height (screen);
		
	/* Based on panel orientation, position the popup.
	 * Ignore window gravity since the window is undecorated.
	 * The orientations are all named backward from what
	 * I expected.
	 */
	switch (cd->orient) {
	case PANEL_APPLET_ORIENT_RIGHT:
		x += button_w;
		if ((y + h) > screen_h)
			y -= (y + h) - screen_h;
			break;
	case PANEL_APPLET_ORIENT_LEFT:
		x -= w;
		if ((y + h) > screen_h)
			y -= (y + h) - screen_h;
		break;
	case PANEL_APPLET_ORIENT_DOWN:
		y += button_h;
		if ((x + w) > screen_w)
			x -= (x + w) - screen_w;
		break;
	case PANEL_APPLET_ORIENT_UP:
		y -= h;
		if ((x + w) > screen_w)
			x -= (x + w) - screen_w;
		break;
	}
		
	gtk_window_move (GTK_WINDOW (window), x, y);
	gtk_window_present (GTK_WINDOW (window));
}

static void
update_popup (ClockData *cd)
{
	GtkWidget *window;
	GtkWidget *button;

	button = cd->toggle;
	
	window = g_object_get_data (G_OBJECT (button), "calendar");
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		if (!window) {
			window = create_calendar (cd, gtk_widget_get_screen (cd->applet));

			g_object_set_data_full (
				G_OBJECT (button), "calendar",
				window, (GDestroyNotify) gtk_widget_destroy);
		}
	} else {
		if (window) {
			/* Destroys the calendar */
			g_object_set_data (G_OBJECT (button), "calendar", NULL);
			window = NULL;
		}
	}

	if (window && GTK_WIDGET_REALIZED (button))
		present_calendar_popup (cd, window, button);
}

static void
toggle_calendar (GtkWidget *button,
                 ClockData *cd)
{
	update_popup (cd);
}

static gboolean
do_not_eat_button_press (GtkWidget      *widget,
                         GdkEventButton *event)
{
	if (event->button != 1)
		g_signal_stop_emission_by_name (widget, "button_press_event");

	return FALSE;
}


static void
create_clock_widget (ClockData *cd)
{
	GtkWidget *clock;
	GtkWidget *toggle;

	clock = gtk_label_new ("hmm?");
	gtk_label_set_justify (GTK_LABEL (clock), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap (GTK_LABEL (clock), TRUE);
	gtk_widget_show (clock);

	toggle = gtk_toggle_button_new ();
	gtk_button_set_relief (GTK_BUTTON (toggle), GTK_RELIEF_NONE);
        
	gtk_container_add (GTK_CONTAINER (toggle), clock);

	g_signal_connect (toggle, "button_press_event",
			  G_CALLBACK (do_not_eat_button_press), NULL);

	g_signal_connect (toggle, "toggled",
			  G_CALLBACK (toggle_calendar), cd);
        
	gtk_widget_show (toggle);        
        
	cd->toggle = toggle;

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
        update_popup (cd);
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

static gboolean
try_config_tool (GdkScreen  *screen,
		 const char *tool)
{
	GtkWidget *dialog;
	GError    *err;
	char      *argv[2];
	char      *app;

	g_return_val_if_fail (tool != NULL, FALSE);

	app = g_find_program_in_path (tool);

	if (!app)
		return FALSE;
		
	argv [0] = app;
	argv [1] = NULL;		

	/* FIXME: use egg_spawn_async_on_screen */
	err = NULL;
	if (g_spawn_async (NULL, argv, NULL, 0, NULL, NULL, NULL, &err)) {
		g_free (app);
		return TRUE;
	}
			
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 _("Failed to launch time configuration tool: %s"),
					 err->message);
		
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
			
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);
			
	gtk_widget_show_all (dialog);			

	g_free (app);
		
	return TRUE;
}

static void
config_date (BonoboUIComponent *uic,
             ClockData         *cd,
             const char        *verbname)
{
	GtkWidget *dialog;
	GdkScreen *screen;

	screen = gtk_widget_get_screen (cd->applet);

	/* FIXME add GST, etc. */
	if (cd->config_tool && try_config_tool (screen, cd->config_tool))
		return;

	else if (try_config_tool (screen, "redhat-config-date"))
		return;
		
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 _("Failed to locate a program for configuring "
					   "the date and time. Perhaps none is installed?"));
		
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
		
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);

	gtk_widget_show_all (dialog);
}

/* current timestamp */
static const BonoboUIVerb clock_menu_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ClockPreferences", display_properties_dialog),
	BONOBO_UI_UNSAFE_VERB ("ClockHelp",        display_help_dialog),
	BONOBO_UI_UNSAFE_VERB ("ClockAbout",       display_about_dialog),
	BONOBO_UI_UNSAFE_VERB ("ClockCopyTime",    copy_time),
	BONOBO_UI_UNSAFE_VERB ("ClockCopyDate",    copy_date),
        BONOBO_UI_UNSAFE_VERB ("ClockConfig",      config_date),
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
config_tool_changed (GConfClient  *client,
                     guint         cnxn_id,
                     GConfEntry   *entry,
                     ClockData    *clock)
{
	const char *value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
		return;

	value = gconf_value_get_string (entry->value);

        g_free (clock->config_tool);
	clock->config_tool = g_strdup (value);
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

        key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_CONFIG_TOOL);
	clock->listeners [6] =
		gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc) config_tool_changed,
				clock, NULL, NULL);
	g_free (key);
}

static gboolean
fill_clock_applet (PanelApplet *applet)
{
	ClockData *cd;
	GError *error;
	
	panel_applet_add_preferences (applet, "/schemas/apps/clock_applet/prefs", NULL);
	panel_applet_set_flags (applet, PANEL_APPLET_EXPAND_MINOR);
	
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
	cd->config_tool = panel_applet_gconf_get_string (applet, KEY_CONFIG_TOOL, NULL);

	cd->timeformat = NULL;

	create_clock_widget (cd);

	gtk_container_set_border_width (GTK_CONTAINER (cd->applet), 0);
	gtk_container_set_border_width (GTK_CONTAINER (cd->toggle), 0);
	gtk_container_add (GTK_CONTAINER (cd->applet), cd->toggle);

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
set_data_sensitive_cb (GtkWidget *w,
		      GtkWidget *wid)
{
	gtk_widget_set_sensitive (wid, TRUE);
}

static void
set_data_insensitive_cb (GtkWidget *w,
		      GtkWidget *wid)
{
	gtk_widget_set_sensitive (wid, FALSE);
}

static void
set_hour_format_cb (GtkWidget *w,
		    gpointer data)
{
	ClockData *clock = g_object_get_data (G_OBJECT (w), "user_data");
	panel_applet_gconf_set_int (PANEL_APPLET (clock->applet),
				    KEY_HOUR_FORMAT,
				    GPOINTER_TO_INT (data),
				    NULL);
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_INTERNET_TIME,
				     FALSE,
				     NULL);
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_UNIX_TIME,
				     FALSE,
				     NULL);
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
				     TRUE,
				     NULL);
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_UNIX_TIME,
				     FALSE,
				     NULL);
}

static void
set_unixtime_cb (GtkWidget *w,
		 ClockData *clock)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_UNIX_TIME,
				     TRUE,
				     NULL);
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_INTERNET_TIME,
				     FALSE,
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
		GError *error = NULL;

		egg_help_display_desktop_on_screen (
				NULL, "clock", "clock", "clock-settings",
				gtk_widget_get_screen (cd->applet),
				&error);

		if (error) {
			GtkWidget *dialog;
			dialog = gtk_message_dialog_new (GTK_WINDOW (widget),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
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
}

static void 
display_properties_dialog (BonoboUIComponent *uic,
			   ClockData         *cd,
			   const gchar       *verbname)
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *twelvehour;
	GtkWidget *twentyfourhour;
	GtkWidget *showseconds;
	GtkWidget *showdate;
	GtkWidget *unixtime;
	GtkWidget *internettime;
	GtkWidget *use_gmt_time;
	GtkWidget *option_menu;
	GtkWidget *menu;
	GtkWidget *label;
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
	gtk_window_set_resizable (GTK_WINDOW (cd->props), FALSE);
	gtk_window_set_screen (GTK_WINDOW (cd->props),
			       gtk_widget_get_screen (cd->applet));
	gtk_container_set_border_width (GTK_CONTAINER (cd->props), 5);
		
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

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cd->props)->vbox), vbox, FALSE, FALSE, 0);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show (hbox);

	label = gtk_label_new_with_mnemonic (_("Clock _type:"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	option_menu = gtk_option_menu_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL(label), option_menu);

	menu = gtk_menu_new ();
	twelvehour = gtk_menu_item_new_with_label (_("12 hour"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), twelvehour); 
	g_object_set_data (G_OBJECT (twelvehour), "user_data", cd);
   	g_signal_connect (G_OBJECT (twelvehour), "activate",
			  G_CALLBACK (set_hour_format_cb),
			  GINT_TO_POINTER (12));			   
	gtk_widget_show (twelvehour);

	twentyfourhour = gtk_menu_item_new_with_label (_("24 hour")); 
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), twentyfourhour);	
	g_object_set_data (G_OBJECT (twentyfourhour), "user_data", cd);
   	g_signal_connect (G_OBJECT (twentyfourhour), "activate",
			  G_CALLBACK (set_hour_format_cb),
			  GINT_TO_POINTER (24));
	gtk_widget_show (twentyfourhour);

	unixtime = gtk_menu_item_new_with_label (_("UNIX time")); 
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), unixtime);
	g_signal_connect (G_OBJECT (unixtime), "activate", 
			  G_CALLBACK (set_unixtime_cb), 
			  cd);
	gtk_widget_show (unixtime);

	internettime = gtk_menu_item_new_with_label (_("Internet time")); 
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), internettime);		   
	g_signal_connect (G_OBJECT (internettime), "activate",
			  G_CALLBACK (set_internettime_cb),
			  cd);
	gtk_widget_show (internettime);		   

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
	gtk_widget_show (option_menu);
	gtk_widget_show (menu);
  
	gtk_box_pack_start (GTK_BOX (hbox), option_menu, FALSE, FALSE, 0);

	showseconds = gtk_check_button_new_with_mnemonic (_("Show _seconds"));
	gtk_box_pack_start (GTK_BOX (vbox), showseconds, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showseconds),
	                              cd->showseconds);
	g_signal_connect (G_OBJECT (showseconds), "toggled",
			  G_CALLBACK (set_show_seconds_cb),
			  cd);	   
	gtk_widget_show (showseconds);

	showdate = gtk_check_button_new_with_mnemonic (_("Show _date"));
	gtk_box_pack_start (GTK_BOX (vbox), showdate, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (showdate),
	                              cd->showdate);
	g_signal_connect (G_OBJECT (showdate), "toggled",
			  G_CALLBACK (set_show_date_cb),
			  cd);
  	gtk_widget_show (showdate);

	use_gmt_time = gtk_check_button_new_with_mnemonic (_("Use _UTC"));
	gtk_box_pack_start (GTK_BOX (vbox), use_gmt_time, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_gmt_time),
	                              cd->gmt_time);
	g_signal_connect (G_OBJECT (use_gmt_time), "toggled",
			  G_CALLBACK (set_gmt_time_cb),
			  cd);	
	gtk_widget_show (use_gmt_time);

	if (cd->internettime)
		gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 3);
	else if (cd->unixtime)
		gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 2);
	else if (cd->hourformat == 24)
		gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 1);
	else if (cd->hourformat == 12)
		gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 0);

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

	/* 12 hour mode -- toggle sensitivity of check button items */
	g_signal_connect (G_OBJECT (twelvehour), "activate",
			  G_CALLBACK (set_data_sensitive_cb),
			  showseconds);
  	g_signal_connect (G_OBJECT (twelvehour), "activate",
			  G_CALLBACK (set_data_sensitive_cb),
			  showdate);	
	g_signal_connect (G_OBJECT (twelvehour), "activate",
			  G_CALLBACK (set_data_sensitive_cb),
			  use_gmt_time);

	/* 24 hour mode -- toggle sensitivity of check button items */
	g_signal_connect (G_OBJECT (twentyfourhour), "activate",
			  G_CALLBACK (set_data_sensitive_cb),
			  showseconds);
  	g_signal_connect (G_OBJECT (twentyfourhour), "activate",
			  G_CALLBACK (set_data_sensitive_cb),
			  showdate);	
	g_signal_connect (G_OBJECT (twentyfourhour), "activate",
			  G_CALLBACK (set_data_sensitive_cb),
			  use_gmt_time);

	/* UNIX time mode -- toggle sensitivity of check button items */			  	  
	g_signal_connect (G_OBJECT (unixtime), "activate",
			  G_CALLBACK (set_data_insensitive_cb),
			  showseconds);
	g_signal_connect (G_OBJECT (unixtime), "activate",
			  G_CALLBACK (set_data_insensitive_cb),
			  showdate);
	g_signal_connect (G_OBJECT (unixtime), "activate",
			  G_CALLBACK (set_data_insensitive_cb),
			  use_gmt_time);

	/* Internet time mode -- toggle sensitivity of check button items */	
	g_signal_connect (G_OBJECT (internettime), "activate",
			  G_CALLBACK (set_data_sensitive_cb),
			  showseconds);		  
	g_signal_connect (G_OBJECT (internettime), "activate",
			  G_CALLBACK (set_data_insensitive_cb),
			  showdate);
	g_signal_connect (G_OBJECT (internettime), "activate",
			  G_CALLBACK (set_data_insensitive_cb),
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
						 GTK_BUTTONS_OK,
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
		"Dan Mueth <d-mueth@uchicago.edu>",
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

static gboolean
clock_factory (PanelApplet *applet,
	       const char  *iid,
	       gpointer     data)
{
	gboolean retval = FALSE;

	if (!strcmp (iid, "OAFIID:GNOME_ClockApplet"))
		retval = fill_clock_applet (applet);

	return retval;
}

PANEL_APPLET_BONOBO_SHLIB_FACTORY ("OAFIID:GNOME_ClockApplet_Factory",
				   PANEL_TYPE_APPLET,
				   "Clock Applet factory",
				   clock_factory, NULL);


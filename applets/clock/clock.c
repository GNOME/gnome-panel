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

 /*
  * Evolution calendar integration TODO:
  *   + Fix treeview scrolling and sizing
  *   + Tooltips for tasks/appointments
  *   + Do everything backwards if the clock is on the bottom
  *   + Double clicking appointments/tasks should open them in evo
  *   + Consider using different colours for different sources
  *   + Consider doing a GtkMenu tearoff type thing
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
#include <gdk/gdkkeysyms.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-help.h>
#include <libgnomeui/gnome-url.h>

#ifdef HAVE_LIBECAL
#include "calendar-client.h"
#endif

#define CLOCK_ICON "gnome-panel-clock"
#define INTERNETSECOND (864)
#define INTERNETBEAT   (86400)

#define N_GCONF_PREFS 7

#define NEVER_SENSITIVE "never_sensitive"

static const char* KEY_FORMAT        = "format";
static const char* KEY_SHOW_SECONDS  = "show_seconds";
static const char* KEY_SHOW_DATE     = "show_date";
static const char* KEY_GMT_TIME      = "gmt_time";
static const char* KEY_CONFIG_TOOL   = "config_tool";
static const char* KEY_CUSTOM_FORMAT = "custom_format";
static const char* KEY_SHOW_WEEK     = "show_week_numbers";

static const char *clock_config_tools [] = {
	"system-config-date",
	"redhat-config-date",
        "/sbin/yast2 timezone",
	"time-admin",
};

/* Needs to match the indices in the combo */
typedef enum {
        CLOCK_FORMAT_INVALID = 0,
	CLOCK_FORMAT_12,
	CLOCK_FORMAT_24,
	CLOCK_FORMAT_UNIX,
	CLOCK_FORMAT_INTERNET,
	CLOCK_FORMAT_CUSTOM
} ClockFormat;

static GConfEnumStringPair format_type_enum_map [] = {
	{ CLOCK_FORMAT_12,       "12-hour"  },
	{ CLOCK_FORMAT_24,       "24-hour"  },
	{ CLOCK_FORMAT_UNIX,     "unix"     },
	{ CLOCK_FORMAT_INTERNET, "internet" },
	{ CLOCK_FORMAT_CUSTOM,   "custom"   },
	{ 0, NULL }
};

typedef struct _ClockData ClockData;

struct _ClockData {
	/* widgets */
	GtkWidget *applet;
	GtkWidget *clockw;
        GtkWidget *toggle;
	GtkWidget *props;
	GtkWidget *about;
        GtkWidget *calendar_popup;
        GtkWidget *calendar;

#ifdef HAVE_LIBECAL
        GtkWidget *task_list;
        GtkWidget *appointment_list;
        
        GtkListStore       *appointments_model;
        GtkListStore       *tasks_model;
        GtkTreeModelFilter *tasks_filter;

        CalendarClient *client;
#endif /* HAVE_LIBECAL */

	/* preferences */
	ClockFormat  format;
	char        *custom_format;
	gboolean     showseconds;
	gboolean     showdate;
	gboolean     gmt_time;
	gboolean     showweek;

        char *config_tool;

	/* runtime data */
        time_t             current_time;
	char              *timeformat;
	guint              timeout;
	PanelAppletOrient  orient;
	int                size;
	GtkAllocation      old_allocation;

	int fixed_width;
	int fixed_height;

        GtkWidget *showseconds_check;
        GtkWidget *showdate_check;
        GtkWidget *gmt_time_check;
        GtkWidget *custom_hbox;
        GtkWidget *custom_label;
        GtkWidget *custom_entry;
        gboolean   custom_format_shown;

	gboolean   can_handle_format_12;

	guint listeners [N_GCONF_PREFS];
};

static void  update_clock (ClockData * cd);
static float get_itime    (time_t current_time);

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
static void position_calendar_popup   (ClockData         *cd,
                                       GtkWidget         *window,
                                       GtkWidget         *button);
static void update_orient (ClockData *cd);

static void
unfix_size (ClockData *cd)
{
	cd->fixed_width = -1;
	cd->fixed_height = -1;
	gtk_widget_queue_resize (cd->toggle);
}

static int
calculate_minimum_width (GtkWidget   *widget,
			 const gchar *text)
{
	PangoContext *context;
	PangoLayout  *layout;
	int	      width, height;
	int	      focus_width = 0;
	int	      focus_pad = 0;

	context = gtk_widget_get_pango_context (widget);

	layout = pango_layout_new (context);
	pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
	pango_layout_set_text (layout, text, -1);
	pango_layout_get_pixel_size (layout, &width, &height);
	g_object_unref (G_OBJECT (layout));
	layout = NULL;

	gtk_widget_style_get (widget,
			      "focus-line-width", &focus_width,
			      "focus-padding", &focus_pad,
			      NULL);

	width += 2 * (focus_width + focus_pad + widget->style->xthickness);

	return width;
}

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
	time_t new_time;

        time (&new_time);

	if (!cd->showseconds &&
	    cd->format != CLOCK_FORMAT_UNIX &&
	    cd->format != CLOCK_FORMAT_CUSTOM) {
		if (cd->format == CLOCK_FORMAT_INTERNET && 
		    (long)get_itime (new_time) !=
		    (long)get_itime (cd->current_time)) {
			update_clock (cd);
		} else if ((cd->format == CLOCK_FORMAT_12 ||
			    cd->format == CLOCK_FORMAT_24) &&
			   new_time / 60 != cd->current_time / 60) {
			update_clock (cd);
		}
	} else {
		update_clock (cd);
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

/* adapted from panel-toplevel.c */
static int
calculate_minimum_height (GtkWidget        *widget,
                          PanelAppletOrient orientation)
{
        PangoContext     *context;
        PangoFontMetrics *metrics;
        int               focus_width = 0;
        int               focus_pad = 0;
        int               ascent;
        int               descent;
        int               thickness;
  
        context = gtk_widget_get_pango_context (widget);
        metrics = pango_context_get_metrics (context,
                                             widget->style->font_desc,
                                             pango_context_get_language (context));
  
        ascent  = pango_font_metrics_get_ascent  (metrics);
        descent = pango_font_metrics_get_descent (metrics);
  
        pango_font_metrics_unref (metrics);

        gtk_widget_style_get (widget,
                              "focus-line-width", &focus_width,
                              "focus-padding", &focus_pad,
                              NULL);

        if (orientation == PANEL_APPLET_ORIENT_UP
            || orientation == PANEL_APPLET_ORIENT_DOWN) {
                thickness = widget->style->ythickness;
        } else {
                thickness = widget->style->xthickness;
        }

        return PANGO_PIXELS (ascent + descent) + 2 * (focus_width + focus_pad + thickness);
}

static gboolean
use_two_line_format (ClockData *cd)
{
        if (cd->size >= 2 * calculate_minimum_height (cd->toggle, cd->orient))
                return TRUE;

        return FALSE;
}

static void
update_timeformat (ClockData *cd)
{
 /* Show date in another line if panel is vertical, or
  * horizontal but large enough to hold two lines of text
  */
	const char *time_format;
	const char *date_format;
	char       *clock_format;

	if (cd->format == CLOCK_FORMAT_12)
		time_format = cd->showseconds ? _("%l:%M:%S %p") : _("%l:%M %p");
	else
		time_format = cd->showseconds ? _("%H:%M:%S") : _("%H:%M");

	if (!cd->showdate)
		clock_format = g_strdup (time_format);

	else {
		/* translators: replace %e with %d if, when the day of the
		 *              month as a decimal number is a single digit, it
		 *              should begin with a 0 in your locale (e.g. "May
		 *              01" instead of "May  1").
		 */
		date_format = _("%a %b %e");

		if (use_two_line_format (cd))
			/* translators: reverse the order of these arguments
			 *              if the time should come before the
			 *              date on a clock in your locale.
			 */
			clock_format = g_strdup_printf (_("%1$s\n%2$s"),
							date_format, time_format);
		else
			/* translators: reverse the order of these arguments
			 *              if the time should come before the
			 *              date on a clock in your locale.
			 */
			clock_format = g_strdup_printf (_("%1$s, %2$s"),
							date_format, time_format);
	}

	g_free (cd->timeformat);
	cd->timeformat = g_locale_from_utf8 (clock_format, -1, NULL, NULL, NULL);
	/* let's be paranoid */
	if (!cd->timeformat)
		cd->timeformat = g_strdup ("???");

	g_free (clock_format);
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

static void
update_clock (ClockData * cd)
{
	struct tm *tm;
	char date[256], hour[256];
	char *utf8, *loc;

	time (&cd->current_time);
	
	if (cd->gmt_time)
		tm = gmtime (&cd->current_time);
	else
		tm = localtime (&cd->current_time);

	if (cd->format == CLOCK_FORMAT_UNIX) {
		if (use_two_line_format (cd)) {
			g_snprintf (hour, sizeof(hour), "%lu\n%05lu",
				    (unsigned long)(cd->current_time / 100000L),
				    (unsigned long)(cd->current_time % 100000L));
		} else {
			g_snprintf (hour, sizeof(hour), "%lu", (unsigned long)cd->current_time);
		}
	} else if (cd->format == CLOCK_FORMAT_INTERNET) {
		float itime = get_itime (cd->current_time);
		if (cd->showseconds)
			g_snprintf (hour, sizeof (hour), "@%3.2f", itime);
		else
			g_snprintf (hour, sizeof (hour), "@%3.0f", itime);
	} else if (cd->format == CLOCK_FORMAT_CUSTOM) {
		char *timeformat = g_locale_from_utf8 (cd->custom_format, -1,
						       NULL, NULL, NULL);
		if (!timeformat)
			strcpy (hour, "???");
		else if (strftime (hour, sizeof (hour), timeformat, tm) <= 0)
			strcpy (hour, "???");
		g_free (timeformat);
	} else {
		if (strftime (hour, sizeof (hour), cd->timeformat, tm) <= 0)
			strcpy (hour, "???");
	}

	utf8 = g_locale_to_utf8 (hour, -1, NULL, NULL, NULL);
	gtk_label_set_text (GTK_LABEL (cd->clockw), utf8);
	g_free (utf8);

	update_orient (cd);
	gtk_widget_queue_resize (cd->toggle);

        if (!cd->showdate) {
                /* Show date in tooltip */
                loc = g_locale_from_utf8 (_("%A %B %d"), -1, NULL, NULL, NULL);
                if (!loc)
                        strcpy (date, "???");
                else if (strftime (date, sizeof (date), loc, tm) <= 0)
                        strcpy (date, "???");
                g_free (loc);

                utf8 = g_locale_to_utf8 (date, -1, NULL, NULL, NULL);
                set_tooltip (cd->applet, cd->toggle, utf8);
                g_free (utf8);
        } else {
#ifdef HAVE_LIBECAL
                set_tooltip (cd->applet, cd->toggle, _("Click to view your appointments and tasks"));
#else
                set_tooltip (cd->applet, cd->toggle, _("Click to view month calendar"));
#endif
        }
}

static void
refresh_clock (ClockData *cd)
{
	unfix_size (cd);
	update_clock (cd);
}

static void
refresh_clock_timeout(ClockData *cd)
{
	int timeouttime;

	unfix_size (cd);
	
	update_timeformat (cd);

	if (cd->timeout)
		g_source_remove (cd->timeout);

	update_clock (cd);
	
	if (cd->format == CLOCK_FORMAT_INTERNET)
		timeouttime = INTERNETSECOND;
	else
		timeouttime = 1000;
	
	cd->timeout = g_timeout_add (timeouttime,
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

	g_object_unref (G_OBJECT (client));

	if (cd->timeout)
		g_source_remove (cd->timeout);
        cd->timeout = 0;

#ifdef HAVE_LIBECAL
        if (cd->client)
                g_object_unref (cd->client);
        cd->client = NULL;

        if (cd->appointments_model)
                g_object_unref (cd->appointments_model);
        cd->appointments_model = NULL;

        if (cd->tasks_model)
                g_object_unref (cd->tasks_model);
        cd->tasks_model = NULL;
        
        if (cd->tasks_filter)
                g_object_unref (cd->tasks_filter);
        cd->tasks_filter = NULL;
#endif /* HAVE_LIBECAL */

	if (cd->about)
		gtk_widget_destroy (cd->about);
        cd->about = NULL;

	if (cd->props)
		gtk_widget_destroy (cd->props);
        cd->props = NULL;

	if (cd->calendar_popup)
		gtk_widget_destroy (cd->calendar_popup);
	cd->calendar_popup = NULL;

	g_free (cd->timeformat);
	g_free (cd->config_tool);
	g_free (cd->custom_format);
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

static gboolean
delete_event (GtkWidget   *widget,
	      GdkEvent    *event,
	      ClockData   *cd)
{
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->toggle), FALSE);
	return TRUE;
}

static inline ClockFormat
clock_locale_format (void)
{
        const char *am;

        am = nl_langinfo (AM_STR);
        return (am[0] == '\0') ? CLOCK_FORMAT_24 : CLOCK_FORMAT_12;
}

#ifdef HAVE_LIBECAL

static void
update_frame_visibility (GtkWidget    *frame,
                         GtkTreeModel *model)
{
        GtkTreeIter iter;
        gboolean    model_empty;

        if (!frame)
                return;

        model_empty = !gtk_tree_model_get_iter_first (model, &iter);

        if (model_empty)
                gtk_widget_hide (frame);
        else
                gtk_widget_show (frame);
}

enum {
        APPOINTMENT_COLUMN_UID,
        APPOINTMENT_COLUMN_SUMMARY,
        APPOINTMENT_COLUMN_DESCRIPTION,
        APPOINTMENT_COLUMN_START_TIME,
        APPOINTMENT_COLUMN_START_TEXT,
        APPOINTMENT_COLUMN_END_TIME,
        APPOINTMENT_COLUMN_ALL_DAY,
        APPOINTMENT_COLUMN_COLOR,
        N_APPOINTMENT_COLUMNS
};

enum {
        TASK_COLUMN_UID,
        TASK_COLUMN_SUMMARY,
        TASK_COLUMN_DESCRIPTION,
        TASK_COLUMN_START_TIME,
        TASK_COLUMN_DUE_TIME,
        TASK_COLUMN_PERCENT_COMPLETE,
        TASK_COLUMN_PERCENT_COMPLETE_TEXT,
        TASK_COLUMN_COMPLETED,
        TASK_COLUMN_COMPLETED_TIME,
        TASK_COLUMN_OVERDUE_ATTR,
        TASK_COLUMN_COLOR,
        TASK_COLUMN_URL,
        TASK_COLUMN_PRIORITY,
        N_TASK_COLUMNS
};

static char *
format_time (ClockFormat format,
             time_t      t,
             guint       year,
             guint       month,
             guint       day,
             gboolean    use_utc)
{
        struct tm *tm;
        char      *time_format;
        char       result [256] = { 0, };

        if (!t)
                return NULL;

        if (use_utc)
                tm = gmtime (&t);
        else
                tm = localtime (&t);
        if (!tm)
                return NULL;

        if (year  == (tm->tm_year + 1900) &&
            month == tm->tm_mon &&
            day   == tm->tm_mday) {
                if (format != CLOCK_FORMAT_12 && format != CLOCK_FORMAT_24)
                        format = clock_locale_format ();

                if (format == CLOCK_FORMAT_12)
                        time_format = g_locale_from_utf8 (_("%l:%M %p"), -1, NULL, NULL, NULL);
                else
                        time_format = g_locale_from_utf8 (_("%H:%M"), -1, NULL, NULL, NULL);
        }
        else {
                /* Translators: If the event did not start on the current day
                 we will display the start date in the most abbreviated way possible. */
                time_format = g_locale_from_utf8 (_("%b %d"), -1, NULL, NULL, NULL);
        }

        strftime (result, sizeof (result), time_format, tm);
        g_free (time_format);

        return g_locale_to_utf8 (result, -1, NULL, NULL, NULL);
}

static void
handle_tasks_changed (ClockData *cd)
{
        GSList *events, *l;

        gtk_list_store_clear (cd->tasks_model);

        events = calendar_client_get_events (cd->client, CALENDAR_EVENT_TASK);
        for (l = events; l; l = l->next) {
                CalendarTask *task = l->data;
                GtkTreeIter   iter;
                char         *percent_complete_text;

                g_assert (CALENDAR_EVENT (task)->type == CALENDAR_EVENT_TASK);
      
                /* FIXME: should this format be locale specific ? */
                percent_complete_text = g_strdup_printf ("%d%%", task->percent_complete);

                gtk_list_store_append (cd->tasks_model, &iter);
                gtk_list_store_set (cd->tasks_model, &iter,
                                    TASK_COLUMN_UID,                   task->uid,
                                    TASK_COLUMN_SUMMARY,               task->summary,
                                    TASK_COLUMN_DESCRIPTION,           task->description,
                                    TASK_COLUMN_START_TIME,            task->start_time,
                                    TASK_COLUMN_DUE_TIME,              task->due_time,
                                    TASK_COLUMN_PERCENT_COMPLETE,      task->percent_complete,
                                    TASK_COLUMN_PERCENT_COMPLETE_TEXT, percent_complete_text,
                                    TASK_COLUMN_COMPLETED,             task->percent_complete == 100,
                                    TASK_COLUMN_COMPLETED_TIME,        task->completed_time,
                                    TASK_COLUMN_COLOR,                 task->color_string,
                                    TASK_COLUMN_URL,                   task->url,
                                    TASK_COLUMN_PRIORITY,              task->priority,
                                    -1);

                g_free (percent_complete_text);
                calendar_event_free (CALENDAR_EVENT (task));
        }
        g_slist_free (events);

        update_frame_visibility (cd->task_list, GTK_TREE_MODEL (cd->tasks_filter));
}

static void
handle_task_completed_toggled (ClockData             *cd,
                               const char            *path_str,
                               GtkCellRendererToggle *cell)
{
        GtkTreePath *child_path, *path;
        GtkTreeIter  iter;
        char        *uid;
        gboolean     task_completed;
        guint        percent_complete;
        
        path       = gtk_tree_path_new_from_string (path_str);
        child_path = gtk_tree_model_filter_convert_path_to_child_path (cd->tasks_filter, path);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (cd->tasks_model), &iter, child_path);
        gtk_tree_model_get (GTK_TREE_MODEL (cd->tasks_model), &iter,
                            TASK_COLUMN_UID,                  &uid,
                            TASK_COLUMN_COMPLETED,            &task_completed,
                            TASK_COLUMN_PERCENT_COMPLETE,     &percent_complete,
                            -1);

        task_completed   = !task_completed;
        percent_complete = task_completed ? 100 : 0;

        calendar_client_set_task_completed (cd->client,
                                            uid,
                                            task_completed,
                                            percent_complete);

        g_free (uid);
        gtk_tree_path_free (path);
        gtk_tree_path_free (child_path);
}

static void
handle_task_percent_complete_edited (ClockData           *cd,
                                     const char          *path_str,
                                     const char          *text,
                                     GtkCellRendererText *cell)
{
        GtkTreePath *child_path, *path;
        GtkTreeIter  iter;
        char        *uid;
        int          percent_complete;
        char        *error = NULL, *text_copy;

        path       = gtk_tree_path_new_from_string (path_str);
        child_path = gtk_tree_model_filter_convert_path_to_child_path (cd->tasks_filter, path);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (cd->tasks_model), &iter, child_path);
        gtk_tree_model_get (GTK_TREE_MODEL (cd->tasks_model), &iter,
                            TASK_COLUMN_UID, &uid,
                            -1);

        text_copy = g_strdup (text);
        text_copy = g_strdelimit (text_copy, "%", ' ');
        text_copy = g_strstrip (text_copy);
        percent_complete = (int) g_strtod (text_copy, &error);
        if (!error || !error [0]) {
                gboolean task_completed;

                percent_complete = CLAMP (percent_complete, 0, 100);
                task_completed = (percent_complete == 100);

                calendar_client_set_task_completed (cd->client,
                                                    uid,
                                                    task_completed,
                                                    percent_complete);
        }

        g_free (uid);
        g_free (text_copy);
        gtk_tree_path_free (path);
        gtk_tree_path_free (child_path);
}

static gboolean
filter_out_tasks (GtkTreeModel *model,
                  GtkTreeIter  *iter,
                  ClockData    *cd)
{
        GTime    start_time;
        GTime    completed_time;
        GTime    one_day_ago;
        gboolean visible;

        gtk_tree_model_get (model, iter,
                            TASK_COLUMN_START_TIME,     &start_time,
                            TASK_COLUMN_COMPLETED_TIME, &completed_time,
                            -1);

        one_day_ago = cd->current_time - (24 * 60 * 60);

        visible = !start_time || start_time <= cd->current_time;
        if (visible)
                visible = !completed_time || completed_time >= one_day_ago;

        return visible;
}

static void
modify_task_text_attributes (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             GValue       *value,
                             gint          column,
                             ClockData    *cd)
{
        GTime           due_time;
        PangoAttrList  *attr_list;
        PangoAttribute *attr;
        GtkTreeIter     child_iter;

        gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                          &child_iter,
                                                          iter);

        if (column != TASK_COLUMN_OVERDUE_ATTR) {
                memset (value, 0, sizeof (GValue));
                gtk_tree_model_get_value (GTK_TREE_MODEL (cd->tasks_model),
                                          &child_iter, column, value);

                return;
        }

        gtk_tree_model_get (GTK_TREE_MODEL (cd->tasks_model), &child_iter,
                            TASK_COLUMN_DUE_TIME, &due_time,
                            -1);
        if (due_time && due_time > cd->current_time)
                return;

        attr_list = pango_attr_list_new ();

        attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
        attr->start_index = 0;
        attr->end_index = G_MAXINT;
        pango_attr_list_insert (attr_list, attr);

        g_value_take_boxed (value, attr_list);
}

static GtkWidget *
create_hig_frame (const char  *title)
{
        GtkWidget *vbox;
        GtkWidget *alignment;
        GtkWidget *label;
        char      *bold_title;

        vbox = gtk_vbox_new (FALSE, 6);

        bold_title = g_strdup_printf ("<b>%s</b>", title);

        alignment = gtk_alignment_new (0, 0.5, 0, 0);
        gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
        gtk_widget_show (alignment);
        
        label = gtk_label_new (bold_title);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_container_add (GTK_CONTAINER (alignment), label);
        gtk_widget_show (label);

        g_free (bold_title);

        return vbox;
}

static gboolean
task_activated_cb (GtkTreeView       *view,
                   GtkTreePath       *path,
                   GtkTreeViewColumn *column,
                   ClockData         *cd)
{
        GtkTreeIter  iter;
        GtkTreePath *child_path;
        char        *uri;

        child_path = gtk_tree_model_filter_convert_path_to_child_path (cd->tasks_filter, path);

        gtk_tree_model_get_iter (GTK_TREE_MODEL (cd->tasks_model), &iter, child_path);
        gtk_tree_model_get (GTK_TREE_MODEL (cd->tasks_model), &iter,
                            TASK_COLUMN_URL, &uri, -1);

        if (uri) {
                gnome_url_show_on_screen (uri,
					  gtk_widget_get_screen (cd->applet),
					  NULL);
        }

        g_free (uri);
        gtk_tree_path_free (child_path);

        return TRUE;
}

static void
set_renderer_pixbuf_color_by_column (GtkCellRenderer *renderer,
                                     GtkTreeModel    *model,
                                     GtkTreeIter     *iter,
                                     gint             column_number)
{
        char      *color_string;
        GdkPixbuf *pixbuf = NULL;
        guint32    color;

        gtk_tree_model_get (model, iter, column_number, &color_string, -1);

        if (color_string) {
                sscanf (color_string, "%06x", &color);
                pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 16, 16);
                gdk_pixbuf_fill (pixbuf, color << 8);

                g_object_set (renderer, "visible", pixbuf != NULL, "pixbuf", pixbuf, NULL);

                if (pixbuf)
                        g_object_unref (pixbuf);

                g_free (color_string);
        }
}

static void
task_pixbuf_cell_data_func (GtkTreeViewColumn *column,
                            GtkCellRenderer   *renderer,
                            GtkTreeModel      *model,
                            GtkTreeIter       *iter,
                            gpointer           data)
{
        set_renderer_pixbuf_color_by_column (renderer,
                                             model,
                                             iter,
                                             TASK_COLUMN_COLOR);
}

static void
appointment_pixbuf_cell_data_func (GtkTreeViewColumn *column,
                                   GtkCellRenderer   *renderer,
                                   GtkTreeModel      *model,
                                   GtkTreeIter       *iter,
                                   gpointer           data)
{
        set_renderer_pixbuf_color_by_column (renderer,
                                             model,
                                             iter,
                                             APPOINTMENT_COLUMN_COLOR);
}

static int
compare_tasks  (GtkTreeModel *model,
                GtkTreeIter  *a,
                GtkTreeIter  *b,
                gpointer      user_data)
{
        int priority_a;
        int priority_b;

        gtk_tree_model_get (model, a, TASK_COLUMN_PRIORITY, &priority_a, -1);
        gtk_tree_model_get (model, b, TASK_COLUMN_PRIORITY, &priority_b, -1);

	/* We change undefined priorities so they appear after 'Low'. */
	if (priority_a <= 0)
		priority_a = 10;
	if (priority_b <= 0)
		priority_b = 10;

	/* We'll just use the ordering of the priority values. */
	if (priority_a < priority_b)
		return -1;
	else if (priority_a > priority_b)
		return 1;
	else {
                GTime due_time_a, due_time_b;

                gtk_tree_model_get (model, a,
				    TASK_COLUMN_DUE_TIME, &due_time_a, -1);
                gtk_tree_model_get (model, b,
				    TASK_COLUMN_DUE_TIME, &due_time_b, -1);

                if (due_time_a < due_time_b)
                        return -1;
                else if (due_time_a > due_time_b)
                        return 1;
                else
                        return 0;
        }
}

static GtkWidget *
create_task_list (ClockData  *cd,
                  GtkWidget **tree_view,
                  GtkWidget **scrolled_window)
{
        GtkWidget         *vbox;
        GtkWidget         *view;
        GtkWidget         *scrolled;
        GtkCellRenderer   *cell;
        GtkTreeViewColumn *column;

        vbox = create_hig_frame (_("Tasks"));

        *scrolled_window = scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                             GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
        gtk_widget_show (scrolled);

        if (!cd->tasks_model) {
                GType column_types [N_TASK_COLUMNS] = {
                        G_TYPE_STRING,         /* uid                     */
                        G_TYPE_STRING,         /* summary                 */
                        G_TYPE_STRING,         /* description             */
                        G_TYPE_LONG,           /* start time              */
                        G_TYPE_LONG,           /* due time                */
                        G_TYPE_UINT,           /* percent complete        */
                        G_TYPE_STRING,         /* percent complete text   */
                        G_TYPE_BOOLEAN,        /* completed               */
                        G_TYPE_LONG,           /* completed time          */
                        PANGO_TYPE_ATTR_LIST,  /* summary text attributes */
                        G_TYPE_STRING,         /* color                   */
                        G_TYPE_STRING,         /* url                     */
                        G_TYPE_INT             /* priority                */
                };

                cd->tasks_model = gtk_list_store_newv (N_TASK_COLUMNS, column_types);

                gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (cd->tasks_model),
                                                 TASK_COLUMN_PRIORITY,
                                                 compare_tasks,
                                                 NULL, NULL);

                gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (cd->tasks_model),
                                                      TASK_COLUMN_PRIORITY,
                                                      GTK_SORT_ASCENDING);

                cd->tasks_filter = GTK_TREE_MODEL_FILTER (
                        gtk_tree_model_filter_new (GTK_TREE_MODEL (cd->tasks_model),
                                                   NULL));
                gtk_tree_model_filter_set_visible_func (
                         cd->tasks_filter,
                         (GtkTreeModelFilterVisibleFunc) filter_out_tasks,
                         cd,
                         NULL);
                gtk_tree_model_filter_set_modify_func (
                         cd->tasks_filter,
                         N_TASK_COLUMNS,
                         column_types,
                         (GtkTreeModelFilterModifyFunc) modify_task_text_attributes,
                         cd,
                         NULL);
        }

        *tree_view = view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (cd->tasks_filter));
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

        g_signal_connect (view, "row-activated",
                          G_CALLBACK (task_activated_cb), cd);

        /* Source color */
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_pixbuf_new ();
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_set_cell_data_func (column, cell,
                                                 (GtkTreeCellDataFunc) task_pixbuf_cell_data_func,
                                                 NULL, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

        /* Completed toggle */
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_toggle_new ();
        g_object_set (cell,
                      "activatable", TRUE,
                      NULL);
        g_signal_connect_swapped (cell, "toggled",
                                  G_CALLBACK (handle_task_completed_toggled), cd);
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_add_attribute (column, cell,
                                            "active", TASK_COLUMN_COMPLETED);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

        /* Percent complete */
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell,
                      "editable", TRUE,
                      NULL);
        g_signal_connect_swapped (cell, "edited",
                                  G_CALLBACK (handle_task_percent_complete_edited), cd);
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_add_attribute (column, cell,
                                            "text", TASK_COLUMN_PERCENT_COMPLETE_TEXT);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

        /* Summary */
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_set_attributes (column, cell,
                                             "text", TASK_COLUMN_SUMMARY,
                                             "strikethrough", TASK_COLUMN_COMPLETED,
                                             "attributes", TASK_COLUMN_OVERDUE_ATTR,
                                             NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

        gtk_container_add (GTK_CONTAINER (scrolled), view);

        gtk_widget_show (view);

        return vbox;
}

static void
mark_day_on_calendar (CalendarClient *client,
		      guint           day,
		      ClockData      *cd)
{
        gtk_calendar_mark_day (GTK_CALENDAR (cd->calendar), day);
}

static void
handle_appointments_changed (ClockData *cd)
{
        GSList *events, *l;
        guint   year, month, day;

        if (cd->calendar) {
                gtk_calendar_clear_marks (GTK_CALENDAR (cd->calendar));

                calendar_client_foreach_appointment_day (cd->client,
                                                         (CalendarDayIter) mark_day_on_calendar,
                                                         cd);
        }

        gtk_list_store_clear (cd->appointments_model);

        events = calendar_client_get_events (cd->client, CALENDAR_EVENT_APPOINTMENT);
        calendar_client_get_date (cd->client, &year, &month, &day);
        for (l = events; l; l = l->next) {
                CalendarAppointment *appointment = l->data;
                GtkTreeIter          iter;
                char                *start_text;

                g_assert (CALENDAR_EVENT (appointment)->type == CALENDAR_EVENT_APPOINTMENT);

                if (appointment->is_all_day)
                        start_text = g_strdup (_("All Day"));
                else
                        start_text = format_time (cd->format,
                                                  appointment->start_time,
                                                  year, month, day,
                                                  cd->gmt_time);


                gtk_list_store_append (cd->appointments_model, &iter);
                gtk_list_store_set (cd->appointments_model, &iter,
                                    APPOINTMENT_COLUMN_UID,         appointment->uid,
                                    APPOINTMENT_COLUMN_SUMMARY,     appointment->summary,
                                    APPOINTMENT_COLUMN_DESCRIPTION, appointment->description,
                                    APPOINTMENT_COLUMN_START_TIME,  appointment->start_time,
                                    APPOINTMENT_COLUMN_START_TEXT,  start_text,
                                    APPOINTMENT_COLUMN_END_TIME,    appointment->end_time,
                                    APPOINTMENT_COLUMN_ALL_DAY,     appointment->is_all_day,
                                    APPOINTMENT_COLUMN_COLOR  ,     appointment->color_string,
                                    -1);

                g_free (start_text);
                calendar_event_free (CALENDAR_EVENT (appointment));
        }
        g_slist_free (events);

        update_frame_visibility (cd->appointment_list, GTK_TREE_MODEL (cd->appointments_model));
}

static GtkWidget *
create_appointment_list (ClockData  *cd,
                         GtkWidget **tree_view,
                         GtkWidget **scrolled_window)
{
        GtkWidget         *vbox;
        GtkWidget         *view;
        GtkWidget         *scrolled;
        GtkCellRenderer   *cell;
        GtkTreeViewColumn *column;

        vbox = create_hig_frame ( _("Appointments"));

        *scrolled_window = scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                             GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
        gtk_widget_show (scrolled);

        if (!cd->appointments_model) {
                cd->appointments_model =
                        gtk_list_store_new (N_APPOINTMENT_COLUMNS,
                                            G_TYPE_STRING,   /* uid              */
                                            G_TYPE_STRING,   /* summary          */
                                            G_TYPE_STRING,   /* description      */
                                            G_TYPE_LONG,     /* start time       */
                                            G_TYPE_STRING,   /* start time text  */
                                            G_TYPE_LONG,     /* end time         */
                                            G_TYPE_BOOLEAN,  /* all day          */
                                            G_TYPE_STRING);  /* color            */

                gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (cd->appointments_model),
                                                      APPOINTMENT_COLUMN_START_TIME,
                                                      GTK_SORT_ASCENDING);

        }

        *tree_view = view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (cd->appointments_model));
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

        /* Source color */
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_pixbuf_new ();
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_set_cell_data_func (column, cell,
                                                 (GtkTreeCellDataFunc) appointment_pixbuf_cell_data_func,
                                                 NULL, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

        /* Start time */
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_add_attribute (column, cell,
                                            "text", APPOINTMENT_COLUMN_START_TEXT);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
  
        /* Summary */
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_add_attribute (column, cell,
                                            "text", APPOINTMENT_COLUMN_SUMMARY);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

        gtk_container_add (GTK_CONTAINER (scrolled), view);

        gtk_widget_show (view);

        return vbox;
}

static void
calendar_day_activated (ClockData   *cd,
			GtkCalendar *calendar)
{
	unsigned int  day;
	unsigned int  month;
	unsigned int  year;
	char         *command_line;
	GError       *error;

	gtk_calendar_get_date (GTK_CALENDAR (cd->calendar),
			       &year, &month, &day);

	command_line = g_strdup_printf ("evolution "
					"calendar:///?startdate=%.4d%.2d%.2d",
					year, month + 1, day);

	error = NULL;

	if (!gdk_spawn_command_line_on_screen (gtk_widget_get_screen (cd->calendar),
					       command_line, &error)) {
		g_printerr ("Cannot launch calendar: %s\n", error->message);
		g_error_free (error);
	}

	g_free (command_line);
}

static void
calendar_day_selected (ClockData *cd)
{
        guint day;

        gtk_calendar_get_date (GTK_CALENDAR (cd->calendar), NULL, NULL, &day);

        calendar_client_select_day (cd->client, day);

        handle_appointments_changed (cd);
        handle_tasks_changed (cd);
}

static void
calendar_month_selected (ClockData *cd)
{
        guint year, month;

        gtk_calendar_get_date (GTK_CALENDAR (cd->calendar), &year, &month, NULL);

        calendar_client_select_month (cd->client, month, year);

        handle_appointments_changed (cd);
        handle_tasks_changed (cd);
}

typedef struct
{
        GtkWidget *calendar;
        GtkWidget *tree;
} ConstraintData;

static void
constrain_list_size (GtkWidget      *widget,
                     GtkRequisition *requisition,
                     ConstraintData *constraint)
{
        GtkRequisition req;
        int            max_height;

        /* constrain width to the calendar width */
        gtk_widget_size_request (constraint->calendar, &req);
        requisition->width = MIN (requisition->width, req.width);

        /* constrain height to be the tree height up to a max */
        max_height = (gdk_screen_get_height (gtk_widget_get_screen (widget)) - req.height) / 3;
        gtk_widget_size_request (constraint->tree, &req);

        requisition->height = MIN (req.height, max_height);
        requisition->height += 2 * widget->style->ythickness;
}

static void
setup_list_size_constraint (GtkWidget *widget,
                            GtkWidget *calendar,
                            GtkWidget *tree)
{
        ConstraintData *constraint;

        constraint           = g_new0 (ConstraintData, 1);
        constraint->calendar = calendar;
        constraint->tree     = tree;

        g_signal_connect_data (widget, "size-request",
                               G_CALLBACK (constrain_list_size), constraint,
                               (GClosureNotify) g_free, 0);
}

#endif /* HAVE_LIBECAL */

static void
add_appointments_and_tasks (ClockData *cd,
                            GtkWidget *vbox)
{
#ifdef HAVE_LIBECAL
        GtkWidget *tree_view;
        GtkWidget *scrolled_window;
        guint      year, month, day;
        
        cd->task_list = create_task_list (cd, &tree_view, &scrolled_window);
        g_object_add_weak_pointer (G_OBJECT (cd->task_list),
                                   (gpointer *) &cd->task_list);
        setup_list_size_constraint (scrolled_window, cd->calendar, tree_view);
        update_frame_visibility (cd->task_list, GTK_TREE_MODEL (cd->tasks_model));

        cd->appointment_list = create_appointment_list (cd, &tree_view, &scrolled_window);
        g_object_add_weak_pointer (G_OBJECT (cd->appointment_list),
                                   (gpointer *) &cd->appointment_list);
        setup_list_size_constraint (scrolled_window, cd->calendar, tree_view);
        update_frame_visibility (cd->appointment_list, GTK_TREE_MODEL (cd->appointments_model));

        switch (cd->orient) {
        case PANEL_APPLET_ORIENT_UP:
                gtk_box_pack_start (GTK_BOX (vbox), cd->appointment_list, TRUE, TRUE, 0);
                gtk_box_pack_start (GTK_BOX (vbox), cd->task_list, TRUE, TRUE, 0);
                break;
        default:
                gtk_box_pack_start (GTK_BOX (vbox), cd->task_list, TRUE, TRUE, 0);
                gtk_box_pack_start (GTK_BOX (vbox), cd->appointment_list, TRUE, TRUE, 0);
                break;
        }

        if (!cd->client) {
                cd->client = calendar_client_new ();

                g_signal_connect_swapped (cd->client, "tasks-changed",
                                          G_CALLBACK (handle_tasks_changed), cd);
                g_signal_connect_swapped (cd->client, "appointments-changed",
                                          G_CALLBACK (handle_appointments_changed), cd);
        }

        gtk_calendar_get_date (GTK_CALENDAR (cd->calendar), &year, &month, &day);

        calendar_client_select_day   (cd->client, day);
        calendar_client_select_month (cd->client, month, year);

        handle_tasks_changed (cd);
        handle_appointments_changed (cd);

        g_signal_connect_swapped (cd->calendar, "day-selected-double-click",
                                  G_CALLBACK (calendar_day_activated), cd);
        g_signal_connect_swapped (cd->calendar, "day-selected",
                                  G_CALLBACK (calendar_day_selected), cd);
        g_signal_connect_swapped (cd->calendar, "month-changed",
                                  G_CALLBACK (calendar_month_selected), cd);
#endif /* HAVE_LIBECAL */
}

static GtkWidget *
create_calendar (ClockData *cd,
		 GdkScreen *screen)
{
	GtkWindow                 *window;
        GtkWidget                 *frame;
        GtkWidget                 *vbox;
	GtkCalendarDisplayOptions  options;
        struct tm                 *tm;

	window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_set_icon_name (window, CLOCK_ICON);
        gtk_window_set_screen (window, screen);

        frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
        gtk_container_add (GTK_CONTAINER (window), frame);
        gtk_widget_show (frame);

        vbox = gtk_vbox_new (FALSE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
        gtk_container_add (GTK_CONTAINER (frame), vbox);
        gtk_widget_show (vbox);

	gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_decorated (window, FALSE);
	gtk_window_set_resizable (window, FALSE);
	gtk_window_stick (window);
	gtk_window_set_title (window, _("Calendar"));

	g_signal_connect (window, "delete_event",
			  G_CALLBACK (delete_event), cd);
			
	g_signal_connect (window, "key_press_event",
			  G_CALLBACK (close_on_escape), cd);
			
	cd->calendar = gtk_calendar_new ();
        g_object_add_weak_pointer (G_OBJECT (cd->calendar),
                                   (gpointer *) &cd->calendar);

	options = gtk_calendar_get_display_options (GTK_CALENDAR (cd->calendar));
	if (cd->showweek)
		options |= GTK_CALENDAR_SHOW_WEEK_NUMBERS;
	gtk_calendar_set_display_options (GTK_CALENDAR (cd->calendar), options);

	if (cd->gmt_time)
		tm = gmtime (&cd->current_time);
	else
		tm = localtime (&cd->current_time);
        gtk_calendar_select_month (GTK_CALENDAR (cd->calendar),
                                   tm->tm_mon,
                                   tm->tm_year + 1900);
        gtk_calendar_select_day (GTK_CALENDAR (cd->calendar), tm->tm_mday);

	switch (cd->orient) {
        case PANEL_APPLET_ORIENT_UP:
                add_appointments_and_tasks (cd, vbox);
                gtk_box_pack_start (GTK_BOX (vbox), cd->calendar, TRUE, FALSE, 0);
                break;
        default:
                gtk_box_pack_start (GTK_BOX (vbox), cd->calendar, TRUE, FALSE, 0);
                add_appointments_and_tasks (cd, vbox);
                break;
        }
        gtk_widget_show (cd->calendar);

	return GTK_WIDGET (window);
}

static void
position_calendar_popup (ClockData *cd,
                         GtkWidget *window,
                         GtkWidget *button)
{
	GtkRequisition  req;
	GdkScreen      *screen;
	GdkRectangle    monitor;
	GdkGravity      gravity = GDK_GRAVITY_NORTH_WEST;
	int             button_w, button_h;
	int             x, y;
	int             w, h;
	int             i, n;
	gboolean        found_monitor = FALSE;

	/* Get root origin of the toggle button, and position above that. */
	gdk_window_get_origin (button->window, &x, &y);

	gtk_window_get_size (GTK_WINDOW (window), &w, &h);
	gtk_widget_size_request (window, &req);
	w = req.width;
	h = req.height;

	button_w = button->allocation.width;
	button_h = button->allocation.height;

	screen = gtk_window_get_screen (GTK_WINDOW (window));

	n = gdk_screen_get_n_monitors (screen);
	for (i = 0; i < n; i++) {
		gdk_screen_get_monitor_geometry (screen, i, &monitor);
		if (x >= monitor.x && x <= monitor.x + monitor.width &&
		    y >= monitor.y && y <= monitor.y + monitor.height) {
			found_monitor = TRUE;
			break;
		}
	}

	if ( ! found_monitor) {
		/* eek, we should be on one of those xinerama
		   monitors */
		monitor.x = 0;
		monitor.y = 0;
		monitor.width = gdk_screen_get_width (screen);
		monitor.height = gdk_screen_get_height (screen);
	}
		
	/* Based on panel orientation, position the popup.
	 * Ignore window gravity since the window is undecorated.
	 * The orientations are all named backward from what
	 * I expected.
	 */
	switch (cd->orient) {
	case PANEL_APPLET_ORIENT_RIGHT:
		x += button_w;
		if ((y + h) > monitor.y + monitor.height)
			y -= (y + h) - (monitor.y + monitor.height);

		if ((y + h) > (monitor.height / 2))
			gravity = GDK_GRAVITY_SOUTH_WEST;
		else
			gravity = GDK_GRAVITY_NORTH_WEST;

		break;
	case PANEL_APPLET_ORIENT_LEFT:
		x -= w;
		if ((y + h) > monitor.y + monitor.height)
			y -= (y + h) - (monitor.y + monitor.height);

		if ((y + h) > (monitor.height / 2))
			gravity = GDK_GRAVITY_SOUTH_EAST;
		else
			gravity = GDK_GRAVITY_NORTH_EAST;

		break;
	case PANEL_APPLET_ORIENT_DOWN:
		y += button_h;
		if ((x + w) > monitor.x + monitor.width)
			x -= (x + w) - (monitor.x + monitor.width);

		gravity = GDK_GRAVITY_NORTH_WEST;

		break;
	case PANEL_APPLET_ORIENT_UP:
		y -= h;
		if ((x + w) > monitor.x + monitor.width)
			x -= (x + w) - (monitor.x + monitor.width);

		gravity = GDK_GRAVITY_SOUTH_WEST;

		break;
	}
		
	gtk_window_move (GTK_WINDOW (window), x, y);
	gtk_window_set_gravity (GTK_WINDOW (window), gravity);
}

static void
present_calendar_popup (ClockData *cd,
			GtkWidget *window,
			GtkWidget *button)
{
        position_calendar_popup (cd, window, button);
        gtk_window_present (GTK_WINDOW (window));
}

static void
update_popup (ClockData *cd)
{
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cd->toggle))) {
                if (cd->calendar_popup)
                        gtk_widget_destroy (cd->calendar_popup);
                cd->calendar_popup = NULL;
                return;
        }

        if (!cd->calendar_popup) {
                cd->calendar_popup = create_calendar (cd, gtk_widget_get_screen (cd->applet));
                g_object_add_weak_pointer (G_OBJECT (cd->calendar_popup),
                                           (gpointer *) &cd->calendar_popup);
        }

        if (cd->calendar_popup && GTK_WIDGET_REALIZED (cd->toggle)) {
#ifdef HAVE_LIBECAL
                if (cd->tasks_filter && cd->task_list)
                        gtk_tree_model_filter_refilter (cd->tasks_filter);
#endif

                present_calendar_popup (cd, cd->calendar_popup, cd->toggle);
                gtk_window_present (GTK_WINDOW (cd->calendar_popup));
        }
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

/* Don't request smaller size then the last one we did, this avoids
   jumping when proportional fonts are used.  We must take care to
   call "unfix_size" whenever options are changed or such where
   we'd want to forget the fixed size */
static void
clock_size_request (GtkWidget *clock, GtkRequisition *req, gpointer data)
{
	ClockData *cd = data;

	if (req->width > cd->fixed_width)
		cd->fixed_width = req->width;
	if (req->height > cd->fixed_height)
		cd->fixed_height = req->height;
	req->width = cd->fixed_width;
	req->height = cd->fixed_height;
}

static inline void
force_no_focus_padding (GtkWidget *widget)
{
        gboolean first_time = TRUE;

        if (first_time) {
                gtk_rc_parse_string ("\n"
                                     "   style \"clock-applet-button-style\"\n"
                                     "   {\n"
                                     "      GtkWidget::focus-line-width=0\n"
                                     "      GtkWidget::focus-padding=0\n"
                                     "   }\n"
                                     "\n"
                                     "    widget \"*.clock-applet-button\" style \"clock-applet-button-style\"\n"
                                     "\n");
                first_time = FALSE;
        }

        gtk_widget_set_name (widget, "clock-applet-button");
}

static void
create_clock_widget (ClockData *cd)
{
	GtkWidget *clock;
	GtkWidget *toggle;
	GtkWidget *alignment;

	clock = gtk_label_new ("hmm?");
	g_signal_connect (clock, "size_request",
			  G_CALLBACK (clock_size_request),
			  cd);
	g_signal_connect_swapped (clock, "style_set",
				  G_CALLBACK (unfix_size),
				  cd);
	gtk_label_set_justify (GTK_LABEL (clock), GTK_JUSTIFY_CENTER);
	gtk_widget_show (clock);

	toggle = gtk_toggle_button_new ();
	gtk_container_set_resize_mode (GTK_CONTAINER (toggle), GTK_RESIZE_IMMEDIATE);
	gtk_button_set_relief (GTK_BUTTON (toggle), GTK_RELIEF_NONE);

        force_no_focus_padding (toggle);

	alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	gtk_container_add (GTK_CONTAINER (alignment), clock);
	gtk_container_set_resize_mode (GTK_CONTAINER (alignment), GTK_RESIZE_IMMEDIATE);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (toggle), alignment);

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

static void
update_orient (ClockData *cd)
{
	const gchar *text;
	int          min_width;
	gdouble      new_angle;
	gdouble      angle;

	text = gtk_label_get_text (GTK_LABEL (cd->clockw));
	min_width = calculate_minimum_width (cd->toggle, text);

	if (cd->orient == PANEL_APPLET_ORIENT_LEFT &&
	    min_width > cd->toggle->allocation.width)
		new_angle = 270;
	else if (cd->orient == PANEL_APPLET_ORIENT_RIGHT &&
		 min_width > cd->toggle->allocation.width)
		new_angle = 90;
	else
		new_angle = 0;

	angle = gtk_label_get_angle (GTK_LABEL (cd->clockw));
	if (angle != new_angle) {
		unfix_size (cd);
		gtk_label_set_angle (GTK_LABEL (cd->clockw), new_angle);
	}
}

/* this is when the panel orientation changes */
static void
applet_change_orient (PanelApplet       *applet,
		      PanelAppletOrient  orient,
		      ClockData         *cd)
{
	if (orient == cd->orient)
		return;

        cd->orient = orient;
        
        unfix_size (cd);
        update_clock (cd);
        update_popup (cd);
}

/* this is when the panel size changes */
static void
toggle_change_pixel_size (GtkWidget     *widget,
			  GtkAllocation *allocation,
			  ClockData	*cd)
{
	int new_size;

	if (cd->old_allocation.width  == allocation->width &&
	    cd->old_allocation.height == allocation->height)
		return;

	cd->old_allocation.width  = allocation->width;
	cd->old_allocation.height = allocation->height;

	if (cd->orient == PANEL_APPLET_ORIENT_LEFT ||
	    cd->orient == PANEL_APPLET_ORIENT_RIGHT)
		new_size = allocation->width;
	else
		new_size = allocation->height;

	cd->size = new_size;

        unfix_size (cd);
	update_timeformat (cd);
	update_clock (cd);
}

static void
copy_time (BonoboUIComponent *uic,
	   ClockData         *cd,
	   const gchar       *verbname)
{
	char string[256];
	char *utf8;

	if (cd->format == CLOCK_FORMAT_UNIX) {
		g_snprintf (string, sizeof(string), "%lu",
			    (unsigned long)cd->current_time);
	} else if (cd->format == CLOCK_FORMAT_INTERNET) {
		float itime = get_itime (cd->current_time);
		if (cd->showseconds)
			g_snprintf (string, sizeof (string), "@%3.2f", itime);
		else
			g_snprintf (string, sizeof (string), "@%3.0f", itime);
	} else {
		struct tm *tm;
		char      *format;

		if (cd->format == CLOCK_FORMAT_CUSTOM) {
			format = g_locale_from_utf8 (cd->custom_format, -1,
						     NULL, NULL, NULL);
		} else if (cd->format == CLOCK_FORMAT_12) {
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
			tm = gmtime (&cd->current_time);
		else
			tm = localtime (&cd->current_time);

		if (!format)
			strcpy (string, "???");
		else if (strftime (string, sizeof (string), format, tm) <= 0)
			strcpy (string, "???");
		g_free (format);
	}

	utf8 = g_locale_to_utf8 (string, -1, NULL, NULL, NULL);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				utf8, -1);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				utf8, -1);
	g_free (utf8);
}

static void
copy_date (BonoboUIComponent *uic,
	   ClockData         *cd,
	   const gchar       *verbname)
{
	struct tm *tm;
	char string[256];
	char *utf8, *loc;

	if (cd->gmt_time)
		tm = gmtime (&cd->current_time);
	else
		tm = localtime (&cd->current_time);

	loc = g_locale_from_utf8 (_("%A, %B %d %Y"), -1, NULL, NULL, NULL);
	if (!loc)
		strcpy (string, "???");
	else if (strftime (string, sizeof (string), loc, tm) <= 0)
		strcpy (string, "???");
	g_free (loc);
	
	utf8 = g_locale_to_utf8 (string, -1, NULL, NULL, NULL);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				utf8, -1);
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
	char     **argv;
        char      *path;

        if (!tool || tool[0] == '\0')
                return FALSE;

        if (!g_shell_parse_argv (tool, NULL, &argv, NULL))
                return FALSE;

        if (!(path = g_find_program_in_path (argv [0]))) {
                g_strfreev (argv);
                return FALSE;
        }

        g_free (path);

	err = NULL;
	if (gdk_spawn_on_screen (screen,
                                 NULL,
                                 argv,
                                 NULL,
                                 G_SPAWN_SEARCH_PATH,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &err)) {
                g_strfreev (argv);
		return TRUE;
	}

        g_strfreev (argv);

	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 _("Failed to launch time configuration tool: %s"),
					 err->message);
	g_error_free (err);
		
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_window_set_icon_name (GTK_WINDOW (dialog), CLOCK_ICON);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_screen (GTK_WINDOW (dialog), screen);
			
	gtk_widget_show_all (dialog);			
		
	return TRUE;
}

static void
config_date (BonoboUIComponent *uic,
             ClockData         *cd,
             const char        *verbname)
{
	GtkWidget *dialog;
	GdkScreen *screen;
	int i;

	screen = gtk_widget_get_screen (cd->applet);

	if (try_config_tool (screen, cd->config_tool))
		return;
	
	for (i = 0; i < G_N_ELEMENTS (clock_config_tools); i++)
		if (try_config_tool (screen, clock_config_tools [i]))
			return;
		
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 _("Failed to locate a program for configuring "
					   "the date and time. Perhaps none is installed?"));
		
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
		
	gtk_window_set_icon_name (GTK_WINDOW (dialog), CLOCK_ICON);
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
format_changed (GConfClient  *client,
                guint         cnxn_id,
                GConfEntry   *entry,
                ClockData    *clock)
{
	const char  *value;
	int          new_format;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
		return;

	value = gconf_value_get_string (entry->value);
	if (!gconf_string_to_enum (format_type_enum_map, value, &new_format))
		return;

	if (!clock->can_handle_format_12 && new_format == CLOCK_FORMAT_12)
		new_format = CLOCK_FORMAT_24;

	if (new_format == clock->format)
		return;

	clock->format = new_format;
	update_timeformat (clock);
	refresh_clock_timeout (clock);
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

static gboolean
check_config_tool_command (const char *config_tool)
{
        char **argv;
        char  *path;

        if (!config_tool || config_tool[0] == '\0')
                return FALSE;

        argv = NULL;
        if (!g_shell_parse_argv (config_tool, NULL, &argv, NULL))
                return FALSE;

        if (!(path = g_find_program_in_path (argv [0]))) {
                g_strfreev (argv);
                return FALSE;
        }

        g_free (path);
        g_strfreev (argv);

        return TRUE;
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

        if (check_config_tool_command (value)) {
                g_free (clock->config_tool);
                clock->config_tool = g_strdup (value);
        }
}

static void
custom_format_changed (GConfClient  *client,
                       guint         cnxn_id,
                       GConfEntry   *entry,
                       ClockData    *clock)
{
	const char *value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_STRING)
		return;

	value = gconf_value_get_string (entry->value);

        g_free (clock->custom_format);
	clock->custom_format = g_strdup (value);

	if (clock->format == CLOCK_FORMAT_CUSTOM)
		refresh_clock (clock);
}

static void
show_week_changed (GConfClient  *client,
		   guint         cnxn_id,
		   GConfEntry   *entry,
		   ClockData    *clock)
{
	GtkCalendarDisplayOptions options;
	gboolean value;
	
	if (!entry->value || entry->value->type != GCONF_VALUE_BOOL)
		return;

	value = gconf_value_get_bool (entry->value);
	
	if (clock->showweek == (value != 0))
		return;

	clock->showweek = (value != 0);

	if (clock->calendar_popup != NULL) {
		options = gtk_calendar_get_display_options (GTK_CALENDAR (clock->calendar));

		if (clock->showweek)
			options |= GTK_CALENDAR_SHOW_WEEK_NUMBERS;
		else
			options &= ~(GTK_CALENDAR_SHOW_WEEK_NUMBERS);

		gtk_calendar_set_display_options (GTK_CALENDAR (clock->calendar),
						  options);

                position_calendar_popup (clock, clock->calendar_popup,
					 clock->toggle);
	}
}

static void
setup_gconf (ClockData *clock)
{
	GConfClient *client;
	char        *key;

	client = gconf_client_get_default ();

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_FORMAT);
	clock->listeners [0] =
		gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc) format_changed,
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
					       KEY_CONFIG_TOOL);
	clock->listeners [4] =
		gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc) config_tool_changed,
				clock, NULL, NULL);
	g_free (key);

	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_CUSTOM_FORMAT);
	clock->listeners [5] =
		gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc) custom_format_changed,
				clock, NULL, NULL);
	g_free (key);
	
	key = panel_applet_gconf_get_full_key (PANEL_APPLET (clock->applet),
					       KEY_SHOW_WEEK);
	clock->listeners [6] =
			gconf_client_notify_add (
				client, key,
				(GConfClientNotifyFunc) show_week_changed,
				clock, NULL, NULL);
	g_free (key);

	g_object_unref (G_OBJECT (client));
}

static void
clock_migrate_to_26 (ClockData *clock)
{
	gboolean  unixtime;
	gboolean  internettime;
	int       hourformat;

	internettime = panel_applet_gconf_get_bool (PANEL_APPLET (clock->applet),
						    "internet_time",
						    NULL);
	unixtime = panel_applet_gconf_get_bool (PANEL_APPLET (clock->applet),
						"unix_time",
						NULL);
	hourformat = panel_applet_gconf_get_int (PANEL_APPLET (clock->applet),
						 "hour_format",
						 NULL);

	if (unixtime)
		clock->format = CLOCK_FORMAT_UNIX;
	else if (internettime)
		clock->format = CLOCK_FORMAT_INTERNET;
	else if (hourformat == 12)
		clock->format = CLOCK_FORMAT_12;
	else if (hourformat == 24)
		clock->format = CLOCK_FORMAT_24;

	panel_applet_gconf_set_string (PANEL_APPLET (clock->applet),
				       KEY_FORMAT,
				       gconf_enum_to_string (format_type_enum_map,
							     clock->format),
				       NULL);
}

static gboolean
fill_clock_applet (PanelApplet *applet)
{
	ClockData         *cd;
	BonoboUIComponent *popup_component;
	GError            *error;
        int                format;
	char              *format_str;
	
	panel_applet_add_preferences (applet, "/schemas/apps/clock_applet/prefs", NULL);
	panel_applet_set_flags (applet, PANEL_APPLET_EXPAND_MINOR);
	
	cd = g_new0 (ClockData, 1);
	cd->fixed_width = -1;
	cd->fixed_height = -1;

	cd->applet = GTK_WIDGET (applet);

	setup_gconf (cd);

        cd->format = CLOCK_FORMAT_INVALID;

	format_str = panel_applet_gconf_get_string (applet, KEY_FORMAT, NULL);
	if (format_str &&
            gconf_string_to_enum (format_type_enum_map, format_str, &format))
                cd->format = format;
	else
		clock_migrate_to_26 (cd);

        g_free (format_str);

	if (cd->format == CLOCK_FORMAT_INVALID)
		cd->format = clock_locale_format ();

	cd->custom_format = panel_applet_gconf_get_string (applet, KEY_CUSTOM_FORMAT, NULL);
	cd->showseconds = panel_applet_gconf_get_bool (applet, KEY_SHOW_SECONDS, NULL);
	
	error = NULL;
	cd->showdate = panel_applet_gconf_get_bool (applet, KEY_SHOW_DATE, &error);
	if (error) {
		g_error_free (error);
		/* if on a small screen don't show date by default */
		if (gdk_screen_width () <= 800)
			cd->showdate = FALSE;
		else
			cd->showdate = TRUE;
	}

	cd->gmt_time = panel_applet_gconf_get_bool (applet, KEY_GMT_TIME, NULL);
	cd->showweek = panel_applet_gconf_get_bool (applet, KEY_SHOW_WEEK, NULL);
	cd->config_tool = panel_applet_gconf_get_string (applet, KEY_CONFIG_TOOL, NULL);

	cd->timeformat = NULL;

	cd->can_handle_format_12 = (clock_locale_format () == CLOCK_FORMAT_12);
	if (!cd->can_handle_format_12 && cd->format == CLOCK_FORMAT_12)
		cd->format = CLOCK_FORMAT_24;

	create_clock_widget (cd);

	gtk_container_set_border_width (GTK_CONTAINER (cd->applet), 0);
	gtk_container_set_border_width (GTK_CONTAINER (cd->toggle), 0);
	gtk_container_add (GTK_CONTAINER (cd->applet), cd->toggle);

#ifndef CLOCK_INPROCESS
	gtk_window_set_default_icon_name (CLOCK_ICON);
#endif
	gtk_widget_show (cd->applet);

	/* FIXME: Update this comment. */
	/* we have to bind change_orient before we do applet_widget_add
	   since we need to get an initial change_orient signal to set our
	   initial oriantation, and we get that during the _add call */
	g_signal_connect (G_OBJECT (cd->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient),
			  cd);

	g_signal_connect (G_OBJECT (cd->toggle),
			  "size_allocate",
			  G_CALLBACK (toggle_change_pixel_size),
			  cd);

	panel_applet_set_background_widget (PANEL_APPLET (cd->applet),
					    GTK_WIDGET (cd->applet));

	panel_applet_setup_menu_from_file (PANEL_APPLET (cd->applet),
					   NULL,
					   "GNOME_ClockApplet.xml",
					   NULL,
					   clock_menu_verbs,
					   cd);

	popup_component = panel_applet_get_popup_component (PANEL_APPLET (cd->applet));

	if (panel_applet_get_locked_down (PANEL_APPLET (cd->applet))) {
		bonobo_ui_component_set_prop (popup_component,
					      "/commands/ClockPreferences",
					      "hidden", "1",
					      NULL);
		bonobo_ui_component_set_prop (popup_component,
					      "/commands/ClockConfig",
					      "hidden", "1",
					      NULL);
	}

        if (!check_config_tool_command (cd->config_tool)) {
                g_free (cd->config_tool);
                cd->config_tool = NULL;
        }

        if (!cd->config_tool) {
                int i;

                for (i = 0; i < G_N_ELEMENTS (clock_config_tools); i++)
                        if (check_config_tool_command (clock_config_tools [i])) {
                                cd->config_tool = g_strdup (clock_config_tools [i]);
                                break;
                        }
        }

        bonobo_ui_component_set_prop (popup_component,
                                      "/commands/ClockConfig",
                                      "hidden", cd->config_tool ? "0" : "1",
                                      NULL);

	return TRUE;
}

static void
setup_writability_sensitivity (ClockData *clock, GtkWidget *w, GtkWidget *label, const char *key)
{
	char *fullkey;
	GConfClient *client;

	client = gconf_client_get_default ();

	fullkey = panel_applet_gconf_get_full_key
		(PANEL_APPLET (clock->applet), key);

	if ( ! gconf_client_key_is_writable (client, fullkey, NULL)) {
		g_object_set_data (G_OBJECT (w), NEVER_SENSITIVE,
				   GINT_TO_POINTER (1));
		gtk_widget_set_sensitive (w, FALSE);
		if (label != NULL) {
			g_object_set_data (G_OBJECT (label), NEVER_SENSITIVE,
					   GINT_TO_POINTER (1));
			gtk_widget_set_sensitive (label, FALSE);
		}
	}

	g_free (fullkey);

	g_object_unref (G_OBJECT (client));
}

static void
update_properties_for_format (ClockData   *cd,
                              GtkComboBox *combo,
                              ClockFormat  format)
{

        /* show the custom format things the first time we actually
         * have a custom format set in GConf, but after that don't
         * unshow it if the format changes
         */
        if (!cd->custom_format_shown &&
            (cd->format == CLOCK_FORMAT_CUSTOM ||
             (cd->custom_format && cd->custom_format [0]))) {
                gtk_widget_show (cd->custom_hbox);
                gtk_widget_show (cd->custom_label);
                gtk_widget_show (cd->custom_entry);
                
                gtk_combo_box_append_text (combo, _("Custom format"));

                cd->custom_format_shown = TRUE;
        }

        /* Some combinations of options do not make sense */
        switch (format) {
        case CLOCK_FORMAT_12:
        case CLOCK_FORMAT_24:
                gtk_widget_set_sensitive (cd->showseconds_check, TRUE);
                gtk_widget_set_sensitive (cd->showdate_check, TRUE);
                gtk_widget_set_sensitive (cd->gmt_time_check, TRUE);
		gtk_widget_set_sensitive (cd->custom_entry, FALSE);
		gtk_widget_set_sensitive (cd->custom_label, FALSE);
                break;
        case CLOCK_FORMAT_UNIX:
                gtk_widget_set_sensitive (cd->showseconds_check, FALSE);
                gtk_widget_set_sensitive (cd->showdate_check, FALSE);
                gtk_widget_set_sensitive (cd->gmt_time_check, FALSE);
                gtk_widget_set_sensitive (cd->custom_entry, FALSE);
                gtk_widget_set_sensitive (cd->custom_label, FALSE);
                break;
        case CLOCK_FORMAT_INTERNET:
                gtk_widget_set_sensitive (cd->showseconds_check, TRUE);
                gtk_widget_set_sensitive (cd->showdate_check, FALSE);
		gtk_widget_set_sensitive (cd->gmt_time_check, FALSE);
		gtk_widget_set_sensitive (cd->custom_entry, FALSE);
		gtk_widget_set_sensitive (cd->custom_label, FALSE);
                break;
	case CLOCK_FORMAT_CUSTOM:
		gtk_widget_set_sensitive (cd->showseconds_check, FALSE);
		gtk_widget_set_sensitive (cd->showdate_check, FALSE);
		gtk_widget_set_sensitive (cd->gmt_time_check, TRUE);
		gtk_widget_set_sensitive (cd->custom_entry, TRUE);
		gtk_widget_set_sensitive (cd->custom_label, TRUE);
                break;
        default:
                g_assert_not_reached ();
                break;
	}
}

static void
set_format_cb (GtkComboBox *combo,
	       ClockData   *cd)
{
        ClockFormat format;

	/* valid values begin from 1 */
	if (cd->can_handle_format_12)
		format = gtk_combo_box_get_active (combo) + 1;
	else
		format = gtk_combo_box_get_active (combo) + 2;

        update_properties_for_format (cd, combo, format);

        if (cd->format != format)
                panel_applet_gconf_set_string (PANEL_APPLET (cd->applet),
                                               KEY_FORMAT,
                                               gconf_enum_to_string (format_type_enum_map, format),
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
set_gmt_time_cb (GtkWidget *w,
		 ClockData *clock)
{
	panel_applet_gconf_set_bool (PANEL_APPLET (clock->applet),
				     KEY_GMT_TIME,
				     GTK_TOGGLE_BUTTON (w)->active,
				     NULL);
}

static void
set_custom_format_cb (GtkEntry  *entry,
		      ClockData *cd)
{
	const char *custom_format;

	custom_format = gtk_entry_get_text (entry);
	panel_applet_gconf_set_string (PANEL_APPLET (cd->applet),
				       KEY_CUSTOM_FORMAT, custom_format, NULL);
}

static void
properties_response_cb (GtkWidget *widget,
			int        id,
			ClockData *cd)
{
	
	if (id == GTK_RESPONSE_HELP) {
		GError *error = NULL;

		gnome_help_display_desktop_on_screen (
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

			gtk_window_set_icon_name (GTK_WINDOW (dialog),
						  CLOCK_ICON);
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
	GtkWidget *combo;
	GtkWidget *label;

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

	gtk_window_set_icon_name (GTK_WINDOW (cd->props), CLOCK_ICON);
	gtk_dialog_set_has_separator (GTK_DIALOG (cd->props), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (cd->props), GTK_RESPONSE_CLOSE);
	gtk_window_set_resizable (GTK_WINDOW (cd->props), FALSE);
	gtk_window_set_screen (GTK_WINDOW (cd->props),
			       gtk_widget_get_screen (cd->applet));
	gtk_container_set_border_width (GTK_CONTAINER (cd->props), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (cd->props)->vbox), 2);
		
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

        combo = gtk_combo_box_new_text ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

	if (cd->can_handle_format_12)
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("12 hour"));
        gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("24 hour"));
        gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("UNIX time"));
        gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Internet time"));

	gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
	gtk_widget_show (combo);

	cd->custom_hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), cd->custom_hbox, TRUE, TRUE, 0);

	cd->custom_label = gtk_label_new_with_mnemonic (_("Custom _format:"));
	gtk_label_set_use_markup (GTK_LABEL (cd->custom_label), TRUE);
	gtk_label_set_justify (GTK_LABEL (cd->custom_label),
			       GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (cd->custom_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (cd->custom_hbox),
                            cd->custom_label,
			    FALSE, FALSE, 0);

	cd->custom_entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (cd->custom_hbox), 
                            cd->custom_entry,
			    FALSE, FALSE, 0);
	gtk_entry_set_text (GTK_ENTRY (cd->custom_entry),
			    cd->custom_format);
	g_signal_connect (cd->custom_entry, "changed",
			  G_CALLBACK (set_custom_format_cb),
			  cd);

	cd->showseconds_check = gtk_check_button_new_with_mnemonic (_("Show _seconds"));
	gtk_box_pack_start (GTK_BOX (vbox), cd->showseconds_check, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->showseconds_check),
	                              cd->showseconds);
	g_signal_connect (cd->showseconds_check, "toggled",
			  G_CALLBACK (set_show_seconds_cb),
			  cd);
	gtk_widget_show (cd->showseconds_check);

	cd->showdate_check = gtk_check_button_new_with_mnemonic (_("Show _date"));
	gtk_box_pack_start (GTK_BOX (vbox), cd->showdate_check, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->showdate_check),
	                              cd->showdate);
	g_signal_connect (cd->showdate_check, "toggled",
			  G_CALLBACK (set_show_date_cb),
			  cd);
  	gtk_widget_show (cd->showdate_check);

	cd->gmt_time_check = gtk_check_button_new_with_mnemonic (_("Use _UTC"));
	gtk_box_pack_start (GTK_BOX (vbox), cd->gmt_time_check, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cd->gmt_time_check),
	                              cd->gmt_time);
	g_signal_connect (cd->gmt_time_check, "toggled",
			  G_CALLBACK (set_gmt_time_cb),
			  cd);	
	gtk_widget_show (cd->gmt_time_check);

	g_signal_connect (cd->props, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
                          &cd->props);
	g_signal_connect (cd->props, "response",
			  G_CALLBACK (properties_response_cb),
                          cd);

        update_properties_for_format (cd, GTK_COMBO_BOX (combo), cd->format);

	/* valid values begin from 1 */
	if (cd->can_handle_format_12)
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo),
					  cd->format - 1);
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo),
					  cd->format - 2);

        g_signal_connect (combo, "changed",
                          G_CALLBACK (set_format_cb), cd);

	/* Now set up the sensitivity based on gconf key writability */
	setup_writability_sensitivity (cd, combo, label, KEY_FORMAT);
	setup_writability_sensitivity (cd, cd->custom_entry, cd->custom_label,
				       KEY_CUSTOM_FORMAT);
	setup_writability_sensitivity (cd, cd->showseconds_check, NULL, KEY_SHOW_SECONDS);
	setup_writability_sensitivity (cd, cd->showdate_check, NULL, KEY_SHOW_DATE);
	setup_writability_sensitivity (cd, cd->gmt_time_check, NULL, KEY_GMT_TIME);

	gtk_widget_show (cd->props);
}

static void
display_help_dialog (BonoboUIComponent *uic,
		     ClockData         *cd,
		     const gchar       *verbname)
{
	GError *error = NULL;

	gnome_help_display_desktop_on_screen (NULL, "clock", "clock", NULL,
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

		gtk_window_set_icon_name (GTK_WINDOW (dialog), CLOCK_ICON);
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

	if (cd->about) {
		gtk_window_set_screen (GTK_WINDOW (cd->about),
				       gtk_widget_get_screen (cd->applet));
		gtk_window_present (GTK_WINDOW (cd->about));
		return;
	}

	cd->about = gtk_about_dialog_new ();
	g_object_set (cd->about,
		      "name",  _("Clock"),
		      "version", VERSION,
		      "copyright", "Copyright \xc2\xa9 1998-2004 Free Software Foundation, Inc.",
		      "comments", _("The Clock displays the current time and date"),
		      "authors", authors,
		      "documenters", documenters,
		      "translator-credits", _("translator-credits"),
		      "logo-icon-name", CLOCK_ICON,
		      NULL);
	
	gtk_window_set_icon_name (GTK_WINDOW (cd->about), CLOCK_ICON);
	gtk_window_set_wmclass (GTK_WINDOW (cd->about), "clock", "Clock");
	gtk_window_set_screen (GTK_WINDOW (cd->about),
			       gtk_widget_get_screen (cd->applet));

	g_signal_connect (G_OBJECT(cd->about), "destroy",
			  (GCallback)gtk_widget_destroyed, &cd->about);
	
	g_signal_connect (cd->about, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	gtk_widget_show (cd->about);
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

#ifdef CLOCK_INPROCESS
PANEL_APPLET_BONOBO_SHLIB_FACTORY ("OAFIID:GNOME_ClockApplet_Factory",
				   PANEL_TYPE_APPLET,
				   "ClockApplet",
				   clock_factory,
				   NULL);
#else
PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_ClockApplet_Factory",
                             PANEL_TYPE_APPLET,
                             "ClockApplet",
                             "0",
                             clock_factory,
                             NULL);
#endif

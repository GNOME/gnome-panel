/*
 * calendar-window.c: toplevel window containing a calendar and
 * tasks/appointments
 *
 * Copyright (C) 2007 Vincent Untz <vuntz@gnome.org>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Vincent Untz <vuntz@gnome.org>
 *
 * Most of the original code comes from clock.c
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

#include <config.h>

#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "calendar-window.h"

#include "clock-applet.h"
#include "clock-utils.h"
#include "clock-typebuiltins.h"
#ifdef HAVE_EDS
#include "calendar-client.h"
#endif

#define KEY_LOCATIONS_EXPANDED      "expand-locations"
#ifdef HAVE_EDS
#  define KEY_APPOINTMENTS_EXPANDED "expand-appointments"
#  define KEY_BIRTHDAYS_EXPANDED    "expand-birthdays"
#  define KEY_TASKS_EXPANDED        "expand-tasks"
#  define KEY_WEATHER_EXPANDED      "expand-weather"

#  define SCHEMA_CALENDAR_APP       "org.gnome.desktop.default-applications.office.calendar"
#  define SCHEMA_TASKS_APP          "org.gnome.desktop.default-applications.office.tasks"
#endif

enum {
	EDIT_LOCATIONS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _CalendarWindowPrivate {
	GtkWidget  *calendar;

	GSettings  *settings;

	gboolean     invert_order;
	gboolean     show_weeks;

	gboolean     locked_down;

	GtkWidget *locations_list;

#ifdef HAVE_EDS
	GDesktopClockFormat time_format;

        CalendarClient *client;

        GtkWidget *appointment_list;
        GtkWidget *birthday_list;
        GtkWidget *weather_list;
        GtkWidget *task_list;

        GtkListStore *appointments_model;
        GtkListStore *tasks_model;

        GtkTreeSelection *previous_selection;

        GtkTreeModelFilter *appointments_filter;
        GtkTreeModelFilter *birthdays_filter;
        GtkTreeModelFilter *tasks_filter;
        GtkTreeModelFilter *weather_filter;
#endif /* HAVE_EDS */
};

G_DEFINE_TYPE_WITH_PRIVATE (CalendarWindow, calendar_window, GTK_TYPE_WINDOW)

enum {
	PROP_0,
	PROP_INVERTORDER,
	PROP_SHOWWEEKS,
	PROP_SETTINGS,
	PROP_LOCKEDDOWN
};

static GSettings *calendar_window_get_settings   (CalendarWindow *calwin);
static void    calendar_window_set_settings      (CalendarWindow *calwin,
						  GSettings      *settings);
static gboolean calendar_window_get_locked_down   (CalendarWindow *calwin);
static void    calendar_window_set_locked_down    (CalendarWindow *calwin,
						   gboolean        locked_down);
static GtkWidget * create_hig_frame 		  (CalendarWindow *calwin,
		  				   const char *title,
                  				   const char *button_label,
		  				   const char *key,
						   GCallback   callback,
						   gboolean    bind_to_locked_down);

#ifdef HAVE_EDS

static void
clock_launch_calendar_tasks_app (CalendarWindow *calwin,
                                 const gchar    *schema_program,
                                 const gchar    *argument)
{
	GSettings *settings;
	gchar *program;
	gchar *command_line;
	GAppInfo *app_info;
	GError *error;

	settings = g_settings_new (schema_program);
	program = g_settings_get_string (settings, "exec");
	g_object_unref (settings);

	if (program == NULL) {
		g_warning ("Cannot launch calendar/tasks application: key not set");
		return;
	}

	if (argument)
		command_line = g_strdup_printf ("%s %s", program, argument);
	else
		command_line = g_strdup (program);
	g_free (program);

	error = NULL;
	app_info = g_app_info_create_from_commandline (command_line,
	                                               NULL,
	                                               G_APP_INFO_CREATE_NONE,
	                                               &error);
	g_free (command_line);

	if (error) {
		g_warning ("Cannot launch calendar/tasks application: %s", error->message);
		g_error_free (error);
		return;
	}

	if (!g_app_info_launch (app_info, NULL, NULL, &error)) {
		g_warning ("Cannot launch calendar/tasks application: %s", error->message);
		g_error_free (error);
	}
}

static void
clock_launch_calendar_app (CalendarWindow *calwin,
			   const char     *argument)
{
	clock_launch_calendar_tasks_app (calwin, SCHEMA_CALENDAR_APP, argument);
}

static void
clock_launch_tasks_app (CalendarWindow *calwin,
			const char     *argument)
{
	clock_launch_calendar_tasks_app (calwin, SCHEMA_TASKS_APP, argument);
}

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
        APPOINTMENT_COLUMN_TYPE,
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
	APPOINTMENT_TYPE_APPOINTMENT,
	APPOINTMENT_TYPE_BIRTHDAY,
	APPOINTMENT_TYPE_WEATHER
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
        TASK_COLUMN_PRIORITY,
        N_TASK_COLUMNS
};

static char *
format_time (GDesktopClockFormat format,
             time_t              t,
             gint                year,
             gint                month,
             gint                day)
{
        GDateTime *dt;
        gchar *time;

        if (!t)
                return NULL;

        dt = g_date_time_new_from_unix_local (t);
        time = NULL;

        if (!dt)
                return NULL;

        if (year == (g_date_time_get_year (dt) + 1900) &&
            month == g_date_time_get_month (dt) &&
            day == g_date_time_get_day_of_month (dt)) {
                if (format == G_DESKTOP_CLOCK_FORMAT_12H) {
                        /* Translators: This is a strftime format string.
                         * It is used to display the time in 12-hours format
                         * (eg, like in the US: 8:10 am). The %p expands to
                         * am/pm.
                         */
                        time = g_date_time_format (dt, _("%l:%M %p"));
                } else {
                        /* Translators: This is a strftime format string.
                         * It is used to display the time in 24-hours format
                         * (eg, like in France: 20:10).
                         */
                        time = g_date_time_format (dt, _("%H:%M"));
                }
        } else {
                /* Translators: This is a strftime format string.
                 * It is used to display the start date of an appointment, in
                 * the most abbreviated way possible.
                 */
                time = g_date_time_format (dt, _("%b %d"));
        }

        return time;
}

static void
handle_tasks_changed (CalendarWindow *calwin)
{
        GSList *events, *l;

        gtk_list_store_clear (calwin->priv->tasks_model);

        events = calendar_client_get_events (calwin->priv->client,
					     CALENDAR_EVENT_TASK);
        for (l = events; l; l = l->next) {
                CalendarTask *task = l->data;
                GtkTreeIter   iter;
                char         *percent_complete_text;

                g_assert (CALENDAR_EVENT (task)->type == CALENDAR_EVENT_TASK);

                /* FIXME: should this format be locale specific ? */
                percent_complete_text = g_strdup_printf ("%d%%", task->percent_complete);

                gtk_list_store_append (calwin->priv->tasks_model, &iter);
                gtk_list_store_set (calwin->priv->tasks_model, &iter,
                                    TASK_COLUMN_UID,                   task->uid,
                                    TASK_COLUMN_SUMMARY,               task->summary,
                                    TASK_COLUMN_DESCRIPTION,           task->description,
                                    TASK_COLUMN_START_TIME,            (gint64)task->start_time,
                                    TASK_COLUMN_DUE_TIME,              (gint64)task->due_time,
                                    TASK_COLUMN_PERCENT_COMPLETE,      task->percent_complete,
                                    TASK_COLUMN_PERCENT_COMPLETE_TEXT, percent_complete_text,
                                    TASK_COLUMN_COMPLETED,             task->percent_complete == 100,
                                    TASK_COLUMN_COMPLETED_TIME,        (gint64)task->completed_time,
                                    TASK_COLUMN_COLOR,                 task->color_string,
                                    TASK_COLUMN_PRIORITY,              task->priority,
                                    -1);

                g_free (percent_complete_text);
                calendar_event_free (CALENDAR_EVENT (task));
        }
        g_slist_free (events);

        update_frame_visibility (calwin->priv->task_list,
				 GTK_TREE_MODEL (calwin->priv->tasks_filter));
}

static void
handle_task_completed_toggled (CalendarWindow        *calwin,
                               const char            *path_str,
                               GtkCellRendererToggle *cell)
{
        GtkTreePath *child_path, *path;
        GtkTreeIter  iter;
        char        *uid;
        gboolean     task_completed;
        guint        percent_complete;

        path       = gtk_tree_path_new_from_string (path_str);
        child_path = gtk_tree_model_filter_convert_path_to_child_path (calwin->priv->tasks_filter, path);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (calwin->priv->tasks_model),
				 &iter, child_path);
        gtk_tree_model_get (GTK_TREE_MODEL (calwin->priv->tasks_model),
			    &iter,
                            TASK_COLUMN_UID,                  &uid,
                            TASK_COLUMN_COMPLETED,            &task_completed,
                            TASK_COLUMN_PERCENT_COMPLETE,     &percent_complete,
                            -1);

        task_completed   = !task_completed;
        percent_complete = task_completed ? 100 : 0;

        calendar_client_set_task_completed (calwin->priv->client,
                                            uid,
                                            task_completed,
                                            percent_complete);

        g_free (uid);
        gtk_tree_path_free (path);
        gtk_tree_path_free (child_path);
}

static void
handle_task_percent_complete_edited (CalendarWindow      *calwin,
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
        child_path = gtk_tree_model_filter_convert_path_to_child_path (calwin->priv->tasks_filter, path);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (calwin->priv->tasks_model),
				 &iter, child_path);
        gtk_tree_model_get (GTK_TREE_MODEL (calwin->priv->tasks_model),
			    &iter, TASK_COLUMN_UID, &uid,
                            -1);

        text_copy = g_strdup (text);
        text_copy = g_strdelimit (text_copy, "%", ' ');
        text_copy = g_strstrip (text_copy);
        percent_complete = (int) g_strtod (text_copy, &error);
        if (!error || !error [0]) {
                gboolean task_completed;

                percent_complete = CLAMP (percent_complete, 0, 100);
                task_completed = (percent_complete == 100);

                calendar_client_set_task_completed (calwin->priv->client,
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
is_for_filter (GtkTreeModel *model,
	      GtkTreeIter  *iter,
	      gpointer      data)
{
	gint type;

	gtk_tree_model_get (model, iter, APPOINTMENT_COLUMN_TYPE, &type, -1);
	return type == GPOINTER_TO_INT (data);
}

static gboolean
filter_out_tasks (GtkTreeModel   *model,
                  GtkTreeIter    *iter,
                  CalendarWindow *calwin)
{
        gint64   start_time64;
        gint64   completed_time64;
	GDateTime *start_time;
	GDateTime *completed_time;
	GDateTime *one_day_ago;
	GDateTime *current_time;
        gboolean visible;

        gtk_tree_model_get (model, iter,
                            TASK_COLUMN_START_TIME,     &start_time64,
                            TASK_COLUMN_COMPLETED_TIME, &completed_time64,
                            -1);

	current_time = g_date_time_new_now_local ();
        start_time = g_date_time_new_from_unix_local (start_time64);
        completed_time = g_date_time_new_from_unix_local (completed_time64);
        one_day_ago = g_date_time_add_days (completed_time, -1);

        visible = g_date_time_compare (start_time, current_time) <= 0;
        if (visible)
		visible = g_date_time_compare (completed_time, one_day_ago) >= 0;

	g_date_time_unref (start_time);
	g_date_time_unref (completed_time);
	g_date_time_unref (one_day_ago);
	g_date_time_unref (current_time);

        return visible;
}

static void
modify_task_text_attributes (GtkTreeModel   *model,
                             GtkTreeIter    *iter,
                             GValue         *value,
                             gint            column,
                             CalendarWindow *calwin)
{
        gint64          due_time64;
        time_t          due_time;
	time_t          current_time;
        PangoAttrList  *attr_list;
        PangoAttribute *attr;
        GtkTreeIter     child_iter;

        gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                          &child_iter,
                                                          iter);

        if (column != TASK_COLUMN_OVERDUE_ATTR) {
                memset (value, 0, sizeof (GValue));
                gtk_tree_model_get_value (GTK_TREE_MODEL (calwin->priv->tasks_model),
                                          &child_iter, column, value);

                return;
        }

        gtk_tree_model_get (GTK_TREE_MODEL (calwin->priv->tasks_model),
			    &child_iter, TASK_COLUMN_DUE_TIME, &due_time64,
                            -1);
        due_time = due_time64;

	current_time = time(NULL);
        if (due_time && due_time > current_time)
                return;

        attr_list = pango_attr_list_new ();

        attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
        attr->start_index = 0;
        attr->end_index = G_MAXINT;
        pango_attr_list_insert (attr_list, attr);

        g_value_take_boxed (value, attr_list);
}

static gboolean
task_activated_cb (GtkTreeView       *view,
                   GtkTreePath       *path,
                   GtkTreeViewColumn *column,
                   CalendarWindow    *calwin)
{
        GtkTreeIter  iter;
        GtkTreePath *child_path;
        char        *uid;
	char        *argument;

        child_path = gtk_tree_model_filter_convert_path_to_child_path (calwin->priv->tasks_filter,
								       path);

        gtk_tree_model_get_iter (GTK_TREE_MODEL (calwin->priv->tasks_model),
				 &iter, child_path);
        gtk_tree_model_get (GTK_TREE_MODEL (calwin->priv->tasks_model),
			    &iter, TASK_COLUMN_UID, &uid, -1);

	argument = g_strdup_printf ("task:%s", uid);

	clock_launch_tasks_app (calwin, argument);

	g_free (argument);
        g_free (uid);
        gtk_tree_path_free (child_path);

        return TRUE;
}

static void
set_renderer_pixbuf_color_by_column (GtkCellRenderer *renderer,
                                     GtkTreeModel    *model,
                                     GtkTreeIter     *iter,
                                     gint             column_number)
{
        gchar *color_string;
        GdkRGBA color;

        gtk_tree_model_get (model, iter, column_number, &color_string, -1);

        if (color_string && gdk_rgba_parse (&color, color_string)) {
                cairo_surface_t *surface;
                cairo_t *cr;

                surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 16, 16);
                cr = cairo_create (surface);

                gdk_cairo_set_source_rgba (cr, &color);
                cairo_paint (cr);

                g_object_set (renderer, "visible", surface != NULL, "surface", surface, NULL);

                cairo_destroy (cr);
                cairo_surface_destroy (surface);
                g_free (color_string);
        }
}

static void
set_renderer_pixbuf_pixmap (GtkCellRenderer *renderer,
			    GtkTreeModel    *model,
			    GtkTreeIter     *iter,
			    const char      *iconpath)
{
	GdkPixbuf   *pixbuf = NULL;
	GError      *error  = NULL;

	if (!g_file_test (iconpath, G_FILE_TEST_IS_REGULAR)) {
		g_printerr ("File '%s' does not exist.\n", iconpath);
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file (iconpath, &error);
	if (error) {
		g_printerr ("Cannot load '%s': %s\n",
			    iconpath, error->message);
		g_error_free (error);
		return;
	}

	g_object_set (renderer,
		      "visible", pixbuf != NULL,
		      "pixbuf", pixbuf,
		      NULL);

	if (pixbuf)
		g_object_unref (pixbuf);
}

static void
set_renderer_pixbuf_pixmap_for_bday (GtkCellRenderer *renderer,
				     GtkTreeModel    *model,
				     GtkTreeIter     *iter,
				     gint             data_column)
{
	const gchar *path   = NULL;
	gchar       *type   = NULL;

	gtk_tree_model_get (model, iter, data_column, &type, -1);
	if (!type)
		return;

	/* type should be in format like this:
	 * pas-id-4121A93E00000001-anniversary
	 * pas-id-41112AF900000003-birthday
	 * ...
	 */
	if (g_strrstr (type, "birthday") != NULL)
		path = CLOCK_EDS_ICONDIR G_DIR_SEPARATOR_S "category_birthday_16.png";
	else if (g_strrstr (type, "anniversary") != NULL)
		path = CLOCK_EDS_ICONDIR G_DIR_SEPARATOR_S "category_gifts_16.png";
	else
		path = CLOCK_EDS_ICONDIR G_DIR_SEPARATOR_S "category_miscellaneous_16.png";

	g_free (type);

	set_renderer_pixbuf_pixmap (renderer, model, iter, path);
}

static void
set_renderer_pixbuf_pixmap_for_weather (GtkCellRenderer *renderer,
					GtkTreeModel    *model,
					GtkTreeIter     *iter)
{
	set_renderer_pixbuf_pixmap (renderer, model, iter,
				    CLOCK_EDS_ICONDIR G_DIR_SEPARATOR_S "category_holiday_16.png");
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
static void
birthday_pixbuf_cell_data_func (GtkTreeViewColumn   *column,
				GtkCellRenderer     *renderer,
				GtkTreeModel        *model,
				GtkTreeIter         *iter,
				gpointer             data)
{

	/* APPOINTMENT_COLUMN_UID contains data to select between
	 * anniversary or birthday
	 */
	set_renderer_pixbuf_pixmap_for_bday (renderer,
					     model,
					     iter,
					     APPOINTMENT_COLUMN_UID);
}
static void
weather_pixbuf_cell_data_func (GtkTreeViewColumn   *column,
			       GtkCellRenderer     *renderer,
			       GtkTreeModel        *model,
			       GtkTreeIter         *iter,
			       gpointer             data)
{

	set_renderer_pixbuf_pixmap_for_weather (renderer,
						model,
						iter);
}

static int
compare_tasks  (GtkTreeModel *model,
                GtkTreeIter  *a,
                GtkTreeIter  *b,
                gpointer      user_data)
{
        gboolean done_a, done_b;
        int priority_a, priority_b;

        gtk_tree_model_get (model, a,
                            TASK_COLUMN_COMPLETED, &done_a,
                            TASK_COLUMN_PRIORITY, &priority_a,
                            -1);
        gtk_tree_model_get (model, b,
                            TASK_COLUMN_COMPLETED, &done_b,
                            TASK_COLUMN_PRIORITY, &priority_b,
                            -1);

        /* Always sort completed tasks last */
        if (done_a != done_b)
                return done_a ? -1 : 1;

        /* We change undefined priorities so they appear as "Normal" */
        if (priority_a <= 0)
                priority_a = 5;
        if (priority_b <= 0)
                priority_b = 5;

	/* We'll just use the ordering of the priority values. */
	if (priority_a < priority_b)
		return -1;
	else if (priority_a > priority_b)
		return 1;
	else {
                gint64 due_time_a64, due_time_b64;
                time_t due_time_a, due_time_b;

                gtk_tree_model_get (model, a,
				    TASK_COLUMN_DUE_TIME, &due_time_a64, -1);
                gtk_tree_model_get (model, b,
				    TASK_COLUMN_DUE_TIME, &due_time_b64, -1);
                due_time_a = due_time_a64;
                due_time_b = due_time_b64;

                if (due_time_a < due_time_b)
                        return -1;
                else if (due_time_a > due_time_b)
                        return 1;
                else {
                        char *summary_a, *summary_b;
                        int res;

                        gtk_tree_model_get (model, a, TASK_COLUMN_SUMMARY, &summary_a, -1);
                        gtk_tree_model_get (model, b, TASK_COLUMN_SUMMARY, &summary_b, -1);

                        res = g_utf8_collate (summary_a ? summary_a: "",
                                              summary_b ? summary_b: "");

                        g_free (summary_a);
                        g_free (summary_b);

                        return res;
                }
        }
}

static void
calendar_window_tree_selection_changed (GtkTreeSelection *selection,
					CalendarWindow   *calwin)
{
	if (selection == calwin->priv->previous_selection)
		return;

	if (calwin->priv->previous_selection) {
		g_signal_handlers_block_by_func (calwin->priv->previous_selection,
						 calendar_window_tree_selection_changed,
						 calwin);
		gtk_tree_selection_unselect_all (calwin->priv->previous_selection);
		g_signal_handlers_unblock_by_func (calwin->priv->previous_selection,
						   calendar_window_tree_selection_changed,
						   calwin);
	}

	calwin->priv->previous_selection = selection;
}

static void
edit_tasks (CalendarWindow *calwin)
{
	clock_launch_tasks_app (calwin, NULL);
}

static GtkWidget *
create_task_list (CalendarWindow *calwin,
                  GtkWidget     **tree_view,
                  GtkWidget     **scrolled_window)
{
        GtkWidget         *list;
        GtkWidget         *view;
        GtkWidget         *scrolled;
        GtkCellRenderer   *cell;
        GtkTreeViewColumn *column;
        GtkTreeSelection  *selection;

	list = create_hig_frame (calwin, 
                                 _("Tasks"), _("Edit"),
                                 KEY_TASKS_EXPANDED,
                                 G_CALLBACK (edit_tasks),
				 FALSE);
 
        *scrolled_window = scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                             GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
	/* we show the widget before adding to the container, since adding to
	 * the container changes the visibility depending on the state of the
	 * expander */
        gtk_widget_show (scrolled);
        gtk_container_add (GTK_CONTAINER (list), scrolled);

        g_assert (calwin->priv->tasks_model != NULL);

        *tree_view = view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (calwin->priv->tasks_filter));
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

        g_signal_connect (view, "row-activated",
                          G_CALLBACK (task_activated_cb), calwin);

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
                                  G_CALLBACK (handle_task_completed_toggled),
				  calwin);
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
                                  G_CALLBACK (handle_task_percent_complete_edited),
				  calwin);
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

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (calendar_window_tree_selection_changed),
			  calwin);

        gtk_container_add (GTK_CONTAINER (scrolled), view);

        gtk_widget_show (view);

        return list;
}

static void
mark_day_on_calendar (CalendarClient *client,
		      guint           day,
		      CalendarWindow *calwin)
{
        gtk_calendar_mark_day (GTK_CALENDAR (calwin->priv->calendar), day);
}

static void
handle_appointments_changed (CalendarWindow *calwin)
{
        GSList *events, *l;
        guint   year, month, day;

        if (calwin->priv->calendar) {
                gtk_calendar_clear_marks (GTK_CALENDAR (calwin->priv->calendar));

                calendar_client_foreach_appointment_day (calwin->priv->client,
                                                         (CalendarDayIter) mark_day_on_calendar,
                                                         calwin);
        }

        gtk_list_store_clear (calwin->priv->appointments_model);

        calendar_client_get_date (calwin->priv->client, &year, &month, &day);

        events = calendar_client_get_events (calwin->priv->client,
					     CALENDAR_EVENT_APPOINTMENT);
        for (l = events; l; l = l->next) {
                CalendarAppointment *appointment = l->data;
                GtkTreeIter          iter;
                char                *start_text;
                gint                 type;

                g_assert (CALENDAR_EVENT (appointment)->type == CALENDAR_EVENT_APPOINTMENT);

                if (appointment->is_all_day)
                        start_text = g_strdup (_("All Day"));
                else
                        start_text = format_time (calwin->priv->time_format,
                                                  appointment->start_time,
                                                  year, month, day);

                if (g_ascii_strcasecmp (appointment->backend_name, "weather") == 0)
                        type = APPOINTMENT_TYPE_WEATHER;
                else if (g_ascii_strcasecmp (appointment->backend_name, "contacts") == 0)
                        type = APPOINTMENT_TYPE_BIRTHDAY;
                else
                        type = APPOINTMENT_TYPE_APPOINTMENT;

                gtk_list_store_append (calwin->priv->appointments_model,
				       &iter);
                gtk_list_store_set (calwin->priv->appointments_model, &iter,
                                    APPOINTMENT_COLUMN_UID,         appointment->uid,
                                    APPOINTMENT_COLUMN_TYPE,        type,
                                    APPOINTMENT_COLUMN_SUMMARY,     appointment->summary,
                                    APPOINTMENT_COLUMN_DESCRIPTION, appointment->description,
                                    APPOINTMENT_COLUMN_START_TIME,  (gint64)appointment->start_time,
                                    APPOINTMENT_COLUMN_START_TEXT,  start_text,
                                    APPOINTMENT_COLUMN_END_TIME,    (gint64)appointment->end_time,
                                    APPOINTMENT_COLUMN_ALL_DAY,     appointment->is_all_day,
                                    APPOINTMENT_COLUMN_COLOR,       appointment->color_string,
                                    -1);

                g_free (start_text);
                calendar_event_free (CALENDAR_EVENT (appointment));
        }
        g_slist_free (events);

        update_frame_visibility (calwin->priv->appointment_list,
				 GTK_TREE_MODEL (calwin->priv->appointments_filter));
        update_frame_visibility (calwin->priv->birthday_list,
				 GTK_TREE_MODEL (calwin->priv->birthdays_filter));
        update_frame_visibility (calwin->priv->weather_list,
				 GTK_TREE_MODEL (calwin->priv->weather_filter));
}

static GtkWidget *
create_list_for_appointment_model (CalendarWindow      *calwin,
				   const char          *label,
				   GtkTreeModelFilter **filter,
				   gint                 filter_type,
				   GtkTreeCellDataFunc  set_pixbuf_cell,
				   gboolean             show_start,
				   GtkWidget          **tree_view,
				   GtkWidget          **scrolled_window,
				   const char          *key,
                                   GCallback            callback)
{
        GtkWidget         *list;
        GtkWidget         *view;
        GtkWidget         *scrolled;
        GtkCellRenderer   *cell;
        GtkTreeViewColumn *column;
	GtkTreeSelection  *selection;

	
	list = create_hig_frame (calwin, label, _("Edit"), key, callback, FALSE);

        *scrolled_window = scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                             GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
	/* we show the widget before adding to the container, since adding to
	 * the container changes the visibility depending on the state of the
	 * expander */
        gtk_widget_show (scrolled);
        gtk_container_add (GTK_CONTAINER (list), scrolled);

	g_assert (calwin->priv->appointments_model != NULL);

        if (!*filter) {
		*filter =
			GTK_TREE_MODEL_FILTER (
				gtk_tree_model_filter_new (GTK_TREE_MODEL (calwin->priv->appointments_model),
							   NULL));
		gtk_tree_model_filter_set_visible_func (
				*filter,
				(GtkTreeModelFilterVisibleFunc) is_for_filter,
				GINT_TO_POINTER (filter_type),
				NULL);
        }

        *tree_view = view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (*filter));
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

        /* Icon */
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_pixbuf_new ();
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_set_cell_data_func (column, cell,
                                                 (GtkTreeCellDataFunc) set_pixbuf_cell,
                                                 NULL, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	if (show_start) {
		/* Start time */
		column = gtk_tree_view_column_new ();
		cell = gtk_cell_renderer_text_new ();
		gtk_tree_view_column_pack_start (column, cell, TRUE);
		gtk_tree_view_column_add_attribute (column, cell,
						    "text", APPOINTMENT_COLUMN_START_TEXT);
		gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
	}

        /* Summary */
        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_add_attribute (column, cell,
                                            "text", APPOINTMENT_COLUMN_SUMMARY);
        gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (calendar_window_tree_selection_changed),
			  calwin);

        gtk_container_add (GTK_CONTAINER (scrolled), view);

        gtk_widget_show (view);

        return list;
}

static void
edit_appointments (CalendarWindow *calwin)
{
	clock_launch_calendar_app (calwin, NULL);
}

static GtkWidget *
create_appointment_list (CalendarWindow  *calwin,
			 GtkWidget      **tree_view,
			 GtkWidget      **scrolled_window)
{
	return create_list_for_appointment_model (
					calwin,
					_("Appointments"),
					&calwin->priv->appointments_filter,
					APPOINTMENT_TYPE_APPOINTMENT,
					appointment_pixbuf_cell_data_func,
					TRUE,
					tree_view,
					scrolled_window,
				        KEY_APPOINTMENTS_EXPANDED,
					G_CALLBACK (edit_appointments));
}

static void
edit_birthdays (CalendarWindow *calwin)
{
	clock_launch_calendar_app (calwin, NULL);
}

static GtkWidget *
create_birthday_list (CalendarWindow  *calwin,
		      GtkWidget      **tree_view,
		      GtkWidget      **scrolled_window)
{
        /* FIXME: Figure out how to get rid of useless localized message in front of the summary */
	return create_list_for_appointment_model (
					calwin,
					_("Birthdays and Anniversaries"),
					&calwin->priv->birthdays_filter,
					APPOINTMENT_TYPE_BIRTHDAY,
					birthday_pixbuf_cell_data_func,
					FALSE,
					tree_view,
					scrolled_window,
					KEY_BIRTHDAYS_EXPANDED,
					G_CALLBACK (edit_birthdays));
}

static void
edit_weather (CalendarWindow *calwin)
{
	clock_launch_calendar_app (calwin, NULL);
}


static GtkWidget *
create_weather_list (CalendarWindow  *calwin,
		     GtkWidget      **tree_view,
		     GtkWidget      **scrolled_window)
{
	return create_list_for_appointment_model (
					calwin,
					_("Weather Information"),
					&calwin->priv->weather_filter,
					APPOINTMENT_TYPE_WEATHER,
					weather_pixbuf_cell_data_func,
					FALSE,
					tree_view,
					scrolled_window,
					KEY_WEATHER_EXPANDED,
					G_CALLBACK (edit_weather));
}

static void
calendar_window_create_tasks_model (CalendarWindow *calwin)
{
	GType column_types [N_TASK_COLUMNS] = {
		G_TYPE_STRING,         /* uid                     */
		G_TYPE_STRING,         /* summary                 */
		G_TYPE_STRING,         /* description             */
		G_TYPE_INT64,          /* start time              */
		G_TYPE_INT64,          /* due time                */
		G_TYPE_UINT,           /* percent complete        */
		G_TYPE_STRING,         /* percent complete text   */
		G_TYPE_BOOLEAN,        /* completed               */
		G_TYPE_INT64,          /* completed time          */
		PANGO_TYPE_ATTR_LIST,  /* summary text attributes */
		G_TYPE_STRING,         /* color                   */
		G_TYPE_INT             /* priority                */
	};

	calwin->priv->tasks_model = gtk_list_store_newv (N_TASK_COLUMNS,
							 column_types);

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (calwin->priv->tasks_model),
					 TASK_COLUMN_PRIORITY,
					 compare_tasks,
					 NULL, NULL);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (calwin->priv->tasks_model),
					      TASK_COLUMN_PRIORITY,
					      GTK_SORT_ASCENDING);

	calwin->priv->tasks_filter = GTK_TREE_MODEL_FILTER (
		gtk_tree_model_filter_new (GTK_TREE_MODEL (calwin->priv->tasks_model),
					   NULL));
	gtk_tree_model_filter_set_visible_func (
		 calwin->priv->tasks_filter,
		 (GtkTreeModelFilterVisibleFunc) filter_out_tasks,
		 calwin,
		 NULL);
	gtk_tree_model_filter_set_modify_func (
		 calwin->priv->tasks_filter,
		 N_TASK_COLUMNS,
		 column_types,
		 (GtkTreeModelFilterModifyFunc) modify_task_text_attributes,
		 calwin,
		 NULL);
}

static void
calendar_window_create_appointments_model (CalendarWindow *calwin)
{
	calwin->priv->appointments_model =
		gtk_list_store_new (N_APPOINTMENT_COLUMNS,
				    G_TYPE_STRING,   /* uid              */
				    G_TYPE_INT,      /* type             */
				    G_TYPE_STRING,   /* summary          */
				    G_TYPE_STRING,   /* description      */
				    G_TYPE_INT64,    /* start time       */
				    G_TYPE_STRING,   /* start time text  */
				    G_TYPE_INT64,    /* end time         */
				    G_TYPE_BOOLEAN,  /* all day          */
				    G_TYPE_STRING);  /* color            */

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (calwin->priv->appointments_model),
					      APPOINTMENT_COLUMN_START_TIME,
					      GTK_SORT_ASCENDING);
}

static void
calendar_day_activated (GtkCalendar    *calendar,
			CalendarWindow *calwin)
{
	unsigned int  day;
	unsigned int  month;
	unsigned int  year;
	char         *argument;

	gtk_calendar_get_date (calendar, &year, &month, &day);

	argument = g_strdup_printf ("calendar:///?startdate="
				    "%.4d%.2d%.2d",
				    year, month + 1, day);

	clock_launch_calendar_app (calwin, argument);

	g_free (argument);
}

static void
calendar_day_selected (GtkCalendar    *calendar,
		       CalendarWindow *calwin)
{
        guint day;

        gtk_calendar_get_date (calendar, NULL, NULL, &day);

        calendar_client_select_day (calwin->priv->client, day);

        handle_appointments_changed (calwin);
        handle_tasks_changed (calwin);
}

static void
calendar_month_selected (GtkCalendar    *calendar,
			 CalendarWindow *calwin)
{
        guint year, month;

        gtk_calendar_get_date (calendar, &year, &month, NULL);

        calendar_client_select_month (calwin->priv->client, month, year);

        handle_appointments_changed (calwin);
        handle_tasks_changed (calwin);
}

typedef struct
{
        GtkWidget *calendar;
        GtkWidget *tree;
} ConstraintData;

static void
get_monitor_geometry (GtkWidget    *widget,
                      GdkRectangle *geometry)
{
	GdkDisplay *display;
	GdkWindow *window;
	GdkMonitor *monitor;

	display = gdk_display_get_default ();
	window = gtk_widget_get_window (widget);
	monitor = gdk_display_get_monitor_at_window (display, window);

	gdk_monitor_get_geometry (monitor, geometry);
}

static void
constrain_list_size (GtkWidget      *widget,
                     GtkAllocation  *allocation,
                     ConstraintData *constraint)
{
        GtkRequisition   req;
	GtkStyleContext *context;
	GtkStateFlags    state;
	GtkBorder        padding;
        GdkRectangle     monitor;
        int              width;
        int              height;
        int              max_height;

        /* constrain width to the calendar width */
        gtk_widget_get_preferred_size (constraint->calendar, &req, NULL);
        width = MIN (allocation->width, req.width);

        get_monitor_geometry (widget, &monitor);

        /* constrain height to be the tree height up to a max */
        max_height = (monitor.height - req.height) / 3;
        gtk_widget_get_preferred_size (constraint->tree, &req, NULL);

	state = gtk_widget_get_state_flags (widget);
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_padding (context, state, &padding);

        height = MIN (req.height, max_height);
        height += padding.top + padding.bottom;
        /* top & bottom border */
        height += 2;

        gtk_widget_set_size_request (widget, width, height);
}

static void
constraint_data_free (ConstraintData *constraint,
                      GClosure       *closure)
{
  g_free (constraint);
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

        g_signal_connect_data (widget, "size-allocate",
                               G_CALLBACK (constrain_list_size), constraint,
                               (GClosureNotify) constraint_data_free, 0);
}

#endif /* HAVE_EDS */

static void
calendar_window_pack_pim (CalendarWindow *calwin,
			  GtkWidget      *vbox)
{
#ifdef HAVE_EDS
	GtkWidget *list;
        GtkWidget *tree_view;
        GtkWidget *scrolled_window;
        guint      year, month, day;

	calendar_window_create_tasks_model (calwin);
	calendar_window_create_appointments_model (calwin);

        list = create_task_list (calwin, &tree_view, &scrolled_window);
        setup_list_size_constraint (scrolled_window,
				    calwin->priv->calendar, tree_view);
        update_frame_visibility (list,
				 GTK_TREE_MODEL (calwin->priv->tasks_model));
        calwin->priv->task_list = list;

	list = create_birthday_list (calwin, &tree_view, &scrolled_window);
        setup_list_size_constraint (scrolled_window,
				    calwin->priv->calendar, tree_view);
        update_frame_visibility (list,
				 GTK_TREE_MODEL (calwin->priv->birthdays_filter));
        calwin->priv->birthday_list = list;

        list = create_weather_list (calwin, &tree_view, &scrolled_window);
        setup_list_size_constraint (scrolled_window,
				    calwin->priv->calendar, tree_view);
        update_frame_visibility (list,
				 GTK_TREE_MODEL (calwin->priv->weather_filter));
	calwin->priv->weather_list = list;

        list = create_appointment_list (calwin, &tree_view, &scrolled_window);
        setup_list_size_constraint (scrolled_window,
				    calwin->priv->calendar, tree_view);
        update_frame_visibility (list,
				 GTK_TREE_MODEL (calwin->priv->appointments_filter));
	calwin->priv->appointment_list = list;

	if (!calwin->priv->invert_order) {
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->task_list,
				    TRUE, TRUE, 0);
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->appointment_list,
				    TRUE, TRUE, 0);
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->birthday_list,
				    TRUE, TRUE, 0);
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->weather_list,
				    TRUE, TRUE, 0);
	} else {
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->weather_list,
				    TRUE, TRUE, 0);
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->birthday_list,
				    TRUE, TRUE, 0);
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->appointment_list,
				    TRUE, TRUE, 0);
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->task_list,
				    TRUE, TRUE, 0);
        }

        if (!calwin->priv->client) {
                calwin->priv->client = calendar_client_new ();

                g_signal_connect_swapped (calwin->priv->client,
					  "tasks-changed",
                                          G_CALLBACK (handle_tasks_changed),
					  calwin);
                g_signal_connect_swapped (calwin->priv->client,
					  "appointments-changed",
                                          G_CALLBACK (handle_appointments_changed),
					  calwin);
        }

        gtk_calendar_get_date (GTK_CALENDAR (calwin->priv->calendar),
			       &year, &month, &day);

        calendar_client_select_day   (calwin->priv->client, day);
        calendar_client_select_month (calwin->priv->client, month, year);

        handle_tasks_changed (calwin);
        handle_appointments_changed (calwin);

        g_signal_connect (calwin->priv->calendar,
			  "day-selected-double-click",
			  G_CALLBACK (calendar_day_activated),
			  calwin);
        g_signal_connect (calwin->priv->calendar,
			  "day-selected",
			  G_CALLBACK (calendar_day_selected),
			  calwin);
        g_signal_connect (calwin->priv->calendar,
			  "month-changed",
			  G_CALLBACK (calendar_month_selected),
			  calwin);
#endif /* HAVE_EDS */
}

static GtkWidget *
calendar_window_create_calendar (CalendarWindow *calwin)
{
	GtkWidget                 *calendar;
	GtkCalendarDisplayOptions  options;
	GDateTime                 *now;

	calendar = gtk_calendar_new ();
	options = gtk_calendar_get_display_options (GTK_CALENDAR (calendar));
	if (calwin->priv->show_weeks)
		options |= GTK_CALENDAR_SHOW_WEEK_NUMBERS;
	else
		options &= ~(GTK_CALENDAR_SHOW_WEEK_NUMBERS);
	gtk_calendar_set_display_options (GTK_CALENDAR (calendar), options);

	now = g_date_time_new_now_local ();

        gtk_calendar_select_month (GTK_CALENDAR (calendar),
                                   g_date_time_get_month (now) - 1,
                                   g_date_time_get_year (now));
        gtk_calendar_select_day (GTK_CALENDAR (calendar),
				 g_date_time_get_day_of_month (now));

	g_date_time_unref (now);

	return calendar;
}

static void
expand_collapse_child (GtkWidget *child,
		       gpointer   data)
{
	gboolean expanded;

	if (data == child || gtk_widget_is_ancestor (data, child))
		return;

	expanded = gtk_expander_get_expanded (GTK_EXPANDER (data));
	g_object_set (child, "visible", expanded, NULL);
}

static void
expand_collapse (GtkWidget  *expander,
		 GParamSpec *pspec,
                 gpointer    data)
{
	GtkWidget *box = data;

	gtk_container_foreach (GTK_CONTAINER (box),
			       (GtkCallback)expand_collapse_child,
			       expander);
}

static void add_child (GtkContainer *container,
                       GtkWidget    *child,
                       GtkExpander  *expander)
{
	expand_collapse_child (child, expander);
}

static GtkWidget *
create_hig_frame (CalendarWindow *calwin,
		  const char *title,
                  const char *button_label,
		  const char *key,
		  GCallback   callback,
		  gboolean    bind_to_locked_down)
{
        GtkWidget *vbox;
        GtkWidget *label;
        GtkWidget *hbox;
        char      *bold_title;
        GtkWidget *expander;

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

        bold_title = g_strdup_printf ("<b>%s</b>", title);
	expander = gtk_expander_new (bold_title);
        g_free (bold_title);
	gtk_expander_set_use_markup (GTK_EXPANDER (expander), TRUE);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), expander, FALSE, FALSE, 0);
	gtk_widget_show_all (vbox);

	g_signal_connect (expander, "notify::expanded",
			  G_CALLBACK (expand_collapse), hbox);
	g_signal_connect (expander, "notify::expanded",
			  G_CALLBACK (expand_collapse), vbox);

	/* FIXME: this doesn't really work, since "add" does not 
	 * get emitted for e.g. gtk_box_pack_start
	 */
	g_signal_connect (vbox, "add", G_CALLBACK (add_child), expander);
	g_signal_connect (hbox, "add", G_CALLBACK (add_child), expander);

        if (button_label) {
                GtkWidget *button_box;
                GtkWidget *button;
                gchar *text;

                button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
                gtk_widget_show (button_box);

                button = gtk_button_new ();
                gtk_container_add (GTK_CONTAINER (button_box), button);

                text = g_markup_printf_escaped ("<small>%s</small>", button_label);
                label = gtk_label_new (text);
                g_free (text);
                gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
                gtk_container_add (GTK_CONTAINER (button), label);

                gtk_widget_show_all (button);

                gtk_box_pack_end (GTK_BOX (hbox), button_box, FALSE, FALSE, 0);

                g_signal_connect_swapped (button, "clicked", callback, calwin);

                g_object_bind_property (expander, "expanded",
                                        button_box, "visible",
                                        G_BINDING_DEFAULT|G_BINDING_SYNC_CREATE);

                if (bind_to_locked_down) {
                        g_object_bind_property (calwin, "locked-down",
                                                button, "visible",
                                                G_BINDING_DEFAULT |
                                                G_BINDING_INVERT_BOOLEAN |
                                                G_BINDING_SYNC_CREATE);
                }
        }

        g_settings_bind (calwin->priv->settings, key,
                         expander, "expanded",
                         G_SETTINGS_BIND_DEFAULT);

        return vbox;
}

static void
edit_locations (CalendarWindow *calwin)
{
	g_signal_emit (calwin, signals[EDIT_LOCATIONS], 0);
}

static void
calendar_window_pack_locations (CalendarWindow *calwin, GtkWidget *vbox)
{
	calwin->priv->locations_list = create_hig_frame (calwin,
							 _("Locations"), _("Edit"),
							 KEY_LOCATIONS_EXPANDED,
							 G_CALLBACK (edit_locations),
							 TRUE);

	/* we show the widget before adding to the container, since adding to
	 * the container changes the visibility depending on the state of the
	 * expander */
	gtk_widget_show (calwin->priv->locations_list);
	gtk_container_add (GTK_CONTAINER (vbox), calwin->priv->locations_list);

	//gtk_box_pack_start (GTK_BOX (vbox), calwin->priv->locations_list, TRUE, FALSE, 0);
}

static void
calendar_window_fill (CalendarWindow *calwin)
{
        GtkWidget *frame;
        GtkWidget *vbox;

        frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
        gtk_container_add (GTK_CONTAINER (calwin), frame);
        gtk_widget_show (frame);

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
        gtk_container_add (GTK_CONTAINER (frame), vbox);
        gtk_widget_show (vbox);

	calwin->priv->calendar = calendar_window_create_calendar (calwin);
        gtk_widget_show (calwin->priv->calendar);

	if (!calwin->priv->invert_order) {
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->calendar, TRUE, FALSE, 0);
                calendar_window_pack_pim (calwin, vbox);
		calendar_window_pack_locations (calwin, vbox);
	} else {
		calendar_window_pack_locations (calwin, vbox);
                calendar_window_pack_pim (calwin, vbox);
                gtk_box_pack_start (GTK_BOX (vbox),
				    calwin->priv->calendar, TRUE, FALSE, 0);
	}
}

GtkWidget *
calendar_window_get_locations_box (CalendarWindow *calwin)
{
	return calwin->priv->locations_list;
}

static GObject *
calendar_window_constructor (GType                  type,
			     guint                  n_construct_properties,
			     GObjectConstructParam *construct_properties)
{
	GObject        *obj;
	CalendarWindow *calwin;

	obj = G_OBJECT_CLASS (calendar_window_parent_class)->constructor (type,
									  n_construct_properties,
									  construct_properties);

	calwin = CALENDAR_WINDOW (obj);

	g_assert (calwin->priv->settings != NULL);

	calendar_window_fill (calwin);

	return obj;
}

static void
calendar_window_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	CalendarWindow *calwin;

	g_return_if_fail (CALENDAR_IS_WINDOW (object));

	calwin = CALENDAR_WINDOW (object);

	switch (prop_id) {
	case PROP_INVERTORDER:
		g_value_set_boolean (value,
				     calendar_window_get_invert_order (calwin));
		break;
	case PROP_SHOWWEEKS:
		g_value_set_boolean (value,
				     calendar_window_get_show_weeks (calwin));
		break;
	case PROP_SETTINGS:
		g_value_set_object (value,
				    calendar_window_get_settings (calwin));
		break;
	case PROP_LOCKEDDOWN:
		g_value_set_boolean (value,
				     calendar_window_get_locked_down (calwin));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
calendar_window_set_property (GObject       *object,
			      guint          prop_id,
			      const GValue  *value,
			      GParamSpec    *pspec)
{
	CalendarWindow *calwin;

	g_return_if_fail (CALENDAR_IS_WINDOW (object));

	calwin = CALENDAR_WINDOW (object);

	switch (prop_id) {
	case PROP_INVERTORDER:
		calendar_window_set_invert_order (calwin,
						  g_value_get_boolean (value));
		break;
	case PROP_SHOWWEEKS:
		calendar_window_set_show_weeks (calwin,
						g_value_get_boolean (value));
		break;
	case PROP_SETTINGS:
		calendar_window_set_settings (calwin,
					      g_value_get_object (value));
		break;
	case PROP_LOCKEDDOWN:
		calendar_window_set_locked_down (calwin,
						 g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
calendar_window_dispose (GObject *object)
{
#ifdef HAVE_EDS
	CalendarWindow *calwin;

	calwin = CALENDAR_WINDOW (object);

        if (calwin->priv->client)
                g_object_unref (calwin->priv->client);
        calwin->priv->client = NULL;

        if (calwin->priv->appointments_model)
                g_object_unref (calwin->priv->appointments_model);
        calwin->priv->appointments_model = NULL;

        if (calwin->priv->tasks_model)
                g_object_unref (calwin->priv->tasks_model);
        calwin->priv->tasks_model = NULL;

        if (calwin->priv->appointments_filter)
                g_object_unref (calwin->priv->appointments_filter);
        calwin->priv->appointments_filter = NULL;

        if (calwin->priv->birthdays_filter)
                g_object_unref (calwin->priv->birthdays_filter);
        calwin->priv->birthdays_filter = NULL;

        if (calwin->priv->tasks_filter)
                g_object_unref (calwin->priv->tasks_filter);
        calwin->priv->tasks_filter = NULL;

        if (calwin->priv->weather_filter)
                g_object_unref (calwin->priv->weather_filter);
        calwin->priv->weather_filter = NULL;

	g_clear_object (&calwin->priv->settings);
#endif /* HAVE_EDS */

	G_OBJECT_CLASS (calendar_window_parent_class)->dispose (object);
}

static void
calendar_window_class_init (CalendarWindowClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->constructor = calendar_window_constructor;
	gobject_class->get_property = calendar_window_get_property;
        gobject_class->set_property = calendar_window_set_property;
	gobject_class->dispose = calendar_window_dispose;

	signals[EDIT_LOCATIONS] = g_signal_new ("edit-locations",
						G_TYPE_FROM_CLASS (gobject_class),
						G_SIGNAL_RUN_FIRST,
						G_STRUCT_OFFSET (CalendarWindowClass, edit_locations),
						NULL,
						NULL,
						NULL,
						G_TYPE_NONE, 0);

	g_object_class_install_property (
		gobject_class,
		PROP_INVERTORDER,
		g_param_spec_boolean ("invert-order",
				      "Invert Order",
				      "Invert order of the calendar and tree views",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_SHOWWEEKS,
		g_param_spec_boolean ("show-weeks",
				      "Show Weeks",
				      "Show weeks in the calendar",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		gobject_class,
		PROP_SETTINGS,
		g_param_spec_object ("settings",
				     "Applet settings",
				     "",
				     G_TYPE_SETTINGS,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		gobject_class,
		PROP_LOCKEDDOWN,
		g_param_spec_boolean ("locked-down",
				      "Locked Down",
				      "Whether the window should offer access to preferences",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	gtk_widget_class_set_css_name (widget_class, "gp-calendar-window");
}

static void
calendar_window_init (CalendarWindow *calwin)
{
	GtkWindow *window;

	calwin->priv = calendar_window_get_instance_private (calwin);

	window = GTK_WINDOW (calwin);
	gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_decorated (window, FALSE);
	gtk_window_set_resizable (window, FALSE);
	gtk_window_stick (window);
	gtk_window_set_title (window, _("Calendar"));
	gtk_window_set_icon_name (window, CLOCK_ICON);

#ifdef HAVE_EDS
	calwin->priv->previous_selection = NULL;
#endif
}

GtkWidget *
calendar_window_new (GSettings  *applet_settings,
		     gboolean    invert_order)
{
	CalendarWindow *calwin;

	calwin = g_object_new (CALENDAR_TYPE_WINDOW,
			       "type", GTK_WINDOW_TOPLEVEL,
			       "invert-order", invert_order,
			       "settings", applet_settings,
			       NULL);

	return GTK_WIDGET (calwin);
}

void
calendar_window_refresh (CalendarWindow *calwin)
{
	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

#ifdef HAVE_EDS
	if (calwin->priv->appointments_filter && calwin->priv->appointment_list)
		gtk_tree_model_filter_refilter (calwin->priv->appointments_filter);
	if (calwin->priv->birthdays_filter && calwin->priv->birthday_list)
		gtk_tree_model_filter_refilter (calwin->priv->birthdays_filter);
	if (calwin->priv->tasks_filter && calwin->priv->task_list)
		gtk_tree_model_filter_refilter (calwin->priv->tasks_filter);
	if (calwin->priv->weather_filter && calwin->priv->weather_list)
		gtk_tree_model_filter_refilter (calwin->priv->weather_filter);
#endif
}

gboolean
calendar_window_get_invert_order (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin), FALSE);

	return calwin->priv->invert_order;
}

void
calendar_window_set_invert_order (CalendarWindow *calwin,
				  gboolean        invert_order)
{
	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

	if (invert_order == calwin->priv->invert_order)
		return;

	calwin->priv->invert_order = invert_order;
	//FIXME: update the order of the content of the window

	g_object_notify (G_OBJECT (calwin), "invert-order");
}

gboolean
calendar_window_get_show_weeks (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin), FALSE);

	return calwin->priv->show_weeks;
}

void
calendar_window_set_show_weeks (CalendarWindow *calwin,
				gboolean        show_weeks)
{
	GtkCalendarDisplayOptions options;

	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

	if (show_weeks == calwin->priv->show_weeks)
		return;

	calwin->priv->show_weeks = show_weeks;

	if (calwin->priv->calendar) {
		options = gtk_calendar_get_display_options (GTK_CALENDAR (calwin->priv->calendar));

		if (show_weeks)
			options |= GTK_CALENDAR_SHOW_WEEK_NUMBERS;
		else
			options &= ~(GTK_CALENDAR_SHOW_WEEK_NUMBERS);

		gtk_calendar_set_display_options (GTK_CALENDAR (calwin->priv->calendar),
						  options);
	}

	g_object_notify (G_OBJECT (calwin), "show-weeks");
}

GDesktopClockFormat
calendar_window_get_time_format (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin),
			      G_DESKTOP_CLOCK_FORMAT_24H);

#ifdef HAVE_EDS
	return calwin->priv->time_format;
#else
	return G_DESKTOP_CLOCK_FORMAT_24H;
#endif
}

void
calendar_window_set_time_format (CalendarWindow      *calwin,
				 GDesktopClockFormat  time_format)
{
	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

#ifdef HAVE_EDS
	if (time_format != G_DESKTOP_CLOCK_FORMAT_12H &&
	    time_format != G_DESKTOP_CLOCK_FORMAT_24H)
		time_format = clock_locale_format ();

	if (time_format == calwin->priv->time_format)
		return;

	calwin->priv->time_format = time_format;
	/* Time to display for appointments has changed */
	if (calwin->priv->appointments_model)
		handle_appointments_changed (calwin);
#endif
}

static GSettings *
calendar_window_get_settings (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin), NULL);

	return calwin->priv->settings;
}

static void
calendar_window_set_settings (CalendarWindow *calwin,
			      GSettings      *settings)
{
	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

	/* This only ever called once, so we can ignore the previous
	   value. */
	calwin->priv->settings = g_object_ref (settings);
}

static gboolean
calendar_window_get_locked_down (CalendarWindow *calwin)
{
	g_return_val_if_fail (CALENDAR_IS_WINDOW (calwin), FALSE);

	return calwin->priv->locked_down;
}

static void
calendar_window_set_locked_down (CalendarWindow *calwin,
				 gboolean        locked_down)
{
	g_return_if_fail (CALENDAR_IS_WINDOW (calwin));

	if (locked_down == calwin->priv->locked_down)
		return;

	calwin->priv->locked_down = locked_down;

	g_object_notify (G_OBJECT (calwin), "locked-down");
}

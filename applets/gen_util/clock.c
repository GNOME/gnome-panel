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

typedef struct _ClockData ClockData;
typedef void (*ClockUpdateFunc) (ClockData *, time_t);
struct _ClockData {
	GtkWidget *applet;
	GtkWidget *clockw;
	int timeout;
	int hourformat;
	ClockUpdateFunc update_func;
	PanelOrientType orient;

	GtkWidget *props;
	int prop_hourformat;
};


static void clock_properties(AppletWidget *, gpointer);


typedef struct {
	GtkWidget *date;
	GtkWidget *time;
} ComputerClock;

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

	return 1;
}

static gint
applet_session_save(GtkWidget * w,
		    const char *cfgpath,
		    const char *globcfgpath,
		    gpointer data)
{
	ClockData *cd = data;
	gnome_config_push_prefix(cfgpath);
	gnome_config_set_int("hourformat", cd->hourformat);
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
	gtk_label_set(GTK_LABEL(cc->date), date);

	/* This format string is used, to display the actual time.  */
	if (cd->hourformat == 0) {
		if (strftime(hour, 20, _("%I:%M %p"), tm) == 20)
			hour[19] = '\0';
	} else if (cd->hourformat == 1) {
		if (strftime(hour, 20, _("%H:%M"), tm) == 20)
			hour[19] = '\0';
	}
	gtk_label_set(GTK_LABEL(cc->time), hour);
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
	gtk_container_border_width(GTK_CONTAINER(align), 4);
	gtk_container_add(GTK_CONTAINER(frame), align);
	gtk_widget_show(align);

	vbox = gtk_vbox_new(FALSE, FALSE);
	gtk_container_add(GTK_CONTAINER(align), vbox);
	gtk_widget_show(vbox);

	cc = g_new(ComputerClock, 1);
	cc->date = gtk_label_new("");
	cc->time = gtk_label_new("");

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

static ClockData *
create_clock_widget(GtkWidget * applet)
{
	GtkWidget *clock;
	ClockData *cd;
	time_t current_time;

	cd = g_new(ClockData, 1);

	/*FIXME: different clock types here */
	create_computer_clock_widget(&clock, &cd->update_func);

	cd->clockw = clock;
	cd->applet = applet;

	cd->props = NULL;

	/* Install timeout handler */

	cd->timeout = gtk_timeout_add(3000, clock_timeout_callback, cd);

	cd->orient = ORIENT_UP;

	gtk_signal_connect(GTK_OBJECT(clock), "destroy",
			   (GtkSignalFunc) destroy_clock,
			   cd);
	/* Call the clock's update function so that it paints its first state */

	time(&current_time);

	(*cd->update_func) (cd, current_time);

	return cd;
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
make_clock_applet(const gchar * param)
{
	ClockData *cd;
	GtkWidget *applet;

	applet = applet_widget_new_with_param(param);
	if (!applet)
		g_error("Can't create applet!\n");

	cd = create_clock_widget(applet);

	if (APPLET_WIDGET(applet)->cfgpath &&
	    *(APPLET_WIDGET(applet)->cfgpath)) {
		gnome_config_push_prefix(APPLET_WIDGET(applet)->cfgpath);
		cd->hourformat = gnome_config_get_int("hourformat=0");
		gnome_config_pop_prefix();
	} else {
		cd->hourformat = 0;
	}


	/*we have to bind change_orient before we do applet_widget_add 
	   since we need to get an initial change_orient signal to set our
	   initial oriantation, and we get that during the _add call */
	gtk_signal_connect(GTK_OBJECT(applet), "change_orient",
			   GTK_SIGNAL_FUNC(applet_change_orient),
			   cd);

	gtk_widget_show(cd->clockw);
	applet_widget_add(APPLET_WIDGET(applet), cd->clockw);
	gtk_widget_show(applet);

	gtk_signal_connect(GTK_OBJECT(applet), "session_save",
			   GTK_SIGNAL_FUNC(applet_session_save),
			   cd);

	applet_widget_register_callback(APPLET_WIDGET(applet),
					"properties",
					_("Properties"),
					clock_properties,
					cd);

	return applet;
}

static void
apply_properties(GtkWidget * widget, gint button_num, gpointer data)
{
	ClockData *cd = data;
	time_t current_time;

	cd->hourformat = cd->prop_hourformat;

	time(&current_time);
	(*cd->update_func) (cd, current_time);
}

static int
close_properties(GtkWidget * w, gpointer data)
{
	ClockData *cd = data;

	cd->props = NULL;
	return FALSE;
}

static void
set_hour_format_cb(GtkWidget * w, gpointer data)
{
	g_return_if_fail(w != NULL);
	if(GTK_TOGGLE_BUTTON(w)->active) {
		ClockData *cd = gtk_object_get_user_data(GTK_OBJECT(w));
		cd->prop_hourformat = (long)data;
		gnome_property_box_changed (GNOME_PROPERTY_BOX (cd->props));
	}
}

static void
clock_properties(AppletWidget * applet, gpointer data)
{
	GtkWidget *table;
	GtkWidget *twelvehour;
	GtkWidget *twentyfourhour;
	ClockData *cd = data;

	if(cd->props) {
		gdk_window_raise(cd->props->window);
		return;
	}
	cd->props = gnome_property_box_new();
	gtk_window_set_title (GTK_WINDOW (cd->props),
			      _("Clock properties"));

	table = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(table);

	gtk_container_border_width(GTK_CONTAINER(table), GNOME_PAD);
	gtk_table_set_row_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings(GTK_TABLE(table), GNOME_PAD_SMALL);


	twelvehour = gtk_radio_button_new_with_label(NULL, "12 hour");
	gtk_table_attach(GTK_TABLE(table), twelvehour, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL | GTK_EXPAND,
			 0, 0);
	gtk_object_set_user_data(GTK_OBJECT(twelvehour), (gpointer) data);
	gtk_widget_show(twelvehour);

	twentyfourhour = gtk_radio_button_new_with_label(
			 gtk_radio_button_group(GTK_RADIO_BUTTON(twelvehour)),
					        "24 hour");

	gtk_table_attach(GTK_TABLE(table), twentyfourhour, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL | GTK_EXPAND,
			 0, 0);
	gtk_object_set_user_data(GTK_OBJECT(twentyfourhour), (gpointer) data);
	gtk_widget_show(twentyfourhour);

	switch (cd->hourformat) {
	case 0:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(twelvehour),
					    TRUE);
		break;

	case 1:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(twentyfourhour),
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


	gnome_property_box_append_page(GNOME_PROPERTY_BOX(cd->props), table,
				       gtk_label_new(_("Clock")));
	gtk_signal_connect(GTK_OBJECT(cd->props), "apply",
			   GTK_SIGNAL_FUNC(apply_properties), data);
	gtk_signal_connect(GTK_OBJECT(cd->props), "delete_event",
			   GTK_SIGNAL_FUNC(close_properties), data);
	gtk_signal_connect(GTK_OBJECT(cd->props), "destroy",
			   GTK_SIGNAL_FUNC(close_properties), data);

	gtk_widget_show(cd->props);
}

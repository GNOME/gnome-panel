/*
 * GNOME time/date display module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 *
 * Feel free to implement new look and feels :-)
 */

#include <stdio.h>
#ifdef HAVE_LIBINTL
#    include <libintl.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include "applet-lib.h"
#include "applet-widget.h"
#include "panel.h"

#define CLOCK_DATA "clock_data"

typedef void (*ClockUpdateFunc) (GtkWidget *clock, time_t current_time);

GtkWidget *aw;

int applet_id=-1; /*this is our id we use to comunicate with the panel*/


typedef struct {
	int timeout;
	ClockUpdateFunc update_func;
} ClockData;

typedef struct {
	GtkWidget *date;
	GtkWidget *time;
} ComputerClock;


static void
free_data(GtkWidget *widget, gpointer data)
{
	g_free(data);
}

void
do_callback(short id,
	    const char *callback_name)
{
  printf("Not doing callback %s\n", callback_name);
}

static int
clock_timeout_callback (gpointer data)
{
	time_t     current_time;
	GtkWidget *clock;
	ClockData *cd;
	
	time (&current_time);

	clock = data;
	cd = gtk_object_get_data(GTK_OBJECT(clock), CLOCK_DATA);

	(*cd->update_func) (clock, current_time);

	return 1;
}

static void
computer_clock_update_func(GtkWidget *clock, time_t current_time)
{
	ComputerClock *cc;
	char          *strtime;
	char           date[20], hour[20];

	cc = gtk_object_get_user_data(GTK_OBJECT(clock));

	strtime = ctime (&current_time);

	strncpy (date, strtime, 10);
	date[10] = '\0';
	gtk_label_set (GTK_LABEL (cc->date), date);

	strtime += 11;
	strncpy (hour, strtime, 5);
	hour[5] = '\0';
	gtk_label_set (GTK_LABEL (cc->time), hour);
}

static void
create_computer_clock_widget(GtkWidget **clock, ClockUpdateFunc *update_func)
{
	GtkWidget     *frame;
	GtkWidget     *align;
	GtkWidget     *vbox;
	ComputerClock *cc;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
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
destroy_clock (GtkWidget *widget, void *data)
{
	ClockData *cd;

	cd = gtk_object_get_data(GTK_OBJECT(widget), CLOCK_DATA);
	
	gtk_timeout_remove (cd->timeout);

	g_free(cd);
}

static GtkWidget *
create_clock_widget (GtkWidget *window)
{
	ClockData       *cd;
	GtkWidget       *clock;
	time_t           current_time;

	cd = g_new(ClockData, 1);

	/*FIXME: different clock types here*/
	create_computer_clock_widget(&clock, &cd->update_func);

	/* Install timeout handler */

	cd->timeout = gtk_timeout_add(3000, clock_timeout_callback, clock);

	gtk_object_set_data(GTK_OBJECT(clock), CLOCK_DATA, cd);

	gtk_signal_connect(GTK_OBJECT(clock), "destroy",
			   (GtkSignalFunc) destroy_clock,
			   NULL);
	/* Call the clock's update function so that it paints its first state */

	time(&current_time);
	
	(*cd->update_func) (clock, current_time);

	return clock;
}

/*these are commands sent over corba:*/
void
change_orient(int id, int orient)
{
	PanelOrientType o = (PanelOrientType)orient;
	puts("CHANGE_ORIENT");
}

void
session_save(int id, int panel, int pos)
{
	/*FIXME: save the position*/
	puts("SESSION_SAVE");
}

static gint
applet_die(gpointer data)
{
	exit(0);
}

void
shutdown_applet(int id)
{
	puts("SHUTDOWN_APPLET");
	/*kill our window*/
	gtk_widget_unref(aw);
	gtk_timeout_add(100,applet_die,NULL);
}


int
main(int argc, char **argv)
{
	GtkWidget *clock;
	char *result;
	
	gnome_init("clock_applet", &argc, &argv);

	if (!gnome_panel_applet_init_corba (&argc, &argv)){
		fprintf (stderr, "Could not comunicate with the panel\n");
		exit (1);
	}

	aw = applet_widget_new ();

	clock = create_clock_widget (GTK_WIDGET(aw));
	gtk_widget_show(clock);
	applet_widget_add (APPLET_WIDGET (aw), clock);
	gtk_widget_show (aw);

	/*FIXME: do session saving, find out panel and pos from the panel
		 so we can restore them on the next startup*/
	result = gnome_panel_prepare_and_transfer(aw,argv[0],&applet_id,0,0);
	printf ("Done\n");
	if (result){
		printf ("Could not talk to the Panel: %s\n", result);
		exit (1);
	}

	applet_corba_gtk_main ("IDL:GNOME/Applet:1.0");

	return 0;
}

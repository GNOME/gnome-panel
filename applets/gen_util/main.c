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
#include <config.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <config.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include <applet-widget.h>

#include "mailcheck.h"
#include "clock.h"
#include "printer.h"

static void
make_new_applet(const gchar *goad_id)
{
	if(strstr(goad_id,"gen_util_mailcheck"))
		make_mailcheck_applet(goad_id);
	else if(strstr(goad_id,"gen_util_printer"))
		make_printer_applet(goad_id);
	else if(strstr(goad_id,"gen_util_clock"))
		make_clock_applet(goad_id);
}

/*when we get a command to start a new widget*/
static void
applet_start_new_applet(const gchar *goad_id, gpointer data)
{
	make_new_applet(goad_id);
}

int
main(int argc, char **argv)
{
	gchar *goad_id;
	char *argstr = NULL;
	GList *list = NULL;

	/*this is needed for printer applet*/
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sa.sa_flags   = SA_NOCLDSTOP;
	sigemptyset (&sa.sa_mask);
	sigaction (SIGCHLD, &sa, NULL);

	/* Initialize the i18n stuff */
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	
	list = g_list_append(list,"gen_util_clock");
	list = g_list_append(list,"gen_util_mailcheck");
	list = g_list_append(list,"gen_util_printer");

	applet_widget_init("gen_util_applet", VERSION, argc, argv,
			   NULL, 0, NULL, TRUE, list,
			   applet_start_new_applet, NULL);
	g_list_free(list);

	goad_id = goad_server_activation_id();
	if(!goad_id)
	  goad_id = "gen_util_clock"; /* badhack */
	make_new_applet(goad_id);

	applet_widget_gtk_main();

	return 0;
}

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

int start_clock = 0, start_mailcheck = 0, start_printer = 0;

static const struct poptOption options[] = {
  {"clock", '\0', POPT_ARG_NONE, &start_clock, 0, N_("Start in clock mode"), NULL},
  {"mailcheck", '\0', POPT_ARG_NONE, &start_mailcheck, 0, N_("Start in mailcheck mode"), NULL},
  {"printer", '\0', POPT_ARG_NONE, &start_printer, 0, N_("Start in printer mode"), NULL},
  {NULL, '\0', 0, NULL, 0}
};

static void
make_new_applet(const gchar *param)
{
	if(strstr(param,"gen_util_mailcheck"))
		make_mailcheck_applet("");
	else if(strstr(param,"gen_util_printer"))
		make_printer_applet("");
	else if(strstr(param,"gen_util_clock"))
		make_clock_applet("");
}

/*when we get a command to start a new widget*/
static void
applet_start_new_applet(const gchar *param, gpointer data)
{
	make_new_applet(param);
}

int
main(int argc, char **argv)
{
	gchar *param;
	char *argstr = NULL;

	/*this is needed for printer applet*/
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sa.sa_flags   = SA_NOCLDSTOP;
	sigemptyset (&sa.sa_mask);
	sigaction (SIGCHLD, &sa, NULL);

	/* Initialize the i18n stuff */
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	applet_widget_init("gen_util_applet", VERSION, argc, argv,
			   options, 0, NULL, TRUE, TRUE,
			   applet_start_new_applet, NULL);

	if(!goad_server_activation_id()
	   || !strcmp(goad_server_activation_id(), "gen_util_clock"))
	  make_clock_applet("");
	if(!strcmp(goad_server_activation_id(), "gen_util_mailcheck"))
	  make_mailcheck_applet("");
	if(!strcmp(goad_server_activation_id(), "gen_util_printer"))
	  make_printer_applet("");

	applet_widget_gtk_main();

	return 0;
}

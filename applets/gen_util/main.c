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

/* These are the arguments that our application supports.  */
static struct argp_option arguments[] =
{
  { "clock", -1, NULL, 0, N_("Start in clock mode"), 1 },
  { "mailcheck", -1, NULL, 0, N_("Start in mailcheck mode"), 1 },
  { "printer", -1, NULL, 0, N_("Start in printer mode"), 1 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Forward declaration of the function that gets called when one of
   our arguments is recognized.  */
/* we ignore the arguments */
static error_t
parse_an_arg (int key, char *arg, struct argp_state *state)
{
	return 0;
}

/* This structure defines our parser.  It can be used to specify some
   options for how our parsing function should be called.  */
static struct argp parser =
{
  arguments,			/* Options.  */
  parse_an_arg,			/* The parser function.  */
  NULL,				/* Some docs.  */
  NULL,				/* Some more docs.  */
  NULL,				/* Child arguments -- gnome_init fills
				   this in for us.  */
  NULL,				/* Help filter.  */
  NULL				/* Translation domain; for the app it
				   can always be NULL.  */
};



static void
make_new_applet(const gchar *param)
{
	if(strstr(param,"--mailcheck"))
		make_mailcheck_applet(param);
	else if(strstr(param,"--printer"))
		make_printer_applet(param);
	else
		make_clock_applet(param);
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

	/*this is needed for printer applet*/
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sa.sa_flags   = SA_NOCLDSTOP;
	sigemptyset (&sa.sa_mask);
	sigaction (SIGCHLD, &sa, NULL);

	/* Initialize the i18n stuff */
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	/*we make a param string, instead of first parsing the params with
	  argp, we will allways use a string, since the panel will only
	  give us a string */
	param = make_param_string(argc,argv);

	applet_widget_init("gen_util_applet", &parser, argc, argv, 0, NULL,
			   argv[0],TRUE,TRUE,applet_start_new_applet,NULL);

	make_new_applet(param);
	g_free(param);

	applet_widget_gtk_main();

	return 0;
}

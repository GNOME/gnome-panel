/*
 * GNOME time/date display module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 *
 * Feel free to implement new look and feels :-)
 */

#include <config.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include <applet-widget.h>

#include "mailcheck.h"
#include "clock.h"
#include "printer.h"

static GtkWidget *
make_new_applet(const gchar *goad_id)
{
	if(strstr(goad_id,"gen_util_mailcheck"))
		return make_mailcheck_applet(goad_id);
	else if(strstr(goad_id,"gen_util_printer"))
		return make_printer_applet(goad_id);
	else if(strstr(goad_id,"gen_util_clock"))
		return make_clock_applet(goad_id);

	return NULL;
}

#if 1

static CORBA_Object
activator (PortableServer_POA poa,
	   const char *goad_id,
	   const char **params,
	   gpointer *impl_ptr,
	   CORBA_Environment *ev)
{
	GtkWidget *widget;

	widget = make_new_applet (goad_id);
	if (widget == NULL) {
		g_warning (_("Don't know how to activate `%s'\n"), goad_id);
		return CORBA_OBJECT_NIL;
	}

	return applet_widget_corba_activate (widget, poa, goad_id,
					     params, impl_ptr, ev);
}

static void
deactivator (PortableServer_POA poa,
	     const char *goad_id,
	     gpointer impl_ptr,
	     CORBA_Environment *ev)
{
	applet_widget_corba_deactivate (poa, goad_id, impl_ptr, ev);
}

static const char *repo_id[]={ "IDL:GNOME/Applet:1.0", NULL };
static GnomePluginObject applets_list[] = { 
	{ repo_id, "gen_util_mailcheck", NULL, "Mailcheck applet",
	  &activator, &deactivator },
	{ repo_id, "gen_util_printer", NULL, "Printer applet",
	  &activator, &deactivator },
	{ repo_id, "gen_util_clock", NULL, "Clock applet",
	  &activator, &deactivator },
	{ NULL }
};

GnomePlugin GNOME_Plugin_info = { 
	applets_list,
	NULL
};

#else
/*when we get a command to start a new widget*/
static GtkWidget *
applet_start_new_applet(const gchar *goad_id, const char **params, int nparams)
{
  return make_new_applet(goad_id);
}

int
main(int argc, char **argv)
{
	const gchar *goad_id;

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
			   NULL, 0, NULL);
	applet_factory_new("gen_util_applet", NULL, applet_start_new_applet);

	goad_id = goad_server_activation_id();
	if(!goad_id)
	  goad_id = "gen_util_clock"; /* badhack */
	if(strcmp(goad_id, "gen_util_applet")) /* Only do if factory wasn't requested (odd) */
	  make_new_applet(goad_id);

	applet_widget_gtk_main();

	return 0;
}

#endif

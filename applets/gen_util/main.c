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

#include <panel-applet.h>

#include <gtk/gtk.h>
#include <libbonobo.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnome/libgnome.h>
#include <bonobo/bonobo-shlib-factory.h>

//#include "mailcheck.h"
#include "clock.h"
//#include "printer.h"


static BonoboObject *
genutil_factory (BonoboGenericFactory *this,
	       const gchar          *iid,
	       gpointer              data)
{
	BonoboObject *applet = NULL;

	g_print ("genutil_factory: %s\n", iid);
	
	if (!strcmp (iid, "OAFIID:GNOME_ClockApplet"))
		applet = make_clock_applet();

	return applet;
}

BONOBO_ACTIVATION_SHLIB_FACTORY ("OAFIID:GNOME_GenUtilApplet_Factory",
				 "GenUtil Applet factory",
				 genutil_factory, NULL);



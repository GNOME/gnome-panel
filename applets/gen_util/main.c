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

#include "mailcheck.h"
#include "clock.h"
#include "pager.h"
#include "tasklist.h"
//#include "printer.h"

static gboolean
genutil_factory (PanelApplet *applet,
		 const gchar *iid,
		 gpointer     data)
{
	gboolean retval = FALSE;

	g_print ("genutil_factory: %s\n", iid);

	if (!strcmp (iid, "OAFIID:GNOME_ClockApplet"))
		retval = fill_clock_applet(applet);
	
	if (!strcmp (iid, "OAFIID:GNOME_PagerApplet"))
		retval = fill_pager_applet(applet);

	if (!strcmp (iid, "OAFIID:GNOME_TasklistApplet"))
		retval = fill_tasklist_applet(applet);
		
	if (!strcmp (iid, "OAFIID:GNOME_MailcheckApplet"))
		retval = fill_mailcheck_applet(applet);

	return retval;
}

PANEL_APPLET_BONOBO_SHLIB_FACTORY ("OAFIID:GNOME_GenUtilApplet_Factory",
				   PANEL_TYPE_APPLET,
				   "GenUtil Applet factory",
				    genutil_factory, NULL);

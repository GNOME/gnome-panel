/* Gnome panel: status applet
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  George Lebl
 */

#include <gdk/gdkx.h>
#include <config.h>
#include <string.h>
#include <signal.h>
#include <gnome.h>

#include "panel-include.h"
#include "gnome-panel.h"

static Status *the_status = NULL; /*"there can only be one" status applet*/
static GtkWidget *offscreen = NULL; /*offscreen window for putting status
				      spots if there is no status applet*/
static GSList *spots;

StatusSpot *
new_status_spot(void)
{
	StatusSpot *ss = g_new0(StatusSpot,1);
	/*FIXME: get some sort of socket out there! if there is no status applet,
	  just make an offscreen window for it*/
	spots = g_slist_prepend(spots,ss);
	return ss;
}

void
status_spot_remove(StatusSpot *ss)
{
	/*FIXME: fill in*/
	spots = g_slist_remove(spots,ss);
}

void
load_status_applet(PanelWidget *panel, int pos)
{
	/*FIXME: fill in*/
}

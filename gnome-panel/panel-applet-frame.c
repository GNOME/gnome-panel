#include <libbonoboui.h>

#include "panel-applet-frame.h"

#include "applet.h"

void
panel_applet_load (const gchar *iid,
		   PanelWidget *panel,
		   gint         pos)
{
	GtkWidget *control;
	gboolean   success;

	control = bonobo_widget_new_control (iid, NULL);

	success = register_toy (control, 
				NULL,     /* FIXME: data */
				NULL,     /* FIXME: data_destroy */
				panel,
				pos,
				FALSE,
				APPLET_BONOBO);
	if (!success)
		g_warning ("Cannot register control widget\n");
}

#include <libbonoboui.h>

#include "panel-applet-frame.h"

#include "applet.h"

static GtkWidget *
panel_bonobo_applet_widget (const gchar *iid)
{
	GtkWidget *control;
	GtkWidget *event_box;

	event_box = gtk_event_box_new ();

	control = bonobo_widget_new_control (iid, NULL);

	gtk_container_add (GTK_CONTAINER (event_box), control);

	gtk_widget_show_all (event_box);

	return event_box;
}

void
panel_bonobo_applet_load (const gchar *iid,
			  PanelWidget *panel,
			  gint         pos)
{
	GtkWidget  *widget;
	AppletInfo *info;

	widget = panel_bonobo_applet_widget (iid);

	info = panel_applet_register (widget, 
				      NULL,     /* FIXME: data */
				      NULL,     /* FIXME: data_destroy */
				      panel,
				      pos,
				      FALSE,
				      APPLET_BONOBO);

	if (!info)
		g_warning ("Cannot register control widget\n");
}

#include <string.h>

#include <gtk/gtk.h>
#include <libbonobo.h>

#include "applet-widget.h"
#include "applet-init.h"

static BonoboObject *
test_applet_new (const gchar *iid)
{
	GtkWidget *widget;
	GtkWidget *label;

	widget = applet_widget_new (iid);

	label = gtk_label_new ("Hello");
	gtk_widget_show (label);

	applet_widget_add (APPLET_WIDGET (widget), label);
	gtk_widget_show (widget);

	return BONOBO_OBJECT (APPLET_WIDGET (widget)->object);
}

static BonoboObject *
test_applet_factory (BonoboGenericFactory *this,
		     const gchar          *iid,
		     gpointer              data)
{
        BonoboObject *applet = NULL;

        if (!strcmp (iid, "OAFIID:OAFIID:GNOME_Panel_TestApplet"))
                applet = test_applet_new (iid);

        return applet;
}

APPLET_BONOBO_FACTORY ("OAFIID:GNOME_Panel_TestApplet",
		       "A Test Applet for the GNOME-2.0 Panel",
		       "0",
		       test_applet_factory,
		       NULL)

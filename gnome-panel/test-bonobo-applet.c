#include <config.h>
#include <string.h>

#include <libbonoboui.h>

static BonoboObject *
test_applet_new (const gchar *iid)
{
	BonoboControl *control;
	GtkWidget     *label;

	g_message ("test_applet_new: %s\n", iid);

	label = gtk_label_new ("Hello");
	gtk_widget_show (label);

	control = bonobo_control_new (label);

	return BONOBO_OBJECT (control);
}


static BonoboObject *
test_applet_factory (BonoboGenericFactory *this,
		     const gchar          *iid,
		     gpointer              data)
{
	BonoboObject *applet = NULL;
    
	g_message ("test_applet_factory: %s\n", iid);

	if (!strcmp (iid, "OAFIID:GNOME_Panel_TestBonoboApplet"))
		applet = test_applet_new (iid); 
    
	g_message ("test_applet_factory: returning %p\n", applet);
    
	return applet;
}

BONOBO_ACTIVATION_FACTORY ("OAFIID:GNOME_Panel_TestBonoboApplet_Factory",
			   "A Test Applet for the GNOME-2.0 Panel",
			   "0",
			   test_applet_factory,
			   NULL)

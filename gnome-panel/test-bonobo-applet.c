/*
 * test-bonobo-applet.c:
 *
 * Authors:
 *    Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2001 Sun Microsystems, Inc.
 */

#include <string.h>

#include <libbonoboui.h>

#include "panel-applet.h"

static void
test_applet_on_do1 (BonoboUIComponent *uic,
		    gpointer           user_data,
		    const gchar       *verbname)
{
        g_message ("do1 called\n");
}

static void
test_applet_on_do2 (BonoboUIComponent *uic,
		    gpointer           user_data,
		    const gchar       *verbname)
{
        g_message ("do2 called\n");
}

static void
test_applet_on_do3 (BonoboUIComponent *uic,
		    gpointer           user_data,
		    const gchar       *verbname)
{
        g_message ("do3 called\n");
}

static const BonoboUIVerb test_applet_menu_verbs [] = {
        BONOBO_UI_VERB ("TestAppletDo1", test_applet_on_do1),
        BONOBO_UI_VERB ("TestAppletDo2", test_applet_on_do2),
        BONOBO_UI_VERB ("TestAppletDo3", test_applet_on_do3),

        BONOBO_UI_VERB_END
};

static const char test_applet_menu_xml [] =
	"<popup name=\"button3\">\n"
	"   <menuitem name=\"Test Item 1\" verb=\"TestAppletDo1\" _label=\"Test This One\"/>\n"
	"   <menuitem name=\"Test Item 2\" verb=\"TestAppletDo2\" _label=\"Test This Two\"/>\n"
	"   <menuitem name=\"Test Item 3\" verb=\"TestAppletDo3\" _label=\"Test This Three\"/>\n"
	"</popup>\n";

static BonoboObject *
test_applet_new (const gchar *iid)
{
	PanelApplet *applet;
	GtkWidget   *label;

	g_message ("test_applet_new: %s\n", iid);

	label = gtk_label_new ("Hello");

	applet = panel_applet_new (label);

	panel_applet_setup_menu (applet, test_applet_menu_xml, test_applet_menu_verbs, NULL);

	return BONOBO_OBJECT (panel_applet_get_control (applet));
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

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_Panel_TestBonoboApplet_Factory",
			     "A Test Applet for the GNOME-2.0 Panel",
			     "0",
			     test_applet_factory,
			     NULL)

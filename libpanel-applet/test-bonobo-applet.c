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
#include <gtk/gtktooltips.h>

#include "panel-applet.h"

static GtkTooltips *test_applet_tooltips = NULL;

static void
test_applet_on_do (BonoboUIComponent *uic,
		   gpointer           user_data,
		   const gchar       *verbname)
{
        g_message ("%s called\n", verbname);
}

static const BonoboUIVerb test_applet_menu_verbs [] = {
        BONOBO_UI_VERB ("TestAppletDo1", test_applet_on_do),
        BONOBO_UI_VERB ("TestAppletDo2", test_applet_on_do),
        BONOBO_UI_VERB ("TestAppletDo3", test_applet_on_do),

        BONOBO_UI_VERB_END
};

static const char test_applet_menu_xml [] =
	"<popup name=\"button3\">\n"
	"   <menuitem name=\"Test Item 1\" verb=\"TestAppletDo1\" _label=\"Test This One\"/>\n"
	"   <menuitem name=\"Test Item 2\" verb=\"TestAppletDo2\" _label=\"Test This Two\"/>\n"
	"   <menuitem name=\"Test Item 3\" verb=\"TestAppletDo3\" _label=\"Test This Three\"/>\n"
	"</popup>\n";

static void
test_applet_setup_tooltips (GtkWidget *widget)
{
	g_return_if_fail (!GTK_WIDGET_NO_WINDOW (widget));

	if (!test_applet_tooltips)
		test_applet_tooltips = gtk_tooltips_new ();

	gtk_tooltips_set_tip (test_applet_tooltips, widget, "Hello Tip", "");
}

static void
test_applet_handle_orient_change (PanelApplet       *applet,
				  PanelAppletOrient  orient,
				  GtkLabel          *label)
{
        gchar *text;

	g_return_if_fail (label && GTK_IS_LABEL (label));

        text = g_strdup (gtk_label_get_text (label));

        g_strreverse (text);

        gtk_label_set_text (label, text);

        g_free (text);
}

static void
test_applet_handle_size_change (PanelApplet *applet,
				gint         size,
				GtkLabel    *label)
{
	switch (size) {
	case GNOME_PANEL_XX_SMALL:
		gtk_label_set_markup (label, "<span size=\"xx-small\">Hello</span>");
		break;
	case GNOME_PANEL_X_SMALL:
		gtk_label_set_markup (label, "<span size=\"x-small\">Hello</span>");
		break;
	case GNOME_PANEL_SMALL:
		gtk_label_set_markup (label, "<span size=\"small\">Hello</span>");
		break;
	case GNOME_PANEL_MEDIUM:
		gtk_label_set_markup (label, "<span size=\"medium\">Hello</span>");
		break;
	case GNOME_PANEL_LARGE:
		gtk_label_set_markup (label, "<span size=\"large\">Hello</span>");
		break;
	case GNOME_PANEL_X_LARGE:
		gtk_label_set_markup (label, "<span size=\"x-large\">Hello</span>");
		break;
	case GNOME_PANEL_XX_LARGE:
		gtk_label_set_markup (label, "<span size=\"xx-large\">Hello</span>");
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
test_applet_handle_background_change (PanelApplet               *applet,
				      PanelAppletBackgroundType  type,
				      GdkColor                  *colour,
				      const gchar               *pixmap,
				      GtkLabel                  *label)
{
	switch (type) {
	case PANEL_NO_BACKGROUND:
		g_message ("Setting background to default");
		break;
	case PANEL_COLOUR_BACKGROUND:
		g_message ("Setting background to #%2x%2x%2x",
			    colour->red, colour->green, colour->blue);
		break;
	case PANEL_PIXMAP_BACKGOUND:
		g_message ("Setting background to '%s'", pixmap);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static BonoboObject *
test_applet_new (const gchar *iid)
{
	PanelApplet *applet;
	GtkWidget   *label;

	g_message ("test_applet_new: %s\n", iid);

	label = gtk_label_new (NULL);

	applet = panel_applet_new (label);

	test_applet_handle_size_change (applet, GNOME_PANEL_MEDIUM, label);

	panel_applet_setup_menu (applet, test_applet_menu_xml, test_applet_menu_verbs, NULL);

	test_applet_setup_tooltips (GTK_WIDGET (applet));

	g_signal_connect (G_OBJECT (applet),
			  "change_orient",
			  (GCallback) test_applet_handle_orient_change,
			  label);

	g_signal_connect (G_OBJECT (applet),
			  "change_size",
			  (GCallback) test_applet_handle_size_change,
			  label);

	g_signal_connect (G_OBJECT (applet),
			  "change_background",
			  (GCallback) test_applet_handle_background_change,
			  label);
			  
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

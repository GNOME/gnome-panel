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

typedef struct {
	PanelApplet   base;
	GtkWidget    *label;
} TestApplet;

static GType
test_applet_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelAppletClass),
			NULL, NULL, NULL, NULL, NULL,
			sizeof (TestApplet),
			0, NULL, NULL
		};

		type = g_type_register_static (
				PANEL_TYPE_APPLET, "TestApplet", &info, 0);
	}

	return type;
}

static void
test_applet_setup_tooltips (GtkWidget *widget)
{
	g_return_if_fail (!GTK_WIDGET_NO_WINDOW (widget));

	if (!test_applet_tooltips)
		test_applet_tooltips = gtk_tooltips_new ();

	gtk_tooltips_set_tip (test_applet_tooltips, widget, "Hello Tip", "");
}

static void
test_applet_handle_orient_change (TestApplet        *applet,
				  PanelAppletOrient  orient,
				  gpointer           dummy)
{
        gchar *text;

        text = g_strdup (gtk_label_get_text (GTK_LABEL (applet->label)));

        g_strreverse (text);

        gtk_label_set_text (GTK_LABEL (applet->label), text);

        g_free (text);
}

static void
test_applet_handle_size_change (TestApplet *applet,
				gint        size,
				gpointer    dummy)
{
	switch (size) {
	case GNOME_Vertigo_PANEL_XX_SMALL:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"xx-small\">Hello</span>");
		break;
	case GNOME_Vertigo_PANEL_X_SMALL:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"x-small\">Hello</span>");
		break;
	case GNOME_Vertigo_PANEL_SMALL:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"small\">Hello</span>");
		break;
	case GNOME_Vertigo_PANEL_MEDIUM:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"medium\">Hello</span>");
		break;
	case GNOME_Vertigo_PANEL_LARGE:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"large\">Hello</span>");
		break;
	case GNOME_Vertigo_PANEL_X_LARGE:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"x-large\">Hello</span>");
		break;
	case GNOME_Vertigo_PANEL_XX_LARGE:
		gtk_label_set_markup (
			GTK_LABEL (applet->label), "<span size=\"xx-large\">Hello</span>");
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
test_applet_handle_background_change (TestApplet                *applet,
				      PanelAppletBackgroundType  type,
				      GdkColor                  *color,
				      GdkPixmap                 *pixmap,
				      gpointer                   dummy)
{
	switch (type) {
	case PANEL_NO_BACKGROUND:
		g_message ("Setting background to default");
		gdk_window_set_back_pixmap (applet->label->window, NULL, FALSE);
		break;
	case PANEL_COLOR_BACKGROUND:
		g_message ("Setting background to #%2x%2x%2x",
			    color->red, color->green, color->blue);
		gdk_window_set_back_pixmap (applet->label->window, NULL, FALSE);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		g_message ("Setting background to '%p'", pixmap);
		gdk_window_set_back_pixmap (applet->label->window, pixmap, FALSE);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static gboolean
test_applet_fill (TestApplet *applet)
{
	applet->label = gtk_label_new (NULL);

	gtk_container_add (GTK_CONTAINER (applet), applet->label);

	gtk_widget_show_all (GTK_WIDGET (applet));

	test_applet_handle_size_change (applet, GNOME_Vertigo_PANEL_MEDIUM, NULL);

	panel_applet_setup_menu (
		PANEL_APPLET (applet), test_applet_menu_xml, test_applet_menu_verbs, NULL);

	test_applet_setup_tooltips (GTK_WIDGET (applet));

	g_signal_connect (G_OBJECT (applet),
			  "change_orient",
			  G_CALLBACK (test_applet_handle_orient_change),
			  NULL);

	g_signal_connect (G_OBJECT (applet),
			  "change_size",
			  G_CALLBACK (test_applet_handle_size_change),
			  NULL);

	g_signal_connect (G_OBJECT (applet),
			  "change_background",
			  G_CALLBACK (test_applet_handle_background_change),
			  NULL);

	return TRUE;
}

static gboolean
test_applet_factory (TestApplet  *applet,
		     const gchar *iid,
		     gpointer     data)
{
	gboolean retval = FALSE;
    
	if (!strcmp (iid, "OAFIID:GNOME_Panel_TestBonoboApplet"))
		retval = test_applet_fill (applet); 
    
	return retval;
}

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_Panel_TestBonoboApplet_Factory",
			     test_applet_get_type (),
			     "A Test Applet for the GNOME-2.0 Panel",
			     "0",
			     (PanelAppletFactoryCallback) test_applet_factory,
			     NULL)

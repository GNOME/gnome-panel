/*
 * panel-applet-frame.c:
 *
 * Authors:
 *    Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2001 Sun Microsystems, Inc.
 */

#include <libbonoboui.h>

#include "panel-applet-frame.h"

#include "applet.h"

#undef PANEL_APPLET_FRAME_DEBUG

static void
popup_handle_verb (BonoboUIComponent *uic,
		   gpointer           user_data,
		   const gchar       *verbname)
{
#ifdef PANEL_APPLET_FRAME_DEBUG
        g_message ("Verb %s invoked\n", verbname);
#endif
}

static BonoboUIVerb popup_verbs [] = {
        BONOBO_UI_VERB ("RemoveAppletFromPanel", popup_handle_verb),
        BONOBO_UI_VERB ("MoveApplet", popup_handle_verb),

        BONOBO_UI_VERB_END
};

static gchar popup_xml [] =
        "<popups>\n"
        "  <popup name=\"button3\">\n"
        "    <separator/>\n"
        "    <menuitem name=\"remove\" verb=\"RemoveAppletFromPanel\" _label=\"Remove From Panel\""
        "              pixtype=\"stock\" pixname=\"gtk-remove\"/>\n"
        "    <menuitem name=\"move\" verb=\"MoveApplet\" _label=\"Move\"/>\n"
        "  </popup>\n"
        "</popups>\n";


static GtkWidget *
panel_bonobo_applet_widget (const gchar *iid)
{
	BonoboControlFrame *control_frame;
	Bonobo_Control      control;
	BonoboUIComponent  *ui_component;
	GtkWidget          *widget;
	GtkWidget          *event_box;

	widget = bonobo_widget_new_control (iid, NULL);

	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (widget));

	control = bonobo_control_frame_get_control (control_frame);

	ui_component = bonobo_ui_component_new_default ();

	{
		CORBA_Environment  env;
		Bonobo_UIContainer popup_container;

		CORBA_exception_init (&env);

		popup_container = Bonobo_Control_getPopupContainer (control, &env);

		bonobo_ui_component_set_container (ui_component, popup_container, &env);

		CORBA_exception_free (&env);
	}

	bonobo_ui_component_set_translate (ui_component, "/", popup_xml, NULL);

	bonobo_ui_component_add_verb_list (ui_component, popup_verbs);

	event_box = gtk_event_box_new ();

	gtk_container_add (GTK_CONTAINER (event_box), widget);

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

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

struct _PanelAppletFramePrivate {
	AppletInfo *applet_info;
};

static GObjectClass *parent_class;

static void
popup_handle_verb (BonoboUIComponent *uic,
		   gpointer           user_data,
		   const gchar       *verbname)
{
#ifdef PANEL_APPLET_FRAME_DEBUG
        g_message ("Verb %s invoked\n", verbname);
#endif
}

static void
popup_handle_remove (BonoboUIComponent *uic,
		     PanelAppletFrame  *frame,
		     const gchar       *verbname)
{
	panel_applet_clean (frame->priv->applet_info);
}

static BonoboUIVerb popup_verbs [] = {
        BONOBO_UI_UNSAFE_VERB ("RemoveAppletFromPanel", popup_handle_remove),
        BONOBO_UI_UNSAFE_VERB ("MoveApplet", popup_handle_verb),

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

void
panel_bonobo_applet_load (const gchar *iid,
			  PanelWidget *panel,
			  gint         pos)
{
	GtkWidget  *frame;
	AppletInfo *info;

	frame = panel_applet_frame_new (iid);

	gtk_widget_show_all (frame);

	info = panel_applet_register (frame, 
				      NULL,     /* FIXME: data */
				      NULL,     /* FIXME: data_destroy */
				      panel,
				      pos,
				      FALSE,
				      APPLET_BONOBO);

	if (!info)
		g_warning ("Cannot register control widget\n");

	panel_applet_frame_set_info (PANEL_APPLET_FRAME (frame), info);
}

void
panel_applet_frame_set_info (PanelAppletFrame *frame,
			     AppletInfo       *info)
{
	frame->priv->applet_info = info;

}

static void
panel_applet_frame_finalize (GObject *object)
{
	PanelAppletFrame *frame = PANEL_APPLET_FRAME (object);

        g_free (frame->priv);
        frame->priv = NULL;

        parent_class->finalize (object);
}

static void
panel_applet_frame_class_init (PanelAppletFrameClass *klass,
			       gpointer               dummy)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = panel_applet_frame_finalize;
}

static void
panel_applet_frame_instance_init (PanelAppletFrame      *frame,
				  PanelAppletFrameClass *klass)
{
	frame->priv = g_new0 (PanelAppletFramePrivate, 1);

	frame->priv->applet_info = NULL;
}

GType
panel_applet_frame_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelAppletFrameClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_applet_frame_class_init,
			NULL,
			NULL,
			sizeof (PanelAppletFrame),
			0,
			(GInstanceInitFunc) panel_applet_frame_instance_init,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_EVENT_BOX,
					       "PanelAppletFrame",
					       &info, 0);
	}

	return type;
}

void
panel_applet_frame_construct (PanelAppletFrame  *frame,
			      const gchar       *iid)
{
	BonoboControlFrame *control_frame;
	Bonobo_Control      control;
	BonoboUIComponent  *ui_component;
	GtkWidget          *widget;

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

        bonobo_ui_component_add_verb_list_with_data (ui_component, popup_verbs, frame);

        gtk_container_add (GTK_CONTAINER (frame), widget);
}

GtkWidget *
panel_applet_frame_new (const gchar *iid)
{
	PanelAppletFrame *frame;

	frame = g_object_new (PANEL_TYPE_APPLET_FRAME, NULL);

	panel_applet_frame_construct (frame, iid);

	return GTK_WIDGET (frame);
}

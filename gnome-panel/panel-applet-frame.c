/*
 * panel-applet-frame.c:
 *
 * Authors:
 *    Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2001 Sun Microsystems, Inc.
 */

#include <libbonoboui.h>
#include <gconf/gconf.h>

#include "panel-applet-frame.h"
#include "applet.h"

#undef PANEL_APPLET_FRAME_DEBUG

struct _PanelAppletFramePrivate {
	GNOME_PanelAppletShell  applet_shell;
	Bonobo_PropertyBag      property_bag;

	AppletInfo             *applet_info;

	gchar                  *iid;
	gchar                  *unique_key;
};

static GObjectClass *parent_class;

static void
popup_handle_remove (BonoboUIComponent *uic,
		     PanelAppletFrame  *frame,
		     const gchar       *verbname)
{
	g_return_if_fail (frame && PANEL_IS_APPLET_FRAME (frame));

	panel_applet_clean (frame->priv->applet_info);
}

static void
popup_handle_move (BonoboUIComponent *uic,
		   PanelAppletFrame  *frame,
		   const gchar       *verbname)
{
	PanelWidget *panel;
	GtkWidget   *widget;

	g_return_if_fail (frame && GTK_IS_WIDGET (frame));

	widget = GTK_WIDGET (frame);

	g_return_if_fail (PANEL_IS_WIDGET (widget->parent));

	panel = PANEL_WIDGET (widget->parent);

	panel_widget_applet_drag_start (panel, widget, PW_DRAG_OFF_CENTER);
}

static BonoboUIVerb popup_verbs [] = {
        BONOBO_UI_UNSAFE_VERB ("RemoveAppletFromPanel", popup_handle_remove),
        BONOBO_UI_UNSAFE_VERB ("MoveApplet",            popup_handle_move),

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
panel_applet_frame_load (const gchar *iid,
			 PanelWidget *panel,
			 gint         pos)
{
	GtkWidget  *frame;
	AppletInfo *info;

	frame = panel_applet_frame_new (iid);

	gtk_widget_show_all (frame);

	info = panel_applet_register (frame, 
				      frame,     /* FIXME: ref? */
				      NULL,      /* FIXME: data_destroy */
				      panel,
				      pos,
				      FALSE,
				      APPLET_BONOBO);

	if (!info)
		g_warning (_("Cannot register control widget\n"));

	panel_applet_frame_set_info (PANEL_APPLET_FRAME (frame), info);
}

void
panel_applet_frame_save_position (PanelAppletFrame *frame,
				  const gchar      *base_key)
{
	/*
	 * FIXME: implement
	 */
}

void
panel_applet_frame_save_session (PanelAppletFrame *frame,
				 const gchar      *base_key)
{
	CORBA_Environment  env;
	gchar             *global_key;
	gchar             *private_key;

	CORBA_exception_init (&env);

	global_key  = g_strdup_printf ("%s/%s", base_key, frame->priv->iid);
	private_key = g_strdup_printf ("%s/%s", base_key, frame->priv->unique_key);

	GNOME_PanelAppletShell_saveYourself (frame->priv->applet_shell,
					     global_key,
					     private_key,
					     &env); 
	if (BONOBO_EX (&env))
		g_warning (G_STRLOC " : exception return from saveYourself '%s'",
			   BONOBO_EX_REPOID (&env));

	g_free (global_key);
	g_free (private_key);

	CORBA_exception_free (&env);
}

void
panel_applet_frame_change_orient (PanelAppletFrame *frame,
				  PanelOrient       orient)
{
	bonobo_pbclient_set_short (frame->priv->property_bag, 
				   "panel-applet-orient",
				   orient,
				   NULL);
}

void
panel_applet_frame_change_size (PanelAppletFrame *frame,
				PanelSize         size)
{
	bonobo_pbclient_set_short (frame->priv->property_bag, 
				   "panel-applet-size",
				   size,
				   NULL);
}

void
panel_applet_frame_change_background_pixmap (PanelAppletFrame *frame,
					     gchar            *pixmap)
{
	gchar *bg_str;

	bg_str = g_strdup_printf ("pixmap:%s", pixmap);

	bonobo_pbclient_set_string (frame->priv->property_bag, 
				    "panel-applet-background",
				    bg_str,
				    NULL);

	g_free (bg_str);
}

void
panel_applet_frame_change_background_color (PanelAppletFrame *frame,
					    guint16           red,
					    guint16           green,
					    guint16           blue)
{
	gchar *bg_str;

	bg_str = g_strdup_printf ("color:#%.2x%.2x%.2x", red, green, blue);

	bonobo_pbclient_set_string (frame->priv->property_bag, 
				    "panel-applet-background",
				    bg_str,
				    NULL);

	g_free (bg_str);
}

void
panel_applet_frame_clear_background (PanelAppletFrame *frame)
{
	gchar *bg_str = "none:";

	bonobo_pbclient_set_string (frame->priv->property_bag, 
				    "panel-applet-background",
				    bg_str,
				    NULL);
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

	g_free (frame->priv->iid);
	g_free (frame->priv->unique_key);

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

	frame->priv->applet_shell = CORBA_OBJECT_NIL;
	frame->priv->property_bag = CORBA_OBJECT_NIL;
	frame->priv->applet_info  = NULL;
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

static GNOME_PanelAppletShell
panel_applet_frame_get_applet_shell (Bonobo_Control control)
{
	CORBA_Environment      env;
	GNOME_PanelAppletShell retval;

	CORBA_exception_init (&env);

	retval = Bonobo_Unknown_queryInterface (control, 
						"IDL:GNOME/PanelAppletShell:1.0",
						&env);
	if (BONOBO_EX (&env)) {
		g_warning (_("Unable to obtain AppletShell interface from control\n"));

		retval = CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&env);

	return retval;
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
	if (!widget) {
		g_warning (G_STRLOC ": failed to load %s", iid);
		return;
	}

	frame->priv->iid = g_strdup (iid);

	frame->priv->unique_key = gconf_unique_key ();

        control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (widget));

        control = bonobo_control_frame_get_control (control_frame);

	frame->priv->applet_shell = panel_applet_frame_get_applet_shell (control);

	frame->priv->property_bag = 
		bonobo_control_frame_get_control_property_bag (control_frame, NULL);

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

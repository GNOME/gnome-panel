/*
 * panel-applet-frame.c:
 *
 * Copyright (C) 2001 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>
#include <string.h>

#include <libbonoboui.h>
#include <gconf/gconf.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "panel-applet-frame.h"
#include "panel-gconf.h"
#include "session.h"
#include "applet.h"

#undef PANEL_APPLET_FRAME_DEBUG

struct _PanelAppletFramePrivate {
	GNOME_Vertigo_PanelAppletShell  applet_shell;
	Bonobo_PropertyBag              property_bag;

	AppletInfo                     *applet_info;

	gchar                          *iid;
};

static GObjectClass *parent_class;

void
panel_applet_frame_save_to_gconf (PanelAppletFrame *frame,
				  const char       *gconf_key)
{
	GConfClient *client;
	char        *profile;
	char        *temp_key;

	client  = panel_gconf_get_client ();
	profile = session_get_current_profile ();

	temp_key = panel_gconf_applets_profile_get_full_key (profile, gconf_key, "bonobo-iid");
	gconf_client_set_string (client, temp_key, frame->priv->iid, NULL);
	g_free (temp_key);
	g_free (profile);
}

void
panel_applet_frame_load_from_gconf (PanelWidget *panel_widget,
				    gint         position,
				    const char  *gconf_key,
				    gboolean     use_default)
{
	GConfClient *client;
	char        *temp_key;
	char        *applet_iid;

	g_return_if_fail (panel_widget);
	g_return_if_fail (gconf_key);

	client  = panel_gconf_get_client ();

	temp_key = use_default ? panel_gconf_applets_default_profile_get_full_key ("medium", gconf_key, "bonobo-iid") :
		panel_gconf_applets_profile_get_full_key (session_get_current_profile (), gconf_key, "bonobo-iid");
	applet_iid = gconf_client_get_string (client, temp_key, NULL);
	g_free (temp_key);

	panel_applet_frame_load (applet_iid, panel_widget, position, gconf_key);

	g_free (applet_iid);
}

static void
popup_handle_remove (BonoboUIComponent *uic,
		     PanelAppletFrame  *frame,
		     const gchar       *verbname)
{
	AppletInfo *info = frame->priv->applet_info;
	frame->priv->applet_info = NULL;
	panel_applet_clean (info);
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
			 gint         pos,
			 const char  *gconf_key)
{
	GtkWidget  *frame = NULL;
	AppletInfo *info;
	char       *real_key;

	g_return_if_fail (iid);
	g_return_if_fail (panel);

	if (gconf_key)
		real_key = g_strdup (gconf_key);
	else
		real_key = gconf_unique_key ();

	frame = panel_applet_frame_new (iid, real_key);

	if (!frame) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL,
						 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("There was a problem loading the applet."));
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		
		return;
	}
	
	gtk_widget_show_all (frame);

	info = panel_applet_register (frame, 
				      NULL /* data */,
				      NULL /* data_destroy */,
				      panel,
				      pos,
				      FALSE,
				      APPLET_BONOBO,
				      real_key);

	if (!info)
		g_warning (_("Cannot register control widget\n"));

	g_free (real_key);

	panel_applet_frame_set_info (PANEL_APPLET_FRAME (frame), info);
}

void
panel_applet_frame_get_expand_flags (PanelAppletFrame *frame,
				     gboolean         *expand_major,
				     gboolean         *expand_minor)
{
	CORBA_Environment  env;
	CORBA_boolean major = 0, minor = 0;

	CORBA_exception_init (&env);

	GNOME_Vertigo_PanelAppletShell_getExpandFlags (frame->priv->applet_shell,
						       &major, &minor,
						       &env);
	
	if (BONOBO_EX (&env))
		g_warning (G_STRLOC " : exception return from getExpandFlags '%s'",
			   BONOBO_EX_REPOID (&env));
	else {
		*expand_major = major;
		*expand_minor = minor;
	}
	
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
panel_applet_frame_change_background_pixmap (PanelAppletFrame *frame)
{
	GdkNativeWindow  pixmap_xid;
	PanelWidget     *panel_widget;
	gchar           *bg_str;

	g_return_if_fail (PANEL_IS_APPLET_FRAME (frame));
	g_return_if_fail (PANEL_IS_WIDGET (GTK_WIDGET (frame)->parent));

	panel_widget = PANEL_WIDGET (GTK_WIDGET (frame)->parent);

	pixmap_xid = gdk_x11_drawable_get_xid (GDK_DRAWABLE (GTK_WIDGET (panel_widget)->window));

	bg_str = g_strdup_printf ("pixmap:%d,%d,%d",
				  pixmap_xid,
				  GTK_WIDGET (frame)->allocation.x,
				  GTK_WIDGET (frame)->allocation.y);

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

	bg_str = g_strdup_printf ("color:#%.4x%.4x%.4x", red, green, blue);

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
	frame->priv->iid = NULL;

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

static GNOME_Vertigo_PanelAppletShell
panel_applet_frame_get_applet_shell (Bonobo_Control control)
{
	CORBA_Environment              env;
	GNOME_Vertigo_PanelAppletShell retval;

	CORBA_exception_init (&env);

	retval = Bonobo_Unknown_queryInterface (control, 
						"IDL:GNOME/Vertigo/PanelAppletShell:1.0",
						&env);
	if (BONOBO_EX (&env)) {
		g_warning (_("Unable to obtain AppletShell interface from control\n"));

		retval = CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&env);

	return retval;
}

GtkWidget *
panel_applet_frame_construct (PanelAppletFrame *frame,
			      const char       *iid,
			      const char       *gconf_key)
{
	BonoboControlFrame *control_frame;
	Bonobo_Control      control;
	BonoboUIComponent  *ui_component;
	GtkWidget          *widget;
	gchar              *moniker;

	moniker = g_strdup_printf ("%s!prefs_key=/apps/panel/profiles/%s/applets/%s/prefs", 
				   iid,
				   session_get_current_profile (),
				   gconf_key);

        widget = bonobo_widget_new_control (moniker, NULL);

	g_free (moniker);

	if (!widget) {
		g_warning (G_STRLOC ": failed to load %s", iid);
		return NULL;
	}

	frame->priv->iid = g_strdup (iid);

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

	return widget;
}

GtkWidget *
panel_applet_frame_new (const char *iid,
			const char *gconf_key)
{
	PanelAppletFrame *frame;

	g_return_val_if_fail (iid && gconf_key, NULL);

	frame = g_object_new (PANEL_TYPE_APPLET_FRAME, NULL);

	if (!panel_applet_frame_construct (frame, iid, gconf_key)) {
		g_object_unref (frame);
		return NULL;
	}

	return GTK_WIDGET (frame);
}

/*
 * panel-applet.c:
 *
 * Authors:
 *    Mark McLoughlin <mark@skynet.ie>
 *
 * Copyright 2001 Sun Microsystems, Inc.
 */

#include <unistd.h>
#include <bonobo/bonobo-ui-util.h>

#include "panel-applet.h"

struct _PanelAppletPrivate {
	BonoboControl *control;
};

static GObjectClass *parent_class;

void
panel_applet_setup_menu (PanelApplet        *applet,
			 const gchar        *xml,
			 const BonoboUIVerb *verb_list,
			 gpointer            user_data)
{
	BonoboUIComponent *popup_component;

	g_return_if_fail (applet && xml && verb_list);

	popup_component = panel_applet_get_popup_component (applet);

	bonobo_ui_component_set (popup_component, "/",
				 "<popups><popup name=\"button3\"/></popups>", NULL);

	bonobo_ui_component_set_translate (popup_component, "/", xml, NULL);

	bonobo_ui_component_add_verb_list_with_data (popup_component, verb_list, user_data);
}

void
panel_applet_setup_menu_from_file (PanelApplet        *applet, 
				   const gchar        *opt_datadir,
				   const gchar        *file,
				   const gchar        *opt_app_name,
				   const BonoboUIVerb *verb_list,
				   gpointer            user_data)
{
	BonoboUIComponent *popup_component;
	gchar             *app_name = NULL;

	g_return_if_fail (applet && file && verb_list);

	if (!opt_datadir)
		opt_datadir = GNOME_DATADIR;

	if (!opt_app_name)
		opt_app_name = app_name = g_strdup_printf ("%d", getpid ());

	popup_component = panel_applet_get_popup_component (applet);

	bonobo_ui_util_set_ui (popup_component, opt_datadir, file, opt_app_name, NULL);

	bonobo_ui_component_add_verb_list_with_data (popup_component, verb_list, user_data);

	if (app_name)
		g_free (app_name);
}

BonoboControl *
panel_applet_get_control (PanelApplet *applet)
{
	g_return_val_if_fail (applet && PANEL_IS_APPLET (applet), NULL);

	return applet->priv->control;
}

BonoboUIComponent *
panel_applet_get_popup_component (PanelApplet *applet)
{
	g_return_val_if_fail (applet && PANEL_IS_APPLET (applet), NULL);

	return bonobo_control_get_popup_ui_component (applet->priv->control);
}

static void
panel_applet_finalize (GObject *object)
{
	PanelApplet *applet = PANEL_APPLET (object);
	
	g_free (applet->priv);
	applet->priv = NULL;

	parent_class->finalize (object);
}

static gboolean
panel_applet_button_press (GtkWidget      *widget,
			   GdkEventButton *event)
{
	PanelApplet *applet = PANEL_APPLET (widget);

	if (event->button == 3)
		bonobo_control_do_popup (applet->priv->control,
					 event->button,
					 event->time);

	return FALSE;
}

static void
panel_applet_class_init (PanelAppletClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	widget_class->button_press_event = panel_applet_button_press;

	gobject_class->finalize = panel_applet_finalize;
}

static void
panel_applet_instance_init (PanelApplet *applet)
{
	applet->priv = g_new0 (PanelAppletPrivate, 1);

	gtk_widget_set_events (GTK_WIDGET (applet), 
			       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
}

GtkType
panel_applet_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info = {
			"PanelApplet",
			sizeof (PanelApplet),
			sizeof (PanelAppletClass),
			(GtkClassInitFunc) panel_applet_class_init,
			(GtkObjectInitFunc) panel_applet_instance_init,
			NULL,
			NULL,
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (GTK_TYPE_EVENT_BOX, &info);
	}

	return type;
}

void
panel_applet_construct (PanelApplet *applet, 
			GtkWidget   *widget)
{
	PanelAppletPrivate *priv;

	priv = applet->priv;

	gtk_container_add (GTK_CONTAINER (applet), widget);

	gtk_widget_show_all (GTK_WIDGET (applet));

	priv->control = bonobo_control_new (GTK_WIDGET (applet));
}

PanelApplet *
panel_applet_new (GtkWidget *widget)
{
	PanelApplet *applet;

	applet = gtk_type_new (PANEL_TYPE_APPLET);

	panel_applet_construct (applet, widget);

	return applet;
}

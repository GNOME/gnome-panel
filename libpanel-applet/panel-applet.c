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
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>

#include "panel-applet.h"
#include "panel-applet-private.h"
#include "panel-applet-shell.h"
#include "panel-marshal.h"
#include "panel-typebuiltins.h"

struct _PanelAppletPrivate {
	PanelAppletShell  *shell;

	BonoboControl     *control;
	PanelAppletOrient  orient;
};

static GObjectClass *parent_class;

enum {
        CHANGE_ORIENT,
        LAST_SIGNAL
};

static guint panel_applet_signals [LAST_SIGNAL];

void
panel_applet_change_orient (PanelApplet       *applet,
			    PanelAppletOrient  orient)
{
	if (applet->priv->orient != orient) {
		applet->priv->orient = orient;

		g_signal_emit (G_OBJECT (applet),
			       panel_applet_signals [CHANGE_ORIENT],
			       0, orient);
	}
}

void
panel_applet_setup_menu (PanelApplet        *applet,
			 const gchar        *xml,
			 const BonoboUIVerb *verb_list,
			 gpointer            user_data)
{
	BonoboUIComponent *popup_component;

	g_return_if_fail (applet && xml && verb_list);

	popup_component = panel_applet_get_popup_component (applet);

	bonobo_ui_component_set (popup_component, "/", "<popups/>", NULL);

	bonobo_ui_component_set_translate (popup_component, "/popups", xml, NULL);

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
panel_applet_class_init (PanelAppletClass *klass,
			 gpointer          dummy)
{
	GObjectClass   *gobject_class = (GObjectClass *) klass;
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	widget_class->button_press_event = panel_applet_button_press;

	gobject_class->finalize = panel_applet_finalize;

	panel_applet_signals [CHANGE_ORIENT] =
                g_signal_new ("change_orient",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelAppletClass, change_orient),
                              NULL,
			      NULL,
                              panel_marshal_VOID__ENUM,
                              G_TYPE_NONE,
			      1,
			      PANEL_TYPE_G_NOME__PANEL_ORIENT);
}

static void
panel_applet_instance_init (PanelApplet      *applet,
			    PanelAppletClass *klass)
{
	applet->priv = g_new0 (PanelAppletPrivate, 1);

	applet->priv->orient = PANEL_APPLET_ORIENT_UP;

	gtk_widget_set_events (GTK_WIDGET (applet), 
			       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
}

GType
panel_applet_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelAppletClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_applet_class_init,
			NULL,
			NULL,
			sizeof (PanelApplet),
			0,
			(GInstanceInitFunc) panel_applet_instance_init,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_EVENT_BOX, "PanelApplet",
					       &info, 0);
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

	priv->shell = panel_applet_shell_new (applet);

	bonobo_object_add_interface (BONOBO_OBJECT (priv->control),
				     BONOBO_OBJECT (priv->shell));
}

PanelApplet *
panel_applet_new (GtkWidget *widget)
{
	PanelApplet *applet;

	applet = g_object_new (PANEL_TYPE_APPLET, NULL);

	panel_applet_construct (applet, widget);

	return applet;
}

int
panel_applet_factory_main (int                     argc,
			   char                  **argv,
			   const gchar            *iid,
			   const gchar            *name,
			   const gchar            *version,
			   BonoboFactoryCallback   callback,
			   gpointer                data)
{
	GnomeProgram *program;
	int           retval;

	program = gnome_program_init (name, version,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_NONE);

	retval = bonobo_generic_factory_main (iid, callback, data);

	return retval;
}

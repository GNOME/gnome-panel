#include <gtk/gtk.h>
#include <gnome.h>
#include "applet-widget.h"

static void applet_widget_class_init	(AppletWidgetClass *klass);
static void applet_widget_init		(AppletWidget      *applet_widget);

static GdkCursor *fleur_cursor;

guint
applet_widget_get_type ()
{
	static guint applet_widget_type = 0;

	if (!applet_widget_type) {
		GtkTypeInfo applet_widget_info = {
			"AppletWidget",
			sizeof (AppletWidget),
			sizeof (AppletWidgetClass),
			(GtkClassInitFunc) applet_widget_class_init,
			(GtkObjectInitFunc) applet_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		applet_widget_type = gtk_type_unique (gtk_window_get_type (),
						      &applet_widget_info);
	}

	return applet_widget_type;
}

static void
applet_widget_class_init (AppletWidgetClass *class)
{
}

static void
applet_widget_init (AppletWidget *applet_widget)
{
	applet_widget->eb = gtk_event_box_new();
	gtk_widget_show(applet_widget->eb);

	GTK_WINDOW(applet_widget)->type = GTK_WINDOW_POPUP;

	gtk_container_add(GTK_CONTAINER(applet_widget),applet_widget->eb);
	gtk_window_set_policy (GTK_WINDOW (applet_widget), 1, 1, 1);

	if(!fleur_cursor)
		fleur_cursor = gdk_cursor_new(GDK_FLEUR);
}

GtkWidget*
applet_widget_new (void)
{
	AppletWidget *applet;

	applet = gtk_type_new(applet_widget_get_type());

	return GTK_WIDGET(applet);
}

void
applet_widget_add (AppletWidget *applet, GtkWidget *widget)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(widget != NULL);

	gtk_container_add(GTK_CONTAINER(applet->eb),widget);
}

void
applet_widget_remove (AppletWidget *applet, GtkWidget *widget)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(widget != NULL);

	gtk_container_remove(GTK_CONTAINER(applet->eb),widget);
}

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

void
applet_widget_move_grab_add (AppletWidget *applet)
{
	gtk_grab_add(GTK_WIDGET(applet));
	gdk_pointer_grab(GTK_WIDGET(applet)->window,
			 TRUE,
			 APPLET_EVENT_MASK,
			 NULL,
			 fleur_cursor,
			 GDK_CURRENT_TIME);
}

void
applet_widget_move_grab_remove (AppletWidget *applet)
{
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gtk_grab_remove(GTK_WIDGET(applet));
}

/* logout.c - Panel applet to end current session.  */
/* Original author unknown. CORBAized by Elliot Lee */

#include "gnome.h"
#include "applet-lib.h"
#include "applet-widget.h"

GtkWidget *applet;

static gint
destroy_applet(GtkWidget *widget, gpointer data)
{
	gtk_exit(0);
	return FALSE;
}

static void
logout(void)
{
	gnome_panel_quit();
}

static GtkWidget *
create_logout_widget(void)
{
	GtkWidget *button;
	GtkWidget *pixmap;
	char *pixmap_name;

	pixmap_name = gnome_unconditional_pixmap_file("gnome-term-night.png");

	if (!pixmap_name)
		button = gtk_button_new_with_label(_("Log out"));
	else {
		button = gtk_button_new();
		pixmap = gnome_pixmap_new_from_file(pixmap_name);
		g_free(pixmap_name);
		gtk_container_add(GTK_CONTAINER(button), pixmap);
		gtk_widget_show(pixmap);
		gtk_widget_set_usize(button, pixmap->requisition.width,
				     pixmap->requisition.height);
	}

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(logout), NULL);

	return button;
}

int
main(int argc, char *argv[])
{
	GtkWidget *logout;

	panel_corba_register_arguments();

	gnome_init("logout_applet", NULL, argc, argv, 0, NULL);

	applet = applet_widget_new(argv[0]);
	if (!applet)
		g_error("Can't create applet!\n");

	logout = create_logout_widget();
	gtk_widget_show(logout);
	applet_widget_add(APPLET_WIDGET(applet), logout);
	gtk_widget_show(applet);
	gtk_signal_connect(GTK_OBJECT(applet),"destroy",
			   GTK_SIGNAL_FUNC(destroy_applet),
			   NULL);

	applet_widget_gtk_main();

	return 0;
}

/* logout.c - Panel applet to end current session.  */
/* Original author unknown. CORBAized by Elliot Lee */
/* uncorbized by George Lebl */

#include <gnome.h>
#include "panel.h"

static void
logout(void)
{
	panel_quit();
}

GtkWidget *
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
		gtk_widget_set_usize(button, 48, 48);
		/*gtk_widget_set_usize(button, pixmap->requisition.width,
				     pixmap->requisition.height);*/
	}

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(logout), NULL);

	return button;
}

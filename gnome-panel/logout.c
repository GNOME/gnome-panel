/* logout.c - Panel applet to end current session.  */
/* Original author unknown. CORBAized by Elliot Lee */
/* uncorbized by George Lebl */

#include <config.h>
#include <gnome.h>

#include "panel-include.h"

extern GtkTooltips *panel_tooltips;

static void
logout(void)
{
	panel_quit();
}

static GtkWidget *
create_logout_widget(void)
{
	GtkWidget *button;
	GtkWidget *pixmap;
	char *pixmap_name;

	pixmap_name = gnome_pixmap_file("gnome-term-night.png");

	button = button_widget_new_from_file(pixmap_name,
					     LOGOUT_TILE,
					     FALSE,
					     ORIENT_UP,
					     _("Log out"));
	g_free(pixmap_name);
	gtk_tooltips_set_tip (panel_tooltips,button,_("Log out of GNOME"),NULL);

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(logout), NULL);

	return button;
}

void
load_logout_applet(PanelWidget *panel, int pos)
{
	GtkWidget *logout;

	logout = create_logout_widget();

	if(logout)
		register_toy(logout, NULL, panel, pos, APPLET_LOGOUT);
}


/* logout.c - Panel applet to end current session.  */
/* Original author unknown. CORBAized by Elliot Lee */
/* uncorbized by George Lebl */

#include <config.h>
#include <gnome.h>

#include "panel-include.h"

extern GtkTooltips *panel_tooltips;

extern GlobalConfig global_config;

static void
logout(GtkWidget *widget)
{
	if(global_config.drawer_auto_close) {
		GtkWidget *parent =
			gtk_object_get_data(GTK_OBJECT(widget->parent),PANEL_PARENT);
		g_return_if_fail(parent!=NULL);
		if(IS_DRAWER_WIDGET(parent)) {
			BasePWidget *basep = BASEP_WIDGET(parent);
			GtkWidget *grandparent = PANEL_WIDGET(basep->panel)->master_widget->parent;
			GtkWidget *grandparentw = gtk_object_get_data(GTK_OBJECT(grandparent),
								      PANEL_PARENT);

			drawer_widget_close_drawer(DRAWER_WIDGET(parent));
			if(IS_SNAPPED_WIDGET(grandparentw))
				SNAPPED_WIDGET(grandparentw)->drawers_open--;
			else if(IS_CORNER_WIDGET(grandparentw))
		                CORNER_WIDGET(grandparentw)->drawers_open--;
		}
	}

	panel_quit();
}

static GtkWidget *
create_logout_widget(void)
{
	GtkWidget *button;
	GtkWidget *pixmap;
	char *pixmap_name;

	pixmap_name = gnome_pixmap_file("gnome-term-night.png");

	button = button_widget_new_from_file(pixmap_name,-1,
					     MISC_TILE,
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

static GtkWidget *
create_lock_widget(void)
{
	GtkWidget *button;
	GtkWidget *pixmap;
	char *pixmap_name;

	pixmap_name = gnome_pixmap_file("gnome-lockscreen.png");

	button = button_widget_new_from_file(pixmap_name,-1,
					     MISC_TILE,
					     FALSE,
					     ORIENT_UP,
					     _("Lock screen"));
	g_free(pixmap_name);
	gtk_tooltips_set_tip (panel_tooltips,button,_("Lock screen"),NULL);

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(panel_lock), NULL);

	return button;
}

void
load_lock_applet(PanelWidget *panel, int pos)
{
	GtkWidget *lock;

	lock = create_lock_widget();

	if(lock)
		register_toy(lock, NULL, panel, pos, APPLET_LOCK);
}


/* logout.c - Panel applet to end current session.  */
/* Original author unknown. CORBAized by Elliot Lee */

#include "gnome.h"
#include "applet-lib.h"
#include "panel.h"
#include "mico-parse.h"

GtkWidget *plug;
int applet_id = (-1);

void
change_orient(int id, int orient)
{
	/*PanelOrientType o = (PanelOrientType) orient;*/
}

void
session_save(int id, const char *cfgpath, const char *globcfgpath)
{
}

static gint
destroy_plug(GtkWidget *widget, gpointer data)
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

	pixmap_name = gnome_unconditional_pixmap_file("gnome-term-night.xpm");

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
	char *result;
	char *cfgpath;
	char *globcfgpath;
	guint32 winid;
	char *myinvoc;

	myinvoc = get_full_path(argv[0]);
	if(!myinvoc)
		return 1;

	panel_corba_register_arguments();

	gnome_init("logout_applet", NULL, argc, argv, 0, NULL);

	if (!gnome_panel_applet_init_corba())
		g_error("Could not communicate with the panel\n");

	result = gnome_panel_applet_request_id(myinvoc, &applet_id,
					       &cfgpath, &globcfgpath,
					       &winid);
	if (result)
		g_error("Could not talk to the panel: %s\n", result);

	g_free(myinvoc);

	g_free(cfgpath);	/* We should load up config data first... */
	g_free(globcfgpath);

	plug = gtk_plug_new(winid);

	logout = create_logout_widget();
	gtk_widget_show(logout);
	gtk_container_add(GTK_CONTAINER(plug), logout);
	gtk_widget_show(plug);
	gtk_signal_connect(GTK_OBJECT(plug),"destroy",
			   GTK_SIGNAL_FUNC(destroy_plug),
			   NULL);


	result = gnome_panel_applet_register(plug, applet_id);
	if (result)
		g_error("Could not talk to the Panel: %s\n", result);

	applet_corba_gtk_main("IDL:GNOME/Applet:1.0");

	return 0;
}

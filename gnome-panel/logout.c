/* logout.c - Panel applet to end current session.  */

#include "gnome.h"
#include "applet-lib.h"
#include "applet-widget.h"
#include "panel.h"
#include "mico-parse.h"

#define APPLET_ID "Logout"

GtkWidget *aw;
int applet_id=-1;

static void
logout(void)
{
  g_warning("I don't know how to log out!\n");
}

static GtkWidget *
create_logout_widget (GtkWidget *aw)
{
  GtkWidget *button;
  GtkWidget *pixmap;
  char *pixmap_name;
  
  pixmap_name=gnome_unconditional_pixmap_file("gnome-term-night.xpm");

  if(!pixmap_name)
    button = gtk_button_new_with_label (_("Log out"));
  else {
    button = gtk_button_new ();
    pixmap = gnome_pixmap_new_from_file (pixmap_name);
    g_free(pixmap_name);
    gtk_container_add(GTK_CONTAINER(button),pixmap);
    gtk_widget_show(pixmap);
    gtk_widget_set_usize (button, pixmap->requisition.width,
			  pixmap->requisition.height);
  }

  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (logout), NULL);

  gtk_container_add(GTK_CONTAINER(aw), button);
}

int
main(int argc, char *argv[])
{
  GtkWidget *logout;
  char *result;

  panel_corba_register_arguments ();
  gnome_init("logout_applet", NULL, argc, argv, 0, NULL);

  if(!gnome_panel_applet_init_corba ())
    {
      fprintf(stderr, "Could not communicate with the panel\n");
      exit(1);
    }

  aw = applet_widget_new();

  logout = create_logout_widget(GTK_WIDGET(aw));
  gtk_widget_show(logout);
  applet_widget_add(APPLET_WIDGET(aw), clock);
  gtk_widget_show(aw);
  result = gnome_panel_prepare_and_transfer(aw, argv[0], &applet_id, 0, 0);
  if (result)
    {
      printf ("Could not talk to the Panel: %s\n", result);
      exit (1);
    }
  
  applet_corba_gtk_main ("IDL:GNOME/Applet:1.0");
  
  return 0;
}

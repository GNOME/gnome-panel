/* logout.c - Panel applet to end current session.  */
/* Original author unknown. CORBAized by Elliot Lee */

#include "gnome.h"
#include "applet-lib.h"
#include "applet-widget.h"
#include "panel.h"
#include "mico-parse.h"

#define APPLET_ID "Logout"

GtkWidget *aw;
int applet_id=-1;

void
change_orient(int id, int orient)
{
  PanelOrientType o = (PanelOrientType)orient;
}

void
session_save(int id, const char *cfgpath, const char *globcfgpath)
{
}

static gint
quit_logout(gpointer data)
{
	exit(0);
}


void
shutdown_applet(int id)
{
  gtk_widget_destroy(aw);
  gtk_idle_add(quit_logout,NULL);
}

static void
logout(void)
{
  gnome_panel_quit();
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

  return button;
}

int
main(int argc, char *argv[])
{
  GtkWidget *logout;
  char *result;
  char *cfgpath;
  char *globcfgpath;

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
  applet_widget_add(APPLET_WIDGET(aw), GTK_WIDGET(logout));
  gtk_widget_show(aw);

  {
    char *mypath, *myinvoc;

    if(argv[0][0] == '/')
      myinvoc = g_strdup(argv[0]);
    else
      {
	mypath = getcwd(NULL, 0);
	myinvoc = g_copy_strings(mypath, "/", argv[0], NULL);
	free(mypath);
      }
    
    result = gnome_panel_applet_request_id (GTK_WIDGET(aw),
					    myinvoc, &applet_id,
					    &cfgpath,&globcfgpath);
    if(result)
      g_error("Could not talk to the panel: %s\n", result);

    g_free(myinvoc);
  }

  g_free(cfgpath); /* We should load up config data first... */
  g_free(globcfgpath);

  result = gnome_panel_prepare_and_transfer(aw, applet_id);
  if (result)
    g_error("Could not talk to the Panel: %s\n", result);
  
  applet_corba_gtk_main ("IDL:GNOME/Applet:1.0");
  
  return 0;
}

#ifndef APPLET_LIB_H
#define APPLET_LIB_H

BEGIN_GNOME_DECLS

char *gnome_panel_applet_request_id (GtkWidget *widget, char *path, int *id,
				     char **cfgpath);
char *gnome_panel_prepare_and_transfer (GtkWidget *widget, int id);
int gnome_panel_applet_init_corba (void);
void applet_corba_gtk_main (char *str);

/* This is defined by the applet, not the library */
void do_callback (short id,
		  const char *callback_name);
END_GNOME_DECLS

#endif

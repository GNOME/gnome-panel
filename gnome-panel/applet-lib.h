#ifndef APPLET_LIB_H
#define APPLET_LIB_H

BEGIN_GNOME_DECLS

char *gnome_panel_prepare_and_transfer (GtkWidget *widget, char *path, int *id,
					int panel, int pos);
int gnome_panel_applet_init_corba (int *argc, char ***argv);
void applet_corba_gtk_main (char *str);
END_GNOME_DECLS

#endif

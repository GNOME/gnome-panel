#include "applet-widget.h"
#ifndef APPLET_LIB_H
#define APPLET_LIB_H

BEGIN_GNOME_DECLS

typedef void (*AppletCallbackFunc)(int id, gpointer data);

char *gnome_panel_applet_request_id (GtkWidget *widget, char *path, int *id,
				     char **cfgpath);
char *gnome_panel_prepare_and_transfer (GtkWidget *widget, int id);
int gnome_panel_applet_init_corba (void);
void gnome_panel_applet_register_callback (AppletWidget *aw,
					   int id,
					   char *name,
					   char *menutext,
					   AppletCallbackFunc func,
					   gpointer data);
void applet_corba_gtk_main (char *str);

END_GNOME_DECLS

#endif

#ifndef APPLET_LIB_H
#define APPLET_LIB_H

BEGIN_GNOME_DECLS

typedef void (*AppletCallbackFunc)(int id, gpointer data);

char *gnome_panel_applet_request_id (char *path, int *id,
				     char **cfgpath, char **globcfgpath,
				     guint32 *winid);
char *gnome_panel_applet_register (GtkWidget *widget, int id);
char *gnome_panel_applet_abort_id (int id);
char *gnome_panel_applet_request_glob_cfg (char **globcfgpath);
char *gnome_panel_quit (void);
int gnome_panel_applet_init_corba (void);
void gnome_panel_applet_register_callback (int id,
					   char *name,
					   char *menutext,
					   AppletCallbackFunc func,
					   gpointer data);
void applet_corba_gtk_main (char *str);

void move_grab_add (GtkWidget *applet);
void move_grab_remove (GtkWidget *applet);

END_GNOME_DECLS

#endif

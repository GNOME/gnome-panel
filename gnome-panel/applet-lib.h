#ifndef APPLET_LIB_H
#define APPLET_LIB_H

#include <applet-widget.h>

BEGIN_GNOME_DECLS

#ifndef PANEL_UTIL_H
/*from panel-util.h*/
char *get_full_path(char *argv0);
#endif

/*all the cfgpaths in this interface are load paths (might be an old
  session different from current) ... except the save_session which gets
  the current session stuff (not in this file, but implemented in any
  applet)*/

char *gnome_panel_applet_request_id (const char *path, const char *param,
				     int dorestart,
				     int *applet_id, char **cfgpath,
				     char **globcfgpath, guint32 *winid);
char *gnome_panel_applet_register (GtkWidget *widget, int applet_id);
char *gnome_panel_applet_abort_id (int applet_id);
char *gnome_panel_applet_remove_from_panel (int applet_id);
char *gnome_panel_applet_request_glob_cfg (char **globcfgpath);
char *gnome_panel_quit (void);
int gnome_panel_applet_init_corba (void);
int gnome_panel_applet_reinit_corba (void);
void gnome_panel_applet_register_callback (int applet_id,
					   char *name,
					   char *menutext,
					   AppletCallbackFunc func,
					   gpointer data);
void applet_corba_gtk_main (char *str);

void gnome_panel_applet_cleanup (gint applet_id);

char * gnome_panel_applet_add_tooltip (gint applet_id, char *tooltip);
char * gnome_panel_applet_remove_tooltip (gint applet_id);


END_GNOME_DECLS

#endif

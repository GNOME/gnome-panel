/* applet-lib: low level interface glue to the applet<->panel communication
 * (C) 1998 the Free Software Foundation
 *
 * Author:  George Lebl
 */
#ifndef APPLET_LIB_H
#define APPLET_LIB_H

#include <applet-widget.h>
#include <libgnorba/gnorba.h>

/*this file contains the more private parts of the applet communications
  applets should not use these functions directly, in time applet-lib and
  applet-widget might be merged to avoid code bloat*/

BEGIN_GNOME_DECLS

/*all the cfgpaths in this interface are load paths (might be an old
  session different from current) ... except the save_session which gets
  the current session stuff (not in this file, but implemented in any
  applet)*/

char *gnome_panel_applet_request_id (const char *path, const char *param,
				     int dorestart,
				     int *applet_id, char **cfgpath,
				     char **globcfgpath, guint32 *winid);
char *gnome_panel_applet_register (GtkWidget *widget, int applet_id,
				   const char *goad_id);
char *gnome_panel_applet_abort_id (int applet_id);
char *gnome_panel_applet_remove_from_panel (int applet_id, const char *goad_id);
char *gnome_panel_applet_request_glob_cfg (char **globcfgpath);
int gnome_panel_applet_get_panel_orient (int applet_id);
char *gnome_panel_quit (void);
char *gnome_panel_sync_config (int applet_id);
int gnome_panel_applet_init_corba (CORBA_ORB panel_orb);
int gnome_panel_applet_reinit_corba (CORBA_ORB panel_orb);
void gnome_panel_applet_register_callback (int applet_id,
					   char *name,
					   char *stock_item,
					   char *menutext,
					   AppletCallbackFunc func,
					   gpointer data);
void gnome_panel_applet_unregister_callback(int applet_id, char *name);
void gnome_panel_applet_register_callback_dir (int applet_id,
					       char *name,
					       char *stock_item,
					       char *menutext);
void gnome_panel_applet_unregister_callback_dir(int applet_id, char *name);


void applet_corba_gtk_main (char *str);
void applet_corba_gtk_main_quit (void);

void gnome_panel_applet_cleanup (int applet_id);

char * gnome_panel_applet_add_tooltip (int applet_id, char *tooltip);
char * gnome_panel_applet_remove_tooltip (int applet_id);

/*these functions need to be implemented in the applet, applet-widget
  takes care of them*/
void _gnome_applet_change_orient(int applet_id, int orient);
int _gnome_applet_session_save(int applet_id, const char *cfgpath,
			       const char *globcfgpath);
void _gnome_applet_start_new_applet(const char *params);
void _gnome_applet_back_change(int applet_id, int back_type,
			       const char *pixmap, GdkColor *color);
void _gnome_applet_tooltips_state(int applet_id, int enabled);


/*this is currently not used, it's an empty function for now, but it
  should register the orbit arguments*/
void panel_corba_register_arguments (void);

END_GNOME_DECLS

#endif

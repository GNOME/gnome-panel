/* mico-glue.h - Glue connecting MICO to C.  */

#ifndef __MICO_GLUE_H__
#define __MICO_GLUE_H__

BEGIN_GNOME_DECLS	

void panel_corba_gtk_main (char *service_name);

int panel_corba_call_launcher(const char *path);

void send_applet_session_save (const char *ior, int id,
			       const char *cfgpath,
			       const char *globcfgpath);
void send_applet_shutdown_applet (const char *ior, int id);
void send_applet_change_orient (const char *ior, int id,  int orient);
void send_applet_do_callback (const char *ior, int id, char *callback_name);

END_GNOME_DECLS

#endif /* __MICO_GLUE_H__ */

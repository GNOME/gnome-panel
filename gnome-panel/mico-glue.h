
BEGIN_GNOME_DECLS	
void corba_gtk_main (int *argc, char ***argv, char *service_name, void *corba_object);
void panel_corba_gtk_main (int *argc, char ***argv, char *service_name);

void send_applet_session_save (const char *ior, int id, int panel, int pos);
void send_applet_shutdown_applet (const char *ior, int id);
void send_applet_change_orient (const char *ior, int id,  int orient);
void send_applet_do_callback (const char *ior, int id, char *callback_name);
END_GNOME_DECLS

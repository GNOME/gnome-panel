
BEGIN_GNOME_DECLS	
void corba_gtk_main (int *argc, char ***argv, char *service_name, void *corba_object);
void panel_corba_gtk_main (int *argc, char ***argv, char *service_name);

void send_applet_session_save (const char *ior, int panel, int pos);
void send_applet_shutdown_applet (const char *ior);
void send_applet_change_orient (const char *ior, int orient);
END_GNOME_DECLS

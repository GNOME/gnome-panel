/* mico-glue.h - Glue connecting MICO to C.  */

#ifndef __MICO_GLUE_H__
#define __MICO_GLUE_H__

#include "panel-widget.h"

BEGIN_GNOME_DECLS	

void panel_corba_gtk_main (char *service_name);

void panel_corba_clean_up(void);

int send_applet_session_save (const char *ior, int applet_id,
			       const char *cfgpath,
			       const char *globcfgpath);
void send_applet_change_orient (const char *ior, int applet_id,  int orient);
void send_applet_do_callback (const char *ior, int applet_id,
			       const char *callback_name);
void send_applet_start_new_applet (const char *ior, const char *param);
void send_applet_change_back (const char *ior, int applet_id,
			      PanelBackType back_type, const char *pixmap,
			      const GdkColor* color);
void send_applet_tooltips_state (const char *ior, int enabled);

END_GNOME_DECLS

#endif /* __MICO_GLUE_H__ */

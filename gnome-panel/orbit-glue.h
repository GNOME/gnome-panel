/* corba-glue.h - Glue connecting CORBA to panel.  */

#ifndef __ORBIT_GLUE_H__
#define __ORBIT_GLUE_H__

#include <libgnorba/gnorba.h>
#include "panel-widget.h"

BEGIN_GNOME_DECLS	

void panel_corba_gtk_init(CORBA_ORB panel_orb);
void panel_corba_gtk_main (char *service_name);
void panel_corba_gtk_main_quit(void);

void panel_corba_clean_up(void);

int send_applet_session_save (CORBA_Object appl, int applet_id,
			       const char *cfgpath,
			       const char *globcfgpath);
void send_applet_change_orient (CORBA_Object appl, int applet_id,  int orient);
void send_applet_do_callback (CORBA_Object appl, int applet_id,
			       const char *callback_name);
void send_applet_start_new_applet (CORBA_Object appl, const char *param);
void send_applet_change_back (CORBA_Object appl, int applet_id,
			      PanelBackType back_type, const char *pixmap,
			      const GdkColor* color);
void send_applet_tooltips_state (CORBA_Object appl, int applet_id, int enabled);

END_GNOME_DECLS

#endif /* __ORBIT_GLUE_H__ */

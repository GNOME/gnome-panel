#ifndef EXTERN_H
#define EXTERN_H

#include <glib.h>
#include "panel.h"
#include "panel-widget.h"
#include "gnome-panel.h"

#include <libgnorba/gnorba.h>

BEGIN_GNOME_DECLS

typedef struct _Extern Extern;
struct _Extern {
	POA_GNOME_PanelSpot servant;
	GNOME_PanelSpot pspot;
        GNOME_Applet applet;

	int refcount; /* primitive refcounting */

	gboolean didnt_want_save; /* normally FALSE, if an app returned TRUE
				     from save session it's set to TRUE,
				     in which case we won't warn about it
				     croaking and wanting a reload, because
				     it wouldn't work anyway. */

	gboolean clean_remove; /* normally FALSE, if TRUE, the user or the
				  applet requested to be killed, thus the
				  panel should not ask about putting the
				  applet back as if it may have crashed,
			          this is why it is important that applets
				  use the _remove method (unregister_us
				  corba call), so that this gets set, when
			       	  they want to leave cleanly */

	char *goad_id;
	char *cfg;
	GtkWidget *ebox;
	gboolean started;
	gboolean exactpos;
	gboolean send_position;
	gboolean send_draw;
	PanelOrientation orient; /* current orient, if it doesn't change,
				    don't send any orient change */
	
	int send_draw_timeout;
	int send_draw_idle;
	gboolean send_draw_queued;

	AppletInfo *info;
};

Extern *extern_ref		(Extern *ext);
void	extern_unref		(Extern *ext);
void	extern_clean		(Extern *ext);

void	extern_before_remove	(Extern *ext);

void	load_extern_applet	(const char *goad_id,
				 const char *cfgpath,
				 PanelWidget *panel,
				 int pos,
				 gboolean exactpos,
				 gboolean queue);
void	load_queued_externs	(void);

void	panel_corba_clean_up	(void);
gint	panel_corba_gtk_init	(CORBA_ORB panel_orb);

/* to be called when we want to send a draw signal to an applet */
void	extern_send_draw 	(Extern *ext);

void	save_applet		(AppletInfo *info,
				 gboolean ret);

void	extern_save_last_position (Extern *ext, gboolean sync);

END_GNOME_DECLS



#endif

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
void extern_clean(Extern *ext);

void load_extern_applet(char *goad_id, char *cfgpath, PanelWidget *panel,
			int pos, gboolean exactpos, gboolean queue);
void load_queued_externs(void);

void panel_corba_clean_up(void);
gint panel_corba_gtk_init(CORBA_ORB panel_orb);

/* to be called when we want to send a draw signal to an applet */
void extern_send_draw(Extern *ext);

void save_applet(AppletInfo *info, gboolean ret);

END_GNOME_DECLS



#endif

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

	char *goad_id;
	char *cfg;
	GtkWidget *ebox;
	int started;

	AppletInfo *info;
};
void extern_clean(Extern *ext);

void load_extern_applet(char *goad_id, char *cfgpath, PanelWidget *panel, int pos, int queue);
void load_queued_externs(void);

void panel_corba_clean_up(void);
void panel_corba_gtk_init(CORBA_ORB panel_orb);

END_GNOME_DECLS

#endif

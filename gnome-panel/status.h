#ifndef STATUS_H
#define STATUS_H

#include <gnome.h>
#include "panel.h"
#include "panel-widget.h"
#include "gnome-panel.h"

#include <libgnorba/gnorba.h>

BEGIN_GNOME_DECLS

/*XXX we get some sore of conflict if we just use Status*/
typedef struct _StatusApplet StatusApplet;
struct _StatusApplet {
	GtkWidget *table;
	AppletInfo *info;
};

typedef struct _StatusSpot StatusSpot;
struct _StatusSpot {
	POA_GNOME_StatusSpot servant;
	GNOME_StatusSpot sspot;

	gulong wid;
};

StatusSpot *new_status_spot(void);
void status_spot_remove(StatusSpot *ss);
void load_status_applet(PanelWidget *panel, int pos);

END_GNOME_DECLS

#endif

#ifndef STATUS_H
#define STATUS_H

#include <gnome.h>
#include "panel.h"
#include "panel-types.h"
#include "panel-widget.h"
#include "gnome-panel.h"

#include <libgnorba/gnorba.h>

BEGIN_GNOME_DECLS

/*XXX we get some sore of conflict if we just use Status*/
typedef struct _StatusApplet StatusApplet;
struct _StatusApplet {
	GtkWidget *frame;
	GtkWidget *handle; /*a gtk handlebox but one that doesn't undock*/
	PanelOrientation orient;
	PanelSizeType size;
	AppletInfo *info;
};

typedef struct _StatusSpot StatusSpot;
struct _StatusSpot {
	POA_GNOME_StatusSpot servant;
	GNOME_StatusSpot sspot;
	
	GtkWidget *socket;

	gulong wid;
};

StatusSpot *new_status_spot(void);
void status_spot_remove(StatusSpot *ss);
/*kill all status spots*/
void status_spot_remove_all(void);

/*returns TRUE if it could create an applet, FALSE if one already exists*/
int load_status_applet(PanelWidget *panel, int pos);
void status_applet_update(StatusApplet *s);

/*if we set this to TRUE, then the thing will ignore add_status requests*/
extern int status_inhibit_add;

END_GNOME_DECLS

#endif

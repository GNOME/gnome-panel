#ifndef STATUS_H
#define STATUS_H

#include "panel.h"
#include "panel-types.h"
#include "panel-widget.h"
#include "GNOME_Panel.h"

#include <bonobo-activation/bonobo-activation.h>

G_BEGIN_DECLS

/*XXX we get some sore of conflict if we just use Status*/
typedef struct _StatusApplet StatusApplet;
struct _StatusApplet {
	GtkWidget *frame;
	GtkWidget *handle; /*a gtk handlebox but one that doesn't undock*/
	GtkOrientation orient;
	int size;
	AppletInfo *info;
};

typedef struct _StatusSpot StatusSpot;
struct _StatusSpot {
	POA_GNOME_Vertigo_StatusSpot  servant;
	GNOME_Vertigo_StatusSpot      sspot;
	
	GtkWidget                    *socket;

	gulong                        wid;
};

void status_applet_create_offscreen(void);

StatusSpot *new_status_spot(void);
void status_spot_remove(StatusSpot *ss, gboolean destroy_socket);
/*kill all status spots*/
void status_spot_remove_all(void);

/*returns TRUE if it could create an applet, FALSE if one already exists*/
gboolean load_status_applet(PanelWidget *panel, int pos, gboolean exactpos, gboolean use_default);
void status_applet_update(StatusApplet *s);
void status_applet_put_offscreen(StatusApplet *s);

StatusSpot * status_applet_get_ss(guint32 winid);

/*inhibit adding and updating for the purpose of quitting*/
extern gboolean status_inhibit;

G_END_DECLS

#endif

#ifndef EXTERN_H
#define EXTERN_H

#include <glib.h>
#include "panel.h"
#include "panel-widget.h"
#include <libgnorba/gnorba.h>

BEGIN_GNOME_DECLS

typedef struct _Extern Extern;
struct _Extern {
        CORBA_Object obj;

	char *goad_id;
	char *cfg;
	GtkWidget *ebox;
	GList *goad_ids; /*applet goad_ids which can be started from
			   this object*/
};
void extern_clean(Extern *ext);

void load_extern_applet(char *goad_id, char *cfgpath, PanelWidget *panel, int pos);

/*stuff for corba*/
int applet_request_id (const char *goad_id,
		       char **cfgpath,
		       char **globcfgpath, guint32 *winid);
void applet_register (CORBA_Object obj,
		      int applet_id,
		      const char *goad_id,
		      const char *goad_ids);
guint32 reserve_applet_spot (Extern *ext, PanelWidget *panel, int pos,
			     AppletType type);
void applet_abort_id(int applet_id);
int applet_get_panel(int applet_id);
int applet_get_pos(int applet_id);
PanelOrientType applet_get_panel_orient(int applet_id);
void applet_show_menu(int applet_id);
void applet_drag_start(int applet_id);
void applet_drag_stop(int applet_id);
void applet_set_tooltip(int applet_id, const char *tooltip);

END_GNOME_DECLS

#endif

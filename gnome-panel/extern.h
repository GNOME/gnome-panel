#ifndef EXTERN_H
#define EXTERN_H

#include <glib.h>
#include "panel.h"
#include "panel-widget.h"

BEGIN_GNOME_DECLS

typedef struct _Extern Extern;
struct _Extern {
	char *ior;
	char *path;
	char *params;
	char *cfg;
};
void extern_clean(Extern *ext);

void load_extern_applet(char *path, char *params, char *cfgpath,
			PanelWidget *panel, int pos);

/*stuff for corba*/
int applet_request_id (const char *path, const char *param,
		       int dorestart, char **cfgpath,
		       char **globcfgpath, guint32 *winid);
void applet_register (const char * ior, int applet_id);
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

#ifndef MAIN_H
#define MAIN_H

#include <gtk/gtk.h>
#include "panel.h"

BEGIN_GNOME_DECLS

typedef enum {
	SNAPPED_PANEL,
	DRAWER_PANEL,
	CORNER_PANEL, /*not yet implemented*/
	FREE_PANEL, /*not yet implemented*/
	TABBED_PANEL /*not yet implemented*/
} PanelType;


typedef struct _PanelData PanelData;
struct _PanelData {
	PanelType type;
	GtkWidget *panel;
};

/*get the default panel widget if the panel has more then one or
  just get the that one*/
PanelWidget * get_def_panel_widget(GtkWidget *panel);

void load_applet(char *id, char *path, char *params,
		 char *pixmap, char *tooltip,
		 int pos, int panel, char *cfgpath);
void orientation_change(int applet_id, PanelWidget *panel);
void back_change(int applet_id, PanelWidget *panel);

PanelOrientType get_applet_orient(PanelWidget *panel);

void panel_setup(GtkWidget *panel);

/* this applet has finished loading, if it was the one we were waiting
   on, start the next applet */
void exec_queue_done(int applet_id);

typedef struct _AppletChild AppletChild;
/*used in the SIGCHLD handler and as a list of started applets*/
struct _AppletChild {
	int applet_id;
	pid_t pid;
};

#define get_panel_parent(appletw) \
	(gtk_object_get_data( \
	     gtk_object_get_data(GTK_OBJECT(appletw), \
				 PANEL_APPLET_PARENT_KEY), \
	     PANEL_PARENT))


END_GNOME_DECLS

#endif

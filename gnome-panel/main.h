#ifndef MAIN_H
#define MAIN_H

BEGIN_GNOME_DECLS

void load_applet(char *id, char *path, char *params,
		 char *pixmap, char *tooltip,
		 int pos, int panel, char *cfgpath);
void orientation_change(gint applet_id, PanelWidget *panel);
void back_change(gint applet_id, PanelWidget *panel);

PanelOrientType get_applet_orient(PanelWidget *panel);

/* this applet has finished loading, if it was the one we were waiting
   on, start the next applet */
void exec_queue_done(gint applet_id);

typedef struct _AppletChild AppletChild;
/*used in the SIGCHLD handler and as a list of started applets*/
struct _AppletChild {
	gint applet_id;
	pid_t pid;
};


END_GNOME_DECLS

#endif

#ifndef SWALLOW_H
#define SWALLOW_H

#include <panel-widget.h>

BEGIN_GNOME_DECLS

typedef enum {
	SWALLOW_VERTICAL,
	SWALLOW_HORIZONTAL
} SwallowOrient;

typedef struct {
	GtkWidget *ebox;
/* 	GtkWidget *handle_n; */
/* 	GtkWidget *handle_w; */
	GtkWidget *socket;
        GtkWidget *handle_box;
        char *title;
	char *path;
	int width;
	int height;
	guint32 wid;
} Swallow;

void load_swallow_applet(char *path, char *params, int width, int height,
			 PanelWidget *panela, int pos);
void set_swallow_applet_orient(Swallow *swallow, SwallowOrient orient);

/*I couldn't resist the naming of this function*/
void ask_about_swallowing(PanelWidget *panel, int pos);

END_GNOME_DECLS

#endif

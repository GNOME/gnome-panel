#ifndef SWALLOW_H
#define SWALLOW_H

BEGIN_GNOME_DECLS

typedef enum {
	SWALLOW_VERTICAL,
	SWALLOW_HORIZONTAL
} SwallowOrient;

typedef struct {
	GtkWidget *table;
	GtkWidget *handle_n;
	GtkWidget *handle_w;
	GtkWidget *socket;
	gchar *title;
	guint32 wid;
} Swallow;

Swallow * create_swallow_applet(char *arguments, SwallowOrient orient);

void set_swallow_applet_orient(Swallow *swallow, SwallowOrient orient);

END_GNOME_DECLS

#endif

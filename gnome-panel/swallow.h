#ifndef SWALLOW_H
#define SWALLOW_H

#include "panel-widget.h"

G_BEGIN_DECLS

typedef enum {
	SWALLOW_VERTICAL,
	SWALLOW_HORIZONTAL
} SwallowOrient;

typedef struct {
	int ref_count;

	GtkWidget *ebox;
	GtkWidget *socket;
        GtkWidget *handle_box;
	GtkWidget *frame;
        char *title;
	char *path;
	int width;
	int height;
	guint32 wid;
	gboolean clean_remove;
} Swallow;

void load_swallow_applet (const char  *path,
			  const char  *params,
			  int          width,
			  int          height,
			  PanelWidget *panela,
			  int          pos,
			  gboolean     exactpos,
			  const char  *gconf_key);

void swallow_save_to_gconf   (Swallow    *swallow,
			      const char *gconf_key);

void swallow_load_from_gconf (PanelWidget *panel_widget,
			      gint         position,
			      const char  *gconf_key,
			      gboolean     use_default);

void set_swallow_applet_orient(Swallow *swallow, SwallowOrient orient);

/*I couldn't resist the naming of this function*/
void ask_about_swallowing(PanelWidget *panel, int pos, gboolean exactpos);

G_END_DECLS

#endif

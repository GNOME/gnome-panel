#ifndef PANEL_CONFIG_H
#define PANEL_CONFIG_H

#include "panel-widget.h"
#include "border-widget.h"
#include "aligned-widget.h"
#include "sliding-widget.h"
#include "drawer-widget.h"

/* used to temporarily store config values until the 'Apply'
   button is pressed. */
typedef struct _PerPanelConfig PerPanelConfig;
struct _PerPanelConfig {
	GtkWidget		*panel;

	/*basep types*/
	int			hidebuttons;
	int			hidebutton_pixmaps;
	BasePMode               mode;

	/*floating widget*/
	PanelOrientation        orient;
	int                     x, y;

	/*indexes for toggle*/
	int edge;
	int align;

	gint16                  offset;

	
	/*panel types*/
	int			sz;
	gboolean		fit_pixmap_bg;
	gboolean		strech_pixmap_bg;
	gboolean		rotate_pixmap_bg;
	PanelBackType		back_type;
	char			*back_pixmap;
	GdkColor		back_color;
	
	int			register_changes; /*used for startup*/
	GtkWidget		*config_window;
	GtkWidget		*pix_entry;

	/*size*/
	GtkWidget               *size_menu;

	/*color buttons*/
	GtkWidget		*non;
	GtkWidget		*pix;
	GtkWidget		*col;
	GtkWidget		*backsel;

	/*position widgets*/
	GtkWidget               *toggle[4][3];    /* left */

	/*floating buttons*/
	GtkWidget               *h_orient;
	GtkWidget               *v_orient;
	GtkWidget               *x_spin;
	GtkWidget               *y_spin;

	/*sliding buttons*/
	GtkWidget               *offset_spin;
	GtkObject               *offset_adj;
};

void panel_config(GtkWidget *panel);
void update_config_size(GtkWidget *panel);
void update_config_back(PanelWidget *panel);
void update_config_edge (BasePWidget *w);
void update_config_anchor (BasePWidget *w);
void update_config_offset (BasePWidget *w);
void update_config_align (BasePWidget *w);
void update_config_floating_pos (BasePWidget *w);
void update_config_floating_orient (BasePWidget *w);
void kill_config_dialog(GtkWidget *panel);
#endif /* PANEL_CONFIG_H */

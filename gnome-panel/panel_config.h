#ifndef PANEL_CONFIG_H
#define PANEL_CONFIG_H

#include "panel-widget.h"

/* used to temporarily store config values until the 'Apply'
   button is pressed. */
typedef struct _PerPanelConfig PerPanelConfig;
struct _PerPanelConfig {
	GtkWidget		*panel;

	/*basep types*/
	int			hidebuttons;
	int			hidebutton_pixmaps;

	/*snapped types*/
	SnappedPos		snapped_pos;
	SnappedMode		snapped_mode;
	
	/*corner types*/
	CornerPos		corner_pos;
	PanelOrientation	corner_orient;
        CornerMode              corner_mode;

	/*panel types*/
	int			fit_pixmap_bg;
	PanelBackType		back_type;
	char			*back_pixmap;
	GdkColor		back_color;
	
	int			register_changes; /*used for startup*/
	GtkWidget		*config_window;
	GtkWidget		*pix_entry;

	/*snapped buttons*/
	GtkWidget		*r_button; /*se*/
	GtkWidget		*l_button; /*nw*/
	GtkWidget		*t_button; /*ne*/
	GtkWidget		*b_button; /*sw*/
	GtkWidget		*non;
	GtkWidget		*pix;
	GtkWidget		*col;
	GtkWidget		*backsel;
};

void panel_config(GtkWidget *panel);
void update_config_orient(GtkWidget *panel);
void update_config_back(PanelWidget *panel);
void kill_config_dialog(GtkWidget *panel);
#endif /* PANEL_CONFIG_H */

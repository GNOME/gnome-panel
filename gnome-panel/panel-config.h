#ifndef PANEL_CONFIG_H
#define PANEL_CONFIG_H

#include "panel-widget.h"
#include "border-widget.h"
#include "aligned-widget.h"
#include "sliding-widget.h"

#include "panel-util.h"

/* used to temporarily store config values until the 'Apply'
   button is pressed. */
typedef struct _PerPanelConfig PerPanelConfig;
struct _PerPanelConfig {
	GtkWidget		*panel;

	UpdateFunction		update_function;
	gpointer		update_data;

	/*basep types*/
	int			screen;
	gboolean		hidebuttons;
	gboolean		hidebutton_pixmaps;
	BasePMode               mode;
	gboolean		avoid_on_maximize;

	/*floating widget*/
	GtkOrientation        orient;
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
	
	gboolean		register_changes; /*used for startup*/
	gboolean		ppc_origin_change; /* if true then the dialog
						      will NOT be updated on changes,
						      so that we don't get into infinite
						      loops, set before we ourselves
						      apply changes. */
	GtkWidget		*config_window;
	GtkWidget		*type_tab; /* the type specific tab of
					      the notebook, this is just
					      an eventbox, which we will
					      be able to change contents
					      of */
	GtkWidget		*type_tab_label;

	GtkWidget		*pix_entry;

	/*avoid_on_maximize*/
	GtkWidget               *avoid_on_maximize_button;

	/*avoid_on_maximize*/
	GtkWidget               *screen_spin;

	/*hiding stuff*/
	GtkWidget               *autohide_button;
	GtkWidget               *hidebuttons_button;
	GtkWidget               *hidebutton_pixmaps_button;

	/*size*/
	GtkWidget               *size_menu;

	/*color buttons*/
	GtkWidget		*non;
	GtkWidget		*pix;
	GtkWidget		*col;
	GtkWidget		*backsel;
	GtkWidget		*back_om; /* the back type option menu */

	/*position widgets*/
#define POSITION_EDGES 4
#define POSITION_ALIGNS 3
	GtkWidget               *toggle[POSITION_EDGES][POSITION_ALIGNS];

	/*floating buttons*/
	GtkWidget               *h_orient;
	GtkWidget               *v_orient;
	GtkWidget               *x_spin;
	GtkWidget               *y_spin;

	/*sliding buttons*/
	GtkWidget               *offset_spin;
};

void panel_config_register_changes (PerPanelConfig *ppc);

void panel_config (GtkWidget *panel);
void update_config_size (GtkWidget *panel);
void update_config_avoid_on_maximize (BasePWidget *panel);
void update_config_screen (BasePWidget *panel);
void update_config_mode (BasePWidget *panel);
void update_config_hidebuttons (BasePWidget *panel);
void update_config_back (PanelWidget *panel);
void update_config_edge (BasePWidget *w);
void update_config_anchor (BasePWidget *w);
void update_config_offset (BasePWidget *w);
void update_config_offset_limit (BasePWidget *panel);
void update_config_align (BasePWidget *w);
void update_config_floating_pos (BasePWidget *panel);
void update_config_floating_orient (BasePWidget *panel);
void update_config_floating_pos_limits (BasePWidget *panel);
void update_config_type (BasePWidget *panel);
void kill_config_dialog (GtkWidget *panel);

GtkWidget *make_size_widget (PerPanelConfig *ppc);


#endif /* PANEL_CONFIG_H */

#include <gtk/gtk.h>

#include "gnome.h"
#include "panel.h"
#include "config.h"

/* Used for all the packing and padding options */
#define CONFIG_PADDING_SIZE 3


/* used to temporarily store config values until the 'Apply'
 * button is pressed. */
Panel config_panel;


GtkWidget *config_window;

static gint 
config_delete (GtkWidget *widget, gpointer data)
{
	/* allow window destroy */
	return(TRUE);
}

static void 
config_destroy(GtkWidget *widget, gpointer data)
{
    config_window = NULL;
}

static void 
set_position (GtkWidget *widget, gpointer data)
{
	PanelPos position = (PanelPos) data;
	
	config_panel.pos = position;
}

static void 
set_mode (GtkWidget *widget, gpointer data)
{
	PanelMode mode = (PanelMode) data;
	
	config_panel.mode = mode;
}

static void 
set_toggle_button_value (GtkWidget *widget, gpointer data)
{
	if(GTK_TOGGLE_BUTTON(widget)->active)
		*(int *)data=TRUE;
	else
		*(int *)data=FALSE;
}

static void
config_apply (GtkWidget *widget, gpointer data)
{
	panel_reconfigure(&config_panel);
}

/* FIXME: I think these should probly go in a notebook.. I have to
 * give some thought as to the best way to present all these config
 * options.  Also, delay, and movement step size should be configurable. */

GtkWidget *
position_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *box;
	GtkWidget *vbox;
	
	/* main vbox */
	vbox = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (vbox), CONFIG_PADDING_SIZE);
	gtk_widget_show (vbox);
	
	/* Position frame */
	frame = gtk_frame_new ("Position");
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);
	
	/* Top Position */
	button = gtk_radio_button_new_with_label (NULL, "Top");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_position), 
			    (gpointer)PANEL_POS_TOP);
	if (config_panel.pos == PANEL_POS_TOP) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
	/* Bottom Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  "Bottom");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_position), 
			    (gpointer)PANEL_POS_BOTTOM);
	if (config_panel.pos == PANEL_POS_BOTTOM) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
	/* Left Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  "Left");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_position), 
			    (gpointer)PANEL_POS_LEFT);
	if (config_panel.pos == PANEL_POS_LEFT) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);

	/* Right Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  "Right");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_position), 
			    (gpointer)PANEL_POS_RIGHT);
	if (config_panel.pos == PANEL_POS_RIGHT) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);

	/* Auto-hide/stayput frame */
	frame = gtk_frame_new ("Minimize Options");
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);
	
	/* Stay Put */
	button = gtk_radio_button_new_with_label (NULL, "Explicitly Hide");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_mode), 
			    (gpointer)PANEL_STAYS_PUT);
	if (config_panel.mode == PANEL_STAYS_PUT) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
	/* Auto-hide */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  "Auto Hide");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_mode), 
			    (gpointer)PANEL_GETS_HIDDEN);
	if (config_panel.mode == PANEL_GETS_HIDDEN) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
	return (vbox);
}

static void
step_size_scale_update (GtkAdjustment *adjustment, gpointer data)
{
	double scale_val = adjustment->value;
	config_panel.step_size = (gint) scale_val;
}

static void
delay_scale_update (GtkAdjustment *adjustment, gpointer data)
{
	double scale_val = adjustment->value;
	config_panel.minimize_delay = (gint) scale_val;
}

static void
minimized_size_scale_update (GtkAdjustment *adjustment, gpointer data)
{
	double scale_val = adjustment->value;
	config_panel.minimized_size = (gint) scale_val;
}



GtkWidget *
animation_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *box;
	GtkWidget *vbox;
	GtkWidget *scale;
	GtkObject *step_size_scale_data;
	GtkObject *delay_scale_data;
	GtkObject *minimized_size_scale_data;
	
	/* main vbox */
	vbox = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (vbox), CONFIG_PADDING_SIZE);
	gtk_widget_show (vbox);


	/* Animation step_size scale frame */
	frame = gtk_frame_new ("Animation Speed");
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_widget_show (box);

	/* Animation step_size scale */
	step_size_scale_data = gtk_adjustment_new ((double) config_panel.step_size, 
						   3.0, 100.0, 1.0, 1.0, 0.0);
	scale = gtk_hscale_new (GTK_ADJUSTMENT (step_size_scale_data));
	gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
	gtk_scale_set_digits (GTK_SCALE (scale), 0);
	gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
	gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
	gtk_box_pack_start (GTK_BOX (box), scale, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_signal_connect(GTK_OBJECT (step_size_scale_data), 
			   "value_changed",
			   GTK_SIGNAL_FUNC (step_size_scale_update), NULL);
	gtk_widget_show (scale);

	/* Minimized Delay scale frame */
	frame = gtk_frame_new ("Minimize Delay (ms)");
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);

	/* minimize_delay scale */
	delay_scale_data = gtk_adjustment_new ((double) config_panel.minimize_delay, 
					       30.0, 1000.0, 1.0, 1.0, 0.0);
	scale = gtk_hscale_new (GTK_ADJUSTMENT (delay_scale_data));
	gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
	gtk_scale_set_digits (GTK_SCALE (scale), 0);
	gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
	gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
	gtk_box_pack_start (GTK_BOX (box), scale, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_signal_connect(GTK_OBJECT (delay_scale_data), 
			   "value_changed",
			   GTK_SIGNAL_FUNC (delay_scale_update), NULL);
	gtk_widget_show (scale);

	/* Minimized size scale frame */
	frame = gtk_frame_new ("Minimized Size (pixels)");
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);

	/* minimized_size scale */
	minimized_size_scale_data = gtk_adjustment_new ((double) config_panel.minimized_size, 
					       1.0, 10.0, 1.0, 1.0, 0.0);
	scale = gtk_hscale_new (GTK_ADJUSTMENT (minimized_size_scale_data));
	gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
	gtk_scale_set_digits (GTK_SCALE (scale), 0);
	gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
	gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
	gtk_box_pack_start (GTK_BOX (box), scale, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_signal_connect(GTK_OBJECT (minimized_size_scale_data), 
			   "value_changed",
			   GTK_SIGNAL_FUNC (minimized_size_scale_update), NULL);
	gtk_widget_show (scale);

	return (vbox);
}

GtkWidget *
misc_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *box;
	GtkWidget *vbox;
	
	/* main vbox */
	vbox = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (vbox), CONFIG_PADDING_SIZE);
	gtk_widget_show (vbox);
	
	/* Tooltips frame */
	frame = gtk_frame_new ("Tooltips");
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);
	
	/* Tooltips enable */
	button = gtk_check_button_new_with_label ("Tooltips enabled");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(config_panel.tooltips_enabled));
	if (config_panel.tooltips_enabled) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
	return (vbox);
}


void 
panel_config(void)
{
	GtkWidget *box1;
	GtkWidget *box2;
	GtkWidget *label;
	GtkWidget *notebook;
	GtkWidget *button;
	GtkWidget *page;
	
	/* return if the window is already up. */
	if (config_window)
		return;

	/* so far, these are the only ones that can be set */
	config_panel.mode = the_panel->mode;
	config_panel.pos = the_panel->pos;
	
	config_panel.step_size = the_panel->step_size;
	config_panel.delay = the_panel->delay;
	config_panel.minimize_delay = the_panel->minimize_delay;
	config_panel.minimized_size = the_panel->minimized_size;

	config_panel.tooltips_enabled = the_panel->tooltips_enabled;
 
	
	/* main window */
	config_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect(GTK_OBJECT(config_window), "delete_event",
			   GTK_SIGNAL_FUNC (config_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(config_window), "destroy",
			   GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_window_set_title (GTK_WINDOW(config_window), "Panel Configuration");
	gtk_container_border_width (GTK_CONTAINER(config_window), CONFIG_PADDING_SIZE);
	
	/* main vbox */
	box1 = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_add(GTK_CONTAINER (config_window), box1);
	gtk_widget_show(box1);
	
	/* notebook */
	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX (box1), notebook, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show(notebook);
	
	/* label for Position notebook page */
	label = gtk_label_new ("Orientation");
	gtk_widget_show (label);
	
	/* Position notebook page */
	page = position_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);
	
	/* label for Animation notebook page */
	label = gtk_label_new ("Animation");
	gtk_widget_show (label);
	
	/* Animation notebook page */
	page = animation_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

	/* label for Miscellaneous notebook page */
	label = gtk_label_new ("Miscellaneous");
	gtk_widget_show (label);
	
	/* Miscellaneous notebook page */
	page = misc_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

	/* hbox for close and apply buttons */
	box2 = gtk_hbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_box_pack_start(GTK_BOX (box1), box2, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (box2);
	
	/* close button */
	button = gtk_button_new_with_label("Close");
	gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy), 
				   (gpointer)config_window);
	gtk_box_pack_start(GTK_BOX (box2), button, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);

	/* apply button */
	button = gtk_button_new_with_label("Apply");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (config_apply), 
			    NULL);
	gtk_box_pack_start(GTK_BOX (box2), button, TRUE, TRUE, 
			   CONFIG_PADDING_SIZE);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default (button);
	gtk_widget_show (button);

	
	gtk_widget_set_usize (config_window, 300, -1);
	/* show main window */
	gtk_widget_show (config_window);
}


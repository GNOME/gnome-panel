#include <gtk/gtk.h>

#include <config.h>
#include <gnome.h>
#include "panel.h"
#include "panel-widget.h"
#include "panel_config.h"
#include "panel_config_global.h"

/* Used for all the packing and padding options */
#define CONFIG_PADDING_SIZE 3


/* used to temporarily store config values until the 'Apply'
 * button is pressed. */
PanelConfig panel_config_struct;

extern GlobalConfig global_config;


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
set_snapped (GtkWidget *widget, gpointer data)
{
	PanelSnapped snapped = (PanelSnapped) data;
	
	panel_config_struct.snapped = snapped;
}

static void 
set_mode (GtkWidget *widget, gpointer data)
{
	PanelMode mode = (PanelMode) data;
	
	panel_config_struct.mode = mode;
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
	PanelWidget *panel = data;
	if(panel_config_struct.mode == PANEL_AUTO_HIDE) {
		panel_widget_change_params(panel,
					   panel_config_struct.orient,
					   panel_config_struct.snapped,
					   panel_config_struct.mode,
					   panel->state,
					   global_config.auto_hide_step_size,
					   global_config.minimized_size,
					   global_config.minimize_delay,
					   panel->drawer_drop_zone_pos);
	 } else {
		panel_widget_change_params(panel,
					   panel_config_struct.orient,
					   panel_config_struct.snapped,
					   panel_config_struct.mode,
					   panel->state,
					   global_config.explicit_hide_step_size,
					   global_config.minimized_size,
					   global_config.minimize_delay,
					   panel->drawer_drop_zone_pos);
	}
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
	frame = gtk_frame_new (_("Position"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);
	
	/* Top Position */
	button = gtk_radio_button_new_with_label (NULL, _("Top"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_TOP);
	if (panel_config_struct.snapped == PANEL_TOP) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
	/* Bottom Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Bottom"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_BOTTOM);
	if (panel_config_struct.snapped == PANEL_BOTTOM) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
	/* Left Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Left"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_LEFT);
	if (panel_config_struct.snapped == PANEL_LEFT) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);

	/* Right Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Right"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_RIGHT);
	if (panel_config_struct.snapped == PANEL_RIGHT) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);

	/* Auto-hide/stayput frame */
	frame = gtk_frame_new (_("Minimize Options"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);
	
	/* Stay Put */
	button = gtk_radio_button_new_with_label (NULL, _("Explicitly Hide"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_mode), 
			    (gpointer)PANEL_EXPLICIT_HIDE);
	if (panel_config_struct.mode == PANEL_EXPLICIT_HIDE) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
	/* Auto-hide */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Auto Hide"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_mode), 
			    (gpointer)PANEL_AUTO_HIDE);
	if (panel_config_struct.mode == PANEL_AUTO_HIDE) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
	return (vbox);
}

void 
panel_config(PanelWidget *panel)
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
	panel_config_struct.orient = panel->orient;
	panel_config_struct.snapped = panel->snapped;
	panel_config_struct.mode = panel->mode;
	
	
	/* main window */
	config_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect(GTK_OBJECT(config_window), "delete_event",
			   GTK_SIGNAL_FUNC (config_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(config_window), "destroy",
			   GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_window_set_title (GTK_WINDOW(config_window),
			      _("Panel Configuration"));
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
	label = gtk_label_new (_("Orientation"));
	gtk_widget_show (label);
	
	/* Position notebook page */
	page = position_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);
	
	/* hbox for close and apply buttons */
	box2 = gtk_hbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_box_pack_start(GTK_BOX (box1), box2, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (box2);
	
	/* close button */
	button = gtk_button_new_with_label(_("Close"));
	gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy), 
				   (gpointer)config_window);
	gtk_box_pack_start(GTK_BOX (box2), button, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_widget_show (button);

	/* apply button */
	button = gtk_button_new_with_label(_("Apply"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (config_apply), 
			    panel);
	gtk_box_pack_start(GTK_BOX (box2), button, TRUE, TRUE, 
			   CONFIG_PADDING_SIZE);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default (button);
	gtk_widget_show (button);

	
	gtk_widget_set_usize (config_window, 300, -1);
	/* show main window */
	gtk_widget_show (config_window);
}


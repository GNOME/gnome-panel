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
	return TRUE;
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
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
}

static void 
set_mode (GtkWidget *widget, gpointer data)
{
	PanelMode mode = (PanelMode) data;
	
	panel_config_struct.mode = mode;
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
}

static void 
set_toggle_button_value (GtkWidget *widget, gpointer data)
{
	if(GTK_TOGGLE_BUTTON(widget)->active)
		*(int *)data=TRUE;
	else
		*(int *)data=FALSE;
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
}

static void
config_apply (GtkWidget *widget, int page, gpointer data)
{
	PanelWidget *panel = data;
	if(panel_config_struct.mode == PANEL_AUTO_HIDE) {
		panel_widget_change_params(panel,
					   panel_config_struct.orient,
					   panel_config_struct.snapped,
					   panel_config_struct.mode,
					   panel->state,
					   panel->drawer_drop_zone_pos);
	 } else {
		panel_widget_change_params(panel,
					   panel_config_struct.orient,
					   panel_config_struct.snapped,
					   panel_config_struct.mode,
					   panel->state,
					   panel->drawer_drop_zone_pos);
	}
}

GtkWidget *
position_notebook_page(GtkWidget *propertybox)
{
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *box;
	GtkWidget *vbox;

	panel_config_struct.config_box = 0;
	
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

	panel_config_struct.config_box = propertybox;

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
	config_window = gnome_property_box_new ();
	gtk_signal_connect(GTK_OBJECT(config_window), "destroy",
			   GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_window_set_title (GTK_WINDOW(config_window),
			      _("Panel Configuration"));
	gtk_container_border_width (GTK_CONTAINER(config_window), CONFIG_PADDING_SIZE);
	
	/* label for Position notebook page */
	label = gtk_label_new (_("Orientation"));
	gtk_widget_show (label);
	
	/* Position notebook page */
	page = position_notebook_page (GNOME_PROPERTY_BOX (config_window));
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (config_window),
					page, gtk_label_new (_("Orientation")));

	gtk_signal_connect (GTK_OBJECT (config_window), "apply",
			    GTK_SIGNAL_FUNC (config_apply), panel);
	
	/* show main window */
	gtk_widget_show (config_window);
}


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
GlobalConfig temp_config;

extern GlobalConfig global_config;

extern GList *panels;


static GtkWidget *config_window;

static int
config_destroy(GtkWidget *widget, gpointer data)
{
	config_window = NULL;
	return FALSE;
}

static void 
set_toggle_button_value (GtkWidget *widget, gpointer data)
{
	if(GTK_TOGGLE_BUTTON(widget)->active)
		*(int *)data=TRUE;
	else
		*(int *)data=FALSE;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
}


static void
config_apply (GtkWidget *widget, int page, gpointer data)
{
	global_config.minimize_delay = temp_config.minimize_delay;
	global_config.minimized_size = temp_config.minimized_size;
	global_config.auto_hide_step_size = temp_config.auto_hide_step_size;
	global_config.explicit_hide_step_size =
		temp_config.explicit_hide_step_size;
	global_config.tooltips_enabled = temp_config.tooltips_enabled;
	global_config.show_small_icons = temp_config.show_small_icons;
	global_config.movement_type = temp_config.movement_type;

	apply_global_config();
}

static void
gint_scale_update (GtkAdjustment *adjustment, gpointer data)
{
	gint *val = data;
	double scale_val = adjustment->value;
	*val = (gint) scale_val;
	gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
}

GtkWidget *
make_gint_scale_frame(gchar *title, gint *data, double min, double max)
{
	GtkWidget *frame;
	GtkWidget *box;
	GtkWidget *scale;
	GtkObject *scale_data;

	/* scale frame */
	frame = gtk_frame_new (title);
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_widget_show (box);

	/* Animation step_size scale */
	scale_data = gtk_adjustment_new((double) (*data),
					min, max, 1.0, 1.0, 0.0);
	scale = gtk_hscale_new (GTK_ADJUSTMENT (scale_data));
	gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
	gtk_scale_set_digits (GTK_SCALE (scale), 0);
	gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
	gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
	gtk_box_pack_start (GTK_BOX (box), scale, TRUE, TRUE, CONFIG_PADDING_SIZE);
	gtk_signal_connect(GTK_OBJECT (scale_data), 
			   "value_changed",
			   GTK_SIGNAL_FUNC (gint_scale_update),
			   data);
	gtk_widget_show (scale);

	return frame;
}

GtkWidget *
animation_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	
	/* main vbox */
	vbox = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (vbox), CONFIG_PADDING_SIZE);

	/* AutoHide Animation step_size scale frame */
	frame = make_gint_scale_frame(_("Auto-Hide Animation Speed"),
				      &(temp_config.auto_hide_step_size),
				      1.0,100.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	/* ExplicitHide Animation step_size scale frame */
	frame = make_gint_scale_frame(_("Explicit-Hide Animation Speed"),
				      &(temp_config.explicit_hide_step_size),
				      1.0,100.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	/* Minimize Delay scale frame */
	frame = make_gint_scale_frame(_("Auto-Hide Minimize Delay (ms)"),
				      &(temp_config.minimize_delay),
				      30.0,1000.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	/* Minimized size scale frame */
	frame = make_gint_scale_frame(_("Auto-Hide Minimized Size (pixels)"),
				      &(temp_config.minimized_size),
				      1.0,10.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	return (vbox);
}

static void 
set_movement (GtkWidget *widget, gpointer data)
{
	PanelMovementType move_type = (PanelMovementType) data;

	temp_config.movement_type = move_type;
	
	gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
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
	
	/* Tooltips frame */
	frame = gtk_frame_new (_("Icon Tooltips"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Tooltips enable */
	button = gtk_check_button_new_with_label (_("Tooltips enabled"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.tooltips_enabled));
	if (temp_config.tooltips_enabled)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);

	/* Menu frame */
	frame = gtk_frame_new (_("Menus"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Small Icons */
	button = gtk_check_button_new_with_label (_("Show small icons"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.show_small_icons));
	if (temp_config.show_small_icons)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);

	/* Movement frame */
	frame = gtk_frame_new (_("Movement"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);

	/* Switched */
	button = gtk_radio_button_new_with_label (NULL, _("Switched movement"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_movement), 
			    (gpointer)PANEL_SWITCH_MOVE);
	if (temp_config.movement_type == PANEL_SWITCH_MOVE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	

	/* Free */
	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
		_("Free movement (doesn't disturb other applets)"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_movement), 
			    (gpointer)PANEL_FREE_MOVE);
	if (temp_config.movement_type == PANEL_FREE_MOVE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	
	
	return (vbox);
}


void 
panel_config_global(void)
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

	temp_config.minimize_delay = global_config.minimize_delay;
	temp_config.minimized_size = global_config.minimized_size;
	temp_config.auto_hide_step_size = global_config.auto_hide_step_size;
	temp_config.explicit_hide_step_size =
		global_config.explicit_hide_step_size;
	temp_config.tooltips_enabled = global_config.tooltips_enabled;
	temp_config.show_small_icons = global_config.show_small_icons;

	/* main window */
	config_window = gnome_property_box_new ();
	gtk_signal_connect(GTK_OBJECT(config_window), "destroy",
			   GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_signal_connect (GTK_OBJECT (config_window), "delete_event",
			    GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_window_set_title (GTK_WINDOW(config_window),
			      _("Global Panel Configuration"));
	gtk_container_border_width (GTK_CONTAINER(config_window), CONFIG_PADDING_SIZE);
	
	/* Animation notebook page */
	page = animation_notebook_page ();
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (config_window),
					page, gtk_label_new (_("Animation settings")));

	/* Miscellaneous notebook page */
	page = misc_notebook_page ();
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (config_window),
					page, gtk_label_new (_("Miscellaneous")));

	gtk_signal_connect (GTK_OBJECT (config_window), "apply",
			    GTK_SIGNAL_FUNC (config_apply), NULL);

	/* show main window */
	gtk_widget_show_all (config_window);
}


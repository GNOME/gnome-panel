#include <gtk/gtk.h>

#include <string.h>
#include <config.h>
#include <gnome.h>

#include "panel-include.h"

/* Used for all the packing and padding options */
#define CONFIG_PADDING_SIZE 3


/* used to temporarily store config values until the 'Apply'
 * button is pressed. */
GlobalConfig temp_config;

extern GlobalConfig global_config;
extern int config_sync_timeout;
extern int panels_to_sync;
extern GList *applets_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

extern GList *panels;

static GtkWidget *aniframe[3];


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
	if(config_window)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
}


static void
config_apply (GtkWidget *widget, int page, gpointer data)
{
	memcpy(&global_config,&temp_config,sizeof(GlobalConfig));

	apply_global_config();

	globals_to_sync = TRUE;
	panel_config_sync();
}

static void
int_scale_update (GtkAdjustment *adjustment, gpointer data)
{
	int *val = data;
	double scale_val = adjustment->value;
	*val = (int) scale_val;
	if(config_window)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
}

GtkWidget *
make_int_scale_frame(char *title, int *data, double min, double max)
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
			   GTK_SIGNAL_FUNC (int_scale_update),
			   data);
	gtk_widget_show (scale);

	return frame;
}

static int
set_anim_button_value(GtkWidget *w, gpointer data)
{
	int i;
	int active = GTK_TOGGLE_BUTTON(w)->active;

	for(i=0;i<3;i++)
		gtk_widget_set_sensitive(aniframe[i],!active);
	temp_config.disable_animations = active;

	if(config_window)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
	return FALSE;
}

GtkWidget *
animation_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *button;
	
	/* main vbox */
	vbox = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (vbox), CONFIG_PADDING_SIZE);

	/* Animation disable */
	button = gtk_check_button_new_with_label (_("Disable animations"));
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_anim_button_value),NULL); 
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);


	/* AutoHide Animation step_size scale frame */
	frame = make_int_scale_frame(_("Auto-Hide Animation Speed"),
				      &(temp_config.auto_hide_step_size),
				      1.0,100.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	aniframe[0] = frame;

	/* ExplicitHide Animation step_size scale frame */
	frame = make_int_scale_frame(_("Explicit-Hide Animation Speed"),
				      &(temp_config.explicit_hide_step_size),
				      1.0,100.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	aniframe[1] = frame;

	/* DrawerHide Animation step_size scale frame */
	frame = make_int_scale_frame(_("Drawer Animation Speed"),
				      &(temp_config.drawer_step_size),
				      1.0,100.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	aniframe[2] = frame;

	/* Minimize Delay scale frame */
	frame = make_int_scale_frame(_("Auto-Hide Minimize Delay (ms)"),
				      &(temp_config.minimize_delay),
				      30.0,1000.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	/* Minimized size scale frame */
	frame = make_int_scale_frame(_("Auto-Hide Minimized Size (pixels)"),
				      &(temp_config.minimized_size),
				      1.0,10.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	/*we have to do this after everything we need aniframe varaibles set*/
	if (temp_config.disable_animations)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);

	return (vbox);
}

static int
set_movement (GtkWidget *widget, gpointer data)
{
	PanelMovementType move_type = (PanelMovementType) data;

	if(!(GTK_TOGGLE_BUTTON(widget)->active))
		return FALSE;

	temp_config.movement_type = move_type;
	
	if(config_window)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
	return FALSE;
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
	frame = gtk_frame_new (_("Tooltips"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Tooltips enable */
	button = gtk_check_button_new_with_label (_("Tooltips enabled"));
	if (temp_config.tooltips_enabled)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.tooltips_enabled));
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
	if (temp_config.show_small_icons)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.show_small_icons));
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
	if (temp_config.movement_type == PANEL_SWITCH_MOVE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_movement), 
			    (gpointer)PANEL_SWITCH_MOVE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	

	/* Free */
	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
		_("Free movement (doesn't disturb other applets)"));
	if (temp_config.movement_type == PANEL_FREE_MOVE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_movement), 
			    (gpointer)PANEL_FREE_MOVE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	
	
	/* Logout frame */
	frame = gtk_frame_new (_("Log Out"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Prompt before log out */
	button = gtk_check_button_new_with_label (_("Prompt before logout"));
	if (temp_config.prompt_for_logout)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.prompt_for_logout));
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);

	return (vbox);
}


void 
panel_config_global(void)
{
	GtkWidget *page;
	GtkWidget *box;
	
	/* return if the window is already up. */
	if (config_window)
		return;

	memcpy(&temp_config,&global_config,sizeof(GlobalConfig));

	/* main window */
	box = gnome_property_box_new ();
	/*gtk_window_position(GTK_WINDOW(box), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(box), FALSE, FALSE, TRUE);
	gtk_signal_connect(GTK_OBJECT(box), "destroy",
			   GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_signal_connect (GTK_OBJECT (box), "delete_event",
			    GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_window_set_title (GTK_WINDOW(box),
			      _("Global Panel Configuration"));
	gtk_container_border_width (GTK_CONTAINER(box), CONFIG_PADDING_SIZE);
	
	/* Animation notebook page */
	page = animation_notebook_page ();
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (box),
					page, gtk_label_new (_("Animation settings")));

	/* Miscellaneous notebook page */
	page = misc_notebook_page ();
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (box),
					page, gtk_label_new (_("Miscellaneous")));

	gtk_signal_connect (GTK_OBJECT (box), "apply",
			    GTK_SIGNAL_FUNC (config_apply), NULL);

	config_window = box;

	/* show main window */
	gtk_widget_show_all (config_window);
}


#include <gtk/gtk.h>
#ifdef HAVE_LIBINTL
#include <libintl.h>
#define _(String) gettext(String)
#else
#define _(String) (String)
#endif

#include "gnome.h"
#include "panel.h"
#include "panel-widget.h"
#include "config.h"
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
	global_config.minimize_delay = temp_config.minimize_delay;
	global_config.minimized_size = temp_config.minimized_size;
	global_config.auto_hide_step_size = temp_config.auto_hide_step_size;
	global_config.explicit_hide_step_size =
		temp_config.explicit_hide_step_size;
	global_config.tooltips_enabled = temp_config.tooltips_enabled;
	global_config.show_small_icons = temp_config.show_small_icons;

	apply_global_config();
}

static void
gint_scale_update (GtkAdjustment *adjustment, gpointer data)
{
	gint *val = data;
	double scale_val = adjustment->value;
	*val = (gint) scale_val;
}

GtkWidget *
make_gint_scale_frame(gchar *title, gint *data)
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
					3.0, 100.0, 1.0, 1.0, 0.0);
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
	gtk_widget_show (vbox);

	/* AutoHide Animation step_size scale frame */
	frame = make_gint_scale_frame(_("Auto-Hide Animation Speed"),
				      &(temp_config.auto_hide_step_size));
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);

	/* ExplicitHide Animation step_size scale frame */
	frame = make_gint_scale_frame(_("Explicit-Hide Animation Speed"),
				      &(temp_config.explicit_hide_step_size));
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);

	/* Minimize Delay scale frame */
	frame = make_gint_scale_frame(_("Auto-Hide Minimize Delay (ms)"),
				      &(temp_config.minimize_delay));
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);

	/* Minimized size scale frame */
	frame = make_gint_scale_frame(_("Auto-Hide Minimized Size (pixels)"),
				      &(temp_config.minimized_size));
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);

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
	frame = gtk_frame_new (_("Icon Tooltips"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);
	
	/* Tooltips enable */
	button = gtk_check_button_new_with_label (_("Tooltips enabled"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.tooltips_enabled));
	if (temp_config.tooltips_enabled) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	gtk_widget_show (button);

	/* Menu frame */
	frame = gtk_frame_new (_("Menus"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	gtk_widget_show (frame);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);
	
	/* Small Icons */
	button = gtk_check_button_new_with_label (_("Show small icons"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.show_small_icons));
	if (temp_config.show_small_icons) {
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	}
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	gtk_widget_show (button);
	
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
	config_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect(GTK_OBJECT(config_window), "delete_event",
			   GTK_SIGNAL_FUNC (config_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(config_window), "destroy",
			   GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_window_set_title (GTK_WINDOW(config_window),
			      _("Global Panel Configuration"));
	gtk_container_border_width (GTK_CONTAINER(config_window), CONFIG_PADDING_SIZE);
	
	/* main vbox */
	box1 = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_add(GTK_CONTAINER (config_window), box1);
	gtk_widget_show(box1);
	
	/* notebook */
	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX (box1), notebook, FALSE, FALSE, CONFIG_PADDING_SIZE);
	gtk_widget_show(notebook);
	
	/* label for Animation notebook page */
	label = gtk_label_new (_("Animation settings"));
	gtk_widget_show (label);
	
	/* Animation notebook page */
	page = animation_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

	/* label for Miscellaneous notebook page */
	label = gtk_label_new (_("Miscellaneous"));
	gtk_widget_show (label);
	
	/* Miscellaneous notebook page */
	page = misc_notebook_page ();
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


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


GtkWidget *config_window, *file_entry;

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
    panel_config_struct.config_box = 0;
}

static gint
set_snapped (GtkWidget *widget, gpointer data)
{
	PanelSnapped snapped = (PanelSnapped) data;

	if(!(GTK_TOGGLE_BUTTON(widget)->active))
		return FALSE;
	
	panel_config_struct.snapped = snapped;
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
	return FALSE;
}

static gint
set_mode (GtkWidget *widget, gpointer data)
{
	PanelMode mode = (PanelMode) data;

	if(!(GTK_TOGGLE_BUTTON(widget)->active))
		return FALSE;
	
	panel_config_struct.mode = mode;
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
	return FALSE;
}

static gint
set_toggle_button_value (GtkWidget *widget, gpointer data)
{
	if(GTK_TOGGLE_BUTTON(widget)->active)
		*(int *)data=TRUE;
	else
		*(int *)data=FALSE;
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
	return FALSE;
}

static void
config_apply (GtkWidget *widget, int page, gpointer data)
{
	PanelWidget *panel = data;
	char *back_pixmap;
	
	if(panel_config_struct.pixmap_enable)
		back_pixmap = GTK_ENTRY (gnome_file_entry_gtk_entry (
				GNOME_FILE_ENTRY (file_entry)))->text;
	else
		back_pixmap = "";
	
	panel_widget_change_params(panel,
				   panel_config_struct.orient,
				   panel_config_struct.snapped,
				   panel_config_struct.mode,
				   panel_config_struct.fit_pixmap_bg,
				   panel->state,
				   panel->drawer_drop_zone_pos,
				   back_pixmap);
	gtk_widget_queue_draw (GTK_WIDGET (panel));
}

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
	
	/* Position frame */
	frame = gtk_frame_new (_("Position"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, CONFIG_PADDING_SIZE);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Top Position */
	button = gtk_radio_button_new_with_label (NULL, _("Top"));
	if (panel_config_struct.snapped == PANEL_TOP)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_TOP);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	
	/* Bottom Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Bottom"));
	if (panel_config_struct.snapped == PANEL_BOTTOM)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_BOTTOM);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);
	
	/* Left Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Left"));
	if (panel_config_struct.snapped == PANEL_LEFT)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_LEFT);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);

	/* Right Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Right"));
	if (panel_config_struct.snapped == PANEL_RIGHT)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_RIGHT);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, CONFIG_PADDING_SIZE);

	/* Auto-hide/stayput frame */
	frame = gtk_frame_new (_("Minimize Options"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, CONFIG_PADDING_SIZE);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Stay Put */
	button = gtk_radio_button_new_with_label (NULL, _("Explicitly Hide"));
	if (panel_config_struct.mode == PANEL_EXPLICIT_HIDE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_mode), 
			    (gpointer)PANEL_EXPLICIT_HIDE);
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, CONFIG_PADDING_SIZE);
	
	/* Auto-hide */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Auto Hide"));
	if (panel_config_struct.mode == PANEL_AUTO_HIDE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_mode), 
			    (gpointer)PANEL_AUTO_HIDE);
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, CONFIG_PADDING_SIZE);

	return (vbox);
}

static GtkWidget *
align (GtkWidget *w, float x)
{
	GtkWidget *align = gtk_alignment_new (x, 0.5, 1.0, 1.0);

	gtk_container_add (GTK_CONTAINER (align), w);
	return align;
}

static gint
value_changed ()
{
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
	return FALSE;
}

static void 
set_pixmap_enable (GtkWidget *widget, gpointer data)
{
	GtkWidget *w = data;
	gint enabled = GTK_TOGGLE_BUTTON(widget)->active;

	panel_config_struct.pixmap_enable = enabled;

	gtk_widget_set_sensitive(w,enabled);

	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
}

static void
set_fit_pixmap_bg (GtkToggleButton *toggle, gpointer data)
{
	panel_config_struct.fit_pixmap_bg = toggle->active;

	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
}

static void
color_changed_cb( GnomeColorSelector *widget, gchar **color )
{
        char *tmp;
 	int r,g,b;

	/*FIXME*/
	return;

	/* FIXME ugh, mem leak..anyone have a better way of doing this? */
	tmp = g_malloc(24);
        if( !tmp )
        {
	        g_warning(_("Can't allocate memory for color\n"));
                return;
        }
        gnome_color_selector_get_color_int(
        	widget, &r, &g, &b, 255 );
	
	sprintf( tmp, "#%02x%02x%02x", r, g, b );
        *color = tmp;
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
}
			   

static GtkWidget *
pixmap_page (PanelWidget *panel)
{
	GtkWidget *box, *f, *t;
	GtkWidget *vbox;
	GtkWidget *w;
	GtkWidget *fit;

	vbox = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (vbox), CONFIG_PADDING_SIZE);

	/*image frame*/
	f = gtk_frame_new (_("Image file"));
	gtk_container_border_width(GTK_CONTAINER (f), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), f, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);

	box = gtk_vbox_new (0, 0);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (f), box);

	file_entry = gnome_file_entry_new ("pixmap", _("Browse"));

	fit = gtk_check_button_new_with_label (_("Scale image to fit panel"));
	w = gtk_check_button_new_with_label (_("Enable Background Image"));
	/*always set to true, because in the beginning we don't have
	  any pixmap so it's not gonna be set by default anyhow*/
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w), TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (set_pixmap_enable), 
			    file_entry);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (set_pixmap_enable), 
			    fit);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);

	t = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (file_entry));
	gtk_signal_connect (GTK_OBJECT (t), "changed",
			    GTK_SIGNAL_FUNC (value_changed), NULL);
	gtk_box_pack_start (GTK_BOX (box), file_entry, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);
	
	gtk_entry_set_text (GTK_ENTRY (t), panel->back_pixmap ?
			    panel->back_pixmap : "");

	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (fit),
				     panel->fit_pixmap_bg);
	gtk_signal_connect (GTK_OBJECT (fit), "toggled",
			    GTK_SIGNAL_FUNC (set_fit_pixmap_bg),
			    NULL);
	gtk_box_pack_start (GTK_BOX (box), fit, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);


#if 0
	/*color frame*/
	f = gtk_frame_new (_("Background color"));
	gtk_container_border_width(GTK_CONTAINER (f), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), f, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);

	box = gtk_vbox_new (0, 0);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (f), box);

	w = gnome_color_selector_new((SetColorFunc)color_changed_cb, NULL);

	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
#endif

	return vbox;
}
	     
void 
panel_config(PanelWidget *panel)
{
	GtkWidget *page;
	
	/* return if the window is already up. */
	if (config_window) {
		gdk_window_raise(config_window->window);
		return;
	}

	panel_config_struct.config_box = 0;

	/* so far, these are the only ones that can be set */
	panel_config_struct.orient = panel->orient;
	panel_config_struct.snapped = panel->snapped;
	panel_config_struct.mode = panel->mode;
	panel_config_struct.fit_pixmap_bg = panel->fit_pixmap_bg;
	/*panel_config_struct.pixmap_enable = panel->back_pixmap != NULL;*/
	panel_config_struct.pixmap_enable = TRUE;
	
	/* main window */
	config_window = gnome_property_box_new ();
	gtk_signal_connect(GTK_OBJECT(config_window), "destroy",
			   GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_window_set_title (GTK_WINDOW(config_window),
			      _("Panel properties"));
	gtk_container_border_width (GTK_CONTAINER(config_window), CONFIG_PADDING_SIZE);
	
	if(panel->snapped!=PANEL_DRAWER &&
	   panel->snapped!=PANEL_FREE) {
		/* Position notebook page */
		page = position_notebook_page ();
		gnome_property_box_append_page (GNOME_PROPERTY_BOX (config_window),
						page, gtk_label_new (_("Orientation")));
	}
						

	/* Backing pixmap configuration */
	page = pixmap_page (panel);
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (config_window),
					page, gtk_label_new (_("Background image")));
	
	gtk_signal_connect (GTK_OBJECT (config_window), "apply",
			    GTK_SIGNAL_FUNC (config_apply), panel);
	
	panel_config_struct.config_box = config_window;

	/* show main window */
	gtk_widget_show_all (config_window);
}


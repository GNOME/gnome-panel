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
	g_free(panel_config_struct.back_pixmap);
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
	
	panel_widget_change_params(panel,
				   panel_config_struct.orient,
				   panel_config_struct.snapped,
				   panel_config_struct.mode,
				   panel->state,
				   panel->drawer_drop_zone_pos,
				   panel_config_struct.back_type,
				   panel_config_struct.back_pixmap,
				   panel_config_struct.fit_pixmap_bg,
				   &panel_config_struct.back_color);
	gtk_widget_queue_draw (GTK_WIDGET (panel));
	
	panel_sync_config();
}

GtkWidget *
position_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *box;
	GtkWidget *hbox;
        GtkWidget *table;
	
	/* main hbox */
	hbox = gtk_hbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (hbox), CONFIG_PADDING_SIZE);
	
	/* Position frame */
	frame = gtk_frame_new (_("Position"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, CONFIG_PADDING_SIZE);
	
	/* table for frame */
	table = gtk_table_new(3, 3, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), CONFIG_PADDING_SIZE);
	gtk_table_set_col_spacings(GTK_TABLE(table), CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER(table), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), table);
	
	/* Top Position */
	button = gtk_radio_button_new_with_label (NULL, _("Top"));
	if (panel_config_struct.snapped == PANEL_TOP)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_TOP);
        gtk_table_attach(GTK_TABLE(table), button, 1, 2, 0, 1,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);
	
	/* Bottom Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Bottom"));
	if (panel_config_struct.snapped == PANEL_BOTTOM)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_BOTTOM);
        gtk_table_attach(GTK_TABLE(table), button, 1, 2, 2, 3,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);
	
	/* Left Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Left"));
	if (panel_config_struct.snapped == PANEL_LEFT)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_LEFT);
        gtk_table_attach(GTK_TABLE(table), button, 0, 1, 1, 2,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);

	/* Right Position */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Right"));
	if (panel_config_struct.snapped == PANEL_RIGHT)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_snapped), 
			    (gpointer)PANEL_RIGHT);
        gtk_table_attach(GTK_TABLE(table), button, 2, 3, 1, 2,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);

	/* Auto-hide/stayput frame */
	frame = gtk_frame_new (_("Minimize Options"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, CONFIG_PADDING_SIZE);

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

	return (hbox);
}

static GtkWidget *
align (GtkWidget *w, float x)
{
	GtkWidget *align = gtk_alignment_new (x, 0.5, 1.0, 1.0);

	gtk_container_add (GTK_CONTAINER (align), w);
	return align;
}

static gint
value_changed (GtkWidget *w, gpointer data)
{
	g_free(panel_config_struct.back_pixmap);
	panel_config_struct.back_pixmap =
		g_strdup(gtk_entry_get_text(GTK_ENTRY(w)));
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
	return FALSE;
}

static void
set_fit_pixmap_bg (GtkToggleButton *toggle, gpointer data)
{
	panel_config_struct.fit_pixmap_bg = toggle->active;

	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
}

static void
color_changed_cb(GnomeColorSelector *sel, gpointer data)
{
 	gint r,g,b;

        gnome_color_selector_get_color_int(sel,&r,&g,&b, 65355);

	panel_config_struct.back_color.red = r;
	panel_config_struct.back_color.green = g;
	panel_config_struct.back_color.blue =b;
	
	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
}
			   
static gint
set_back (GtkWidget *widget, gpointer data)
{
	GtkWidget *pix;
	GtkWidget *col;
	PanelBackType back_type = PTOI(data);
	if(!GTK_TOGGLE_BUTTON(widget)->active)
		return FALSE;
	
	pix = gtk_object_get_data(GTK_OBJECT(widget),"pix");
	col = gtk_object_get_data(GTK_OBJECT(widget),"col");
	
	if(back_type == PANEL_BACK_NONE) {
		gtk_widget_set_sensitive(pix,FALSE);
		gtk_widget_set_sensitive(col,FALSE);
	} else if(back_type == PANEL_BACK_COLOR) {
		gtk_widget_set_sensitive(pix,FALSE);
		gtk_widget_set_sensitive(col,TRUE);
	} else  {
		gtk_widget_set_sensitive(pix,TRUE);
		gtk_widget_set_sensitive(col,FALSE);
	}
	
	panel_config_struct.back_type = back_type;

	if (panel_config_struct.config_box)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (panel_config_struct.config_box));
}


static GtkWidget *
background_page (PanelWidget *panel)
{
	GtkWidget *box, *f, *t;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *w;
	GtkWidget *pix;
	GtkWidget *col;
	GtkWidget *non;
	GnomeColorSelector *sel;

	vbox = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (vbox), CONFIG_PADDING_SIZE);

	/*selector frame*/
	f = gtk_frame_new (_("Background"));
	gtk_container_border_width(GTK_CONTAINER (f), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), f, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);

	box = gtk_hbox_new (0, 0);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (f), box);
	
	/*standard background*/
	non = gtk_radio_button_new_with_label (NULL, _("Standard"));
	gtk_box_pack_start (GTK_BOX (box), non, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	

	/* pixmap */
	pix = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (non)),
		_("Pixmap"));
	gtk_box_pack_start (GTK_BOX (box), pix, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	
	
	/* color */
	col = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (non)),
		_("Color"));
	gtk_box_pack_start (GTK_BOX (box), col, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	

	/*image frame*/
	f = gtk_frame_new (_("Image file"));
	if(panel_config_struct.back_type == PANEL_BACK_PIXMAP) {
		gtk_widget_set_sensitive(f,TRUE);
	} else  {
		gtk_widget_set_sensitive(f,FALSE);
	}
	gtk_object_set_data(GTK_OBJECT(pix),"pix",f);
	gtk_object_set_data(GTK_OBJECT(col),"pix",f);
	gtk_object_set_data(GTK_OBJECT(non),"pix",f);
	gtk_container_border_width(GTK_CONTAINER (f), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), f, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);

	box = gtk_vbox_new (0, 0);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (f), box);

	file_entry = gnome_file_entry_new ("pixmap", _("Browse"));


	t = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (file_entry));
	gtk_signal_connect (GTK_OBJECT (t), "changed",
			    GTK_SIGNAL_FUNC (value_changed), NULL);
	gtk_box_pack_start (GTK_BOX (box), file_entry, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);
	
	gtk_entry_set_text (GTK_ENTRY(t),
			    panel->back_pixmap?panel->back_pixmap:"");

	w = gtk_check_button_new_with_label (_("Scale image to fit panel"));
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w),
				     panel->fit_pixmap_bg);
	gtk_signal_connect (GTK_OBJECT (w), "toggled",
			    GTK_SIGNAL_FUNC (set_fit_pixmap_bg),
			    NULL);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);


	/*color frame*/
	box = gtk_hbox_new (0, 0);
	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);
	f = gtk_frame_new (_("Background color"));
	if(panel_config_struct.back_type == PANEL_BACK_COLOR) {
		gtk_widget_set_sensitive(f,TRUE);
	} else  {
		gtk_widget_set_sensitive(f,FALSE);
	}
	gtk_object_set_data(GTK_OBJECT(pix),"col",f);
	gtk_object_set_data(GTK_OBJECT(col),"col",f);
	gtk_object_set_data(GTK_OBJECT(non),"col",f);
	gtk_container_border_width(GTK_CONTAINER (f), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (box), f, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);

	box = gtk_vbox_new (0, 0);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (f), box);

	sel = gnome_color_selector_new(color_changed_cb, NULL);
        gnome_color_selector_set_color_int(sel,
		panel_config_struct.back_color.red,
		panel_config_struct.back_color.green,
		panel_config_struct.back_color.blue,
		65355);

	gtk_box_pack_start (GTK_BOX (box),
			    gnome_color_selector_get_button (sel),
			    FALSE, FALSE, CONFIG_PADDING_SIZE);

	gtk_signal_connect (GTK_OBJECT (non), "toggled", 
			    GTK_SIGNAL_FUNC (set_back), 
			    ITOP(PANEL_BACK_NONE));
	gtk_signal_connect (GTK_OBJECT (pix), "toggled", 
			    GTK_SIGNAL_FUNC (set_back), 
			    ITOP(PANEL_BACK_PIXMAP));
	gtk_signal_connect (GTK_OBJECT (col), "toggled", 
			    GTK_SIGNAL_FUNC (set_back), 
			    ITOP(PANEL_BACK_COLOR));
	
	if(panel_config_struct.back_type == PANEL_BACK_NONE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (non), TRUE);
	else if(panel_config_struct.back_type == PANEL_BACK_COLOR)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (col), TRUE);
	else
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (pix), TRUE);

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
	panel_config_struct.back_pixmap = g_strdup(panel->back_pixmap);
	panel_config_struct.back_color = panel->back_color;
	panel_config_struct.back_type = panel->back_type;
	
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
						

	/* Backing configuration */
	page = background_page (panel);
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (config_window),
					page, gtk_label_new (_("Background")));
	
	gtk_signal_connect (GTK_OBJECT (config_window), "apply",
			    GTK_SIGNAL_FUNC (config_apply), panel);
	
	panel_config_struct.config_box = config_window;

	/* show main window */
	gtk_widget_show_all (config_window);
}


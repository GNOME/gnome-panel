#include <gtk/gtk.h>

#include <config.h>
#include <gnome.h>
#include "panel.h"
#include "main.h"
#include "panel-widget.h"
#include "snapped-widget.h"
#include "drawer-widget.h"
#include "corner-widget.h"
#include "panel_config.h"
#include "panel_config_global.h"

/* Used for all the packing and padding options */
#define CONFIG_PADDING_SIZE 3

extern GlobalConfig global_config;

/* used to temporarily store config values until the 'Apply'
   button is pressed. */
typedef struct _PerPanelConfig PerPanelConfig;
struct _PerPanelConfig {
	GtkWidget		*panel;

	/*drawer types*/
	/*nothing!*/
	
	/*snapped types*/
	SnappedPos		snapped_pos;
	SnappedMode		snapped_mode;
	
	/*corner types*/
	CornerPos		corner_pos;
	PanelOrientation	corner_orient;

	int			fit_pixmap_bg;
	PanelBackType		back_type;
	char			*back_pixmap;
	GdkColor		back_color;
	
	int			register_changes; /*used for startup*/
	GtkWidget		*config_window;
	GtkWidget		*pix_entry;
	int			pix_ch_signal;

	/*snapped buttons*/
	GtkWidget		*r_button; /*se*/
	GtkWidget		*l_button; /*nw*/
	GtkWidget		*t_button; /*ne*/
	GtkWidget		*b_button; /*sw*/
	GtkWidget		*non;
	GtkWidget		*pix;
	GtkWidget		*col;
	GnomeColorSelector	*backsel;
};

static GList *ppconfigs=NULL;

static PerPanelConfig *
get_config_struct(GtkWidget *panel)
{
	GList *list;
	for(list=ppconfigs;list!=NULL;list=g_list_next(list)) {
		PerPanelConfig *ppc = list->data;
		if(ppc->panel == panel)
			return ppc;
	}
	return NULL;
}

void
update_config_orient(GtkWidget *panel)
{
	PerPanelConfig *ppc = get_config_struct(panel);
	if(!ppc)
		return;
	if(IS_SNAPPED_WIDGET(panel)) {
		switch(SNAPPED_WIDGET(panel)->pos) {
		case SNAPPED_TOP:
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->t_button),
						    TRUE);
			break;
		case SNAPPED_BOTTOM:
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->b_button),
						    TRUE);
			break;
		case SNAPPED_LEFT:
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->l_button),
						    TRUE);
			break;
		case SNAPPED_RIGHT:
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->r_button),
						    TRUE);
			break;
		}
	} else if(IS_CORNER_WIDGET(panel)) {
		switch(CORNER_WIDGET(panel)->pos) {
		case CORNER_NE:
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->t_button),
						    TRUE);
			break;
		case CORNER_SW:
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->b_button),
						    TRUE);
			break;
		case CORNER_NW:
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->l_button),
						    TRUE);
			break;
		case CORNER_SE:
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->r_button),
						    TRUE);
			break;
		}
	}
}

void
update_config_back(GtkWidget *panel)
{
	GtkWidget *t;
	PerPanelConfig *ppc = get_config_struct(panel);
	/*if the panel has more panel_widgets they will all
	  have the same background so we don't care which one
	  we get*/
	PanelWidget *pw = get_def_panel_widget(panel);

	if(!ppc)
		return;
	switch(pw->back_type) {
	case PANEL_BACK_NONE:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->non),
					    TRUE);
		break;
	case PANEL_BACK_COLOR:
		gnome_color_selector_set_color_int(ppc->backsel,
			pw->back_color.red,
			pw->back_color.green,
			pw->back_color.blue,
			65355);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->col),
					    TRUE);
		break;
	case PANEL_BACK_PIXMAP:
		t=gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(ppc->pix_entry));
		gtk_entry_set_text(GTK_ENTRY(t),pw->back_pixmap);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->pix),
					    TRUE);
		break;
	}
}

static int 
config_destroy(GtkWidget *widget, gpointer data)
{
	PerPanelConfig *ppc = data;
	GtkWidget *t;
	
	t=gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(ppc->pix_entry));
	gtk_signal_disconnect(GTK_OBJECT(t),
			      ppc->pix_ch_signal);
	
	ppconfigs = g_list_remove(ppconfigs,ppc);
	
	g_free(ppc->back_pixmap);
	g_free(ppc);

	return FALSE;
}

static void
config_apply (GtkWidget *widget, int page, gpointer data)
{
	PerPanelConfig *ppc = data;
	
	if(IS_SNAPPED_WIDGET(ppc->panel))
		snapped_widget_change_params(SNAPPED_WIDGET(ppc->panel),
					     ppc->snapped_pos,
					     ppc->snapped_mode,
					     SNAPPED_WIDGET(ppc->panel)->state,
					     ppc->back_type,
					     ppc->back_pixmap,
					     ppc->fit_pixmap_bg,
					     &ppc->back_color);
	else if(IS_CORNER_WIDGET(ppc->panel))
		corner_widget_change_params(CORNER_WIDGET(ppc->panel),
					    ppc->corner_pos,
					    ppc->corner_orient,
					    CORNER_WIDGET(ppc->panel)->state,
					    ppc->back_type,
					    ppc->back_pixmap,
					    ppc->fit_pixmap_bg,
					    &ppc->back_color);
	else if(IS_DRAWER_WIDGET(ppc->panel)) {
		PanelWidget *pw =
			PANEL_WIDGET(DRAWER_WIDGET(ppc->panel)->panel);
		panel_widget_change_params(pw,
					   pw->orient,
					   ppc->back_type,
					   ppc->back_pixmap,
					   ppc->fit_pixmap_bg,
					   &ppc->back_color);
	}
	gtk_widget_queue_draw (ppc->panel);
}

static void
snapped_set_pos (GtkWidget *widget, gpointer data)
{
	SnappedPos pos = (SnappedPos) data;
	PerPanelConfig *ppc = gtk_object_get_user_data(GTK_OBJECT(widget));

	if(!(GTK_TOGGLE_BUTTON(widget)->active))
		return;
	
	ppc->snapped_pos = pos;
	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
}

static void
snapped_set_mode (GtkWidget *widget, gpointer data)
{
	SnappedMode mode = (SnappedMode) data;
	PerPanelConfig *ppc = gtk_object_get_user_data(GTK_OBJECT(widget));

	if(!(GTK_TOGGLE_BUTTON(widget)->active))
		return;
	
	ppc->snapped_mode = mode;
	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
}


static GtkWidget *
snapped_notebook_page(PerPanelConfig *ppc)
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
	gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	
	/* table for frame */
	table = gtk_table_new(3, 3, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), CONFIG_PADDING_SIZE);
	gtk_table_set_col_spacings(GTK_TABLE(table), CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER(table), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), table);
	
	/* Top Position */
	ppc->t_button = gtk_radio_button_new_with_label (NULL, _("Top"));
	gtk_object_set_user_data(GTK_OBJECT(ppc->t_button),ppc);
	gtk_signal_connect (GTK_OBJECT (ppc->t_button), "toggled", 
			    GTK_SIGNAL_FUNC (snapped_set_pos), 
			    (gpointer)SNAPPED_TOP);
        gtk_table_attach(GTK_TABLE(table), ppc->t_button, 1, 2, 0, 1,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);
	
	/* Bottom Position */
	ppc->b_button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (ppc->t_button)),
			  _("Bottom"));
	gtk_object_set_user_data(GTK_OBJECT(ppc->b_button),ppc);
	gtk_signal_connect (GTK_OBJECT (ppc->b_button), "toggled", 
			    GTK_SIGNAL_FUNC (snapped_set_pos), 
			    (gpointer)SNAPPED_BOTTOM);
        gtk_table_attach(GTK_TABLE(table), ppc->b_button, 1, 2, 2, 3,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);
	
	/* Left Position */
	ppc->l_button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (ppc->t_button)),
			  _("Left"));
	gtk_object_set_user_data(GTK_OBJECT(ppc->l_button),ppc);
	gtk_signal_connect (GTK_OBJECT (ppc->l_button), "toggled", 
			    GTK_SIGNAL_FUNC (snapped_set_pos), 
			    (gpointer)SNAPPED_LEFT);
        gtk_table_attach(GTK_TABLE(table), ppc->l_button, 0, 1, 1, 2,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);

	/* Right Position */
	ppc->r_button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (ppc->t_button)),
			  _("Right"));
	gtk_object_set_user_data(GTK_OBJECT(ppc->r_button),ppc);
	gtk_signal_connect (GTK_OBJECT (ppc->r_button), "toggled", 
			    GTK_SIGNAL_FUNC (snapped_set_pos), 
			    (gpointer)SNAPPED_RIGHT);
        gtk_table_attach(GTK_TABLE(table), ppc->r_button, 2, 3, 1, 2,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);

	switch(ppc->snapped_pos) {
	case SNAPPED_TOP:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->t_button),
					    TRUE);
		break;
	case SNAPPED_BOTTOM:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->b_button),
					    TRUE);
		break;
	case SNAPPED_LEFT:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->l_button),
					    TRUE);
		break;
	case SNAPPED_RIGHT:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->r_button),
					    TRUE);
		break;
	}

	/* Auto-hide/stayput frame */
	frame = gtk_frame_new (_("Minimize Options"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Stay Put */
	button = gtk_radio_button_new_with_label (NULL, _("Explicitly Hide"));
	gtk_object_set_user_data(GTK_OBJECT(button),ppc);
	if (ppc->snapped_mode == SNAPPED_EXPLICIT_HIDE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (snapped_set_mode), 
			    (gpointer)SNAPPED_EXPLICIT_HIDE);
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);
	
	/* Auto-hide */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Auto Hide"));
	gtk_object_set_user_data(GTK_OBJECT(button),ppc);
	if (ppc->snapped_mode == SNAPPED_AUTO_HIDE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (snapped_set_mode), 
			    (gpointer)SNAPPED_AUTO_HIDE);
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	return (hbox);
}

static void
corner_set_pos (GtkWidget *widget, gpointer data)
{
	CornerPos pos = (CornerPos) data;
	PerPanelConfig *ppc = gtk_object_get_user_data(GTK_OBJECT(widget));

	if(!(GTK_TOGGLE_BUTTON(widget)->active))
		return;
	
	ppc->corner_pos = pos;
	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
}

static void
corner_set_orient (GtkWidget *widget, gpointer data)
{
	PanelOrientation orient = (PanelOrientation) data;
	PerPanelConfig *ppc = gtk_object_get_user_data(GTK_OBJECT(widget));

	if(!(GTK_TOGGLE_BUTTON(widget)->active))
		return;
	
	ppc->corner_orient = orient;
	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
}

static GtkWidget *
corner_notebook_page(PerPanelConfig *ppc)
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
	gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);
	
	/* table for frame */
	table = gtk_table_new(3, 3, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), CONFIG_PADDING_SIZE);
	gtk_table_set_col_spacings(GTK_TABLE(table), CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER(table), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), table);
	
	/* North East Position */
	ppc->t_button = gtk_radio_button_new_with_label (NULL, _("North East"));
	gtk_object_set_user_data(GTK_OBJECT(ppc->t_button),ppc);
	gtk_signal_connect (GTK_OBJECT (ppc->t_button), "toggled", 
			    GTK_SIGNAL_FUNC (corner_set_pos), 
			    (gpointer)CORNER_NE);
        gtk_table_attach(GTK_TABLE(table), ppc->t_button, 2, 3, 0, 1,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);
	
	/* Bottom Position */
	ppc->b_button = gtk_radio_button_new_with_label (
		 gtk_radio_button_group (GTK_RADIO_BUTTON (ppc->t_button)),
		 _("South West"));
	gtk_object_set_user_data(GTK_OBJECT(ppc->b_button),ppc);
	gtk_signal_connect (GTK_OBJECT (ppc->b_button), "toggled", 
			    GTK_SIGNAL_FUNC (corner_set_pos), 
			    (gpointer)CORNER_SW);
        gtk_table_attach(GTK_TABLE(table), ppc->b_button, 0, 1, 2, 3,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);
	
	/* North West Position */
	ppc->l_button = gtk_radio_button_new_with_label (
		  gtk_radio_button_group (GTK_RADIO_BUTTON (ppc->t_button)),
		  _("North West"));
	gtk_object_set_user_data(GTK_OBJECT(ppc->l_button),ppc);
	gtk_signal_connect (GTK_OBJECT (ppc->l_button), "toggled", 
			    GTK_SIGNAL_FUNC (corner_set_pos), 
			    (gpointer)CORNER_NW);
        gtk_table_attach(GTK_TABLE(table), ppc->l_button, 0, 1, 0, 1,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);

	/* South East Position */
	ppc->r_button = gtk_radio_button_new_with_label (
		 gtk_radio_button_group (GTK_RADIO_BUTTON (ppc->t_button)),
		 _("South East"));
	gtk_object_set_user_data(GTK_OBJECT(ppc->r_button),ppc);
	gtk_signal_connect (GTK_OBJECT (ppc->r_button), "toggled", 
			    GTK_SIGNAL_FUNC (corner_set_pos), 
			    (gpointer)CORNER_SE);
        gtk_table_attach(GTK_TABLE(table), ppc->r_button, 2, 3, 2, 3,
			 GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_SHRINK, 0, 0);

	switch(ppc->corner_pos) {
	case CORNER_NE:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->t_button),
					    TRUE);
		break;
	case CORNER_SW:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->b_button),
					    TRUE);
		break;
	case CORNER_NW:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->l_button),
					    TRUE);
		break;
	case CORNER_SE:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ppc->r_button),
					    TRUE);
		break;
	}

	/* Orientation frame */
	frame = gtk_frame_new (_("Orientation"));
	gtk_container_border_width(GTK_CONTAINER (frame), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, CONFIG_PADDING_SIZE);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Horizontal */
	button = gtk_radio_button_new_with_label (NULL, _("Horizontal"));
	gtk_object_set_user_data(GTK_OBJECT(button),ppc);
	if (ppc->corner_orient == PANEL_HORIZONTAL)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (corner_set_orient), 
			    (gpointer)PANEL_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);
	
	/* Vertical */
	button = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
			  _("Vertical"));
	gtk_object_set_user_data(GTK_OBJECT(button),ppc);
	if (ppc->corner_orient == PANEL_VERTICAL)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (corner_set_orient), 
			    (gpointer)PANEL_VERTICAL);
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE,
			    CONFIG_PADDING_SIZE);

	return (hbox);
}


static int
value_changed (GtkWidget *w, gpointer data)
{
	PerPanelConfig *ppc = data;

	g_free(ppc->back_pixmap);
	ppc->back_pixmap = g_strdup(gtk_entry_get_text(GTK_ENTRY(w)));
	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
	return FALSE;
}

static void
set_fit_pixmap_bg (GtkToggleButton *toggle, gpointer data)
{
	PerPanelConfig *ppc = data;
	ppc->fit_pixmap_bg = toggle->active;

	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
}

static void
color_changed_cb(GnomeColorSelector *sel, gpointer data)
{
 	int r,g,b;
	PerPanelConfig *ppc = data;

        gnome_color_selector_get_color_int(sel,&r,&g,&b, 65355);

	ppc->back_color.red = r;
	ppc->back_color.green = g;
	ppc->back_color.blue =b;
	
	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
}
			   
static int
set_back (GtkWidget *widget, gpointer data)
{
	GtkWidget *pixf,*colf;
	PerPanelConfig *ppc = gtk_object_get_user_data(GTK_OBJECT(widget));
	PanelBackType back_type = PTOI(data);

	if(!GTK_TOGGLE_BUTTON(widget)->active)
		return FALSE;

	pixf = gtk_object_get_data(GTK_OBJECT(widget),"pix");
	colf = gtk_object_get_data(GTK_OBJECT(widget),"col");
	
	if(back_type == PANEL_BACK_NONE) {
		gtk_widget_set_sensitive(pixf,FALSE);
		gtk_widget_set_sensitive(colf,FALSE);
	} else if(back_type == PANEL_BACK_COLOR) {
		gtk_widget_set_sensitive(pixf,FALSE);
		gtk_widget_set_sensitive(colf,TRUE);
	} else  {
		gtk_widget_set_sensitive(pixf,TRUE);
		gtk_widget_set_sensitive(colf,FALSE);
	}
	
	ppc->back_type = back_type;

	if (ppc->register_changes)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (ppc->config_window));
	return FALSE;
}


static GtkWidget *
background_page (PerPanelConfig *ppc)
{
	GtkWidget *box, *f, *t;
	GtkWidget *vbox;
	GtkWidget *w;

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
	ppc->non = gtk_radio_button_new_with_label (NULL, _("Standard"));
	gtk_box_pack_start (GTK_BOX (box), ppc->non, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	
	gtk_object_set_user_data(GTK_OBJECT(ppc->non),ppc);

	/* pixmap */
	ppc->pix = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (ppc->non)),
		_("Pixmap"));
	gtk_box_pack_start (GTK_BOX (box), ppc->pix, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	
	gtk_object_set_user_data(GTK_OBJECT(ppc->pix),ppc);
	
	/* color */
	ppc->col = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (ppc->non)),
		_("Color"));
	gtk_box_pack_start (GTK_BOX (box), ppc->col, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);	
	gtk_object_set_user_data(GTK_OBJECT(ppc->col),ppc);

	/*image frame*/
	f = gtk_frame_new (_("Image file"));
	if(ppc->back_type == PANEL_BACK_PIXMAP) {
		gtk_widget_set_sensitive(f,TRUE);
	} else  {
		gtk_widget_set_sensitive(f,FALSE);
	}
	gtk_object_set_data(GTK_OBJECT(ppc->pix),"pix",f);
	gtk_object_set_data(GTK_OBJECT(ppc->col),"pix",f);
	gtk_object_set_data(GTK_OBJECT(ppc->non),"pix",f);
	gtk_container_border_width(GTK_CONTAINER (f), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (vbox), f, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);

	box = gtk_vbox_new (0, 0);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (f), box);

	ppc->pix_entry = gnome_file_entry_new ("pixmap", _("Browse"));


	t = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (ppc->pix_entry));
	ppc->pix_ch_signal = 
		gtk_signal_connect (GTK_OBJECT (t), "changed",
				    GTK_SIGNAL_FUNC (value_changed), ppc);
	gtk_box_pack_start (GTK_BOX (box), ppc->pix_entry, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);
	
	gtk_entry_set_text (GTK_ENTRY(t),
			    ppc->back_pixmap?ppc->back_pixmap:"");

	w = gtk_check_button_new_with_label (_("Scale image to fit panel"));
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w),
				     ppc->fit_pixmap_bg);
	gtk_signal_connect (GTK_OBJECT (w), "toggled",
			    GTK_SIGNAL_FUNC (set_fit_pixmap_bg),
			    ppc);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE,
			    CONFIG_PADDING_SIZE);


	/*color frame*/
	box = gtk_hbox_new (0, 0);
	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);
	f = gtk_frame_new (_("Background color"));
	if(ppc->back_type == PANEL_BACK_COLOR) {
		gtk_widget_set_sensitive(f,TRUE);
	} else  {
		gtk_widget_set_sensitive(f,FALSE);
	}
	gtk_object_set_data(GTK_OBJECT(ppc->pix),"col",f);
	gtk_object_set_data(GTK_OBJECT(ppc->col),"col",f);
	gtk_object_set_data(GTK_OBJECT(ppc->non),"col",f);
	gtk_container_border_width(GTK_CONTAINER (f), CONFIG_PADDING_SIZE);
	gtk_box_pack_start (GTK_BOX (box), f, FALSE, FALSE, 
			    CONFIG_PADDING_SIZE);

	box = gtk_vbox_new (0, 0);
	gtk_container_border_width(GTK_CONTAINER (box), CONFIG_PADDING_SIZE);
	gtk_container_add (GTK_CONTAINER (f), box);

	ppc->backsel = gnome_color_selector_new(color_changed_cb, ppc);
        gnome_color_selector_set_color_int(ppc->backsel,
		ppc->back_color.red,
		ppc->back_color.green,
		ppc->back_color.blue,
		65355);

	gtk_box_pack_start (GTK_BOX (box),
			    gnome_color_selector_get_button (ppc->backsel),
			    FALSE, FALSE, CONFIG_PADDING_SIZE);

	gtk_signal_connect (GTK_OBJECT (ppc->non), "toggled", 
			    GTK_SIGNAL_FUNC (set_back), 
			    ITOP(PANEL_BACK_NONE));
	gtk_signal_connect (GTK_OBJECT (ppc->pix), "toggled", 
			    GTK_SIGNAL_FUNC (set_back), 
			    ITOP(PANEL_BACK_PIXMAP));
	gtk_signal_connect (GTK_OBJECT (ppc->col), "toggled", 
			    GTK_SIGNAL_FUNC (set_back), 
			    ITOP(PANEL_BACK_COLOR));
	
	if(ppc->back_type == PANEL_BACK_NONE)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (ppc->non), TRUE);
	else if(ppc->back_type == PANEL_BACK_COLOR)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (ppc->col), TRUE);
	else
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (ppc->pix), TRUE);

	return vbox;
}
	     
void 
panel_config(GtkWidget *panel)
{
	GtkWidget *page;
	PerPanelConfig *ppc;
	
	ppc = get_config_struct(panel);
	
	/* return if the window is already up. */
	if (ppc) {
		gdk_window_raise(ppc->config_window->window);
		return;
	}
	
	ppc = g_new(PerPanelConfig,1);
	ppconfigs = g_list_prepend(ppconfigs,ppc);
	ppc->register_changes = FALSE; /*don't notify property box of changes
					 until everything is all set up*/

	if(IS_SNAPPED_WIDGET(panel)) {
		SnappedWidget *snapped = SNAPPED_WIDGET(panel);
		PanelWidget *pw = PANEL_WIDGET(SNAPPED_WIDGET(panel)->panel);
		ppc->snapped_pos = snapped->pos;
		ppc->snapped_mode = snapped->mode;
		ppc->fit_pixmap_bg = pw->fit_pixmap_bg;
		ppc->back_pixmap = g_strdup(pw->back_pixmap);
		ppc->back_color = pw->back_color;
		ppc->back_type = pw->back_type;
	} else if(IS_CORNER_WIDGET(panel)) {
		CornerWidget *corner = CORNER_WIDGET(panel);
		PanelWidget *pw = PANEL_WIDGET(CORNER_WIDGET(panel)->panel);
		ppc->corner_pos = corner->pos;
		ppc->corner_orient = pw->orient;
		ppc->fit_pixmap_bg = pw->fit_pixmap_bg;
		ppc->back_pixmap = g_strdup(pw->back_pixmap);
		ppc->back_color = pw->back_color;
		ppc->back_type = pw->back_type;
	} else if(IS_DRAWER_WIDGET(panel)) {
		PanelWidget *pw = PANEL_WIDGET(DRAWER_WIDGET(panel)->panel);
		ppc->fit_pixmap_bg = pw->fit_pixmap_bg;
		ppc->back_pixmap = g_strdup(pw->back_pixmap);
		ppc->back_color = pw->back_color;
		ppc->back_type = pw->back_type;
	}
	
	ppc->panel = panel;
	
	/* main window */
	ppc->config_window = gnome_property_box_new ();
	gtk_signal_connect(GTK_OBJECT(ppc->config_window), "destroy",
			   GTK_SIGNAL_FUNC (config_destroy), ppc);
	gtk_window_set_title (GTK_WINDOW(ppc->config_window),
			      _("Panel properties"));
	gtk_container_border_width (GTK_CONTAINER(ppc->config_window),
				    CONFIG_PADDING_SIZE);
	
	if(IS_SNAPPED_WIDGET(panel)) {
		/* Snapped notebook page */
		page = snapped_notebook_page (ppc);
		gnome_property_box_append_page (
				GNOME_PROPERTY_BOX (ppc->config_window),
				page, gtk_label_new (_("Edge Panel")));
	} else if(IS_CORNER_WIDGET(panel)) {
		/* Corner notebook page */
		page = corner_notebook_page (ppc);
		gnome_property_box_append_page (
				GNOME_PROPERTY_BOX (ppc->config_window),
				page, gtk_label_new (_("Corner Panel")));
	}
						

	/* Backing configuration */
	page = background_page (ppc);
	gnome_property_box_append_page (GNOME_PROPERTY_BOX (ppc->config_window),
					page, gtk_label_new (_("Background")));
	
	gtk_signal_connect (GTK_OBJECT (ppc->config_window), "apply",
			    GTK_SIGNAL_FUNC (config_apply), ppc);
	
	ppc->register_changes = TRUE;

	/* show main window */
	gtk_widget_show_all (ppc->config_window);
}


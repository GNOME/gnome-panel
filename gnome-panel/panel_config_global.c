#include "config.h"
#include <gnome.h>
#include <string.h>

#include "panel-include.h"

/* used to temporarily store config values until the 'Apply'
 * button is pressed. */
GlobalConfig temp_config;

extern GlobalConfig global_config;
extern int config_sync_timeout;
extern int panels_to_sync;
extern int applets_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

static GtkWidget *aniframe[2];

static GtkWidget *tilefile[LAST_TILE];
static GtkWidget *tileborder[LAST_TILE];

static GtkWidget *entry_up[LAST_TILE];
static GtkWidget *entry_down[LAST_TILE];

static GtkWidget *config_window;

static void
config_destroy(GtkWidget *widget, gpointer data)
{
	config_window = NULL;
}

static void 
set_toggle_button_value (GtkWidget *widget, gpointer data)
{
	*(int *)data=(GTK_TOGGLE_BUTTON(widget)->active==TRUE);
	if(config_window)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
}


static void
config_apply (GtkWidget *widget, int page, gpointer data)
{
	int i;
	
	if(page!=-1)
		return;
	
	for(i=0;i<LAST_TILE;i++) {
		g_free(global_config.tile_up[i]);
		g_free(global_config.tile_down[i]);
	}
	memcpy(&global_config,&temp_config,sizeof(GlobalConfig));

	for(i=0;i<LAST_TILE;i++) {
		global_config.tile_up[i] =
			gnome_icon_entry_get_filename(GNOME_ICON_ENTRY(entry_up[i]));
		global_config.tile_down[i] =
			gnome_icon_entry_get_filename(GNOME_ICON_ENTRY(entry_down[i]));
	}

	apply_global_config();

	globals_to_sync = TRUE;
	panel_config_sync();
}

static void
destroy_egg(GtkWidget *widget, int **pages)
{
	if(pages)
		*pages = 0;
}

/*thy evil easter egg*/
static int
config_event(GtkWidget *widget,GdkEvent *event,GtkNotebook *nbook)
{
	GtkWidget *w;
	char *file;
	static int clicks=0;
	static int pages=0;
	GdkEventButton *bevent;
	
	if(event->type != GDK_BUTTON_PRESS)
		return FALSE;
	
	bevent = (GdkEventButton *)event;
	if(bevent->button != 3)
		clicks = 0;
	else
		clicks++;
	
	if(clicks<3)
		return FALSE;
	clicks = 0;
	
	if(pages==0) {
		file = gnome_unconditional_pixmap_file("gnome-gegl.png");
		if (file && g_file_exists (file))
			w = gnome_pixmap_new_from_file (file);
		else
			w = gtk_label_new("<insert picture of goat here>");
		g_free(file);
		gtk_widget_show(w);
		/*the GEGL shall not be translated*/
		gtk_notebook_append_page (nbook, w,
					  gtk_label_new ("GEGL"));
		gtk_notebook_set_page(nbook,-1);
		pages = 1;
		gtk_signal_connect(GTK_OBJECT(widget),"destroy",
				   GTK_SIGNAL_FUNC(destroy_egg),&pages);
	} else {
		gtk_notebook_set_page(nbook,-1);
	}
	return FALSE;
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

static GtkWidget *
make_int_scale_box (char *title, int *data,
		    double min, double max, double step)
{
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *scale;
	GtkObject *scale_data;

	/* vbox for frame */
	box = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_widget_show (box);

	/* scale label */
	label = gtk_label_new(title);
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 1.0);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);
	gtk_widget_show (label);

	/* Animation step_size scale */
	scale_data = gtk_adjustment_new((double) (*data),
					min, max, step, step, 0.0);
	scale = gtk_hscale_new (GTK_ADJUSTMENT (scale_data));
	gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
	gtk_scale_set_digits (GTK_SCALE (scale), 0);
	gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
	gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
	gtk_box_pack_start (GTK_BOX (box), scale, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT (scale_data), 
			   "value_changed",
			   GTK_SIGNAL_FUNC (int_scale_update),
			   data);
	gtk_widget_show (scale);

	return box;
}

static int
set_anim_button_value(GtkWidget *w, gpointer data)
{
	int disable = !(GTK_TOGGLE_BUTTON(w)->active);

	gtk_widget_set_sensitive(aniframe[0],!disable);
	gtk_widget_set_sensitive(aniframe[1],!disable);
	temp_config.disable_animations = disable;

	if(config_window)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
	return FALSE;
}

static GtkWidget *
animation_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *box;
	GtkWidget *vbox;
	GtkWidget *button;
	GtkWidget *w;
	GtkWidget *frame_vbox;

	/* main vbox */
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), GNOME_PAD_SMALL);

	/* Animation enable */
	button = gtk_check_button_new_with_label (_("Enable animations"));
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_anim_button_value),NULL); 
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	w = gtk_check_button_new_with_label (_("Constant speed animations"));
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	/*we have to do this after everything we need aniframe varaibles set*/
	if (temp_config.simple_movement)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
	/*note it's not a frame, but it's disabled anyhow*/
	aniframe[0] = w;
	if (temp_config.disable_animations)
		gtk_widget_set_sensitive(w,FALSE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.simple_movement));

	/* AutoHide Animation step_size scale frame */
	frame = gtk_frame_new (_("Animation speed"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	if (temp_config.disable_animations)
		gtk_widget_set_sensitive (frame, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show (frame);
	aniframe[1] = frame;

	frame_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (frame_vbox), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), frame_vbox);
	gtk_widget_show (frame_vbox);

	box = make_int_scale_box(_("Auto hide"),
				 &(temp_config.auto_hide_step_size),
				 1.0, 100.0, 1.0);
	gtk_box_pack_start (GTK_BOX (frame_vbox), box, TRUE, FALSE,0);
	gtk_widget_show (box);

	/* ExplicitHide Animation step_size scale frame */
	box = make_int_scale_box(_("Explicit hide"),
				   &(temp_config.explicit_hide_step_size),
				   1.0, 100.0, 1.0);
	gtk_box_pack_start (GTK_BOX (frame_vbox), box, TRUE, FALSE,0);

	/* DrawerHide Animation step_size scale frame */
	box = make_int_scale_box(_("Drawer sliding"),
				   &(temp_config.drawer_step_size),
				   1.0, 100.0, 1.0);
	gtk_box_pack_start (GTK_BOX (frame_vbox), box, TRUE, FALSE,0);

	
	frame = gtk_frame_new (_("Auto hide"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show (frame);

	frame_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (frame_vbox), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), frame_vbox);
	gtk_widget_show (frame_vbox);

	/* Minimize Delay scale frame */
	box = make_int_scale_box (_("Delay (ms)"),
				  &(temp_config.minimize_delay),
				  30.0, 10000.0, 10.0);
	gtk_box_pack_start (GTK_BOX (frame_vbox), box, TRUE, FALSE, 0);

	/* Minimized size scale frame */
	box = make_int_scale_box (_("Size (pixels)"),
				  &(temp_config.minimized_size),
				  1.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (frame_vbox), box, TRUE, FALSE, 0);

	/*we have to do this after everything we need aniframe varaibles set*/
	if (!temp_config.disable_animations)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

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

static int
set_icon_button_value(GtkWidget *w, gpointer data)
{
	int i = GPOINTER_TO_INT(data);
	int active = GTK_TOGGLE_BUTTON(w)->active;

	gtk_widget_set_sensitive(tilefile[i],active);
	gtk_widget_set_sensitive(tileborder[i],active);
	temp_config.tiles_enabled[i] = active; 

	if(config_window)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
	return FALSE;
}

enum {
	LAUNCHER_PAGE,
	DRAWER_PAGE,
	MENU_PAGE,
	SPECIAL_PAGE,
	N_PAGES
};

static gint
show_page (GtkWidget *w, gpointer data)
{
	GtkWidget *notebook = gtk_object_get_user_data (GTK_OBJECT (w));
	gtk_notebook_set_page (GTK_NOTEBOOK (notebook), GPOINTER_TO_INT (data));
	return FALSE;
}

static GtkWidget *
icon_notebook_page(int i, GtkWidget *config_box)
{
	GtkWidget *frame;
	GtkWidget *w;
	GtkWidget *table;
	GtkWidget *vbox;
	GtkWidget *toggle;

        char *icon_titles[]={
                N_("Launcher icon"),
                N_("Drawer icon"),
                N_("Menu icon"),
                N_("Special icon")};

	/* Image frame */
	frame = gtk_frame_new (_(icon_titles[i]));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_widget_set_sensitive(frame,temp_config.tiles_enabled[i]);
	
	/* main vbox */
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	/* toggle button */
 	toggle = gtk_check_button_new_with_label (_("Tiles enabled"));
	if (temp_config.tiles_enabled[i])
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
	gtk_signal_connect (GTK_OBJECT (toggle), "toggled", 
			    GTK_SIGNAL_FUNC (set_icon_button_value), 
			    GINT_TO_POINTER(i));
	gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
	
	
	/* table for frame */
	tilefile[i] = table = gtk_table_new(2,3,FALSE);
	gtk_container_set_border_width(GTK_CONTAINER (table), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	
	/* image file entry widgets */
	entry_up[i] = create_icon_entry(table,"tile_file",0, 1,
					_("Normal Tile"),
					global_config.tile_up[i],
					config_box);
	entry_down[i] = create_icon_entry(table,"tile_file",1, 2,
					  _("Clicked Tile"),
					  global_config.tile_down[i],
					  config_box);

	w = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Minimized size scale frame */
	tileborder[i] = w = make_int_scale_box (_("Border width (tile only)"),
						&(temp_config.tile_border[i]),
						0.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(w,temp_config.tiles_enabled[i]); 

	/* Minimized size scale frame */
	w = make_int_scale_box (_("Depth (displacement when pressed)"),
				&(temp_config.tile_depth[i]),
				0.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	return frame;
}

static GtkWidget *
buttons_notebook_page (GtkWidget *config_box)
{
	  GtkWidget *m, *w;
	  GtkWidget *box;
	  GtkWidget *vbox;
	  GtkWidget *label;
	  GtkWidget *notebook;
	  GtkWidget *page;
	  int i;

	  const char *labels[] = { N_("Launcher"),
				  N_("Drawer"),
				  N_("Menu"),
				  N_("Special") };

	  /* main vbox */
	  vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	  gtk_container_set_border_width(GTK_CONTAINER (vbox), GNOME_PAD_SMALL);

	  box = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
	  gtk_widget_show (box);

	  label = gtk_label_new (_("Button Type: "));
	  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	  gtk_widget_show (label);

	  m = gtk_menu_new ();
	  notebook = gtk_notebook_new ();
	  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	  gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);

	  for (i = 0; i < N_PAGES; i++) {	  
		  w = gtk_menu_item_new_with_label (_(labels[i]));
		  gtk_signal_connect (GTK_OBJECT (w), "activate",
				      GTK_SIGNAL_FUNC (show_page),
				      GINT_TO_POINTER (i));
		  gtk_widget_show (w);
		  gtk_menu_append (GTK_MENU (m), w);

		  page = icon_notebook_page (i, config_box);
		  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, NULL);

		  gtk_object_set_user_data (GTK_OBJECT (w), notebook);
	  }

	  w = gtk_option_menu_new ();
	  gtk_option_menu_set_menu (GTK_OPTION_MENU (w), m);
	  gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	  gtk_widget_show (w);

	  gtk_box_pack_start (GTK_BOX (vbox), notebook, FALSE, FALSE, 0);
	  gtk_widget_show (notebook);
	  
	  /* show/hide frame */
	  w = gtk_check_button_new_with_label (_("Make buttons flush with panel edge"));
	  if (temp_config.hide_panel_frame)
		  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
	  gtk_signal_connect (GTK_OBJECT (w), "toggled",
			      GTK_SIGNAL_FUNC (set_toggle_button_value),
			      &(temp_config.hide_panel_frame));
	  gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	  
	  
	  /* only show tiles when mouse is over the button */
	  w = gtk_check_button_new_with_label (_("Show button tiles only when cursor is over the button"));
	  if (temp_config.tile_when_over)
		  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
	  gtk_signal_connect (GTK_OBJECT (w), "toggled",
			      GTK_SIGNAL_FUNC (set_toggle_button_value),
			      &(temp_config.tile_when_over));
	  gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	  
	  return (vbox);
}




static GtkWidget *
applets_notebook_page (void)
{
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *box;
	GtkWidget *vbox;

	/* main vbox */
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), GNOME_PAD_SMALL);

	/* Movement frame */
	frame = gtk_frame_new (_("Default movement mode"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (box), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Switched */
	button = gtk_radio_button_new_with_label (NULL, _("Switched movement (or use Ctrl)"));
	if (temp_config.movement_type == PANEL_SWITCH_MOVE)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_movement), 
			    (gpointer)PANEL_SWITCH_MOVE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	/* Free */
	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
		_("Free movement (doesn't disturb other applets) (or use Alt)"));
	if (temp_config.movement_type == PANEL_FREE_MOVE)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_movement), 
			    (gpointer)PANEL_FREE_MOVE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	/* Push */
	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
		_("Push movement (or use Shift)"));
	if (temp_config.movement_type == PANEL_PUSH_MOVE)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_movement), 
			    (gpointer)PANEL_PUSH_MOVE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);	

	box = make_int_scale_box(_("Applet padding"),
				 &(temp_config.applet_padding),
				 0.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
	

	return vbox;
}

static GtkWidget *
misc_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *box;
	GtkWidget *table;
	GtkWidget *vbox;
	
	/* main vbox */
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	
	/* Menu frame */
	frame = gtk_frame_new (_("Menus"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	
	/* table for frame */
	table = gtk_table_new(2,2,FALSE);
	gtk_container_set_border_width(GTK_CONTAINER (table), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), table);
	
	/* Small Icons */
	button = gtk_check_button_new_with_label (_("Show small icons"));
	if (temp_config.show_small_icons)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.show_small_icons));
	gtk_table_attach_defaults(GTK_TABLE(table),button, 0,1,0,1);

	/* Dot Buttons */
	button = gtk_check_button_new_with_label (_("Show [...] buttons"));
	if (temp_config.show_dot_buttons)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.show_dot_buttons));
	gtk_table_attach_defaults(GTK_TABLE(table),button, 1,2,0,1);

	/* Off Panel Popup menus */
	button = gtk_check_button_new_with_label (_("Show popup menus outside of panels"));
	if (temp_config.off_panel_popups)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.off_panel_popups));
	gtk_table_attach_defaults(GTK_TABLE(table),button, 0,1,1,2);

	/* Hungry Menus */
	button = gtk_check_button_new_with_label (_("Keep menus in memory"));
	if (temp_config.hungry_menus)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.hungry_menus));
	gtk_table_attach_defaults(GTK_TABLE(table),button, 1,2,1,2);
	
	/* Miscellaneous frame */
	frame = gtk_frame_new (_("Miscellaneous"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER (box), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), box);

	/* Tooltips enable */
	button = gtk_check_button_new_with_label (_("Tooltips enabled"));
	if (temp_config.tooltips_enabled)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.tooltips_enabled));
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);	
	
	/* only show tiles when mouse is over the button */
	button = gtk_check_button_new_with_label (_("Show hints on panel startup"));
	if (temp_config.show_startup_hints)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    GTK_SIGNAL_FUNC (set_toggle_button_value),
			    &(temp_config.show_startup_hints));
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	/* Drawer/launcher auto close */
	button = gtk_check_button_new_with_label (_("Close drawer if a launcher inside it is pressed"));
	if (temp_config.drawer_auto_close)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.drawer_auto_close));
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	/* Autoraise */
	button = gtk_check_button_new_with_label (_("Raise panels on mouse-over (non GNOME compliant window managers only)"));
	if (temp_config.autoraise)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.autoraise));
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	/* Keep on bottom */
	button = gtk_check_button_new_with_label (_("Keep panel below windows (GNOME compliant window managers only)"));
	if (temp_config.keep_bottom)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.keep_bottom));
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	return (vbox);
}

static void
globalcfg_help(void)
{
    gchar *tmp;

    tmp = gnome_help_file_find_file ("users-guide", "confglobal.html");
    if (tmp) {
       gnome_help_goto(0, tmp);
       g_free(tmp);
    }
}

void 
panel_config_global(void)
{
/*      static GnomeHelpMenuEntry help_entry = { NULL, "properties-global" };*/
	GtkWidget *page;
	GtkWidget *box;
	GtkWidget *prop_nbook;
	
	/* return if the window is already up. */
	if (config_window) {
		gdk_window_raise(config_window->window);
		gtk_widget_show(config_window);
		return;
	}

	memcpy(&temp_config,&global_config,sizeof(GlobalConfig));

	/* main window */
	box = gnome_property_box_new ();
	gtk_window_set_wmclass(GTK_WINDOW(box),
			       "global_panel_properties","Panel");
	gtk_widget_set_events(box,
			      gtk_widget_get_events(box) |
			      GDK_BUTTON_PRESS_MASK);
	/*gtk_window_set_position(GTK_WINDOW(box), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(box), FALSE, FALSE, TRUE);
	gtk_signal_connect(GTK_OBJECT(box), "destroy",
			   GTK_SIGNAL_FUNC (config_destroy), NULL);
	gtk_window_set_title (GTK_WINDOW(box),
			      _("Global panel configuration"));
	gtk_container_set_border_width (GTK_CONTAINER(box), GNOME_PAD_SMALL);
	
	prop_nbook = GNOME_PROPERTY_BOX (box)->notebook;

	/* Animation notebook page */
	page = animation_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK(prop_nbook),
				  page, gtk_label_new (_("Animation")));

	/* General icon notebook page */
	page = buttons_notebook_page (box);
	gtk_notebook_append_page (GTK_NOTEBOOK(prop_nbook),
				    page, gtk_label_new (_("Buttons")));


#if 0
	/* Specific icon notebook pages */
	for(i = 0; i<LAST_TILE; i++) {
		page = icon_notebook_page (i,box);
		gtk_notebook_append_page (GTK_NOTEBOOK(prop_nbook),
					  page, gtk_label_new (_(icon_titles[i])));
		
	}
#endif
	/* applet settings */
	page = applets_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK (prop_nbook),
				  page, gtk_label_new (_("Applets")));
		
	/* Miscellaneous notebook page */
	page = misc_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK(prop_nbook),
				  page, gtk_label_new (_("Miscellaneous")));

	gtk_signal_connect (GTK_OBJECT (box), "apply",
			    GTK_SIGNAL_FUNC (config_apply), NULL);

/*	help_entry.name = gnome_app_id;
	gtk_signal_connect (GTK_OBJECT (box), "help",
			    GTK_SIGNAL_FUNC (gnome_help_pbox_display),
			    &help_entry);
*/
	gtk_signal_connect (GTK_OBJECT (box), "help",
			    GTK_SIGNAL_FUNC (globalcfg_help),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (box), "event",
			    GTK_SIGNAL_FUNC (config_event),
			    prop_nbook);
	
	config_window = box;

	/* show main window */
	gtk_widget_show_all (config_window);
}


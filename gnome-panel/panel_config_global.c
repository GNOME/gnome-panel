#include <gtk/gtk.h>

#include <string.h>
#include <config.h>
#include <gnome.h>

#include "panel-include.h"

/* used to temporarily store config values until the 'Apply'
 * button is pressed. */
GlobalConfig temp_config;

extern GlobalConfig global_config;
extern int config_sync_timeout;
extern int panels_to_sync;
extern GSList *applets_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

static GtkWidget *aniframe[4];

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
		if (file && g_file_exists (file)) {
			w = gnome_pixmap_new_from_file (file);
			gtk_widget_show(w);
			/*the GEGL shall not be translated*/
			gtk_notebook_append_page (nbook, w,
						  gtk_label_new ("GEGL"));
			gtk_notebook_set_page(nbook,-1);
			pages = 1;
		}
		g_free(file);
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
make_int_scale_frame(char *title, int *data,
		     double min, double max, double step)
{
	GtkWidget *frame;
	GtkWidget *box;
	GtkWidget *scale;
	GtkObject *scale_data;

	/* scale frame */
	frame = gtk_frame_new (title);
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);

	/* vbox for frame */
	box = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_container_set_border_width(GTK_CONTAINER (box), GNOME_PAD_SMALL);
	gtk_widget_show (box);

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

	return frame;
}

static int
set_anim_button_value(GtkWidget *w, gpointer data)
{
	int i;
	int disable = !(GTK_TOGGLE_BUTTON(w)->active);

	for(i=0;i<4;i++)
		gtk_widget_set_sensitive(aniframe[i],!disable);
	temp_config.disable_animations = disable;

	if(config_window)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (config_window));
	return FALSE;
}

static GtkWidget *
animation_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *button;
	GtkWidget *w;
	
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
	aniframe[3] = w;
	if (temp_config.disable_animations)
		gtk_widget_set_sensitive(w,FALSE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.simple_movement));

	/* AutoHide Animation step_size scale frame */
	frame = make_int_scale_frame(_("Auto-hide animation speed"),
				      &(temp_config.auto_hide_step_size),
				      1.0, 100.0, 1.0);
	if (temp_config.disable_animations)
		gtk_widget_set_sensitive(frame,FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,0);

	aniframe[0] = frame;

	/* ExplicitHide Animation step_size scale frame */
	frame = make_int_scale_frame(_("Explicit-hide animation speed"),
				      &(temp_config.explicit_hide_step_size),
				      1.0, 100.0, 1.0);
	if (temp_config.disable_animations)
		gtk_widget_set_sensitive(frame,FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,0);

	aniframe[1] = frame;

	/* DrawerHide Animation step_size scale frame */
	frame = make_int_scale_frame(_("Drawer animation speed"),
				      &(temp_config.drawer_step_size),
				      1.0, 100.0, 1.0);
	if (temp_config.disable_animations)
		gtk_widget_set_sensitive(frame,FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE,0);

	aniframe[2] = frame;

	/* Minimize Delay scale frame */
	frame = make_int_scale_frame(_("Auto-hide minimize delay (ms)"),
				      &(temp_config.minimize_delay),
				      30.0, 10000.0, 10.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

	/* Minimized size scale frame */
	frame = make_int_scale_frame(_("Auto-hide minimized size (pixels)"),
				      &(temp_config.minimized_size),
				      1.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

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

#if 0
static GtkWidget *
genicon_notebook_page(void)
{
	  GtkWidget *frame;
	  GtkWidget *w;
	  GtkWidget *box;
	  GtkWidget *vbox;
	  
	  /* main vbox */
	  vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	  gtk_container_set_border_width(GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	  
	  /* General frame */
	  frame = gtk_frame_new (_("General"));
	  gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	  
	  /* vbox for frame */
	  box = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	  gtk_container_set_border_width(GTK_CONTAINER (box), GNOME_PAD_SMALL);
	  gtk_container_add (GTK_CONTAINER (frame), box);
	  
	  /* Enable tiles frame */
	  w = gtk_check_button_new_with_label (_("Tiles enabled"));
	  if (temp_config.tiles_enabled[0])
		  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
	  gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			      GTK_SIGNAL_FUNC (set_icon_button_value), 
			      &(temp_config.tiles_enabled[0]));
	  gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	  return (vbox);
}
#endif


static GtkWidget *
icon_notebook_page(int i, GtkWidget *config_box)
{
	GtkWidget *frame;
	GtkWidget *w;
	GtkWidget *table;
	GtkWidget *vbox;
	GtkWidget *toggle;
	
	/* main vbox */
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), GNOME_PAD_SMALL);

	/* toggle button */
 	toggle = gtk_check_button_new_with_label (_("Tiles enabled"));
	if (temp_config.tiles_enabled[i])
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
	gtk_signal_connect (GTK_OBJECT (toggle), "toggled", 
			    GTK_SIGNAL_FUNC (set_icon_button_value), 
			    GINT_TO_POINTER(i));
	gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
	
	/* Image frame */
	tilefile[i] = frame = gtk_frame_new (_("Image files"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(frame,temp_config.tiles_enabled[i]);
	
	/* table for frame */
	table = gtk_table_new(3,3,FALSE);
	gtk_container_set_border_width(GTK_CONTAINER (table), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), table);
	
	/* image file entry widgets */
	entry_up[i] = create_icon_entry(table,"tile_file",1,
					_("Tile filename (up)"),
					global_config.tile_up[i],
					config_box);
	entry_down[i] = create_icon_entry(table,"tile_file",2,
					  _("Tile filename (down)"),
					  global_config.tile_down[i],
					  config_box);


	
	/* Minimized size scale frame */
	tileborder[i] = w = make_int_scale_frame(_("Border width (tile only)"),
						 &(temp_config.tile_border[i]),
						 0.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(w,temp_config.tiles_enabled[i]); 

	/* Minimized size scale frame */
	w = make_int_scale_frame(_("Depth (displacement when pressed)"),
				 &(temp_config.tile_depth[i]),
				 0.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	return (vbox);
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
	
	/* Tooltips frame */
	frame = gtk_frame_new (_("Tooltips"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
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
	button = gtk_check_button_new_with_label (_("Show ... buttons"));
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

	/* Movement frame */
	frame = gtk_frame_new (_("Movement"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (box), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), box);

	/* Switched */
	button = gtk_radio_button_new_with_label (NULL, _("Switched movement"));
	if (temp_config.movement_type == PANEL_SWITCH_MOVE)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_movement), 
			    (gpointer)PANEL_SWITCH_MOVE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	/* Free */
	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
		_("Free movement (doesn't disturb other applets)"));
	if (temp_config.movement_type == PANEL_FREE_MOVE)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_movement), 
			    (gpointer)PANEL_FREE_MOVE);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
	
	/* Miscellaneous frame */
	frame = gtk_frame_new (_("Miscellaneous"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (box), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), box);
	
	/* Prompt before log out */
	button = gtk_check_button_new_with_label (_("Prompt before logout"));
	if (temp_config.prompt_for_logout)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.prompt_for_logout));
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

	/* Drawer/launcher auto close */
	button = gtk_check_button_new_with_label (_("Close drawer if a launcher inside it is pressed"));
	if (temp_config.drawer_auto_close)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(temp_config.drawer_auto_close));
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	/* Minimize Delay scale frame */
	frame = make_int_scale_frame(_("Applet padding"),
				      &(temp_config.applet_padding),
				      0.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

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
	char *icon_titles[]={
		N_("Launcher icon"),
		N_("Drawer icon"),
		N_("Menu icon"),
		N_("Logout icon")};
	int i;
	
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

#if 0
	  /* General icon notebook page */
	  page = genicon_notebook_page ();
	  gtk_notebook_append_page (GTK_NOTEBOOK(prop_nbook),
				    page, gtk_label_new (_("General icon settings")));

#endif
	/* Specific icon notebook pages */
	for(i = 0; i<LAST_TILE; i++) {
		page = icon_notebook_page (i,box);
		gtk_notebook_append_page (GTK_NOTEBOOK(prop_nbook),
					  page, gtk_label_new (_(icon_titles[i])));
		
	}
		
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


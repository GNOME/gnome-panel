/*   gnome-panel-properties: crapplet for global panel properties
 *
 *   Copyright (C) 1999 Free Software Foundation
 *   Author: George Lebl <jirka@5z.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include "capplet-widget.h"

/*we just use the GlobalConfig and LAST_TILE definitions, some panel-util stuff,
  and the DEFAULT_* defines from session.h !!!*/
#include "panel-types.h"
#include "panel_config_global.h"
#include "panel-util.h"
#include "session.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_alphagamma.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libart_lgpl/art_pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libart_lgpl/art_affine.h>
#include "nothing.cP"

static GlobalConfig global_config;
static GlobalConfig loaded_config;

/* animation page */
static GtkWidget *enable_animations_cb;
static GtkWidget *simple_movement_cb;
static GtkWidget *anim_frame;
static GtkAdjustment *auto_hide_step_size;
static GtkAdjustment *explicit_hide_step_size;
static GtkAdjustment *drawer_step_size;
static GtkAdjustment *minimize_delay;
static GtkAdjustment *minimized_size;


/* buttons page */
static GtkWidget *tile_enable_cb[LAST_TILE];
static GtkWidget *tile_file_box[LAST_TILE];
static GtkWidget *tile_border_box[LAST_TILE];
static GtkAdjustment *tile_border[LAST_TILE];
static GtkAdjustment *tile_depth[LAST_TILE];

static GtkWidget *entry_up[LAST_TILE];
static GtkWidget *entry_down[LAST_TILE];

static GtkWidget *hide_panel_frame_cb;
static GtkWidget *tile_when_over_cb;
static GtkWidget *saturate_when_over_cb;


/* applet page*/
static GtkWidget *movement_type_switch_rb;
static GtkWidget *movement_type_free_rb;
static GtkWidget *movement_type_push_rb;
static GtkAdjustment *applet_padding;


/* miscellaneous page */
static GtkWidget *show_small_icons_cb;
static GtkWidget *show_dot_buttons_cb;
static GtkWidget *off_panel_popups_cb;
static GtkWidget *hungry_menus_cb;
static GtkWidget *tooltips_enabled_cb;
static GtkWidget *drawer_auto_close_cb;
static GtkWidget *autoraise_cb;
static GtkWidget *keep_bottom_cb;

static gboolean changing = TRUE;
static GtkWidget *capplet;

static void 
changed_cb(void)
{
	if(!changing)
		capplet_widget_state_changed(CAPPLET_WIDGET(capplet), TRUE);
}

static GtkWidget *
make_int_scale_box (char *title, GtkAdjustment **adj,
		    double min, double max, double step)
{
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *scale;

	/* vbox for frame */
	box = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_widget_show (box);

	/* scale label */
	label = gtk_label_new(title);
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 1.0);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);
	gtk_widget_show (label);

	/* Animation step_size scale */
	*adj = GTK_ADJUSTMENT(gtk_adjustment_new(min, min, max, step, step, 0.0));
	scale = gtk_hscale_new (*adj);
	gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
	gtk_scale_set_digits (GTK_SCALE (scale), 0);
	gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
	gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
	gtk_box_pack_start (GTK_BOX (box), scale, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT (*adj), 
			   "value_changed",
			   GTK_SIGNAL_FUNC (changed_cb),NULL);
	gtk_widget_show (scale);

	return box;
}

static int
set_anim_button_value(GtkWidget *w, gpointer data)
{
	int enable = GTK_TOGGLE_BUTTON(w)->active;

	gtk_widget_set_sensitive(simple_movement_cb,enable);
	gtk_widget_set_sensitive(anim_frame,enable);

	if(!changing)
		capplet_widget_state_changed(CAPPLET_WIDGET(capplet), TRUE);
	return FALSE;
}

static void
sync_animation_page_with_config(GlobalConfig *conf)
{
	gtk_widget_set_sensitive(simple_movement_cb,
				 !conf->disable_animations);
	gtk_widget_set_sensitive(anim_frame,
				 !conf->disable_animations);

	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(enable_animations_cb),
				    /*notice the not*/
				    !conf->disable_animations);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(simple_movement_cb),
				    conf->simple_movement);
	gtk_adjustment_set_value(auto_hide_step_size,
				 conf->auto_hide_step_size);
	gtk_adjustment_set_value(explicit_hide_step_size,
				 conf->explicit_hide_step_size);
	gtk_adjustment_set_value(drawer_step_size,
				 conf->drawer_step_size);
	gtk_adjustment_set_value(minimize_delay,
				 conf->minimize_delay);
	gtk_adjustment_set_value(minimized_size,
				 conf->minimized_size);
}
static void
sync_config_with_animation_page(GlobalConfig *conf)
{
	/*notice the not*/
	conf->disable_animations =
		! GTK_TOGGLE_BUTTON(enable_animations_cb)->active;
	conf->simple_movement =
		GTK_TOGGLE_BUTTON(simple_movement_cb)->active;
	conf->auto_hide_step_size = auto_hide_step_size->value;
	conf->explicit_hide_step_size = explicit_hide_step_size->value;
	conf->drawer_step_size = drawer_step_size->value;
	conf->minimize_delay = minimize_delay->value;
	conf->minimized_size = minimized_size->value;
}

static GtkWidget *
animation_notebook_page(void)
{
	GtkWidget *frame;
	GtkWidget *box;
	GtkWidget *vbox;
	GtkWidget *frame_vbox;

	/* main vbox */
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), GNOME_PAD_SMALL);

	/* Animation enable */
	enable_animations_cb = gtk_check_button_new_with_label (_("Enable animations"));
	gtk_signal_connect (GTK_OBJECT (enable_animations_cb), "toggled", 
			    GTK_SIGNAL_FUNC (set_anim_button_value),NULL); 
	gtk_box_pack_start (GTK_BOX (vbox), enable_animations_cb, FALSE, FALSE, 0);

	simple_movement_cb = gtk_check_button_new_with_label (_("Constant speed animations"));
	gtk_box_pack_start (GTK_BOX (vbox), simple_movement_cb, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (simple_movement_cb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb), NULL);

	/* AutoHide Animation step_size scale frame */
	anim_frame = gtk_frame_new (_("Animation speed"));
	gtk_container_set_border_width(GTK_CONTAINER (anim_frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), anim_frame, TRUE, TRUE, 0);
	gtk_widget_show (anim_frame);

	frame_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (frame_vbox), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (anim_frame), frame_vbox);
	gtk_widget_show (frame_vbox);

	box = make_int_scale_box(_("Auto hide"),
				 &auto_hide_step_size,
				 1.0, 100.0, 1.0);
	gtk_box_pack_start (GTK_BOX (frame_vbox), box, TRUE, FALSE,0);
	gtk_widget_show (box);

	/* ExplicitHide Animation step_size scale frame */
	box = make_int_scale_box(_("Explicit hide"),
				   &explicit_hide_step_size,
				   1.0, 100.0, 1.0);
	gtk_box_pack_start (GTK_BOX (frame_vbox), box, TRUE, FALSE,0);

	/* DrawerHide Animation step_size scale frame */
	box = make_int_scale_box(_("Drawer sliding"),
				   &drawer_step_size,
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
				  &minimize_delay,
				  30.0, 10000.0, 10.0);
	gtk_box_pack_start (GTK_BOX (frame_vbox), box, TRUE, FALSE, 0);

	/* Minimized size scale frame */
	box = make_int_scale_box (_("Size (pixels)"),
				  &minimized_size,
				  1.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (frame_vbox), box, TRUE, FALSE, 0);

	return (vbox);
}

static void
sync_buttons_page_with_config(GlobalConfig *conf)
{
	int i;
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(hide_panel_frame_cb),
				    conf->hide_panel_frame);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(tile_when_over_cb),
				    conf->tile_when_over);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(saturate_when_over_cb),
				    conf->saturate_when_over);
	
	for(i=0;i<LAST_TILE;i++) {
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(tile_enable_cb[i]),
					    conf->tiles_enabled[i]);
		gnome_icon_entry_set_icon(GNOME_ICON_ENTRY(entry_up[i]),
					  conf->tile_up[i]);
		gnome_icon_entry_set_icon(GNOME_ICON_ENTRY(entry_down[i]),
					  conf->tile_down[i]);
		gtk_adjustment_set_value(tile_border[i],conf->tile_border[i]);
		gtk_adjustment_set_value(tile_depth[i],conf->tile_depth[i]);
	}
}
static void
sync_config_with_buttons_page(GlobalConfig *conf)
{
	int i;
	conf->hide_panel_frame =
		GTK_TOGGLE_BUTTON(hide_panel_frame_cb)->active;
	conf->tile_when_over =
		GTK_TOGGLE_BUTTON(tile_when_over_cb)->active;
	conf->saturate_when_over =
		GTK_TOGGLE_BUTTON(saturate_when_over_cb)->active;

	for(i=0;i<LAST_TILE;i++) {
		conf->tiles_enabled[i] =
			GTK_TOGGLE_BUTTON(tile_enable_cb[i])->active;
		g_free(conf->tile_up[i]);
		conf->tile_up[i] =
			gnome_icon_entry_get_filename(GNOME_ICON_ENTRY(entry_up[i]));
		g_free(conf->tile_down[i]);
		conf->tile_down[i] =
			gnome_icon_entry_get_filename(GNOME_ICON_ENTRY(entry_down[i]));
		conf->tile_border[i] = tile_border[i]->value;
		conf->tile_depth[i] = tile_depth[i]->value;
	}
}

static void
set_icon_button(GtkWidget *w, gpointer data)
{
	int i = GPOINTER_TO_INT(data);
	int active = GTK_TOGGLE_BUTTON(w)->active;

	gtk_widget_set_sensitive(tile_file_box[i],active);
	gtk_widget_set_sensitive(tile_border_box[i],active);

	changed_cb();
}

enum {
	LAUNCHER_PAGE,
	DRAWER_PAGE,
	MENU_PAGE,
	SPECIAL_PAGE,
	N_PAGES
};

static void
show_page (GtkWidget *w, gpointer data)
{
	GtkWidget *notebook = gtk_object_get_user_data (GTK_OBJECT (w));
	gtk_notebook_set_page (GTK_NOTEBOOK (notebook), GPOINTER_TO_INT (data));
}

static GtkWidget *
icon_notebook_page(int i)
{
	GtkWidget *frame;
	GtkWidget *w;
	GtkWidget *table;
	GtkWidget *vbox;

        char *icon_titles[]={
                N_("Launcher icon"),
                N_("Drawer icon"),
                N_("Menu icon"),
                N_("Special icon")};

	/* Image frame */
	frame = gtk_frame_new (_(icon_titles[i]));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	
	/* main vbox */
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	/* toggle button */
 	tile_enable_cb[i] = gtk_check_button_new_with_label (_("Tiles enabled"));
	gtk_signal_connect (GTK_OBJECT (tile_enable_cb[i]), "toggled", 
			    GTK_SIGNAL_FUNC (set_icon_button), 
			    GINT_TO_POINTER(i));
	gtk_box_pack_start (GTK_BOX (vbox), tile_enable_cb[i], FALSE, FALSE, 0);
	
	
	/* table for frame */
	tile_file_box[i] = table = gtk_table_new(2,3,FALSE);
	gtk_container_set_border_width(GTK_CONTAINER (table), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	
	/* image file entry widgets */
	entry_up[i] = create_icon_entry(table,"tile_file",0, 1,
					_("Normal Tile"),
					global_config.tile_up[i],
					NULL);
	w = gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (entry_up[i]));
	gtk_signal_connect_while_alive(GTK_OBJECT(w), "changed",
				       GTK_SIGNAL_FUNC(changed_cb), 
				       NULL,
				       GTK_OBJECT(capplet));

	entry_down[i] = create_icon_entry(table,"tile_file",1, 2,
					  _("Clicked Tile"),
					  global_config.tile_down[i],
					  NULL);
	w = gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (entry_down[i]));
	gtk_signal_connect_while_alive(GTK_OBJECT(w), "changed",
				       GTK_SIGNAL_FUNC(changed_cb), 
				       NULL,
				       GTK_OBJECT(capplet));

	w = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
	gtk_widget_show (w);

	/* Minimized size scale frame */
	tile_border_box[i] = w = make_int_scale_box (_("Border width (tile only)"),
						&tile_border[i],
						0.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	/* Minimized size scale frame */
	w = make_int_scale_box (_("Depth (displacement when pressed)"),
				&tile_depth[i],
				0.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	return frame;
}

static GtkWidget *
buttons_notebook_page (void)
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

		  page = icon_notebook_page (i);
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
	  hide_panel_frame_cb = gtk_check_button_new_with_label (_("Make buttons flush with panel edge"));
	  gtk_signal_connect (GTK_OBJECT (hide_panel_frame_cb), "toggled",
			      GTK_SIGNAL_FUNC (changed_cb), NULL);
	  gtk_box_pack_start (GTK_BOX (vbox), hide_panel_frame_cb, FALSE, FALSE, 0);
	  

	  /* only show tiles when mouse is over the button */
	  tile_when_over_cb = gtk_check_button_new_with_label (_("Show button tiles only when cursor is over the button"));
	  gtk_signal_connect (GTK_OBJECT (tile_when_over_cb), "toggled",
			      GTK_SIGNAL_FUNC (changed_cb), NULL);
	  gtk_box_pack_start (GTK_BOX (vbox), tile_when_over_cb, FALSE, FALSE, 0);

	  /* saturate on mouseovers hack */
	  saturate_when_over_cb = gtk_check_button_new_with_label (_("Keep saturation low when cursor is not on the button"));
	  gtk_signal_connect (GTK_OBJECT (saturate_when_over_cb), "toggled",
			      GTK_SIGNAL_FUNC (changed_cb), NULL);
	  gtk_box_pack_start (GTK_BOX (vbox), saturate_when_over_cb, FALSE, FALSE, 0);
	  
	  return (vbox);
}


static void
sync_applets_page_with_config(GlobalConfig *conf)
{
	switch(conf->movement_type) {
	case PANEL_SWITCH_MOVE:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(movement_type_switch_rb),
					    TRUE);
		break;
	case PANEL_FREE_MOVE:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(movement_type_free_rb),
					    TRUE);
		break;
	case PANEL_PUSH_MOVE:
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(movement_type_push_rb),
					    TRUE);
		break;
	default: break;
	}
	gtk_adjustment_set_value(applet_padding,conf->applet_padding);
}
static void
sync_config_with_applets_page(GlobalConfig *conf)
{
	if(GTK_TOGGLE_BUTTON(movement_type_switch_rb)->active)
		conf->movement_type = PANEL_SWITCH_MOVE;
	else if(GTK_TOGGLE_BUTTON(movement_type_free_rb)->active)
		conf->movement_type = PANEL_FREE_MOVE;
	else if(GTK_TOGGLE_BUTTON(movement_type_push_rb)->active)
		conf->movement_type = PANEL_PUSH_MOVE;
	
	conf->applet_padding = applet_padding->value;
}


static GtkWidget *
applets_notebook_page (void)
{
	GtkWidget *frame;
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
	movement_type_switch_rb = gtk_radio_button_new_with_label (NULL, _("Switched movement (or use Ctrl)"));
	gtk_signal_connect (GTK_OBJECT (movement_type_switch_rb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb), NULL);
	gtk_box_pack_start (GTK_BOX (box), movement_type_switch_rb, FALSE, FALSE, 0);

	/* Free */
	movement_type_free_rb = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (movement_type_switch_rb)),
		_("Free movement (doesn't disturb other applets) (or use Alt)"));
	gtk_signal_connect (GTK_OBJECT (movement_type_free_rb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb), NULL);
	gtk_box_pack_start (GTK_BOX (box), movement_type_free_rb, FALSE, FALSE, 0);

	/* Push */
	movement_type_push_rb = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (movement_type_switch_rb)),
		_("Push movement (or use Shift)"));
	gtk_signal_connect (GTK_OBJECT (movement_type_push_rb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb), NULL);
	gtk_box_pack_start (GTK_BOX (box), movement_type_push_rb, FALSE, FALSE, 0);	

	box = make_int_scale_box(_("Applet padding"),
				 &applet_padding,
				 0.0, 10.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
	

	return vbox;
}

static void
sync_misc_page_with_config(GlobalConfig *conf)
{
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(show_small_icons_cb),
				    conf->show_small_icons);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(show_dot_buttons_cb),
				    conf->show_dot_buttons);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(off_panel_popups_cb),
				    conf->off_panel_popups);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(hungry_menus_cb),
				    conf->hungry_menus);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(tooltips_enabled_cb),
				    conf->tooltips_enabled);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(drawer_auto_close_cb),
				    conf->drawer_auto_close);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(autoraise_cb),
				    conf->autoraise);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(keep_bottom_cb),
				    conf->keep_bottom);
}
static void
sync_config_with_misc_page(GlobalConfig *conf)
{
	conf->show_small_icons =
		GTK_TOGGLE_BUTTON(show_small_icons_cb)->active;
	conf->show_dot_buttons =
		GTK_TOGGLE_BUTTON(show_dot_buttons_cb)->active;
	conf->off_panel_popups =
		GTK_TOGGLE_BUTTON(off_panel_popups_cb)->active;
	conf->hungry_menus =
		GTK_TOGGLE_BUTTON(hungry_menus_cb)->active;
	conf->tooltips_enabled =
		GTK_TOGGLE_BUTTON(tooltips_enabled_cb)->active;
	conf->drawer_auto_close =
		GTK_TOGGLE_BUTTON(drawer_auto_close_cb)->active;
	conf->autoraise =
		GTK_TOGGLE_BUTTON(autoraise_cb)->active;
	conf->keep_bottom =
		GTK_TOGGLE_BUTTON(keep_bottom_cb)->active;
}

static GtkWidget *
misc_notebook_page(void)
{
	GtkWidget *frame;
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
	show_small_icons_cb = gtk_check_button_new_with_label (_("Show small icons"));
	gtk_signal_connect (GTK_OBJECT (show_small_icons_cb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb), NULL);
	gtk_table_attach_defaults(GTK_TABLE(table),show_small_icons_cb, 0,1,0,1);

	/* Dot Buttons */
	show_dot_buttons_cb = gtk_check_button_new_with_label (_("Show [...] buttons"));
	gtk_signal_connect (GTK_OBJECT (show_dot_buttons_cb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb),  NULL);
	gtk_table_attach_defaults(GTK_TABLE(table),show_dot_buttons_cb, 1,2,0,1);

	/* Off Panel Popup menus */
	off_panel_popups_cb = gtk_check_button_new_with_label (_("Show popup menus outside of panels"));
	gtk_signal_connect (GTK_OBJECT (off_panel_popups_cb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb),  NULL);
	gtk_table_attach_defaults(GTK_TABLE(table),off_panel_popups_cb, 0,1,1,2);

	/* Hungry Menus */
	hungry_menus_cb = gtk_check_button_new_with_label (_("Keep menus in memory"));
	gtk_signal_connect (GTK_OBJECT (hungry_menus_cb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb), NULL);
	gtk_table_attach_defaults(GTK_TABLE(table),hungry_menus_cb, 1,2,1,2);
	
	/* Miscellaneous frame */
	frame = gtk_frame_new (_("Miscellaneous"));
	gtk_container_set_border_width(GTK_CONTAINER (frame), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	
	/* vbox for frame */
	box = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER (box), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), box);

	/* Tooltips enable */
	tooltips_enabled_cb = gtk_check_button_new_with_label (_("Tooltips enabled"));
	gtk_signal_connect (GTK_OBJECT (tooltips_enabled_cb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb), NULL);
	gtk_box_pack_start (GTK_BOX (box), tooltips_enabled_cb, FALSE, FALSE, 0);	
	
	/* Drawer/launcher auto close */
	drawer_auto_close_cb = gtk_check_button_new_with_label (_("Close drawer if a launcher inside it is pressed"));
	gtk_signal_connect (GTK_OBJECT (drawer_auto_close_cb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb),  NULL);
	gtk_box_pack_start (GTK_BOX (box), drawer_auto_close_cb, FALSE, FALSE, 0);

	/* Autoraise */
	autoraise_cb = gtk_check_button_new_with_label (_("Raise panels on mouse-over (non GNOME compliant window managers only)"));
	gtk_signal_connect (GTK_OBJECT (autoraise_cb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb), NULL);
	gtk_box_pack_start (GTK_BOX (box), autoraise_cb, FALSE, FALSE, 0);

	/* Keep on bottom */
	keep_bottom_cb = gtk_check_button_new_with_label (_("Keep panel below windows (GNOME compliant window managers only)"));
	gtk_signal_connect (GTK_OBJECT (keep_bottom_cb), "toggled", 
			    GTK_SIGNAL_FUNC (changed_cb), NULL);
	gtk_box_pack_start (GTK_BOX (box), keep_bottom_cb, FALSE, FALSE, 0);

	return (vbox);
}

static void
help(GtkWidget *capplet)
{
	gchar *tmp;

	tmp = gnome_help_file_find_file ("users-guide", "confglobal.html");
	if (tmp) {
		gnome_help_goto(0, tmp);
		g_free(tmp);
	} else {
		GtkWidget *mbox;

		mbox = gnome_message_box_new(_("No help is available/installed for these settings. Please make sure you\nhave the GNOME User's Guide installed on your system."),
					     GNOME_MESSAGE_BOX_ERROR,
					     _("Close"), NULL);

		gtk_widget_show(mbox);
	}
}

static void
loadup_vals(void)
{
	GString *buf;
	char *tile_def[]={"normal","purple","green","blue"};
	int i;
	
	buf = g_string_new(NULL);

	/*set up global options*/
	
	gnome_config_push_prefix("/panel/Config/");

	global_config.tooltips_enabled =
		gnome_config_get_bool("tooltips_enabled=TRUE");

	global_config.show_small_icons =
		gnome_config_get_bool("show_small_icons=TRUE");

	global_config.show_dot_buttons =
		gnome_config_get_bool("show_dot_buttons=FALSE");

	global_config.hungry_menus =
		gnome_config_get_bool("hungry_menus=TRUE");

	global_config.off_panel_popups =
		gnome_config_get_bool("off_panel_popups=TRUE");
		
	global_config.disable_animations =
		gnome_config_get_bool("disable_animations=FALSE");
		
	g_string_sprintf(buf,"auto_hide_step_size=%d",
			 DEFAULT_AUTO_HIDE_STEP_SIZE);
	global_config.auto_hide_step_size=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"explicit_hide_step_size=%d",
			 DEFAULT_EXPLICIT_HIDE_STEP_SIZE);
	global_config.explicit_hide_step_size=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"drawer_step_size=%d",
			 DEFAULT_DRAWER_STEP_SIZE);
	global_config.drawer_step_size=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"minimize_delay=%d", DEFAULT_MINIMIZE_DELAY);
	global_config.minimize_delay=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"minimized_size=%d", DEFAULT_MINIMIZED_SIZE);
	global_config.minimized_size=gnome_config_get_int(buf->str);
		
	g_string_sprintf(buf,"movement_type=%d", PANEL_SWITCH_MOVE);
	global_config.movement_type=gnome_config_get_int(buf->str);

	global_config.applet_padding=gnome_config_get_int("applet_padding=3");

	global_config.autoraise = gnome_config_get_bool("autoraise=TRUE");

	global_config.keep_bottom = gnome_config_get_bool("keep_bottom=FALSE");

	global_config.drawer_auto_close = gnome_config_get_bool("drawer_auto_close=FALSE");
	global_config.simple_movement = gnome_config_get_bool("simple_movement=FALSE");
	global_config.hide_panel_frame = gnome_config_get_bool("hide_panel_frame=FALSE");
	global_config.tile_when_over = gnome_config_get_bool("tile_when_over=FALSE");
	global_config.saturate_when_over = gnome_config_get_bool("saturate_when_over=TRUE");
	for(i=0;i<LAST_TILE;i++) {
		g_string_sprintf(buf,"tiles_enabled_%d=FALSE",i);
		global_config.tiles_enabled[i] =
			gnome_config_get_bool(buf->str);

		g_free(global_config.tile_up[i]);
		g_string_sprintf(buf,"tile_up_%d=tiles/tile-%s-up.png",
			   i, tile_def[i]);
		global_config.tile_up[i] = gnome_config_get_string(buf->str);

		g_free(global_config.tile_down[i]);
		g_string_sprintf(buf,"tile_down_%d=tiles/tile-%s-down.png",
			   i,tile_def[i]);
		global_config.tile_down[i] = gnome_config_get_string(buf->str);

		g_string_sprintf(buf,"tile_border_%d=2",i);
		global_config.tile_border[i] = gnome_config_get_int(buf->str);
		g_string_sprintf(buf,"tile_depth_%d=2",i);
		global_config.tile_depth[i] = gnome_config_get_int(buf->str);
	}
	g_string_free(buf,TRUE);
		
	gnome_config_pop_prefix();
}

static void
tell_panel(void)
{
	CORBA_Environment ev;
	GNOME_Panel panel_client = CORBA_OBJECT_NIL;

	panel_client =
		goad_server_activate_with_repo_id(NULL,
						  "IDL:GNOME/Panel:1.0",
						  GOAD_ACTIVATE_EXISTING_ONLY,
						  NULL);
	
	if(!panel_client) return;
	CORBA_exception_init(&ev);
	GNOME_Panel_notice_config_changes(panel_client, &ev);
	CORBA_exception_free(&ev);
}

static void
write_config(GlobalConfig *conf)
{
	int i;
	GString *buf;
	gnome_config_push_prefix("/panel/Config/");

	gnome_config_set_int("auto_hide_step_size",
			     conf->auto_hide_step_size);
	gnome_config_set_int("explicit_hide_step_size",
			     conf->explicit_hide_step_size);
	gnome_config_set_int("drawer_step_size",
			     conf->drawer_step_size);
	gnome_config_set_int("minimized_size",
			     conf->minimized_size);
	gnome_config_set_int("minimize_delay",
			     conf->minimize_delay);
	gnome_config_set_int("movement_type",
			     (int)conf->movement_type);
	gnome_config_set_bool("tooltips_enabled",
			      conf->tooltips_enabled);
	gnome_config_set_bool("show_small_icons",
			      conf->show_small_icons);
	gnome_config_set_bool("show_dot_buttons",
			      conf->show_dot_buttons);
	gnome_config_set_bool("hungry_menus",
			      conf->hungry_menus);
	gnome_config_set_bool("off_panel_popups",
			      conf->off_panel_popups);
	gnome_config_set_bool("disable_animations",
			      conf->disable_animations);
	gnome_config_set_int("applet_padding",
			     conf->applet_padding);
	gnome_config_set_bool("autoraise",
			      conf->autoraise);
	gnome_config_set_bool("keep_bottom",
			      conf->keep_bottom);
	gnome_config_set_bool("drawer_auto_close",
			      conf->drawer_auto_close);
	gnome_config_set_bool("simple_movement",
			      conf->simple_movement);
	gnome_config_set_bool("hide_panel_frame",
			      conf->hide_panel_frame);
	gnome_config_set_bool("tile_when_over",
			      conf->tile_when_over);
	gnome_config_set_bool("saturate_when_over",
			      conf->saturate_when_over);
	buf = g_string_new(NULL);
	for(i=0;i<LAST_TILE;i++) {
		g_string_sprintf(buf,"tiles_enabled_%d",i);
		gnome_config_set_bool(buf->str,
				      conf->tiles_enabled[i]);
		g_string_sprintf(buf,"tile_up_%d",i);
		gnome_config_set_string(buf->str,
					conf->tile_up[i]);
		g_string_sprintf(buf,"tile_down_%d",i);
		gnome_config_set_string(buf->str,
					conf->tile_down[i]);
		g_string_sprintf(buf,"tile_border_%d",i);
		gnome_config_set_int(buf->str,
				     conf->tile_border[i]);
		g_string_sprintf(buf,"tile_depth_%d",i);
		gnome_config_set_int(buf->str,
				     conf->tile_depth[i]);
	}
	g_string_free(buf,TRUE);
	gnome_config_pop_prefix();
	gnome_config_sync();
	
	tell_panel();
}

static void
try(GtkWidget *capplet, gpointer data)
{
	sync_config_with_animation_page(&global_config);
	sync_config_with_buttons_page(&global_config);
	sync_config_with_applets_page(&global_config);
	sync_config_with_misc_page(&global_config);
	write_config(&global_config);
}

static void
revert(GtkWidget *capplet, gpointer data)
{
	sync_animation_page_with_config(&loaded_config);
	sync_buttons_page_with_config(&loaded_config);
	sync_applets_page_with_config(&loaded_config);
	sync_misc_page_with_config(&loaded_config);
	global_config = loaded_config;
	write_config(&global_config);
}

static void
setup_the_ui(GtkWidget *capplet)
{
	GtkWidget *page;
	GtkWidget *nbook;
	
	nbook = gtk_notebook_new();
	gtk_container_add(GTK_CONTAINER(capplet),nbook);

	/* Animation notebook page */
	page = animation_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK(nbook),
				  page, gtk_label_new (_("Animation")));

	/* General icon notebook page */
	page = buttons_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK(nbook),
				    page, gtk_label_new (_("Buttons")));

	/* applet settings */
	page = applets_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK (nbook),
				  page, gtk_label_new (_("Applets")));
		
	/* Miscellaneous notebook page */
	page = misc_notebook_page ();
	gtk_notebook_append_page (GTK_NOTEBOOK(nbook),
				  page, gtk_label_new (_("Miscellaneous")));


	gtk_signal_connect (GTK_OBJECT (nbook), "event",
			    GTK_SIGNAL_FUNC (config_event),
			    nbook);

	/* show notebook */
	gtk_widget_show_all(nbook);

	sync_animation_page_with_config(&loaded_config);
	sync_buttons_page_with_config(&loaded_config);
	sync_applets_page_with_config(&loaded_config);
	sync_misc_page_with_config(&loaded_config);

	/* Finished */
	gtk_widget_show_all(capplet);
	gtk_signal_connect(GTK_OBJECT(capplet), "try",
			   GTK_SIGNAL_FUNC(try), NULL);
	gtk_signal_connect(GTK_OBJECT(capplet), "revert",
			   GTK_SIGNAL_FUNC(revert), NULL);
	gtk_signal_connect(GTK_OBJECT(capplet), "ok",
			   GTK_SIGNAL_FUNC(try), NULL);
	gtk_signal_connect(GTK_OBJECT(capplet), "cancel",
			   GTK_SIGNAL_FUNC(revert), NULL);
	gtk_signal_connect(GTK_OBJECT(capplet), "help",
			   GTK_SIGNAL_FUNC(help), NULL);
}

int
main (int argc, char **argv)
{
	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	changing = TRUE;

	if(gnome_capplet_init("gnome-panel-properties", VERSION, argc,
			      argv, NULL, 0, NULL) < 0)
		return 1;
	
	loadup_vals();
	
	loaded_config = global_config;

	capplet = capplet_widget_new();

	setup_the_ui(capplet);

	changing = FALSE;
	capplet_gtk_main();

	return 0;
}
